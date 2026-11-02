#include "ResearchCharts.h"

#include "ResearchData.h"
#include "Theme.h"

#include <QDesktopServices>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QUrl>

#include <algorithm>
#include <cmath>

namespace research {

namespace palette {
QColor blue() { return QColor(0x5b, 0x9b, 0xe6); }
QColor yellow() { return QColor(0xf0, 0xb4, 0x4c); }
QColor red() { return QColor(0xe3, 0x62, 0x4f); }
QColor green() { return QColor(0x5f, 0xd3, 0x8d); }
QColor purple() { return QColor(0xa2, 0x60, 0xf0); }
QColor cyan() { return QColor(0x4c, 0xc9, 0xe8); }
QColor pink() { return QColor(0xe0, 0x52, 0x97); }
QColor orange() { return QColor(0xf0, 0x8a, 0x4c); }
QColor darkGreen() { return QColor(0x2f, 0x9e, 0x5a); }
}

namespace {

QFont smallFont(int px, bool bold = false)
{
    QFont f;
    f.setFamilies(Theme::monoFamilies());
    f.setPixelSize(px);
    f.setBold(bold);
    return f;
}

// A floating box of lines (the first one bold) near a point, kept inside `bounds`.
void drawTooltip(QPainter &p, const QPointF &at, const QStringList &lines, const QVector<QColor> &swatches,
                 const QRectF &bounds)
{
    if (lines.isEmpty())
        return;
    const Palette &c = Theme::current();
    const QFont bold = smallFont(11, true), normal = smallFont(11);
    const QFontMetrics fb(bold), fn(normal);
    int w = 0;
    for (int i = 0; i < lines.size(); ++i)
        w = std::max(w, (i == 0 ? fb : fn).horizontalAdvance(lines[i]) + (i > 0 && i - 1 < swatches.size() ? 14 : 0));
    const int lineH = 17;
    QRectF box(at.x() + 14, at.y() - 10, w + 20, lines.size() * lineH + 12);
    if (box.right() > bounds.right())
        box.moveRight(at.x() - 14);
    if (box.bottom() > bounds.bottom())
        box.moveBottom(bounds.bottom());
    if (box.top() < bounds.top())
        box.moveTop(bounds.top());
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(c.TooltipBorder, 1));
    QColor bg = c.Tooltip;
    bg.setAlpha(235);
    p.setBrush(bg);
    p.drawRoundedRect(box, 4, 4);
    for (int i = 0; i < lines.size(); ++i) {
        const QRectF r(box.left() + 10, box.top() + 6 + i * lineH, box.width() - 20, lineH);
        p.setFont(i == 0 ? bold : normal);
        p.setPen(i == 0 ? c.Text : c.TextDim);
        double x = r.left();
        if (i > 0 && i - 1 < swatches.size() && swatches[i - 1].isValid()) {
            p.fillRect(QRectF(x, r.center().y() - 4, 8, 8), swatches[i - 1]);
            x += 14;
        }
        p.drawText(QRectF(x, r.top(), r.right() - x, r.height()), Qt::AlignVCenter | Qt::AlignLeft, lines[i]);
    }
    p.restore();
}

void drawYAxis(QPainter &p, const QRectF &plot, const QVector<double> &ticks, double lo, double hi, const Format &format,
               bool rightSide = false)
{
    const Palette &c = Theme::current();
    p.setFont(smallFont(10));
    for (double t : ticks) {
        const double y = plot.bottom() - (t - lo) / (hi - lo) * plot.height();
        if (!rightSide) {
            p.setPen(QPen(c.GridLine, 1));
            p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        }
        p.setPen(c.TextFaint);
        const QRectF label = rightSide ? QRectF(plot.right() + 6, y - 8, 70, 16) : QRectF(0, y - 8, plot.left() - 8, 16);
        p.drawText(label, (rightSide ? Qt::AlignLeft : Qt::AlignRight) | Qt::AlignVCenter, format ? format(t) : QString::number(t));
    }
}

double niceStep(double raw)
{
    const double mag = std::pow(10, std::floor(std::log10(raw)));
    const double f = raw / mag;
    return (f <= 1 ? 1 : f <= 2 ? 2 : f <= 2.5 ? 2.5 : f <= 5 ? 5 : 10) * mag;
}

} // namespace

QVector<double> niceTicks(double lo, double hi, int count)
{
    if (!std::isfinite(lo) || !std::isfinite(hi))
        return {};
    if (hi <= lo)
        hi = lo + (lo == 0 ? 1 : std::abs(lo) * 0.1);
    const double step = niceStep((hi - lo) / std::max(1, count));
    QVector<double> out;
    for (double t = std::floor(lo / step) * step;; t += step) {
        out.push_back(std::abs(t) < step * 1e-9 ? 0 : t);
        if (t >= hi - step * 1e-9 || out.size() > 50)
            break;
    }
    return out;
}

// ---------------------------------------------------------------- BarChart

BarChart::BarChart(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setMinimumHeight(260);
}

void BarChart::setData(const QStringList &categories, const QVector<Series> &series, Format format, bool showChange,
                       bool changeInPoints)
{
    m_categories = categories;
    m_series = series;
    m_format = std::move(format);
    m_showChange = showChange;
    m_changeInPoints = changeInPoints;
    update();
}

QRectF BarChart::plotRect() const
{
    const bool legend = m_series.size() > 1;
    return QRectF(70, 24, width() - 70 - 16, height() - 24 - 30 - (legend ? 26 : 0));
}

