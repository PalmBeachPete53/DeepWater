#include "QuantCharts.h"

#include "QuantData.h"
#include "ResearchCharts.h"
#include "Theme.h"

#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

namespace quant {

QFont chartFont(int px, bool bold)
{
    QFont f;
    f.setFamilies(Theme::monoFamilies());
    f.setStyleHint(QFont::Monospace);
    f.setPixelSize(px);
    f.setBold(bold);
    return f;
}

QString thousands(double v, int decimals)
{
    if (!ok(v))
        return "—";
    return QLocale(QLocale::English).toString(v, 'f', decimals);
}

QString signedThousands(double v, int decimals)
{
    if (!ok(v))
        return "—";
    return (v > 0 ? "+" : "") + thousands(v, decimals);
}

QString shortNumber(double v)
{
    if (!ok(v))
        return "—";
    const double a = std::fabs(v);
    const QString sign = v < 0 ? "-" : "";
    if (a >= 1e9) return sign + QString::number(a / 1e9, 'f', a >= 1e10 ? 0 : 1) + "B";
    if (a >= 1e6) return sign + QString::number(a / 1e6, 'f', a >= 1e7 ? 0 : 1) + "M";
    if (a >= 1e3) return sign + QString::number(a / 1e3, 'f', 0) + "k";
    return sign + QString::number(a, 'f', 0);
}

// ---------------------------------------------------------------- Chart

Chart::Chart(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setMinimumHeight(180);
}

void Chart::setTitle(const QVector<TitlePart> &parts)
{
    m_title = parts;
    if (!parts.isEmpty() && m_margins.top() < 30)
        m_margins.setTop(30);
    update();
}

Chart::Axis Chart::axisFor(double lo, double hi, int count, double pad)
{
    if (!ok(lo) || !ok(hi)) {
        lo = 0;
        hi = 1;
    }
    if (hi - lo < 1e-9) {
        const double d = std::fabs(hi) > 1e-9 ? std::fabs(hi) * 0.05 : 1;
        lo -= d;
        hi += d;
    }
    const double p = (hi - lo) * pad;
    return axisAround(lo - p, hi + p, count);
}

Chart::Axis Chart::axisAround(double lo, double hi, int count)
{
    Axis a;
    a.ticks = research::niceTicks(lo, hi, count);
    if (a.ticks.size() < 2) {
        a.ticks = {lo, hi};
    }
    a.lo = a.ticks.first();
    a.hi = a.ticks.last();
    return a;
}

QRectF Chart::plotRect() const
{
    return QRectF(m_margins.left(), m_margins.top(), width() - m_margins.left() - m_margins.right(),
                  height() - m_margins.top() - m_margins.bottom());
}

void Chart::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    drawTitle(p);
    paint(p, plotRect());
}

void Chart::mouseMoveEvent(QMouseEvent *e)
{
    m_mouse = e->position();
    m_hovering = true;
    update();
}

void Chart::leaveEvent(QEvent *)
{
    m_hovering = false;
    update();
}

void Chart::drawTitle(QPainter &p)
{
    if (m_title.isEmpty())
        return;
    const Palette &c = Theme::current();
    p.setFont(chartFont(11, true));
    const QFontMetrics fm(p.font());
    double x = 10;
    for (const TitlePart &t : m_title) {
        p.setPen(t.color.isValid() ? t.color : c.Text);
        p.drawText(QPointF(x, 17), t.text);
        x += fm.horizontalAdvance(t.text);
    }
}

void Chart::drawEmpty(QPainter &p, const QString &text)
{
    p.setPen(Theme::current().TextFaint);
    p.setFont(chartFont(11));
    p.drawText(rect(), Qt::AlignCenter, text);
}

void Chart::drawValueAxis(QPainter &p, const QRectF &plot, const Axis &axis, const Format &format, bool right, bool grid,
                          const QColor &color)
{
    const Palette &c = Theme::current();
    p.setFont(chartFont(9));
    for (double t : axis.ticks) {
        const double y = plot.bottom() - (t - axis.lo) / (axis.hi - axis.lo) * plot.height();
        if (grid) {
            p.setPen(QPen(c.GridLine, 1));
            p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        }
        p.setPen(color.isValid() ? color : c.TextFaint);
        const QString s = format ? format(t) : QString::number(t);
        if (right)
            p.drawText(QRectF(plot.right() + 6, y - 8, m_margins.right() - 8, 16), Qt::AlignLeft | Qt::AlignVCenter, s);
        else
            p.drawText(QRectF(0, y - 8, plot.left() - 6, 16), Qt::AlignRight | Qt::AlignVCenter, s);
    }
}

void Chart::drawDateAxis(QPainter &p, const QRectF &plot, const QVector<QDate> &dates, bool grid)
{
    const int n = dates.size();
    if (n == 0)
        return;
    const Palette &c = Theme::current();
    QString format = m_dateFormat;
    if (format.isEmpty())
        format = dates.first().daysTo(dates.last()) > 400 ? "MMM yyyy" : QLocale().language() == QLocale::French ? "d MMM" : "MMM d";
    const int count = std::max(2, int(plot.width() / 130));
    p.setFont(chartFont(9));
    for (int k = 0; k < count; ++k) {
        const int i = n == 1 ? 0 : int(std::lround(double(k) * (n - 1) / (count - 1)));
        const double x = xOf(plot, i, n);
        if (grid) {
            p.setPen(QPen(c.GridLine, 1));
            p.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
        }
        p.setPen(c.TextFaint);
        // The first label starts at its tick and the last one ends at its tick, so neither is cut off.
        const QRectF r = k == 0 ? QRectF(x, plot.bottom() + 5, 120, 16)
                       : k == count - 1 ? QRectF(x - 120, plot.bottom() + 5, 120, 16) : QRectF(x - 60, plot.bottom() + 5, 120, 16);
        const Qt::Alignment align = k == 0 ? Qt::AlignLeft : k == count - 1 ? Qt::AlignRight : Qt::AlignHCenter;
        p.drawText(r, align | Qt::AlignVCenter, QLocale().toString(dates[i], format));
    }
}

void Chart::drawLabelAxis(QPainter &p, const QRectF &plot, const QStringList &labels, bool grid)
{
    const int n = labels.size();
    if (n == 0)
        return;
    const Palette &c = Theme::current();
    p.setFont(chartFont(9));
    const QFontMetrics fm(p.font());
    int maxW = 0;
    for (const QString &l : labels)
        maxW = std::max(maxW, fm.horizontalAdvance(l));
    const int every = std::max(1, int(std::ceil((maxW + 16) * n / plot.width())));
    for (int i = 0; i < n; i += every) {
        const double x = xOf(plot, i, n);
        if (grid) {
            p.setPen(QPen(c.GridLine, 1));
            p.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
        }
        p.setPen(c.TextFaint);
        p.drawText(QRectF(x - 60, plot.bottom() + 5, 120, 16), Qt::AlignHCenter | Qt::AlignVCenter, labels[i]);
    }
}

