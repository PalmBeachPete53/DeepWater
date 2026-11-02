#pragma once

// The trades of the footprint chart and of Big Trades (a Deepwater addition, not in Quant Terminal), one feed per chart
// pane whichever uses it: Binance's aggregated trades for one
// pair, the history from REST (/api/v3/aggTrades, newest first, page by page back in time, up to a budget) and the
// live ones from the `<pair>@aggTrade` WebSocket. Binance says who the aggressor was (m = buyer is maker), so the
// sides are exact. Trade ids are consecutive: a live trade that skips ids (a dropped socket, the app hidden) has
// the gap fetched from REST before it goes in, so no trade is counted twice or missed.

#include "../core/Footprint.h"

#include <QObject>
#include <QString>

#include <deque>

class QTimer;
class QWebSocket;

namespace qtm {

class TradeFeed : public QObject
{
    Q_OBJECT
public:
    // `ticker` is Binance's pair (BTCUSDT), `tfSeconds` the chart's candle length; trades before
    // `chartStart` (the chart's first bar) aren't fetched.
    TradeFeed(const QString &ticker, double tfSeconds, double chartStart, int budget, QObject *parent = nullptr);
    ~TradeFeed() override;

    void stop();     // the socket closed, nothing more fetched (the app hidden: resume() catches up)
    void resume();
    void setRowTicks(int rowTicks) { m_book.setRowTicks(rowTicks); }
    // The chart dropped its oldest bars: the footprints go with them.
    void setChartStart(double t);

    const footprint::Book &book() const { return m_book; }
    // Every trade kept, in id (= time) order with no gaps: what Big Trades reads. The history goes in at the front, page
    // by page, live trades at the back. A new generation means it was emptied and filled again (a new feed, or a gap
    // longer than the budget): whoever read it starts over.
    struct TapeTrade {
        qint64 id;
        footprint::Trade t;   // the side is Binance's (exact)
    };
    const std::deque<TapeTrade> &tape() const { return m_tape; }
    quint64 tapeGeneration() const { return m_tapeGen; }
    bool ready() const { return m_hiId >= 0; }
    double tickSize() const { return m_book.grid().tickSize; }
    // Candles from this open time on have every trade; the ones before are partial (where the history stopped).
    double completeFrom() const { return m_completeFrom; }
    bool loadingHistory() const { return m_backfilling; }
    QString error() const { return m_error; }

signals:
    void changed();         // trades went in (the chart redraws, at most a few times a second)
    void stateChanged();    // ready / history done / error: the legend line

private:
    using Raw = TapeTrade;
    void fetchTickSize();
    void openSocket(int hostIdx);
    void fetchLatest();
    void fetchBack();
    void fetchForward();
    void drain();
    bool addTrade(const Raw &r);   // false: before the chart's first bar (or empty), not kept
    void setError(const QString &e);
    static Raw parse(const QJsonObject &o);

    QString m_ticker;
    double m_tf;
    double m_chartStart;
    size_t m_budget;
    footprint::Book m_book;
    QWebSocket *m_ws = nullptr;
    QTimer *m_retry = nullptr;
    std::deque<Raw> m_pending;   // live trades waiting for the ones before them
    std::deque<TapeTrade> m_tape;
    quint64 m_tapeGen = 0;
    qint64 m_loId = -1, m_hiId = -1;   // the ids in the book: m_loId..m_hiId, no gaps
    double m_completeFrom = NaN;
    bool m_backfilling = false, m_forwardBusy = false, m_stopped = false, m_tickKnown = false;
    // Which exchange the history came from: binance.us has its own trades and ids, so its live stream must be
    // binance.us's too (known with the first page; the socket opens then).
    bool m_us = false, m_exchangeKnown = false;
    int m_token = 0;   // bumped by stop(): answers to older requests are dropped
    QString m_error;
};

} // namespace qtm
