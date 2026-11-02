#include "ChokepointsPage.h"

#include "MarketData.h"
#include "Theme.h"
#include "VesselStore.h"

#include <QGridLayout>
#include <QLabel>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {
constexpr qint64 WindowMs = qint64(ChokepointMonitor::WindowHours) * 3600 * 1000;
}

void drawHourlyChart(QPainter &p, const QRectF &rect, const QVector<ChokepointMonitor::Counts> &hours,
                     const QVector<bool> &recorded, const QColor &tankerColor, const QColor &cargoColor)
{
    // Shade hours with no data first, merging consecutive ones into bands.
    const double slotW = rect.width() / std::max<qsizetype>(1, hours.size());
    QColor gap = Theme::current().TextFaint;
    gap.setAlpha(38);
    p.setPen(Qt::NoPen);
    p.setBrush(gap);
    for (int i = 0; i < recorded.size();) {
        if (recorded[i]) { ++i; continue; }
        int j = i;
        while (j < recorded.size() && !recorded[j]) ++j;
        p.drawRect(QRectF(rect.left() + i * slotW, rect.top(), (j - i) * slotW, rect.height()));
        i = j;
    }

    int peak = 1;
    for (const auto &h : hours)
        peak = std::max(peak, h.total());

    // Baseline
    p.setPen(QPen(Theme::current().PanelLine, 1));
    p.drawLine(rect.bottomLeft(), rect.bottomRight());

    const double slot = rect.width() / std::max<qsizetype>(1, hours.size());
    const double barW = std::max(1.5, slot * 0.68);
    p.setPen(Qt::NoPen);
    for (int i = 0; i < hours.size(); ++i) {
        const double x = rect.left() + i * slot + (slot - barW) / 2;
        const double hT = rect.height() * hours[i].tankers / peak;
        const double hC = rect.height() * hours[i].cargo / peak;
        p.setBrush(tankerColor);
        p.drawRect(QRectF(x, rect.bottom() - hT, barW, hT));
        p.setBrush(cargoColor);
        p.drawRect(QRectF(x, rect.bottom() - hT - hC, barW, hC));
    }
}

void drawDailyChart(QPainter &p, const QRectF &rect, const QVector<PortWatch::Day> &days,
                    const QColor &tankerColor, const QColor &bulkColor, const QColor &otherColor)
{
    int peak = 1;
    for (const auto &d : days)
        peak = std::max(peak, d.total);
    p.setPen(QPen(Theme::current().PanelLine, 1));
    p.drawLine(rect.bottomLeft(), rect.bottomRight());
    if (days.isEmpty())
        return;
    const double slot = rect.width() / days.size();
    const double barW = std::max(1.0, slot * 0.72);
    p.setPen(Qt::NoPen);
    for (int i = 0; i < days.size(); ++i) {
        const PortWatch::Day &d = days[i];
        const double x = rect.left() + i * slot + (slot - barW) / 2;
        double y = rect.bottom();
        const int other = std::max(0, d.total - d.tanker - d.dryBulk);
        for (const auto &[n, color] : {std::pair(d.tanker, tankerColor), std::pair(d.dryBulk, bulkColor),
                                       std::pair(other, otherColor)}) {
            const double h = rect.height() * n / peak;
            p.setBrush(color);
            p.drawRect(QRectF(x, y - h, barW, h));
            y -= h;
        }
    }
}

PortWatchSummary summarize(const PortWatch *portWatch, const QString &portId)
{
    PortWatchSummary s;
    const QVector<PortWatch::Day> days = portWatch->days(portId);
    if (days.isEmpty())
        return s;
    s.valid = true;
    s.latest = days.last();
    s.avg7 = portWatch->average(portId, 7).total;
    s.avg90 = portWatch->average(portId, 90).total;
    s.changePct = s.avg90 > 0 ? (s.avg7 - s.avg90) / s.avg90 * 100 : 0;
    return s;
}

// ---------------------------------------------------------------- DailyChart

DailyChart::DailyChart(MarketData *data, QWidget *parent)
    : QWidget(parent)
    , m_data(data)
{
    setMinimumHeight(90);
}

void DailyChart::setDays(const QVector<PortWatch::Day> &days)
{
    m_days = days;
    update();
}

void DailyChart::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QColor other = Theme::current().TextFaint;
    other.setAlpha(150);
    drawDailyChart(p, QRectF(rect()).adjusted(0, 4, 0, -18), m_days, m_data->categoryColor("energy"),
                   m_data->categoryColor("industrial"), other);
    QFont f = font();
    f.setPointSizeF(10);
    p.setFont(f);
    p.setPen(Theme::current().TextDim);
    if (!m_days.isEmpty()) {
        const QLocale loc;
        p.drawText(QRectF(0, height() - 16, width(), 16), Qt::AlignLeft | Qt::AlignVCenter,
                   loc.toString(m_days.first().date, QLocale::ShortFormat));
        p.drawText(QRectF(0, height() - 16, width(), 16), Qt::AlignRight | Qt::AlignVCenter,
                   loc.toString(m_days.last().date, QLocale::ShortFormat));
    }
}