void Chart::drawTooltip(QPainter &p, const QPointF &at, const QStringList &lines, const QVector<QColor> &swatches)
{
    if (lines.isEmpty())
        return;
    const Palette &c = Theme::current();
    const QFont bold = chartFont(11, true), normal = chartFont(11);
    const QFontMetrics fb(bold), fn(normal);
    int w = 0;
    for (int i = 0; i < lines.size(); ++i)
        w = std::max(w, (i == 0 ? fb : fn).horizontalAdvance(lines[i]) + (i > 0 && i - 1 < swatches.size() ? 14 : 0));
    const int lineH = 17;
    QRectF box(at.x() + 14, at.y() - 10, w + 20, lines.size() * lineH + 12);
    const QRectF bounds = rect();
    if (box.right() > bounds.right())
        box.moveRight(at.x() - 14);
    if (box.bottom() > bounds.bottom())
        box.moveBottom(bounds.bottom());
    if (box.top() < bounds.top())
        box.moveTop(bounds.top());
    p.save();
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

double Chart::xOf(const QRectF &plot, int i, int n) const
{
    return n <= 1 ? plot.center().x() : plot.left() + plot.width() * i / (n - 1);
}

int Chart::indexAt(const QRectF &plot, double x, int n) const
{
    if (n <= 0 || x < plot.left() - 10 || x > plot.right() + 10)
        return -1;
    return n == 1 ? 0 : std::clamp(int(std::lround((x - plot.left()) / plot.width() * (n - 1))), 0, n - 1);
}

namespace {

QString dateText(const QDate &d)
{
    return QLocale().toString(d, "MMM d, yyyy");
}

QColor withAlpha(QColor c, int a)
{
    c.setAlpha(a);
    return c;
}

// A path through the finite values of a series.
QPainterPath linePath(const QVector<double> &values, const std::function<double(int)> &xOf,
                      const std::function<double(double)> &yOf, bool steps = false)
{
    QPainterPath path;
    bool started = false;
    double lastY = 0;
    for (int i = 0; i < values.size(); ++i) {
        if (!ok(values[i]))
            continue;
        const QPointF pt(xOf(i), yOf(values[i]));
        if (!started) {
            path.moveTo(pt);
            started = true;
        } else if (steps) {
            path.lineTo(pt.x(), lastY);
            path.lineTo(pt);
        } else {
            path.lineTo(pt);
        }
        lastY = pt.y();
    }
    return path;
}

} // namespace

// ---------------------------------------------------------------- MultiLineChart

MultiLineChart::MultiLineChart(QWidget *parent)
    : Chart(parent)
{
    m_margins = QMargins(16, 16, 64, 28);
}

void MultiLineChart::setData(const QVector<QDate> &dates, const QVector<Line> &lines, Format format)
{
    m_dates = dates;
    m_lines = lines;
    m_format = std::move(format);
    changed();
}

void MultiLineChart::paintLayer(QPainter &p, const QRectF &plot, const Axis &axis, int n, bool byLabel)
{
    const Palette &c = Theme::current();
    drawValueAxis(p, plot, axis, m_format, m_rightAxis, true);
    if (byLabel)
        drawLabelAxis(p, plot, m_xLabels, false);
    else
        drawDateAxis(p, plot, m_dates, false);
    auto X = [&](int i) { return xOf(plot, i, n); };
    auto Y = [&](double v) { return plot.bottom() - (v - axis.lo) / (axis.hi - axis.lo) * plot.height(); };
    if (m_zeroLine) {
        p.setPen(QPen(c.TextFaint, 1, Qt::DotLine));
        p.drawLine(QPointF(plot.left(), Y(0)), QPointF(plot.right(), Y(0)));
    }
    p.save();
    p.setClipRect(plot);
    if (m_fillBetween && m_lines.size() >= 2) {
        // Green where the first series is above the second, red where it is below.
        const QVector<double> &a = m_lines[0].values, &b = m_lines[1].values;
        QPainterPath above, below;
        for (int i = 0; i + 1 < n; ++i) {
            if (!ok(a.value(i)) || !ok(b.value(i)) || !ok(a.value(i + 1)) || !ok(b.value(i + 1)))
                continue;
            QPolygonF quad {QPointF(X(i), Y(a[i])), QPointF(X(i + 1), Y(a[i + 1])), QPointF(X(i + 1), Y(b[i + 1])), QPointF(X(i), Y(b[i]))};
            (a[i] + a[i + 1] >= b[i] + b[i + 1] ? above : below).addPolygon(quad);
        }
        p.fillPath(above, withAlpha(c.Up, 60));
        p.fillPath(below, withAlpha(c.Down, 60));
    }
    if (m_signedArea && !m_lines.isEmpty()) {
        const QVector<double> &a = m_lines[0].values;
        QPainterPath pos, neg;
        for (int i = 0; i + 1 < n; ++i) {
            if (!ok(a.value(i)) || !ok(a.value(i + 1)))
                continue;
            QPolygonF quad {QPointF(X(i), Y(a[i])), QPointF(X(i + 1), Y(a[i + 1])), QPointF(X(i + 1), Y(0)), QPointF(X(i), Y(0))};
            (a[i] + a[i + 1] >= 0 ? pos : neg).addPolygon(quad);
        }
        p.fillPath(pos, withAlpha(c.Up, 70));
        p.fillPath(neg, withAlpha(c.Down, 70));
    }
    for (const Line &l : m_lines) {
        p.setPen(QPen(l.color, 1.6));
        p.setBrush(Qt::NoBrush);
        p.drawPath(linePath(l.values, X, Y));
    }
    p.restore();
    if (m_endLabels) {
        // The last value of each series at its end, nudged apart when they overlap.
        struct Label { double y; QString text; QColor color; };
        QVector<Label> labels;
        for (const Line &l : m_lines) {
            int last = l.values.size() - 1;
            while (last >= 0 && !ok(l.values[last]))
                --last;
            if (last >= 0)
                labels.push_back({Y(l.values[last]), l.name + " " + (m_format ? m_format(l.values[last]) : QString::number(l.values[last])), l.color});
        }
        std::sort(labels.begin(), labels.end(), [](const Label &a, const Label &b) { return a.y < b.y; });
        for (int i = 1; i < labels.size(); ++i)
            if (labels[i].y - labels[i - 1].y < 13)
                labels[i].y = labels[i - 1].y + 13;
        // Inside the plot, against its right edge, on a tint of the background so the lines don't cut through.
        p.setFont(chartFont(9, true));
        const QFontMetrics fm(p.font());
        for (const Label &l : labels) {
            const QRectF r(plot.right() - fm.horizontalAdvance(l.text) - 10, l.y - 8, fm.horizontalAdvance(l.text) + 8, 16);
            p.fillRect(r, withAlpha(c.Background, 200));
            p.setPen(l.color);
            p.drawText(r, Qt::AlignCenter, l.text);
        }
    }
    if (m_legend) {
        p.setFont(chartFont(9));
        const QFontMetrics fm(p.font());
        double w = 0;
        for (const Line &l : m_lines)
            w += 18 + fm.horizontalAdvance(l.name) + 12;
        double lx = plot.right() - w;
        const double ly = m_title.isEmpty() ? plot.top() - 8 : plot.top() + 10;   // under the title when there is one
        for (const Line &l : m_lines) {
            p.setPen(QPen(l.color, 1.5));
            p.drawLine(QPointF(lx, ly), QPointF(lx + 12, ly));
            p.setPen(c.TextDim);
            p.drawText(QPointF(lx + 16, ly + 3), l.name);
            lx += 18 + fm.horizontalAdvance(l.name) + 12;
        }
    }
}

void MultiLineChart::paint(QPainter &p, const QRectF &plot)
{
    const Palette &c = Theme::current();
    double lo = INFINITY, hi = -INFINITY;
    for (const Line &l : m_lines)
        for (double v : l.values)
            if (ok(v)) {
                lo = std::min(lo, v);
                hi = std::max(hi, v);
            }
    const bool byLabel = !m_xLabels.isEmpty();
    if ((m_dates.isEmpty() && !byLabel) || !std::isfinite(lo)) {
        drawEmpty(p, tr("No data"));
        return;
    }
    if (m_zeroLine || m_signedArea) {
        lo = std::min(lo, 0.0);
        hi = std::max(hi, 0.0);
    }
    const Axis axis = axisFor(lo, hi, 6);
    const int n = byLabel ? m_xLabels.size() : m_dates.size();
    auto X = [&](int i) { return xOf(plot, i, n); };
    // The static layer, redrawn when the data, the size or the theme change (setters call changed()).
    const qreal dpr = devicePixelRatioF();
    if (m_layer.isNull() || m_layer.size() != size() * dpr || m_layer.devicePixelRatio() != dpr || m_layerPlot != plot
        || m_layerTheme != Theme::current().id || m_layerDateFormat != m_dateFormat) {
        m_layer = QPixmap(size() * dpr);
        m_layer.setDevicePixelRatio(dpr);
        m_layer.fill(Qt::transparent);
        QPainter lp(&m_layer);
        lp.setRenderHints(p.renderHints());
        lp.setFont(p.font());
        lp.setPen(p.pen());
        lp.setBrush(p.brush());
        paintLayer(lp, plot, axis, n, byLabel);
        m_layerPlot = plot;
        m_layerTheme = Theme::current().id;
        m_layerDateFormat = m_dateFormat;
    }
    p.drawPixmap(0, 0, m_layer);
    const int h = m_hovering ? indexAt(plot, m_mouse.x(), n) : -1;
    if (h >= 0) {
        const double x = X(h);
        p.setPen(QPen(c.ControlBorderHover, 1, Qt::DashLine));
        p.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
        QStringList lines {byLabel ? m_xLabels[h] : (m_dateFormat == QLatin1String("MMM") ? QLocale().toString(m_dates[h], "d MMM") : dateText(m_dates[h]))};
        QVector<QColor> sw;
        for (const Line &l : m_lines) {
            const double v = l.values.value(h, NaN);
            if (!ok(v))
                continue;
            lines << QString("%1: %2").arg(l.name, m_format ? m_format(v) : QString::number(v));
            sw << l.color;
        }
        drawTooltip(p, QPointF(x, m_mouse.y()), lines, sw);
    }
}

// ---------------------------------------------------------------- HistogramChart

HistogramChart::HistogramChart(QWidget *parent)
    : Chart(parent)
{
    m_margins = QMargins(50, 30, 16, 28);
}

void HistogramChart::setData(const QVector<Sample> &samples, int bins, Format format)
{
    m_samples = samples;
    m_bins = std::max(4, bins);
    m_format = std::move(format);
    update();
}

void HistogramChart::paint(QPainter &p, const QRectF &plot)
{
    const Palette &c = Theme::current();
    double lo = INFINITY, hi = -INFINITY;
    for (const Sample &s : m_samples)
        for (double v : s.values)
            if (ok(v)) {
                lo = std::min(lo, v);
                hi = std::max(hi, v);
            }
    if (!std::isfinite(lo)) {
        drawEmpty(p, tr("No data"));
        return;
    }
    const Axis ax = axisFor(lo, hi, 6, 0.02);
    const double width = (ax.hi - ax.lo) / m_bins;
    QVector<QVector<int>> counts(m_samples.size(), QVector<int>(m_bins, 0));
    int maxCount = 1;
    for (int i = 0; i < m_samples.size(); ++i)
        for (double v : m_samples[i].values) {
            if (!ok(v))
                continue;
            const int b = std::clamp(int((v - ax.lo) / width), 0, m_bins - 1);
            maxCount = std::max(maxCount, ++counts[i][b]);
        }
    Axis ay;
    ay.lo = 0;
    ay.hi = maxCount * 1.05;
    ay.ticks = research::niceTicks(0, ay.hi, 4);
    ay.hi = std::max(ay.hi, ay.ticks.last());
    drawValueAxis(p, plot, ay, [](double v) { return QString::number(v, 'f', 0); }, false, true);
    p.setFont(chartFont(9));
    for (double t : ax.ticks) {
        const double x = plot.left() + (t - ax.lo) / (ax.hi - ax.lo) * plot.width();
        p.setPen(c.TextFaint);
        p.drawText(QRectF(x - 40, plot.bottom() + 5, 80, 16), Qt::AlignCenter, m_format ? m_format(t) : QString::number(t));
    }
    auto Y = [&](double v) { return plot.bottom() - v / ay.hi * plot.height(); };
    p.save();
    p.setClipRect(plot);
    for (int i = 0; i < m_samples.size(); ++i) {
        p.setPen(QPen(m_samples[i].color, 1));
        p.setBrush(withAlpha(m_samples[i].color, 90));
        for (int b = 0; b < m_bins; ++b) {
            if (counts[i][b] == 0)
                continue;
            const double x0 = plot.left() + b * plot.width() / m_bins, x1 = plot.left() + (b + 1) * plot.width() / m_bins;
            p.drawRect(QRectF(x0, Y(counts[i][b]), x1 - x0, plot.bottom() - Y(counts[i][b])));
        }
    }
    p.restore();
    p.setFont(chartFont(9));
    const QFontMetrics fm(p.font());
    double w = 0;
    for (const Sample &s : m_samples)
        w += 16 + fm.horizontalAdvance(s.name) + 12;
    double x = plot.right() - w;
    const double y = plot.top() - 8;
    for (const Sample &s : m_samples) {
        p.fillRect(QRectF(x, y - 4, 10, 8), s.color);
        p.setPen(c.TextDim);
        p.drawText(QPointF(x + 14, y + 3), s.name);
        x += 16 + fm.horizontalAdvance(s.name) + 12;
    }
    if (m_hovering && plot.contains(m_mouse)) {
        const int b = std::clamp(int((m_mouse.x() - plot.left()) / plot.width() * m_bins), 0, m_bins - 1);
        QStringList lines {QString("%1 – %2").arg(m_format ? m_format(ax.lo + b * width) : QString::number(ax.lo + b * width),
                                                   m_format ? m_format(ax.lo + (b + 1) * width) : QString::number(ax.lo + (b + 1) * width))};
        QVector<QColor> sw;
        for (int i = 0; i < m_samples.size(); ++i) {
            lines << QString("%1: %2").arg(m_samples[i].name).arg(counts[i][b]);
            sw << m_samples[i].color;
        }
        drawTooltip(p, m_mouse, lines, sw);
    }
}

// ---------------------------------------------------------------- OverlayChart

OverlayChart::OverlayChart(QWidget *parent)
    : Chart(parent)
{
    m_margins = QMargins(48, 30, 48, 28);
}

void OverlayChart::setData(const QVector<QDate> &dates, const QString &nameA, const QColor &colorA, const QVector<double> &a,
                           const QString &nameB, const QColor &colorB, const QVector<double> &b)
{
    m_dates = dates;
    m_nameA = nameA;
    m_colorA = colorA;
    m_a = a;
    m_nameB = nameB;
    m_colorB = colorB;
    m_b = b;
    update();
}

void OverlayChart::paint(QPainter &p, const QRectF &plot)
{
    const Palette &c = Theme::current();
    const int n = m_dates.size();
    if (n == 0 || m_a.size() != n || m_b.size() != n) {
        drawEmpty(p, tr("No data"));
        return;
    }
    auto range = [](const QVector<double> &v, double *lo, double *hi) {
        *lo = INFINITY;
        *hi = -INFINITY;
        for (double x : v)
            if (ok(x)) {
                *lo = std::min(*lo, x);
                *hi = std::max(*hi, x);
            }
    };
    double loA, hiA, loB, hiB;
    range(m_a, &loA, &hiA);
    range(m_b, &loB, &hiB);
    const Axis axA = axisFor(loA, hiA, 5), axB = axisFor(loB, hiB, 5);
    const Format fmt = [](double v) { return QString::number(v, 'f', v >= 1000 ? 0 : 1); };
    drawValueAxis(p, plot, axA, fmt, false, true, m_colorA);
    drawValueAxis(p, plot, axB, fmt, true, false, m_colorB);
    drawDateAxis(p, plot, m_dates, false);
    auto X = [&](int i) { return xOf(plot, i, n); };
    auto YA = [&](double v) { return plot.bottom() - (v - axA.lo) / (axA.hi - axA.lo) * plot.height(); };
    auto YB = [&](double v) { return plot.bottom() - (v - axB.lo) / (axB.hi - axB.lo) * plot.height(); };
    p.save();
    p.setClipRect(plot);
    auto draw = [&](const QVector<double> &v, const QColor &color, const std::function<double(double)> &Y) {
        QPainterPath line = linePath(v, X, Y);
        if (line.isEmpty())
            return;
        QPainterPath area = line;
        area.lineTo(line.currentPosition().x(), plot.bottom());
        area.lineTo(line.elementAt(0).x, plot.bottom());
        area.closeSubpath();
        QLinearGradient g(0, plot.top(), 0, plot.bottom());
        g.setColorAt(0, withAlpha(color, 70));
        g.setColorAt(1, withAlpha(color, 8));
        p.fillPath(area, g);
        p.setPen(QPen(color, 1.5));
        p.setBrush(Qt::NoBrush);
        p.drawPath(line);
    };
    draw(m_b, m_colorB, YB);
    draw(m_a, m_colorA, YA);
    if (m_minMax) {
        auto mark = [&](const QVector<double> &v, const QColor &color, const std::function<double(double)> &Y) {
            int iMin = -1, iMax = -1;
            for (int i = 0; i < n; ++i) {
                if (!ok(v[i]))
                    continue;
                if (iMin < 0 || v[i] < v[iMin]) iMin = i;
                if (iMax < 0 || v[i] > v[iMax]) iMax = i;
            }
            if (iMin < 0)
                return;
            p.setPen(Qt::NoPen);
            p.setBrush(color);
            const QPointF hiPt(X(iMax), Y(v[iMax])), loPt(X(iMin), Y(v[iMin]));
            p.drawPolygon(QPolygonF({hiPt + QPointF(0, -3), hiPt + QPointF(-5, -11), hiPt + QPointF(5, -11)}));
            p.drawPolygon(QPolygonF({loPt + QPointF(0, 3), loPt + QPointF(-5, 11), loPt + QPointF(5, 11)}));
        };
        mark(m_a, m_colorA, YA);
        mark(m_b, m_colorB, YB);
    }
    p.restore();
    const int h = m_hovering ? indexAt(plot, m_mouse.x(), n) : -1;
    if (h >= 0) {
        const double x = X(h);
        p.setPen(QPen(c.ControlBorderHover, 1, Qt::DashLine));
        p.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
        drawTooltip(p, QPointF(x, m_mouse.y()),
                    {dateText(m_dates[h]), QString("%1: %2").arg(m_nameA, QString::number(m_a[h], 'f', 2)),
                     QString("%1: %2").arg(m_nameB, QString::number(m_b[h], 'f', 2))},
                    {m_colorA, m_colorB});
    }
}

// ---------------------------------------------------------------- SpreadChart

SpreadChart::SpreadChart(QWidget *parent)
    : Chart(parent)
{
    m_margins = QMargins(56, 30, 16, 28);
}

void SpreadChart::setData(const QVector<QDate> &dates, const QVector<double> &spread, const QVector<double> &mean,
                          const QVector<double> &upper, const QVector<double> &lower, int window)
{
    m_dates = dates;
    m_spread = spread;
    m_mean = mean;
    m_upper = upper;
    m_lower = lower;
    m_window = window;
    update();
}

void SpreadChart::paint(QPainter &p, const QRectF &plot)
{
    const Palette &c = Theme::current();
    const int n = m_dates.size();
    if (n == 0 || m_spread.size() != n) {
        drawEmpty(p, tr("No data"));
        return;
    }
    double lo = INFINITY, hi = -INFINITY;
    for (const QVector<double> *v : {&m_spread, &m_mean, &m_upper, &m_lower})
        for (double x : *v)
            if (ok(x)) {
                lo = std::min(lo, x);
                hi = std::max(hi, x);
            }
    const Axis axis = axisFor(lo, hi, 5);
    const Format fmt = m_format ? m_format : Format([](double v) { return QString::number(v, 'f', std::fabs(v) >= 100 ? 0 : 1); });
    drawValueAxis(p, plot, axis, fmt, false, true);
    drawDateAxis(p, plot, m_dates, false);
    auto X = [&](int i) { return xOf(plot, i, n); };
    auto Y = [&](double v) { return plot.bottom() - (v - axis.lo) / (axis.hi - axis.lo) * plot.height(); };
    const QColor spread = research::palette::green(), mean = research::palette::yellow(), band = research::palette::purple();
    p.save();
    p.setClipRect(plot);
    // The band between the two sigma lines, filled, per run of valid values.
    QPainterPath fill;
    for (int i = 0; i + 1 < n; ++i) {
        if (!ok(m_upper.value(i)) || !ok(m_lower.value(i)) || !ok(m_upper.value(i + 1)) || !ok(m_lower.value(i + 1)))
            continue;
        fill.addPolygon(QPolygonF({QPointF(X(i), Y(m_upper[i])), QPointF(X(i + 1), Y(m_upper[i + 1])),
                                   QPointF(X(i + 1), Y(m_lower[i + 1])), QPointF(X(i), Y(m_lower[i]))}));
    }
    p.fillPath(fill, withAlpha(band, 45));
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(band, 1, Qt::DashLine));
    p.drawPath(linePath(m_upper, X, Y));
    p.drawPath(linePath(m_lower, X, Y));
    p.setPen(QPen(mean, 1.4));
    p.drawPath(linePath(m_mean, X, Y));
    p.setPen(QPen(spread, 1.6));
    p.drawPath(linePath(m_spread, X, Y));
    p.restore();
    // Legend, top right.
    p.setFont(chartFont(9));
    const QFontMetrics fm(p.font());
    struct Item { QString text; QColor color; bool dashed; };
    const QVector<Item> items {{m_legend.value(0, tr("Spread")), spread, false},
                               {m_legend.value(1, QString("SMA(%1)").arg(m_window)), mean, false},
                               {m_legend.value(2, "±2σ"), band, true}};
    double w = 0;
    for (const Item &it : items)
        w += 18 + fm.horizontalAdvance(it.text) + 12;
    double x = plot.right() - w;
    const double y = plot.top() - 8;
    for (const Item &it : items) {
        p.setPen(QPen(it.color, 1.5, it.dashed ? Qt::DashLine : Qt::SolidLine));
        p.drawLine(QPointF(x, y), QPointF(x + 12, y));
        p.setPen(c.TextDim);
        p.drawText(QPointF(x + 16, y + 3), it.text);
        x += 18 + fm.horizontalAdvance(it.text) + 12;
    }
    const int h = m_hovering ? indexAt(plot, m_mouse.x(), n) : -1;
    if (h >= 0) {
        const double hx = X(h);
        p.setPen(QPen(c.ControlBorderHover, 1, Qt::DashLine));
        p.drawLine(QPointF(hx, plot.top()), QPointF(hx, plot.bottom()));
        QStringList lines {dateText(m_dates[h])};
        QVector<QColor> sw;
        auto add = [&](const QString &name, double v, const QColor &col) {
            if (!ok(v))
                return;
            lines << QString("%1: %2").arg(name, m_format ? m_format(v) : QString::number(v, 'f', 2));
            sw << col;
        };
        add(m_legend.value(0, tr("Spread")), m_spread[h], spread);
        add(m_legend.value(1, QString("SMA(%1)").arg(m_window)), m_mean.value(h, NaN), mean);
        add(m_legend.isEmpty() ? "+2σ" : tr("High"), m_upper.value(h, NaN), band);
        add(m_legend.isEmpty() ? "-2σ" : tr("Low"), m_lower.value(h, NaN), band);
        drawTooltip(p, QPointF(hx, m_mouse.y()), lines, sw);
    }
}

