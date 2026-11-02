#pragma once

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>
#include <QVector>
#include <QVector3D>

class VesselStore;

struct Chokepoint {
    QString id;
    const char *name;          // untranslated; see ChokepointMonitor::name()
    const char *description;
    double lat;
    double lon;
    double radiusKm;
    QVector3D center;
    float cosRadius;           // cos of the zone's angular radius
    const char *portwatchId;   // IMF PortWatch chokepoint id, for historical transits
    const char *portwatchScope; // PortWatch zone name when it covers a different area than ours, else ""
};

// Watches live AIS positions and records tanker / cargo-ship passages through
// major maritime chokepoints. A passage is a ship seen inside a zone after not
// having been there for several hours. Events are kept on disk for the
// 48-hour window, together with the hours during which data was actually
// received, so "no passage" can be told apart from "app not running"
// (aisstream.io has no history to backfill from).
//
// Counts are lower bounds: they only include ships covered by the AIS feed
// and whose type is known.
class ChokepointMonitor : public QObject
{
    Q_OBJECT
public:
    static constexpr int WindowHours = 48;

    explicit ChokepointMonitor(VesselStore *store, QObject *parent = nullptr);
    ~ChokepointMonitor() override;

    const QVector<Chokepoint> &chokepoints() const { return m_zones; }
    QString name(int zone) const;
    QString description(int zone) const;
    int zoneAt(const QVector3D &pos) const;   // -1 when outside every zone

    struct Counts {
        int tankers = 0;
        int cargo = 0;
        int unknown = 0;   // ships in the zone whose type is not known yet (not in total)
        int total() const { return tankers + cargo; }
    };
    // True when any ship (of any type) reported from the zone in the last 30 minutes.
    bool hasCoverage(int zone) const;
    // Ships in the zone now (seen in the last 30 minutes): their counts, and the tankers and cargo
    // ships. Both come from one pass over the vessels for every zone, kept until the store reports
    // a change or for a second at most: the globe asks at every frame.
    Counts inZoneNow(int zone) const;
    QVector<qint64> vesselsInZone(int zone) const;
    Counts passages(int zone, qint64 windowMs) const;
    // WindowHours hourly buckets, oldest first; the last one is the current hour.
    QVector<Counts> hourly(int zone) const;
    // Same layout: whether AIS data was being received during each hour.
    QVector<bool> recordedHours() const;
    int recordedHourCount() const;

signals:
    void passageRecorded(int zone);

private:
    struct Event {
        qint64 mmsi;
        qint64 timeMs;
        qint8 zone;
        qint8 vesselClass;
    };

    void onVesselUpdated(qint64 mmsi);
    void refreshInZone() const;
    void load();
    void save();
    void prune();

    VesselStore *m_store;
    QVector<Chokepoint> m_zones;
    QVector<Event> m_events;
    QHash<qint64, qint64> m_lastInZone;   // (mmsi * 16 + zone) -> last time seen inside
    QVector<qint64> m_lastReportMs;       // per zone: last position report of any ship
    QSet<qint64> m_recordedHours;         // epoch hour numbers with data received
    QTimer m_saveTimer;
    bool m_dirty = false;
    // inZoneNow() / vesselsInZone(), per zone, as of the store's revision and time below.
    mutable QVector<Counts> m_inZone;
    mutable QVector<QVector<qint64>> m_inZoneShips;
    mutable quint64 m_inZoneRevision = 0;
    mutable qint64 m_inZoneMs = 0;
};
