#include "MacroPanel.h"

#include "MacroData.h"
#include "Theme.h"

#include <QLabel>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollArea>
#include <QToolTip>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

// ---------------------------------------------------------------- IndicatorPanel

IndicatorPanel::IndicatorPanel(MacroIndicators *macro, QWidget *parent)
    : QWidget(parent)
    , m_macro(macro)
{
    setObjectName("sidePanel");
    setAttribute(Qt::WA_StyledBackground);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *content = new QWidget;
    content->setObjectName("panelContent");
    auto *cl = new QVBoxLayout(content);
    cl->setContentsMargins(16, 20, 16, 16);
    cl->setSpacing(6);

    auto *kicker = new QLabel(tr("Macro").toUpper());
    kicker->setObjectName("kicker");
    kicker->setStyleSheet(QString("color: %1;").arg(Theme::current().TextDim.name()));
    cl->addWidget(kicker);
    auto *title = new QLabel(tr("Indicators by country"));
    title->setObjectName("title");
    title->setWordWrap(true);
    cl->addWidget(title);
    auto *hint = new QLabel(tr("Click an indicator to color the globe and rank countries."));
    hint->setObjectName("dim");
    hint->setWordWrap(true);
    cl->addWidget(hint);
    cl->addSpacing(8);

    for (int i = 0; i < MacroIndicators::Count; ++i) {
        const MacroIndicators::Group group = MacroIndicators::info(MacroIndicators::Id(i)).group;
        if (i == 0 || group != MacroIndicators::info(MacroIndicators::Id(i - 1)).group) {
            auto *heading = new QLabel((group == MacroIndicators::Group::Economy        ? tr("Economy")
                                        : group == MacroIndicators::Group::Latest       ? tr("Latest official figures")
                                        : group == MacroIndicators::Group::SupplyChain ? tr("Supply chains")
                                                                                        : tr("Capital flows and trade"))
                                           .toUpper());
            heading->setObjectName("sectionLabel");
            cl->addWidget(heading);
        }
        auto *item = new IndicatorItem(m_macro, MacroIndicators::Id(i));
        connect(item, &IndicatorItem::clicked, this, &IndicatorPanel::indicatorClicked);
        m_items.push_back(item);
        cl->addWidget(item);
    }
    cl->addStretch();
    scroll->setWidget(content);
    outer->addWidget(scroll, 1);

    m_status = new QLabel;
    m_status->setObjectName("footer");
    m_status->setWordWrap(true);
    outer->addWidget(m_status);

    connect(m_macro, &MacroIndicators::updated, this, &IndicatorPanel::refresh);
    refresh();
}

void IndicatorPanel::setActive(int id)
{
    for (IndicatorItem *item : std::as_const(m_items))
        item->setActive(item->id() == id);
}

void IndicatorPanel::refresh()
{
    for (IndicatorItem *item : std::as_const(m_items))
        item->refresh();
    WorldEconomy *economy = m_macro->economy();
    if (!economy->hasData())
        m_status->setText(economy->errorString().isEmpty() ? tr("Loading IMF data…")
                                                           : tr("IMF data unavailable: %1").arg(economy->errorString()));
    else
        m_status->setText(tr("IMF World Economic Outlook and balance of payments · BIS policy rates"));
}

// ---------------------------------------------------------------- IndicatorItem

IndicatorItem::IndicatorItem(MacroIndicators *macro, MacroIndicators::Id id, QWidget *parent)
    : QWidget(parent)
    , m_macro(macro)
    , m_id(id)
{
    setFixedHeight(78);
    setCursor(Qt::PointingHandCursor);
    setToolTip(MacroIndicators::info(id).description);
    refresh();
}

void IndicatorItem::setActive(bool active)
{
    m_active = active;
    update();
}

