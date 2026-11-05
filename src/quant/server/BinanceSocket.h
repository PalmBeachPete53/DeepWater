#pragma once

// One Binance WebSocket stream (`<pair>@aggTrade`, `<pair>@depth@100ms`, `<pair>@kline_1m` ...), on binance.com's
// stream hosts tried in turn, or binance.us's. Each JSON message goes to `message` with the local time it was received,
// `opened` says it connected, and `closed(opened)` comes once, when it gives up: the connection ended (opened) or no
// host answered (!opened). Whoever owns it decides whether to open another. Deleting it closes the socket, and nothing
// is called after. Used by the trade, depth and candle streams (TradeStream, DepthStream, KlineStream).

#include <QObject>
#include <QStringList>

#include <functional>

class QJsonObject;
class QWebSocket;

namespace qtm {

class BinanceSocket : public QObject
{
public:
    BinanceSocket(QObject *parent, const QString &stream, bool us,
                  std::function<void(const QJsonObject &message, double receivedMs)> message,
                  std::function<void()> opened, std::function<void(bool opened)> closed);
    ~BinanceSocket() override;

private:
    void open(int hostIdx);

    QString m_stream;
    QStringList m_hosts;
    std::function<void(const QJsonObject &, double)> m_message;
    std::function<void()> m_opened;
    std::function<void(bool)> m_closed;
    QWebSocket *m_ws = nullptr;
};

} // namespace qtm
