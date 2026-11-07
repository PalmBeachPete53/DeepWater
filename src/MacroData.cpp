#include "MacroData.h"

#include "HttpCache.h"

#include <QCryptographicHash>
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QStringList>
#include <QUrlQuery>
#include <QtConcurrent/QtConcurrentRun>

#include <cmath>
#include <utility>

#include <zlib.h>

// ---------------------------------------------------------------- Shared helpers

// Quote-aware CSV split: BIS TITLE columns contain commas.
QList<QByteArray> splitCsvLine(const QByteArray &line)
{
    QList<QByteArray> fields;
    QByteArray field;
    bool quoted = false;
    for (char ch : line) {
        if (ch == '"') quoted = !quoted;
        else if (ch == ',' && !quoted) { fields << field; field.clear(); }
        else field += ch;
    }
    fields << field;
    return fields;
}

static quint32 le32(const QByteArray &b, qsizetype at)
{
    if (at < 0 || at + 4 > b.size())
        return 0;
    const auto *p = reinterpret_cast<const uchar *>(b.constData() + at);
    return p[0] | p[1] << 8 | p[2] << 16 | quint32(p[3]) << 24;
}
static quint16 le16(const QByteArray &b, qsizetype at)
{
    if (at < 0 || at + 2 > b.size())
        return 0;
    const auto *p = reinterpret_cast<const uchar *>(b.constData() + at);
    return quint16(p[0] | p[1] << 8);
}

QByteArray unzipFile(const QByteArray &zip, const QByteArray &prefix)
{
    // End of central directory record, then each central directory entry.
    const qsizetype eocd = zip.lastIndexOf(QByteArray("PK\x05\x06", 4));
    if (eocd < 0)
        return {};
    qsizetype at = le32(zip, eocd + 16);
    for (int i = 0, n = le16(zip, eocd + 10); i < n && le32(zip, at) == 0x02014b50; ++i) {
        const int method = le16(zip, at + 10);
        const quint32 packed = le32(zip, at + 20), size = le32(zip, at + 24);
        const int nameLen = le16(zip, at + 28), extraLen = le16(zip, at + 30), commentLen = le16(zip, at + 32);
        const qsizetype local = le32(zip, at + 42);
        const QByteArray name = zip.mid(at + 46, nameLen);
        at += 46 + nameLen + extraLen + commentLen;
        if (!name.startsWith(prefix) || le32(zip, local) != 0x04034b50)
            continue;
        const qsizetype data = local + 30 + le16(zip, local + 26) + le16(zip, local + 28);
        if (data + packed > zip.size() || size > 64 * 1024 * 1024)
            return {};
        if (method == 0)
            return zip.mid(data, packed);
        if (method != 8)
            return {};
        QByteArray out(size, Qt::Uninitialized);
        z_stream z {};
        if (inflateInit2(&z, -MAX_WBITS) != Z_OK)
            return {};
        z.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(zip.constData() + data));
        z.avail_in = packed;
        z.next_out = reinterpret_cast<Bytef *>(out.data());
        z.avail_out = size;
        const int status = inflate(&z, Z_FINISH);
        inflateEnd(&z);
        return status == Z_STREAM_END && z.total_out == size ? out : QByteArray();
    }
    return {};
}

bool forEachZipLine(const QByteArray &zip, const QByteArray &prefix, const std::function<void(const QByteArray &)> &f)
{
    const qsizetype eocd = zip.lastIndexOf(QByteArray("PK\x05\x06", 4));
    if (eocd < 0)
        return false;
    qsizetype at = le32(zip, eocd + 16);
    for (int i = 0, n = le16(zip, eocd + 10); i < n && le32(zip, at) == 0x02014b50; ++i) {
        const int method = le16(zip, at + 10);
        const quint32 packed = le32(zip, at + 20);
        const int nameLen = le16(zip, at + 28), extraLen = le16(zip, at + 30), commentLen = le16(zip, at + 32);
        const qsizetype local = le32(zip, at + 42);
        const QByteArray name = zip.mid(at + 46, nameLen);
        at += 46 + nameLen + extraLen + commentLen;
        if (!name.startsWith(prefix) || le32(zip, local) != 0x04034b50)
            continue;
        const qsizetype data = local + 30 + le16(zip, local + 26) + le16(zip, local + 28);
        if (data + packed > zip.size() || (method != 0 && method != 8))
            return false;
        QByteArray pending;   // the start of a line cut by the end of a chunk
        auto feed = [&](const char *p, qsizetype n) {
            qsizetype start = 0;
            for (qsizetype k = 0; k < n; ++k) {
                if (p[k] != '\n')
                    continue;
                if (pending.isEmpty()) {
                    f(QByteArray::fromRawData(p + start, k - start));
                } else {
                    pending.append(p + start, k - start);
                    f(pending);
                    pending.clear();
                }
                start = k + 1;
            }
            pending.append(p + start, n - start);
        };
        if (method == 0) {
            feed(zip.constData() + data, packed);
        } else {
            z_stream z {};
            if (inflateInit2(&z, -MAX_WBITS) != Z_OK)
                return false;
            z.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(zip.constData() + data));
            z.avail_in = packed;
            QByteArray chunk(1 << 20, Qt::Uninitialized);
            int status = Z_OK;
            while (status == Z_OK) {
                z.next_out = reinterpret_cast<Bytef *>(chunk.data());
                z.avail_out = uInt(chunk.size());
                status = inflate(&z, Z_NO_FLUSH);
                if (status != Z_OK && status != Z_STREAM_END)
                    break;
                feed(chunk.constData(), chunk.size() - z.avail_out);
            }
            inflateEnd(&z);
            if (status != Z_STREAM_END)
                return false;
        }
        if (!pending.isEmpty())
            f(pending);
        return true;
    }
    return false;
}

