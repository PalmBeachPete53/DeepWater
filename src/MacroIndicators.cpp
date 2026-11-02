#include "MacroIndicators.h"

#include "DomesticFinance.h"
#include "KeyIndicators.h"
#include "MacroData.h"
#include "SupplyChain.h"
#include "Theme.h"

#include <QLocale>

#include <algorithm>
#include <cmath>

namespace {

QColor mix(const QColor &a, const QColor &b, double t)
{
    t = std::clamp(t, 0.0, 1.0);
    return QColor::fromRgbF(float(a.redF() + (b.redF() - a.redF()) * t),
                            float(a.greenF() + (b.greenF() - a.greenF()) * t),
                            float(a.blueF() + (b.blueF() - a.blueF()) * t));
}

double percentile(QVector<double> v, double q)
{
    if (v.isEmpty())
        return 0;
    std::sort(v.begin(), v.end());
    const double pos = q * (v.size() - 1);
    const int i = int(pos);
    return i + 1 < v.size() ? v[i] + (v[i + 1] - v[i]) * (pos - i) : v[i];
}

WorldEconomy::Indicator weoIndicator(MacroIndicators::Id id)
{
    switch (id) {
    case MacroIndicators::GdpGrowth: return WorldEconomy::GdpGrowth;
    case MacroIndicators::Inflation: return WorldEconomy::Inflation;
    case MacroIndicators::Unemployment: return WorldEconomy::Unemployment;
    case MacroIndicators::CurrentAccount: return WorldEconomy::CurrentAccount;
    case MacroIndicators::GovBalance: return WorldEconomy::GovBalance;
    case MacroIndicators::GovDebt: return WorldEconomy::GovDebt;
    case MacroIndicators::GdpPerCapita: return WorldEconomy::GdpPerCapitaPpp;
    case MacroIndicators::GdpSize: return WorldEconomy::GdpUsd;
    case MacroIndicators::ExportVolume: return WorldEconomy::ExportVolume;
    default: return WorldEconomy::IndicatorCount;
    }
}

bool fromBop(MacroIndicators::Id id)
{
    return id >= MacroIndicators::NetCapitalInflows && id <= MacroIndicators::TradeOpenness;
}

// The key indicator behind a monthly heatmap indicator.
KeyIndicators::Id keyIndicator(MacroIndicators::Id id)
{
    return id == MacroIndicators::InflationLatest ? KeyIndicators::Inflation : KeyIndicators::Unemployment;
}

bool fromKey(MacroIndicators::Id id)
{
    return id == MacroIndicators::InflationLatest || id == MacroIndicators::UnemploymentLatest;
}

bool isCrop(MacroIndicators::Id id)
{
    return id >= MacroIndicators::WheatYield && id <= MacroIndicators::SoyYield;
}

supply::Crop cropOf(MacroIndicators::Id id)
{
    return supply::Crop(id - MacroIndicators::WheatYield);
}

} // namespace

MacroIndicators::MacroIndicators(WorldEconomy *economy, WorldPolicyRates *rates, BalanceOfPayments *bop,
                                 EconomySectors *sectors, QObject *parent)
    : QObject(parent)
    , m_economy(economy)
    , m_rates(rates)
    , m_bop(bop)
    , m_sectors(sectors)
{
    connect(m_economy, &RemoteDataSet::updated, this, &MacroIndicators::updated);
    connect(m_rates, &RemoteDataSet::updated, this, &MacroIndicators::updated);
    connect(m_bop, &RemoteDataSet::updated, this, &MacroIndicators::updated);
    connect(m_sectors, &RemoteDataSet::updated, this, &MacroIndicators::updated);
}

void MacroIndicators::setFinance(const Finance &finance)
{
    m_finance = finance;
    for (RemoteDataSet *d : std::initializer_list<RemoteDataSet *> {finance.bis, finance.oecd, finance.imf, finance.worldBank})
        connect(d, &RemoteDataSet::updated, this, &MacroIndicators::updated);
}

