#include "AiOpencodeView.h"

#include "McpServer.h"
#include "ModelSlots.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

namespace {

QLabel *dimLabel(const QString &text = {})
{
    auto *l = new QLabel(text);
    l->setObjectName("dim");
    l->setWordWrap(true);
    return l;
}

QLabel *stateLabel()
{
    auto *l = new QLabel;
    l->setObjectName("slotState");
    l->setWordWrap(true);
    l->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return l;
}

void setTone(QLabel *l, const char *tone)
{
    l->setProperty("tone", tone);
    l->style()->unpolish(l);
    l->style()->polish(l);
}

QPushButton *segButton(const QString &text)
{
    auto *b = new QPushButton(text);
    b->setObjectName("segButton");
    b->setCursor(Qt::PointingHandCursor);
    return b;
}

QString shortPath(const QString &path)
{
    const QString home = QDir::homePath();
    return path.startsWith(home + '/') ? "~" + path.mid(home.size()) : path;
}

} // namespace

AiOpencodeView::AiOpencodeView(ModelSlotsView *slotsView, QWidget *parent)
    : QWidget(parent)
    , m_server(McpServer::instance())
{
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(8);
    v->addWidget(modelslots::integrationChip(modelslots::OpencodeId, slotsView));
    v->addWidget(dimLabel(tr("opencode, an AI agent that runs in the terminal (opencode.ai), can read Deepwater's data while "
                             "the app is open: Deepwater serves it the tools of the skills of its slot over MCP, on this "
                             "computer only (127.0.0.1), behind a token. opencode hands what the tools return to the model "
                             "it uses, which usually runs at a provider, not on this computer. Every call is in the tool "
                             "journal (AI › Local).")));
    m_found = stateLabel();
    v->addWidget(m_found);

    auto *serveRow = new QHBoxLayout;
    serveRow->setSpacing(12);
    m_serve = new QCheckBox(tr("Serve Deepwater's tools to opencode"));
    m_serve->setObjectName("calCheck");
    m_serve->setCursor(Qt::PointingHandCursor);
    serveRow->addWidget(m_serve);
    m_serverState = stateLabel();
    serveRow->addWidget(m_serverState, 1);
    v->addLayout(serveRow);

    v->addWidget(dimLabel(tr("A skill of its slot ticked or unticked counts at the next call; opencode lists the tools of a "
                             "newly ticked skill when it connects again (a new opencode, or opencode reload).")));

    auto *buttons = new QHBoxLayout;
    buttons->setSpacing(8);
    m_add = segButton(tr("Add to opencode"));
    m_add->setToolTip(tr("Runs opencode mcp add deepwater --global with this address and token."));
    m_copy = segButton(tr("Copy the configuration"));
    m_copy->setToolTip(tr("The JSON to put in opencode.json by hand, token included."));
    m_newToken = segButton(tr("New token"));
    m_newToken->setToolTip(tr("Refuses the current token from now on; opencode must then be updated."));
    buttons->addWidget(m_add);
    buttons->addWidget(m_copy);
    buttons->addWidget(m_newToken);
    buttons->addStretch(1);
    v->addLayout(buttons);
    m_registered = stateLabel();
    v->addWidget(m_registered);
    m_calls = dimLabel();
    v->addWidget(m_calls);

    if (!m_server) {   // MainWindow makes it; without it (a test build) there's nothing to drive
        setEnabled(false);
        m_found->setText(tr("The MCP server isn't available in this build."));
        return;
    }
    connect(m_serve, &QCheckBox::toggled, m_server, &McpServer::setEnabled);
    connect(m_add, &QPushButton::clicked, this, &AiOpencodeView::addToOpencode);
    connect(m_copy, &QPushButton::clicked, this, &AiOpencodeView::copyConfiguration);
    connect(m_newToken, &QPushButton::clicked, this, [this] {
        if (QMessageBox::question(this, tr("New token"),
                                  tr("opencode won't reach Deepwater with its current token any more, until it's updated "
                                     "(Update opencode). Make a new token?"),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
            m_server->newToken();
    });
    connect(m_server, &McpServer::stateChanged, this, &AiOpencodeView::updateState);
    detect();
    updateState();
}

void AiOpencodeView::showEvent(QShowEvent *e)
{
    // opencode may have been installed, or its configuration changed, while the app was open.
    detect();
    updateState();
    QWidget::showEvent(e);
}

QString AiOpencodeView::executable()
{
    QString exe = QStandardPaths::findExecutable("opencode");
    if (!exe.isEmpty()) return exe;
    const QString home = QDir::homePath();
    return QStandardPaths::findExecutable("opencode", {home + "/.opencode/bin", "/opt/homebrew/bin", "/usr/local/bin",
                                                      home + "/.local/bin", home + "/.bun/bin", home + "/.npm-global/bin"});
}

QString AiOpencodeView::configFile()
{
    QString dir = qEnvironmentVariable("XDG_CONFIG_HOME");
    if (dir.isEmpty()) dir = QDir::homePath() + "/.config";
    dir += "/opencode/";
    for (const char *name : {"opencode.json", "opencode.jsonc", "config.json"})
        if (QFileInfo::exists(dir + name)) return dir + name;
    return dir + "opencode.json";
}

AiOpencodeView::Registration AiOpencodeView::registration(const QString &url, const QString &token)
{
    QFile f(configFile());
    if (!f.exists()) return Registration::None;
    if (!f.open(QIODevice::ReadOnly)) return Registration::Unreadable;
    QByteArray text = f.readAll();
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(text, &err);
    if (err.error != QJsonParseError::NoError) {
        // JSONC: drop the // and /* */ comments outside strings, and trailing commas, then try again.
        static const QRegularExpression comments(R"re("(?:\\.|[^"\\])*"|//[^\n]*|/\*.*?\*/)re",
                                                 QRegularExpression::DotMatchesEverythingOption);
        QString s = QString::fromUtf8(text);
        QString out;
        qsizetype last = 0;
        for (auto it = comments.globalMatch(s); it.hasNext();) {
            const QRegularExpressionMatch m = it.next();
            out += s.mid(last, m.capturedStart() - last);
            if (m.captured().startsWith('"')) out += m.captured();
            last = m.capturedEnd();
        }
        out += s.mid(last);
        static const QRegularExpression trailing(R"(,(\s*[}\]]))");
        out.replace(trailing, "\\1");
        doc = QJsonDocument::fromJson(out.toUtf8(), &err);
        if (err.error != QJsonParseError::NoError) return Registration::Unreadable;
    }
    const QJsonObject mcp = doc.object().value("mcp").toObject();
    // opencode 2 keeps the servers under mcp.servers; opencode 1 put them directly under mcp.
    QJsonObject entry = mcp.value("servers").toObject().value(McpServer::Name).toObject();
    if (entry.isEmpty()) entry = mcp.value(McpServer::Name).toObject();
    if (entry.isEmpty()) return Registration::None;
    const bool sameUrl = entry.value("url").toString() == url;
    const bool sameToken = entry.value("headers").toObject().value("Authorization").toString() == "Bearer " + token;
    const bool on = entry.value("enabled").toBool(true);
    return sameUrl && sameToken && on ? Registration::Current : Registration::Stale;
}

void AiOpencodeView::detect()
{
    const QString exe = executable();
    if (exe == m_exe && (!m_version.isEmpty() || exe.isEmpty())) return;
    m_exe = exe;
    m_version.clear();
    if (m_exe.isEmpty()) return;
    run({"--version"}, [this](int code, const QString &output) {
        static const QRegularExpression re(R"(v?(\d+\.\d+[\w.\-]*))");
        const QRegularExpressionMatch m = re.match(output);
        m_version = code == 0 && m.hasMatch() ? m.captured(1) : QStringLiteral("?");
        updateState();
    });
}

void AiOpencodeView::updateState()
{
    if (!m_server) return;
    if (m_exe.isEmpty()) {
        m_found->setText(tr("opencode isn't on this computer (opencode.ai to install it); the address can still be added "
                            "to its configuration by hand (Copy the configuration)."));
        setTone(m_found, "dim");
    } else {
        m_found->setText(m_version.isEmpty() ? tr("opencode found: %1.").arg(shortPath(m_exe))
                                             : tr("opencode %1 found: %2.").arg(m_version, shortPath(m_exe)));
        setTone(m_found, "good");
    }

    m_serve->blockSignals(true);
    m_serve->setChecked(m_server->enabled());
    m_serve->blockSignals(false);
    if (!m_server->enabled()) {
        m_serverState->setText(tr("Off: opencode can't reach Deepwater."));
        setTone(m_serverState, "dim");
    } else if (m_server->listening()) {
        m_serverState->setText(tr("Serving at %1").arg(m_server->url()));
        setTone(m_serverState, "good");
    } else {
        m_serverState->setText(tr("Couldn't listen on port %1: %2").arg(m_server->port()).arg(m_server->error()));
        setTone(m_serverState, "bad");
    }

    const Registration r = registration(m_server->url(), m_server->token());
    const QString file = shortPath(configFile());
    switch (r) {
    case Registration::Current:
        m_registered->setText(tr("In opencode's configuration (%1).").arg(file));
        setTone(m_registered, "good");
        break;
    case Registration::Stale:
        m_registered->setText(tr("opencode's configuration (%1) has Deepwater with another address or token, or turned off: "
                                 "update it.").arg(file));
        setTone(m_registered, "bad");
        break;
    case Registration::Unreadable:
        m_registered->setText(tr("Couldn't read opencode's configuration (%1).").arg(file));
        setTone(m_registered, "bad");
        break;
    case Registration::None:
        m_registered->setText(tr("Not in opencode's configuration yet (%1).").arg(file));
        setTone(m_registered, "dim");
        break;
    }
    m_add->setText(r == Registration::Stale ? tr("Update opencode") : tr("Add to opencode"));
    m_add->setEnabled(!m_exe.isEmpty() && !m_process && r != Registration::Current);
    m_calls->setText(tr("%n call(s) from opencode since Deepwater started.", nullptr, m_server->calls()));
}

void AiOpencodeView::addToOpencode()
{
    if (m_exe.isEmpty() || m_process) return;
    m_add->setEnabled(false);
    m_registered->setText(tr("Adding Deepwater to opencode…"));
    setTone(m_registered, "dim");
    // opencode writes its own configuration (the format moves between versions), then reloads its background service.
    run({"mcp", "add", McpServer::Name, "--global", "--url", m_server->url(), "--header",
         "Authorization=Bearer " + m_server->token()},
        [this](int code, const QString &output) {
            if (code != 0) {
                m_registered->setText(tr("opencode refused: %1").arg(output.trimmed().right(300)));
                setTone(m_registered, "bad");
                m_add->setEnabled(true);
                return;
            }
            run({"reload"}, [this](int, const QString &) { updateState(); });
        });
}

void AiOpencodeView::copyConfiguration()
{
    const QJsonObject entry {{"type", "remote"}, {"url", m_server->url()},
                             {"headers", QJsonObject {{"Authorization", "Bearer " + m_server->token()}}}};
    const QJsonObject config {{"mcp", QJsonObject {{"servers", QJsonObject {{McpServer::Name, entry}}}}}};
    QApplication::clipboard()->setText(QString::fromUtf8(QJsonDocument(config).toJson(QJsonDocument::Indented)));
    m_registered->setText(tr("Copied, token included: paste it only into opencode's configuration."));
    setTone(m_registered, "dim");
}

void AiOpencodeView::run(const QStringList &args, const std::function<void(int, const QString &)> &done)
{
    if (m_process) return;
    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    m_process->setWorkingDirectory(QDir::homePath());   // a global change: no project directory
    QProcess *p = m_process;
    auto *timeout = new QTimer(p);
    timeout->setSingleShot(true);
    connect(timeout, &QTimer::timeout, p, [p] { p->kill(); });
    connect(p, &QProcess::finished, this, [this, p, done](int code, QProcess::ExitStatus status) {
        const QString output = QString::fromUtf8(p->readAll());
        m_process = nullptr;
        p->deleteLater();
        done(status == QProcess::NormalExit ? code : -1, output);
    });
    connect(p, &QProcess::errorOccurred, this, [this, p, done](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart) return;
        m_process = nullptr;
        p->deleteLater();
        done(-1, p->errorString());
    });
    timeout->start(30000);
    p->start(m_exe, args);
}
