// The order book depth shared by the chart panes (see DepthStream.h). A Deepwater addition.
#include "DepthStream.h"

#include "Async.h"
#include "BinanceSocket.h"
#include "Server.h"
#include "../core/DepthRecord.h"
#include "../core/Error.h"

#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QSet>
#include <QThread>
#include <QTimer>
#include <QtConcurrent>

#include <algorithm>
#include <atomic>
#include <ctime>
#include <deque>
#ifdef _WIN32
#include <windows.h>
#endif

namespace qtm {

namespace {
constexpr int SNAPSHOT_LEVELS = 5000;     // Binance's most (request weight 250 of the 6000 a minute)
constexpr double SNAPSHOT_GAP_MS = 3000;  // between two snapshot requests, whatever happens
constexpr int RETRY_MS = 5000;
// The timer samples this far behind the exchange's clock (as estimated from the updates): an update stamped before
// a sample time has arrived by then, so the column has it.
constexpr double TIMER_LAG_MS = 300;

std::atomic<quint64> g_rawIds{0};

// A blocking backend call on the UI's request pool (which the backend waits for before it goes), its answer back on
// `ctx`'s thread while `ctx` exists. What it throws comes back in `fail`.
template <typename T>
void runOff(QObject *ctx, std::function<T()> work, std::function<void(const T &)> ok, DepthSource::Fail fail)
{
    struct Out {
        T value{};
        QString error;
        bool good = true;
    };
    auto *w = new QFutureWatcher<Out>(ctx);
    QObject::connect(w, &QFutureWatcherBase::finished, ctx, [w, ok, fail] {
        const Out r = w->result();
        w->deleteLater();
        if (r.good) ok(r.value);
        else if (fail) fail(r.error);
    });
    w->setFuture(QtConcurrent::run(async::requestPool(), [work] {
        Out r;
        try {
            r.value = work();
        } catch (const Error &e) {
            r.good = false;
            r.error = e.message();
        } catch (const std::exception &e) {
            r.good = false;
            r.error = QString::fromUtf8(e.what());
        }
        return r;
    }));
}

// Files being written by a stream of this app: two mustn't append to the same file (a stream going while the next one
// for the pair starts).
QMutex g_recMutex;
QSet<QString> g_recFiles;

double threadCpuMs()
{
#ifdef _WIN32
    FILETIME created, exited, kernel, user;
    GetThreadTimes(GetCurrentThread(), &created, &exited, &kernel, &user);
    auto ms = [](const FILETIME &t) { return double((quint64(t.dwHighDateTime) << 32) | t.dwLowDateTime) / 1e4; };
    return ms(kernel) + ms(user);   // 100 ns units to milliseconds
#else
    timespec ts{};
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
    return double(ts.tv_sec) * 1e3 + double(ts.tv_nsec) / 1e6;
#endif
}
double num(const QJsonValue &v) { return v.isString() ? v.toString().toDouble() : v.toDouble(); }

std::vector<depth::Level> levels(const QJsonArray &a, double tick)
{
    std::vector<depth::Level> out;
    out.reserve(size_t(a.size()));
    for (const QJsonValue v : a) {
        const QJsonArray l = v.toArray();
        out.push_back({std::llround(num(l.at(0)) / tick), num(l.at(1))});
    }
    return out;
}

} // namespace

// ---------------------------------------------------------------- Binance

void BinanceDepthSource::config(QObject *context, const QString &ticker, std::function<void(const Config &)> ok, Fail fail)
{
    Backend *b = m_backend;
    runOff<Config>(context, [b, ticker] {
        // The tick size, and which exchange answers with the book (a 5-level snapshot, weight 5).
        Config c;
        c.tick = b->binanceTickSize(ticker);
        const QJsonObject d = b->binanceDepth(ticker, 5, &c.us);
        const double bid = num(d.value("bids").toArray().first().toArray().first());
        const double ask = num(d.value("asks").toArray().first().toArray().first());
        c.price = bid > 0 && ask > 0 ? (bid + ask) / 2 : std::max(bid, ask);
        return c;
    }, std::move(ok), std::move(fail));
}

void BinanceDepthSource::snapshot(QObject *context, const QString &ticker, double tick, int count,
                                  std::function<void(const Snapshot &)> ok, Fail fail)
{
    Backend *b = m_backend;
    runOff<Snapshot>(context, [b, ticker, tick, count] {
        Snapshot s;
        const QJsonObject d = b->binanceDepth(ticker, count, &s.us);
        s.book.lastUpdateId = d.value("lastUpdateId").toInteger();
        s.book.bids = levels(d.value("bids").toArray(), tick);   // parsed here, off the stream's thread
        s.book.asks = levels(d.value("asks").toArray(), tick);
        return s;
    }, std::move(ok), std::move(fail));
}

DepthSource::Live *BinanceDepthSource::live(QObject *parent, const QString &ticker, bool us, double tick,
                                             std::function<void(const depth::DepthUpdate &, double)> update,
                                             std::function<void(bool)> closed)
{
    return new BinanceSocket(parent, ticker.toLower() + "@depth@100ms", us,
        [tick, update = std::move(update)](const QJsonObject &o, double recv) {
            if (o.value("e").toString() != "depthUpdate") return;
            depth::DepthUpdate u;
            u.firstId = o.value("U").toInteger();
            u.lastId = o.value("u").toInteger();
            if (o.contains("pu")) u.prevLastId = o.value("pu").toInteger();
            u.time = o.value("E").toDouble() / 1000;
            u.bids = levels(o.value("b").toArray(), tick);
            u.asks = levels(o.value("a").toArray(), tick);
            update(u, recv);
        },
        {}, std::move(closed));
}

// ---------------------------------------------------------------- recorder (the worker's thread)

// Writes a pair's recording: one file per UTC hour (yyyy-MM-ddTHH.dwdepth), a header when it's new, a Reset when the
// session starts, a Checkpoint of the book when a file starts (so each file can be read on its own), and the records
// as they come. Blocks go to disk every half second.
class DepthRecorder
{
public:
    DepthRecorder(const QString &dir, const depthrec::Header &h) : m_dir(dir), m_h(h) {}
    ~DepthRecorder()
    {
        flush();
        release();
    }
    // Before each record: the file of its hour (a new hour: the book's checkpoint starts it). False: can't write.
    bool at(double time, const depth::OrderBook &book)
    {
        const QDateTime t = QDateTime::fromMSecsSinceEpoch(qint64(time * 1000), QTimeZone::UTC);
        const QString name = m_dir + "/" + t.toString("yyyy-MM-dd'T'HH") + ".dwdepth";
        if (name == m_name && m_file.isOpen()) return true;
        flush();
        release();
        QDir().mkpath(m_dir);
        {
            QMutexLocker lock(&g_recMutex);
            if (g_recFiles.contains(name)) {   // the previous stream of this pair is still closing it
                m_note = "the file is still in use";
                return false;
            }
            g_recFiles.insert(name);
        }
        m_note.clear();
        m_name = name;
        m_file.setFileName(name);
        const bool fresh = !m_file.exists() || m_file.size() == 0;
        if (!m_file.open(QIODevice::Append)) {
            m_note = "can't write " + name;
            release();
            return false;
        }
        if (fresh) m_bytes += m_file.write(depthrec::encodeHeader(m_h));
        // A session starts: the book starts over. A new hour of the same session: the book as it is (a file can be
        // read on its own, and read after the previous one the book and the samples just go on).
        if (!m_started) m_w.reset(time);
        m_started = true;
        if (book.synced()) m_w.checkpoint(time, book);
        return true;
    }
    depthrec::Writer &writer() { return m_w; }
    void flush()
    {
        const QByteArray b = m_w.takeBlock();
        if (b.isEmpty() || !m_file.isOpen()) return;
        m_bytes += m_file.write(b);
        m_file.flush();
    }
    qint64 bytes() const { return m_bytes; }
    QString file() const { return m_name; }
    QString note() const { return m_note; }
    // Deletes this pair's recordings older than `days`.
    static void prune(const QString &dir, int days)
    {
        const QDateTime limit = QDateTime::currentDateTimeUtc().addDays(-days);
        for (const QFileInfo &f : QDir(dir).entryInfoList({"*.dwdepth"}, QDir::Files))
            if (f.lastModified() < limit) QFile::remove(f.absoluteFilePath());
    }

private:
    void release()
    {
        if (m_file.isOpen()) m_file.close();
        if (!m_name.isEmpty()) {
            QMutexLocker lock(&g_recMutex);
            g_recFiles.remove(m_name);
        }
        m_name.clear();
    }
    QString m_dir, m_name, m_note;
    depthrec::Header m_h;
    depthrec::Writer m_w;
    QFile m_file;
    qint64 m_bytes = 0;
    bool m_started = false;
};

// ---------------------------------------------------------------- the worker (its own thread)

// Lives on the stream's thread: the socket, the book, the samplers (one per interval), the recorder. Only DepthStream
// talks to it, by queued calls; it answers the same way.
class DepthStreamWorker : public QObject
{
public:
    DepthStreamWorker(DepthStream *stream, std::shared_ptr<DepthSource> source, const QString &ticker,
                      const QString &recordRoot)
        : m_stream(stream), m_source(std::move(source)), m_ticker(ticker), m_root(recordRoot)
    {
    }
    ~DepthStreamWorker() override { stop(); }

