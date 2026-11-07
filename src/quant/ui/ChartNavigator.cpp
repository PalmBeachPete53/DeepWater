// The range slider under a chart pane (see ChartNavigator.h). A Deepwater addition.
#include "ChartNavigator.h"

#include "Theme.h"

#include "../chart/Chart.h"

#include <QMouseEvent>
#include <QPainter>

#include <algorithm>
#include <cmath>

namespace qtm {

using namespace rangeslider;

namespace {
bool darkTheme()
{
    const QColor bg = theme::chartColor("--bg");
    return 0.2126 * bg.redF() + 0.7152 * bg.greenF() + 0.0722 * bg.blueF() < 0.5;
}
} // namespace

ChartNavigator::ChartNavigator(lw::Chart *chart, std::function<const Bars &()> bars, std::function<double(double)> toChart,
                               QWidget *parent)
    : QWidget(parent), m_chart(chart), m_bars(std::move(bars)), m_toChart(std::move(toChart))
{
    setMouseTracking(true);   // the cursor follows what's under the mouse
    setAttribute(Qt::WA_OpaquePaintEvent);
    connect(chart, &lw::Chart::visibleLogicalRangeChanged, this, qOverload<>(&QWidget::update));
    connect(chart, &QObject::destroyed, this, [this] { m_chart = nullptr; });
}

Span ChartNavigator::span() const
{
    if (!m_chart) return {};
    lw::TimeScale *ts = m_chart->timeScale();
    return rangeslider::span(ts->pointCount(), ts->width(), params());
}

std::optional<Range> ChartNavigator::range() const
{
    if (!m_chart) return std::nullopt;
    const auto r = m_chart->timeScale()->getVisibleLogicalRange();
    if (!r) return std::nullopt;
    return Range{r->from - 0.5, r->to + 0.5};
}

std::optional<std::pair<double, double>> ChartNavigator::windowX() const
{
    const Span s = span();
    const auto r = range();
    if (!s.ok || !r) return std::nullopt;
    const Range w = clip(*r, s);
    return std::pair(s.x(w.from), s.x(w.to));
}

void ChartNavigator::set(const Range &r)
{
    if (m_chart) m_chart->timeScale()->setVisibleLogicalRange({r.from + 0.5, r.to - 0.5});   // edges → the edge bars' centres
}

void ChartNavigator::updateCursor(double x)
{
    switch (m_gesture.drag()) {
    case Drag::Pan:
        setCursor(Qt::ClosedHandCursor);   // grabbing
        return;
    case Drag::Left:
    case Drag::Right:
        setCursor(Qt::SizeHorCursor);      // ew-resize
        return;
    case Drag::None:
        break;
    }
    const Span s = span();
    const auto r = range();
    switch (r ? hit(x, *r, s, params()) : Drag::None) {
    case Drag::Left:
    case Drag::Right:
        setCursor(Qt::SizeHorCursor);
        break;
    case Drag::Pan:
        setCursor(Qt::OpenHandCursor);     // grab
        break;
    case Drag::None:
        setCursor(s.ok && x <= s.w ? Qt::PointingHandCursor : Qt::ArrowCursor);
        break;
    }
}

void ChartNavigator::mousePressEvent(QMouseEvent *e)
{
    // Qt keeps sending the mouse to the widget pressed until the button is let go (what setPointerCapture does on the
    // web): the drag goes on when the mouse leaves the strip.
    if (e->button() != Qt::LeftButton) return;
    const auto r = range();
    if (!r) return;
    if (const auto to = m_gesture.press(e->position().x(), *r, span())) set(*to);
    updateCursor(e->position().x());
    update();
    e->accept();
}

void ChartNavigator::mouseMoveEvent(QMouseEvent *e)
{
    const double x = e->position().x();
    if (m_gesture.drag() == Drag::None) {
        updateCursor(x);
        return;
    }
    if (const auto to = m_gesture.move(x, span())) set(*to);
    e->accept();
}

void ChartNavigator::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton) return;
    m_gesture.release();
    updateCursor(e->position().x());
    update();
}

void ChartNavigator::leaveEvent(QEvent *)
{
    if (m_gesture.drag() == Drag::None) unsetCursor();
}

