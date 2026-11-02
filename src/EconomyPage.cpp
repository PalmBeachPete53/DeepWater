#include "EconomyPage.h"

#include "CountriesView.h"

#include "OverviewWidgets.h"
#include "QuantCharts.h"
#include "Theme.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QStackedWidget>
#include <QStyle>
#include <QTime>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace economy {

namespace {

constexpr double NaN = research::NaN;

QColor withAlpha(QColor c, int alpha)
{
    c.setAlpha(alpha);
    return c;
}

// About `count` round ticks covering lo…hi; the axis is stretched to the outer ones.
QVector<double> niceTicks(double *lo, double *hi, int count)
{
    if (!(*hi > *lo)) {
        const double pad = std::abs(*lo) > 0 ? std::abs(*lo) * 0.05 : 1;
        *lo -= pad;
        *hi += pad;
    }
    const double raw = (*hi - *lo) / std::max(1, count);
    const double mag = std::pow(10, std::floor(std::log10(raw)));
    double step = mag;
    for (double m : {1.0, 2.0, 2.5, 5.0, 10.0})
        if (m * mag >= raw) { step = m * mag; break; }
    const double first = std::floor(*lo / step) * step, last = std::ceil(*hi / step) * step;
    QVector<double> ticks;
    for (double t = first; t <= last + step * 0.5; t += step)
        ticks << (std::abs(t) < step * 1e-9 ? 0 : t);
    *lo = first;
    *hi = last;
    return ticks;
}

// The series' spacing: 90 days and more is quarterly, 28 monthly, else daily or weekly.
int spacingDays(const Series &s)
{
    if (s.size() < 3)
        return 1;
    QVector<qint64> gaps;
    for (int i = std::max(1, int(s.size()) - 8); i < s.size(); ++i)
        gaps << s[i - 1].date.daysTo(s[i].date);
    std::sort(gaps.begin(), gaps.end());
    return int(gaps[gaps.size() / 2]);
}

// "Oct 29, 2026" / "29 oct. 2026", "Oct 29" / "29 oct.": the day before the month in French.
QString dayText(const QDate &d, bool year)
{
    const QLocale loc;
    const bool dayFirst = loc.language() != QLocale::English;
    return loc.toString(d, dayFirst ? (year ? "d MMM yyyy" : "d MMM") : (year ? "MMM d, yyyy" : "MMM d"));
}

QString dateText(const QDate &d, int spacing)
{
    const QLocale loc;
    if (spacing >= 80)
        return QString("Q%1 %2").arg((d.month() - 1) / 3 + 1).arg(d.year());
    if (spacing >= 28)
        return loc.toString(d, "MMM yyyy");
    return dayText(d, true);
}

QString sourceText(const Indicator &i)
{
    switch (i.source) {
    case Source::Fred: return "FRED · " + i.code;
    case Source::Oecd: return "OECD · KEI";
    case Source::Yahoo: return "Yahoo · " + i.code;
    case Source::StockFearGreed: return text(QT_TRANSLATE_NOOP("Economy", "Deepwater estimate"));
    case Source::CryptoFearGreed: return "alternative.me";
    case Source::AltcoinSeason:
    case Source::CryptoCap:
    case Source::BtcDominance: return "CoinMarketCap";
    }
    return {};
}

// Whether the change is shown in points (rates, spreads, indexes crossing zero) rather than in percent.
bool changeInPoints(const Indicator &i)
{
    return i.unit == Unit::Percent || i.zeroLine || i.zones != Zones::None || i.source == Source::StockFearGreed
           || i.source == Source::CryptoFearGreed || i.source == Source::AltcoinSeason;
}

QString changeText(const Indicator &i, double last, double prev)
{
    if (!std::isfinite(last) || !std::isfinite(prev))
        return {};
    const QLocale loc;
    if (changeInPoints(i)) {
        const double d = last - prev;
        const QString n = (d >= 0 ? "+" : "") + loc.toString(d, 'f', i.decimals);
        return i.unit == Unit::Percent ? text(QT_TRANSLATE_NOOP("Economy", "%1 pp")).arg(n) : n;
    }
    if (prev == 0)
        return {};
    const double pct = 100 * (last / prev - 1);
    return (pct >= 0 ? "+" : "") + loc.toString(pct, 'f', 2) + "%";
}

} // namespace