    void setNeeds(const DepthStream::Needs &n)
    {
        const bool recording = n.record != m_needs.record || n.keepDays != m_needs.keepDays;
        m_needs = n;
        if (recording) {
            if (!n.record) m_rec.reset();
            else ensureRecorder();
        }
        // Configured even while stopped: a pane in replay shows the recordings.
        if (!m_configured && !n.history.empty()) fetchConfig();
        if (m_configured) syncSamplers();
        if (n.active && !n.history.empty()) start();
        else stop();
    }

private:
    struct Channel {
        std::unique_ptr<heatmap::Sampler> sampler;
    };

    void start()
    {
        if (!m_stopped) return;
        // Made here, not in the constructor: they must live on this thread.
        if (!m_tick50) {
            m_tick50 = new QTimer(this);
            m_tick50->setInterval(50);
            connect(m_tick50, &QTimer::timeout, this, [this] {
                if (!std::isnan(m_offsetMs))
                    sampleTo((double(QDateTime::currentMSecsSinceEpoch()) + m_offsetMs - TIMER_LAG_MS) / 1000);
            });
            m_statsTimer = new QTimer(this);
            m_statsTimer->setInterval(500);
            connect(m_statsTimer, &QTimer::timeout, this, [this] { emitStats(); });
            m_retry = new QTimer(this);
            m_retry->setSingleShot(true);
            m_cpuAt = threadCpuMs();
            m_cpuWall = DepthStream::steadyMs();
        }
        m_stopped = false;
        m_tick50->start();
        m_statsTimer->start();
        if (m_configured) openLive();   // else once configured
    }

