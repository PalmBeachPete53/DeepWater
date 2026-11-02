// qtm_test_heatmap: the liquidity heatmap's data (core/OrderBook.*, core/Heatmap.*) on small hand-made sequences of
// depth updates. Every expected number is worked out in the comments next to it. Same style as the other test
// programs: plain checks, a count at the end, non-zero exit on any failure. `load` runs the load test only.
#include "core/DepthRecord.h"
#include "core/Heatmap.h"
#include "core/HeatmapEvents.h"
#include "core/OrderBook.h"

#include <QElapsedTimer>
#include <QString>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <map>
#include <random>

using namespace qtm::depth;
using namespace qtm::heatmap;
namespace footprint = qtm::footprint;

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
// A decoded quantity: within 0.07 % of what went in.
constexpr double CODE_TOL = 7e-4;
void nearQty(double want, double got, const QString &ctx)
{
    check(std::fabs(want - got) <= CODE_TOL * std::fabs(want) + 1e-300,
          QStringLiteral("%1: want %2, got %3").arg(ctx).arg(want, 0, 'g', 17).arg(got, 0, 'g', 17));
}
QString res(OrderBook::Result r)
{
    switch (r) {
    case OrderBook::Result::Applied: return "Applied";
    case OrderBook::Result::Stale: return "Stale";
    case OrderBook::Result::Buffered: return "Buffered";
    case OrderBook::Result::NeedSnapshot: return "NeedSnapshot";
    case OrderBook::Result::Crossed: return "Crossed";
    }
    return "?";
}
void eqRes(OrderBook::Result want, OrderBook::Result got, const QString &ctx)
{
    check(want == got, QStringLiteral("%1: want %2, got %3").arg(ctx, res(want), res(got)));
}

constexpr double T0 = 1700000000;   // a multiple of 0.25 s and of 0.1 s

DepthUpdate upd(int64_t U, int64_t u, std::vector<Level> bids, std::vector<Level> asks, double t = T0, int64_t pu = -1)
{
    DepthUpdate d;
    d.firstId = U;
    d.lastId = u;
    d.prevLastId = pu;
    d.time = t;
    d.bids = std::move(bids);
    d.asks = std::move(asks);
    return d;
}
DepthSnapshot snap(int64_t id, std::vector<Level> bids, std::vector<Level> asks)
{
    DepthSnapshot s;
    s.lastUpdateId = id;
    s.bids = std::move(bids);
    s.asks = std::move(asks);
    return s;
}

// ---------------------------------------------------------------- the book

void testBookLevels()
{
    CAT = "book/levels";
    OrderBook b;
    eqRes(OrderBook::Result::Applied, b.setSnapshot(snap(100, {{99, 1}, {98, 2}}, {{101, 3}, {102, 4}})), "snapshot");
    check(b.synced(), "synced after the snapshot");
    eq<int64_t>(99, b.bestBid().value_or(-1), "best bid 99");
    eq<int64_t>(101, b.bestAsk().value_or(-1), "best ask 101");
    eq<size_t>(4, b.levelCount(), "4 levels");

    // Add a bid at 100 (new best), change the ask at 101 from 3 to 1.5.
    eqRes(OrderBook::Result::Applied, b.apply(upd(101, 101, {{100, 5}}, {{101, 1.5}})), "add + modify");
    eq<int64_t>(100, b.bestBid().value_or(-1), "best bid now 100");
    near(5, b.bidQty(100), "bid 100 = 5");
    near(1.5, b.askQty(101), "ask 101 = 1.5 (absolute, not added)");
    near(2, b.bidQty(98), "bid 98 untouched");

    // Remove the bid at 99; remove an ask at 105 the book never had (normal on Binance: nothing happens).
    eqRes(OrderBook::Result::Applied, b.apply(upd(102, 103, {{99, 0}}, {{105, 0}})), "delete");
    near(0, b.bidQty(99), "bid 99 gone");
    eq<size_t>(4, b.levelCount(), "100, 98 | 101, 102");
    eq<int64_t>(103, b.lastUpdateId(), "last id 103");

    // Remove the best ask: the next one becomes best.
    b.apply(upd(104, 104, {}, {{101, 0}}));
    eq<int64_t>(102, b.bestAsk().value_or(-1), "best ask 102 after 101 went");
    eq<uint64_t>(3, b.stats().applied, "3 updates applied");

    // Every bid gone: no best bid.
    b.apply(upd(105, 105, {{100, 0}, {98, 0}}, {}));
    check(!b.bestBid(), "no best bid once the bids are gone");
    check(b.synced(), "an empty side isn't an error");
}

void testSequence()
{
    CAT = "book/sequence";
    OrderBook b;
    // Updates before the snapshot are buffered.
    eqRes(OrderBook::Result::Buffered, b.apply(upd(95, 98, {{50, 9}}, {})), "95-98 buffered");
    eqRes(OrderBook::Result::Buffered, b.apply(upd(99, 102, {{97, 7}}, {})), "99-102 buffered");
    eqRes(OrderBook::Result::Buffered, b.apply(upd(103, 105, {}, {{110, 1}})), "103-105 buffered");
    eq<size_t>(3, b.bufferedCount(), "3 buffered");
    check(!b.synced(), "not synced before a snapshot");

    // Snapshot 100: 95-98 is in it (dropped), 99-102 overlaps it (U ≤ 101 ≤ u: applied), then 103-105.
    eqRes(OrderBook::Result::Applied, b.setSnapshot(snap(100, {{99, 1}}, {{101, 1}})), "snapshot 100 + buffer");
    eq<int64_t>(105, b.lastUpdateId(), "last id 105");
    near(0, b.bidQty(50), "95-98 dropped: no bid at 50");
    near(7, b.bidQty(97), "99-102 applied");
    near(1, b.askQty(110), "103-105 applied");
    eq<size_t>(0, b.bufferedCount(), "buffer emptied");

    // Already in the book: ignored.
    eqRes(OrderBook::Result::Stale, b.apply(upd(100, 104, {{97, 99}}, {})), "100-104 stale");
    near(7, b.bidQty(97), "stale update not applied");
    // Ending exactly at the last id applied (a resend): stale too.
    eqRes(OrderBook::Result::Stale, b.apply(upd(103, 105, {{97, 99}}, {})), "103-105 again: stale");
    near(7, b.bidQty(97), "resend not applied");

    // 106 is missing: a hole. The update stays buffered, a snapshot is needed; what comes after waits too.
    eqRes(OrderBook::Result::NeedSnapshot, b.apply(upd(107, 108, {{96, 2}}, {})), "hole at 106");
    check(!b.synced(), "not synced after a hole");
    eq<size_t>(1, b.bufferedCount(), "the update after the hole buffered");
    eqRes(OrderBook::Result::Buffered, b.apply(upd(109, 110, {{95, 3}}, {})), "next one buffered");
    eq<uint64_t>(1, b.stats().gaps, "one gap counted");

    // Resync: a snapshot up to 106 fills the hole; 107-108 and 109-110 go in after it.
    eqRes(OrderBook::Result::Applied, b.setSnapshot(snap(106, {{99, 1}}, {{101, 1}})), "resync at 106");
    check(b.synced(), "synced again");
    eq<int64_t>(110, b.lastUpdateId(), "last id 110");
    near(2, b.bidQty(96), "107-108 in");
    near(3, b.bidQty(95), "109-110 in");
    near(0, b.bidQty(97), "the snapshot replaced the book (97 was only in the old one)");

    // A snapshot older than the updates waiting: still a hole (111-119 missing); they stay buffered for the next.
    b.desync();
    b.apply(upd(120, 121, {{94, 4}}, {}));
    b.apply(upd(122, 123, {{93, 5}}, {}));
    eqRes(OrderBook::Result::NeedSnapshot, b.setSnapshot(snap(110, {{99, 1}}, {{101, 1}})), "snapshot too old");
    check(!b.synced(), "not synced on a too-old snapshot");
    eq<size_t>(2, b.bufferedCount(), "120-121 and 122-123 still buffered");
    eqRes(OrderBook::Result::Applied, b.setSnapshot(snap(119, {{99, 1}}, {{101, 1}})), "snapshot 119");
    near(4, b.bidQty(94), "120-121 applied after it");
    near(5, b.bidQty(93), "then 122-123");
    eq<int64_t>(123, b.lastUpdateId(), "last id 123");

    // Futures chain updates by pu (the previous u); the first after a snapshot only has to overlap it.
    OrderBook f;
    f.setSnapshot(snap(200, {{99, 1}}, {{101, 1}}));
    eqRes(OrderBook::Result::Applied, f.apply(upd(195, 205, {{98, 1}}, {}, T0, 190)), "futures: first overlaps 200");
    eqRes(OrderBook::Result::Applied, f.apply(upd(206, 210, {{97, 1}}, {}, T0, 205)), "futures: pu 205 = last u");
    eqRes(OrderBook::Result::NeedSnapshot, f.apply(upd(212, 215, {{96, 1}}, {}, T0, 211)), "futures: pu 211 ≠ 210");

    // The buffer is bounded: the oldest go.
    OrderBook c;
    for (size_t i = 0; i < OrderBook::MAX_BUFFERED + 10; ++i) c.apply(upd(int64_t(i), int64_t(i), {}, {}));
    eq<size_t>(OrderBook::MAX_BUFFERED, c.bufferedCount(), "buffer capped");
}

