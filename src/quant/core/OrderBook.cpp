// The heatmap's order book (see OrderBook.h). A Deepwater addition, not ported from Quant Terminal.
#include "OrderBook.h"

#include <algorithm>

namespace qtm::depth {

namespace {
void setLevel(std::map<int64_t, double> &side, const Level &l)
{
    if (l.qty > 0)
        side[l.tick] = l.qty;
    else
        side.erase(l.tick);   // removing a level the book doesn't have is normal (deep levels, or already gone)
}
double qtyIn(const std::map<int64_t, double> &side, int64_t tick)
{
    const auto it = side.find(tick);
    return it == side.end() ? 0 : it->second;
}
} // namespace

std::optional<int64_t> OrderBook::bestBid() const
{
    if (m_bids.empty()) return std::nullopt;
    return m_bids.rbegin()->first;
}

std::optional<int64_t> OrderBook::bestAsk() const
{
    if (m_asks.empty()) return std::nullopt;
    return m_asks.begin()->first;
}

double OrderBook::bidQty(int64_t tick) const { return qtyIn(m_bids, tick); }
double OrderBook::askQty(int64_t tick) const { return qtyIn(m_asks, tick); }

bool OrderBook::crossed() const
{
    return !m_bids.empty() && !m_asks.empty() && m_bids.rbegin()->first >= m_asks.begin()->first;
}

void OrderBook::applyLevels(const DepthUpdate &u)
{
    for (const Level &l : u.bids) setLevel(m_bids, l);
    for (const Level &l : u.asks) setLevel(m_asks, l);
}

void OrderBook::lose()
{
    m_synced = false;
    m_buffer.clear();
}

void OrderBook::desync() { lose(); }

void OrderBook::reset()
{
    m_bids.clear();
    m_asks.clear();
    m_buffer.clear();
    m_lastId = -1;
    m_lastTime = 0;
    m_synced = false;
    m_sinceSnapshot = 0;
}

OrderBook::Result OrderBook::setSnapshot(const DepthSnapshot &s)
{
    m_bids.clear();
    m_asks.clear();
    for (const Level &l : s.bids)
        if (l.qty > 0) m_bids[l.tick] = l.qty;
    for (const Level &l : s.asks)
        if (l.qty > 0) m_asks[l.tick] = l.qty;
    m_lastId = s.lastUpdateId;
    m_lastTime = s.time;
    m_synced = true;
    m_sinceSnapshot = 0;
    ++m_stats.snapshots;

    std::vector<DepthUpdate> pending;
    pending.swap(m_buffer);
    // What the snapshot already has goes; the rest must start right after it.
    pending.erase(std::remove_if(pending.begin(), pending.end(),
                                 [&](const DepthUpdate &u) { return u.lastId <= s.lastUpdateId; }),
                  pending.end());
    if (crossed()) {
        ++m_stats.crossed;
        lose();
        return Result::Crossed;
    }
    // A snapshot older than the first update waiting is a hole like any other: applySynced finds it, and that update
    // and the ones after it stay buffered for the next snapshot.
    Result r = Result::Applied;
    for (size_t i = 0; i < pending.size(); ++i) {
        if (!m_synced) {   // a hole or a crossed book on the way: the rest waits for the next snapshot
            apply(pending[i]);
            continue;
        }
        const Result ri = applySynced(pending[i]);
        if (ri != Result::Stale) r = ri;
    }
    return r;
}

OrderBook::Result OrderBook::apply(const DepthUpdate &u)
{
    if (m_synced) return applySynced(u);
    if (m_buffer.size() >= MAX_BUFFERED) m_buffer.erase(m_buffer.begin());
    m_buffer.push_back(u);
    return Result::Buffered;
}

OrderBook::Result OrderBook::applySynced(const DepthUpdate &u)
{
    if (u.lastId <= m_lastId) {
        ++m_stats.stale;
        return Result::Stale;
    }
    // Futures chain each update to the previous one (pu); the first one after a snapshot, like spot's, must only
    // overlap it.
    const bool hole = (u.prevLastId >= 0 && m_sinceSnapshot > 0) ? u.prevLastId != m_lastId : u.firstId > m_lastId + 1;
    if (hole) {
        ++m_stats.gaps;
        lose();
        m_buffer.push_back(u);   // it follows the hole: the next snapshot may cover what's missing
        return Result::NeedSnapshot;
    }
    applyLevels(u);
    m_lastId = u.lastId;
    m_lastTime = u.time;
    ++m_sinceSnapshot;
    ++m_stats.applied;
    if (crossed()) {
        ++m_stats.crossed;
        lose();
        return Result::Crossed;
    }
    return Result::Applied;
}

size_t OrderBook::trim(int64_t centerTick, int64_t maxTicks)
{
    const size_t before = levelCount();
    const int64_t lo = centerTick - maxTicks, hi = centerTick + maxTicks;
    for (auto *side : {&m_bids, &m_asks}) {
        side->erase(side->begin(), side->lower_bound(lo));
        side->erase(side->upper_bound(hi), side->end());
    }
    return before - levelCount();
}

} // namespace qtm::depth