    void stop()
    {
        if (m_rec && !m_stopped && m_configured) {
            m_rec->at(exchangeNow(), m_book);
            m_rec->writer().lost(exchangeNow());
            m_rec->flush();
        }
        m_stopped = true;
        ++m_token;
        m_snapshotBusy = false;
        if (m_retry) {
            m_retry->stop();
            m_retry->disconnect();
        }
        if (m_tick50) m_tick50->stop();
        if (m_statsTimer) m_statsTimer->stop();
        closeLive();
        m_book.reset();
        for (auto &[iv, c] : m_channels) c.sampler->reset();
        m_offsetMs = NAN;
    }

    double exchangeNow() const
    {
        const double now = double(QDateTime::currentMSecsSinceEpoch());
        return (std::isnan(m_offsetMs) ? now : now + m_offsetMs) / 1000;
    }

    void setError(const QString &e)
    {
        if (m_error == e) return;
        m_error = e;
        emitStats();
    }

    void ensureRecorder()
    {
        if (!m_needs.record || m_rec || !m_configured) return;
        const QString dir = DepthStream::recordDir(m_root, m_ticker, m_us);
        DepthRecorder::prune(dir, std::max(1, m_needs.keepDays));
        m_rec = std::make_unique<DepthRecorder>(dir, depthrec::Header{m_ticker, m_tick, m_us});
    }