void MacroIndicators::setSupplyChain(supply::SupplyChain *supply)
{
    m_supply = supply;
    connect(supply, &supply::SupplyChain::updated, this, &MacroIndicators::updated);
}

void MacroIndicators::setKeyIndicators(KeyIndicators *key)
{
    m_key = key;
    connect(key, &KeyIndicators::updated, this, &MacroIndicators::updated);
}

QString MacroIndicators::latestNote(Id id)
{
    switch (id) {
    case InflationLatest:
        return tr("Consumer prices over the latest month published, against the same month a year before: the "
                  "national offices' figures as compiled by Eurostat, the OECD and the IMF (BLS for the United "
                  "States).");
    case UnemploymentLatest:
        return tr("Latest monthly unemployment rate, seasonally adjusted, as compiled by Eurostat and the OECD "
                  "(BLS for the United States): about 45 economies.");
    case CrudeStocks:
        return tr("Crude oil stocks at the end of the latest month each country reported to JODI, against the same "
                  "month a year before: about 60 countries, one to three months behind.");
    default:
        return tr("Latest month-end figure for each central bank.");
    }
}

MacroIndicators::Info MacroIndicators::info(Id id)
{
    const QString imf = tr("IMF World Economic Outlook");
    const QString bis = tr("Bank for International Settlements");
    const QString bop = tr("IMF balance of payments statistics") + " · " + imf;
    const QString offices = tr("National statistical offices, via Eurostat, OECD, IMF and BLS");
    const Group flows = Group::FlowsAndTrade;
    const Group supplyGroup = Group::SupplyChain;
    const QString crops = tr("USDA Production, Supply and Distribution · Eurostat crop statistics (EU members)");
    auto cropInfo = [&](const QString &name, const QString &crop) {
        return Info {id, name, tr("%1 yield per hectare against the year before.").arg(crop), crops, Scale::Diverging,
                     true, supplyGroup};
    };
    switch (id) {
    case WheatYield:
        return cropInfo(tr("Wheat yield"), tr("Wheat"));
    case CornYield:
        return cropInfo(tr("Corn yield"), tr("Corn"));
    case RiceYield:
        return cropInfo(tr("Rice yield"), tr("Rice"));
    case SoyYield:
        return cropInfo(tr("Soybean yield"), tr("Soybean"));
    case RoadFreight:
        return {id, tr("Road freight"), tr("Goods carried by road in tonne-kilometres, against the year before."),
                tr("International Transport Forum (OECD), annual transport trends"), Scale::Diverging, true, supplyGroup};
    case CrudeStocks:
        return {id, tr("Crude oil stocks"), tr("Crude stocks at the end of the latest month, against a year before."),
                tr("Joint Organisations Data Initiative (JODI-Oil)"), Scale::Diverging, false, supplyGroup};
    case InflationLatest:
        return {id, tr("Inflation, latest month"), tr("Consumer prices against a year before, latest month published."),
                offices, Scale::Sequential, false, Group::Latest};
    case UnemploymentLatest:
        return {id, tr("Unemployment, latest month"), tr("Share of the labour force without a job, latest month published."),
                offices, Scale::Sequential, false, Group::Latest};
    case GdpGrowth:
        return {id, tr("Real GDP growth"), tr("Annual change in output, adjusted for inflation."), imf,
                Scale::Diverging, true};
    case Inflation:
        return {id, tr("Inflation"), tr("Average consumer prices, annual change."), imf, Scale::Sequential, true};
    case Unemployment:
        return {id, tr("Unemployment"), tr("Share of the labour force without a job."), imf, Scale::Sequential, true};
    case PolicyRate:
        return {id, tr("Policy rate"), tr("Central bank policy rate at the end of the latest month."), bis,
                Scale::Sequential, false};
    case RealRate:
        return {id, tr("Real policy rate"),
                tr("Policy rate minus the IMF inflation estimate for this year. Positive means tight money."),
                bis + " · " + imf, Scale::Diverging, false};
    case CurrentAccount:
        return {id, tr("Current account"),
                tr("Balance of trade, income and transfers with the rest of the world, in % of GDP."), imf,
                Scale::Diverging, true};
    case GovBalance:
        return {id, tr("Budget balance"), tr("General government net lending (+) or borrowing (−), in % of GDP."),
                imf, Scale::Diverging, true};
    case GovDebt:
        return {id, tr("Public debt"), tr("General government gross debt, in % of GDP."), imf, Scale::Sequential,
                true};
    case GdpPerCapita:
        return {id, tr("GDP per capita"), tr("At purchasing power parity, in international dollars."), imf,
                Scale::Log, true};
    case GdpSize:
        return {id, tr("GDP"), tr("Size of the economy at current prices, in US dollars."), imf, Scale::Log, true};
    case NetCapitalInflows:
        return {id, tr("Net capital inflows"),
                tr("Foreign investment into the country minus residents' investment abroad (financial account), "
                   "in % of GDP. Positive: the country imports capital, the counterpart of a current account "
                   "deficit."),
                bop, Scale::Diverging, true, flows};
    case FdiInflows:
        return {id, tr("FDI inflows"),
                tr("Direct investment by foreigners in the country (stakes of 10% or more, reinvested earnings, "
                   "intra-group loans), in % of GDP. Negative: disinvestment."),
                bop, Scale::Diverging, true, flows};
    case PortfolioInflows:
        return {id, tr("Portfolio inflows"),
                tr("Purchases of the country's shares and bonds by foreigners, net of sales, in % of GDP. "
                   "The most volatile capital flows."),
                bop, Scale::Diverging, true, flows};
    case TradeBalance:
        return {id, tr("Trade balance"), tr("Exports minus imports of goods and services, in % of GDP."), bop,
                Scale::Diverging, true, flows};
    case TradeOpenness:
        return {id, tr("Trade openness"), tr("Exports plus imports of goods and services, in % of GDP."), bop,
                Scale::Sequential, true, flows};
    case ExportVolume:
        return {id, tr("Export volume"), tr("Annual change in exports of goods and services, at constant prices."),
                imf, Scale::Diverging, true, flows};
    case Count:
        break;
    }
    return {};
}