void forEachWorldBankValue(const QByteArray &zip, const std::function<void(const QByteArray &, int, double)> &f)
{
    // API_<code>_….csv: a few lines of preamble, a header "Country Name","Country Code","Indicator Name",
    // "Indicator Code","1960",…, then one line per country or aggregate.
    const QByteArray csv = unzipFile(zip, "API_");
    QVector<int> years;
    for (const QByteArray &raw : csv.split('\n')) {
        const QList<QByteArray> fields = splitCsvLine(raw.trimmed());
        if (fields.value(0).endsWith("Country Name")) {   // the header (the file starts with a BOM)
            for (const QByteArray &y : fields.mid(4))
                years.push_back(y.toInt());
            continue;
        }
        if (years.isEmpty() || fields.size() < 5 || fields[1].size() != 3)
            continue;
        for (int i = 4; i < fields.size() && i - 4 < years.size(); ++i)
            if (!fields[i].isEmpty() && years[i - 4] > 0)
                f(fields[1], years[i - 4], fields[i].toDouble());
    }
}

// ---------------------------------------------------------------- RemoteDataSet

RemoteDataSet::RemoteDataSet(const QString &cacheName, qint64 staleSecs, QObject *parent)
    : QObject(parent)
    , m_cacheName(cacheName)
    , m_staleSecs(staleSecs)
{
    m_retryTimer.setSingleShot(true);
    connect(&m_retryTimer, &QTimer::timeout, this, &RemoteDataSet::fetch);
}

RemoteDataSet::~RemoteDataSet()
{
    m_cacheLoad.waitForFinished();
}

void RemoteDataSet::loadCached()
{
    // A few cached copies are megabytes of CSV or JSON: parsed one after the other on the UI thread they
    // held the window back at launch.
    m_cacheLoad = QtConcurrent::run([this] {
        QByteArray body;
        QDateTime at;
        if (HttpCache::load(m_cacheName, &body, &at) && parse(body)) {
            m_fetchedAt = at;
            readFreshness();
        }
        m_error.clear();
    });
}

void RemoteDataSet::waitCached() const
{
    m_cacheLoad.waitForFinished();
}

void RemoteDataSet::readFreshness()
{
    QByteArray body;
    QDateTime at;
    if (!HttpCache::load(m_cacheName + "_freshness", &body, &at))
        return;
    const QJsonObject o = QJsonDocument::fromJson(body).object();
    m_validators = {o.value("etag").toString().toUtf8(), o.value("lastModified").toString().toUtf8()};
    m_validatorsUrl = o.value("url").toString();
    m_hash = o.value("hash").toString().toLatin1();
    m_conditionalWorks = o.value("conditional").toBool();
    m_lastBytes = o.value("bytes").toInteger(-1);
    m_changedAt = QDateTime::fromString(o.value("changed").toString(), Qt::ISODate);
    if (at > m_fetchedAt)   // a check that found nothing new doesn't write the copy again
        m_fetchedAt = at;
}

void RemoteDataSet::saveFreshness() const
{
    const QJsonObject o {{"etag", QString::fromUtf8(m_validators.etag)},
                         {"lastModified", QString::fromUtf8(m_validators.lastModified)},
                         {"url", m_validatorsUrl},
                         {"hash", QString::fromLatin1(m_hash)},
                         {"conditional", m_conditionalWorks},
                         {"bytes", m_lastBytes},
                         {"changed", m_changedAt.toString(Qt::ISODate)}};
    HttpCache::save(m_cacheName + "_freshness", QJsonDocument(o).toJson(QJsonDocument::Compact), m_fetchedAt);
}

qint64 RemoteDataSet::checkEverySecs() const
{
    waitCached();
    return fresh::checkEverySecs(staleSecsNow(), m_conditionalWorks, m_lastBytes);
}

void RemoteDataSet::refreshIfStale()
{
    waitCached();
    if (m_retryTimer.isActive())   // it failed a moment ago and tries again on its own
        return;
    if (!m_fetchedAt.isValid() || m_fetchedAt.secsTo(QDateTime::currentDateTimeUtc()) >= checkEverySecs())
        fetch();
}

void RemoteDataSet::setEnabled(bool on)
{
    m_enabled = on;
    if (on)
        refreshIfStale();
    else
        m_retryTimer.stop();
}

void RemoteDataSet::fetch()
{
    waitCached();
    if (m_fetching || !m_enabled)
        return;
    QString why;
    if (!ready(&why)) {
        if (m_error != why) {
            m_error = why;
            emit updated();
        }
        return;
    }
    m_fetching = true;
    m_partUrls = urls();
    m_parts.clear();
    fetchPart();
}

void RemoteDataSet::fetchPart()
{
    const bool single = m_partUrls.size() == 1 && m_parts.isEmpty();
    const QUrl url = m_partUrls.takeFirst();
    QNetworkRequest request(url);
    request.setTransferTimeout(m_transferTimeoutMs);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Deepwater/0.1"));
    prepare(request);
    // "Unless it's still this": only with the answer already read, and for a data set of one request (the bodies of
    // the others wouldn't be there to join).
    m_sent = {};
    if (single && m_fetchedAt.isValid() && m_validatorsUrl == url.toString()) {
        m_sent = m_validators;
        if (!m_sent.etag.isEmpty())
            request.setRawHeader("If-None-Match", m_sent.etag);
        if (!m_sent.lastModified.isEmpty())
            request.setRawHeader("If-Modified-Since", m_sent.lastModified);
    }
    QNetworkReply *reply = m_net.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, single, url] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            m_fetching = false;
            m_error = reply->errorString();
            m_retryTimer.start(5 * 60 * 1000);
            emit updated();
            return;
        }
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const fresh::Validators received {reply->rawHeader("ETag"), reply->rawHeader("Last-Modified")};
        switch (fresh::judgeConditional(m_sent, status, received)) {
        case fresh::Conditional::Works: m_conditionalWorks = true; break;
        case fresh::Conditional::Ignored: m_conditionalWorks = false; break;
        case fresh::Conditional::Unknown: break;
        }
        if (status == 304) {
            unchanged();
            return;
        }
        if (single) {
            m_received = received;
            m_receivedUrl = url.toString();
        } else {
            m_received = {};
            m_receivedUrl.clear();
        }
        m_parts.push_back(reply->readAll());
        if (m_partUrls.isEmpty()) {
            QCryptographicHash hash(QCryptographicHash::Sha1);
            m_lastBytes = 0;
            for (const QByteArray &part : std::as_const(m_parts)) {
                hash.addData(part);
                m_lastBytes += part.size();
            }
            m_receivedHash = hash.result().toHex();
            if (m_fetchedAt.isValid() && m_receivedHash == m_hash) {
                m_validators = m_received;
                m_validatorsUrl = m_receivedUrl;
                m_parts.clear();
                unchanged();
                return;
            }
        }
        if (!m_partUrls.isEmpty())
            fetchPart();
        else if (Parser parse = parser())   // on the thread pool; published here unless we're gone by then
            QtConcurrent::run(std::move(parse), std::exchange(m_parts, {}))
                .then(this, [this](const Parsed &parsed) { finished(parsed.publish(), parsed.cacheBody); });
        else
            finish(joinParts(std::exchange(m_parts, {})));
    });
}