void BarChart::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const Palette &c = Theme::current();
    const QRectF plot = plotRect();
    if (m_categories.isEmpty()) {
        p.setPen(c.TextFaint);
        p.setFont(smallFont(11));
        p.drawText(rect(), Qt::AlignCenter, tr("No data"));
        return;
    }
    double lo = 0, hi = 0;
    for (const Series &s : m_series)
        for (double v : s.values)
            if (ok(v)) {
                lo = std::min(lo, v);
                hi = std::max(hi, v);
            }
    // Room above the tallest bar for its change label.
    const QVector<double> ticks = niceTicks(lo < 0 ? lo * 1.08 : lo, hi * 1.08, 5);
    lo = ticks.first();
    hi = ticks.last();
    drawYAxis(p, plot, ticks, lo, hi, m_format);
    auto yOf = [&](double v) { return plot.bottom() - (v - lo) / (hi - lo) * plot.height(); };

    const int n = m_categories.size(), k = std::max<int>(1, m_series.size());
    const double groupW = plot.width() / n;
    const double gap = groupW * (k > 1 ? 0.18 : 0.2);
    const double barW = (groupW - gap) / k;
    if (m_hover >= 0) {
        QColor h = c.Hover;
        p.fillRect(QRectF(plot.left() + m_hover * groupW, plot.top(), groupW, plot.height()), h);
    }
    const double zero = yOf(0);
    const QFont changeFont = smallFont(9, true);
    for (int i = 0; i < n; ++i) {
        for (int s = 0; s < m_series.size(); ++s) {
            const double v = m_series[s].values.value(i, NaN);
            if (!ok(v))
                continue;
            const double x = plot.left() + i * groupW + gap / 2 + s * barW;
            const double y = yOf(v);
            QRectF bar(x + 1, std::min(y, zero), barW - 2, std::abs(zero - y));
            QPainterPath path;
            path.addRoundedRect(bar, 2, 2);
            p.fillPath(path, m_series[s].color);
            if (m_showChange && i > 0) {
                const double prev = m_series[s].values.value(i - 1, NaN);
                if (ok(prev) && prev != 0) {
                    const double ch = m_changeInPoints ? (v - prev) * 100 : (v - prev) / std::abs(prev);
                    const QString t = m_changeInPoints ? QString("%1%2").arg(ch > 0 ? "+" : "").arg(ch, 0, 'f', 0) + "%"
                                                       : percent(ch, 0);
                    p.setFont(changeFont);
                    p.setPen(ch >= 0 ? c.Up : c.Down);
                    const QRectF tr(x - 10, (v >= 0 ? bar.top() - 14 : bar.bottom() + 2), barW + 20, 12);
                    p.drawText(tr, Qt::AlignCenter, t);
                }
            }
        }
        p.setFont(smallFont(10, true));
        p.setPen(c.TextDim);
        p.drawText(QRectF(plot.left() + i * groupW, plot.bottom() + 6, groupW, 16), Qt::AlignCenter, m_categories[i]);
    }
    if (lo < 0) {
        p.setPen(QPen(c.ControlBorderHover, 1));
        p.drawLine(QPointF(plot.left(), zero), QPointF(plot.right(), zero));
    }
    // Legend.
    if (m_series.size() > 1) {
        p.setFont(smallFont(10));
        const QFontMetrics fm(p.font());
        double total = 0;
        for (const Series &s : m_series)
            total += 14 + fm.horizontalAdvance(s.name) + 18;
        double x = plot.center().x() - total / 2;
        const double y = height() - 16;
        for (const Series &s : m_series) {
            p.fillRect(QRectF(x, y - 4, 9, 9), s.color);
            p.setPen(c.TextDim);
            p.drawText(QPointF(x + 14, y + 4), s.name);
            x += 14 + fm.horizontalAdvance(s.name) + 18;
        }
    }
    if (m_hover >= 0 && m_hover < n) {
        QStringList lines {m_categories[m_hover]};
        QVector<QColor> sw;
        for (const Series &s : m_series) {
            const double v = s.values.value(m_hover, NaN);
            lines << QString("%1: %2").arg(s.name, ok(v) ? m_format(v) : "N/A");
            sw << s.color;
        }
        drawTooltip(p, mapFromGlobal(QCursor::pos()), lines, sw, rect());
    }
}

void BarChart::mouseMoveEvent(QMouseEvent *e)
{
    const QRectF plot = plotRect();
    int h = -1;
    if (plot.contains(e->position()) && !m_categories.isEmpty())
        h = int((e->position().x() - plot.left()) / (plot.width() / m_categories.size()));
    if (h != m_hover || h >= 0) {
        m_hover = h;
        update();
    }
}

void BarChart::leaveEvent(QEvent *)
{
    m_hover = -1;
    update();
}

// ---------------------------------------------------------------- LineChart

LineChart::LineChart(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setMinimumHeight(200);
}

void LineChart::setData(const QStringList &categories, const QVector<Series> &series, Format format)
{
    m_categories = categories;
    m_series = series;
    m_format = std::move(format);
    update();
}

QRectF LineChart::plotRect() const
{
    const int bottom = (m_rotated ? 44 : 26) + (m_legend ? 28 : 0);
    return QRectF(72, 16, width() - 72 - 20, height() - 16 - bottom);
}

