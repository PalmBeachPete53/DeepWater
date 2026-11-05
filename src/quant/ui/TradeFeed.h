#pragma once

// A chart pane's trades, for its footprint, its Big Trades and its heatmap's bubbles and events (a Deepwater addition,
// not in Quant Terminal). The trades come from the backend's stream for the pair (server/TradeStream), shared with every
// other pane on that pair: this is the pane's side of it, with no network of its own. It builds the pane's footprint
// candles from the stream's tape (from the pane's first bar, up to its budget of trades, newest first) and shows the
// readers the stretch of the tape the pane has, so each pane sees what it would have fetched alone.

#include "../core/Footprint.h"
#include "../server/TradeStream.h"

#include <QObject>
#include <QString>

#include <deque>
#include <memory>

namespace qtm {

class TradeFeed : public QObject
{
    Q_OBJECT
public:
    using TapeTrade = qtm::TapeTrade;

    // The pane's stretch of the shared tape, in id (= time) order: valid until the stream next changes.
    class Tape
    {
    public:
        using const_iterator = std::deque<TapeTrade>::const_iterator;
        Tape() = default;
        Tape(const_iterator b, const_iterator e) : m_b(b), m_e(e) {}
        const_iterator begin() const { return m_b; }
        const_iterator end() const { return m_e; }
        size_t size() const { return size_t(m_e - m_b); }
        bool empty() const { return m_b == m_e; }
        const TapeTrade &front() const { return *m_b; }
        const TapeTrade &back() const { return *(m_e - 1); }
        const TapeTrade &operator[](size_t i) const { return m_b[std::ptrdiff_t(i)]; }

    private:
        const_iterator m_b{}, m_e{};
    };

    // `ticker` is Binance's pair (BTCUSDT), `tfSeconds` the chart's candle length; trades before `chartStart` (the
    // chart's first bar) aren't taken. `streams`: the backend's (Backend::tradeStreams()) unless a test gives its own.
    TradeFeed(const QString &ticker, double tfSeconds, double chartStart, int budget, QObject *parent = nullptr,
              TradeStreams *streams = nullptr);
    ~TradeFeed() override;

    void stop();     // the pane stops taking trades (the app hidden, replay): resume() catches up
    void resume();
    void setRowTicks(int rowTicks) { m_book.setRowTicks(rowTicks); }
    // The chart dropped its oldest bars: the footprints go with them.
    void setChartStart(double t);

    const footprint::Book &book() const { return m_book; }
    // Every trade the pane has, in id (= time) order with no gaps: what Big Trades and the heatmap's events read. The
    // history goes in at the front, page by page, live trades at the back. A new generation means it was emptied and
    // filled again (a new feed, or a gap longer than the budget): whoever read it starts over.
    Tape tape() const;
    quint64 tapeGeneration() const { return m_tapeGen; }
    bool ready() const { return m_hi >= 0; }
    double tickSize() const { return m_book.grid().tickSize; }
    // Candles from this open time on have every trade; the ones before are partial (where the history stopped).
    double completeFrom() const;
    bool loadingHistory() const;
    QString error() const;

signals:
    void changed();         // trades went in (the chart redraws, at most a few times a second)
    void stateChanged();    // ready / history done / error: the legend line

private:
    TradeStream *stream() const { return m_sub ? m_sub->stream() : nullptr; }
    bool covered() const;   // the stream's history reaches back to the pane's first bar
    bool take();            // the stream's trades the pane doesn't have yet; true when any went in or out
    void restart();
    void streamChanged();
    void moveLoTo(double time);

    std::unique_ptr<TradeStreams::Subscription> m_sub;
    double m_tf;
    double m_start;
    size_t m_budget;
    footprint::Book m_book;
    quint64 m_tapeGen = 0, m_streamGen = 0;
    qint64 m_lo = -1, m_hi = -1;   // the trades taken: ids m_lo..m_hi (-1: none yet / not ready)
    double m_loTime = NaN;         // the oldest one's time
    double m_trimmedFrom = NaN;    // live, the oldest candles dropped (whole) to keep the budget: the first one left
    bool m_full = false;           // the budget was reached going back: nothing older taken
    bool m_stopped = false;
    // What the legend was last told (stateChanged() when one of them changes).
    bool m_shownReady = false, m_shownLoading = false;
    QString m_shownError;
};

} // namespace qtm