MacroIndicators::Explanation MacroIndicators::explanation(Id id)
{
    switch (id) {
    case GdpGrowth:
        return {tr("The change in the volume of everything the economy produces in a year (gross domestic product), "
                   "with price increases stripped out."),
                tr("Around 2 to 3% is typical for advanced economies, 4 to 7% for fast-growing emerging ones. Below "
                   "zero the economy is shrinking; two quarters of decline in a row are usually called a recession."),
                tr("Rebounds after a crisis look strong because they start from a low base, and small economies "
                   "built on one resource (oil, mining) can swing by double digits from one year to the next.")};
    case Inflation:
        return {tr("The average rise in consumer prices over the year, measured on a basket of the goods and "
                   "services households buy."),
                tr("Most central banks aim for about 2%. Lasting higher inflation erodes purchasing power and usually "
                   "brings higher interest rates; negative values (deflation) can signal weak demand."),
                tr("An annual average hides turning points: inflation can already be falling while the year's average "
                   "is still high. Hyperinflation cases (hundreds of %) sit far beyond the color scale.")};
    case Unemployment:
        return {tr("The share of the labour force (people working or looking for work) who have no job and are "
                   "actively looking for one."),
                tr("A low rate points to a tight labour market, which pushes wages up. Rising unemployment is one of "
                   "the clearest signs of a downturn."),
                tr("Definitions and surveys differ between countries. Where most jobs are informal, few people "
                   "register as unemployed, so the rate can look low. The IMF covers only about 100 countries.")};
    case PolicyRate:
        return {tr("The key interest rate set by the central bank, which steers the cost of borrowing for banks, "
                   "firms and households."),
                tr("Central banks raise it to cool inflation and cut it to support activity. Higher rates also tend to "
                   "attract foreign capital and support the currency."),
                tr("Compare it with inflation (see the real policy rate): 5% is tight money with 2% inflation but loose "
                   "with 10%. Euro area members all show the ECB rate.")};
    case RealRate:
        return {tr("The policy rate minus this year's expected inflation (IMF estimate): the cost of money once price "
                   "increases are taken into account."),
                tr("Positive: monetary policy is restrictive and saving pays. Negative: borrowing is cheap in real "
                   "terms, which supports activity but can feed inflation."),
                tr("It combines the latest monthly rate with an annual inflation estimate, so it is an approximation, "
                   "and the neutral level differs from one country to another.")};
    case CurrentAccount:
        return {tr("A country's transactions with the rest of the world: exports minus imports of goods and services, "
                   "plus income (dividends, interest, wages) and transfers (remittances, aid) received minus paid."),
                tr("A surplus means the country earns more abroad than it spends and lends the difference to the rest "
                   "of the world; a deficit means it borrows from abroad (see net capital inflows)."),
                tr("A deficit is not bad in itself, since it can finance investment, but a large and lasting one "
                   "funded by short-term capital leaves a country exposed if investors pull out.")};
    case GovBalance:
        return {tr("Government revenue minus spending, all levels of government together. Negative values are "
                   "deficits."),
                tr("Deficits add to public debt. Many countries run deficits of 2 to 4% of GDP; the EU rule of thumb "
                   "caps them at 3%."),
                tr("Deficits widen on their own in recessions (less tax, more benefits) and shrink in booms, and oil "
                   "exporters' budgets follow oil prices.")};
    case GovDebt:
        return {tr("Everything the government owes (bonds, loans), without deducting the assets it holds."),
                tr("More debt means more interest to pay and less room to respond to the next crisis. Whether it is "
                   "sustainable depends more on interest rates compared with growth than on any fixed threshold."),
                tr("Japan carries more than 200% thanks to low rates and domestic savers, while some emerging countries "
                   "get into trouble at 60% when their debt is in foreign currency.")};
    case GdpPerCapita:
        return {tr("GDP divided by population, converted at purchasing power parity: prices are adjusted so that a "
                   "dollar buys the same amount everywhere."),
                tr("The usual measure of average living standards. The color scale is logarithmic: each step is a "
                   "multiplication, not an addition."),
                tr("It is an average and says nothing about inequality. Small financial centres and oil states rank "
                   "very high because corporate profits or resource income count in their GDP.")};
    case GdpSize:
        return {tr("The total value of what the economy produces in a year, at current prices, converted into US "
                   "dollars at market exchange rates."),
                tr("The weight of the country in the world economy and in global markets. The color scale is "
                   "logarithmic."),
                tr("Market exchange rates make it swing with currencies: a 20% devaluation shrinks GDP in dollars by "
                   "about as much, even if output has not changed.")};
    case NetCapitalInflows:
        return {tr("Money invested in the country by foreigners (companies, shares, bonds, loans) minus money "
                   "residents invest abroad over the year: the financial account of the balance of payments, with "
                   "the sign flipped."),
                tr("Positive: the country imports capital, the mirror image of a current account deficit. Negative: it "
                   "exports capital, like the surplus countries (Germany, China, oil exporters) that lend to the "
                   "rest of the world."),
                tr("Changes in central bank reserves are included. Because of errors and omissions in the statistics, "
                   "it never matches the current account exactly.")};
    case FdiInflows:
        return {tr("Long-term investment by foreign companies in the country: building or buying plants, taking "
                   "stakes of 10% or more, reinvesting profits and lending within the group."),
                tr("Seen as the most stable form of capital, bringing technology and jobs. Inflows of 2 to 5% of GDP "
                   "are common in open emerging economies."),
                tr("Financial hubs and low-tax countries (Ireland, Luxembourg, the Netherlands…) show huge swings, "
                   "driven by multinationals moving money through holding companies rather than by real "
                   "investment.")};
    case PortfolioInflows:
        return {tr("Purchases of the country's shares and bonds (government and corporate) by foreigners, net of "
                   "their sales, over the year."),
                tr("Large inflows lower borrowing costs and lift asset prices. Negative values mean foreigners are "
                   "selling, often in a crisis or when global interest rates rise."),
                tr("This \"hot money\" can reverse within weeks. Financial centres show very large figures compared "
                   "with the size of their economy.")};
    case TradeBalance:
        return {tr("Exports minus imports of goods and services: merchandise, but also tourism, transport, financial "
                   "and digital services."),
                tr("The main part of the current account. A surplus means the country sells more to the rest of the "
                   "world than it buys from it."),
                tr("A deficit can reflect strong domestic demand or heavy investment rather than weak competitiveness, "
                   "and commodity exporters' balances swing with prices.")};
    case TradeOpenness:
        return {tr("Exports plus imports of goods and services compared with GDP: how much of the economy goes "
                   "through international trade."),
                tr("Small open economies and trade hubs exceed 100% (Singapore, Luxembourg, Ireland); large economies "
                   "such as the United States or Brazil are around 25%."),
                tr("Re-exported goods count twice, once coming in and once going out, which inflates the ratio for "
                   "hubs. Large countries are naturally less open.")};
    case ExportVolume:
        return {tr("The change in the quantity of goods and services exported, with price changes stripped out (IMF "
                   "estimates and projections)."),
                tr("Shows whether foreign demand for the country's products is growing; it follows world trade and "
                   "the country's competitiveness."),
                tr("For commodity exporters, revenue can fall while volumes rise when prices drop: look at the trade "
                   "balance too.")};
    case InflationLatest:
        return {tr("The rise in consumer prices over the twelve months to the latest month published, as measured by "
                   "the country's statistical office (the harmonised index for EU countries)."),
                tr("The figure the central bank reacts to. Compare it with the annual average above: when the monthly "
                   "figure is below the year's average, inflation is coming down."),
                tr("Each country publishes on its own calendar, so the months differ by one or two across the map. "
                   "Baskets and methods differ too; the IMF's compilation covers most countries with a lag.")};
    case UnemploymentLatest:
        return {tr("The share of the labour force without a job in the latest month, seasonally adjusted, on the "
                   "harmonised (ILO) definition where the source applies it."),
                tr("The timeliest reading of the labour market. A rise of half a point from its low has usually come "
                   "with a recession in the United States."),
                tr("Only the countries Eurostat and the OECD compile monthly (about 45); elsewhere see the IMF's "
                   "annual figure. Quarterly surveys are shown at the middle of their quarter.")};
    case WheatYield:
    case CornYield:
    case RiceYield:
    case SoyYield:
        return {tr("The harvest per hectare of the crop (tonnes per hectare) in the marketing year, against the year "
                   "before: the USDA's estimates and forecasts for most countries, revised every month with its World "
                   "Agricultural Supply and Demand Estimates; Eurostat's harvest figures for each EU member."),
                tr("Yields move with the weather more than with the area sown: a drought, a heat wave at flowering or a "
                   "wet harvest shows here first, and a big producer's fall (the United States, Brazil, Russia, India, "
                   "China) tightens world supply and lifts prices. Green is a better harvest than the year before."),
                tr("The current marketing year is a forecast until the harvest is in, and southern hemisphere crops "
                   "are harvested months after northern ones. A rebound after a bad year looks strong; small producers "
                   "swing widely. The USDA gives the European Union as a whole, so EU members come from Eurostat, a "
                   "year behind.")};
    case RoadFreight:
        return {tr("Goods carried by lorries on the country's roads, in tonne-kilometres (one tonne over one "
                   "kilometre), over the year, against the year before."),
                tr("Road haulage moves most goods within a country, so it follows industrial output, retail and "
                   "construction closely: a fall often comes with a slowdown, a rise with a pickup or with goods moving "
                   "from rail to road."),
                tr("Annual figures from national surveys, a year or more behind, about 45 countries. European figures "
                   "count the country's own hauliers, wherever they drive, so a shift to foreign hauliers looks like "
                   "a fall.")};
    case CrudeStocks:
        return {tr("Crude oil held in the country at the end of the month (refineries, terminals, tank farms and, "
                   "where reported, strategic reserves), against the same month a year before."),
                tr("Rising stocks point to ample supply or weak refinery demand and weigh on prices; falling stocks "
                   "point to a tight market. Importers build strategic stocks when prices are low."),
                tr("Countries report to JODI themselves, with different coverage: some leave out strategic or "
                   "military stocks, some report none (China, the United Arab Emirates, Singapore). A month's "
                   "maintenance at a big refinery moves a small country's stocks a lot.")};
    case Count:
        break;
    }
    return {};
}

