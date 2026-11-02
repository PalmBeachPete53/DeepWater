#include "VesselStore.h"

#include "GeoData.h"

#include <QDataStream>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QtConcurrent/QtConcurrentRun>

namespace {
constexpr qint64 StaleMs = 30 * 60 * 1000;     // drop vessels silent for 30 minutes
constexpr qint64 TrailStepMs = 60 * 1000;      // at most one trail point per minute
constexpr int TrailLength = 60;
constexpr quint32 CacheMagic = 0x44575653;   // "DWVS"
constexpr quint16 CacheVersion = 2;           // v2 adds when each ship was last heard of
constexpr qint64 StaticKeepMs = 30LL * 24 * 3600 * 1000;   // forget ships not heard of for a month
constexpr qint64 StaticTouchMs = 24LL * 3600 * 1000;       // refresh an unchanged entry's date daily

QString cachePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/vessel_static.cache";
}

Vessel::Class classify(int shipType)
{
    if (shipType == 0)
        return Vessel::Unknown;
    if (shipType >= 80 && shipType <= 89)
        return Vessel::Tanker;
    if (shipType >= 70 && shipType <= 79)
        return Vessel::Cargo;
    return Vessel::Other;
}
}

Vessel::Class Vessel::vesselClass() const
{
    return classify(shipType);
}

VesselStore::VesselStore(QObject *parent)
    : QObject(parent)
{
    // Classes hidden in the previous session; all of them on the first launch.
    const QSettings settings;
    if (settings.contains("hiddenVesselClasses")) {
        m_hidden.clear();
        for (const QVariant &cls : settings.value("hiddenVesselClasses").toList())
            m_hidden.insert(cls.toInt());
    }

    m_emitTimer.setInterval(1000);
    connect(&m_emitTimer, &QTimer::timeout, this, [this] {
        if (m_dirty) {
            m_dirty = false;
            ++m_revision;
            emit changed();
        }
    });
    m_emitTimer.start();

    m_pruneTimer.setInterval(60 * 1000);
    connect(&m_pruneTimer, &QTimer::timeout, this, &VesselStore::prune);
    m_pruneTimer.start();

    loadCache();
    m_saveTimer.setInterval(2 * 60 * 1000);
    connect(&m_saveTimer, &QTimer::timeout, this, &VesselStore::saveCache);
    m_saveTimer.start();
}

VesselStore::~VesselStore()
{
    m_cacheWrite.waitForFinished();
    if (m_cacheDirty)
        writeCache(m_static);
}

void VesselStore::applyStatic(Vessel &v, const StaticInfo &info)
{
    v.shipType = info.shipType;
    if (!info.name.isEmpty())
        v.name = info.name;
    v.destination = info.destination;
    v.callSign = info.callSign;
    v.imo = info.imo;
    v.draught = info.draught;
    v.length = info.length;
}

void VesselStore::loadCache()
{
    QFile file(cachePath());
    if (!file.open(QIODevice::ReadOnly))
        return;
    QDataStream in(&file);
    quint32 magic = 0;
    quint16 version = 0;
    quint32 count = 0;
    in >> magic >> version >> count;
    if (magic != CacheMagic || version < 1 || version > CacheVersion)
        return;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    m_static.reserve(int(count));
    for (quint32 i = 0; i < count && in.status() == QDataStream::Ok; ++i) {
        qint64 mmsi;
        StaticInfo s;
        qint32 type, length;
        in >> mmsi >> type >> s.name >> s.destination >> s.callSign >> s.imo >> s.draught >> length;
        if (version >= 2)
            in >> s.lastSeenMs;
        else
            s.lastSeenMs = now;   // a v1 file has no dates: keep its ships for a month from now
        s.shipType = type;
        s.length = length;
        if (now - s.lastSeenMs < StaticKeepMs)
            m_static.insert(mmsi, s);
    }
    if (version < CacheVersion || m_static.size() < qsizetype(count))
        m_cacheDirty = true;   // rewritten in the current format, without the forgotten ships
}

void VesselStore::saveCache()
{
    if (!m_cacheDirty || m_cacheWrite.isRunning())
        return;
    const qint64 cutoff = QDateTime::currentMSecsSinceEpoch() - StaticKeepMs;
    m_static.removeIf([cutoff](const auto &entry) { return entry.value().lastSeenMs < cutoff; });
    m_cacheDirty = false;
    // Written on the thread pool from a shared copy of the table (megabytes: a pause on the UI thread
    // every two minutes otherwise). Marked dirty again if the write fails.
    m_cacheWrite = QtConcurrent::run(&VesselStore::writeCache, m_static);
    m_cacheWrite.then(this, [this](bool ok) {
        if (!ok)
            m_cacheDirty = true;
    });
}

