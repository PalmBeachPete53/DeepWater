#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QObject>
#include <QProcess>
#include <QStringList>
#include <QVector>

// The app's private Python environment for machine learning (<app data>/ml/venv) and the jobs run in it: its
// creation from a system interpreter, the versions of the stack (scikit-learn, LightGBM, XGBoost, PyTorch), and
// scripts run as child processes whose stdout lines are relayed ("LOG …", "PROGRESS i/n", "RESULT {json}").
class MlEnvironment : public QObject
{
    Q_OBJECT
public:
    enum class State { NoInterpreter, NotCreated, Installing, Ready, Broken };
    struct Interpreter {
        QString path;
        QString version;   // "3.15.0"
    };

    explicit MlEnvironment(QObject *parent = nullptr);

    static QString root();         // <app data>/ml
    static QString venvDir();      // root()/venv
    static QString scriptsDir();   // root()/scripts, refreshed from the bundled copies
    static QString dataDir();      // root()/data: the CSV exports
    static QString runsDir();      // root()/runs: one folder per training
    QString python() const;        // the venv's interpreter
    static QStringList packages();

    // System interpreters found, newest first; the one chosen is kept in the settings (ml/python).
    static QVector<Interpreter> interpreters();
    QString interpreter() const;
    void setInterpreter(const QString &path);

    State state() const { return m_state; }
    QJsonObject versions() const { return m_versions; }   // from check_env.py
    bool isBusy() const { return m_process != nullptr; }
    QString lastError() const { return m_error; }

    // Creates the venv and installs the packages (or upgrades them when it exists). Log lines come through log().
    void create();
    // Runs check_env.py and keeps the versions.
    void check();
    // Runs a bundled script with arguments; stdout lines come through log() / progress() / result().
    bool run(const QString &script, const QStringList &args, const QString &workDir = QString());
    void cancel();
    // Copies the bundled scripts to scriptsDir() (every launch: they follow the app's version).
    static void installScripts();

signals:
    void stateChanged();
    void log(const QString &line);
    void progress(int done, int total);
    void result(const QJsonObject &json);
    void finished(bool ok);

private:
    void setState(State s);
    void startStep();
    void onLine(const QString &line);

    State m_state = State::NotCreated;
    QJsonObject m_versions;
    QString m_error;
    QProcess *m_process = nullptr;
    QVector<QPair<QString, QStringList>> m_steps;   // the creation's commands, run one after the other
    QByteArray m_pending;                           // stdout not yet ended by a newline
    bool m_creating = false;
    bool m_checking = false;
    QJsonObject m_lastResult;
};
