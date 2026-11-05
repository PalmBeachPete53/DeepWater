#pragma once

#include "MlData.h"
#include "ToolJournal.h"

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QStringList>

class LocalTools;
class QTcpServer;
class QTcpSocket;

// Deepwater's tools served over MCP (Model Context Protocol, Streamable HTTP) to an agent outside the app: opencode
// (AI › Models › Integration). The same tools as the local models' (LocalTools), granted by the slot opencode was dragged
// into (ModelSlots; none outside a slot) and checked again at each call; every call goes to the tool journal (model "opencode").
//
// It listens on 127.0.0.1 only, while the app is open and serving is on (ai/opencodeServe), and answers only requests
// that carry the token (Authorization: Bearer, ai/opencodeToken) and no foreign Origin or Host (a web page can't reach
// it through the browser). POST /mcp takes one JSON-RPC message and answers it in JSON (no SSE stream: GET is 405);
// the tools never change while a session is open, they're refused at call time once a skill is unticked.
class McpServer : public QObject
{
    Q_OBJECT
public:
    explicit McpServer(const MlDataExporter::Sources &sources, QObject *parent = nullptr);
    ~McpServer() override;
    static McpServer *instance();   // the one MainWindow made, or null

    bool enabled() const;
    void setEnabled(bool on);
    bool listening() const;
    QString error() const { return m_error; }   // why it doesn't listen
    quint16 port() const;
    QString url() const;
    QString token() const { return m_token; }
    void newToken();                             // opencode must then be updated
    QStringList skills() const;                  // those of the slot holding opencode, now
    int calls() const { return m_calls; }        // since the app started

    static constexpr quint16 DefaultPort = 47821;
    static constexpr const char *Name = "deepwater";   // the server's name in opencode's configuration

signals:
    void stateChanged();
    void journalAdded(const tooljournal::Entry &e);

private:
    struct Connection {
        QByteArray buffer;
        bool busy = false;   // a call is running: the next request waits
    };
    struct Request {
        QByteArray method, path;
        QHash<QByteArray, QByteArray> headers;   // lower-case names
        QByteArray body;
    };
    void start();
    void stop();
    void accept();
    void read(QTcpSocket *socket);
    void handle(QTcpSocket *socket, const Request &r);
    void rpc(QTcpSocket *socket, const QJsonObject &message, const QByteArray &session);
    void callTool(QTcpSocket *socket, const QJsonValue &id, const QJsonObject &params, const QByteArray &session);
    void send(QTcpSocket *socket, int status, const QByteArray &body, const QList<QPair<QByteArray, QByteArray>> &headers = {});
    void sendResult(QTcpSocket *socket, const QJsonValue &id, const QJsonValue &result, const QByteArray &session = {});
    void sendError(QTcpSocket *socket, const QJsonValue &id, int code, const QString &message);
    bool authorized(const Request &r) const;

    LocalTools *m_tools;
    QTcpServer *m_server = nullptr;
    QHash<QTcpSocket *, Connection> m_connections;
    QHash<QByteArray, int> m_orders;   // paper orders placed per session
    QString m_token;
    QString m_error;
    int m_calls = 0;
};
