#include "LocalEngine.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QProcess>
#include <QThread>

#ifdef Q_OS_UNIX
#include <sys/resource.h>
#endif
#ifdef Q_OS_WIN
#include "LocalSandboxWin.h"
#endif

namespace {

constexpr char SandboxExec[] = "/usr/bin/sandbox-exec";

QString quoted(const QString &path)
{
    QString s = path;
    s.replace('\\', "\\\\").replace('"', "\\\"");
    return '"' + s + '"';
}

// The app's bundle (deepwater.app), or the helper's folder outside one.
QString bundlePath()
{
    QDir dir(QCoreApplication::applicationDirPath());
    if (dir.dirName() == "MacOS" && dir.cdUp() && dir.dirName() == "Contents" && dir.cdUp())
        return dir.canonicalPath();
    return QFileInfo(QCoreApplication::applicationDirPath()).canonicalFilePath();
}

} // namespace

LocalEngine::LocalEngine(QObject *parent) : QObject(parent) {}

LocalEngine::~LocalEngine()
{
    if (m_process) {
        m_process->disconnect(this);
        m_process->kill();
        m_process->waitForFinished(1000);
    }
}

QString LocalEngine::helperPath()
{
#ifdef Q_OS_WIN
    return QCoreApplication::applicationDirPath() + "/deepwater-llm.exe";
#else
    return QCoreApplication::applicationDirPath() + "/deepwater-llm";
#endif
}

bool LocalEngine::available()
{
#ifdef Q_OS_MACOS
    return QFileInfo(helperPath()).isExecutable() && QFileInfo(SandboxExec).isExecutable();
#elif defined(Q_OS_WIN)
    return QFileInfo::exists(helperPath());
#else
    return false;   // no sandbox written for other systems yet: the engine doesn't run there
#endif
}

int LocalEngine::checkSandbox(const QString &file)
{
#ifdef Q_OS_WIN
    QString error;
    if (!localsandbox::prepare({}, QCoreApplication::applicationDirPath(), &error)) {
        qWarning().noquote() << error;
        return 2;
    }
    QProcess p;
    localsandbox::applyTo(&p);
    p.setWorkingDirectory(QCoreApplication::applicationDirPath());
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start(helperPath(), {"--sandbox-selftest", file});
    const bool done = p.waitForFinished(90000);
    qWarning().noquote() << QString::fromUtf8(p.readAll()) << "started:" << (p.error() != QProcess::FailedToStart);
    return done && p.exitStatus() == QProcess::NormalExit ? p.exitCode() : 2;
#else
    Q_UNUSED(file);
    return 2;
#endif
}

QString LocalEngine::sandboxProfile(const QStringList &files)
{
    QStringList reads;
    for (const QString &f : files) reads << "(literal " + quoted(QFileInfo(f).canonicalFilePath()) + ')';
    // system.sb: what any process needs (the system's libraries and frameworks, its own memory, the clock, sysctls).
    // The GPU: Metal's device and IOSurface user clients. Everything else is denied, network and writes included.
    return QStringList {
        "(version 1)",
        "(deny default)",
        "(import \"system.sb\")",
        "(allow process-exec (literal " + quoted(QFileInfo(helperPath()).canonicalFilePath()) + "))",
        "(allow file-read* (subpath " + quoted(bundlePath()) + ") " + reads.join(' ') + ')',
        "(allow iokit-open-user-client (iokit-user-client-class \"AGXDeviceUserClient\" \"IOSurfaceRootUserClient\"))",
    }.join('\n');
}

void LocalEngine::load(const QString &model, const QStringList &files, int context)
{
    // DEEPWATER_LLM_CPU: for the Windows CI, to check that the CPU mode really keeps the GPU's drivers out.
    start(model, files, context, qEnvironmentVariableIsSet("DEEPWATER_LLM_CPU"));
}

