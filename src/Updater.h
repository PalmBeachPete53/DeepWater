#pragma once

#include "UpdateRelease.h"

#include <QFile>
#include <QNetworkAccessManager>
#include <QObject>
#include <QStringList>
#include <QTimer>

#include <functional>

class QNetworkReply;
class QProcess;

// Keeps the app up to date from GitHub. A desktop app can't receive GitHub's events, so it asks: every 15 minutes it
// reads the release `latest-main`, which the CI replaces after each successful build of main, with an ETag so that an
// unchanged answer costs nothing against GitHub's rate limit. A later build sets state Available and emits
// updateFound() once per commit. download() fetches the .dmg, checks its size and SHA-256, copies the app out of it and
// runs the copy with --version: it must say the release's commit. install() then quits, and a small script, once this
// process is gone, puts the new bundle in place, keeps the old one in <app data>/update/previous.app and starts the app.
//
// The repository is private, so reading it takes a token: the key saved in Sources › Updates, or else `gh auth token`
// when the GitHub CLI is signed in. The latter is asked for when needed and never stored.
//
// For tests with a local server: DEEPWATER_UPDATE_API replaces https://api.github.com/repos/<owner>/<repo>, and
// DEEPWATER_UPDATE_PRETEND ("<commit> <time>") stands for the build's own commit, to play an older version.
class Updater : public QObject
{
    Q_OBJECT
public:
    enum class State {
        Idle,           // not checked yet
        Checking,
        UpToDate,
        NeedsToken,     // GitHub answered 404 or 401: no token, or one that can't read the repository
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
    bool turnedOff() const;
    QNetworkRequest apiRequest(const QUrl &url) const;
    void askGitHubCli();
    void checkAccess();
    void startCheck();
    void onRelease(const update::Release &release);
    void fetchChanges();
    void fetchAsset(const QUrl &url, bool authorized);
    void prepare();
    void runStep(const QString &program, const QStringList &args, int timeoutMs,
                 std::function<void(int exitCode, const QByteArray &output)> done);
    void unmountThen(std::function<void()> next);
    QString workDir() const;

    QNetworkAccessManager m_net;
    QTimer m_timer;
    State m_state = State::Idle;
    QString m_error;
    update::Release m_release;
    QStringList m_changes;
    QString m_target;
    QString m_token;           // saved key or GitHub CLI's, in memory only
    bool m_triedCli = false;
    bool m_unattended = false;
    QByteArray m_etag;         // of m_release's answer
    QFile m_file;
    QNetworkReply *m_download = nullptr;
    qint64 m_received = 0;
    QString m_mountPoint;
};
