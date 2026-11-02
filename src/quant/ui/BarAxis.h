#pragma once

// Where a pane's bars are on the chart, for the layers that draw at bars and at times inside them (footprint, Big
// Trades, the liquidity heatmap). The chart's logical indexes are every series' times, so with a compared symbol (US
// minutes going further back than a crypto pair's, or sitting between its bars) bar i is not index i: each bar's index
// is asked of the chart. Built once a frame; without a compared symbol (as many time points as bars) it is the identity
// and costs nothing.

#include "../core/Bar.h"

#include <functional>
#include <vector>

namespace lw {
class TimeScale;
}

namespace qtm {

class BarAxis
{
public:
    // False when nothing can be placed (no bars, no visible range, no width).
    bool build(lw::TimeScale *ts, const Bars &bars, const std::function<double(double)> &toChart, double tfSeconds);

    int n = 0;
    int from = 0, to = -1;   // the visible bars, one more on each side
    double bs = 1;           // px per logical index (a bar's slot)
    double tf = 60;
    double t0 = 0, t1 = 0;   // the visible bars' times: bars[from].time … bars[to].time + tf

    double logical(int i) const { return m_identity ? double(i) : m_logical[size_t(i)]; }
    double xOfBar(int i) const { return xOfLogical(logical(i)); }   // the bar's centre
    double xOfLogical(double L) const { return m_xa + (L - m_la) * bs; }
    // A time: its bar (the last one at or before it), then its place in the bar's slot, which ends at the next time
    // point of the chart (a compared symbol's) or after tf, whichever comes first. Before the first bar: its left edge.
    double xOfTime(double t) const;
    int barAt(double t) const;   // the last bar at or before t (−1: before the first)
    // Bars i and i + 1 are side by side on the chart (consecutive indexes): a run of them is linear in time.
    bool adjacent(int i) const { return logical(i + 1) == logical(i) + 1; }

private:
    const Bars *m_bars = nullptr;
    lw::TimeScale *m_ts = nullptr;
    std::function<double(double)> m_toChart;
    bool m_identity = true;
    std::vector<double> m_logical;
    double m_xa = 0, m_la = 0;
};

} // namespace qtm
