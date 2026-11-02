#pragma once

// Big Trades: large aggressive orders, found in the trade stream (a Deepwater addition to the Charts tab, not in
// Quant Terminal). Pure code, no network and no painting: the feed (ui/TradeFeed) classifies the trades
// (footprint::TradeClassifier) and hands them over in time order, the chart layer (ui/BigTradesLayer) draws what
// comes out.
//
//   trades → Aggregator (one aggressive order split into several executions → one aggregate)
//          → Detector   (is the aggregate big? manual min/max, or a rolling percentile × a volatility factor)
//          → BigTrade
//
// Binance's aggTrades already merge the fills of one order at one price and one instant; the aggregation here also
// joins an order that sweeps several prices, or a burst of orders within the window.

#include "Footprint.h"

#include <deque>
#include <optional>
#include <vector>

namespace qtm::bigtrades {

using footprint::Classified;
using footprint::Side;
using footprint::Trade;

// ---------------------------------------------------------------- aggregation

enum class PriceMode {
    SamePrice,   // one price only
    Sweep,       // prices moving one way: up for a buy (lifting the asks), down for a sell
};

struct AggregationParams {
    double windowMs = 50;          // trades less than this after the aggregate's first one join it
    bool exactTimestamp = false;   // instead: only trades with the first one's exact millisecond
    PriceMode priceMode = PriceMode::SamePrice;
};

struct Aggregate {
    double time = 0;          // the first execution, UTC seconds
    double lastTime = 0;
    int64_t firstSeq = 0, lastSeq = 0;
    Side side = Side::Unknown;
    double volume = 0;
    double notional = 0;      // Σ price × qty, for the volume-weighted average price
    double minPrice = 0, maxPrice = 0, lastPrice = 0;
    int executions = 0;
    bool approximate = false; // a side was found by quote or tick rule, not given by the feed

    double vwap() const { return volume > 0 ? notional / volume : NaN; }
};

// Feed it classified trades in time order. A trade that can't join the open aggregate closes it (returned) and opens
// the next one. Trades whose side couldn't be found are never joined and close as aggregates of unknown side.
class Aggregator
{
public:
    explicit Aggregator(AggregationParams p = {}) : m_p(p) {}

    const AggregationParams &params() const { return m_p; }
    std::optional<Aggregate> add(const Trade &t, const Classified &c);
    // The open aggregate, closed: at the end of a run, or live once `now` is past its window (nothing can join it
    // any more, so it shouldn't wait for the next trade to show).
    std::optional<Aggregate> flush();
    std::optional<Aggregate> flushIfDone(double now);
    const std::optional<Aggregate> &open() const { return m_open; }

    bool joins(const Aggregate &a, const Trade &t, Side side) const;

private:
    AggregationParams m_p;
    std::optional<Aggregate> m_open;
};

// ---------------------------------------------------------------- rolling percentile

// The values of the last `windowSeconds`, kept sorted beside a queue in arrival order for the ones leaving. Sorted in
// blocks of ~512 (a binary search finds the block, the memmove stays inside it): a value in or out costs about a
// microsecond with tens of thousands in the window, never a full sort. Percentiles are interpolated linearly between
// ranks (numpy's default): with n sorted values, p sits at rank (n − 1) × p / 100.
class RollingPercentile
{
public:
    explicit RollingPercentile(double windowSeconds = 1800) : m_window(windowSeconds) {}

    void setWindow(double seconds) { m_window = seconds; }
    double window() const { return m_window; }
    void add(double time, double value);   // times never go back
    void expire(double now);               // drops the values older than now − window
    size_t size() const { return m_size; }
    double percentile(double p) const;     // NaN when empty
    void clear();

private:
    double m_window;
    void insertSorted(double v);
    void eraseSorted(double v);
    double at(size_t k) const;   // the k-th smallest