void RemoteDataSet::finish(const QByteArray &body)
{
    const bool ok = parse(body);
    finished(ok, ok ? cacheBody(body) : QByteArray());
}

void RemoteDataSet::finished(bool ok, const QByteArray &cacheBody)
{
    m_fetching = false;
    m_manual = false;
    if (!ok) {
        m_retryTimer.start(5 * 60 * 1000);
        emit updated();
        return;
    }
    m_error.clear();
    m_fetchedAt = QDateTime::currentDateTimeUtc();
    m_changedAt = m_fetchedAt;
    m_validators = m_received;
    m_validatorsUrl = m_receivedUrl;
    m_hash = m_receivedHash;
    HttpCache::save(m_cacheName, cacheBody, m_fetchedAt);
    saveFreshness();
    emit updated();
}

// The source was asked and has nothing new: the data and everything drawn from it stay as they are.
void RemoteDataSet::unchanged()
{
    m_fetching = false;
    const bool tell = m_manual || !m_error.isEmpty();
    m_manual = false;
    m_error.clear();
    m_retryTimer.stop();
    m_fetchedAt = QDateTime::currentDateTimeUtc();
    saveFreshness();
    if (tell)
        emit updated();
}

// ---------------------------------------------------------------- PolicyRates

PolicyRates::PolicyRates(QObject *parent)
    : RemoteDataSet("bis_policy_rates", 12 * 3600, parent)
{
    m_banks = {
        {"USD", "US", "Fed"}, {"EUR", "XM", "ECB"}, {"GBP", "GB", "BoE"},
        {"JPY", "JP", "BoJ"}, {"CHF", "CH", "SNB"}, {"AUD", "AU", "RBA"},
        {"CAD", "CA", "BoC"}, {"NZD", "NZ", "RBNZ"}, {"CNY", "CN", "PBoC"},
    };
    loadCached();
}

QUrl PolicyRates::url() const
{
    QStringList areas;
    for (const Bank &b : m_banks)
        areas << b.area;
    const QString start = QDate::currentDate().addYears(-3).toString(Qt::ISODate);
    return QUrl(QString("https://stats.bis.org/api/v1/data/WS_CBPOL/D.%1/all?startPeriod=%2&format=csv")
                    .arg(areas.join('+'), start));
}

bool PolicyRates::read(const QByteArray &body, const QVector<Bank> &banks, QHash<QString, QVector<Point>> *out,
                       QString *error)
{
    // CSV with a header row; the columns we need are located by name.
    const QList<QByteArray> lines = body.split('\n');
    if (lines.size() < 2) {
        *error = tr("No policy rate data");
        return false;
    }
    const QList<QByteArray> header = lines.first().trimmed().split(',');
    const int areaCol = int(header.indexOf("REF_AREA"));
    const int dateCol = int(header.indexOf("TIME_PERIOD"));
    const int valueCol = int(header.indexOf("OBS_VALUE"));
    if (areaCol < 0 || dateCol < 0 || valueCol < 0) {
        *error = tr("Unexpected policy rate format");
        return false;
    }
    QHash<QString, QString> currencyOf;
    for (const Bank &b : banks)
        currencyOf.insert(b.area, b.currency);

    QHash<QString, QVector<Point>> history;
    for (int i = 1; i < lines.size(); ++i) {
        const QList<QByteArray> fields = splitCsvLine(lines[i].trimmed());
        if (fields.size() <= std::max({areaCol, dateCol, valueCol}) || fields[valueCol].isEmpty())
            continue;
        const QString currency = currencyOf.value(QString::fromLatin1(fields[areaCol]));
        const QDate date = QDate::fromString(QString::fromLatin1(fields[dateCol]), Qt::ISODate);
        bool ok = false;
        const double rate = fields[valueCol].toDouble(&ok);
        // The BIS writes "NaN" for missing days, which toDouble() accepts.
        if (!currency.isEmpty() && date.isValid() && ok && std::isfinite(rate))
            history[currency].push_back({date, rate});
    }
    if (history.isEmpty()) {
        *error = tr("No policy rate data");
        return false;
    }
    for (auto &points : history)
        std::sort(points.begin(), points.end(), [](const Point &a, const Point &b) { return a.date < b.date; });
    *out = history;
    return true;
}

bool PolicyRates::parse(const QByteArray &body)
{
    QHash<QString, QVector<Point>> history;
    if (!read(body, m_banks, &history, &m_error))
        return false;
    m_history = history;
    return true;
}

RemoteDataSet::Parser PolicyRates::parser()
{
    return [this, banks = m_banks](const QList<QByteArray> &parts) {
        QHash<QString, QVector<Point>> history;
        QString error;
        const bool ok = read(parts.value(0), banks, &history, &error);
        return Parsed {[this, ok, history, error] {
                           if (!ok) {
                               m_error = error;
                               return false;
                           }
                           m_history = history;
                           return true;
                       },
                       parts.value(0)};
    };
}

