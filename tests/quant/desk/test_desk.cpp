// The Desk and Journal logic (src/desk) on made-up data. Plain checks, a count at the end, non-zero exit on any failure.
#include "desk/Fundamentals.h"
#include "desk/Instruments.h"
#include "desk/Journal.h"
#include "desk/Market.h"
#include "desk/NewsTags.h"
#include "desk/Positioning.h"
#include "desk/Rates.h"
#include "desk/Risk.h"
#include "desk/Sessions.h"

#include <QCoreApplication>
#include <QTimeZone>

#include <cmath>
#include <functional>
#include <iostream>

using namespace desk;

namespace {

int checks = 0, fails = 0;
QString CAT = "misc";

void check(bool ok, const QString &msg)
{
    ++checks;
    if (!ok) {
        ++fails;
        std::cout << "FAIL [" << CAT.toStdString() << "] " << msg.toStdString() << "\n";
    }
}
void near(double want, double got, double tol, const QString &ctx)
{
    check(std::abs(want - got) <= tol, QStringLiteral("%1: want %2, got %3").arg(ctx).arg(want).arg(got));
}
void eqs(const QString &want, const QString &got, const QString &ctx)
{
    check(want == got, QStringLiteral("%1: want \"%2\", got \"%3\"").arg(ctx, want, got));
}
void eqi(qint64 want, qint64 got, const QString &ctx)
{
    check(want == got, QStringLiteral("%1: want %2, got %3").arg(ctx).arg(want).arg(got));
}

void testInstruments()
{
    CAT = "instruments";
    check(instruments().size() >= 100, "about a hundred markets");
    int fx = 0;
    for (const Instrument &i : instruments())
        fx += i.cls == AssetClass::Fx;
    eqi(28, fx, "the 28 pairs of the eight majors");
    for (const char *text : {"EURUSD", "EUR/USD", "eurusd.m", "EURUSDm", "EURUSD_i", "EURUSD=X"}) {
        const Instrument *i = findInstrument(text);
        check(i && i->id == "EURUSD", QString("finds EURUSD from %1").arg(text));
    }
    check(findInstrument("GOLD") && findInstrument("GOLD")->id == "XAUUSD", "gold alias");
    check(findInstrument("XAUUSD.pro") && findInstrument("XAUUSD.pro")->contractSize == 100, "gold is 100 oz a lot");
    check(findInstrument("NAS100") && findInstrument("NAS100")->id == "USTEC", "index alias");
    check(!findInstrument("ZZZQQQ"), "an unknown symbol stays unknown");
    bool direct = false;
    eqs("EURUSD=X", dollarPair("EUR", &direct), "euro pair");
    check(direct, "euro is quoted in dollars");
    eqs("JPY=X", dollarPair("JPY", &direct), "yen pair");
    check(!direct, "the yen pair is yen per dollar");
    const Instrument *usdjpy = findInstrument("USDJPY");
    check(usdjpy && usdjpy->symbol == "JPY=X" && usdjpy->quote == "JPY", "USD/JPY uses Yahoo's symbol, quoted in yen");
}

void testRisk()
{
    CAT = "risk";
    RiskInput in;
    in.balance = 10000;
    in.risk = 1;
    in.entry = 1.12;
    in.stop = 1.115;
    in.target = 1.13;
    RiskResult r = computeRisk(in);
    check(r.valid, "valid");
    check(r.isLong, "a stop under the entry is a long");
    near(0.005, r.stopDistance, 1e-9, "stop distance");
    near(100, r.riskAmount, 1e-9, "1% of 10,000");
    near(20000, r.units, 1e-6, "units");
    near(0.2, r.lots, 1e-9, "lots");
    near(200, r.rewardAmount, 1e-6, "reward");
    near(2, r.rewardRisk, 1e-9, "ratio");
    near(100.0 / 3, r.breakevenWinRate, 1e-9, "breakeven");
    check(gradeOf(r.rewardRisk) == RiskGrade::Acceptable, "2 is acceptable");
    check(gradeOf(0.9) == RiskGrade::Poor && gradeOf(1.2) == RiskGrade::Thin && gradeOf(3) == RiskGrade::Strong, "grades");

    in.riskIsPercent = false;
    in.risk = 50;
    r = computeRisk(in);
    near(50, r.riskAmount, 1e-9, "an amount");
    near(0.5, r.riskPercent, 1e-9, "as a share of the balance");

    // A short on USD/JPY in a dollar account: a yen is worth 1/150 of a dollar.
    in = {};
    in.balance = 10000;
    in.risk = 1;
    in.entry = 150;
    in.stop = 151;
    in.quoteToAccount = 1.0 / 150;
    r = computeRisk(in);
    check(r.valid && !r.isLong, "a stop above is a short");
    near(15000, r.units, 1e-6, "units with the conversion");
    check(!r.hasTarget, "no target");

    in.target = 152;
    check(computeRisk(in).error == "target", "a target on the stop's side is refused");
    in.target = 0;
    in.stop = in.entry;
    check(computeRisk(in).error == "stop", "a stop on the entry is refused");
    in.stop = 151;
    in.balance = 0;
    check(computeRisk(in).error == "balance", "no balance");
    in.balance = 1000;
    in.risk = 0;
    check(computeRisk(in).error == "risk", "no risk");
    in.risk = 1;
    in.quoteToAccount = 0;
    check(computeRisk(in).error == "conversion", "no conversion");
}

void testAmounts()
{
    CAT = "amounts";
    near(1234.56, parseAmount("1,234.56"), 1e-9, "english");
    near(1234.56, parseAmount("1 234,56"), 1e-9, "french");
    near(1234.56, parseAmount("1.234,56"), 1e-9, "german");
    near(1234, parseAmount("1,234"), 1e-9, "thousands");
    near(12.5, parseAmount("12,5"), 1e-9, "decimal comma");
    near(0.125, parseAmount("0,125"), 1e-9, "a decimal after zero");
    near(1234567, parseAmount("1,234,567"), 1e-9, "several commas");
    near(-12.5, parseAmount("-12.50"), 1e-9, "negative");
    near(-12.5, parseAmount("(12.50)"), 1e-9, "accounting");
    near(-3, parseAmount(QString(QChar(0x2212)) + "3"), 1e-9, "the minus sign");
    near(1234, parseAmount("$1,234"), 1e-9, "currency sign");
    check(std::isnan(parseAmount("n/a")) && std::isnan(parseAmount("")), "not a number");
    check(parseTradeTime("2026.01.15 10:30:00").isValid(), "mt time");
    check(parseTradeTime("2026-01-15 10:30").isValid(), "iso-ish");
    eqi(15, parseTradeTime("15/01/2026 10:30:00.123").date().day(), "day first, fractions dropped");
    check(!parseTradeTime("soon").isValid(), "not a time");
}

void testStatements()
{
    CAT = "mt4";
    const QByteArray mt4 = R"(<html><body><table>
<tr><td colspan=14><b>Closed Transactions:</b></td></tr>
<tr><td>Ticket</td><td>Open Time</td><td>Type</td><td>Size</td><td>Item</td><td>Price</td><td>S / L</td><td>T / P</td><td>Close Time</td><td>Price</td><td>Commission</td><td>Taxes</td><td>Swap</td><td>Profit</td></tr>
<tr><td>1001</td><td>2026.03.02 09:00:00</td><td>buy</td><td>0.50</td><td>eurusd</td><td>1.08000</td><td>1.07500</td><td>1.09000</td><td>2026.03.02 15:00:00</td><td>1.08500</td><td>-3.50</td><td>0.00</td><td>-0.40</td><td>250.00</td></tr>
<tr><td>1002</td><td>2026.03.03 09:00:00</td><td>sell</td><td>1.00</td><td>gbpusd</td><td>1.27000</td><td>0.00000</td><td>0.00000</td><td>2026.03.04 09:00:00</td><td>1.27300</td><td>-7.00</td><td>0.00</td><td>0.00</td><td>-300.00</td></tr>
<tr><td>1003</td><td>2026.03.01 00:00:00</td><td>balance</td><td colspan=11>Deposit</td><td>10 000.00</td></tr>
</table></body></html>)";
    ImportResult r = readStatement(mt4);
    eqs("MetaTrader 4 HTML", r.format, "format");
    eqi(2, r.trades.size(), "two trades, the deposit left out");
    if (r.trades.size() == 2) {
        eqs("EURUSD", r.trades[0].symbol, "symbol");
        check(r.trades[0].isLong && !r.trades[1].isLong, "sides");
        near(246.1, r.trades[0].net(), 1e-9, "net with commission and swap");
        near(6, r.trades[0].holdHours(), 1e-9, "held");
        near(0.5, r.trades[0].volume, 1e-9, "volume");
    }