// ---------------------------------------------------------------- HourlyChart

HourlyChart::HourlyChart(MarketData *data, QWidget *parent)
    : QWidget(parent)
    , m_data(data)
{
    setMinimumHeight(90);
}

void HourlyChart::setHours(const QVector<ChokepointMonitor::Counts> &hours, const QVector<bool> &recorded)
{
    m_hours = hours;
    m_recorded = recorded;
    update();
}

void HourlyChart::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF chart = QRectF(rect()).adjusted(0, 4, 0, -18);
    drawHourlyChart(p, chart, m_hours, m_recorded, m_data->categoryColor("energy"),
                    m_data->categoryColor("industrial"));
    QFont f = font();
    f.setPointSizeF(10);
    p.setFont(f);
    p.setPen(Theme::current().TextDim);
    p.drawText(QRectF(0, height() - 16, width(), 16), Qt::AlignLeft | Qt::AlignVCenter, tr("−48 h"));
    p.drawText(QRectF(0, height() - 16, width(), 16), Qt::AlignRight | Qt::AlignVCenter, tr("now"));
}

// ---------------------------------------------------------------- ChokepointCard

ChokepointCard::ChokepointCard(ChokepointMonitor *monitor, PortWatch *portWatch, MarketData *data, int zone,
                               QWidget *parent)
    : QWidget(parent)
    , m_monitor(monitor)
    , m_portWatch(portWatch)
    , m_data(data)
    , m_zone(zone)
{
    setMinimumSize(280, 368);
    setCursor(Qt::PointingHandCursor);
}

void ChokepointCard::enterEvent(QEnterEvent *)
{
    m_hover = true;
    update();
}

void ChokepointCard::leaveEvent(QEvent *)
{
    m_hover = false;
    update();
}

void ChokepointCard::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && rect().contains(e->position().toPoint()))
        emit clicked(m_zone);
}