double PolicyRates::current(const QString &currency) const
{
    const QVector<Point> &h = m_history[currency];
    return h.isEmpty() ? 0 : h.last().rate;
}

bool PolicyRates::lastChange(const QString &currency, QDate *date, double *delta) const
{
    const QVector<Point> h = m_history.value(currency);
    for (int i = int(h.size()) - 1; i > 0; --i) {
        if (std::abs(h[i].rate - h[i - 1].rate) > 1e-9) {
            *date = h[i].date;
            *delta = h[i].rate - h[i - 1].rate;
            return true;
        }
    }
    return false;
}

double PolicyRates::changeOver(const QString &currency, int days) const
{
    const QVector<Point> h = m_history.value(currency);
    if (h.isEmpty())
        return 0;
    const QDate from = h.last().date.addDays(-days);
    for (const Point &p : h)
        if (p.date >= from)
            return h.last().rate - p.rate;
    return 0;
}

// ---------------------------------------------------------------- CotData

CotData::CotData(QObject *parent)
    : RemoteDataSet("cftc_tff", 12 * 3600, parent)
{
    m_contracts = {
        {"EUR", "099741"}, {"JPY", "097741"}, {"GBP", "096742"}, {"CHF", "092741"},
        {"AUD", "232741"}, {"CAD", "090741"}, {"NZD", "112741"},
    };
    loadCached();
}

QUrl CotData::url() const
{
    QStringList codes;
    for (const Contract &c : m_contracts)
        codes << QString("'%1'").arg(c.code);
    const QString since = QDate::currentDate().addDays(-380).toString(Qt::ISODate);
    QUrlQuery q;
    q.addQueryItem("$select", "report_date_as_yyyy_mm_dd,cftc_contract_market_code,open_interest_all,"
                              "asset_mgr_positions_long,asset_mgr_positions_short,"
                              "lev_money_positions_long,lev_money_positions_short");
    q.addQueryItem("$where", QString("cftc_contract_market_code in(%1) AND report_date_as_yyyy_mm_dd >= '%2'")
                                 .arg(codes.join(','), since));
    q.addQueryItem("$limit", "2000");
    QUrl url("https://publicreporting.cftc.gov/resource/gpe5-46if.json");
    url.setQuery(q);
    return url;
}

bool CotData::parse(const QByteArray &body)
{
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (!doc.isArray()) {
        m_error = doc.object().value("message").toString(tr("Unexpected CFTC response"));
        return false;
    }
    QHash<QString, QString> currencyOf;
    for (const Contract &c : std::as_const(m_contracts))
        currencyOf.insert(c.code, c.currency);

    // Socrata returns numbers as strings.
    auto num = [](const QJsonObject &o, const char *key) { return o.value(QLatin1String(key)).toString().toLongLong(); };
    QHash<QString, QVector<Week>> weeks;
    for (const QJsonValue &v : doc.array()) {
        const QJsonObject o = v.toObject();
        const QString currency = currencyOf.value(o.value("cftc_contract_market_code").toString());
        if (currency.isEmpty())
            continue;
        Week w;
        w.date = QDate::fromString(o.value("report_date_as_yyyy_mm_dd").toString().left(10), Qt::ISODate);
        w.assetMgrNet = num(o, "asset_mgr_positions_long") - num(o, "asset_mgr_positions_short");
        w.levFundsNet = num(o, "lev_money_positions_long") - num(o, "lev_money_positions_short");
        w.openInterest = num(o, "open_interest_all");
        if (w.date.isValid())
            weeks[currency].push_back(w);
    }
    if (weeks.isEmpty()) {
        m_error = tr("No CFTC positioning data");
        return false;
    }
    for (auto &list : weeks)
        std::sort(list.begin(), list.end(), [](const Week &a, const Week &b) { return a.date < b.date; });
    m_weeks = weeks;
    return true;
}

// ---------------------------------------------------------------- WorldEconomy

namespace {

// IMF DataMapper codes, in WorldEconomy::Indicator order.
const char *const WeoCodes[] = {"NGDP_RPCH", "PCPIPCH", "LUR", "BCA_NGDPD",
                                "GGXCNL_NGDP", "GGXWDG_NGDP", "NGDPD", "PPPPC", "TX_RPCH"};
static_assert(std::size(WeoCodes) == WorldEconomy::IndicatorCount);

constexpr int FirstYear = 2010;

} // namespace

WorldEconomy::WorldEconomy(QObject *parent)
    // The cache name changes with the indicator list, so that a cached body without
    // the new codes isn't used for a day.
    : RemoteDataSet("imf_weo_2", 24 * 3600, parent)
{
    // ISO3 -> [ISO2, English name, continent], for the countries the WEO covers.
    QFile file(":/country_names.json");
    if (file.open(QIODevice::ReadOnly)) {
        const QJsonObject names = QJsonDocument::fromJson(file.readAll()).object();
        for (auto it = names.begin(); it != names.end(); ++it) {
            const QJsonArray a = it.value().toArray();
            m_index.insert(it.key(), int(m_countries.size()));
            m_countries.push_back({it.key(), a.at(0).toString(), a.at(1).toString(), a.at(2).toString()});
        }
    }
    loadCached();
}

const WorldEconomy::Country *WorldEconomy::country(const QString &iso3) const
{
    const int i = m_index.value(iso3, -1);
    return i < 0 ? nullptr : &m_countries[i];
}

QString WorldEconomy::displayName(const QString &iso3) const
{
    const Country *c = country(iso3);
    return c ? QCoreApplication::translate("Country", c->name.toUtf8().constData()) : iso3;
}

QString WorldEconomy::isoForName(const QString &name) const
{
    static const QHash<QString, QString> aliases = {
        {"UAE", "ARE"}, {"DR Congo", "COD"}, {"Côte d'Ivoire", "CIV"},
    };
    if (const QString iso = aliases.value(name); !iso.isEmpty())
        return iso;
    for (const Country &c : m_countries)
        if (c.name == name)
            return c.iso3;
    return {};
}