// ---------------------------------------------------------------- ZScoreChart

ZScoreChart::ZScoreChart(QWidget *parent)
    : Chart(parent)
{
    m_margins = QMargins(40, 30, 70, 28);
}

void ZScoreChart::setData(const QVector<QDate> &dates, const QVector<double> &z)
{
    m_dates = dates;
    m_z = z;
    update();
}

void ZScoreChart::paint(QPainter &p, const QRectF &plot)
{
    const Palette &c = Theme::current();
    const int n = m_dates.size();
    if (n == 0 || m_z.size() != n) {
        drawEmpty(p, tr("No data"));
        return;
    }
    double zmax = 3;
    for (double v : m_z)
        if (ok(v))
            zmax = std::max(zmax, std::ceil(std::fabs(v)));
    Axis axis;
    axis.lo = -zmax;
    axis.hi = zmax;
    for (double t = -zmax; t <= zmax + 1e-9; t += 1)
        axis.ticks.push_back(t);
    drawValueAxis(p, plot, axis, [](double v) { return QString::number(v, 'f', 1); }, false, false);
    drawDateAxis(p, plot, m_dates, false);
    auto X = [&](int i) { return xOf(plot, i, n); };
    auto Y = [&](double v) { return plot.bottom() - (v - axis.lo) / (axis.hi - axis.lo) * plot.height(); };
    // Zones.
    p.fillRect(QRectF(plot.left(), plot.top(), plot.width(), Y(2) - plot.top()), withAlpha(c.Down, 45));
    p.fillRect(QRectF(plot.left(), Y(-2), plot.width(), plot.bottom() - Y(-2)), withAlpha(c.Up, 45));
    p.setPen(QPen(c.TextFaint, 1, Qt::DotLine));
    p.drawLine(QPointF(plot.left(), Y(2)), QPointF(plot.right(), Y(2)));
    p.drawLine(QPointF(plot.left(), Y(-2)), QPointF(plot.right(), Y(-2)));
    p.setPen(QPen(c.TextDim, 1));
    p.drawLine(QPointF(plot.left(), Y(0)), QPointF(plot.right(), Y(0)));
    p.setFont(chartFont(8, true));
    p.setPen(c.Down);
    p.drawText(QRectF(plot.right() + 4, Y(2) - 8, m_margins.right() - 4, 16), Qt::AlignLeft | Qt::AlignVCenter, tr("SELL (+2σ)"));
    p.setPen(c.Up);
    p.drawText(QRectF(plot.right() + 4, Y(-2) - 8, m_margins.right() - 4, 16), Qt::AlignLeft | Qt::AlignVCenter, tr("BUY (-2σ)"));
    // The line, coloured by distance from zero: text colour near zero, yellow around 1σ, orange at 2σ and beyond.
    auto colorOf = [&](double z) {
        const double a = std::min(std::fabs(z), 2.5);
        const QColor base = c.TextDim, mid = research::palette::yellow(), far = research::palette::orange();
        if (a <= 1)
            return QColor::fromRgbF(base.redF() + (mid.redF() - base.redF()) * a, base.greenF() + (mid.greenF() - base.greenF()) * a,
                                    base.blueF() + (mid.blueF() - base.blueF()) * a);
        const double t = std::min(1.0, (a - 1) / 1.5);
        return QColor::fromRgbF(mid.redF() + (far.redF() - mid.redF()) * t, mid.greenF() + (far.greenF() - mid.greenF()) * t,
                                mid.blueF() + (far.blueF() - mid.blueF()) * t);
    };
    p.save();
    p.setClipRect(plot);
    int last = -1;
    for (int i = 0; i + 1 < n; ++i) {
        if (!ok(m_z[i]) || !ok(m_z[i + 1]))
            continue;
        p.setPen(QPen(colorOf((m_z[i] + m_z[i + 1]) / 2), 1.5));
        p.drawLine(QPointF(X(i), Y(m_z[i])), QPointF(X(i + 1), Y(m_z[i + 1])));
    }
    for (int i = n - 1; i >= 0; --i)
        if (ok(m_z[i])) {
            last = i;
            break;
        }
    if (last >= 0) {
        p.setPen(QPen(c.Background, 1.5));
        p.setBrush(research::palette::yellow());
        p.drawEllipse(QPointF(X(last), Y(m_z[last])), 4, 4);
    }
    p.restore();
    const int h = m_hovering ? indexAt(plot, m_mouse.x(), n) : -1;
    if (h >= 0 && ok(m_z[h])) {
        const double hx = X(h);
        p.setPen(QPen(c.ControlBorderHover, 1, Qt::DashLine));
        p.drawLine(QPointF(hx, plot.top()), QPointF(hx, plot.bottom()));
        drawTooltip(p, QPointF(hx, m_mouse.y()), {dateText(m_dates[h]), QString("z: %1").arg(QString::number(m_z[h], 'f', 2))},
                    {colorOf(m_z[h])});
    }
}