// The closes as a line over the track, each bar at its logical index: per pixel column the bars in it (first, lowest,
// highest, last close: thousands of bars still make at most 4 points a pixel), with a light area under it. Drawn into an
// image once (a few thousand antialiased segments cost ~4 ms a frame), again only when the bars, the size or the
// theme change.
const QPixmap &ChartNavigator::trackImage(const Span &s, const QRectF &track)
{
    const Bars &bars = m_bars();
    CurveKey key;
    key.n = int(bars.size());
    key.points = m_chart ? m_chart->timeScale()->pointCount() : 0;
    if (key.n) {
        key.first = bars.first().time;
        key.last = bars.last().time;
        key.lastClose = bars.last().close;
    }
    key.w = track.width();
    key.h = track.height();
    key.dpr = devicePixelRatioF();
    key.theme = theme::chartColor("--accent").rgb() ^ (theme::chartColor("--panel").rgb() << 1);
    if (key == m_trackKey && !m_track.isNull()) return m_track;
    m_trackKey = key;
    ++m_curveBuilds;
    m_curvePoints = 0;

    const QSize px(std::max(1, int(std::ceil(track.width() * key.dpr))), std::max(1, int(std::ceil(track.height() * key.dpr))));
    m_track = QPixmap(px);
    m_track.setDevicePixelRatio(key.dpr);
    m_track.fill(theme::chartColor("--panel"));
    if (key.n < 2 || !m_chart) return m_track;

    // Each bar's logical index (a compared symbol's times can sit between them).
    lw::TimeScale *ts = m_chart->timeScale();
    double mn = INFINITY, mx = -INFINITY;
    for (const Bar &b : bars)
        if (std::isfinite(b.close)) {
            mn = std::min(mn, b.close);
            mx = std::max(mx, b.close);
        }
    if (!(mx >= mn)) return m_track;
    const double pad = 3, h = track.height();
    auto y = [&](double v) { return h - pad - (mx > mn ? (v - mn) / (mx - mn) : 0.5) * (h - 2 * pad); };
    QPainterPath line;
    bool started = false;
    int col = -1;
    double first = NAN, lo = INFINITY, hi = -INFINITY, last = NAN;
    auto flush = [&] {
        if (col < 0 || std::isnan(first)) return;
        const double x = col + 0.5;
        m_curveLastX = x;
        if (!started) {
            line.moveTo(x, y(first));
            started = true;
        }
        line.lineTo(x, y(first));
        if (hi > lo) {
            line.lineTo(x, y(lo == first ? hi : lo));
            line.lineTo(x, y(lo == first ? lo : hi));
        }
        line.lineTo(x, y(last));
    };
    for (int i = 0; i < key.n; ++i) {
        const double v = bars[i].close;
        if (!std::isfinite(v)) continue;
        const auto L = ts->timeToLogical(m_toChart(bars[i].time));
        if (!L) continue;
        ++m_curvePoints;
        const int c = std::clamp(int(std::floor(s.x(double(*L)))), 0, int(std::ceil(track.width())));
        if (c != col) {
            flush();
            col = c;
            first = v;
            lo = hi = last = v;
        } else {
            lo = std::min(lo, v);
            hi = std::max(hi, v);
            last = v;
        }
    }
    flush();
    if (line.isEmpty()) return m_track;
    QPainter q(&m_track);
    q.setRenderHint(QPainter::Antialiasing, true);
    QPainterPath area = line;
    area.lineTo(line.currentPosition().x(), h);
    area.lineTo(line.elementAt(0).x, h);
    area.closeSubpath();
    q.fillPath(area, theme::chartColor("--accent", 0.12));
    q.setPen(QPen(theme::chartColor("--accent", 0.7), 1));
    q.setBrush(Qt::NoBrush);
    q.drawPath(line);
    return m_track;
}

void ChartNavigator::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    const QRectF all = rect();
    p.fillRect(all, theme::chartColor("--bg"));
    p.setPen(QPen(theme::chartColor("--border"), 1));
    p.drawLine(QPointF(0, 0.5), QPointF(all.width(), 0.5));
    const Span s = span();
    if (!s.ok) return;
    const QRectF track(0, 4, s.w, all.height() - 8);
    p.drawPixmap(track.topLeft(), trackImage(s, track));

    // The window: what the chart shows (cut to the strip), the rest shaded.
    const auto w = windowX();
    if (!w) return;
    const double x0 = w->first, x1 = w->second;
    const QColor shade = darkTheme() ? QColor(0, 0, 0, 110) : QColor(0, 0, 0, 38);
    if (x0 > 0) p.fillRect(QRectF(0, track.top(), x0, track.height()), shade);
    if (x1 < s.w) p.fillRect(QRectF(x1, track.top(), s.w - x1, track.height()), shade);
    const QColor accent = theme::chartColor("--accent");
    p.setPen(QPen(accent, 1));
    // (an explicit QBrush: `? QColor : Qt::NoBrush` would make NoBrush the colour 0, black)
    p.setBrush(m_gesture.drag() == Drag::Pan ? QBrush(theme::chartColor("--accent", 0.10)) : QBrush(Qt::NoBrush));
    p.drawRect(QRectF(x0 + 0.5, track.top() + 0.5, std::max(1.0, x1 - x0 - 1), track.height() - 1));
    // Handles: a small bar on each edge with a grip.
    p.setRenderHint(QPainter::Antialiasing, true);
    const double hh = track.height() * 0.62, hw = 6;
    for (double hx : {x0, x1}) {
        const QRectF h(hx - hw / 2, track.center().y() - hh / 2, hw, hh);
        p.setPen(QPen(theme::chartColor("--bg"), 1));
        p.setBrush(accent);
        p.drawRoundedRect(h, 2, 2);
        p.setPen(QPen(theme::chartColor("--on-accent", 0.8), 1));
        p.drawLine(QPointF(hx, h.top() + 3), QPointF(hx, h.bottom() - 3));
    }
}

} // namespace qtm
