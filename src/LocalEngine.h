#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QStringList>

class QProcess;

// The local engine (AI › Local): runs one GGUF model in deepwater-llm (src/llm), a helper process next to the app's
// executable, inside a Seatbelt sandbox written for that model. Inside it, the helper may read the model's files and
// the app's own bundle (its code; Metal looks the bundle up) and use the GPU; nothing else: no other file of the user's,
// no network, no writing, no other program, an empty environment, no core dump. It gets conversations on its standard
// input and answers on its standard output (JSON lines), and quits when the app closes its input.
//
// Which models may run is the slots' business (ModelSlotsView::mayRun): the engine runs the files it is given.
class LocalEngine : public QObject
{
    Q_OBJECT
public:
    enum class State { Off, Loading, Ready, Busy };

    explicit LocalEngine(QObject *parent = nullptr);
    ~LocalEngine() override;

    // Whether this build has the engine (a universal build doesn't), and where it is.
    static QString helperPath();
    static bool available();
    // The Seatbelt profile for a model's files (exposed for the tests and the README's description).
    static QString sandboxProfile(const QStringList &files);

    // Loads a model (the first file; the others are the parts of a split one). `context` in tokens.
    void load(const QString &model, const QStringList &files, int context);
    void unload();
    // Sends the whole conversation ([{role, content}]); the engine reuses what it already evaluated of it.
    void chat(const QJsonArray &messages, const QJsonObject &options = {});
    void stopAnswer();

    State state() const { return m_state; }
    QString model() const { return m_model; }
    QJsonObject info() const { return m_info; }   // the helper's "ready" line: model, params, bytes, ctx, devices…

signals:
    void stateChanged();
    void loaded(const QJsonObject &info);
    void token(const QString &text);
    void answered(const QJsonObject &stats);   // the helper's "done" line
    void failed(const QString &message);

private:
    void readOutput();
    void finished();
    void setState(State s);

    QProcess *m_process = nullptr;
    State m_state = State::Off;
    QString m_model;
    QJsonObject m_info;
    QByteArray m_buffer;
    QByteArray m_stderr;   // the end of it, for the message when the helper fails
    bool m_unloading = false;
};