    CAT = "mt5";
    const QString mt5Text = R"(<html><body><table>
<tr><td colspan="13">Trade History Report</td></tr>
<tr><th colspan="13"><b>Positions</b></th></tr>
<tr><td>Time</td><td>Position</td><td>Symbol</td><td>Type</td><td>Volume</td><td>Price</td><td>S / L</td><td>T / P</td><td>Time</td><td>Price</td><td>Commission</td><td>Swap</td><td>Profit</td></tr>
<tr><td>2026.04.01 08:00:00</td><td>555</td><td>XAUUSD</td><td>buy</td><td>0.10</td><td>2300.00</td><td></td><td></td><td>2026.04.01 12:00:00</td><td>2310.00</td><td>-1.00</td><td>0.00</td><td>100.00</td></tr>
<tr><th colspan="13"><b>Orders</b></th></tr>
<tr><td>2026.04.01 08:00:00</td><td>777</td><td>XAUUSD</td><td>buy</td><td>0.10</td><td>0.10</td><td>2300.00</td><td></td><td></td><td>2026.04.01 08:00:00</td><td>filled</td><td></td><td></td></tr>
</table></body></html>)";
    // MT5 writes UTF-16 with a BOM.
    QByteArray utf16("\xFF\xFE");
    utf16.append(reinterpret_cast<const char *>(mt5Text.utf16()), mt5Text.size() * 2);
    r = readStatement(utf16);
    eqs("MetaTrader 5 HTML", r.format, "format");
    eqi(1, r.trades.size(), "one position, the orders left out");
    if (!r.trades.isEmpty()) {
        eqs("555", r.trades[0].id, "ticket");
        near(99, r.trades[0].net(), 1e-9, "net");
        near(2310, r.trades[0].closePrice, 1e-9, "the second price column is the close");
    }

    CAT = "ctrader";
    const QByteArray ct = "Symbol,Opening Direction,Opening Time,Closing Time,Entry Price,Closing Price,Closing Quantity,Commission,Swap,Net USD,Balance USD\n"
                          "EURUSD,Buy,15/05/2026 09:00:00.100,15/05/2026 10:00:00.200,\"1,09000\",\"1,09100\",\"1,00\",-5,0,95,10095\n"
                          "GBPUSD,Sell,16/05/2026 09:00:00.100,16/05/2026 11:30:00.000,1.27,1.272,0.5,-2.5,0,-100,9995\n";
    r = readStatement(ct);
    eqs("cTrader CSV", r.format, "format");
    eqi(2, r.trades.size(), "rows");
    if (r.trades.size() == 2) {
        check(r.trades[0].isLong && !r.trades[1].isLong, "directions");
        near(95 - 5, r.trades[0].net(), 1e-9, "net");
        near(1.09, r.trades[0].openPrice, 1e-9, "decimal comma in quotes");
        check(r.trades[0].id != r.trades[1].id, "generated ids differ");
    }

    CAT = "csv";
    const QByteArray semi = "Instrument;Side;Lots;Open Date;Close Date;Entry;Exit;P/L\nUS500;short;2;2026-06-01 14:00;2026-06-01 16:00;5000;4990;20\nbad;row\n";
    r = readStatement(semi);
    eqi(1, r.trades.size(), "semicolons, column names detected");
    eqi(1, r.skipped, "the broken row is counted");
    check(r.mapping.value(int(TradeField::Profit), -1) == 7, "P/L is the profit column");
    r = readStatement("a,b,c\n1,2,3\n");
    eqs("columns", r.error, "columns not found");
    r = readStatement("");
    eqs("empty", r.error, "empty file");
    r = readStatement("<html><table><tr><td>nothing</td></tr></table></html>");
    eqs("format", r.error, "html without trades");

    CAT = "merge";
    Trade a;
    a.id = "1";
    a.strategy = "breakout";
    a.profit = 10;
    Trade b = a;
    b.strategy.clear();
    b.profit = 20;
    Trade c;
    c.id = "2";
    int added = 0, updated = 0;
    const QVector<Trade> merged = mergeTrades({a}, {b, c}, &added, &updated);
    eqi(2, merged.size(), "merged");
    eqi(1, added, "added");
    eqi(1, updated, "updated");
    near(20, merged[0].profit, 1e-9, "the new figure wins");
    eqs("breakout", merged[0].strategy, "the journal's strategy survives");

    Trade j;
    j.id = "x";
    j.symbol = "EURUSD";
    j.openTime = QDateTime(QDate(2026, 1, 2), QTime(3, 4), QTimeZone::UTC);
    j.strategy = "s";
    const Trade back = Trade::fromJson(j.toJson());
    check(back.id == "x" && back.openTime == j.openTime && !back.isClosed() && back.strategy == "s", "json round trip keeps an open trade open");
}