    // The tick size, and which exchange answers with the book: the storage row depends on the price, and the stream
    // must be that exchange's.
    void fetchConfig()
    {
        if (m_configBusy) return;
        m_configBusy = true;
        m_source->config(this, m_ticker,
            [this](const DepthSource::Config &c) {
                m_configBusy = false;
                m_tick = c.tick;
                m_us = c.us;
                m_rowTicks = heatmap::autoStorageRowTicks(c.price, m_tick);
                m_configured = true;
                DepthStream *stream = m_stream;
                const double tick = m_tick;
                const int rows = m_rowTicks;
                QMetaObject::invokeMethod(stream, [stream, tick, rows] { stream->onConfigured(tick, rows); },
                                          Qt::QueuedConnection);
                setError(QString());
                syncSamplers();   // and the recordings
                ensureRecorder();
                if (!m_stopped) openLive();
            },
            [this](const QString &e) {   // tried again even when stopped: the recordings need it too (replay)
                m_configBusy = false;
                setError(e);
                QTimer::singleShot(RETRY_MS, this, [this] {
                    if (!m_configured && !m_needs.history.empty()) fetchConfig();
                });
            });
    }

    // A sampler per interval asked for; a new one starts with the recordings of its history window.
    void syncSamplers()
    {
        for (auto it = m_channels.begin(); it != m_channels.end();)
            it = m_needs.history.count(it->first) ? std::next(it) : m_channels.erase(it);
        for (const auto &[iv, minutes] : m_needs.history) {
            if (m_channels.count(iv)) continue;
            heatmap::SamplerParams sp;
            sp.intervalMs = iv;
            sp.rowTicks = m_rowTicks;
            sp.bandRows = DepthStream::BAND_ROWS;
            Channel &c = m_channels[iv];
            c.sampler = std::make_unique<heatmap::Sampler>(sp);
            preload(iv, minutes, sp);
        }
    }

    // The recordings that reach into the history window, replayed in order through a book and a sampler of their own
    // (as the live stream would have drawn them), the columns handed over in batches. A file of another tick size or
    // exchange is skipped. Holes between sessions stay holes.
    void preload(int iv, int minutes, const heatmap::SamplerParams &sp)
    {
        QElapsedTimer t;
        t.start();
        if (m_rec) m_rec->flush();   // this session's own records too
        const QString dir = DepthStream::recordDir(m_root, m_ticker, m_us);
        const double now = double(QDateTime::currentMSecsSinceEpoch()) / 1000;
        const double from = now - double(minutes) * 60;
        const QStringList names = QDir(dir).entryList({"*.dwdepth"}, QDir::Files, QDir::Name);
        depthrec::Replayer rp(sp);
        DepthStream *stream = m_stream;
        auto hand = [&] {
            std::vector<heatmap::Column> cols = rp.take();
            cols.erase(std::remove_if(cols.begin(), cols.end(), [from](const heatmap::Column &c) { return c.meta.time < from; }),
                       cols.end());
            if (cols.empty()) return;
            m_loadedColumns += cols.size();
            QMetaObject::invokeMethod(stream, [stream, iv, cols = std::move(cols)]() mutable {
                stream->onColumns(iv, std::move(cols), NAN);
            }, Qt::QueuedConnection);
        };
        for (const QString &n : names) {
            // yyyy-MM-ddTHH: the hour it covers; files ending before the window are skipped
            const QDateTime h = QDateTime::fromString(n.left(13), "yyyy-MM-dd'T'HH");
            if (!h.isValid()) continue;
            QDateTime hu = h;
            hu.setTimeZone(QTimeZone::UTC);
            if (double(hu.toSecsSinceEpoch()) + 3600 < from) continue;
            QFile f(dir + "/" + n);
            if (!f.open(QIODevice::ReadOnly)) continue;
            depthrec::Reader r(f.readAll());
            if (!r.ok() || r.header().ticker != m_ticker || r.header().us != m_us
                || std::fabs(r.header().tickSize - m_tick) > 1e-12)
                continue;
            ++m_loadedFiles;
            depthrec::Record rec;
            int k = 0;
            while (r.next(rec)) {
                rp.feed(rec);
                if (++k % 20000 == 0) hand();
            }
            rp.feed(depthrec::Record{depthrec::Type::Lost, rec.time, {}, {}});   // a file's end: what follows is a hole
            hand();
        }
        m_loadMs += double(t.nsecsElapsed()) / 1e6;
        emitStats();
    }

