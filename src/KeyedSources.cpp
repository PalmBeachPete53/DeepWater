#include "KeyedSources.h"

#include "HttpCache.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>

#include <algorithm>
#include <cmath>

namespace {

const qint64 TwiceADay = 12 * 3600;

int sinceYear()
{
    return QDate::currentDate().year() - 2;
}

double monthTime(const QDate &d)
{
    return d.isValid() ? d.year() + (d.month() - 0.5) / 12.0 : 0;
}

double dayTime(const QDate &d)
{
    return d.isValid() ? d.year() + (d.dayOfYear() - 0.5) / d.daysInYear() : 0;
}

// % over a year from a monthly index series (month -> level).
QMap<double, double> yearOnYear(const QMap<double, double> &level)
{
    QMap<double, double> out;
    for (auto it = level.begin(); it != level.end(); ++it)
        for (auto p = level.begin(); p != level.end(); ++p)
            if (std::abs(p.key() - (it.key() - 1)) < 1e-6 && p.value() > 0) {
                out.insert(it.key(), 100 * (it.value() / p.value() - 1));
                break;
            }
    return out;
}

} // namespace

QString sourceKey(const QString &id)
{
    return QSettings().value("keys/" + id).toString().trimmed();
}

void setSourceKey(const QString &id, const QString &key)
{
    QSettings settings;
    if (key.trimmed().isEmpty())
        settings.remove("keys/" + id);
    else
        settings.setValue("keys/" + id, key.trimmed());
}

// ---------------------------------------------------------------- KeyedSeriesSet

KeyedSeriesSet::KeyedSeriesSet(const QString &cacheName, const QString &keyId, qint64 staleSecs, int seriesCount,
                               QObject *parent)
    : CountrySeriesSet(cacheName, staleSecs, seriesCount, parent)
    , m_keyId(keyId)
{
}

bool KeyedSeriesSet::ready(QString *why) const
{
    if (hasKey())
        return true;
    *why = tr("No API key");
    return false;
}

// ---------------------------------------------------------------- FredIndicators

FredIndicators::FredIndicators(QObject *parent)
    : KeyedSeriesSet("keyed_fred", "fred", TwiceADay, SeriesCount, parent)
{
    loadCached();
}

QList<QUrl> FredIndicators::urls() const
{
    QList<QUrl> out;
    for (const char *series : {"PCEPI", "HOUST", "ICSA", "BOPGSTB", "UMCSENT"})
        out << QUrl(QString("https://api.stlouisfed.org/fred/series/observations?series_id=%1&api_key=%2&file_type=json"
                            "&observation_start=%3-01-01")
                        .arg(series, key()).arg(sinceYear()));
    return out;
}

QByteArray FredIndicators::joinParts(const QList<QByteArray> &parts) const
{
    QByteArray out = header();
    for (int k = 0; k < parts.size() && k < SeriesCount; ++k) {
        QMap<double, double> values;
        for (const QJsonValue &v : QJsonDocument::fromJson(parts[k]).object().value("observations").toArray()) {
            const QJsonObject o = v.toObject();
            bool ok = false;
            const double value = o.value("value").toString().toDouble(&ok);   // "." when missing
            const QDate d = QDate::fromString(o.value("date").toString(), Qt::ISODate);
            if (ok && d.isValid())
                values.insert(k == InitialClaims ? dayTime(d) : monthTime(d), value);
        }
        if (k == PceInflationYoY)
            values = yearOnYear(values);
        for (auto it = values.begin(); it != values.end(); ++it)
            addLine(out, "USA", k, it.key(), k == TradeBalance ? it.value() / 1000 : it.value());
    }
    return out;
}

// ---------------------------------------------------------------- EiaIndicators

EiaIndicators::EiaIndicators(QObject *parent)
    : KeyedSeriesSet("keyed_eia", "eia", TwiceADay, SeriesCount, parent)
{
    loadCached();
}

QList<QUrl> EiaIndicators::urls() const
{
    QList<QUrl> out;
    for (const char *series : {"PET.WCESTUS1.W", "PET.WCRFPUS2.W", "PET.EMM_EPM0_PTE_NUS_DPG.W"})
        out << QUrl(QString("https://api.eia.gov/v2/seriesid/%1?api_key=%2&start=%3-01-01").arg(series, key()).arg(sinceYear()));
    return out;
}

QByteArray EiaIndicators::joinParts(const QList<QByteArray> &parts) const
{
    QByteArray out = header();
    for (int k = 0; k < parts.size() && k < SeriesCount; ++k) {
        const QJsonArray data = QJsonDocument::fromJson(parts[k]).object().value("response").toObject().value("data").toArray();
        for (const QJsonValue &v : data) {
            const QJsonObject o = v.toObject();
            const QDate d = QDate::fromString(o.value("period").toString().left(10), Qt::ISODate);
            const QJsonValue value = o.value("value");
            const double x = value.isDouble() ? value.toDouble() : value.toString().toDouble();
            if (d.isValid() && std::isfinite(x))
                addLine(out, "USA", k, dayTime(d), k == CrudeStocks ? x / 1000 : x);
        }
    }
    return out;
}

