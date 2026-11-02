// What the liquidity heatmap marks (see HeatmapEvents.h). A Deepwater addition, not ported from Quant Terminal.
#include "HeatmapEvents.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace qtm::heatmap {

void EventDetector::addTrade(double time, int64_t tick, double qty, footprint::Side aggressor)
{
    if (!(qty > 0)) return;
    m_trades.push_back({time, tick, qty, aggressor});
}

void EventDetector::push(const LiquidityEvent &e)
{
    m_events.push_back(e);
    while (m_events.size() > maxEvents) m_events.pop_front();
}

void EventDetector::dropBefore(double t)
{
    while (!m_events.empty() && m_events.front().t1 < t) m_events.pop_front();
}

double EventDetector::qtyAt(int64_t row) const
{
    const int64_t i = row - m_curAnchor;
    return i >= 0 && i < int64_t(m_curQty.size()) ? m_curQty[size_t(i)] : 0;
}

// One interval at the touch: the same tick as before adds to the episode, another starts a new one.
void EventDetector::episodeStep(Episode &e, int, int64_t touch, double qty, double traded, double tStart)
{
    if (!e.on || e.tick != touch) e = Episode{true, touch, tStart, 0, 0, 0};
    e.sumQty += qty;
    e.traded += traded;
    ++e.n;
}

// The touch moved: away from the level's side (it held) or through it (it gave way).
void EventDetector::episodeEnd(Episode &e, int side, int64_t newTouch, double t)
{
    if (!e.on || newTouch == e.tick) return;
    const bool held = newTouch != NO_TICK && (side > 0 ? newTouch > e.tick : newTouch < e.tick);
    const double mean = e.n ? e.sumQty / e.n : 0;
    if (held && t - e.t0 >= m_p.absorbMinSeconds - 1e-9 && mean > 0) {
        ++m_diag.heldTouches;
        m_diag.bestRatio = std::max(m_diag.bestRatio, e.traded / mean);
    }
    if (held && t - e.t0 >= m_p.absorbMinSeconds - 1e-9 && mean > 0 && e.traded >= m_p.absorbRatio * mean) {
        LiquidityEvent ev;
        ev.kind = LiquidityEvent::Kind::Absorbed;
        ev.side = side;
        ev.tick = e.tick;
        ev.row = floorDiv(e.tick, m_rowTicks);
        ev.t0 = e.t0;
        ev.t1 = t;
        ev.qty = mean;
        ev.traded = e.traded;
        push(ev);
    }
    e.on = false;
}

void EventDetector::addColumn(const ColumnMeta &m, const Code *rows, int band)
{
    if (m_hasPrev && !(m.time > m_prev.time)) return;
    const float *dec = decodeTable();
    m_curQty.resize(size_t(band));
    for (int i = 0; i < band; ++i) m_curQty[size_t(i)] = dec[rows[i]];
    m_curAnchor = m.anchorRow;

    // The trades of the interval since the previous column.
    std::unordered_map<int64_t, double> tradedRow;
    double sellsAtBid = 0, buysAtAsk = 0;
    while (!m_trades.empty() && m_trades.front().time <= m.time) {
        const Trade t = m_trades.front();
        m_trades.pop_front();
        if (!m_hasPrev) continue;   // before the first column: nothing to compare with
        tradedRow[floorDiv(t.tick, m_rowTicks)] += t.qty;
        if (t.side == footprint::Side::Sell && t.tick == m_prev.bestBid) sellsAtBid += t.qty;
        if (t.side == footprint::Side::Buy && t.tick == m_prev.bestAsk) buysAtAsk += t.qty;
    }
    if (!m_hasPrev) {
        m_prev = m;
        m_prevQty = m_curQty;
        m_hasPrev = true;
        return;
    }

    const bool pBb = m_prev.bestBid != NO_TICK, pBa = m_prev.bestAsk != NO_TICK;
    const int64_t pBbRow = pBb ? floorDiv(m_prev.bestBid, m_rowTicks) : 0, pBaRow = pBa ? floorDiv(m_prev.bestAsk, m_rowTicks) : 0;
    const bool bb = m.bestBid != NO_TICK, ba = m.bestAsk != NO_TICK;
    const int64_t bbRow = bb ? floorDiv(m.bestBid, m_rowTicks) : 0, baRow = ba ? floorDiv(m.bestAsk, m_rowTicks) : 0;

    // ---- pulled: big levels near the touch that went without trading
    std::vector<double> nz;
    nz.reserve(m_prevQty.size());
    for (double q : m_prevQty)
        if (q > 0) nz.push_back(q);
    if (!nz.empty()) {
        std::nth_element(nz.begin(), nz.begin() + ptrdiff_t(nz.size() / 2), nz.end());
        const double big = m_p.bigFactor * nz[nz.size() / 2];
        for (size_t i = 0; i < m_prevQty.size(); ++i) {
            const double q = m_prevQty[i];
            if (!(q > 0) || q < big) continue;
            const int64_t row = m_prev.anchorRow + int64_t(i);
            int side = 0;
            int64_t dist = 0;
            if (pBb && row < pBbRow && (!pBa || row < pBaRow)) {
                side = 1;
                dist = pBbRow - row;
            } else if (pBa && row > pBaRow && (!pBb || row > pBbRow)) {
                side = -1;
                dist = row - pBaRow;
            } else {
                continue;
            }
            if (dist < 1 || dist > m_p.nearRows) continue;
            const double now = qtyAt(row);
            if (now > (1 - m_p.dropFraction) * q) continue;
            const double drop = q - now;
            const auto it = tradedRow.find(row);
            const double traded = it == tradedRow.end() ? 0 : it->second;
            if (traded >= m_p.tradedShare * drop) continue;   // filled, not pulled
            const bool pending = std::any_of(m_candidates.begin(), m_candidates.end(),
                                             [&](const Candidate &c) { return c.row == row && c.side == side; });
            if (!pending) m_candidates.push_back({row, side, m.time, drop, q, traded, m.time + m_p.reachSeconds});
        }
    }
    for (size_t k = 0; k < m_candidates.size();) {
        const Candidate &c = m_candidates[k];
        const bool back = qtyAt(c.row) > (1 - m_p.dropFraction) * c.before;
        const bool reached = c.side > 0 ? (bb && bbRow <= c.row) : (ba && baRow >= c.row);
        if (!back && reached) {
            LiquidityEvent ev;
            ev.kind = LiquidityEvent::Kind::Pulled;
            ev.side = c.side;
            ev.row = c.row;
            ev.t0 = c.t0;
            ev.t1 = m.time;
            ev.qty = c.qty;
            ev.traded = c.traded;
            push(ev);
        }
        if (back || reached || m.time > c.deadline)
            m_candidates.erase(m_candidates.begin() + ptrdiff_t(k));
        else
            ++k;
    }

    // ---- absorbed: the touch held while aggressive volume hit it
    if (pBb) episodeStep(m_bidEp, 1, m_prev.bestBid, m_prev.bidQty, sellsAtBid, m_prev.time);
    if (pBa) episodeStep(m_askEp, -1, m_prev.bestAsk, m_prev.askQty, buysAtAsk, m_prev.time);
    episodeEnd(m_bidEp, 1, m.bestBid, m.time);
    episodeEnd(m_askEp, -1, m.bestAsk, m.time);

    m_prev = m;
    m_prevQty = m_curQty;
}

} // namespace qtm::heatmap
