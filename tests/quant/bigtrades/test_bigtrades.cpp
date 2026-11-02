// qtm_test_bigtrades: Big Trades (core/BigTrades.*) on small hand-made sets of trades. Every expected number is worked
// out in the comments next to it. Same style as the other test programs: plain checks, a count at the end, non-zero
// exit on any failure.
#include "core/BigTrades.h"

#include <QElapsedTimer>
#include <QString>

#include <algorithm>
#include <iostream>
#include <random>

using namespace qtm;
using namespace qtm::bigtrades;
using footprint::TradeClassifier;
using footprint::ClassifyMode;

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
void near(double want, double got, const QString &ctx, double tol = 1e-9)
{
    check(std::fabs(want - got) <= tol * std::max(1.0, std::fabs(want)),
          QStringLiteral("%1: want %2, got %3").arg(ctx).arg(want, 0, 'g', 17).arg(got, 0, 'g', 17));
}

constexpr double T0 = 1700000000;   // millisecond tests start here
constexpr double M0 = 1699999980;   // a whole minute (28333333 × 60), for the minute-bar tests

int64_t g_seq = 0;
Trade tr(double ms, double price, double qty, Side side = Side::Unknown, double base = T0)
{
    Trade t;
    t.time = base + ms / 1000;
    t.seq = ++g_seq;
    t.price = price;
    t.qty = qty;
    t.side = side;
    return t;
}
Classified exact(Side s) { return {s, false}; }

// Runs trades through an aggregator and returns every aggregate, the last one flushed.
std::vector<Aggregate> aggregate(const std::vector<Trade> &trades, AggregationParams p)
{
    Aggregator ag(p);
    std::vector<Aggregate> out;
    for (const Trade &t : trades)
        if (auto a = ag.add(t, exact(t.side))) out.push_back(*a);
    if (auto a = ag.flush()) out.push_back(*a);
    return out;
}

// ---------------------------------------------------------------- aggregation

void testWindow()
{
    CAT = "aggregation/window";
    // Buys at 100: 0, 10, 40 ms join (all under 50 ms after the first); 60 ms starts a new one, 70 ms joins it.
    std::vector<Aggregate> a = aggregate({tr(0, 100, 1, Side::Buy), tr(10, 100, 2, Side::Buy), tr(40, 100, 3, Side::Buy),
                                          tr(60, 100, 4, Side::Buy), tr(70, 100, 5, Side::Buy)}, {});
    eq<size_t>(2, a.size(), "two aggregates");
    if (a.size() == 2) {
        near(6, a[0].volume, "first: 1 + 2 + 3");
        eq(3, a[0].executions, "first: 3 executions");
        near(T0, a[0].time, "first: starts at 0 ms");
        near(T0 + 0.040, a[0].lastTime, "first: ends at 40 ms");
        near(9, a[1].volume, "second: 4 + 5");
        eq(2, a[1].executions, "second: 2 executions");
    }
    // The window counts from the aggregate's first trade: 0, 30, 60 ms → (0, 30) and (60), although 60 − 30 < 50.
    a = aggregate({tr(0, 100, 1, Side::Buy), tr(30, 100, 1, Side::Buy), tr(60, 100, 1, Side::Buy)}, {});
    eq<size_t>(2, a.size(), "window from the first trade, not the previous");
    // Exactly 50 ms after is outside ("less than the window"); 49 ms is inside.
    eq<size_t>(2, aggregate({tr(0, 100, 1, Side::Buy), tr(50, 100, 1, Side::Buy)}, {}).size(), "50 ms: new aggregate");
    eq<size_t>(1, aggregate({tr(0, 100, 1, Side::Buy), tr(49, 100, 1, Side::Buy)}, {}).size(), "49 ms: same aggregate");
    // A wider window joins more.
    AggregationParams wide;
    wide.windowMs = 200;
    eq<size_t>(1, aggregate({tr(0, 100, 1, Side::Buy), tr(30, 100, 1, Side::Buy), tr(60, 100, 1, Side::Buy)}, wide).size(),
               "200 ms window: one aggregate");
    // Exact timestamp only: 5, 5, 6 ms → (5, 5) and (6).
    AggregationParams exactTs;
    exactTs.exactTimestamp = true;
    a = aggregate({tr(5, 100, 1, Side::Buy), tr(5, 100, 2, Side::Buy), tr(6, 100, 4, Side::Buy)}, exactTs);
    eq<size_t>(2, a.size(), "exact timestamp: two aggregates");
    if (a.size() == 2) {
        near(3, a[0].volume, "exact timestamp: 1 + 2");
        near(4, a[1].volume, "exact timestamp: 4 alone");
    }
}