Trade closedTrade(const QString &id, const QDateTime &close, double profit, const QString &symbol = "EURUSD", bool isLong = true)
{
    Trade t;
    t.id = id;
    t.symbol = symbol;
    t.isLong = isLong;
    t.openTime = close.addSecs(-3600);
    t.closeTime = close;
    t.profit = profit;
    return t;
}

void testStats()
{
    CAT = "stats";
    const QDateTime d0(QDate(2026, 1, 5), QTime(12, 0), QTimeZone::UTC);   // a Monday
    QVector<Trade> trades {closedTrade("a", d0, 100), closedTrade("b", d0.addDays(1), 50), closedTrade("c", d0.addDays(2), -60),
                           closedTrade("d", d0.addDays(3), -40), closedTrade("e", d0.addDays(4), -20), closedTrade("f", d0.addDays(31), 200, "XAUUSD", false)};
    Trade open = closedTrade("g", d0, 999);
    open.closeTime = QDateTime();
    trades << open;
    JournalStats s = computeStats(trades, 1000);
    eqi(6, s.closed, "the open trade doesn't count");
    eqi(3, s.wins, "wins");
    eqi(3, s.losses, "losses");
    near(230, s.net, 1e-9, "net");
    near(50, s.winRate, 1e-9, "win rate");
    near(350.0 / 120, s.profitFactor, 1e-9, "profit factor");
    near(350.0 / 3, s.avgWin, 1e-9, "average win");
    near(40, s.avgLoss, 1e-9, "average loss");
    near(230.0 / 6, s.expectancy, 1e-9, "expectancy");
    near(120, s.maxDrawdown, 1e-9, "max drawdown");
    near(120.0 / 1150 * 100, s.maxDrawdownPct, 1e-9, "as a share of the balance at the peak");
    near(230.0 / 120, s.recoveryFactor, 1e-9, "recovery");
    eqi(2, s.longestWinStreak, "win streak");
    eqi(3, s.longestLossStreak, "loss streak");
    eqi(1, s.currentStreak, "current streak");
    near(1, s.avgHoldHours, 1e-9, "held an hour");
    eqi(2, s.byMonth.size(), "months");
    eqi(1, s.byQuarter.size(), "quarter");
    near(100, s.byWeekday[0].net, 1e-9, "Monday");
    eqs("XAUUSD", s.bySymbol.first().label, "the best symbol first");
    near(200, s.best.first().net(), 1e-9, "best trade");
    near(-60, s.worst.first().net(), 1e-9, "worst trade");
    eqi(3, s.best.size(), "only winners among the best");
    check(s.score >= 0 && s.score <= 100, "score in range");
    eqi(6, s.radar.size(), "six measures");
    near(230, s.equity.last().cumulative, 1e-9, "equity ends at the net");
    check(computeStats({}, 1000).closed == 0, "no trades, no crash");

    Filter f;
    f.side = -1;
    eqi(1, applyFilter(trades, f).size(), "short filter");
    f = {};
    f.symbol = "EURUSD";
    eqi(6, applyFilter(trades, f).size(), "symbol filter keeps the open one");
    f = {};
    f.year = 2025;
    eqi(0, applyFilter(trades, f).size(), "year filter");
}