    void openLive()
    {
        if (m_stopped || !m_configured) return;
        closeLive();
        const int token = m_liveToken;
        m_live = m_source->live(this, m_ticker, m_us, m_tick,
            [this, token](const depth::DepthUpdate &u, double recv) {
                if (token == m_liveToken) onUpdate(u, recv);
            },
            [this, token](bool opened) {
                if (token != m_liveToken) return;
                closeLive();
                if (m_rec && m_rec->at(exchangeNow(), m_book)) m_rec->writer().lost(exchangeNow());
                m_book.desync();   // updates are missed from now on: the next ones wait for a snapshot
                if (m_stopped) return;
                m_retry->disconnect();
                connect(m_retry, &QTimer::timeout, this, [this] { openLive(); });
                if (!opened) {
                    setError("Binance's depth stream isn't reachable (trying again)");
                    m_retry->start(RETRY_MS);
                } else {
                    m_retry->start(3000);
                }
            });
    }

    void closeLive()
    {
        ++m_liveToken;
        if (m_live) {
            m_live->deleteLater();   // maybe from inside its own callback
            m_live = nullptr;
        }
    }

    void onUpdate(const depth::DepthUpdate &u, double recv)
    {
        const double eventMs = u.time * 1000;
        // The exchange's clock as seen from here (latency included), smoothed.
        const double off = eventMs - recv;
        m_offsetMs = std::isnan(m_offsetMs) ? off : m_offsetMs + 0.05 * (off - m_offsetMs);
        m_net.push_back(recv - eventMs);
        ++m_updates;
        m_levelsIn += u.bids.size() + u.asks.size();

        if (m_rec && m_rec->at(u.time, m_book)) m_rec->writer().update(u);   // recorded as received, before it goes in
        sampleTo(u.time);   // the columns before this update show the book before it
        const depth::OrderBook::Result r = m_book.apply(u);
        if (r == depth::OrderBook::Result::NeedSnapshot || r == depth::OrderBook::Result::Crossed
            || r == depth::OrderBook::Result::Buffered)
            requestSnapshot();
    }

