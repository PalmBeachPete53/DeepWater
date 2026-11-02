#pragma once

// Footprint (order flow) aggregation: a Deepwater addition to the Charts tab, not in Quant Terminal.
//
// For every candle, the traded volume per price row, split by the aggressor's side:
//   bid volume = aggressive sells (trades hitting the bid), ask volume = aggressive buys (lifting the ask).
// Prices are kept as whole ticks (int64) so rows never drift with floating point; a row is `rowTicks`
// ticks (the grouping). Pure code: no network, no painting. The feed (ui/FootprintFeed) classifies and
// buckets the trades, the chart layer (ui/FootprintLayer) only reads what is here.

#include "Bar.h"

#include <cstdint>
#include <map>
#include <vector>

namespace qtm::footprint {

// ---------------------------------------------------------------- prices as ticks

// a / b rounded down (C++ `/` rounds toward zero, wrong for negative prices: spreads).
inline int64_t floorDiv(int64_t a, int64_t b)
{
    const int64_t q = a / b;
    return (a % b != 0 && ((a < 0) != (b < 0))) ? q - 1 : q;
}

struct TickGrid {
    double tickSize = 0.01;
    int rowTicks = 1;   // grouping: 1, 2, 4… ticks per row

    // The nearest whole tick: 0.1 + 0.2 at a 0.1 tick is 3, not 2.
    int64_t toTick(double price) const { return std::llround(price / tickSize); }
    int64_t rowOfTick(int64_t tick) const { return floorDiv(tick, rowTicks); }
    int64_t rowOf(double price) const { return rowOfTick(toTick(price)); }
    double rowLow(int64_t row) const { return double(row * rowTicks) * tickSize; }   // bottom price of a row
    double rowHeight() const { return rowTicks * tickSize; }
};

// A grouping that gives a typical candle (the median range of the last 100 bars) about `targetRows` rows:
// the nearest 1-2-5 × 10^k number of ticks. 1 when there are no bars to judge from.
int autoRowTicks(const Bars &bars, double tickSize, int targetRows = 24);

// ---------------------------------------------------------------- trade side

enum class Side : int8_t { Unknown = 0, Buy = 1, Sell = -1 };   // the aggressor

// How the side is found, best first. Each mode falls back to the next one when a trade lacks what it needs.
enum class ClassifyMode {
    Aggressor,   // the feed says (Binance aggTrade: m = buyer is maker → the seller was the aggressor)
    Quote,       // against the best bid/ask at the trade (Lee-Ready): ≥ ask buy, ≤ bid sell, else vs the mid
    TickRule,    // up = buy, down = sell, unchanged = same side as the trade before
};

inline Side binanceSide(bool buyerIsMaker) { return buyerIsMaker ? Side::Sell : Side::Buy; }

struct Trade {
    double time = 0;          // UTC seconds
    int64_t seq = 0;          // the feed's trade id: orders trades of the same millisecond
    double price = 0;
    double qty = 0;
    Side side = Side::Unknown;   // from the feed, when it gives it
    double bid = NaN, ask = NaN; // best quote when the trade printed, when known
};

struct Classified {
    Side side = Side::Unknown;
    bool approximate = false;   // found by quote or tick rule, not given by the feed
};

// Stateful (the tick rule remembers the last price and side): feed it the trades in time order.
class TradeClassifier
{
public:
    explicit TradeClassifier(ClassifyMode mode = ClassifyMode::Aggressor, double tickSize = 0.01)
        : m_mode(mode), m_grid{tickSize, 1} {}

    ClassifyMode mode() const { return m_mode; }
    Classified classify(const Trade &t);
    void reset();

private:
    Side tickRule(int64_t tick) const;

    ClassifyMode m_mode;
    TickGrid m_grid;
    bool m_hasLast = false;
    int64_t m_lastTick = 0;
    Side m_lastSide = Side::Unknown;
};

// ---------------------------------------------------------------- one candle

struct Level {
    double bid = 0;   // aggressive sells
    double ask = 0;   // aggressive buys
    double volume() const { return bid + ask; }
    double delta() const { return ask - bid; }
};

// A trade as stored: classified, its price in ticks.
struct StoredTrade {
    double time = 0;
    int64_t seq = 0;
    int64_t tick = 0;
    double qty = 0;
    Side side = Side::Unknown;
    bool approximate = false;
};

class CandleFootprint
{
public:
    // One trade: touches its row and the candle's totals only (amortised O(1): rows are a dense array that
    // grows at either end with room to spare). Order doesn't matter: open/close go by (time, seq).
    void add(const StoredTrade &t, int64_t row);

    bool empty() const { return m_trades == 0; }
    int64_t lowRow() const { return m_lowRow; }     // lowest and highest row that traded
    int64_t highRow() const { return m_highRow; }
    // A row's volumes; zero for a row inside the candle nothing traded at, and for rows outside it.
    Level level(int64_t row) const;

    double bidVolume() const { return m_bid; }
    double askVolume() const { return m_ask; }
    double delta() const { return m_ask - m_bid; }
    // Trades whose side couldn't be found (the very first one under the tick rule): in the candle's volume, in
    // no row's bid or ask.
    double unclassifiedVolume() const { return m_unknown; }
    double volume() const { return m_bid + m_ask + m_unknown; }

