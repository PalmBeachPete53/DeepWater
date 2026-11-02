#pragma once

// The order book depth of the liquidity heatmap (a Deepwater addition, not in Quant Terminal), one feed per chart pane:
// Binance's diff depth stream (`<pair>@depth@100ms`) and REST snapshots (/api/v3/depth, 5000 levels a side), the book
// rebuilt and sampled (core/OrderBook, core/Heatmap) on a thread of its own, the columns handed to the UI thread where
// they go in the store the chart layer reads.
//
// Binance gives the book by price level (MBP), updated every 100 ms: an order placed and pulled within 100 ms is
// never seen. It has no history of the book: the heatmap starts with the feed and fills in live. A snapshot reaches
// 5000 levels from the price on each side; levels further away only show once they change.
//
// Like the trade feed, the exchange is the one the REST answer came from: binance.us is a separate exchange, so its
// stream is used when the history comes from there.

#include "../core/Heatmap.h"

#include <QObject>
#include <QString>

#include <chrono>
#include <cmath>
#include <deque>
#include <memory>

class QJsonArray;
class QThread;
class QTimer;
class QWebSocket;

namespace qtm {

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
    QString recordFile, recordNote;   // recordNote: why it isn't recording (another chart writes the same file)
    int loadedFiles = 0;
    size_t loadedColumns = 0;
    double loadMs = 0;
    bool loading = false;
};

class DepthWorker;
class DepthRecorder;

class DepthFeed : public QObject
{
    Q_OBJECT
public:
    struct Params {
        int intervalMs = 250;
        int historyMinutes = 120;
        int bandRows = 800;
        heatmap::Agg agg = heatmap::Agg::Max;   // the pyramid's (the first heatmap's zoom aggregation)
        int stableMs = 0;   // > 0: what is shown is only the liquidity that rested this long (heatmap::stableColumn)
        bool record = false;  // write what Binance sends to disk (core/DepthRecord), an hourly file per pair
        int keepDays = 7;     // recordings older than this are deleted when recording starts
    };
    DepthFeed(const QString &ticker, const Params &p, QObject *parent = nullptr);
    ~DepthFeed() override;

    void stop();     // the socket closed (the app hidden, replay): the book is lost, resume() resyncs
    void resume();
    void setHistoryMinutes(int minutes);   // the store keeps its newest columns
    void setAgg(heatmap::Agg agg);         // the pyramid is rebuilt from the store (~0.1 s for 2 hours)
    void setStableMs(int ms);              // the filtered store and its pyramid are rebuilt from the samples
    void setRecording(bool on, int keepDays);
    // Where this pair's recordings go: quant-terminal/data/depth/<PAIR> (binance.us: <PAIR>-US).
    static QString recordDir(const QString &ticker, bool us);

    const QString &ticker() const { return m_ticker; }
    const Params &params() const { return m_p; }
    // What the heatmap shows: the samples, or with the stability filter on, what rested long enough in them. Null until
    // Binance has said the tick size and the price (the storage row depends on both).
    const heatmap::ColumnStore *store() const { return m_stable ? m_stable.get() : m_store.get(); }
    quint64 storeId() const { return m_storeId; }   // unique across feeds: a new store or pyramid is a new id
    // The shown store reduced in time (null with the store).
    const heatmap::ColumnPyramid *pyramid() const { return m_pyramid.get(); }
    // The samples themselves (what pulled / absorbed liquidity is read from), and an id that changes with them only.
    const heatmap::ColumnStore *rawStore() const { return m_store.get(); }
    quint64 rawId() const { return m_rawId; }
    int stableSamples() const;   // 0 when the filter is off
    double lastDisplayRebuildMs() const { return m_displayRebuildMs; }
    // The shown store is being rebuilt in the background (a setting changed): the previous one stays on show.
    bool rebuildingDisplay() const { return m_rebuilding; }
    double tickSize() const { return m_tick; }
    int rowTicks() const { return m_rowTicks; }
    double rowPrice() const { return m_tick * m_rowTicks; }
    const DepthStats &stats() const { return m_stats; }
    QString error() const { return m_stats.error; }

