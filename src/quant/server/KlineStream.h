#pragma once

// The live candle of a Binance chart, shared by every chart pane on the same pair and timeframe (a Deepwater addition,
// not in Quant Terminal): Binance's `<pair>@kline_<interval>` stream sends the forming candle as it changes (and its
// final state when it closes). One stream per pair and timeframe, whatever the number of panes: KlineStreams::subscribe()
// hands out subscriptions, the stream runs while one is held and closes with the last. A dropped connection is opened
// again 3 s later; when no host answers at all, the subscriptions hear `failed` and the panes poll the REST API instead
// (ChartPane::poll), as for any other exchange.
//
// The network is behind KlineSource: Binance's (BinanceKlineSource) or a test's.

#include "../core/Bar.h"

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QString>

#include <functional>
#include <memory>

class QTimer;

namespace qtm {

class KlineSource
{
public:
    using Live = QObject;   // the live candles; deleting it closes them, and nothing is called after
    virtual ~KlineSource() = default;
    // `bar` for each update of the forming candle; `opened` once connected; `closed(opened)` once, when it gives up:
    // the connection ended (opened) or none could be made (!opened).
    virtual Live *live(QObject *parent, const QString &ticker, const QString &interval, std::function<void(const Bar &)> bar,
                       std::function<void()> opened, std::function<void(bool opened)> closed) = 0;
};

class BinanceKlineSource : public KlineSource
{
public:
    Live *live(QObject *parent, const QString &ticker, const QString &interval, std::function<void(const Bar &)> bar,
               std::function<void()> opened, std::function<void(bool)> closed) override;
};

class KlineStream : public QObject
{
    Q_OBJECT
public:
    KlineStream(std::shared_ptr<KlineSource> source, const QString &ticker, const QString &interval, int reconnectMs,
                QObject *parent = nullptr);
    ~KlineStream() override;

    const QString &ticker() const { return m_ticker; }
    const QString &interval() const { return m_interval; }
    bool connected() const { return m_connected; }
    bool failed() const { return m_failed; }   // no host answered: idle until started again
    void start();                              // (again, after it failed)

signals:
    void bar(const qtm::Bar &b);
    void opened();
    void failedToOpen();

private:
    void open();
    void close();

    std::shared_ptr<KlineSource> m_source;
    QString m_ticker, m_interval;
    int m_reconnectMs;
    KlineSource::Live *m_live = nullptr;
    int m_liveToken = 0;   // bumped when the live stream is closed: its late messages are dropped
    QTimer *m_reconnect = nullptr;
    bool m_connected = false, m_failed = false;
};

// A pane's hold on a stream: its signals are the stream's. Deleting it lets the stream go when no one else uses it.
class KlineSubscription : public QObject
{
    Q_OBJECT
public:
    ~KlineSubscription() override;
    bool connected() const { return m_stream && m_stream->connected(); }
    KlineStream *stream() const { return m_stream; }

signals:
    void bar(const qtm::Bar &b);
    void opened();
    void failed();   // no host answered: the pane polls instead (and lets this go)

private:
    friend class KlineStreams;
    KlineSubscription() = default;
    std::function<void(KlineSubscription *)> m_release;
    QPointer<KlineStream> m_stream;
};

class KlineStreams
{
    struct State;

public:
    explicit KlineStreams(std::shared_ptr<KlineSource> source, int reconnectMs = 3000);
    ~KlineStreams();

    // `ticker`: Binance's pair (BTCUSDT); `interval`: Binance's kline interval (interval(tf)).
    std::unique_ptr<KlineSubscription> subscribe(const QString &ticker, const QString &interval);
    int streamCount() const;
    KlineStream *find(const QString &ticker, const QString &interval) const;
    // Binance's kline interval for a chart timeframe ("1D" → "1d"), empty when it has none.
    static QString interval(const QString &tf);

private:
    std::shared_ptr<State> m_state;
};

} // namespace qtm