int MacroIndicators::defaultYear(Id id) const
{
    if (isCrop(id))
        return m_supply ? m_supply->crops()->defaultYear(cropOf(id)) : currentYear() - 1;
    if (id == RoadFreight)
        return m_supply && m_supply->road()->lastYear() ? m_supply->road()->defaultYear() : currentYear() - 1;
    if (fromBop(id))
        return m_bop->latestYear() ? m_bop->latestYear() : currentYear() - 1;
    return currentYear();
}

bool MacroIndicators::hasData(Id id) const
{
    if (isCrop(id))
        return m_supply && m_supply->crops()->hasData() && m_economy->hasData();
    if (id == RoadFreight)
        return m_supply && m_supply->road()->hasData() && m_economy->hasData();
    if (id == CrudeStocks)
        return m_supply && m_supply->oil()->hasData() && m_economy->hasData();
    if (fromKey(id))
        return m_key && m_key->hasData();
    // Balance of payments figures are shares of the WEO's GDP.
    return m_economy->hasData() && (!fromBop(id) || m_bop->hasData());
}

int MacroIndicators::lastYear(Id id) const
{
    if (isCrop(id))
        return std::max(m_supply ? m_supply->crops()->lastYear(cropOf(id)) : 0, defaultYear(id));
    if (id == RoadFreight)
        return std::max(m_supply ? m_supply->road()->lastYear() : 0, defaultYear(id));
    if (fromBop(id))
        return std::max(m_bop->lastYear(), defaultYear(id));
    int last = currentYear();
    for (const auto &c : m_economy->countries()) {
        const auto s = m_economy->series(c.iso3, WorldEconomy::GdpGrowth);
        if (!s.isEmpty())
            last = std::max(last, s.lastKey());
    }
    return last;
}

