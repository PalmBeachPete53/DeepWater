#pragma once

// The liquidity heatmap's data (a Deepwater addition to the Charts tab, not in Quant Terminal): the order book
// (OrderBook.h) sampled at a fixed interval into columns, each column the resting quantity per price row at that
// instant, kept in a ring of columns with a bounded memory. Pure code: no network, no painting. The feed
// (ui/DepthFeed) runs the book and the sampler, the chart layer (ui/HeatmapLayer) reads the store.
//
//   updates → OrderBook → Sampler (every 250 ms: the book's state then, summed into rows) → Column → ColumnStore
//
// A row is `rowTicks` ticks (the storage resolution: BTCUSDT's 0.01 tick is far too fine for a heatmap, so ~2 $ there,
// see autoStorageRowTicks); the display can group rows further. A column keeps `bandRows` rows around the price
// (the band), anchored on a row that only moves when the price nears the band's edge, so columns usually line up.
// Bids and asks go in the same rows (liquidity is liquidity); the best bid and ask are kept with each column.
// Quantities are stored as 16-bit codes on a log scale (encode/decode): 0.07 % at most off, from 1e-14 to 1e24.

#include "OrderBook.h"

#include <array>
#include <cstdint>
#include <limits>
#include <vector>

namespace qtm::heatmap {

using depth::OrderBook;

// ---------------------------------------------------------------- quantities as 16-bit codes

// 0 ↔ 0; otherwise code = 1 + round((log2(q) + 48) × 512), clamped to [1, 65535]: 512 steps per doubling, so a
// decoded quantity is within 2^(1/1024) − 1 ≈ 0.068 % of the original. Codes compare like the quantities (max of codes
// = code of the max).
using Code = uint16_t;
constexpr int CODE_STEPS = 512;
constexpr int CODE_LOG_MIN = -48;
Code encode(double qty);
float decode(Code c);              // a table lookup
const float *decodeTable();        // the 65536 decoded values

// a / b rounded down (rows of negative prices: spreads).
inline int64_t floorDiv(int64_t a, int64_t b)
{
    const int64_t q = a / b;
    return (a % b != 0 && ((a < 0) != (b < 0))) ? q - 1 : q;
}

// The storage row for a symbol: the 1-2-5 × 10^k number of ticks nearest to `relative` × price (2e-5: 2 $ at
// 100,000 $), at least 1 tick.
int autoStorageRowTicks(double price, double tickSize, double relative = 2e-5);

// ---------------------------------------------------------------- columns

constexpr int64_t NO_TICK = std::numeric_limits<int64_t>::min();

struct ColumnMeta {
    double time = 0;            // the sample's time, UTC seconds (a multiple of the interval)
    int64_t anchorRow = 0;      // the band's lowest row (absolute: tick / rowTicks rounded down)
    int64_t bestBid = NO_TICK;  // ticks; NO_TICK = that side was empty
    int64_t bestAsk = NO_TICK;
    uint32_t count = 1;         // samples behind it: 1 for a sample, those it reduces for a pyramid column
    float bidQty = 0, askQty = 0;   // the quantity at the best bid and the best ask themselves (rows mix both sides)
};

struct Column {
    ColumnMeta meta;
    std::vector<Code> rows;     // bandRows codes, rows[i] = row meta.anchorRow + i
};

// ---------------------------------------------------------------- sampling

struct SamplerParams {
    int intervalMs = 250;
    int rowTicks = 1;
    int bandRows = 800;
    // Levels this many band heights or more from the price are dropped from the book (OrderBook::trim) at each column:
    // the book stays bounded. 0 = never.
    int trimBands = 4;
    // At most this many columns for one advance(): a book left without updates for longer only gets the last ones.
    int maxCatchUp = 2400;
};

class Sampler
{
public:
    explicit Sampler(const SamplerParams &p = {}) : m_p(p) {}
    const SamplerParams &params() const { return m_p; }

    // The columns of every sample time T (a multiple of the interval) with last < T ≤ now: the book as it is, so call
    // it with an update's time BEFORE applying that update (the columns before it then show the book before it) and
    // from a timer (no update for a while: the columns repeat the state). Nothing while the book isn't synced: those
    // sample times are skipped, the heatmap has a hole there. The first call only sets the clock.
    std::vector<Column> advance(OrderBook &book, double now);

