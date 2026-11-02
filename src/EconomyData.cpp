#include "EconomyData.h"

#include <QCoreApplication>
#include <QPointer>

#include <algorithm>
#include <cmath>
#include <memory>

namespace economy {

namespace {

// Range sets as the panels offer them.
const QStringList Daily {"1M", "3M", "6M", "1Y", "2Y", "5Y"};
const QStringList Weekly {"3M", "6M", "1Y", "2Y", "5Y"};
const QStringList ShortHistory {"1M", "3M", "6M", "1Y", "2Y"};   // ICE spreads (FRED keeps three years), altcoin season
const QStringList Monthly {"1Y", "2Y", "5Y", "10Y"};
const QStringList Quarterly {"2Y", "5Y", "10Y", "20Y", "MAX"};

Indicator fred(const char *title, const char *help, const char *id, Unit unit, double scale, int decimals, Hue hue,
               const QStringList &ranges, const QString &range, bool zeroLine = false)
{
    Indicator i;
    i.title = title;
    i.help = help;
    i.source = Source::Fred;
    i.code = id;
    i.unit = unit;
    i.scale = scale;
    i.decimals = decimals;
    i.hue = hue;
    i.ranges = ranges;
    i.range = std::max(0, int(ranges.indexOf(range)));
    i.zeroLine = zeroLine;
    return i;
}

Indicator other(const char *title, const char *help, Source source, const QString &code, Unit unit, int decimals, Hue hue,
                const QStringList &ranges, const QString &range, Zones zones = Zones::None)
{
    Indicator i = fred(title, help, "", unit, 1, decimals, hue, ranges, range);
    i.source = source;
    i.code = code;
    i.zones = zones;
    return i;
}

Cell chart(const Indicator &i, int stretch = 1) { return {i, false, stretch}; }
Cell gauge(const Indicator &i, int stretch = 1) { return {i, true, stretch}; }

constexpr double K = 1e3, M = 1e6, B = 1e9;

} // namespace

QString text(const char *english)
{
    return QCoreApplication::translate("Economy", english);
}

const QVector<Section> &sections()
{
    using U = Unit;
    using H = Hue;
    static const QVector<Section> all = [] {
        const Indicator stockFg = other(
            QT_TRANSLATE_NOOP("Economy", "Stock market Fear & Greed index"),
            QT_TRANSLATE_NOOP("Economy", "Deepwater's estimate of the stock market's mood, built like CNN's index: "
                                         "momentum, volatility, safe haven demand, breadth and junk bond demand, each "
                                         "ranked against its last year (0 extreme fear, 100 extreme greed), averaged. "
                                         "An estimate, not CNN's own figure."),
            Source::StockFearGreed, {}, U::Number, 0, H::Amber, Daily, "1Y", Zones::FearGreed);
        Indicator stockFgHistory = stockFg;
        stockFgHistory.title = QT_TRANSLATE_NOOP("Economy", "Fear & Greed – historical");
        stockFgHistory.zones = Zones::None;
        const Indicator cryptoFg = other(
            QT_TRANSLATE_NOOP("Economy", "Crypto Fear & Greed"),
            QT_TRANSLATE_NOOP("Economy", "alternative.me's crypto Fear & Greed index: volatility, momentum and volume, "
                                         "social media, bitcoin dominance and search trends, from 0 (extreme fear) to "
                                         "100 (extreme greed). Updated daily."),
            Source::CryptoFearGreed, {}, U::Number, 0, H::Amber, Daily, "1Y", Zones::FearGreed);
        Indicator cryptoFgHistory = cryptoFg;
        cryptoFgHistory.title = QT_TRANSLATE_NOOP("Economy", "Fear & Greed – historical");
        const Indicator altSeason = other(
            QT_TRANSLATE_NOOP("Economy", "Altcoin season index"),
            QT_TRANSLATE_NOOP("Economy", "CoinMarketCap's altcoin season index: the share of the 100 largest coins "
                                         "(less stablecoins and wrapped tokens) that did better than bitcoin over 90 "
                                         "days. 75 and above is an altcoin season, 25 and below a bitcoin season."),
            Source::AltcoinSeason, {}, U::Number, 0, H::Amber, ShortHistory, "1Y", Zones::AltSeason);
        Indicator altSeasonHistory = altSeason;
        altSeasonHistory.title = QT_TRANSLATE_NOOP("Economy", "Altcoin season – historical");

        QVector<Section> s;
        s.push_back({QT_TRANSLATE_NOOP("Economy", "Sentiment"), {
            {gauge(stockFg, 2),
             chart(fred(QT_TRANSLATE_NOOP("Economy", "VIX volatility index"),
                        QT_TRANSLATE_NOOP("Economy", "The volatility of the S&P 500 that options price for the next 30 "
                                                     "days, in percent a year. Below 15 is calm, above 30 is stress."),
                        "VIXCLS", U::Number, 1, 2, H::Red, Daily, "1Y"), 3)},
            {chart(stockFgHistory),
             chart(fred(QT_TRANSLATE_NOOP("Economy", "Michigan sentiment"),
                        QT_TRANSLATE_NOOP("Economy", "The University of Michigan's index of consumer sentiment "
                                                     "(1966 = 100): how households see their finances and the economy."),
                        "UMCSENT", U::Number, 1, 2, H::Green, Monthly, "1Y")),
             chart(fred(QT_TRANSLATE_NOOP("Economy", "Financial stress index"),
                        QT_TRANSLATE_NOOP("Economy", "The St. Louis Fed's financial stress index, weekly, from 18 rates, "
                                                     "spreads and volatilities. Zero is normal stress, above zero more."),
                        "STLFSI4", U::Number, 1, 2, H::Amber, Weekly, "1Y", true))},
            {chart(fred(QT_TRANSLATE_NOOP("Economy", "Financial conditions"),
                        QT_TRANSLATE_NOOP("Economy", "The Chicago Fed's national financial conditions index (NFCI), "
                                                     "weekly. Negative: looser than average; positive: tighter."),
                        "NFCI", U::Number, 1, 2, H::Purple, Weekly, "1Y", true)),
             chart(fred(QT_TRANSLATE_NOOP("Economy", "High yield spread"),
                        QT_TRANSLATE_NOOP("Economy", "The extra yield of US high-yield corporate bonds over Treasuries "
                                                     "(ICE BofA option-adjusted spread). It widens when credit risk is "
                                                     "feared."),
                        "BAMLH0A0HYM2", U::Percent, 1, 2, H::Pink, ShortHistory, "1Y")),
             chart(fred(QT_TRANSLATE_NOOP("Economy", "Policy uncertainty"),
                        QT_TRANSLATE_NOOP("Economy", "The daily US economic policy uncertainty index (Baker, Bloom and "
                                                     "Davis), from newspaper articles about policy and the economy."),
                        "USEPUINDXD", U::Number, 1, 2, H::Mint, Monthly, "1Y"))},
        }});
        s.push_back({QT_TRANSLATE_NOOP("Economy", "Crypto"), {
            {gauge(cryptoFg), gauge(altSeason)},
            {chart(cryptoFgHistory), chart(altSeasonHistory)},
            {chart(other(QT_TRANSLATE_NOOP("Economy", "Total crypto market cap"),
                         QT_TRANSLATE_NOOP("Economy", "The value of every listed cryptocurrency (CoinMarketCap), daily "
                                                      "over the last year and weekly before."),
                         Source::CryptoCap, {}, U::Dollars, 2, H::Amber, Daily, "1Y")),
             chart(other(QT_TRANSLATE_NOOP("Economy", "Bitcoin dominance"),
                         QT_TRANSLATE_NOOP("Economy", "Bitcoin's share of the crypto market's total value "
                                                      "(CoinMarketCap). It falls when money moves to other coins."),
                         Source::BtcDominance, {}, U::Percent, 2, H::Amber, Daily, "1Y"))},
        }});
        s.push_back({QT_TRANSLATE_NOOP("Economy", "Growth"), {
            {chart(fred(QT_TRANSLATE_NOOP("Economy", "US GDP"),
                        QT_TRANSLATE_NOOP("Economy", "US gross domestic product, quarterly, at a seasonally adjusted "
                                                     "annual rate, in current dollars (BEA)."),
                        "GDP", U::Dollars, B, 2, H::Green, Quarterly, "10Y")),
             chart(fred(QT_TRANSLATE_NOOP("Economy", "Real personal consumption"),
                        QT_TRANSLATE_NOOP("Economy", "What households spend, monthly, at an annual rate, in chained 2017 "
                                                     "dollars (BEA): about two thirds of GDP."),
                        "PCEC96", U::Dollars, B, 2, H::Red, Monthly, "10Y"))},
            {chart(fred(QT_TRANSLATE_NOOP("Economy", "Capacity utilization"),
                        QT_TRANSLATE_NOOP("Economy", "The share of industry's capacity in use (Federal Reserve). Around "
                                                     "80% the economy runs hot; low readings mean slack."),
                        "TCU", U::Percent, 1, 2, H::Purple, Monthly, "10Y")),
             chart(fred(QT_TRANSLATE_NOOP("Economy", "Durable goods orders"),
                        QT_TRANSLATE_NOOP("Economy", "New orders for goods meant to last three years or more (Census), "
                                                     "monthly: a lead on business investment."),
                        "DGORDER", U::Dollars, M, 2, H::Red, Monthly, "10Y"))},
            {chart(fred(QT_TRANSLATE_NOOP("Economy", "Chicago Fed activity index"),
                        QT_TRANSLATE_NOOP("Economy", "The Chicago Fed national activity index (CFNAI), from 85 monthly "
                                                     "series. Zero is trend growth; below -0.7 has often meant a recession."),
                        "CFNAI", U::Number, 1, 2, H::Yellow, Monthly, "10Y", true)),
             chart(fred(QT_TRANSLATE_NOOP("Economy", "Manufacturing employment"),
                        QT_TRANSLATE_NOOP("Economy", "Jobs in US manufacturing (BLS payroll survey), monthly."),
                        "MANEMP", U::Count, K, 2, H::Yellow, Monthly, "10Y"))},
        }});
        s.push_back({QT_TRANSLATE_NOOP("Economy", "Inflation"), {
            {chart(fred(QT_TRANSLATE_NOOP("Economy", "CPI headline"),
                        QT_TRANSLATE_NOOP("Economy", "The consumer price index for all urban consumers (BLS, 1982-84 = "
                                                     "100), seasonally adjusted."),
                        "CPIAUCSL", U::Number, 1, 2, H::Red, Monthly, "5Y")),
             chart(fred(QT_TRANSLATE_NOOP("Economy", "CPI core"),
                        QT_TRANSLATE_NOOP("Economy", "The consumer price index less food and energy, the more stable part "
                                                     "of inflation."),
                        "CPILFESL", U::Number, 1, 2, H::Red, Monthly, "5Y"))},
            {chart(fred(QT_TRANSLATE_NOOP("Economy", "Producer price index"),
                        QT_TRANSLATE_NOOP("Economy", "Prices producers get for all commodities (BLS, 1982 = 100): costs "
                                                     "that often reach consumers later."),
                        "PPIACO", U::Number, 1, 2, H::Red, Monthly, "5Y")),
             chart(fred(QT_TRANSLATE_NOOP("Economy", "PCE price index"),
                        QT_TRANSLATE_NOOP("Economy", "The price index of personal consumption expenditures (BEA, 2017 = "
                                                     "100): the measure the Fed's 2% target is set on."),
                        "PCEPI", U::Number, 1, 2, H::Red, Monthly, "5Y"))},
            {chart(fred(QT_TRANSLATE_NOOP("Economy", "Inflation expectations (1Y)"),
                        QT_TRANSLATE_NOOP("Economy", "The inflation households expect over the next year (University of "
                                                     "Michigan survey, median)."),
                        "MICH", U::Percent, 1, 2, H::Red, Monthly, "5Y")),
             chart(fred(QT_TRANSLATE_NOOP("Economy", "Inflation expectations (5Y)"),
                        QT_TRANSLATE_NOOP("Economy", "Expected inflation over five years, from the Cleveland Fed's model "
                                                     "of Treasury yields, inflation swaps and surveys."),
                        "EXPINF5YR", U::Percent, 1, 2, H::Red, Monthly, "5Y"))},
        }});
        s.push_back({QT_TRANSLATE_NOOP("Economy", "Employment"), {
            {chart(fred(QT_TRANSLATE_NOOP("Economy", "Unemployment rate"),
                        QT_TRANSLATE_NOOP("Economy", "The share of the labor force without a job and looking for one "
                                                     "(BLS household survey)."),
                        "UNRATE", U::Percent, 1, 2, H::Purple, Monthly, "5Y")),
             chart(fred(QT_TRANSLATE_NOOP("Economy", "Nonfarm payrolls"),
                        QT_TRANSLATE_NOOP("Economy", "Jobs on US payrolls outside farming (BLS establishment survey); the "
                                                     "monthly change is the jobs report's headline."),
                        "PAYEMS", U::Count, K, 2, H::Orange, Monthly, "5Y"))},
            {chart(fred(QT_TRANSLATE_NOOP("Economy", "Initial jobless claims"),
                        QT_TRANSLATE_NOOP("Economy", "New claims for unemployment insurance, weekly (Department of Labor): "
                                                     "the quickest sign of layoffs."),
                        "ICSA", U::Count, 1, 2, H::Orange, Weekly, "5Y")),
             chart(fred(QT_TRANSLATE_NOOP("Economy", "Continuing claims"),
                        QT_TRANSLATE_NOOP("Economy", "People still claiming unemployment insurance, weekly: rises when "
                                                     "finding a new job takes longer."),
                        "CCSA", U::Count, 1, 2, H::Orange, Weekly, "5Y"))},
            {chart(fred(QT_TRANSLATE_NOOP("Economy", "Labor participation rate"),
                        QT_TRANSLATE_NOOP("Economy", "The share of the population aged 16 and over working or looking for "
                                                     "work."),
                        "CIVPART", U::Percent, 1, 2, H::Purple, Monthly, "5Y")),
             chart(fred(QT_TRANSLATE_NOOP("Economy", "JOLTS job openings"),
                        QT_TRANSLATE_NOOP("Economy", "Jobs open on the last business day of the month (BLS job openings and "
                                                     "labor turnover survey)."),
                        "JTSJOL", U::Count, K, 2, H::Orange, Monthly, "5Y"))},
        }});
        s.push_back({QT_TRANSLATE_NOOP("Economy", "Rates"), {
            {chart(fred(QT_TRANSLATE_NOOP("Economy", "Fed funds rate"),
                        QT_TRANSLATE_NOOP("Economy", "The effective federal funds rate, daily: what banks pay each other "
                                                     "overnight, steered by the Fed's target range."),
                        "DFF", U::Percent, 1, 2, H::Teal, Daily, "5Y")),
             chart(fred(QT_TRANSLATE_NOOP("Economy", "10-year Treasury yield"),
                        QT_TRANSLATE_NOOP("Economy", "The yield of the 10-year US Treasury note at constant maturity, daily."),
                        "DGS10", U::Percent, 1, 2, H::Teal, Daily, "5Y"))},
            {chart(fred(QT_TRANSLATE_NOOP("Economy", "2-year Treasury yield"),
                        QT_TRANSLATE_NOOP("Economy", "The yield of the 2-year US Treasury note, daily: it follows where the "
                                                     "market expects the Fed's rate to go."),
                        "DGS2", U::Percent, 1, 2, H::Teal, Daily, "5Y")),
             chart(fred(QT_TRANSLATE_NOOP("Economy", "Yield curve (10Y-2Y)"),
                        QT_TRANSLATE_NOOP("Economy", "The 10-year yield less the 2-year. Below zero (an inverted curve) has "
                                                     "come before most US recessions."),
                        "T10Y2Y", U::Percent, 1, 2, H::Teal, Daily, "5Y", true))},
            {chart(fred(QT_TRANSLATE_NOOP("Economy", "Yield curve (10Y-3M)"),
                        QT_TRANSLATE_NOOP("Economy", "The 10-year yield less the 3-month bill, the spread the New York Fed's "
                                                     "recession model uses."),
                        "T10Y3M", U::Percent, 1, 2, H::Teal, Daily, "5Y", true)),
             chart(fred(QT_TRANSLATE_NOOP("Economy", "Investment grade spread"),
                        QT_TRANSLATE_NOOP("Economy", "The extra yield of US investment-grade corporate bonds over "
                                                     "Treasuries (ICE BofA option-adjusted spread)."),
                        "BAMLC0A0CM", U::Percent, 1, 2, H::Teal, ShortHistory, "2Y"))},
        }});
        s.push_back({QT_TRANSLATE_NOOP("Economy", "Housing"), {
            {chart(fred(QT_TRANSLATE_NOOP("Economy", "Housing starts"),
                        QT_TRANSLATE_NOOP("Economy", "New homes whose construction began, monthly, at a seasonally adjusted "
                                                     "annual rate (Census)."),
                        "HOUST", U::Count, K, 2, H::Orange, Monthly, "5Y")),
             chart(fred(QT_TRANSLATE_NOOP("Economy", "Building permits"),
                        QT_TRANSLATE_NOOP("Economy", "New homes authorized by permits, at an annual rate: they come before "
                                                     "the starts."),
                        "PERMIT", U::Count, K, 2, H::Orange, Monthly, "5Y"))},
            {chart(fred(QT_TRANSLATE_NOOP("Economy", "New home sales"),
                        QT_TRANSLATE_NOOP("Economy", "Sales of new single-family homes, at an annual rate (Census)."),
                        "HSN1F", U::Count, K, 2, H::Orange, Monthly, "5Y")),
             chart(fred(QT_TRANSLATE_NOOP("Economy", "Case-Shiller home price"),
                        QT_TRANSLATE_NOOP("Economy", "The S&P CoreLogic Case-Shiller US national home price index "
                                                     "(January 2000 = 100), not seasonally adjusted."),
                        "CSUSHPINSA", U::Number, 1, 2, H::Yellow, Monthly, "5Y"))},
            {chart(fred(QT_TRANSLATE_NOOP("Economy", "Active listings"),
                        QT_TRANSLATE_NOOP("Economy", "Homes for sale in the US (Realtor.com), monthly: the supply buyers "
                                                     "choose from."),
                        "ACTLISCOUUS", U::Count, 1, 2, H::Orange, Monthly, "5Y")),
             chart(fred(QT_TRANSLATE_NOOP("Economy", "30-year mortgage rate"),
                        QT_TRANSLATE_NOOP("Economy", "The average 30-year fixed mortgage rate, weekly (Freddie Mac)."),
                        "MORTGAGE30US", U::Percent, 1, 2, H::Teal, Weekly, "5Y"))},
        }});
        s.push_back({QT_TRANSLATE_NOOP("Economy", "Consumer"), {
            {chart(fred(QT_TRANSLATE_NOOP("Economy", "Sahm rule recession indicator"),
                        QT_TRANSLATE_NOOP("Economy", "The 3-month average unemployment rate less its low of the past year, "
                                                     "in points. At 0.5 or more a recession has usually begun."),
                        "SAHMREALTIME", U::Percent, 1, 2, H::Purple, Monthly, "10Y", true)),
             chart(other(QT_TRANSLATE_NOOP("Economy", "Consumer confidence"),
                         QT_TRANSLATE_NOOP("Economy", "The OECD's consumer confidence indicator for the United States, "
                                                      "monthly (long-term average = 100)."),
                         Source::Oecd, "USA.M.CCICP.....", U::Number, 2, H::Yellow, Monthly, "10Y"))},
            {chart(fred(QT_TRANSLATE_NOOP("Economy", "Debt service ratio"),
                        QT_TRANSLATE_NOOP("Economy", "Household debt payments as a share of disposable income, quarterly "
                                                     "(Federal Reserve)."),
                        "TDSP", U::Percent, 1, 2, H::Purple, Quarterly, "10Y")),
             chart(fred(QT_TRANSLATE_NOOP("Economy", "Consumer credit"),
                        QT_TRANSLATE_NOOP("Economy", "Credit owed by households outside mortgages (cards, car and student "
                                                     "loans), monthly (Federal Reserve G.19)."),
                        "TOTALSL", U::Dollars, M, 2, H::Green, Monthly, "10Y"))},
            {chart(fred(QT_TRANSLATE_NOOP("Economy", "Personal savings rate"),
                        QT_TRANSLATE_NOOP("Economy", "Saving as a share of disposable personal income, monthly (BEA)."),
                        "PSAVERT", U::Percent, 1, 2, H::Purple, Monthly, "10Y")),
             chart(fred(QT_TRANSLATE_NOOP("Economy", "Real retail sales"),
                        QT_TRANSLATE_NOOP("Economy", "Retail and food services sales deflated by the CPI, monthly, in 1982-84 "
                                                     "dollars."),
                        "RRSFS", U::Dollars, M, 2, H::Red, Monthly, "10Y"))},
        }});
        s.push_back({QT_TRANSLATE_NOOP("Economy", "Liquidity"), {
            {chart(fred(QT_TRANSLATE_NOOP("Economy", "Fed balance sheet"),
                        QT_TRANSLATE_NOOP("Economy", "Total assets of the Federal Reserve, weekly (H.4.1): it grows with "
                                                     "quantitative easing and shrinks with tightening."),
                        "WALCL", U::Dollars, M, 2, H::Green, Weekly, "5Y")),
             chart(fred(QT_TRANSLATE_NOOP("Economy", "Treasury general account"),
                        QT_TRANSLATE_NOOP("Economy", "The Treasury's cash at the Fed, weekly. When it rises it drains "
                                                     "reserves from the banks."),
                        "WTREGEN", U::Dollars, M, 2, H::Green, Weekly, "5Y"))},
            {chart(fred(QT_TRANSLATE_NOOP("Economy", "Reverse repo"),
                        QT_TRANSLATE_NOOP("Economy", "Cash money market funds and others park at the Fed overnight (reverse "
                                                     "repurchase agreements), daily."),
                        "RRPONTSYD", U::Dollars, B, 2, H::Green, Daily, "5Y")),
             chart(fred(QT_TRANSLATE_NOOP("Economy", "Retail money funds"),
                        QT_TRANSLATE_NOOP("Economy", "Retail money market fund balances, weekly (Federal Reserve H.6)."),
                        "WRMFNS", U::Dollars, B, 2, H::Green, Weekly, "5Y"))},
            {chart(fred(QT_TRANSLATE_NOOP("Economy", "M2 money supply"),
                        QT_TRANSLATE_NOOP("Economy", "Cash, deposits and retail money funds, monthly, seasonally adjusted "
                                                     "(Federal Reserve H.6)."),
                        "M2SL", U::Dollars, B, 2, H::Green, Monthly, "5Y")),
             chart(fred(QT_TRANSLATE_NOOP("Economy", "M2 velocity"),
                        QT_TRANSLATE_NOOP("Economy", "Nominal GDP divided by M2, quarterly: how many times a dollar of M2 is "
                                                     "spent in a year."),
                        "M2V", U::Number, 1, 3, H::Purple, Quarterly, "5Y"))},
        }});
        s.push_back({QT_TRANSLATE_NOOP("Economy", "Global"), {
            {chart(other(QT_TRANSLATE_NOOP("Economy", "Dollar index (DXY)"),
                         QT_TRANSLATE_NOOP("Economy", "The ICE dollar index: the dollar against six currencies, the euro "
                                                      "for 58% (Yahoo Finance, daily close)."),
                         Source::Yahoo, "DX-Y.NYB", U::Number, 2, H::Amber, Daily, "5Y")),
             chart(fred(QT_TRANSLATE_NOOP("Economy", "USD/JPY exchange rate"),
                        QT_TRANSLATE_NOOP("Economy", "Yen per dollar at noon in New York, daily (Federal Reserve H.10)."),
                        "DEXJPUS", U::Yen, 1, 2, H::Amber, Daily, "5Y"))},
            {chart(fred(QT_TRANSLATE_NOOP("Economy", "Japan 10Y yield"),
                        QT_TRANSLATE_NOOP("Economy", "Japan's 10-year government bond yield, monthly average (OECD)."),
                        "IRLTLT01JPM156N", U::Percent, 1, 2, H::Teal, Monthly, "5Y")),
             chart(fred(QT_TRANSLATE_NOOP("Economy", "Euro area CPI"),
                        QT_TRANSLATE_NOOP("Economy", "The euro area's harmonised index of consumer prices (Eurostat, 2025 = "
                                                     "100), monthly."),
                        "CP0000EZ19M086NEST", U::Number, 1, 2, H::Red, Monthly, "5Y"))},
        }});
        return s;
    }();
    return all;
}

QColor hueColor(Hue hue)
{
    switch (hue) {
    case Hue::Red: return QColor(0xd9, 0x5a, 0x63);
    case Hue::Green: return QColor(0x4f, 0xb0, 0x6d);
    case Hue::Amber: return QColor(0xe0, 0xa1, 0x3c);
    case Hue::Purple: return QColor(0x8f, 0x6c, 0xdc);
    case Hue::Teal: return QColor(0x5b, 0xb4, 0xc8);
    case Hue::Orange: return QColor(0xe5, 0x80, 0x3e);
    case Hue::Yellow: return QColor(0xd8, 0xbf, 0x4e);
    case Hue::Pink: return QColor(0xd8, 0x5d, 0x91);
    case Hue::Mint: return QColor(0x4c, 0xc8, 0xa0);
    }
    return QColor(0xe0, 0xa1, 0x3c);
}

int rangeDays(const QString &range)
{
    if (range == "MAX")
        return 0;
    const int n = range.left(range.size() - 1).toInt();
    if (range.endsWith('M'))
        return n * 31;
    if (range.endsWith('Y'))
        return n * 366;
    return 366;
}

QString zoneLabel(Zones zones, double v)
{
    if (!std::isfinite(v))
        return {};
    if (zones == Zones::AltSeason) {
        if (v >= 75) return text(QT_TRANSLATE_NOOP("Economy", "Altcoin season"));
        if (v > 50) return text(QT_TRANSLATE_NOOP("Economy", "Alt leaning"));
        if (v > 25) return text(QT_TRANSLATE_NOOP("Economy", "BTC leaning"));
        return text(QT_TRANSLATE_NOOP("Economy", "Bitcoin season"));
    }
    if (v < 25) return text(QT_TRANSLATE_NOOP("Economy", "Extreme fear"));
    if (v < 45) return text(QT_TRANSLATE_NOOP("Economy", "Fear"));
    if (v <= 55) return text(QT_TRANSLATE_NOOP("Economy", "Neutral"));
    if (v <= 75) return text(QT_TRANSLATE_NOOP("Economy", "Greed"));
    return text(QT_TRANSLATE_NOOP("Economy", "Extreme greed"));
}

QColor zoneColor(Zones zones, double v)
{
    if (zones == Zones::AltSeason) {
        if (v >= 75) return QColor(0x7b, 0x8c, 0xf0);
        if (v > 50) return QColor(0xa3, 0xa6, 0xd8);
        if (v > 25) return QColor(0xe8, 0xb5, 0x3c);
        return QColor(0xf0, 0x92, 0x3a);
    }
    if (v < 25) return QColor(0xe0, 0x4f, 0x5a);
    if (v < 45) return QColor(0xe8, 0x84, 0x45);
    if (v <= 55) return QColor(0xe0, 0xc0, 0x45);
    if (v <= 75) return QColor(0x9c, 0xc8, 0x4f);
    return QColor(0x4f, 0xc0, 0x6d);
}

namespace {

QDate sinceFor(const Indicator &i)
{
    int days = 0;
    for (const QString &r : i.ranges) {
        const int d = rangeDays(r);
        if (d == 0)
            return {};   // MAX: everything
        days = std::max(days, d);
    }
    // A little more, so the first point of the longest range has one before it; from January 1st, so the request
    // (and its cached copy) stays the same all year.
    const QDate from = QDate::currentDate().addDays(-days - 120);
    return QDate(from.year(), 1, 1);
}

Series scaled(Series s, double scale)
{
    if (scale != 1)
        for (Point &p : s)
            p.value *= scale;
    return s;
}

} // namespace

void load(const Indicator &i, QObject *ctx, Done done)
{
    auto *client = research::Client::instance();
    switch (i.source) {
    case Source::Fred:
        client->fredSeries(i.code, sinceFor(i), ctx,
                           [done, scale = i.scale](const Series &s, const QString &e) { done(scaled(s, scale), e); });
        return;
    case Source::Oecd:
        client->oecdSeries(i.code, sinceFor(i), ctx, done);
        return;
    case Source::Yahoo:
        client->prices(i.code, "10y", "1d", ctx, done);
        return;
    case Source::CryptoFearGreed:
        client->cryptoFearGreed(ctx, done);
        return;
    case Source::AltcoinSeason:
        client->altcoinSeason(ctx, done);
        return;
    case Source::CryptoCap:
    case Source::BtcDominance:
        client->cryptoMarket(ctx, [done, cap = i.source == Source::CryptoCap](const research::CryptoMarket &m, const QString &e) {
            done(cap ? m.cap : m.btcDominance, e);
        });
        return;
    case Source::StockFearGreed:
        stockFearGreed(ctx, [done](const FearGreed &f, const QString &e) { done(f.index, e); });
        return;
    }
}

// ---------------------------------------------------------------- stock market Fear & Greed

namespace {

// Values on `dates` (the S&P 500's days): the series' last value on or before each date, NaN before its start.
QVector<double> alignTo(const QVector<QDate> &dates, const Series &s)
{
    QVector<double> out(dates.size(), research::NaN);
    int j = 0;
    for (int i = 0; i < dates.size(); ++i) {
        while (j < s.size() && s[j].date <= dates[i])
            ++j;
        if (j > 0)
            out[i] = s[j - 1].value;
    }
    return out;
}

QVector<double> movingAverage(const QVector<double> &v, int n)
{
    QVector<double> out(v.size(), research::NaN);
    double sum = 0;
    int count = 0;
    for (int i = 0; i < v.size(); ++i) {
        if (std::isfinite(v[i])) { sum += v[i]; ++count; }
        if (i >= n && std::isfinite(v[i - n])) { sum -= v[i - n]; --count; }
        if (i >= n - 1 && count == n)
            out[i] = sum / n;
    }
    return out;
}

QVector<double> change(const QVector<double> &v, int n)
{
    QVector<double> out(v.size(), research::NaN);
    for (int i = n; i < v.size(); ++i)
        if (std::isfinite(v[i]) && std::isfinite(v[i - n]) && v[i - n] != 0)
            out[i] = v[i] / v[i - n] - 1;
    return out;
}

// Each day's value ranked among the last `window` days' (itself included): 0 the lowest, 100 the highest.
QVector<double> percentileRank(const QVector<double> &v, int window)
{
    QVector<double> out(v.size(), research::NaN);
    for (int i = 0; i < v.size(); ++i) {
        if (!std::isfinite(v[i]))
            continue;
        int below = 0, equal = 0, n = 0;
        for (int k = std::max(0, i - window + 1); k <= i; ++k) {
            if (!std::isfinite(v[k]))
                continue;
            ++n;
            if (v[k] < v[i]) ++below;
            else if (v[k] == v[i]) ++equal;
        }
        if (n >= window / 2)   // half a year at least
            out[i] = 100.0 * (below + 0.5 * (equal - 1)) / std::max(1, n - 1);
    }
    return out;
}

struct FearGreedJob {
    QHash<QString, Series> series;
    int pending = 0;
    QString error;
};

FearGreed computeFearGreed(const QHash<QString, Series> &in)
{
    const Series &spx = in.value("^GSPC");
    QVector<QDate> dates;
    QVector<double> close;
    for (const Point &p : spx) {
        dates << p.date;
        close << p.value;
    }
    const QVector<double> vix = alignTo(dates, in.value("^VIX")), spy = alignTo(dates, in.value("SPY")),
                          tlt = alignTo(dates, in.value("TLT")), rsp = alignTo(dates, in.value("RSP")),
                          hy = alignTo(dates, in.value("BAMLH0A0HYM2")), ig = alignTo(dates, in.value("BAMLC0A0CM"));
    const int n = dates.size();
    QVector<double> momentum(n, research::NaN), volatility(n, research::NaN), haven(n, research::NaN),
        breadth(n, research::NaN), junk(n, research::NaN);
    const QVector<double> ma125 = movingAverage(close, 125), vixMa50 = movingAverage(vix, 50);
    const QVector<double> spy20 = change(spy, 20), tlt20 = change(tlt, 20), rsp20 = change(rsp, 20);
    const QDate hyStart = in.value("BAMLH0A0HYM2").isEmpty() ? QDate() : in.value("BAMLH0A0HYM2").first().date;
    for (int i = 0; i < n; ++i) {
        if (std::isfinite(ma125[i]) && ma125[i] > 0) momentum[i] = close[i] / ma125[i] - 1;
        if (std::isfinite(vixMa50[i]) && vixMa50[i] > 0 && std::isfinite(vix[i])) volatility[i] = -(vix[i] / vixMa50[i] - 1);
        if (std::isfinite(spy20[i]) && std::isfinite(tlt20[i])) haven[i] = spy20[i] - tlt20[i];
        if (std::isfinite(rsp20[i]) && std::isfinite(spy20[i])) breadth[i] = rsp20[i] - spy20[i];
        if (hyStart.isValid() && dates[i] >= hyStart && std::isfinite(hy[i]) && std::isfinite(ig[i])) junk[i] = -(hy[i] - ig[i]);
    }
    constexpr int Year = 252;
    const QVector<QVector<double>> scores {percentileRank(momentum, Year), percentileRank(volatility, Year),
                                           percentileRank(haven, Year), percentileRank(breadth, Year),
                                           percentileRank(junk, Year)};
    const QStringList names {text(QT_TRANSLATE_NOOP("Economy", "Momentum")), text(QT_TRANSLATE_NOOP("Economy", "Volatility")),
                             text(QT_TRANSLATE_NOOP("Economy", "Safe haven demand")), text(QT_TRANSLATE_NOOP("Economy", "Breadth")),
                             text(QT_TRANSLATE_NOOP("Economy", "Junk bond demand"))};
    FearGreed out;
    for (int i = 0; i < n; ++i) {
        double sum = 0;
        int count = 0;
        for (const QVector<double> &s : scores)
            if (std::isfinite(s[i])) { sum += s[i]; ++count; }
        if (count >= 3)
            out.index.push_back({dates[i], std::round(sum / count)});
    }
    if (n > 0)
        for (int k = 0; k < scores.size(); ++k)
            out.components.push_back({names[k], scores[k][n - 1]});
    return out;
}

} // namespace

void stockFearGreed(QObject *ctx, std::function<void(const FearGreed &, const QString &)> done)
{
    auto job = std::make_shared<FearGreedJob>();
    auto *client = research::Client::instance();
    QPointer<QObject> guard(ctx);
    auto finished = [job, done, guard] {
        if (--job->pending > 0 || !guard)
            return;
        if (job->series.value("^GSPC").isEmpty()) {
            done({}, job->error.isEmpty() ? text(QT_TRANSLATE_NOOP("Economy", "No S&P 500 prices")) : job->error);
            return;
        }
        done(computeFearGreed(job->series), {});
    };
    // The spreads as the Sentiment and Rates panels ask for them (one download for all three).
    Indicator spreadIndicator;
    spreadIndicator.ranges = ShortHistory;
    const QStringList symbols {"^GSPC", "^VIX", "SPY", "TLT", "RSP"};
    const QStringList spreads {"BAMLH0A0HYM2", "BAMLC0A0CM"};
    job->pending = symbols.size() + spreads.size();
    for (const QString &sym : symbols)
        client->prices(sym, "10y", "1d", ctx, [job, sym, finished](const Series &s, const QString &e) {
            job->series[sym] = s;
            if (!e.isEmpty() && job->error.isEmpty()) job->error = e;
            finished();
        });
    for (const QString &id : spreads)
        client->fredSeries(id, sinceFor(spreadIndicator), ctx, [job, id, finished](const Series &s, const QString &e) {
            job->series[id] = s;
            if (!e.isEmpty() && job->error.isEmpty()) job->error = e;
            finished();
        });
}

} // namespace economy
