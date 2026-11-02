#include "SupplyChain.h"

#include "HttpCache.h"
#include "KeyedSources.h"
#include "Theme.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QNetworkReply>
#include <QSet>
#include <QTimeZone>
#include <QUrlQuery>

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

namespace supply {

namespace {

const double NaN = std::nan("");

QJsonObject bundled()
{
    static const QJsonObject root = [] {
        QFile f(":/supply_chain.json");
        return f.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(f.readAll()).object() : QJsonObject();
    }();
    return root;
}

// Calls f(category codes, one per dimension, value) for each value of a JSON-stat 2.0 data set (Eurostat).
void forEachJsonStat(const QJsonObject &root, const std::function<void(const QStringList &, double)> &f)
{
    const QJsonArray ids = root.value("id").toArray();
    const QJsonArray sizes = root.value("size").toArray();
    const QJsonObject dims = root.value("dimension").toObject();
    QVector<QStringList> codes;
    for (int d = 0; d < ids.size(); ++d) {
        QStringList list(sizes.at(d).toInt());
        const QJsonObject index = dims.value(ids.at(d).toString()).toObject().value("category").toObject().value("index").toObject();
        for (auto it = index.begin(); it != index.end(); ++it)
            if (it.value().toInt() < list.size())
                list[it.value().toInt()] = it.key();
        codes.push_back(list);
    }
    const QJsonObject values = root.value("value").toObject();
    QStringList at(codes.size());
    for (auto it = values.begin(); it != values.end(); ++it) {
        qint64 flat = it.key().toLongLong();
        for (int d = codes.size() - 1; d >= 0; --d) {
            const int n = std::max<qsizetype>(1, codes[d].size());
            at[d] = codes[d].value(int(flat % n));
            flat /= n;
        }
        f(at, it.value().toDouble());
    }
}

QDate ymd(const QString &text)
{
    return QDate::fromString(text.left(10), Qt::ISODate);
}

// A compact "year:value year:value" (or "yyyy-MM-dd:value") list.
template <typename Key>
QByteArray joinSeries(const QMap<Key, double> &series, const std::function<QByteArray(const Key &)> &key)
{
    QByteArray out;
    for (auto it = series.begin(); it != series.end(); ++it) {
        if (!out.isEmpty())
            out += ' ';
        out += key(it.key()) + ':' + QByteArray::number(it.value(), 'g', 8);
    }
    return out;
}

} // namespace

QString iso3FromIso2(const QString &iso2)
{
    static const QHash<QString, QString> map = [] {
        QHash<QString, QString> m;
        QFile f(":/country_names.json");
        if (f.open(QIODevice::ReadOnly)) {
            const QJsonObject names = QJsonDocument::fromJson(f.readAll()).object();
            for (auto it = names.begin(); it != names.end(); ++it)
                m.insert(it.value().toArray().at(0).toString(), it.key());
        }
        m.insert("EL", "GRC");   // Eurostat
        m.insert("UK", "GBR");
        return m;
    }();
    return map.value(iso2);
}

// ---------------------------------------------------------------- CropYields

namespace {

int psdCrop(const QByteArray &code)
{
    if (code == "0410000") return int(Crop::Wheat);
    if (code == "0440000") return int(Crop::Corn);
    if (code == "0422110") return int(Crop::Rice);
    if (code == "2222000") return int(Crop::Soybeans);
    return -1;
}

int eurostatCrop(const QString &code)
{
    if (code == "C1100") return int(Crop::Wheat);
    if (code == "C1500") return int(Crop::Corn);
    if (code == "C2000") return int(Crop::Rice);
    if (code == "I1130") return int(Crop::Soybeans);
    return -1;
}

} // namespace

CropYields::CropYields(QObject *parent)
    : RemoteDataSet("supply_crop_yields", 7 * 24 * 3600, parent)
{
    m_transferTimeoutMs = 120 * 1000;
    loadCached();
}

QUrl CropYields::url() const
{
    return urls().first();
}

QList<QUrl> CropYields::urls() const
{
    return {QUrl("https://apps.fas.usda.gov/psdonline/downloads/psd_grains_pulses_csv.zip"),
            QUrl("https://apps.fas.usda.gov/psdonline/downloads/psd_oilseeds_csv.zip"),
            QUrl("https://ec.europa.eu/eurostat/api/dissemination/statistics/1.0/data/apro_cpsh1?strucpro=YLD_HUMD_EU_T_HA"
                 "&crops=C1100&crops=C1500&crops=C2000&crops=I1130&sinceTimePeriod=2008&format=JSON&lang=en")};
}

QByteArray CropYields::build(const QList<QByteArray> &parts, QString *error)
{
    if (parts.size() != 3) {
        *error = QCoreApplication::translate("supply", "Incomplete download");
        return {};
    }
    QHash<QString, QString> countries;
    const QJsonObject psd = bundled().value("psdCountries").toObject();
    for (auto it = psd.begin(); it != psd.end(); ++it)
        countries.insert(it.key(), it.value().toString());

    // iso3 -> crop -> year -> t/ha, for each source.
    using Table = QHash<QString, std::array<QMap<int, double>, CropCount>>;
    Table usda, eurostat;
    for (int part = 0; part < 2; ++part) {
        int colCommodity = -1, colCountry = -1, colYear = -1, colAttribute = -1, colValue = -1;
        const bool ok = forEachZipLine(parts[part], "psd_", [&](const QByteArray &raw) {
            const QByteArray line = raw.endsWith('\r') ? raw.chopped(1) : raw;
            if (colValue < 0) {
                const QList<QByteArray> header = splitCsvLine(line);
                colCommodity = header.indexOf("Commodity_Code");
                colCountry = header.indexOf("Country_Code");
                colYear = header.indexOf("Market_Year");
                colAttribute = header.indexOf("Attribute_ID");
                colValue = header.indexOf("Value");
                return;
            }
            if (!line.contains(",184,") || psdCrop(line.left(7)) < 0)   // 184: yield, the other rows skipped fast
                return;
            const QList<QByteArray> f = splitCsvLine(line);
            if (f.size() <= std::max({colCommodity, colCountry, colYear, colAttribute, colValue}) || f[colAttribute] != "184")
                return;
            const int crop = psdCrop(f[colCommodity]);
            const QString iso3 = countries.value(QString::fromLatin1(f[colCountry]));
            const double v = f[colValue].toDouble();
            if (crop >= 0 && !iso3.isEmpty() && v > 0)
                usda[iso3][crop].insert(f[colYear].toInt(), v);
        });
        if (!ok || colValue < 0) {
            *error = QCoreApplication::translate("supply", "Unreadable USDA file");
            return {};
        }
    }
    const QJsonObject es = QJsonDocument::fromJson(parts[2]).object();
    QStringList ids;
    for (const QJsonValue &v : es.value("id").toArray())
        ids << v.toString();
    const int dCrop = ids.indexOf("crops"), dGeo = ids.indexOf("geo"), dTime = ids.indexOf("time");
    if (dCrop >= 0 && dGeo >= 0 && dTime >= 0)
        forEachJsonStat(es, [&](const QStringList &c, double v) {
            const int crop = eurostatCrop(c[dCrop]);
            const QString iso3 = iso3FromIso2(c[dGeo]);
            if (crop >= 0 && !iso3.isEmpty() && v > 0)
                eurostat[iso3][crop].insert(c[dTime].toInt(), v);
        });

    // The USDA's figure for each country it covers; Eurostat for the EU members, which the USDA only gives together.
    QByteArray out;
    auto write = [&](const QString &iso3, int crop, char source, const QMap<int, double> &years) {
        out += iso3.toLatin1() + '\t' + QByteArray::number(crop) + '\t' + source + '\t'
               + joinSeries<int>(years, [](const int &y) { return QByteArray::number(y); }) + '\n';
    };
    for (auto it = usda.begin(); it != usda.end(); ++it)
        for (int c = 0; c < CropCount; ++c)
            if (!it.value()[c].isEmpty())
                write(it.key(), c, 'U', it.value()[c]);
    for (auto it = eurostat.begin(); it != eurostat.end(); ++it)
        for (int c = 0; c < CropCount; ++c)
            if (!it.value()[c].isEmpty() && usda.value(it.key())[c].isEmpty())
                write(it.key(), c, 'E', it.value()[c]);
    if (out.isEmpty())
        *error = QCoreApplication::translate("supply", "No yields in the download");
    return out;
}

bool CropYields::read(const QByteArray &compact, Values *out, QString *error)
{
    Values values;
    for (const QByteArray &line : compact.split('\n')) {
        const QList<QByteArray> f = line.split('\t');
        if (f.size() != 4)
            continue;
        const int crop = f[1].toInt();
        if (crop < 0 || crop >= CropCount)
            continue;
        Series &s = values[QString::fromLatin1(f[0])][crop];
        s.source = f[2] == "E" ? Source::Eurostat : Source::Usda;
        for (const QByteArray &pair : f[3].split(' ')) {
            const int colon = pair.indexOf(':');
            if (colon > 0)
                s.years.insert(pair.left(colon).toInt(), pair.mid(colon + 1).toDouble());
        }
    }
    if (values.isEmpty()) {
        *error = QCoreApplication::translate("supply", "No yields in the download");
        return false;
    }
    *out = std::move(values);
    return true;
}

bool CropYields::parse(const QByteArray &body)
{
    return read(body, &m_values, &m_error);
}

RemoteDataSet::Parser CropYields::parser()
{
    return [this](const QList<QByteArray> &parts) {
        QString error;
        const QByteArray compact = build(parts, &error);
        auto values = std::make_shared<Values>();
        const bool ok = !compact.isEmpty() && read(compact, values.get(), &error);
        return Parsed {[this, ok, values, error] {
                           if (!ok) {
                               m_error = error;
                               return false;
                           }
                           m_values = std::move(*values);
                           return true;
                       },
                       compact};
    };
}

double CropYields::yield(const QString &iso3, Crop crop, int year) const
{
    waitCached();
    const auto it = m_values.constFind(iso3);
    return it == m_values.constEnd() ? NaN : it.value()[int(crop)].years.value(year, NaN);
}

double CropYields::change(const QString &iso3, Crop crop, int year) const
{
    const double now = yield(iso3, crop, year), before = yield(iso3, crop, year - 1);
    return now > 0 && before > 0 ? 100 * (now / before - 1) : NaN;
}

QMap<int, double> CropYields::series(const QString &iso3, Crop crop) const
{
    waitCached();
    const auto it = m_values.constFind(iso3);
    return it == m_values.constEnd() ? QMap<int, double>() : it.value()[int(crop)].years;
}

CropYields::Source CropYields::source(const QString &iso3, Crop crop) const
{
    waitCached();
    const auto it = m_values.constFind(iso3);
    return it == m_values.constEnd() ? Source::Usda : it.value()[int(crop)].source;
}

int CropYields::lastYear(Crop crop) const
{
    waitCached();
    int last = 0;
    for (const auto &crops : m_values)
        if (!crops[int(crop)].years.isEmpty())
            last = std::max(last, crops[int(crop)].years.lastKey());
    return last;
}

int CropYields::defaultYear(Crop crop) const
{
    waitCached();
    const int last = lastYear(crop);
    if (!last)
        return QDate::currentDate().year() - 1;
    QMap<int, int> counts;
    for (const auto &crops : m_values) {
        const QMap<int, double> &y = crops[int(crop)].years;
        for (int year = last - 3; year <= last; ++year)
            if (y.contains(year) && y.contains(year - 1))
                ++counts[year];
    }
    int best = 0;
    for (int n : std::as_const(counts))
        best = std::max(best, n);
    for (int year = last; year > last - 3; --year)
        if (counts.value(year) >= 0.9 * best)
            return year;
    return last;
}

// ---------------------------------------------------------------- RoadFreight

RoadFreight::RoadFreight(QObject *parent)
    : RemoteDataSet("supply_road_freight", 7 * 24 * 3600, parent)
{
    loadCached();
}

QUrl RoadFreight::url() const
{
    return QUrl("https://sdmx.oecd.org/public/rest/data/OECD.ITF,DSD_TRENDS@DF_TRENDSFREIGHT,/all?startPeriod=2008"
                "&dimensionAtObservation=AllDimensions&format=csvfile");
}

void RoadFreight::prepare(QNetworkRequest &request) const
{
    request.setRawHeader("Accept-Language", "en");   // the OECD's server fails on Qt's default
}

bool RoadFreight::parse(const QByteArray &body)
{
    QHash<QString, QMap<int, double>> values;
    QList<QByteArray> header;
    int cArea = -1, cMeasure = -1, cUnit = -1, cMode = -1, cTime = -1, cValue = -1, cMult = -1;
    int last = 0;
    for (QByteArray line : body.split('\n')) {
        if (line.endsWith('\r'))
            line.chop(1);
        if (line.isEmpty())
            continue;
        const QList<QByteArray> f = splitCsvLine(line);
        if (header.isEmpty()) {
            header = f;
            if (!header.isEmpty() && header[0].startsWith("\xEF\xBB\xBF"))
                header[0] = header[0].mid(3);
            cArea = header.indexOf("REF_AREA");
            cMeasure = header.indexOf("MEASURE");
            cUnit = header.indexOf("UNIT_MEASURE");
            cMode = header.indexOf("TRANSPORT_MODE");
            cTime = header.indexOf("TIME_PERIOD");
            cValue = header.indexOf("OBS_VALUE");
            cMult = header.indexOf("UNIT_MULT");
            continue;
        }
        if (std::min({cArea, cMeasure, cUnit, cMode, cTime, cValue}) < 0 || f.size() < header.size())
            continue;
        if (f[cMeasure] != "FREIGHT" || f[cMode] != "ROAD" || f[cUnit] != "TKM" || f[cValue].isEmpty())
            continue;
        // Million tonne-kilometres, whatever the multiplier of the observation.
        const int mult = cMult >= 0 && !f[cMult].isEmpty() ? f[cMult].toInt() : 6;
        const double v = f[cValue].toDouble() * std::pow(10.0, mult - 6);
        const int year = f[cTime].left(4).toInt();
        if (v > 0 && year > 0) {
            values[QString::fromLatin1(f[cArea])].insert(year, v);
            last = std::max(last, year);
        }
    }
    if (values.isEmpty()) {
        m_error = tr("No road freight figures in the answer");
        return false;
    }
    m_values = std::move(values);
    m_lastYear = last;
    return true;
}

double RoadFreight::tonneKm(const QString &iso3, int year) const
{
    waitCached();
    return m_values.value(iso3).value(year, NaN);
}

double RoadFreight::change(const QString &iso3, int year) const
{
    const double now = tonneKm(iso3, year), before = tonneKm(iso3, year - 1);
    return now > 0 && before > 0 ? 100 * (now / before - 1) : NaN;
}

int RoadFreight::defaultYear() const
{
    waitCached();
    // The latest year at least 70% of the countries of the year before have reported.
    for (int year = m_lastYear; year > m_lastYear - 4; --year) {
        int now = 0, before = 0;
        for (const auto &s : m_values) {
            now += s.contains(year) && s.contains(year - 1);
            before += s.contains(year - 1) && s.contains(year - 2);
        }
        if (now >= 0.7 * before && now > 0)
            return year;
    }
    return m_lastYear;
}

// ---------------------------------------------------------------- OilStocks

OilStocks::OilStocks(QObject *parent)
    : RemoteDataSet("supply_jodi_crude_stocks", 7 * 24 * 3600, parent)
{
    m_transferTimeoutMs = 120 * 1000;
    loadCached();
}

QUrl OilStocks::url() const
{
    return QUrl("https://www.jodidata.org/_resources/files/downloads/oil-data/world_Primary_CSV.zip");
}

QByteArray OilStocks::build(const QByteArray &zip, QString *error)
{
    // REF_AREA,TIME_PERIOD,ENERGY_PRODUCT,FLOW_BREAKDOWN,UNIT_MEASURE,OBS_VALUE,ASSESSMENT_CODE; "-" or "x"
    // when not reported.
    QHash<QByteArray, QMap<QDate, double>> values;
    const bool ok = forEachZipLine(zip, "", [&](const QByteArray &line) {
        if (!line.contains(",CRUDEOIL,CLOSTLV,KBBL,"))
            return;
        const QList<QByteArray> f = line.trimmed().split(',');
        if (f.size() < 6 || f[1] < "2015")
            return;
        bool number = false;
        const double v = f[5].toDouble(&number);
        const QDate month = QDate::fromString(QString::fromLatin1(f[1]), "yyyy-MM");
        if (number && month.isValid())
            values[f[0]].insert(month, v);
    });
    if (!ok) {
        *error = QCoreApplication::translate("supply", "Unreadable JODI file");
        return {};
    }
    QByteArray out;
    for (auto it = values.begin(); it != values.end(); ++it) {
        const QString iso3 = iso3FromIso2(QString::fromLatin1(it.key()));
        if (iso3.isEmpty() || it.value().isEmpty())
            continue;
        out += iso3.toLatin1() + '\t'
               + joinSeries<QDate>(it.value(), [](const QDate &d) { return d.toString("yyyy-MM").toLatin1(); }) + '\n';
    }
    if (out.isEmpty())
        *error = QCoreApplication::translate("supply", "No crude stocks in the download");
    return out;
}

bool OilStocks::read(const QByteArray &compact, Values *out, QDate *newest, QString *error)
{
    Values values;
    QDate last;
    for (const QByteArray &line : compact.split('\n')) {
        const int tab = line.indexOf('\t');
        if (tab != 3)
            continue;
        QMap<QDate, double> &s = values[QString::fromLatin1(line.left(3))];
        for (const QByteArray &pair : line.mid(4).split(' ')) {
            const int colon = pair.indexOf(':');
            const QDate month = QDate::fromString(QString::fromLatin1(pair.left(colon)), "yyyy-MM");
            if (colon > 0 && month.isValid()) {
                s.insert(month, pair.mid(colon + 1).toDouble());
                if (!last.isValid() || month > last)
                    last = month;
            }
        }
    }
    if (values.isEmpty()) {
        *error = QCoreApplication::translate("supply", "No crude stocks in the download");
        return false;
    }
    *out = std::move(values);
    *newest = last;
    return true;
}

bool OilStocks::parse(const QByteArray &body)
{
    return read(body, &m_values, &m_newest, &m_error);
}

QByteArray OilStocks::cacheBody(const QByteArray &body) const
{
    return body;   // parser() hands the compact form already
}

RemoteDataSet::Parser OilStocks::parser()
{
    return [this](const QList<QByteArray> &parts) {
        QString error;
        const QByteArray compact = build(parts.value(0), &error);
        auto values = std::make_shared<Values>();
        auto newest = std::make_shared<QDate>();
        const bool ok = !compact.isEmpty() && read(compact, values.get(), newest.get(), &error);
        return Parsed {[this, ok, values, newest, error] {
                           if (!ok) {
                               m_error = error;
                               return false;
                           }
                           m_values = std::move(*values);
                           m_newest = *newest;
                           return true;
                       },
                       compact};
    };
}

bool OilStocks::latest(const QString &iso3, Latest *out) const
{
    waitCached();
    const QMap<QDate, double> s = m_values.value(iso3);
    if (s.isEmpty() || s.lastKey().addMonths(12) <= m_newest)
        return false;
    out->month = s.lastKey();
    out->value = s.last();
    out->yearAgo = s.value(out->month.addYears(-1), 0);
    out->change = out->yearAgo > 0 ? 100 * (out->value / out->yearAgo - 1) : NaN;
    return out->value > 0;
}

QHash<QString, double> OilStocks::changes() const
{
    waitCached();
    QHash<QString, double> out;
    for (auto it = m_values.begin(); it != m_values.end(); ++it) {
        Latest l;
        if (latest(it.key(), &l) && std::isfinite(l.change))
            out.insert(it.key(), l.change);
    }
    return out;
}

// ---------------------------------------------------------------- EiaOilStocks

EiaOilStocks::EiaOilStocks(QObject *parent)
    : RemoteDataSet("supply_eia_crude_stocks", 12 * 3600, parent)
{
    loadCached();
}

QStringList EiaOilStocks::seriesIds()
{
    return {"W_EPC0_SAX_YCUOK_MBBL", "WCESTP31", "WCSSTUS1", "WCESTUS1"};
}

QUrl EiaOilStocks::url() const
{
    QUrl url("https://api.eia.gov/v2/petroleum/stoc/wstk/data/");
    QUrlQuery q;
    q.addQueryItem("api_key", sourceKey("eia"));
    q.addQueryItem("frequency", "weekly");
    q.addQueryItem("data[0]", "value");
    q.addQueryItem("start", QDate::currentDate().addYears(-3).toString(Qt::ISODate));
    for (const QString &id : seriesIds())
        q.addQueryItem("facets[series][]", id);
    q.addQueryItem("length", "5000");
    url.setQuery(q);
    return url;
}

bool EiaOilStocks::ready(QString *why) const
{
    if (!sourceKey("eia").isEmpty())
        return true;
    *why = tr("A free EIA key is needed (Sources)");
    return false;
}

bool EiaOilStocks::parse(const QByteArray &body)
{
    const QJsonObject root = QJsonDocument::fromJson(body).object();
    QHash<QString, DatedSeries> values;
    for (const QJsonValue &v : root.value("response").toObject().value("data").toArray()) {
        const QJsonObject o = v.toObject();
        const QDate day = ymd(o.value("period").toString());
        const QJsonValue value = o.value("value");
        const double x = value.isString() ? value.toString().toDouble() : value.toDouble();
        if (day.isValid() && x > 0)
            values[o.value("series").toString()].insert(day, x);
    }
    if (values.isEmpty()) {
        m_error = root.contains("error") ? root.value("error").toString() : tr("No stocks in the answer");
        return false;
    }
    m_values = std::move(values);
    return true;
}

// ---------------------------------------------------------------- PressureIndex

PressureIndex::PressureIndex(QObject *parent)
    : RemoteDataSet("supply_gscpi", 24 * 3600, parent)
{
    loadCached();
}

QUrl PressureIndex::url() const
{
    return QUrl("https://www.newyorkfed.org/medialibrary/research/interactives/data/gscpi/gscpi_interactive_data.csv");
}

bool PressureIndex::parse(const QByteArray &body)
{
    // Date,Jan-22,Feb-22,… : one column per vintage, the latest last; rows "30-Sep-1997,-0.49,…".
    DatedSeries values;
    int column = -1;
    for (QByteArray line : body.split('\n')) {
        line = line.trimmed();
        const QList<QByteArray> f = line.split(',');
        if (column < 0) {
            if (f.value(0).startsWith("Date") || f.value(0).endsWith("Date"))
                for (int i = f.size() - 1; i > 0; --i)
                    if (!f[i].trimmed().isEmpty()) {
                        column = i;
                        break;
                    }
            continue;
        }
        if (f.size() <= column || f[0].isEmpty())
            continue;
        const QDate day = QLocale(QLocale::English).toDate(QString::fromLatin1(f[0]), "d-MMM-yyyy");
        bool ok = false;
        const double v = f[column].toDouble(&ok);
        if (day.isValid() && ok)
            values.insert(QDate(day.year(), day.month(), 1), v);
    }
    if (values.size() < 12) {
        m_error = tr("No index values in the answer");
        return false;
    }
    m_values = std::move(values);
    return true;
}

// ---------------------------------------------------------------- FreightIndices

FreightIndices::FreightIndices(QObject *parent)
    : RemoteDataSet("supply_fred_freight", 12 * 3600, parent)
{
    loadCached();
}

QVector<FreightIndices::Info> FreightIndices::infos()
{
    return {{"TRUCKD11", tr("Truck tonnage"), tr("American Trucking Associations, index 2015 = 100, seasonally adjusted")},
            {"FRGSHPUSM649NCIS", tr("Freight shipments"), tr("Cass Freight Index, shipments, index January 1990 = 1")},
            {"TSIFRGHT", tr("Freight transportation services"), tr("US Bureau of Transportation Statistics, index 2000 = 100")},
            {"PCU483111483111", tr("Deep sea freight prices"), tr("Producer price index, deep sea freight transportation")}};
}

QUrl FreightIndices::url() const
{
    return urls().first();
}

QList<QUrl> FreightIndices::urls() const
{
    QList<QUrl> list;
    for (const Info &i : infos()) {
        QUrl url("https://api.stlouisfed.org/fred/series/observations");
        QUrlQuery q;
        q.addQueryItem("series_id", i.id);
        q.addQueryItem("api_key", sourceKey("fred"));
        q.addQueryItem("file_type", "json");
        q.addQueryItem("observation_start", QDate::currentDate().addYears(-8).toString(Qt::ISODate));
        url.setQuery(q);
        list << url;
    }
    return list;
}

QByteArray FreightIndices::joinParts(const QList<QByteArray> &parts) const
{
    // One JSON object per series, keyed by its id.
    QJsonObject all;
    const QVector<Info> list = infos();
    for (int i = 0; i < parts.size() && i < list.size(); ++i)
        all.insert(list[i].id, QJsonDocument::fromJson(parts[i]).object());
    return QJsonDocument(all).toJson(QJsonDocument::Compact);
}

bool FreightIndices::ready(QString *why) const
{
    if (!sourceKey("fred").isEmpty())
        return true;
    *why = tr("A free FRED key is needed (Sources)");
    return false;
}

bool FreightIndices::parse(const QByteArray &body)
{
    const QJsonObject all = QJsonDocument::fromJson(body).object();
    QHash<QString, DatedSeries> values;
    for (auto it = all.begin(); it != all.end(); ++it)
        for (const QJsonValue &o : it.value().toObject().value("observations").toArray()) {
            bool ok = false;
            const double v = o.toObject().value("value").toString().toDouble(&ok);
            const QDate day = ymd(o.toObject().value("date").toString());
            if (ok && day.isValid())
                values[it.key()].insert(day, v);
        }
    if (values.isEmpty()) {
        m_error = tr("No figures in the answer");
        return false;
    }
    m_values = std::move(values);
    return true;
}

// ---------------------------------------------------------------- PortActivity

namespace {

const char *PortWatchBase = "https://services9.arcgis.com/weJ1QsnbMYJlCHdG/ArcGIS/rest/services/";
constexpr int PaceMs = 1500;               // after an answer, before the next request
constexpr qint64 StaleSecs = 24 * 3600;    // PortWatch publishes once a week, a few days behind
constexpr int PortsMaxAgeDays = 30;
const char *CacheName = "supply_portwatch_ports";

QDate fromEpochMs(const QJsonValue &v)
{
    return v.isNull() || v.isUndefined() ? QDate()
                                         : QDateTime::fromMSecsSinceEpoch(qint64(v.toDouble()), QTimeZone::UTC).date();
}

} // namespace

PortActivity::PortActivity(QObject *parent)
    : QObject(parent)
{
    m_retryTimer.setSingleShot(true);
    connect(&m_retryTimer, &QTimer::timeout, this, &PortActivity::refreshNow);
    loadCache();
}

QUrl PortActivity::probeUrl()
{
    return QUrl(QString(PortWatchBase) + "PortWatch_ports_database/FeatureServer/0/query?where=1%3D0&outFields=portid&f=json");
}

const PortActivity::Port *PortActivity::port(const QString &id) const
{
    const auto it = m_index.constFind(id);
    return it == m_index.constEnd() ? nullptr : &m_ports[it.value()];
}

QVector<PortActivity::Disruption> PortActivity::recentDisruptions(int days) const
{
    const QDate since = QDate::currentDate().addDays(-days);
    QVector<Disruption> out;
    for (const Disruption &d : m_disruptions)
        if (!d.to.isValid() ? d.from.isValid() && d.from >= QDate::currentDate().addYears(-1) : d.to >= since)
            out.push_back(d);
    std::sort(out.begin(), out.end(), [](const Disruption &a, const Disruption &b) { return a.from > b.from; });
    return out;
}

void PortActivity::refreshIfStale()
{
    if (!m_fetchedAt.isValid() || m_fetchedAt.secsTo(QDateTime::currentDateTimeUtc()) > StaleSecs)
        refreshNow();
}

void PortActivity::setEnabled(bool on)
{
    m_enabled = on;
    if (on)
        refreshIfStale();
    else
        m_retryTimer.stop();
}

void PortActivity::refreshNow()
{
    if (m_fetching || !m_enabled)
        return;
    m_fetching = true;
    m_inFlight = 0;
    m_steps.clear();
    m_partial.clear();
    m_newPorts.clear();
    const bool portsFresh = !m_ports.isEmpty() && m_portsFetchedAt.isValid()
                            && m_portsFetchedAt.daysTo(QDateTime::currentDateTimeUtc()) < PortsMaxAgeDays;
    if (!portsFresh)
        m_steps.push_back({Step::PortsPage, 0, {}, false});
    m_steps.push_back({Step::Disruptions, 0, {}, false});
    m_steps.push_back({Step::LatestDate, 0, {}, false});
    emit updated();
    next();
}

void PortActivity::next()
{
    if (!m_fetching)
        return;
    if (m_steps.isEmpty()) {
        if (m_inFlight == 0)
            finish();
        return;
    }
    // The port list, the disruptions and the latest date one after the other; then the days, three at a time
    // (each takes the service ~10 s).
    while (!m_steps.isEmpty()) {
        const bool day = m_steps.first().kind == Step::Day;
        if (m_inFlight >= (day ? 3 : 1) || (!day && m_inFlight > 0))
            break;
        ++m_inFlight;
        request(m_steps.takeFirst());
    }
}

void PortActivity::retry(Step step, const QString &error, int delayMs)
{
    if (++step.tries >= 8) {
        if (step.kind != Step::Day) {
            fail(error);
            return;
        }
        m_error = error;   // this day waits for the next fetch
        m_partial.remove(step.day);
        qWarning("PortWatch ports: %s gave up on %s", qPrintable(error), qPrintable(step.day.toString(Qt::ISODate)));
        QTimer::singleShot(PaceMs, this, &PortActivity::next);
        return;
    }
    m_steps.prepend(step);
    QTimer::singleShot(delayMs, this, &PortActivity::next);
}

void PortActivity::request(const Step &step)
{
    QString path;
    // Every value percent-encoded by hand ("1=1" left as is makes the service answer "Invalid URL").
    struct Query {
        QStringList parts;
        void addQueryItem(const QString &key, const QString &value)
        {
            parts << key + '=' + QString::fromLatin1(QUrl::toPercentEncoding(value));
        }
    } q;
    switch (step.kind) {
    case Step::PortsPage:
        path = "PortWatch_ports_database/FeatureServer/0/query";
        q.addQueryItem("where", "1=1");
        q.addQueryItem("outFields", "portid,portname,country,ISO3,lat,lon,vessel_count_total,vessel_count_container,"
                                    "vessel_count_dry_bulk,vessel_count_tanker,industry_top1,industry_top2,industry_top3,"
                                    "share_country_maritime_import,share_country_maritime_export,LOCODE");
        q.addQueryItem("resultOffset", QString::number(step.offset));
        q.addQueryItem("resultRecordCount", "1000");
        break;
    case Step::Disruptions:
        path = "portwatch_disruptions_database/FeatureServer/0/query";
        q.addQueryItem("where", "1=1");
        q.addQueryItem("outFields", "eventid,eventtype,eventname,alertlevel,country,fromdate,todate,severitytext,lat,long,"
                                    "affectedports");
        q.addQueryItem("resultRecordCount", "2000");
        break;
    case Step::LatestDate:
        path = "Daily_Ports_Data/FeatureServer/0/query";
        q.addQueryItem("where", "1=1");
        q.addQueryItem("outStatistics",
                       R"([{"statisticType":"max","onStatisticField":"date","outStatisticFieldName":"latest"}])");
        break;
    case Step::Day:
        // A filter on anything but the date fields (the port id), or a sum over several days, makes the
        // service time out; "import" and "export" alone are reserved words it rejects.
        path = "Daily_Ports_Data/FeatureServer/0/query";
        q.addQueryItem("where", QString("year = %1 AND month = %2 AND day = %3")
                                    .arg(step.day.year()).arg(step.day.month()).arg(step.day.day()));
        q.addQueryItem("outFields", "portid,portcalls,import_cargo,import_tanker,export_cargo,export_tanker");
        q.addQueryItem("resultOffset", QString::number(step.offset));
        q.addQueryItem("resultRecordCount", "1000");
        break;
    }
    q.addQueryItem("returnGeometry", "false");
    q.addQueryItem("f", "json");
    QUrl url(QString(PortWatchBase) + path);
    url.setQuery(q.parts.join('&'), QUrl::StrictMode);
    QNetworkRequest req(url);
    req.setTransferTimeout(90 * 1000);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Deepwater/0.1"));
    QNetworkReply *reply = m_net.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, step] {
        reply->deleteLater();
        --m_inFlight;
        if (!m_fetching)
            return;   // an earlier step failed
        const QByteArray body = reply->readAll();
        if (reply->error() != QNetworkReply::NoError && body.isEmpty()) {
            retry(step, reply->errorString(), 10 * 1000);   // 504s and timeouts happen too
            return;
        }
        if (handle(step, body))
            QTimer::singleShot(PaceMs, this, &PortActivity::next);
    });
}