    // One column of the book now, in the band from `anchorRow` (used by advance; public for the tests).
    static Column sample(const OrderBook &book, double time, int64_t anchorRow, int rowTicks, int bandRows);
    int64_t anchorRow() const { return m_anchor; }
    bool anchored() const { return m_anchored; }
    void reset();

private:
    // Keeps the band where it is while the price stays in its middle half, recentres it otherwise.
    bool updateAnchor(const OrderBook &book);

    SamplerParams m_p;
    int64_t m_lastK = 0;   // the last sample time, in intervals since the epoch
    bool m_started = false;
    bool m_anchored = false;
    int64_t m_anchor = 0;
    std::vector<double> m_scratch;
};

// ---------------------------------------------------------------- the store

// A ring of columns, oldest first: once full, each new column replaces the oldest. Columns have an absolute index
// (0 for the first column ever appended) so a reader can tell which ones it has already seen.
class ColumnStore
{
public:
    ColumnStore(int bandRows, size_t capacity);

    int bandRows() const { return m_band; }
    size_t capacity() const { return m_cap; }
    size_t size() const { return m_size; }
    bool empty() const { return m_size == 0; }
    uint64_t firstIndex() const { return m_end - m_size; }   // absolute index of the oldest column kept
    uint64_t endIndex() const { return m_end; }              // one past the newest

    // Columns come in time order (a column not after the newest is ignored); false when ignored.
    bool append(const Column &c);
    bool append(const ColumnMeta &meta, const Code *rows);
    const ColumnMeta &meta(uint64_t index) const;   // index in [firstIndex, endIndex)
    const Code *rows(uint64_t index) const;
    // The first column kept with a time ≥ t (endIndex when none).
    uint64_t lowerBound(double t) const;

    // Fewer or more columns: the newest are kept.
    void setCapacity(size_t capacity);
    void clear();
    // Memory held: the codes, the metadata and the object.
    size_t bytes() const;
    static size_t bytesFor(int bandRows, size_t capacity);

private:
    size_t slot(uint64_t index) const { return size_t(index % m_cap); }

    int m_band;
    size_t m_cap;
    size_t m_size = 0;
    uint64_t m_end = 0;
    std::vector<Code> m_codes;      // m_cap × m_band, slot-major
    std::vector<ColumnMeta> m_meta;
};

// ---------------------------------------------------------------- the pyramid

// How several cells become one when they fall in one pixel (zoomed out).
enum class Agg {
    Max,    // the biggest: a wall stays visible however far out
    Mean,   // the average: steady liquidity outweighs a moment's
};

// The store reduced in time, so a view of hours reads thousands of columns, not hundreds of thousands: level L
// (1 … levels) has one column per 4^L sample intervals (1 s, 4 s, 16 s … at 250 ms), its rows the max or the mean of
// the samples in that time (holes left out: `count` says how many there were), in the band of the newest one. Buckets
// are aligned on multiples of their length since the epoch, so they nest, and a texel of a power-of-two number of
// samples holds whole buckets. Every level keeps about the store's time span. The open bucket of each level (the
// newest, still filling) isn't in it yet: closedUntil() says from when to read the samples themselves.
class ColumnPyramid
{
public:
    static constexpr int FACTOR = 4;
    ColumnPyramid(int bandRows, int intervalMs, size_t baseCapacity, Agg agg, int levels = 6);

    void add(const ColumnMeta &m, const Code *rows);   // every sample appended to the store, in the same order
    Agg agg() const { return m_agg; }
    int levels() const { return int(m_levels.size()); }
    const ColumnStore &level(int L) const { return m_levels[size_t(L - 1)].store; }   // L in 1 … levels()
    int64_t samplesPer(int L) const;                   // 4^L
    double closedUntil(int L) const;                   // the open bucket's start (UTC s); -inf when nothing came
    void setBaseCapacity(size_t baseCapacity);
    size_t bytes() const;

private:
    struct Level {
        int64_t bucketMs = 0;
        ColumnStore store;
        bool open = false;
        int64_t bucket = 0;   // the open bucket's index
        ColumnMeta meta;      // its newest sample's band and best prices, the samples counted
        std::vector<Code> max;
        std::vector<double> sum;
        Level(int band, size_t cap, int64_t ms) : bucketMs(ms), store(band, cap) {}
    };
    void close(Level &l);
    void reanchor(Level &l, int64_t anchor);
    static size_t capacityFor(size_t base, int L);

