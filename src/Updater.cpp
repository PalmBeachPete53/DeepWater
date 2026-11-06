#include "Updater.h"

#include "BuildInfo.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QNetworkReply>
#include <QProcess>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>

namespace {

// A public repository: no user needs an account or a token to update.
const char *const Repository = "https://api.github.com/repos/PalmBeachPete53/DeepWater";
// The channel the CI posts to (.github/workflows/build.yml). Its name is no secret worth keeping: see isBuildSignal().
const char *const Relay = "wss://ntfy.sh/deepwater-updates-08dccc156782ea69cc5eb407/ws";
const int FirstCheckMs = 20 * 1000;
const int WatchSocketMs = 30 * 1000;
const int SilenceMs = 150 * 1000;       // more than three keepalives: the connection is dead (a Mac that slept)
const int SignalsApartMs = 20 * 1000;   // a noisy channel causes no more than this
const int CheckAgainMs = 60 * 1000;     // GitHub's API may lag a moment behind the release just made

QString apiBase()
{
    const QByteArray custom = qgetenv("DEEPWATER_UPDATE_API");
    return custom.isEmpty() ? QString::fromLatin1(Repository) : QString::fromUtf8(custom);
}

// This system's installer among a release's files.
QString installerSuffix()
{
#ifdef Q_OS_WIN
    return QStringLiteral(".exe");
#else
    return QStringLiteral(".dmg");
#endif
}

// The running bundle: <bundle>.app/Contents/MacOS/<binary>.
QString runningBundle()
{
    QDir dir(QCoreApplication::applicationDirPath());
    return dir.cdUp() && dir.cdUp() ? dir.absolutePath() : QString();
}

} // namespace

Updater::Updater(QObject *parent)
    : QObject(parent)
{
    QSettings().remove("keys/github");   // the token older versions asked for: nothing reads it now
    const QString bundle = runningBundle();
    m_target = update::installTarget(bundle, QFileInfo::exists(QFileInfo(bundle).absolutePath() + "/CMakeCache.txt"));
    connect(&m_socket, &QWebSocket::connected, this, [this] {
        m_lastHeard.start();
        if (m_listenedBefore)   // what was published while the connection was down
            check();
        m_listenedBefore = true;
    });
    connect(&m_socket, &QWebSocket::textMessageReceived, this, &Updater::onSocketMessage);
    m_timer.setInterval(WatchSocketMs);
    connect(&m_timer, &QTimer::timeout, this, &Updater::keepListening);
    m_timer.start();
    keepListening();
#if defined(Q_OS_MACOS) || defined(Q_OS_WIN)
    QTimer::singleShot(FirstCheckMs, this, [this] {
        if (autoCheck() && !turnedOff()) check();
    });
#endif
}

// Every 30 s: connected if it should be, and only then. A socket that closed is opened again here, so a relay that is
// down costs one attempt each time.
void Updater::keepListening()
{
#if !defined(Q_OS_MACOS) && !defined(Q_OS_WIN)
    return;   // builds are published for these two only
#endif
    const bool wanted = autoCheck() && !turnedOff();
    const QAbstractSocket::SocketState state = m_socket.state();
    if (!wanted) {
        if (state != QAbstractSocket::UnconnectedState)
            m_socket.close();
    } else if (state == QAbstractSocket::UnconnectedState) {
        const QByteArray custom = qgetenv("DEEPWATER_UPDATE_SOCKET");
        m_socket.open(QUrl(custom.isEmpty() ? QString::fromLatin1(Relay) : QString::fromUtf8(custom)));
    } else if (state == QAbstractSocket::ConnectedState && m_lastHeard.elapsed() > SilenceMs) {
        m_socket.abort();
        keepListening();   // reopen at once: this is the Mac waking up
    }
}

void Updater::onSocketMessage(const QString &message)
{
    m_lastHeard.start();
    if (!update::isBuildSignal(message.toUtf8()))
        return;
    if (m_lastSignal.isValid() && m_lastSignal.elapsed() < SignalsApartMs)
        return;
    m_lastSignal.start();
    check();
    QTimer::singleShot(CheckAgainMs, this, &Updater::check);
}