double MacroIndicators::value(Id id, const QString &iso3, int year) const
{
    const double nan = std::nan("");
    if (isCrop(id))
        return m_supply ? m_supply->crops()->change(iso3, cropOf(id), year) : nan;
    if (id == RoadFreight)
        return m_supply ? m_supply->road()->change(iso3, year) : nan;
    if (id == CrudeStocks) {
        supply::OilStocks::Latest l;
        return m_supply && m_supply->oil()->latest(iso3, &l) ? l.change : nan;
    }
    if (fromKey(id))
        return m_key ? m_key->figure(iso3, keyIndicator(id)).value : nan;
    if (id == PolicyRate || id == RealRate) {
        double rate;
        QDate month;
        double change;
        if (!policyRate(iso3, &rate, &month, &change))
            return nan;
        if (id == PolicyRate)
            return rate;
        return rate - m_economy->value(iso3, WorldEconomy::Inflation, month.year());
    }
    if (fromBop(id)) {
        // Shares of GDP: the IMF publishes flows in dollars.
        const double gdp = m_economy->value(iso3, WorldEconomy::GdpUsd, year);
        if (!(gdp > 0))
            return nan;
        auto bop = [&](BalanceOfPayments::Series s) { return m_bop->value(iso3, s, year); };
        double v = nan;
        switch (id) {
        case NetCapitalInflows: v = -bop(BalanceOfPayments::FinancialAccount); break;
        case FdiInflows: v = bop(BalanceOfPayments::FdiInflows); break;
        case PortfolioInflows: v = bop(BalanceOfPayments::PortfolioInflows); break;
        case TradeBalance: v = bop(BalanceOfPayments::Exports) - bop(BalanceOfPayments::Imports); break;
        case TradeOpenness: v = bop(BalanceOfPayments::Exports) + bop(BalanceOfPayments::Imports); break;
        default: break;
        }
        return 100 * v / gdp;
    }
    return m_economy->value(iso3, weoIndicator(id), year);
}

