#include "KeyIndicators.h"

#include "KeyedSources.h"
#include "NationalSources.h"
#include "SurveyHeadlines.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QNetworkRequest>

#include <cmath>

namespace {

const qint64 TwiceADay = 12 * 3600;

// The first year asked of the sources: enough for a figure, its previous one and a year-on-year change.
QString sinceYear()
{
    return QString::number(QDate::currentDate().year() - 2);
}

// Eurostat's country codes are ISO 3166 alpha-2 but for Greece and the United Kingdom.
QString eurostatIso3(const QString &geo)
{
    if (geo == "EL") return "GRC";
    if (geo == "UK") return "GBR";
    return CountrySeriesSet::iso3For(geo);
}

// A JSON-stat data set (Eurostat): calls `f` with the geo code, the time period and the value of every cell.
void forEachJsonStatValue(const QByteArray &body,
                          const std::function<void(const QString &geo, const QByteArray &period, double value)> &f)
{
    const QJsonObject doc = QJsonDocument::fromJson(body).object();
    const QJsonArray ids = doc.value("id").toArray();
    const QJsonArray sizes = doc.value("size").toArray();
    int geoDim = -1, timeDim = -1;
    for (int i = 0; i < ids.size(); ++i) {
        if (ids[i].toString() == "geo") geoDim = i;
        if (ids[i].toString() == "time") timeDim = i;
    }
    if (geoDim < 0 || timeDim < 0 || sizes.size() != ids.size())
        return;
    // The codes of a dimension by position: "index" is a code -> position object (or a list of codes).
    auto codes = [&](const QString &dim) {
        const QJsonValue index = doc.value("dimension").toObject().value(dim).toObject().value("category").toObject().value("index");
        QStringList out;
        if (index.isArray()) {
            for (const QJsonValue &v : index.toArray())
                out << v.toString();
        } else {
            const QJsonObject o = index.toObject();
            out = QStringList(o.size(), QString());
            for (auto it = o.begin(); it != o.end(); ++it)
                if (const int pos = it.value().toInt(); pos >= 0 && pos < out.size())
                    out[pos] = it.key();
        }
        return out;
    };
    const QStringList geos = codes("geo");
    const QStringList times = codes("time");
    QVector<qint64> stride(ids.size(), 1);
    for (int i = ids.size() - 2; i >= 0; --i)
        stride[i] = stride[i + 1] * sizes[i + 1].toInt();
    const QJsonObject values = doc.value("value").toObject();
    for (auto it = values.begin(); it != values.end(); ++it) {
        bool ok = false;
        const qint64 n = it.key().toLongLong(&ok);
        if (!ok || !it.value().isDouble())
            continue;
        const int g = int((n / stride[geoDim]) % sizes[geoDim].toInt());
        const int t = int((n / stride[timeDim]) % sizes[timeDim].toInt());
        f(geos.value(g), times.value(t).toLatin1(), it.value().toDouble());
    }
}

// Series of an SDMX-JSON message (IMF): calls `f` with the country, the period and the value of every
// observation of a series whose dimension `dimId` has the value `dimValue`.
void forEachSdmxJsonValue(const QByteArray &body, const QString &dimId, const QString &dimValue,
                          const std::function<void(const QString &country, const QByteArray &period, double value)> &f)
{
    const QJsonObject data = QJsonDocument::fromJson(body).object().value("data").toObject();
    const QJsonObject dimensions = data.value("structures").toArray().at(0).toObject().value("dimensions").toObject();
    const QJsonArray seriesDims = dimensions.value("series").toArray();
    auto ids = [](const QJsonValue &dim) {
        QStringList list;
        for (const QJsonValue &v : dim.toObject().value("values").toArray())
            list << v.toObject().value("id").toString(v.toObject().value("value").toString());
        return list;
    };
    int countryDim = -1, wantedDim = -1;
    for (int i = 0; i < seriesDims.size(); ++i) {
        const QString id = seriesDims[i].toObject().value("id").toString();
        if (id == "COUNTRY") countryDim = i;
        if (id == dimId) wantedDim = i;
    }
    if (countryDim < 0 || wantedDim < 0)
        return;
    const QStringList countries = ids(seriesDims[countryDim]);
    const QStringList wanted = ids(seriesDims[wantedDim]);
    const QStringList periods = ids(dimensions.value("observation").toArray().at(0));
    const QJsonObject series = data.value("dataSets").toArray().at(0).toObject().value("series").toObject();
    for (auto it = series.begin(); it != series.end(); ++it) {
        const QStringList key = it.key().split(':');
        if (wanted.value(key.value(wantedDim).toInt()) != dimValue)
            continue;
        const QString country = countries.value(key.value(countryDim).toInt());
        const QJsonObject obs = it.value().toObject().value("observations").toObject();
        for (auto o = obs.begin(); o != obs.end(); ++o) {
            bool ok = false;
            const double v = o.value().toArray().at(0).toString().toDouble(&ok);
            if (ok)
                f(country, periods.value(o.key().toInt()).toLatin1(), v);
        }
    }
}

} // namespace