bool PortActivity::handle(const Step &step, const QByteArray &body)
{
    const QJsonObject root = QJsonDocument::fromJson(body).object();
    if (root.contains("error")) {
        const QJsonObject e = root.value("error").toObject();
        // 429: the quota of the minute is spent. The service also fails a valid query after ~12 s now and then
        // (measured: one in three or so), and answers the same query at once right after.
        retry(step, e.value("message").toString(), e.value("code").toInt() == 429 ? 65 * 1000 : 3 * 1000);
        return false;
    }
    const QJsonArray features = root.value("features").toArray();
    const bool more = root.value("exceededTransferLimit").toBool();
    switch (step.kind) {
    case Step::PortsPage:
        for (const QJsonValue &fv : features) {
            const QJsonObject a = fv.toObject().value("attributes").toObject();
            Port p;
            p.id = a.value("portid").toString();
            p.name = a.value("portname").toString();
            p.country = a.value("country").toString();
            p.iso3 = a.value("ISO3").toString();
            p.locode = a.value("LOCODE").toString();
            p.lat = a.value("lat").toDouble();
            p.lon = a.value("lon").toDouble();
            p.vessels = a.value("vessel_count_total").toInt();
            p.container = int(a.value("vessel_count_container").toDouble());
            p.dryBulk = int(a.value("vessel_count_dry_bulk").toDouble());
            p.tanker = int(a.value("vessel_count_tanker").toDouble());
            for (const char *k : {"industry_top1", "industry_top2", "industry_top3"})
                if (const QString s = a.value(k).toString(); !s.isEmpty())
                    p.industries << s;
            p.shareImports = a.value("share_country_maritime_import").toDouble();
            p.shareExports = a.value("share_country_maritime_export").toDouble();
            if (!p.id.isEmpty())
                m_newPorts.push_back(p);
        }
        if (more || features.size() == 1000) {
            m_steps.prepend({Step::PortsPage, step.offset + int(features.size()), {}, false});
        } else if (!m_newPorts.isEmpty()) {
            // The whole list: shown at once (grey until the days arrive), kept with the activity of the last fetch.
            QHash<QString, Port> previous;
            for (const Port &p : std::as_const(m_ports))
                previous.insert(p.id, p);
            m_ports = std::exchange(m_newPorts, {});
            m_portsFetchedAt = QDateTime::currentDateTimeUtc();
            m_index.clear();
            for (int i = 0; i < m_ports.size(); ++i) {
                Port &p = m_ports[i];
                m_index.insert(p.id, i);
                if (const auto it = previous.constFind(p.id); it != previous.constEnd()) {
                    p.calls = it->calls;
                    p.callsYearAgo = it->callsYearAgo;
                    p.imports = it->imports;
                    p.exports = it->exports;
                    p.importsYearAgo = it->importsYearAgo;
                    p.exportsYearAgo = it->exportsYearAgo;
                }
            }
            saveCache();
            emit updated();
        }
        break;
    case Step::Disruptions: {
        QVector<Disruption> list;
        for (const QJsonValue &fv : features) {
            const QJsonObject a = fv.toObject().value("attributes").toObject();
            Disruption d;
            d.id = a.value("eventid").toInt();
            d.type = a.value("eventtype").toString();
            d.name = a.value("eventname").toString();
            d.alert = a.value("alertlevel").toString();
            d.countries = a.value("country").toString();
            d.severity = a.value("severitytext").toString();
            for (const QString &s : a.value("affectedports").toString().split(',', Qt::SkipEmptyParts))
                d.ports << s.trimmed();
            d.from = fromEpochMs(a.value("fromdate"));
            d.to = fromEpochMs(a.value("todate"));
            d.lat = a.value("lat").toDouble();
            d.lon = a.value("long").toDouble();
            list.push_back(d);
        }
        if (!list.isEmpty()) {
            m_disruptions = list;
            if (!m_ports.isEmpty())
                saveCache();
            emit updated();
        }
        break;
    }
    case Step::LatestDate: {
        const QDate end = ymd(features.at(0).toObject().value("attributes").toObject().value("latest").toString());
        if (!end.isValid()) {
            fail(tr("No date in the answer"));
            return false;
        }
        m_windowEnd = end;
        // Only the days not downloaded yet; days out of the window are dropped.
        QSet<QDate> wanted;
        for (int i = 0; i < WindowDays; ++i)
            wanted << end.addDays(-i) << end.addDays(-i - 364);
        for (auto it = m_days.begin(); it != m_days.end();)
            it = wanted.contains(it.key()) ? std::next(it) : m_days.erase(it);
        for (int i = 0; i < WindowDays; ++i)
            for (const QDate &d : {end.addDays(-i), end.addDays(-i - 364)})
                if (!m_days.contains(d))
                    m_steps.push_back({Step::Day, 0, d, d < end.addDays(-WindowDays)});
        break;
    }
    case Step::Day: {
        DayTotals &day = m_partial[step.day];
        for (const QJsonValue &fv : features) {
            const QJsonObject a = fv.toObject().value("attributes").toObject();
            std::array<double, 3> &w = day[a.value("portid").toString()];
            w[0] += a.value("portcalls").toDouble();
            w[1] += a.value("import_cargo").toDouble() + a.value("import_tanker").toDouble();
            w[2] += a.value("export_cargo").toDouble() + a.value("export_tanker").toDouble();
        }
        if (more || features.size() == 1000) {
            m_steps.prepend({Step::Day, step.offset + int(features.size()), step.day, step.yearAgo});
        } else {
            m_days.insert(step.day, m_partial.take(step.day));
            saveCache();   // a day kept is a day not asked again, should the fetch stop here
        }
        break;
    }
    }
    return true;
}

