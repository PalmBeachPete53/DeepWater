// Big Trades (see BigTrades.h). A Deepwater addition, not ported from Quant Terminal.
#include "BigTrades.h"

#include <algorithm>
#include <cmath>

namespace qtm::bigtrades {

namespace {
int64_t msOf(double seconds) { return std::llround(seconds * 1000); }
} // namespace

// ---------------------------------------------------------------- aggregation

bool Aggregator::joins(const Aggregate &a, const Trade &t, Side side) const
{
    if (side == Side::Unknown || side != a.side) return false;
    const int64_t dt = msOf(t.time) - msOf(a.time);
    if (m_p.exactTimestamp ? dt != 0 : (dt < 0 || double(dt) >= m_p.windowMs)) return false;
    switch (m_p.priceMode) {
    case PriceMode::SamePrice:
        return t.price == a.lastPrice;
    case PriceMode::Sweep:
        return side == Side::Buy ? t.price >= a.lastPrice : t.price <= a.lastPrice;
    }
    return false;
}

std::optional<Aggregate> Aggregator::add(const Trade &t, const Classified &c)
{
    if (m_open && joins(*m_open, t, c.side)) {
        Aggregate &a = *m_open;
        a.lastTime = t.time;
        a.lastSeq = t.seq;
        a.volume += t.qty;
        a.notional += t.price * t.qty;
        a.minPrice = std::min(a.minPrice, t.price);
        a.maxPrice = std::max(a.maxPrice, t.price);
        a.lastPrice = t.price;
        ++a.executions;
        a.approximate = a.approximate || c.approximate;
        return std::nullopt;
    }
    std::optional<Aggregate> closed = std::move(m_open);
    Aggregate a;
    a.time = a.lastTime = t.time;
    a.firstSeq = a.lastSeq = t.seq;
    a.side = c.side;
    a.volume = t.qty;
    a.notional = t.price * t.qty;
    a.minPrice = a.maxPrice = a.lastPrice = t.price;
    a.executions = 1;
    a.approximate = c.approximate;
    m_open = a;
    return closed;
}

std::optional<Aggregate> Aggregator::flush()
{
    std::optional<Aggregate> out = std::move(m_open);
    m_open.reset();
    return out;
}

std::optional<Aggregate> Aggregator::flushIfDone(double now)
{
    if (!m_open) return std::nullopt;
    const int64_t dt = msOf(now) - msOf(m_open->time);
    const bool done = m_open->side == Side::Unknown || (m_p.exactTimestamp ? dt > 0 : double(dt) >= m_p.windowMs);
    return done ? flush() : std::nullopt;
}

// ---------------------------------------------------------------- rolling percentile

namespace {
constexpr size_t BLOCK = 512;
}

void RollingPercentile::insertSorted(double v)
{
    if (m_blocks.empty()) {
        m_blocks.push_back({v});
        ++m_size;
        return;
    }
    // the first block whose largest value is ≥ v (or the last one)
    auto it = std::partition_point(m_blocks.begin(), m_blocks.end(), [v](const std::vector<double> &b) { return b.back() < v; });
    if (it == m_blocks.end()) --it;
    std::vector<double> &b = *it;
    b.insert(std::upper_bound(b.begin(), b.end(), v), v);
    if (b.size() > 2 * BLOCK) {
        std::vector<double> tail(b.begin() + BLOCK, b.end());
        b.resize(BLOCK);
        m_blocks.insert(it + 1, std::move(tail));
    }
    ++m_size;
}

void RollingPercentile::eraseSorted(double v)
{
    auto it = std::partition_point(m_blocks.begin(), m_blocks.end(), [v](const std::vector<double> &b) { return b.back() < v; });
    if (it == m_blocks.end()) return;
    std::vector<double> &b = *it;
    const auto pos = std::lower_bound(b.begin(), b.end(), v);
    if (pos == b.end() || *pos != v) return;   // not there (can't happen: every value out went in)
    b.erase(pos);
    if (b.empty()) m_blocks.erase(it);
    --m_size;
}

double RollingPercentile::at(size_t k) const
{
    for (const std::vector<double> &b : m_blocks) {
        if (k < b.size()) return b[k];
        k -= b.size();
    }
    return NaN;
}

void RollingPercentile::add(double time, double value)
{
    m_fifo.push_back({time, value});
    insertSorted(value);
}

void RollingPercentile::expire(double now)
{
    const double cut = now - m_window;   // the window is (now − window, now]
    while (!m_fifo.empty() && m_fifo.front().first <= cut) {
        eraseSorted(m_fifo.front().second);
        m_fifo.pop_front();
    }
}

double RollingPercentile::percentile(double p) const
{
    if (m_size == 0) return NaN;
    const double h = double(m_size - 1) * std::clamp(p, 0.0, 100.0) / 100;
    const size_t lo = size_t(std::floor(h));
    const double a = at(lo);
    if (lo + 1 >= m_size) return a;
    return a + (h - double(lo)) * (at(lo + 1) - a);
}

void RollingPercentile::clear()
{
    m_fifo.clear();
    m_blocks.clear();
    m_size = 0;
}

// ---------------------------------------------------------------- volatility

void MinuteAtr::add(double time, double price)
{
    const double minute = std::floor(time / 60) * 60;
    if (m_hasBar && minute > m_minute) {
        const double tr = m_hasPrevClose ? std::max({m_high - m_low, std::fabs(m_high - m_prevClose), std::fabs(m_low - m_prevClose)})
                                         : m_high - m_low;
        m_tr.push_back(tr);
        while (int(m_tr.size()) > std::max(m_short, m_long)) m_tr.pop_front();
        m_prevClose = m_close;
        m_hasPrevClose = true;
        m_hasBar = false;
    }
    if (!m_hasBar) {
        m_hasBar = true;
        m_minute = minute;
        m_open = m_high = m_low = m_close = price;
        return;
    }
    m_high = std::max(m_high, price);
    m_low = std::min(m_low, price);
    m_close = price;
}

double MinuteAtr::atr(int bars) const
{
    if (bars <= 0 || int(m_tr.size()) < bars) return NaN;
    double sum = 0;
    for (auto it = m_tr.end() - bars; it != m_tr.end(); ++it) sum += *it;
    return sum / bars;
}

double MinuteAtr::factor() const
{
    const double s = atr(m_short), l = atr(m_long);
    if (!(l > 0) || std::isnan(s)) return 1;
    return std::clamp(s / l, 0.5, 2.0);
}

// ---------------------------------------------------------------- detection

Detector::Detector(AggregationParams agg, ThresholdParams thr)
    : m_agg(agg), m_thr(thr), m_window(thr.windowMinutes * 60), m_atr(thr.atrShort, thr.atrLong)
{
}

Threshold Detector::threshold() const
{
    Threshold r;
    r.automatic = m_thr.automatic;
    r.samples = m_window.size();
    if (!m_thr.automatic) {
        r.value = m_thr.minVolume;
        return r;
    }
    r.volatilityFactor = m_thr.volatility ? m_atr.factor() : 1;
    r.volatilityReady = m_thr.volatility && m_atr.ready();
    if (r.samples < size_t(std::max(1, m_thr.minSamples))) {
        r.warmingUp = true;
        r.value = m_thr.minVolume > 0 ? m_thr.minVolume : NaN;
        return r;
    }
    r.percentileValue = m_window.percentile(m_thr.percentile);
    r.value = r.percentileValue * r.volatilityFactor;
    return r;
}

bool Detector::passes(double volume, const Threshold &thr) const
{
    if (std::isnan(thr.value)) return false;
    return volume >= thr.value && (m_thr.maxVolume <= 0 || volume <= m_thr.maxVolume);
}

std::optional<BigTrade> Detector::judge(const Aggregate &a)
{
    if (a.side == Side::Unknown) return std::nullopt;   // neither a buy nor a sell: not counted at all
    ++m_aggregates;
    m_maxVolume = std::max(m_maxVolume, a.volume);
    if (m_thr.automatic) m_window.expire(a.time);   // the window only feeds the automatic threshold
    const Threshold thr = threshold();
    const bool big = passes(a.volume, thr);
    if (m_thr.automatic) m_window.add(a.time, a.volume);   // after judging: an aggregate doesn't raise its own bar
    if (!big) return std::nullopt;
    return BigTrade{a, thr.value};
}

std::optional<BigTrade> Detector::add(const Trade &t, const Classified &c)
{
    std::optional<BigTrade> out;
    // A trade that can't join closes the open aggregate, judged before this trade moves the volatility.
    if (m_agg.open() && !m_agg.joins(*m_agg.open(), t, c.side)) out = judge(*m_agg.flush());
    m_atr.add(t.time, t.price);
    m_agg.add(t, c);
    return out;
}

std::optional<BigTrade> Detector::flush()
{
    if (auto a = m_agg.flush()) return judge(*a);
    return std::nullopt;
}

std::optional<BigTrade> Detector::flushIfDone(double now)
{
    if (auto a = m_agg.flushIfDone(now)) return judge(*a);
    return std::nullopt;
}

// ---------------------------------------------------------------- bubbles

double bubbleRadius(double volume, double refVolume, const BubbleStyle &s)
{
    if (!(refVolume > 0) || !(volume > 0)) return s.minRadius;
    return std::clamp(s.maxRadius * std::sqrt(volume / refVolume), s.minRadius, s.maxRadius);
}

double bubbleOpacity(double volume, double refVolume, const BubbleStyle &s)
{
    if (!(refVolume > 0) || !(volume > 0)) return s.minOpacity;
    return std::min(1.0, s.minOpacity + (1 - s.minOpacity) * std::sqrt(std::min(1.0, volume / refVolume)));
}

} // namespace qtm::bigtrades