// ---------------------------------------------------------------- BeaIndicators

BeaIndicators::BeaIndicators(QObject *parent)
    : KeyedSeriesSet("keyed_bea", "bea", TwiceADay, SeriesCount, parent)
{
    loadCached();
}

QList<QUrl> BeaIndicators::urls() const
{
    const int year = QDate::currentDate().year();
    return {QUrl(QString("https://apps.bea.gov/api/data/?UserID=%1&method=GetData&datasetname=NIPA&TableName=T10101"
                         "&Frequency=Q&Year=%2,%3&ResultFormat=JSON")
                     .arg(key()).arg(year - 1).arg(year))};
}

QByteArray BeaIndicators::joinParts(const QList<QByteArray> &parts) const
{
    QByteArray out = header();
    const QJsonArray data = QJsonDocument::fromJson(parts.value(0)).object().value("BEAAPI").toObject()
                                .value("Results").toObject().value("Data").toArray();
    for (const QJsonValue &v : data) {
        const QJsonObject o = v.toObject();
        if (o.value("LineNumber").toString() != "1")
            continue;
        const QString period = o.value("TimePeriod").toString();   // "2026Q3"
        bool ok = false;
        const double value = o.value("DataValue").toString().remove(',').toDouble(&ok);
        if (period.size() == 6 && period[4] == 'Q' && ok)
            addLine(out, "USA", GdpAnnualized, period.left(4).toInt() + (period.mid(5).toInt() - 0.5) / 4.0, value);
    }
    return out;
}

// ---------------------------------------------------------------- BanxicoIndicators

BanxicoIndicators::BanxicoIndicators(QObject *parent)
    : KeyedSeriesSet("keyed_banxico", "banxico", TwiceADay, SeriesCount, parent)
{
    loadCached();
}

QList<QUrl> BanxicoIndicators::urls() const
{
    // SP30578: annual inflation (INPC); SF61745: target rate. Dates as dd/MM/yyyy.
    const QDate today = QDate::currentDate();
    return {QUrl(QString("https://www.banxico.org.mx/SieAPIRest/service/v1/series/SP30578,SF61745/datos/%1/%2?token=%3")
                     .arg(today.addYears(-2).toString(Qt::ISODate), today.toString(Qt::ISODate), key()))};
}

QByteArray BanxicoIndicators::joinParts(const QList<QByteArray> &parts) const
{
    QByteArray out = header();
    const QJsonArray series = QJsonDocument::fromJson(parts.value(0)).object().value("bmx").toObject().value("series").toArray();
    for (const QJsonValue &s : series) {
        const QJsonObject o = s.toObject();
        const QString id = o.value("idSerie").toString();
        const int k = id == "SP30578" ? CpiYoY : id == "SF61745" ? PolicyRate : -1;
        if (k < 0)
            continue;
        QMap<double, double> values;
        for (const QJsonValue &d : o.value("datos").toArray()) {
            const QJsonObject p = d.toObject();
            const QDate date = QDate::fromString(p.value("fecha").toString(), "dd/MM/yyyy");
            bool ok = false;
            const double value = p.value("dato").toString().remove(',').toDouble(&ok);
            if (date.isValid() && ok)
                values.insert(monthTime(date), value);   // a daily rate keeps the month's last value
        }
        for (auto it = values.begin(); it != values.end(); ++it)
            addLine(out, "MEX", k, it.key(), it.value());
    }
    return out;
}

// ---------------------------------------------------------------- EcosIndicators

EcosIndicators::EcosIndicators(QObject *parent)
    : KeyedSeriesSet("keyed_ecos", "ecos", TwiceADay, SeriesCount, parent)
{
    loadCached();
}

QList<QUrl> EcosIndicators::urls() const
{
    // Consumer price index, all items (statistic 901Y009, item 0), monthly, as YYYYMM.
    const QDate today = QDate::currentDate();
    return {QUrl(QString("https://ecos.bok.or.kr/api/StatisticSearch/%1/json/kr/1/100/901Y009/M/%2/%3/0")
                     .arg(key(), today.addYears(-2).toString("yyyyMM"), today.toString("yyyyMM")))};
}