void IndicatorItem::refresh()
{
    const QHash<QString, double> values = m_macro->values(m_id, m_macro->defaultYear(m_id));
    m_values = values.values().toVector();
    std::sort(m_values.begin(), m_values.end());
    m_median = m_values.isEmpty() ? 0 : m_values[m_values.size() / 2];

    // Histogram over the color range; values beyond it fall in the end bins.
    constexpr int Bins = 28;
    const MacroIndicators::Range r = m_macro->range(m_id, values);
    const bool log = MacroIndicators::info(m_id).scale == MacroIndicators::Scale::Log;
    m_bins.fill(0, Bins);
    m_binColors.clear();
    for (double v : std::as_const(m_values)) {
        const double x = log ? std::log10(std::max(v, 1e-9)) : v;
        const int b = std::clamp(int((x - r.lo) / (r.hi - r.lo) * Bins), 0, Bins - 1);
        ++m_bins[b];
    }
    for (int b = 0; b < Bins; ++b) {
        const double x = r.lo + (r.hi - r.lo) * (b + 0.5) / Bins;
        m_binColors.push_back(m_macro->color(m_id, log ? std::pow(10.0, x) : x, r));
    }
    update();
}

void IndicatorItem::paintEvent(QPaintEvent *)
{
    const Palette &pal = Theme::current();
    const MacroIndicators::Info info = MacroIndicators::info(m_id);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);

    p.setPen(QPen(m_active ? pal.Accent : pal.PanelLine, 1));
    p.setBrush(m_active ? pal.Active : (m_hover ? pal.Hover : Qt::transparent));
    p.drawRoundedRect(r, 8, 8);

    QFont f = font();
    f.setPointSizeF(12.5);
    f.setWeight(QFont::Medium);
    p.setFont(f);
    p.setPen(pal.Text);
    const QRectF nameRect(12, 8, width() - 110, 18);
    p.drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter,
               p.fontMetrics().elidedText(info.name, Qt::ElideRight, int(nameRect.width())));
    p.drawText(QRectF(0, 8, width() - 12, 18), Qt::AlignRight | Qt::AlignVCenter,
               m_values.isEmpty() ? QString::fromUtf8("—") : m_macro->format(m_id, m_median));

    f.setWeight(QFont::Normal);
    f.setPointSizeF(10.5);
    p.setFont(f);
    p.setPen(pal.TextDim);
    const QString when = info.yearly ? QString::number(m_macro->defaultYear(m_id)) : tr("Latest");
    p.drawText(QRectF(12, 26, width() - 24, 16), Qt::AlignLeft | Qt::AlignVCenter,
               tr("%1 · %2 countries").arg(when).arg(m_values.size()));
    p.drawText(QRectF(0, 26, width() - 12, 16), Qt::AlignRight | Qt::AlignVCenter, tr("median"));

    // Histogram of country values.
    const QRectF hist(12, 48, width() - 24, 20);
    const int maxBin = m_bins.isEmpty() ? 0 : *std::max_element(m_bins.begin(), m_bins.end());
    if (maxBin > 0) {
        const double w = hist.width() / m_bins.size();
        p.setPen(Qt::NoPen);
        for (int b = 0; b < m_bins.size(); ++b) {
            const double h = m_bins[b] ? std::max(2.0, hist.height() * m_bins[b] / maxBin) : 1.0;
            QColor c = m_binColors[b];
            if (!m_bins[b])
                c.setAlpha(80);
            p.setBrush(c);
            p.drawRoundedRect(QRectF(hist.left() + b * w + 0.5, hist.bottom() - h, w - 1, h), 1, 1);
        }
    }
}

void IndicatorItem::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && rect().contains(e->position().toPoint()))
        emit clicked(m_id);
}

void IndicatorItem::enterEvent(QEnterEvent *)
{
    m_hover = true;
    update();
}

void IndicatorItem::leaveEvent(QEvent *)
{
    m_hover = false;
    update();
}

// ---------------------------------------------------------------- RankingList

namespace {
constexpr int RankRowHeight = 26;
}

RankingList::RankingList(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
}

void RankingList::setRows(const QVector<Row> &rows, bool diverging, double scaleMax)
{
    m_rows = rows;
    m_diverging = diverging;
    m_maxAbs = std::max(scaleMax, 1e-9);
    m_hover = -1;
    setFixedHeight(int(rows.size()) * RankRowHeight);
    update();
}

int RankingList::rowAt(const QPointF &pos) const
{
    const int i = int(pos.y()) / RankRowHeight;
    return i >= 0 && i < m_rows.size() ? i : -1;
}

