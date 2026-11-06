#include "UpdateRelease.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace update {

Release parseRelease(const QByteArray &json, const QString &suffix)
{
    const QJsonObject root = QJsonDocument::fromJson(json).object();
    Release r;
    // The body: "commit: <hash>" and "time: <seconds>", each on its line, anything else ignored.
    const QString body = root.value("body").toString();
    static const QRegularExpression commitLine("^commit:\\s*([0-9a-f]{40})\\s*$", QRegularExpression::MultilineOption);
    static const QRegularExpression timeLine("^time:\\s*(\\d+)\\s*$", QRegularExpression::MultilineOption);
    if (const auto m = commitLine.match(body); m.hasMatch())
        r.commit = m.captured(1);
    if (const auto m = timeLine.match(body); m.hasMatch())
        r.commitTime = m.captured(1).toLongLong();
    static const QRegularExpression changeLine("^change:\\s*([0-9a-f]{40})\\s+(.+?)\\s*$", QRegularExpression::MultilineOption);
    for (auto it = changeLine.globalMatch(body); it.hasNext();) {
        const QRegularExpressionMatch m = it.next();
        r.changeCommits << m.captured(1);
        r.changeSubjects << m.captured(2);
    }
    for (const QJsonValue &v : root.value("assets").toArray()) {
        const QJsonObject asset = v.toObject();
        const QString name = asset.value("name").toString();
        if (!name.endsWith(suffix, Qt::CaseInsensitive))
            continue;
        r.assetName = name;
        r.assetUrl = QUrl(asset.value("url").toString());
        r.browserUrl = QUrl(asset.value("browser_download_url").toString());
        r.assetSize = qint64(asset.value("size").toDouble());
        const QString digest = asset.value("digest").toString();
        if (digest.startsWith("sha256:"))
            r.sha256 = digest.mid(7).toLower();
        break;
    }
    return r.isValid() ? r : Release();
}

bool isNewer(const QString &currentCommit, qint64 currentTime, const Release &release)
{
    if (!release.isValid() || release.commit == currentCommit)
        return false;
    if (currentCommit.isEmpty() || currentTime <= 0)
        return true;
    return release.commitTime > currentTime;
}

QStringList changesSince(const Release &release, const QString &currentCommit)
{
    QStringList out;
    for (int i = 0; i < release.changeCommits.size(); ++i) {
        if (release.changeCommits[i] == currentCommit)
            break;
        out << release.changeSubjects.value(i);
    }
    return out;
}

bool isBuildSignal(const QByteArray &json)
{
    return QJsonDocument::fromJson(json).object().value("event").toString() == QLatin1String("message");
}

QString shortCommit(const QString &commit)
{
    return commit.left(7);
}

QString installTarget(const QString &bundlePath, bool hasCMakeCache)
{
    if (!bundlePath.endsWith(".app") || bundlePath.contains("/AppTranslocation/") || bundlePath.startsWith("/Volumes/"))
        return {};
    return hasCMakeCache ? QStringLiteral("/Applications/deepwater.app") : bundlePath;
}

} // namespace update