// ---------------------------------------------------------------- OecdIndicators

OecdIndicators::OecdIndicators(QObject *parent)
    : CountrySeriesSet("oecd_key_indicators", TwiceADay, SeriesCount, parent)
{
    loadCached();
}

QList<QUrl> OecdIndicators::urls() const
{
    // Key Economic Indicators (KEI). Key: REF_AREA.FREQ.MEASURE.UNIT_MEASURE.ACTIVITY.ADJUSTMENT.TRANSFORMATION;
    // the growth rates over a year (GY) and the levels (_Z) only, for every area (~1.8 MB monthly, ~250 KB quarterly).
    const QString base = "https://sdmx.oecd.org/public/rest/data/OECD.SDD.STES,DSD_KEI@DF_KEI,4.0/";
    const QString tail = "&dimensionAtObservation=AllDimensions&format=csvfile";
    return {QUrl(base + ".M.CP+UNEMP+PRVM+TOVM+BCICP+CCICP+LI+IR3TIB+IRLT+H_EARN....GY+_Z?startPeriod=" + sinceYear()
                 + "-01" + tail),
            QUrl(base + ".Q.B1GQ_Q+CA_GDP....G1+GY+_Z?startPeriod=" + sinceYear() + "-Q1" + tail)};
}

void OecdIndicators::prepare(QNetworkRequest &request) const
{
    // As for the house prices: the OECD server fails now and then on Qt's default "Accept-Language: en-US,*".
    request.setRawHeader("Accept-Language", "en");
}

QByteArray OecdIndicators::joinParts(const QList<QByteArray> &parts) const
{
    QByteArray out = header();
    for (const QByteArray &part : parts) {
        forEachCsvRow(part, [&](const auto &field) {
            const QByteArray measure = field("MEASURE"), unit = field("UNIT_MEASURE"), activity = field("ACTIVITY"),
                             transformation = field("TRANSFORMATION");
            int k = -1;
            if (measure == "CP" && unit == "GR" && transformation == "GY") k = CpiYoY;
            else if (measure == "UNEMP" && unit == "PT_LF") k = Unemployment;
            else if (measure == "PRVM" && unit == "GR" && activity == "BTE" && transformation == "GY") k = IndustryYoY;
            else if (measure == "TOVM" && unit == "GR" && transformation == "GY") k = RetailYoY;
            else if (measure == "BCICP") k = BusinessConfidence;
            else if (measure == "CCICP") k = ConsumerConfidence;
            else if (measure == "LI" && unit == "IX") k = LeadingIndicator;
            else if (measure == "IR3TIB") k = ShortRate;
            else if (measure == "IRLT") k = LongRate;
            else if (measure == "H_EARN" && unit == "GR" && transformation == "GY") k = WagesYoY;
            else if (measure == "B1GQ_Q" && unit == "GR" && transformation == "G1") k = GdpQoQ;
            else if (measure == "B1GQ_Q" && unit == "GR" && transformation == "GY") k = GdpYoY;
            else if (measure == "CA_GDP") k = CurrentAccountGdp;
            if (k >= 0 && !field("OBS_VALUE").isEmpty())
                addLine(out, QString::fromLatin1(field("REF_AREA")), k, periodTime(field("TIME_PERIOD")),
                        field("OBS_VALUE").toDouble());
        });
    }
    return out;
}

// ---------------------------------------------------------------- EurostatIndicators

EurostatIndicators::EurostatIndicators(QObject *parent)
    : CountrySeriesSet("eurostat_kei", TwiceADay, SeriesCount, parent)
{
    loadCached();
}

