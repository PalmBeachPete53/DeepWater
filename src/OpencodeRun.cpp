#include "OpencodeRun.h"

#include "AiOpencodeView.h"

#include <QDateTime>
#include <QDir>
#include <QJsonDocument>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>

namespace {
constexpr char Agent[] = "deepwater-chat";
}

OpencodeRun::OpencodeRun(QObject *parent) : QObject(parent) {}

OpencodeRun::~OpencodeRun()
{
    kill();
}

bool OpencodeRun::available()
{
    return !AiOpencodeView::executable().isEmpty();
}

QByteArray OpencodeRun::configuration()
{
    // Denied at the top as well as on the agent: the user's global configuration can allow a tool by name, and these win.
    const QJsonObject deny {{"*", "deny"}, {"skill", "deny"}};
    const QJsonObject agent {{"mode", "primary"}, {"permission", deny}};
    const QJsonObject config {{"permission", deny}, {"agent", QJsonObject {{Agent, agent}}}};
    return QJsonDocument(config).toJson(QJsonDocument::Compact);
}

void OpencodeRun::listModels()
{
    const QString exe = AiOpencodeView::executable();
    if (m_lister || exe.isEmpty()) return;
    auto *p = new QProcess(this);
    m_lister = p;
    auto *timeout = new QTimer(p);
    timeout->setSingleShot(true);
    connect(timeout, &QTimer::timeout, p, [p] { p->kill(); });
    connect(p, &QProcess::finished, this, [this, p](int code, QProcess::ExitStatus status) {
        static const QRegularExpression model(R"(^[\w.-]+/[\w.:/-]+$)");
        QStringList models;
        if (code == 0 && status == QProcess::NormalExit)
            for (const QString &line : QString::fromUtf8(p->readAllStandardOutput()).split('\n', Qt::SkipEmptyParts))
                if (model.match(line.trimmed()).hasMatch()) models << line.trimmed();
        m_lister = nullptr;
        p->deleteLater();
        emit modelsListed(models);
    });
    connect(p, &QProcess::errorOccurred, this, [this, p](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart) return;
        m_lister = nullptr;
        p->deleteLater();
        emit modelsListed({});
    });
    timeout->start(30000);
    p->start(exe, {"models"});
}

void OpencodeRun::ask(const QString &message, const QString &model)
{
    if (m_process) return;
    const QString exe = AiOpencodeView::executable();
    if (exe.isEmpty()) {
        emit failed(tr("opencode isn't on this computer (opencode.ai to install it)."));
        return;
    }
    const QString folder = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/ai/opencode";
    QDir().mkpath(folder);
    m_buffer.clear();
    m_error.clear();
    m_usage = {};
    m_stopped = false;
    m_startedAt = QDateTime::currentMSecsSinceEpoch();

    QStringList args {"run", "--standalone", "--format", "json", "--agent", Agent, "--title", "Deepwater"};
    if (!m_session.isEmpty()) args << "--session" << m_session;
    if (!model.isEmpty()) args << "--model" << model;
    auto *p = new QProcess(this);
    m_process = p;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("OPENCODE_CONFIG_CONTENT", QString::fromUtf8(configuration()));
    p->setProcessEnvironment(env);
    p->setWorkingDirectory(folder);
    connect(p, &QProcess::readyReadStandardOutput, this, &OpencodeRun::readOutput);
    connect(p, &QProcess::finished, this, [this, p](int code, QProcess::ExitStatus status) {
        if (p != m_process) return;
        finished(code, status != QProcess::NormalExit);
    });
    connect(p, &QProcess::errorOccurred, this, [this, p](QProcess::ProcessError e) {
        if (p != m_process || e != QProcess::FailedToStart) return;
        m_error = p->errorString();
        finished(-1, true);
    });
    p->start(exe, args);
    if (m_process == p) {
        p->write(message.toUtf8());   // on its input: the message doesn't show in the process list
        p->closeWriteChannel();
    }
    emit busyChanged();
}

void OpencodeRun::readOutput()
{
    if (!m_process) return;
    m_buffer += m_process->readAllStandardOutput();
    qsizetype end;
    while ((end = m_buffer.indexOf('\n')) >= 0) {
        const QJsonObject event = QJsonDocument::fromJson(m_buffer.left(end)).object();
        m_buffer.remove(0, end + 1);
        if (!event.isEmpty()) handleEvent(event);
        if (!m_process) return;
    }
}

void OpencodeRun::handleEvent(const QJsonObject &e)
{
    const QString type = e.value("type").toString();
    const QJsonObject part = e.value("part").toObject();
    if (const QString session = e.value("sessionID").toString(); !session.isEmpty()) m_session = session;
    if (type == "text") {
        emit text(part.value("text").toString());
    } else if (type == "step_finish") {
        const QJsonObject tokens = part.value("tokens").toObject();
        m_usage["tokens"] = m_usage.value("tokens").toInt() + tokens.value("output").toInt();
        m_usage["prompt_tokens"] = m_usage.value("prompt_tokens").toInt() + tokens.value("input").toInt();
        m_usage["reused_tokens"] = m_usage.value("reused_tokens").toInt() + tokens.value("cache").toObject().value("read").toInt();
    } else if (type == "error") {
        m_error = e.value("error").toObject().value("message").toString();
    } else if (type == "tool_use" && part.value("state").toObject().value("status").toString() == "completed") {
        // Denied by the agent: a tool that ran means the limits didn't hold.
        const QString tool = part.value("tool").toString();
        kill();
        m_session.clear();
        emit busyChanged();
        emit failed(tr("Stopped: opencode ran its own tool \"%1\", which Deepwater denies it here.").arg(tool));
    }
}

void OpencodeRun::finished(int code, bool crashed)
{
    readOutput();
    if (!m_process) return;   // stopped by what the output said
    const QString errors = QString::fromUtf8(m_process->readAllStandardError()).trimmed();
    m_process->deleteLater();
    m_process = nullptr;
    emit busyChanged();
    if (m_stopped) {
        QJsonObject stats = m_usage;
        stats["stop"] = "stopped";
        stats["gen_ms"] = double(QDateTime::currentMSecsSinceEpoch() - m_startedAt);
        emit answered(stats);
    } else if (!m_error.isEmpty() || code != 0 || crashed) {
        emit failed(!m_error.isEmpty() ? tr("opencode: %1").arg(m_error)
                    : errors.isEmpty() ? tr("opencode stopped (exit code %1).").arg(code)
                                       : tr("opencode: %1").arg(errors.right(300)));
    } else {
        QJsonObject stats = m_usage;
        stats["stop"] = "eos";
        stats["gen_ms"] = double(QDateTime::currentMSecsSinceEpoch() - m_startedAt);
        emit answered(stats);
    }
}

void OpencodeRun::stop()
{
    if (!m_process) return;
    m_stopped = true;
    m_process->terminate();
    QTimer::singleShot(2000, m_process, [p = m_process] { p->kill(); });
}

void OpencodeRun::kill()
{
    if (!m_process) return;
    QProcess *p = m_process;
    m_process = nullptr;   // its signals no longer count
    p->disconnect(this);
    p->kill();
    p->waitForFinished(1000);
    p->deleteLater();
}

void OpencodeRun::reset()
{
    const bool wasBusy = busy();
    kill();
    m_session.clear();
    if (wasBusy) emit busyChanged();
}
