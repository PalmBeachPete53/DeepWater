#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QUrl>

// What the update system reads from GitHub, without any network or UI, so it can be tested on made-up answers.
//
// The CI publishes each successful build of main as the one release `latest-main`. Its body says which commit it is
// (see .github/workflows/build.yml), so an install compares that commit with the one it was built from.
namespace update {

struct Release {
    QString commit;          // full hash, from the release's body
    qint64 commitTime = 0;   // seconds since the epoch, from the release's body
    QString assetName;       // the .dmg
    QUrl assetUrl;           // the API's address for the asset, not the browser's: it also works on a private repository
    qint64 assetSize = 0;
    QString sha256;          // lower case hex; empty when GitHub didn't give a digest

    bool isValid() const { return !commit.isEmpty() && commitTime > 0 && assetUrl.isValid() && !assetUrl.isEmpty(); }
};

// An invalid Release when the answer isn't a build the CI published, or holds no .dmg.
Release parseRelease(const QByteArray &json);

// True when the release should be offered to a build of this commit made at this time. Only a later commit counts: a
// build from a checkout ahead of the last published one, the developer's own, must not be told to go back. A build
// that doesn't know its commit (made without git) takes any release.
bool isNewer(const QString &currentCommit, qint64 currentTime, const Release &release);

// The subject lines of a compare answer, newest first, at most `limit`.
QStringList parseCompareSubjects(const QByteArray &json, int limit = 30);

// The first 7 characters of a hash, as git shows it.
QString shortCommit(const QString &commit);

// Where to install, given the running bundle: itself, or /Applications/deepwater.app for a build inside a CMake build
// directory (`hasCMakeCache`), which the next `cmake --build` would otherwise write over a foreign bundle. Empty when
// the app doesn't run from a bundle, or runs from a translocated copy that macOS deletes (a quarantined download not
// moved out of its folder or its disk image).
QString installTarget(const QString &bundlePath, bool hasCMakeCache);

} // namespace update