void LineChart::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const Palette &c = Theme::current();
    const QRectF plot = plotRect();
    double lo = INFINITY, hi = -INFINITY;
    for (const Series &s : m_series)
        for (double v : s.values)
            if (ok(v)) {
                lo = std::min(lo, v);
                hi = std::max(hi, v);
            }
    if (m_categories.isEmpty() || !std::isfinite(lo)) {
        p.setPen(c.TextFaint);
        p.setFont(smallFont(11));
        p.drawText(rect(), Qt::AlignCenter, tr("No data"));
        return;
    }
    const double pad = (hi - lo) * 0.08;
    const QVector<double> ticks = niceTicks(lo - pad, hi + pad, 6);
    lo = ticks.first();
    hi = ticks.last();
    drawYAxis(p, plot, ticks, lo, hi, m_format);
    const int n = m_categories.size();
    auto xOf = [&](int i) { return n == 1 ? plot.center().x() : plot.left() + plot.width() * i / (n - 1); };
    auto yOf = [&](double v) { return plot.bottom() - (v - lo) / (hi - lo) * plot.height(); };

    // X labels: as many as fit.
    p.setFont(smallFont(10));
    const QFontMetrics fm(p.font());
    int maxLabel = 0;
    for (const QString &s : m_categories)
        maxLabel = std::max(maxLabel, fm.horizontalAdvance(s));
    const int every = m_labelEvery > 0 ? m_labelEvery
                                       : std::max(1, int(std::ceil((maxLabel + 14) * (m_rotated ? 0.55 : 1.0) * n / plot.width())));
    p.setPen(c.TextFaint);
    for (int i = 0; i < n; i += every) {
        const double x = xOf(i);
        p.setPen(QPen(c.GridLine, 1));
        p.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
        p.setPen(c.TextFaint);
        if (m_rotated) {
            p.save();
            p.translate(x, plot.bottom() + 8);
            p.rotate(-35);
            p.drawText(QRectF(-maxLabel - 4, -8, maxLabel, 16), Qt::AlignRight | Qt::AlignVCenter, m_categories[i]);
            p.restore();
        } else {
            p.drawText(QRectF(x - 50, plot.bottom() + 6, 100, 16), Qt::AlignCenter, m_categories[i]);
        }
    }
    for (const Series &s : m_series) {
        QPainterPath line, area;
        bool started = false;
        double firstX = 0, lastX = 0;
        for (int i = 0; i < n; ++i) {
            const double v = s.values.value(i, NaN);
            if (!ok(v))
                continue;
            const QPointF pt(xOf(i), yOf(v));
            if (!started) {
                line.moveTo(pt);
                area.moveTo(pt.x(), plot.bottom());
                area.lineTo(pt);
                firstX = pt.x();
                started = true;
            } else {
                line.lineTo(pt);
                area.lineTo(pt);
            }
            lastX = pt.x();
        }
        if (!started)
            continue;
        if (s.area) {
            area.lineTo(lastX, plot.bottom());
            area.lineTo(firstX, plot.bottom());
            QLinearGradient g(0, plot.top(), 0, plot.bottom());
            QColor top = s.color;
            top.setAlpha(70);
            QColor bottom = s.color;
            bottom.setAlpha(5);
            g.setColorAt(0, top);
            g.setColorAt(1, bottom);
            p.fillPath(area, g);
        }
        QPen pen(s.color, 2);
        if (s.dashed)
            pen.setStyle(Qt::DashLine);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawPath(line);
        if (s.dots) {
            p.setBrush(s.color);
            p.setPen(QPen(c.Background, 1));
            for (int i = 0; i < n; ++i) {
                const double v = s.values.value(i, NaN);
                if (ok(v))
                    p.drawEllipse(QPointF(xOf(i), yOf(v)), m_hover == i ? 5 : 3.5, m_hover == i ? 5 : 3.5);
            }
        }
    }
    if (m_legend) {
        p.setFont(smallFont(10));
        double total = 0;
        for (const Series &s : m_series)
            if (!s.name.isEmpty())
                total += 26 + fm.horizontalAdvance(s.name) + 18;
        double x = plot.center().x() - total / 2;
        const double y = height() - 14;
        for (const Series &s : m_series) {
            if (s.name.isEmpty())
                continue;
            QPen pen(s.color, 2);
            if (s.dashed)
                pen.setStyle(Qt::DashLine);
            p.setPen(pen);
            p.drawLine(QPointF(x, y), QPointF(x + 18, y));
            p.setPen(c.TextDim);
            p.drawText(QPointF(x + 24, y + 4), s.name);
            x += 26 + fm.horizontalAdvance(s.name) + 18;
        }
    }
    if (m_hover >= 0 && m_hover < n) {
        const double x = xOf(m_hover);
        p.setPen(QPen(c.ControlBorderHover, 1, Qt::DashLine));
        p.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
        QStringList lines {m_tooltipTitle.isEmpty() ? m_categories[m_hover] : m_tooltipTitle + " - " + m_categories[m_hover]};
        QVector<QColor> sw;
        for (const Series &s : m_series) {
            const double v = s.values.value(m_hover, NaN);
            if (!ok(v))
                continue;
            lines << QString("%1: %2").arg(s.name, m_format(v));
            sw << s.color;
        }
        drawTooltip(p, QPointF(x, mapFromGlobal(QCursor::pos()).y()), lines, sw, rect());
    }
}

void LineChart::mouseMoveEvent(QMouseEvent *e)
{
    const QRectF plot = plotRect();
    const int n = m_categories.size();
    int h = -1;
    if (n > 0 && e->position().x() >= plot.left() - 10 && e->position().x() <= plot.right() + 10)
        h = n == 1 ? 0 : std::clamp(int(std::round((e->position().x() - plot.left()) / plot.width() * (n - 1))), 0, n - 1);
    m_hover = h;
    update();
}

void LineChart::leaveEvent(QEvent *)
{
    m_hover = -1;
    update();
}

// ---------------------------------------------------------------- DonutChart

DonutChart::DonutChart(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setMinimumSize(160, 160);
}

void DonutChart::setData(const QVector<Slice> &slices, const QString &centerTitle, const QString &centerSub,
                         const QString &centerValue, const QColor &centerColor)
{
    m_slices = slices;
    m_title = centerTitle;
    m_sub = centerSub;
    m_value = centerValue;
    m_valueColor = centerColor;
    update();
}