QByteArray EcosIndicators::joinParts(const QList<QByteArray> &parts) const
{
    QByteArray out = header();
    QMap<double, double> level;
    const QJsonArray rows = QJsonDocument::fromJson(parts.value(0)).object().value("StatisticSearch").toObject().value("row").toArray();
    for (const QJsonValue &v : rows) {
        const QJsonObject o = v.toObject();
        const QString time = o.value("TIME").toString();   // "202609"
        bool ok = false;
        const double value = o.value("DATA_VALUE").toString().toDouble(&ok);
        if (time.size() == 6 && ok)
            level.insert(time.left(4).toInt() + (time.mid(4).toInt() - 0.5) / 12.0, value);
    }
    const QMap<double, double> yoy = yearOnYear(level);
    for (auto it = yoy.begin(); it != yoy.end(); ++it)
        addLine(out, "KOR", CpiYoY, it.key(), it.value());
    return out;
}

// ---------------------------------------------------------------- EvdsIndicators

EvdsIndicators::EvdsIndicators(QObject *parent)
    : KeyedSeriesSet("keyed_evds", "evds", TwiceADay, SeriesCount, parent)
{
    loadCached();
}

QList<QUrl> EvdsIndicators::urls() const
{
    // TP.FG.J0: consumer price index (2003 = 100), monthly.
    const QDate today = QDate::currentDate();
    return {QUrl(QString("https://evds2.tcmb.gov.tr/service/evds/series=TP.FG.J0&startDate=%1&endDate=%2&type=json")
                     .arg(today.addYears(-2).toString("dd-MM-yyyy"), today.toString("dd-MM-yyyy")))};
}

void EvdsIndicators::prepare(QNetworkRequest &request) const
{
    request.setRawHeader("key", key().toUtf8());
}

QByteArray EvdsIndicators::joinParts(const QList<QByteArray> &parts) const
{
    QByteArray out = header();
    QMap<double, double> level;
    for (const QJsonValue &v : QJsonDocument::fromJson(parts.value(0)).object().value("items").toArray()) {
        const QJsonObject o = v.toObject();
        const QStringList ym = o.value("Tarih").toString().split('-');   // "2026-9"
        const QJsonValue value = o.value("TP_FG_J0");
        const double x = value.isDouble() ? value.toDouble() : value.toString().toDouble();
        if (ym.size() == 2 && x > 0)
            level.insert(ym[0].toInt() + (ym[1].toInt() - 0.5) / 12.0, x);
    }
    const QMap<double, double> yoy = yearOnYear(level);
    for (auto it = yoy.begin(); it != yoy.end(); ++it)
        addLine(out, "TUR", CpiYoY, it.key(), it.value());
    return out;
}

// ---------------------------------------------------------------- ComtradeProducts

ComtradeProducts::ComtradeProducts(QObject *parent)
    : QObject(parent)
{
}

QDateTime ComtradeProducts::fetchedAt(const QString &iso3) const
{
    return m_fetchedAt.value(iso3);
}

ComtradeProducts::Trade ComtradeProducts::trade(const QString &iso3) const
{
    return m_trades.value(iso3);
}

QUrl ComtradeProducts::probeUrl() const
{
    return QUrl("https://comtradeapi.un.org/files/v1/app/reference/Reporters.json");
}

void ComtradeProducts::fetchReporters(const std::function<void()> &then)
{
    if (!m_reporters.isEmpty()) {
        then();
        return;
    }
    m_afterReporters.push_back(then);
    if (m_reportersLoading)
        return;
    m_reportersLoading = true;
    auto apply = [this](const QByteArray &body) {
        const QJsonArray results = QJsonDocument::fromJson(body).object().value("results").toArray();
        for (const QJsonValue &v : results) {
            const QJsonObject o = v.toObject();
            const QString iso3 = o.value("reporterCodeIsoAlpha3").toString();
            if (!iso3.isEmpty())
                m_reporters.insert(iso3, o.value("id").toInt(o.value("reporterCode").toInt()));
        }
        return !m_reporters.isEmpty();
    };
    QByteArray cached;
    QDateTime at;
    if (HttpCache::load("comtrade_reporters", &cached, &at) && at.daysTo(QDateTime::currentDateTimeUtc()) < 90 && apply(cached)) {
        m_reportersLoading = false;
        for (const auto &f : std::exchange(m_afterReporters, {}))
            f();
        return;
    }
    QNetworkRequest request(probeUrl());
    request.setTransferTimeout(30000);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Deepwater/0.1"));
    QNetworkReply *reply = m_net.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, apply] {
        reply->deleteLater();
        m_reportersLoading = false;
        const QByteArray body = reply->readAll();
        if (reply->error() != QNetworkReply::NoError || !apply(body))
            m_lastError = reply->error() != QNetworkReply::NoError ? reply->errorString() : tr("No reporter list");
        else
            HttpCache::save("comtrade_reporters", body, QDateTime::currentDateTimeUtc());
        for (const auto &f : std::exchange(m_afterReporters, {}))
            f();
    });
}

