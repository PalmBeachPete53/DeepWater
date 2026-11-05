#pragma once

#include <QDate>
#include <QDateTime>
#include <QString>
#include <QVector>

// Reading markets from candles: indices of currencies, technical signals, stop pools, seasonality, a composite of risk
// appetite. Plain functions on plain data: every network call and every widget lives elsewhere.
namespace desk {

struct Candle {
    qint64 time = 0;   // seconds since the epoch, UTC
    double open = 0, high = 0, low = 0, close = 0, volume = 0;
};

// --- Currency strength: unweighted indices of the eight majors.

struct CurrencySeries {
    QString currency;
    QVector<qint64> times;
    QVector<double> dollarsPerUnit;   // what one unit is worth in dollars; for USD itself leave empty
};
struct StrengthLine {
    QString currency;
    QVector<double> index;   // 100 at the first bar: the geometric mean of the currency against the seven others
};
struct Strength {
    QVector<qint64> times;   // the bars every currency has
    QVector<StrengthLine> lines;
};
// A currency's index is the mean of the logs of its pairs against all the others, so no pair weighs more than
// another. Only the dollar pairs are needed: EUR/JPY is EUR/USD over JPY/USD.
Strength currencyStrength(const QVector<CurrencySeries> &series);
// % change of each line from bar `from` to the last, strongest first.
QVector<std::pair<QString, double>> rankStrength(const Strength &s, int from = 0);

// --- Technical signals of one market, from daily candles.

enum class Trend { Bullish, Neutral, Bearish };
enum class Momentum { Overbought, Neutral, Oversold };
enum class Volatility { Low, Normal, High };
enum class VolumeState { NoData, Below, Average, Above };
enum class Structure { Bullish, Ranging, Bearish };
enum class LiquidityState { None, PoolAbove, PoolBelow, Near, Swept };

struct Signals {
    bool valid = false;
    double last = 0, changePct = 0;
    Trend trend = Trend::Neutral;
    double trendGapPct = 0;      // EMA 20 against EMA 50
    Momentum momentum = Momentum::Neutral;
    double rsi = 50;
    Volatility volatility = Volatility::Normal;
    double volAnnual = 0;        // 20-day, annualised, %
    double volBaseline = 0;      // the same over the year
    VolumeState volume = VolumeState::NoData;
    double volumeRatio = 0;      // the last 5 days against the 60 before
    Structure structure = Structure::Ranging;
    LiquidityState liquidity = LiquidityState::None;
    double liquidityDistancePct = 0;   // to the nearest untested pool; signed
    // 0 to 100, 50 neutral: trend, momentum, volatility, volume, structure, liquidity.
    double radar[6] = {50, 50, 50, 50, 50, 50};
};
Signals computeSignals(const QVector<Candle> &daily, const QVector<Candle> &volumeProxy = {});
double rsiOf(const QVector<double> &close, int period = 14);
QVector<double> emaOf(const QVector<double> &values, int period);

// --- Stop pools: where stops likely rest, from swing highs and lows.

struct Pool {
    bool buySide = true;   // above the market: stops of the shorts. false: below, stops of the longs
    double price = 0;
    int touches = 0;       // swings that formed the level
    qint64 firstTime = 0, lastTime = 0;
    bool swept = false;
    qint64 sweptTime = 0;
    double distancePct = 0;   // from the last close
};
struct Sweep {
    qint64 time = 0;
    bool buySide = true;
    double price = 0;
    int touches = 0;
};
struct SessionExtreme {
    QDate date;
    double high = 0, low = 0;
};
struct Liquidity {
    QVector<Pool> pools;      // nearest first
    QVector<Sweep> sweeps;    // newest first
    QVector<SessionExtreme> days;   // newest first
    double previousDayHigh = 0, previousDayLow = 0;
    double atr = 0;
    int untested = 0;
    bool stopRunBuySideCloser = false;
    enum Likelihood { Low, Moderate, High } stopRun = Low;
};
// `pivot`: bars on each side of a swing. Swings closer than `tolerance` ATR merge into one pool.
Liquidity findLiquidity(const QVector<Candle> &bars, int pivot = 3, double tolerance = 0.25);

// --- Seasonality from daily closes.

struct SeasonalOptions {
    int years = 20;
    int firstMonth = 1, lastMonth = 12;   // the period, inclusive
    bool excludeOutliers = false;         // leave out the years beyond 1.5 sigma of the period's return
};
struct SeasonalYear {
    int year = 0;
    double periodReturn = 0;   // %
    bool excluded = false;
};
struct Seasonal {
    bool valid = false;
    QVector<double> path;      // mean cumulative return by day of year (366 points, %), smoothed over 3 days
    QVector<SeasonalYear> years;
    int positive = 0, negative = 0;
    double winRate = 0;        // % of the years with a positive period
    double meanReturn = 0;
    double byMonth[12] = {};   // mean monthly return, %
    double byWeekday[5] = {};  // mean daily return, %, Monday first, inside the period
    int bestMonth = 0, worstMonth = 0;   // 0-11
    double volatilityPeriod = 0, volatilityAll = 0, volatilityRatio = 1;   // daily std devs, %
};
Seasonal computeSeasonal(const QVector<Candle> &daily, const SeasonalOptions &options, int currentYear);

// --- Risk appetite: a composite of markets ranked against their own past year.

struct AppetiteComponent {
    QString id, name, note;
    QVector<double> raw;          // the measure, oldest first
    bool invert = false;          // higher means less appetite (the VIX)
    double value = 0;             // the last raw value
    double percentile = 50;       // 0-100, already inverted when it must be
};
struct Appetite {
    bool valid = false;
    QVector<QDate> dates;
    QVector<double> history;      // composite by day
    double now = 50, weekAgo = 50, monthAgo = 50;
    QVector<AppetiteComponent> components;
};
enum class AppetiteZone { ExtremeOff, Off, Neutral, On, ExtremeOn };
AppetiteZone zoneOf(double value);
// The percentile of the last of `values` among the `window` before it.
double percentileOf(const QVector<double> &values, int index, int window = 252);
// `components` hold raw series aligned on `dates`.
Appetite computeAppetite(const QVector<QDate> &dates, QVector<AppetiteComponent> components);

} // namespace desk
