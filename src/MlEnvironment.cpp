#include "MlEnvironment.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <QVersionNumber>

#include <algorithm>

MlEnvironment::MlEnvironment(QObject *parent)
    : QObject(parent)
{
    QDir().mkpath(root());
    installScripts();
    const bool venv = QFile::exists(python());
    m_state = venv ? State::Ready : interpreter().isEmpty() ? State::NoInterpreter : State::NotCreated;
    if (venv)
        check();
}

QString MlEnvironment::root()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/ml";
}

QString MlEnvironment::venvDir() { return root() + "/venv"; }
QString MlEnvironment::scriptsDir() { return root() + "/scripts"; }
QString MlEnvironment::dataDir() { return root() + "/data"; }
QString MlEnvironment::runsDir() { return root() + "/runs"; }

QString MlEnvironment::python() const
{
#ifdef Q_OS_WIN
    return venvDir() + "/Scripts/python.exe";   // a Windows venv
#else
    return venvDir() + "/bin/python";
#endif
}

QStringList MlEnvironment::packages()
{
    return {"numpy", "pandas", "pyarrow", "scikit-learn", "lightgbm", "xgboost", "torch", "joblib"};
}

QVector<MlEnvironment::Interpreter> MlEnvironment::interpreters()
{
    // The usual places on a Mac: python.org's framework (every version), Homebrew, /usr/local, Apple's.
    QStringList candidates;
    const QDir framework("/Library/Frameworks/Python.framework/Versions");
    for (const QString &v : framework.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed))
        if (v != "Current")
            candidates << framework.filePath(v + "/bin/python3");
    for (const char *p : {"/opt/homebrew/bin/python3", "/usr/local/bin/python3", "/usr/bin/python3"})
        candidates << QString::fromLatin1(p);
#ifdef Q_OS_WIN
    // python.org's installer (per user, then for everyone), then whatever PATH has. The Microsoft Store stub in
    // WindowsApps fails the version check below and is skipped.
    for (const QString &base : {qEnvironmentVariable("LOCALAPPDATA") + "/Programs/Python",
                                qEnvironmentVariable("ProgramFiles") + "/Python"}) {
        const QDir dir(base);
        for (const QString &v : dir.entryList({"Python3*"}, QDir::Dirs, QDir::Name | QDir::Reversed))
            candidates << dir.filePath(v + "/python.exe");
    }
    for (const char *name : {"python", "python3"})
        if (const QString p = QStandardPaths::findExecutable(QString::fromLatin1(name)); !p.isEmpty())
            candidates << p;
#endif
    QVector<Interpreter> out;
    QSet<QString> seen;
    for (const QString &path : candidates) {
        if (!QFile::exists(path))
            continue;
        const QString real = QFileInfo(path).canonicalFilePath();
        if (seen.contains(real))
            continue;
        seen.insert(real);
        QProcess p;
        p.start(path, {"-c", "import platform;print(platform.python_version())"});
        if (!p.waitForFinished(5000) || p.exitCode() != 0)
            continue;
        out.push_back({path, QString::fromUtf8(p.readAllStandardOutput()).trimmed()});
    }
    // Newest first; 3.9 (Apple's) last, since the current wheels want 3.10 or newer.
    std::stable_sort(out.begin(), out.end(), [](const Interpreter &a, const Interpreter &b) {
        return QVersionNumber::fromString(a.version) > QVersionNumber::fromString(b.version);
    });
    return out;
}

QString MlEnvironment::interpreter() const
{
    const QString saved = QSettings().value("ml/python").toString();
    if (!saved.isEmpty() && QFile::exists(saved))
        return saved;
    const QVector<Interpreter> list = interpreters();
    return list.isEmpty() ? QString() : list.first().path;
}

void MlEnvironment::setInterpreter(const QString &path)
{
    QSettings().setValue("ml/python", path);
    if (m_state == State::NoInterpreter && !path.isEmpty())
        setState(State::NotCreated);
}

void MlEnvironment::installScripts()
{
    QDir().mkpath(scriptsDir());
    for (const char *name : {"train.py", "check_env.py"}) {
        const QString target = scriptsDir() + '/' + name;
        QFile::remove(target);
        QFile::copy(QString(":/ml/") + name, target);
        QFile(target).setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::ReadOther);
    }
}

void MlEnvironment::setState(State s)
{
    if (m_state == s)
        return;
    m_state = s;
    emit stateChanged();
}