void ComtradeProducts::request(const QString &iso3)
{
    if (!m_enabled || iso3.isEmpty() || m_fetching.contains(iso3))
        return;
    const QString cacheName = "comtrade_" + iso3.toLower();
    if (!m_trades.contains(iso3)) {
        QByteArray cached;
        QDateTime at;
        if (HttpCache::load(cacheName, &cached, &at)) {
            QString error;
            const Trade t = parse(cached, &error);
            if (t.valid()) {
                m_trades.insert(iso3, t);
                m_fetchedAt.insert(iso3, at);
            }
        }
    }
    if (const auto at = m_fetchedAt.constFind(iso3); at != m_fetchedAt.constEnd() && at->daysTo(QDateTime::currentDateTimeUtc()) < 30) {
        QMetaObject::invokeMethod(this, [this, iso3] { emit ready(iso3); }, Qt::QueuedConnection);
        return;
    }
    m_fetching.insert(iso3);
    fetchReporters([this, iso3] { fetchTrade(iso3); });
}

void ComtradeProducts::fetchTrade(const QString &iso3)
{
    const int reporter = m_reporters.value(iso3, 0);
    if (!reporter) {
        m_fetching.remove(iso3);
        Trade t;
        t.error = m_lastError.isEmpty() ? tr("Not a Comtrade reporter") : m_lastError;
        m_trades.insert(iso3, t);
        emit ready(iso3);
        return;
    }
    // The latest full year most countries have filed: two years back (one year back is often incomplete).
    const int year = QDate::currentDate().year() - 2;
    const QString key = sourceKey(keyId());
    const QString path = key.isEmpty() ? "public/v1/preview" : "data/v1/get";
    QUrl url(QString("https://comtradeapi.un.org/%1/C/A/HS?reporterCode=%2&period=%3&partnerCode=0&flowCode=X,M&cmdCode=AG2"
                     "&motCode=0&customsCode=C00&partner2Code=0&includeDesc=true")
                 .arg(path).arg(reporter).arg(year));
    QNetworkRequest request(url);
    request.setTransferTimeout(60000);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Deepwater/0.1"));
    if (!key.isEmpty())
        request.setRawHeader("Ocp-Apim-Subscription-Key", key.toUtf8());
    QNetworkReply *reply = m_net.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, iso3, year] {
        reply->deleteLater();
        m_fetching.remove(iso3);
        Trade t;
        const QByteArray body = reply->readAll();
        if (reply->error() != QNetworkReply::NoError) {
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            t.error = status == 429 ? tr("Comtrade's free quota is used up for now") : reply->errorString();
        } else {
            QString error;
            t = parse(body, &error);
            t.error = error;
            t.year = error.isEmpty() ? year : 0;
        }
        if (t.valid()) {
            // Written with the year, so the cache parses back to the same figures.
            QJsonObject root = QJsonDocument::fromJson(body).object();
            root.insert("dwYear", year);
            HttpCache::save("comtrade_" + iso3.toLower(), QJsonDocument(root).toJson(QJsonDocument::Compact),
                            QDateTime::currentDateTimeUtc());
            m_fetchedAt.insert(iso3, QDateTime::currentDateTimeUtc());
            m_lastFetch = m_fetchedAt.value(iso3);
        }
        m_lastError = t.error;
        m_trades.insert(iso3, t);
        emit ready(iso3);
    });
}

ComtradeProducts::Trade ComtradeProducts::parse(const QByteArray &body, QString *error)
{
    Trade t;
    const QJsonObject root = QJsonDocument::fromJson(body).object();
    t.year = root.value("dwYear").toInt();
    const QJsonArray data = root.value("data").toArray();
    if (data.isEmpty()) {
        *error = root.value("errorMessage").toString(root.value("message").toString(tr("No data for this country")));
        return t;
    }
    for (const QJsonValue &v : data) {
        const QJsonObject o = v.toObject();
        Product p;
        p.code = o.value("cmdCode").toString();
        p.name = o.value("cmdDesc").toString();
        p.value = o.value("primaryValue").toDouble();
        if (p.code.isEmpty() || p.code == "TOTAL" || p.value <= 0)
            continue;
        const QString flow = o.value("flowCode").toString();
        if (flow == "X") {
            t.exports.push_back(p);
            t.exportTotal += p.value;
        } else if (flow == "M") {
            t.imports.push_back(p);
            t.importTotal += p.value;
        }
    }
    auto byValue = [](const Product &a, const Product &b) { return a.value > b.value; };
    std::sort(t.exports.begin(), t.exports.end(), byValue);
    std::sort(t.imports.begin(), t.imports.end(), byValue);
    if (t.exports.isEmpty() && t.imports.isEmpty())
        *error = tr("No data for this country");
    return t;
}