QHash<QString, double> MacroIndicators::values(Id id, int year) const
{
    QHash<QString, double> result;
    if (fromKey(id)) {
        if (!m_key)
            return result;
        // The countries of the WEO only: the map's and the rankings' set.
        const QHash<QString, double> all = m_key->values(keyIndicator(id));
        for (const auto &c : m_economy->countries())
            if (const auto it = all.constFind(c.iso3); it != all.constEnd())
                result.insert(c.iso3, it.value());
        return result;
    }
    for (const auto &c : m_economy->countries()) {
        const double v = value(id, c.iso3, year);
        if (std::isfinite(v))
            result.insert(c.iso3, v);
    }
    return result;
}

QString MacroIndicators::yearNote(Id id, int year) const
{
    const int now = currentYear();
    if (isCrop(id))
        return year >= now - 1 ? tr("%1/%2 marketing year · USDA estimate or forecast").arg(year).arg((year + 1) % 100, 2, 10, QChar('0'))
                               : tr("%1/%2 marketing year").arg(year).arg((year + 1) % 100, 2, 10, QChar('0'));
    if (id == RoadFreight)
        return tr("%1 against %2").arg(year).arg(year - 1);
    return year > now    ? tr("%1 · IMF projection").arg(year)
           : year == now ? tr("%1 · IMF estimate").arg(year)
                         : QString::number(year);
}

