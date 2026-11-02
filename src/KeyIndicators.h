#pragma once

#include "DomesticFinance.h"

#include <QDate>

// The latest monthly and quarterly figures of each country's economy (the table Trading Economics shows for a
// country: growth, inflation, unemployment, output, confidence, rates, public finances), from the free official
// sources Trading Economics itself draws on. Four data sets, each a CountrySeriesSet (ISO3 -> series -> time ->
// value, time a fractional year: the middle of the month or quarter), and a façade that picks, for each
// indicator and country, the most direct source available: the national office (BLS for the US), then
// Eurostat, then the OECD, then the IMF.

// OECD Key Economic Indicators (sdmx.oecd.org, no key): about 45 economies (the OECD, the G20 and a few
// others), monthly and quarterly, as compiled from the national offices.
class OecdIndicators : public CountrySeriesSet
{
    Q_OBJECT
public:
    enum Series {
        CpiYoY,               // consumer prices, % over a year
        Unemployment,         // % of the labour force, seasonally adjusted
        IndustryYoY,          // industrial production (mining, manufacturing, utilities), volume, % over a year
        RetailYoY,            // retail trade volume, % over a year
        BusinessConfidence,   // composite indicator, long-term average = 100
        ConsumerConfidence,   // composite indicator, long-term average = 100
        LeadingIndicator,     // composite leading indicator, amplitude adjusted, 100 = trend
        ShortRate,            // 3-month interbank rate, % a year
        LongRate,             // 10-year government bond yield, % a year
        WagesYoY,             // hourly earnings in manufacturing, % over a year
        GdpQoQ,               // GDP volume, % on the previous quarter
        GdpYoY,               // GDP volume, % over a year
        CurrentAccountGdp,    // % of GDP, quarterly
        SeriesCount
    };
    explicit OecdIndicators(QObject *parent = nullptr);

protected:
    QList<QUrl> urls() const override;
    void prepare(QNetworkRequest &request) const override;
    QByteArray joinParts(const QList<QByteArray> &parts) const override;
};

// Eurostat (ec.europa.eu/eurostat, no key): the EU, EFTA and candidate countries, from the national offices
// on harmonised definitions (HICP, Labour Force Survey, ESA 2010).
class EurostatIndicators : public CountrySeriesSet
{
    Q_OBJECT
public:
    enum Series {
        HicpYoY,              // harmonised consumer prices, % over a year
        Unemployment,         // % of the labour force, seasonally adjusted
        GdpQoQ,               // GDP volume, % on the previous quarter, seasonally and calendar adjusted
        IndustryYoY,          // industrial production, % over a year, calendar adjusted
        RetailYoY,            // retail trade volume, % over a year, calendar adjusted
        ConsumerConfidence,   // consumer confidence indicator, balance of answers
        PpiYoY,               // producer prices in industry, domestic market, % over a year
        GovDebtGdp,           // general government gross debt, % of GDP, quarterly
        GovBalanceGdp,        // general government net lending (+) / borrowing (−), % of GDP, quarterly
        SeriesCount
    };
    explicit EurostatIndicators(QObject *parent = nullptr);

protected:
    QList<QUrl> urls() const override;
    QByteArray joinParts(const QList<QByteArray> &parts) const override;
};

// IMF consumer price index database (api.imf.org, no key): about 190 countries, monthly, as reported by
// the national offices.
class ImfConsumerPrices : public CountrySeriesSet
{
    Q_OBJECT
public:
    enum Series {
        CpiYoY,   // all items, % over a year
        CpiMoM,   // all items, % on the previous month
        SeriesCount
    };
    explicit ImfConsumerPrices(QObject *parent = nullptr);

protected:
    QList<QUrl> urls() const override;
    QByteArray joinParts(const QList<QByteArray> &parts) const override;
};

// US Bureau of Labor Statistics (api.bls.gov, public API v1: no key, 25 requests a day): the figures of the
// first Friday and the CPI release, straight from the source. One request per series, five series.
class BlsIndicators : public CountrySeriesSet
{
    Q_OBJECT
public:
    enum Series {
        CpiYoY,           // CPI-U, all items, not seasonally adjusted, % over a year (from the index)
        CoreCpiYoY,       // CPI-U less food and energy, % over a year
        Unemployment,     // U-3 rate, seasonally adjusted, %
        PayrollsChange,   // total nonfarm payrolls, change on the month, thousands
        EarningsYoY,      // average hourly earnings, private sector, % over a year
        SeriesCount
    };
    explicit BlsIndicators(QObject *parent = nullptr);

protected:
    QList<QUrl> urls() const override;
    QByteArray joinParts(const QList<QByteArray> &parts) const override;
};

