#include "LeftPanel.h"

#include "MarketData.h"
#include "Theme.h"
#include "VesselStore.h"

#include <QAbstractButton>
#include <QCheckBox>
#include <QGridLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QScrollArea>
#include <QSet>
#include <QToolTip>
#include <QVariantAnimation>
#include <QVBoxLayout>

namespace {

// Filled dot when the layer is on, hollow ring when it is off.
QIcon dotIcon(const QColor &color, bool on)
{
    QPixmap pm(24, 24);
    pm.setDevicePixelRatio(2);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(on ? Qt::NoPen : QPen(color.darker(160), 1.5));
    p.setBrush(on ? QBrush(color) : Qt::NoBrush);
    p.drawEllipse(QPointF(6, 6), 4.5, 4.5);
    return QIcon(pm);
}

QLabel *sectionLabel(const QString &text)
{
    auto *l = new QLabel(text.toUpper());
    l->setObjectName("sectionLabel");
    return l;
}

QString formatPrice(double v)
{
    return QLocale().toString(v, 'f', v >= 1000 ? 0 : (v >= 100 ? 1 : (v >= 10 ? 2 : 3)));
}

// Where a row's price comes from, for its tooltip.
QString priceSource(const Commodity &c)
{
    switch (c.source) {
    case Commodity::Source::Futures:
        return c.settlement
            ? LeftPanel::tr("%1 futures (%2), close of %3, via Yahoo Finance")
                  .arg(c.future.market, c.contract, QLocale().toString(c.time.date(), QLocale::ShortFormat))
            : LeftPanel::tr("%1 futures (%2), %3, about 10 minutes delayed, via Yahoo Finance")
                  .arg(c.future.market, c.contract, QLocale().toString(c.time.toLocalTime(), QLocale::ShortFormat));
    case Commodity::Source::Monthly:
        return LeftPanel::tr("IMF monthly average, %1").arg(QLocale().toString(c.time.date(), "MMMM yyyy"));
    case Commodity::Source::None:
        break;
    }
    return LeftPanel::tr("No price yet");
}

// Ship-shaped (triangle) icon for the vessel layers.
QIcon shipIcon(const QColor &color, bool on)
{
    QPixmap pm(24, 24);
    pm.setDevicePixelRatio(2);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(on ? Qt::NoPen : QPen(color.darker(160), 1.3));
    p.setBrush(on ? QBrush(color) : Qt::NoBrush);
    p.drawPolygon(QPolygonF {QPointF(6, 1.5), QPointF(10.5, 10.5), QPointF(1.5, 10.5)});
    return QIcon(pm);
}

// Small on/off switch: a pill whose knob slides to the right when checked.
class ToggleSwitch : public QAbstractButton
{
public:
    explicit ToggleSwitch(QWidget *parent = nullptr)
        : QAbstractButton(parent)
    {
        setCheckable(true);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        setFixedSize(28, 16);
        m_slide.setDuration(140);
        m_slide.setEasingCurve(QEasingCurve::OutCubic);
        connect(&m_slide, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
            m_position = v.toDouble();
            update();
        });
        connect(this, &QAbstractButton::toggled, this, [this](bool on) {
            m_slide.stop();
            m_slide.setStartValue(m_position);
            m_slide.setEndValue(on ? 1.0 : 0.0);
            m_slide.start();
        });
    }
    // Without the slide, for the initial state.
    void setCheckedNow(bool on)
    {
        setChecked(on);
        m_slide.stop();
        m_position = on ? 1.0 : 0.0;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        const Palette &pal = Theme::current();
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF track = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        QColor off = underMouse() ? pal.ControlBorderHover : pal.ControlBorder;
        const QColor on = pal.Accent;
        // Track color fades between off and on with the knob.
        const double t = m_position;
        const QColor fill = QColor::fromRgbF(float(off.redF() + (on.redF() - off.redF()) * t),
                                             float(off.greenF() + (on.greenF() - off.greenF()) * t),
                                             float(off.blueF() + (on.blueF() - off.blueF()) * t));
        p.setPen(Qt::NoPen);
        p.setBrush(fill);
        p.drawRoundedRect(track, track.height() / 2, track.height() / 2);
        const double d = track.height() - 4;
        const double x = track.left() + 2 + (track.width() - 4 - d) * t;
        // White knob, outlined on light themes where the off track is pale too.
        p.setPen(Theme::isLight(pal) ? QPen(pal.ControlBorderHover, 0.8) : Qt::NoPen);
        p.setBrush(Qt::white);
        p.drawEllipse(QRectF(x, track.top() + 2, d, d));
    }
    void enterEvent(QEnterEvent *) override { update(); }
    void leaveEvent(QEvent *) override { update(); }

private:
    QVariantAnimation m_slide;
    double m_position = 0;   // knob, 0 (off) to 1 (on)
};

} // namespace