QList<QUrl> EurostatIndicators::urls() const
{
    // The dissemination API's JSON-stat, one data set per series (~10 KB each), in the order of Series.
    const QString base = "https://ec.europa.eu/eurostat/api/dissemination/statistics/1.0/data/";
    const QString month = "&sinceTimePeriod=" + sinceYear() + "-01&format=JSON&lang=EN";
    const QString quarter = "&sinceTimePeriod=" + sinceYear() + "-Q1&format=JSON&lang=EN";
    // HICP: the ECOICOP ver. 2 data set (prc_hicp_minr) took over from prc_hicp_manr, frozen at 2025, in 2026.
    return {QUrl(base + "prc_hicp_minr?coicop18=TOTAL&unit=RCH_A" + month),
            QUrl(base + "une_rt_m?s_adj=SA&age=TOTAL&unit=PC_ACT&sex=T" + month),
            QUrl(base + "namq_10_gdp?unit=CLV_PCH_PRE&s_adj=SCA&na_item=B1GQ" + quarter),
            QUrl(base + "sts_inpr_m?nace_r2=B-D&s_adj=CA&unit=PCH_SM" + month),
            QUrl(base + "sts_trtu_m?nace_r2=G47&indic_bt=VOL_SLS&s_adj=CA&unit=PCH_SM" + month),
            QUrl(base + "ei_bssi_m_r2?indic=BS-CSMCI-BAL&s_adj=SA" + month),
            QUrl(base + "sts_inppd_m?nace_r2=B-E36&s_adj=NSA&unit=PCH_SM" + month),
            QUrl(base + "gov_10q_ggdebt?unit=PC_GDP&sector=S13&na_item=GD" + quarter),
            QUrl(base + "gov_10q_ggnfa?unit=PC_GDP&sector=S13&na_item=B9&s_adj=NSA" + quarter)};
}

QByteArray EurostatIndicators::joinParts(const QList<QByteArray> &parts) const
{
    QByteArray out = header();
    for (int k = 0; k < parts.size() && k < SeriesCount; ++k)
        forEachJsonStatValue(parts[k], [&](const QString &geo, const QByteArray &period, double v) {
            addLine(out, eurostatIso3(geo), k, periodTime(period), v);
        });
    return out;
}

// ---------------------------------------------------------------- ImfConsumerPrices

ImfConsumerPrices::ImfConsumerPrices(QObject *parent)
    : CountrySeriesSet("imf_consumer_prices", TwiceADay, SeriesCount, parent)
{
    loadCached();
}

QList<QUrl> ImfConsumerPrices::urls() const
{
    // All items, period average, % over a year and over a month, monthly, every country. The service ignores
    // startPeriod but honours the SDMX 3.0 component filter (~300 KB; the whole history would be 3.7 MB).
    return {QUrl("https://api.imf.org/external/sdmx/3.0/data/dataflow/IMF.STA/CPI/+/"
                 "*.CPI._T.YOY_PCH_PA_PT+POP_PCH_PA_PT.M?c%5BTIME_PERIOD%5D=ge:" + sinceYear()
                 + "-M01&attributes=none&measures=all")};
}

QByteArray ImfConsumerPrices::joinParts(const QList<QByteArray> &parts) const
{
    QByteArray out = header();
    const struct { const char *code; int k; } wanted[] = {{"YOY_PCH_PA_PT", CpiYoY}, {"POP_PCH_PA_PT", CpiMoM}};
    for (const auto &w : wanted)
        forEachSdmxJsonValue(parts.value(0), "TYPE_OF_TRANSFORMATION", w.code,
                             [&](const QString &country, const QByteArray &period, double v) {
                                 addLine(out, country == "KOS" ? QString("XKX") : country, w.k, periodTime(period), v);
                             });
    return out;
}

// ---------------------------------------------------------------- BlsIndicators

BlsIndicators::BlsIndicators(QObject *parent)
    : CountrySeriesSet("bls_key_indicators", 24 * 3600, SeriesCount, parent)
{
    loadCached();
}

