#include "DomesticFinance.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QSet>

#include <algorithm>
#include <cmath>

namespace {

const QByteArray CacheMagic = "DWSERIES2";

// The columns of a CSV header, by name.
QHash<QByteArray, int> columns(const QByteArray &headerLine)
{
    QHash<QByteArray, int> out;
    const QList<QByteArray> names = splitCsvLine(headerLine.trimmed());
    for (int i = 0; i < names.size(); ++i)
        out.insert(names[i].startsWith("\xEF\xBB\xBF") ? names[i].mid(3) : names[i], i);
    return out;
}

} // namespace

// Lines of an SDMX CSV (BIS, OECD): calls `f` with the field getter of each data line.
void forEachCsvRow(const QByteArray &csv, const std::function<void(const std::function<QByteArray(const char *)> &)> &f)
{
    const QList<QByteArray> lines = csv.split('\n');
    if (lines.isEmpty())
        return;
    const QHash<QByteArray, int> cols = columns(lines.first());
    for (int i = 1; i < lines.size(); ++i) {
        const QList<QByteArray> fields = splitCsvLine(lines[i].trimmed());
        if (fields.size() < 2)
            continue;
        f([&](const char *name) { return fields.value(cols.value(name, -1)); });
    }
}

namespace {

// The countries of resources/country_names.json: ISO3 codes, and ISO2 -> ISO3.
struct CountryCodes {
    QSet<QString> iso3;
    QHash<QString, QString> fromIso2;
};

const CountryCodes &countryCodes()
{
    static const CountryCodes codes = [] {
        CountryCodes c;
        QFile file(":/country_names.json");
        if (file.open(QIODevice::ReadOnly)) {
            const QJsonObject names = QJsonDocument::fromJson(file.readAll()).object();
            for (auto it = names.begin(); it != names.end(); ++it) {
                c.iso3.insert(it.key());
                c.fromIso2.insert(it.value().toArray().at(0).toString(), it.key());
            }
        }
        return c;
    }();
    return codes;
}

} // namespace

// ---------------------------------------------------------------- CountrySeriesSet

CountrySeriesSet::CountrySeriesSet(const QString &cacheName, qint64 staleSecs, int seriesCount, QObject *parent)
    : RemoteDataSet(cacheName, staleSecs, parent)
    , m_seriesCount(seriesCount)
{
}

QMap<double, double> CountrySeriesSet::series(const QString &iso3, int k) const
{
    const auto it = m_values.constFind(iso3);
    return it == m_values.constEnd() || k < 0 || k >= it->size() ? QMap<double, double>() : it->at(k);
}

double CountrySeriesSet::latest(const QString &iso3, int k, double *time) const
{
    const QMap<double, double> s = series(iso3, k);
    if (s.isEmpty())
        return std::nan("");
    if (time)
        *time = s.lastKey();
    return s.last();
}

double CountrySeriesSet::at(const QString &iso3, int k, double time, double tolerance) const
{
    const QMap<double, double> s = series(iso3, k);
    auto it = s.upperBound(time + 1e-6);
    if (it == s.constBegin())
        return std::nan("");
    --it;
    return time - it.key() <= tolerance ? it.value() : std::nan("");
}

double CountrySeriesSet::median(int k) const
{
    QVector<double> values;
    for (const auto &perSeries : m_values)
        if (k < perSeries.size() && !perSeries[k].isEmpty())
            values.push_back(perSeries[k].last());
    if (values.isEmpty())
        return std::nan("");
    std::sort(values.begin(), values.end());
    return values[values.size() / 2];
}

QByteArray CountrySeriesSet::header()
{
    return CacheMagic + '\n';
}

void CountrySeriesSet::addLine(QByteArray &out, const QString &iso3, int k, double time, double value)
{
    // Aggregates (WLD, EUU, OECD…) aren't countries, and would skew the medians.
    if (!countryCodes().iso3.contains(iso3) || !std::isfinite(value) || time <= 0)
        return;
    out += iso3.toLatin1() + ' ' + QByteArray::number(k) + ' ' + QByteArray::number(time, 'f', 3) + ' '
           + QByteArray::number(value, 'g', 6) + '\n';
}

double CountrySeriesSet::periodTime(const QByteArray &period)
{
    bool ok = false;
    const int year = period.left(4).toInt(&ok);
    if (!ok || year < 1900)
        return 0;
    if (period.size() == 4)
        return year;
    if (period.size() == 7 && period[5] == 'Q')
        return year + (period.mid(6).toInt() - 0.5) / 4.0;   // mid-quarter, apart from whole years
    if (period.size() == 7 && period[4] == '-' && period[5] >= '0' && period[5] <= '9')
        return year + (period.mid(5).toInt() - 0.5) / 12.0;   // "2024-05": mid-month
    if (period.size() == 8 && period[5] == 'M')
        return year + (period.mid(6).toInt() - 0.5) / 12.0;   // "2024-M05" (IMF)
    return 0;
}