void testSide()
{
    CAT = "aggregation/side";
    // buy, sell, buy within 20 ms at one price: three aggregates, the side change always splits.
    std::vector<Aggregate> a = aggregate({tr(0, 100, 1, Side::Buy), tr(10, 100, 1, Side::Sell), tr(20, 100, 1, Side::Buy)}, {});
    eq<size_t>(3, a.size(), "side changes split");
    if (a.size() == 3) {
        check(a[0].side == Side::Buy && a[1].side == Side::Sell && a[2].side == Side::Buy, "sides kept");
    }
    // Two sells then a buy at the same instant: (sell, sell) and (buy).
    a = aggregate({tr(0, 100, 1, Side::Sell), tr(0, 100, 2, Side::Sell), tr(0, 100, 3, Side::Buy)}, {});
    eq<size_t>(2, a.size(), "sell, sell, buy at the same ms");
    // A trade of unknown side is never joined (and joins nothing).
    a = aggregate({tr(0, 100, 1, Side::Unknown), tr(10, 100, 1, Side::Unknown)}, {});
    eq<size_t>(2, a.size(), "unknown sides stay apart");
}

void testPrice()
{
    CAT = "aggregation/price";
    // Buys at 100 (2), 100.5 (1), 101 (3) within 20 ms.
    const std::vector<Trade> sweepUp = {tr(0, 100, 2, Side::Buy), tr(10, 100.5, 1, Side::Buy), tr(20, 101, 3, Side::Buy)};
    // Same price only: three aggregates.
    eq<size_t>(3, aggregate(sweepUp, {}).size(), "same price: three");
    // Sweep: one aggregate of 6; VWAP = (100 × 2 + 100.5 × 1 + 101 × 3) / 6 = 603.5 / 6 = 100.583333…; min 100, max 101.
    AggregationParams sweep;
    sweep.priceMode = PriceMode::Sweep;
    std::vector<Aggregate> a = aggregate(sweepUp, sweep);
    eq<size_t>(1, a.size(), "sweep: one");
    if (a.size() == 1) {
        near(6, a[0].volume, "sweep: volume");
        near(603.5 / 6, a[0].vwap(), "sweep: VWAP");
        near(100, a[0].minPrice, "sweep: min");
        near(101, a[0].maxPrice, "sweep: max");
        eq(3, a[0].executions, "sweep: executions");
    }
    // A buy sweep goes up only: 101 then 100.5 splits.
    eq<size_t>(2, aggregate({tr(0, 101, 1, Side::Buy), tr(10, 100.5, 1, Side::Buy)}, sweep).size(), "buy going down splits");
    // A sell sweep goes down: 100 (1), 99.5 (1), 99.5 (2) → one; VWAP = (100 + 99.5 + 199) / 4 = 99.625.
    a = aggregate({tr(0, 100, 1, Side::Sell), tr(10, 99.5, 1, Side::Sell), tr(20, 99.5, 2, Side::Sell)}, sweep);
    eq<size_t>(1, a.size(), "sell sweep down: one");
    if (a.size() == 1) {
        near(99.625, a[0].vwap(), "sell sweep: VWAP");
        near(99.5, a[0].minPrice, "sell sweep: min");
        near(100, a[0].maxPrice, "sell sweep: max");
    }
    // A sell going back up splits.
    eq<size_t>(2, aggregate({tr(0, 99.5, 1, Side::Sell), tr(10, 100, 1, Side::Sell)}, sweep).size(), "sell going up splits");
    // Same price mode, the VWAP of one price is that price.
    a = aggregate({tr(0, 100.25, 1, Side::Buy), tr(1, 100.25, 3, Side::Buy)}, {});
    if (a.size() == 1) near(100.25, a[0].vwap(), "one price: VWAP");
}