    int m_band, m_intervalMs;
    Agg m_agg;
    std::vector<Level> m_levels;
};

// ---------------------------------------------------------------- grouping

// A column's rows summed by groups of `factor` rows, groups aligned on absolute row multiples of `factor` (row r goes
// in group floor(r / factor)), so a level stays in the same group whatever the column's anchor. out[i] is group
// first + i; returns first.
int64_t groupRows(const Code *rows, int bandRows, int64_t anchorRow, int factor, std::vector<double> &out);

// ---------------------------------------------------------------- normalisation

// The percentile of the quantities of a set of cells (the visible ones), zeros left out (empty rows aren't liquidity):
// add the codes, then ask. Nearest rank: the smallest value with at least p % of the cells at or under it.
class CodeHistogram
{
public:
    CodeHistogram() { clear(); }
    void clear();
    void add(Code c)
    {
        if (c) {
            ++m_bins[c];
            ++m_count;
        }
    }
    void add(const Code *codes, size_t n);
    uint64_t count() const { return m_count; }
    double percentile(double p) const;   // p in [0, 100]; 0 when no cell

private:
    std::array<uint32_t, 65536> m_bins;
    uint64_t m_count = 0;
};

// How a quantity becomes a colour.
struct Look {
    double percentile = 95;   // the reference size: this percentile of the visible cells
    double gain = 1;          // intensity: x = gain × q / reference, clamped to [0, 1]
    double gamma = 1;         // contrast: shown = x^gamma (< 1 lifts small levels, > 1 keeps only the big ones)
    double threshold = 0.02;  // a cell under threshold × reference isn't drawn (whatever the gain and contrast)
};

// 0 = not drawn; otherwise the intensity in (0, 1]. `reference` ≤ 0 (no cell) draws nothing.
double intensity(double qty, double reference, const Look &look);

// Palette index per code for one reference and look: 0 = not drawn, 1..255 = intensity 1/255..1 (rounded, at least
// 1 for a drawn cell). Built when the reference or the look changes (65536 entries, ~0.5 ms), then a cell is one
// lookup.
class IntensityLut
{
public:
    void build(double reference, const Look &look);
    uint8_t operator[](Code c) const { return m_lut[c]; }
    uint8_t ofQty(double q) const { return m_lut[encode(q)]; }
    double reference() const { return m_ref; }

private:
    std::array<uint8_t, 65536> m_lut{};
    double m_ref = 0;
};

// ---------------------------------------------------------------- stability (an approximation)

// The liquidity that rested at least `samples` sample intervals: per row, the smallest quantity over the column and the
// samples − 1 before it, rows matched by price (a band that moved still lines up; a row outside an older column's band
// counts as empty). Binance gives one quantity per price level, not per order, so this is an approximation of "orders
// that stayed": an order pulled and another placed at the same level in between look like one that stayed, and changes
// within a sample interval aren't seen. All zero when the window reaches before the store's oldest column or over a
// hole (a missing sample: what rested then is unknown). samples ≤ 1: the column itself.
void stableColumn(const ColumnStore &s, uint64_t index, int samples, int intervalMs, std::vector<Code> &out);

// ---------------------------------------------------------------- palettes

struct Rgb {
    uint8_t r = 0, g = 0, b = 0;
    bool operator==(const Rgb &) const = default;
};
// A colour at an intensity (0…1) and how opaque it is there: faint liquidity barely tints the background.
struct PaletteStop {
    double v;
    Rgb c;
    double alpha;
    bool operator==(const PaletteStop &) const = default;
};

enum class PaletteKind {
    Deep,      // dark blue → cyan → yellow → white (on a light background: light blue → blue → violet → crimson)
    Thermal,   // black → red → orange → yellow → white (on a light background: yellow → orange → red → black)
    Mono,      // the background → white (dark themes) or black (light ones)
    Custom,    // low → mid → high, the user's three colours
};
// The stops of a palette. `background` picks Mono's colour; low / mid / high are Custom's.
std::vector<PaletteStop> paletteStops(PaletteKind k, Rgb background, Rgb low = {}, Rgb mid = {}, Rgb high = {});
// 256 opaque colours (0xffRRGGBB): 0 is the background (nothing drawn), 1…255 the scale blended over it, interpolated
// between the stops in sRGB.
std::array<uint32_t, 256> buildPalette(const std::vector<PaletteStop> &stops, Rgb background);
double luminance(uint32_t rgb);   // relative (0…1), for the tests and Mono's choice

} // namespace qtm::heatmap