void PortActivity::fail(const QString &error)
{
    m_fetching = false;
    m_error = error.isEmpty() ? tr("PortWatch refused the request") : error;
    qWarning("PortWatch ports: %s", qPrintable(m_error));
    m_steps.clear();
    m_retryTimer.start(30 * 60 * 1000);
    emit updated();
}

void PortActivity::finish()
{
    m_fetching = false;
    if (!m_newPorts.isEmpty()) {
        m_ports = std::exchange(m_newPorts, {});
        m_portsFetchedAt = QDateTime::currentDateTimeUtc();
    }
    m_index.clear();
    for (int i = 0; i < m_ports.size(); ++i)
        m_index.insert(m_ports[i].id, i);
    applyWindow();
    // Every day in: fresh until tomorrow. Days missing: asked again in half an hour.
    if (m_pairs == WindowDays) {
        m_error.clear();
        m_fetchedAt = QDateTime::currentDateTimeUtc();
    } else {
        m_retryTimer.start(30 * 60 * 1000);
    }
    saveCache();
    emit updated();
}

void PortActivity::applyWindow()
{
    QVector<QDate> pairs;
    for (int i = 0; i < WindowDays && m_windowEnd.isValid(); ++i) {
        const QDate d = m_windowEnd.addDays(-i);
        if (m_days.contains(d) && m_days.contains(d.addDays(-364)))
            pairs.push_back(d);
    }
    m_pairs = int(pairs.size());
    for (Port &p : m_ports) {
        double w[6] = {};
        bool seen = false;
        for (const QDate &d : std::as_const(pairs)) {
            const auto now = m_days[d].constFind(p.id), before = m_days[d.addDays(-364)].constFind(p.id);
            if (now != m_days[d].constEnd()) {
                seen = true;
                for (int k = 0; k < 3; ++k)
                    w[k] += (*now)[k];
            }
            if (before != m_days[d.addDays(-364)].constEnd())
                for (int k = 0; k < 3; ++k)
                    w[3 + k] += (*before)[k];
        }
        p.calls = seen ? int(w[0]) : -1;
        p.imports = w[1];
        p.exports = w[2];
        p.callsYearAgo = seen || w[3] > 0 ? int(w[3]) : -1;
        p.importsYearAgo = w[4];
        p.exportsYearAgo = w[5];
    }
}

