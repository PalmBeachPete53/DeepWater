#pragma once

// server.js's HTTP side, on QHttpServer: the allowed() checks, URL decoding, /api/events (server-
// sent events) and the /api/* routes, which run Backend::handle() on a worker thread. There is no
// browser front end any more: /custom/<file> still serves your indicator files, anything else
// outside /api answers 404.

#include <QByteArray>
#include <QHash>
#include <QJsonValue>
#include <QObject>
#include <QString>
#include <QThreadPool>
#include <QTimer>

#include <memory>
#include <vector>

class QHttpServer;
class QHttpServerRequest;
class QHttpServerResponder;
class QTcpServer;

namespace qtm {

class Backend;

class HttpApi : public QObject
{
    Q_OBJECT
public:
    HttpApi(Backend *backend, const QString &customDir, QObject *parent = nullptr);
    ~HttpApi() override;

    // Listens on 127.0.0.1:port, or the next free one (up to `tries` more). Returns the port or 0.
    int listen(quint16 port, int tries);
    int port() const { return m_port; }

    // broadcast(type, payload): one SSE event to every open /api/events stream.
    void broadcast(const QString &type, const QJsonValue &payload);

    // The decision server.js allowed() makes, from the request's method and headers.
    static bool allowed(const QString &method, const QByteArray &host, const QByteArray &xqt, const QByteArray &origin,
                        bool hasOrigin);
    // URLSearchParams -> Object.fromEntries (the last value wins).
    static QHash<QString, QString> parseQuery(const QString &encodedQuery);
    // new URL(req.url).pathname: dot segments resolved, still percent-encoded.
    static QString normalizePath(const QString &encodedPath);

private:
    void onRequest(const QHttpServerRequest &req, QHttpServerResponder &responder);
    void keepAlive();

    Backend *m_backend;
    QString m_customDir;
    QHttpServer *m_server = nullptr;
    QTcpServer *m_tcp = nullptr;
    int m_port = 0;
    QThreadPool m_pool;
    QTimer m_keepAlive;
    std::vector<std::shared_ptr<QHttpServerResponder>> m_clients;
};

} // namespace qtm
