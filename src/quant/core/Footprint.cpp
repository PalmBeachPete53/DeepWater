// Footprint aggregation (see Footprint.h). A Deepwater addition, not ported from Quant Terminal.
#include "Footprint.h"

#include <algorithm>
#include <climits>
#include <cmath>

namespace qtm::footprint {

int autoRowTicks(const Bars &bars, double tickSize, int targetRows)
{
    if (!(tickSize > 0) || targetRows < 1) return 1;
    std::vector<double> ranges;
    for (qsizetype i = std::max<qsizetype>(0, bars.size() - 100); i < bars.size(); ++i) {
        const double r = bars[i].high - bars[i].low;
        if (r > 0) ranges.push_back(r);
    }
    if (ranges.empty()) return 1;
    std::sort(ranges.begin(), ranges.end());
    const size_t n = ranges.size();
    const double median = n % 2 ? ranges[n / 2] : (ranges[n / 2 - 1] + ranges[n / 2]) / 2;
    const double raw = median / targetRows / tickSize;
    if (!(raw > 1)) return 1;
    // The nearest 1-2-5 step on a log scale: 208 ticks → 200, 400 → 500.
    const double k = std::pow(10.0, std::floor(std::log10(raw)));
    double best = k, bestDist = INFINITY;
    for (double c : {1.0, 2.0, 5.0, 10.0}) {
        const double d = std::fabs(std::log(c * k) - std::log(raw));
        if (d < bestDist) {
            bestDist = d;
            best = c * k;
        }
    }
    return int(std::min(best, double(INT_MAX)));
}

// ---------------------------------------------------------------- classification

Side TradeClassifier::tickRule(int64_t tick) const
{
    if (!m_hasLast) return Side::Unknown;   // nothing to compare the first trade with
    if (tick > m_lastTick) return Side::Buy;
    if (tick < m_lastTick) return Side::Sell;
    return m_lastSide;
}

Classified TradeClassifier::classify(const Trade &t)
{
    const int64_t tick = m_grid.toTick(t.price);
    Classified out;
    if (m_mode == ClassifyMode::Aggressor && t.side != Side::Unknown) {
        out.side = t.side;
    } else if (m_mode != ClassifyMode::TickRule && isNum(t.bid) && isNum(t.ask) && t.ask >= t.bid) {
        // Lee-Ready, in ticks so the mid is exact: 2·price against bid + ask.
        const int64_t bid = m_grid.toTick(t.bid), ask = m_grid.toTick(t.ask);
        if (tick >= ask) out.side = Side::Buy;
        else if (tick <= bid) out.side = Side::Sell;
        else if (2 * tick > bid + ask) out.side = Side::Buy;
        else if (2 * tick < bid + ask) out.side = Side::Sell;
        else out.side = tickRule(tick);   // right at the mid
        out.approximate = true;
    } else {
        out.side = tickRule(tick);
        out.approximate = true;
    }
    m_hasLast = true;
    m_lastTick = tick;
    if (out.side != Side::Unknown) m_lastSide = out.side;
    return out;
}

void TradeClassifier::reset()
{
    m_hasLast = false;
    m_lastTick = 0;
    m_lastSide = Side::Unknown;
}

// ---------------------------------------------------------------- one candle

void CandleFootprint::add(const StoredTrade &t, int64_t row)
{
    const Stamp s{t.time, t.seq, t.tick};
    auto before = [](const Stamp &a, const Stamp &b) { return a.time < b.time || (a.time == b.time && a.seq < b.seq); };
    if (m_trades == 0) {
        m_base = row - 8;
        m_levels.assign(17, Level{});
        m_lowRow = m_highRow = row;
        m_lowTick = m_highTick = t.tick;
        m_open = m_close = s;
    } else {
        const int64_t size = int64_t(m_levels.size());
        if (row < m_base) {
            const int64_t grow = (m_base - row) + std::max<int64_t>(16, size / 2);
            m_levels.insert(m_levels.begin(), size_t(grow), Level{});
            m_base -= grow;
        } else if (row >= m_base + size) {
            const int64_t grow = (row - (m_base + size) + 1) + std::max<int64_t>(16, size / 2);
            m_levels.resize(size_t(size + grow));
        }
        m_lowRow = std::min(m_lowRow, row);
        m_highRow = std::max(m_highRow, row);
        m_lowTick = std::min(m_lowTick, t.tick);
        m_highTick = std::max(m_highTick, t.tick);
        if (before(s, m_open)) m_open = s;
        if (before(m_close, s)) m_close = s;
    }
    Level &l = m_levels[size_t(row - m_base)];
    switch (t.side) {
    case Side::Buy:
        l.ask += t.qty;
        m_ask += t.qty;
        break;
    case Side::Sell:
        l.bid += t.qty;
        m_bid += t.qty;
        break;
    case Side::Unknown:
        m_unknown += t.qty;
        break;
    }
    ++m_trades;
    if (t.approximate) ++m_approx;
    ++m_version;
}

Level CandleFootprint::level(int64_t row) const
{
    if (row < m_base || row >= m_base + int64_t(m_levels.size())) return {};
    return m_levels[size_t(row - m_base)];
}

// ---------------------------------------------------------------- analysis

Analysis analyze(const CandleFootprint &c, const Params &params)
{
    Analysis a;
    if (c.empty()) return a;
    const int64_t lo = c.lowRow(), hi = c.highRow();
    a.lowRow = lo;
    a.highRow = hi;
    a.pocRow = lo;
    a.delta = c.delta();
    a.volume = c.volume();
    a.imbalance.assign(size_t(hi - lo + 1), NoImbalance);
    double minVolume = params.minVolume;
    if (params.autoMinVolume) {
        double sum = 0;
        int rows = 0;
        for (int64_t r = lo; r <= hi; ++r)
            if (const double v = c.level(r).volume(); v > 0) {
                sum += v;
                ++rows;
            }
        minVolume = rows ? sum / rows : 0;
    }
    auto strongEnough = [&](double v) { return v > 0 && v >= minVolume; };
    int64_t pocDist = INT64_MAX;
    for (int64_t r = lo; r <= hi; ++r) {
        const Level l = c.level(r);
        const double vol = l.volume();
        // Ties go to the row nearest the middle (|2r − (lo + hi)|), then to the lower one (rows go up).
        const int64_t dist = std::llabs(2 * r - (lo + hi));
        if (vol > a.pocVolume || (vol == a.pocVolume && vol > 0 && dist < pocDist)) {
            a.pocVolume = vol;
            a.pocRow = r;
            pocDist = dist;
        }
        a.maxLevelVolume = std::max(a.maxLevelVolume, vol);
        a.maxAbsLevelDelta = std::max(a.maxAbsLevelDelta, std::fabs(l.delta()));
        uint8_t &flag = a.imbalance[size_t(r - lo)];
        if (r > lo && strongEnough(l.ask) && l.ask >= params.ratio * c.level(r - 1).bid) flag |= BuyImbalance;
        if (r < hi && strongEnough(l.bid) && l.bid >= params.ratio * c.level(r + 1).ask) flag |= SellImbalance;
    }
    // Stacked imbalances: runs of at least stackCount consecutive rows with the same side's imbalance.
    const int need = std::max(1, params.stackCount);
    for (const auto &[bit, side] : {std::pair{BuyImbalance, Side::Buy}, std::pair{SellImbalance, Side::Sell}}) {
        int64_t runFrom = 0, run = 0;
        for (int64_t r = lo; r <= hi + 1; ++r) {
            const bool on = r <= hi && (a.imbalance[size_t(r - lo)] & bit);
            if (on) {
                if (run++ == 0) runFrom = r;
            } else {
                if (run >= need) a.stacks.push_back({runFrom, r - 1, side});
                run = 0;
            }
        }
    }
    std::stable_sort(a.stacks.begin(), a.stacks.end(), [](const Zone &x, const Zone &y) { return x.fromRow < y.fromRow; });
    return a;
}

// ---------------------------------------------------------------- the book

const CandleFootprint &Book::add(double candleTime, const Trade &t, const Classified &c)
{
    const StoredTrade st{t.time, t.seq, m_grid.toTick(t.price), t.qty, c.side, c.approximate};
    m_trades.push_back({candleTime, st});
    if (st.approximate) ++m_approx;
    CandleFootprint &cf = m_candles[candleTime];
    cf.add(st, m_grid.rowOfTick(st.tick));
    return cf;
}

const CandleFootprint *Book::candle(double candleTime) const
{
    const auto it = m_candles.find(candleTime);
    return it == m_candles.end() ? nullptr : &it->second;
}

void Book::setRowTicks(int rowTicks)
{
    rowTicks = std::max(1, rowTicks);
    if (rowTicks == m_grid.rowTicks) return;
    m_grid.rowTicks = rowTicks;
    m_candles.clear();
    for (const Entry &e : m_trades) m_candles[e.candleTime].add(e.t, m_grid.rowOfTick(e.t.tick));
}

void Book::dropBefore(double time)
{
    m_candles.erase(m_candles.begin(), m_candles.lower_bound(time));
    std::erase_if(m_trades, [time](const Entry &e) { return e.candleTime < time; });
    m_approx = 0;
    for (const Entry &e : m_trades)
        if (e.t.approximate) ++m_approx;
}

double Book::trimToTrades(size_t maxTrades)
{
    if (m_trades.size() > maxTrades) {
        size_t left = m_trades.size();
        double cut = NaN;
        for (const auto &[time, c] : m_candles) {
            if (left <= maxTrades) {
                cut = time;
                break;
            }
            left -= size_t(c.trades());
        }
        if (std::isnan(cut)) clear();
        else dropBefore(cut);
    }
    return m_candles.empty() ? NaN : m_candles.begin()->first;
}

void Book::clear()
{
    m_candles.clear();
    m_trades.clear();
    m_approx = 0;
}

QString formatVolume(double v)
{
    const double a = std::fabs(v);
    auto trim = [](QString s) {
        if (s.contains('.')) {
            while (s.endsWith('0')) s.chop(1);
            if (s.endsWith('.')) s.chop(1);
        }
        return s;
    };
    if (a == 0) return "0";
    if (a >= 1e6) return trim(QString::number(v / 1e6, 'f', 1)) + "M";
    if (a >= 1e4) return trim(QString::number(v / 1e3, 'f', 1)) + "K";
    if (a >= 100) return QString::number(std::llround(v));
    if (a >= 10) return trim(QString::number(v, 'f', 1));
    if (a >= 1) return trim(QString::number(v, 'f', 2));
    // 3 decimals, more for smaller volumes (two significant digits), at most 8 (Binance's step)
    const int decimals = std::clamp(1 - int(std::floor(std::log10(a))), 3, 8);
    return trim(QString::number(v, 'f', decimals));
}

int candleIndex(const Values &openTimes, double time)
{
    const auto it = std::upper_bound(openTimes.begin(), openTimes.end(), time);
    return int(it - openTimes.begin()) - 1;
}

} // namespace qtm::footprint
