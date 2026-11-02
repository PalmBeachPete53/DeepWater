#include "Chokepoints.h"

#include "GeoData.h"
#include "VesselStore.h"

#include <QCoreApplication>
#include <QDataStream>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

namespace {

constexpr double EarthRadiusKm = 6371.0;
constexpr qint64 HourMs = 3600 * 1000;
constexpr qint64 RevisitGapMs = 6 * HourMs;   // a ship re-entering after this counts again
constexpr qint64 KeepMs = (ChokepointMonitor::WindowHours + 1) * HourMs;
constexpr qint64 InZoneFreshMs = 30 * 60 * 1000;
constexpr quint32 FileMagic = 0x44574350;     // "DWCP"
constexpr quint16 FileVersion = 2;   // v2 adds the recorded hours

QString eventsPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/chokepoint_events.bin";
}

qint64 key(qint64 mmsi, int zone)
{
    return mmsi * 16 + zone;
}

} // namespace

ChokepointMonitor::ChokepointMonitor(VesselStore *store, QObject *parent)
    : QObject(parent)
    , m_store(store)
{
    // Zones are circles generous enough to catch ships between sparse AIS reports.
    m_zones = {
        {"hormuz", QT_TRANSLATE_NOOP("Chokepoints", "Strait of Hormuz"),
         QT_TRANSLATE_NOOP("Chokepoints", "Gulf crude, products and LNG exports"), 26.55, 56.45, 60, {}, 0, "chokepoint6", ""},
        {"babelmandeb", QT_TRANSLATE_NOOP("Chokepoints", "Bab-el-Mandeb"),
         QT_TRANSLATE_NOOP("Chokepoints", "Red Sea gateway between Suez and the Indian Ocean"), 12.6, 43.35, 50, {}, 0, "chokepoint4", ""},
        {"suez", QT_TRANSLATE_NOOP("Chokepoints", "Suez Canal"),
         QT_TRANSLATE_NOOP("Chokepoints", "Shortest sea route between Europe and Asia"), 30.55, 32.35, 95, {}, 0, "chokepoint1", ""},
        {"singapore", QT_TRANSLATE_NOOP("Chokepoints", "Singapore Strait"),
         QT_TRANSLATE_NOOP("Chokepoints", "Malacca route between the Indian Ocean and the Pacific"), 1.2, 103.9, 60, {}, 0, "chokepoint5", "Malacca Strait"},
        {"panama", QT_TRANSLATE_NOOP("Chokepoints", "Panama Canal"),
         QT_TRANSLATE_NOOP("Chokepoints", "Atlantic–Pacific link for grain, LPG and containers"), 9.1, -79.7, 50, {}, 0, "chokepoint2", ""},
        {"bosphorus", QT_TRANSLATE_NOOP("Chokepoints", "Bosphorus"),
         QT_TRANSLATE_NOOP("Chokepoints", "Black Sea grain and oil exports"), 41.1, 29.05, 25, {}, 0, "chokepoint3", ""},
        {"gibraltar", QT_TRANSLATE_NOOP("Chokepoints", "Strait of Gibraltar"),
         QT_TRANSLATE_NOOP("Chokepoints", "Gateway between the Mediterranean and the Atlantic"), 35.95, -5.6, 40, {}, 0, "chokepoint8", ""},
        {"dover", QT_TRANSLATE_NOOP("Chokepoints", "Dover Strait"),
         QT_TRANSLATE_NOOP("Chokepoints", "Lane to and from the North Sea ports"), 51.0, 1.45, 40, {}, 0, "chokepoint9", ""},
        {"danish", QT_TRANSLATE_NOOP("Chokepoints", "Danish Straits"),
         QT_TRANSLATE_NOOP("Chokepoints", "Entrance to the Baltic Sea"), 55.6, 11.5, 80, {}, 0, "chokepoint10", "Oresund Strait"},
        {"goodhope", QT_TRANSLATE_NOOP("Chokepoints", "Cape of Good Hope"),
         QT_TRANSLATE_NOOP("Chokepoints", "Route around Africa for ships avoiding Suez"), -34.8, 19.0, 150, {}, 0, "chokepoint7", ""},
    };
    for (Chokepoint &z : m_zones) {
        z.center = Geo::toVector(z.lat, z.lon);
        z.cosRadius = float(std::cos(z.radiusKm / EarthRadiusKm));
    }

    m_lastReportMs.fill(0, m_zones.size());
    load();
    connect(m_store, &VesselStore::vesselUpdated, this, &ChokepointMonitor::onVesselUpdated);
    m_saveTimer.setInterval(2 * 60 * 1000);
    connect(&m_saveTimer, &QTimer::timeout, this, [this] {
        prune();
        save();
    });
    m_saveTimer.start();
}

