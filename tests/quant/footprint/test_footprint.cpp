// qtm_test_footprint: the footprint aggregation (core/Footprint.*) on small hand-made sets of trades. Every
// expected number is worked out in the comments next to it. Same style as the other test programs: plain
// checks, a count at the end, non-zero exit on any failure.
#include "core/Footprint.h"

#include <QString>

#include <algorithm>
#include <iostream>
#include <random>

using namespace qtm;
using namespace qtm::footprint;

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
template <typename T>
void eq(T want, T got, const QString &ctx)
{
    check(want == got, QStringLiteral("%1: want %2, got %3").arg(ctx).arg(want).arg(got));
}
void eqSide(Side want, Side got, const QString &ctx)
{
    eq(int(want), int(got), ctx);
}
void near(double want, double got, const QString &ctx)
{
    check(std::fabs(want - got) <= 1e-9 * std::max(1.0, std::fabs(want)),
          QStringLiteral("%1: want %2, got %3").arg(ctx).arg(want, 0, 'g', 17).arg(got, 0, 'g', 17));
}

Trade tr(double price, double qty = 1, Side side = Side::Unknown, double bid = NaN, double ask = NaN)
{
    static int64_t seq = 0;
    Trade t;
    t.time = double(++seq);
    t.seq = seq;
    t.price = price;
    t.qty = qty;
    t.side = side;
    t.bid = bid;
    t.ask = ask;
    return t;
}

// A row's volumes straight into a candle (one tick per row): `bid` sold, `ask` bought.
void put(CandleFootprint &c, int64_t row, double bid, double ask)
{
    static int64_t seq = 0;
    if (bid > 0) c.add({double(++seq), seq, row, bid, Side::Sell, false}, row);
    if (ask > 0) c.add({double(++seq), seq, row, ask, Side::Buy, false}, row);
}

// ---------------------------------------------------------------- ticks and rows

void testTicks()
{
    CAT = "ticks";
    eq<int64_t>(3, TickGrid{0.1, 1}.toTick(0.1 + 0.2), "0.1 + 0.2 at 0.1 (0.30000000000000004 → 3)");
    eq<int64_t>(17203, TickGrid{0.25, 1}.toTick(4300.75), "4300.75 at 0.25");
    eq<int64_t>(-5, TickGrid{0.25, 1}.toTick(-1.25), "-1.25 at 0.25");
    eq<int64_t>(1234, TickGrid{1e-8, 1}.toTick(0.00001234), "0.00001234 at 1e-8");
    eq<int64_t>(6800012, TickGrid{0.01, 1}.toTick(68000.12), "68000.12 at 0.01");
    // A price between ticks (a feed rounding error) goes to the nearest one.
    eq<int64_t>(17203, TickGrid{0.25, 1}.toTick(4300.7500001), "4300.7500001 at 0.25");

    eq<int64_t>(-2, floorDiv(-5, 4), "floorDiv(-5, 4)");
    eq<int64_t>(-1, floorDiv(-4, 4), "floorDiv(-4, 4)");
    eq<int64_t>(1, floorDiv(7, 4), "floorDiv(7, 4)");
    eq<int64_t>(0, floorDiv(0, 4), "floorDiv(0, 4)");

    // 4 ticks of 0.25 = $1 rows: 4300.75 (tick 17203) is in row 4300, which runs from 4300.00 to 4301.00.
    const TickGrid g{0.25, 4};
    eq<int64_t>(4300, g.rowOf(4300.75), "4300.75 in 4-tick rows");
    eq<int64_t>(4300, g.rowOf(4300.00), "4300.00 in 4-tick rows");
    eq<int64_t>(4301, g.rowOf(4301.00), "4301.00 in 4-tick rows");
    near(4300.0, g.rowLow(4300), "row 4300's bottom");
    near(1.0, g.rowHeight(), "row height");
    // Negative prices (spreads): -1.25 is tick -5, row floor(-5 / 4) = -2, which runs from -2.00 to -1.00.
    eq<int64_t>(-2, g.rowOf(-1.25), "-1.25 in 4-tick rows");
    near(-2.0, g.rowLow(-2), "row -2's bottom");

    // Auto grouping: median range 50 over 24 rows at a 0.01 tick = 208.3 ticks → 200 (nearest 1-2-5 step).
    Bars bars;
    for (double r : {50.0, 60.0, 40.0, 55.0, 45.0}) bars.push_back({0, 100, 100 + r, 100, 100, 1, {}});
    eq(200, autoRowTicks(bars, 0.01, 24), "auto rows, median range 50");
    // 400 ticks is nearer 500 than 200 on a log scale (log 400/200 = 0.69 > log 500/400 = 0.22).
    bars = {{0, 100, 196, 100, 100, 1, {}}};
    eq(500, autoRowTicks(bars, 0.01, 24), "auto rows, range 96 → 400 ticks");
    eq(1, autoRowTicks({}, 0.01, 24), "auto rows without bars");
    bars = {{0, 100, 100.05, 100, 100, 1, {}}};
    eq(1, autoRowTicks(bars, 0.01, 24), "auto rows, range below one tick per row");
}