void DonutChart::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const Palette &c = Theme::current();
    double total = 0;
    for (const Slice &s : m_slices)
        total += std::max(0.0, s.value);
    const double margin = m_leaders ? std::min(width(), height()) * 0.24 : 6;
    const double d = std::min(width(), height()) - 2 * margin;
    const QRectF outer(width() / 2.0 - d / 2, height() / 2.0 - d / 2, d, d);
    const double inner = d / 2 * (1 - m_thickness);
    if (total <= 0) {
        p.setPen(QPen(c.PanelLine, d / 2 - inner));
        const double r = (d / 2 + inner) / 2;
        p.drawEllipse(outer.center(), r, r);
        return;
    }
    double angle = m_start;
    const QPointF center = outer.center();
    struct LabelPos {
        QPointF anchor, elbow;
        bool right;
        int index;
    };
    QVector<LabelPos> labels;
    for (int i = 0; i < m_slices.size(); ++i) {
        const Slice &s = m_slices[i];
        const double span = -360.0 * std::max(0.0, s.value) / total;
        QPainterPath path;
        QRectF o = outer;
        if (i == m_hover)
            o.adjust(-4, -4, 4, 4);
        path.arcMoveTo(o, angle);
        path.arcTo(o, angle, span);
        const QRectF in(center.x() - inner, center.y() - inner, inner * 2, inner * 2);
        path.arcTo(in, angle + span, -span);
        path.closeSubpath();
        p.fillPath(path, s.color);
        p.setPen(QPen(c.Panel, 1.2));
        p.drawPath(path);
        if (m_leaders && std::abs(span) > 0.3) {
            const double mid = (angle + span / 2) * M_PI / 180;
            const QPointF dir(std::cos(mid), -std::sin(mid));
            labels.push_back({center + dir * (d / 2 + 2), center + dir * (d / 2 + 22), dir.x() >= 0, i});
        }
        angle += span;
    }
    // Center.
    p.setPen(Qt::NoPen);
    p.setBrush(c.Background);
    p.drawEllipse(center, inner - 2, inner - 2);
    p.setPen(c.Accent);
    p.setFont(smallFont(std::max(9, int(inner / 6)), true));
    const double ty = center.y() - inner * 0.35;
    if (!m_title.isEmpty())
        p.drawText(QRectF(center.x() - inner, ty - 10, inner * 2, 20), Qt::AlignCenter, m_title);
    p.setPen(c.TextFaint);
    p.setFont(smallFont(std::max(8, int(inner / 8))));
    if (!m_sub.isEmpty())
        p.drawText(QRectF(center.x() - inner, ty + 6, inner * 2, 16), Qt::AlignCenter, m_sub);
    if (!m_value.isEmpty()) {
        p.setPen(m_valueColor.isValid() ? m_valueColor : c.Text);
        p.setFont(smallFont(std::max(10, int(inner / 4.5)), true));
        p.drawText(QRectF(center.x() - inner, center.y() + inner * 0.05, inner * 2, inner * 0.5), Qt::AlignCenter, m_value);
    }
    // Leader labels, spread so they don't overlap on each side.
    if (m_leaders) {
        for (int side = 0; side < 2; ++side) {
            QVector<LabelPos *> list;
            for (LabelPos &l : labels)
                if (l.right == (side == 1))
                    list.push_back(&l);
            std::sort(list.begin(), list.end(), [](LabelPos *a, LabelPos *b) { return a->elbow.y() < b->elbow.y(); });
            double last = -1e9;
            for (LabelPos *l : list) {
                if (l->elbow.y() < last + 26)
                    l->elbow.setY(last + 26);
                last = l->elbow.y();
            }
        }
        for (const LabelPos &l : labels) {
            const Slice &s = m_slices[l.index];
            const double endX = l.elbow.x() + (l.right ? 16 : -16);
            p.setPen(QPen(c.TextFaint, 1));
            p.drawLine(l.anchor, l.elbow);
            p.drawLine(l.elbow, QPointF(endX, l.elbow.y()));
            const QRectF r = l.right ? QRectF(endX + 4, l.elbow.y() - 13, 120, 13) : QRectF(endX - 124, l.elbow.y() - 13, 120, 13);
            p.setFont(smallFont(10));
            p.setPen(c.Text);
            p.drawText(r, (l.right ? Qt::AlignLeft : Qt::AlignRight) | Qt::AlignVCenter, s.label);
            if (!s.detail.isEmpty()) {
                p.setPen(s.detailColor.isValid() ? s.detailColor : c.TextDim);
                p.drawText(r.translated(0, 12), (l.right ? Qt::AlignLeft : Qt::AlignRight) | Qt::AlignVCenter, s.detail);
            }
        }
    }
    if (m_hover >= 0 && m_hover < m_slices.size()) {
        const Slice &s = m_slices[m_hover];
        QStringList lines {s.label, QString("%1").arg(percent(s.value / total, 1, false))};
        if (!s.detail.isEmpty())
            lines << s.detail;
        drawTooltip(p, mapFromGlobal(QCursor::pos()), lines, {}, rect());
    }
}

void DonutChart::mouseMoveEvent(QMouseEvent *e)
{
    const double margin = m_leaders ? std::min(width(), height()) * 0.24 : 6;
    const double d = std::min(width(), height()) - 2 * margin;
    const QPointF center(width() / 2.0, height() / 2.0);
    const QPointF v = e->position() - center;
    const double r = std::hypot(v.x(), v.y());
    int h = -1;
    double total = 0;
    for (const Slice &s : m_slices)
        total += std::max(0.0, s.value);
    if (total > 0 && r <= d / 2 + 4 && r >= d / 2 * (1 - m_thickness) - 2) {
        double a = std::atan2(-v.y(), v.x()) * 180 / M_PI;   // counter-clockwise from 3 o'clock
        double rel = m_start - a;                            // slices run clockwise from m_start
        rel = std::fmod(std::fmod(rel, 360) + 360, 360);
        double acc = 0;
        for (int i = 0; i < m_slices.size(); ++i) {
            acc += 360.0 * std::max(0.0, m_slices[i].value) / total;
            if (rel <= acc) {
                h = i;
                break;
            }
        }
    }
    if (h != m_hover || h >= 0) {
        m_hover = h;
        update();
    }
}