// ---------------------------------------------------------------- ScatterChart

ScatterChart::ScatterChart(QWidget *parent)
    : Chart(parent)
{
    m_margins = QMargins(56, 30, 20, 40);
    m_pointColor = research::palette::pink();
}

void ScatterChart::setData(const QVector<QDate> &dates, const QVector<double> &x, const QVector<double> &y, double alpha, double beta)
{
    m_dates = dates;
    m_x = x;
    m_y = y;
    m_alpha = alpha;
    m_beta = beta;
    update();
}

void ScatterChart::setLabels(const QString &xLabel, const QString &yLabel, const QColor &xColor, const QColor &yColor)
{
    m_xLabel = xLabel;
    m_yLabel = yLabel;
    m_xColor = xColor;
    m_yColor = yColor;
    update();
}

void ScatterChart::setMarginals(bool on)
{
    m_marginals = on;
    m_margins = on ? QMargins(110, 24, 20, 80) : QMargins(56, 30, 20, 40);
    update();
}

void ScatterChart::paint(QPainter &p, const QRectF &plot)
{
    const Palette &c = Theme::current();
    const int n = std::min(m_x.size(), m_y.size());
    if (n < 3) {
        drawEmpty(p, tr("No data"));
        return;
    }
    double loX = INFINITY, hiX = -INFINITY, loY = INFINITY, hiY = -INFINITY;
    for (int i = 0; i < n; ++i) {
        if (!ok(m_x[i]) || !ok(m_y[i]))
            continue;
        loX = std::min(loX, m_x[i]);
        hiX = std::max(hiX, m_x[i]);
        loY = std::min(loY, m_y[i]);
        hiY = std::max(hiY, m_y[i]);
    }
    if (!std::isfinite(loX)) {
        drawEmpty(p, tr("No data"));
        return;
    }
    // Symmetric around zero so the crosshair sits in the middle.
    const double mx = std::max(std::fabs(loX), std::fabs(hiX)) * 1.08, my = std::max(std::fabs(loY), std::fabs(hiY)) * 1.08;
    const Axis axX = axisAround(-mx, mx, 5), axY = axisAround(-my, my, 5);
    const Format fmt = [](double v) { return QString::number(v, 'f', 1); };
    auto X = [&](double v) { return plot.left() + (v - axX.lo) / (axX.hi - axX.lo) * plot.width(); };
    auto Y = [&](double v) { return plot.bottom() - (v - axY.lo) / (axY.hi - axY.lo) * plot.height(); };
    drawValueAxis(p, plot, axY, fmt, false, !m_marginals);
    p.setFont(chartFont(9));
    for (double t : axX.ticks) {
        const double x = X(t);
        if (!m_marginals) {
            p.setPen(QPen(c.GridLine, 1));
            p.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
        }
        p.setPen(c.TextFaint);
        p.drawText(QRectF(x - 30, plot.bottom() + 4, 60, 14), Qt::AlignCenter, fmt(t));
    }
    p.setPen(QPen(c.PanelLine, 1));
    p.drawRect(plot);
    p.setPen(QPen(c.TextDim, 1));
    p.drawLine(QPointF(X(0), plot.top()), QPointF(X(0), plot.bottom()));
    p.drawLine(QPointF(plot.left(), Y(0)), QPointF(plot.right(), Y(0)));
    // Axis titles.
    p.setFont(chartFont(9, true));
    if (!m_xLabel.isEmpty()) {
        p.setPen(m_xColor.isValid() ? m_xColor : c.TextDim);
        p.drawText(QRectF(plot.left(), plot.bottom() + 18, plot.width(), 14), Qt::AlignCenter, m_xLabel);
    }
    if (!m_yLabel.isEmpty()) {
        p.save();
        p.translate(12, plot.center().y());
        p.rotate(-90);
        p.setPen(m_yColor.isValid() ? m_yColor : c.TextDim);
        p.drawText(QRectF(-plot.height() / 2, -7, plot.height(), 14), Qt::AlignCenter, m_yLabel);
        p.restore();
    }
    // Histograms of each variable along the sides, with a smoothed density.
    if (m_marginals) {
        const QColor bar = research::palette::yellow(), curve = research::palette::red();
        auto histogram = [&](const QVector<double> &v, const Axis &ax, int bins) {
            QVector<double> counts(bins, 0.0);
            for (double x : v)
                if (ok(x)) {
                    const int b = std::clamp(int((x - ax.lo) / (ax.hi - ax.lo) * bins), 0, bins - 1);
                    counts[b] += 1;
                }
            return counts;
        };
        auto density = [&](const QVector<double> &v, const Axis &ax, int steps) {
            // Gaussian kernel, Silverman's bandwidth, scaled to counts per bin below.
            QVector<double> vals;
            for (double x : v)
                if (ok(x))
                    vals.push_back(x);
            QVector<double> out(steps, 0.0);
            if (vals.size() < 2)
                return out;
            const double sd = stdev(vals), h = std::max(1e-6, 1.06 * sd * std::pow(double(vals.size()), -0.2));
            for (int s = 0; s < steps; ++s) {
                const double x = ax.lo + (ax.hi - ax.lo) * (s + 0.5) / steps;
                double d = 0;
                for (double xv : vals)
                    d += std::exp(-0.5 * std::pow((x - xv) / h, 2));
                out[s] = d / (vals.size() * h * std::sqrt(2 * M_PI));
            }
            return out;
        };
        const int bins = 24, steps = 80;
        // Bottom: X.
        {
            const QVector<double> h = histogram(m_x, axX, bins), d = density(m_x, axX, steps);
            double maxC = 1;
            for (double v : h) maxC = std::max(maxC, v);
            const double binW = (axX.hi - axX.lo) / bins, scale = n * binW;   // density × n × bin width ≈ counts
            const QRectF area(plot.left(), plot.bottom() + 34, plot.width(), 40);
            for (int b = 0; b < bins; ++b) {
                const double hh = h[b] / maxC * area.height();
                p.fillRect(QRectF(X(axX.lo + b * binW) + 0.5, area.top(), plot.width() / bins - 1, hh), withAlpha(bar, 200));
            }
            QPainterPath path;
            for (int s = 0; s < steps; ++s) {
                const QPointF pt(X(axX.lo + (axX.hi - axX.lo) * (s + 0.5) / steps), area.top() + std::min(area.height(), d[s] * scale / maxC * area.height()));
                if (s == 0) path.moveTo(pt); else path.lineTo(pt);
            }
            p.setPen(QPen(curve, 1.3));
            p.setBrush(Qt::NoBrush);
            p.drawPath(path);
        }
        // Left: Y.
        {
            const QVector<double> h = histogram(m_y, axY, bins), d = density(m_y, axY, steps);
            double maxC = 1;
            for (double v : h) maxC = std::max(maxC, v);
            const double binW = (axY.hi - axY.lo) / bins, scale = n * binW;
            const QRectF area(plot.left() - 50, plot.top(), 40, plot.height());
            for (int b = 0; b < bins; ++b) {
                const double ww = h[b] / maxC * area.width();
                const double y1 = Y(axY.lo + (b + 1) * binW), y0 = Y(axY.lo + b * binW);
                p.fillRect(QRectF(area.right() - ww, y1 + 0.5, ww, y0 - y1 - 1), withAlpha(bar, 200));
            }
            QPainterPath path;
            for (int s = 0; s < steps; ++s) {
                const QPointF pt(area.right() - std::min(area.width(), d[s] * scale / maxC * area.width()), Y(axY.lo + (axY.hi - axY.lo) * (s + 0.5) / steps));
                if (s == 0) path.moveTo(pt); else path.lineTo(pt);
            }
            p.setPen(QPen(curve, 1.3));
            p.setBrush(Qt::NoBrush);
            p.drawPath(path);
        }
    }
    p.save();
    p.setClipRect(plot.adjusted(-1, -1, 1, 1));
    // Points.
    p.setPen(Qt::NoPen);
    p.setBrush(withAlpha(m_pointColor, 190));
    int nearest = -1;
    double best = 1e9;
    for (int i = 0; i < n; ++i) {
        if (!ok(m_x[i]) || !ok(m_y[i]))
            continue;
        const QPointF pt(X(m_x[i]), Y(m_y[i]));
        p.drawEllipse(pt, 2.4, 2.4);
        if (m_hovering) {
            const double d = std::hypot(pt.x() - m_mouse.x(), pt.y() - m_mouse.y());
            if (d < best && d < 12) {
                best = d;
                nearest = i;
            }
        }
    }
    // Regression line.
    if (ok(m_alpha) && ok(m_beta)) {
        p.setPen(QPen(research::palette::yellow(), 1.5));
        p.drawLine(QPointF(X(axX.lo), Y(m_alpha + m_beta * axX.lo)), QPointF(X(axX.hi), Y(m_alpha + m_beta * axX.hi)));
    }
    p.restore();
    // Statistics box, bottom right.
    if (!m_stats.isEmpty()) {
        p.setFont(chartFont(9));
        const QFontMetrics fm(p.font());
        int wl = 0, wv = 0;
        for (const auto &[k, v] : m_stats) {
            wl = std::max(wl, fm.horizontalAdvance(k));
            wv = std::max(wv, fm.horizontalAdvance(v));
        }
        const int lineH = 14;
        const QRectF box(plot.right() - (wl + wv + 36), plot.bottom() - (m_stats.size() * lineH + 12) - 6, wl + wv + 36,
                         m_stats.size() * lineH + 12);
        p.setPen(QPen(c.PanelLine, 1));
        p.setBrush(withAlpha(c.Panel, 230));
        p.drawRoundedRect(box, 3, 3);
        for (int i = 0; i < m_stats.size(); ++i) {
            const double y = box.top() + 6 + i * lineH;
            p.setPen(c.TextDim);
            p.drawText(QRectF(box.left() + 10, y, wl + 4, lineH), Qt::AlignLeft | Qt::AlignVCenter, m_stats[i].first);
            p.setPen(c.Text);
            p.drawText(QRectF(box.right() - 10 - wv, y, wv, lineH), Qt::AlignRight | Qt::AlignVCenter, m_stats[i].second);
        }
    }
    if (nearest >= 0) {
        p.setPen(QPen(c.Text, 1));
        p.setBrush(m_pointColor);
        p.drawEllipse(QPointF(X(m_x[nearest]), Y(m_y[nearest])), 4, 4);
        drawTooltip(p, QPointF(X(m_x[nearest]), Y(m_y[nearest])),
                    {nearest < m_dates.size() ? dateText(m_dates[nearest]) : QString(),
                     QString("%1: %2%").arg(m_xLabel.isEmpty() ? "x" : m_xLabel, QString::number(m_x[nearest], 'f', 2)),
                     QString("%1: %2%").arg(m_yLabel.isEmpty() ? "y" : m_yLabel, QString::number(m_y[nearest], 'f', 2))},
                    {m_xColor, m_yColor});
    }
}

