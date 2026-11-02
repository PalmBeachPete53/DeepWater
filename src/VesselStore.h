#pragma once

#include <QFuture>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>
#include <QVector>
#include <QVector3D>

struct Vessel {
    // Only commodity carriers are displayed. AIS ship types do not say what a
    // ship carries, so cargo is inferred from the type: tankers move oil, gas
    // and chemicals; cargo ships move dry bulk (ores, grain, coal) and containers.
    enum Class { Unknown, Tanker, Cargo, Other };

    qint64 mmsi = 0;
    QString name;
    double lat = 0;
    double lon = 0;
    double sog = 0;        // speed over ground, knots
    double cog = 0;        // course over ground, degrees
    int heading = 511;     // degrees; 511 = not available
    int shipType = 0;      // AIS ship type code; 0 until static data arrives
    QString destination;
    QString callSign;
    qint64 imo = 0;
    double draught = 0;    // metres
    int length = 0;        // metres
    qint64 lastSeenMs = 0; // epoch ms of the last position report

    QVector3D pos;               // unit vector of (lat, lon)
    QVector<QVector3D> trail;    // past positions, oldest first
    qint64 lastTrailMs = 0;

    Class vesselClass() const;
    double bearing() const { return heading != 511 ? heading : cog; }
};

// Latest known state of every vessel heard on the AIS feed.
//
// Ships broadcast their static data (type, name, size) only every ~6 minutes,
// so it is cached on disk: on the next launch, ships are classified as soon as
// their first position arrives.
class VesselStore : public QObject
{
    Q_OBJECT
public:
    explicit VesselStore(QObject *parent = nullptr);
    ~VesselStore() override;

    void updatePosition(qint64 mmsi, const QString &name, double lat, double lon,
                        double sog, double cog, int heading);
    void updateStatic(qint64 mmsi, const QString &name, int shipType, const QString &destination,
                      const QString &callSign, qint64 imo, double draught, int length);
    void clear();

    const QHash<qint64, Vessel> &vessels() const { return m_vessels; }
    // How many times changed() was emitted: what views cache against.
    quint64 revision() const { return m_revision; }
    const Vessel *vessel(qint64 mmsi) const;
    int count(Vessel::Class cls) const;

    bool isClassVisible(Vessel::Class cls) const { return !m_hidden.contains(cls); }
    void setClassVisible(Vessel::Class cls, bool visible);

signals:
    // Throttled: emitted at most once per second while data is flowing.
    void changed();
    void filterChanged();
    // Emitted synchronously for every position or classification update.
    void vesselUpdated(qint64 mmsi);

private:
    struct StaticInfo {
        int shipType = 0;
        QString name;
        QString destination;
        QString callSign;
        qint64 imo = 0;
        double draught = 0;
        int length = 0;
        qint64 lastSeenMs = 0;   // last static report, to forget ships not heard of for a month
        bool sameAs(const StaticInfo &o) const
        {
            return shipType == o.shipType && name == o.name && destination == o.destination
                   && callSign == o.callSign && imo == o.imo && draught == o.draught && length == o.length;
        }
    };
    static bool writeCache(const QHash<qint64, StaticInfo> &entries);

    void markDirty();
    void prune();
    void applyStatic(Vessel &v, const StaticInfo &info);
    void loadCache();
    void saveCache();

    // Vessels per class, kept up to date as they come, change class and go (count() is asked every second).
    void track(const Vessel &v, int delta) { m_classCount[v.vesselClass()] += delta; }

    QHash<qint64, Vessel> m_vessels;
    int m_classCount[Vessel::Other + 1] = {};
    QHash<qint64, StaticInfo> m_static;   // by MMSI; outlives pruning and is persisted
    QTimer m_saveTimer;
    bool m_cacheDirty = false;
    QFuture<bool> m_cacheWrite;   // the last save, written on the thread pool
    // Ship layers start off; the user turns them on in the left panel (Live shipping).
    QSet<int> m_hidden {Vessel::Tanker, Vessel::Cargo, Vessel::Unknown};
    QTimer m_emitTimer;
    QTimer m_pruneTimer;
    bool m_dirty = false;
    quint64 m_revision = 0;
};