void testStrength()
{
    CAT = "strength";
    const QVector<qint64> times {100, 200, 300};
    CurrencySeries usd {"USD", {}, {}};
    CurrencySeries eur {"EUR", times, {1.0, 1.1, 1.21}};   // the euro gains 10% a bar against the dollar
    CurrencySeries jpy {"JPY", {100, 200, 250, 300}, {0.01, 0.01, 0.01, 0.01}};   // an extra bar only the yen has
    Strength s = currencyStrength({usd, eur, jpy});
    eqi(3, s.times.size(), "the bars all have");
    eqi(3, s.lines.size(), "three lines");
    near(100, s.lines[0].index.first(), 1e-9, "starts at 100");
    // EUR gained 21% against two currencies: its index is the mean of the logs against both, so +21%.
    near(121, s.lines[1].index.last(), 1e-6, "euro index");
    // USD lost 21% against EUR and nothing against JPY.
    near(100 * std::exp(std::log(1 / 1.21) / 2), s.lines[0].index.last(), 1e-6, "dollar index");
    near(s.lines[0].index.last(), s.lines[2].index.last(), 1e-6, "yen moves with the dollar here");
    const auto rank = rankStrength(s);
    eqs("EUR", rank.first().first, "strongest first");
    near(21, rank.first().second, 1e-6, "its change");
    check(currencyStrength({usd}).lines.isEmpty(), "one currency makes no index");
}

QVector<Candle> series(int n, const std::function<double(int)> &price, double volume = 1000)
{
    QVector<Candle> out;
    const qint64 t0 = QDateTime(QDate(2024, 1, 1), QTime(0, 0), QTimeZone::UTC).toSecsSinceEpoch();
    for (int i = 0; i < n; ++i) {
        const double c = price(i), o = i ? price(i - 1) : c;
        out.push_back({t0 + i * 86400, o, std::max(o, c) + 0.1, std::min(o, c) - 0.1, c, volume});
    }
    return out;
}