// ---------------------------------------------------------------- LongShortBars

LongShortBars::LongShortBars(QWidget *parent)
    : Chart(parent)
{
    m_margins = QMargins(16, 30, 16, 44);
}

void LongShortBars::setData(const QVector<Group> &groups)
{
    m_groups = groups;
    update();
}

void LongShortBars::paint(QPainter &p, const QRectF &plot)
{
    const Palette &c = Theme::current();
    if (m_groups.isEmpty()) {
        drawEmpty(p, tr("No data"));
        return;
    }
    const int n = m_groups.size();
    const double slot = plot.width() / n, barW = std::min(70.0, slot * 0.6);
    p.setFont(chartFont(9));
    int hover = -1;
    for (int i = 0; i < n; ++i) {
        const Group &g = m_groups[i];
        const double total = g.longPos + g.shortPos;
        const double x = plot.left() + slot * i + (slot - barW) / 2;
        const double hLong = total > 0 ? plot.height() * g.longPos / total : 0;
        const QRectF longRect(x, plot.bottom() - hLong, barW, hLong), shortRect(x, plot.top(), barW, plot.height() - hLong);
        p.fillRect(shortRect, withAlpha(c.Down, 170));
        p.fillRect(longRect, withAlpha(c.Up, 170));
        p.setPen(c.TextDim);
        p.drawText(QRectF(plot.left() + slot * i, plot.bottom() + 6, slot, 14), Qt::AlignCenter, g.label);
        if (m_hovering && QRectF(x, plot.top(), barW, plot.height()).contains(m_mouse))
            hover = i;
    }
    // Legend.
    const double ly = height() - 10;
    double lx = plot.center().x() - 40;
    for (const auto &[label, color] : {std::make_pair(tr("Long"), c.Up), std::make_pair(tr("Short"), c.Down)}) {
        p.setPen(QPen(color, 2));
        p.drawLine(QPointF(lx, ly), QPointF(lx + 10, ly));
        p.setPen(c.TextDim);
        p.drawText(QPointF(lx + 14, ly + 3), label);
        lx += 50;
    }
    if (hover >= 0) {
        const Group &g = m_groups[hover];
        const double total = g.longPos + g.shortPos;
        drawTooltip(p, m_mouse,
                    {g.label, tr("Long: %1 (%2%)").arg(thousands(g.longPos), QString::number(total > 0 ? 100 * g.longPos / total : 0, 'f', 1)),
                     tr("Short: %1 (%2%)").arg(thousands(g.shortPos), QString::number(total > 0 ? 100 * g.shortPos / total : 0, 'f', 1))},
                    {c.Up, c.Down});
    }
}