class BrazilIndicators;
class ArgentinaIndicators;
class SouthAfricaIndicators;
class SingaporeIndicators;
class FredIndicators;
class EiaIndicators;
class BeaIndicators;
class BanxicoIndicators;
class EcosIndicators;
class EvdsIndicators;
class SurveyHeadlines;

// The headline indicators of a country, each from the most direct free source that covers it: the national
// office or central bank read directly (BLS, IBGE, INDEC, SARB, SingStat, and with a key Banxico, the Bank of
// Korea, the Turkish central bank), then Eurostat, the OECD and the IMF; FRED, the EIA and the BEA (keys) for the
// United States; the private surveys' headlines from the economic calendar.
class KeyIndicators : public QObject
{
    Q_OBJECT
public:
    enum Id {
        GdpQoQ, GdpYoY, Inflation, CoreInflation, Unemployment, Payrolls, IndustrialProduction, RetailSales,
        BusinessConfidence, ConsumerConfidence, LeadingIndicator, ProducerPrices, Wages, ShortRate, LongRate,
        CurrentAccount, GovDebt, GovBalance,
        // National and keyed sources
        GdpAnnualized, PceInflation, EconomicActivity, HousingStarts, InitialClaims, TradeBalanceUsd,
        CrudeStocks, CrudeProduction, GasolinePrice,
        // Private surveys
        ManufacturingPmi, ServicesPmi, CompositePmi, BusinessClimate, EconomicSentiment, ConsumerSentiment,
        PrivatePayrolls,
        Count
    };
    enum class Unit { Percent, PercentOfGdp, Points, Thousands, BillionDollars, MillionBarrels, ThousandBpd, Dollars };
    struct Sets {
        OecdIndicators *oecd = nullptr;
        EurostatIndicators *eurostat = nullptr;
        ImfConsumerPrices *imf = nullptr;
        BlsIndicators *bls = nullptr;
        BrazilIndicators *brazil = nullptr;
        ArgentinaIndicators *argentina = nullptr;
        SouthAfricaIndicators *southAfrica = nullptr;
        SingaporeIndicators *singapore = nullptr;
        FredIndicators *fred = nullptr;
        EiaIndicators *eia = nullptr;
        BeaIndicators *bea = nullptr;
        BanxicoIndicators *banxico = nullptr;
        EcosIndicators *ecos = nullptr;
        EvdsIndicators *evds = nullptr;
        SurveyHeadlines *surveys = nullptr;
    };
    struct Info {
        Id id;
        QString name;
        QString teSlug;      // the indicator's name on tradingeconomics.com (the catalogue's key)
        Unit unit;
        int decimals;
        bool diverging;      // a sign worth showing (changes, balances)
    };
    struct Figure {
        double value = std::nan("");
        double previous = std::nan("");
        double time = 0;         // fractional year of the latest period
        bool quarterly = false;
        QString via;             // the data set it came from: "Eurostat", "OECD", "IMF", "BLS"
        bool valid() const { return std::isfinite(value); }
        // "Sep 2026" / "Q3 2026".
        QString period() const;
    };

    explicit KeyIndicators(const Sets &sets, QObject *parent = nullptr);

    static Info info(Id id);
    Figure figure(const QString &iso3, Id id) const;
    // The latest value of every country with one.
    QHash<QString, double> values(Id id) const;
    QString format(Id id, double value) const;
    // Any of the four data sets has data (the others may still be downloading).
    bool hasData() const;
    // Error of the data sets, or empty while one is still loading or has data.
    QString status() const;

    const Sets &sets() const { return m_sets; }

signals:
    void updated();

private:
    struct Candidate {
        const CountrySeriesSet *set;
        int series;
        bool quarterly;
        QString via;
        bool survey = false;   // the series comes from SurveyHeadlines: its publisher is the source
    };
    QVector<Candidate> candidates(Id id) const;
    QMap<double, double> seriesOf(const Candidate &c, const QString &iso3) const;
    QString viaOf(const Candidate &c, const QString &iso3) const;

    Sets m_sets;
};