void PortActivity::saveCache() const
{
    QJsonArray ports;
    for (const Port &p : m_ports)
        ports.append(QJsonArray {p.id, p.name, p.country, p.iso3, p.locode, p.lat, p.lon, p.vessels, p.container,
                                 p.dryBulk, p.tanker, p.industries.join('|'), p.shareImports, p.shareExports, p.calls,
                                 p.callsYearAgo, p.imports, p.exports, p.importsYearAgo, p.exportsYearAgo});
    QJsonArray events;
    for (const Disruption &d : m_disruptions)
        events.append(QJsonArray {d.id, d.type, d.name, d.alert, d.countries, d.severity, d.ports.join('|'),
                                  d.from.toString(Qt::ISODate), d.to.toString(Qt::ISODate), d.lat, d.lon});
    QJsonObject days;
    for (auto it = m_days.begin(); it != m_days.end(); ++it) {
        QJsonArray rows;
        for (auto r = it.value().begin(); r != it.value().end(); ++r)
            rows.append(QJsonArray {r.key(), r.value()[0], r.value()[1], r.value()[2]});
        days.insert(it.key().toString(Qt::ISODate), rows);
    }
    const QJsonObject root {{"ports", ports},
                            {"days", days},
                            {"disruptions", events},
                            {"windowEnd", m_windowEnd.toString(Qt::ISODate)},
                            {"portsFetchedAt", m_portsFetchedAt.toString(Qt::ISODate)}};
    HttpCache::save(CacheName, QJsonDocument(root).toJson(QJsonDocument::Compact), m_fetchedAt);
}