    void requestSnapshot()
    {
        if (m_snapshotBusy || m_stopped) return;
        m_snapshotBusy = true;
        const double wait = std::max(0.0, m_lastSnapshotAt + SNAPSHOT_GAP_MS - DepthStream::steadyMs());
        const int token = m_token;
        QTimer::singleShot(int(wait), this, [this, token] {
            if (token != m_token) return;
            m_lastSnapshotAt = DepthStream::steadyMs();
            m_source->snapshot(this, m_ticker, m_tick, SNAPSHOT_LEVELS,
                [this, token](const DepthSource::Snapshot &d) {
                    if (token != m_token) return;
                    m_snapshotBusy = false;
                    if (d.us != m_us) {
                        setError("Binance answered from another exchange (trying again)");
                        requestSnapshot();
                        return;
                    }
                    const depth::DepthSnapshot &s = d.book;
                    // How far the snapshot reaches: levels beyond only show once they change.
                    auto byTick = [](const depth::Level &a, const depth::Level &b) { return a.tick < b.tick; };
                    m_snapLow = s.bids.empty() ? NAN : double(std::min_element(s.bids.begin(), s.bids.end(), byTick)->tick) * m_tick;
                    m_snapHigh = s.asks.empty() ? NAN : double(std::max_element(s.asks.begin(), s.asks.end(), byTick)->tick) * m_tick;
                    if (m_rec && m_rec->at(exchangeNow(), m_book)) m_rec->writer().snapshot(exchangeNow(), s);
                    const depth::OrderBook::Result r = m_book.setSnapshot(s);
                    if (r == depth::OrderBook::Result::Applied) setError(QString());
                    else requestSnapshot();   // older than the stream (it was buffering meanwhile), or crossed: again
                    emitStats();
                },
                [this, token](const QString &e) {
                    if (token != m_token) return;
                    m_snapshotBusy = false;
                    setError(e);
                    requestSnapshot();   // after the gap
                });
        });
    }

    void sampleTo(double exchangeTime)
    {
        DepthStream *stream = m_stream;
        const double madeAt = DepthStream::steadyMs();
        for (auto &[iv, c] : m_channels) {
            std::vector<heatmap::Column> cols = c.sampler->advance(m_book, exchangeTime);
            if (cols.empty()) continue;
            const int interval = iv;
            QMetaObject::invokeMethod(stream, [stream, interval, cols = std::move(cols), madeAt]() mutable {
                stream->onColumns(interval, std::move(cols), madeAt);
            }, Qt::QueuedConnection);
        }
    }

    void emitStats()
    {
        DepthStats s;
        if (m_rec) {
            m_rec->flush();   // every half second: a crash loses at most that
            s.recording = m_rec->note().isEmpty() && !m_rec->file().isEmpty();
            s.recordedBytes = m_rec->bytes();
            s.recordFile = m_rec->file();
            s.recordNote = m_rec->note();
        }
        s.loadedFiles = m_loadedFiles;
        s.loadedColumns = m_loadedColumns;
        s.loadMs = m_loadMs;
        s.synced = m_book.synced();
        s.us = m_us;
        s.error = m_error;
        s.levels = m_book.levelCount();
        const auto &bs = m_book.stats();
        s.applied = bs.applied;
        s.gaps = bs.gaps;
        s.crossed = bs.crossed;
        s.snapshots = bs.snapshots;
        const double wall = DepthStream::steadyMs(), cpu = threadCpuMs();
        const double dt = std::max(1.0, wall - m_cpuWall);
        s.threadCpu = 100 * (cpu - m_cpuAt) / dt;
        s.updatesPerSec = double(m_updates) * 1000 / dt;
        s.levelsPerSec = double(m_levelsIn) * 1000 / dt;
        if (!m_net.empty()) {
            std::vector<double> v(m_net.begin(), m_net.end());
            std::nth_element(v.begin(), v.begin() + ptrdiff_t(v.size() / 2), v.end());
            s.netLatencyMs = v[v.size() / 2];
        }
        s.snapshotLow = m_snapLow;
        s.snapshotHigh = m_snapHigh;
        m_cpuAt = cpu;
        m_cpuWall = wall;
        m_updates = m_levelsIn = 0;
        m_net.clear();
        DepthStream *stream = m_stream;
        QMetaObject::invokeMethod(stream, [stream, s] { stream->onStats(s); }, Qt::QueuedConnection);
    }

