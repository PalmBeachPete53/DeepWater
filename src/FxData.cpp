#include "FxData.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonObject>
#include <QNetworkReply>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStringList>

#include <cmath>

namespace {

constexpr int HistoryDays = 400;        // ~1 year of business days plus margin
constexpr qint64 StaleSecs = 6 * 3600;

QString cachePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/fx_rates.json";
}

} // namespace

FxData::FxData(QObject *parent)
    : QObject(parent)
{
    // The seven majors, plus crosses that matter for commodity flows.
    m_pairs = {
        {"EUR", "USD"}, {"GBP", "USD"}, {"USD", "JPY"}, {"USD", "CHF"}, {"AUD", "USD"},
        {"USD", "CAD"}, {"NZD", "USD"}, {"EUR", "GBP"}, {"EUR", "JPY"}, {"USD", "CNY"},
    };
    m_retryTimer.setSingleShot(true);
    connect(&m_retryTimer, &QTimer::timeout, this, &FxData::fetch);
    loadCache();
}

void FxData::refreshIfStale()
{
    if (!m_fetchedAt.isValid() || m_fetchedAt.secsTo(QDateTime::currentDateTimeUtc()) > StaleSecs)
        fetch();
}

void FxData::setEnabled(bool on)
{
    m_enabled = on;
    if (on)
        refreshIfStale();
    else
        m_retryTimer.stop();
}

void FxData::fetch()
{
    if (m_fetching || !m_enabled)
        return;
    m_fetching = true;

    QStringList symbols;
    for (const FxPair &p : std::as_const(m_pairs))
        for (const QString &c : {p.base, p.quote})
            if (c != "USD" && !symbols.contains(c))
                symbols << c;
    const QDate end = QDate::currentDate();
    const QDate start = end.addDays(-HistoryDays);
    const QUrl url(QString("https://api.frankfurter.dev/v2/rates?from=%1&to=%2&base=USD&quotes=%3&providers=ECB")
                       .arg(start.toString(Qt::ISODate), end.toString(Qt::ISODate), symbols.join(',')));
    QNetworkRequest request(url);
    request.setTransferTimeout(30 * 1000);
    QNetworkReply *reply = m_net.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        m_fetching = false;
        const QByteArray body = reply->readAll();
        if (reply->error() != QNetworkReply::NoError || !parse(body)) {
            if (m_error.isEmpty() || reply->error() != QNetworkReply::NoError)
                m_error = reply->errorString();
            m_retryTimer.start(5 * 60 * 1000);
            emit updated();
            return;
        }
        m_error.clear();
        m_fetchedAt = QDateTime::currentDateTimeUtc();
        QDir().mkpath(QFileInfo(cachePath()).absolutePath());
        QSaveFile file(cachePath());
        if (file.open(QIODevice::WriteOnly)) {
            const QJsonDocument response = QJsonDocument::fromJson(body);
            QJsonObject cache {{"fetchedAt", m_fetchedAt.toString(Qt::ISODate)},
                               {"response", response.isArray() ? QJsonValue(response.array()) : QJsonValue(response.object())}};
            file.write(QJsonDocument(cache).toJson(QJsonDocument::Compact));
            file.commit();
        }
        emit updated();
    });
}

