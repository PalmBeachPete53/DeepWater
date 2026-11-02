// qtm_test_navigator: the range slider under the charts. First core/RangeSlider on its own (hand-worked cases, then
// 100,000 random gestures against its invariants), then ui/ChartNavigator on a real lw::Chart, offscreen, driven with
// real mouse events. Same style as the other test programs: plain checks, a count at the end, non-zero exit on failure.
#include "chart/Chart.h"
#include "core/RangeSlider.h"
#include "ui/BarAxis.h"
#include "ui/ChartNavigator.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QMouseEvent>
#include <QWheelEvent>

#include <cmath>
#include <iostream>
#include <memory>
#include <random>

using namespace qtm;
using namespace qtm::rangeslider;

namespace {

int checks = 0, fails = 0;
QString CAT = "misc";

void check(bool ok, const QString &msg)
{
    ++checks;
    if (!ok) {
        ++fails;
        std::cout << "FAIL [" << CAT.toStdString() << "] " << msg.toStdString() << "\n";
    }
}
void near(double want, double got, const QString &ctx, double tol = 1e-9)
{
    check(std::fabs(want - got) <= tol * std::max(1.0, std::fabs(want)),
          QStringLiteral("%1: want %2, got %3").arg(ctx).arg(want, 0, 'g', 12).arg(got, 0, 'g', 12));
}
QString name(Drag d)
{
    switch (d) {
    case Drag::None: return "None";
    case Drag::Pan: return "Pan";
    case Drag::Left: return "Left";
    case Drag::Right: return "Right";
    }
    return "?";
}
void eqDrag(Drag want, Drag got, const QString &ctx) { check(want == got, ctx + ": want " + name(want) + ", got " + name(got)); }

// ================================================================ core/RangeSlider

// 1000 points over 1000 px: the span is −0.5 … 1007.5 (1008 bars with the right margin of 8), 1.008 bars a px.
const Params P{10, 8, 8};
Span S1000() { return span(1000, 1000, P); }

void testSpan()
{
    CAT = "span";
    const Span s = S1000();
    check(s.ok, "1000 points over 1000 px");
    near(-0.5, s.lo, "lo: bar 0 starts at −½");
    near(1007.5, s.hi, "hi: the last bar ends at 999.5, then 8 empty bars");
    near(0, s.x(-0.5), "x(lo) = 0");
    near(1000, s.x(1007.5), "x(hi) = width");
    near(503.5, s.logical(500), "500 px ↔ logical −0.5 + 500 × 1.008 = 503.5");
    check(!span(0, 1000, P).ok, "no points: not ok");
    check(!span(10, 1, P).ok, "1 px: not ok");
    check(!span(10, NAN, P).ok, "no width: not ok");
}

void testHit()
{
    CAT = "hit";
    const Span s = S1000();
    // Window 500 … 600: x0 = 500.5 / 1.008 = 496.53, x1 = 600.5 / 1.008 = 595.73; edges grab 8 px out, 8 in.
    const Range r{500, 600};
    const double x0 = s.x(500), x1 = s.x(600);
    near(496.527777777778, x0, "x0", 1e-9);
    eqDrag(Drag::Left, hit(x0, r, s, P), "on the left edge");
    eqDrag(Drag::Left, hit(x0 - 7.9, r, s, P), "7.9 px left of it");
    eqDrag(Drag::None, hit(x0 - 8.1, r, s, P), "8.1 px left of it: outside");
    eqDrag(Drag::Left, hit(x0 + 7.9, r, s, P), "7.9 px inside");
    eqDrag(Drag::Pan, hit(x0 + 8.1, r, s, P), "8.1 px inside: the window");
    eqDrag(Drag::Pan, hit((x0 + x1) / 2, r, s, P), "the middle");
    eqDrag(Drag::Right, hit(x1 - 7.9, r, s, P), "7.9 px inside the right edge");
    eqDrag(Drag::Right, hit(x1 + 7.9, r, s, P), "7.9 px right of it");
    eqDrag(Drag::None, hit(x1 + 8.1, r, s, P), "8.1 px right of it: outside");
    eqDrag(Drag::None, hit(100, r, s, P), "far left: outside");
    // A narrow window (500 … 510: 9.92 px): edges grab a third inwards (3.31 px), the middle third pans.
    const Range n{500, 510};
    const double n0 = s.x(500), n1 = s.x(510);
    eqDrag(Drag::Pan, hit((n0 + n1) / 2, n, s, P), "narrow window: its middle pans");
    eqDrag(Drag::Left, hit(n0 + 3.2, n, s, P), "narrow window: 3.2 px in grabs the left edge");
    eqDrag(Drag::Right, hit(n1 - 3.2, n, s, P), "narrow window: 3.2 px in from the right grabs the right edge");
    // A window under a px: both edges in reach, the nearer wins.
    const Range t{500, 500.5};
    eqDrag(Drag::Left, hit(s.x(500) - 2, t, s, P), "tiny window: left of it, the left edge");
    eqDrag(Drag::Right, hit(s.x(500.5) + 2, t, s, P), "tiny window: right of it, the right edge");
    // A range past the span (the chart scrolled into the void on the right) is drawn as a sliver at the end: grabbing
    // it takes its edge (dragging it left brings the view back).
    const Range past{1100, 1200};
    eqDrag(Drag::Left, hit(995, past, s, P), "range past the span: a sliver at the end, its edge grabbed");
    // What is drawn is what is hit: clip(), no minimum width (a 3-bar view is drawn 3 bars wide).
    const Range c3 = clip({500, 503}, s);
    near(3, c3.width(), "clip keeps a 3-bar view 3 bars wide");
    const Range cp = clip(past, s);
    near(1007.5, cp.from, "clip past the span: at hi");
    near(0, cp.width(), "clip past the span: no width");
    const Range inv = clip({600, 500}, s);
    check(inv.width() >= 0, "clip of an inverted range: no negative width");
    eqDrag(Drag::None, hit(500, Range{}, Span{}, P), "no span: nothing");
}

void testConstrain()
{
    CAT = "constrain";
    const Span s = S1000();
    Range r = constrain({-50, 50}, Drag::Pan, s, P);
    near(-0.5, r.from, "pan past the left: stops at lo");
    near(99.5, r.to, "pan past the left: same width (100)");
    r = constrain({990, 1100}, Drag::Pan, s, P);
    near(1007.5, r.to, "pan past the right: stops at hi");
    near(897.5, r.from, "pan past the right: same width (110)");
    r = constrain({-100, 5000}, Drag::Pan, s, P);
    near(-0.5, r.from, "pan wider than the span: all of it");
    near(1007.5, r.to, "pan wider than the span: all of it");
    r = constrain({500, 503}, Drag::Pan, s, P);
    near(3, r.width(), "pan keeps a width under minBars (the chart zoomed in further)");
    r = constrain({595, 600}, Drag::Left, s, P);
    near(590, r.from, "left edge closer than 10 bars: pushed back to 10");
    near(600, r.to, "left edge: the right one stays");
    r = constrain({-100, 600}, Drag::Left, s, P);
    near(-0.5, r.from, "left edge past lo: lo");
    r = constrain({500, 2000}, Drag::Right, s, P);
    near(1007.5, r.to, "right edge past hi: hi");
    near(500, r.from, "right edge: the left one stays");
    r = constrain({500, 501}, Drag::Right, s, P);
    near(510, r.to, "right edge closer than 10: 10");
    r = constrain({1004, 1010}, Drag::Right, s, P);
    near(997.5, r.from, "a right resize at the end: the left edge makes room for 10 bars (997.5 ≤ last two bars' 997.5)");
    near(1007.5, r.to, "a right resize at the end: hi");
    // A 5-bar window can't end at hi (it would hold only empty bars): it stops with the last two bars in view.
    r = constrain({1002.5, 1007.5}, Drag::Pan, s, P);
    near(997.5, r.from, "a narrow pan to the end: starts on the second-last bar (997.5)");
    near(5, r.width(), "and keeps its width");
    // A span shorter than minBars (3 points + 8): the whole span.
    const Span tiny = span(1, 100, P);
    r = constrain({0, 0.5}, Drag::Right, tiny, P);
    near(tiny.lo, r.from, "span shorter than minBars: its start");
    near(tiny.hi, r.to, "span shorter than minBars: its end");
}

void testGesture()
{
    CAT = "gesture";
    const Span s = S1000();
    const double perPx = s.total() / s.w;   // 1.008 bars a px
    // Pan: press in the middle of 500 … 600, move 100 px right: 100.8 bars later, same width.
    {
        Gesture g(P);
        const double xm = s.x(550);
        check(!g.press(xm, {500, 600}, s), "press inside: nothing yet");
        eqDrag(Drag::Pan, g.drag(), "press inside: pan");
        const auto r = g.move(xm + 100, s);
        check(bool(r), "move: a range");
        if (r) {
            near(500 + 100 * perPx, r->from, "pan +100 px: from");
            near(100, r->width(), "pan: same width");
        }
        // The chart hasn't applied it yet (its next frame) and the mouse moves on: from the press, not the stale range.
        const auto r2 = g.move(xm + 150, s);
        if (r2) near(500 + 150 * perPx, r2->from, "pan +150 px before the chart caught up: no drift");
        g.release();
        eqDrag(Drag::None, g.drag(), "release");
        check(!g.move(xm + 300, s), "after release: nothing");
    }
    // Click outside, at logical 200: the window (width 100) centred there, then dragged on.
    {
        Gesture g(P);
        const auto r = g.press(s.x(200), {500, 600}, s);
        check(bool(r), "press outside: a range at once");
        if (r) {
            near(150, r->from, "centred: from 150");
            near(250, r->to, "centred: to 250");
        }
        eqDrag(Drag::Pan, g.drag(), "then it pans");
        const auto r2 = g.move(s.x(200) + 10, s);
        if (r2) near(150 + 10 * perPx, r2->from, "dragging on from the click");
        // At the very start: centred on 3, it stops at lo.
        Gesture h(P);
        const auto r3 = h.press(s.x(3), {500, 600}, s);
        if (r3) {
            near(-0.5, r3->from, "click near the start: lo");
            near(99.5, r3->to, "click near the start: same width");
        }
        // and the drag holds it where the mouse took it: moving back left doesn't move it, right moves it once past
        const auto r4 = h.move(s.x(3) - 5, s);
        if (r4) near(-0.5, r4->from, "held at the start");
    }
    // Resize the left edge: the right stays, at least 10 bars, never before lo.
    {
        Gesture g(P);
        check(!g.press(s.x(500), {500, 600}, s), "press on the left edge");
        eqDrag(Drag::Left, g.drag(), "left edge");
        auto r = g.move(s.x(300), s);
        if (r) {
            near(300, r->from, "left edge to 300: zoomed out");
            near(600, r->to, "the right edge stays");
        }
        r = g.move(s.x(700), s);
        if (r) near(590, r->from, "left edge dragged past the right: 10 bars");
        r = g.move(-500, s);
        if (r) near(-0.5, r->from, "dragged out of the strip: lo");
    }
    // Resize the right edge.
    {
        Gesture g(P);
        g.press(s.x(600), {500, 600}, s);
        eqDrag(Drag::Right, g.drag(), "right edge");
        auto r = g.move(s.x(900), s);
        if (r) near(900, r->to, "right edge to 900");
        r = g.move(s.w + 500, s);
        if (r) near(1007.5, r->to, "dragged out of the strip: hi");
        r = g.move(0, s);
        if (r) {
            near(500, r->from, "right edge to the start: the left stays");
            near(510, r->to, "and 10 bars");
        }
    }
    // Every bar already in view (the chart zoomed out past the data): a pan changes nothing.
    {
        Gesture g(P);
        check(!g.press(s.x(500), {-20, 1100}, s), "all in view: a press inside does nothing");
        check(!g.move(s.x(600), s), "all in view: a pan does nothing (the zoom stays)");
        Gesture h(P);
        check(!h.press(s.x(500), {-20, 1100}, s) && !h.move(s.x(400), s), "all in view: a click outside does nothing either");
    }
    // The chart scrolled partly past the data (950 … 1050, the span ends at 1007.5): a pan keeps its width (100) and
    // comes back in.
    {
        Gesture g(P);
        g.press(s.x(975), {950, 1050}, s);
        eqDrag(Drag::Pan, g.drag(), "partly past the data: the window grabbed");
        const auto r = g.move(s.x(975) - 50, s);
        if (r) {
            near(100, r->width(), "past the data: the width kept");
            check(r->to <= s.hi + 1e-9, "past the data: back inside");
        }
    }
    // The chart scrolled before the data (−50 … 50, drawn −0.5 … 50): grabbed where it is drawn, its left edge follows
    // the mouse from the first move (10 px = 10.08 bars), at its full width (100).
    {
        Gesture g(P);
        g.press(s.x(20), {-50, 50}, s);
        const auto r = g.move(s.x(20) + 10, s);
        if (r) {
            near(-0.5 + 10 * perPx, r->from, "scrolled before the data: the drawn left edge follows the mouse");
            near(100, r->width(), "at the chart's full width");
        }
    }
    // A pan of a window under 10 bars keeps it.
    {
        Gesture g(P);
        g.press(s.x(502.5), {500, 505}, s);
        const auto r = g.move(s.x(502.5) + 50, s);
        if (r) near(5, r->width(), "5 bars wide: still 5 after a pan");
    }
    // Presses that do nothing.
    {
        Gesture g(P);
        check(!g.press(-1, {500, 600}, s) && g.drag() == Drag::None, "press left of the strip: nothing");
        check(!g.press(s.w + 1, {500, 600}, s) && g.drag() == Drag::None, "press under the price axis: nothing");
        check(!g.press(10, {500, 600}, Span{}) && g.drag() == Drag::None, "no span: nothing");
    }
}

// Random spans, ranges and gestures; after every move: inside the span, a pan keeps its width (or is frozen), a
// resize keeps its other edge and 10 bars, nothing is NaN.
void testFuzz()
{
    CAT = "fuzz";
    std::mt19937_64 rng(2026);
    auto U = [&](double a, double b) { return std::uniform_real_distribution<double>(a, b)(rng); };
    int moves = 0, violations = 0;
    QString first;
    auto bad = [&](const QString &m) {
        if (!violations++) first = m;
    };
    for (int g = 0; g < 100000; ++g) {
        const int points = int(std::exp(U(0, std::log(20000.0))));
        const double w = U(50, 3000);
        const Span s = span(points, w, P);
        if (!s.ok) continue;
        // ranges inside, partly outside, tiny, huge
        double from = U(s.lo - s.total() * 0.3, s.hi);
        double width = std::exp(U(std::log(0.5), std::log(s.total() * 1.5)));
        const Range vis{from, from + width};
        Gesture gs(P);
        const double px = U(-20, w + 20);
        const auto r0 = gs.press(px, vis, s);
        const Drag d = gs.drag();
        const Range shown = clip(vis, s);
        auto checkRange = [&](const Range &r, const char *when) {
            if (!std::isfinite(r.from) || !std::isfinite(r.to)) bad(QString("%1: NaN").arg(when));
            if (r.from < s.lo - 1e-6 || r.to > s.hi + 1e-6)
                bad(QString("%1: %2 … %3 outside %4 … %5").arg(when).arg(r.from).arg(r.to).arg(s.lo).arg(s.hi));
            if (d == Drag::Pan && std::fabs(r.width() - std::min(vis.width(), s.total())) > 1e-6)
                bad(QString("%1: pan width %2, was %3").arg(when).arg(r.width()).arg(vis.width()));
            const double minW = std::min(P.minBars, s.total());
            if (r.from > s.lastFrom() + 1e-6)
                bad(QString("%1: starts at %2, after the last two bars (%3)").arg(when).arg(r.from).arg(s.lastFrom()));
            if ((d == Drag::Left || d == Drag::Right) && r.width() < minW - 1e-6)
                bad(QString("%1: resize under %2 bars (%3)").arg(when).arg(minW).arg(r.width()));
            if (d == Drag::Left && std::fabs(r.to - std::clamp(shown.to, s.lo + minW, s.hi)) > 1e-6)
                bad(QString("%1: left resize moved the right edge (%2, was %3)").arg(when).arg(r.to).arg(shown.to));
            if (d == Drag::Right && std::fabs(r.from - std::clamp(shown.from, s.lo, std::max(s.lo, std::min(s.hi - minW, s.lastFrom())))) > 1e-6)
                bad(QString("%1: right resize moved the left edge (%2, was %3)").arg(when).arg(r.from).arg(shown.from));
        };
        if (r0) checkRange(*r0, "press");
        if (px < 0 || px > w) {
            if (d != Drag::None || r0) bad("a press outside the strip did something");
            continue;
        }
        for (int m = 0; m < 20; ++m) {
            const auto r = gs.move(U(-w, 2 * w), s);
            ++moves;
            if (r) checkRange(*r, "move");
        }
    }
    check(violations == 0, QString("%1 violations in %2 moves; first: %3").arg(violations).arg(moves).arg(first));
    std::cout << "  fuzz: 100,000 gestures, " << moves << " moves checked\n";
}

// ================================================================ ui/ChartNavigator on a real chart

struct Rig {
    std::unique_ptr<QWidget> host;
    lw::Chart *chart = nullptr;
    lw::Series *main = nullptr;
    ChartNavigator *nav = nullptr;
    Bars bars;
    int rangeSignals = 0;