// ---------------------------------------------------------------- side classification

void testClassifyFeed()
{
    CAT = "classify/feed";
    // Binance aggTrade: m = buyer is maker, so the seller hit the bid.
    eqSide(Side::Sell, binanceSide(true), "m = true");
    eqSide(Side::Buy, binanceSide(false), "m = false");
    TradeClassifier c(ClassifyMode::Aggressor, 0.5);
    Classified r = c.classify(tr(100, 1, Side::Sell));
    eqSide(Side::Sell, r.side, "feed sell");
    check(!r.approximate, "feed side is exact");
    r = c.classify(tr(99, 1, Side::Buy));   // down tick, but the feed says buy: the feed wins
    eqSide(Side::Buy, r.side, "feed buy on a down tick");
    // A trade without a side falls back: no quote, so the tick rule (99 → 99.5 is up).
    r = c.classify(tr(99.5));
    eqSide(Side::Buy, r.side, "feed without side → tick rule");
    check(r.approximate, "fallback is approximate");
}

void testClassifyQuote()
{
    CAT = "classify/quote";
    // bid 99.5 / ask 100.5 (mid 100) at a 0.25 tick.
    TradeClassifier c(ClassifyMode::Quote, 0.25);
    auto at = [&](double price) { return c.classify(tr(price, 1, Side::Unknown, 99.5, 100.5)); };
    Classified r = at(100.5);
    eqSide(Side::Buy, r.side, "at the ask");
    check(r.approximate, "quote rule is approximate");
    eqSide(Side::Buy, at(101).side, "through the ask");
    eqSide(Side::Sell, at(99.5).side, "at the bid");
    eqSide(Side::Sell, at(99).side, "through the bid");
    eqSide(Side::Buy, at(100.25).side, "above the mid");
    eqSide(Side::Sell, at(99.75).side, "below the mid");
    // At the mid: the tick rule against the trade before (99.75 → 100 is up).
    eqSide(Side::Buy, at(100).side, "at the mid after a lower trade");
    eqSide(Side::Buy, at(100).side, "at the mid, unchanged: same side as before");
    // No quote, or a crossed one: tick rule (100 → 99.75 is down).
    eqSide(Side::Sell, c.classify(tr(99.75)).side, "no quote → tick rule");
    eqSide(Side::Buy, c.classify(tr(100, 1, Side::Unknown, 101, 99)).side, "crossed quote → tick rule");
    // The quote mode ignores a side given by the feed.
    eqSide(Side::Sell, c.classify(tr(99.5, 1, Side::Buy, 99.5, 100.5)).side, "quote mode, feed side ignored");
}

void testClassifyTickRule()
{
    CAT = "classify/tick";
    TradeClassifier c(ClassifyMode::TickRule, 0.5);
    // 100, 100.5, 100.5, 100, 100, 101, 101
    //   ?   up    =     down  =    up   =
    //   ?   B     B     S     S    B    B
    const double prices[] = {100, 100.5, 100.5, 100, 100, 101, 101};
    const Side want[] = {Side::Unknown, Side::Buy, Side::Buy, Side::Sell, Side::Sell, Side::Buy, Side::Buy};
    for (int i = 0; i < 7; ++i) {
        const Classified r = c.classify(tr(prices[i], 1, Side::Sell));   // the feed's side is ignored
        eqSide(want[i], r.side, QStringLiteral("trade %1 at %2").arg(i).arg(prices[i]));
        check(r.approximate, QStringLiteral("trade %1 approximate").arg(i));
    }
    // Two prices within half a tick of each other are the same tick: unchanged, same side.
    c.reset();
    c.classify(tr(100));
    c.classify(tr(100.5));                             // up: buy
    eqSide(Side::Buy, c.classify(tr(100.5000001)).side, "same tick after float noise");
    // reset() forgets the last trade.
    c.reset();
    eqSide(Side::Unknown, c.classify(tr(50)).side, "first trade after reset");
}