void testSignals()
{
    CAT = "signals";
    near(100, rsiOf({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}), 1e-9, "only gains");
    near(50, rsiOf({1, 2}), 1e-9, "too few closes");
    const Signals up = computeSignals(series(200, [](int i) { return 100 + i * 0.5 + std::sin(i) * 0.3; }));
    check(up.valid, "enough candles");
    check(up.trend == Trend::Bullish, "a rising market is bullish");
    check(up.momentum == Momentum::Overbought, "and overbought");
    const Signals down = computeSignals(series(200, [](int i) { return 300 - i * 0.5 + std::sin(i) * 0.3; }));
    check(down.trend == Trend::Bearish && down.momentum == Momentum::Oversold, "a falling market");
    check(!computeSignals(series(30, [](int i) { return 100.0 + i; })).valid, "too few candles");
    const Signals flat = computeSignals(series(200, [](int i) { return 100 + std::sin(i * 0.7) * 2; }));
    check(flat.trend == Trend::Neutral, "no trend in a range");
    check(flat.volume == VolumeState::Average, "steady volume is average");
    QVector<Candle> burst = series(200, [](int i) { return 100 + std::sin(i * 0.7) * 2; });
    for (int i = 195; i < 200; ++i)
        burst[i].volume = 3000;
    check(computeSignals(burst).volume == VolumeState::Above, "a burst of volume");
    QVector<Candle> none = series(200, [](int i) { return 100.0 + i; }, 0);
    check(computeSignals(none).volume == VolumeState::NoData, "no volume at all");
    check(computeSignals(none, series(200, [](int) { return 1.0; })).volume == VolumeState::Average, "a fund's volume stands in");
    for (double r : up.radar)
        check(r >= 0 && r <= 100, "radar in range");
}

void testLiquidity()
{
    CAT = "liquidity";
    // A range with equal highs near 110 and equal lows near 90, ending in the middle.
    QVector<Candle> bars;
    const double path[] = {100, 104, 108, 110, 108, 104, 100, 96, 92, 90, 92, 96, 100, 104, 108, 110, 108, 104, 100, 96, 92, 90, 92, 96, 100, 102, 100};
    qint64 t = 1000000;
    for (double c : path) {
        bars.push_back({t, c, c + 0.2, c - 0.2, c, 100});
        t += 86400;
    }
    Liquidity l = findLiquidity(bars, 2);
    check(!l.pools.isEmpty(), "finds pools");
    int buy = 0, sell = 0, doubleTouch = 0;
    for (const Pool &p : l.pools) {
        (p.buySide ? buy : sell)++;
        if (p.touches == 2)
            ++doubleTouch;
    }
    check(buy >= 1 && sell >= 1, "both sides");
    eqi(2, doubleTouch, "equal highs and equal lows each merge into one pool");
    eqi(l.pools.size(), l.untested, "nothing swept yet");
    eqi(0, l.sweeps.size(), "no sweep");
    near(110.2, l.pools[0].buySide ? l.pools[0].price : l.pools[1].price, 1e-9, "the pool's level");

    // A wick through the highs sweeps the pool.
    bars.push_back({t, 100, 112, 99, 100, 100});
    l = findLiquidity(bars, 2);
    check(!l.sweeps.isEmpty() && l.sweeps[0].buySide, "the pool above is swept");
    eqi(2, l.sweeps[0].touches, "with its touches");
    check(l.untested < int(l.pools.size()), "fewer untested");
    check(l.pools.last().swept, "swept pools come last");
    check(l.previousDayHigh > 0 && l.previousDayLow > 0, "previous day's extremes");
    check(findLiquidity({}, 2).pools.isEmpty(), "no bars");
}

void testSeasonal()
{
    CAT = "seasonal";
    // 5 full years, gains in the first half of each year and losses in the second, except that 2021 gains far more.
    QVector<Candle> bars;
    double price = 100;
    for (QDate d(2020, 1, 1); d < QDate(2026, 1, 1); d = d.addDays(1)) {
        if (d.dayOfWeek() > 5)
            continue;
        double r = d.month() <= 6 ? 0.1 : -0.1;
        if (d.year() == 2021 && d.month() <= 6)
            r = 1.0;
        price *= 1 + r / 100.0;
        bars.push_back({QDateTime(d, QTime(12, 0), QTimeZone::UTC).toSecsSinceEpoch(), price, price, price, price, 0});
    }
    SeasonalOptions o;
    o.years = 20;
    Seasonal s = computeSeasonal(bars, o, 2026);
    check(s.valid, "valid");
    eqi(366, s.path.size(), "a path over the year");
    check(s.byMonth[0] > 0 && s.byMonth[11] < 0, "months' signs");
    check(s.bestMonth <= 5 && s.worstMonth >= 6, "best and worst months");
    o.firstMonth = 1;
    o.lastMonth = 6;
    s = computeSeasonal(bars, o, 2026);
    near(100, s.winRate, 1e-9, "the first half always gains");
    eqi(6, s.positive, "six years");
    o.excludeOutliers = true;
    s = computeSeasonal(bars, o, 2026);
    int excluded = 0;
    for (const SeasonalYear &y : s.years)
        excluded += y.excluded && y.year == 2021;
    eqi(1, excluded, "the wild year is left out");
    eqi(5, s.positive, "five years remain");
    o = {};
    o.firstMonth = 7;
    o.lastMonth = 12;
    s = computeSeasonal(bars, o, 2026);
    near(0, s.winRate, 1e-9, "the second half never gains");
    check(s.byWeekday[0] < 0, "weekday returns inside the period");
    check(!computeSeasonal({}, {}, 2026).valid, "no data");
}