double WorldEconomy::value(const QString &iso3, Indicator indicator, int year) const
{
    const auto it = m_values.constFind(iso3);
    if (it == m_values.constEnd())
        return std::nan("");
    return it->at(indicator).value(year, std::nan(""));
}

QMap<int, double> WorldEconomy::series(const QString &iso3, Indicator indicator) const
{
    const auto it = m_values.constFind(iso3);
    return it == m_values.constEnd() ? QMap<int, double>() : it->at(indicator);
}

QUrl WorldEconomy::url() const
{
    QStringList codes;
    for (const char *code : WeoCodes)
        codes << QString::fromLatin1(code);
    // IMF SDMX 3.0 API, all countries. The time filter is ignored by the
    // service, so this returns every year since 1980 (~1.4 MB).
    return QUrl("https://api.imf.org/external/sdmx/3.0/data/dataflow/IMF.RES/WEO/+/*." + codes.join('+')
                + ".A?attributes=none&measures=all");
}

bool WorldEconomy::read(const QByteArray &body, const QHash<QString, int> &index, Values *values, QString *error)
{
    // SDMX-JSON: series keyed "country:indicator:frequency" (indices into the
    // structure's dimension values), observations keyed by time period index.
    const QJsonObject data = QJsonDocument::fromJson(body).object().value("data").toObject();
    const QJsonObject structure = data.value("structures").toArray().at(0).toObject();
    const QJsonObject dimensions = structure.value("dimensions").toObject();
    const QJsonArray seriesDims = dimensions.value("series").toArray();
    int countryDim = -1, indicatorDim = -1;
    for (int i = 0; i < seriesDims.size(); ++i) {
        const QString id = seriesDims[i].toObject().value("id").toString();
        if (id == "COUNTRY") countryDim = i;
        if (id == "INDICATOR") indicatorDim = i;
    }
    auto ids = [](const QJsonValue &dim) {
        QStringList list;
        for (const QJsonValue &v : dim.toObject().value("values").toArray())
            list << v.toObject().value("id").toString(v.toObject().value("value").toString());
        return list;
    };
    if (countryDim < 0 || indicatorDim < 0) {
        *error = tr("Unexpected IMF response");
        return false;
    }
    const QStringList countries = ids(seriesDims[countryDim]);
    const QStringList indicators = ids(seriesDims[indicatorDim]);
    QVector<int> years;
    for (const QString &y : ids(dimensions.value("observation").toArray().at(0)))
        years.push_back(y.toInt());
    QHash<QString, int> indicatorIndex;
    for (int k = 0; k < IndicatorCount; ++k)
        indicatorIndex.insert(QString::fromLatin1(WeoCodes[k]), k);

    QHash<QString, QVector<QMap<int, double>>> result;
    const QJsonObject series = data.value("dataSets").toArray().at(0).toObject().value("series").toObject();
    for (auto it = series.begin(); it != series.end(); ++it) {
        const QStringList key = it.key().split(':');
        const QString iso = countries.value(key.value(countryDim).toInt());
        const int k = indicatorIndex.value(indicators.value(key.value(indicatorDim).toInt()), -1);
        if (k < 0 || !index.contains(iso))
            continue;   // regional aggregates
        auto &perIndicator = result[iso];
        perIndicator.resize(IndicatorCount);
        const QJsonObject obs = it.value().toObject().value("observations").toObject();
        for (auto o = obs.begin(); o != obs.end(); ++o) {
            const int year = years.value(o.key().toInt());
            bool ok = false;
            double v = o.value().toArray().at(0).toString().toDouble(&ok);
            if (k == GdpUsd)
                v /= 1e9;   // reported in dollars; kept in billions
            if (year >= FirstYear && ok && std::isfinite(v))
                perIndicator[k].insert(year, v);
        }
    }
    if (result.isEmpty()) {
        *error = tr("No IMF data");
        return false;
    }
    *values = result;
    return true;
}

bool WorldEconomy::parse(const QByteArray &body)
{
    Values values;
    if (!read(body, m_index, &values, &m_error))
        return false;
    m_values = values;
    return true;
}

RemoteDataSet::Parser WorldEconomy::parser()
{
    return [this, index = m_index](const QList<QByteArray> &parts) {
        Values values;
        QString error;
        const bool ok = read(parts.value(0), index, &values, &error);
        return Parsed {[this, ok, values, error] {
                           if (!ok) {
                               m_error = error;
                               return false;
                           }
                           m_values = values;
                           return true;
                       },
                       parts.value(0)};
    };
}

// ---------------------------------------------------------------- BalanceOfPayments

namespace {

// (BOP_ACCOUNTING_ENTRY, INDICATOR) codes, in BalanceOfPayments::Series order.
const char *const BopCodes[][2] = {
    {"L_NIL_T", "D_F"}, {"L_NIL_T", "P_F"}, {"NNAFANIL_T", "FAB"}, {"CD_T", "GS"}, {"DB_T", "GS"},
};
static_assert(std::size(BopCodes) == BalanceOfPayments::SeriesCount);

} // namespace

BalanceOfPayments::BalanceOfPayments(QObject *parent)
    : RemoteDataSet("imf_bop", 24 * 3600, parent)
{
    loadCached();
}

double BalanceOfPayments::value(const QString &iso3, Series series, int year) const
{
    const auto it = m_values.constFind(iso3);
    if (it == m_values.constEnd())
        return std::nan("");
    return it->at(series).value(year, std::nan(""));
}

QUrl BalanceOfPayments::url() const
{
    // IMF SDMX 3.0 API, annual flows in US dollars for all countries (~1.8 MB). The key
    // is a cross product, so a few unused series (net direct investment…) come along.
    return QUrl("https://api.imf.org/external/sdmx/3.0/data/dataflow/IMF.STA/BOP/+/"
                "*.L_NIL_T+NNAFANIL_T+CD_T+DB_T.D_F+P_F+FAB+GS.USD.A?attributes=none&measures=all");
}

