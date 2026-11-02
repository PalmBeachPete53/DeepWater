// The liquidity heatmap's depth feed (see DepthFeed.h). A Deepwater addition.
#include "DepthFeed.h"

#include "../core/DepthRecord.h"
#include "../core/Error.h"
#include "../server/Server.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QSet>
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QWebSocket>
#include <QtConcurrent>

#include <algorithm>
#include <atomic>
#include <ctime>

namespace qtm {

namespace {
// The same stream hosts as the trade feed.
const QStringList WS_HOSTS = {"wss://stream.binance.com:9443/ws/", "wss://data-stream.binance.vision/ws/"};
const QStringList WS_HOSTS_US = {"wss://stream.binance.us:9443/ws/"};
constexpr int SNAPSHOT_LEVELS = 5000;     // Binance's most (request weight 250 of the 6000 a minute)
constexpr double SNAPSHOT_GAP_MS = 3000;  // between two snapshot requests, whatever happens
constexpr int RETRY_MS = 5000;
// The timer samples this far behind the exchange's clock (as estimated from the updates): an update stamped before
// a sample time has arrived by then, so the column has it.
constexpr double TIMER_LAG_MS = 300;

// A blocking backend call on the thread pool, its answer back on `ctx`'s thread (api::run answers on the UI thread).
// Errors come back as {"error": message}.
void runOff(QObject *ctx, std::function<QJsonObject()> work, std::function<void(const QJsonObject &)> done)
{
    auto *w = new QFutureWatcher<QJsonObject>(ctx);
    QObject::connect(w, &QFutureWatcherBase::finished, ctx, [w, done] {
        done(w->result());
        w->deleteLater();
    });
    w->setFuture(QtConcurrent::run([work] {
        try {
            return work();
        } catch (const Error &e) {
            return QJsonObject{{"error", e.message()}};
        } catch (const std::exception &e) {
            return QJsonObject{{"error", QString::fromUtf8(e.what())}};
        }
    }));
}

// Files being written by a feed of this app: two charts of the same pair mustn't append to the same file.
QMutex g_recMutex;
QSet<QString> g_recFiles;

double threadCpuMs()
{
    timespec ts{};
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
    return double(ts.tv_sec) * 1e3 + double(ts.tv_nsec) / 1e6;
}
double num(const QJsonValue &v) { return v.isString() ? v.toString().toDouble() : v.toDouble(); }
} // namespace

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
            if (g_recFiles.contains(name)) {   // another chart of this pair writes it
                m_note = "another chart records this pair";
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

// ---------------------------------------------------------------- the feed (UI thread)

DepthFeed::DepthFeed(const QString &ticker, const Params &p, QObject *parent)
    : QObject(parent), m_ticker(ticker.toUpper()), m_p(p)
{
    static const int reg = [] {
        qRegisterMetaType<DepthStats>("qtm::DepthStats");
        return 0;
    }();
    (void)reg;
    m_thread = new QThread;
    m_thread->setObjectName("depth " + m_ticker);
    m_worker = new DepthWorker(this, m_ticker, m_p);
    m_worker->moveToThread(m_thread);
    m_thread->start();
    QMetaObject::invokeMethod(m_worker, &DepthWorker::start, Qt::QueuedConnection);
}

DepthFeed::~DepthFeed()
{
    // The worker goes on its own thread (its socket and timers live there), then the thread.
    DepthWorker *w = m_worker;
    QMetaObject::invokeMethod(m_worker, [w] { delete w; }, Qt::BlockingQueuedConnection);
    m_thread->quit();
    m_thread->wait();
    delete m_thread;
}

void DepthFeed::stop() { QMetaObject::invokeMethod(m_worker, &DepthWorker::stop, Qt::QueuedConnection); }

QString DepthFeed::recordDir(const QString &ticker, bool us)
{
    Backend *b = Backend::instance();
    return (b ? b->dataDir() : QDir::tempPath()) + "/depth/" + ticker.toUpper() + (us ? "-US" : "");
}

void DepthFeed::setRecording(bool on, int keepDays)
{
    if (on == m_p.record && keepDays == m_p.keepDays) return;
    m_p.record = on;
    m_p.keepDays = keepDays;
    DepthWorker *w = m_worker;
    QMetaObject::invokeMethod(w, [w, on, keepDays] { w->setRecording(on, keepDays); }, Qt::QueuedConnection);
}
void DepthFeed::resume() { QMetaObject::invokeMethod(m_worker, &DepthWorker::start, Qt::QueuedConnection); }

size_t DepthFeed::capacity() const
{
    return size_t(std::max(1.0, double(m_p.historyMinutes) * 60000.0 / m_p.intervalMs));
}

void DepthFeed::setHistoryMinutes(int minutes)
{
    m_p.historyMinutes = std::max(1, minutes);
    if (m_store) m_store->setCapacity(capacity());
    if (m_stable) m_stable->setCapacity(capacity());
    if (m_pyramid) m_pyramid->setBaseCapacity(capacity());
}

void DepthFeed::onConfigured(double tick, int rowTicks)
{
    if (m_store && m_tick == tick && m_rowTicks == rowTicks) return;
    m_tick = tick;
    m_rowTicks = rowTicks;
    m_store = std::make_unique<heatmap::ColumnStore>(m_p.bandRows, capacity());
    static std::atomic<quint64> rawIds{0};
    m_rawId = ++rawIds;
    rebuildPyramid();
    emit stateChanged();
}

void DepthFeed::newStoreId()
{
    static std::atomic<quint64> ids{0};
    m_storeId = ++ids;
}

int DepthFeed::stableSamples() const
{
    return m_p.stableMs > 0 ? std::max(2, int(std::lround(double(m_p.stableMs) / m_p.intervalMs)) + 1) : 0;
}

void DepthFeed::addToDisplay(Display &d, const heatmap::ColumnStore &raw, uint64_t index, int intervalMs,
                             std::vector<heatmap::Code> &scratch)
{
    if (d.stable) {
        heatmap::stableColumn(raw, index, d.k, intervalMs, scratch);
        d.stable->append(raw.meta(index), scratch.data());
        d.pyramid->add(raw.meta(index), scratch.data());
    } else {
        d.pyramid->add(raw.meta(index), raw.rows(index));
    }
    d.end = index + 1;
}

std::shared_ptr<DepthFeed::Display> DepthFeed::buildDisplay(const heatmap::ColumnStore &raw, int intervalMs, size_t capacity,
                                                            heatmap::Agg agg, int k)
{
    QElapsedTimer t;
    t.start();
    auto d = std::make_shared<Display>();
    d->k = k;
    d->pyramid = std::make_unique<heatmap::ColumnPyramid>(raw.bandRows(), intervalMs, capacity, agg);
    if (k) d->stable = std::make_unique<heatmap::ColumnStore>(raw.bandRows(), capacity);
    std::vector<heatmap::Code> scratch;
    d->end = raw.firstIndex();
    for (uint64_t i = raw.firstIndex(); i < raw.endIndex(); ++i) addToDisplay(*d, raw, i, intervalMs, scratch);
    d->end = raw.endIndex();
    d->ms = double(t.nsecsElapsed()) / 1e6;
    return d;
}

void DepthFeed::install(Display &&d)
{
    m_stable = std::move(d.stable);
    m_pyramid = std::move(d.pyramid);
    m_displayK = d.k;
    m_displayRebuildMs = d.ms;
    if (m_stable) m_stable->setCapacity(capacity());   // the history may have changed meanwhile
    m_pyramid->setBaseCapacity(capacity());
    newStoreId();
}

void DepthFeed::rebuildPyramid()
{
    if (!m_store) return;
    const quint64 gen = ++m_rebuildGen;
    const int k = stableSamples(), iv = m_p.intervalMs;
    const size_t cap = capacity();
    const heatmap::Agg agg = m_p.agg;
    if (m_store->size() < 2000) {   // ~10 ms at most: at once
        m_rebuilding = false;
        install(std::move(*buildDisplay(*m_store, iv, cap, agg, k)));
        return;
    }
    // A copy of the samples (~10 ms for 2 hours) for the worker; the samples keep coming in meanwhile.
    auto copy = std::make_shared<const heatmap::ColumnStore>(*m_store);
    m_rebuilding = true;
    auto *w = new QFutureWatcher<std::shared_ptr<Display>>(this);
    connect(w, &QFutureWatcherBase::finished, this, [this, w, gen, iv] {
        std::shared_ptr<Display> d = w->result();
        w->deleteLater();
        if (gen != m_rebuildGen || !d || !m_store) return;   // another setting came meanwhile: its rebuild wins
        // The samples that came during the rebuild (the oldest may have gone from the ring meanwhile).
        for (uint64_t i = std::max(d->end, m_store->firstIndex()); i < m_store->endIndex(); ++i)
            addToDisplay(*d, *m_store, i, iv, m_stableScratch);
        install(std::move(*d));
        m_rebuilding = false;
        emit changed();
    });
    w->setFuture(QtConcurrent::run([copy, iv, cap, agg, k] { return buildDisplay(*copy, iv, cap, agg, k); }));
}

void DepthFeed::setStableMs(int ms)
{
    ms = std::max(0, ms);
    if (ms == m_p.stableMs) return;
    m_p.stableMs = ms;
    rebuildPyramid();
    emit changed();
}

void DepthFeed::setAgg(heatmap::Agg agg)
{
    if (agg == m_p.agg) return;
    m_p.agg = agg;
    rebuildPyramid();
    emit changed();
}


void DepthFeed::onColumns(std::vector<heatmap::Column> cols, double madeAt)
{
    if (!m_store) return;
    bool any = false;
    const int k = m_displayK;   // the shown store's (a rebuild for another setting may be under way)
    for (const heatmap::Column &c : cols)
        if (m_store->append(c)) {
            if (m_stable) {
                heatmap::stableColumn(*m_store, m_store->endIndex() - 1, k, m_p.intervalMs, m_stableScratch);
                m_stable->append(c.meta, m_stableScratch.data());
                m_pyramid->add(c.meta, m_stableScratch.data());
            } else {
                m_pyramid->add(c.meta, c.rows.data());
            }
            any = true;
        }
    if (!any) return;
    m_newestMadeAt = madeAt;
    emit changed();
}

void DepthFeed::onStats(const DepthStats &s)
{
    m_stats = s;
    emit stateChanged();
}

// ---------------------------------------------------------------- the worker (its own thread)

DepthWorker::DepthWorker(DepthFeed *feed, const QString &ticker, const DepthFeed::Params &p)
    : m_feed(feed), m_ticker(ticker), m_p(p)
{
}

DepthWorker::~DepthWorker()
{
    stop();
}

void DepthWorker::start()
{
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
        connect(m_statsTimer, &QTimer::timeout, this, &DepthWorker::emitStats);
        m_retry = new QTimer(this);
        m_retry->setSingleShot(true);
        m_cpuAt = threadCpuMs();
        m_cpuWall = DepthFeed::steadyMs();
    }
    if (!m_stopped && (m_ws || m_tick50->isActive())) return;
    m_stopped = false;
    m_tick50->start();
    m_statsTimer->start();
    if (!m_configured)
        fetchConfig();   // then the recordings, then the socket
    else if (m_preloaded)
        openSocket(0);
}

double DepthWorker::exchangeNow() const
{
    const double now = double(QDateTime::currentMSecsSinceEpoch());
    return (std::isnan(m_offsetMs) ? now : now + m_offsetMs) / 1000;
}

void DepthWorker::setRecording(bool on, int keepDays)
{
    m_p.record = on;
    m_p.keepDays = keepDays;
    if (!on) m_rec.reset();
    else ensureRecorder();
}

void DepthWorker::ensureRecorder()
{
    if (!m_p.record || m_rec || !m_configured) return;
    const QString dir = DepthFeed::recordDir(m_ticker, m_us);
    DepthRecorder::prune(dir, std::max(1, m_p.keepDays));
    m_rec = std::make_unique<DepthRecorder>(dir, depthrec::Header{m_ticker, m_tick, m_us});
}

// The recordings that reach into the history window, replayed in order through a book and a sampler of their own (as
// the live feed would have drawn them), the columns handed over in batches. A file of another tick size or exchange is
// skipped. Holes between sessions stay holes.
void DepthWorker::preload()
{
    m_preloaded = true;
    QElapsedTimer t;
    t.start();
    const QString dir = DepthFeed::recordDir(m_ticker, m_us);
    const double now = double(QDateTime::currentMSecsSinceEpoch()) / 1000;
    const double from = now - double(m_p.historyMinutes) * 60;
    QStringList names = QDir(dir).entryList({"*.dwdepth"}, QDir::Files, QDir::Name);
    heatmap::SamplerParams sp = m_sampler->params();
    depthrec::Replayer rp(sp);
    DepthFeed *feed = m_feed;
    auto hand = [&](bool all) {
        std::vector<heatmap::Column> cols = rp.take();
        cols.erase(std::remove_if(cols.begin(), cols.end(), [from](const heatmap::Column &c) { return c.meta.time < from; }),
                   cols.end());
        if (cols.empty()) return;
        m_loadedColumns += cols.size();
        QMetaObject::invokeMethod(feed, [feed, cols = std::move(cols)]() mutable { feed->onColumns(std::move(cols), NAN); },
                                  Qt::QueuedConnection);
        (void)all;
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
        if (!r.ok() || r.header().ticker != m_ticker || r.header().us != m_us || std::fabs(r.header().tickSize - m_tick) > 1e-12)
            continue;
        ++m_loadedFiles;
        depthrec::Record rec;
        int k = 0;
        while (r.next(rec)) {
            rp.feed(rec);
            if (++k % 20000 == 0) hand(false);
        }
        rp.feed(depthrec::Record{depthrec::Type::Lost, rec.time, {}, {}});   // a file's end: what follows is a hole
        hand(true);
    }
    m_loadMs = double(t.nsecsElapsed()) / 1e6;
    emitStats();
}

void DepthWorker::stop()
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
    if (m_ws) {
        QWebSocket *s = m_ws;
        m_ws = nullptr;
        s->close();
    }
    m_book.reset();
    if (m_sampler) m_sampler->reset();
    m_offsetMs = NAN;
}