void DonutChart::leaveEvent(QEvent *)
{
    m_hover = -1;
    update();
}

// ---------------------------------------------------------------- Gauge

Gauge::Gauge(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(220, 170);
}

void Gauge::setValue(double score, const QString &label)
{
    m_score = score;
    m_label = label;
    update();
}

void Gauge::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const Palette &c = Theme::current();
    const double r = std::min(width() / 2.0 - 50, height() - 70.0);
    const QPointF center(width() / 2.0, 30 + r);
    const QRectF arc(center.x() - r, center.y() - r, 2 * r, 2 * r);
    // Five zones from strong sell (left) to strong buy (right).
    const QColor zones[5] {palette::red(), palette::orange(), palette::yellow(), palette::green(), palette::darkGreen()};
    for (int i = 0; i < 5; ++i) {
        QPen pen(zones[i], 9, Qt::SolidLine, Qt::FlatCap);
        p.setPen(pen);
        p.drawArc(arc, int((180 - i * 36) * 16), int(-34 * 16));
    }
    const char *names[5] {QT_TR_NOOP("Strong\nSell"), QT_TR_NOOP("Sell"), QT_TR_NOOP("Hold"), QT_TR_NOOP("Buy"), QT_TR_NOOP("Strong\nBuy")};
    p.setFont(smallFont(10));
    p.setPen(c.TextDim);
    for (int i = 0; i < 5; ++i) {
        const double a = (180 - (i * 36 + 18)) * M_PI / 180;
        const QPointF at = center + QPointF(std::cos(a), -std::sin(a)) * (r + 26);
        p.drawText(QRectF(at.x() - 30, at.y() - 12, 60, 24), Qt::AlignCenter, tr(names[i]));
    }
    if (ok(m_score)) {
        const double t = std::clamp((m_score - 1) / 4, 0.0, 1.0);
        const double a = (180 - t * 180) * M_PI / 180;
        const QPointF tip = center + QPointF(std::cos(a), -std::sin(a)) * (r - 14);
        p.setPen(QPen(c.Accent, 2.5, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(center, tip);
        p.setBrush(c.Accent);
        p.setPen(Qt::NoPen);
        p.drawEllipse(center, 5, 5);
    }
    p.setPen(c.Text);
    p.setFont(smallFont(14, true));
    p.drawText(QRectF(0, center.y() + 14, width(), 24), Qt::AlignCenter, m_label.toUpper());
}

// ---------------------------------------------------------------- HBars

HBars::HBars(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void HBars::setData(const QVector<Bar> &bars, double max)
{
    m_bars = bars;
    m_max = max;
    updateGeometry();
    update();
}

QSize HBars::sizeHint() const
{
    return QSize(300, std::max<int>(1, m_bars.size()) * m_rowHeight);
}

void HBars::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const Palette &c = Theme::current();
    double max = m_max;
    for (const Bar &b : m_bars)
        max = std::max(max, b.value);
    const double left = m_labelWidth + 8, right = width() - 4;
    for (int i = 0; i < m_bars.size(); ++i) {
        const Bar &b = m_bars[i];
        const QRectF row(0, i * m_rowHeight, width(), m_rowHeight);
        p.setFont(smallFont(10));
        p.setPen(c.TextDim);
        p.drawText(QRectF(0, row.top(), m_labelWidth, row.height()), Qt::AlignRight | Qt::AlignVCenter, b.label);
        const QRectF track(left, row.top() + 4, right - left, row.height() - 8);
        p.fillRect(track, c.Background);
        p.setPen(QPen(c.PanelLine, 1));
        p.drawRect(track);
        const double w = max > 0 ? std::max(track.height(), track.width() * b.value / max) : track.height();
        const QRectF bar(track.left(), track.top(), b.value > 0 || max <= 0 ? w : track.height() * 1.2, track.height());
        p.fillRect(bar, b.color);
        p.setFont(smallFont(9, true));
        p.setPen(QColor(255, 255, 255, 220));
        const QString t = b.text.isEmpty() ? QString::number(b.value, 'f', 0) : b.text;
        p.drawText(bar.adjusted(4, 0, -4, 0), (bar.width() > 60 ? Qt::AlignRight : Qt::AlignCenter) | Qt::AlignVCenter, t);
    }
}

// ---------------------------------------------------------------- RatingScale

RatingScale::RatingScale(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(40);
}

void RatingScale::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const Palette &c = Theme::current();
    const QRectF bar(8, 10, width() - 16, 5);
    QLinearGradient g(bar.topLeft(), bar.topRight());
    g.setColorAt(0, palette::red());
    g.setColorAt(0.5, palette::yellow());
    g.setColorAt(1, palette::green());
    p.setPen(Qt::NoPen);
    p.setBrush(g);
    p.drawRoundedRect(bar, 2.5, 2.5);
    if (ok(m_value)) {
        const double x = bar.left() + bar.width() * std::clamp((m_value - 1) / 4, 0.0, 1.0);
        p.setBrush(c.Background);
        p.setPen(QPen(c.Text, 2));
        p.drawEllipse(QPointF(x, bar.center().y()), 6, 6);
    }
    const char *names[5] {QT_TR_NOOP("Strong Sell"), QT_TR_NOOP("Sell"), QT_TR_NOOP("Hold"), QT_TR_NOOP("Buy"), QT_TR_NOOP("Strong Buy")};
    p.setFont(smallFont(9));
    p.setPen(c.TextFaint);
    for (int i = 0; i < 5; ++i) {
        const double x = bar.left() + bar.width() * i / 4;
        const Qt::Alignment a = i == 0 ? Qt::AlignLeft : i == 4 ? Qt::AlignRight : Qt::AlignHCenter;
        const QRectF r = i == 0 ? QRectF(x, 20, 100, 14) : i == 4 ? QRectF(x - 100, 20, 100, 14) : QRectF(x - 50, 20, 100, 14);
        p.drawText(r, a | Qt::AlignVCenter, tr(names[i]));
    }
}

