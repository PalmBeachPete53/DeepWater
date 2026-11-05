#pragma once

// A chart pane's order book depth, for its liquidity heatmap (a Deepwater addition, not in Quant Terminal). The depth
// comes from the backend's stream for the pair (server/DepthStream), shared with every other pane on that pair: one
// socket, one book, one recorder, one store of columns per sampling interval. This is the pane's side of it, with no
// network or thread of its own: what the heatmap shows of the columns (the stability filter's store and the time
// pyramid, for the pane's history and zoom aggregation), rebuilt in the background when a setting changes.

#include "../core/Heatmap.h"
#include "../server/DepthStream.h"

#include <QObject>
#include <QString>

#include <cmath>
#include <memory>

namespace qtm {

class DepthFeed : public QObject
{
    Q_OBJECT
public:
    struct Params {
        int intervalMs = 250;
        int historyMinutes = 120;
        heatmap::Agg agg = heatmap::Agg::Max;   // the pyramid's (the first heatmap's zoom aggregation)
        int stableMs = 0;   // > 0: what is shown is only the liquidity that rested this long (heatmap::stableColumn)
        bool record = false;  // write what Binance sends to disk (core/DepthRecord), an hourly file per pair
        int keepDays = 7;     // recordings older than this are deleted when recording starts
    };
    // `streams`: the backend's (Backend::depthStreams()) unless a test gives its own.
    DepthFeed(const QString &ticker, const Params &p, QObject *parent = nullptr, DepthStreams *streams = nullptr);
    ~DepthFeed() override;

    void stop();     // the pane stops the live stream (the app hidden, replay): it stops when no pane runs it
    void resume();
    void setHistoryMinutes(int minutes);   // the shown store keeps its newest columns
    void setAgg(heatmap::Agg agg);         // the pyramid is rebuilt from the samples (~0.1 s for 2 hours)
    void setStableMs(int ms);              // the filtered store and its pyramid are rebuilt from the samples
    void setRecording(bool on, int keepDays);

    const QString &ticker() const { return m_ticker; }
    const Params &params() const { return m_p; }
    // What the heatmap shows: the samples, or with the stability filter on, what rested long enough in them. Null until
    // Binance has said the tick size and the price (the storage row depends on both).
    const heatmap::ColumnStore *store() const { return m_stable ? m_stable.get() : rawStore(); }
    quint64 storeId() const { return m_storeId; }   // unique across feeds: a new store or pyramid is a new id
    // The shown store reduced in time (null with the store).
    const heatmap::ColumnPyramid *pyramid() const { return rawStore() ? m_pyramid.get() : nullptr; }
    // The samples themselves, shared with the pair's other panes at this interval (what pulled / absorbed liquidity is
    // read from), and an id that changes with them only. They reach back as far as the longest history among them.
    const heatmap::ColumnStore *rawStore() const;
    quint64 rawId() const;
    int stableSamples() const;   // 0 when the filter is off
    double lastDisplayRebuildMs() const { return m_displayRebuildMs; }
    // The shown store is being rebuilt in the background (a setting changed): the previous one stays on show.
    bool rebuildingDisplay() const { return m_rebuilding; }
    double tickSize() const;
    int rowTicks() const;
    double rowPrice() const { return tickSize() * rowTicks(); }
    const DepthStats &stats() const;
    QString error() const { return stats().error; }

    // When the newest column was made on the stream's thread (steady clock, ms): how long it took to reach the screen.
    double newestMadeAtMs() const;
    static double steadyMs() { return DepthStream::steadyMs(); }

signals:
    void changed();        // new columns
    void stateChanged();   // synced / error / stats

private:
    DepthStream *stream() const { return m_sub ? m_sub->stream() : nullptr; }
    void subscribeNeeds();
    void onColumns(int intervalMs);
    void checkStore();   // a new store of samples (the stream configured): the display is built again
    size_t capacity() const;

    QString m_ticker;
    Params m_p;
    bool m_active = true;
    std::unique_ptr<DepthStreams::Subscription> m_sub;
    quint64 m_storeId = 0;
    quint64 m_seenRawId = 0;
    std::unique_ptr<heatmap::ColumnPyramid> m_pyramid;
    std::unique_ptr<heatmap::ColumnStore> m_stable;
    uint64_t m_displayEnd = 0;   // the samples' index the display is built up to
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
};

} // namespace qtm