QString Updater::currentCommit()
{
    const QByteArray pretend = qgetenv("DEEPWATER_UPDATE_PRETEND");
    return pretend.isEmpty() ? QStringLiteral(DEEPWATER_COMMIT) : QString::fromUtf8(pretend).section(' ', 0, 0);
}

qint64 Updater::currentTime()
{
    const QByteArray pretend = qgetenv("DEEPWATER_UPDATE_PRETEND");
    return pretend.isEmpty() ? qint64(DEEPWATER_COMMIT_TIME) : QString::fromUtf8(pretend).section(' ', 1, 1).toLongLong();
}

bool Updater::hasUpdate() const
{
    return m_state == State::Available || m_state == State::Downloading || m_state == State::Preparing
           || m_state == State::Ready
           || (m_state == State::Failed && update::isNewer(currentCommit(), currentTime(), m_release));
}

bool Updater::installsInPlace()
{
#ifdef Q_OS_MACOS
    return true;
#else
    return false;
#endif
}

QString Updater::cantInstallReason() const
{
#ifndef Q_OS_MACOS
    return tr("Updates install on macOS only for now.");
#else
    if (m_target.isEmpty())
        return tr("Move Deepwater to your Applications folder first: macOS runs it from a temporary copy here.");
    const QString folder = QFileInfo(m_target).absolutePath();
    if (!QFileInfo(folder).isWritable())
        return tr("Deepwater can't write to %1.").arg(folder);
    return {};
#endif
}

// The GitHub row's switch in Sources: off, no request at all, as for every source.
bool Updater::turnedOff() const
{
    return QSettings().value("sources/disabled").toStringList().contains("github");
}

bool Updater::autoCheck() const
{
    return QSettings().value("updates/auto", true).toBool();
}

void Updater::setAutoCheck(bool on)
{
    QSettings().setValue("updates/auto", on);
    keepListening();
}

QString Updater::workDir() const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/update";
    QDir().mkpath(dir);
    return dir;
}

void Updater::setState(State state, const QString &error)
{
    m_state = state;
    m_error = error;
    emit changed();
    if (m_unattended && state == State::Available)
        QTimer::singleShot(0, this, &Updater::download);
    if (m_unattended && state == State::Ready)
        QTimer::singleShot(0, this, &Updater::install);
}

QNetworkRequest Updater::apiRequest(const QUrl &url) const
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Deepwater"));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    request.setTransferTimeout(30 * 1000);
    return request;
}

void Updater::check()
{
    // Nothing interrupts a download, and a found update stays found until there is a later one.
    if (m_state == State::Checking || m_state == State::Downloading || m_state == State::Preparing || m_state == State::Ready)
        return;
    if (turnedOff()) {
        setState(State::Failed, tr("GitHub is turned off in Sources."));
        return;
    }
    setState(State::Checking);
    QNetworkRequest request = apiRequest(QUrl(apiBase() + "/releases/tags/latest-main"));
    if (!m_etag.isEmpty())
        request.setRawHeader("If-None-Match", m_etag);
    QNetworkReply *reply = m_net.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status == 304) {
            onRelease(m_release);
            return;
        }
        if (status == 404) {   // nothing published yet
            setState(State::UpToDate);
            return;
        }
        // GitHub allows 60 requests an hour from one address without an account. An unchanged answer doesn't count, and an
        // app asks rarely, so this takes several computers behind one address.
        if (status == 403 || status == 429) {
            setState(State::Failed, tr("GitHub is limiting requests from this connection: try again in an hour."));
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            setState(State::Failed, reply->errorString());
            return;
        }
        const update::Release release = update::parseRelease(reply->readAll(), installerSuffix());
        if (!release.isValid()) {
            setState(State::Failed, tr("GitHub's answer holds no build."));
            return;
        }
        m_etag = reply->rawHeader("ETag");
        onRelease(release);
    });
}