// ---------------------------------------------------------------- one candle: delta, POC, imbalances

// The reference candle, at a 0.25 tick (one tick per row):
//   price   row  bid  ask   volume  delta
//   100.75  403    2    9     11     +7
//   100.50  402    3   12     15     +9
//   100.25  401    4    1      5     -3
//   100.00  400   10    5     15     -5
//   totals        19   27     46     +8
// Buy imbalances, ask(P) ≥ 3 × bid(P − 1):
//   403: 9 ≥ 3 × 3 = 9 yes (equal counts)   402: 12 ≥ 3 × 4 = 12 yes   401: 1 ≥ 30 no   400: lowest row, none
// Sell imbalances, bid(P) ≥ 3 × ask(P + 1):
//   400: 10 ≥ 3 × 1 = 3 yes   401: 4 ≥ 36 no   402: 3 ≥ 27 no   403: highest row, none (bid 2 > 0 against nothing)
// POC: 402 and 400 tie at 15; the middle is 401.5, 402 is 0.5 away and 400 1.5 → 402.
Book referenceBook()
{
    Book b(TickGrid{0.25, 1});
    TradeClassifier c(ClassifyMode::Aggressor, 0.25);
    struct Row {
        double price, bid, ask;
    };
    // Spread over several trades each, in a mixed-up order, to check that only the sums matter.
    const Row rows[] = {{100.50, 1, 4}, {100.00, 6, 2}, {100.75, 2, 9}, {100.25, 4, 1},
                        {100.50, 2, 8}, {100.00, 4, 3}};
    for (const Row &r : rows) {
        Trade s = tr(r.price, r.bid, Side::Sell), a = tr(r.price, r.ask, Side::Buy);
        b.add(60, s, c.classify(s));
        b.add(60, a, c.classify(a));
    }
    return b;
}