ChokepointMonitor::~ChokepointMonitor()
{
    save();
}

QString ChokepointMonitor::name(int zone) const
{
    return QCoreApplication::translate("Chokepoints", m_zones[zone].name);
}

QString ChokepointMonitor::description(int zone) const
{
    return QCoreApplication::translate("Chokepoints", m_zones[zone].description);
}

int ChokepointMonitor::zoneAt(const QVector3D &pos) const
{
    for (int i = 0; i < m_zones.size(); ++i)
        if (QVector3D::dotProduct(pos, m_zones[i].center) >= m_zones[i].cosRadius)
            return i;
    return -1;
}

void ChokepointMonitor::onVesselUpdated(qint64 mmsi)
{
    const Vessel *v = m_store->vessel(mmsi);
    if (!v)
        return;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (!m_recordedHours.contains(now / HourMs)) {
        m_recordedHours.insert(now / HourMs);
        m_dirty = true;
    }
    const int zone = zoneAt(v->pos);
    if (zone < 0)
        return;
    m_lastReportMs[zone] = now;

    const Vessel::Class cls = v->vesselClass();
    if (cls != Vessel::Tanker && cls != Vessel::Cargo)
        return;
    qint64 &last = m_lastInZone[key(mmsi, zone)];
    if (now - last > RevisitGapMs) {
        m_events.push_back({mmsi, now, qint8(zone), qint8(cls)});
        m_dirty = true;
        emit passageRecorded(zone);
    }
    last = now;
}

void ChokepointMonitor::refreshInZone() const
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (!m_inZone.isEmpty() && m_inZoneRevision == m_store->revision() && now - m_inZoneMs < 1000)
        return;
    m_inZoneRevision = m_store->revision();
    m_inZoneMs = now;
    m_inZone = QVector<Counts>(m_zones.size());
    m_inZoneShips = QVector<QVector<qint64>>(m_zones.size());
    const qint64 cutoff = now - InZoneFreshMs;
    for (const Vessel &v : m_store->vessels()) {
        if (v.lastSeenMs < cutoff)
            continue;
        const Vessel::Class cls = v.vesselClass();
        for (int zone = 0; zone < m_zones.size(); ++zone) {
            const Chokepoint &z = m_zones[zone];
            if (QVector3D::dotProduct(v.pos, z.center) < z.cosRadius)
                continue;
            Counts &c = m_inZone[zone];
            switch (cls) {
            case Vessel::Tanker: ++c.tankers; break;
            case Vessel::Cargo: ++c.cargo; break;
            case Vessel::Unknown: ++c.unknown; break;
            default: break;
            }
            if (cls == Vessel::Tanker || cls == Vessel::Cargo)
                m_inZoneShips[zone].push_back(v.mmsi);
        }
    }
}

ChokepointMonitor::Counts ChokepointMonitor::inZoneNow(int zone) const
{
    refreshInZone();
    return m_inZone[zone];
}

QVector<qint64> ChokepointMonitor::vesselsInZone(int zone) const
{
    refreshInZone();
    // A ship can have left the store since (pruned, or found not to be a carrier).
    QVector<qint64> out;
    for (qint64 mmsi : std::as_const(m_inZoneShips[zone]))
        if (m_store->vessel(mmsi))
            out.push_back(mmsi);
    return out;
}

bool ChokepointMonitor::hasCoverage(int zone) const
{
    return QDateTime::currentMSecsSinceEpoch() - m_lastReportMs[zone] < InZoneFreshMs;
}