void testFlush()
{
    CAT = "aggregation/flush";
    Aggregator ag;
    ag.add(tr(0, 100, 1, Side::Buy), exact(Side::Buy));
    check(!ag.flushIfDone(T0 + 0.030).has_value(), "30 ms later: still open");
    check(ag.open().has_value(), "still open after 30 ms");
    const auto a = ag.flushIfDone(T0 + 0.050);
    check(a.has_value() && a->volume == 1, "50 ms later: closed");
    check(!ag.open().has_value(), "nothing open after the flush");
    AggregationParams exactTs;
    exactTs.exactTimestamp = true;
    Aggregator ex(exactTs);
    ex.add(tr(7, 100, 1, Side::Buy), exact(Side::Buy));
    check(!ex.flushIfDone(T0 + 0.007).has_value(), "exact: same ms still open");
    check(ex.flushIfDone(T0 + 0.008).has_value(), "exact: next ms closed");
}

void testTickRule()
{
    CAT = "aggregation/tick rule";
    // No side from the feed: the tick rule at a 0.5 tick.
    //   0 ms 100    first trade: unknown
    //  10 ms 100.5  up: buy
    //  20 ms 100.5  same: buy           → joins the 10 ms buy (same price, 10 ms)
    //  30 ms 100    down: sell
    //  40 ms 100    same: sell          → joins the 30 ms sell
    // Aggregates: [unknown 1], [buy 2 + 3], [sell 4 + 5], every classified one approximate.
    TradeClassifier cl(ClassifyMode::TickRule, 0.5);
    Aggregator ag;
    std::vector<Aggregate> a;
    const double prices[] = {100, 100.5, 100.5, 100, 100};
    for (int i = 0; i < 5; ++i) {
        const Trade t = tr(10 * i, prices[i], i + 1);
        if (auto x = ag.add(t, cl.classify(t))) a.push_back(*x);
    }
    if (auto x = ag.flush()) a.push_back(*x);
    eq<size_t>(3, a.size(), "three aggregates");
    if (a.size() == 3) {
        check(a[0].side == Side::Unknown, "first: unknown");
        check(a[1].side == Side::Buy && a[1].volume == 5 && a[1].executions == 2, "second: buy of 2 + 3");
        check(a[2].side == Side::Sell && a[2].volume == 9 && a[2].executions == 2, "third: sell of 4 + 5");
        check(a[1].approximate && a[2].approximate, "tick-rule sides are approximate");
    }
    // Unknown aggregates are left out of the detection: never big, never counted.
    ThresholdParams manual;
    manual.automatic = false;
    Detector d({}, manual);
    TradeClassifier cl2(ClassifyMode::TickRule, 0.5);
    std::vector<BigTrade> big;
    for (int i = 0; i < 5; ++i) {
        const Trade t = tr(1000 + 10 * i, prices[i], i + 1);
        if (auto b = d.add(t, cl2.classify(t))) big.push_back(*b);
    }
    if (auto b = d.flush()) big.push_back(*b);
    eq<size_t>(2, big.size(), "manual min 0: the buy and the sell, not the unknown one");
    eq<int64_t>(2, d.aggregates(), "two aggregates counted");
}

// ---------------------------------------------------------------- rolling percentile

