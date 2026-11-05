#pragma once

// The order book depth of one Binance pair, shared by every chart pane whose liquidity heatmap shows it (a Deepwater
// addition, not in Quant Terminal): Binance's diff depth stream (`<pair>@depth@100ms`) and REST snapshots
// (/api/v3/depth, 5000 levels a side), the book rebuilt (core/OrderBook) and sampled into columns (core/Heatmap) on a
// thread of its own, the recordings written and read back (core/DepthRecord).
//
// One stream per pair, whatever the number of panes: DepthStreams::subscribe() hands out subscriptions, each with its
// pane's needs (its sampling interval and history, recording, running or paused). The pair has one socket, one book,
// one recorder; each interval asked for has one sampler and one store of columns, shared by the panes sampling at it
// and kept as long as the longest history among them. What each pane makes of the columns (its stability filter, its
// time pyramid) is ui/DepthFeed.
//
// Binance gives the book by price level (MBP), updated every 100 ms: an order placed and pulled within 100 ms is
// never seen. It has no history of the book: the heatmap starts with the stream (and the recordings) and fills in
// live. A snapshot reaches 5000 levels from the price on each side; levels further away only show once they change.
// The exchange is the one the REST answer came from: binance.us is a separate exchange, so its stream is used when
// the snapshot comes from there.
//
// The network is behind DepthSource: Binance's (BinanceDepthSource, through Backend) or a test's.

#include "../core/Heatmap.h"
#include "../core/OrderBook.h"

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QString>

#include <chrono>
#include <cmath>
#include <functional>
#include <map>
#include <memory>

class QThread;

namespace qtm {

class Backend;
class DepthStreamWorker;

// What the worker says every half second, for the legend and the debug overlay.
struct DepthStats {
    bool synced = false;
    bool us = false;
    QString error;
    size_t levels = 0;                 // in the book
    uint64_t applied = 0, gaps = 0, crossed = 0, snapshots = 0;
    double updatesPerSec = 0, levelsPerSec = 0;
    double netLatencyMs = NAN;         // local receipt − exchange event time (the two clocks' offset included)
    double threadCpu = 0;              // the worker thread's CPU, % of one core
    double snapshotLow = NAN, snapshotHigh = NAN;   // the prices the last snapshot reached (5000 levels a side)
    // Recordings: being written (this session's bytes, the file), and what was loaded from them at the start.
    bool recording = false;
    qint64 recordedBytes = 0;
    QString recordFile, recordNote;   // recordNote: why it isn't recording
    int loadedFiles = 0;
    size_t loadedColumns = 0;
    double loadMs = 0;
    bool loading = false;
};

// Where the depth comes from. Every call is made on the stream's worker thread, and the answers come back to it.
class DepthSource
{
public:
    using Fail = std::function<void(const QString &error)>;
    struct Config {
        double tick = 0;     // the price step
        bool us = false;     // answered by binance.us
        double price = 0;    // about where it trades (the storage rows depend on it)
    };
    struct Snapshot {
        depth::DepthSnapshot book;
        bool us = false;
    };
    using Live = QObject;   // the live updates; deleting it closes them, and nothing is called after
    virtual ~DepthSource() = default;
    // The answers arrive later, on `context`'s thread, and only while `context` exists.
    virtual void config(QObject *context, const QString &ticker, std::function<void(const Config &)> ok, Fail fail) = 0;
    virtual void snapshot(QObject *context, const QString &ticker, double tick, int levels,
                          std::function<void(const Snapshot &)> ok, Fail fail) = 0;
    // `update(u, receivedMs)` for each diff (prices in ticks of `tick`); `closed(opened)` once, when it gives up: the
    // connection ended (opened) or none could be made (!opened).
    virtual Live *live(QObject *parent, const QString &ticker, bool us, double tick,
                       std::function<void(const depth::DepthUpdate &, double receivedMs)> update,
                       std::function<void(bool opened)> closed) = 0;
};

class BinanceDepthSource : public DepthSource
{
public:
    explicit BinanceDepthSource(Backend *backend) : m_backend(backend) {}
    void config(QObject *context, const QString &ticker, std::function<void(const Config &)> ok, Fail fail) override;
    void snapshot(QObject *context, const QString &ticker, double tick, int levels,
                  std::function<void(const Snapshot &)> ok, Fail fail) override;
    Live *live(QObject *parent, const QString &ticker, bool us, double tick,
               std::function<void(const depth::DepthUpdate &, double)> update,
               std::function<void(bool)> closed) override;

private:
    Backend *m_backend;
};

class DepthStream : public QObject
{
    Q_OBJECT
public:
    static constexpr int BAND_ROWS = 800;   // rows per column around the price

