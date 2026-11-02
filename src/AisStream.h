#pragma once

#include <QObject>
#include <QTimer>
#include <QWebSocket>

class VesselStore;

// Client for the aisstream.io real-time AIS feed. Subscribes to position and
// static-data reports worldwide and feeds them into a VesselStore. Reconnects
// automatically with backoff.
class AisStream : public QObject
{
    Q_OBJECT
public:
    enum class Status { Off, Connecting, Live, Error };

    explicit AisStream(VesselStore *store, QObject *parent = nullptr);

    // An empty key stops the stream.
    void setApiKey(const QString &key);
    // Sources page: turned off, the stream stays closed but keeps its key.
    void setEnabled(bool on);
    bool isEnabled() const { return m_enabled; }
    QString apiKey() const { return m_apiKey; }

    Status status() const { return m_status; }
    QString errorString() const { return m_error; }

    // Parses one aisstream JSON message (also usable to replay recorded data).
    void processMessage(const QByteArray &json);

signals:
    void statusChanged();

private:
    void connectToFeed();
    void scheduleReconnect();
    void setStatus(Status status, const QString &error = {});

    void createSocket();

    VesselStore *m_store;
    QWebSocket *m_socket = nullptr;   // created once the system's certificates are loaded (see the constructor)
    QString m_apiKey;
    Status m_status = Status::Off;
    QString m_error;
    QTimer m_reconnectTimer;
    QTimer m_watchdog;
    int m_backoffSec = 5;
    bool m_wanted = false;
    bool m_enabled = true;
    // Per connection attempt, to tell a rejected key from a network failure.
    bool m_connected = false;
    bool m_gotData = false;
    QString m_socketError;
};