void testPercentile()
{
    CAT = "percentile";
    RollingPercentile rp(1e9);
    std::vector<double> v;
    for (int i = 1; i <= 100; ++i) v.push_back(i);
    std::shuffle(v.begin(), v.end(), std::mt19937(3));
    for (double x : v) rp.add(0, x);
    // 1..100: rank (n − 1) × p / 100 = 99 × p / 100, value = rank + 1.
    near(99.01, rp.percentile(99), "p99 of 1..100 (rank 98.01)");
    near(50.5, rp.percentile(50), "p50 of 1..100 (rank 49.5)");
    near(25.75, rp.percentile(25), "p25 of 1..100 (rank 24.75)");
    near(1, rp.percentile(0), "p0");
    near(100, rp.percentile(100), "p100");
    eq<size_t>(100, rp.size(), "size");
    RollingPercentile one;
    one.add(0, 7);
    near(7, one.percentile(99), "a single value");
    check(std::isnan(RollingPercentile().percentile(50)), "empty: NaN");

    // The window is (now − 60, now]: values at 0, 10, 30, 59 s.
    RollingPercentile w(60);
    w.add(0, 10);
    w.add(10, 20);
    w.add(30, 30);
    w.add(59, 40);
    w.expire(60);   // 0 ≤ 60 − 60: out
    eq<size_t>(3, w.size(), "expire(60): 10 out");
    near(30, w.percentile(50), "expire(60): p50 of 20, 30, 40");
    w.expire(70);   // 10 ≤ 10: out
    near(35, w.percentile(50), "expire(70): p50 of 30, 40");
    w.expire(200);
    check(w.size() == 0 && std::isnan(w.percentile(50)), "expire(200): empty");
    // Duplicates go out one at a time.
    RollingPercentile d(10);
    d.add(0, 5);
    d.add(1, 5);
    d.add(2, 5);
    d.add(3, 7);
    d.expire(10);   // the 5 at t = 0
    eq<size_t>(3, d.size(), "one duplicate out");
    near(5, d.percentile(0), "min still 5");
    near(7, d.percentile(100), "max 7");

    // Many values across many blocks, against a full sort.
    RollingPercentile big(500);
    std::deque<std::pair<double, double>> ref;
    std::mt19937 rng(11);
    std::lognormal_distribution<double> size(0, 1.5);
    std::uniform_int_distribution<int> dup(0, 9);
    bool same = true;
    for (int i = 0; i < 60000; ++i) {
        const double t = i * 0.05;
        const double x = dup(rng) == 0 ? 1.0 : std::round(size(rng) * 1000) / 1000;   // duplicates on purpose
        big.expire(t);
        while (!ref.empty() && ref.front().first <= t - 500) ref.pop_front();
        big.add(t, x);
        ref.push_back({t, x});
        if (i % 997 == 0) {
            std::vector<double> s;
            for (const auto &r : ref) s.push_back(r.second);
            std::sort(s.begin(), s.end());
            for (double p : {0.0, 10.0, 50.0, 90.0, 99.0, 99.9, 100.0}) {
                const double h = (s.size() - 1) * p / 100;
                const size_t lo = size_t(h);
                const double want = lo + 1 < s.size() ? s[lo] + (h - lo) * (s[lo + 1] - s[lo]) : s[lo];
                if (std::fabs(want - big.percentile(p)) > 1e-12 || big.size() != s.size()) same = false;
            }
        }
    }
    check(same, "60,000 values, 10,000 in the window: every percentile = the full sort's");

    // Speed: a million values in and out of a 36,000-value window (30 min at 20 aggregates a second).
    RollingPercentile fast(1800);
    QElapsedTimer timer;
    timer.start();
    double sink = 0;
    for (int i = 0; i < 1000000; ++i) {
        const double t = i * 0.05;
        fast.expire(t);
        fast.add(t, size(rng));
        if (i % 100 == 0) sink += fast.percentile(99);
    }
    const double ms = timer.nsecsElapsed() / 1e6;
    std::cout << "  rolling percentile: 1,000,000 values through a " << fast.size() << "-value window in " << ms
              << " ms (" << ms * 1000 / 1e6 << " µs each)\n";
    check(ms < 3000 && sink > 0, QStringLiteral("a million values in under 3 s (took %1 ms)").arg(ms));
}

// ---------------------------------------------------------------- volatility

// Minute k (from M0) gets trades at 100, 100 + r, 100 (close 100): its true range is r.
void feedMinutes(MinuteAtr &atr, const std::vector<double> &ranges)
{
    for (size_t k = 0; k < ranges.size(); ++k) {
        const double m = M0 + 60 * double(k);
        atr.add(m + 1, 100);
        atr.add(m + 2, 100 + ranges[k]);
        atr.add(m + 3, 100);
    }
}

