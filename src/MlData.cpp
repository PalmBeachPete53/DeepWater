#include "MlData.h"

#include "CountryData.h"
#include "DomesticFinance.h"
#include "FxData.h"
#include "KeyedSources.h"
#include "MacroData.h"
#include "MlEnvironment.h"
#include "NationalSources.h"
#include "QuantData.h"
#include "ResearchData.h"
#include "SurveyHeadlines.h"
#include "fusion/Store.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QRegularExpression>
#include <QTextStream>

#include <functional>

#include <cmath>

namespace {

QString csvCell(const QString &s)
{
    if (s.contains(',') || s.contains('"') || s.contains('\n'))
        return '"' + QString(s).replace('"', "\"\"") + '"';
    return s;
}

int writeFile(const QString &name, const QStringList &header, const std::function<void(QTextStream &, int &)> &body)
{
    QDir().mkpath(MlDataExporter::dir());
    QFile f(MlDataExporter::dir() + '/' + name);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return -1;
    QTextStream out(&f);
    out << header.join(',') << '\n';
    int rows = 0;
    body(out, rows);
    return rows;
}

} // namespace

MlDataExporter::MlDataExporter(const Sources &sources, QObject *parent)
    : QObject(parent)
    , m_s(sources)
{
}

QString MlDataExporter::dir()
{
    return MlEnvironment::dataDir();
}

QStringList MlDataExporter::symbols()
{
    return QSettings().value("ml/symbols", QStringList {"SPY", "QQQ", "GLD", "USO", "TLT", "^VIX", "EURUSD=X", "BTC-USD"}).toStringList();
}

void MlDataExporter::setSymbols(const QStringList &symbols)
{
    QSettings().setValue("ml/symbols", symbols);
}

QVector<MlDataExporter::File> MlDataExporter::files()
{
    QVector<File> out;
    for (const QFileInfo &fi : QDir(dir()).entryInfoList({"*.csv"}, QDir::Files, QDir::Name)) {
        File f;
        f.name = fi.fileName();
        f.bytes = fi.size();
        f.written = fi.lastModified();
        QFile file(fi.filePath());
        if (file.open(QIODevice::ReadOnly)) {
            int rows = -1;   // the header
            while (!file.atEnd()) {
                file.readLine();
                ++rows;
            }
            f.rows = std::max(0, rows);
        }
        out.push_back(f);
    }
    return out;
}

QStringList MlDataExporter::columns(const QString &file)
{
    QFile f(dir() + '/' + file);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(f.readLine()).trimmed().split(',');
}

QVector<MlDataExporter::CountrySet> MlDataExporter::countrySets(const Sources &s)
{
    const KeyIndicators::Sets &k = s.key ? s.key->sets() : KeyIndicators::Sets();
    const QVector<CountrySet> all = {
        {k.oecd, "oecd", {"cpi_yoy", "unemployment", "industry_yoy", "retail_yoy", "business_confidence", "consumer_confidence",
                          "leading_indicator", "short_rate", "long_rate", "wages_yoy", "gdp_qoq", "gdp_yoy", "current_account_gdp"}},
        {k.eurostat, "eurostat", {"hicp_yoy", "unemployment", "gdp_qoq", "industry_yoy", "retail_yoy", "consumer_confidence", "ppi_yoy",
                                  "gov_debt_gdp", "gov_balance_gdp"}},
        {k.imf, "imf_cpi", {"cpi_yoy", "cpi_mom"}},
        {k.bls, "bls", {"cpi_yoy", "core_cpi_yoy", "unemployment", "payrolls_change", "earnings_yoy"}},
        {k.brazil, "brazil", {"cpi_yoy", "unemployment", "industry_yoy", "retail_yoy"}},
        {k.argentina, "argentina", {"cpi_yoy", "activity_yoy", "unemployment"}},
        {k.southAfrica, "south_africa", {"cpi_yoy", "ppi_yoy", "policy_rate"}},
        {k.singapore, "singapore", {"cpi_yoy"}},
        {k.fred, "fred", {"pce_inflation_yoy", "housing_starts", "initial_claims", "trade_balance_bn", "consumer_sentiment"}},
        {k.eia, "eia", {"crude_stocks_mb", "crude_production_kbd", "gasoline_price"}},
        {k.bea, "bea", {"gdp_annualized"}},
        {k.banxico, "banxico", {"cpi_yoy", "policy_rate"}},
        {k.ecos, "ecos", {"cpi_yoy"}},
        {k.evds, "evds", {"cpi_yoy"}},
        {k.surveys, "surveys", {"manufacturing_pmi", "services_pmi", "composite_pmi", "business_climate", "economic_sentiment",
                                "consumer_sentiment", "private_payrolls"}},
        {s.property, "bis_property", {"real_house_prices", "household_credit_gdp", "corporate_credit_gdp", "household_debt_service", "private_debt_service"}},
        {s.housePrices, "oecd_housing", {"price_to_income", "price_to_rent"}},
        {s.globalDebt, "imf_gdd", {"household_debt_gdp", "corporate_debt_gdp", "government_debt_gdp"}},
        {s.bankCredit, "worldbank_credit", {"private_credit_gdp", "bad_loans_pct"}},
    };
    QVector<CountrySet> out;
    for (const CountrySet &c : all)
        if (c.set) out << c;
    return out;
}