void Updater::onRelease(const update::Release &release)
{
    if (!update::isNewer(currentCommit(), currentTime(), release)) {
        m_release = release;
        setState(State::UpToDate);
        return;
    }
    m_release = release;
    m_changes = update::changesSince(release, currentCommit());
    setState(State::Available);
    QSettings settings;
    if (settings.value("updates/announced").toString() != release.commit) {
        settings.setValue("updates/announced", release.commit);
        emit updateFound();
    }
}

void Updater::download()
{
    if (m_state != State::Available && !(m_state == State::Failed && m_release.isValid()))
        return;
    if (const QString why = cantInstallReason(); !why.isEmpty()) {
        setState(State::Failed, why);
        return;
    }
    m_file.close();
    m_file.setFileName(workDir() + "/" + QFileInfo(m_release.assetName).fileName());
    if (!m_file.open(QIODevice::WriteOnly)) {
        setState(State::Failed, m_file.errorString());
        return;
    }
    m_received = 0;
    setState(State::Downloading);
    fetchAsset(m_release.assetUrl);
}

void Updater::fetchAsset(const QUrl &url)
{
    QNetworkRequest request = apiRequest(url);
    request.setRawHeader("Accept", "application/octet-stream");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);   // to its storage
    request.setTransferTimeout(60 * 1000);
    m_download = m_net.get(request);
    connect(m_download, &QIODevice::readyRead, this, [this] {
        if (m_download->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() / 100 != 2)
            return;
        const QByteArray chunk = m_download->readAll();
        if (m_file.write(chunk) != chunk.size()) {
            m_download->abort();
            return;
        }
        m_received += chunk.size();
        emit progress(m_received, m_release.assetSize);
    });
    connect(m_download, &QNetworkReply::finished, this, [this] {
        QNetworkReply *reply = m_download;
        m_download = nullptr;
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status / 100 == 3) {
            const QUrl next = reply->url().resolved(reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl());
            fetchAsset(next);
            return;
        }
        const QString written = m_file.fileName();
        const bool diskError = m_file.error() != QFile::NoError;
        const QString diskMessage = m_file.errorString();
        m_file.close();
        if (reply->error() != QNetworkReply::NoError || status / 100 != 2 || diskError) {
            QFile::remove(written);
            setState(State::Failed, diskError ? diskMessage : reply->errorString());
            return;
        }
        prepare();
    });
}

void Updater::runStep(const QString &program, const QStringList &args, int timeoutMs,
                      std::function<void(int, const QByteArray &)> done)
{
    auto *process = new QProcess(this);
    process->setProcessChannelMode(QProcess::SeparateChannels);
    auto *timeout = new QTimer(process);
    timeout->setSingleShot(true);
    connect(timeout, &QTimer::timeout, process, &QProcess::kill);
    connect(process, &QProcess::finished, this, [process, done](int code, QProcess::ExitStatus status) {
        const QByteArray out = process->readAllStandardOutput();
        process->deleteLater();
        done(status == QProcess::NormalExit ? code : -1, out);
    });
    connect(process, &QProcess::errorOccurred, this, [process, done](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            process->deleteLater();
            done(-1, {});
        }
    });
    timeout->start(timeoutMs);
    process->start(program, args);
}

void Updater::unmountThen(std::function<void()> next)
{
    if (m_mountPoint.isEmpty()) {
        next();
        return;
    }
    const QString mount = m_mountPoint;
    m_mountPoint.clear();
    runStep("/usr/bin/hdiutil", {"detach", mount, "-quiet", "-force"}, 60000, [mount, next](int, const QByteArray &) {
        QDir().rmdir(mount);
        next();
    });
}