    struct Needs {
        std::map<int, int> history;   // interval (ms) → minutes of columns to keep
        bool record = false;          // write what Binance sends to disk, an hourly file per pair
        int keepDays = 7;             // recordings older than this are deleted when recording starts
        bool active = false;          // the live stream runs
    };

    // `recordRoot`: the data folder; recordings go to <recordRoot>/depth/<PAIR> (binance.us: <PAIR>-US).
    DepthStream(std::shared_ptr<DepthSource> source, const QString &ticker, const QString &recordRoot,
                QObject *parent = nullptr);
    ~DepthStream() override;

    const QString &ticker() const { return m_ticker; }
    // Null until Binance has said the tick size and the price (the storage row depends on both), and for an interval
    // no pane samples at.
    const heatmap::ColumnStore *store(int intervalMs) const;
    quint64 rawId(int intervalMs) const;            // unique across stores: a new store is a new id
    double newestMadeAt(int intervalMs) const;      // when its newest column was made on the worker (steady ms)
    double tickSize() const { return m_tick; }
    int rowTicks() const { return m_rowTicks; }
    const DepthStats &stats() const { return m_stats; }
    bool running() const { return m_needs.active; }
    const Needs &needs() const { return m_needs; }
    void setNeeds(const Needs &n);   // DepthStreams sets it from the subscriptions

    static QString recordDir(const QString &recordRoot, const QString &ticker, bool us);
    static double steadyMs()
    {
        return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }

signals:
    void columns(int intervalMs);   // new columns in that interval's store
    void configured();              // the stores were made (tick size and rows known)
    void stateChanged();            // synced / error / stats

private:
    friend class DepthStreamWorker;
    void onConfigured(double tick, int rowTicks);
    void onColumns(int intervalMs, std::vector<heatmap::Column> cols, double madeAt);
    void onStats(const DepthStats &s);
    size_t capacity(int intervalMs, int minutes) const;
    void makeStore(int intervalMs);

    struct Channel {
        int minutes = 0;
        std::unique_ptr<heatmap::ColumnStore> store;
        quint64 rawId = 0;
        double madeAt = NAN;
    };
    QString m_ticker;
    Needs m_needs;
    std::map<int, Channel> m_channels;
    QThread *m_thread = nullptr;
    DepthStreamWorker *m_worker = nullptr;
    double m_tick = 0;
    int m_rowTicks = 0;
    DepthStats m_stats;
};

// The streams, one per pair, and who uses them.
class DepthStreams
{
    struct State;

public:
    DepthStreams(std::shared_ptr<DepthSource> source, const QString &recordRoot);
    ~DepthStreams();

    struct Needs {
        int intervalMs = 250;
        int historyMinutes = 120;
        bool record = false;
        int keepDays = 7;
        bool active = true;
    };
    // A pane's hold on its pair's stream. Deleting it lets the stream go when no one else uses it.
    class Subscription
    {
    public:
        ~Subscription();
        DepthStream *stream() const { return m_stream; }   // null once the streams are gone (the app closing)
        const Needs &needs() const { return m_needs; }
        void setNeeds(const Needs &n);

    private:
        friend class DepthStreams;
        Subscription() = default;
        std::weak_ptr<State> m_state;
        QPointer<DepthStream> m_stream;
        Needs m_needs;
    };

    std::unique_ptr<Subscription> subscribe(const QString &ticker, const Needs &n);
    int streamCount() const;
    DepthStream *find(const QString &ticker) const;

private:
    std::shared_ptr<State> m_state;
};

} // namespace qtm
