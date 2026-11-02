#include "PortWatch.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimeZone>
#include <QUrlQuery>

namespace {

const char *Endpoint =
    "https://services9.arcgis.com/weJ1QsnbMYJlCHdG/ArcGIS/rest/services/Daily_Chokepoints_Data/FeatureServer/0/query";
constexpr int MaxRows = 1000;          // the service's maxRecordCount
constexpr int WindowDays = 95;         // 10 zones x 95 days stays under MaxRows
constexpr qint64 StaleSecs = 12 * 3600;

QString cachePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/portwatch.json";
}

} // namespace

PortWatch::PortWatch(QObject *parent)
    : QObject(parent)
{
    m_retryTimer.setSingleShot(true);
    connect(&m_retryTimer, &QTimer::timeout, this, &PortWatch::fetch);
    loadCache();
}

void PortWatch::onFailure(const QString &error)
{
    // The ArcGIS service intermittently rejects valid queries (often after a
    // ~10 s stall), so retry a few times before waiting for the next check.
    m_error = error;
    static const int delaysSec[] = {60, 300, 900};
    if (m_failures < int(std::size(delaysSec)))
        m_retryTimer.start(delaysSec[m_failures] * 1000);
    ++m_failures;
    emit updated();
}

void PortWatch::setChokepoints(const QStringList &portIds)
{
    m_portIds = portIds;
}

void PortWatch::refreshIfStale()
{
    if (!m_fetchedAt.isValid() || m_fetchedAt.secsTo(QDateTime::currentDateTimeUtc()) > StaleSecs)
        fetch();
}

void PortWatch::setEnabled(bool on)
{
    m_enabled = on;
    if (on)
        refreshIfStale();
    else
        m_retryTimer.stop();
}

void PortWatch::fetch()
{
    if (m_fetching || !m_enabled || m_portIds.isEmpty())
        return;
    m_fetching = true;

    // The service rejects orderByFields (and outFields=*) in combination with
    // a filter, so the window is selected on the integer year/month/day fields
    // and sorted client-side.
    QStringList quoted;
    for (const QString &id : std::as_const(m_portIds))
        quoted << QString("'%1'").arg(id);
    const QDate from = QDate::currentDate().addDays(-WindowDays);
    const QString window = QString("(year > %1 OR (year = %1 AND month > %2) OR (year = %1 AND month = %2 AND day >= %3))")
                               .arg(from.year()).arg(from.month()).arg(from.day());
    QUrlQuery q;
    q.addQueryItem("where", QString("portid IN (%1) AND %2").arg(quoted.join(','), window));
    q.addQueryItem("outFields", "date,portid,n_tanker,n_dry_bulk,n_container,n_general_cargo,n_roro,n_total,"
                                "capacity_tanker,capacity");
    q.addQueryItem("resultRecordCount", QString::number(MaxRows));
    q.addQueryItem("returnGeometry", "false");
    q.addQueryItem("f", "json");
    QUrl url(Endpoint);
    url.setQuery(q);

    QNetworkRequest request(url);
    request.setTransferTimeout(60 * 1000);   // the service can take 10+ seconds
    QNetworkReply *reply = m_net.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        m_fetching = false;
        const QByteArray body = reply->readAll();
        if (reply->error() != QNetworkReply::NoError && body.isEmpty()) {
            onFailure(reply->errorString());
            return;
        }
        if (!parse(body)) {
            onFailure(m_error);
            return;
        }
        m_error.clear();
        m_failures = 0;
        m_fetchedAt = QDateTime::currentDateTimeUtc();
        QDir().mkpath(QFileInfo(cachePath()).absolutePath());
        QSaveFile file(cachePath());
        if (file.open(QIODevice::WriteOnly)) {
            QJsonObject cache {{"fetchedAt", m_fetchedAt.toString(Qt::ISODate)},
                               {"response", QJsonDocument::fromJson(body).object()}};
            file.write(QJsonDocument(cache).toJson(QJsonDocument::Compact));
            file.commit();
        }
        emit updated();
    });
}

bool PortWatch::parse(const QByteArray &json)
{
    const QJsonObject root = QJsonDocument::fromJson(json).object();
    if (root.contains("error")) {
        m_error = root.value("error").toObject().value("message").toString();
        return false;
    }
    QHash<QString, QVector<Day>> days;
    for (const QJsonValue &fv : root.value("features").toArray()) {
        const QJsonObject a = fv.toObject().value("attributes").toObject();
        Day d;
        // DateOnly fields come back as "YYYY-MM-DD"; older services used epoch ms.
        const QJsonValue dv = a.value("date");
        d.date = dv.isString() ? QDate::fromString(dv.toString(), Qt::ISODate)
                               : QDateTime::fromMSecsSinceEpoch(qint64(dv.toDouble()), QTimeZone::UTC).date();
        if (!d.date.isValid())
            continue;
        d.tanker = a.value("n_tanker").toInt();
        d.dryBulk = a.value("n_dry_bulk").toInt();
        d.container = a.value("n_container").toInt();
        d.other = a.value("n_general_cargo").toInt() + a.value("n_roro").toInt();
        d.total = a.value("n_total").toInt();
        d.tankerCapacity = qint64(a.value("capacity_tanker").toDouble());
        d.capacity = qint64(a.value("capacity").toDouble());
        days[a.value("portid").toString()].push_back(d);
    }
    if (days.isEmpty()) {
        m_error = tr("No data returned");
        return false;
    }
    for (auto &list : days)
        std::sort(list.begin(), list.end(), [](const Day &x, const Day &y) { return x.date < y.date; });
    m_days = days;
    return true;
}

void PortWatch::loadCache()
{
    QFile file(cachePath());
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonObject cache = QJsonDocument::fromJson(file.readAll()).object();
    if (parse(QJsonDocument(cache.value("response").toObject()).toJson(QJsonDocument::Compact)))
        m_fetchedAt = QDateTime::fromString(cache.value("fetchedAt").toString(), Qt::ISODate);
    m_error.clear();
}

QDate PortWatch::latestDate() const
{
    QDate latest;
    for (const auto &list : m_days)
        if (!list.isEmpty() && (!latest.isValid() || list.last().date > latest))
            latest = list.last().date;
    return latest;
}

PortWatch::Average PortWatch::average(const QString &portId, int lastDays) const
{
    Average avg;
    const QVector<Day> list = m_days.value(portId);
    const int n = std::min<int>(lastDays, list.size());
    if (n == 0)
        return avg;
    for (int i = list.size() - n; i < list.size(); ++i) {
        avg.total += list[i].total;
        avg.tanker += list[i].tanker;
    }
    avg.total /= n;
    avg.tanker /= n;
    return avg;
}
