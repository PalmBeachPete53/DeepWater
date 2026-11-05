#include "McpServer.h"

#include "LocalTools.h"
#include "ModelSlots.h"

#include <QCoreApplication>
#include <QDate>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QRandomGenerator>
#include <QSettings>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUuid>

#include <algorithm>

namespace {

McpServer *g_instance = nullptr;

constexpr qsizetype MaxHeader = 16 * 1024;
constexpr qsizetype MaxBody = 1024 * 1024;

// The MCP revisions this server speaks (the newest first); a client asking for another gets the newest.
const QStringList &protocolVersions()
{
    static const QStringList list {"2025-11-25", "2025-06-18", "2025-03-26", "2024-11-05"};
    return list;
}

QString randomToken()
{
    QByteArray bytes(32, Qt::Uninitialized);
    QRandomGenerator::system()->fillRange(reinterpret_cast<quint32 *>(bytes.data()), bytes.size() / 4);
    return QString::fromLatin1(bytes.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

bool sameSecret(const QByteArray &a, const QByteArray &b)
{
    if (a.size() != b.size()) return false;
    unsigned char diff = 0;
    for (qsizetype i = 0; i < a.size(); ++i) diff |= static_cast<unsigned char>(a[i] ^ b[i]);
    return diff == 0;
}

QByteArray reason(int status)
{
    switch (status) {
    case 200: return "OK";
    case 202: return "Accepted";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 413: return "Payload Too Large";
    default: return "Error";
    }
}

QString instructions()
{
    return "Deepwater is a desktop application for markets and economics, open on the user's computer. Its tools read "
           "the data it has loaded: the series of the countries (inflation, growth, unemployment, …), the IMF WEO, "
           "policy rates, FX rates, CFTC positions, golden records fused from several sources, and prices. Call "
           "list_data first to see what is there. Give the period of every figure and never invent one. Today is " +
           QDate::currentDate().toString(Qt::ISODate) + ".";
}

} // namespace

McpServer::McpServer(const MlDataExporter::Sources &sources, QObject *parent)
    : QObject(parent)
    , m_tools(new LocalTools(sources, this))
{
    g_instance = this;
    QSettings s;
    m_token = s.value("ai/opencodeToken").toString();
    if (m_token.isEmpty()) {
        m_token = randomToken();
        s.setValue("ai/opencodeToken", m_token);
    }
    if (enabled()) start();
}

McpServer::~McpServer()
{
    if (g_instance == this) g_instance = nullptr;
}

McpServer *McpServer::instance()
{
    return g_instance;
}

bool McpServer::enabled() const
{
    return QSettings().value("ai/opencodeServe", false).toBool();
}

void McpServer::setEnabled(bool on)
{
    QSettings().setValue("ai/opencodeServe", on);
    on ? start() : stop();
    emit stateChanged();
}

bool McpServer::listening() const
{
    return m_server && m_server->isListening();
}

quint16 McpServer::port() const
{
    const int p = QSettings().value("ai/opencodePort", int(DefaultPort)).toInt();
    return p > 1024 && p < 65536 ? quint16(p) : DefaultPort;
}

QString McpServer::url() const
{
    return QString("http://127.0.0.1:%1/mcp").arg(port());
}

void McpServer::newToken()
{
    m_token = randomToken();
    QSettings().setValue("ai/opencodeToken", m_token);
    m_orders.clear();
    emit stateChanged();
}

QStringList McpServer::skills() const
{
    const QVector<modelslots::Slot> all = modelslots::load();
    const int slot = modelslots::slotHolding(all, modelslots::OpencodeId);
    return slot < 0 ? QStringList() : all[slot].skills;
}

void McpServer::start()
{
    if (listening()) return;
    if (!m_server) {
        m_server = new QTcpServer(this);
        connect(m_server, &QTcpServer::newConnection, this, &McpServer::accept);
    }
    m_error.clear();
    if (!m_server->listen(QHostAddress::LocalHost, port()))
        m_error = m_server->errorString();
}

void McpServer::stop()
{
    if (m_server) m_server->close();
    for (QTcpSocket *s : m_connections.keys()) s->abort();
    m_connections.clear();
    m_orders.clear();
    m_error.clear();
}

void McpServer::accept()
{
    while (QTcpSocket *socket = m_server->nextPendingConnection()) {
        m_connections.insert(socket, {});
        connect(socket, &QTcpSocket::readyRead, this, [this, socket] { read(socket); });
        connect(socket, &QTcpSocket::disconnected, this, [this, socket] {
            m_connections.remove(socket);
            socket->deleteLater();
        });
    }
}

// HTTP/1.1, one request at a time per connection (keep-alive): headers, then Content-Length bytes of body.
void McpServer::read(QTcpSocket *socket)
{
    auto it = m_connections.find(socket);
    if (it == m_connections.end()) return;
    it->buffer += socket->readAll();
    while (!it->busy) {
        const qsizetype end = it->buffer.indexOf("\r\n\r\n");
        if (end < 0) {
            if (it->buffer.size() > MaxHeader) {
                send(socket, 413, {});
                socket->disconnectFromHost();
            }
            return;
        }
        Request r;
        const QList<QByteArray> lines = it->buffer.left(end).split('\n');
        const QList<QByteArray> first = lines.value(0).trimmed().split(' ');
        r.method = first.value(0);
        r.path = first.value(1);
        for (qsizetype i = 1; i < lines.size(); ++i) {
            const qsizetype colon = lines[i].indexOf(':');
            if (colon > 0) r.headers.insert(lines[i].left(colon).trimmed().toLower(), lines[i].mid(colon + 1).trimmed());
        }
        if (r.headers.contains("transfer-encoding")) {   // chunked bodies aren't taken
            send(socket, 400, {});
            socket->disconnectFromHost();
            return;
        }
        bool ok = true;
        const qint64 length = r.headers.value("content-length", "0").toLongLong(&ok);
        if (!ok || length < 0 || length > MaxBody) {
            send(socket, 413, {});
            socket->disconnectFromHost();
            return;
        }
        if (it->buffer.size() < end + 4 + length) return;   // the rest of the body hasn't come yet
        r.body = it->buffer.mid(end + 4, length);
        it->buffer.remove(0, end + 4 + length);
        handle(socket, r);
        it = m_connections.find(socket);
        if (it == m_connections.end()) return;
    }
}

bool McpServer::authorized(const Request &r) const
{
    // The Host must be this machine (no DNS rebinding), a browser's Origin too, and the token must match.
    const QByteArray host = r.headers.value("host");
    const QByteArray p = QByteArray::number(port());
    if (host != "127.0.0.1:" + p && host != "localhost:" + p) return false;
    if (r.headers.contains("origin")) {
        const QByteArray origin = r.headers.value("origin");
        if (origin != "http://127.0.0.1:" + p && origin != "http://localhost:" + p) return false;
    }
    const QByteArray auth = r.headers.value("authorization");
    return auth.startsWith("Bearer ") && sameSecret(auth.mid(7).trimmed(), m_token.toLatin1());
}

void McpServer::handle(QTcpSocket *socket, const Request &r)
{
    const QByteArray path = r.path.split('?').value(0);
    if (path != "/mcp") return send(socket, 404, {});
    if (!authorized(r)) return send(socket, r.headers.contains("authorization") ? 403 : 401, {}, {{"WWW-Authenticate", "Bearer"}});
    QByteArray session = r.headers.value("mcp-session-id");
    if (r.method == "DELETE") {
        m_orders.remove(session);
        return send(socket, 200, {});
    }
    if (r.method != "POST") return send(socket, 405, {}, {{"Allow", "POST, DELETE"}});
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(r.body, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return sendError(socket, QJsonValue::Null, -32700, "parse error: one JSON-RPC message per request");
    const QJsonObject message = doc.object();
    if (!message.contains("id") || !message.contains("method"))   // a notification or a response: nothing to answer
        return send(socket, 202, {});
    rpc(socket, message, session);
}

void McpServer::rpc(QTcpSocket *socket, const QJsonObject &message, const QByteArray &session)
{
    const QJsonValue id = message.value("id");
    const QString method = message.value("method").toString();
    const QJsonObject params = message.value("params").toObject();
    if (method == "initialize") {
        const QString asked = params.value("protocolVersion").toString();
        const QByteArray created = QUuid::createUuid().toByteArray(QUuid::WithoutBraces);
        m_orders.insert(created, 0);
        const QJsonObject result {
            {"protocolVersion", protocolVersions().contains(asked) ? asked : protocolVersions().first()},
            {"capabilities", QJsonObject {{"tools", QJsonObject {{"listChanged", false}}}}},
            {"serverInfo", QJsonObject {{"name", Name}, {"title", "Deepwater"},
                                        {"version", QCoreApplication::applicationVersion().isEmpty() ? QStringLiteral("1") : QCoreApplication::applicationVersion()}}},
            {"instructions", instructions()},
        };
        return sendResult(socket, id, result, created);
    }
    if (method == "ping") return sendResult(socket, id, QJsonObject {});
    if (method == "tools/list") {
        // Only the tools of the skills granted now; one ticked later shows at opencode's next connection.
        QJsonArray tools;
        for (const LocalTools::Tool &t : LocalTools::toolsFor(skills())) {
            QJsonObject schema = t.parameters;
            if (schema.isEmpty()) schema = {{"type", "object"}, {"properties", QJsonObject {}}};
            tools.append(QJsonObject {{"name", t.name}, {"description", t.description}, {"inputSchema", schema}});
        }
        return sendResult(socket, id, QJsonObject {{"tools", tools}});
    }
    if (method == "tools/call") return callTool(socket, id, params, session);
    if (method == "resources/list") return sendResult(socket, id, QJsonObject {{"resources", QJsonArray {}}});
    if (method == "prompts/list") return sendResult(socket, id, QJsonObject {{"prompts", QJsonArray {}}});
    sendError(socket, id, -32601, "method not found: " + method);
}

void McpServer::callTool(QTcpSocket *socket, const QJsonValue &id, const QJsonObject &params, const QByteArray &session)
{
    LocalTools::Call call;
    call.name = params.value("name").toString();
    call.arguments = params.value("arguments").toObject();
    const QStringList granted = skills();   // checked at each call: a skill may have been unticked since
    QElapsedTimer clock;
    clock.start();
    QPointer<QTcpSocket> target(socket);
    auto finish = [this, target, id, call, clock, session](QJsonObject result) {
        if (LocalTools::isOrder(call.name) && !result.contains("error")) ++m_orders[session];
        const QString error = result.value("error").toString();
        const QByteArray json = QJsonDocument(result).toJson(QJsonDocument::Compact);
        tooljournal::Entry e;
        e.time = QDateTime::currentDateTime();
        e.model = "opencode";
        e.tool = call.name.isEmpty() ? QStringLiteral("?") : call.name;
        e.arguments = call.arguments;
        e.outcome = error.isEmpty() ? "ok" : error.startsWith("not allowed") ? "refused" : "error";
        e.detail = error.isEmpty() ? QString::fromUtf8(json.left(160)) : error;
        e.resultChars = int(json.size());
        e.ms = clock.elapsed();
        tooljournal::append(e);
        ++m_calls;
        emit journalAdded(e);
        emit stateChanged();
        if (!target) return;
        auto it = m_connections.find(target.data());
        if (it != m_connections.end()) it->busy = false;
        sendResult(target, id, QJsonObject {{"content", QJsonArray {QJsonObject {{"type", "text"}, {"text", QString::fromUtf8(json)}}}},
                                            {"isError", !error.isEmpty()}});
        if (target && it != m_connections.end() && !it->buffer.isEmpty())
            QMetaObject::invokeMethod(this, [this, target] { if (target) read(target); }, Qt::QueuedConnection);
    };
    const auto tool = std::find_if(LocalTools::tools().begin(), LocalTools::tools().end(),
                                   [&](const LocalTools::Tool &t) { return t.name == call.name; });
    if (tool != LocalTools::tools().end() && !granted.contains(tool->skill))
        return finish({{"error", "not allowed: Deepwater doesn't grant opencode the skill \"" + tool->skill +
                                 "\" (AI › Models › Slots)"}});
    if (LocalTools::isOrder(call.name) && m_orders.value(session) >= LocalTools::MaxOrders)
        return finish({{"error", QString("not allowed: at most %1 paper orders per opencode session").arg(LocalTools::MaxOrders)}});
    if (auto it = m_connections.find(socket); it != m_connections.end()) it->busy = true;
    m_tools->call(call, granted, finish);
}

void McpServer::send(QTcpSocket *socket, int status, const QByteArray &body, const QList<QPair<QByteArray, QByteArray>> &headers)
{
    if (!socket || socket->state() != QAbstractSocket::ConnectedState) return;
    QByteArray out = "HTTP/1.1 " + QByteArray::number(status) + ' ' + reason(status) + "\r\n";
    if (!body.isEmpty()) out += "Content-Type: application/json\r\n";
    out += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    out += "Cache-Control: no-store\r\n";
    for (const auto &[name, value] : headers) out += name + ": " + value + "\r\n";
    out += "\r\n" + body;
    socket->write(out);
}

void McpServer::sendResult(QTcpSocket *socket, const QJsonValue &id, const QJsonValue &result, const QByteArray &session)
{
    const QJsonObject reply {{"jsonrpc", "2.0"}, {"id", id}, {"result", result}};
    QList<QPair<QByteArray, QByteArray>> headers;
    if (!session.isEmpty()) headers.append({"Mcp-Session-Id", session});
    send(socket, 200, QJsonDocument(reply).toJson(QJsonDocument::Compact), headers);
}

void McpServer::sendError(QTcpSocket *socket, const QJsonValue &id, int code, const QString &message)
{
    const QJsonObject reply {{"jsonrpc", "2.0"}, {"id", id}, {"error", QJsonObject {{"code", code}, {"message", message}}}};
    send(socket, 200, QJsonDocument(reply).toJson(QJsonDocument::Compact));
}