void RankingList::paintEvent(QPaintEvent *e)
{
    const Palette &pal = Theme::current();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QFont f = font();
    f.setPointSizeF(11.5);
    p.setFont(f);
    const QFontMetrics fm(f);

    const double nameLeft = 32;
    const double nameWidth = width() * 0.40;
    const double valueWidth = 64;
    const QRectF barArea(nameLeft + nameWidth + 6, 0, width() - nameLeft - nameWidth - valueWidth - 12, 0);
    const double zeroX = m_diverging ? barArea.center().x() : barArea.left();
    const double scale = (m_diverging ? barArea.width() / 2 : barArea.width()) / m_maxAbs;

    const int first = std::max(0, e->rect().top() / RankRowHeight);
    const int last = std::min(int(m_rows.size()) - 1, e->rect().bottom() / RankRowHeight);
    for (int i = first; i <= last; ++i) {
        const Row &row = m_rows[i];
        const QRectF r(0, i * RankRowHeight, width(), RankRowHeight);
        if (i == m_hover) {
            p.setPen(Qt::NoPen);
            p.setBrush(pal.Hover);
            p.drawRoundedRect(r.adjusted(0, 1, 0, -1), 5, 5);
        }
        p.setPen(pal.TextFaint);
        p.drawText(QRectF(4, r.top(), nameLeft - 8, r.height()), Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(i + 1));
        p.setPen(pal.Text);
        p.drawText(QRectF(nameLeft, r.top(), nameWidth, r.height()), Qt::AlignLeft | Qt::AlignVCenter,
                   fm.elidedText(row.name, Qt::ElideRight, int(nameWidth)));

        // Bar, clipped to the area when the value is off the scale.
        const double len = std::clamp(row.value * scale, -(zeroX - barArea.left()), barArea.right() - zeroX);
        const QRectF bar = QRectF(QPointF(zeroX, r.center().y() - 4), QPointF(zeroX + len, r.center().y() + 4)).normalized();
        p.setPen(Qt::NoPen);
        p.setBrush(row.color);
        p.drawRoundedRect(bar.adjusted(0, 0, len == 0 ? 1 : 0, 0), 2, 2);

        p.setPen(pal.Text);
        p.drawText(QRectF(width() - valueWidth - 4, r.top(), valueWidth, r.height()),
                   Qt::AlignRight | Qt::AlignVCenter, row.text);
    }
    if (m_diverging && !m_rows.isEmpty()) {
        p.setPen(QPen(pal.PanelLine, 1));
        p.drawLine(QPointF(zeroX, first * RankRowHeight), QPointF(zeroX, (last + 1) * RankRowHeight));
    }
}

void RankingList::mouseMoveEvent(QMouseEvent *e)
{
    const int i = rowAt(e->position());
    if (i != m_hover) {
        m_hover = i;
        update();
    }
}

void RankingList::mouseReleaseEvent(QMouseEvent *e)
{
    const int i = rowAt(e->position());
    if (e->button() == Qt::LeftButton && i >= 0)
        emit countryClicked(m_rows[i].iso3);
}

void RankingList::leaveEvent(QEvent *)
{
    m_hover = -1;
    update();
}

// ---------------------------------------------------------------- AnnualChart

AnnualChart::AnnualChart(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setMinimumHeight(120);
}

void AnnualChart::setSeries(const QMap<int, double> &values, int firstProjected, const QColor &positive,
                            const QColor &negative, const std::function<QString(double)> &format)
{
    m_values = values;
    m_firstProjected = firstProjected;
    m_positive = positive;
    m_negative = negative;
    m_format = format;
    update();
}

QRectF AnnualChart::plotRect() const
{
    return QRectF(rect()).adjusted(2, 18, -2, -18);
}

int AnnualChart::yearAt(const QPointF &pos) const
{
    if (m_values.isEmpty())
        return 0;
    const QRectF plot = plotRect();
    const int n = m_values.lastKey() - m_values.firstKey() + 1;
    const int i = int((pos.x() - plot.left()) / plot.width() * n);
    return i >= 0 && i < n ? m_values.firstKey() + i : 0;
}