void DepthWorker::setError(const QString &e)
{
    if (m_error == e) return;
    m_error = e;
    emitStats();
}

// The tick size, and which exchange answers with the book (a 5-level snapshot, weight 5): the storage row depends on
// the price, and the stream must be that exchange's.
void DepthWorker::fetchConfig()
{
    if (m_configBusy) return;
    m_configBusy = true;
    const QString ticker = m_ticker;
    runOff(this,
        [ticker] {
            Backend *b = Backend::instance();
            if (!b) throw Error("The local server isn't running");
            const double tick = b->binanceTickSize(ticker);
            bool us = false;
            const QJsonObject d = b->binanceDepth(ticker, 5, &us);
            const double bid = num(d.value("bids").toArray().first().toArray().first());
            const double ask = num(d.value("asks").toArray().first().toArray().first());
            return QJsonObject{{"tick", tick}, {"us", us}, {"price", bid > 0 && ask > 0 ? (bid + ask) / 2 : std::max(bid, ask)}};
        },
        [this](const QJsonObject &r) {
            m_configBusy = false;
            if (r.contains("error")) {   // tried again even when stopped: the recordings need it too (replay)
                setError(r.value("error").toString());
                QTimer::singleShot(RETRY_MS, this, [this] { fetchConfig(); });
                return;
            }
            m_tick = r.value("tick").toDouble();
            m_us = r.value("us").toBool();
            m_rowTicks = heatmap::autoStorageRowTicks(r.value("price").toDouble(), m_tick);
            heatmap::SamplerParams sp;
            sp.intervalMs = m_p.intervalMs;
            sp.rowTicks = m_rowTicks;
            sp.bandRows = m_p.bandRows;
            m_sampler = std::make_unique<heatmap::Sampler>(sp);
            m_configured = true;
            DepthFeed *feed = m_feed;
            const double tick = m_tick;
            const int rows = m_rowTicks;
            QMetaObject::invokeMethod(feed, [feed, tick, rows] { feed->onConfigured(tick, rows); }, Qt::QueuedConnection);
            setError(QString());
            preload();
            ensureRecorder();
            if (!m_stopped) openSocket(0);
        });
}