void testCrossed()
{
    CAT = "book/crossed";
    OrderBook b;
    b.setSnapshot(snap(10, {{99, 1}}, {{101, 1}}));
    // A bid at 101 reaches the best ask: a level was missed somewhere.
    eqRes(OrderBook::Result::Crossed, b.apply(upd(11, 11, {{101, 2}}, {})), "bid 101 vs ask 101");
    check(!b.synced(), "crossed: not synced");
    eq<uint64_t>(1, b.stats().crossed, "counted");
    Sampler s;
    s.advance(b, T0);
    check(s.advance(b, T0 + 5).empty(), "no column from a crossed book");
    eqRes(OrderBook::Result::Buffered, b.apply(upd(12, 12, {}, {{102, 1}})), "next update waits for a snapshot");
    eqRes(OrderBook::Result::Applied, b.setSnapshot(snap(12, {{100, 1}}, {{101, 1}})), "snapshot repairs it");
    check(b.synced(), "synced again");

    // A crossed snapshot (shouldn't happen) isn't trusted either.
    OrderBook c;
    eqRes(OrderBook::Result::Crossed, c.setSnapshot(snap(1, {{102, 1}}, {{101, 1}})), "crossed snapshot");
    check(!c.synced(), "not synced");
    // Bid at 100 and ask at 101: touching ticks, not crossed.
    eqRes(OrderBook::Result::Applied, c.setSnapshot(snap(2, {{100, 1}}, {{101, 1}})), "1-tick spread is fine");
}

void testTrim()
{
    CAT = "book/trim";
    OrderBook b;
    std::vector<Level> bids, asks;
    for (int64_t t = 0; t < 50; ++t) bids.push_back({t, 1});
    for (int64_t t = 51; t <= 100; ++t) asks.push_back({t, 1});
    b.setSnapshot(snap(1, bids, asks));
    // Keep 40..60: bids 40..49 (10), asks 51..60 (10).
    eq<size_t>(80, b.trim(50, 10), "80 levels dropped");
    eq<size_t>(20, b.levelCount(), "20 left");
    eq<int64_t>(40, b.bids().begin()->first, "lowest bid 40");
    eq<int64_t>(60, b.asks().rbegin()->first, "highest ask 60");
}

// ---------------------------------------------------------------- quantities as codes

void testCodes()
{
    CAT = "codes";
    eq<int>(0, encode(0), "0 → 0");
    eq<int>(0, encode(-1), "negative → 0");
    eq<int>(0, encode(NAN), "NaN → 0");
    near(0, decode(0), "0 ← 0");
    eq<int>(1, encode(1e-30), "tiny clamps to 1");
    eq<int>(65535, encode(1e40), "huge clamps to 65535");
    double worst = 0;
    bool monotonic = true;
    Code prev = 0;
    for (double l = -14; l <= 24; l += 0.0137) {
        const double q = std::pow(10.0, l);
        const Code c = encode(q);
        worst = std::max(worst, std::fabs(decode(c) - q) / q);
        if (c < prev) monotonic = false;
        prev = c;
    }
    check(worst <= CODE_TOL, QStringLiteral("worst relative error %1 ≤ 0.07 %").arg(worst));
    check(monotonic, "codes grow with the quantity");
    // 1 = 2^0 → code 1 + 48 × 512 = 24577, exact.
    eq<int>(24577, encode(1), "1 → 24577");
    near(1, decode(24577), "24577 → 1 exactly");
    std::cout << "  codes: worst error " << worst * 100 << " %\n";
}

void testAutoRows()
{
    CAT = "rows/auto";
    // 2e-5 × 100,000 $ = 2 $ = 200 ticks of 0.01.
    eq(200, autoStorageRowTicks(100000, 0.01), "BTCUSDT at 100k: 200 ticks (2 $)");
    // 2e-5 × 3,000 = 0.06 = 6 ticks of 0.01 → nearest 1-2-5: 5.
    eq(5, autoStorageRowTicks(3000, 0.01), "ETHUSDT at 3k: 5 ticks");
    // Already coarser than 2e-5: 1 tick.
    eq(1, autoStorageRowTicks(0.5, 0.0001), "a coarse tick: 1");
    eq(1, autoStorageRowTicks(0, 0.01), "no price: 1");
}

// ---------------------------------------------------------------- sampling

double qtyOf(const Column &c, int64_t row) { return decode(c.rows[size_t(row - c.meta.anchorRow)]); }

void testSampler()
{
    CAT = "sampler";
    OrderBook b;
    b.setSnapshot(snap(1, {{98, 1}, {99, 2}}, {{101, 3}, {102, 4}}));
    SamplerParams p;
    p.intervalMs = 250;
    p.rowTicks = 1;
    p.bandRows = 8;
    Sampler s(p);
    // The first call only starts the clock.
    check(s.advance(b, T0 + 0.1).empty(), "first call: no column");
    // T0+0.25 and T0+0.5 have passed: two columns, the same book.
    std::vector<Column> cols = s.advance(b, T0 + 0.6);
    eq<size_t>(2, cols.size(), "two sample times passed");
    if (cols.size() == 2) {
        near(T0 + 0.25, cols[0].meta.time, "first at T0+0.25");
        near(T0 + 0.5, cols[1].meta.time, "second at T0+0.5");
        // mid = (99 + 101) / 2 = 100 → band from 100 − 4 = 96 (rows 96..103).
        eq<int64_t>(96, cols[0].meta.anchorRow, "anchor 96");
        eq<int64_t>(99, cols[0].meta.bestBid, "best bid kept");
        eq<int64_t>(101, cols[0].meta.bestAsk, "best ask kept");
        near(2, cols[0].meta.bidQty, "quantity at the best bid (99) kept");
        near(3, cols[0].meta.askQty, "quantity at the best ask (101) kept");
        nearQty(1, qtyOf(cols[0], 98), "row 98 = 1");
        nearQty(2, qtyOf(cols[0], 99), "row 99 = 2");
        near(0, qtyOf(cols[0], 100), "row 100 empty");
        nearQty(3, qtyOf(cols[0], 101), "row 101 = 3");
        nearQty(4, qtyOf(cols[0], 102), "row 102 = 4");
        check(cols[0].rows == cols[1].rows, "nothing changed: same rows");
    }
    // An update at T0+0.8: the column of T0+0.75 is taken first (before it), so it still has bid 99 = 2.
    cols = s.advance(b, T0 + 0.8);
    b.apply(upd(2, 2, {{99, 0}}, {}, T0 + 0.8));
    eq<size_t>(1, cols.size(), "T0+0.75");
    if (cols.size() == 1) nearQty(2, qtyOf(cols[0], 99), "T0+0.75 is before the update");
    cols = s.advance(b, T0 + 1.3);
    eq<size_t>(2, cols.size(), "T0+1.0, T0+1.25");
    if (cols.size() == 2) {
        near(0, qtyOf(cols[0], 99), "T0+1.0 after the update: row 99 empty");
        eq<int64_t>(98, cols[0].meta.bestBid, "best bid now 98");
    }
    // No update for 1.75 s: 7 columns (1.5 … 3.0), each the same as the last one.
    const std::vector<Code> last = cols.empty() ? std::vector<Code>{} : cols.back().rows;
    cols = s.advance(b, T0 + 3.05);
    eq<size_t>(7, cols.size(), "7 columns without an update");
    bool same = true;
    for (const Column &c : cols) same = same && c.rows == last;
    check(same, "they repeat the previous state");
    if (!cols.empty()) near(T0 + 3.0, cols.back().meta.time, "last at T0+3.0");
    // Not synced: no column, those times are skipped (a hole), then it starts again.
    b.desync();
    check(s.advance(b, T0 + 4.0).empty(), "unsynced: nothing");
    b.setSnapshot(snap(10, {{99, 1}}, {{101, 1}}));
    cols = s.advance(b, T0 + 4.3);
    eq<size_t>(1, cols.size(), "only T0+4.25 after the resync");
    if (!cols.empty()) near(T0 + 4.25, cols[0].meta.time, "T0+4.25");
    // Time not moving or going back: nothing.
    check(s.advance(b, T0 + 4.3).empty(), "same time: nothing");
    check(s.advance(b, T0 + 2).empty(), "earlier time: nothing");

    // Catch-up capped: 10 s without anything, at most 3 columns, the last ones.
    SamplerParams q = p;
    q.maxCatchUp = 3;
    Sampler s2(q);
    s2.advance(b, T0);
    cols = s2.advance(b, T0 + 10);
    eq<size_t>(3, cols.size(), "capped at 3");
    if (cols.size() == 3) near(T0 + 9.5, cols[0].meta.time, "the last three: 9.5, 9.75, 10");
}