    int64_t openTick() const { return m_open.tick; }
    int64_t closeTick() const { return m_close.tick; }
    int64_t highTick() const { return m_highTick; }
    int64_t lowTick() const { return m_lowTick; }
    int64_t trades() const { return m_trades; }
    int64_t approximateTrades() const { return m_approx; }
    // Bumped by every add(): what the chart layer keys its cached analysis on.
    uint64_t version() const { return m_version; }

private:
    struct Stamp {
        double time = 0;
        int64_t seq = 0;
        int64_t tick = 0;
    };
    std::vector<Level> m_levels;   // m_levels[i] is row m_base + i
    int64_t m_base = 0;
    int64_t m_lowRow = 0, m_highRow = 0;
    int64_t m_lowTick = 0, m_highTick = 0;
    Stamp m_open, m_close;
    double m_bid = 0, m_ask = 0, m_unknown = 0;
    int64_t m_trades = 0, m_approx = 0;
    uint64_t m_version = 0;
};

// ---------------------------------------------------------------- what the chart shows of a candle

struct Params {
    double ratio = 3.0;       // diagonal imbalance: one side ≥ ratio × the other side one row away
    double minVolume = 0;     // rows whose stronger side is below this never count as imbalanced
    int stackCount = 3;       // this many imbalances in a row, same side, make a stacked imbalance zone
    // Instead of minVolume: the candle's own average volume per row that traded. Thin rows of a busy candle don't
    // count, and the threshold of a candle doesn't change with what else is on screen.
    bool autoMinVolume = false;
};

enum ImbalanceFlag : uint8_t { NoImbalance = 0, BuyImbalance = 1, SellImbalance = 2 };

struct Zone {
    int64_t fromRow = 0, toRow = 0;   // inclusive
    Side side = Side::Unknown;
};

struct Analysis {
    int64_t lowRow = 0, highRow = 0;
    int64_t pocRow = 0;           // the row with the most volume (ties: nearest the candle's middle, then lowest)
    double pocVolume = 0;
    double maxLevelVolume = 0;    // for the profile's bar lengths
    double maxAbsLevelDelta = 0;  // for the delta colours' intensity
    double delta = 0, volume = 0;
    std::vector<uint8_t> imbalance;   // ImbalanceFlag bits, one per row from lowRow to highRow
    std::vector<Zone> stacks;         // bottom to top

    uint8_t imbalanceAt(int64_t row) const
    {
        return row < lowRow || row > highRow ? 0 : imbalance[size_t(row - lowRow)];
    }
};

// Diagonal imbalances, compared across one row:
//   buy at P  when ask(P) ≥ ratio × bid(P − 1 row)
//   sell at P when bid(P) ≥ ratio × ask(P + 1 row)
// with the stronger side > 0 and ≥ minVolume (or the candle's average row volume, autoMinVolume). A row inside the candle that didn't trade counts as 0; the row
// below the candle's low (and above its high) is no comparison at all, so the lowest row has no buy imbalance
// and the highest no sell imbalance.
Analysis analyze(const CandleFootprint &c, const Params &params);

// ---------------------------------------------------------------- every candle

// The candles' footprints, by bar open time, and the trades they were built from: changing the grouping
// rebuilds the rows from those trades without fetching anything again.
class Book
{
public:
    explicit Book(TickGrid grid = {}) : m_grid(grid) {}

    const TickGrid &grid() const { return m_grid; }
    void setRowTicks(int rowTicks);   // regroups every candle

    // A classified trade into the candle that opens at `candleTime`.
    const CandleFootprint &add(double candleTime, const Trade &t, const Classified &c);
    const CandleFootprint *candle(double candleTime) const;
    const std::map<double, CandleFootprint> &candles() const { return m_candles; }

    // Forget the candles (and their trades) that open before `time`.
    void dropBefore(double time);
    // Forget the oldest candles, whole, until at most `maxTrades` trades are left. Returns the first candle kept
    // (NaN when none is).
    double trimToTrades(size_t maxTrades);
    void clear();

    size_t tradeCount() const { return m_trades.size(); }
    bool approximate() const { return m_approx > 0; }
    int64_t approximateTrades() const { return m_approx; }

private:
    struct Entry {
        double candleTime;
        StoredTrade t;
    };
    TickGrid m_grid;
    std::map<double, CandleFootprint> m_candles;
    std::vector<Entry> m_trades;
    int64_t m_approx = 0;
};

// A volume as short as it reads: 0.00005, 0.045, 2.35, 12.3, 845, 12.3K, 1.2M (two significant digits at least
// below 1, so a thin market's 0.00005 isn't shown as 0).
QString formatVolume(double v);

// ---------------------------------------------------------------- candles in time

// The bar a trade at `time` belongs to: the last open time ≤ time (openTimes ascending). -1 before the first.
int candleIndex(const Values &openTimes, double time);
// The open time of the `tfSeconds` candle holding `time` (intraday, UTC-aligned, like Binance's klines).
inline double candleOpen(double time, double tfSeconds) { return std::floor(time / tfSeconds) * tfSeconds; }

} // namespace qtm::footprint