QString MacroIndicators::detail(Id id, const QString &iso3, int year) const
{
    if (!m_supply)
        return {};
    const QLocale loc;
    if (isCrop(id)) {
        const supply::CropYields *c = m_supply->crops();
        const double now = c->yield(iso3, cropOf(id), year), before = c->yield(iso3, cropOf(id), year - 1);
        if (!(now > 0))
            return {};
        return tr("%1 t/ha (%2 the year before) · %3")
            .arg(loc.toString(now, 'f', 2), loc.toString(before, 'f', 2),
                 c->source(iso3, cropOf(id)) == supply::CropYields::Source::Eurostat ? QStringLiteral("Eurostat")
                                                                                     : QStringLiteral("USDA"));
    }
    if (id == RoadFreight) {
        const double now = m_supply->road()->tonneKm(iso3, year);
        return now > 0 ? tr("%1 bn tonne-km in %2").arg(loc.toString(now / 1000, 'f', now >= 10000 ? 0 : 1)).arg(year)
                       : QString();
    }
    if (id == CrudeStocks) {
        supply::OilStocks::Latest l;
        if (!m_supply->oil()->latest(iso3, &l))
            return {};
        return tr("%1 million barrels at the end of %2")
            .arg(loc.toString(l.value / 1000, 'f', l.value >= 100000 ? 0 : 1), loc.toString(l.month, "MMMM yyyy"));
    }
    return {};
}