LeftPanel::LeftPanel(MarketData *data, VesselStore *vessels, QWidget *parent)
    : QWidget(parent)
    , m_data(data)
    , m_vessels(vessels)
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
    cl->setContentsMargins(12, 8, 12, 12);
    cl->setSpacing(2);

    // Overview
    cl->addWidget(sectionLabel(tr("Overview")));
    QSet<QString> countries;
    for (const Site &s : m_data->sites())
        countries.insert(s.country);
    auto *grid = new QGridLayout;
    grid->setContentsMargins(8, 0, 8, 0);
    grid->setHorizontalSpacing(12);
    auto addStat = [&](int col, const QString &value, const QString &label) {
        auto *v = new QLabel(value);
        v->setObjectName("bigStat");
        auto *l = new QLabel(label.toUpper());
        l->setObjectName("sectionLabelTight");
        grid->addWidget(v, 0, col);
        grid->addWidget(l, 1, col);
    };
    addStat(0, QString::number(m_data->sites().size()), tr("Sites"));
    addStat(1, QString::number(countries.size()), tr("Countries"));
    addStat(2, QString::number(m_data->commodities().size()), tr("Markets"));
    cl->addLayout(grid);

    cl->addSpacing(10);
    cl->addWidget(sectionLabel(tr("Top movers")));
    for (int i = 0; i < 5; ++i) {
        auto *row = new MarketRow(m_data, {});
        connect(row, &MarketRow::clicked, this, &LeftPanel::commoditySelected);
        m_movers.push_back(row);
        cl->addWidget(row);
    }
    refreshMovers();

    // Layer toggles, with a switch next to the title to show or hide them all at once: on when
    // every layer shows; clicking it then hides them all, otherwise shows them all.
    cl->addSpacing(10);
    auto *layersTitle = new QHBoxLayout;
    layersTitle->setSpacing(4);
    layersTitle->addWidget(sectionLabel(tr("Layers")));
    auto *allLayers = new ToggleSwitch;
    layersTitle->addWidget(allLayers, 0, Qt::AlignBottom);
    layersTitle->addStretch();
    cl->addLayout(layersTitle);
    QVector<QCheckBox *> layerBoxes;
    auto syncAllLayers = [this, allLayers] {
        bool all = true;
        for (const Category &cat : m_data->categories())
            all = all && m_data->isCategoryVisible(cat.id);
        if (allLayers->isChecked() != all)
            allLayers->setChecked(all);
        allLayers->setToolTip(all ? tr("Hide all layers") : tr("Show all layers"));
    };
    for (const Category &cat : m_data->categories()) {
        auto *row = new QWidget;
        auto *rl = new QHBoxLayout(row);
        rl->setContentsMargins(8, 0, 8, 0);
        auto *cb = new QCheckBox(QString(cat.name).replace("&", "&&"));
        cb->setObjectName("layerToggle");
        const bool visible = m_data->isCategoryVisible(cat.id);
        cb->setIcon(dotIcon(cat.color, visible));
        cb->setChecked(visible);
        cb->setCursor(Qt::PointingHandCursor);
        auto *count = new QLabel(QString::number(m_data->siteCount(cat.id)));
        count->setObjectName("dim");
        rl->addWidget(cb, 1);
        rl->addWidget(count);
        cl->addWidget(row);
        connect(cb, &QCheckBox::toggled, this, [this, cb, id = cat.id, color = cat.color, syncAllLayers](bool on) {
            cb->setIcon(dotIcon(color, on));
            m_data->setCategoryVisible(id, on);
            syncAllLayers();
        });
        layerBoxes.push_back(cb);
    }
    syncAllLayers();
    allLayers->setCheckedNow(allLayers->isChecked());
    // clicked, not toggled: syncAllLayers sets the state without acting on it.
    connect(allLayers, &QAbstractButton::clicked, this, [allLayers, layerBoxes](bool on) {
        // The boxes' own updates would flip the switch back and forth until the last one.
        const QSignalBlocker blocker(allLayers);
        for (QCheckBox *cb : layerBoxes)
            cb->setChecked(on);
        allLayers->setChecked(on);
    });

    // Live AIS vessel layers
    cl->addSpacing(10);
    cl->addWidget(sectionLabel(tr("Live shipping")));
    const struct {
        Vessel::Class cls;
        QString label;
        QString tip;
        QColor color;
        QLabel **count;
    } vesselLayers[] = {
        {Vessel::Tanker, tr("Tankers"), tr("Crude, products, LNG and chemical tankers"),
         m_data->categoryColor("energy"), &m_tankerCount},
        {Vessel::Cargo, tr("Cargo ships"), tr("Dry bulk carriers (ores, coal, grain) and container ships"),
         m_data->categoryColor("industrial"), &m_cargoCount},
        {Vessel::Unknown, tr("Unidentified"),
         tr("Ships whose type has not been received yet (sent every ~6 minutes)"),
         Theme::current().TextFaint, &m_unknownCount},
    };
    for (const auto &layer : vesselLayers) {
        auto *row = new QWidget;
        auto *rl = new QHBoxLayout(row);
        rl->setContentsMargins(8, 0, 8, 0);
        auto *cb = new QCheckBox(layer.label);
        cb->setObjectName("layerToggle");
        cb->setToolTip(layer.tip);
        const bool visible = m_vessels->isClassVisible(layer.cls);
        cb->setIcon(layer.cls == Vessel::Unknown ? dotIcon(layer.color, visible) : shipIcon(layer.color, visible));
        cb->setChecked(visible);
        cb->setCursor(Qt::PointingHandCursor);
        *layer.count = new QLabel;
        (*layer.count)->setObjectName("dim");
        rl->addWidget(cb, 1);
        rl->addWidget(*layer.count);
        cl->addWidget(row);
        connect(cb, &QCheckBox::toggled, this, [this, cb, cls = layer.cls, color = layer.color](bool on) {
            cb->setIcon(cls == Vessel::Unknown ? dotIcon(color, on) : shipIcon(color, on));
            m_vessels->setClassVisible(cls, on);
        });
    }
    refreshVesselCounts();
    connect(m_vessels, &VesselStore::changed, this, &LeftPanel::refreshVesselCounts);

    // Watchlist grouped by category
    for (const Category &cat : m_data->categories()) {
        bool any = false;
        for (const Commodity &c : m_data->commodities()) {
            if (c.categoryId != cat.id)
                continue;
            if (!any) {
                cl->addSpacing(10);
                cl->addWidget(sectionLabel(cat.name));
                any = true;
            }
            auto *row = new MarketRow(m_data, c.id);
            connect(row, &MarketRow::clicked, this, &LeftPanel::commoditySelected);
            m_rows.push_back(row);
            cl->addWidget(row);
        }
    }
    cl->addStretch();
    scroll->setWidget(content);
    outer->addWidget(scroll, 1);

    auto *footer = new QLabel(tr("Real prices: delayed futures, IMF monthly averages · sample sites"));
    footer->setWordWrap(true);
    footer->setObjectName("footer");
    outer->addWidget(footer);

    connect(m_data, &MarketData::pricesUpdated, this, &LeftPanel::refreshPrices);
}