QString formatValue(const Indicator &i, double v, double step)
{
    if (!std::isfinite(v))
        return "—";
    const QLocale loc;
    const bool axis = step > 0;
    auto decimalsFor = [&](double unit) {
        if (!axis)
            return i.decimals;
        const double s = step / unit;
        return s >= 1 ? 0 : std::clamp(int(std::ceil(-std::log10(s) - 1e-9)), 0, 3);
    };
    if (i.unit == Unit::Dollars || i.unit == Unit::Count) {
        const double a = std::abs(axis ? std::max(std::abs(v), step) : v);
        const struct { double div; const char *suffix; } scales[] {{1e12, "T"}, {1e9, "B"}, {1e6, "M"}, {1e3, "K"}, {1, ""}};
        for (const auto &sc : scales) {
            if (a >= sc.div || sc.div == 1) {
                const int dec = sc.div == 1 && !axis ? 0 : decimalsFor(sc.div);
                QString n = loc.toString(v / sc.div, 'f', dec);
                // "194K" rather than "194,00K"; "1,40M" keeps its decimals.
                if (!axis && dec > 0 && std::abs(v / sc.div - std::round(v / sc.div)) < 0.5 * std::pow(10, -dec))
                    n = loc.toString(std::round(v / sc.div), 'f', 0);
                n += sc.suffix;
                return i.unit == Unit::Dollars ? (v < 0 ? "-$" + n.mid(1) : "$" + n) : n;
            }
        }
    }
    const int dec = decimalsFor(1);
    const QString n = loc.toString(v, 'f', dec);
    if (i.unit == Unit::Percent)
        return n + "%";
    if (i.unit == Unit::Yen)
        return "¥" + n;
    return n;
}

// ---------------------------------------------------------------- ChartView

ChartView::ChartView(const Indicator &indicator, QWidget *parent)
    : QWidget(parent)
    , m_ind(indicator)
    , m_range(indicator.ranges.value(indicator.range))
{
    setMouseTracking(true);
    setMinimumHeight(160);
}

void ChartView::setSeries(const Series &series)
{
    m_all = series;
    m_message.clear();
    setRange(m_range);
}

void ChartView::setMessage(const QString &message)
{
    m_message = message;
    changed();
}

void ChartView::setRange(const QString &range)
{
    m_range = range;
    m_first = 0;
    m_from = m_to = QDate();
    if (!m_all.isEmpty()) {
        m_to = m_all.back().date;
        const int days = rangeDays(range);
        m_from = days > 0 ? m_to.addDays(-days) : m_all.front().date;
        while (m_first < m_all.size() - 1 && m_all[m_first].date < m_from)
            ++m_first;
        // A sparse series (quarterly over two years) keeps at least two points.
        m_first = std::min(m_first, std::max(0, int(m_all.size()) - 2));
        m_from = std::min(m_from, m_all[m_first].date);
        double lo = INFINITY, hi = -INFINITY;
        for (int k = m_first; k < m_all.size(); ++k) {
            lo = std::min(lo, m_all[k].value);
            hi = std::max(hi, m_all[k].value);
        }
        if (m_ind.zones != Zones::None) {
            lo = 0;
            hi = 100;
        } else if (m_ind.zeroLine) {
            lo = std::min(lo, 0.0);
            hi = std::max(hi, 0.0);
        } else {
            const double pad = (hi - lo) * 0.06;
            // A series that never goes below zero keeps its axis there.
            lo = lo >= 0 ? std::max(0.0, lo - pad) : lo - pad;
            hi += pad;
        }
        m_lo = lo;
        m_hi = hi;
        const bool positive = m_lo >= 0;
        m_ticks = niceTicks(&m_lo, &m_hi, std::clamp(height() / 45, 3, 7));
        if (positive && m_lo < 0) {
            m_lo = 0;
            m_ticks.erase(std::remove_if(m_ticks.begin(), m_ticks.end(), [](double t) { return t < 0; }), m_ticks.end());
        }
        if (m_ind.zones != Zones::None) {
            m_lo = 0;
            m_hi = 100;
            m_ticks = {0, 25, 50, 75, 100};
        }
    }
    changed();
}

void ChartView::resizeEvent(QResizeEvent *)
{
    setRange(m_range);   // the tick count follows the height
}

QRectF ChartView::plotRect() const
{
    return QRectF(rect()).adjusted(62, 30, -14, -40);
}

double ChartView::xOf(const QRectF &plot, const QDate &d) const
{
    const double span = std::max<qint64>(1, m_from.daysTo(m_to));
    return plot.left() + plot.width() * m_from.daysTo(d) / span;
}

double ChartView::yOf(const QRectF &plot, double v) const
{
    return plot.bottom() - plot.height() * (v - m_lo) / (m_hi - m_lo);
}