void testAppetite()
{
    CAT = "appetite";
    near(100, percentileOf({1, 2, 3, 4}, 3), 1e-9, "the highest");
    near(0, percentileOf({4, 3, 2, 1}, 3), 1e-9, "the lowest");
    QVector<QDate> dates;
    QVector<double> rising, vix, withGaps;
    for (int i = 0; i < 300; ++i) {
        dates << QDate(2025, 1, 1).addDays(i);
        rising << double(i);
        vix << 30.0 - i * 0.05;
        withGaps << (i % 10 == 0 ? std::nan("") : double(i));
    }
    AppetiteComponent a {"eq", "Equity", {}, rising};
    AppetiteComponent b {"vix", "VIX", {}, vix, true};
    Appetite x = computeAppetite(dates, {a, b});
    check(x.valid, "valid");
    near(100, x.now, 1e-9, "rising stocks and a falling VIX: full appetite");
    eqi(2, x.components.size(), "components");
    near(100, x.components[1].percentile, 1e-9, "the VIX is inverted");
    near(vix.last(), x.components[1].value, 1e-9, "its last value");
    AppetiteComponent shortOne {"short", "Short", {}, {1, 2, 3}};
    check(computeAppetite(dates, {a, shortOne}).components.size() == 1, "a misaligned series is left out");
    AppetiteComponent gappy {"gap", "Gaps", {}, withGaps};
    check(computeAppetite(dates, {gappy}).valid, "gaps don't break it");
    check(zoneOf(10) == AppetiteZone::ExtremeOff && zoneOf(35) == AppetiteZone::Off && zoneOf(50) == AppetiteZone::Neutral
              && zoneOf(70) == AppetiteZone::On && zoneOf(90) == AppetiteZone::ExtremeOn, "zones");
}

void testFundamentals()
{
    CAT = "fundamentals";
    Macro us;
    us.currency = "USD";
    us.policyRate = 4;
    us.policyChange = 0.25;
    us.cpi = 3.7;
    us.gdp = 2.2;
    us.unemployment = 4.2;
    us.unemploymentPrevious = 4.1;
    us.pmi = 57.6;
    us.confidence = 91.7;
    us.confidencePrevious = 80;
    Score s = scoreCurrency(us);
    eqi(6, s.covered, "six themes");
    eqs("hike", s.cells[int(FundTheme::Rates)].key, "rates");
    eqs("above_target", s.cells[int(FundTheme::Inflation)].key, "inflation");
    eqs("softening", s.cells[int(FundTheme::Labour)].key, "labour");
    check(s.cells[int(FundTheme::Labour)].reading == Reading::Weak, "a rising unemployment rate is weak");
    near(0.1, s.cells[int(FundTheme::Labour)].change, 1e-9, "its change");
    eqi(67, s.score, "five strong, one weak");
    check(s.bias == Score::Bullish, "bullish");

    Macro cad;
    cad.currency = "CAD";
    cad.policyRate = 2.25;
    cad.policyChange = -0.25;
    cad.gdp = -0.1;
    Score c = scoreCurrency(cad);
    eqi(2, c.covered, "only two themes");
    eqi(-100, c.score, "both weak");
    check(c.bias == Score::Bearish, "bearish");
    check(c.cells[int(FundTheme::Activity)].reading == Reading::None, "no PMI, no reading");
    eqi(0, scoreCurrency({}).covered, "no data");

    const QVector<Score> all = scoreAll({cad, us});
    eqs("USD", all.first().currency, "strongest first");
    const PairVerdict v = comparePair(s, c);
    eqs("USD", v.favoured, "the verdict");
    eqi(2, v.compared, "only shared themes compared");
    eqi(2, v.leadsA, "USD leads both");
    check(comparePair(s, s).favoured.isEmpty(), "a tie favours nobody");
}

