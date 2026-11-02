#include "Store.h"

#include "../core/Error.h"
#include "Json.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#ifdef _WIN32
#include <QDateTime>
#include <QFileInfo>
#else
#include <sys/stat.h>
#endif

namespace qtm {

Store::Store(const QString &dataDir) : m_dir(dataDir)
{
    QDir().mkpath(m_dir);
}

QJsonValue Store::read(const QString &name, const QJsonValue &fallback) const
{
    QFile f(QDir(m_dir).filePath(name + ".json"));
    if (!f.open(QIODevice::ReadOnly))
        return fallback;
    bool ok = false;
    const QJsonValue v = jsv::parse(f.readAll(), &ok);
    return ok ? v : fallback;
}

void Store::write(const QString &name, const QJsonValue &value)
{
    QMutexLocker lock(&m_writeMutex);
    QDir().mkpath(m_dir);
    const QString file = QDir(m_dir).filePath(name + ".json");
    const QString tmp = QStringLiteral("%1.%2.tmp").arg(file).arg(QCoreApplication::applicationPid());
    const QByteArray text = jsv::stringify(value, 2);
    {
        QFile f(tmp);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate) || f.write(text) != text.size())
            throw Error(QStringLiteral("Couldn't write %1: %2").arg(tmp, f.errorString()));
    }
    if (std::rename(QFile::encodeName(tmp).constData(), QFile::encodeName(file).constData()) != 0) {
        const int e = errno;
        // Windows refuses to rename over a file that antivirus or the indexer has open.
        // Fall back to writing in place rather than losing the save.
        if (e != EPERM && e != EBUSY && e != EACCES)
            throw Error(QStringLiteral("Couldn't save %1: %2").arg(file, QString::fromLocal8Bit(std::strerror(e))));
        QFile f(file);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) f.write(text);
        QFile::remove(tmp);
    }
}

double mtimeMs(const QString &path)
{
#ifdef _WIN32
    const QFileInfo fi(path);
    return fi.exists() ? double(fi.lastModified().toMSecsSinceEpoch()) : -1;
#else
    struct stat st;
    if (::stat(QFile::encodeName(path).constData(), &st) != 0)
        return -1;
#ifdef __APPLE__
    return double(st.st_mtimespec.tv_sec) * 1000.0 + double(st.st_mtimespec.tv_nsec) / 1e6;
#else
    return double(st.st_mtim.tv_sec) * 1000.0 + double(st.st_mtim.tv_nsec) / 1e6;
#endif
#endif
}

QStringList readDir(const QString &dir, bool *exists)
{
    QDir d(dir);
    if (exists) *exists = d.exists();
    if (!d.exists()) return {};
    QStringList names = d.entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot, QDir::Unsorted);
    std::sort(names.begin(), names.end(), [](const QString &a, const QString &b) {
        return a.toUtf8() < b.toUtf8();
    });
    return names;
}

} // namespace qtm
