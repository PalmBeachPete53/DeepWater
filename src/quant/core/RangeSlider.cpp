// The range slider's logic (see RangeSlider.h). A Deepwater addition.
#include "RangeSlider.h"

#include <algorithm>
#include <cmath>

namespace qtm::rangeslider {

Span span(int points, double widthPx, const Params &p)
{
    Span s;
    if (points < 1 || !(widthPx > 1)) return s;
    s.lo = -0.5;
    s.hi = points - 0.5 + std::max(0.0, p.rightPad);
    s.w = widthPx;
    s.points = points;
    s.ok = true;
    return s;
}

Range clip(const Range &visible, const Span &s)
{
    Range r{std::clamp(visible.from, s.lo, s.hi), std::clamp(visible.to, s.lo, s.hi)};
    if (r.to < r.from) r.to = r.from;
    return r;
}

Drag hit(double x, const Range &visible, const Span &s, const Params &p)
{
    if (!s.ok) return Drag::None;
    const Range c = clip(visible, s);
    const double x0 = s.x(c.from), x1 = s.x(c.to);
    const double inner = std::max(0.0, std::min(p.grabPx, (x1 - x0) / 3));
    // The two zones never overlap (each reaches in a third of the window at most), but at a zero-width window's own
    // x, which goes to the left edge.
    if (x >= x0 - p.grabPx && x <= x0 + inner) return Drag::Left;
    if (x >= x1 - inner && x <= x1 + p.grabPx) return Drag::Right;
    if (x > x0 && x < x1) return Drag::Pan;
    return Drag::None;
}

Range constrain(Range r, Drag mode, const Span &s, const Params &p)
{
    const double total = s.total();
    const double minW = std::min(p.minBars, total);
    const double lastFrom = s.lastFrom();
    if (mode == Drag::Pan) {   // the width as it is (minBars is for resizing: the chart may be zoomed in further)
        const double w = std::clamp(r.width(), 0.0, total);
        r.from = std::clamp(r.from, s.lo, std::max(s.lo, std::min(s.hi - w, lastFrom)));
        r.to = r.from + w;
        return r;
    }
    if (mode == Drag::Left) {
        r.to = std::clamp(r.to, s.lo + minW, s.hi);
        r.from = std::clamp(r.from, s.lo, std::max(s.lo, std::min(r.to - minW, lastFrom)));
        return r;
    }
    // Right (and None): the left edge first, then the right one.
    r.from = std::clamp(r.from, s.lo, std::max(s.lo, std::min(s.hi - minW, lastFrom)));
    r.to = std::clamp(r.to, r.from + minW, s.hi);
    return r;
}

std::optional<Range> Gesture::press(double x, const Range &visible, const Span &s)
{
    m_drag = Drag::None;
    if (!s.ok || x < 0 || x > s.w) return std::nullopt;
    const double L = s.logical(x);
    // The window as the strip shows it (the chart may show more than the span: scrolled past the data, or zoomed out
    // further than all of it).
    const Range shown = clip(visible, s);
    switch (hit(x, visible, s, m_p)) {
    case Drag::Left:
        m_drag = Drag::Left;
        m_fixed = shown.to;
        return std::nullopt;
    case Drag::Right:
        m_drag = Drag::Right;
        m_fixed = shown.from;
        return std::nullopt;
    case Drag::Pan:
        m_drag = Drag::Pan;
        m_width = visible.width();   // the chart's own width (it may reach past the span: the first move brings it in)
        m_grab = L - shown.from;     // held where it is drawn: its drawn left edge follows the mouse
        m_frozen = visible.width() >= s.total();   // all of it in view already: nothing to scroll, the zoom stays
        return std::nullopt;
    case Drag::None:
        break;
    }
    // Outside the window: centre it here, then keep dragging it.
    m_drag = Drag::Pan;
    m_width = visible.width();
    m_frozen = visible.width() >= s.total();
    if (m_frozen) return std::nullopt;
    const Range r = constrain({L - m_width / 2, L + m_width / 2}, Drag::Pan, s, m_p);
    m_grab = L - r.from;
    return r;
}

std::optional<Range> Gesture::move(double x, const Span &s)
{
    if (m_drag == Drag::None || !s.ok) return std::nullopt;
    const double L = s.logical(std::clamp(x, 0.0, s.w));
    switch (m_drag) {
    case Drag::Pan:
        if (m_frozen) return std::nullopt;
        return constrain({L - m_grab, L - m_grab + m_width}, Drag::Pan, s, m_p);
    case Drag::Left:
        return constrain({L, m_fixed}, Drag::Left, s, m_p);
    case Drag::Right:
        return constrain({m_fixed, L}, Drag::Right, s, m_p);
    case Drag::None:
        break;
    }
    return std::nullopt;
}

} // namespace qtm::rangeslider