void testSampleRows()
{
    CAT = "sampler/rows";
    // Rows of 2 ticks: ticks 100 and 101 are both row 50 (sum 1 + 2); bid 97 → row 48, ask 103 → row 51.
    OrderBook b;
    b.setSnapshot(snap(1, {{97, 0.5}, {100, 1}}, {{101, 2}, {103, 4}}));
    Column c = Sampler::sample(b, T0, 46, 2, 8);
    nearQty(3, qtyOf(c, 50), "bid 100 + ask 101 in row 50 (bids and asks share a row)");
    nearQty(0.5, qtyOf(c, 48), "row 48");
    nearQty(4, qtyOf(c, 51), "row 51");
    // Levels outside the band (rows 46..53 = ticks 92..107) are left out.
    b.apply(upd(2, 2, {{91, 9}}, {{108, 9}}));
    c = Sampler::sample(b, T0, 46, 2, 8);
    double total = 0;
    for (Code r : c.rows) total += decode(r);
    nearQty(7.5, total, "only 0.5 + 1 + 2 + 4 in the band");

    // Negative ticks (a spread): tick −1 is row −1 with 2-tick rows (rounded down), tick −3 row −2.
    OrderBook n;
    n.setSnapshot(snap(1, {{-3, 1}}, {{-1, 2}}));
    c = Sampler::sample(n, T0, -4, 2, 6);
    nearQty(2, qtyOf(c, -1), "tick −1 → row −1");
    nearQty(1, qtyOf(c, -2), "tick −3 → row −2");

    // The band moves only when the price leaves its middle half: band 8 from 96 keeps 98 ≤ mid row < 102.
    SamplerParams p;
    p.bandRows = 8;
    Sampler s(p);
    OrderBook m;
    // A bid 50 ticks down: further than trimBands (4) × 8 rows = 32 ticks from the band's middle (100).
    m.setSnapshot(snap(1, {{50, 1}, {99, 1}}, {{101, 1}}));   // mid 100 → anchor 96
    s.advance(m, T0);
    near(1, m.bidQty(50), "far level still there before the first column");
    s.advance(m, T0 + 0.25);
    eq<int64_t>(96, s.anchorRow(), "anchor 96");
    near(0, m.bidQty(50), "the column trimmed the far level from the book");
    near(1, m.bidQty(99), "near levels stay");
    m.setSnapshot(snap(2, {{100, 1}}, {{102, 1}}));   // mid 101: inside, stays
    s.advance(m, T0 + 0.5);
    eq<int64_t>(96, s.anchorRow(), "mid 101: anchor stays 96");
    m.setSnapshot(snap(3, {{101, 1}}, {{103, 1}}));   // mid 102: out of the middle half → 102 − 4 = 98
    s.advance(m, T0 + 0.75);
    eq<int64_t>(98, s.anchorRow(), "mid 102: recentred at 98");
    m.setSnapshot(snap(4, {{95, 1}}, {{97, 1}}));     // mid 96 < 98 + 2 → 92
    s.advance(m, T0 + 1.0);
    eq<int64_t>(92, s.anchorRow(), "mid 96: recentred at 92");
    // One side empty: the other's best is the price.
    m.setSnapshot(snap(5, {}, {{95, 1}}));            // 95 inside 94..97: stays
    s.advance(m, T0 + 1.25);
    eq<int64_t>(92, s.anchorRow(), "asks only: their best is the price");
}

// ---------------------------------------------------------------- the store

Column col(double time, std::vector<double> q, int64_t anchor = 0)
{
    Column c;
    c.meta.time = time;
    c.meta.anchorRow = anchor;
    for (double v : q) c.rows.push_back(encode(v));
    return c;
}

void testStore()
{
    CAT = "store";
    ColumnStore st(3, 4);
    check(st.empty(), "empty");
    eq<uint64_t>(0, st.lowerBound(5), "lowerBound on empty = end");
    for (int i = 1; i <= 6; ++i) check(st.append(col(i, {double(i), 0, 1})), QStringLiteral("append %1").arg(i));
    // Capacity 4: columns 1 and 2 (indices 0, 1) are gone.
    eq<size_t>(4, st.size(), "size 4");
    eq<uint64_t>(2, st.firstIndex(), "first index 2");
    eq<uint64_t>(6, st.endIndex(), "end index 6");
    near(3, st.meta(2).time, "oldest kept: time 3");
    nearQty(6, decode(st.rows(5)[0]), "newest: 6");
    nearQty(1, decode(st.rows(3)[2]), "rows intact after wrapping");
    check(!st.append(col(6, {1, 1, 1})), "same time refused");
    check(!st.append(col(5.5, {1, 1, 1})), "earlier time refused");
    check(!st.append(col(7, {1, 1})), "wrong height refused");
    eq<uint64_t>(4, st.lowerBound(4.5), "first ≥ 4.5: index 4 (time 5)");
    eq<uint64_t>(2, st.lowerBound(0), "before everything: the oldest");
    eq<uint64_t>(6, st.lowerBound(10), "after everything: end");
    eq<uint64_t>(3, st.lowerBound(4), "exact time 4: index 3");

    // Memory stays put once full.
    const size_t full = st.bytes();
    for (int i = 7; i < 1000; ++i) st.append(col(i, {1, 2, 3}));
    eq<size_t>(full, st.bytes(), "bytes constant once full");

    // Fewer columns: the newest stay.
    ColumnStore s2(3, 4);
    for (int i = 1; i <= 6; ++i) s2.append(col(i, {double(i), 0, 0}));
    s2.setCapacity(2);
    eq<size_t>(2, s2.size(), "shrunk to 2");
    eq<uint64_t>(4, s2.firstIndex(), "first index 4");
    near(5, s2.meta(4).time, "time 5 kept");
    nearQty(6, decode(s2.rows(5)[0]), "column 6 kept");
    // More: what's there stays, new ones fill up to the capacity.
    s2.setCapacity(5);
    for (int i = 7; i <= 9; ++i) s2.append(col(i, {double(i), 0, 0}));
    eq<size_t>(5, s2.size(), "grown to 5");
    bool ok = true;
    for (uint64_t k = s2.firstIndex(); k < s2.endIndex(); ++k)
        ok = ok && s2.meta(k).time == double(k + 1) && std::fabs(decode(s2.rows(k)[0]) - double(k + 1)) < 0.01;
    check(ok, "times 5..9 in order after both resizes");
    s2.append(col(10, {10, 0, 0}));
    near(6, s2.meta(s2.firstIndex()).time, "then it wraps as usual");

    // The real size: 800 rows, 2 hours at 250 ms = 28,800 columns.
    const size_t real = ColumnStore::bytesFor(800, 28800);
    std::cout << "  store: 800 rows x 28,800 columns (2 h at 250 ms) = " << real / 1e6 << " MB ("
              << double(real) / 2 / 1e6 << " MB an hour; at 100 ms: " << double(real) * 2.5 / 2 / 1e6 << " MB an hour)\n";
    ColumnStore big(800, 28800);
    Column c = col(0, std::vector<double>(800, 1.0));
    QElapsedTimer fill;
    fill.start();
    for (int i = 0; i < 30000; ++i) {
        c.meta.time = i;
        big.append(c);
    }
    // Filling grows the memory in steps: a column must not copy the whole store (it did: 11 s for 2 hours).
    const double fillMs = double(fill.nsecsElapsed()) / 1e6;
    check(fillMs < 1000, QStringLiteral("filling 30,000 columns took %1 ms (< 1 s)").arg(fillMs));
    std::cout << "  store: filling 30,000 columns of 800 rows: " << fillMs << " ms\n";
    check(big.bytes() <= real + 1024, QStringLiteral("filled store holds %1 bytes, ≤ %2").arg(big.bytes()).arg(real));
}

// ---------------------------------------------------------------- grouping

void testGrouping()
{
    CAT = "grouping";
    // Rows 10..15 hold 1..6.
    const Column c = col(0, {1, 2, 3, 4, 5, 6}, 10);
    std::vector<double> out;
    // 4 rows a group: group 2 = rows 8..11 (only 10, 11 here: 1 + 2), group 3 = rows 12..15 (3 + 4 + 5 + 6).
    eq<int64_t>(2, groupRows(c.rows.data(), 6, 10, 4, out), "first group 2");
    eq<size_t>(2, out.size(), "two groups");
    if (out.size() == 2) {
        nearQty(3, out[0], "group 2 = 3");
        nearQty(18, out[1], "group 3 = 18");
    }
    // 2 rows: 5 = 10+11 → 3, 6 = 12+13 → 7, 7 = 14+15 → 11.
    eq<int64_t>(5, groupRows(c.rows.data(), 6, 10, 2, out), "first group 5");
    if (out.size() == 3) {
        nearQty(3, out[0], "rows 10-11");
        nearQty(7, out[1], "rows 12-13");
        nearQty(11, out[2], "rows 14-15");
    } else
        check(false, "three groups of 2");
    // 1 row: the rows themselves.
    groupRows(c.rows.data(), 6, 10, 1, out);
    eq<size_t>(6, out.size(), "6 groups of 1");
    if (out.size() == 6) nearQty(4, out[3], "row 13 = 4");
    // Groups don't depend on the anchor: rows −3..0 hold 1..4; groups of 2 are (−4, −3) (−2, −1) (0, 1).
    const Column n = col(0, {1, 2, 3, 4}, -3);
    eq<int64_t>(-2, groupRows(n.rows.data(), 4, -3, 2, out), "first group −2");
    if (out.size() == 3) {
        nearQty(1, out[0], "rows −4, −3");
        nearQty(5, out[1], "rows −2, −1");
        nearQty(4, out[2], "rows 0, 1");
    } else
        check(false, "three groups over negative rows");
    // The total is the same whatever the grouping.
    double total = 0;
    groupRows(c.rows.data(), 6, 10, 3, out);
    for (double v : out) total += v;
    nearQty(21, total, "1 + … + 6 = 21 in groups of 3");
}

