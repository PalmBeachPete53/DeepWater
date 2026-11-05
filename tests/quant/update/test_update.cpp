// What the update system reads from GitHub, on made-up answers. Plain checks, a count at the end.
#include "UpdateRelease.h"

#include <QCoreApplication>

#include <iostream>

using namespace update;

namespace {

int checks = 0;
int fails = 0;

void check(bool ok, const char *what)
{
    ++checks;
    if (!ok) {
        ++fails;
        std::cout << "FAIL " << what << '\n';
    }
}

const QString HashA = QString(40, 'a');
const QString HashB = QString(40, 'b');

QByteArray releaseJson(const QString &body, const QString &assets)
{
    return QString(R"({"tag_name":"latest-main","body":%1,"assets":[%2]})").arg(body, assets).toUtf8();
}

QString dmgAsset()
{
    return R"({"name":"Deepwater-0.1-abc-macOS-arm64.dmg","url":"https://api.github.com/repos/o/r/releases/assets/1","size":1234,"digest":"sha256:ABCDEF"})";
}

void testParseRelease()
{
    const QString body = QString(R"("Build of main\ncommit: %1\ntime: 1000\n")").arg(HashB);
    Release r = parseRelease(releaseJson(body, dmgAsset()));
    check(r.isValid(), "a published build parses");
    check(r.commit == HashB, "its commit");
    check(r.commitTime == 1000, "its time");
    check(r.assetName.endsWith(".dmg"), "its asset");
    check(r.assetSize == 1234, "its size");
    check(r.sha256 == "abcdef", "its digest, in lower case");
    check(r.assetUrl.toString().endsWith("/assets/1"), "the API's address for the asset");

    r = parseRelease(releaseJson(body, R"({"name":"notes.txt","url":"https://x/1","size":1})" ", " + dmgAsset()));
    check(r.assetName.endsWith(".dmg"), "the .dmg among other assets");

    check(!parseRelease(releaseJson(body, R"({"name":"notes.txt","url":"https://x/1","size":1})")).isValid(), "no .dmg, no build");
    check(!parseRelease(releaseJson(R"("no commit here")", dmgAsset())).isValid(), "a body without a commit");
    check(!parseRelease(releaseJson(QString(R"("commit: %1")").arg(HashB), dmgAsset())).isValid(), "a body without a time");
    check(!parseRelease(releaseJson(R"("commit: short\ntime: 1000")", dmgAsset())).isValid(), "a commit that isn't a full hash");
    check(!parseRelease("not json").isValid(), "not JSON");
    check(!parseRelease(R"({"message":"Not Found"})").isValid(), "an error answer");
}

void testIsNewer()
{
    Release r = parseRelease(releaseJson(QString(R"("commit: %1\ntime: 1000")").arg(HashB), dmgAsset()));
    check(isNewer(HashA, 500, r), "a later commit is offered");
    check(!isNewer(HashB, 1000, r), "the same commit isn't");
    check(!isNewer(HashA, 2000, r), "a build ahead of the release isn't told to go back");
    check(!isNewer(HashA, 1000, r), "the same time isn't later");
    check(isNewer(QString(), 0, r), "a build that doesn't know its commit takes any release");
    check(!isNewer(HashA, 500, Release()), "an invalid release never is");
}

void testCompare()
{
    const QByteArray json = R"({"commits":[{"commit":{"message":"Oldest\n\nbody"}},{"commit":{"message":"Middle"}},{"commit":{"message":"Newest\nmore"}}]})";
    const QStringList s = parseCompareSubjects(json);
    check(s == QStringList({"Newest", "Middle", "Oldest"}), "subjects only, newest first");
    check(parseCompareSubjects(json, 2) == QStringList({"Newest", "Middle"}), "the limit keeps the newest");
    check(parseCompareSubjects("{}").isEmpty(), "no commits");
    check(shortCommit(HashA).size() == 7, "a short hash");
}

void testInstallTarget()
{
    check(installTarget("/Applications/deepwater.app", false) == "/Applications/deepwater.app", "installed: itself");
    check(installTarget("/Users/me/dev/build/deepwater.app", true) == "/Applications/deepwater.app", "a build folder: Applications");
    check(installTarget("/Users/me/Tools/deepwater.app", false) == "/Users/me/Tools/deepwater.app", "anywhere else: itself");
    check(installTarget("/usr/local/bin", false).isEmpty(), "not a bundle");
    check(installTarget("/private/var/folders/x/AppTranslocation/1/d/deepwater.app", false).isEmpty(), "a translocated copy");
    check(installTarget("/Volumes/Deepwater/deepwater.app", false).isEmpty(), "still on its disk image");
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    testParseRelease();
    testIsNewer();
    testCompare();
    testInstallTarget();
    std::cout << checks << " checks, " << fails << " failed\n";
    return fails ? 1 : 0;
}