    std::deque<std::pair<double, double>> m_fifo;   // time, value
    std::vector<std::vector<double>> m_blocks;      // each sorted, and every block ≤ the next
    size_t m_size = 0;
};

// ---------------------------------------------------------------- volatility

// One-minute bars built from the trades (so the threshold doesn't depend on the chart's timeframe), and the ratio of
// a short to a long average true range: ATR(n) = mean of the last n closed minutes' true ranges, a true range being
// max(high − low, |high − previous close|, |low − previous close|). Minutes without a trade have no bar.
class MinuteAtr
{
public:
    MinuteAtr(int shortBars = 14, int longBars = 120) : m_short(shortBars), m_long(longBars) {}

    void add(double time, double price);
    int closedBars() const { return int(m_tr.size()); }
    bool ready() const { return closedBars() >= m_long; }   // enough closed minutes for the long ATR
    double atr(int bars) const;   // NaN without enough closed minutes
    // clamp(ATR short / ATR long, 0.5, 2); 1 until `longBars` minutes have closed (or a flat market).
    double factor() const;

private:
    int m_short, m_long;
    bool m_hasBar = false;
    double m_minute = 0, m_open = 0, m_high = 0, m_low = 0, m_close = 0;
    bool m_hasPrevClose = false;
    double m_prevClose = 0;
    std::deque<double> m_tr;   // the last `longBars` closed minutes' true ranges
};

// ---------------------------------------------------------------- detection

struct ThresholdParams {
    bool automatic = true;
    double minVolume = 0;       // manual: the threshold. Auto: the fallback while the window fills (0 = nothing shown)
    double maxVolume = 0;       // aggregates above it aren't shown (0 = no maximum); in both modes, for stacking tranches
    double percentile = 99;
    double windowMinutes = 30;
    bool volatility = true;     // auto threshold × MinuteAtr::factor()
    int atrShort = 14, atrLong = 120;
    int minSamples = 100;       // aggregates in the window before the percentile is trusted
};

struct Threshold {
    double value = NaN;         // what an aggregate must reach (NaN: nothing is big yet)
    bool automatic = false;
    bool warmingUp = false;     // auto, but under minSamples: the manual fallback is used
    double percentileValue = NaN;
    double volatilityFactor = 1;
    bool volatilityReady = false;   // false: 1 until atrLong minutes of trades have closed
    size_t samples = 0;
};

struct BigTrade {
    Aggregate a;
    double threshold = NaN;     // the one it was judged against
};

// Aggregates, then judges every aggregate when it closes, against the threshold of that moment (computed without it,
// so the history never looks ahead); only then does its size go into the rolling window.
class Detector
{
public:
    explicit Detector(AggregationParams agg = {}, ThresholdParams thr = {});

    // Trades in time order. Returns the big trade this one completed, if any.
    std::optional<BigTrade> add(const Trade &t, const Classified &c);
    std::optional<BigTrade> flush();
    std::optional<BigTrade> flushIfDone(double now);

    Threshold threshold() const;
    bool passes(double volume, const Threshold &thr) const;
    const ThresholdParams &params() const { return m_thr; }
    const Aggregator &aggregator() const { return m_agg; }
    int64_t aggregates() const { return m_aggregates; }
    // The biggest aggregate judged so far, big or not: the bubbles' size reference, the same for every tranche
    // (Big Trades 0.5-2 and Big Trades ≥ 2 on one chart draw a 2 the same size).
    double maxVolume() const { return m_maxVolume; }

private:
    std::optional<BigTrade> judge(const Aggregate &a);

    Aggregator m_agg;
    ThresholdParams m_thr;
    RollingPercentile m_window;
    MinuteAtr m_atr;
    int64_t m_aggregates = 0;
    double m_maxVolume = 0;
};

// ---------------------------------------------------------------- bubbles

struct BubbleStyle {
    double minRadius = 4, maxRadius = 24;   // px
    double minOpacity = 0.35;
};

// Area proportional to volume: r = maxRadius × √(volume / refVolume), kept between the two radii. refVolume is the
// biggest big trade loaded (so a bubble doesn't change size while scrolling); refVolume ≤ 0 gives minRadius.
double bubbleRadius(double volume, double refVolume, const BubbleStyle &s);
// minOpacity + (1 − minOpacity) × √(volume / refVolume), at most 1.
double bubbleOpacity(double volume, double refVolume, const BubbleStyle &s);

} // namespace qtm::bigtrades