// ---------------------------------------------------------------- the pyramid

void feed(ColumnPyramid &p, const Column &c) { p.add(c.meta, c.rows.data()); }

void testPyramid()
{
    CAT = "pyramid";
    // 250 ms samples: level 1 = 1 s buckets (4 samples), level 2 = 4 s (16).
    ColumnPyramid mx(3, 250, 1000, Agg::Max, 2), mean(3, 250, 1000, Agg::Mean, 2);
    eq<int64_t>(4, mx.samplesPer(1), "4 samples a level-1 column");
    eq<int64_t>(16, mx.samplesPer(2), "16 a level-2 column");
    check(std::isinf(mx.closedUntil(1)), "nothing yet: closed until −inf");
    // Second T0: rows (1,0,4) (2,0,3) (3,0,2) (6,0,1); max = (6,0,4), mean = (3,0,2.5).
    const std::vector<std::vector<double>> q = {{1, 0, 4}, {2, 0, 3}, {3, 0, 2}, {6, 0, 1}};
    for (int i = 0; i < 4; ++i) {
        const Column c = col(T0 + 0.25 * i, q[size_t(i)], 10);
        feed(mx, c);
        feed(mean, c);
    }
    eq<size_t>(0, mx.level(1).size(), "the bucket is still open");
    near(T0, mx.closedUntil(1), "level 1 closed until T0 (its open bucket)");
    // The first sample of the next second closes it.
    const Column next = col(T0 + 1.0, {1, 1, 1}, 10);
    feed(mx, next);
    feed(mean, next);
    eq<size_t>(1, mx.level(1).size(), "one level-1 column");
    if (mx.level(1).size() == 1 && mean.level(1).size() == 1) {
        const uint64_t i = mx.level(1).firstIndex();
        near(T0, mx.level(1).meta(i).time, "at the bucket's start");
        eq<uint32_t>(4, mx.level(1).meta(i).count, "4 samples in it");
        const Code *m = mx.level(1).rows(i);
        nearQty(6, decode(m[0]), "max row 0 = 6");
        near(0, decode(m[1]), "max row 1 = 0");
        nearQty(4, decode(m[2]), "max row 2 = 4");
        const Code *a = mean.level(1).rows(mean.level(1).firstIndex());
        // Means of decoded codes: within twice the code error.
        check(std::fabs(decode(a[0]) - 3) <= 2 * CODE_TOL * 3, QStringLiteral("mean row 0 = 3 (%1)").arg(decode(a[0])));
        check(std::fabs(decode(a[2]) - 2.5) <= 2 * CODE_TOL * 2.5, QStringLiteral("mean row 2 = 2.5 (%1)").arg(decode(a[2])));
    }
    near(T0 + 1, mx.closedUntil(1), "now closed until T0+1");

    // A hole: only 2 samples in second T0+1 (1.0 above and 1.5 = 3): the mean is over those 2, 2 for row 0.
    feed(mean, col(T0 + 1.5, {3, 3, 3}, 10));
    feed(mean, col(T0 + 2.0, {0, 0, 0}, 10));
    if (mean.level(1).size() == 2) {
        const uint64_t i = mean.level(1).endIndex() - 1;
        eq<uint32_t>(2, mean.level(1).meta(i).count, "2 samples (holes left out)");
        check(std::fabs(decode(mean.level(1).rows(i)[0]) - 2) <= 2 * CODE_TOL * 2, "mean of 1 and 3 = 2");
    } else
        check(false, "two level-1 columns");

    // The band moves inside a bucket: the bucket follows the newest band. Rows 10..12 hold 5, 0, 7, then rows 11..13
    // hold 3, 2, 0: row 10 (5) has no place in the new band, row 12 keeps max(7, 2) = 7 (old index 2 → new index 1).
    ColumnPyramid r(3, 250, 1000, Agg::Max, 1);
    feed(r, col(T0, {5, 0, 7}, 10));
    feed(r, col(T0 + 0.25, {3, 2, 0}, 11));
    feed(r, col(T0 + 1, {0, 0, 0}, 11));
    if (r.level(1).size() == 1) {
        const uint64_t i = r.level(1).firstIndex();
        eq<int64_t>(11, r.level(1).meta(i).anchorRow, "newest band");
        const Code *rows = r.level(1).rows(i);
        nearQty(3, decode(rows[0]), "row 11 = 3 (only the second sample had it)");
        nearQty(7, decode(rows[1]), "row 12 = 7, moved from the old band");
        near(0, decode(rows[2]), "row 13 empty");
    } else
        check(false, "one column after re-anchoring");

    // Level 2 (4 s) of a minute of samples: 15 buckets, 14 closed (the last one is open), 16 samples each.
    ColumnPyramid big(1, 250, 10000, Agg::Max, 2);
    for (int i = 0; i < 240; ++i) {
        Column c = col(T0 + 0.25 * i, {double(i % 16 + 1)}, 0);
        big.add(c.meta, c.rows.data());
    }
    eq<size_t>(14, big.level(2).size(), "14 closed 4-second columns (the 15th is open)");
    eq<size_t>(59, big.level(1).size(), "59 closed seconds");
    bool all16 = true;
    for (uint64_t i = big.level(2).firstIndex(); i < big.level(2).endIndex(); ++i)
        all16 = all16 && std::fabs(decode(big.level(2).rows(i)[0]) - 16) < 0.02 && big.level(2).meta(i).count == 16;
    check(all16, "each 4-second column: max 16 of 16 samples");
    near(T0 + 56, big.closedUntil(2), "level 2 closed until T0+56");

    // Capacity follows the store's: 1000 samples → level 1 keeps ⌈1000/4⌉ + 1 = 251, level 2 ⌈1000/16⌉ + 1 = 64.
    ColumnPyramid cap(1, 250, 1000, Agg::Max, 2);
    for (int i = 0; i < 20000; ++i) {
        Column c = col(T0 + 0.25 * i, {1}, 0);
        cap.add(c.meta, c.rows.data());
    }
    eq<size_t>(251, cap.level(1).size(), "level 1 bounded");
    eq<size_t>(64, cap.level(2).size(), "level 2 bounded");
    std::cout << "  pyramid: 6 levels over 800 rows x 28,800 columns (2 h at 250 ms) = "
              << double(ColumnPyramid(800, 250, 28800, Agg::Max).bytes() + 0) / 1e6 << " MB empty; full ~"
              << double(ColumnStore::bytesFor(800, 28800)) / 3 / 1e6 << " MB (a third of the store)\n";
}

// ---------------------------------------------------------------- normalisation

void testNormalisation()
{
    CAT = "normalisation";
    CodeHistogram h;
    eq(0.0, h.percentile(95), "no cell: 0");
    for (int v = 1; v <= 100; ++v) h.add(encode(v));
    for (int i = 0; i < 50; ++i) h.add(Code(0));   // empty rows don't count
    eq<uint64_t>(100, h.count(), "100 cells, zeros left out");
    // Nearest rank: p95 of 1..100 = the 95th smallest = 95.
    nearQty(95, h.percentile(95), "p95 = 95");
    nearQty(50, h.percentile(50), "p50 = 50");
    nearQty(100, h.percentile(100), "p100 = 100");
    nearQty(1, h.percentile(0), "p0 = the smallest");
    // A heavy tail doesn't move p95 much: one 1e6 among them.
    h.add(encode(1e6));
    nearQty(96, h.percentile(95), "101 cells: rank ceil(95.95) = 96 → 96");
    h.clear();
    eq<uint64_t>(0, h.count(), "cleared");

    Look L;   // gain 1, gamma 1, threshold 0.02
    near(0.5, intensity(5, 10, L), "5 of 10 = 0.5");
    near(1, intensity(20, 10, L), "twice the reference: clamped to 1");
    near(0, intensity(0.1, 10, L), "0.01 × ref < threshold 0.02: not drawn");
    near(0.03, intensity(0.3, 10, L), "0.03 × ref ≥ threshold: drawn");
    near(0, intensity(5, 0, L), "no reference: nothing");
    near(0, intensity(0, 10, L), "no quantity: nothing");
    Look G = L;
    G.gain = 2;
    near(0.4, intensity(2, 10, G), "gain 2: 2 × 0.2 = 0.4");
    near(1, intensity(6, 10, G), "gain 2: 1.2 clamped to 1");
    G.gain = 10;
    near(0, intensity(0.1, 10, G), "the threshold ignores the gain: 0.01 still hidden");
    Look C = L;
    C.gamma = 2;
    near(0.25, intensity(5, 10, C), "contrast 2: 0.5² = 0.25");
    C.gamma = 0.5;
    near(0.5, intensity(2.5, 10, C), "contrast 0.5: √0.25 = 0.5");

    IntensityLut lut;
    Look C2 = L;
    C2.gamma = 2;
    lut.build(10, C2);
    eq<int>(0, lut[0], "LUT: empty → 0");
    // 0.5² × 255 = 63.75 → 64.
    eq<int>(64, lut.ofQty(5), "LUT: 5 → 64");
    eq<int>(255, lut.ofQty(100), "LUT: above the reference → 255");
    eq<int>(0, lut.ofQty(0.1), "LUT: under the threshold → 0");
    // 0.025² × 255 = 0.16, drawn: at least 1.
    eq<int>(1, lut.ofQty(0.25), "LUT: a faint cell still drawn (1)");
    bool mono = true;
    for (int c = 2; c < 65536; ++c) mono = mono && lut[Code(c)] >= lut[Code(c - 1)];
    check(mono, "LUT grows with the code");

    QElapsedTimer t;
    t.start();
    for (int i = 0; i < 20; ++i) lut.build(10 + i, C2);
    std::cout << "  normalisation: LUT build " << double(t.nsecsElapsed()) / 20 / 1e3 << " µs\n";
}