QStringList MlDataExporter::weoIndicators()
{
    return {"gdp_growth", "inflation", "unemployment", "current_account_gdp", "gov_balance_gdp", "gov_debt_gdp",
            "gdp_usd_bn", "gdp_per_capita_ppp", "export_volume_growth"};
}

int MlDataExporter::writeCountrySeries()
{
    const QVector<CountrySet> sets = countrySets(m_s);
    return writeFile("country_series.csv", {"source", "iso3", "series", "time", "value"}, [&](QTextStream &out, int &rows) {
        for (const CountrySet &ns : sets) {
            if (!ns.set || !ns.set->hasData())
                continue;
            for (const QString &iso3 : ns.set->countries())
                for (int k = 0; k < ns.series.size(); ++k) {
                    const QMap<double, double> s = ns.set->series(iso3, k);
                    for (auto it = s.begin(); it != s.end(); ++it) {
                        out << ns.source << ',' << iso3 << ',' << ns.series[k] << ',' << QString::number(it.key(), 'f', 3) << ','
                            << QString::number(it.value(), 'g', 10) << '\n';
                        ++rows;
                    }
                }
        }
    });
}

int MlDataExporter::writeWeo()
{
    if (!m_s.economy || !m_s.economy->hasData())
        return 0;
    const QStringList names = weoIndicators();
    return writeFile("weo.csv", {"iso3", "indicator", "year", "value"}, [&](QTextStream &out, int &rows) {
        for (const WorldEconomy::Country &c : m_s.economy->countries())
            for (int i = 0; i < WorldEconomy::IndicatorCount && i < names.size(); ++i) {
                const QMap<int, double> s = m_s.economy->series(c.iso3, WorldEconomy::Indicator(i));
                for (auto it = s.begin(); it != s.end(); ++it)
                    if (std::isfinite(it.value())) {
                        out << c.iso3 << ',' << names[i] << ',' << it.key() << ',' << QString::number(it.value(), 'g', 10) << '\n';
                        ++rows;
                    }
            }
    });
}

int MlDataExporter::writeRates()
{
    if (!m_s.rates || !m_s.economy)
        return 0;
    return writeFile("policy_rates.csv", {"iso3", "month", "rate"}, [&](QTextStream &out, int &rows) {
        for (const WorldEconomy::Country &c : m_s.economy->countries())
            for (const WorldPolicyRates::Point &p : m_s.rates->history(c.iso2)) {
                out << c.iso3 << ',' << p.month.toString(Qt::ISODate) << ',' << QString::number(p.rate, 'g', 10) << '\n';
                ++rows;
            }
    });
}

int MlDataExporter::writeFx()
{
    if (!m_s.fx || m_s.fx->dates().isEmpty())
        return 0;
    const QVector<FxPair> &pairs = m_s.fx->pairs();
    QStringList header {"date"};
    QVector<QVector<double>> columns;
    for (int i = 0; i < pairs.size(); ++i) {
        header << pairs[i].base + pairs[i].quote;
        columns.push_back(m_s.fx->series(i));
    }
    return writeFile("fx_daily.csv", header, [&](QTextStream &out, int &rows) {
        const QVector<QDate> &dates = m_s.fx->dates();
        for (int d = 0; d < dates.size(); ++d) {
            out << dates[d].toString(Qt::ISODate);
            for (const QVector<double> &col : columns)
                out << ',' << (d < col.size() && std::isfinite(col[d]) ? QString::number(col[d], 'g', 10) : QString());
            out << '\n';
            ++rows;
        }
    });
}

