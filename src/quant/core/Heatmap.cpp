// The liquidity heatmap's data (see Heatmap.h). A Deepwater addition, not ported from Quant Terminal.
#include "Heatmap.h"

#include <algorithm>
#include <climits>
#include <cmath>

namespace qtm::heatmap {

// ---------------------------------------------------------------- codes

Code encode(double qty)
{
    if (!(qty > 0)) return 0;
    const double v = std::round((std::log2(qty) - CODE_LOG_MIN) * CODE_STEPS) + 1;
    return Code(std::clamp(v, 1.0, 65535.0));
}

const float *decodeTable()
{
    static const std::vector<float> table = [] {
        std::vector<float> t(65536);
        t[0] = 0;
        for (int c = 1; c < 65536; ++c) t[size_t(c)] = float(std::exp2(double(c - 1) / CODE_STEPS + CODE_LOG_MIN));
        return t;
    }();
    return table.data();
}

float decode(Code c) { return decodeTable()[c]; }

int autoStorageRowTicks(double price, double tickSize, double relative)
{
    if (!(price > 0) || !(tickSize > 0)) return 1;
    const double target = relative * price / tickSize;
    if (target <= 1) return 1;
    const double e = std::floor(std::log10(target));
    double best = 1, bestErr = INFINITY;
    for (double k : {e, e + 1})
        for (double m : {1.0, 2.0, 5.0}) {
            const double c = m * std::pow(10.0, k);
            const double err = std::fabs(std::log(c / target));
            if (err < bestErr) {
                bestErr = err;
                best = c;
            }
        }
    return int(std::clamp(best, 1.0, double(INT_MAX)));
}

// ---------------------------------------------------------------- sampling

void Sampler::reset()
{
    m_started = false;
    m_anchored = false;
    m_lastK = 0;
}

bool Sampler::updateAnchor(const OrderBook &book)
{
    const auto bb = book.bestBid(), ba = book.bestAsk();
    if (!bb && !ba) return false;
    const int64_t mid = (bb && ba) ? floorDiv(*bb + *ba, 2) : (bb ? *bb : *ba);
    const int64_t midRow = floorDiv(mid, m_p.rowTicks);
    const int64_t band = m_p.bandRows;
    if (!m_anchored || midRow < m_anchor + band / 4 || midRow >= m_anchor + band - band / 4) {
        m_anchor = midRow - band / 2;
        m_anchored = true;
    }
    return true;
}

Column Sampler::sample(const OrderBook &book, double time, int64_t anchorRow, int rowTicks, int bandRows)
{
    Column c;
    c.meta.time = time;
    c.meta.anchorRow = anchorRow;
    if (const auto b = book.bestBid()) {
        c.meta.bestBid = *b;
        c.meta.bidQty = float(book.bidQty(*b));
    }
    if (const auto a = book.bestAsk()) {
        c.meta.bestAsk = *a;
        c.meta.askQty = float(book.askQty(*a));
    }
    std::vector<double> sums(size_t(bandRows), 0.0);
    const int64_t lo = anchorRow * rowTicks, hi = (anchorRow + bandRows) * rowTicks - 1;
    for (const auto *side : {&book.bids(), &book.asks()})
        for (auto it = side->lower_bound(lo), end = side->upper_bound(hi); it != end; ++it)
            sums[size_t(floorDiv(it->first, rowTicks) - anchorRow)] += it->second;
    c.rows.resize(size_t(bandRows));
    for (size_t i = 0; i < sums.size(); ++i) c.rows[i] = encode(sums[i]);
    return c;
}

std::vector<Column> Sampler::advance(OrderBook &book, double now)
{
    const int64_t k = floorDiv(std::llround(now * 1000), m_p.intervalMs);
    std::vector<Column> out;
    if (!m_started || !book.synced()) {   // the first call sets the clock; unsynced, the sample times are skipped
        m_started = true;
        m_lastK = k;
        return out;
    }
    if (k <= m_lastK) return out;
    const int64_t from = std::max(m_lastK + 1, k - int64_t(std::max(1, m_p.maxCatchUp)) + 1);
    m_lastK = k;
    if (!updateAnchor(book)) return out;
    // Nothing changed between these sample times (the update that comes next is after them): the same rows each time.
    const Column c = sample(book, double(from * m_p.intervalMs) / 1000, m_anchor, m_p.rowTicks, m_p.bandRows);
    out.reserve(size_t(k - from + 1));
    for (int64_t j = from; j <= k; ++j) {
        out.push_back(c);
        out.back().meta.time = double(j * m_p.intervalMs) / 1000;
    }
    if (m_p.trimBands > 0) {
        const int64_t center = (m_anchor + m_p.bandRows / 2) * m_p.rowTicks;
        book.trim(center, int64_t(m_p.trimBands) * m_p.bandRows * m_p.rowTicks);
    }
    return out;
}

// ---------------------------------------------------------------- store

ColumnStore::ColumnStore(int bandRows, size_t capacity) : m_band(std::max(1, bandRows)), m_cap(std::max<size_t>(1, capacity)) {}

size_t ColumnStore::bytesFor(int bandRows, size_t capacity)
{
    return capacity * (size_t(bandRows) * sizeof(Code) + sizeof(ColumnMeta));
}

size_t ColumnStore::bytes() const
{
    return m_codes.capacity() * sizeof(Code) + m_meta.capacity() * sizeof(ColumnMeta) + sizeof(*this);
}

bool ColumnStore::append(const Column &c)
{
    if (c.rows.size() != size_t(m_band)) return false;
    return append(c.meta, c.rows.data());
}

bool ColumnStore::append(const ColumnMeta &meta, const Code *rows)
{
    if (m_size && !(meta.time > this->meta(m_end - 1).time)) return false;
    const size_t s = slot(m_end);
    if (s >= m_meta.size()) {
        // Grows in steps (half as much again) until the ring is full, never past the capacity: the memory follows the
        // columns kept, and a column only copies the store when a step is taken.
        if (m_meta.capacity() < s + 1) {
            const size_t want = std::min(m_cap, std::max<size_t>(s + 1, m_meta.size() + m_meta.size() / 2 + 64));
            m_meta.reserve(want);
            m_codes.reserve(want * size_t(m_band));
        }
        m_meta.resize(s + 1);
        m_codes.resize((s + 1) * size_t(m_band));
    }
    m_meta[s] = meta;
    std::copy(rows, rows + m_band, m_codes.begin() + ptrdiff_t(s * size_t(m_band)));
    ++m_end;
    m_size = std::min(m_size + 1, m_cap);
    return true;
}

const ColumnMeta &ColumnStore::meta(uint64_t index) const { return m_meta[slot(index)]; }

const Code *ColumnStore::rows(uint64_t index) const { return m_codes.data() + slot(index) * size_t(m_band); }

uint64_t ColumnStore::lowerBound(double t) const
{
    uint64_t lo = firstIndex(), hi = m_end;
    while (lo < hi) {
        const uint64_t mid = lo + (hi - lo) / 2;
        if (meta(mid).time < t)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo;
}

void ColumnStore::setCapacity(size_t capacity)
{
    capacity = std::max<size_t>(1, capacity);
    if (capacity == m_cap) return;
    const size_t keep = std::min(m_size, capacity);
    const uint64_t first = m_end - keep;
    // Slots are index % capacity: the kept columns (consecutive indices, at most `capacity`) each get their own.
    size_t need = 0;
    for (uint64_t i = first; i < m_end; ++i) need = std::max(need, size_t(i % capacity) + 1);
    std::vector<ColumnMeta> newMeta(need);
    std::vector<Code> newCodes(need * size_t(m_band));
    for (uint64_t i = first; i < m_end; ++i) {
        const size_t ns = size_t(i % capacity);
        newMeta[ns] = meta(i);
        std::copy(rows(i), rows(i) + m_band, newCodes.begin() + ptrdiff_t(ns * size_t(m_band)));
    }
    m_meta = std::move(newMeta);
    m_codes = std::move(newCodes);
    m_cap = capacity;
    m_size = keep;
}

void ColumnStore::clear()
{
    m_meta = {};
    m_codes = {};
    m_size = 0;
}

// ---------------------------------------------------------------- pyramid

size_t ColumnPyramid::capacityFor(size_t base, int L)
{
    size_t div = 1;
    for (int i = 0; i < L; ++i) div *= FACTOR;
    return std::max<size_t>(2, (base + div - 1) / div + 1);
}

ColumnPyramid::ColumnPyramid(int bandRows, int intervalMs, size_t baseCapacity, Agg agg, int levels)
    : m_band(std::max(1, bandRows)), m_intervalMs(std::max(1, intervalMs)), m_agg(agg)
{
    int64_t ms = m_intervalMs;
    for (int L = 1; L <= levels; ++L) {
        ms *= FACTOR;
        m_levels.emplace_back(m_band, capacityFor(baseCapacity, L), ms);
    }
}

int64_t ColumnPyramid::samplesPer(int L) const
{
    int64_t n = 1;
    for (int i = 0; i < L; ++i) n *= FACTOR;
    return n;
}

double ColumnPyramid::closedUntil(int L) const
{
    const Level &l = m_levels[size_t(L - 1)];
    if (l.open) return double(l.bucket * l.bucketMs) / 1000;
    return l.store.empty() ? -INFINITY : l.store.meta(l.store.endIndex() - 1).time + double(l.bucketMs) / 1000;
}

void ColumnPyramid::setBaseCapacity(size_t baseCapacity)
{
    for (size_t i = 0; i < m_levels.size(); ++i) m_levels[i].store.setCapacity(capacityFor(baseCapacity, int(i) + 1));
}

size_t ColumnPyramid::bytes() const
{
    size_t b = sizeof(*this);
    for (const Level &l : m_levels) b += l.store.bytes() + l.max.capacity() * sizeof(Code) + l.sum.capacity() * sizeof(double);
    return b;
}

// The bucket's rows follow the newest sample's band: what was gathered moves with it (rows outside it are dropped).
void ColumnPyramid::reanchor(Level &l, int64_t anchor)
{
    const int64_t shift = anchor - l.meta.anchorRow;   // row i of the old band is row i − shift of the new one
    auto move = [&](auto &v, auto zero) {
        auto old = v;
        std::fill(v.begin(), v.end(), zero);
        for (int i = 0; i < m_band; ++i) {
            const int64_t j = i - shift;
            if (j >= 0 && j < m_band) v[size_t(j)] = old[size_t(i)];
        }
    };
    if (m_agg == Agg::Max)
        move(l.max, Code(0));
    else
        move(l.sum, 0.0);
    l.meta.anchorRow = anchor;
}

void ColumnPyramid::close(Level &l)
{
    ColumnMeta m = l.meta;
    m.time = double(l.bucket * l.bucketMs) / 1000;
    std::vector<Code> rows(static_cast<size_t>(m_band), Code(0));
    if (m_agg == Agg::Max)
        rows = l.max;
    else
        for (int i = 0; i < m_band; ++i) rows[size_t(i)] = encode(l.sum[size_t(i)] / double(m.count));
    l.store.append(m, rows.data());
    l.open = false;
}

void ColumnPyramid::add(const ColumnMeta &m, const Code *rows)
{
    const int64_t tMs = std::llround(m.time * 1000);
    const float *dec = decodeTable();
    for (Level &l : m_levels) {
        const int64_t b = floorDiv(tMs, l.bucketMs);
        if (l.open && b < l.bucket) continue;   // out of order: ignored, like the store does
        if (l.open && b != l.bucket) close(l);
        if (!l.open) {
            l.open = true;
            l.bucket = b;
            l.meta = m;
            l.meta.count = 0;
            if (m_agg == Agg::Max)
                l.max.assign(size_t(m_band), 0);
            else
                l.sum.assign(size_t(m_band), 0.0);
        } else if (m.anchorRow != l.meta.anchorRow) {
            reanchor(l, m.anchorRow);
        }
        l.meta.time = m.time;
        l.meta.bestBid = m.bestBid;
        l.meta.bestAsk = m.bestAsk;
        l.meta.count += m.count;
        if (m_agg == Agg::Max) {
            for (int i = 0; i < m_band; ++i)
                if (rows[i] > l.max[size_t(i)]) l.max[size_t(i)] = rows[i];
        } else {
            for (int i = 0; i < m_band; ++i)
                if (rows[i]) l.sum[size_t(i)] += double(dec[rows[i]]) * m.count;
        }
    }
}

// ---------------------------------------------------------------- grouping

int64_t groupRows(const Code *rows, int bandRows, int64_t anchorRow, int factor, std::vector<double> &out)
{
    factor = std::max(1, factor);
    const int64_t first = floorDiv(anchorRow, factor);
    const int64_t last = floorDiv(anchorRow + bandRows - 1, factor);
    out.assign(size_t(last - first + 1), 0.0);
    const float *dec = decodeTable();
    for (int i = 0; i < bandRows; ++i)
        if (rows[i]) out[size_t(floorDiv(anchorRow + i, factor) - first)] += dec[rows[i]];
    return first;
}

// ---------------------------------------------------------------- normalisation

void CodeHistogram::clear()
{
    m_bins.fill(0);
    m_count = 0;
}

void CodeHistogram::add(const Code *codes, size_t n)
{
    for (size_t i = 0; i < n; ++i) add(codes[i]);
}

double CodeHistogram::percentile(double p) const
{
    if (!m_count) return 0;
    const double rank = std::max(1.0, std::ceil(std::clamp(p, 0.0, 100.0) * double(m_count) / 100));
    uint64_t cum = 0;
    for (size_t c = 1; c < m_bins.size(); ++c) {
        cum += m_bins[c];
        if (double(cum) >= rank) return decode(Code(c));
    }
    return decode(65535);
}

double intensity(double qty, double reference, const Look &look)
{
    if (!(reference > 0) || !(qty > 0)) return 0;
    const double r = qty / reference;
    if (r < look.threshold) return 0;
    const double x = std::min(1.0, look.gain * r);
    if (!(x > 0)) return 0;
    return look.gamma == 1 ? x : std::pow(x, look.gamma);
}

void IntensityLut::build(double reference, const Look &look)
{
    m_ref = reference;
    m_lut[0] = 0;
    const float *dec = decodeTable();
    for (size_t c = 1; c < m_lut.size(); ++c) {
        const double v = intensity(dec[c], reference, look);
        m_lut[c] = v > 0 ? uint8_t(std::clamp<long>(std::lround(v * 255), 1, 255)) : 0;
    }
}

// ---------------------------------------------------------------- stability

void stableColumn(const ColumnStore &s, uint64_t index, int samples, int intervalMs, std::vector<Code> &out)
{
    const int band = s.bandRows();
    const Code *rows = s.rows(index);
    out.assign(rows, rows + band);
    if (samples <= 1) return;
    const uint64_t back = uint64_t(samples - 1);
    const double t = s.meta(index).time;
    // The window must be there and whole: samples consecutive sample times, no hole.
    if (index < s.firstIndex() + back
        || std::llround((t - s.meta(index - back).time) * 1000) != int64_t(back) * intervalMs) {
        std::fill(out.begin(), out.end(), Code(0));
        return;
    }
    const int64_t anchor = s.meta(index).anchorRow;
    for (uint64_t j = 1; j <= back; ++j) {
        const ColumnMeta &m = s.meta(index - j);
        const Code *r = s.rows(index - j);
        const int64_t shift = anchor - m.anchorRow;   // row i here is row i + shift there
        bool any = false;
        for (int i = 0; i < band; ++i) {
            if (!out[size_t(i)]) continue;
            const int64_t k = i + shift;
            const Code v = k >= 0 && k < band ? r[k] : Code(0);
            if (v < out[size_t(i)]) out[size_t(i)] = v;
            any = any || out[size_t(i)];
        }
        if (!any) return;
    }
}

// ---------------------------------------------------------------- palettes

double luminance(uint32_t rgb)
{
    auto lin = [](double c) {
        c /= 255;
        return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * lin((rgb >> 16) & 0xff) + 0.7152 * lin((rgb >> 8) & 0xff) + 0.0722 * lin(rgb & 0xff);
}

std::vector<PaletteStop> paletteStops(PaletteKind k, Rgb bg, Rgb low, Rgb mid, Rgb high)
{
    // On a light background the presets darken instead (white walls would vanish into it).
    const bool dark = luminance(0xff000000u | uint32_t(bg.r) << 16 | uint32_t(bg.g) << 8 | bg.b) < 0.4;
    if (!dark && k == PaletteKind::Deep)
        return {{0.0, {160, 190, 235}, 0.35}, {0.30, {80, 120, 210}, 0.62}, {0.60, {95, 60, 170}, 0.82},
                {0.85, {160, 25, 80}, 0.94}, {1.0, {60, 0, 20}, 1.0}};
    if (!dark && k == PaletteKind::Thermal)
        return {{0.0, {250, 220, 120}, 0.35}, {0.35, {245, 150, 40}, 0.72}, {0.60, {220, 70, 20}, 0.86},
                {0.85, {150, 20, 10}, 0.95}, {1.0, {40, 0, 0}, 1.0}};
    switch (k) {
    case PaletteKind::Deep:
        return {{0.0, {20, 40, 120}, 0.30}, {0.30, {30, 100, 220}, 0.62}, {0.60, {40, 210, 220}, 0.82},
                {0.85, {250, 225, 60}, 0.94}, {1.0, {255, 255, 235}, 1.0}};
    case PaletteKind::Thermal:
        return {{0.0, {70, 0, 0}, 0.35}, {0.35, {185, 25, 15}, 0.72}, {0.60, {245, 105, 20}, 0.86},
                {0.85, {255, 210, 60}, 0.95}, {1.0, {255, 255, 240}, 1.0}};
    case PaletteKind::Mono: {
        const Rgb ink = dark ? Rgb{255, 255, 255} : Rgb{0, 0, 0};
        return {{0.0, ink, 0.10}, {1.0, ink, 1.0}};
    }
    case PaletteKind::Custom:
        return {{0.0, low, 0.35}, {0.5, mid, 0.75}, {1.0, high, 1.0}};
    }
    return {};
}

std::array<uint32_t, 256> buildPalette(const std::vector<PaletteStop> &stops, Rgb bg)
{
    std::array<uint32_t, 256> pal{};
    auto pack = [](double r, double g, double b) {
        auto c = [](double x) { return uint32_t(std::clamp(std::lround(x), 0L, 255L)); };
        return 0xff000000u | c(r) << 16 | c(g) << 8 | c(b);
    };
    pal[0] = pack(bg.r, bg.g, bg.b);
    if (stops.empty()) {
        for (size_t i = 1; i < 256; ++i) pal[i] = pal[0];
        return pal;
    }
    for (int i = 1; i < 256; ++i) {
        const double v = i / 255.0;
        size_t s = 0;
        while (s + 1 < stops.size() && v > stops[s + 1].v) ++s;
        const PaletteStop &a = stops[s], &b = stops[std::min(s + 1, stops.size() - 1)];
        const double f = b.v > a.v ? std::clamp((v - a.v) / (b.v - a.v), 0.0, 1.0) : 0;
        auto mix = [f](double x, double y) { return x + (y - x) * f; };
        const double al = std::clamp(mix(a.alpha, b.alpha), 0.0, 1.0);
        auto over = [al](double fg, double back) { return fg * al + back * (1 - al); };
        pal[size_t(i)] = pack(over(mix(a.c.r, b.c.r), bg.r), over(mix(a.c.g, b.c.g), bg.g), over(mix(a.c.b, b.c.b), bg.b));
    }
    return pal;
}

} // namespace qtm::heatmap