void DepthWorker::openSocket(int hostIdx)
{
    if (m_stopped) return;
    const QStringList &hosts = m_us ? WS_HOSTS_US : WS_HOSTS;
    if (hostIdx >= hosts.size()) {
        setError("Binance's depth stream isn't reachable (trying again)");
        m_retry->disconnect();
        connect(m_retry, &QTimer::timeout, this, [this] { openSocket(0); });
        m_retry->start(RETRY_MS);
        return;
    }
    auto *sock = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    m_ws = sock;
    auto opened = std::make_shared<bool>(false);
    auto closed = std::make_shared<bool>(false);
    connect(sock, &QWebSocket::connected, this, [opened] { *opened = true; });
    connect(sock, &QWebSocket::textMessageReceived, this, [this, sock](const QString &m) {
        if (m_ws == sock) onMessage(m.toUtf8());
    });
    auto onClose = [this, sock, opened, closed, hostIdx] {
        if (*closed) return;
        *closed = true;
        sock->deleteLater();
        if (m_ws != sock) return;
        m_ws = nullptr;
        if (m_rec && m_rec->at(exchangeNow(), m_book)) m_rec->writer().lost(exchangeNow());
        m_book.desync();   // updates are missed from now on: the next ones wait for a snapshot
        if (m_stopped) return;
        if (!*opened) {
            openSocket(hostIdx + 1);
        } else {
            m_retry->disconnect();
            connect(m_retry, &QTimer::timeout, this, [this] { openSocket(0); });
            m_retry->start(3000);
        }
    };
    connect(sock, &QWebSocket::disconnected, this, onClose);
    connect(sock, &QWebSocket::errorOccurred, this, [sock, onClose](QAbstractSocket::SocketError) {
        if (sock->state() == QAbstractSocket::UnconnectedState) onClose();
    });
    sock->open(QUrl(hosts[hostIdx] + m_ticker.toLower() + "@depth@100ms"));
}