void AnnualChart::paintEvent(QPaintEvent *)
{
    if (m_values.isEmpty())
        return;
    const Palette &pal = Theme::current();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF plot = plotRect();
    double lo = 0, hi = 0;
    double maxValue = m_values.first();
    for (double v : m_values) {
        lo = std::min(lo, v);
        hi = std::max(hi, v);
        maxValue = std::max(maxValue, v);
    }
    if (hi - lo < 1e-9)
        hi = lo + 1;
    auto y = [&](double v) { return plot.bottom() - (v - lo) / (hi - lo) * plot.height(); };
    const int first = m_values.firstKey();
    const int n = m_values.lastKey() - first + 1;
    const double slot = plot.width() / n;

    p.setPen(Qt::NoPen);
    for (auto it = m_values.begin(); it != m_values.end(); ++it) {
        QColor c = it.value() >= 0 ? m_positive : m_negative;
        if (it.key() >= m_firstProjected)
            c.setAlphaF(0.4f);
        p.setBrush(c);
        const double x = plot.left() + (it.key() - first) * slot;
        p.drawRect(QRectF(QPointF(x + slot * 0.15, y(std::max(0.0, it.value()))),
                          QPointF(x + slot * 0.85, y(std::min(0.0, it.value())))));
    }
    p.setPen(QPen(pal.PanelLine, 1));
    p.drawLine(QPointF(plot.left(), y(0)), QPointF(plot.right(), y(0)));

    QFont f = font();
    f.setPointSizeF(10);
    p.setFont(f);
    p.setPen(pal.TextDim);
    p.drawText(QRectF(plot.left(), plot.bottom() + 2, 60, 16), Qt::AlignLeft | Qt::AlignVCenter,
               QString::number(first));
    p.drawText(QRectF(plot.right() - 60, plot.bottom() + 2, 60, 16), Qt::AlignRight | Qt::AlignVCenter,
               QString::number(m_values.lastKey()));
    if (m_firstProjected > first && m_firstProjected <= m_values.lastKey()) {
        const double x = plot.left() + (m_firstProjected - first) * slot;
        p.setPen(QPen(pal.TextFaint, 1, Qt::DashLine));
        p.drawLine(QPointF(x, plot.top() - 4), QPointF(x, plot.bottom()));
        p.setPen(pal.TextDim);
        p.drawText(QRectF(x + 4, 0, 120, 16), Qt::AlignLeft | Qt::AlignVCenter, tr("IMF projections"));
    }
    p.drawText(QRectF(plot.left(), 0, plot.width() / 2, 16), Qt::AlignLeft | Qt::AlignVCenter,
               tr("max %1").arg(m_format(maxValue)));
}

void AnnualChart::mouseMoveEvent(QMouseEvent *e)
{
    const int year = yearAt(e->position());
    if (year && m_values.contains(year))
        QToolTip::showText(e->globalPosition().toPoint(),
                           QString("%1: %2").arg(year).arg(m_format(m_values[year])), this);
    else
        QToolTip::hideText();
}

// ---------------------------------------------------------------- SectorChart

SectorChart::SectorChart(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
}

void SectorChart::setBars(const QVector<Bar> &bars)
{
    m_bars = bars;
    setFixedHeight(int(bars.size()) * RowHeight - 6);
    update();
}

QColor SectorChart::color(int sector)
{
    // Categorical: green for the land, amber for industry, blue for services, darker on light themes.
    static const char *const dark[] = {"#8cc37a", "#e3a857", "#7fa8e6"};
    static const char *const light[] = {"#3f8a3a", "#b8691c", "#3567b0"};
    return QColor((Theme::isLight(Theme::current()) ? light : dark)[std::clamp(sector, 0, 2)]);
}

QRectF SectorChart::barRect(int bar) const
{
    return QRectF(0, bar * RowHeight + 16, width(), 16);
}