void testCandle()
{
    CAT = "candle";
    const Book b = referenceBook();
    const CandleFootprint *c = b.candle(60);
    check(c != nullptr, "candle at 60");
    if (!c) return;
    eq<int64_t>(400, c->lowRow(), "low row");
    eq<int64_t>(403, c->highRow(), "high row");
    near(2, c->level(403).bid, "403 bid");
    near(9, c->level(403).ask, "403 ask");
    near(3, c->level(402).bid, "402 bid");
    near(12, c->level(402).ask, "402 ask");
    near(4, c->level(401).bid, "401 bid");
    near(1, c->level(401).ask, "401 ask");
    near(10, c->level(400).bid, "400 bid");
    near(5, c->level(400).ask, "400 ask");
    near(7, c->level(403).delta(), "403 delta");
    near(9, c->level(402).delta(), "402 delta");
    near(-3, c->level(401).delta(), "401 delta");
    near(-5, c->level(400).delta(), "400 delta");
    near(15, c->level(400).volume(), "400 volume");
    near(0, c->level(399).volume(), "row below the candle");
    near(0, c->level(404).volume(), "row above the candle");
    near(19, c->bidVolume(), "bid volume");
    near(27, c->askVolume(), "ask volume");
    near(8, c->delta(), "candle delta");
    near(46, c->volume(), "candle volume");
    eq<int64_t>(12, c->trades(), "trades");
    eq<int64_t>(0, c->approximateTrades(), "approximate trades");
    check(!b.approximate(), "book exact");
    // Open/close by time: the first trade was at 100.50, the last at 100.00; high 100.75, low 100.00.
    eq<int64_t>(402, c->openTick(), "open tick");
    eq<int64_t>(400, c->closeTick(), "close tick");
    eq<int64_t>(403, c->highTick(), "high tick");
    eq<int64_t>(400, c->lowTick(), "low tick");

    const Analysis a = analyze(*c, Params{3.0, 0, 3});
    eq<int64_t>(402, a.pocRow, "POC (tie 402/400 → nearer the middle)");
    near(15, a.pocVolume, "POC volume");
    near(15, a.maxLevelVolume, "max row volume");
    near(9, a.maxAbsLevelDelta, "max |row delta|");
    near(8, a.delta, "analysis delta");
    near(46, a.volume, "analysis volume");
    eq<int>(BuyImbalance, a.imbalanceAt(403), "403 buy (9 = 3 × 3)");
    eq<int>(BuyImbalance, a.imbalanceAt(402), "402 buy (12 = 3 × 4)");
    eq<int>(NoImbalance, a.imbalanceAt(401), "401 none");
    eq<int>(SellImbalance, a.imbalanceAt(400), "400 sell (10 ≥ 3 × 1); no buy at the lowest row");
    eq<int>(NoImbalance, a.imbalanceAt(399), "outside the candle");
    eq<size_t>(0, a.stacks.size(), "no stack of 3");

    // Two in a row are enough with N = 2: the buy imbalances at 402-403.
    const Analysis a2 = analyze(*c, Params{3.0, 0, 2});
    eq<size_t>(1, a2.stacks.size(), "stacks with N = 2");
    if (a2.stacks.size() == 1) {
        eq<int64_t>(402, a2.stacks[0].fromRow, "stack from");
        eq<int64_t>(403, a2.stacks[0].toRow, "stack to");
        eqSide(Side::Buy, a2.stacks[0].side, "stack side");
    }
    // A ratio just above 3: 9 < 3.01 × 3 and 12 < 3.01 × 4, only 400's sell (10 ≥ 3.01) is left.
    const Analysis a3 = analyze(*c, Params{3.01, 0, 3});
    eq<int>(NoImbalance, a3.imbalanceAt(403), "ratio 3.01: 403");
    eq<int>(NoImbalance, a3.imbalanceAt(402), "ratio 3.01: 402");
    eq<int>(SellImbalance, a3.imbalanceAt(400), "ratio 3.01: 400");
    // Minimum volume 10: 403 (ask 9) drops, 402 (12) and 400 (bid 10) stay.
    const Analysis a4 = analyze(*c, Params{3.0, 10, 3});
    eq<int>(NoImbalance, a4.imbalanceAt(403), "min 10: 403 (9)");
    eq<int>(BuyImbalance, a4.imbalanceAt(402), "min 10: 402 (12)");
    eq<int>(SellImbalance, a4.imbalanceAt(400), "min 10: 400 (10)");
    // Auto minimum: the average row volume, 46 / 4 rows = 11.5. 403 (ask 9) and 400 (bid 10) drop, 402 (12) stays.
    Params autoMin{3.0, 0, 3};
    autoMin.autoMinVolume = true;
    const Analysis a5 = analyze(*c, autoMin);
    eq<int>(NoImbalance, a5.imbalanceAt(403), "auto min 11.5: 403 (9)");
    eq<int>(BuyImbalance, a5.imbalanceAt(402), "auto min 11.5: 402 (12)");
    eq<int>(NoImbalance, a5.imbalanceAt(400), "auto min 11.5: 400 (10)");
}