std::vector<depth::Level> DepthWorker::levels(const QJsonArray &a, double tick)
{
    std::vector<depth::Level> out;
    out.reserve(size_t(a.size()));
    for (const QJsonValue v : a) {
        const QJsonArray l = v.toArray();
        out.push_back({std::llround(num(l.at(0)) / tick), num(l.at(1))});
    }
    return out;
}

void DepthWorker::onMessage(const QByteArray &m)
{
    if (!m_sampler) return;
    const double recv = double(QDateTime::currentMSecsSinceEpoch());
    const QJsonObject o = QJsonDocument::fromJson(m).object();
    if (o.value("e").toString() != "depthUpdate") return;
    depth::DepthUpdate u;
    u.firstId = o.value("U").toInteger();
    u.lastId = o.value("u").toInteger();
    if (o.contains("pu")) u.prevLastId = o.value("pu").toInteger();
    const double eventMs = o.value("E").toDouble();
    u.time = eventMs / 1000;
    u.bids = levels(o.value("b").toArray(), m_tick);
    u.asks = levels(o.value("a").toArray(), m_tick);
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

void DepthWorker::requestSnapshot()
{
    if (m_snapshotBusy || m_stopped) return;
    m_snapshotBusy = true;
    const double wait = std::max(0.0, m_lastSnapshotAt + SNAPSHOT_GAP_MS - DepthFeed::steadyMs());
    const int token = m_token;
    QTimer::singleShot(int(wait), this, [this, token] {
        if (token != m_token) return;
        m_lastSnapshotAt = DepthFeed::steadyMs();
        const QString ticker = m_ticker;
        runOff(this,
            [ticker] {
                Backend *b = Backend::instance();
                if (!b) throw Error("The local server isn't running");
                bool us = false;
                QJsonObject d = b->binanceDepth(ticker, SNAPSHOT_LEVELS, &us);
                d.insert("_us", us);
                return d;
            },
            [this, token](const QJsonObject &d) {
                if (token != m_token) return;
                m_snapshotBusy = false;
                if (d.contains("error")) {
                    setError(d.value("error").toString());
                    requestSnapshot();   // after the gap
                    return;
                }
                if (d.value("_us").toBool() != m_us) {
                    setError("Binance answered from another exchange (trying again)");
                    requestSnapshot();
                    return;
                }
                depth::DepthSnapshot s;
                s.lastUpdateId = d.value("lastUpdateId").toInteger();
                s.bids = levels(d.value("bids").toArray(), m_tick);
                s.asks = levels(d.value("asks").toArray(), m_tick);
                // How far the snapshot reaches: levels beyond only show once they change.
                m_snapLow = s.bids.empty() ? NAN : double(std::min_element(s.bids.begin(), s.bids.end(), [](auto &a, auto &b) {
                                                       return a.tick < b.tick;
                                                   })->tick) * m_tick;
                m_snapHigh = s.asks.empty() ? NAN : double(std::max_element(s.asks.begin(), s.asks.end(), [](auto &a, auto &b) {
                                                        return a.tick < b.tick;
                                                    })->tick) * m_tick;
                if (m_rec && m_rec->at(exchangeNow(), m_book)) m_rec->writer().snapshot(exchangeNow(), s);
                const depth::OrderBook::Result r = m_book.setSnapshot(s);
                if (r == depth::OrderBook::Result::Applied) {
                    setError(QString());
                } else {
                    // Older than the stream (it was buffering while the request ran), or crossed: again.
                    requestSnapshot();
                }
                emitStats();
            });
    });
}

void DepthWorker::sampleTo(double exchangeTime)
{
    if (!m_sampler) return;
    std::vector<heatmap::Column> cols = m_sampler->advance(m_book, exchangeTime);
    if (cols.empty()) return;
    DepthFeed *feed = m_feed;
    const double madeAt = DepthFeed::steadyMs();
    QMetaObject::invokeMethod(
        feed, [feed, cols = std::move(cols), madeAt]() mutable { feed->onColumns(std::move(cols), madeAt); },
        Qt::QueuedConnection);
}

void DepthWorker::emitStats()
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
    const double wall = DepthFeed::steadyMs(), cpu = threadCpuMs();
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
    DepthFeed *feed = m_feed;
    QMetaObject::invokeMethod(feed, [feed, s] { feed->onStats(s); }, Qt::QueuedConnection);
}

} // namespace qtm