void ChartView::paintLayer(QPainter &p, const QRectF &plot)
{
    const Palette &pal = Theme::current();
    p.setRenderHint(QPainter::Antialiasing);

    // Zones behind everything.
    if (m_ind.zones != Zones::None) {
        struct Band { double lo, hi; const char *label; };
        const QVector<Band> bands = m_ind.zones == Zones::FearGreed
            ? QVector<Band> {{0, 25, QT_TRANSLATE_NOOP("Economy", "Extreme fear")}, {25, 45, QT_TRANSLATE_NOOP("Economy", "Fear")},
                             {45, 55, QT_TRANSLATE_NOOP("Economy", "Neutral")}, {55, 75, QT_TRANSLATE_NOOP("Economy", "Greed")},
                             {75, 100, QT_TRANSLATE_NOOP("Economy", "Extreme greed")}}
            : QVector<Band> {{0, 25, QT_TRANSLATE_NOOP("Economy", "Bitcoin season")}, {25, 75, nullptr},
                             {75, 100, QT_TRANSLATE_NOOP("Economy", "Altcoin season")}};
        p.setFont(quant::chartFont(9));
        for (const Band &b : bands) {
            if (!b.label)
                continue;
            const QRectF r(plot.left(), yOf(plot, b.hi), plot.width(), yOf(plot, b.lo) - yOf(plot, b.hi));
            const QColor c = zoneColor(m_ind.zones, (b.lo + b.hi) / 2);
            p.fillRect(r, withAlpha(c, 34));
            p.setPen(QPen(withAlpha(c, 90), 1, Qt::DashLine));
            p.drawLine(QPointF(r.left(), r.top()), QPointF(r.right(), r.top()));
            p.setPen(withAlpha(c, 220));
            p.drawText(r.adjusted(8, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, text(b.label));
        }
    }

    // Value axis and its grid.
    p.setFont(quant::chartFont(9));
    const double step = m_ticks.size() > 1 ? m_ticks[1] - m_ticks[0] : 0;
    for (double t : m_ticks) {
        const double y = yOf(plot, t);
        p.setPen(QPen(pal.GridLine, 1));
        p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        p.setPen(pal.TextFaint);
        p.drawText(QRectF(0, y - 8, plot.left() - 6, 16), Qt::AlignRight | Qt::AlignVCenter,
                   m_ind.zones != Zones::None ? QString::number(t) : formatValue(m_ind, t, step));
    }

    // Date axis: evenly spaced labels with their grid lines.
    const qint64 span = m_from.daysTo(m_to);
    const int labels = std::clamp(int(plot.width() / 120), 2, 9);
    const QLocale loc;
    for (int k = 0; k < labels; ++k) {
        const QDate d = m_from.addDays(span * k / (labels - 1));
        const double x = xOf(plot, d);
        p.setPen(QPen(pal.GridLine, 1));
        p.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
        p.setPen(pal.TextFaint);
        const QString t = span > 400 ? loc.toString(d, "MMM yyyy") : dayText(d, false);
        const Qt::Alignment a = k == 0 ? Qt::AlignLeft : k == labels - 1 ? Qt::AlignRight : Qt::AlignHCenter;
        const QRectF r = k == 0 ? QRectF(x, plot.bottom() + 3, 100, 14)
                       : k == labels - 1 ? QRectF(x - 100, plot.bottom() + 3, 100, 14)
                                         : QRectF(x - 50, plot.bottom() + 3, 100, 14);
        p.drawText(r, a | Qt::AlignTop, t);
    }

    // The line and the area under it (against zero when asked).
    QPolygonF line;
    for (int k = m_first; k < m_all.size(); ++k)
        line << QPointF(xOf(plot, m_all[k].date), yOf(plot, m_all[k].value));
    const QColor hue = hueColor(m_ind.hue);
    if (line.size() >= 2) {
        p.save();
        p.setClipRect(plot.adjusted(0, -2, 0, 2));
        if (m_ind.zones == Zones::None) {
            const double base = m_ind.zeroLine && m_lo < 0 && m_hi > 0 ? yOf(plot, 0) : plot.bottom();
            QPainterPath area;
            area.moveTo(line.first().x(), base);
            for (const QPointF &pt : line)
                area.lineTo(pt);
            area.lineTo(line.last().x(), base);
            area.closeSubpath();
            QLinearGradient g(0, plot.top(), 0, plot.bottom());
            g.setColorAt(0, withAlpha(hue, 105));
            g.setColorAt(1, withAlpha(hue, 18));
            p.fillPath(area, g);
            p.setPen(QPen(hue, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.drawPolyline(line);
        } else {
            // Coloured by zone, point to point.
            for (int k = 1; k < line.size(); ++k) {
                const double v = (m_all[m_first + k - 1].value + m_all[m_first + k].value) / 2;
                p.setPen(QPen(zoneColor(m_ind.zones, v), 1.6, Qt::SolidLine, Qt::RoundCap));
                p.drawLine(line[k - 1], line[k]);
            }
        }
        p.restore();
    }
    if (m_ind.zeroLine && m_lo < 0 && m_hi > 0) {
        p.setPen(QPen(pal.TextFaint, 1, Qt::DotLine));
        const double y = yOf(plot, 0);
        p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
    }

    // Under the axis: the last observation and the source.
    p.setFont(quant::chartFont(9));
    p.setPen(pal.TextFaint);
    const QRectF foot(8, height() - 17, width() - 16, 14);
    p.drawText(foot, Qt::AlignLeft | Qt::AlignVCenter,
               text(QT_TRANSLATE_NOOP("Economy", "Latest: %1")).arg(dateText(m_all.back().date, spacingDays(m_all))));
    p.drawText(foot, Qt::AlignRight | Qt::AlignVCenter, sourceText(m_ind));
}

void ChartView::paintEvent(QPaintEvent *)
{
    const Palette &pal = Theme::current();
    QPainter p(this);
    if (m_all.isEmpty() || !m_message.isEmpty()) {
        p.setPen(pal.TextFaint);
        p.setFont(quant::chartFont(11));
        p.drawText(rect().adjusted(24, 0, -24, 0), Qt::AlignCenter | Qt::TextWordWrap, m_message.isEmpty() ? text(QT_TRANSLATE_NOOP("Economy", "No data")) : m_message);
        return;
    }
    const QRectF plot = plotRect();
    if (plot.width() < 40 || plot.height() < 30)
        return;
    const qreal dpr = devicePixelRatioF();
    if (m_layer.isNull() || m_layer.size() != size() * dpr || m_layerTheme != pal.id) {
        m_layer = QPixmap(size() * dpr);
        m_layer.setDevicePixelRatio(dpr);
        m_layer.fill(Qt::transparent);
        QPainter lp(&m_layer);
        paintLayer(lp, plot);
        m_layerTheme = pal.id;
    }
    p.drawPixmap(0, 0, m_layer);
    p.setRenderHint(QPainter::Antialiasing);

    // The last value and its change since the observation before.
    const double last = m_all.back().value;
    const double prev = m_all.size() > 1 ? m_all[m_all.size() - 2].value : NaN;
    const QString change = changeText(m_ind, last, prev);
    const QFont big = quant::chartFont(14, true), small = quant::chartFont(10, true);
    const int changeWidth = change.isEmpty() ? 0 : QFontMetrics(small).horizontalAdvance(change) + 8;
    const QString value = formatValue(m_ind, last);
    const QRectF top(plot.left(), 4, plot.width(), 22);
    p.setFont(small);
    const bool up = std::isfinite(prev) && last > prev, down = std::isfinite(prev) && last < prev;
    p.setPen(up ? pal.Up : down ? pal.Down : pal.TextDim);
    p.drawText(top, Qt::AlignRight | Qt::AlignVCenter, change);
    p.setFont(big);
    p.setPen(m_ind.zones != Zones::None ? zoneColor(m_ind.zones, last) : pal.Text);
    p.drawText(top.adjusted(0, 0, -changeWidth, 0), Qt::AlignRight | Qt::AlignVCenter, value);

    // Hover: the nearest observation.
    if (!m_hover || !plot.adjusted(-4, 0, 4, 0).contains(m_mouse))
        return;
    int best = -1;
    double bestDx = INFINITY;
    for (int k = m_first; k < m_all.size(); ++k) {
        const double dx = std::abs(xOf(plot, m_all[k].date) - m_mouse.x());
        if (dx < bestDx) { bestDx = dx; best = k; }
    }
    if (best < 0)
        return;
    const Point &pt = m_all[best];
    const QPointF at(xOf(plot, pt.date), yOf(plot, pt.value));
    p.setPen(QPen(pal.TextFaint, 1, Qt::DashLine));
    p.drawLine(QPointF(at.x(), plot.top()), QPointF(at.x(), plot.bottom()));
    const QColor dot = m_ind.zones != Zones::None ? zoneColor(m_ind.zones, pt.value) : hueColor(m_ind.hue);
    p.setPen(QPen(pal.Background, 1.5));
    p.setBrush(dot);
    p.drawEllipse(at, 4, 4);

    const QString l1 = dateText(pt.date, spacingDays(m_all));
    QString l2 = text(QT_TRANSLATE_NOOP("Economy", "Value: %1")).arg(formatValue(m_ind, pt.value));
    if (m_ind.zones != Zones::None)
        l2 += " · " + zoneLabel(m_ind.zones, pt.value);
    const QFont f1 = quant::chartFont(10, true), f2 = quant::chartFont(10);
    const int w = std::max(QFontMetrics(f1).horizontalAdvance(l1), QFontMetrics(f2).horizontalAdvance(l2)) + 16;
    const int h = 38;
    QRectF box(at.x() + 12, at.y() - h - 6, w, h);
    if (box.right() > width() - 4) box.moveRight(at.x() - 12);
    if (box.top() < 2) box.moveTop(at.y() + 10);
    p.setPen(pal.TooltipBorder);
    p.setBrush(pal.Tooltip);
    p.drawRoundedRect(box, 3, 3);
    p.setPen(pal.Text);
    p.setFont(f1);
    p.drawText(box.adjusted(8, 4, -8, -h / 2.0), Qt::AlignLeft | Qt::AlignVCenter, l1);
    p.setFont(f2);
    p.setPen(pal.TextDim);
    p.drawText(box.adjusted(8, h / 2.0 - 2, -8, -4), Qt::AlignLeft | Qt::AlignVCenter, l2);
}

void ChartView::mouseMoveEvent(QMouseEvent *e)
{
    m_mouse = e->position();
    m_hover = true;
    update();
}

void ChartView::leaveEvent(QEvent *)
{
    m_hover = false;
    update();
}

// ---------------------------------------------------------------- GaugeView

GaugeView::GaugeView(const Indicator &indicator, QWidget *parent)
    : QWidget(parent)
    , m_ind(indicator)
{
    setMinimumHeight(150);
}

void GaugeView::setSeries(const Series &series)
{
    m_series = series;
    m_message.clear();
    update();
}

void GaugeView::setComponents(const QVector<FearGreedComponent> &components)
{
    m_components = components;
    update();
}

void GaugeView::setMessage(const QString &message)
{
    m_message = message;
    update();
}

void GaugeView::paintEvent(QPaintEvent *)
{
    const Palette &pal = Theme::current();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    if (m_series.isEmpty() || !m_message.isEmpty()) {
        p.setPen(pal.TextFaint);
        p.setFont(quant::chartFont(11));
        p.drawText(rect().adjusted(24, 0, -24, 0), Qt::AlignCenter | Qt::TextWordWrap, m_message.isEmpty() ? text(QT_TRANSLATE_NOOP("Economy", "No data")) : m_message);
        return;
    }
    const double value = std::clamp(m_series.back().value, 0.0, 100.0);
    const bool parts = !m_components.isEmpty() && width() > 560;
    // The dial and its readout take the left part when the components are listed, else the middle.
    const double blockW = parts ? width() * 0.55 : width();
    const double r = std::min(height() * 0.48, blockW * 0.22);
    const double cx = blockW / 2 - r * 0.55, cy = height() * 0.40 + r * 0.55;
    const QRectF arcRect(cx - r, cy - r, 2 * r, 2 * r);
    const double thick = std::max(7.0, r * 0.14);

    // The arc, coloured along its length.
    const QVector<std::pair<double, QColor>> stops = m_ind.zones == Zones::AltSeason
        ? QVector<std::pair<double, QColor>> {{0, QColor(0xf0, 0x92, 0x3a)}, {0.5, QColor(0xb8, 0xb2, 0xb8)}, {1, QColor(0x6f, 0x84, 0xf0)}}
        : QVector<std::pair<double, QColor>> {{0, QColor(0xe0, 0x4f, 0x5a)}, {0.3, QColor(0xe8, 0x8a, 0x40)},
                                              {0.5, QColor(0xe0, 0xc0, 0x45)}, {0.72, QColor(0x9c, 0xc8, 0x4f)},
                                              {1, QColor(0x4f, 0xc0, 0x6d)}};
    auto colorAt = [&](double t) {
        for (int k = 1; k < stops.size(); ++k) {
            if (t <= stops[k].first) {
                const double u = (t - stops[k - 1].first) / (stops[k].first - stops[k - 1].first);
                const QColor a = stops[k - 1].second, b = stops[k].second;
                return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * u, a.greenF() + (b.greenF() - a.greenF()) * u,
                                        a.blueF() + (b.blueF() - a.blueF()) * u);
            }
        }
        return stops.back().second;
    };
    constexpr int Steps = 72;
    for (int k = 0; k < Steps; ++k) {
        const double t0 = double(k) / Steps;
        p.setPen(QPen(colorAt(t0 + 0.5 / Steps), thick, Qt::SolidLine, k == 0 || k == Steps - 1 ? Qt::RoundCap : Qt::FlatCap));
        // Qt's angles: 0 at three o'clock, counter-clockwise, in sixteenths of a degree. 0 is on the left.
        const double a0 = 180 - 180.0 * k / Steps;
        p.drawArc(arcRect, int(a0 * 16), int(-(180.0 / Steps + 0.4) * 16));
    }
    // Scale labels.
    p.setFont(quant::chartFont(9));
    p.setPen(pal.TextFaint);
    const bool alt = m_ind.zones == Zones::AltSeason;
    p.drawText(QRectF(cx - r - 30, cy + thick / 2 + 2, 60, 14), Qt::AlignCenter, alt ? "BTC" : "0");
    p.drawText(QRectF(cx + r - 30, cy + thick / 2 + 2, 60, 14), Qt::AlignCenter, alt ? "ALT" : "100");
    p.drawText(QRectF(cx - 30, cy - r - thick / 2 - 16, 60, 14), Qt::AlignCenter, "50");

    // The needle.
    const QColor zc = zoneColor(m_ind.zones, value);
    const double angle = M_PI * (1 - value / 100);
    const QPointF tip(cx + std::cos(angle) * r * 0.82, cy - std::sin(angle) * r * 0.82);
    p.setPen(QPen(zc, 3, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(cx, cy), tip);
    p.setPen(Qt::NoPen);
    p.setBrush(zc);
    p.drawEllipse(QPointF(cx, cy), 6.5, 6.5);

    // Value, class, date.
    const double tx = cx + r + 28;
    p.setFont(quant::chartFont(int(std::clamp(r * 0.42, 22.0, 34.0)), true));
    p.setPen(pal.Text);
    const QFontMetricsF fmBig(p.font());
    p.drawText(QPointF(tx, cy - r * 0.38), QString::number(int(std::round(value))));
    p.setFont(quant::chartFont(12, true));
    p.setPen(zc);
    p.drawText(QPointF(tx, cy - r * 0.38 + 22), zoneLabel(m_ind.zones, value).toUpper());
    p.setFont(quant::chartFont(10));
    p.setPen(pal.TextFaint);
    p.drawText(QRectF(cx - r - 40, cy + thick / 2 + 18, 2 * r + 80, 16), Qt::AlignCenter,
               text(QT_TRANSLATE_NOOP("Economy", "Last: %1")).arg(dayText(m_series.back().date, true)));

    // Components, as bars.
    if (!parts)
        return;
    const double left = blockW + 10, right = width() - 20;
    const double rowH = 24;
    double y = std::max(14.0, height() / 2.0 - m_components.size() * rowH / 2);
    p.setFont(quant::chartFont(10, true));
    p.setPen(pal.TextDim);
    p.drawText(QRectF(left, y - 20, right - left, 16), Qt::AlignLeft | Qt::AlignVCenter,
               text(QT_TRANSLATE_NOOP("Economy", "Components (0 fear · 100 greed)")).toUpper());
    const double nameW = std::min(180.0, (right - left) * 0.48);
    for (const FearGreedComponent &c : m_components) {
        p.setFont(quant::chartFont(10));
        p.setPen(pal.Text);
        p.drawText(QRectF(left, y, nameW, rowH), Qt::AlignLeft | Qt::AlignVCenter, c.name);
        const QRectF track(left + nameW, y + rowH / 2 - 4, right - left - nameW - 36, 8);
        p.setPen(Qt::NoPen);
        p.setBrush(pal.PanelLine);
        p.drawRoundedRect(track, 2, 2);
        if (std::isfinite(c.score)) {
            p.setBrush(zoneColor(Zones::FearGreed, c.score));
            p.drawRoundedRect(QRectF(track.left(), track.top(), track.width() * c.score / 100, track.height()), 2, 2);
        }
        p.setPen(std::isfinite(c.score) ? pal.Text : pal.TextFaint);
        p.drawText(QRectF(track.right() + 4, y, 32, rowH), Qt::AlignRight | Qt::AlignVCenter,
                   std::isfinite(c.score) ? QString::number(int(std::round(c.score))) : "—");
        y += rowH;
    }
}

} // namespace economy

// ---------------------------------------------------------------- EconomyPage

using namespace economy;

EconomyPage::EconomyPage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("economyPage");
    setAttribute(Qt::WA_StyledBackground);
    setFont(quant::chartFont(12));
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);

    auto *sub = new QWidget;
    sub->setObjectName("qtSubBar");
    sub->setAttribute(Qt::WA_StyledBackground);
    sub->setFixedHeight(36);
    auto *sh = new QHBoxLayout(sub);
    sh->setContentsMargins(6, 0, 12, 0);
    sh->setSpacing(2);
    m_tabs = new QButtonGroup(this);
    const QVector<Section> &all = sections();
    for (int i = 0; i < all.size(); ++i) {
        auto *b = new QToolButton;
        b->setObjectName("qtSubTab");
        b->setText(text(all[i].name).toUpper());
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedHeight(36);
        m_tabs->addButton(b, i);
        sh->addWidget(b);
    }
    // Countries: a list of countries and each one's indicators in tables (CountriesView), after the US board.
    const int sectionCount = int(all.size()) + 1;
    {
        auto *b = new QToolButton;
        b->setObjectName("qtSubTab");
        b->setText(tr("Countries").toUpper());
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedHeight(36);
        m_tabs->addButton(b, countriesSection());
        sh->addWidget(b);
    }
    sh->addStretch(1);
    m_status = new QLabel;
    m_status->setObjectName("ecoStatus");
    sh->addWidget(m_status);
    sh->addSpacing(8);
    m_refresh = new QPushButton(tr("Refresh").toUpper());
    m_refresh->setObjectName("newsRefresh");
    m_refresh->setCursor(Qt::PointingHandCursor);
    m_refresh->setToolTip(tr("Download every indicator of this section again"));
    sh->addWidget(m_refresh);
    v->addWidget(sub);

    m_stack = new QStackedWidget;
    v->addWidget(m_stack, 1);
    m_built.fill(nullptr, sectionCount);
    m_loaders.resize(sectionCount);
    for (int i = 0; i < sectionCount; ++i) {
        auto *placeholder = new QWidget;
        m_stack->addWidget(placeholder);
    }

    connect(m_tabs, &QButtonGroup::idClicked, this, &EconomyPage::showSection);
    connect(m_refresh, &QPushButton::clicked, this, [this] { reload(true); });
    // The sources publish at most a few times a day: every half hour is plenty (cached answers cost nothing).
    m_timer.setInterval(30 * 60 * 1000);
    connect(&m_timer, &QTimer::timeout, this, [this] { reload(false); });

    showSection(std::clamp(QSettings().value("economy/section", 0).toInt(), 0, sectionCount - 1));
    updateStatus();
}