void PortActivity::loadCache()
{
    QByteArray body;
    QDateTime at;
    if (!HttpCache::load(CacheName, &body, &at))
        return;
    const QJsonObject root = QJsonDocument::fromJson(body).object();
    for (const QJsonValue &v : root.value("ports").toArray()) {
        const QJsonArray a = v.toArray();
        if (a.size() < 20)
            continue;
        Port p;
        p.id = a[0].toString();
        p.name = a[1].toString();
        p.country = a[2].toString();
        p.iso3 = a[3].toString();
        p.locode = a[4].toString();
        p.lat = a[5].toDouble();
        p.lon = a[6].toDouble();
        p.vessels = a[7].toInt();
        p.container = a[8].toInt();
        p.dryBulk = a[9].toInt();
        p.tanker = a[10].toInt();
        p.industries = a[11].toString().split('|', Qt::SkipEmptyParts);
        p.shareImports = a[12].toDouble();
        p.shareExports = a[13].toDouble();
        p.calls = a[14].toInt();
        p.callsYearAgo = a[15].toInt();
        p.imports = a[16].toDouble();
        p.exports = a[17].toDouble();
        p.importsYearAgo = a[18].toDouble();
        p.exportsYearAgo = a[19].toDouble();
        m_index.insert(p.id, int(m_ports.size()));
        m_ports.push_back(p);
    }
    for (const QJsonValue &v : root.value("disruptions").toArray()) {
        const QJsonArray a = v.toArray();
        if (a.size() < 11)
            continue;
        Disruption d;
        d.id = a[0].toInt();
        d.type = a[1].toString();
        d.name = a[2].toString();
        d.alert = a[3].toString();
        d.countries = a[4].toString();
        d.severity = a[5].toString();
        d.ports = a[6].toString().split('|', Qt::SkipEmptyParts);
        d.from = ymd(a[7].toString());
        d.to = ymd(a[8].toString());
        d.lat = a[9].toDouble();
        d.lon = a[10].toDouble();
        m_disruptions.push_back(d);
    }
    m_windowEnd = ymd(root.value("windowEnd").toString());
    const QJsonObject days = root.value("days").toObject();
    for (auto it = days.begin(); it != days.end(); ++it) {
        DayTotals &day = m_days[ymd(it.key())];
        for (const QJsonValue &r : it.value().toArray()) {
            const QJsonArray a = r.toArray();
            day.insert(a.at(0).toString(), {a.at(1).toDouble(), a.at(2).toDouble(), a.at(3).toDouble()});
        }
    }
    applyWindow();
    m_portsFetchedAt = QDateTime::fromString(root.value("portsFetchedAt").toString(), Qt::ISODate);
    if (!m_ports.isEmpty())
        m_fetchedAt = at;
}