ChokepointMonitor::Counts ChokepointMonitor::passages(int zone, qint64 windowMs) const
{
    Counts c;
    const qint64 cutoff = QDateTime::currentMSecsSinceEpoch() - windowMs;
    for (const Event &e : m_events) {
        if (e.zone != zone || e.timeMs < cutoff)
            continue;
        if (e.vesselClass == Vessel::Tanker) ++c.tankers; else ++c.cargo;
    }
    return c;
}

QVector<ChokepointMonitor::Counts> ChokepointMonitor::hourly(int zone) const
{
    QVector<Counts> buckets(WindowHours);
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 currentHourStart = now - now % HourMs;
    for (const Event &e : m_events) {
        if (e.zone != zone)
            continue;
        const qint64 hoursAgo = (currentHourStart - (e.timeMs - e.timeMs % HourMs)) / HourMs;
        if (hoursAgo < 0 || hoursAgo >= WindowHours)
            continue;
        Counts &b = buckets[WindowHours - 1 - int(hoursAgo)];
        if (e.vesselClass == Vessel::Tanker) ++b.tankers; else ++b.cargo;
    }
    return buckets;
}

QVector<bool> ChokepointMonitor::recordedHours() const
{
    QVector<bool> out(WindowHours, false);
    const qint64 currentHour = QDateTime::currentMSecsSinceEpoch() / HourMs;
    for (int i = 0; i < WindowHours; ++i)
        out[i] = m_recordedHours.contains(currentHour - (WindowHours - 1 - i));
    return out;
}

int ChokepointMonitor::recordedHourCount() const
{
    const QVector<bool> hours = recordedHours();
    return int(std::count(hours.begin(), hours.end(), true));
}

void ChokepointMonitor::prune()
{
    const qint64 cutoff = QDateTime::currentMSecsSinceEpoch() - KeepMs;
    for (auto it = m_recordedHours.begin(); it != m_recordedHours.end();) {
        if (*it * HourMs < cutoff) it = m_recordedHours.erase(it); else ++it;
    }
    const auto end = std::remove_if(m_events.begin(), m_events.end(),
                                    [cutoff](const Event &e) { return e.timeMs < cutoff; });
    if (end != m_events.end()) {
        m_events.erase(end, m_events.end());
        m_dirty = true;
    }
    for (auto it = m_lastInZone.begin(); it != m_lastInZone.end();) {
        if (it.value() < cutoff) it = m_lastInZone.erase(it); else ++it;
    }
}

void ChokepointMonitor::load()
{
    QFile file(eventsPath());
    if (!file.open(QIODevice::ReadOnly))
        return;
    QDataStream in(&file);
    quint32 magic = 0, count = 0;
    quint16 version = 0;
    in >> magic >> version >> count;
    if (magic != FileMagic || version < 1 || version > FileVersion)
        return;
    for (quint32 i = 0; i < count && in.status() == QDataStream::Ok; ++i) {
        Event e;
        in >> e.mmsi >> e.timeMs >> e.zone >> e.vesselClass;
        if (e.zone < 0 || e.zone >= m_zones.size())
            continue;
        m_events.push_back(e);
        qint64 &last = m_lastInZone[key(e.mmsi, e.zone)];
        last = std::max(last, e.timeMs);
        if (version == 1)
            m_recordedHours.insert(e.timeMs / HourMs);   // best effort for old files
    }
    if (version >= 2) {
        quint32 hours = 0;
        in >> hours;
        for (quint32 i = 0; i < hours && in.status() == QDataStream::Ok; ++i) {
            qint64 h;
            in >> h;
            m_recordedHours.insert(h);
        }
    }
    prune();
}

void ChokepointMonitor::save()
{
    if (!m_dirty)
        return;
    QDir().mkpath(QFileInfo(eventsPath()).absolutePath());
    QSaveFile file(eventsPath());
    if (!file.open(QIODevice::WriteOnly))
        return;
    QDataStream out(&file);
    out << FileMagic << FileVersion << quint32(m_events.size());
    for (const Event &e : std::as_const(m_events))
        out << e.mmsi << e.timeMs << e.zone << e.vesselClass;
    out << quint32(m_recordedHours.size());
    for (qint64 h : std::as_const(m_recordedHours))
        out << h;
    if (file.commit())
        m_dirty = false;
}