// ---------------------------------------------------------------- stability

void testStable()
{
    CAT = "stable";
    ColumnStore st(4, 100);
    // Rows 10..13, 250 ms apart: (5,5,0,5) (4,5,5,5) (6,1,5,5).
    st.append(col(T0, {5, 5, 0, 5}, 10));
    st.append(col(T0 + 0.25, {4, 5, 5, 5}, 10));
    st.append(col(T0 + 0.5, {6, 1, 5, 5}, 10));
    std::vector<Code> out;
    // Over 3 samples at the third: the smallest of each row = (4, 1, 0, 5).
    stableColumn(st, 2, 3, 250, out);
    nearQty(4, decode(out[0]), "row 10: min(5,4,6) = 4");
    nearQty(1, decode(out[1]), "row 11: min(5,5,1) = 1");
    near(0, decode(out[2]), "row 12: empty at the first sample → 0");
    nearQty(5, decode(out[3]), "row 13: 5 throughout");
    // 2 samples at the second: (4, 5, 0, 5).
    stableColumn(st, 1, 2, 250, out);
    nearQty(4, decode(out[0]), "2 samples: row 10 = 4");
    near(0, decode(out[2]), "2 samples: row 12 = 0");
    // 3 samples at the second: the window reaches before the store → nothing.
    stableColumn(st, 1, 3, 250, out);
    check(std::all_of(out.begin(), out.end(), [](Code c) { return c == 0; }), "window before the store: all 0");
    // 1 sample: the column itself.
    stableColumn(st, 1, 1, 250, out);
    nearQty(4, decode(out[0]), "1 sample: itself");
    // A hole: the next sample comes 0.5 s later (one missing) → nothing over 2 samples.
    st.append(col(T0 + 1.0, {7, 7, 7, 7}, 10));
    stableColumn(st, 3, 2, 250, out);
    check(std::all_of(out.begin(), out.end(), [](Code c) { return c == 0; }), "over a hole: all 0");
    // The band moves: rows 11..14 hold 5 each; the sample before had rows 10..13 at 7. Over 2: rows 11–13 = 5,
    // row 14 (outside the older band) = 0.
    st.append(col(T0 + 1.25, {5, 5, 5, 5}, 11));
    stableColumn(st, 4, 2, 250, out);
    nearQty(5, decode(out[0]), "moved band: row 11 = min(5, 7)");
    nearQty(5, decode(out[2]), "moved band: row 13 = min(5, 7)");
    near(0, decode(out[3]), "moved band: row 14 wasn't in the older band → 0");
}

// ---------------------------------------------------------------- pulled / absorbed

// A column of 20 rows from 90 (ticks = rows: rowTicks 1): every row 1 but those given, the best bid and ask.
Column book(double t, int64_t bid, int64_t ask, std::vector<std::pair<int64_t, double>> levels = {}, float bidQty = 1,
            float askQty = 1)
{
    std::vector<double> q(20, 1.0);
    for (auto [row, v] : levels) q[size_t(row - 90)] = v;
    Column c = col(t, q, 90);
    c.meta.bestBid = bid;
    c.meta.bestAsk = ask;
    c.meta.bidQty = bidQty;
    c.meta.askQty = askQty;
    return c;
}
// The tests' thresholds (worked out with these; the app's defaults are stricter for the big level, looser for absorption).
EventParams testParams()
{
    EventParams p;
    p.bigFactor = 3;
    p.absorbRatio = 2;
    return p;
}
void add(EventDetector &d, const Column &c) { d.addColumn(c.meta, c.rows.data(), int(c.rows.size())); }
size_t countKind(const EventDetector &d, LiquidityEvent::Kind k)
{
    return size_t(std::count_if(d.events().begin(), d.events().end(), [k](const LiquidityEvent &e) { return e.kind == k; }));
}

void testPulled()
{
    CAT = "events/pulled";
    using K = LiquidityEvent::Kind;
    // A bid of 10 at row 95 (5 rows under the best bid 100, median 1 → big), gone at T0+0.25 without a trade, then the
    // best bid comes down 100 → 99 → 97 → 95: pulled at T0+0.25, reached at T0+1.0, 10 − 1 = 9 gone.
    {
        EventDetector d(1, testParams());
        add(d, book(T0, 100, 101, {{95, 10}}));
        add(d, book(T0 + 0.25, 100, 101));
        add(d, book(T0 + 0.5, 99, 100));
        add(d, book(T0 + 0.75, 97, 98));
        eq<size_t>(0, d.events().size(), "not reached yet");
        add(d, book(T0 + 1.0, 95, 96));
        eq<size_t>(1, countKind(d, K::Pulled), "pulled once reached");
        if (!d.events().empty()) {
            const LiquidityEvent &e = d.events().front();
            eq<int64_t>(95, e.row, "at row 95");
            eq(1, e.side, "a bid");
            near(T0 + 0.25, e.t0, "went at T0+0.25");
            near(T0 + 1.0, e.t1, "reached at T0+1.0");
            nearQty(9, e.qty, "9 went");
        }
    }
    // Filled, not pulled: 8 sold at 95 during the drop (8 ≥ 0.3 × 9).
    {
        EventDetector d(1, testParams());
        add(d, book(T0, 100, 101, {{95, 10}}));
        d.addTrade(T0 + 0.1, 95, 8, footprint::Side::Sell);
        add(d, book(T0 + 0.25, 100, 101));
        add(d, book(T0 + 0.5, 95, 96));
        eq<size_t>(0, d.events().size(), "traded away: not pulled");
    }
    // The price never gets there within 5 s.
    {
        EventDetector d(1, testParams());
        add(d, book(T0, 100, 101, {{95, 10}}));
        for (int i = 1; i <= 24; ++i) add(d, book(T0 + 0.25 * i, 100, 101));   // 6 s at 100
        add(d, book(T0 + 6.25, 95, 96));
        eq<size_t>(0, d.events().size(), "not reached within 5 s: nothing");
    }
    // Too far: 15 rows under the touch (nearRows 10). Band rows 90..109: best bid 105, level at 90.
    {
        EventDetector d(1, testParams());
        add(d, book(T0, 105, 106, {{90, 10}}));
        add(d, book(T0 + 0.25, 105, 106));
        add(d, book(T0 + 0.5, 90, 91));
        eq<size_t>(0, d.events().size(), "too far from the touch: nothing");
    }
    // It comes back before the price gets there (moved, not pulled for good).
    {
        EventDetector d(1, testParams());
        add(d, book(T0, 100, 101, {{95, 10}}));
        add(d, book(T0 + 0.25, 100, 101));
        add(d, book(T0 + 0.5, 100, 101, {{95, 10}}));
        add(d, book(T0 + 0.75, 95, 96, {{95, 10}}));
        eq<size_t>(0, d.events().size(), "came back: nothing");
    }
    // An ask of 10 at row 106 (5 above the best ask 101), gone, then the best ask goes up to 106.
    {
        EventDetector d(1, testParams());
        add(d, book(T0, 100, 101, {{106, 10}}));
        add(d, book(T0 + 0.25, 100, 101));
        add(d, book(T0 + 0.5, 105, 106));
        eq<size_t>(1, countKind(d, K::Pulled), "an ask pulled");
        if (!d.events().empty()) eq(-1, d.events().front().side, "an ask");
    }
    // A small level (2.5 < big = 3 × median 1) gone for good and reached: nothing (the same with 10 is pulled).
    {
        EventDetector d(1, testParams());
        add(d, book(T0, 100, 101, {{95, 2.5}}));
        add(d, book(T0 + 0.25, 100, 101, {{95, 0.1}}));
        add(d, book(T0 + 0.5, 95, 96, {{95, 0.1}}));
        eq<size_t>(0, d.events().size(), "under 3 × median: not big, nothing");
    }
}

