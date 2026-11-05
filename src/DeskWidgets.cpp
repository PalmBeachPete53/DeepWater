#include "DeskWidgets.h"

#include "QuantViews.h"
#include "Theme.h"

#include <QDateTime>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace deskui {

QFrame *tile(const QString &title, QLabel **value, QLabel **sub)
{
    QVBoxLayout *body;
    QFrame *f = quant::card(title, &body);
    body->setSpacing(2);
    auto *v = quant::label("—", "qtBigValue");
    v->setWordWrap(true);
    body->addWidget(v);
    *value = v;
    auto *s = quant::label({}, "qtNote");
    s->setWordWrap(true);
    body->addWidget(s);
    body->addStretch(1);   // tiles of a row keep their figures on one line, whatever wraps under them
    if (sub)
        *sub = s;
    return f;
}

void setSigned(QLabel *value, const QString &text, double sign)
{
    const Palette &c = Theme::current();
    value->setText(text);
    value->setStyleSheet(QString("color: %1").arg((sign > 0 ? c.Up : sign < 0 ? c.Down : c.Text).name()));
}

namespace {

class RowBackground : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &index) const override
    {
        const QVariant bg = index.data(Qt::BackgroundRole);
        if (bg.canConvert<QBrush>())
            p->fillRect(opt.rect, bg.value<QBrush>());
        QStyledItemDelegate::paint(p, opt, index);
    }
};

} // namespace

QTableWidget *table(const QStringList &headers)
{
    auto *t = new QTableWidget(0, int(headers.size()));
    t->setObjectName("qtTable");
    t->setItemDelegate(new RowBackground(t));
    t->setHorizontalHeaderLabels(headers);
    t->verticalHeader()->hide();
    t->verticalHeader()->setDefaultSectionSize(26);
    t->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    t->horizontalHeader()->setHighlightSections(false);
    t->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionMode(QAbstractItemView::NoSelection);
    t->setFocusPolicy(Qt::NoFocus);
    t->setShowGrid(false);
    t->setFrameShape(QFrame::NoFrame);
    t->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    t->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    fitColumns(t);
    return t;
}

void fitColumns(QTableWidget *t, int stretch)
{
    for (int c = 0; c < t->columnCount(); ++c)
        t->horizontalHeader()->setSectionResizeMode(c, stretch < 0 || c == stretch ? QHeaderView::Stretch : QHeaderView::ResizeToContents);
}

