// One Binance WebSocket stream, each host tried in turn (see BinanceSocket.h). A Deepwater addition.
#include "BinanceSocket.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QWebSocket>

#include <memory>

namespace qtm {

namespace {
const QStringList HOSTS = {"wss://stream.binance.com:9443/ws/", "wss://data-stream.binance.vision/ws/"};
const QStringList HOSTS_US = {"wss://stream.binance.us:9443/ws/"};
} // namespace

BinanceSocket::BinanceSocket(QObject *parent, const QString &stream, bool us,
                             std::function<void(const QJsonObject &, double)> message, std::function<void()> opened,
                             std::function<void(bool)> closed)
    : QObject(parent), m_stream(stream), m_hosts(us ? HOSTS_US : HOSTS), m_message(std::move(message)),
      m_opened(std::move(opened)), m_closed(std::move(closed))
{
    open(0);
}

BinanceSocket::~BinanceSocket()
{
    if (m_ws) {
        m_ws->disconnect(this);   // nothing comes back once deleted
        m_ws->close();
    }
}

void BinanceSocket::open(int hostIdx)
{
    if (hostIdx >= m_hosts.size()) {
        if (m_closed) m_closed(false);
        return;
    }
    auto *sock = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    m_ws = sock;
    auto opened = std::make_shared<bool>(false);
    auto done = std::make_shared<bool>(false);
    connect(sock, &QWebSocket::connected, this, [this, opened] {
        *opened = true;
        if (m_opened) m_opened();
    });
    connect(sock, &QWebSocket::textMessageReceived, this, [this, sock](const QString &m) {
        if (m_ws != sock) return;
        const double recv = double(QDateTime::currentMSecsSinceEpoch());
        m_message(QJsonDocument::fromJson(m.toUtf8()).object(), recv);
    });
    auto onClose = [this, sock, opened, done, hostIdx] {
        if (*done) return;
        *done = true;
        sock->deleteLater();
        if (m_ws != sock) return;
        m_ws = nullptr;
        if (!*opened) open(hostIdx + 1);
        else if (m_closed) m_closed(true);
    };
    connect(sock, &QWebSocket::disconnected, this, onClose);
    connect(sock, &QWebSocket::errorOccurred, this, [sock, onClose](QAbstractSocket::SocketError) {
        if (sock->state() == QAbstractSocket::UnconnectedState) onClose();
    });
    sock->open(QUrl(m_hosts[hostIdx] + m_stream));
}

} // namespace qtm
