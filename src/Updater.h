#pragma once

#include "UpdateRelease.h"

#include <QFile>
#include <QNetworkAccessManager>
#include <QObject>
#include <QStringList>
#include <QElapsedTimer>
#include <QTimer>
#include <QWebSocket>

#include <functional>

class QNetworkReply;
class QProcess;

// Keeps the app up to date from GitHub. GitHub has no WebSocket to listen to and its webhooks need a public address, so
// the news goes through a relay: the CI replaces the release `latest-main` after each successful build of main, then
// posts to a channel on ntfy.sh, and the app holds a WebSocket open on that channel. The post carries no information and
// isn't trusted: it only makes the app read the release, with an ETag so that an unchanged answer costs nothing against
// GitHub's rate limit. The app also reads it on starting and on each reconnection, for what it missed; it never polls.
// A later build sets state Available and emits updateFound() once per commit. download() fetches the .dmg, checks its size and SHA-256, copies the app out of it and
// runs the copy with --version: it must say the release's commit. install() then quits, and a small script, once this
// process is gone, puts the new bundle in place, keeps the old one in <app data>/update/previous.app and starts the app.
//
// The repository is public, so a check takes no account and no token.
//
// For tests with a local server: DEEPWATER_UPDATE_API replaces https://api.github.com/repos/<owner>/<repo>,
// DEEPWATER_UPDATE_SOCKET the relay's channel, and
// DEEPWATER_UPDATE_PRETEND ("<commit> <time>") stands for the build's own commit, to play an older version.
class Updater : public QObject
{
    Q_OBJECT
public:
    enum class State {
        Idle,           // not checked yet
        Checking,
        UpToDate,
        Available,
        Downloading,
        Preparing,      // checking the download and the app inside it
        Ready,          // install() can run
        Failed,         // error() says why; a later check starts over
    };

    explicit Updater(QObject *parent = nullptr);

    State state() const { return m_state; }
    QString error() const { return m_error; }
    update::Release release() const { return m_release; }
    QStringList changes() const { return m_changes; }   // the commits' subjects from this build to the release
    qint64 received() const { return m_received; }
    bool hasUpdate() const;

    // Where install() puts the app; empty, with the reason in cantInstallReason(), when it can't.
    QString target() const { return m_target; }
    QString cantInstallReason() const;
    // macOS replaces itself; on Windows the dialog sends the user to the installer.
    static bool installsInPlace();

    // --update-auto (tests): downloads what it finds and installs it without a click.
    void setUnattended(bool on) { m_unattended = on; }

    bool autoCheck() const;
    void setAutoCheck(bool on);

    static QString currentCommit();
    static qint64 currentTime();

public slots:
    void check();
    void download();
    void install();

signals:
    void changed();
    void progress(qint64 received, qint64 total);
    // A build not announced yet: once per commit, across launches.
    void updateFound();

private:
    void setState(State state, const QString &error = QString());
    void keepListening();
    void onSocketMessage(const QString &message);
    bool turnedOff() const;
    QNetworkRequest apiRequest(const QUrl &url) const;
    void onRelease(const update::Release &release);
    void fetchAsset(const QUrl &url);
    void prepare();
    void runStep(const QString &program, const QStringList &args, int timeoutMs,
                 std::function<void(int exitCode, const QByteArray &output)> done);
    void unmountThen(std::function<void()> next);
    QString workDir() const;

    QNetworkAccessManager m_net;
    QWebSocket m_socket;
    QTimer m_timer;            // looks after the socket
    QElapsedTimer m_lastHeard;   // anything from the relay, which sends a keepalive every 45 s
    QElapsedTimer m_lastSignal;
    bool m_listenedBefore = false;
    State m_state = State::Idle;
    QString m_error;
    update::Release m_release;
    QStringList m_changes;
    QString m_target;
    bool m_unattended = false;
    QByteArray m_etag;         // of m_release's answer
    QFile m_file;
    QNetworkReply *m_download = nullptr;
    qint64 m_received = 0;
    QString m_mountPoint;
};