// ---------------------------------------------------------------- StepChart

StepChart::StepChart(QWidget *parent)
    : Chart(parent)
{
    m_margins = QMargins(50, 30, 16, 28);
}

void StepChart::setData(const QVector<QDate> &dates, const QVector<Line> &lines)
{
    m_dates = dates;
    m_lines = lines;
    update();
}

void StepChart::paint(QPainter &p, const QRectF &plot)
{
    const Palette &c = Theme::current();
    const int n = m_dates.size();
    double lo = INFINITY, hi = -INFINITY;
    for (const Line &l : m_lines)
        for (double v : l.values)
            if (ok(v)) {
                lo = std::min(lo, v);
                hi = std::max(hi, v);
            }
    if (n == 0 || !std::isfinite(lo)) {
        drawEmpty(p, tr("No data"));
        return;
    }
    lo = std::min(lo, 0.0);
    hi = std::max(hi, 0.0);
    const Axis axis = axisFor(lo, hi, 6);
    drawValueAxis(p, plot, axis, shortNumber, false, true);
    drawDateAxis(p, plot, m_dates, false);
    auto X = [&](int i) { return xOf(plot, i, n); };
    auto Y = [&](double v) { return plot.bottom() - (v - axis.lo) / (axis.hi - axis.lo) * plot.height(); };
    p.setPen(QPen(c.TextDim, 1));
    p.drawLine(QPointF(plot.left(), Y(0)), QPointF(plot.right(), Y(0)));
    p.save();
    p.setClipRect(plot);
    for (const Line &l : m_lines) {
        p.setPen(QPen(l.color, 1.3));
        p.setBrush(Qt::NoBrush);
        p.drawPath(linePath(l.values, X, Y, true));
    }
    p.restore();
    // Legend, top right.
    p.setFont(chartFont(9));
    const QFontMetrics fm(p.font());
    double w = 0;
    for (const Line &l : m_lines)
        w += 18 + fm.horizontalAdvance(l.name) + 12;
    double x = plot.right() - w;
    const double y = plot.top() - 8;
    for (const Line &l : m_lines) {
        p.setPen(QPen(l.color, 1.5));
        p.drawLine(QPointF(x, y), QPointF(x + 12, y));
        p.setPen(c.TextDim);
        p.drawText(QPointF(x + 16, y + 3), l.name);
        x += 18 + fm.horizontalAdvance(l.name) + 12;
    }
    const int h = m_hovering ? indexAt(plot, m_mouse.x(), n) : -1;
    if (h >= 0) {
        const double hx = X(h);
        p.setPen(QPen(c.ControlBorderHover, 1, Qt::DashLine));
        p.drawLine(QPointF(hx, plot.top()), QPointF(hx, plot.bottom()));
        QStringList lines {m_dates[h].toString(Qt::ISODate)};
        QVector<QColor> sw;
        for (const Line &l : m_lines) {
            const double v = l.values.value(h, NaN);
            if (!ok(v))
                continue;
            lines << QString("%1: %2").arg(l.name, signedThousands(v));
            sw << l.color;
        }
        drawTooltip(p, QPointF(hx, m_mouse.y()), lines, sw);
    }
}

