// Where a pane's bars are on the chart (see BarAxis.h). A Deepwater addition.
#include "BarAxis.h"

#include "../chart/Chart.h"

#include <algorithm>
#include <cmath>

namespace qtm {

bool BarAxis::build(lw::TimeScale *ts, const Bars &bars, const std::function<double(double)> &toChart, double tfSeconds)
{
    m_bars = &bars;
    m_ts = ts;
    m_toChart = toChart;
    n = int(bars.size());
    tf = tfSeconds > 0 ? tfSeconds : 60;
    from = 0;
    to = -1;
    if (!n || !ts) return false;
    const auto range = ts->getVisibleLogicalRange();
    if (!range) return false;
    // Each bar's index: the identity when the chart has no other times, else asked (binary searches, once a frame).
    m_identity = ts->pointCount() == n;
    if (!m_identity) {
        m_logical.resize(size_t(n));
        double prev = -1;
        for (int i = 0; i < n; ++i) {
            const auto L = ts->timeToLogical(toChart(bars[i].time));
            m_logical[size_t(i)] = L ? double(*L) : prev + 1;   // (every bar is a time point of the main series)
            prev = m_logical[size_t(i)];
        }
    }
    // The visible bars: indexes in the visible range, one more each side.
    auto firstAtOrAfter = [&](double L) {
        int lo = 0, hi = n;
        while (lo < hi) {
            const int mid = (lo + hi) / 2;
            if (logical(mid) < L)
                lo = mid + 1;
            else
                hi = mid;
        }
        return lo;
    };
    from = std::max(0, firstAtOrAfter(std::floor(range->from)) - 1);
    to = std::min(n - 1, firstAtOrAfter(std::ceil(range->to) + 1));
    if (to < from) return false;
    // x is linear in the index: two whole indexes give it (the chart only answers for whole ones).
    const int la = int(std::floor(range->from));
    const auto xa = ts->logicalToCoordinate(la), xb = ts->logicalToCoordinate(la + 1);
    if (!xa) return false;
    m_la = la;
    m_xa = *xa;
    bs = xb ? std::max(0.01, *xb - *xa) : 6;
    t0 = bars[from].time;
    t1 = bars[to].time + tf;
    return true;
}

int BarAxis::barAt(double t) const
{
    const Bars &b = *m_bars;
    const auto it = std::upper_bound(b.begin(), b.end(), t, [](double v, const Bar &bar) { return v < bar.time; });
    return int(it - b.begin()) - 1;
}

double BarAxis::xOfTime(double t) const
{
    if (!n) return 0;
    const int i = barAt(t);
    if (i < 0) return xOfBar(0) - bs / 2;
    const double L = logical(i);
    // the slot: until the next time point of the chart, at most tf
    double slot = tf;
    if (!m_identity || i + 1 < n) {
        const double start = m_toChart((*m_bars)[size_t(i)].time);
        double next = NAN;
        if (m_identity)
            next = m_toChart((*m_bars)[size_t(i + 1)].time);
        else if (const auto p = m_ts->pointTime(int(L) + 1))
            next = *p;
        if (std::isfinite(next) && next > start) slot = std::min(tf, next - start);
    }
    const double f = std::clamp((t - (*m_bars)[size_t(i)].time) / slot, 0.0, 1.0);
    return xOfLogical(L + f - 0.5);
}

} // namespace qtm