// ---------------------------------------------------------------- SupplyChain

SupplyChain::SupplyChain(QObject *parent)
    : QObject(parent)
    , m_crops(new CropYields(this))
    , m_road(new RoadFreight(this))
    , m_oil(new OilStocks(this))
    , m_eia(new EiaOilStocks(this))
    , m_pressure(new PressureIndex(this))
    , m_freight(new FreightIndices(this))
    , m_ports(new PortActivity(this))
{
    for (RemoteDataSet *d : std::initializer_list<RemoteDataSet *> {m_crops, m_road, m_oil, m_eia, m_pressure, m_freight})
        connect(d, &RemoteDataSet::updated, this, &SupplyChain::updated);
    connect(m_ports, &PortActivity::updated, this, &SupplyChain::updated);
    for (const QJsonValue &v : bundled().value("oilSites").toArray()) {
        const QJsonObject o = v.toObject();
        m_sites.push_back({o.value("id").toString(), o.value("name").toString(), o.value("iso3").toString(),
                           o.value("lat").toDouble(), o.value("lon").toDouble(), o.value("kind").toString(),
                           o.value("eia").toString(), o.value("about").toString()});
    }
}

const OilSite *SupplyChain::oilSite(const QString &id) const
{
    for (const OilSite &s : m_sites)
        if (s.id == id)
            return &s;
    return nullptr;
}