    explicit Rig(int n, int width = 1200)
    {
        host = std::make_unique<QWidget>();
        host->resize(width, 536);
        lw::ChartOptions o;
        o.rightOffset = 8;
        chart = new lw::Chart(o, host.get());
        chart->setGeometry(0, 0, width, 500);
        lw::SeriesOptions so;
        main = chart->addSeries(lw::SeriesType::Candlestick, so);
        setBars(n);
        nav = new ChartNavigator(chart, [this]() -> const Bars & { return bars; }, [](double t) { return t; }, host.get());
        nav->setGeometry(0, 500, width, ChartNavigator::HEIGHT);
        QObject::connect(chart, &lw::Chart::visibleLogicalRangeChanged, [this] { ++rangeSignals; });
        host->show();
        chart->timeScale()->setVisibleLogicalRange({double(n - 160), double(n + 8)});   // ChartPane's fresh view
        frame();
    }
    void setBars(int n, double t0 = 1.7e9)
    {
        bars.clear();
        QVector<lw::DataItem> d;
        double p = 100;
        for (int i = 0; i < n; ++i) {
            const double t = t0 + 60.0 * i;
            const double c = p + std::sin(i * 0.05) * 2 + (i % 7) * 0.1;
            bars.push_back({t, p, std::max(p, c) + 0.5, std::min(p, c) - 0.5, c, 1, {}});
            lw::DataItem it;
            it.time = t;
            it.open = p;
            it.high = std::max(p, c) + 0.5;
            it.low = std::min(p, c) - 0.5;
            it.close = c;
            d.push_back(it);
            p = c;
        }
        main->setData(d);
    }
    void addBar()
    {
        const Bar &l = bars.last();
        Bar b{l.time + 60, l.close, l.close + 1, l.close - 1, l.close + 0.3, 1, {}};
        bars.push_back(b);
        lw::DataItem it;
        it.time = b.time;
        it.open = b.open;
        it.high = b.high;
        it.low = b.low;
        it.close = b.close;
        main->update(it);
    }
    // Let the chart apply what was asked (its next frame) and paint, the navigator too.
    void frame()
    {
        QCoreApplication::processEvents();
        chart->repaint();
        nav->repaint();
        QCoreApplication::processEvents();
    }
    // The chart's visible range as edges (what the strip works with).
    Range range() const { return *nav->range(); }
    // The chart's own range (edge bars' centres).
    lw::LogicalRange chartRange() const { return *chart->timeScale()->getVisibleLogicalRange(); }
    void mouse(QEvent::Type t, double x, Qt::MouseButton b = Qt::LeftButton)
    {
        const QPointF pos(x, 18);
        QMouseEvent e(t, pos, nav->mapToGlobal(pos), t == QEvent::MouseMove ? Qt::NoButton : b,
                      t == QEvent::MouseButtonRelease ? Qt::NoButton : (t == QEvent::MouseMove ? Qt::MouseButtons(b) : Qt::MouseButtons(b)),
                      Qt::NoModifier);
        QCoreApplication::sendEvent(nav, &e);
    }
    void hover(double x)
    {
        const QPointF pos(x, 18);
        QMouseEvent e(QEvent::MouseMove, pos, nav->mapToGlobal(pos), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(nav, &e);
    }
    void drag(double from, double to, int steps, bool framesBetween = true)
    {
        mouse(QEvent::MouseButtonPress, from);
        if (framesBetween) frame();
        for (int i = 1; i <= steps; ++i) {
            mouse(QEvent::MouseMove, from + (to - from) * i / steps);
            if (framesBetween) frame();
        }
        mouse(QEvent::MouseButtonRelease, to);
        frame();
    }
};

void testRig()
{
    CAT = "ui/window";
    Rig r(3000);
    const Span s = r.nav->span();
    check(s.ok, "span ok");
    near(3000 - 0.5 + 9, s.hi, "span: 3000 points, then up to 9 bars after the last (a fresh chart's view)");
    near(r.chart->timeScale()->width(), s.w, "the track is the chart's plot width (lines up with the time axis)");
    // The fresh view (bars n − 160 … n + 8 centred at the edges) is exactly the window at the right end.
    const lw::LogicalRange cr = r.chartRange();
    const Range e = r.range();
    near(cr.from - 0.5, e.from, "edges = centres − ½");
    near(cr.to + 0.5, e.to, "edges = centres + ½");
    near(s.hi, e.to, "the fresh view ends exactly at the strip's end (no jump on the first drag)");
    const auto w = r.nav->windowX();
    check(bool(w), "a window");
    if (w) {
        near(s.x(e.from), w->first, "window left = the chart's left edge", 1e-9);
        near(s.x(e.to), w->second, "window right = the chart's right edge", 1e-9);
    }
    check(r.rangeSignals >= 1, QString("the chart said its range changed (%1)").arg(r.rangeSignals));
}

// Drags with a frame between moves can see the price axis (and so the plot width) change mid-way: they are checked on
// what must hold whatever the width (zoom kept, other edge kept, about where the mouse went); bursts of moves without
// a frame are checked exactly.
void testRigDrag()
{
    CAT = "ui/drag";
    Rig r(3000);
    auto pp = [&] { const Span sp = r.nav->span(); return sp.total() / sp.w; };
    auto [x0, x1] = *r.nav->windowX();
    Range before = r.range();
    // Pan 300 px left, a frame between moves.
    double k = pp();
    r.drag((x0 + x1) / 2, (x0 + x1) / 2 - 300, 12);
    Range after = r.range();
    near(before.width(), after.width(), "pan: the chart's zoom kept", 1e-9);
    check(std::fabs(after.from - (before.from - 300 * k)) < 1.5, QString("pan: about 300 px of bars (%1 vs %2)").arg(after.from).arg(before.from - 300 * k));
    // A burst of 40 moves without a frame (moves faster than the chart draws): exactly where the mouse is.
    std::tie(x0, x1) = *r.nav->windowX();
    before = r.range();
    k = pp();
    r.drag((x0 + x1) / 2, (x0 + x1) / 2 + 200, 40, false);
    after = r.range();
    near(before.from + 200 * k, after.from, "a burst of 40 moves without a frame: lands where the mouse is", 1e-9);
    near(before.width(), after.width(), "and keeps the zoom", 1e-9);
    // Left edge out 150 px (burst): zoom out, the right edge stays.
    std::tie(x0, x1) = *r.nav->windowX();
    before = r.range();
    k = pp();
    r.drag(x0, x0 - 150, 10, false);
    after = r.range();
    near(before.to, after.to, "left edge: the right one stays", 1e-9);
    near(before.from - 150 * k, after.from, "left edge: 150 px further", 1e-9);
    // Right edge across the left one: 10 bars, the left edge stays.
    std::tie(x0, x1) = *r.nav->windowX();
    before = r.range();
    r.drag(x1, x0 - 400, 10);
    after = r.range();
    near(before.from, after.from, "right edge past the left: the left stays", 1e-9);
    near(10, after.width(), "and 10 bars wide", 1e-9);
    near(10, r.chartRange().to - r.chartRange().from + 1, "the chart shows 10 bars", 1e-9);
    // The narrow window (10 bars ≈ 4 px) still pans by its middle.
    std::tie(x0, x1) = *r.nav->windowX();
    before = r.range();
    k = pp();
    r.drag((x0 + x1) / 2, (x0 + x1) / 2 + 100, 5, false);
    after = r.range();
    near(10, after.width(), "a 4 px window pans (still 10 bars)", 1e-9);
    near(before.from + 100 * k, after.from, "and moves", 1e-9);
    // The drag goes on outside the strip (Qt keeps the mouse for it): far left, the window stops at the start.
    std::tie(x0, x1) = *r.nav->windowX();
    r.drag((x0 + x1) / 2, -2000, 6);
    near(-0.5, r.range().from, "dragged far out of the strip: the first bar's left edge", 1e-9);
    near(0, r.chartRange().from, "the chart starts on bar 0", 1e-9);
    // And far right: a 10-bar window can only go as far as keeping the last two bars in view (the chart's rule).
    std::tie(x0, x1) = *r.nav->windowX();
    r.drag((x0 + x1) / 2, r.nav->span().w + 5000, 6);
    near(3000 - 2.5, r.range().from, "dragged far right, 10 bars wide: starts on the second-last bar", 1e-9);
    near(10, r.range().width(), "still 10 bars wide: the chart took the range as asked (no correction)", 1e-9);
    // Click outside, then keep dragging (burst): centred where clicked, then follows.
    const Span s = r.nav->span();
    r.mouse(QEvent::MouseButtonPress, 100);
    const double L = s.logical(100);
    r.mouse(QEvent::MouseMove, 160);
    r.mouse(QEvent::MouseButtonRelease, 160);
    r.frame();
    near(L + 60 * s.total() / s.w, r.range().from + r.range().width() / 2, "click outside then drag on: centred on the mouse", 1e-9);
    // Each click alone: the chart centred there.
    for (double x : {300.0, 500.0, 700.0}) {
        const Span sp = r.nav->span();
        r.mouse(QEvent::MouseButtonPress, x);
        r.mouse(QEvent::MouseButtonRelease, x);
        r.frame();
        near(sp.logical(x), r.range().from + r.range().width() / 2, QString("click at %1 px: centred there").arg(x), 1e-9);
    }
    // A right press, or a press under the price axis: nothing.
    const Range d0 = r.range();
    r.mouse(QEvent::MouseButtonPress, 300, Qt::RightButton);
    r.mouse(QEvent::MouseButtonRelease, 300, Qt::RightButton);
    r.mouse(QEvent::MouseButtonPress, r.nav->span().w + 20);
    r.mouse(QEvent::MouseMove, r.nav->span().w + 20 - 300);
    r.mouse(QEvent::MouseButtonRelease, r.nav->span().w + 20 - 300);
    r.frame();
    near(d0.from, r.range().from, "right button / price axis: nothing", 1e-9);
}

void testRigSync()
{
    CAT = "ui/sync";
    Rig r(3000);
    // The chart zoomed with the wheel: the window follows.
    const auto w0 = *r.nav->windowX();
    const int sig0 = r.rangeSignals;
    QWheelEvent wheel(QPointF(600, 250), r.chart->mapToGlobal(QPointF(600, 250)), QPoint(), QPoint(0, 360), Qt::NoButton,
                      Qt::NoModifier, Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(r.chart, &wheel);
    r.frame();
    const auto w1 = *r.nav->windowX();
    check(r.rangeSignals > sig0, "the wheel: the chart said so");
    check(w1.second - w1.first < w0.second - w0.first, "the wheel zoomed in: the window narrower");
    near(r.nav->span().x(r.range().from), w1.first, "window = the chart's new range", 1e-9);
    // Dragging in the chart itself (scroll): the window follows.
    {
        QMouseEvent p(QEvent::MouseButtonPress, QPointF(600, 250), r.chart->mapToGlobal(QPointF(600, 250)), Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(r.chart, &p);
        for (int i = 1; i <= 10; ++i) {
            QMouseEvent m(QEvent::MouseMove, QPointF(600 + 30 * i, 250), r.chart->mapToGlobal(QPointF(600 + 30 * i, 250)),
                          Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
            QCoreApplication::sendEvent(r.chart, &m);
            r.frame();
        }
        QMouseEvent rel(QEvent::MouseButtonRelease, QPointF(900, 250), r.chart->mapToGlobal(QPointF(900, 250)), Qt::LeftButton,
                        Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(r.chart, &rel);
        r.frame();
    }
    const auto w2 = *r.nav->windowX();
    check(w2.first < w1.first, "the chart dragged right (back in time): the window moved left");
    near(r.nav->span().x(r.range().from), w2.first, "window = the chart's range after its own drag", 1e-9);
}

void testRigLive()
{
    CAT = "ui/live";
    Rig r(3000);
    // At the right end: new bars, the chart follows, the window with it.
    Range before = r.range();
    for (int i = 0; i < 5; ++i) {
        r.addBar();
        r.frame();
    }
    Range after = r.range();
    near(before.from + 5, after.from, "window at the end: follows 5 new bars", 1e-9);
    near(3005 - 0.5 + 9, r.nav->span().hi, "the strip grew with them");
    near(r.nav->span().hi, after.to, "and the window still ends at the end");
    // Away from the end: stays.
    auto [x0, x1] = *r.nav->windowX();
    r.drag((x0 + x1) / 2, (x0 + x1) / 2 - 400, 8);
    before = r.range();
    for (int i = 0; i < 5; ++i) {
        r.addBar();
        r.frame();
    }
    after = r.range();
    near(before.from, after.from, "window away from the end: stays put", 1e-9);
    // A pan going on while a bar comes in: the window stays under the mouse (the strip gets one bar longer).
    std::tie(x0, x1) = *r.nav->windowX();
    before = r.range();
    r.mouse(QEvent::MouseButtonPress, (x0 + x1) / 2);
    r.addBar();
    r.frame();
    const Span s = r.nav->span();
    r.mouse(QEvent::MouseMove, (x0 + x1) / 2 + 50);
    r.mouse(QEvent::MouseButtonRelease, (x0 + x1) / 2 + 50);
    r.frame();
    near(before.width(), r.range().width(), "panning while a bar comes in: the zoom kept", 1e-9);
    check(std::fabs(r.range().from - (before.from + 50 * s.total() / s.w)) < 1.5, "and about 50 px further");
}

void testRigData()
{
    CAT = "ui/data";
    Rig r(3000);
    // A new symbol with fewer bars (what ChartPane does: new data, then the last 160 bars).
    r.setBars(500, 1.6e9);
    r.chart->timeScale()->setVisibleLogicalRange({500 - 160, 500 + 8});
    r.frame();
    near(500 - 0.5 + 9, r.nav->span().hi, "new data: the strip is the new bars");
    near(r.nav->span().hi, r.range().to, "the fresh view at the strip's end");
    const auto w = r.nav->windowX();
    check(w && w->first >= 0 && w->second <= r.nav->span().w + 1e-9, "the window inside the strip");
    near(500, r.nav->curvePoints(), "the line has the new 500 bars");
    // A compared series with times between the main bars (every 30 s): twice the time points.
    lw::SeriesOptions so;
    lw::Series *cmp = r.chart->addSeries(lw::SeriesType::Line, so);
    QVector<lw::DataItem> d;
    for (int i = 0; i < 500; ++i) {
        lw::DataItem it;
        it.time = 1.6e9 + 60.0 * i + 30;
        it.value = 50 + i * 0.01;
        d.push_back(it);
    }
    cmp->setData(d);
    r.frame();
    near(1000, r.chart->timeScale()->pointCount(), "1000 time points with the compared series");
    near(1000 - 0.5 + 9, r.nav->span().hi, "the strip spans every time point, not just the main bars");
    near(500, r.nav->curvePoints(), "each main bar placed at its own index");
    // The last main bar is logical 998 (the compared times are the odd indexes), not 499: the line ends there.
    {
        const Span sp = r.nav->span();
        check(std::fabs(r.nav->curveLastX() - sp.x(998)) <= 1.5,
              QString("the line ends at the last bar's index 998 (%1 px, want %2)").arg(r.nav->curveLastX()).arg(sp.x(998)));
    }
    // Drag to the end: the chart's last point (+9).
    auto [x0, x1] = *r.nav->windowX();
    r.drag((x0 + x1) / 2, r.nav->span().w + 1000, 6);
    near(1000 - 0.5 + 9, r.range().to, "the window at the end = the chart's last point + 9");
    // Window and chart agree: clicks.
    for (double x : {300.0, 500.0, 700.0}) {
        const Span sp = r.nav->span();
        r.mouse(QEvent::MouseButtonPress, x);
        r.mouse(QEvent::MouseButtonRelease, x);
        r.frame();
        near(sp.logical(x), r.range().from + r.range().width() / 2, QString("click at %1 px: centred there").arg(x), 1e-9);
    }
    // No data at all: nothing breaks.
    r.chart->removeSeries(cmp);
    r.setBars(0);
    r.frame();
    check(!r.nav->span().ok, "no data: no span");
    check(!r.nav->windowX(), "no data: no window");
    r.drag(100, 400, 4);
    check(true, "dragging over an empty strip: no crash");
    // One bar.
    r.setBars(1);
    r.frame();
    check(r.nav->span().ok, "one bar: a span");
    r.drag(100, 400, 4);
    const auto one = r.chart->timeScale()->getVisibleLogicalRange();
    check(one && std::isfinite(one->from) && std::isfinite(one->to), "one bar: a finite range after a drag");
}

void testRigCacheAndLayout()
{
    CAT = "ui/cache";
    Rig r(3000);
    r.frame();
    const int b0 = r.nav->curveBuilds();
    for (int i = 0; i < 50; ++i) r.nav->repaint();
    check(r.nav->curveBuilds() == b0, QString("50 repaints, nothing changed: the line not rebuilt (%1 → %2)").arg(b0).arg(r.nav->curveBuilds()));
    auto [x0, x1] = *r.nav->windowX();
    r.drag((x0 + x1) / 2, (x0 + x1) / 2 - 200, 20);
    check(r.nav->curveBuilds() == b0, "a pan doesn't rebuild the line either");
    r.addBar();
    r.frame();
    check(r.nav->curveBuilds() == b0 + 1, "a new bar: rebuilt once");
    // Resized (the app window): the strip follows the chart's plot width.
    r.host->resize(800, 536);
    r.chart->setGeometry(0, 0, 800, 500);
    r.nav->setGeometry(0, 500, 800, ChartNavigator::HEIGHT);
    r.frame();
    near(r.chart->timeScale()->width(), r.nav->span().w, "resized: the strip = the new plot width");
    check(r.nav->curveBuilds() == b0 + 2, "resized: the line rebuilt once");
    const Span s = r.nav->span();
    const auto w = *r.nav->windowX();
    near(s.x(r.range().from), w.first, "resized: the window still on the chart's range", 1e-9);
    // Speed: a repaint of the strip, and a pan step with the chart.
    QElapsedTimer t;
    t.start();
    for (int i = 0; i < 200; ++i) r.nav->repaint();
    const double paintMs = double(t.nsecsElapsed()) / 1e6 / 200;
    std::tie(x0, x1) = *r.nav->windowX();
    r.mouse(QEvent::MouseButtonPress, (x0 + x1) / 2);
    t.restart();
    for (int i = 1; i <= 100; ++i) {
        r.mouse(QEvent::MouseMove, (x0 + x1) / 2 - i);
        r.chart->repaint();
        r.nav->repaint();
    }
    const double stepMs = double(t.nsecsElapsed()) / 1e6 / 100;
    r.mouse(QEvent::MouseButtonRelease, (x0 + x1) / 2 - 100);
    check(paintMs < 2, QString("a strip repaint %1 ms (< 2)").arg(paintMs));
    std::cout << "  ui: strip repaint " << paintMs << " ms, pan step (chart + strip) " << stepMs << " ms (offscreen, 3000 bars)\n";
}

void testRigCursorsAndLifetime()
{
    CAT = "ui/cursor";
    {
        Rig r(3000);
        auto [x0, x1] = *r.nav->windowX();
        r.hover(x0);
        check(r.nav->cursor().shape() == Qt::SizeHorCursor, "on the left edge: ew-resize");
        r.hover(x1 + 5);
        check(r.nav->cursor().shape() == Qt::SizeHorCursor, "5 px right of the right edge: ew-resize");
        r.hover((x0 + x1) / 2);
        check(r.nav->cursor().shape() == Qt::OpenHandCursor, "inside: grab");
        r.mouse(QEvent::MouseButtonPress, (x0 + x1) / 2);
        check(r.nav->cursor().shape() == Qt::ClosedHandCursor, "dragging: grabbing");
        r.mouse(QEvent::MouseMove, (x0 + x1) / 2 - 500);   // over the shaded part, still dragging
        check(r.nav->cursor().shape() == Qt::ClosedHandCursor, "dragging over the shaded part: still grabbing");
        r.mouse(QEvent::MouseButtonRelease, (x0 + x1) / 2 - 500);
        r.frame();
        r.hover(10);
        check(r.nav->cursor().shape() == Qt::PointingHandCursor, "outside the window: a click centres it there");
        r.hover(r.nav->span().w + 20);
        check(r.nav->cursor().shape() == Qt::ArrowCursor, "under the price axis: arrow");
    }
    CAT = "ui/lifetime";
    {
        Rig r(3000);
        delete r.chart;   // the chart goes first (a layout change)
        r.chart = nullptr;
        r.nav->repaint();
        r.mouse(QEvent::MouseButtonPress, 100);
        r.mouse(QEvent::MouseMove, 200);
        r.mouse(QEvent::MouseButtonRelease, 200);
        check(!r.nav->windowX(), "the chart gone: no window, no crash");
    }
}

// ================================================================ ui/BarAxis: bars at their own chart index

void testBarAxis()
{
    CAT = "bar axis";
    auto id = [](double t) { return t; };
    // Without a compared symbol: the identity, and the old formula (bar i at index i, a time at its share of tf).
    {
        Rig r(3000);
        BarAxis ax;
        check(ax.build(r.chart->timeScale(), r.bars, id, 60), "built");
        lw::TimeScale *ts = r.chart->timeScale();
        const int i = 2950;
        near(i, ax.logical(i), "no compare: bar i is index i");
        near(*ts->logicalToCoordinate(i), ax.xOfBar(i), "xOfBar = the chart's coordinate", 1e-9);
        near(*ts->logicalToCoordinate(i) - 0.25 * ax.bs, ax.xOfTime(r.bars[i].time + 15), "15 s into a 60 s bar: a quarter in", 1e-9);
        near(*ts->logicalToCoordinate(2999) + 0.5 * ax.bs, ax.xOfTime(r.bars[2999].time + 600), "past the last bar's slot: its right edge", 1e-9);
        check(ax.barAt(r.bars[0].time - 1) == -1, "before the first bar: none");
        near(*ts->logicalToCoordinate(0) - 0.5 * ax.bs, ax.xOfTime(r.bars[0].time - 1000), "before the first bar: its left edge", 1e-9);
        const auto range = *ts->getVisibleLogicalRange();
        check(ax.from <= int(std::floor(range.from)) && ax.to >= std::min(2999, int(std::ceil(range.to))), "the visible bars covered");
    }
    // A compared symbol going back 500 minutes further: every bar's index is 500 more.
    {
        Rig r(3000);
        lw::SeriesOptions so;
        lw::Series *cmp = r.chart->addSeries(lw::SeriesType::Line, so);
        QVector<lw::DataItem> d;
        for (int i = -500; i < 3000; ++i) {
            lw::DataItem it;
            it.time = 1.7e9 + 60.0 * i;
            it.value = 1;
            d.push_back(it);
        }
        cmp->setData(d);
        r.chart->timeScale()->setVisibleLogicalRange({3500 - 160, 3500 + 8});
        r.frame();
        BarAxis ax;
        check(ax.build(r.chart->timeScale(), r.bars, id, 60), "built (compare going further back)");
        lw::TimeScale *ts = r.chart->timeScale();
        near(500, ax.logical(0), "bar 0 is index 500");
        near(3499, ax.logical(2999), "the last bar is index 3499 (not 2999)");
        near(*ts->logicalToCoordinate(3490), ax.xOfBar(2990), "xOfBar(2990) = the chart's index 3490", 1e-9);
        check(ax.from >= 2990 - 170 && ax.to == 2999, QString("the visible bars are the last ones (%1 … %2)").arg(ax.from).arg(ax.to));
        check(ax.adjacent(2990), "bars side by side on the chart");
    }
    // A compared symbol's minutes between the bars (+30 s): bar i is index 2i, a bar's slot ends at the next point.
    {
        Rig r(1000);
        lw::SeriesOptions so;
        lw::Series *cmp = r.chart->addSeries(lw::SeriesType::Line, so);
        QVector<lw::DataItem> d;
        for (int i = 0; i < 1000; ++i) {
            lw::DataItem it;
            it.time = 1.7e9 + 60.0 * i + 30;
            it.value = 1;
            d.push_back(it);
        }
        cmp->setData(d);
        r.chart->timeScale()->setVisibleLogicalRange({1800, 2000});
        r.frame();
        BarAxis ax;
        check(ax.build(r.chart->timeScale(), r.bars, id, 60), "built (interleaved)");
        lw::TimeScale *ts = r.chart->timeScale();
        near(1900, ax.logical(950), "bar 950 is index 1900");
        near(*ts->logicalToCoordinate(1900), ax.xOfBar(950), "at the chart's coordinate", 1e-9);
        near(*ts->logicalToCoordinate(1900), ax.xOfTime(r.bars[950].time + 15), "15 s in: half its slot (30 s to the next point)", 1e-9);
        check(!ax.adjacent(950), "bars not side by side (a compared point between): a heatmap run breaks there");
        check(ax.from <= 900 && ax.to >= 999, QString("the visible bars (indexes 1800 … 2000 = bars 900 … 1000): %1 … %2").arg(ax.from).arg(ax.to));
    }
}

} // namespace

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    testSpan();
    testHit();
    testConstrain();
    testGesture();
    testFuzz();
    testRig();
    testRigDrag();
    testRigSync();
    testRigLive();
    testRigData();
    testRigCacheAndLayout();
    testRigCursorsAndLifetime();
    testBarAxis();
    std::cout << checks << " checks, " << fails << " failed\n";
    return fails ? 1 : 0;
}