// ---------------------------------------------------------------- ProportionBar

ProportionBar::ProportionBar(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(30);
}

void ProportionBar::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    double total = 0;
    for (const Part &part : m_parts)
        total += std::max(0.0, part.value);
    if (total <= 0) {
        p.fillRect(rect(), Theme::current().PanelLine);
        return;
    }
    double x = 0;
    for (const Part &part : m_parts) {
        const double w = width() * std::max(0.0, part.value) / total;
        p.fillRect(QRectF(x, 0, w, height()), part.color);
        x += w;
    }
}

// ---------------------------------------------------------------- StackedBarChart

StackedBarChart::StackedBarChart(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setMinimumHeight(280);
}

void StackedBarChart::setData(const QStringList &categories, const QVector<Stack> &stacks, const QVector<double> &line,
                              const QString &lineName, const QStringList &lineAxisLabels, double lineMin, double lineMax,
                              std::function<QString(double)> lineFormat)
{
    m_categories = categories;
    m_stacks = stacks;
    m_line = line;
    m_lineName = lineName;
    m_lineLabels = lineAxisLabels;
    m_lineMin = lineMin;
    m_lineMax = lineMax;
    m_lineFormat = std::move(lineFormat);
    update();
}

QRectF StackedBarChart::plotRect() const
{
    return QRectF(60, 14, width() - 60 - 90, height() - 14 - 58);
}

void StackedBarChart::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const Palette &c = Theme::current();
    const QRectF plot = plotRect();
    const int n = m_categories.size();
    if (n == 0) {
        p.setPen(c.TextFaint);
        p.setFont(smallFont(11));
        p.drawText(rect(), Qt::AlignCenter, tr("No data"));
        return;
    }
    double hi = 1;
    for (int i = 0; i < n; ++i) {
        double sum = 0;
        for (const Stack &s : m_stacks)
            sum += std::max(0.0, s.values.value(i, 0));
        hi = std::max(hi, sum);
    }
    const QVector<double> ticks = niceTicks(0, hi, 6);
    hi = ticks.last();
    drawYAxis(p, plot, ticks, 0, hi, [](double v) { return QString::number(v, 'f', 0); });
    // Axis titles.
    p.save();
    p.setPen(c.TextFaint);
    p.setFont(smallFont(9));
    p.translate(14, plot.center().y());
    p.rotate(-90);
    p.drawText(QRectF(-80, -8, 160, 16), Qt::AlignCenter, tr("Number of Analysts"));
    p.restore();
    // Right axis: the line's scale.
    p.setFont(smallFont(9));
    for (int i = 0; i < m_lineLabels.size(); ++i) {
        const double v = m_lineMin + (m_lineMax - m_lineMin) * i / std::max(1, int(m_lineLabels.size()) - 1);
        const double y = plot.bottom() - (v - m_lineMin) / (m_lineMax - m_lineMin) * plot.height();
        p.setPen(palette::yellow());
        p.drawText(QRectF(plot.right() + 8, y - 8, 80, 16), Qt::AlignLeft | Qt::AlignVCenter, m_lineLabels[i]);
    }
    const double groupW = plot.width() / n;
    const double barW = std::min(groupW * 0.72, 160.0);
    QPolygonF linePts;
    for (int i = 0; i < n; ++i) {
        const double x = plot.left() + i * groupW + (groupW - barW) / 2;
        double base = 0;
        for (const Stack &s : m_stacks) {
            const double v = std::max(0.0, s.values.value(i, 0));
            const double y0 = plot.bottom() - base / hi * plot.height();
            const double y1 = plot.bottom() - (base + v) / hi * plot.height();
            QColor col = s.color;
            if (i == m_hover)
                col = col.lighter(118);
            p.fillRect(QRectF(x, y1, barW, y0 - y1), col);
            base += v;
        }
        p.setPen(c.TextFaint);
        p.setFont(smallFont(9));
        p.drawText(QRectF(plot.left() + i * groupW, plot.bottom() + 6, groupW, 14), Qt::AlignCenter, m_categories[i]);
        const double lv = m_line.value(i, NaN);
        if (ok(lv))
            linePts << QPointF(plot.left() + i * groupW + groupW / 2,
                               plot.bottom() - (lv - m_lineMin) / (m_lineMax - m_lineMin) * plot.height());
    }
    if (linePts.size() > 1) {
        p.setPen(QPen(palette::yellow(), 2));
        p.setBrush(Qt::NoBrush);
        p.drawPolyline(linePts);
    }
    // Legend.
    p.setFont(smallFont(10));
    const QFontMetrics fm(p.font());
    QVector<std::pair<QString, QColor>> items {{m_lineName, palette::yellow()}};
    for (const Stack &s : m_stacks)
        items.push_back({s.name, s.color});
    double total = 0;
    for (const auto &[name, col] : items)
        total += 16 + fm.horizontalAdvance(name) + 14;
    double x = plot.center().x() - total / 2;
    const double y = height() - 14;
    for (int i = 0; i < items.size(); ++i) {
        if (i == 0) {
            p.setPen(QPen(items[i].second, 1.5));
            p.setBrush(Qt::NoBrush);
            p.drawRect(QRectF(x, y - 5, 10, 10));
        } else {
            p.fillRect(QRectF(x, y - 5, 10, 10), items[i].second);
        }
        p.setPen(c.TextDim);
        p.drawText(QPointF(x + 15, y + 4), items[i].first);
        x += 16 + fm.horizontalAdvance(items[i].first) + 14;
    }
    if (m_hover >= 0 && m_hover < n) {
        QStringList lines {m_categories[m_hover]};
        QVector<QColor> sw;
        if (ok(m_line.value(m_hover, NaN))) {
            lines << QString("%1: %2").arg(m_lineName, m_lineFormat(m_line[m_hover]));
            sw << palette::yellow();
        }
        for (int s = m_stacks.size() - 1; s >= 0; --s) {
            lines << QString("%1: %2").arg(m_stacks[s].name).arg(m_stacks[s].values.value(m_hover, 0), 0, 'f', 0);
            sw << m_stacks[s].color;
        }
        drawTooltip(p, mapFromGlobal(QCursor::pos()), lines, sw, rect());
    }
}