QList<QUrl> BlsIndicators::urls() const
{
    // Public API v1: one series per request, the last three years, no key (25 requests a day per address).
    const QString base = "https://api.bls.gov/publicAPI/v1/timeseries/data/";
    return {QUrl(base + "CUUR0000SA0"),       // CPI-U all items, NSA, index
            QUrl(base + "CUUR0000SA0L1E"),    // CPI-U less food and energy, NSA, index
            QUrl(base + "LNS14000000"),       // unemployment rate, SA
            QUrl(base + "CES0000000001"),     // total nonfarm employment, SA, thousands
            QUrl(base + "CES0500000003")};    // average hourly earnings, private, SA, dollars
}

QByteArray BlsIndicators::joinParts(const QList<QByteArray> &parts) const
{
    QByteArray out = header();
    for (int k = 0; k < parts.size() && k < SeriesCount; ++k) {
        // month (as a time) -> value, oldest first.
        QMap<double, double> level;
        const QJsonArray series = QJsonDocument::fromJson(parts[k]).object().value("Results").toObject()
                                      .value("series").toArray();
        for (const QJsonValue &point : series.at(0).toObject().value("data").toArray()) {
            const QJsonObject p = point.toObject();
            const QByteArray period = p.value("period").toString().toLatin1();   // "M09"; "M13" is the year's average
            const int month = period.startsWith('M') ? period.mid(1).toInt() : 0;
            bool ok = false;
            const double v = p.value("value").toString().toDouble(&ok);
            if (ok && month >= 1 && month <= 12)
                level.insert(p.value("year").toString().toInt() + (month - 0.5) / 12.0, v);
        }
        for (auto it = level.begin(); it != level.end(); ++it) {
            double v = std::nan("");
            switch (k) {
            case CpiYoY:
            case CoreCpiYoY:
            case EarningsYoY: {
                // Against the same month a year before.
                const auto prev = level.constFind(it.key() - 1);
                if (prev != level.constEnd() && prev.value() > 0)
                    v = 100 * (it.value() / prev.value() - 1);
                else {
                    for (auto p = level.constBegin(); p != level.constEnd(); ++p)
                        if (std::abs(p.key() - (it.key() - 1)) < 1e-6 && p.value() > 0)
                            v = 100 * (it.value() / p.value() - 1);
                }
                break;
            }
            case PayrollsChange:
                if (it != level.begin() && std::abs(std::prev(it).key() - (it.key() - 1.0 / 12)) < 1e-6)
                    v = it.value() - std::prev(it).value();
                break;
            default:
                v = it.value();
            }
            addLine(out, "USA", k, it.key(), v);
        }
    }
    return out;
}

// ---------------------------------------------------------------- KeyIndicators

KeyIndicators::KeyIndicators(const Sets &sets, QObject *parent)
    : QObject(parent)
    , m_sets(sets)
{
    for (RemoteDataSet *d : std::initializer_list<RemoteDataSet *> {
             sets.oecd, sets.eurostat, sets.imf, sets.bls, sets.brazil, sets.argentina, sets.southAfrica, sets.singapore,
             sets.fred, sets.eia, sets.bea, sets.banxico, sets.ecos, sets.evds, sets.surveys})
        if (d)
            connect(d, &RemoteDataSet::updated, this, &KeyIndicators::updated);
}