void EconomyPage::showSection(int index)
{
    if (index < 0 || index >= m_built.size())
        return;
    m_tabs->button(index)->setChecked(true);
    QSettings().setValue("economy/section", index);
    if (!m_built[index]) {
        QWidget *section = buildSection(index);
        QWidget *placeholder = m_stack->widget(index);
        m_stack->insertWidget(index, section);
        m_stack->removeWidget(placeholder);
        placeholder->deleteLater();
        m_built[index] = section;
        for (const auto &load : std::as_const(m_loaders[index]))
            load();
    }
    m_stack->setCurrentIndex(index);
    // A country's page has its own state and refresh.
    m_status->setVisible(index != countriesSection());
    m_refresh->setVisible(index != countriesSection());
}

void EconomyPage::openCountry(const QString &iso3, int tab)
{
    showSection(countriesSection());
    if (m_countries)
        m_countries->openCountry(iso3, tab);
}

QWidget *EconomyPage::buildSection(int index)
{
    if (index == countriesSection()) {
        m_countries = new CountriesView;
        return m_countries;
    }
    auto *scroll = new QScrollArea;
    scroll->setObjectName("ecoScroll");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *content = new QWidget;
    content->setObjectName("ecoContent");
    content->setAttribute(Qt::WA_StyledBackground);
    auto *v = new QVBoxLayout(content);
    v->setContentsMargins(10, 10, 10, 10);
    v->setSpacing(10);
    const Section &section = sections()[index];
    for (const QVector<Cell> &row : section.rows) {
        auto *h = new QHBoxLayout;
        h->setSpacing(10);
        // A row of dials only is lower; a dial next to a chart takes the chart's height.
        const bool dialsOnly = std::all_of(row.begin(), row.end(), [](const Cell &c) { return c.gauge; });
        for (const Cell &cell : row) {
            QWidget *panel = buildCell(index, cell);
            panel->setFixedHeight(dialsOnly ? 230 : 290);
            h->addWidget(panel, cell.stretch);
        }
        v->addLayout(h);
    }
    v->addStretch(1);
    scroll->setWidget(content);
    return scroll;
}