void testEdges()
{
    CAT = "edges";
    // One row: nothing above or below to compare with, even with a lot of volume on one side.
    CandleFootprint one;
    put(one, 50, 0, 100);
    Analysis a = analyze(one, Params{});
    eq<int>(NoImbalance, a.imbalanceAt(50), "single row, ask only");
    eq<int64_t>(50, a.pocRow, "single row POC");

    // The lowest row is all asks and the highest all bids: the obvious imbalances would need the rows outside the
    // candle, so there are none.
    //   row  bid  ask
    //    12    8    0    sell? highest row → no;  buy? ask 0 → no
    //    11    1    1    buy: 1 ≥ 3 × bid(10) = 0 → yes;  sell: 1 ≥ 3 × ask(12) = 0 → yes
    //    10    0    8    buy? lowest row → no;  sell? bid 0 → no
    CandleFootprint edge;
    put(edge, 10, 0, 8);
    put(edge, 11, 1, 1);
    put(edge, 12, 8, 0);
    a = analyze(edge, Params{});
    eq<int>(NoImbalance, a.imbalanceAt(10), "lowest row, asks only");
    eq<int>(BuyImbalance | SellImbalance, a.imbalanceAt(11), "middle row against zeros on both sides");
    eq<int>(NoImbalance, a.imbalanceAt(12), "highest row, bids only");

    // A row inside the candle that didn't trade counts as zero:
    //   row  bid  ask
    //    22    0    2    buy: 2 ≥ 3 × bid(21) = 0 → yes (highest row: sell impossible anyway)
    //    21    —    —    no trade
    //    20    6    0    sell: 6 ≥ 3 × ask(21) = 0 → yes (lowest row: buy impossible anyway)
    CandleFootprint gap;
    put(gap, 20, 6, 0);
    put(gap, 22, 0, 2);
    a = analyze(gap, Params{});
    eq<int64_t>(20, a.lowRow, "gap: low row");
    eq<int64_t>(22, a.highRow, "gap: high row");
    eq<size_t>(3, a.imbalance.size(), "gap: three rows");
    eq<int>(SellImbalance, a.imbalanceAt(20), "gap: 20 sell against the empty row");
    eq<int>(NoImbalance, a.imbalanceAt(21), "gap: the empty row itself");
    eq<int>(BuyImbalance, a.imbalanceAt(22), "gap: 22 buy against the empty row");
    eq<int64_t>(20, a.pocRow, "gap: POC 20 (6 > 2)");
    // Minimum volume 3: 22's ask of 2 is too small, 20's bid of 6 is enough.
    a = analyze(gap, Params{3.0, 3, 3});
    eq<int>(NoImbalance, a.imbalanceAt(22), "gap, min 3: 22");
    eq<int>(SellImbalance, a.imbalanceAt(20), "gap, min 3: 20");

    // Auto minimum over the rows that traded only: (6 + 2) / 2 = 4, the empty row 21 isn't one of them.
    // 20's bid of 6 is enough, 22's ask of 2 isn't.
    Params autoMin{3.0, 0, 3};
    autoMin.autoMinVolume = true;
    a = analyze(gap, autoMin);
    eq<int>(SellImbalance, a.imbalanceAt(20), "gap, auto min 4: 20");
    eq<int>(NoImbalance, a.imbalanceAt(22), "gap, auto min 4: 22");
    // An empty candle analyses to nothing.
    a = analyze(CandleFootprint{}, Params{});
    check(a.imbalance.empty() && a.stacks.empty() && a.volume == 0, "empty candle");
    // POC ties at the exact middle (rows 30 and 32 equal, 31 smaller, middle 31): same distance, the lower one.
    CandleFootprint tie;
    put(tie, 30, 2, 2);
    put(tie, 31, 1, 0);
    put(tie, 32, 2, 2);
    eq<int64_t>(30, analyze(tie, Params{}).pocRow, "POC tie at equal distance → lower row");
}