void fitHeight(QTableWidget *t, int maxRows)
{
    const int rows = maxRows > 0 ? std::min(t->rowCount(), maxRows) : t->rowCount();
    t->setFixedHeight(t->horizontalHeader()->sizeHint().height() + rows * t->verticalHeader()->defaultSectionSize() + 2);
    t->setVerticalScrollBarPolicy(maxRows > 0 && t->rowCount() > maxRows ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
}

QTableWidgetItem *cell(const QString &text, const QColor &color, Qt::Alignment align, bool bold)
{
    auto *it = new QTableWidgetItem(text);
    it->setTextAlignment(align | Qt::AlignVCenter);
    if (color.isValid())
        it->setForeground(color);
    if (bold)
        it->setFont(quant::mono(11, true));
    return it;
}

QString pct(double v, int decimals)
{
    return std::isnan(v) ? QString("—") : QString("%1%2%").arg(v >= 0 ? "+" : "", QLocale().toString(v, 'f', decimals));
}

QString amount(double v, int decimals)
{
    return std::isnan(v) ? QString("—") : QLocale().toString(v, 'f', decimals);
}

QString signedAmount(double v, int decimals)
{
    return std::isnan(v) ? QString("—") : QString("%1%2").arg(v >= 0 ? "+" : "", QLocale().toString(v, 'f', decimals));
}

// ------------------------------------------------------------------ BarsChart

BarsChart::BarsChart(QWidget *parent)
    : Chart(parent)
{
    m_margins = QMargins(56, 30, 12, 26);
    setMinimumHeight(190);
}

void BarsChart::setData(const QStringList &labels, const QVector<double> &values, quant::Format format)
{
    m_labels = labels;
    m_values = values;
    m_format = std::move(format);
    update();
}

QString BarsChart::aiRead() const
{
    QStringList parts;
    for (int i = 0; i < m_labels.size() && i < m_values.size(); ++i)
        parts << QString("%1=%2").arg(m_labels[i], aiNumber(m_values[i]));
    return aiTitle() + ": " + parts.join(", ");
}

void BarsChart::paint(QPainter &p, const QRectF &plot)
{
    const Palette &c = Theme::current();
    if (m_values.isEmpty()) {
        drawEmpty(p, tr("No data"));
        return;
    }
    const auto [lo, hi] = std::minmax_element(m_values.begin(), m_values.end());
    const Axis axis = axisFor(std::min(*lo, 0.0), std::max(*hi, 0.0));
    drawValueAxis(p, plot, axis, m_format, false, true);
    const auto y = [&](double v) { return plot.bottom() - (v - axis.lo) / (axis.hi - axis.lo) * plot.height(); };
    const int n = m_values.size();
    const double slot = plot.width() / n, width = std::max(2.0, slot * 0.68);
    int hover = -1;
    p.setPen(Qt::NoPen);
    for (int i = 0; i < n; ++i) {
        const double x = plot.left() + slot * i + (slot - width) / 2;
        const QRectF r = QRectF(QPointF(x, y(std::max(m_values[i], 0.0))), QPointF(x + width, y(std::min(m_values[i], 0.0))));
        if (m_hovering && m_mouse.x() >= plot.left() + slot * i && m_mouse.x() < plot.left() + slot * (i + 1))
            hover = i;
        p.setBrush(m_values[i] >= 0 ? c.Up : c.Down);
        p.drawRect(r);
    }
    p.setFont(quant::chartFont(9));
    p.setPen(c.TextDim);
    const int step = std::max(1, int(std::ceil(46.0 / slot)));
    for (int i = 0; i < n; i += step)
        p.drawText(QRectF(plot.left() + slot * i - 20, plot.bottom() + 4, slot + 40, 16), Qt::AlignCenter, m_labels.value(i));
    if (hover >= 0)
        drawTooltip(p, m_mouse, {m_labels.value(hover), m_format(m_values[hover])}, {m_values[hover] >= 0 ? c.Up : c.Down});
}

// ------------------------------------------------------------------ HBarsChart

HBarsChart::HBarsChart(QWidget *parent)
    : Chart(parent)
{
    m_margins = QMargins(92, 30, 16, 22);
    setMinimumHeight(170);
}

void HBarsChart::setData(const QStringList &labels, const QVector<double> &values, quant::Format format, const QStringList &tips)
{
    m_labels = labels;
    m_values = values;
    m_tips = tips;
    m_format = std::move(format);
    update();
}

QString HBarsChart::aiRead() const
{
    QStringList parts;
    for (int i = 0; i < m_labels.size() && i < m_values.size(); ++i)
        parts << QString("%1=%2").arg(m_labels[i], aiNumber(m_values[i]));
    return aiTitle() + ": " + parts.join(", ");
}

void HBarsChart::paint(QPainter &p, const QRectF &plot)
{
    const Palette &c = Theme::current();
    if (m_values.isEmpty()) {
        drawEmpty(p, tr("No data"));
        return;
    }
    const auto [lo, hi] = std::minmax_element(m_values.begin(), m_values.end());
    const double from = std::min(*lo, 0.0), to = std::max(*hi, 0.0);
    const double span = to > from ? to - from : 1;
    const auto x = [&](double v) { return plot.left() + (v - from) / span * plot.width(); };
    const int n = m_values.size();
    const double slot = plot.height() / n, height = std::min(slot * 0.66, 18.0);
    p.setFont(quant::chartFont(10));
    int hover = -1;
    for (int i = 0; i < n; ++i) {
        const double top = plot.top() + slot * i + (slot - height) / 2;
        const QRectF r(QPointF(x(std::min(m_values[i], 0.0)), top), QPointF(x(std::max(m_values[i], 0.0)), top + height));
        p.setPen(Qt::NoPen);
        p.setBrush(m_neutral ? c.TextDim : m_values[i] >= 0 ? c.Up : c.Down);
        p.drawRect(r);
        p.setPen(c.TextDim);
        p.drawText(QRectF(4, plot.top() + slot * i, plot.left() - 10, slot), Qt::AlignRight | Qt::AlignVCenter,
                   p.fontMetrics().elidedText(m_labels.value(i), Qt::ElideRight, int(plot.left()) - 14));
        if (m_hovering && m_mouse.y() >= plot.top() + slot * i && m_mouse.y() < plot.top() + slot * (i + 1))
            hover = i;
    }
    p.setPen(QPen(c.PanelLine, 1));
    p.drawLine(QPointF(x(0), plot.top()), QPointF(x(0), plot.bottom()));
    p.setPen(c.TextFaint);
    p.setFont(quant::chartFont(9));
    p.drawText(QRectF(plot.left(), plot.bottom() + 3, plot.width(), 14), Qt::AlignLeft, m_format(from));
    p.drawText(QRectF(plot.left(), plot.bottom() + 3, plot.width(), 14), Qt::AlignRight, m_format(to));
    if (hover >= 0) {
        QStringList lines {m_labels.value(hover), m_format(m_values[hover])};
        if (!m_tips.value(hover).isEmpty())
            lines = m_tips[hover].split('\n');
        drawTooltip(p, m_mouse, lines, {});
    }
}

// ------------------------------------------------------------------ DotsChart

DotsChart::DotsChart(QWidget *parent)
    : Chart(parent)
{
    m_margins = QMargins(56, 30, 14, 26);
    setMinimumHeight(190);
}

void DotsChart::setData(const QVector<double> &x, const QVector<double> &y, quant::Format xFormat, quant::Format yFormat, const QStringList &tips)
{
    m_x = x;
    m_y = y;
    m_tips = tips;
    m_xFormat = std::move(xFormat);
    m_yFormat = std::move(yFormat);
    update();
}

QString DotsChart::aiRead() const
{
    QStringList parts;
    for (int i = 0; i < m_x.size() && i < 60; ++i)
        parts << QString("%1: %2").arg(m_xFormat(m_x[i]), aiNumber(m_y[i]));
    return aiTitle() + ": " + parts.join(", ");
}

double DotsChart::mapX(const QRectF &plot, double x, double lo, double hi) const
{
    if (m_logX) {
        const double a = std::log(std::max(lo, 1e-3)), b = std::log(std::max(hi, 1e-3));
        return plot.left() + (std::log(std::max(x, 1e-3)) - a) / std::max(b - a, 1e-9) * plot.width();
    }
    return plot.left() + (x - lo) / std::max(hi - lo, 1e-9) * plot.width();
}

void DotsChart::paint(QPainter &p, const QRectF &plot)
{
    const Palette &c = Theme::current();
    if (m_x.isEmpty()) {
        drawEmpty(p, tr("No data"));
        return;
    }
    double xLo = m_xLo, xHi = m_xHi;
    if (xHi <= xLo) {
        const auto [a, b] = std::minmax_element(m_x.begin(), m_x.end());
        xLo = *a;
        xHi = *b > *a ? *b : *a + 1;
    }
    const auto [yl, yh] = std::minmax_element(m_y.begin(), m_y.end());
    const Axis axis = axisFor(std::min(*yl, 0.0), std::max(*yh, 0.0));
    drawValueAxis(p, plot, axis, m_yFormat, false, true);
    const auto y = [&](double v) { return plot.bottom() - (v - axis.lo) / (axis.hi - axis.lo) * plot.height(); };
    p.setPen(QPen(c.ControlBorderHover, 1, Qt::DashLine));
    p.drawLine(QPointF(plot.left(), y(0)), QPointF(plot.right(), y(0)));
    p.setPen(Qt::NoPen);
    int hover = -1;
    double best = 144;
    for (int i = 0; i < m_x.size(); ++i) {
        const QPointF at(mapX(plot, m_x[i], xLo, xHi), y(m_y[i]));
        p.setBrush(m_y[i] >= 0 ? c.Up : c.Down);
        p.drawEllipse(at, 3, 3);
        const double d = (at.x() - m_mouse.x()) * (at.x() - m_mouse.x()) + (at.y() - m_mouse.y()) * (at.y() - m_mouse.y());
        if (m_hovering && d < best) {
            best = d;
            hover = i;
        }
    }
    p.setPen(c.TextFaint);
    p.setFont(quant::chartFont(9));
    for (int i = 0; i <= 4; ++i) {
        const double f = i / 4.0;
        const double value = m_logX ? std::exp(std::log(std::max(xLo, 1e-3)) * (1 - f) + std::log(std::max(xHi, 1e-3)) * f) : xLo + (xHi - xLo) * f;
        const double at = plot.left() + plot.width() * f;
        p.drawText(QRectF(at - 40, plot.bottom() + 5, 80, 14), Qt::AlignCenter, m_xFormat(value));
    }
    if (hover >= 0) {
        const QStringList lines = !m_tips.value(hover).isEmpty() ? m_tips[hover].split('\n') : QStringList {m_xFormat(m_x[hover]), m_yFormat(m_y[hover])};
        drawTooltip(p, m_mouse, lines, {});
    }
}

// ------------------------------------------------------------------ RadarChart

RadarChart::RadarChart(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(190);
}

void RadarChart::setData(const QStringList &names, const QVector<double> &values)
{
    m_names = names;
    m_values = values;
    update();
}

QString RadarChart::aiRead() const
{
    QStringList parts;
    for (int i = 0; i < m_names.size() && i < m_values.size(); ++i)
        parts << QString("%1=%2").arg(m_names[i], aiNumber(m_values[i]));
    return parts.join(", ");
}

void RadarChart::paintEvent(QPaintEvent *)
{
    const int n = m_names.size();
    if (n < 3 || m_values.size() != n)
        return;
    const Palette &c = Theme::current();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QPointF center(width() / 2.0, height() / 2.0 + 2);
    // Room for the longest name beside the web: names differ a lot in length between languages.
    const QFontMetricsF labels(quant::chartFont(10));
    double longest = 0;
    for (const QString &name : m_names)
        longest = std::max(longest, labels.horizontalAdvance(name));
    const double radius = std::max(30.0, std::min(width() / 2.0 - std::max(62.0, longest * 0.87 + 16), height() / 2.0 - 24));
    const auto at = [&](int i, double r) {
        const double a = -M_PI / 2 + 2 * M_PI * i / n;
        return center + QPointF(std::cos(a) * r, std::sin(a) * r);
    };
    p.setPen(QPen(c.PanelLine, 1));
    p.setBrush(Qt::NoBrush);
    for (int ring = 1; ring <= 4; ++ring) {
        QPolygonF poly;
        for (int i = 0; i < n; ++i)
            poly << at(i, radius * ring / 4);
        p.drawPolygon(poly);
    }
    for (int i = 0; i < n; ++i)
        p.drawLine(center, at(i, radius));
    QPolygonF shape;
    for (int i = 0; i < n; ++i)
        shape << at(i, radius * std::clamp(m_values[i], 0.0, 100.0) / 100.0);
    QColor fill = c.Accent;
    fill.setAlpha(60);
    p.setBrush(fill);
    p.setPen(QPen(c.Accent, 1.6));
    p.drawPolygon(shape);
    p.setBrush(c.Accent);
    for (const QPointF &pt : shape)
        p.drawEllipse(pt, 2.5, 2.5);
    p.setFont(quant::chartFont(10));
    p.setPen(c.TextDim);
    for (int i = 0; i < n; ++i) {
        const QPointF pt = at(i, radius + 12);
        const double cosA = std::cos(-M_PI / 2 + 2 * M_PI * i / n);
        const Qt::Alignment horizontal = cosA > 0.2 ? Qt::AlignLeft : cosA < -0.2 ? Qt::AlignRight : Qt::AlignHCenter;
        const QRectF box = horizontal == Qt::AlignLeft ? QRectF(pt.x(), pt.y() - 9, width() - pt.x(), 18)
                         : horizontal == Qt::AlignRight ? QRectF(0, pt.y() - 9, pt.x(), 18)
                                                        : QRectF(pt.x() - 60, pt.y() - 9, 120, 18);
        p.drawText(box, horizontal | Qt::AlignVCenter, m_names[i]);
    }
}

// ------------------------------------------------------------------ GaugeDial

GaugeDial::GaugeDial(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(170);
}

void GaugeDial::setValue(double value, const QString &caption)
{
    m_value = value;
    m_caption = caption;
    update();
}

QString GaugeDial::aiRead() const
{
    return m_value < 0 ? QString() : QString("%1 (%2)").arg(aiNumber(m_value), m_caption);
}

void GaugeDial::paintEvent(QPaintEvent *)
{
    const Palette &c = Theme::current();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const double radius = std::min(width() / 2.0 - 26, height() - 56.0);
    const QPointF center(width() / 2.0, radius + 16);
    const int ticks = 40;
    const double value = std::clamp(m_value, 0.0, 100.0);
    for (int i = 0; i <= ticks; ++i) {
        const double f = double(i) / ticks;
        const double a = M_PI * (1 - f);
        const QPointF dir(std::cos(a), -std::sin(a));
        QColor col = f < 0.4 ? c.Down : f <= 0.6 ? c.TextDim : c.Up;
        if (m_value < 0 || f * 100 > value)
            col.setAlpha(50);
        p.setPen(QPen(col, 3.2, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(center + dir * (radius - 14), center + dir * radius);
    }
    p.setFont(quant::chartFont(9));
    p.setPen(c.TextFaint);
    for (int v : {0, 20, 40, 60, 80, 100}) {
        const double a = M_PI * (1 - v / 100.0);
        const QPointF at = center + QPointF(std::cos(a), -std::sin(a)) * (radius + 11);
        p.drawText(QRectF(at.x() - 14, at.y() - 7, 28, 14), Qt::AlignCenter, QString::number(v));
    }
    if (m_value < 0)
        return;
    const QColor tone = value < 40 ? c.Down : value <= 60 ? c.Text : c.Up;
    p.setFont(quant::chartFont(30, true));
    p.setPen(tone);
    p.drawText(QRectF(center.x() - 70, center.y() - 42, 140, 42), Qt::AlignCenter, QString::number(qRound(value)));
    p.setFont(quant::chartFont(11, true));
    p.drawText(QRectF(center.x() - 120, center.y() + 2, 240, 18), Qt::AlignCenter, m_caption);
}

// ------------------------------------------------------------------ LevelsChart

LevelsChart::LevelsChart(QWidget *parent)
    : Chart(parent)
{
    m_margins = QMargins(10, 30, 110, 26);
    setMinimumHeight(380);
}

void LevelsChart::setData(const QVector<qint64> &times, const QVector<double> &closes, const QVector<Level> &levels, int decimals)
{
    m_times = times;
    m_closes = closes;
    m_levels = levels;
    m_decimals = decimals;
    update();
}

QString LevelsChart::aiRead() const
{
    QStringList parts;
    for (const Level &l : m_levels)
        parts << QString("%1 %2").arg(l.label, aiNumber(l.price));
    return aiTitle() + QString(": last %1; levels: ").arg(m_closes.isEmpty() ? QString("-") : aiNumber(m_closes.last())) + parts.join(", ");
}

void LevelsChart::paint(QPainter &p, const QRectF &plot)
{
    const Palette &c = Theme::current();
    if (m_closes.size() < 2) {
        drawEmpty(p, tr("No data"));
        return;
    }
    auto [lowIt, highIt] = std::minmax_element(m_closes.begin(), m_closes.end());
    double lo = *lowIt, hi = *highIt;
    // Only the levels near the price widen the scale: a far one would flatten the line.
    const double span = hi - lo;
    QVector<Level> shown;
    for (const Level &l : m_levels)
        if (l.price >= lo - span * 0.35 && l.price <= hi + span * 0.35) {
            shown.push_back(l);
            lo = std::min(lo, l.price);
            hi = std::max(hi, l.price);
        }
    const double pad = std::max(hi - lo, std::abs(hi) * 1e-6) * 0.05;
    lo -= pad;
    hi += pad;
    const auto y = [&](double v) { return plot.bottom() - (v - lo) / (hi - lo) * plot.height(); };
    const int n = m_closes.size();
    p.setPen(QPen(c.PanelLine, 1));
    p.drawRect(plot);
    p.setFont(quant::chartFont(9));
    double lastLabel = -100;
    QVector<Level> ordered = shown;
    std::sort(ordered.begin(), ordered.end(), [](const Level &a, const Level &b) { return a.price > b.price; });
    for (const Level &l : ordered) {
        p.setPen(QPen(l.color, 1, l.dashed ? Qt::DashLine : Qt::SolidLine));
        p.drawLine(QPointF(plot.left(), y(l.price)), QPointF(plot.right(), y(l.price)));
        const double at = std::max(y(l.price), lastLabel + 12);
        lastLabel = at;
        p.setPen(l.color);
        p.drawText(QRectF(plot.right() + 5, at - 7, m_margins.right() - 6, 14), Qt::AlignLeft | Qt::AlignVCenter,
                   QString("%1 %2").arg(l.label, QLocale().toString(l.price, 'f', m_decimals)));
    }
    QPainterPath path;
    for (int i = 0; i < n; ++i) {
        const QPointF pt(plot.left() + plot.width() * i / (n - 1), y(m_closes[i]));
        if (i == 0) path.moveTo(pt); else path.lineTo(pt);
    }
    p.setPen(QPen(c.Text, 1.3));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);
    p.setPen(c.TextFaint);
    for (int i = 0; i < 5; ++i) {
        const int k = (n - 1) * i / 4;
        const QDateTime t = QDateTime::fromSecsSinceEpoch(m_times.value(k)).toLocalTime();
        p.drawText(QRectF(plot.left() + plot.width() * i / 4 - 40, plot.bottom() + 5, 80, 14), Qt::AlignCenter, QLocale().toString(t.date(), "d MMM"));
    }
    if (m_hovering && plot.contains(m_mouse)) {
        const int k = std::clamp(int(std::round((m_mouse.x() - plot.left()) / plot.width() * (n - 1))), 0, n - 1);
        const QDateTime t = QDateTime::fromSecsSinceEpoch(m_times.value(k)).toLocalTime();
        p.setPen(QPen(c.ControlBorderHover, 1, Qt::DashLine));
        p.drawLine(QPointF(plot.left() + plot.width() * k / (n - 1), plot.top()), QPointF(plot.left() + plot.width() * k / (n - 1), plot.bottom()));
        drawTooltip(p, m_mouse, {QLocale().toString(t, QLocale::ShortFormat), QLocale().toString(m_closes[k], 'f', m_decimals)}, {});
    }
}

// ------------------------------------------------------------------ split bars

class SplitBarDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &index) const override
    {
        const double share = index.data(Qt::UserRole).toDouble();
        const Palette &c = Theme::current();
        const QRectF r = QRectF(opt.rect).adjusted(8, opt.rect.height() / 2.0 - 3.5, -8, -(opt.rect.height() / 2.0 - 3.5));
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        p->setPen(Qt::NoPen);
        p->setBrush(c.Down);
        p->drawRoundedRect(r, 2, 2);
        p->setBrush(c.Up);
        p->drawRoundedRect(QRectF(r.left(), r.top(), r.width() * std::clamp(share, 0.0, 100.0) / 100.0, r.height()), 2, 2);
        p->restore();
    }
};

void useSplitBars(QTableWidget *t, int column)
{
    t->setItemDelegateForColumn(column, new SplitBarDelegate(t));
}

} // namespace deskui