void SupplyChain::refreshIfStale()
{
    for (RemoteDataSet *d : std::initializer_list<RemoteDataSet *> {m_crops, m_road, m_oil, m_eia, m_pressure, m_freight})
        d->refreshIfStale();
    m_ports->refreshIfStale();
}

QString SupplyChain::cropName(Crop crop)
{
    switch (crop) {
    case Crop::Wheat: return tr("Wheat");
    case Crop::Corn: return tr("Corn");
    case Crop::Rice: return tr("Rice");
    case Crop::Soybeans: return tr("Soybeans");
    }
    return {};
}

QString SupplyChain::siteKindName(const QString &kind)
{
    if (kind == "hub") return tr("Storage and trading hub");
    if (kind == "reserve") return tr("Strategic reserve");
    if (kind == "export") return tr("Export terminal");
    if (kind == "import") return tr("Import terminal");
    return kind;
}

QString SupplyChain::disruptionTypeName(const QString &type)
{
    if (type == "EQ") return tr("Earthquake");
    if (type == "TC") return tr("Tropical cyclone");
    if (type == "FL") return tr("Flood");
    if (type == "WF") return tr("Wildfire");
    if (type == "VO") return tr("Volcano");
    if (type == "DR") return tr("Drought");
    if (type == "TS") return tr("Tsunami");
    return tr("Other disruption");
}

double SupplyChain::changePct(double now, double before)
{
    return before > 0 && now >= 0 ? 100 * (now / before - 1) : NaN;
}

QColor SupplyChain::portColor(const PortActivity::Port &port)
{
    const Palette &pal = Theme::current();
    const double c = changePct(port.calls, port.callsYearAgo);
    if (port.calls < 0 || !std::isfinite(c))
        return pal.TextFaint;
    // Full color at ±40%.
    const double t = std::clamp(std::abs(c) / 40.0, 0.0, 1.0);
    const QColor &to = c >= 0 ? pal.Up : pal.Down;
    const QColor &from = pal.TextDim;
    const double k = 0.25 + 0.75 * t;
    return QColor::fromRgbF(float(from.redF() + (to.redF() - from.redF()) * k), float(from.greenF() + (to.greenF() - from.greenF()) * k),
                            float(from.blueF() + (to.blueF() - from.blueF()) * k));
}

QColor SupplyChain::alertColor(const QString &alert)
{
    if (alert == "RED")
        return QColor(0xe5, 0x48, 0x4d);
    if (alert == "ORANGE")
        return QColor(0xf5, 0x9e, 0x0b);
    return QColor(0x46, 0xa7, 0x58);
}

} // namespace supply