void LeftPanel::setActiveCommodity(const QString &id)
{
    m_activeCommodity = id;
    for (MarketRow *r : std::as_const(m_rows))
        r->setActive(r->commodityId() == id);
    for (MarketRow *r : std::as_const(m_movers))
        r->setActive(r->commodityId() == id);
}

void LeftPanel::refreshMovers()
{
    // Daily changes of futures only: a monthly average's change is over a month.
    QVector<const Commodity *> sorted;
    for (const Commodity &c : m_data->commodities())
        if (c.source == Commodity::Source::Futures && !std::isnan(c.changePct()))
            sorted.push_back(&c);
    std::sort(sorted.begin(), sorted.end(), [](const Commodity *a, const Commodity *b) {
        return std::abs(a->changePct()) > std::abs(b->changePct());
    });
    for (int i = 0; i < m_movers.size(); ++i) {
        m_movers[i]->setCommodity(i < sorted.size() ? sorted[i]->id : QString());
        m_movers[i]->setActive(!m_activeCommodity.isEmpty() && m_movers[i]->commodityId() == m_activeCommodity);
    }
}

void LeftPanel::refreshVesselCounts()
{
    m_tankerCount->setText(QLocale().toString(m_vessels->count(Vessel::Tanker)));
    m_cargoCount->setText(QLocale().toString(m_vessels->count(Vessel::Cargo)));
    m_unknownCount->setText(QLocale().toString(m_vessels->count(Vessel::Unknown)));
}

