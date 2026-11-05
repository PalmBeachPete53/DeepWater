#pragma once

// The trades of one Binance pair, shared by every chart pane that shows them: the footprint, Big Trades, the
// heatmap's bubbles and events (a Deepwater addition, not in Quant Terminal). Binance's aggregated trades: the
// history from REST (/api/v3/aggTrades, newest first, page by page back in time) and the live ones from the
// `<pair>@aggTrade` WebSocket. Binance says who the aggressor was (m = buyer is maker), so the sides are exact. Trade
// ids are consecutive: a live trade that skips ids (a dropped socket, the app hidden) has the gap fetched from REST
// before it goes in, so no trade is counted twice or missed.
//
// One stream per pair, whatever the number of panes: TradeStreams::subscribe() hands out subscriptions, each with its
// pane's needs (no trades before its first bar, a budget of trades, running or paused). The stream fetches back to
// the earliest first bar and up to the largest budget, runs while one subscription does, and closes with the last.
// What each pane makes of the trades (its footprint candles, its view of the tape) is ui/TradeFeed.
//
// The network is behind TradeSource: Binance's (BinanceTradeSource, through Backend) or a test's. The stream lives on
// the thread that subscribes (the UI thread): REST answers and socket messages come back to it.

#include "../core/Footprint.h"

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QString>

#include <cmath>
#include <deque>
#include <functional>
#include <memory>
#include <vector>

class QJsonObject;
class QTimer;

namespace qtm {

class Backend;

struct TapeTrade {
    qint64 id = -1;
    footprint::Trade t;   // the side is Binance's (exact)
};

// Where the trades come from.
class TradeSource
{
public:
    using Fail = std::function<void(const QString &error)>;
    struct Page {
        std::vector<TapeTrade> trades;   // in id order
        bool us = false;                 // answered by binance.us (its own trades and ids)
    };
    using Live = QObject;   // the live trades; deleting it closes them, and nothing is called after
    virtual ~TradeSource() = default;
    // The answers arrive later, on `context`'s thread, and only while `context` exists.
    virtual void tickSize(QObject *context, const QString &ticker, std::function<void(double)> ok, Fail fail) = 0;
    // Up to `limit` trades from id `fromId` on; fromId < 0: the latest ones.
    virtual void page(QObject *context, const QString &ticker, qint64 fromId, int limit,
                      std::function<void(const Page &)> ok, Fail fail) = 0;
    // binance.us's live trades (`us`) or binance.com's. `closed(opened)` comes once, when it gives up: the connection
    // ended (opened) or none could be made (!opened).
    virtual Live *live(QObject *parent, const QString &ticker, bool us, std::function<void(const TapeTrade &)> trade,
                       std::function<void(bool opened)> closed) = 0;
};

// Binance through the backend's providers (REST) and its stream hosts (WebSocket).
class BinanceTradeSource : public TradeSource
{
public:
    explicit BinanceTradeSource(Backend *backend) : m_backend(backend) {}
    void tickSize(QObject *context, const QString &ticker, std::function<void(double)> ok, Fail fail) override;
    void page(QObject *context, const QString &ticker, qint64 fromId, int limit, std::function<void(const Page &)> ok,
              Fail fail) override;
    Live *live(QObject *parent, const QString &ticker, bool us, std::function<void(const TapeTrade &)> trade,
               std::function<void(bool opened)> closed) override;
    static TapeTrade parse(const QJsonObject &o);   // one aggTrade, REST or WebSocket

private:
    Backend *m_backend;
};

class TradeStream : public QObject
{
    Q_OBJECT
public:
    struct Timing {
        int pageGapMs = 100;     // between batches of history pages: ~10 requests a second, under Binance's limits
        int retryMs = 5000;      // after a failed request
        int reconnectMs = 3000;  // after the live stream dropped
    };
    static constexpr int PAGE = 1000;           // Binance's most trades per request
    static constexpr int PARALLEL_PAGES = 4;    // history pages asked for at once

    TradeStream(std::shared_ptr<TradeSource> source, const QString &ticker, Timing timing, QObject *parent = nullptr);
    ~TradeStream() override;