// ---------------------------------------------------------------- RegimeChart

RegimeChart::RegimeChart(QWidget *parent)
    : Chart(parent)
{
    m_margins = QMargins(56, 30, 16, 28);
}

void RegimeChart::setData(const QVector<QDate> &dates, const QVector<double> &price, const QVector<int> &regime,
                          const QVector<double> &probability, const QStringList &labels, const QVector<QColor> &colors)
{
    m_dates = dates;
    m_price = price;
    m_regime = regime;
    m_probability = probability;
    m_labels = labels;
    m_colors = colors;
    update();
}

void RegimeChart::paint(QPainter &p, const QRectF &plot)
{
    const Palette &c = Theme::current();
    const int n = m_dates.size();
    if (n == 0 || m_price.size() != n || m_regime.size() != n) {
        drawEmpty(p, tr("No data"));
        return;
    }
    double lo = INFINITY, hi = -INFINITY;
    for (double v : m_price)
        if (ok(v)) {
            lo = std::min(lo, v);
            hi = std::max(hi, v);
        }
    const Axis axis = axisFor(lo, hi, 5);
    const Format fmt = [](double v) { return QString::number(v, 'f', v >= 1000 ? 0 : 2); };
    auto X = [&](int i) { return xOf(plot, i, n); };
    auto Y = [&](double v) { return plot.bottom() - (v - axis.lo) / (axis.hi - axis.lo) * plot.height(); };
    auto colorOf = [&](int r) { return r >= 0 && r < m_colors.size() ? m_colors[r] : c.TextDim; };
    // Shading: one band per run of the same regime.
    p.save();
    p.setClipRect(plot);
    int start = 0;
    for (int i = 1; i <= n; ++i) {
        if (i < n && m_regime[i] == m_regime[start])
            continue;
        const double x0 = start == 0 ? plot.left() : (X(start - 1) + X(start)) / 2;
        const double x1 = i == n ? plot.right() : (X(i - 1) + X(i)) / 2;
        p.fillRect(QRectF(x0, plot.top(), x1 - x0, plot.height()), withAlpha(colorOf(m_regime[start]), 38));
        start = i;
    }
    p.restore();
    drawValueAxis(p, plot, axis, fmt, false, true);
    drawDateAxis(p, plot, m_dates, false);
    p.save();
    p.setClipRect(plot);
    for (int i = 0; i + 1 < n; ++i) {
        if (!ok(m_price[i]) || !ok(m_price[i + 1]))
            continue;
        p.setPen(QPen(colorOf(m_regime[i + 1]), 1.6));
        p.drawLine(QPointF(X(i), Y(m_price[i])), QPointF(X(i + 1), Y(m_price[i + 1])));
    }
    p.restore();
    // Legend, top right.
    p.setFont(chartFont(9));
    const QFontMetrics fm(p.font());
    double w = 0;
    for (const QString &l : m_labels)
        w += 16 + fm.horizontalAdvance(l) + 12;
    double x = plot.right() - w;
    const double y = plot.top() - 8;
    for (int k = 0; k < m_labels.size(); ++k) {
        p.fillRect(QRectF(x, y - 4, 10, 8), colorOf(k));
        p.setPen(c.TextDim);
        p.drawText(QPointF(x + 14, y + 3), m_labels[k]);
        x += 16 + fm.horizontalAdvance(m_labels[k]) + 12;
    }
    const int h = m_hovering ? indexAt(plot, m_mouse.x(), n) : -1;
    if (h >= 0) {
        const double hx = X(h);
        p.setPen(QPen(c.ControlBorderHover, 1, Qt::DashLine));
        p.drawLine(QPointF(hx, plot.top()), QPointF(hx, plot.bottom()));
        QStringList lines {dateText(m_dates[h]), QString("%1: %2").arg(tr("Price"), fmt(m_price[h]))};
        QVector<QColor> sw {c.Text};
        if (m_regime[h] >= 0 && m_regime[h] < m_labels.size()) {
            const double pr = m_probability.value(h, NaN);
            lines << (ok(pr) ? QString("%1 (%2%)").arg(m_labels[m_regime[h]], QString::number(100 * pr, 'f', 0)) : m_labels[m_regime[h]]);
            sw << colorOf(m_regime[h]);
        }
        drawTooltip(p, QPointF(hx, m_mouse.y()), lines, sw);
    }
}