void LocalEngine::start(const QString &model, const QStringList &files, int context, bool cpuOnly)
{
    unload();
    m_files = files;
    m_context = context;
    m_cpuOnly = cpuOnly;
    if (!available()) {
        emit failed(tr("The local engine isn't part of this build."));
        return;
    }
    if (files.isEmpty()) return;
    m_model = model;
    m_info = {};
    m_buffer.clear();
    m_stderr.clear();
    m_unloading = false;

    m_process = new QProcess(this);
    // An environment of its own: nothing of the app's (keys, paths) reaches the helper.
    QProcessEnvironment env;
    env.insert("LANG", "en_US.UTF-8");
#ifdef Q_OS_WIN
    // Windows won't create a process in an AppContainer without these, which locate the system and the container's folder.
    // None holds anything of the app's; the keys stay out as on macOS.
    const QProcessEnvironment system = QProcessEnvironment::systemEnvironment();
    // GGML_VK_VISIBLE_DEVICES picks the graphics card when a PC has several; the CI uses it for its software driver, which
    // llama.cpp otherwise skips as not being a GPU.
    for (const char *name : {"SystemRoot", "SystemDrive", "USERPROFILE", "LOCALAPPDATA", "APPDATA", "TEMP", "TMP",
                             "GGML_VK_VISIBLE_DEVICES"})
        if (system.contains(name)) env.insert(name, system.value(name));
    if (cpuOnly)
        env.insert("VK_LOADER_DRIVERS_DISABLE", "*");   // the Vulkan loader then opens no driver at all
#endif
    m_process->setProcessEnvironment(env);
#ifdef Q_OS_WIN
    m_process->setWorkingDirectory(QCoreApplication::applicationDirPath());
    QString refused;
    if (!localsandbox::prepare(files, QCoreApplication::applicationDirPath(), &refused)) {
        delete m_process;
        m_process = nullptr;
        emit failed(refused);
        return;
    }
    localsandbox::applyTo(m_process);
#else
    m_process->setWorkingDirectory("/");
#endif
#ifdef Q_OS_UNIX
    m_process->setChildProcessModifier([] {
        const rlimit none {0, 0};
        setrlimit(RLIMIT_CORE, &none);   // no core dump of the model's memory
        const rlimit files {64, 64};
        setrlimit(RLIMIT_NOFILE, &files);
    });
#endif
    connect(m_process, &QProcess::readyReadStandardOutput, this, &LocalEngine::readOutput);
    connect(m_process, &QProcess::readyReadStandardError, this, [this] {
        m_stderr += m_process->readAllStandardError();
        if (m_stderr.size() > 8192) m_stderr = m_stderr.right(8192);
    });
    connect(m_process, &QProcess::finished, this, &LocalEngine::finished);
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) finished();
    });

    const int threads = std::max(1, QThread::idealThreadCount() / 2);
    setState(State::Loading);
    QStringList arguments {"--model", files.first(), "--ctx", QString::number(context), "--threads", QString::number(threads)};
    if (cpuOnly)
        arguments << "--gpu-layers" << "0";
#ifdef Q_OS_WIN
    m_process->start(helperPath(), arguments);
#else
    m_process->start(SandboxExec, QStringList {"-p", sandboxProfile(files), helperPath()} + arguments);
#endif
}

void LocalEngine::unload()
{
    if (!m_process) return;
    m_unloading = true;
    QProcess *p = m_process;
    m_process = nullptr;
    p->disconnect(this);
    p->closeWriteChannel();   // the helper quits when its input closes
    if (!p->waitForFinished(1500)) {
        p->kill();
        p->waitForFinished(1000);
    }
    p->deleteLater();
    m_model.clear();
    m_info = {};
    setState(State::Off);
}

void LocalEngine::chat(const QJsonArray &messages, const QJsonObject &options)
{
    if (!m_process || m_state != State::Ready) return;
    QJsonObject req = options;
    req.insert("type", "chat");
    req.insert("messages", messages);
    m_process->write(QJsonDocument(req).toJson(QJsonDocument::Compact) + '\n');
    setState(State::Busy);
}

void LocalEngine::stopAnswer()
{
    if (m_process && m_state == State::Busy) m_process->write("{\"type\":\"stop\"}\n");
}

void LocalEngine::readOutput()
{
    m_buffer += m_process->readAllStandardOutput();
    int nl;
    while ((nl = m_buffer.indexOf('\n')) >= 0) {
        const QByteArray line = m_buffer.left(nl);
        m_buffer.remove(0, nl + 1);
        const QJsonObject o = QJsonDocument::fromJson(line).object();
        const QString type = o.value("type").toString();
        if (type == "token") {
            emit token(o.value("text").toString());
        } else if (type == "ready") {
            m_info = o;
            setState(State::Ready);
            emit loaded(o);
        } else if (type == "done") {
            setState(State::Ready);
            emit answered(o);
        } else if (type == "error") {
            if (m_state == State::Busy) setState(State::Ready);
            emit failed(o.value("message").toString());
        }
    }
}

void LocalEngine::finished()
{
    if (!m_process || m_unloading) return;
#ifdef Q_OS_WIN
    // Windows reports a crash as a normal exit whose code is the NTSTATUS, 0xC...
    const bool crashed = m_process->exitStatus() == QProcess::CrashExit || uint(m_process->exitCode()) >= 0xC0000000u;
    if (m_state == State::Loading && !m_cpuOnly && crashed && m_process->error() != QProcess::FailedToStart) {
        qWarning().noquote() << "local engine: it stopped while loading, trying again on the CPU";
        const QString model = m_model;
        const QStringList files = m_files;
        const int context = m_context;
        m_process->deleteLater();
        m_process = nullptr;
        start(model, files, context, true);
        return;
    }
#endif
    QString why;
    if (m_process->error() == QProcess::FailedToStart) why = tr("the engine couldn't start (%1)").arg(m_process->errorString());
    else if (m_process->exitStatus() == QProcess::CrashExit) why = tr("the engine stopped unexpectedly");
    else why = tr("the engine quit (code %1)").arg(m_process->exitCode());
    QStringList tail;
    for (const QByteArray &l : m_stderr.split('\n'))
        if (!l.trimmed().isEmpty() && !l.contains("control-looking token")) tail << QString::fromUtf8(l.trimmed());
    if (!tail.isEmpty()) why += ": " + tail.mid(std::max<qsizetype>(0, tail.size() - 3)).join(" · ");
    m_process->deleteLater();
    m_process = nullptr;
    m_model.clear();
    m_info = {};
    setState(State::Off);
    emit failed(why);
}

void LocalEngine::setState(State s)
{
    if (m_state == s) return;
    m_state = s;
    emit stateChanged();
}