QString CountrySeriesSet::iso3For(const QString &iso2)
{
    return countryCodes().fromIso2.value(iso2);
}

bool CountrySeriesSet::parse(const QByteArray &body)
{
    QHash<QString, QVector<QMap<double, double>>> result;
    if (body.startsWith(CacheMagic)) {
        const QList<QByteArray> lines = body.split('\n');
        for (int i = 1; i < lines.size(); ++i) {
            const QList<QByteArray> f = lines[i].split(' ');
            const int k = f.value(1).toInt();
            if (f.size() != 4 || k < 0 || k >= m_seriesCount)
                continue;
            auto &perSeries = result[QString::fromLatin1(f[0])];
            perSeries.resize(m_seriesCount);
            perSeries[k].insert(f[2].toDouble(), f[3].toDouble());
        }
    }
    if (result.isEmpty()) {
        m_error = tr("No data in the response");
        return false;
    }
    m_values = result;
    return true;
}

// ---------------------------------------------------------------- PropertyCredit

PropertyCredit::PropertyCredit(QObject *parent)
    : CountrySeriesSet("bis_property_credit", 7 * 24 * 3600, SeriesCount, parent)
{
    loadCached();
}

QList<QUrl> PropertyCredit::urls() const
{
    return {
        // Real residential property prices, index 2010 = 100 (~330 KB).
        QUrl("https://stats.bis.org/api/v1/data/WS_SPP/Q..R.628/all?startPeriod=1970&format=csv"),
        // Credit to households (H) and non-financial corporations (N) from all sectors, market value, % of GDP,
        // adjusted for breaks (~2.2 MB).
        QUrl("https://stats.bis.org/api/v1/data/WS_TC/Q..H+N.A.M.770.A/all?startPeriod=1980&format=csv"),
        // Debt service ratios of households (H) and of the private non-financial sector (P) (~380 KB).
        QUrl("https://stats.bis.org/api/v1/data/WS_DSR/Q..H+P/all?format=csv"),
    };
}

QByteArray PropertyCredit::joinParts(const QList<QByteArray> &parts) const
{
    QByteArray out = header();
    forEachCsvRow(parts.value(0), [&](const auto &field) {
        addLine(out, iso3For(QString::fromLatin1(field("REF_AREA"))), RealHousePrices, periodTime(field("TIME_PERIOD")),
                field("OBS_VALUE").isEmpty() ? std::nan("") : field("OBS_VALUE").toDouble());
    });
    forEachCsvRow(parts.value(1), [&](const auto &field) {
        const QByteArray who = field("TC_BORROWERS");
        const int k = who == "H" ? HouseholdCredit : who == "N" ? CorporateCredit : -1;
        if (k >= 0 && !field("OBS_VALUE").isEmpty())
            addLine(out, iso3For(QString::fromLatin1(field("BORROWERS_CTY"))), k, periodTime(field("TIME_PERIOD")),
                    field("OBS_VALUE").toDouble());
    });
    forEachCsvRow(parts.value(2), [&](const auto &field) {
        const QByteArray who = field("DSR_BORROWERS");
        const int k = who == "H" ? HouseholdDebtService : who == "P" ? PrivateDebtService : -1;
        if (k >= 0 && !field("OBS_VALUE").isEmpty())
            addLine(out, iso3For(QString::fromLatin1(field("BORROWERS_CTY"))), k, periodTime(field("TIME_PERIOD")),
                    field("OBS_VALUE").toDouble());
    });
    return out;
}

// ---------------------------------------------------------------- HousePriceRatios

HousePriceRatios::HousePriceRatios(QObject *parent)
    : CountrySeriesSet("oecd_house_price_ratios", 7 * 24 * 3600, SeriesCount, parent)
{
    loadCached();
}

QList<QUrl> HousePriceRatios::urls() const
{
    // Quarterly price-to-income and price-to-rent ratios in % of their long-term average (~920 KB).
    return {QUrl("https://sdmx.oecd.org/public/rest/data/OECD.ECO.MPD,DSD_AN_HOUSE_PRICES@DF_HOUSE_PRICES,/"
                 ".Q.HPI_YDH_AVG+HPI_RPI_AVG.?startPeriod=1990&dimensionAtObservation=AllDimensions&format=csvfile")};
}

void HousePriceRatios::prepare(QNetworkRequest &request) const
{
    request.setRawHeader("Accept-Language", "en");
}