KeyIndicators::Info KeyIndicators::info(Id id)
{
    switch (id) {
    case GdpQoQ: return {id, tr("GDP growth"), "gdp-growth", Unit::Percent, 1, true};
    case GdpYoY: return {id, tr("GDP growth over a year"), "gdp-growth-annual", Unit::Percent, 1, true};
    case GdpAnnualized: return {id, tr("GDP growth, annualised"), "gdp-growth", Unit::Percent, 1, true};
    case Inflation: return {id, tr("Inflation"), "inflation-cpi", Unit::Percent, 1, false};
    case CoreInflation: return {id, tr("Core inflation"), "core-inflation-rate", Unit::Percent, 1, false};
    case PceInflation: return {id, tr("PCE inflation"), "pce-price-index-annual-change", Unit::Percent, 1, false};
    case Unemployment: return {id, tr("Unemployment rate"), "unemployment-rate", Unit::Percent, 1, false};
    case Payrolls: return {id, tr("Non-farm payrolls"), "non-farm-payrolls", Unit::Thousands, 0, true};
    case PrivatePayrolls: return {id, tr("Private payrolls (ADP)"), "adp-employment-change", Unit::Thousands, 0, true};
    case InitialClaims: return {id, tr("Initial jobless claims"), "initial-jobless-claims", Unit::Thousands, 0, false};
    case IndustrialProduction: return {id, tr("Industrial production"), "industrial-production", Unit::Percent, 1, true};
    case EconomicActivity: return {id, tr("Economic activity"), "economic-activity-index", Unit::Percent, 1, true};
    case RetailSales: return {id, tr("Retail sales"), "retail-sales-annual", Unit::Percent, 1, true};
    case HousingStarts: return {id, tr("Housing starts"), "housing-starts", Unit::Thousands, 0, false};
    case BusinessConfidence: return {id, tr("Business confidence"), "business-confidence", Unit::Points, 1, false};
    case ConsumerConfidence: return {id, tr("Consumer confidence"), "consumer-confidence", Unit::Points, 1, false};
    case LeadingIndicator: return {id, tr("Leading indicator"), "leading-economic-index", Unit::Points, 1, false};
    case ManufacturingPmi: return {id, tr("Manufacturing PMI"), "manufacturing-pmi", Unit::Points, 1, false};
    case ServicesPmi: return {id, tr("Services PMI"), "services-pmi", Unit::Points, 1, false};
    case CompositePmi: return {id, tr("Composite PMI"), "composite-pmi", Unit::Points, 1, false};
    case BusinessClimate: return {id, tr("Business climate"), "business-confidence", Unit::Points, 1, false};
    case EconomicSentiment: return {id, tr("Economic sentiment"), "zew-economic-sentiment-index", Unit::Points, 1, true};
    case ConsumerSentiment: return {id, tr("Consumer sentiment"), "consumer-confidence", Unit::Points, 1, false};
    case ProducerPrices: return {id, tr("Producer prices"), "producer-prices-change", Unit::Percent, 1, true};
    case Wages: return {id, tr("Wage growth"), "wage-growth", Unit::Percent, 1, true};
    case ShortRate: return {id, tr("3-month interbank rate"), "interbank-rate", Unit::Percent, 2, false};
    case LongRate: return {id, tr("10-year bond yield"), "government-bond-yield", Unit::Percent, 2, false};
    case CurrentAccount: return {id, tr("Current account"), "current-account-to-gdp", Unit::PercentOfGdp, 1, true};
    case TradeBalanceUsd: return {id, tr("Trade balance"), "balance-of-trade", Unit::BillionDollars, 1, true};
    case GovDebt: return {id, tr("Government debt"), "government-debt-to-gdp", Unit::PercentOfGdp, 1, false};
    case GovBalance: return {id, tr("Budget balance"), "government-budget", Unit::PercentOfGdp, 1, true};
    case CrudeStocks: return {id, tr("Crude oil stocks"), "crude-oil-stocks-change", Unit::MillionBarrels, 1, false};
    case CrudeProduction: return {id, tr("Crude oil production"), "crude-oil-production", Unit::ThousandBpd, 0, false};
    case GasolinePrice: return {id, tr("Gasoline price"), "gasoline-prices", Unit::Dollars, 2, false};
    case Count: break;
    }
    return {};
}