void LeftPanel::refreshPrices()
{
    for (MarketRow *r : std::as_const(m_rows))
        r->refresh();
    refreshMovers();
}

// ---------------------------------------------------------------- MarketRow

MarketRow::MarketRow(MarketData *data, const QString &commodityId, QWidget *parent)
    : QWidget(parent)
    , m_data(data)
    , m_id(commodityId)
{
    setFixedHeight(44);
    setCursor(Qt::PointingHandCursor);
}

void MarketRow::setCommodity(const QString &id)
{
    m_id = id;
    update();
}

void MarketRow::setActive(bool active)
{
    m_active = active;
    update();
}

void MarketRow::refresh()
{
    update();
}

bool MarketRow::event(QEvent *e)
{
    if (e->type() == QEvent::ToolTip) {
        if (const Commodity *c = m_data->commodity(m_id))
            QToolTip::showText(static_cast<QHelpEvent *>(e)->globalPos(), priceSource(*c), this);
        return true;
    }
    return QWidget::event(e);
}

void MarketRow::paintEvent(QPaintEvent *)
{
    const Commodity *c = m_data->commodity(m_id);
    if (!c)
        return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r = QRectF(rect()).adjusted(0, 1, 0, -1);

    if (m_active || m_hover) {
        p.setPen(Qt::NoPen);
        p.setBrush(m_active ? Theme::current().Active : Theme::current().Hover);
        p.drawRoundedRect(r, 6, 6);
    }
    if (m_active) {
        p.setBrush(m_data->categoryColor(c->categoryId));
        p.drawRoundedRect(QRectF(r.left(), r.top() + 8, 3, r.height() - 16), 1.5, 1.5);
    }

    const double pct = c->changePct();
    const QColor trend = std::isnan(pct) ? Theme::current().TextDim : (pct >= 0 ? Theme::current().Up : Theme::current().Down);

    QFont f = font();
    f.setPointSizeF(12.5);
    f.setWeight(QFont::Medium);
    p.setFont(f);
    p.setPen(Theme::current().Text);
    p.drawText(QRectF(12, r.top() + 5, 130, 18), Qt::AlignLeft | Qt::AlignVCenter, c->name);
    f.setWeight(QFont::Normal);
    f.setPointSizeF(10.5);
    p.setFont(f);
    p.setPen(Theme::current().TextDim);
    // A monthly average says which month.
    const QString unit = c->source == Commodity::Source::Monthly
        ? QString("%1 · %2").arg(c->unit, QLocale().toString(c->time.date(), "MMM"))
        : c->unit;
    p.drawText(QRectF(12, r.top() + 23, 130, 16), Qt::AlignLeft | Qt::AlignVCenter, unit);

    // Inline sparkline
    const QRectF spark(width() * 0.47, r.top() + 10, width() * 0.22, r.height() - 20);
    if (c->history.size() >= 2) {
        const auto [lo, hi] = std::minmax_element(c->history.begin(), c->history.end());
        const double span = *hi - *lo > 1e-12 ? *hi - *lo : 1.0;
        QPainterPath path;
        for (int i = 0; i < c->history.size(); ++i) {
            const QPointF pt(spark.left() + spark.width() * i / (c->history.size() - 1),
                             spark.bottom() - (c->history[i] - *lo) / span * spark.height());
            if (i == 0) path.moveTo(pt); else path.lineTo(pt);
        }
        p.setPen(QPen(trend, 1.2));
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
    }

    f.setPointSizeF(12.5);
    f.setWeight(QFont::Medium);
    p.setFont(f);
    p.setPen(Theme::current().Text);
    const QRectF right(0, r.top() + 5, width() - 10, 18);
    p.drawText(right, Qt::AlignRight | Qt::AlignVCenter,
               c->hasPrice() ? formatPrice(c->price) : QString::fromUtf8("—"));
    f.setPointSizeF(10.5);
    p.setFont(f);
    p.setPen(trend);
    if (!std::isnan(pct))
        p.drawText(QRectF(0, r.top() + 23, width() - 10, 16), Qt::AlignRight | Qt::AlignVCenter,
                   QString("%1%2%").arg(pct >= 0 ? "+" : "", QLocale().toString(pct, 'f', 2)));
}

void MarketRow::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && rect().contains(e->position().toPoint()))
        emit clicked(m_id);
}

void MarketRow::enterEvent(QEnterEvent *)
{
    m_hover = true;
    update();
}

void MarketRow::leaveEvent(QEvent *)
{
    m_hover = false;
    update();
}