QByteArray HousePriceRatios::joinParts(const QList<QByteArray> &parts) const
{
    QByteArray out = header();
    forEachCsvRow(parts.value(0), [&](const auto &field) {
        const QByteArray measure = field("MEASURE");
        const int k = measure == "HPI_YDH_AVG" ? PriceToIncome : measure == "HPI_RPI_AVG" ? PriceToRent : -1;
        if (k >= 0 && !field("OBS_VALUE").isEmpty())
            addLine(out, QString::fromLatin1(field("REF_AREA")), k, periodTime(field("TIME_PERIOD")),
                    field("OBS_VALUE").toDouble());
    });
    return out;
}

// ---------------------------------------------------------------- GlobalDebt

GlobalDebt::GlobalDebt(QObject *parent)
    : CountrySeriesSet("imf_global_debt_2", 7 * 24 * 3600, SeriesCount, parent)
{
    loadCached();
}

QList<QUrl> GlobalDebt::urls() const
{
    // Loans and debt securities of households (S14) and non-financial corporations (S11), debt of the general
    // government (S13), % of GDP (~200 KB).
    return {QUrl("https://api.imf.org/external/sdmx/3.0/data/dataflow/IMF.FAD/GDD/+/"
                 "*.F3T4_S14_POGDP_PT+F3T4_S11_POGDP_PT+FL_S13_POGDP_PT.A?attributes=none&measures=all")};
}

QByteArray GlobalDebt::joinParts(const QList<QByteArray> &parts) const
{
    // SDMX-JSON, as for the WEO: series keyed by indices into the dimension values.
    const QJsonObject data = QJsonDocument::fromJson(parts.value(0)).object().value("data").toObject();
    const QJsonObject dimensions = data.value("structures").toArray().at(0).toObject().value("dimensions").toObject();
    const QJsonArray seriesDims = dimensions.value("series").toArray();
    auto ids = [](const QJsonValue &dim) {
        QStringList list;
        for (const QJsonValue &v : dim.toObject().value("values").toArray())
            list << v.toObject().value("id").toString(v.toObject().value("value").toString());
        return list;
    };
    int countryDim = -1, indicatorDim = -1;
    for (int i = 0; i < seriesDims.size(); ++i) {
        const QString id = seriesDims[i].toObject().value("id").toString();
        if (id == "COUNTRY") countryDim = i;
        if (id == "INDICATOR") indicatorDim = i;
    }
    QByteArray out = header();
    if (countryDim < 0 || indicatorDim < 0)
        return out;
    const QStringList countries = ids(seriesDims[countryDim]);
    const QStringList indicators = ids(seriesDims[indicatorDim]);
    const QStringList years = ids(dimensions.value("observation").toArray().at(0));
    const QJsonObject series = data.value("dataSets").toArray().at(0).toObject().value("series").toObject();
    for (auto it = series.begin(); it != series.end(); ++it) {
        const QStringList key = it.key().split(':');
        const QString indicator = indicators.value(key.value(indicatorDim).toInt());
        const int k = indicator.startsWith("F3T4_S14") ? HouseholdDebt
                      : indicator.startsWith("F3T4_S11") ? CorporateDebt
                      : indicator.startsWith("FL_S13")   ? GovernmentDebt
                                                         : -1;
        if (k < 0)
            continue;
        const QString iso3 = countries.value(key.value(countryDim).toInt());
        const QJsonObject obs = it.value().toObject().value("observations").toObject();
        for (auto o = obs.begin(); o != obs.end(); ++o) {
            bool ok = false;
            const double v = o.value().toArray().at(0).toString().toDouble(&ok);
            if (ok)
                addLine(out, iso3, k, years.value(o.key().toInt()).toInt(), v);
        }
    }
    return out;
}

// ---------------------------------------------------------------- BankCredit

BankCredit::BankCredit(QObject *parent)
    : CountrySeriesSet("worldbank_bank_credit", 7 * 24 * 3600, SeriesCount, parent)
{
    loadCached();
}

QList<QUrl> BankCredit::urls() const
{
    // Bulk downloads, as for the sectors (~100 KB and ~40 KB).
    return {QUrl("https://api.worldbank.org/v2/en/indicator/FS.AST.PRVT.GD.ZS?downloadformat=csv"),
            QUrl("https://api.worldbank.org/v2/en/indicator/FB.AST.NPER.ZS?downloadformat=csv")};
}

QByteArray BankCredit::joinParts(const QList<QByteArray> &parts) const
{
    QByteArray out = header();
    for (int k = 0; k < parts.size() && k < SeriesCount; ++k)
        forEachWorldBankValue(parts[k], [&](const QByteArray &iso3, int year, double v) {
            addLine(out, QString::fromLatin1(iso3), k, year, v);
        });
    return out;
}