void testVolatility()
{
    CAT = "volatility";
    // ATR(2) / ATR(8). Ranges 1, 1, 1, 1, 1, 1, 8, 8: ATR(2) = 8, ATR(8) = 22 / 8 = 2.75, ratio 2.91 → capped at 2.
    MinuteAtr a(2, 8);
    feedMinutes(a, {1, 1, 1, 1, 1, 1, 8, 8});
    eq(7, a.closedBars(), "minute 7 still open");
    near(1, a.factor(), "7 closed minutes of 8 needed: factor 1");
    a.add(M0 + 8 * 60 + 1, 100);   // minute 8 opens: minute 7 closes
    eq(8, a.closedBars(), "8 closed minutes");
    near(8, a.atr(2), "ATR(2)");
    near(2.75, a.atr(8), "ATR(8)");
    near(2, a.factor(), "factor capped at 2");
    // Ranges 4 × 6, then 1, 1: ATR(2) = 1, ATR(8) = 26 / 8 = 3.25, ratio 0.31 → floored at 0.5.
    MinuteAtr b(2, 8);
    feedMinutes(b, {4, 4, 4, 4, 4, 4, 1, 1});
    b.add(M0 + 8 * 60 + 1, 100);
    near(3.25, b.atr(8), "ATR(8), calming down");
    near(0.5, b.factor(), "factor floored at 0.5");
    // Steady ranges: 1.
    MinuteAtr c(2, 8);
    feedMinutes(c, {3, 3, 3, 3, 3, 3, 3, 3});
    c.add(M0 + 8 * 60 + 1, 100);
    near(1, c.factor(), "steady: factor 1");
    // Between the bounds: ranges 2 × 6, then 3, 3 → ATR(2) = 3, ATR(8) = 18 / 8 = 2.25, factor 4 / 3.
    MinuteAtr e(2, 8);
    feedMinutes(e, {2, 2, 2, 2, 2, 2, 3, 3});
    e.add(M0 + 8 * 60 + 1, 100);
    near(4.0 / 3, e.factor(), "factor 3 / 2.25");
    // A gap counts in the true range: a minute closing at 100, the next trading 105-106 → max(1, 6, 5) = 6.
    MinuteAtr g(1, 1);
    g.add(M0 + 1, 100);
    g.add(M0 + 61, 105);
    g.add(M0 + 62, 106);
    g.add(M0 + 121, 106);   // closes the second minute
    near(6, g.atr(1), "true range with a gap");
    // A minute without trades has no bar: the next one compares with the last close before it.
    MinuteAtr h(1, 1);
    h.add(M0 + 1, 100);
    h.add(M0 + 181, 101);   // three minutes later
    h.add(M0 + 241, 101);
    near(1, h.atr(1), "minutes without trades skipped: |101 − 100|");
}

// ---------------------------------------------------------------- thresholds

// Aggregates of the given sizes, one second apart, sides alternating (so none join), all at 100.
std::vector<BigTrade> runSizes(Detector &d, const std::vector<double> &sizes, double startMs = 0)
{
    std::vector<BigTrade> out;
    for (size_t i = 0; i < sizes.size(); ++i) {
        const Side s = i % 2 ? Side::Sell : Side::Buy;
        if (auto b = d.add(tr(startMs + 1000.0 * double(i), 100, sizes[i], s), exact(s))) out.push_back(*b);
    }
    if (auto b = d.flush()) out.push_back(*b);
    return out;
}

void testManual()
{
    CAT = "threshold/manual";
    ThresholdParams p;
    p.automatic = false;
    p.minVolume = 5;
    Detector d({}, p);
    const Threshold t = d.threshold();
    check(!t.automatic && t.value == 5, "manual threshold = min");
    check(d.passes(5, t), "5 ≥ 5");
    check(!d.passes(4.99, t), "4.99 < 5");
    check(d.passes(1000, t), "max 0: no upper limit");
    p.maxVolume = 10;
    Detector m({}, p);
    check(m.passes(10, m.threshold()), "max 10: 10 in");
    check(!m.passes(10.01, m.threshold()), "max 10: 10.01 out");
    // Through the detector: sizes 4, 5, 12, 7 with min 5, max 10 → 5 and 7.
    const std::vector<BigTrade> big = runSizes(m, {4, 5, 12, 7});
    eq<size_t>(2, big.size(), "min 5 max 10: two big trades");
    near(12, m.maxVolume(), "the biggest aggregate, big or not: 12 (above max)");
    if (big.size() == 2) {
        near(5, big[0].a.volume, "first: 5");
        near(7, big[1].a.volume, "second: 7");
        near(5, big[0].threshold, "judged against 5");
    }
    // Min 0, max 0: everything.
    ThresholdParams all;
    all.automatic = false;
    Detector e({}, all);
    eq<size_t>(3, runSizes(e, {0.001, 1, 1000}).size(), "min 0: every aggregate");
    // The max filter in automatic mode too: sizes 1..100, then 150 (≥ p99 = 99.01, but above max 120) and 110.
    ThresholdParams autoMax;
    autoMax.volatility = false;
    autoMax.maxVolume = 120;
    std::vector<double> sizes;
    for (int i = 1; i <= 100; ++i) sizes.push_back(i);
    sizes.push_back(150);
    sizes.push_back(110);
    Detector am({}, autoMax);
    const std::vector<BigTrade> ab = runSizes(am, sizes);
    eq<size_t>(1, ab.size(), "auto with max 120: only 110");
    if (ab.size() == 1) near(110, ab[0].a.volume, "auto with max: 110");
}