// ---------------------------------------------------------------- StackedAreaChart

StackedAreaChart::StackedAreaChart(QWidget *parent)
    : Chart(parent)
{
    m_margins = QMargins(56, 30, 16, 28);
}

void StackedAreaChart::setData(const QVector<QDate> &dates, const QVector<QVector<double>> &series, const QStringList &labels,
                               const QVector<QColor> &colors)
{
    m_dates = dates;
    m_series = series;
    m_labels = labels;
    m_colors = colors;
    update();
}

void StackedAreaChart::paint(QPainter &p, const QRectF &plot)
{
    const Palette &c = Theme::current();
    const int n = m_dates.size();
    if (n == 0 || m_series.isEmpty()) {
        drawEmpty(p, tr("No data"));
        return;
    }
    Axis axis;
    axis.lo = 0;
    axis.hi = 1;
    axis.ticks = {0, 0.25, 0.5, 0.75, 1};
    drawValueAxis(p, plot, axis, [](double v) { return QString::number(100 * v, 'f', 0) + "%"; }, false, true);
    drawDateAxis(p, plot, m_dates, false);
    auto X = [&](int i) { return xOf(plot, i, n); };
    auto Y = [&](double v) { return plot.bottom() - std::clamp(v, 0.0, 1.0) * plot.height(); };
    p.save();
    p.setClipRect(plot);
    QVector<double> base(n, 0.0);
    for (int k = 0; k < m_series.size(); ++k) {
        const QVector<double> &s = m_series[k];
        QPainterPath area;
        bool started = false;
        QVector<double> top(n);
        for (int i = 0; i < n; ++i) {
            const double v = s.value(i, 0.0);
            top[i] = base[i] + (ok(v) ? v : 0);
            const QPointF pt(X(i), Y(top[i]));
            if (!started) {
                area.moveTo(pt);
                started = true;
            } else {
                area.lineTo(pt);
            }
        }
        for (int i = n - 1; i >= 0; --i)
            area.lineTo(X(i), Y(base[i]));
        area.closeSubpath();
        const QColor color = k < m_colors.size() ? m_colors[k] : c.TextDim;
        p.fillPath(area, withAlpha(color, 150));
        base = top;
    }
    p.restore();
    p.setFont(chartFont(9));
    const QFontMetrics fm(p.font());
    double w = 0;
    for (const QString &l : m_labels)
        w += 16 + fm.horizontalAdvance(l) + 12;
    double x = plot.right() - w;
    const double y = plot.top() - 8;
    for (int k = 0; k < m_labels.size(); ++k) {
        p.fillRect(QRectF(x, y - 4, 10, 8), k < m_colors.size() ? m_colors[k] : c.TextDim);
        p.setPen(c.TextDim);
        p.drawText(QPointF(x + 14, y + 3), m_labels[k]);
        x += 16 + fm.horizontalAdvance(m_labels[k]) + 12;
    }
    const int h = m_hovering ? indexAt(plot, m_mouse.x(), n) : -1;
    if (h >= 0) {
        const double hx = X(h);
        p.setPen(QPen(c.ControlBorderHover, 1, Qt::DashLine));
        p.drawLine(QPointF(hx, plot.top()), QPointF(hx, plot.bottom()));
        QStringList lines {dateText(m_dates[h])};
        QVector<QColor> sw;
        for (int k = 0; k < m_series.size(); ++k) {
            lines << QString("%1: %2%").arg(m_labels.value(k), QString::number(100 * m_series[k].value(h, 0.0), 'f', 1));
            sw << (k < m_colors.size() ? m_colors[k] : c.TextDim);
        }
        drawTooltip(p, QPointF(hx, m_mouse.y()), lines, sw);
    }
}

namespace {
QStringList isoDates(const QVector<QDate> &dates)
{
    QStringList out;
    for (const QDate &d : dates) out << d.toString(Qt::ISODate);
    return out;
}
} // namespace

QString Chart::aiTitle() const
{
    QStringList parts;
    for (const TitlePart &t : m_title) parts << t.text;
    return parts.join(' ').trimmed();
}

QString Chart::aiRead() const
{
    return {};   // a chart with no values of its own to give: what it draws
}

QString MultiLineChart::aiRead() const
{
    const QStringList labels = m_xLabels.isEmpty() ? isoDates(m_dates) : m_xLabels;
    QStringList out {aiTitle()};
    for (const Line &l : m_lines)
        if (const QString line = aiSeries(l.name, labels, l.values); !line.isEmpty()) out << line;
    return out.join('\n').trimmed();
}

QString OverlayChart::aiRead() const
{
    const QStringList labels = isoDates(m_dates);
    return QStringList {aiTitle(), aiSeries(m_nameA, labels, m_a), aiSeries(m_nameB, labels, m_b)}.join('\n').trimmed();
}

QString SpreadChart::aiRead() const
{
    const QStringList labels = isoDates(m_dates);
    return QStringList {aiTitle(), aiSeries(QStringLiteral("Spread"), labels, m_spread), aiSeries(QStringLiteral("Mean"), labels, m_mean, 1),
                        aiSeries(QStringLiteral("Upper band"), labels, m_upper, 1), aiSeries(QStringLiteral("Lower band"), labels, m_lower, 1)}
        .join('\n').trimmed();
}

QString ZScoreChart::aiRead() const
{
    return QStringList {aiTitle(), aiSeries(QStringLiteral("Z-score"), isoDates(m_dates), m_z)}.join('\n').trimmed();
}

QString StepChart::aiRead() const
{
    const QStringList labels = isoDates(m_dates);
    QStringList out {aiTitle()};
    for (const Line &l : m_lines)
        if (const QString line = aiSeries(l.name, labels, l.values); !line.isEmpty()) out << line;
    return out.join('\n').trimmed();
}

QString RegimeChart::aiRead() const
{
    const QStringList labels = isoDates(m_dates);
    QStringList out {aiTitle(), aiSeries(QStringLiteral("Price"), labels, m_price), aiSeries(QStringLiteral("Probability"), labels, m_probability, 6)};
    if (!m_regime.isEmpty() && m_regime.back() >= 0 && m_regime.back() < m_labels.size()) out << "Latest regime: " + m_labels[m_regime.back()];
    return out.join('\n').trimmed();
}

QString StackedAreaChart::aiRead() const
{
    const QStringList labels = isoDates(m_dates);
    QStringList out {aiTitle()};
    for (int k = 0; k < m_series.size(); ++k)
        if (const QString line = aiSeries(m_labels.value(k), labels, m_series[k], 6); !line.isEmpty()) out << line;
    return out.join('\n').trimmed();
}

QString LongShortBars::aiRead() const
{
    QStringList out {aiTitle()};
    for (const Group &g : m_groups) out << QString("%1: long %2, short %3").arg(g.label, aiNumber(g.longPos), aiNumber(g.shortPos));
    return out.join('\n').trimmed();
}

} // namespace quant