void testSessions()
{
    CAT = "sessions";
    const FxSession &london = fxSessions()[2];
    eqs("london", london.id, "order");
    // Tuesday 27 Oct 2026, 16:51 UTC: London closes at 17:00 local (17:00 UTC, BST ended on the 25th).
    const QDateTime now(QDate(2026, 10, 27), QTime(16, 51), QTimeZone::UTC);
    SessionState st = sessionState(london, now);
    check(st.open, "London is open");
    eqi(9 * 60, now.secsTo(st.nextClose), "closes in nine minutes");
    check(st.progress > 0.9 && st.progress < 1, "almost done");
    const QStringList open = openSessions(now);
    check(open.contains("london") && open.contains("newyork") && !open.contains("tokyo"), "London and New York overlap");
    // In summer London opens an hour earlier in UTC: daylight saving.
    QDateTime o, c;
    sessionWindow(london, QDate(2026, 7, 7), &o, &c);
    eqi(7, o.time().hour(), "summer: 07:00 UTC");
    sessionWindow(london, QDate(2026, 1, 6), &o, &c);
    eqi(8, o.time().hour(), "winter: 08:00 UTC");
    sessionWindow(london, QDate(2026, 10, 31), &o, &c);
    check(!o.isValid(), "no session on a Saturday");
    const QDateTime saturday(QDate(2026, 10, 31), QTime(12, 0), QTimeZone::UTC);
    st = sessionState(london, saturday);
    check(!st.open, "closed at the weekend");
    eqi(1, st.nextOpen.date().dayOfWeek(), "reopens on Monday");
    check(openSessions(saturday).isEmpty(), "nothing open at the weekend");
    const QVector<RecapWindow> w = recapWindows(now, 3);
    check(!w.isEmpty() && w.first().id == "newyork" && w.first().running, "the running session comes first");
    for (const RecapWindow &r : w)
        check(r.start <= now && r.start < r.end, "windows have started");
}

void testNews()
{
    CAT = "news";
    NewsTags t = tagHeadline("Fed expected to hold rates steady, CPI in focus");
    check(t.topics.contains("central_bank") && t.topics.contains("inflation"), "topics");
    check(t.impact == NewsImpact::High, "CPI is high impact");
    t = tagHeadline("UK retail sales fall more than expected");
    check(t.topics.contains("retail") && t.impact == NewsImpact::Important, "retail");
    t = tagHeadline("Nvidia Q3 results beat estimates");
    check(t.topics.contains("earnings") && t.impact == NewsImpact::Normal, "a company's results alone");
    t = tagHeadline("A quiet day for gardening");
    check(t.topics.isEmpty() && t.impact == NewsImpact::Normal, "no topic");
    check(tagHeadline("Prefed").topics.isEmpty(), "whole words only");
}

