#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QUrl>

// What the update system reads from GitHub, without any network or UI, so it can be tested on made-up answers.
//
// The CI publishes each successful build of main as the one release `latest-main` of this public repository, so reading it
// takes no account. Its body says which commit it is and lists the last ones (see .github/workflows/build.yml), so an install
// compares that commit with the one it was built from and shows what changed.
namespace update {

struct Release {
    QString commit;          // full hash, from the release's body
    qint64 commitTime = 0;   // seconds since the epoch, from the release's body
    QString assetName;       // this system's installer
    QUrl assetUrl;           // the API's address for the asset
    QUrl browserUrl;         // the address a person downloads it from
    qint64 assetSize = 0;
    QString sha256;          // lower case hex; empty when GitHub didn't give a digest
    QStringList changeCommits, changeSubjects;   // the body's "change: <hash> <subject>" lines, newest first

    bool isValid() const { return !commit.isEmpty() && commitTime > 0 && assetUrl.isValid() && !assetUrl.isEmpty(); }
};

// An invalid Release when the answer isn't a build the CI published, or holds no file ending in `suffix`.
Release parseRelease(const QByteArray &json, const QString &suffix = QStringLiteral(".dmg"));

// True when the release should be offered to a build of this commit made at this time. Only a later commit counts: a
// build from a checkout ahead of the last published one, the developer's own, must not be told to go back. A build
// that doesn't know its commit (made without git) takes any release.
bool isNewer(const QString &currentCommit, qint64 currentTime, const Release &release);

// What the release brings to a build of this commit: its changes newer than that commit, newest first. A commit the list
// doesn't reach (a build far behind, or never pushed) gets the whole list.
QStringList changesSince(const Release &release, const QString &currentCommit);

// True for a line of the relay's WebSocket that announces a build. The others are its "open" and "keepalive" events.
// It only ever means "go and ask GitHub", so anyone able to post to the channel can do no more than cause a check.
bool isBuildSignal(const QByteArray &json);

// The first 7 characters of a hash, as git shows it.
QString shortCommit(const QString &commit);

// Where to install, given the running bundle: itself, or /Applications/deepwater.app for a build inside a CMake build
// directory (`hasCMakeCache`), which the next `cmake --build` would otherwise write over a foreign bundle. Empty when
// the app doesn't run from a bundle, or runs from a translocated copy that macOS deletes (a quarantined download not
// moved out of its folder or its disk image).
QString installTarget(const QString &bundlePath, bool hasCMakeCache);

} // namespace update