void MlEnvironment::create()
{
    if (m_process)
        return;
    const QString base = interpreter();
    if (base.isEmpty()) {
        m_error = tr("No Python 3 found on this computer: install python.org's or Homebrew's, then try again");
        setState(State::NoInterpreter);
        emit finished(false);
        return;
    }
    m_steps.clear();
    if (!QFile::exists(python()))
        m_steps.push_back({base, {"-m", "venv", venvDir()}});
    m_steps.push_back({python(), {"-m", "pip", "install", "--upgrade", "pip"}});
    m_steps.push_back({python(), QStringList {"-m", "pip", "install", "--upgrade"} + packages()});
    m_creating = true;
    m_error.clear();
    setState(State::Installing);
    emit log(tr("Creating the environment with %1 in %2").arg(base, venvDir()));
    startStep();
}

void MlEnvironment::startStep()
{
    if (m_steps.isEmpty()) {
        m_creating = false;
        emit log(tr("Packages installed. Checking the versions…"));
        check();
        return;
    }
    const auto step = m_steps.takeFirst();
    emit log("$ " + step.first + ' ' + step.second.join(' '));
    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_process, &QProcess::readyRead, this, [this] {
        m_pending += m_process->readAll();
        int at;
        while ((at = m_pending.indexOf('\n')) >= 0) {
            onLine(QString::fromUtf8(m_pending.left(at)).trimmed());
            m_pending.remove(0, at + 1);
        }
    });
    connect(m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        if (!m_pending.isEmpty()) {
            onLine(QString::fromUtf8(m_pending).trimmed());
            m_pending.clear();
        }
        m_process->deleteLater();
        m_process = nullptr;
        const bool ok = status == QProcess::NormalExit && code == 0;
        if (m_creating) {
            if (ok) {
                startStep();
            } else {
                m_creating = false;
                m_error = tr("Installation failed (exit code %1)").arg(code);
                emit log(m_error);
                setState(QFile::exists(python()) ? State::Broken : State::NotCreated);
                emit finished(false);
            }
            return;
        }
        if (m_checking) {
            m_checking = false;
            const QJsonObject r = m_lastResult;
            bool complete = ok;
            for (const char *name : {"scikit-learn", "lightgbm", "xgboost", "torch"})
                if (r.value(QLatin1String(name)).isNull())
                    complete = false;
            m_versions = r;
            if (!complete)
                m_error = r.contains("errors") ? QJsonDocument(r.value("errors").toObject()).toJson(QJsonDocument::Compact)
                                               : tr("Some packages are missing");
            else
                m_error.clear();
            setState(complete ? State::Ready : State::Broken);
            emit finished(complete);
            return;
        }
        emit finished(ok);
    });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        if (m_process)
            emit log(tr("Couldn't start %1").arg(m_process->program()));
    });
    m_process->start(step.first, step.second);
}

void MlEnvironment::onLine(const QString &line)
{
    if (line.isEmpty())
        return;
    if (line.startsWith("PROGRESS ")) {
        const QStringList parts = line.mid(9).split('/');
        if (parts.size() == 2)
            emit progress(parts[0].toInt(), parts[1].toInt());
        return;
    }
    if (line.startsWith("RESULT ")) {
        m_lastResult = QJsonDocument::fromJson(line.mid(7).toUtf8()).object();
        emit result(m_lastResult);
        return;
    }
    emit log(line.startsWith("LOG ") ? line.mid(4) : line);
}

void MlEnvironment::check()
{
    if (m_process || !QFile::exists(python()))
        return;
    m_checking = true;
    m_lastResult = QJsonObject();
    m_steps.clear();
    m_steps.push_back({python(), {scriptsDir() + "/check_env.py"}});
    startStep();
}

bool MlEnvironment::run(const QString &script, const QStringList &args, const QString &workDir)
{
    if (m_process || m_state != State::Ready)
        return false;
    m_lastResult = QJsonObject();
    m_steps.clear();
    if (!workDir.isEmpty())
        QDir().mkpath(workDir);
    m_steps.push_back({python(), QStringList {scriptsDir() + '/' + script} + args});
    startStep();
    return m_process != nullptr;
}

void MlEnvironment::cancel()
{
    m_steps.clear();
    m_creating = false;
    m_checking = false;
    if (m_process) {
        m_process->kill();
        emit log(tr("Stopped"));
    }
}