QVector<KeyIndicators::Candidate> KeyIndicators::candidates(Id id) const
{
    // The most direct source first (the national office, then the compilations); a country gets the first one
    // that covers it unless a later one is clearly fresher. Null sets are skipped.
    const Sets &s = m_sets;
    using O = OecdIndicators;
    using E = EurostatIndicators;
    using I = ImfConsumerPrices;
    using B = BlsIndicators;
    using BR = BrazilIndicators;
    using AR = ArgentinaIndicators;
    using ZA = SouthAfricaIndicators;
    using SG = SingaporeIndicators;
    using F = FredIndicators;
    using EI = EiaIndicators;
    using BX = BanxicoIndicators;
    using SV = SurveyHeadlines;
    QVector<Candidate> out;
    auto add = [&](const CountrySeriesSet *set, int series, bool quarterly, const char *via) {
        if (set) out.push_back({set, series, quarterly, QString::fromLatin1(via), false});
    };
    auto survey = [&](int series) {
        if (s.surveys) out.push_back({s.surveys, series, false, QString(), true});
    };
    const char *bls = "BLS", *eu = "Eurostat", *oecd = "OECD", *imf = "IMF", *fred = "FRED", *eia = "EIA", *bea = "BEA",
               *ibge = "IBGE", *bcb = "Banco Central do Brasil", *indec = "INDEC", *sarb = "SARB", *singstat = "SingStat",
               *banxico = "Banxico", *bok = "Bank of Korea", *cbrt = "CBRT";
    switch (id) {
    case GdpQoQ: add(s.eurostat, E::GdpQoQ, true, eu); add(s.oecd, O::GdpQoQ, true, oecd); break;
    case GdpYoY: add(s.oecd, O::GdpYoY, true, oecd); break;
    case GdpAnnualized: add(s.bea, BeaIndicators::GdpAnnualized, true, bea); break;
    case Inflation:
        add(s.bls, B::CpiYoY, false, bls); add(s.brazil, BR::CpiYoY, false, bcb); add(s.argentina, AR::CpiYoY, false, indec);
        add(s.southAfrica, ZA::CpiYoY, false, sarb); add(s.singapore, SG::CpiYoY, false, singstat);
        add(s.banxico, BX::CpiYoY, false, banxico); add(s.ecos, EcosIndicators::CpiYoY, false, bok);
        add(s.evds, EvdsIndicators::CpiYoY, false, cbrt);
        add(s.eurostat, E::HicpYoY, false, eu); add(s.oecd, O::CpiYoY, false, oecd); add(s.imf, I::CpiYoY, false, imf);
        break;
    case CoreInflation: add(s.bls, B::CoreCpiYoY, false, bls); break;
    case PceInflation: add(s.fred, F::PceInflationYoY, false, fred); break;
    case Unemployment:
        add(s.bls, B::Unemployment, false, bls); add(s.brazil, BR::Unemployment, false, ibge);
        add(s.argentina, AR::Unemployment, true, indec); add(s.eurostat, E::Unemployment, false, eu);
        add(s.oecd, O::Unemployment, false, oecd);
        break;
    case Payrolls: add(s.bls, B::PayrollsChange, false, bls); break;
    case PrivatePayrolls: survey(SV::PrivatePayrolls); break;
    case InitialClaims: add(s.fred, F::InitialClaims, false, fred); break;
    case IndustrialProduction:
        add(s.brazil, BR::IndustryYoY, false, ibge); add(s.eurostat, E::IndustryYoY, false, eu); add(s.oecd, O::IndustryYoY, false, oecd);
        break;
    case EconomicActivity: add(s.argentina, AR::ActivityYoY, false, indec); break;
    case RetailSales:
        add(s.brazil, BR::RetailYoY, false, ibge); add(s.eurostat, E::RetailYoY, false, eu); add(s.oecd, O::RetailYoY, false, oecd);
        break;
    case HousingStarts: add(s.fred, F::HousingStarts, false, fred); break;
    case BusinessConfidence: add(s.oecd, O::BusinessConfidence, false, oecd); break;
    case ConsumerConfidence: add(s.eurostat, E::ConsumerConfidence, false, eu); add(s.oecd, O::ConsumerConfidence, false, oecd); break;
    case LeadingIndicator: add(s.oecd, O::LeadingIndicator, false, oecd); break;
    case ManufacturingPmi: survey(SV::ManufacturingPmi); break;
    case ServicesPmi: survey(SV::ServicesPmi); break;
    case CompositePmi: survey(SV::CompositePmi); break;
    case BusinessClimate: survey(SV::BusinessClimate); break;
    case EconomicSentiment: survey(SV::EconomicSentiment); break;
    case ConsumerSentiment: survey(SV::ConsumerSentiment); add(s.fred, F::ConsumerSentiment, false, fred); break;
    case ProducerPrices: add(s.southAfrica, ZA::PpiYoY, false, sarb); add(s.eurostat, E::PpiYoY, false, eu); break;
    case Wages: add(s.bls, B::EarningsYoY, false, bls); add(s.oecd, O::WagesYoY, false, oecd); break;
    case ShortRate: add(s.oecd, O::ShortRate, false, oecd); break;
    case LongRate: add(s.oecd, O::LongRate, false, oecd); break;
    case CurrentAccount: add(s.oecd, O::CurrentAccountGdp, true, oecd); break;
    case TradeBalanceUsd: add(s.fred, F::TradeBalance, false, fred); break;
    case GovDebt: add(s.eurostat, E::GovDebtGdp, true, eu); break;
    case GovBalance: add(s.eurostat, E::GovBalanceGdp, true, eu); break;
    case CrudeStocks: add(s.eia, EI::CrudeStocks, false, eia); break;
    case CrudeProduction: add(s.eia, EI::CrudeProduction, false, eia); break;
    case GasolinePrice: add(s.eia, EI::GasolinePrice, false, eia); break;
    case Count: break;
    }
    return out;
}