void testRates()
{
    CAT = "rates";
    const QString html = R"(<h4><a id="1">2026 FOMC Meetings</a></h4>
<div class="row fomc-meeting"><div class="fomc-meeting__month col"><strong>October</strong></div><div class="fomc-meeting__date col">27-28</div></div>
<div class="row fomc-meeting"><div class="fomc-meeting__month col"><strong>Dec</strong></div><div class="fomc-meeting__date col">8-9*</div></div>
<h4><a id="2">2025 FOMC Meetings</a></h4>
<div class="row fomc-meeting"><div class="fomc-meeting__month col"><strong>Apr/May</strong></div><div class="fomc-meeting__date col">30-1</div></div>
<div class="row fomc-meeting"><div class="fomc-meeting__month col"><strong>August</strong></div><div class="fomc-meeting__date col">22 (notation vote)</div></div>)";
    const QVector<QDate> meetings = parseFomcCalendar(html);
    eqi(3, meetings.size(), "three scheduled meetings, the notation vote left out");
    if (meetings.size() == 3) {
        check(meetings[0] == QDate(2025, 5, 1), "a meeting across two months ends in the second");
        check(meetings[2] == QDate(2026, 12, 9), "a starred date");
    }
    eqs("ZQZ26.CBT", fedFutureSymbol(QDate(2026, 12, 1)), "december");
    eqs("ZQF27.CBT", fedFutureSymbol(QDate(2027, 1, 15)), "january");

    // 4.00% until 16 Dec, then 25 bp higher priced in full: December's average is (15 x 4 + 16 x 4.25) / 31.
    const double dec = (15 * 4.0 + 16 * 4.25) / 31;
    FedPath path = fedPath({{QDate(2026, 11, 1), 96.0}, {QDate(2026, 12, 1), 100 - dec}, {QDate(2027, 1, 1), 95.75}, {QDate(2027, 2, 1), 95.75}},
                           {QDate(2026, 10, 28), QDate(2026, 12, 15), QDate(2027, 1, 27)}, QDate(2026, 11, 5));
    near(4, path.currentRate, 1e-9, "the rate today");
    eqi(2, path.meetings.size(), "past meetings left out");
    near(25, path.meetings[0].changeBp, 0.5, "a full hike in December");
    near(100, path.meetings[0].pHike, 2, "priced in full");
    near(0, path.meetings[1].changeBp, 1, "nothing more in January");
    near(25, path.meetings[1].cumulativeBp, 1, "cumulative");
    near(100, path.meetings[1].pHold, 5, "a hold after that");

    // Half a step: 50% hold, 50% +25.
    FedPath half = fedPath({{QDate(2026, 11, 1), 96.0}, {QDate(2026, 12, 1), 96.0 - 0.125 * 16 / 31}, {QDate(2027, 1, 1), 95.875}},
                           {QDate(2026, 12, 15)}, QDate(2026, 11, 5));
    near(50, half.meetings[0].pHike, 1, "half a hike");
    near(50, half.meetings[0].pHold, 1, "half a hold");
    FedPath cut = fedPath({{QDate(2026, 11, 1), 96.0}, {QDate(2026, 12, 1), 96.25}, {QDate(2027, 1, 1), 96.25}}, {QDate(2026, 12, 25)}, QDate(2026, 11, 5));
    near(100, cut.meetings[0].pCut, 1, "a cut, from the next month's contract late in the month");
    check(fedPath({}, meetings, QDate(2026, 1, 1)).meetings.isEmpty(), "no contract, no path");

    near(100, rangePosition({1, 2, 3}), 1e-9, "top of the range");
    near(0, rangePosition({3, 2, 1}), 1e-9, "bottom of the range");
    near(50, rangePosition({2, 2}), 1e-9, "flat");
    near(-1, rangePosition({}), 1e-9, "no data");
    near(100, rangePosition({100, 1, 2}, 2), 1e-9, "only the window counts");

    const QVector<TffWeek> tff = parseTff(R"([{"report_date_as_yyyy_mm_dd":"2026-10-27T00:00:00.000","open_interest_all":"100","lev_money_positions_long":"7","lev_money_positions_short":"3"},
                                              {"report_date_as_yyyy_mm_dd":"2026-10-20T00:00:00.000","open_interest_all":"90"},{"nothing":"1"}])");
    eqi(2, tff.size(), "rows with a date");
    check(tff.first().date == QDate(2026, 10, 20), "oldest first");
    eqi(7, tff.last().levLong, "a field");
}

void testPositioning()
{
    CAT = "positioning";
    Outlook o = parseOutlook(R"({"error":false,"message":"","symbols":[
        {"name":"EURUSD","shortPercentage":65,"longPercentage":35,"shortVolume":1500.5,"longVolume":800,"longPositions":100,"shortPositions":220,"totalPositions":320,"avgShortPrice":1.1,"avgLongPrice":1.2},
        {"name":"","shortPercentage":50,"longPercentage":50},{"name":"GBPUSD","shortPercentage":0,"longPercentage":0}],"general":{}})");
    check(o.ok, "an answer");
    eqi(1, o.rows.size(), "rows without a name or a share are left out");
    eqs("EURUSD", o.rows.first().symbol, "the symbol");
    eqi(220, o.rows.first().shortPositions, "positions");
    o = parseOutlook(R"({"outlook":[{"symbol":"USDJPY","longPercent":"71.5","shortPercent":"28.5","longVolumeLots":10}]})");
    check(o.ok && o.rows.first().longPct == 71.5, "the other names, and numbers as strings");
    check(o.rows.first().longVolume == 10, "lots");
    o = parseOutlook(R"({"error":true,"message":"Invalid session."})");
    check(!o.ok && o.sessionInvalid, "an ended session is told apart");
    o = parseOutlook(R"({"error":true,"message":"Too many requests."})");
    check(!o.ok && !o.sessionInvalid && o.message == "Too many requests.", "another refusal keeps the session");
    check(!parseOutlook("<html>").ok, "not JSON");
    check(!parseOutlook(R"({"error":false,"symbols":[]})").ok, "nothing in it");
    QString message;
    eqs("abc", parseLogin(R"({"error":false,"message":"","session":"abc"})", &message), "a session");
    check(parseLogin(R"({"error":true,"message":"Invalid email or password."})", &message).isEmpty(), "refused");
    eqs("Invalid email or password.", message, "says why");
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    testInstruments();
    testRisk();
    testAmounts();
    testStatements();
    testStats();
    testStrength();
    testSignals();
    testLiquidity();
    testSeasonal();
    testAppetite();
    testFundamentals();
    testSessions();
    testNews();
    testRates();
    testPositioning();
    std::cout << checks << " checks, " << fails << " failed\n";
    return fails ? 1 : 0;
}
