#pragma once

#include <QJsonObject>
#include <QObject>

class QProcess;

// One conversation with the model opencode (opencode.ai) is set to use, for LocalChat: each message runs
// `opencode run --standalone --format json --agent deepwater-chat`, the same session from one to the next.
//
// opencode only supplies the model here. Its own tools (shell, files, web, MCP) are denied by the agent given in
// OPENCODE_CONFIG_CONTENT, whatever the user's configuration says, and it runs in an empty folder of the app's: the tools
// Deepwater offers are written in the conversation and run by LocalChat, against the slot, like a local model's. --standalone
// is what makes that configuration apply (the background service keeps its own); an unknown agent is an error, not a
// run without limits. Should one of its tools ever complete all the same, the run is killed and reported as a failure.
class OpencodeRun : public QObject
{
    Q_OBJECT
public:
    explicit OpencodeRun(QObject *parent = nullptr);
    ~OpencodeRun() override;

    static bool available();         // opencode is on this computer
    static QByteArray configuration();   // what goes in OPENCODE_CONFIG_CONTENT

    // The next message of the conversation, with a model ("provider/name"; empty: the one opencode is set to use).
    void ask(const QString &text, const QString &model = {});
    void listModels();               // `opencode models`, answered by modelsListed
    void stop();                     // ends the answer; `answered` follows, with what was written
    void reset();                    // forgets the conversation, without a signal
    bool busy() const { return m_process != nullptr; }

signals:
    void busyChanged();
    void text(const QString &text);
    void answered(const QJsonObject &stats);   // as LocalEngine's: stop, tokens, gen_ms, prompt_tokens, reused_tokens
    void failed(const QString &message);
    void modelsListed(const QStringList &models);   // empty when it couldn't tell

private:
    void readOutput();
    void handleEvent(const QJsonObject &event);
    void finished(int code, bool crashed);
    void kill();

    QProcess *m_process = nullptr;
    QProcess *m_lister = nullptr;
    QByteArray m_buffer;
    QString m_session;
    QString m_error;     // what the run reported as its error
    QJsonObject m_usage;
    qint64 m_startedAt = 0;
    bool m_stopped = false;
};