    DepthStream *m_stream;   // the queued calls go to it on the UI thread (it waits for this thread before going)
    std::shared_ptr<DepthSource> m_source;
    QString m_ticker, m_root;
    DepthStream::Needs m_needs;
    double m_tick = 0;
    int m_rowTicks = 0;
    bool m_us = false, m_configured = false, m_stopped = true;
    depth::OrderBook m_book;
    std::map<int, Channel> m_channels;
    DepthSource::Live *m_live = nullptr;
    int m_liveToken = 0;           // bumped when the live stream is closed: its late messages are dropped
    QTimer *m_tick50 = nullptr;    // samples while no update comes
    QTimer *m_statsTimer = nullptr;
    QTimer *m_retry = nullptr;
    bool m_snapshotBusy = false, m_configBusy = false;
    std::unique_ptr<DepthRecorder> m_rec;
    double m_loadMs = 0;
    int m_loadedFiles = 0;
    size_t m_loadedColumns = 0;
    double m_lastSnapshotAt = -1e18;   // steady ms
    double m_offsetMs = NAN;           // exchange time − local time, smoothed
    int m_token = 0;
    QString m_error;
    // Stats over the last half second.
    uint64_t m_updates = 0, m_levelsIn = 0;
    std::deque<double> m_net;
    double m_cpuAt = 0, m_cpuWall = 0;
    double m_snapLow = NAN, m_snapHigh = NAN;
};

// ---------------------------------------------------------------- the stream (UI thread)

DepthStream::DepthStream(std::shared_ptr<DepthSource> source, const QString &ticker, const QString &recordRoot,
                         QObject *parent)
    : QObject(parent), m_ticker(ticker.toUpper())
{
    static const int reg = [] {
        qRegisterMetaType<DepthStats>("qtm::DepthStats");
        return 0;
    }();
    (void)reg;
    m_thread = new QThread;
    m_thread->setObjectName("depth " + m_ticker);
    m_worker = new DepthStreamWorker(this, std::move(source), m_ticker, recordRoot);
    m_worker->moveToThread(m_thread);
    m_thread->start();
}

DepthStream::~DepthStream()
{
    // The worker goes on its own thread (its socket and timers live there), then the thread.
    DepthStreamWorker *w = m_worker;
    QMetaObject::invokeMethod(m_worker, [w] { delete w; }, Qt::BlockingQueuedConnection);
    m_thread->quit();
    m_thread->wait();
    delete m_thread;
}

QString DepthStream::recordDir(const QString &recordRoot, const QString &ticker, bool us)
{
    return (recordRoot.isEmpty() ? QDir::tempPath() : recordRoot) + "/depth/" + ticker.toUpper() + (us ? "-US" : "");
}

size_t DepthStream::capacity(int intervalMs, int minutes) const
{
    return size_t(std::max(1.0, double(std::max(1, minutes)) * 60000.0 / std::max(1, intervalMs)));
}

void DepthStream::makeStore(int intervalMs)
{
    Channel &c = m_channels[intervalMs];
    c.store = std::make_unique<heatmap::ColumnStore>(BAND_ROWS, capacity(intervalMs, c.minutes));
    c.rawId = ++g_rawIds;
    c.madeAt = NAN;
}

const heatmap::ColumnStore *DepthStream::store(int intervalMs) const
{
    const auto it = m_channels.find(intervalMs);
    return it == m_channels.end() ? nullptr : it->second.store.get();
}

quint64 DepthStream::rawId(int intervalMs) const
{
    const auto it = m_channels.find(intervalMs);
    return it == m_channels.end() ? 0 : it->second.rawId;
}

double DepthStream::newestMadeAt(int intervalMs) const
{
    const auto it = m_channels.find(intervalMs);
    return it == m_channels.end() ? NAN : it->second.madeAt;
}

void DepthStream::setNeeds(const Needs &n)
{
    m_needs = n;
    for (auto it = m_channels.begin(); it != m_channels.end();)
        it = n.history.count(it->first) ? std::next(it) : m_channels.erase(it);
    for (const auto &[iv, minutes] : n.history) {
        Channel &c = m_channels[iv];
        c.minutes = minutes;
        if (c.store) c.store->setCapacity(capacity(iv, minutes));
        else if (m_tick > 0) makeStore(iv);
    }
    DepthStreamWorker *w = m_worker;
    QMetaObject::invokeMethod(w, [w, n] { w->setNeeds(n); }, Qt::QueuedConnection);
}

void DepthStream::onConfigured(double tick, int rowTicks)
{
    const bool same = m_tick == tick && m_rowTicks == rowTicks;
    m_tick = tick;
    m_rowTicks = rowTicks;
    for (auto &[iv, c] : m_channels)
        if (!c.store || !same) makeStore(iv);
    emit configured();
}

void DepthStream::onColumns(int intervalMs, std::vector<heatmap::Column> cols, double madeAt)
{
    const auto it = m_channels.find(intervalMs);
    if (it == m_channels.end() || !it->second.store) return;   // no pane samples at it any more
    bool any = false;
    for (const heatmap::Column &c : cols) any = it->second.store->append(c) || any;
    if (!any) return;
    it->second.madeAt = madeAt;
    emit columns(intervalMs);
}

void DepthStream::onStats(const DepthStats &s)
{
    m_stats = s;
    emit stateChanged();
}

// ---------------------------------------------------------------- the streams

struct DepthStreams::State {
    std::shared_ptr<DepthSource> source;
    QString root;
    struct Entry {
        DepthStream *stream = nullptr;
        std::vector<Subscription *> subs;
    };
    QHash<QString, Entry> streams;

