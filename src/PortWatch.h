#pragma once

#include <QDate>
#include <QDateTime>
#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVector>

// Historical chokepoint transits from IMF PortWatch (daily counts per vessel
// type, derived from satellite AIS). Public ArcGIS endpoint, no key. Published
// with a lag of several days, so it complements the live AIS feed rather than
// replacing it. Cached on disk and refreshed every 12 hours.
class PortWatch : public QObject
{
    Q_OBJECT
public:
    struct Day {
        QDate date;
        int tanker = 0;
        int dryBulk = 0;
        int container = 0;
        int other = 0;              // general cargo + ro-ro
        int total = 0;
        qint64 tankerCapacity = 0;  // deadweight tonnes
        qint64 capacity = 0;
    };

    explicit PortWatch(QObject *parent = nullptr);

    // PortWatch ids ("chokepoint1"...) to fetch; call before refresh().
    void setChokepoints(const QStringList &portIds);
    void refreshIfStale();

    // Sources page: a source turned off makes no requests (its last cached copy stays in use);
    // refreshNow() downloads at once whatever the age of the data.
    void setEnabled(bool on);
    bool isEnabled() const { return m_enabled; }
    void refreshNow() { fetch(); }
    bool isFetching() const { return m_fetching; }

    // Days for one chokepoint, oldest first (empty if unknown).
    QVector<Day> days(const QString &portId) const { return m_days.value(portId); }
    QDate latestDate() const;
    QDateTime fetchedAt() const { return m_fetchedAt; }
    QString errorString() const { return m_error; }

    // Average daily transits (all types and tankers only) over the last n days.
    struct Average { double total = 0; double tanker = 0; };
    Average average(const QString &portId, int lastDays) const;

signals:
    void updated();

private:
    void fetch();
    bool parse(const QByteArray &json);
    void onFailure(const QString &error);
    void loadCache();

    QNetworkAccessManager m_net;
    QStringList m_portIds;
    QHash<QString, QVector<Day>> m_days;
    QDateTime m_fetchedAt;
    QString m_error;
    bool m_fetching = false;
    bool m_enabled = true;
    QTimer m_retryTimer;
    int m_failures = 0;
};
