#pragma once

// The range slider's logic (ui/ChartNavigator draws it and passes the mouse on): which part of the strip the mouse is
// on, and what a gesture does to the chart's visible range. Pure code (no widgets), in the chart's logical units as
// EDGES: bar i of the chart's time points spans i − ½ … i + ½, and a range is where the view starts and ends. (The
// chart's own visible range names the bars at its edges, their centres: edges = centres ∓ ½; ui/ChartNavigator converts.)
//
// The strip spans the chart's time points plus `rightPad` empty bars after the last one (as a fresh chart shows).
// A gesture never takes the window out of that span nor under `minBars` bars, nor past the last two real bars (the chart
// keeps two in view), and a pan never changes its width (with
// all the bars already in view, there is nothing to pan and the chart is left as it is). What it holds is fixed when it starts:
// a pan keeps the window's width, a resize keeps the other edge, so moves that come faster than the chart applies the
// ranges (it does at its next frame) don't drift.

#include <algorithm>
#include <optional>

namespace qtm::rangeslider {

struct Params {
    double minBars = 10;
    double rightPad = 9;   // ChartPane's fresh view: the last 160 bars and the edge 9 bars after the last one
    double grabPx = 8;   // an edge grabs this far outwards, and inwards at most a third of the window
};

struct Range {
    double from = 0, to = 0;
    double width() const { return to - from; }
};

struct Span {
    double lo = 0, hi = 0;   // logical
    double w = 0;            // px
    int points = 0;
    bool ok = false;
    // The chart keeps at least two real bars in view (Lightweight Charts' rule): a window can't start after this.
    double lastFrom() const { return points - 0.5 - std::min(2, points); }
    double x(double L) const { return (L - lo) / (hi - lo) * w; }
    double logical(double x) const { return lo + x / w * (hi - lo); }
    double total() const { return hi - lo; }
};

// The strip for `points` time points over `widthPx` (not ok without points or width).
Span span(int points, double widthPx, const Params &p = {});

enum class Drag { None, Pan, Left, Right };

// What a press at x would grab: an edge, the window (inside), or nothing (outside it: a click there recentres).
Drag hit(double x, const Range &visible, const Span &s, const Params &p = {});

// The window as the strip shows it: the range with both edges kept inside the span (no minimum width: a chart zoomed in
// past minBars is drawn as it is).
Range clip(const Range &visible, const Span &s);

// `want` kept in the span. Pan moves it as a block, its width as it is (cut to the span); Left / Right move only that
// edge (the other stays) and keep it at least minBars wide (or the whole span when shorter); None clips both edges.
Range constrain(Range want, Drag mode, const Span &s, const Params &p = {});

class Gesture
{
public:
    explicit Gesture(const Params &p = {}) : m_p(p) {}
    // A press at x: the range to set now (a click outside the window centres it there), or none. It then drags.
    std::optional<Range> press(double x, const Range &visible, const Span &s);
    std::optional<Range> move(double x, const Span &s);   // the range to set, or none
    void release() { m_drag = Drag::None; }
    Drag drag() const { return m_drag; }

private:
    Params m_p;
    Drag m_drag = Drag::None;
    double m_grab = 0;     // pan: where the mouse holds the window, logical units from its left edge
    double m_width = 0;    // pan: the window's width
    double m_fixed = 0;    // resize: the edge that stays
    bool m_frozen = false; // pan with every bar already in view: nothing to do
};

} // namespace qtm::rangeslider