QMap<double, double> KeyIndicators::seriesOf(const Candidate &c, const QString &iso3) const
{
    return c.survey ? m_sets.surveys->enabledSeries(iso3, c.series) : c.set->series(iso3, c.series);
}

QString KeyIndicators::viaOf(const Candidate &c, const QString &iso3) const
{
    if (!c.survey)
        return c.via;
    const QString id = m_sets.surveys->publisherOf(iso3, c.series);
    for (const SurveyHeadlines::Publisher &p : SurveyHeadlines::publishers())
        if (p.id == id)
            return p.name;
    return id;
}

namespace {

// A later compilation of the same figures wins only when it is clearly more recent (a quarter or more): the
// sources publish on their own calendars, and some data sets freeze (a discontinued series) or lag.
const double Fresher = 0.2;

} // namespace

KeyIndicators::Figure KeyIndicators::figure(const QString &iso3, Id id) const
{
    Figure f;
    for (const Candidate &c : candidates(id)) {
        const QMap<double, double> s = seriesOf(c, iso3);
        if (s.isEmpty() || (f.valid() && s.lastKey() < f.time + Fresher))
            continue;
        f.value = s.last();
        f.time = s.lastKey();
        f.previous = s.size() > 1 ? std::prev(s.end(), 2).value() : std::nan("");
        f.quarterly = c.quarterly;
        f.via = viaOf(c, iso3);
    }
    return f;
}

QHash<QString, double> KeyIndicators::values(Id id) const
{
    QHash<QString, double> out;
    QHash<QString, double> when;
    for (const Candidate &c : candidates(id))
        for (const QString &iso3 : c.set->countries()) {
            const QMap<double, double> s = seriesOf(c, iso3);
            if (s.isEmpty())
                continue;
            const double time = s.lastKey(), v = s.last();
            const auto it = when.constFind(iso3);
            if (it != when.constEnd() && time < it.value() + Fresher)
                continue;
            out.insert(iso3, v);
            when.insert(iso3, time);
        }
    return out;
}

QString KeyIndicators::format(Id id, double v) const
{
    if (!std::isfinite(v))
        return QString::fromUtf8("—");
    const Info i = info(id);
    const QLocale loc;
    const QString sign = v < 0 ? QString::fromUtf8("−") : i.diverging && v > 0 ? "+" : "";
    const QString num = sign + loc.toString(std::abs(v), 'f', i.decimals);
    switch (i.unit) {
    case Unit::Percent:
    case Unit::PercentOfGdp: return tr("%1%").arg(num);
    case Unit::Thousands: return tr("%1k").arg(num);
    case Unit::Points: return num;
    case Unit::BillionDollars: return tr("$%1 bn").arg(num);
    case Unit::MillionBarrels: return tr("%1 mb").arg(num);
    case Unit::ThousandBpd: return tr("%1 kb/d").arg(num);
    case Unit::Dollars: return tr("$%1").arg(num);
    }
    return num;
}

QString KeyIndicators::Figure::period() const
{
    if (!valid() || time <= 0)
        return QString();
    const int year = int(std::floor(time + 1e-6));
    const double frac = time - year;
    if (quarterly)
        return tr("Q%1 %2").arg(int(std::lround(frac * 4 + 0.5))).arg(year);
    const int month = std::clamp(int(std::lround(frac * 12 + 0.5)), 1, 12);
    return QLocale().toString(QDate(year, month, 1), "MMM yyyy");
}

bool KeyIndicators::hasData() const
{
    for (const CountrySeriesSet *d : std::initializer_list<const CountrySeriesSet *> {m_sets.oecd, m_sets.eurostat, m_sets.imf, m_sets.bls})
        if (d && d->hasData())
            return true;
    return false;
}

QString KeyIndicators::status() const
{
    for (const CountrySeriesSet *d : std::initializer_list<const CountrySeriesSet *> {m_sets.oecd, m_sets.eurostat, m_sets.imf, m_sets.bls})
        if (d && !d->hasData() && !d->errorString().isEmpty())
            return d->errorString();
    return QString();
}
