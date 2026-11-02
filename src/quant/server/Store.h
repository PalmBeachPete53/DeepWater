#pragma once

// Port of lib/store.js: everything you save (layouts, watchlists, drawings, alerts, paper account)
// lives in the data folder as plain JSON files. Writes go to a temp file first so a crash can't
// corrupt them.

#include <QJsonValue>
#include <QMutex>
#include <QString>

namespace qtm {

class Store
{
public:
    explicit Store(const QString &dataDir);

    QString dataDir() const { return m_dir; }
    QJsonValue read(const QString &name, const QJsonValue &fallback) const;
    void write(const QString &name, const QJsonValue &value);

private:
    QString m_dir;
    QMutex m_writeMutex;    // one writer at a time: the temp file name is per process, like the JS
};

// fs.statSync(file).mtimeMs, with sub-millisecond precision. -1 when the file doesn't exist.
double mtimeMs(const QString &path);
// fs.readdirSync(dir): names sorted byte-wise (libuv sorts them). Empty when the folder is missing.
QStringList readDir(const QString &dir, bool *exists = nullptr);

} // namespace qtm