bool BalanceOfPayments::read(const QByteArray &body, Values *values, int *lastYear, int *latestYear, QString *error)
{
    // SDMX-JSON, as for the WEO: series keyed by indices into the dimension values.
    const QJsonObject data = QJsonDocument::fromJson(body).object().value("data").toObject();
    const QJsonObject structure = data.value("structures").toArray().at(0).toObject();
    const QJsonObject dimensions = structure.value("dimensions").toObject();
    const QJsonArray seriesDims = dimensions.value("series").toArray();
    int countryDim = -1, entryDim = -1, indicatorDim = -1;
    for (int i = 0; i < seriesDims.size(); ++i) {
        const QString id = seriesDims[i].toObject().value("id").toString();
        if (id == "COUNTRY") countryDim = i;
        if (id == "BOP_ACCOUNTING_ENTRY") entryDim = i;
        if (id == "INDICATOR") indicatorDim = i;
    }
    auto ids = [](const QJsonValue &dim) {
        QStringList list;
        for (const QJsonValue &v : dim.toObject().value("values").toArray())
            list << v.toObject().value("id").toString(v.toObject().value("value").toString());
        return list;
    };
    if (countryDim < 0 || entryDim < 0 || indicatorDim < 0) {
        *error = tr("Unexpected IMF response");
        return false;
    }
    const QStringList countries = ids(seriesDims[countryDim]);
    const QStringList entries = ids(seriesDims[entryDim]);
    const QStringList indicators = ids(seriesDims[indicatorDim]);
    QVector<int> years;
    for (const QString &y : ids(dimensions.value("observation").toArray().at(0)))
        years.push_back(y.toInt());
    QHash<QString, int> seriesIndex;
    for (int k = 0; k < SeriesCount; ++k)
        seriesIndex.insert(QString("%1.%2").arg(BopCodes[k][0], BopCodes[k][1]), k);

    QHash<QString, QVector<QMap<int, double>>> result;
    QMap<int, int> reporting;   // year -> countries with a financial account balance
    const QJsonObject series = data.value("dataSets").toArray().at(0).toObject().value("series").toObject();
    for (auto it = series.begin(); it != series.end(); ++it) {
        const QStringList key = it.key().split(':');
        const QString code = entries.value(key.value(entryDim).toInt()) + '.'
                             + indicators.value(key.value(indicatorDim).toInt());
        const int k = seriesIndex.value(code, -1);
        if (k < 0)
            continue;
        auto &perSeries = result[countries.value(key.value(countryDim).toInt())];
        perSeries.resize(SeriesCount);
        const QJsonObject obs = it.value().toObject().value("observations").toObject();
        for (auto o = obs.begin(); o != obs.end(); ++o) {
            const int year = years.value(o.key().toInt());
            bool ok = false;
            const double v = o.value().toArray().at(0).toString().toDouble(&ok);
            if (year < FirstYear || !ok || !std::isfinite(v))
                continue;
            perSeries[k].insert(year, v / 1e9);   // reported in dollars; kept in billions
            if (k == FinancialAccount)
                ++reporting[year];
        }
    }
    if (result.isEmpty() || reporting.isEmpty()) {
        *error = tr("No IMF balance of payments data");
        return false;
    }
    // Latest year with at least 80% of the best coverage: the most recent year is
    // often reported by only a handful of countries.
    int best = 0;
    for (int n : std::as_const(reporting))
        best = std::max(best, n);
    *lastYear = reporting.lastKey();
    *latestYear = *lastYear;
    for (auto it = reporting.begin(); it != reporting.end(); ++it)
        if (it.value() >= best * 0.8)
            *latestYear = it.key();
    *values = result;
    return true;
}

bool BalanceOfPayments::parse(const QByteArray &body)
{
    Values values;
    int lastYear = 0, latestYear = 0;
    if (!read(body, &values, &lastYear, &latestYear, &m_error))
        return false;
    m_values = values;
    m_lastYear = lastYear;
    m_latestYear = latestYear;
    return true;
}

RemoteDataSet::Parser BalanceOfPayments::parser()
{
    return [this](const QList<QByteArray> &parts) {
        Values values;
        int lastYear = 0, latestYear = 0;
        QString error;
        const bool ok = read(parts.value(0), &values, &lastYear, &latestYear, &error);
        return Parsed {[this, ok, values, lastYear, latestYear, error] {
                           if (!ok) {
                               m_error = error;
                               return false;
                           }
                           m_values = values;
                           m_lastYear = lastYear;
                           m_latestYear = latestYear;
                           return true;
                       },
                       parts.value(0)};
    };
}

// ---------------------------------------------------------------- TradeFlows

namespace {

const QByteArray TradeCacheMagic = "DWTRADE1";

// Countries have three-letter codes; aggregates (G001, U150, TX126…) contain digits.
bool isCountryCode(const QString &code)
{
    return code.size() == 3 && std::all_of(code.begin(), code.end(), [](QChar c) { return c >= 'A' && c <= 'Z'; });
}

} // namespace

TradeFlows::TradeFlows(QObject *parent)
    : RemoteDataSet("imf_trade_flows", 7 * 24 * 3600, parent)
{
    m_transferTimeoutMs = 3 * 60 * 1000;   // the service takes a while to start sending
    loadCached();
}

QUrl TradeFlows::url() const
{
    // Exports of goods (FOB, US dollars) of every reporter to every partner, all years: the
    // service ignores time filters.
    return QUrl("https://api.imf.org/external/sdmx/3.0/data/dataflow/IMF.STA/IMTS/+/*.XG_FOB_USD.*.A"
                "?attributes=none&measures=all");
}

