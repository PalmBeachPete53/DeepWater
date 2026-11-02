#pragma once

#include <QColor>
#include <QDate>
#include <QHash>
#include <QObject>
#include <QVector>

class BalanceOfPayments;
class BankCredit;
class GlobalDebt;
class HousePriceRatios;
class PropertyCredit;
class EconomySectors;
class KeyIndicators;
class ComtradeProducts;
class WorldEconomy;
class WorldPolicyRates;
namespace supply { class SupplyChain; }

// The macro indicators shown per country: definitions, values from the IMF
// (World Economic Outlook, balance of payments), BIS and World Bank data sets, formatting
// and the color scale used for the globe heatmap.
class MacroIndicators : public QObject
{
    Q_OBJECT
public:
    enum Id {
        GdpGrowth, Inflation, Unemployment, PolicyRate, RealRate,
        CurrentAccount, GovBalance, GovDebt, GdpPerCapita, GdpSize,
        // Capital flows and trade
        NetCapitalInflows, FdiInflows, PortfolioInflows, TradeBalance, TradeOpenness, ExportVolume,
        // Latest monthly figures from the national offices (Eurostat, OECD, IMF, BLS)
        InflationLatest, UnemploymentLatest,
        // Supply chains: crop yields, road freight and crude stocks, as a change over a year
        WheatYield, CornYield, RiceYield, SoyYield, RoadFreight, CrudeStocks,
        Count
    };
    enum class Group {
        Economy,
        FlowsAndTrade,
        Latest,   // monthly figures of the official statistics
        SupplyChain,
    };
    enum class Scale {
        Diverging,    // signed values, centered on zero
        Sequential,   // low to high
        Log,          // sequential on a log scale (sizes, incomes)
    };
    struct Info {
        Id id;
        QString name;
        QString description;
        QString source;
        Scale scale;
        bool yearly;   // IMF annual series; otherwise latest monthly figure
        Group group = Group::Economy;
    };
    // Color range for one indicator and year, from the 5th to the 95th
    // percentile so that a few extreme countries do not wash out the map.
    struct Range {
        double lo = 0;
        double hi = 1;
    };

    MacroIndicators(WorldEconomy *economy, WorldPolicyRates *rates, BalanceOfPayments *bop,
                    EconomySectors *sectors, QObject *parent = nullptr);

    static Info info(Id id);
    // Longer explanation for the help popover next to the indicator's title.
    struct Explanation {
        QString what;      // what it measures
        QString reading;   // how to read it
        QString caveat;    // what to keep in mind
    };
    static Explanation explanation(Id id);
    WorldEconomy *economy() const { return m_economy; }
    WorldPolicyRates *rates() const { return m_rates; }
    BalanceOfPayments *bop() const { return m_bop; }
    EconomySectors *sectors() const { return m_sectors; }
    // Property and debt of the domestic market (BIS, OECD, IMF, World Bank): shown in the country views.
    struct Finance {
        PropertyCredit *bis = nullptr;
        HousePriceRatios *oecd = nullptr;
        GlobalDebt *imf = nullptr;
        BankCredit *worldBank = nullptr;
    };
    void setFinance(const Finance &finance);
    const Finance &finance() const { return m_finance; }
    // The latest monthly and quarterly figures of each country (Eurostat, OECD, IMF, BLS): the two monthly
    // indicators and the country sheet's table.
    void setKeyIndicators(KeyIndicators *key);
    KeyIndicators *keyIndicators() const { return m_key; }
    // Trade by product (UN Comtrade), downloaded when a country sheet asks for it.
    void setComtrade(ComtradeProducts *comtrade) { m_comtrade = comtrade; }
    ComtradeProducts *comtrade() const { return m_comtrade; }
    // Crop yields, road freight and crude stocks (the Supply chains group).
    void setSupplyChain(supply::SupplyChain *supply);
    supply::SupplyChain *supplyChain() const { return m_supply; }
    // What "latest" means for a monthly indicator (the note under its title).
    static QString latestNote(Id id);
    // The label of a year on the slider ("2026 · IMF estimate").
    QString yearNote(Id id, int year) const;
    // More about a country's value, for the globe's tooltip (the level behind a change); empty when none.
    QString detail(Id id, const QString &iso3, int year) const;
    // IMF figures for the current year are estimates, later ones projections.
    static int currentYear() { return QDate::currentDate().year(); }
    static int firstYear() { return 2010; }
    // Year shown by default: the current year, or for balance of payments figures
    // (reported, no projections) the latest year most countries have reported.
    int defaultYear(Id id) const;
    int lastYear(Id id) const;
    bool hasData(Id id) const;   // false while its data set is loading

    // NaN when unavailable. `year` is ignored for monthly indicators.
    double value(Id id, const QString &iso3, int year) const;
    QHash<QString, double> values(Id id, int year) const;   // iso3 -> value, countries with data
    QString format(Id id, double value) const;
    // Policy rate details: month of the latest figure and change over 12 months.
    bool policyRate(const QString &iso3, double *rate, QDate *month, double *change12m) const;

    Range range(Id id, const QHash<QString, double> &values) const;
    QColor color(Id id, double value, const Range &range) const;
    // Position of a value on the scale, from 0 (low end) to 1 (high end), clamped: prism map heights.
    double level(Id id, double value, const Range &range) const;
    // Colors along the scale for a legend, from low to high.
    QVector<QColor> ramp(Id id) const;

signals:
    void updated();

private:
    WorldEconomy *m_economy;
    WorldPolicyRates *m_rates;
    BalanceOfPayments *m_bop;
    EconomySectors *m_sectors;
    Finance m_finance;
    KeyIndicators *m_key = nullptr;
    ComtradeProducts *m_comtrade = nullptr;
    supply::SupplyChain *m_supply = nullptr;
};
