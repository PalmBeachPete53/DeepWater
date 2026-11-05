#include "PageTranslator.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QProcess>
#include <QTimer>

namespace PageTranslator {

namespace {

QString helperPath()
{
    return QCoreApplication::applicationDirPath() + "/deepwater-translate";
}

Result read(const QByteArray &output)
{
    Result r;
    const QJsonObject o = QJsonDocument::fromJson(output).object();
    r.source = o.value("source").toString();
    const QString error = o.value("error").toString();
    if (error == "same_language") r.failure = Failure::SameLanguage;
    else if (error == "not_installed") r.failure = Failure::NotInstalled;
    else if (error == "unsupported") r.failure = Failure::Unsupported;
    else if (error == "system_too_old") r.failure = Failure::SystemTooOld;
    else if (!error.isEmpty() || !o.value("texts").isArray()) r.failure = Failure::Failed;
    for (const QJsonValue &v : o.value("texts").toArray())
        r.texts << v.toString();
    return r;
}

} // namespace

bool available()
{
    return QFileInfo(helperPath()).isExecutable();
}

void translate(const QStringList &texts, const QString &target, QObject *ctx, std::function<void(const Result &)> done)
{
    auto *process = new QProcess(ctx);
    auto *timeout = new QTimer(process);
    timeout->setSingleShot(true);
    QObject::connect(timeout, &QTimer::timeout, process, &QProcess::kill);
    QObject::connect(process, &QProcess::finished, ctx, [process, done] {
        const Result r = read(process->readAllStandardOutput());
        process->deleteLater();
        done(r);
    });
    QObject::connect(process, &QProcess::errorOccurred, ctx, [process, done](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;   // the others end in finished()
        Result r;
        r.failure = Failure::Failed;
        process->deleteLater();
        done(r);
    });
    process->start(helperPath(), QStringList());
    process->write(QJsonDocument(QJsonObject {{"target", target}, {"texts", QJsonArray::fromStringList(texts)}}).toJson(QJsonDocument::Compact));
    process->closeWriteChannel();
    timeout->start(3 * 60 * 1000);
}

} // namespace PageTranslator
