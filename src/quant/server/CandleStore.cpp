#include "CandleStore.h"

#include "../core/Error.h"
#include "../core/Series.h"

#include <QCryptographicHash>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

#include <algorithm>
#include <cmath>

namespace qtm {

namespace {
constexpr quint32 Magic = 0x44574342;   // "DWCB"
constexpr quint32 Version = 1;
}

CandleRefresh planCandleRefresh(const StoredCandles &stored, const QString &tf, int limit, qint64 now)
{
    if (stored.set.candles.isEmpty() || stored.limit < limit || now - stored.fullAt > CandleStore::FullAfter
        || now < stored.fullAt)
        return {};
    qint64 seconds = 0;
    try {
        seconds = parseTf(tf).seconds;
    } catch (const Error &) {
        return {};
    }
    if (seconds <= 0)
        return {};
    const double missed = std::ceil(double(now - stored.set.candles.last().time) / double(seconds)) + 2;
    if (missed >= limit)
        return {};
    return {false, int(std::max(5.0, missed))};
}

CandleStore::CandleStore(const QString &dir) : m_dir(dir) {}

// A hash: symbols hold characters files can't, and 1m and 1M are the same file name on Windows and macOS.
QString CandleStore::fileOf(const QString &symbol, const QString &tf) const
{
    const QByteArray key = (symbol + QLatin1Char('|') + tf).toUtf8();
    return m_dir + QLatin1Char('/') + QString::fromLatin1(QCryptographicHash::hash(key, QCryptographicHash::Sha1).toHex()) + ".candles";
}

std::optional<StoredCandles> CandleStore::read(const QString &symbol, const QString &tf) const
{
    QFile file(fileOf(symbol, tf));
    if (!file.open(QIODevice::ReadOnly))
        return std::nullopt;
    QDataStream in(&file);
    in.setVersion(QDataStream::Qt_6_0);
    quint32 magic = 0, version = 0;
    in >> magic >> version;
    if (magic != Magic || version != Version)
        return std::nullopt;
    StoredCandles s;
    qint32 limit = 0, count = 0;
    in >> s.set.symbol >> s.set.provider >> limit >> s.fullAt >> count;
    if (in.status() != QDataStream::Ok || count < 0 || count > 1000000)
        return std::nullopt;
    s.limit = limit;
    s.set.candles.reserve(count);
    for (qint32 i = 0; i < count; ++i) {
        Bar b;
        in >> b.time >> b.open >> b.high >> b.low >> b.close >> b.volume;
        s.set.candles.push_back(b);
    }
    if (in.status() != QDataStream::Ok)
        return std::nullopt;   // cut short: as if it weren't there
    return s;
}

bool CandleStore::write(const QString &symbol, const QString &tf, const StoredCandles &stored) const
{
    if (!QDir().mkpath(m_dir))
        return false;
    QSaveFile file(fileOf(symbol, tf));
    if (!file.open(QIODevice::WriteOnly))
        return false;
    QDataStream out(&file);
    out.setVersion(QDataStream::Qt_6_0);
    out << Magic << Version << stored.set.symbol << stored.set.provider << qint32(stored.limit) << stored.fullAt
        << qint32(stored.set.candles.size());
    for (const Bar &b : stored.set.candles)
        out << b.time << b.open << b.high << b.low << b.close << b.volume;
    if (!file.commit())
        return false;
    trim();
    return true;
}

void CandleStore::trim() const
{
    const QFileInfoList files = QDir(m_dir).entryInfoList({"*.candles"}, QDir::Files, QDir::Time);   // newest first
    for (int i = MaxFiles; i < files.size(); ++i)
        QFile::remove(files[i].absoluteFilePath());
}

} // namespace qtm