void testAuto()
{
    CAT = "threshold/auto";
    ThresholdParams p;   // automatic, p99, 30 min, minSamples 100
    p.volatility = false;
    std::vector<double> sizes;
    for (int i = 1; i <= 100; ++i) sizes.push_back(i);
    // The 101st aggregate is judged against p99 of 1..100 = 99.01: 99.5 is big, 99 isn't.
    for (double x : {99.5, 99.0}) {
        Detector d({}, p);
        std::vector<double> s = sizes;
        s.push_back(x);
        const std::vector<BigTrade> big = runSizes(d, s);
        if (x == 99.5) {
            eq<size_t>(1, big.size(), "99.5 ≥ 99.01: big");
            if (big.size() == 1) near(99.01, big[0].threshold, "judged against p99 = 99.01");
        } else {
            eq<size_t>(0, big.size(), "99 < 99.01: not big (and 1..100 were the warm-up)");
        }
        const Threshold t = d.threshold();
        check(t.automatic && !t.warmingUp && t.samples == 101, "after: auto, warmed up, 101 samples");
    }
    // An aggregate doesn't count toward its own threshold: 100 aggregates of 1, then one of 1 → p99 = 1, 1 ≥ 1 big.
    Detector flat({}, p);
    std::vector<double> ones(101, 1.0);
    eq<size_t>(1, runSizes(flat, ones).size(), "all equal: the 101st is ≥ p99");
    // The window: 100 aggregates of 50 at the start, then 100 of 1 more than 30 minutes later; the 50s have left, so a 2
    // is big (p99 of the 1s is 1).
    Detector win({}, p);
    std::vector<double> fifty(100, 50.0);
    runSizes(win, fifty, 0);
    std::vector<double> later(100, 1.0);
    later.push_back(2);
    const std::vector<BigTrade> wb = runSizes(win, later, 31 * 60 * 1000.0);
    eq<size_t>(1, wb.size(), "old aggregates left the 30 min window");
    if (wb.size() == 1) near(2, wb[0].a.volume, "the 2 is big");
}

void testWarmup()
{
    CAT = "threshold/warm-up";
    ThresholdParams p;
    p.volatility = false;
    std::vector<double> sizes;
    for (int i = 1; i <= 50; ++i) sizes.push_back(i);
    // 50 aggregates < 100 samples: the fallback. 0 = nothing shown.
    Detector none({}, p);
    eq<size_t>(0, runSizes(none, sizes).size(), "fallback 0: nothing during the warm-up");
    const Threshold t = none.threshold();
    check(t.warmingUp && std::isnan(t.value) && t.samples == 50, "warming up, no threshold, 50 samples");
    // Fallback 20: 20..50 are big (31 of them).
    p.minVolume = 20;
    Detector fb({}, p);
    const std::vector<BigTrade> big = runSizes(fb, sizes);
    eq<size_t>(31, big.size(), "fallback 20: 20..50");
    if (!big.empty()) near(20, big.front().a.volume, "first big: 20");
    check(fb.threshold().warmingUp && fb.threshold().value == 20, "threshold shows the fallback");
}