void ChokepointCard::paintEvent(QPaintEvent *)
{
    const Palette &ui = Theme::current();
    const QColor tanker = m_data->categoryColor("energy");
    const QColor cargo = m_data->categoryColor("industrial");
    const QLocale loc;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(m_hover ? ui.ControlBorderHover : ui.ControlBorder, 1));
    p.setBrush(m_hover ? ui.Hover : ui.Panel);
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 10, 10);

    const double pad = 16;
    const double w = width() - 2 * pad;
    QFont f = font();

    f.setPointSizeF(14);
    f.setWeight(QFont::DemiBold);
    p.setFont(f);
    p.setPen(ui.Text);
    p.drawText(QRectF(pad, 12, w, 22), Qt::AlignLeft | Qt::AlignVCenter, m_monitor->name(m_zone));

    f.setPointSizeF(11);
    f.setWeight(QFont::Normal);
    p.setFont(f);
    p.setPen(ui.TextDim);
    p.drawText(QRectF(pad, 34, w, 18), Qt::AlignLeft | Qt::AlignVCenter,
               p.fontMetrics().elidedText(m_monitor->description(m_zone), Qt::ElideRight, int(w)));

    // Two stat columns: ships in the zone now, and passages over the window.
    const ChokepointMonitor::Counts now = m_monitor->inZoneNow(m_zone);
    const ChokepointMonitor::Counts day = m_monitor->passages(m_zone, WindowMs);
    auto stat = [&](double x, const QString &label, const ChokepointMonitor::Counts &c) {
        QFont lf = font();
        lf.setPointSizeF(9.5);
        lf.setWeight(QFont::DemiBold);
        lf.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
        p.setFont(lf);
        p.setPen(ui.TextSection);
        p.drawText(QRectF(x, 62, w / 2, 14), Qt::AlignLeft | Qt::AlignVCenter, label.toUpper());
        QFont nf = font();
        nf.setPointSizeF(24);
        nf.setWeight(QFont::DemiBold);
        p.setFont(nf);
        p.setPen(ui.Text);
        p.drawText(QRectF(x, 78, w / 2, 32), Qt::AlignLeft | Qt::AlignVCenter, loc.toString(c.total()));
        QFont sf = font();
        sf.setPointSizeF(10.5);
        p.setFont(sf);
        const QString t = loc.toString(c.tankers), g = loc.toString(c.cargo);
        p.setPen(tanker);
        p.drawText(QRectF(x, 110, w / 2, 16), Qt::AlignLeft | Qt::AlignVCenter, QString::fromUtf8("▲ ") + t);
        const double off = p.fontMetrics().horizontalAdvance(QString::fromUtf8("▲ ") + t) + 10;
        p.setPen(cargo);
        p.drawText(QRectF(x + off, 110, w / 2, 16), Qt::AlignLeft | Qt::AlignVCenter, QString::fromUtf8("▲ ") + g);
        if (c.unknown > 0) {
            const double off2 = off + p.fontMetrics().horizontalAdvance(QString::fromUtf8("▲ ") + g) + 10;
            p.setPen(ui.TextFaint);
            p.drawText(QRectF(x + off2, 110, w / 2 - off2, 16), Qt::AlignLeft | Qt::AlignVCenter,
                       tr("+%1 unidentified").arg(loc.toString(c.unknown)));
        }
    };
    stat(pad, tr("In zone now"), now);
    stat(pad + w / 2, tr("Passages · 48 h"), day);

    if (!m_monitor->hasCoverage(m_zone) && day.total() == 0) {
        // Say why the card is empty rather than showing misleading zeros.
        QFont mf = font();
        mf.setPointSizeF(10.5);
        p.setFont(mf);
        p.setPen(ui.TextFaint);
        p.drawText(QRectF(pad, 140, w, 64), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap,
                   tr("No AIS reports from this zone right now: aisstream.io relies on volunteer "
                      "receivers and does not cover every region."));
    } else {
        drawHourlyChart(p, QRectF(pad, 140, w, 64), m_monitor->hourly(m_zone), m_monitor->recordedHours(), tanker,
                        cargo);
        QFont af = font();
        af.setPointSizeF(9.5);
        p.setFont(af);
        p.setPen(ui.TextFaint);
        p.drawText(QRectF(pad, 208, w, 14), Qt::AlignLeft | Qt::AlignVCenter, tr("−48 h"));
        p.drawText(QRectF(pad, 208, w, 14), Qt::AlignHCenter | Qt::AlignVCenter,
                   tr("%1 h of %2 recorded").arg(m_monitor->recordedHourCount()).arg(ChokepointMonitor::WindowHours));
        p.drawText(QRectF(pad, 208, w, 14), Qt::AlignRight | Qt::AlignVCenter, tr("now"));
    }

    // ---- Historical daily transits (IMF PortWatch)
    p.setPen(QPen(ui.PanelLine, 1));
    p.drawLine(QPointF(pad, 232), QPointF(pad + w, 232));
    const Chokepoint &z = m_monitor->chokepoints()[m_zone];
    QFont lf = font();
    lf.setPointSizeF(9.5);
    lf.setWeight(QFont::DemiBold);
    lf.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
    p.setFont(lf);
    p.setPen(ui.TextSection);
    p.drawText(QRectF(pad, 242, w, 14), Qt::AlignLeft | Qt::AlignVCenter, tr("Daily transits · IMF PortWatch").toUpper());

    const PortWatchSummary s = summarize(m_portWatch, z.portwatchId);
    QFont sf = font();
    sf.setPointSizeF(10.5);
    p.setFont(sf);
    if (!s.valid) {
        p.setPen(ui.TextFaint);
        p.drawText(QRectF(pad, 262, w, 40), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                   m_portWatch->errorString().isEmpty() ? tr("Loading historical data…")
                                                        : tr("Historical data unavailable: %1").arg(m_portWatch->errorString()));
        return;
    }
    p.setPen(ui.TextDim);
    p.drawText(QRectF(pad, 242, w, 14), Qt::AlignRight | Qt::AlignVCenter,
               loc.toString(s.latest.date, QLocale::ShortFormat));

    // Latest day: total, then the breakdown by type.
    QFont nf = font();
    nf.setPointSizeF(15);
    nf.setWeight(QFont::DemiBold);
    p.setFont(nf);
    p.setPen(ui.Text);
    const QString totalText = tr("%1 transits").arg(loc.toString(s.latest.total));
    p.drawText(QRectF(pad, 260, w, 22), Qt::AlignLeft | Qt::AlignVCenter, totalText);
    double x = pad + QFontMetrics(nf).horizontalAdvance(totalText) + 12;
    p.setFont(sf);
    const QColor bulk = cargo;
    for (const auto &[label, color] : {std::pair(tr("%1 tankers").arg(s.latest.tanker), tanker),
                                       std::pair(tr("%1 dry bulk").arg(s.latest.dryBulk), bulk),
                                       std::pair(tr("%1 other").arg(s.latest.total - s.latest.tanker - s.latest.dryBulk),
                                                 ui.TextDim)}) {
        p.setPen(color);
        p.drawText(QRectF(x, 260, pad + w - x, 22), Qt::AlignLeft | Qt::AlignVCenter, label);
        x += p.fontMetrics().horizontalAdvance(label) + 10;
    }

    // Trend: last 7 days against the 90-day average.
    const QColor trend = s.changePct >= 0 ? ui.Up : ui.Down;
    p.setPen(ui.TextDim);
    const QString avgText = tr("7-day avg %1/day · 90-day avg %2/day")
                                .arg(loc.toString(s.avg7, 'f', 0), loc.toString(s.avg90, 'f', 0));
    p.drawText(QRectF(pad, 282, w, 18), Qt::AlignLeft | Qt::AlignVCenter, avgText);
    p.setPen(trend);
    p.drawText(QRectF(pad, 282, w, 18), Qt::AlignRight | Qt::AlignVCenter,
               QString("%1%2%").arg(s.changePct >= 0 ? "+" : "", loc.toString(s.changePct, 'f', 0)));

    QColor other = ui.TextFaint;
    other.setAlpha(150);
    drawDailyChart(p, QRectF(pad, 306, w, 40), m_portWatch->days(z.portwatchId), tanker, bulk, other);
    QFont af = font();
    af.setPointSizeF(9.5);
    p.setFont(af);
    p.setPen(ui.TextFaint);
    const QString scope = *z.portwatchScope ? tr("PortWatch zone: %1").arg(QString::fromUtf8(z.portwatchScope))
                                            : QString();
    p.drawText(QRectF(pad, 348, w, 14), Qt::AlignLeft | Qt::AlignVCenter,
               tr("%1 days").arg(m_portWatch->days(z.portwatchId).size()));
    p.drawText(QRectF(pad, 348, w, 14), Qt::AlignRight | Qt::AlignVCenter, scope);
}