QVector<QPair<double, double>> SectorChart::segments(int bar) const
{
    const Bar &b = m_bars[bar];
    double total = 0;
    for (double s : b.shares)
        if (std::isfinite(s) && s > 0)
            total += s;
    total = std::max(total, 100.0);   // shares of GDP can add up to more than 100 (net subsidies)
    const QRectF r = barRect(bar);
    QVector<QPair<double, double>> out;
    double x = r.left();
    for (double s : b.shares) {
        const double w = std::isfinite(s) && s > 0 ? s / total * r.width() : 0;
        out.push_back({x, x + w});
        x += w;
    }
    return out;
}

void SectorChart::paintEvent(QPaintEvent *)
{
    const Palette &pal = Theme::current();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QFont f = font();
    f.setPointSizeF(10);
    p.setFont(f);
    const QFontMetricsF fm(f);
    const QLocale loc;
    for (int i = 0; i < m_bars.size(); ++i) {
        const QRectF r = barRect(i);
        p.setPen(pal.TextDim);
        p.drawText(QRectF(0, r.top() - 16, width(), 14), Qt::AlignLeft | Qt::AlignVCenter, m_bars[i].label);

        QPainterPath clip;
        clip.addRoundedRect(r, 3, 3);
        p.save();
        p.setClipPath(clip);
        p.fillRect(r, pal.PanelLine);   // what no sector accounts for
        const auto seg = segments(i);
        for (int k = 0; k < 3; ++k) {
            const QRectF s(QPointF(seg[k].first, r.top()), QPointF(seg[k].second, r.bottom()));
            if (s.width() <= 0)
                continue;
            p.fillRect(s, color(k));
            // A hairline between segments.
            if (k > 0)
                p.fillRect(QRectF(s.left(), r.top(), 1, r.height()), pal.Panel);
            const QString text = loc.toString(m_bars[i].shares[k], 'f', m_bars[i].shares[k] < 10 ? 1 : 0) + "%";
            if (fm.horizontalAdvance(text) + 8 <= s.width()) {
                p.setPen(Theme::isLight(pal) ? QColor(Qt::white) : pal.Background);
                p.drawText(s, Qt::AlignCenter, text);
            }
        }
        p.restore();
    }
}

void SectorChart::mouseMoveEvent(QMouseEvent *e)
{
    const QPointF pos = e->position();
    for (int i = 0; i < m_bars.size(); ++i) {
        if (!barRect(i).adjusted(0, -2, 0, 2).contains(pos))
            continue;
        const auto seg = segments(i);
        for (int k = 0; k < 3; ++k)
            if (pos.x() >= seg[k].first && pos.x() < seg[k].second && !m_bars[i].tips[k].isEmpty()) {
                QToolTip::showText(e->globalPosition().toPoint(), m_bars[i].tips[k], this);
                return;
            }
    }
    QToolTip::hideText();
}

void SectorChart::leaveEvent(QEvent *)
{
    QToolTip::hideText();
}

// ---------------------------------------------------------------- SectorHistoryChart

SectorHistoryChart::SectorHistoryChart(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setFixedHeight(150);
}

void SectorHistoryChart::setData(const QMap<int, double> (&shares)[3], const QStringList &names,
                                 const QVector<Event> &events)
{
    m_first = m_last = 0;
    double top = 0;
    for (int k = 0; k < 3; ++k) {
        m_shares[k] = shares[k];
        if (shares[k].isEmpty())
            continue;
        m_first = m_first ? std::min(m_first, shares[k].firstKey()) : shares[k].firstKey();
        m_last = std::max(m_last, shares[k].lastKey());
        for (double v : shares[k])
            top = std::max(top, v);
    }
    m_top = std::max(25.0, std::ceil(top / 25) * 25);
    m_names = names;
    m_events = events;
    update();
}

QRectF SectorHistoryChart::plotRect() const
{
    return QRectF(rect()).adjusted(36, 18, -4, -16);
}

double SectorHistoryChart::xOf(double year) const
{
    const QRectF plot = plotRect();
    return m_last > m_first ? plot.left() + (year - m_first) / (m_last - m_first) * plot.width() : plot.center().x();
}