bool FxData::parse(const QByteArray &json)
{
    // v2 gives one {date, quote, rate} per line; a cache file saved by v1 still holds {"rates": {date: {currency: rate}}}.
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    QJsonObject rates = doc.object().value("rates").toObject();
    if (doc.isArray()) {
        QHash<QString, QJsonObject> days;
        for (const QJsonValue &v : doc.array()) {
            const QJsonObject o = v.toObject();
            days[o.value("date").toString()].insert(o.value("quote").toString(), o.value("rate"));
        }
        for (auto it = days.begin(); it != days.end(); ++it)
            rates.insert(it.key(), it.value());
    }
    if (rates.isEmpty()) {
        m_error = tr("No rates returned");
        return false;
    }
    QStringList keys = rates.keys();
    std::sort(keys.begin(), keys.end());   // ISO dates sort chronologically

    QVector<QDate> dates;
    QHash<QString, QVector<double>> perUsd;
    for (const QString &key : std::as_const(keys)) {
        const QJsonObject day = rates.value(key).toObject();
        dates.push_back(QDate::fromString(key, Qt::ISODate));
        for (auto it = day.begin(); it != day.end(); ++it)
            perUsd[it.key()].push_back(it.value().toDouble());
    }
    for (const auto &values : std::as_const(perUsd))
        if (values.size() != dates.size()) {
            m_error = tr("Incomplete rate history");
            return false;
        }
    perUsd.insert("USD", QVector<double>(dates.size(), 1.0));
    m_dates = dates;
    m_perUsd = perUsd;
    return true;
}

void FxData::loadCache()
{
    QFile file(cachePath());
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonObject cache = QJsonDocument::fromJson(file.readAll()).object();
    const QJsonValue response = cache.value("response");
    const QJsonDocument saved = response.isArray() ? QJsonDocument(response.toArray()) : QJsonDocument(response.toObject());
    if (parse(saved.toJson(QJsonDocument::Compact)))
        m_fetchedAt = QDateTime::fromString(cache.value("fetchedAt").toString(), Qt::ISODate);
    m_error.clear();
}

QVector<double> FxData::series(int pair) const
{
    // base/quote = (quote per USD) / (base per USD)
    const FxPair &p = m_pairs[pair];
    const QVector<double> base = m_perUsd.value(p.base), quote = m_perUsd.value(p.quote);
    QVector<double> out;
    if (base.size() != m_dates.size() || quote.size() != m_dates.size())
        return out;
    out.reserve(m_dates.size());
    for (int i = 0; i < m_dates.size(); ++i)
        out.push_back(base[i] > 0 ? quote[i] / base[i] : 0);
    return out;
}

double FxData::latest(int pair) const
{
    const QVector<double> s = series(pair);
    return s.isEmpty() ? 0 : s.last();
}

double FxData::changePct(int pair) const
{
    const QVector<double> s = series(pair);
    if (s.size() < 2 || s[s.size() - 2] == 0)
        return 0;
    return (s.last() / s[s.size() - 2] - 1) * 100;
}

QVector<QVector<double>> FxData::correlation(int returns, int *used) const
{
    const int n = m_pairs.size();
    QVector<QVector<double>> rets(n);
    for (int i = 0; i < n; ++i) {
        const QVector<double> s = series(i);
        for (int t = 1; t < s.size(); ++t)
            rets[i].push_back(s[t - 1] > 0 && s[t] > 0 ? std::log(s[t] / s[t - 1]) : 0);
    }
    const int available = rets.isEmpty() ? 0 : int(rets[0].size());
    const int count = std::min(returns, available);
    if (used)
        *used = count;

    QVector<QVector<double>> m(n, QVector<double>(n, 0));
    if (count < 3)
        return m;
    QVector<double> mean(n, 0), sd(n, 0);
    for (int i = 0; i < n; ++i) {
        for (int t = available - count; t < available; ++t)
            mean[i] += rets[i][t];
        mean[i] /= count;
        for (int t = available - count; t < available; ++t)
            sd[i] += (rets[i][t] - mean[i]) * (rets[i][t] - mean[i]);
        sd[i] = std::sqrt(sd[i]);
    }
    for (int i = 0; i < n; ++i) {
        for (int j = i; j < n; ++j) {
            double cov = 0;
            for (int t = available - count; t < available; ++t)
                cov += (rets[i][t] - mean[i]) * (rets[j][t] - mean[j]);
            const double r = sd[i] > 0 && sd[j] > 0 ? cov / (sd[i] * sd[j]) : 0;
            m[i][j] = m[j][i] = std::clamp(r, -1.0, 1.0);
        }
    }
    return m;
}