void testStacks()
{
    CAT = "stacks";
    //   row  bid  ask   buy: ask(P) ≥ 3 × bid(P−1)   sell: bid(P) ≥ 3 × ask(P+1)
    //    17    1    3   3 ≥ 27 no                     highest row
    //    16    9    1   1 ≥ 15 no                     9 ≥ 9 yes
    //    15    5    1   1 ≥ 3 no                      5 ≥ 3 yes
    //    14    1    3   3 ≥ 3 yes                     1 ≥ 3 no
    //    13    1    3   3 ≥ 3 yes                     1 ≥ 9 no
    //    12    1    3   3 ≥ 3 yes                     1 ≥ 9 no
    //    11    1    3   3 ≥ 3 yes                     1 ≥ 9 no
    //    10    1    0   lowest row                    1 ≥ 9 no
    // Buy imbalances 11-14 (four in a row), sell imbalances 15-16 (two).
    CandleFootprint c;
    put(c, 10, 1, 0);
    for (int r = 11; r <= 14; ++r) put(c, r, 1, 3);
    put(c, 15, 5, 1);
    put(c, 16, 9, 1);
    put(c, 17, 1, 3);
    Analysis a = analyze(c, Params{3.0, 0, 3});
    for (int r = 11; r <= 14; ++r) eq<int>(BuyImbalance, a.imbalanceAt(r), QStringLiteral("row %1 buy").arg(r));
    eq<int>(SellImbalance, a.imbalanceAt(15), "row 15 sell");
    eq<int>(SellImbalance, a.imbalanceAt(16), "row 16 sell");
    eq<int>(NoImbalance, a.imbalanceAt(10), "row 10");
    eq<int>(NoImbalance, a.imbalanceAt(17), "row 17");
    // N = 3: the four buys make one zone, the two sells none.
    eq<size_t>(1, a.stacks.size(), "N = 3: one zone");
    if (a.stacks.size() == 1) {
        eq<int64_t>(11, a.stacks[0].fromRow, "N = 3: from");
        eq<int64_t>(14, a.stacks[0].toRow, "N = 3: to");
        eqSide(Side::Buy, a.stacks[0].side, "N = 3: side");
    }
    // N = 4: exactly four, still a zone. N = 5: none.
    eq<size_t>(1, analyze(c, Params{3.0, 0, 4}).stacks.size(), "N = 4: one zone");
    eq<size_t>(0, analyze(c, Params{3.0, 0, 5}).stacks.size(), "N = 5: none");
    // N = 2: both, bottom to top.
    a = analyze(c, Params{3.0, 0, 2});
    eq<size_t>(2, a.stacks.size(), "N = 2: two zones");
    if (a.stacks.size() == 2) {
        eq<int64_t>(11, a.stacks[0].fromRow, "N = 2: first from");
        eqSide(Side::Buy, a.stacks[0].side, "N = 2: first side");
        eq<int64_t>(15, a.stacks[1].fromRow, "N = 2: second from");
        eq<int64_t>(16, a.stacks[1].toRow, "N = 2: second to");
        eqSide(Side::Sell, a.stacks[1].side, "N = 2: second side");
    }
    // A break in the middle (row 13 bought only 2: 2 < 3) splits 11-14 into 11-12 and 14: no zone of 3.
    CandleFootprint split;
    put(split, 10, 1, 0);
    put(split, 11, 1, 3);
    put(split, 12, 1, 3);
    put(split, 13, 1, 2);
    put(split, 14, 1, 3);
    a = analyze(split, Params{3.0, 0, 3});
    eq<int>(NoImbalance, a.imbalanceAt(13), "split: 13 (2 < 3)");
    eq<size_t>(0, a.stacks.size(), "split: no zone of 3");
    eq<size_t>(1, analyze(split, Params{3.0, 0, 2}).stacks.size(), "split, N = 2: 11-12");
    // Stacks going up to the top row: the sell side can't reach it, the buy side can.
    CandleFootprint top;
    put(top, 0, 1, 0);
    for (int r = 1; r <= 3; ++r) put(top, r, 1, 3);
    a = analyze(top, Params{3.0, 0, 3});
    eq<size_t>(1, a.stacks.size(), "zone ending at the highest row");
    if (a.stacks.size() == 1) eq<int64_t>(3, a.stacks[0].toRow, "zone ends at the top");
}

// ---------------------------------------------------------------- the book: grouping, incremental, buckets