    const QString &ticker() const { return m_ticker; }
    // Every trade kept, in id (= time) order with no gaps, from the earliest first bar of the panes on: the history
    // goes in at the front page by page, live trades at the back, the oldest leave the front. A new generation means
    // it was emptied and filled again (a gap longer than the budget): whoever read it starts over.
    const std::deque<TapeTrade> &tape() const { return m_tape; }
    quint64 generation() const { return m_gen; }
    bool ready() const { return m_hiId >= 0; }   // the first page is in
    double tickSize() const { return m_tick; }   // 0 until known
    bool loadingHistory() const { return m_backfilling; }
    // The oldest trade fetched (kept or not, NaN before the first page): every trade from it on is in the tape, from
    // the panes' earliest first bar on. reachedFirstTrade(): the history goes back to the pair's very first trade.
    double oldestTime() const { return m_oldestTime; }
    bool reachedFirstTrade() const { return m_loId == 0; }
    qint64 firstId() const { return m_loId; }    // the oldest trade fetched (its id)
    qint64 lastId() const { return m_hiId; }     // the newest trade taken in (kept or not)
    QString error() const { return m_error; }
    bool running() const { return !m_stopped; }

    // What the subscriptions need together (TradeStreams sets it): no trades before `start`, about `budget` trades
    // at most, running or stopped.
    void setNeeds(double start, size_t budget, bool active);
    double start() const { return m_start; }
    size_t budget() const { return m_budget; }

signals:
    void changed();         // trades went in or out
    void stateChanged();    // ready / history done / error

private:
    void stop();
    void resume();
    void fetchTickSize();
    void openLive();
    void closeLive();
    void fetchLatest();
    void fetchBack();
    void fetchForward();
    void drain();
    void backfillIfShort();
    bool keep(const TapeTrade &t) const { return (std::isnan(m_start) || t.t.time >= m_start) && t.t.qty > 0; }
    void retry(void (TradeStream::*what)());
    void setError(const QString &e);

    std::shared_ptr<TradeSource> m_source;
    QString m_ticker;
    Timing m_timing;
    double m_start = NAN;
    size_t m_budget = PAGE;
    double m_tick = 0;
    TradeSource::Live *m_live = nullptr;
    int m_liveToken = 0;          // bumped when the live stream is closed: its late messages are dropped
    QTimer *m_retry = nullptr;
    std::deque<TapeTrade> m_pending;   // live trades waiting for the ones before them
    std::deque<TapeTrade> m_tape;
    quint64 m_gen = 0;
    qint64 m_loId = -1, m_hiId = -1;   // the trades fetched: m_loId..m_hiId, no gaps
    double m_oldestTime = NAN;
    bool m_backfilling = false, m_forwardBusy = false, m_stopped = true, m_tickKnown = false;
    // Which exchange the history came from: binance.us has its own trades and ids, so its live stream must be
    // binance.us's too (known with the first page; the live stream opens then).
    bool m_us = false, m_exchangeKnown = false;
    int m_token = 0;       // bumped by stop(): answers to older requests are dropped
    int m_backToken = 0;   // bumped when the history starts over: the batch on the way is dropped
    QString m_error;
};

// The streams, one per pair, and who uses them.
class TradeStreams
{
    struct State;

public:
    explicit TradeStreams(std::shared_ptr<TradeSource> source, TradeStream::Timing timing = {});
    ~TradeStreams();

    // A pane's hold on its pair's stream. Deleting it lets the stream go when no one else uses it.
    class Subscription
    {
    public:
        ~Subscription();
        TradeStream *stream() const { return m_stream; }   // null once the streams are gone (the app closing)
        void setNeeds(double start, size_t budget);          // the pane's first bar, its budget of trades
        void setActive(bool active);                         // stopped while the app is hidden, or in replay

    private:
        friend class TradeStreams;
        Subscription() = default;
        std::weak_ptr<State> m_state;
        QPointer<TradeStream> m_stream;
        double m_start = NAN;
        size_t m_budget = 0;
        bool m_active = true;
    };

    std::unique_ptr<Subscription> subscribe(const QString &ticker, double start, size_t budget, bool active = true);
    int streamCount() const;   // pairs with a stream (for the tests and the debug overlay)
    TradeStream *find(const QString &ticker) const;   // the pair's stream, null when no pane uses it

private:
    std::shared_ptr<State> m_state;
};

} // namespace qtm