void StackedBarChart::mouseMoveEvent(QMouseEvent *e)
{
    const QRectF plot = plotRect();
    int h = -1;
    if (plot.contains(e->position()) && !m_categories.isEmpty())
        h = int((e->position().x() - plot.left()) / (plot.width() / m_categories.size()));
    m_hover = h;
    update();
}

void StackedBarChart::leaveEvent(QEvent *)
{
    m_hover = -1;
    update();
}

// ---------------------------------------------------------------- SupplyChainGraph

SupplyChainGraph::SupplyChainGraph(QWidget *parent)
    : QWidget(parent)
    , m_metricLabel(tr("REVENUE IMPACT"))
{
    setMouseTracking(true);
    setMinimumHeight(560);
}

void SupplyChainGraph::setData(const Node &center, const QVector<Node> &suppliers, const QVector<Node> &customers)
{
    m_center = center;
    m_suppliers = suppliers;
    m_customers = customers;
    updateGeometry();
    update();
}

QRectF SupplyChainGraph::nodeRect(int side, int index) const
{
    const QVector<Node> &list = side == 0 ? m_suppliers : m_customers;
    const double h = 40, gap = 7;
    const double total = list.size() * (h + gap) - gap;
    const double top = std::max(110.0, (height() + 40) / 2.0 - total / 2);
    const double w = 240;
    const double x = side == 0 ? 24 : width() - 24 - w;
    return QRectF(x, top + index * (h + gap), w, h);
}