int MlDataExporter::writeCot()
{
    if (!m_s.cot || !m_s.cot->fetchedAt().isValid())
        return 0;
    return writeFile("cot_legacy.csv", {"market", "date", "open_interest", "nc_long", "nc_short", "nc_spread", "c_long", "c_short",
                                        "nr_long", "nr_short", "traders"},
                     [&](QTextStream &out, int &rows) {
                         for (const CotReport::Market &m : m_s.cot->markets())
                             for (const CotReport::Week &w : m_s.cot->series(m.id).weeks) {
                                 out << m.id << ',' << w.date.toString(Qt::ISODate) << ',' << w.openInterest << ',' << w.ncLong << ','
                                     << w.ncShort << ',' << w.ncSpread << ',' << w.cLong << ',' << w.cShort << ',' << w.nrLong << ','
                                     << w.nrShort << ',' << w.traders << '\n';
                                 ++rows;
                             }
                     });
}

// The golden records of data fusion saved so far (Economy › Countries), every frequency of those fused from two
// sources or more: a row per period, with the source of its value and whether it was rebased or unchecked.
int MlDataExporter::writeGolden()
{
    const QString dir = countries::fusionDir();
    const QVector<fusion::Summary> index = fusion::loadIndex(dir);
    return writeFile("golden_series.csv", {"iso3", "indicator", "frequency", "date", "time", "value", "source", "rebased", "unchecked"},
                     [&](QTextStream &out, int &rows) {
        for (const fusion::Summary &f : index) {
            if (f.records <= 0)
                continue;
            const std::optional<fusion::Record> r = fusion::load(dir, f.country, f.indicator);
            if (!r)
                continue;
            for (const fusion::Golden &g : r->golden) {
                if (g.sources.size() < 2)
                    continue;
                for (const fusion::GoldenPoint &p : g.points) {
                    const QString source = p.source >= 0 && p.source < g.sources.size() ? g.sources[p.source].label : QString();
                    const double time = p.date.year() + (p.date.dayOfYear() - 1) / double(p.date.daysInYear());
                    out << f.country << ',' << f.indicator << ',' << fusion::freqName(g.freq) << ','
                        << p.date.toString(Qt::ISODate) << ',' << QString::number(time, 'f', 3) << ','
                        << QString::number(p.value, 'g', 10) << ',' << csvCell(source) << ',' << int(p.spliced) << ','
                        << int(p.unverified) << '\n';
                    ++rows;
                }
            }
        }
    });
}

void MlDataExporter::exportAll()
{
    if (isBusy())
        return;
    QDir().mkpath(dir());
    emit log(tr("Writing to %1").arg(dir()));
    const struct { const char *name; int rows; } written[] = {
        {"country_series.csv", writeCountrySeries()}, {"weo.csv", writeWeo()}, {"policy_rates.csv", writeRates()},
        {"fx_daily.csv", writeFx()}, {"cot_legacy.csv", writeCot()}, {"golden_series.csv", writeGolden()}};
    for (const auto &w : written)
        emit log(w.rows < 0 ? tr("%1: couldn't write").arg(w.name)
                 : w.rows == 0 ? tr("%1: no data loaded yet, skipped").arg(w.name)
                               : tr("%1: %n row(s)", nullptr, w.rows).arg(w.name));
    m_symbolsLeft = symbols();
    m_pendingSymbols = m_symbolsLeft.size();
    if (!m_pendingSymbols) {
        emit finished();
        return;
    }
    nextSymbol();
}

void MlDataExporter::nextSymbol()
{
    if (m_symbolsLeft.isEmpty()) {
        m_pendingSymbols = 0;
        emit log(tr("Done."));
        emit finished();
        return;
    }
    const QString symbol = m_symbolsLeft.takeFirst();
    research::Client::instance()->candles(symbol, "10y", "1d", this, [this, symbol](const QVector<research::Bar> &bars, const QString &error) {
        if (!error.isEmpty() || bars.isEmpty()) {
            emit log(tr("%1: %2").arg(symbol, error.isEmpty() ? tr("no prices") : error));
        } else {
            QString name = symbol.toLower();
            name.replace(QRegularExpression("[^a-z0-9]+"), "_");
            const int rows = writeFile("prices_" + name + ".csv", {"date", "open", "high", "low", "close", "volume"},
                                       [&](QTextStream &out, int &n) {
                                           for (const research::Bar &b : bars) {
                                               if (!std::isfinite(b.close))
                                                   continue;
                                               auto num = [](double v) { return std::isfinite(v) ? QString::number(v, 'g', 10) : QString(); };
                                               out << b.time.toLocalTime().date().toString(Qt::ISODate) << ',' << num(b.open) << ',' << num(b.high)
                                                   << ',' << num(b.low) << ',' << num(b.close) << ',' << num(b.volume) << '\n';
                                               ++n;
                                           }
                                       });
            emit log(tr("prices_%1.csv: %n row(s)", nullptr, rows).arg(name));
        }
        --m_pendingSymbols;
        nextSymbol();
    });
}