bool TradeFlows::read(const QByteArray &body, QVector<Flow> *flowsOut, int *yearOut, QString *error)
{
    QVector<Flow> flows;
    int year = 0;
    if (body.startsWith(TradeCacheMagic)) {
        // Our cache: a header line with the year, then "FROM TO value" lines.
        const QList<QByteArray> lines = body.split('\n');
        year = lines.value(0).mid(TradeCacheMagic.size()).trimmed().toInt();
        for (int i = 1; i < lines.size(); ++i) {
            const QList<QByteArray> f = lines[i].split(' ');
            if (f.size() == 3)
                flows.push_back({QString::fromLatin1(f[0]), QString::fromLatin1(f[1]), f[2].toDouble()});
        }
    } else {
        // SDMX-JSON, as for the WEO: series keyed "reporter:indicator:partner:frequency".
        const QJsonObject data = QJsonDocument::fromJson(body).object().value("data").toObject();
        const QJsonObject dimensions =
            data.value("structures").toArray().at(0).toObject().value("dimensions").toObject();
        const QJsonArray seriesDims = dimensions.value("series").toArray();
        int countryDim = -1, partnerDim = -1;
        for (int i = 0; i < seriesDims.size(); ++i) {
            const QString id = seriesDims[i].toObject().value("id").toString();
            if (id == "COUNTRY") countryDim = i;
            if (id == "COUNTERPART_COUNTRY") partnerDim = i;
        }
        auto ids = [](const QJsonValue &dim) {
            QStringList list;
            for (const QJsonValue &v : dim.toObject().value("values").toArray())
                list << v.toObject().value("id").toString(v.toObject().value("value").toString());
            return list;
        };
        if (countryDim < 0 || partnerDim < 0) {
            *error = tr("Unexpected IMF response");
            return false;
        }
        const QStringList countries = ids(seriesDims[countryDim]);
        const QStringList partners = ids(seriesDims[partnerDim]);
        QVector<int> years;
        for (const QString &y : ids(dimensions.value("observation").toArray().at(0)))
            years.push_back(y.toInt());

        // All recent years first, then keep the latest one with nearly full coverage: the
        // last year is often still missing many reporters.
        const int firstYear = QDate::currentDate().year() - 6;
        QMap<int, QVector<Flow>> byYear;
        const QJsonObject series = data.value("dataSets").toArray().at(0).toObject().value("series").toObject();
        for (auto it = series.begin(); it != series.end(); ++it) {
            const QStringList key = it.key().split(':');
            const QString from = countries.value(key.value(countryDim).toInt());
            const QString to = partners.value(key.value(partnerDim).toInt());
            if (!isCountryCode(from) || !isCountryCode(to) || from == to)
                continue;
            const QJsonObject obs = it.value().toObject().value("observations").toObject();
            for (auto o = obs.begin(); o != obs.end(); ++o) {
                const int y = years.value(o.key().toInt());
                bool ok = false;
                const double v = o.value().toArray().at(0).toString().toDouble(&ok);
                if (y >= firstYear && ok && std::isfinite(v) && v > 0)
                    byYear[y].push_back({from, to, v / 1e9});   // reported in dollars
            }
        }
        qsizetype best = 0;
        for (const auto &f : std::as_const(byYear))
            best = std::max(best, f.size());
        for (auto it = byYear.begin(); it != byYear.end(); ++it)
            if (it.value().size() >= best * 0.9)
                year = it.key();
        flows = byYear.value(year);
    }
    if (flows.isEmpty() || !year) {
        *error = tr("No IMF trade data");
        return false;
    }
    std::sort(flows.begin(), flows.end(), [](const Flow &a, const Flow &b) { return a.value > b.value; });
    *flowsOut = flows;
    *yearOut = year;
    return true;
}

bool TradeFlows::parse(const QByteArray &body)
{
    QVector<Flow> flows;
    int year = 0;
    if (!read(body, &flows, &year, &m_error))
        return false;
    m_flows = flows;
    m_year = year;
    return true;
}

RemoteDataSet::Parser TradeFlows::parser()
{
    return [this](const QList<QByteArray> &parts) {
        QVector<Flow> flows;
        int year = 0;
        QString error;
        const bool ok = read(parts.value(0), &flows, &year, &error);
        return Parsed {[this, ok, flows, year, error] {
                           if (!ok) {
                               m_error = error;
                               return false;
                           }
                           m_flows = flows;
                           m_year = year;
                           return true;
                       },
                       ok ? compact(year, flows) : QByteArray()};
    };
}

QByteArray TradeFlows::cacheBody(const QByteArray &) const
{
    return compact(m_year, m_flows);
}

QByteArray TradeFlows::compact(int year, const QVector<Flow> &flows)
{
    QByteArray out = TradeCacheMagic + ' ' + QByteArray::number(year) + '\n';
    out.reserve(int(flows.size()) * 24);
    for (const Flow &f : flows)
        out += f.from.toLatin1() + ' ' + f.to.toLatin1() + ' ' + QByteArray::number(f.value, 'g', 6) + '\n';
    return out;
}

// ---------------------------------------------------------------- EconomySectors

namespace {

// World Bank indicator codes, in EconomySectors::Series order.
const char *const SectorCodes[] = {
    "NV.AGR.TOTL.ZS", "NV.IND.TOTL.ZS", "NV.IND.MANF.ZS", "NV.SRV.TOTL.ZS",
    "SL.AGR.EMPL.ZS", "SL.IND.EMPL.ZS", "SL.SRV.EMPL.ZS",
};
static_assert(std::size(SectorCodes) == EconomySectors::SeriesCount);

const QByteArray SectorsCacheMagic = "DWSECT2";

} // namespace

EconomySectors::EconomySectors(QObject *parent)
    : RemoteDataSet("worldbank_sectors", 7 * 24 * 3600, parent)
{
    loadCached();
}

double EconomySectors::value(const QString &iso3, Series series, int year) const
{
    const auto it = m_values.constFind(iso3);
    if (it == m_values.constEnd())
        return std::nan("");
    return it->at(series).value(year, std::nan(""));
}

int EconomySectors::latestYear(const QString &iso3, Series series) const
{
    const auto it = m_values.constFind(iso3);
    return it == m_values.constEnd() || it->at(series).isEmpty() ? 0 : it->at(series).lastKey();
}

QMap<int, double> EconomySectors::series(const QString &iso3, Series series) const
{
    const auto it = m_values.constFind(iso3);
    return it == m_values.constEnd() ? QMap<int, double>() : it->at(series);
}