// Minutes 0..7 from M0, 13 aggregates each at 4 s intervals, prices 100 / 100 + r alternating (sides alternating too,
// so none join), closing at 100: minute k's true range is r[k]. Sizes 1, 2, 3, … in order.
void testVolatilityThreshold()
{
    CAT = "threshold/volatility";
    ThresholdParams p;
    p.atrShort = 2;
    p.atrLong = 8;
    const double ranges[] = {1, 1, 1, 1, 1, 1, 8, 8};
    auto run = [&](double last) {
        Detector d({}, p);
        std::vector<BigTrade> out;
        int size = 0;
        for (int k = 0; k < 8; ++k)
            for (int j = 0; j < 13; ++j) {
                const Side s = j % 2 ? Side::Sell : Side::Buy;
                Trade t = tr(0, j % 2 ? 100 + ranges[k] : 100, ++size, s, M0 + 60 * k + 1 + 4 * j);
                if (auto b = d.add(t, exact(s))) out.push_back(*b);
            }
        // Minute 8: one aggregate of `last`; its arrival closes minute 7 → ATR(2) / ATR(8) = 8 / 2.75 → 2.
        const Side s = Side::Buy;
        if (auto b = d.add(tr(0, 100, last, s, M0 + 8 * 60 + 1), exact(s))) out.push_back(*b);
        if (auto b = d.flush()) out.push_back(*b);
        return std::pair{out, d.threshold()};
    };
    // Aggregates 1..100 are the warm-up. 101..104 are judged in minute 7 (7 closed minutes: factor 1) against
    // p99 of 1..100 = 99.01, 1..101 = 100, 1..102 = 100.99, 1..103 = 101.98: all four are big.
    // The last one, in minute 8, is judged against p99 of 1..104 = 1 + 0.99 × 103 = 102.97, × 2 = 205.94.
    auto [big206, thr] = run(206);
    eq<size_t>(5, big206.size(), "101..104, then 206");
    const double want[] = {99.01, 100, 100.99, 101.98, 205.94};
    for (size_t i = 0; i < std::min<size_t>(5, big206.size()); ++i)
        near(want[i], big206[i].threshold, QStringLiteral("threshold of big trade %1").arg(i + 1));
    if (big206.size() == 5) near(206, big206[4].a.volume, "206 ≥ 205.94: big");
    near(2, thr.volatilityFactor, "factor 2 (capped)");
    check(thr.volatilityReady, "8 closed minutes: volatility ready");
    near(1 + 0.99 * 104, thr.percentileValue, "p99 after: of 1..104 and 206");
    auto [big205, thr2] = run(205);
    eq<size_t>(4, big205.size(), "205 < 205.94: not big");
    // Volatility off: the same 205 is far above p99 = 102.97.
    p.volatility = false;
    auto [off, thr3] = run(205);
    eq<size_t>(5, off.size(), "volatility off: 205 is big");
    near(1, thr3.volatilityFactor, "volatility off: factor 1");
    check(!thr3.volatilityReady, "volatility off: never ready");
    Detector early({}, ThresholdParams{});
    early.add(tr(0, 100, 1, Side::Buy), exact(Side::Buy));
    check(!early.threshold().volatilityReady, "no closed minute: volatility not ready");
}

// ---------------------------------------------------------------- bubbles

void testBubbles()
{
    CAT = "bubbles";
    const BubbleStyle s;   // radius 4..24, opacity from 0.35
    // r = 24 × √(v / 100), between 4 and 24.
    near(24, bubbleRadius(100, 100, s), "v = ref: max radius");
    near(12, bubbleRadius(25, 100, s), "v = ref / 4: half the radius (a quarter of the area)");
    near(4, bubbleRadius(1, 100, s), "24 × 0.1 = 2.4 → min radius 4");
    near(24, bubbleRadius(400, 100, s), "above ref: max radius");
    near(4, bubbleRadius(10, 0, s), "no reference: min radius");
    // Area ∝ volume between the bounds: 4× the volume, 4× the area.
    const double r1 = bubbleRadius(9, 100, s), r2 = bubbleRadius(36, 100, s);
    near(4, (r2 * r2) / (r1 * r1), "4× the volume, 4× the area");
    // opacity = 0.35 + 0.65 × √(v / ref)
    near(1, bubbleOpacity(100, 100, s), "v = ref: opaque");
    near(0.675, bubbleOpacity(25, 100, s), "v = ref / 4: 0.35 + 0.65 × 0.5");
    near(0.35, bubbleOpacity(0, 100, s), "v = 0: min opacity");
    near(1, bubbleOpacity(400, 100, s), "above ref: opaque");
    BubbleStyle custom;
    custom.minRadius = 2;
    custom.maxRadius = 40;
    custom.minOpacity = 0.6;
    near(10, bubbleRadius(1, 16, custom), "40 × √(1/16) = 10");
    near(0.7, bubbleOpacity(1, 16, custom), "0.6 + 0.4 × 0.25");
}

} // namespace

int main()
{
    testWindow();
    testSide();
    testPrice();
    testFlush();
    testTickRule();
    testPercentile();
    testVolatility();
    testManual();
    testAuto();
    testWarmup();
    testVolatilityThreshold();
    testBubbles();
    std::cout << checks << " checks, " << fails << " failed\n";
    return fails ? 1 : 0;
}