// ---------------------------------------------------------------- ChokepointsPage

ChokepointsPage::ChokepointsPage(ChokepointMonitor *monitor, PortWatch *portWatch, VesselStore *vessels,
                                 MarketData *data, QWidget *parent)
    : QWidget(parent)
    , m_monitor(monitor)
    , m_portWatch(portWatch)
{
    setObjectName("sidePanel");
    setAttribute(Qt::WA_StyledBackground);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *content = new QWidget;
    content->setObjectName("panelContent");
    auto *l = new QVBoxLayout(content);
    l->setContentsMargins(32, 26, 32, 32);
    l->setSpacing(6);

    auto *title = new QLabel(tr("Chokepoints"));
    title->setObjectName("title");
    l->addWidget(title);
    auto *hint = new QLabel(tr("Live tanker and cargo-ship traffic through the main maritime straits and canals. "
                               "Click a card to show it on the map."));
    hint->setObjectName("dim");
    hint->setWordWrap(true);
    l->addWidget(hint);
    auto *method = new QLabel(tr("Passages count tankers and cargo ships seen inside each zone (once per visit) "
                                 "over the last 48 h. aisstream.io keeps no history, so only hours when this app "
                                 "was receiving data are counted; the others are shaded in the charts. Counts "
                                 "are lower bounds: AIS coverage is partial, especially offshore."));
    method->setObjectName("dim");
    method->setWordWrap(true);
    l->addWidget(method);
    m_sourceLabel = new QLabel;
    m_sourceLabel->setObjectName("dim");
    m_sourceLabel->setWordWrap(true);
    l->addWidget(m_sourceLabel);
    l->addSpacing(14);

    auto *grid = new QGridLayout;
    grid->setSpacing(16);
    for (int i = 0; i < m_monitor->chokepoints().size(); ++i) {
        auto *card = new ChokepointCard(m_monitor, m_portWatch, data, i);
        connect(card, &ChokepointCard::clicked, this, &ChokepointsPage::chokepointActivated);
        grid->addWidget(card, i / 3, i % 3);
        m_cards.push_back(card);
    }
    l->addLayout(grid);
    l->addStretch();
    scroll->setWidget(content);
    outer->addWidget(scroll);

    connect(vessels, &VesselStore::changed, this, &ChokepointsPage::refresh);
    connect(m_portWatch, &PortWatch::updated, this, [this] {
        for (ChokepointCard *c : std::as_const(m_cards))
            c->update();
        updateSourceLabel();
    });
    updateSourceLabel();
    refresh();
}

void ChokepointsPage::refresh()
{
    if (!isVisible())
        return;
    for (ChokepointCard *c : std::as_const(m_cards))
        c->update();
}

void ChokepointsPage::updateSourceLabel()
{
    const QDate latest = m_portWatch->latestDate();
    m_sourceLabel->setText(
        latest.isValid()
            ? tr("Daily transits: IMF PortWatch (satellite AIS), published with a delay of a few days. "
                 "Latest day: %1.").arg(QLocale().toString(latest, QLocale::LongFormat))
            : tr("Daily transits: IMF PortWatch (satellite AIS), loading…"));
}