QUrl EconomySectors::url() const
{
    return urls().constFirst();
}

QList<QUrl> EconomySectors::urls() const
{
    QList<QUrl> list;
    for (const char *code : SectorCodes)
        list << QUrl(QString("https://api.worldbank.org/v2/en/indicator/%1?downloadformat=csv").arg(QLatin1String(code)));
    return list;
}

QByteArray EconomySectors::joinParts(const QList<QByteArray> &parts) const
{
    return join(parts);
}

QByteArray EconomySectors::join(const QList<QByteArray> &parts)
{
    QByteArray out = SectorsCacheMagic + '\n';
    for (int k = 0; k < parts.size() && k < SeriesCount; ++k)
        forEachWorldBankValue(parts[k], [&](const QByteArray &iso3, int year, double v) {
            out += iso3 + ' ' + QByteArray::number(k) + ' ' + QByteArray::number(year) + ' ' + QByteArray::number(v, 'g', 5) + '\n';
        });
    return out;
}

bool EconomySectors::read(const QByteArray &body, Values *values, QString *error)
{
    // "ISO3 series year value" lines (see joinParts).
    QHash<QString, QVector<QMap<int, double>>> result;
    if (body.startsWith(SectorsCacheMagic)) {
        const QList<QByteArray> lines = body.split('\n');
        for (int i = 1; i < lines.size(); ++i) {
            const QList<QByteArray> f = lines[i].split(' ');
            const int k = f.value(1).toInt();
            if (f.size() != 4 || k < 0 || k >= SeriesCount)
                continue;
            auto &perSeries = result[QString::fromLatin1(f[0])];
            perSeries.resize(SeriesCount);
            perSeries[k].insert(f[2].toInt(), f[3].toDouble());
        }
    }
    // Some years are gaps written as zeros (Venezuela's accounts after 1990) or glitches (Peru 1977): a year
    // whose three shares add up to less than half or more than 130% of GDP (or of jobs) is dropped.
    for (auto &perSeries : result)
        for (const int from : {int(PrimaryGdp), int(PrimaryJobs)}) {
            const int secondary = from == PrimaryGdp ? SecondaryGdp : SecondaryJobs;
            const int to = from == PrimaryGdp ? TertiaryGdp : TertiaryJobs;
            for (int year : perSeries[from].keys()) {
                const double sum = perSeries[from].value(year) + perSeries[secondary].value(year) + perSeries[to].value(year);
                if (sum < 50 || sum > 130)
                    for (int k = from; k <= to; ++k)
                        perSeries[k].remove(year);
            }
        }
    if (result.isEmpty()) {
        *error = tr("No World Bank data");
        return false;
    }
    *values = result;
    return true;
}

bool EconomySectors::parse(const QByteArray &body)
{
    Values values;
    if (!read(body, &values, &m_error))
        return false;
    m_values = values;
    return true;
}

RemoteDataSet::Parser EconomySectors::parser()
{
    return [this](const QList<QByteArray> &parts) {
        const QByteArray body = join(parts);   // the zip archives into the compact form the cache keeps
        Values values;
        QString error;
        const bool ok = read(body, &values, &error);
        return Parsed {[this, ok, values, error] {
                           if (!ok) {
                               m_error = error;
                               return false;
                           }
                           m_values = values;
                           return true;
                       },
                       body};
    };
}

// ---------------------------------------------------------------- WorldPolicyRates

namespace {

// Euro area members on 1 January 2026.
const QStringList EuroArea = {"AT", "BE", "BG", "CY", "DE", "EE", "ES", "FI", "FR", "GR", "HR",
                              "IE", "IT", "LT", "LU", "LV", "MT", "NL", "PT", "SI", "SK"};

} // namespace

WorldPolicyRates::WorldPolicyRates(QObject *parent)
    : RemoteDataSet("bis_policy_rates_monthly", 24 * 3600, parent)
{
    loadCached();
}

QVector<WorldPolicyRates::Point> WorldPolicyRates::history(const QString &iso2) const
{
    return m_history.value(isEuroArea(iso2) ? QStringLiteral("XM") : iso2);
}

bool WorldPolicyRates::isEuroArea(const QString &iso2) const
{
    return EuroArea.contains(iso2);
}

QUrl WorldPolicyRates::url() const
{
    const QString start = QDate::currentDate().addMonths(-27).toString("yyyy-MM");
    return QUrl(QString("https://stats.bis.org/api/v1/data/WS_CBPOL/M./all?startPeriod=%1&format=csv").arg(start));
}

bool WorldPolicyRates::parse(const QByteArray &body)
{
    const QList<QByteArray> lines = body.split('\n');
    const QList<QByteArray> header = lines.value(0).trimmed().split(',');
    const int areaCol = int(header.indexOf("REF_AREA"));
    const int dateCol = int(header.indexOf("TIME_PERIOD"));
    const int valueCol = int(header.indexOf("OBS_VALUE"));
    if (areaCol < 0 || dateCol < 0 || valueCol < 0) {
        m_error = tr("Unexpected policy rate format");
        return false;
    }
    QHash<QString, QVector<Point>> history;
    for (int i = 1; i < lines.size(); ++i) {
        const QList<QByteArray> fields = splitCsvLine(lines[i].trimmed());
        if (fields.size() <= std::max({areaCol, dateCol, valueCol}))
            continue;
        const QDate month = QDate::fromString(QString::fromLatin1(fields[dateCol]), "yyyy-MM");
        bool ok = false;
        const double rate = fields[valueCol].toDouble(&ok);
        if (month.isValid() && ok && std::isfinite(rate))
            history[QString::fromLatin1(fields[areaCol])].push_back({month, rate});
    }
    if (history.isEmpty()) {
        m_error = tr("No policy rate data");
        return false;
    }
    for (auto &points : history)
        std::sort(points.begin(), points.end(), [](const Point &a, const Point &b) { return a.month < b.month; });
    m_history = history;
    return true;
}