QWidget *EconomyPage::buildCell(int section, const Cell &cell)
{
    const Indicator &ind = cell.indicator;
    auto *panel = new overview::Panel(text(ind.title));
    auto *help = new QLabel("?");
    help->setObjectName("ecoHelp");
    help->setAlignment(Qt::AlignCenter);
    help->setFixedSize(14, 14);
    help->setToolTip("<p>" + text(ind.help).toHtmlEscaped() + "</p>");
    panel->head()->insertWidget(1, help);

    if (cell.gauge) {
        if (ind.source == Source::StockFearGreed) {
            auto *tag = new QLabel(tr("ESTIMATE"));
            tag->setObjectName("resExperimental");
            tag->setToolTip(text(ind.help));
            tag->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            panel->head()->insertWidget(2, tag);
        }
        auto *view = new GaugeView(ind);
        panel->body()->addWidget(view);
        view->setMessage(tr("Loading…"));
        m_loaders[section].push_back([this, view, ind] {
            started();
            if (ind.source == Source::StockFearGreed) {
                stockFearGreed(view, [this, view](const FearGreed &f, const QString &e) {
                    if (!f.index.isEmpty()) {
                        view->setSeries(f.index);
                        view->setComponents(f.components);
                    } else {
                        view->setMessage(e);
                    }
                    finished(f.index.isEmpty() ? e : QString());
                });
                return;
            }
            load(ind, view, [this, view](const Series &s, const QString &e) {
                if (!s.isEmpty()) view->setSeries(s);
                else view->setMessage(e);
                finished(s.isEmpty() ? e : QString());
            });
        });
        return panel;
    }

    QButtonGroup *group = nullptr;
    panel->head()->addWidget(overview::periodTabs(ind.ranges, &group, ind.range));
    auto *view = new ChartView(ind);
    panel->body()->addWidget(view);
    view->setMessage(tr("Loading…"));
    connect(group, &QButtonGroup::idClicked, view, [view, ranges = ind.ranges](int id) { view->setRange(ranges.value(id)); });
    m_loaders[section].push_back([this, view, ind] {
        started();
        load(ind, view, [this, view](const Series &s, const QString &e) {
            if (!s.isEmpty()) view->setSeries(s);
            else view->setMessage(e);
            finished(s.isEmpty() ? e : QString());
        });
    });
    return panel;
}