    // Every subscription's needs together: each interval with its longest history, recording if one records (kept
    // for the longest), running if one runs.
    void update(const QString &key)
    {
        auto it = streams.find(key);
        if (it == streams.end()) return;
        DepthStream::Needs n;
        n.keepDays = 0;
        for (const Subscription *s : it->subs) {
            int &minutes = n.history[s->m_needs.intervalMs];
            minutes = std::max(minutes, s->m_needs.historyMinutes);
            if (s->m_needs.record) {
                n.record = true;
                n.keepDays = std::max(n.keepDays, s->m_needs.keepDays);
            }
            n.active = n.active || s->m_needs.active;
        }
        if (!n.record) n.keepDays = 7;
        it->stream->setNeeds(n);
    }
};

DepthStreams::DepthStreams(std::shared_ptr<DepthSource> source, const QString &recordRoot)
    : m_state(std::make_shared<State>())
{
    m_state->source = std::move(source);
    m_state->root = recordRoot;
}

DepthStreams::~DepthStreams()
{
    for (auto &e : m_state->streams) delete e.stream;   // subscriptions still out see a null stream()
}

std::unique_ptr<DepthStreams::Subscription> DepthStreams::subscribe(const QString &ticker, const Needs &n)
{
    const QString key = ticker.toUpper();
    State::Entry &e = m_state->streams[key];
    if (!e.stream) e.stream = new DepthStream(m_state->source, key, m_state->root);
    std::unique_ptr<Subscription> s(new Subscription);
    s->m_state = m_state;
    s->m_stream = e.stream;
    s->m_needs = n;
    e.subs.push_back(s.get());
    m_state->update(key);
    return s;
}

int DepthStreams::streamCount() const
{
    return int(m_state->streams.size());
}

DepthStream *DepthStreams::find(const QString &ticker) const
{
    const auto it = m_state->streams.constFind(ticker.toUpper());
    return it == m_state->streams.constEnd() ? nullptr : it->stream;
}

DepthStreams::Subscription::~Subscription()
{
    const std::shared_ptr<State> st = m_state.lock();
    if (!st || !m_stream) return;
    const QString key = m_stream->ticker();
    auto it = st->streams.find(key);
    if (it == st->streams.end()) return;
    auto &subs = it->subs;
    subs.erase(std::remove(subs.begin(), subs.end(), this), subs.end());
    if (subs.empty()) {
        // The last one: the stream stops now (its recording closed), and goes once whatever it is doing has returned.
        DepthStream *s = it->stream;
        st->streams.erase(it);
        s->setNeeds(DepthStream::Needs{});
        s->deleteLater();
    } else {
        st->update(key);
    }
}

void DepthStreams::Subscription::setNeeds(const Needs &n)
{
    m_needs = n;
    if (const auto st = m_state.lock(); st && m_stream) st->update(m_stream->ticker());
}

} // namespace qtm