void testBook()
{
    CAT = "book";
    // Grouping the reference candle by 2 ticks: rows 400-401 → 200, 402-403 → 201.
    //   row 201: bid 2 + 3 = 5, ask 9 + 12 = 21;   row 200: bid 4 + 10 = 14, ask 1 + 5 = 6
    // Buy at 201: 21 ≥ 3 × 14 = 42 no. Sell at 200: 14 ≥ 3 × 21 no. POC: 201 (26 > 20).
    Book b = referenceBook();
    b.setRowTicks(2);
    const CandleFootprint *c = b.candle(60);
    check(c != nullptr, "candle after regrouping");
    if (!c) return;
    eq<int64_t>(200, c->lowRow(), "2-tick low row");
    eq<int64_t>(201, c->highRow(), "2-tick high row");
    near(5, c->level(201).bid, "201 bid");
    near(21, c->level(201).ask, "201 ask");
    near(14, c->level(200).bid, "200 bid");
    near(6, c->level(200).ask, "200 ask");
    near(8, c->delta(), "delta unchanged by grouping");
    near(46, c->volume(), "volume unchanged by grouping");
    eq<int64_t>(201, analyze(*c, Params{}).pocRow, "2-tick POC");
    eq<int64_t>(402, c->openTick(), "open unchanged by grouping");
    b.setRowTicks(1);
    eq<int64_t>(403, b.candle(60)->highRow(), "back to 1 tick");
    near(12, b.candle(60)->level(402).ask, "back to 1 tick: 402 ask");

    // Many trades, fed one by one, against the same trades rebuilt at once (and in reverse order).
    std::mt19937 rng(7);
    std::uniform_int_distribution<int> tickDist(-40, 40), sideDist(0, 1), candleDist(0, 4);
    std::uniform_real_distribution<double> qtyDist(0.001, 3);
    std::vector<std::pair<double, Trade>> trades;
    for (int i = 0; i < 20000; ++i) {
        Trade t;
        t.time = 1000 + i * 0.01;
        t.seq = i;
        t.price = 50000 + tickDist(rng) * 0.5;
        t.qty = qtyDist(rng);
        t.side = sideDist(rng) ? Side::Buy : Side::Sell;
        trades.push_back({double(candleDist(rng) * 60), t});
    }
    Book live(TickGrid{0.5, 1}), reversed(TickGrid{0.5, 1});
    for (const auto &[ct, t] : trades) live.add(ct, t, {t.side, false});
    for (auto it = trades.rbegin(); it != trades.rend(); ++it) reversed.add(it->first, it->second, {it->second.side, false});
    eq<size_t>(5, live.candles().size(), "five candles");
    eq<size_t>(20000, live.tradeCount(), "trades kept");
    bool same = true;
    for (const auto &[ct, cf] : live.candles()) {
        const CandleFootprint *r = reversed.candle(ct);
        if (!r || r->lowRow() != cf.lowRow() || r->highRow() != cf.highRow() || r->openTick() != cf.openTick()
            || r->closeTick() != cf.closeTick() || std::fabs(r->delta() - cf.delta()) > 1e-6)
            same = false;
        for (int64_t row = cf.lowRow(); r && row <= cf.highRow(); ++row)
            if (std::fabs(r->level(row).bid - cf.level(row).bid) > 1e-9
                || std::fabs(r->level(row).ask - cf.level(row).ask) > 1e-9)
                same = false;
        // The candle's totals, kept trade by trade, match the sum of its rows.
        double bid = 0, ask = 0;
        for (int64_t row = cf.lowRow(); row <= cf.highRow(); ++row) {
            bid += cf.level(row).bid;
            ask += cf.level(row).ask;
        }
        near(bid, cf.bidVolume(), QStringLiteral("candle %1: bid total vs rows").arg(ct));
        near(ask, cf.askVolume(), QStringLiteral("candle %1: ask total vs rows").arg(ct));
    }
    check(same, "fed in order = fed in reverse");
    // Grouping by 4 then back gives the rows the trades gave at first.
    Book regrouped = live;
    regrouped.setRowTicks(4);
    regrouped.setRowTicks(1);
    bool sameAfter = true;
    for (const auto &[ct, cf] : live.candles())
        for (int64_t row = cf.lowRow(); row <= cf.highRow(); ++row)
            if (regrouped.candle(ct)->level(row).volume() != cf.level(row).volume()) sameAfter = false;
    check(sameAfter, "regrouped and back");
    // 4-tick rows are the sum of their four 1-tick rows.
    Book four = live;
    four.setRowTicks(4);
    bool sums = true;
    for (const auto &[ct, cf] : four.candles())
        for (int64_t row = cf.lowRow(); row <= cf.highRow(); ++row) {
            double v = 0;
            for (int k = 0; k < 4; ++k) v += live.candle(ct)->level(row * 4 + k).volume();
            if (std::fabs(v - cf.level(row).volume()) > 1e-9) sums = false;
        }
    check(sums, "4-tick rows = sum of 1-tick rows");

    // One more trade touches only its row and the candle's totals.
    Book one = referenceBook();
    const uint64_t v0 = one.candle(60)->version();
    Trade t = tr(100.25, 2, Side::Buy);
    one.add(60, t, {Side::Buy, false});
    const CandleFootprint *cf = one.candle(60);
    near(3, cf->level(401).ask, "401 ask after one more buy of 2");
    near(4, cf->level(401).bid, "401 bid unchanged");
    near(12, cf->level(402).ask, "402 unchanged");
    near(10, cf->delta(), "delta 8 + 2");
    near(48, cf->volume(), "volume 46 + 2");
    eq<int64_t>(401, cf->closeTick(), "close is the new trade");
    check(cf->version() == v0 + 1, "version bumped once");
    // A trade outside the candle's range grows it (down here, by 10 rows).
    Trade low = tr(97.75, 1, Side::Sell);
    one.add(60, low, {Side::Sell, false});
    eq<int64_t>(391, one.candle(60)->lowRow(), "grown down to 391");
    near(0, one.candle(60)->level(395).volume(), "the rows in between are empty");
    near(10, one.candle(60)->level(400).bid, "400 kept when growing down");
    near(1, one.candle(60)->level(391).bid, "391 bid");

    // Unclassified trades (the first one under the tick rule) count in the candle's volume only.
    Book tick(TickGrid{0.5, 1});
    TradeClassifier cl(ClassifyMode::TickRule, 0.5);
    for (double p : {100.0, 100.5, 100.0}) {
        const Trade x = tr(p, 1);
        tick.add(0, x, cl.classify(x));
    }
    const CandleFootprint *tc = tick.candle(0);
    near(3, tc->volume(), "tick rule: volume 3");
    near(1, tc->unclassifiedVolume(), "tick rule: the first one unclassified");
    near(1, tc->askVolume(), "tick rule: 100.5 bought");
    near(1, tc->bidVolume(), "tick rule: 100 sold");
    near(1, tc->level(200).volume(), "tick rule: row 200 holds the sell, not the unclassified first trade");
    check(tick.approximate(), "tick rule book is approximate");
    eq<int64_t>(3, tick.approximateTrades(), "three approximate trades");

    // Dropping old candles drops their trades too.
    live.dropBefore(120);
    eq<size_t>(3, live.candles().size(), "dropBefore(120): candles 120, 180, 240");
    check(live.candle(60) == nullptr && live.candle(120) != nullptr, "dropBefore: 60 gone, 120 kept");
    size_t kept = 0;
    for (const auto &[ct, t] : trades) kept += ct >= 120;
    eq(kept, live.tradeCount(), "dropBefore: trades of the kept candles");
    // Trimming to a number of trades drops whole candles, oldest first: 120 goes, then 180 if needed.
    size_t in120 = 0, in180 = 0, in240 = 0;
    for (const auto &[ct, t] : trades) {
        in120 += ct == 120;
        in180 += ct == 180;
        in240 += ct == 240;
    }
    Book trim = live;
    near(120, trim.trimToTrades(in120 + in180 + in240), "trim to everything: nothing goes");
    near(180, trim.trimToTrades(in180 + in240), "trim: candle 120 goes");
    near(240, trim.trimToTrades(in180 + in240 - 1), "trim one below: 180 goes too");
    eq(in240, trim.tradeCount(), "trim: the trades of 240 are left");
    check(std::isnan(trim.trimToTrades(0)) && trim.candles().empty(), "trim to 0: nothing left");
}