void testAbsorbed()
{
    CAT = "events/absorbed";
    using K = LiquidityEvent::Kind;
    auto run = [](int64_t endBid, double sellPerInterval, int64_t sellTick, footprint::Side side) {
        EventDetector d(1, testParams());
        // Best bid 100 holding 5 for 1 s; 3 sold into it every 250 ms (12 ≥ 2 × 5).
        for (int i = 0; i < 5; ++i) {
            if (i) d.addTrade(T0 + 0.25 * i - 0.1, sellTick, sellPerInterval, side);
            add(d, book(T0 + 0.25 * i, 100, 101, {}, 5, 1));
        }
        d.addTrade(T0 + 1.15, sellTick, sellPerInterval, side);
        add(d, book(T0 + 1.25, endBid, endBid + 1, {}, 5, 1));
        return d;
    };
    {
        // 5 intervals at 100 (T0 … T0+1.25): 5 × 3 = 15 sold, mean 5 resting; the bid then goes up to 101: held.
        EventDetector d = run(101, 3, 100, footprint::Side::Sell);
        eq<size_t>(1, countKind(d, K::Absorbed), "absorbed: held with 15 ≥ 2 × 5");
        if (!d.events().empty()) {
            const LiquidityEvent &e = d.events().front();
            eq<int64_t>(100, e.tick, "at tick 100");
            eq(1, e.side, "a bid");
            near(T0, e.t0, "from T0");
            near(T0 + 1.25, e.t1, "to T0+1.25");
            nearQty(15, e.traded, "15 sold into it");
            near(5, e.qty, "5 resting on average", 1e-6);
        }
    }
    eq<size_t>(0, run(99, 3, 100, footprint::Side::Sell).events().size(), "broke through (bid to 99): nothing");
    eq<size_t>(0, run(101, 1.5, 100, footprint::Side::Sell).events().size(), "7.5 < 2 × 5: nothing");
    eq<size_t>(0, run(101, 3, 99, footprint::Side::Sell).events().size(), "sells at another tick: nothing");
    eq<size_t>(0, run(101, 3, 100, footprint::Side::Buy).events().size(), "buys don't hit the bid: nothing");
    // Too short: one interval at 100 (0.25 s < 0.5 s).
    {
        EventDetector d(1, testParams());
        add(d, book(T0, 100, 101, {}, 1, 1));
        d.addTrade(T0 + 0.1, 100, 50, footprint::Side::Sell);
        add(d, book(T0 + 0.25, 101, 102, {}, 1, 1));
        eq<size_t>(0, d.events().size(), "0.25 s at the touch: too short");
    }
    // The ask side: best ask 101 holding 4, 3 bought into it per interval for 4 intervals (12 ≥ 8), then down to 100.
    {
        EventDetector d(1, testParams());
        for (int i = 0; i < 4; ++i) {
            if (i) d.addTrade(T0 + 0.25 * i - 0.1, 101, 3, footprint::Side::Buy);
            add(d, book(T0 + 0.25 * i, 100, 101, {}, 1, 4));
        }
        d.addTrade(T0 + 0.9, 101, 3, footprint::Side::Buy);
        add(d, book(T0 + 1.0, 99, 100, {}, 1, 4));
        eq<size_t>(1, countKind(d, K::Absorbed), "ask absorbed, the price went back down");
        if (!d.events().empty()) {
            eq(-1, d.events().front().side, "an ask");
            nearQty(12, d.events().front().traded, "12 bought into it");
        }
    }
    // Bounded.
    EventDetector d(1, testParams());
    d.maxEvents = 3;
    for (int k = 0; k < 6; ++k) {
        const double t = T0 + 10 * k;
        add(d, book(t, 100, 101, {{95, 10}}));
        add(d, book(t + 0.25, 100, 101));
        add(d, book(t + 0.5, 95, 96));
        add(d, book(t + 5, 100, 101));
    }
    eq<size_t>(3, d.events().size(), "at most maxEvents kept");
}

// ---------------------------------------------------------------- recordings

bool sameLevels(const std::vector<Level> &a, const std::vector<Level> &b)
{
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (a[i].tick != b[i].tick || std::fabs(a[i].qty - b[i].qty) > 1e-12) return false;
    return true;
}

void testRecordFormat()
{
    using namespace qtm::depthrec;
    CAT = "record/format";
    Header h{"BTCUSDT", 0.01, false};
    Writer w;
    w.reset(T0);
    DepthSnapshot s = snap(1000, {{-5, 0.00000001}, {9'999'999'999, 123.45678901}}, {{10'000'000'000, 2}});
    w.snapshot(T0 + 0.1, s);
    w.update(upd(1001, 1003, {{100, 0}, {99, 1.5}}, {{101, 0.25}}, T0 + 0.2));          // spot: no pu
    w.update(upd(1004, 1004, {}, {{-3, 7}}, T0 + 0.3, 1003));                           // futures pu
    const QByteArray first = w.takeBlock();
    w.lost(T0 + 0.4);
    OrderBook b;
    b.setSnapshot(snap(5, {{98, 1}, {99, 2}}, {{101, 3}}));
    w.checkpoint(T0 + 0.5, b);
    const QByteArray second = w.takeBlock();
    check(w.takeBlock().isEmpty(), "nothing left: empty block");
    const QByteArray file = encodeHeader(h) + first + second;

    Reader r(file);
    check(r.ok(), "header read");
    check(r.header().ticker == "BTCUSDT" && r.header().tickSize == 0.01 && !r.header().us, "header: pair, tick, exchange");
    Record x;
    check(r.next(x) && x.type == Type::Reset && x.time == T0, "reset");
    check(r.next(x) && x.type == Type::Snapshot && x.snapshot.lastUpdateId == 1000, "snapshot id");
    check(sameLevels(s.bids, x.snapshot.bids) && sameLevels(s.asks, x.snapshot.asks),
          "snapshot levels exact (negative and 10-digit ticks, 1e-8 quantities)");
    check(r.next(x) && x.type == Type::Update && x.update.firstId == 1001 && x.update.lastId == 1003 && x.update.prevLastId == -1,
          "spot update ids, no pu");
    check(x.update.bids.size() == 2 && x.update.bids[0].qty == 0 && x.update.bids[1].tick == 99 && x.update.bids[1].qty == 1.5,
          "update levels, removal as 0");
    near(T0 + 0.2, x.update.time, "update time");
    check(r.next(x) && x.type == Type::Update && x.update.prevLastId == 1003 && x.update.asks[0].tick == -3, "futures pu kept");
    check(r.next(x) && x.type == Type::Lost && x.time == T0 + 0.4, "lost");
    check(r.next(x) && x.type == Type::Checkpoint && x.snapshot.lastUpdateId == 5 && x.snapshot.bids.size() == 2
              && x.snapshot.asks.size() == 1 && x.snapshot.bids[1].qty == 2,
          "checkpoint of a book");
    check(!r.next(x) && !r.truncated(), "the end, nothing cut");

    // Cut in the middle of the second block (the app stopped while writing): the first block's records, then the end.
    Reader cut(encodeHeader(h) + first + second.left(second.size() / 2));
    int n = 0;
    while (cut.next(x)) ++n;
    eq(4, n, "cut file: the 4 records of the complete block");
    check(cut.truncated(), "cut file: says so");
    // Cut right after a block's first 2 bytes of length.
    Reader cut2(encodeHeader(h) + first + second.left(2));
    n = 0;
    while (cut2.next(x)) ++n;
    check(n == 4 && cut2.truncated(), "cut inside a block's length: the first block, then says so");
    // Damaged bytes inside a block: it stops there, no crash.
    QByteArray bad = file;
    for (qsizetype i = encodeHeader(h).size() + 10; i < encodeHeader(h).size() + 30; ++i) bad[i] = char(bad[i] ^ 0x5a);
    Reader damaged(bad);
    n = 0;
    while (damaged.next(x) && n < 100) ++n;
    check(n < 100 && damaged.truncated(), QStringLiteral("damaged block: stops (%1 records)").arg(n));
    check(!Reader(QByteArray("not a recording")).ok(), "not a recording: refused");
    QByteArray otherVersion = encodeHeader(h);
    otherVersion[8] = char(2);
    check(!Reader(otherVersion).ok(), "another format version: refused");
}

