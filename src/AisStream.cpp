#include "AisStream.h"

#include "VesselStore.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSslConfiguration>
#include <QtConcurrent/QtConcurrentRun>

namespace {

const QUrl FeedUrl("wss://stream.aisstream.io/v0/stream");
constexpr int MaxBackoffSec = 120;
constexpr int SilenceTimeoutMs = 90 * 1000;   // reconnect if the feed goes quiet

// aisstream's MetaData keys have changed case over time; accept both.
QJsonValue meta(const QJsonObject &md, const char *upper, const char *lower)
{
    const QJsonValue v = md.value(QLatin1String(upper));
    return v.isUndefined() ? md.value(QLatin1String(lower)) : v;
}

} // namespace

AisStream::AisStream(VesselStore *store, QObject *parent)
    : QObject(parent)
    , m_store(store)
{
    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout, this, &AisStream::connectToFeed);

    m_watchdog.setSingleShot(true);
    m_watchdog.setInterval(SilenceTimeoutMs);
    connect(&m_watchdog, &QTimer::timeout, this, [this] {
        setStatus(Status::Error, tr("No data received"));
        m_socket->abort();
    });

    // A QWebSocket's constructor loads the system's CA certificates, a tenth of a second at launch: load
    // them on the thread pool, then create the socket (and connect, if a key is set by then).
    QtConcurrent::run([] { QSslConfiguration::defaultConfiguration(); }).then(this, [this] {
        createSocket();
        connectToFeed();
    });
}

void AisStream::createSocket()
{
    m_socket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    connect(m_socket, &QWebSocket::connected, this, [this] {
        m_connected = true;
        // The subscription must arrive within 3 seconds of connecting.
        const QJsonObject sub {
            {"APIKey", m_apiKey},
            {"BoundingBoxes", QJsonArray {QJsonArray {QJsonArray {-90, -180}, QJsonArray {90, 180}}}},
            {"FilterMessageTypes", QJsonArray {"PositionReport", "ShipStaticData"}},
        };
        m_socket->sendTextMessage(QString::fromUtf8(QJsonDocument(sub).toJson(QJsonDocument::Compact)));
        m_watchdog.start();
    });
    connect(m_socket, &QWebSocket::binaryMessageReceived, this, &AisStream::processMessage);
    connect(m_socket, &QWebSocket::textMessageReceived, this,
            [this](const QString &msg) { processMessage(msg.toUtf8()); });
    connect(m_socket, &QWebSocket::disconnected, this, [this] {
        m_watchdog.stop();
        if (!m_wanted)
            return;
        if (m_connected && !m_gotData) {
            // aisstream closes the socket silently when the API key is rejected.
            setStatus(Status::Error, tr("aisstream.io closed the connection without sending data. "
                                        "Check that your API key is valid."));
        } else if (!m_connected) {
            setStatus(Status::Error, m_socketError.isEmpty() ? tr("Could not reach aisstream.io")
                                                             : m_socketError);
        } else if (m_status != Status::Error) {
            const QString reason = m_socket->closeReason();
            setStatus(Status::Error, reason.isEmpty() ? tr("Connection closed") : reason);
        }
        scheduleReconnect();
    });
    connect(m_socket, &QWebSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        m_socketError = m_socket->errorString();
        // A failure before the handshake may not be followed by disconnected().
        if (m_wanted && !m_connected && m_socket->state() == QAbstractSocket::UnconnectedState) {
            setStatus(Status::Error, m_socketError);
            scheduleReconnect();
        }
    });

}

void AisStream::setApiKey(const QString &key)
{
    m_apiKey = key.trimmed();
    m_reconnectTimer.stop();
    m_watchdog.stop();
    m_wanted = false;
    if (m_socket)
        m_socket->abort();
    m_backoffSec = 5;
    m_status = Status::Off;   // so the next attempt shows "connecting" again

    if (m_apiKey.isEmpty() || !m_enabled) {
        setStatus(Status::Off);
        return;
    }
    m_wanted = true;
    connectToFeed();
}

void AisStream::setEnabled(bool on)
{
    if (on == m_enabled)
        return;
    m_enabled = on;
    setApiKey(m_apiKey);
}

void AisStream::connectToFeed()
{
    if (!m_wanted)
        return;
    m_connected = false;
    m_gotData = false;
    m_socketError.clear();
    // While retrying after an error, keep showing the error rather than flickering.
    if (m_status != Status::Error)
        setStatus(Status::Connecting);
    if (m_socket)   // otherwise it opens once created
        m_socket->open(FeedUrl);
}

void AisStream::scheduleReconnect()
{
    if (m_reconnectTimer.isActive())
        return;
    m_reconnectTimer.start(m_backoffSec * 1000);
    m_backoffSec = std::min(m_backoffSec * 2, MaxBackoffSec);
}

void AisStream::setStatus(Status status, const QString &error)
{
    if (status == m_status && error == m_error)
        return;
    m_status = status;
    m_error = error;
    emit statusChanged();
}

void AisStream::processMessage(const QByteArray &json)
{
    const QJsonObject root = QJsonDocument::fromJson(json).object();
    if (root.isEmpty())
        return;

    // aisstream reports problems (e.g. an invalid API key) as {"error": "..."}.
    if (root.contains("error")) {
        setStatus(Status::Error, root.value("error").toString());
        return;
    }

    m_gotData = true;
    m_watchdog.start();
    m_backoffSec = 5;
    if (m_status != Status::Live)
        setStatus(Status::Live);

    const QString type = root.value("MessageType").toString();
    const QJsonObject md = root.value("MetaData").toObject();
    const QJsonObject body = root.value("Message").toObject().value(type).toObject();
    const qint64 mmsi = meta(md, "MMSI", "mmsi").toInteger(body.value("UserID").toInteger());
    if (mmsi <= 0)
        return;
    const QString metaName = meta(md, "ShipName", "shipName").toString();

    if (type == "PositionReport") {
        m_store->updatePosition(mmsi, metaName, body.value("Latitude").toDouble(91),
                                body.value("Longitude").toDouble(181), body.value("Sog").toDouble(),
                                body.value("Cog").toDouble(), body.value("TrueHeading").toInt(511));
    } else if (type == "ShipStaticData") {
        const QJsonObject dim = body.value("Dimension").toObject();
        const int length = dim.value("A").toInt() + dim.value("B").toInt();
        m_store->updateStatic(mmsi, body.value("Name").toString(metaName), body.value("Type").toInt(),
                              body.value("Destination").toString(), body.value("CallSign").toString(),
                              body.value("ImoNumber").toInteger(), body.value("MaximumStaticDraught").toDouble(),
                              length);
    }
}