    // When the newest column was made on the worker (steady clock, ms): how long it took to reach the screen.
    double newestMadeAtMs() const { return m_newestMadeAt; }
    static double steadyMs()
    {
        return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }

signals:
    void changed();        // new columns
    void stateChanged();   // synced / error / stats

private:
    friend class DepthWorker;
    void onConfigured(double tick, int rowTicks);
    void onColumns(std::vector<heatmap::Column> cols, double madeAt);
    void onStats(const DepthStats &s);
    size_t capacity() const;

    QString m_ticker;
    Params m_p;
    QThread *m_thread = nullptr;
    DepthWorker *m_worker = nullptr;
    std::unique_ptr<heatmap::ColumnStore> m_store;
    quint64 m_storeId = 0;
    std::unique_ptr<heatmap::ColumnPyramid> m_pyramid;
    std::unique_ptr<heatmap::ColumnStore> m_stable;
    quint64 m_rawId = 0;
    double m_displayRebuildMs = 0;
    std::vector<heatmap::Code> m_stableScratch;
    // The shown store (filtered or not) and its pyramid, from the samples: at once for a short history, otherwise on a
    // worker thread from a copy of the samples (2 hours filtered: ~0.45 s), the samples that came meanwhile added after.
    void rebuildPyramid();
    struct Display {
        std::unique_ptr<heatmap::ColumnStore> stable;   // null: the samples themselves are shown
        std::unique_ptr<heatmap::ColumnPyramid> pyramid;
        int k = 0;            // samples a level must rest (0: filter off)
        uint64_t end = 0;     // the samples' index it is built up to
        double ms = 0;
    };
    static std::shared_ptr<Display> buildDisplay(const heatmap::ColumnStore &raw, int intervalMs, size_t capacity,
                                                 heatmap::Agg agg, int k);
    static void addToDisplay(Display &d, const heatmap::ColumnStore &raw, uint64_t index, int intervalMs,
                             std::vector<heatmap::Code> &scratch);
    void install(Display &&d);
    int m_displayK = 0;
    bool m_rebuilding = false;
    quint64 m_rebuildGen = 0;
    void newStoreId();
    double m_tick = 0;
    int m_rowTicks = 0;
    DepthStats m_stats;
    double m_newestMadeAt = NAN;
};

// Lives on the feed's thread: the socket, the book, the sampler. Only DepthFeed talks to it.
class DepthWorker : public QObject
{
    Q_OBJECT
public:
    DepthWorker(DepthFeed *feed, const QString &ticker, const DepthFeed::Params &p);
    ~DepthWorker() override;
    void start();
    void stop();

private:
    void fetchConfig();
    void preload();   // the recordings of the history window, replayed into columns before going live
    void openSocket(int hostIdx);
public:
    void setRecording(bool on, int keepDays);
private:
    void ensureRecorder();
    double exchangeNow() const;
    void onMessage(const QByteArray &m);
    void requestSnapshot();
    void sampleTo(double exchangeTime);
    void emitStats();
    void setError(const QString &e);
    static std::vector<depth::Level> levels(const QJsonArray &a, double tick);

    DepthFeed *m_feed;   // the queued calls go to it on the UI thread (it waits for this thread before going)
    QString m_ticker;
    DepthFeed::Params m_p;
    double m_tick = 0;
    int m_rowTicks = 0;
    bool m_us = false, m_configured = false, m_stopped = false;
    depth::OrderBook m_book;
    std::unique_ptr<heatmap::Sampler> m_sampler;
    QWebSocket *m_ws = nullptr;
    QTimer *m_tick50 = nullptr;    // samples while no update comes
    QTimer *m_statsTimer = nullptr;
    QTimer *m_retry = nullptr;
    bool m_snapshotBusy = false, m_configBusy = false, m_preloaded = false;
    std::unique_ptr<DepthRecorder> m_rec;
    QString m_recNote;
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

} // namespace qtm