// A session as the live feed runs it (updates buffered before the snapshot, a hole and a resync, a lost socket),
// recorded, then replayed: the same columns, sample for sample.
void testRecordReplay()
{
    using namespace qtm::depthrec;
    CAT = "record/replay";
    std::mt19937_64 rng(11);
    auto uni = [&](int64_t a, int64_t b) { return std::uniform_int_distribution<int64_t>(a, b)(rng); };
    SamplerParams sp;
    sp.intervalMs = 250;
    sp.rowTicks = 2;
    sp.bandRows = 64;
    OrderBook live;
    Sampler sampler(sp);
    std::vector<Column> liveCols;
    Writer w;
    QByteArray file = encodeHeader({"BTCUSDT", 0.01, false});
    auto sample = [&](double t) {
        for (Column &c : sampler.advance(live, t)) liveCols.push_back(std::move(c));
    };
    auto send = [&](const DepthUpdate &u) {   // what DepthWorker::onMessage does, recording first
        w.update(u);
        sample(u.time);
        live.apply(u);
    };
    w.reset(T0);
    int64_t id = 100;
    int64_t mid = 1000;
    auto randomUpdate = [&](double t) {
        DepthUpdate u;
        u.time = t;
        const int n = int(uni(1, 6));
        for (int j = 0; j < n; ++j) {
            const bool bid = uni(0, 1) == 0;
            const int64_t tick = bid ? mid - uni(1, 50) : mid + uni(1, 50);
            (bid ? u.bids : u.asks).push_back({tick, uni(0, 3) == 0 ? 0.0 : double(uni(1, 900000)) * 1e-5});
        }
        u.firstId = id + 1;
        u.lastId = id + n;
        id = u.lastId;
        return u;
    };
    std::vector<Level> b0, a0;
    for (int k = 1; k <= 40; ++k) {
        b0.push_back({mid - k, double(k) * 0.1});
        a0.push_back({mid + k, double(k) * 0.2});
    }
    // Two updates arrive before the snapshot (buffered), then the snapshot, then 600 updates.
    double t = T0 + 0.05;
    send(randomUpdate(t));
    send(randomUpdate(t += 0.1));
    DepthSnapshot s = snap(id - 1, b0, a0);   // overlaps the second update
    w.snapshot(t, s);
    live.setSnapshot(s);
    for (int i = 0; i < 600; ++i) {
        send(randomUpdate(t += 0.1));
        if (i % 100 == 99) file += w.takeBlock();
        if (i == 300) {   // a hole: ids skip, then a fresh snapshot
            id += 5;
            send(randomUpdate(t += 0.1));
            DepthSnapshot s2;
            s2.lastUpdateId = id;
            for (const auto &[tk, q] : live.bids()) s2.bids.push_back({tk, q});
            for (const auto &[tk, q] : live.asks()) s2.asks.push_back({tk, q});
            w.snapshot(t, s2);
            live.setSnapshot(s2);
        }
        if (i == 450) {   // the socket goes 0.6 s after the last update (2 sample times), then comes back with a snapshot
            sample(t += 0.6);
            w.lost(t);
            live.desync();
            send(randomUpdate(t += 2.0));
            DepthSnapshot s3;
            s3.lastUpdateId = id;
            for (const auto &[tk, q] : live.bids()) s3.bids.push_back({tk, q});
            for (const auto &[tk, q] : live.asks()) s3.asks.push_back({tk, q});
            w.snapshot(t, s3);
            live.setSnapshot(s3);
        }
    }
    file += w.takeBlock();
    // A second session (the app started again later, a new sampler) with the price 10 ticks up: still inside the
    // band's middle half, so a sampler carried over would keep its old anchor where a new one recentres.
    {
        Sampler fresh(sp);
        sampler = fresh;
        t += 60;
        w.reset(t);
        live.reset();
        mid += 10;
        std::vector<Level> b1, a1;
        for (int k = 1; k <= 40; ++k) {
            b1.push_back({mid - k, double(k) * 0.1});
            a1.push_back({mid + k, double(k) * 0.2});
        }
        DepthSnapshot s4 = snap(id, b1, a1);
        w.snapshot(t, s4);
        live.setSnapshot(s4);
        for (int i = 0; i < 100; ++i) send(randomUpdate(t += 0.1));
        file += w.takeBlock();
    }

    Reader r(file);
    Replayer rp(sp);
    Record x;
    int records = 0;
    while (r.next(x)) {
        rp.feed(x);
        ++records;
    }
    const std::vector<Column> cols = rp.take();
    check(liveCols.size() > 200, QStringLiteral("the session made columns (%1)").arg(liveCols.size()));
    eq(liveCols.size(), cols.size(), "replay: as many columns");
    bool same = cols.size() == liveCols.size();
    for (size_t i = 0; same && i < cols.size(); ++i)
        same = cols[i].rows == liveCols[i].rows && cols[i].meta.time == liveCols[i].meta.time
               && cols[i].meta.anchorRow == liveCols[i].meta.anchorRow && cols[i].meta.bestBid == liveCols[i].meta.bestBid
               && cols[i].meta.bestAsk == liveCols[i].meta.bestAsk && cols[i].meta.bidQty == liveCols[i].meta.bidQty;
    check(same, "replay: every column identical to live");
    check(rp.book().synced() == live.synced() && rp.book().lastUpdateId() == live.lastUpdateId(), "replay: the same book at the end");
    eq(live.stats().gaps, rp.book().stats().gaps, "replay: the same holes");
    size_t levels = 0;
    {
        Reader again(file);
        while (again.next(x))
            if (x.type == Type::Update) levels += x.update.bids.size() + x.update.asks.size();
    }
    std::cout << "  record: " << records << " records, " << levels << " update levels, " << file.size() << " bytes ("
              << double(file.size()) / double(levels) << " bytes a level, compressed, snapshots included)\n";
}

// ---------------------------------------------------------------- palettes

void testPalettes()
{
    CAT = "palettes";
    const Rgb dark{19, 17, 28}, light{255, 255, 255};
    for (PaletteKind k : {PaletteKind::Deep, PaletteKind::Thermal, PaletteKind::Mono}) {
        const QString name = k == PaletteKind::Deep ? "deep" : k == PaletteKind::Thermal ? "thermal" : "mono";
        const auto pal = buildPalette(paletteStops(k, dark), dark);
        eq<uint32_t>(0xff13111cu, pal[0], name + ": index 0 is the background");
        // Brighter with more liquidity on a dark background: luminance never goes down (but for 8-bit rounding: Deep
        // dips by 0.0002 at two steps where blue falls faster than green rises).
        bool mono = true;
        for (int i = 2; i < 256; ++i) mono = mono && luminance(pal[size_t(i)]) >= luminance(pal[size_t(i - 1)]) - 0.002;
        check(mono, name + ": luminance grows with the intensity");
        check(luminance(pal[255]) > 0.75, name + ": the top is near white");
        check(luminance(pal[1]) < 0.05, name + ": the faintest is near the background");
        bool opaque = true;
        for (uint32_t c : pal) opaque = opaque && (c >> 24) == 0xff;
        check(opaque, name + ": opaque");
    }
    // Deep's top stop is fully opaque: 255 is the stop's colour exactly.
    eq<uint32_t>(0xffffffebu, buildPalette(paletteStops(PaletteKind::Deep, dark), dark)[255], "deep: top = (255,255,235)");
    // Thermal at 0.35 (index ≈ 89): (185,25,15) at 72 % over the background.
    const auto th = buildPalette(paletteStops(PaletteKind::Thermal, dark), dark);
    const double v89 = 89 / 255.0;   // just under 0.35: between the first two stops, f = (v − 0) / 0.35
    const double f = v89 / 0.35, al = 0.35 + (0.72 - 0.35) * f;
    const double rr = (70 + (185 - 70) * f) * al + 19 * (1 - al);
    eq<uint32_t>(uint32_t(std::lround(rr)), (th[89] >> 16) & 0xff, "thermal: red channel at index 89 worked out");
    // On a light background every preset darkens with more liquidity (white walls would vanish), the top dark.
    for (PaletteKind k : {PaletteKind::Deep, PaletteKind::Thermal, PaletteKind::Mono}) {
        const QString name = k == PaletteKind::Deep ? "deep" : k == PaletteKind::Thermal ? "thermal" : "mono";
        const auto pal = buildPalette(paletteStops(k, light), light);
        bool down = true;
        for (int i = 2; i < 256; ++i) down = down && luminance(pal[size_t(i)]) <= luminance(pal[size_t(i - 1)]) + 0.002;
        check(down, name + " on light: luminance falls with the intensity");
        check(luminance(pal[255]) < 0.05, name + " on light: the top is near black");
        check(luminance(pal[1]) > 0.6, name + " on light: the faintest is near the background");
    }
    // Mono picks black ink on a light background: darker with more liquidity.
    const auto ml = buildPalette(paletteStops(PaletteKind::Mono, light), light);
    eq<uint32_t>(0xffffffffu, ml[0], "mono on light: background white");
    eq<uint32_t>(0xff000000u, ml[255], "mono on light: top black");
    check(luminance(ml[128]) < luminance(ml[1]), "mono on light: darker as it grows");
    // Custom: the three colours at 0, ½, 1 (low at 35 % over the background, high opaque).
    const Rgb lo{0, 0, 255}, mi{0, 255, 0}, hi{255, 0, 0};
    const auto cu = buildPalette(paletteStops(PaletteKind::Custom, dark, lo, mi, hi), dark);
    eq<uint32_t>(0xffff0000u, cu[255], "custom: top = high");
    // Index 1 (v = 1/255): almost low at 35 %: blue ≈ 255 × 0.35 + 28 × 0.65 = 107.45 (a hair of mid in it).
    check(std::abs(int(cu[1] & 0xff) - 107) <= 2, QStringLiteral("custom: index 1 is low over the background (%1)").arg(cu[1] & 0xff));
    // Index 128 (v ≈ 0.502): mid at ~75 %: green ≈ 255 × 0.75 + 17 × 0.25 ≈ 196.
    check(std::abs(int((cu[128] >> 8) & 0xff) - 196) <= 3, QStringLiteral("custom: index 128 is mid (%1)").arg((cu[128] >> 8) & 0xff));
    // No stops: everything is the background.
    eq<uint32_t>(0xff13111cu, buildPalette({}, dark)[200], "no stops: background");
}

