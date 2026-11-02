#include "HttpApi.h"

#include "../core/Error.h"
#include "Json.h"
#include "Server.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHttpHeaders>
#include <QHttpServer>
#include <QHttpServerConfiguration>
#include <QHttpServerRequest>
#include <QHttpServerResponder>
#include <QPointer>
#include <QRegularExpression>
#include <QTcpServer>
#include <QUrl>

namespace qtm {

static const QByteArray TEXT = "text/plain; charset=utf-8";

HttpApi::HttpApi(Backend *backend, const QString &customDir, QObject *parent)
    : QObject(parent), m_backend(backend), m_customDir(customDir)
{
    m_pool.setMaxThreadCount(64);   // like Node, answer many requests at once (they wait on the network)
    m_pool.setExpiryTimeout(60000);
    m_keepAlive.setInterval(25000);
    connect(&m_keepAlive, &QTimer::timeout, this, &HttpApi::keepAlive);
}

HttpApi::~HttpApi()
{
    m_keepAlive.stop();
    m_clients.clear();
    if (m_tcp) m_tcp->close();
    m_pool.waitForDone();
    delete m_server;
}

int HttpApi::listen(quint16 port, int tries)
{
    if (m_port) return m_port;
    if (!m_server) {
        m_server = new QHttpServer;
        QHttpServerConfiguration config;
        config.setMaximumBodySize(qint64(310e6));
        m_server->setConfiguration(config);
        m_server->setMissingHandler(this, [this](const QHttpServerRequest &req, QHttpServerResponder &responder) {
            onRequest(req, responder);
        });
    }
    for (int k = 0; k <= tries; ++k) {
        auto *tcp = new QTcpServer;
        if (tcp->listen(QHostAddress::LocalHost, quint16(port + k))) {
            if (!m_server->bind(tcp)) {
                delete tcp;
                return 0;
            }
            m_tcp = tcp;
            m_port = port + k;
            m_keepAlive.start();
            return m_port;
        }
        const bool inUse = tcp->serverError() == QAbstractSocket::AddressInUseError;
        delete tcp;
        if (!inUse) return 0;
    }
    return 0;
}

// The server only answers to this machine. Writes need a custom header, which a random website
// can't add to a cross-site request, so other pages can't drive your terminal.
bool HttpApi::allowed(const QString &method, const QByteArray &hostHeader, const QByteArray &xqt, const QByteArray &origin,
                      bool hasOrigin)
{
    static const QRegularExpression port(QStringLiteral(":\\d+$")), brackets(QStringLiteral("^\\[|\\]$"));
    QString host = QString::fromUtf8(hostHeader);
    host.replace(port, QString());
    host.replace(brackets, QString());
    if (host != "localhost" && host != "127.0.0.1" && host != "::1") return false;
    if (method != "GET" && method != "HEAD") {
        if (xqt != "1") return false;
        if (hasOrigin && !origin.isEmpty()) {
            // new URL(origin).host: "null" and other opaque origins don't parse.
            const QUrl u(QString::fromUtf8(origin), QUrl::StrictMode);
            if (!u.isValid() || u.scheme().isEmpty() || u.host().isEmpty()) return false;
            QString h = u.host().toLower();
            if (h.contains(':')) h = '[' + h + ']';
            const bool defaultPort = (u.scheme() == "http" && u.port() == 80) || (u.scheme() == "https" && u.port() == 443);
            if (u.port() != -1 && !defaultPort) h += ':' + QString::number(u.port());
            if (h != QString::fromUtf8(hostHeader)) return false;
        }
    }
    return true;
}

QHash<QString, QString> HttpApi::parseQuery(const QString &q)
{
    QHash<QString, QString> out;
    for (const QString &part : q.split('&', Qt::SkipEmptyParts)) {
        const qsizetype eq = part.indexOf('=');
        const QString k = eq < 0 ? part : part.left(eq);
        const QString v = eq < 0 ? QString() : part.mid(eq + 1);
        out.insert(jsv::decodeFormComponent(k), jsv::decodeFormComponent(v));
    }
    return out;
}

QString HttpApi::normalizePath(const QString &encoded)
{
    QStringList out;
    const QStringList segs = encoded.split('/');
    auto isDot = [](const QString &s) { return s == "." || s.compare("%2e", Qt::CaseInsensitive) == 0; };
    auto isDotDot = [](const QString &s) {
        const QString l = s.toLower();
        return l == ".." || l == ".%2e" || l == "%2e." || l == "%2e%2e";
    };
    for (int i = 1; i < segs.size(); ++i) {
        const QString &s = segs[i];
        const bool last = i == segs.size() - 1;
        if (isDot(s)) {
            if (last) out << QString();
        } else if (isDotDot(s)) {
            if (!out.isEmpty()) out.removeLast();
            if (last) out << QString();
        } else {
            out << s;
        }
    }
    return '/' + out.join('/');
}

static QString methodName(QHttpServerRequest::Method m)
{
    switch (m) {
    case QHttpServerRequest::Method::Get: return "GET";
    case QHttpServerRequest::Method::Put: return "PUT";
    case QHttpServerRequest::Method::Delete: return "DELETE";
    case QHttpServerRequest::Method::Post: return "POST";
    case QHttpServerRequest::Method::Head: return "HEAD";
    case QHttpServerRequest::Method::Options: return "OPTIONS";
    case QHttpServerRequest::Method::Patch: return "PATCH";
    case QHttpServerRequest::Method::Connect: return "CONNECT";
    case QHttpServerRequest::Method::Trace: return "TRACE";
    default: return "UNKNOWN";
    }
}

static void sendText(QHttpServerResponder &r, int status, const QByteArray &text)
{
    QHttpHeaders h;
    h.append(QHttpHeaders::WellKnownHeader::ContentType, TEXT);
    h.append(QHttpHeaders::WellKnownHeader::CacheControl, "no-store");
    r.write(text, h, QHttpServerResponder::StatusCode(status));
}

static void sendResponse(QHttpServerResponder &r, const Response &res)
{
    if (res.isText) {
        sendText(r, res.status, res.text);
        return;
    }
    QHttpHeaders h;
    h.append(QHttpHeaders::WellKnownHeader::ContentType, "application/json");
    h.append(QHttpHeaders::WellKnownHeader::CacheControl, "no-store");
    r.write(jsv::stringify(res.json), h, QHttpServerResponder::StatusCode(res.status));
}

void HttpApi::onRequest(const QHttpServerRequest &req, QHttpServerResponder &responder)
{
    const QString method = methodName(req.method());
    const QHttpHeaders &headers = req.headers();
    const QByteArray host = headers.value(QHttpHeaders::WellKnownHeader::Host).toByteArray();
    const QByteArray xqt = headers.value("x-qt").toByteArray();
    const bool hasOrigin = headers.contains(QHttpHeaders::WellKnownHeader::Origin);
    const QByteArray origin = headers.value(QHttpHeaders::WellKnownHeader::Origin).toByteArray();
    if (!allowed(method, host, xqt, origin, hasOrigin))
        return sendText(responder, 403, "Forbidden: Quant Terminal only answers to this computer.");

    const QUrl url = req.url();
    QString p;
    try {
        const QString encoded = normalizePath(url.path(QUrl::FullyEncoded));
        p = jsv::decodeURIComponent(encoded);
        // QUrl turns every '%' into %25 when the request had a malformed escape (/%E0%A4%A), so
        // that arrives here as valid. A path the app made never holds a stray '%', so a second
        // decode that fails means the original was malformed: Node answers that with 400.
        if (encoded.contains("%25")) jsv::decodeURIComponent(p);
    } catch (const Error &) {
        return sendText(responder, 400, "Bad URL");
    }
    const QHash<QString, QString> q = parseQuery(url.query(QUrl::FullyEncoded));

    if (!p.startsWith("/api/")) {
        if (p.startsWith("/custom/")) {
            const QString base = QDir::cleanPath(m_customDir);
            const QString file = QDir::cleanPath(base + '/' + p.mid(8));
            if (!file.startsWith(base + '/')) return sendText(responder, 403, "Forbidden");
            QFile f(file);
            if (!QFileInfo(file).isFile() || !f.open(QIODevice::ReadOnly)) return sendText(responder, 404, "Not found");
            // server.js MIME, by extension (path.extname: the last dot of the name, not a leading one).
            static const QHash<QString, QByteArray> MIME = {
                { ".html", "text/html; charset=utf-8" }, { ".js", "text/javascript; charset=utf-8" },
                { ".mjs", "text/javascript; charset=utf-8" }, { ".css", "text/css; charset=utf-8" },
                { ".json", "application/json" }, { ".svg", "image/svg+xml" }, { ".png", "image/png" },
                { ".ico", "image/x-icon" }, { ".txt", "text/plain; charset=utf-8" },
            };
            const QString name = QFileInfo(file).fileName();
            const qsizetype dot = name.lastIndexOf('.');
            const QString ext = dot > 0 ? name.mid(dot) : QString();
            QHttpHeaders h;
            h.append(QHttpHeaders::WellKnownHeader::ContentType, MIME.value(ext, "application/octet-stream"));
            h.append(QHttpHeaders::WellKnownHeader::CacheControl, p.mid(8).startsWith("vendor/") ? "public, max-age=86400" : "no-cache");
            return responder.write(f.readAll(), h);
        }
        return sendText(responder, 404, "Not found");
    }

    if (p == "/api/events") {
        QHttpHeaders h;
        h.append(QHttpHeaders::WellKnownHeader::ContentType, "text/event-stream");
        h.append(QHttpHeaders::WellKnownHeader::CacheControl, "no-cache");
        h.append(QHttpHeaders::WellKnownHeader::Connection, "keep-alive");
        auto client = std::make_shared<QHttpServerResponder>(std::move(responder));
        client->writeBeginChunked(h);
        client->writeChunk("retry: 3000\n\n");
        m_clients.push_back(client);
        return;
    }

    auto r = std::make_shared<QHttpServerResponder>(std::move(responder));
    const QByteArray body = req.body();
    QPointer<HttpApi> self(this);
    Backend *backend = m_backend;
    m_pool.start([self, r, backend, method, p, q, body] {
        Response res;
        try {
            res = backend->handle(method, p, q, body);
        } catch (const std::exception &e) {
            res.status = 500;
            res.json = QJsonObject { { "error", QString::fromUtf8(e.what()) } };
        }
        QMetaObject::invokeMethod(
            self.data(), [r, res] { sendResponse(*r, res); }, Qt::QueuedConnection);
    });
}

void HttpApi::broadcast(const QString &type, const QJsonValue &payload)
{
    const QByteArray msg = "event: " + type.toUtf8() + "\ndata: " + jsv::stringify(payload) + "\n\n";
    std::vector<std::shared_ptr<QHttpServerResponder>> keep;
    for (auto &c : m_clients) {
        if (c->isResponseCanceled()) continue;
        c->writeChunk(msg);
        keep.push_back(c);
    }
    m_clients = keep;
}

void HttpApi::keepAlive()
{
    std::vector<std::shared_ptr<QHttpServerResponder>> keep;
    for (auto &c : m_clients) {
        if (c->isResponseCanceled()) continue;
        c->writeChunk(": keep-alive\n\n");
        keep.push_back(c);
    }
    m_clients = keep;
}

} // namespace qtm
