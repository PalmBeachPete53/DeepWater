#include "HttpCache.h"

#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QStandardPaths>
#include <QThreadPool>

namespace {

constexpr quint32 Magic = 0x44574843;   // "DWHC"

QString pathFor(const QString &name)
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/cache_" + name + ".bin";
}

} // namespace

namespace HttpCache {

bool load(const QString &name, QByteArray *body, QDateTime *fetchedAt)
{
    QFile file(pathFor(name));
    if (!file.open(QIODevice::ReadOnly))
        return false;
    QDataStream in(&file);
    quint32 magic = 0;
    in >> magic >> *fetchedAt >> *body;
    return magic == Magic && in.status() == QDataStream::Ok;
}

QThreadPool *writer()
{
    static QThreadPool *pool = [] {
        auto *p = new QThreadPool;
        p->setMaxThreadCount(1);   // in order
        return p;
    }();
    return pool;
}

void save(const QString &name, const QByteArray &body, const QDateTime &fetchedAt)
{
    // The path is worked out here: it needs the application's name.
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString path = pathFor(name);
    writer()->start([dir, path, body, fetchedAt] {
        QDir().mkpath(dir);
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly))
            return;
        QDataStream out(&file);
        out << Magic << fetchedAt << body;
        file.commit();
    });
}

void waitForWrites()
{
    writer()->waitForDone();
}

} // namespace HttpCache