void SectorHistoryChart::paintEvent(QPaintEvent *)
{
    if (!m_first)
        return;
    const Palette &pal = Theme::current();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF plot = plotRect();
    auto y = [&](double v) { return plot.bottom() - v / m_top * plot.height(); };
    QFont f = font();
    f.setPixelSize(10);
    p.setFont(f);

    // Periods as bands, single years as lines, numbered at the top.
    for (const Event &e : m_events) {
        const int to = e.to ? e.to : e.from;
        if (to < m_first || e.from > m_last)
            continue;
        const double x0 = xOf(std::max(e.from, m_first)), x1 = xOf(std::min(to, m_last));
        if (e.to) {
            QColor band = pal.TextFaint;
            band.setAlphaF(0.12f);
            p.fillRect(QRectF(QPointF(x0, plot.top()), QPointF(std::max(x1, x0 + 2), plot.bottom())), band);
        } else {
            p.setPen(QPen(pal.TextFaint, 1, Qt::DashLine));
            p.drawLine(QPointF(x0, plot.top()), QPointF(x0, plot.bottom()));
        }
        p.setPen(pal.TextDim);
        p.drawText(QRectF((x0 + std::max(x1, x0)) / 2 - 10, 0, 20, 16), Qt::AlignCenter, QString(QChar(0x2460 + e.number - 1)));
    }

    // Grid and its labels.
    for (double v = 0; v <= m_top + 0.1; v += 25) {
        p.setPen(QPen(pal.PanelLine, 1));
        p.drawLine(QPointF(plot.left(), y(v)), QPointF(plot.right(), y(v)));
        p.setPen(pal.TextDim);
        p.drawText(QRectF(0, y(v) - 7, plot.left() - 4, 14), Qt::AlignRight | Qt::AlignVCenter,
                   tr("%1%").arg(int(v)));
    }
    const int step = m_last - m_first > 40 ? 20 : 10;
    for (int yr = (m_first + step - 1) / step * step; yr <= m_last; yr += step) {
        if (xOf(yr) - plot.left() < 40 || plot.right() - xOf(yr) < 40)   // clear of the first and last years
            continue;
        p.drawText(QRectF(xOf(yr) - 20, plot.bottom() + 2, 40, 14), Qt::AlignCenter, QString::number(yr));
    }
    p.drawText(QRectF(plot.left(), plot.bottom() + 2, 40, 14), Qt::AlignLeft | Qt::AlignVCenter,
               QString::number(m_first));
    p.drawText(QRectF(plot.right() - 40, plot.bottom() + 2, 40, 14), Qt::AlignRight | Qt::AlignVCenter,
               QString::number(m_last));

    // The lines; a gap in the data breaks them.
    for (int k = 0; k < 3; ++k) {
        QPainterPath path;
        int prev = 0;
        for (auto it = m_shares[k].begin(); it != m_shares[k].end(); ++it) {
            const QPointF pt(xOf(it.key()), y(it.value()));
            if (prev && it.key() == prev + 1)
                path.lineTo(pt);
            else
                path.moveTo(pt);
            prev = it.key();
        }
        p.setPen(QPen(SectorChart::color(k), 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
    }
    if (m_hoverYear) {
        p.setPen(QPen(pal.TextDim, 1));
        p.drawLine(QPointF(xOf(m_hoverYear), plot.top()), QPointF(xOf(m_hoverYear), plot.bottom()));
        for (int k = 0; k < 3; ++k)
            if (m_shares[k].contains(m_hoverYear)) {
                p.setPen(Qt::NoPen);
                p.setBrush(SectorChart::color(k));
                p.drawEllipse(QPointF(xOf(m_hoverYear), y(m_shares[k][m_hoverYear])), 3, 3);
            }
    }
}

void SectorHistoryChart::mouseMoveEvent(QMouseEvent *e)
{
    const QRectF plot = plotRect();
    int year = 0;
    if (m_first && plot.adjusted(-4, -18, 4, 0).contains(e->position()) && m_last > m_first)
        year = std::clamp(int(std::lround(m_first + (e->position().x() - plot.left()) / plot.width() * (m_last - m_first))),
                          m_first, m_last);
    if (year != m_hoverYear) {
        m_hoverYear = year;
        update();
    }
    if (!year) {
        QToolTip::hideText();
        return;
    }
    const QLocale loc;
    QStringList lines {QString::number(year)};
    for (int k = 0; k < 3; ++k)
        if (m_shares[k].contains(year))
            lines << tr("%1: %2% of GDP").arg(m_names.value(k), loc.toString(m_shares[k][year], 'f', 1));
    QToolTip::showText(e->globalPosition().toPoint(), lines.join('\n'), this);
}

void SectorHistoryChart::leaveEvent(QEvent *)
{
    m_hoverYear = 0;
    update();
    QToolTip::hideText();
}

// ---------------------------------------------------------------- TimeSeriesChart

TimeSeriesChart::TimeSeriesChart(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setFixedHeight(130);
}

void TimeSeriesChart::setLines(const QVector<Line> &lines, const std::function<QString(double)> &format, double reference)
{
    m_lines.clear();
    m_first = m_last = 0;
    double lo = std::isfinite(reference) ? reference : qInf(), hi = std::isfinite(reference) ? reference : -qInf();
    for (const Line &line : lines) {
        if (line.values.isEmpty())
            continue;
        m_lines.push_back(line);
        m_first = m_first ? std::min(m_first, line.values.firstKey()) : line.values.firstKey();
        m_last = std::max(m_last, line.values.lastKey());
        for (double v : line.values) {
            lo = std::min(lo, v);
            hi = std::max(hi, v);
        }
    }
    const double pad = std::max(1.0, (hi - lo) * 0.08);
    m_lo = lo - pad;
    m_hi = hi + pad;
    m_format = format;
    m_reference = reference;
    update();
}

QRectF TimeSeriesChart::plotRect() const
{
    return QRectF(rect()).adjusted(40, 8, -4, -16);
}

double TimeSeriesChart::xOf(double t) const
{
    const QRectF plot = plotRect();
    return m_last > m_first ? plot.left() + (t - m_first) / (m_last - m_first) * plot.width() : plot.center().x();
}

double TimeSeriesChart::yOf(double v) const
{
    const QRectF plot = plotRect();
    return plot.bottom() - (v - m_lo) / (m_hi - m_lo) * plot.height();
}

void TimeSeriesChart::paintEvent(QPaintEvent *)
{
    if (m_lines.isEmpty())
        return;
    const Palette &pal = Theme::current();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF plot = plotRect();
    QFont f = font();
    f.setPixelSize(10);
    p.setFont(f);

    // Three value labels (bottom, middle, top) and their grid lines.
    for (int i = 0; i < 3; ++i) {
        const double v = m_lo + (m_hi - m_lo) * (0.1 + 0.4 * i);
        p.setPen(QPen(pal.PanelLine, 1));
        p.drawLine(QPointF(plot.left(), yOf(v)), QPointF(plot.right(), yOf(v)));
        p.setPen(pal.TextDim);
        p.drawText(QRectF(0, yOf(v) - 7, plot.left() - 4, 14), Qt::AlignRight | Qt::AlignVCenter, m_format(v));
    }
    if (std::isfinite(m_reference)) {
        p.setPen(QPen(pal.TextFaint, 1, Qt::DashLine));
        p.drawLine(QPointF(plot.left(), yOf(m_reference)), QPointF(plot.right(), yOf(m_reference)));
    }
    p.setPen(pal.TextDim);
    const int first = int(m_first), last = int(m_last);
    const int step = last - first > 30 ? 10 : 5;
    for (int yr = (first + step - 1) / step * step; yr <= last; yr += step)
        if (xOf(yr) - plot.left() >= 40 && plot.right() - xOf(yr) >= 40)
            p.drawText(QRectF(xOf(yr) - 20, plot.bottom() + 2, 40, 14), Qt::AlignCenter, QString::number(yr));
    p.drawText(QRectF(plot.left(), plot.bottom() + 2, 40, 14), Qt::AlignLeft | Qt::AlignVCenter, QString::number(first));
    p.drawText(QRectF(plot.right() - 40, plot.bottom() + 2, 40, 14), Qt::AlignRight | Qt::AlignVCenter,
               QString::number(last));

    // The lines; a gap of more than a year breaks them.
    for (const Line &line : m_lines) {
        QPainterPath path;
        double prev = -1;
        for (auto it = line.values.begin(); it != line.values.end(); ++it) {
            const QPointF pt(xOf(it.key()), yOf(it.value()));
            if (prev >= 0 && it.key() - prev <= 1.01)
                path.lineTo(pt);
            else
                path.moveTo(pt);
            prev = it.key();
        }
        p.setPen(QPen(line.color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
    }
    if (m_hover) {
        p.setPen(QPen(pal.TextDim, 1));
        p.drawLine(QPointF(xOf(m_hover), plot.top()), QPointF(xOf(m_hover), plot.bottom()));
    }
}

void TimeSeriesChart::mouseMoveEvent(QMouseEvent *e)
{
    const QRectF plot = plotRect();
    double t = 0;
    if (!m_lines.isEmpty() && m_last > m_first && plot.adjusted(-4, 0, 4, 0).contains(e->position()))
        t = std::clamp(m_first + (e->position().x() - plot.left()) / plot.width() * (m_last - m_first), m_first, m_last);
    m_hover = t;
    update();
    if (!t) {
        QToolTip::hideText();
        return;
    }
    // Each line's latest value at or before the hovered time (within a year).
    QStringList rows;
    double shown = 0;
    for (const Line &line : m_lines) {
        auto it = line.values.upperBound(t + 1e-6);
        if (it == line.values.constBegin())
            continue;
        --it;
        if (t - it.key() > 1.01)
            continue;
        shown = std::max(shown, it.key());
        rows << QString("%1: %2").arg(line.name, m_format(it.value()));
    }
    if (rows.isEmpty()) {
        QToolTip::hideText();
        return;
    }
    const int year = int(shown);
    const int quarter = int(std::floor((shown - year) * 4)) + 1;   // quarters are mid-quarter
    const bool quarterly = std::any_of(m_lines.begin(), m_lines.end(), [](const Line &l) {
        return std::any_of(l.values.keyBegin(), l.values.keyEnd(), [](double k) { return k != std::floor(k); });
    });
    rows.prepend(quarterly ? tr("%1 Q%2").arg(year).arg(quarter) : QString::number(year));
    QToolTip::showText(e->globalPosition().toPoint(), rows.join('\n'), this);
}

void TimeSeriesChart::leaveEvent(QEvent *)
{
    m_hover = 0;
    update();
    QToolTip::hideText();
}

QString AnnualChart::aiRead() const
{
    QStringList labels;
    QVector<double> values;
    for (auto it = m_values.begin(); it != m_values.end(); ++it) {
        labels << QString::number(it.key());
        values << it.value();
    }
    QString out = aiSeries(QStringLiteral("Annual"), labels, values);
    if (m_firstProjected > 0 && !out.isEmpty()) out += QString(" (from %1: projections)").arg(m_firstProjected);
    return out;
}

QString SectorChart::aiRead() const
{
    QStringList out;
    for (const Bar &b : m_bars)
        out << QString("%1: primary %2 %, secondary %3 %, tertiary %4 %").arg(b.label, aiNumber(b.shares[0]), aiNumber(b.shares[1]), aiNumber(b.shares[2]));
    return out.join('\n');
}

QString SectorHistoryChart::aiRead() const
{
    QStringList out;
    for (int k = 0; k < 3 && k < m_names.size(); ++k) {
        QStringList labels;
        QVector<double> values;
        for (auto it = m_shares[k].begin(); it != m_shares[k].end(); ++it) {
            labels << QString::number(it.key());
            values << it.value();
        }
        if (const QString line = aiSeries(m_names[k] + " (%)", labels, values, 10); !line.isEmpty()) out << line;
    }
    return out.join('\n');
}

QString TimeSeriesChart::aiRead() const
{
    QStringList out;
    for (const Line &l : m_lines) {
        QStringList labels;
        QVector<double> values;
        for (auto it = l.values.begin(); it != l.values.end(); ++it) {
            labels << QString::number(it.key(), 'f', 2);
            values << it.value();
        }
        if (const QString line = aiSeries(l.name, labels, values); !line.isEmpty()) out << line;
    }
    return out.join('\n');
}