void EconomyPage::reload(bool force)
{
    if (force)
        for (const char *group : {"fred", "oecd", "coinmarketcap", "alternative"})
            research::Client::instance()->forget(group);
    const int index = m_tabs->checkedId();
    if (index < 0 || !m_built.value(index))
        return;
    m_errors = 0;
    m_lastError.clear();
    for (const auto &load : std::as_const(m_loaders[index]))
        load();
}

void EconomyPage::started()
{
    ++m_pending;
    updateStatus();
}

void EconomyPage::finished(const QString &error)
{
    m_pending = std::max(0, m_pending - 1);
    if (!error.isEmpty()) {
        ++m_errors;
        m_lastError = error;
    }
    updateStatus();
}

void EconomyPage::updateStatus()
{
    m_refresh->setEnabled(m_pending == 0);
    if (m_pending > 0) {
        m_status->setText(tr("Updating…").toUpper());
        m_status->setProperty("state", "busy");
        m_status->setToolTip({});
    } else if (m_errors > 0) {
        m_status->setText(tr("Unavailable: %1").arg(m_errors).toUpper());
        m_status->setProperty("state", "error");
        m_status->setToolTip(m_lastError);
    } else {
        m_status->setText(tr("Updated %1").arg(QLocale().toString(QTime::currentTime(), "HH:mm")).toUpper());
        m_status->setProperty("state", "ok");
        m_status->setToolTip({});
    }
    m_status->style()->unpolish(m_status);
    m_status->style()->polish(m_status);
}

void EconomyPage::showEvent(QShowEvent *)
{
    m_timer.start();
}

void EconomyPage::hideEvent(QHideEvent *)
{
    m_timer.stop();
}