void SupplyChainGraph::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const Palette &c = Theme::current();
    // Title.
    p.setPen(c.Accent);
    p.setFont(smallFont(20, true));
    p.drawText(QRectF(0, 18, width(), 28), Qt::AlignCenter, tr("SUPPLY CHAIN"));
    p.setPen(c.TextDim);
    p.setFont(smallFont(11));
    p.drawText(QRectF(0, 48, width(), 18), Qt::AlignCenter, tr("%1 Related Companies").arg(m_center.symbol));

    const QRectF box(width() / 2.0 - 75, (height() + 40) / 2.0 - 50, 150, 100);
    auto drawLinks = [&](int side) {
        const QVector<Node> &list = side == 0 ? m_suppliers : m_customers;
        const QColor col = side == 0 ? palette::red() : palette::green();
        for (int i = 0; i < list.size(); ++i) {
            const QRectF r = nodeRect(side, i);
            const QPointF from = side == 0 ? QPointF(r.right(), r.center().y()) : QPointF(r.left(), r.center().y());
            const QPointF to = side == 0 ? QPointF(box.left(), box.center().y()) : QPointF(box.right(), box.center().y());
            QColor lc = col;
            const bool hot = m_hoverSide == side && m_hoverIndex == i;
            lc.setAlpha(hot ? 230 : 110);
            p.setPen(QPen(lc, hot ? 2.2 : 1.1));
            p.drawLine(from, to);
        }
    };
    drawLinks(0);
    drawLinks(1);

    // Center node.
    for (int glow = 3; glow >= 1; --glow) {
        QColor g = c.Accent;
        g.setAlpha(25 * glow);
        p.setPen(QPen(g, glow * 4));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(box, 8, 8);
    }
    p.setPen(QPen(c.Accent, 1.5));
    p.setBrush(c.Panel);
    p.drawRoundedRect(box, 8, 8);
    p.setPen(c.Accent);
    p.setFont(smallFont(18, true));
    p.drawText(QRectF(box.left(), box.top() + 12, box.width(), 26), Qt::AlignCenter, m_center.symbol);
    p.setPen(c.TextDim);
    p.setFont(smallFont(10));
    p.drawText(QRectF(box.left() + 6, box.top() + 40, box.width() - 12, 16), Qt::AlignCenter,
               QFontMetrics(p.font()).elidedText(m_center.name, Qt::ElideRight, int(box.width() - 12)));
    p.setFont(smallFont(12, true));
    p.setPen(c.Text);
    const QString price = ok(m_center.price) ? money(m_center.price, m_center.currency) : QString();
    const QString ch = ok(m_center.changePct) ? percent(m_center.changePct) : QString();
    const QFontMetrics fm(p.font());
    const double pw = fm.horizontalAdvance(price) + 8 + QFontMetrics(smallFont(10)).horizontalAdvance(ch);
    double x = box.center().x() - pw / 2;
    p.drawText(QPointF(x, box.top() + 80), price);
    p.setFont(smallFont(10));
    p.setPen(m_center.changePct >= 0 ? c.Up : c.Down);
    p.drawText(QPointF(x + fm.horizontalAdvance(price) + 8, box.top() + 80), ch);

    if (!m_note.isEmpty()) {
        p.setPen(c.TextDim);
        p.setFont(smallFont(11));
        p.drawText(QRectF(width() / 2.0 - 260, box.bottom() + 24, 520, 60), Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, m_note);
    }

    // Side headers and nodes.
    for (int side = 0; side < 2; ++side) {
        const QVector<Node> &list = side == 0 ? m_suppliers : m_customers;
        const QColor col = side == 0 ? palette::red() : palette::green();
        const QRectF first = nodeRect(side, 0);
        p.setFont(smallFont(16, true));
        p.setPen(col);
        const QString count = QString::number(list.size());
        p.drawText(QPointF(first.left(), first.top() - 12), count);
        p.setFont(smallFont(10, true));
        p.setPen(c.TextDim);
        p.drawText(QPointF(first.left() + QFontMetrics(smallFont(16, true)).horizontalAdvance(count) + 6, first.top() - 13),
                   side == 0 ? tr("SUPPLIERS") : tr("CUSTOMERS"));
        for (int i = 0; i < list.size(); ++i) {
            const Node &n = list[i];
            const QRectF r = nodeRect(side, i);
            const bool hot = m_hoverSide == side && m_hoverIndex == i;
            p.setPen(QPen(hot ? col : c.PanelLine, 1));
            p.setBrush(hot ? c.Hover : c.Panel);
            p.drawRoundedRect(r, 4, 4);
            p.setPen(Qt::NoPen);
            p.setBrush(col);
            p.drawEllipse(QPointF(side == 0 ? r.left() + 11 : r.right() - 11, r.center().y()), 3.5, 3.5);
            const double tx = side == 0 ? r.left() + 22 : r.left() + 10;
            p.setFont(smallFont(11, true));
            p.setPen(n.openable ? c.Text : c.TextDim);
            const QString title = QFontMetrics(smallFont(11, true)).elidedText(n.label.isEmpty() ? n.symbol : n.label,
                                                                             Qt::ElideRight, int(r.width() - 30) / (ok(n.price) ? 2 : 1));
            p.drawText(QPointF(tx, r.top() + 17), title);
            const double sx = tx + QFontMetrics(smallFont(11, true)).horizontalAdvance(title) + 6;
            p.setFont(smallFont(10));
            p.setPen(c.TextDim);
            const QString pr = ok(n.price) ? money(n.price, n.currency) : QString();
            p.drawText(QPointF(sx, r.top() + 17), pr);
            if (ok(n.changePct)) {
                p.setPen(n.changePct >= 0 ? c.Up : c.Down);
                p.drawText(QPointF(sx + QFontMetrics(smallFont(10)).horizontalAdvance(pr) + 6, r.top() + 17),
                           "(" + percent(n.changePct) + ")");
            }
            p.setFont(smallFont(9));
            p.setPen(c.TextFaint);
            p.drawText(QPointF(tx, r.top() + 32), m_metricLabel);
            p.setPen(palette::yellow());
            p.drawText(QPointF(tx + QFontMetrics(smallFont(9)).horizontalAdvance(m_metricLabel) + 6, r.top() + 32),
                       n.shareText.isEmpty() ? percent(n.share, 1, false) : n.shareText);
        }
    }
    // Hover card.
    if (m_hoverSide >= 0) {
        const Node &n = (m_hoverSide == 0 ? m_suppliers : m_customers)[m_hoverIndex];
        const QRectF r = nodeRect(m_hoverSide, m_hoverIndex);
        QStringList lines {(n.label.isEmpty() ? n.symbol : n.label) + "  ·  " + (m_hoverSide == 0 ? tr("supplier") : tr("customer")), n.name};
        if (!n.sector.isEmpty())
            lines << n.sector;
        if (!n.product.isEmpty())
            for (const QString &line : n.product.split('\n'))
                lines << line;
        lines << m_metricLabel.left(1) + m_metricLabel.mid(1).toLower() + ": "
                     + (n.shareText.isEmpty() ? percent(n.share, 1, false) : n.shareText);
        if (!n.url.isEmpty())
            lines << n.url;
        if (!n.openable)
            lines << tr("Not listed on an exchange Yahoo Finance carries");
        drawTooltip(p, m_hoverSide == 0 ? QPointF(r.right(), r.top()) : QPointF(r.left(), r.top()), lines, {}, rect());
    }
}

void SupplyChainGraph::mouseMoveEvent(QMouseEvent *e)
{
    int side = -1, index = -1;
    for (int s = 0; s < 2 && side < 0; ++s) {
        const QVector<Node> &list = s == 0 ? m_suppliers : m_customers;
        for (int i = 0; i < list.size(); ++i)
            if (nodeRect(s, i).contains(e->position())) {
                side = s;
                index = i;
                break;
            }
    }
    const bool openable = side >= 0 && (side == 0 ? m_suppliers : m_customers)[index].openable;
    setCursor(openable ? Qt::PointingHandCursor : Qt::ArrowCursor);
    if (side != m_hoverSide || index != m_hoverIndex) {
        m_hoverSide = side;
        m_hoverIndex = index;
        update();
    }
}

void SupplyChainGraph::mousePressEvent(QMouseEvent *)
{
    if (m_hoverSide >= 0 && (m_hoverSide == 0 ? m_suppliers : m_customers)[m_hoverIndex].openable)
        emit symbolActivated((m_hoverSide == 0 ? m_suppliers : m_customers)[m_hoverIndex].symbol);
}

void SupplyChainGraph::leaveEvent(QEvent *)
{
    m_hoverSide = m_hoverIndex = -1;
    update();
}

} // namespace research