void Updater::prepare()
{
    setState(State::Preparing);
    const QString dmg = m_file.fileName();
    const auto fail = [this, dmg](const QString &why) {
        unmountThen([this, dmg, why] {
            QFile::remove(dmg);
            setState(State::Failed, why);
        });
    };

    if (m_release.assetSize > 0 && QFileInfo(dmg).size() != m_release.assetSize) {
        fail(tr("The download is incomplete."));
        return;
    }
    if (!m_release.sha256.isEmpty()) {
        QFile file(dmg);
        QCryptographicHash hash(QCryptographicHash::Sha256);
        if (!file.open(QIODevice::ReadOnly) || !hash.addData(&file) || hash.result().toHex() != m_release.sha256.toLatin1()) {
            fail(tr("The download doesn't match GitHub's checksum."));
            return;
        }
    }

    const QString work = workDir();
    const QString staging = work + "/staging.app";
    QDir(staging).removeRecursively();
    m_mountPoint = work + "/mount";
    QDir().mkpath(m_mountPoint);
    runStep("/usr/bin/hdiutil", {"attach", dmg, "-nobrowse", "-readonly", "-noautoopen", "-quiet", "-mountpoint", m_mountPoint},
            180000, [this, fail, staging, dmg](int code, const QByteArray &) {
        if (code != 0) {
            m_mountPoint.clear();
            fail(tr("The disk image couldn't be opened."));
            return;
        }
        const QStringList apps = QDir(m_mountPoint).entryList({"*.app"}, QDir::Dirs);
        if (apps.isEmpty()) {
            fail(tr("The disk image holds no application."));
            return;
        }
        runStep("/usr/bin/ditto", {m_mountPoint + "/" + apps.first(), staging}, 300000,
                [this, fail, staging, dmg](int code, const QByteArray &) {
            if (code != 0) {
                fail(tr("The application couldn't be copied out of the disk image."));
                return;
            }
            // The copy must start and be the build the release says it is, before anything is replaced.
            if (!QFileInfo(staging + "/Contents/MacOS/deepwater").isExecutable()) {
                fail(tr("The application in the disk image is incomplete."));
                return;
            }
            runStep(staging + "/Contents/MacOS/deepwater", {"--version"}, 30000,
                    [this, fail, dmg](int code, const QByteArray &out) {
                if (code != 0 || !out.contains(m_release.commit.toLatin1())) {
                    fail(tr("The new version didn't start, so nothing was changed."));
                    return;
                }
                unmountThen([this, dmg] {
                    QFile::remove(dmg);
                    setState(State::Ready);
                });
            });
        });
    });
}

void Updater::install()
{
    if (m_state != State::Ready)
        return;
    const QString work = workDir();
    const QString scriptPath = work + "/install.sh";
    // Run once this process is gone. A failed step puts back what was there, and the app starts either way.
    const QByteArray script = R"SH(#!/bin/sh
pid="$1"; target="$2"; staging="$3"; previous="$4"
i=0
while kill -0 "$pid" 2>/dev/null; do
    i=$((i + 1))
    [ "$i" -gt 600 ] && exit 1
    sleep 0.1
done
rm -rf "$previous"
if [ -e "$target" ] && ! mv "$target" "$previous"; then
    echo "could not move $target aside"
    open "$target"
    exit 1
fi
if ! mv "$staging" "$target"; then
    echo "could not move the new version in place"
    [ -e "$previous" ] && mv "$previous" "$target"
    open "$target"
    exit 1
fi
xattr -dr com.apple.quarantine "$target" 2>/dev/null
open "$target"
)SH";
    QSaveFile file(scriptPath);
    if (!file.open(QIODevice::WriteOnly) || file.write(script) != script.size() || !file.commit()) {
        setState(State::Failed, file.errorString());
        return;
    }
    const QStringList command {"-c", QString("exec /bin/sh \"$0\" \"$@\" >\"%1/install.log\" 2>&1").arg(work),
                               scriptPath, QString::number(QCoreApplication::applicationPid()), m_target,
                               work + "/staging.app", work + "/previous.app"};
    if (!QProcess::startDetached("/bin/sh", command)) {
        setState(State::Failed, tr("The installer couldn't start."));
        return;
    }
    QCoreApplication::quit();
}