// ---------------------------------------------------------------- load

// 1,000 updates a second for a simulated minute (each 1 to 20 levels, a fifth of them deletions, the price walking):
// the book matches a reference copy, the store stays at its capacity, the book bounded, and the time per update in
// the last seconds is the same as in the first ones.
void testLoad()
{
    CAT = "load";
    std::mt19937_64 rng(42);
    auto uni = [&](int64_t a, int64_t b) { return std::uniform_int_distribution<int64_t>(a, b)(rng); };
    auto qty = [&] { return std::uniform_real_distribution<double>(0.001, 5)(rng); };

    SamplerParams sp;
    sp.intervalMs = 100;
    sp.rowTicks = 10;
    sp.bandRows = 800;
    sp.trimBands = 2;   // levels further than 16,000 ticks from the band's middle go
    Sampler sampler(sp);
    ColumnStore store(sp.bandRows, 300);   // 30 s of 100 ms columns: the minute's 600 wrap it

    std::map<int64_t, double> tb, ta;   // what the book should be
    int64_t mid = 1'000'000;
    DepthSnapshot s0;
    s0.lastUpdateId = 1000;
    for (int64_t k = 1; k <= 2000; ++k) {
        tb[mid - k] = qty();
        ta[mid + k] = qty();
    }
    for (auto &[t, q] : tb) s0.bids.push_back({t, q});
    for (auto &[t, q] : ta) s0.asks.push_back({t, q});
    s0.time = 1700000000;
    OrderBook book;
    book.setSnapshot(s0);
    sampler.advance(book, s0.time);
    int64_t id = 1000;

    constexpr int SECONDS = 60, PER_SEC = 1000;
    std::vector<double> usPerUpdate(SECONDS);
    size_t maxLevels = 0, bytesAt40 = 0, notApplied = 0, levelsTotal = 0;
    double worstUs = 0;
    for (int sec = 0; sec < SECONDS; ++sec) {
        // This second's updates, made first (not timed).
        std::vector<DepthUpdate> ups(PER_SEC);
        for (int i = 0; i < PER_SEC; ++i) {
            DepthUpdate &u = ups[size_t(i)];
            u.time = s0.time + sec + (i + 1) / 1000.0;
            if (uni(0, 19) == 0) {   // the price moves; levels it crosses are taken
                const int64_t nm = mid + uni(-20, 20);
                while (!ta.empty() && ta.begin()->first <= nm) {
                    u.asks.push_back({ta.begin()->first, 0});
                    ta.erase(ta.begin());
                }
                while (!tb.empty() && tb.rbegin()->first >= nm) {
                    u.bids.push_back({tb.rbegin()->first, 0});
                    tb.erase(std::prev(tb.end()));
                }
                mid = nm;
            }
            const int n = int(uni(1, 20));
            for (int j = 0; j < n; ++j) {
                const bool bid = uni(0, 1) == 0;
                const int64_t tick = bid ? mid - uni(1, 3000) : mid + uni(1, 3000);
                const double q = uni(0, 4) == 0 ? 0 : qty();
                auto &side = bid ? tb : ta;
                if (q > 0)
                    side[tick] = q;
                else
                    side.erase(tick);
                (bid ? u.bids : u.asks).push_back({tick, q});
            }
            const int64_t levels = int64_t(u.bids.size() + u.asks.size());
            levelsTotal += size_t(levels);
            u.firstId = id + 1;
            u.lastId = id + levels;
            id = u.lastId;
        }
        QElapsedTimer t;
        t.start();
        for (const DepthUpdate &u : ups) {
            for (const Column &c : sampler.advance(book, u.time)) store.append(c);
            if (book.apply(u) != OrderBook::Result::Applied) ++notApplied;
        }
        usPerUpdate[size_t(sec)] = double(t.nsecsElapsed()) / PER_SEC / 1e3;
        worstUs = std::max(worstUs, usPerUpdate[size_t(sec)]);
        maxLevels = std::max(maxLevels, book.levelCount());
        if (sec == 40) bytesAt40 = store.bytes();
    }
    eq<size_t>(0, notApplied, "every update applied");
    // The book matches the reference near the price (further away, trim() may have dropped levels).
    size_t diff = 0;
    for (auto *pair : {&tb, &ta}) {
        const auto &bookSide = pair == &tb ? book.bids() : book.asks();
        for (auto it = pair->lower_bound(mid - 5000); it != pair->end() && it->first <= mid + 5000; ++it) {
            const auto f = bookSide.find(it->first);
            if (f == bookSide.end() || f->second != it->second) ++diff;
        }
        for (auto it = bookSide.lower_bound(mid - 5000); it != bookSide.end() && it->first <= mid + 5000; ++it)
            if (!pair->count(it->first)) ++diff;
    }
    eq<size_t>(0, diff, "book = reference within 5,000 ticks of the price");
    // 60 s of 100 ms columns: 600, of which the store keeps 300.
    check(store.endIndex() >= 599 && store.endIndex() <= 600, QStringLiteral("~600 columns (%1)").arg(store.endIndex()));
    eq<size_t>(300, store.size(), "store at its capacity");
    eq<size_t>(bytesAt40, store.bytes(), "store memory the same at 40 s and 60 s");
    check(maxLevels <= 2 * 16000 + 1, QStringLiteral("book bounded by the trim (max %1 levels)").arg(maxLevels));
    double first = 0, lastS = 0;
    for (int i = 1; i <= 10; ++i) first += usPerUpdate[size_t(i)] / 10;
    for (int i = 50; i < 60; ++i) lastS += usPerUpdate[size_t(i)] / 10;
    check(lastS < 2 * first + 0.5,
          QStringLiteral("time per update constant: %1 µs (s 1-10) vs %2 µs (s 50-59)").arg(first).arg(lastS));
    std::cout << "  load: " << SECONDS * PER_SEC << " updates (" << levelsTotal << " levels), "
              << QString::number(first, 'f', 2).toStdString() << " µs/update in s 1-10, "
              << QString::number(lastS, 'f', 2).toStdString() << " µs in s 50-59, worst second "
              << QString::number(worstUs, 'f', 2).toStdString() << " µs; book max " << maxLevels << " levels; store "
              << store.size() << " columns = " << store.bytes() / 1e3 << " kB\n";

    // The stability filter and the pyramid over 2 hours of 800-row columns (what DepthFeed rebuilds on a setting change).
    {
        ColumnStore big(800, 28800);
        Column c = col(0, std::vector<double>(800, 1.0));
        std::mt19937 r2(7);
        for (int i = 0; i < 28800; ++i) {
            c.meta.time = T0 + 0.25 * i;
            for (Code &x : c.rows) x = encode(1 + double(r2() % 100));
            big.append(c);
        }
        std::vector<Code> out;
        QElapsedTimer tt;
        tt.start();
        for (uint64_t i = big.firstIndex(); i < big.endIndex(); ++i) stableColumn(big, i, 9, 250, out);
        const double stableMs = double(tt.nsecsElapsed()) / 1e6;
        tt.restart();
        ColumnPyramid pyr(800, 250, 28800, Agg::Max);
        for (uint64_t i = big.firstIndex(); i < big.endIndex(); ++i) pyr.add(big.meta(i), big.rows(i));
        const double pyrMax = double(tt.nsecsElapsed()) / 1e6;
        tt.restart();
        ColumnPyramid pyrMean(800, 250, 28800, Agg::Mean);
        for (uint64_t i = big.firstIndex(); i < big.endIndex(); ++i) pyrMean.add(big.meta(i), big.rows(i));
        const double pyrMeanMs = double(tt.nsecsElapsed()) / 1e6;
        std::cout << "  load: 2 h of 800-row columns: stability filter (9 samples) " << stableMs << " ms, pyramid max " << pyrMax
                  << " ms, mean " << pyrMeanMs << " ms\n";
    }

    // One column of a 4,000-level book into 800 rows, alone.
    QElapsedTimer t;
    t.start();
    const int N = 2000;
    for (int i = 0; i < N; ++i) Sampler::sample(book, 0, sampler.anchorRow(), sp.rowTicks, sp.bandRows);
    std::cout << "  load: one column (" << book.levelCount() << " levels, 800 rows) " << double(t.nsecsElapsed()) / N / 1e3
              << " µs\n";
}

} // namespace

int main(int argc, char **argv)
{
    const bool loadOnly = argc > 1 && std::strcmp(argv[1], "load") == 0;
    if (!loadOnly) {
        testBookLevels();
        testSequence();
        testCrossed();
        testTrim();
        testCodes();
        testAutoRows();
        testSampler();
        testSampleRows();
        testStore();
        testGrouping();
        testPyramid();
        testNormalisation();
        testPalettes();
        testStable();
        testPulled();
        testAbsorbed();
        testRecordFormat();
        testRecordReplay();
    }
    testLoad();
    std::cout << checks << " checks, " << fails << " failed\n";
    return fails ? 1 : 0;
}
