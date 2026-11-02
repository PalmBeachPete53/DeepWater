#include "ClaudeClient.h"

#include "KeyedSources.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace {

const char *ApiVersion = "2026-10-21";

// The models whose refusals the server can hand to a fallback model (beta "default" routing).
bool supportsFallback(const QString &model)
{
    return model == "claude-fable-5-1" || model == "claude-opus-5-5" || model == "claude-opus-5" || model == "claude-sonnet-5-5";
}

} // namespace

ClaudeClient::ClaudeClient(QObject *parent)
    : QObject(parent)
{
}

QVector<ClaudeClient::ModelInfo> ClaudeClient::knownModels()
{
    return {{"claude-opus-5-5", "Claude Opus 5.5"},
            {"claude-sonnet-5-5", "Claude Sonnet 5.5"},
            {"claude-haiku-5-5", "Claude Haiku 5.5"},
            {"claude-fable-5-1", "Claude Fable 5.1"}};
}

bool ClaudeClient::hasKey() const
{
    return !sourceKey(keyId()).isEmpty();
}

void ClaudeClient::send(const QString &model, const QString &system, const QJsonArray &messages, const QString &effort, int maxTokens)
{
    if (m_reply)
        return;
    const QString key = sourceKey(keyId());
    if (key.isEmpty()) {
        emit failed(tr("No Anthropic API key: enter one in Sources › AI"));
        return;
    }
    QJsonObject body;
    body.insert("model", model);
    body.insert("max_tokens", maxTokens);
    body.insert("stream", true);
    body.insert("messages", messages);
    if (!system.trimmed().isEmpty())
        body.insert("system", system.trimmed());
    if (!effort.isEmpty())
        body.insert("output_config", QJsonObject {{"effort", effort}});
    body.insert("thinking", QJsonObject {{"type", "adaptive"}, {"display", "summarized"}});
    QNetworkRequest req(QUrl("https://api.anthropic.com/v1/messages"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Deepwater/0.1"));
    req.setRawHeader("x-api-key", key.toUtf8());
    req.setRawHeader("anthropic-version", ApiVersion);
    req.setRawHeader("accept", "text/event-stream");
    if (supportsFallback(model)) {
        body.insert("fallbacks", "default");
        req.setRawHeader("anthropic-beta", "server-side-fallback-2026-07-01");
    }
    req.setTransferTimeout(10 * 60 * 1000);
    m_buffer.clear();
    m_modelUsed = model;
    m_stopReason.clear();
    m_usage = {};
    m_startedAt = QDateTime::currentMSecsSinceEpoch();
    m_reply = m_net.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    QNetworkReply *reply = m_reply;
    connect(reply, &QNetworkReply::readyRead, this, [this, reply] {
        m_buffer += reply->readAll();
        // Events are separated by a blank line.
        int at;
        while ((at = m_buffer.indexOf("\n\n")) >= 0) {
            const QByteArray event = m_buffer.left(at);
            m_buffer.remove(0, at + 2);
            for (const QByteArray &line : event.split('\n'))
                if (line.startsWith("data:"))
                    handleEvent(line.mid(5).trimmed());
        }
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        m_reply = nullptr;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() == QNetworkReply::OperationCanceledError) {
            emit failed(tr("Stopped"));
            return;
        }
        if (reply->error() != QNetworkReply::NoError || status >= 400) {
            // The API explains errors in JSON.
            const QJsonObject err = QJsonDocument::fromJson(m_buffer + reply->readAll()).object().value("error").toObject();
            const QString message = err.value("message").toString();
            emit failed(message.isEmpty() ? reply->errorString() : tr("%1 (HTTP %2)").arg(message).arg(status));
            return;
        }
        emit finished(m_stopReason, m_modelUsed, m_usage, QDateTime::currentMSecsSinceEpoch() - m_startedAt);
    });
}

void ClaudeClient::handleEvent(const QByteArray &data)
{
    const QJsonObject o = QJsonDocument::fromJson(data).object();
    const QString type = o.value("type").toString();
    if (type == "message_start") {
        const QJsonObject message = o.value("message").toObject();
        m_modelUsed = message.value("model").toString(m_modelUsed);
        const QJsonObject usage = message.value("usage").toObject();
        m_usage.inputTokens = usage.value("input_tokens").toInt();
        m_usage.cacheRead = usage.value("cache_read_input_tokens").toInt();
    } else if (type == "content_block_delta") {
        const QJsonObject delta = o.value("delta").toObject();
        const QString kind = delta.value("type").toString();
        if (kind == "text_delta")
            emit textDelta(delta.value("text").toString());
        else if (kind == "thinking_delta")
            emit thinkingDelta(delta.value("thinking").toString());
    } else if (type == "message_delta") {
        m_stopReason = o.value("delta").toObject().value("stop_reason").toString(m_stopReason);
        const QJsonObject usage = o.value("usage").toObject();
        if (usage.contains("output_tokens"))
            m_usage.outputTokens = usage.value("output_tokens").toInt();
        if (usage.contains("input_tokens"))
            m_usage.inputTokens = usage.value("input_tokens").toInt();
    } else if (type == "error") {
        emit failed(o.value("error").toObject().value("message").toString(tr("Error in the stream")));
    }
}

void ClaudeClient::abort()
{
    if (m_reply)
        m_reply->abort();
}

void ClaudeClient::listModels()
{
    const QString key = sourceKey(keyId());
    if (key.isEmpty()) {
        emit modelsListed(knownModels(), QString());
        return;
    }
    QNetworkRequest req(QUrl("https://api.anthropic.com/v1/models?limit=100"));
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Deepwater/0.1"));
    req.setRawHeader("x-api-key", key.toUtf8());
    req.setRawHeader("anthropic-version", ApiVersion);
    req.setTransferTimeout(30000);
    QNetworkReply *reply = m_net.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            const QJsonObject err = QJsonDocument::fromJson(reply->readAll()).object().value("error").toObject();
            emit modelsListed(knownModels(), err.value("message").toString(reply->errorString()));
            return;
        }
        QVector<ModelInfo> models;
        for (const QJsonValue &v : QJsonDocument::fromJson(reply->readAll()).object().value("data").toArray()) {
            const QJsonObject o = v.toObject();
            models.push_back({o.value("id").toString(), o.value("display_name").toString(o.value("id").toString())});
        }
        emit modelsListed(models.isEmpty() ? knownModels() : models, QString());
    });
}
