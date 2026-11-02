#pragma once

#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QObject>
#include <QVector>

class QNetworkReply;

// Claude through the Messages API (raw HTTP, api.anthropic.com, API version 2026-10-21), streamed as
// server-sent events so long answers show as they come. The key is the user's (Sources › AI, "keys/anthropic").
// Thinking is left to the model (adaptive), depth comes from `effort`; on the models that support it the server-side
// fallback is on, so a safety refusal is re-run on another model inside the same call.
class ClaudeClient : public QObject
{
    Q_OBJECT
public:
    struct ModelInfo {
        QString id;
        QString name;
    };
    struct Usage {
        int inputTokens = 0;
        int outputTokens = 0;
        int cacheRead = 0;
    };

    explicit ClaudeClient(QObject *parent = nullptr);

    static QString keyId() { return "anthropic"; }
    static QString defaultModel() { return "claude-opus-5-5"; }
    // The models shown before the API's own list comes in.
    static QVector<ModelInfo> knownModels();
    bool hasKey() const;

    // Streams one answer; `messages` is the conversation so far as the API takes it ([{role, content}]).
    void send(const QString &model, const QString &system, const QJsonArray &messages, const QString &effort, int maxTokens);
    void abort();
    bool isBusy() const { return m_reply != nullptr; }
    void listModels();
    QUrl probeUrl() const { return QUrl("https://api.anthropic.com/v1/models?limit=1"); }

signals:
    void textDelta(const QString &text);
    void thinkingDelta(const QString &text);
    void finished(const QString &stopReason, const QString &modelUsed, const ClaudeClient::Usage &usage, qint64 ms);
    void failed(const QString &error);
    void modelsListed(const QVector<ClaudeClient::ModelInfo> &models, const QString &error);

private:
    void handleEvent(const QByteArray &data);

    QNetworkAccessManager m_net;
    QNetworkReply *m_reply = nullptr;
    QByteArray m_buffer;
    QString m_modelUsed, m_stopReason;
    Usage m_usage;
    qint64 m_startedAt = 0;
};