void testFormat()
{
    CAT = "format";
    const QPair<double, QString> cases[] = {
        {0, "0"}, {0.045, "0.045"}, {0.5, "0.5"}, {2.345, "2.35"}, {12.34, "12.3"}, {845.4, "845"},
        {12345, "12.3K"}, {1.25e6, "1.3M"}, {-0.851, "-0.851"}, {-1.54, "-1.54"},
        // thin markets (binance.us): never "0" or "-0" for a volume that isn't zero
        {0.00163, "0.0016"}, {0.00005, "0.00005"}, {-0.00004, "-0.00004"}, {0.00000001, "0.00000001"},
    };
    for (const auto &[v, want] : cases) eq(want, formatVolume(v), QStringLiteral("formatVolume(%1)").arg(v, 0, 'g', 10));
}

void testBuckets()
{
    CAT = "buckets";
    const Values opens = {0, 60, 120};
    eq(-1, candleIndex(opens, -0.001), "before the first bar");
    eq(0, candleIndex(opens, 0), "at the first open");
    eq(0, candleIndex(opens, 59.999), "just before the second open");
    eq(1, candleIndex(opens, 60), "at the second open");
    eq(2, candleIndex(opens, 500), "after the last open: the last bar");
    eq(-1, candleIndex({}, 10), "no bars");
    near(1700000040, candleOpen(1700000099.999, 60), "1 m candle open");
    near(1699999200, candleOpen(1700000000, 3600), "1 h candle open");
}

} // namespace

int main()
{
    testTicks();
    testClassifyFeed();
    testClassifyQuote();
    testClassifyTickRule();
    testCandle();
    testEdges();
    testStacks();
    testBook();
    testFormat();
    testBuckets();
    std::cout << checks << " checks, " << fails << " failed\n";
    return fails ? 1 : 0;
}