bool VesselStore::writeCache(const QHash<qint64, StaticInfo> &entries)
{
    QDir().mkpath(QFileInfo(cachePath()).absolutePath());
    QSaveFile file(cachePath());
    if (!file.open(QIODevice::WriteOnly))
        return false;
    QDataStream out(&file);
    out << CacheMagic << CacheVersion << quint32(entries.size());
    for (auto it = entries.cbegin(); it != entries.cend(); ++it) {
        const StaticInfo &s = *it;
        out << it.key() << qint32(s.shipType) << s.name << s.destination << s.callSign << s.imo
            << s.draught << qint32(s.length) << s.lastSeenMs;
    }
    return file.commit();
}

void VesselStore::updatePosition(qint64 mmsi, const QString &name, double lat, double lon,
                                 double sog, double cog, int heading)
{
    // 91/181 mean "not available" in AIS; (0, 0) is almost always a bad fix.
    if (lat < -90 || lat > 90 || lon < -180 || lon > 180 || (lat == 0 && lon == 0))
        return;
    const auto known = m_static.constFind(mmsi);
    if (known != m_static.constEnd() && classify(known->shipType) == Vessel::Other)
        return;

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    auto it = m_vessels.find(mmsi);
    if (it == m_vessels.end()) {
        it = m_vessels.insert(mmsi, Vessel());
        it->mmsi = mmsi;
        if (known != m_static.constEnd())
            applyStatic(*it, *known);
        track(*it, +1);
    }
    Vessel &v = *it;
    if (!name.trimmed().isEmpty())
        v.name = name.trimmed();
    v.lat = lat;
    v.lon = lon;
    v.sog = sog < 102.3 ? sog : 0;       // 102.3 = not available
    v.cog = cog < 360 ? cog : 0;         // 360 = not available
    v.heading = heading;
    v.lastSeenMs = now;
    v.pos = Geo::toVector(lat, lon);
    if (now - v.lastTrailMs >= TrailStepMs) {
        v.trail.push_back(v.pos);
        if (v.trail.size() > TrailLength)
            v.trail.removeFirst();
        v.lastTrailMs = now;
    }
    markDirty();
    emit vesselUpdated(mmsi);
}

void VesselStore::updateStatic(qint64 mmsi, const QString &name, int shipType,
                               const QString &destination, const QString &callSign, qint64 imo,
                               double draught, int length)
{
    StaticInfo info;
    info.shipType = shipType;
    info.name = name.trimmed();
    info.destination = destination.trimmed();
    info.callSign = callSign.trimmed();
    info.imo = imo;
    info.draught = draught;
    info.length = length;
    // Ships repeat their static report every few minutes: the cache is only rewritten for news, or
    // daily per ship to keep its date (ships not heard of for a month are forgotten).
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const auto known = m_static.constFind(mmsi);
    if (known == m_static.constEnd() || !known->sameAs(info) || now - known->lastSeenMs > StaticTouchMs) {
        info.lastSeenMs = now;
        m_static.insert(mmsi, info);
        m_cacheDirty = true;
    }

    if (classify(shipType) == Vessel::Other) {
        // Not a commodity carrier (fishing, passenger, tug...): forget it.
        const auto gone = m_vessels.constFind(mmsi);
        if (gone != m_vessels.constEnd()) {
            track(*gone, -1);
            m_vessels.erase(gone);
            markDirty();
        }
        return;
    }
    auto it = m_vessels.find(mmsi);
    if (it == m_vessels.end())
        return;   // no position yet; applied when one arrives
    track(*it, -1);
    applyStatic(*it, info);
    track(*it, +1);
    markDirty();
    emit vesselUpdated(mmsi);
}

void VesselStore::clear()
{
    m_vessels.clear();
    std::fill(std::begin(m_classCount), std::end(m_classCount), 0);
    ++m_revision;
    emit changed();
}

const Vessel *VesselStore::vessel(qint64 mmsi) const
{
    auto it = m_vessels.constFind(mmsi);
    return it == m_vessels.constEnd() ? nullptr : &*it;
}

int VesselStore::count(Vessel::Class cls) const
{
    return m_classCount[cls];
}

void VesselStore::setClassVisible(Vessel::Class cls, bool visible)
{
    if (visible == isClassVisible(cls))
        return;
    if (visible)
        m_hidden.remove(cls);
    else
        m_hidden.insert(cls);
    QVariantList hidden;
    for (int c : std::as_const(m_hidden))
        hidden << c;
    QSettings().setValue("hiddenVesselClasses", hidden);
    emit filterChanged();
}

void VesselStore::markDirty()
{
    m_dirty = true;
}

void VesselStore::prune()
{
    const qint64 cutoff = QDateTime::currentMSecsSinceEpoch() - StaleMs;
    for (auto it = m_vessels.begin(); it != m_vessels.end();) {
        if (it->lastSeenMs < cutoff) {
            track(*it, -1);
            it = m_vessels.erase(it);
        } else {
            ++it;
        }
    }
    markDirty();
}