QString MacroIndicators::format(Id id, double v) const
{
    if (!std::isfinite(v))
        return QString::fromUtf8("—");
    const QLocale loc;
    switch (id) {
    case GdpPerCapita:
        return tr("$%1").arg(loc.toString(v, 'f', 0));
    case GdpSize:
        if (v >= 1000)
            return tr("$%1 tn").arg(loc.toString(v / 1000, 'f', v >= 10000 ? 1 : 2));
        return tr("$%1 bn").arg(loc.toString(v, 'f', v >= 100 ? 0 : 1));
    case PolicyRate:
        return tr("%1%").arg(loc.toString(v, 'f', 2));
    case InflationLatest:
    case UnemploymentLatest:
        return tr("%1%").arg((v < 0 ? QString::fromUtf8("−") : QString()) + loc.toString(std::abs(v), 'f', 1));
    default: {
        const QString sign = info(id).scale == Scale::Diverging && v > 0 ? "+" : "";
        const QString num = loc.toString(std::abs(v), 'f', 1);
        return tr("%1%").arg((v < 0 ? QString::fromUtf8("−") : sign) + num);
    }
    }
}

bool MacroIndicators::policyRate(const QString &iso3, double *rate, QDate *month, double *change12m) const
{
    const WorldEconomy::Country *c = m_economy->country(iso3);
    if (!c)
        return false;
    const QVector<WorldPolicyRates::Point> h = m_rates->history(c->iso2);
    if (h.isEmpty())
        return false;
    *rate = h.last().rate;
    *month = h.last().month;
    *change12m = std::nan("");
    for (const auto &p : h) {
        if (p.month == h.last().month.addMonths(-12)) {
            *change12m = h.last().rate - p.rate;
            break;
        }
    }
    return true;
}

MacroIndicators::Range MacroIndicators::range(Id id, const QHash<QString, double> &values) const
{
    QVector<double> v;
    for (double x : values)
        if (info(id).scale != Scale::Log || x > 0)
            v.push_back(info(id).scale == Scale::Log ? std::log10(x) : x);
    if (v.isEmpty())
        return {};
    double lo = percentile(v, 0.05);
    double hi = percentile(v, 0.95);
    if (info(id).scale == Scale::Diverging) {
        const double m = std::max({std::abs(lo), std::abs(hi), 1e-6});
        return {-m, m};
    }
    if (hi - lo < 1e-9)
        hi = lo + 1;
    return {lo, hi};
}

QColor MacroIndicators::color(Id id, double v, const Range &r) const
{
    const Palette &pal = Theme::current();
    if (info(id).scale == Scale::Diverging) {
        const double t = v / r.hi;
        // Start a little away from the land color so small values still read.
        return t >= 0 ? mix(pal.Land, pal.Up, 0.15 + 0.85 * t) : mix(pal.Land, pal.Down, 0.15 - 0.85 * t);
    }
    const double x = info(id).scale == Scale::Log ? std::log10(std::max(v, 1e-9)) : v;
    return mix(pal.Land, pal.Accent, 0.12 + 0.88 * (x - r.lo) / (r.hi - r.lo));
}

double MacroIndicators::level(Id id, double v, const Range &r) const
{
    const double x = info(id).scale == Scale::Log ? std::log10(std::max(v, 1e-9)) : v;
    return std::clamp((x - r.lo) / (r.hi - r.lo), 0.0, 1.0);
}

QVector<QColor> MacroIndicators::ramp(Id id) const
{
    QVector<QColor> colors;
    const Range r = info(id).scale == Scale::Diverging ? Range {-1, 1} : Range {0, 1};
    for (int i = 0; i <= 10; ++i) {
        const double x = r.lo + (r.hi - r.lo) * i / 10.0;
        colors.push_back(color(id, info(id).scale == Scale::Log ? std::pow(10.0, x) : x, r));
    }
    return colors;
}
