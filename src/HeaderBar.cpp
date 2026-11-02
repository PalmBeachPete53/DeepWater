#include "HeaderBar.h"

#include "Theme.h"

#include <QButtonGroup>
#include <QApplication>
#include <QDateTime>
#include <QMouseEvent>
#include <QSignalBlocker>
#include <QStyle>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QToolButton>
#include <QtMath>

namespace {

QPixmap gearPixmap(const QColor &color)
{
    const int size = 18;
    QPixmap pm(size * 2, size * 2);
    pm.setDevicePixelRatio(2);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.translate(size / 2.0, size / 2.0);

    // Eight teeth around a ring, with a hole in the middle.
    QPainterPath gear;
    const int teeth = 8;
    const double rOuter = 8.0, rInner = 6.2;
    for (int i = 0; i < teeth * 4; ++i) {
        const double a = 2 * M_PI * (i - 0.5) / (teeth * 4);
        const double r = (i % 4 < 2) ? rOuter : rInner;
        const QPointF pt(r * qCos(a), r * qSin(a));
        if (i == 0) gear.moveTo(pt); else gear.lineTo(pt);
    }
    gear.closeSubpath();
    gear.addEllipse(QPointF(0, 0), 2.6, 2.6);
    gear.setFillRule(Qt::OddEvenFill);
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.drawPath(gear);
    return pm;
}

// Code brackets "</>" for the developer tools, 18 px.
QPixmap devPixmap(const QColor &color)
{
    const int size = 18;
    QPixmap pm(size * 2, size * 2);
    pm.setDevicePixelRatio(2);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(color, 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawPolyline(QPolygonF({QPointF(6, 5), QPointF(2, 9), QPointF(6, 13)}));
    p.drawPolyline(QPolygonF({QPointF(12, 5), QPointF(16, 9), QPointF(12, 13)}));
    p.drawLine(QPointF(10.3, 3.5), QPointF(7.7, 14.5));
    return pm;
}

// Pencil (edit mode off) or check mark (on), 16 px.
QPixmap editPixmap(const QColor &color, bool done)
{
    const int size = 16;
    QPixmap pm(size * 2, size * 2);
    pm.setDevicePixelRatio(2);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    if (done) {
        p.setPen(QPen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPolyline(QPolygonF({QPointF(3, 8.5), QPointF(6.5, 12), QPointF(13, 4.5)}));
        return pm;
    }
    // A slanted body with a pointed tip at the bottom left.
    p.translate(size / 2.0, size / 2.0);
    p.rotate(45);
    QPainterPath body;
    body.addRoundedRect(QRectF(-1.9, -7, 3.8, 10.2), 0.8, 0.8);
    QPainterPath tip;
    tip.moveTo(-1.9, 4.2);
    tip.lineTo(1.9, 4.2);
    tip.lineTo(0, 7.4);
    tip.closeSubpath();
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.drawPath(body);
    p.drawPath(tip);
    return pm;
}

// "+" (show a hidden tab) or "×" (hide a tab).
QPixmap glyphPixmap(const QColor &color, int size, bool cross)
{
    QPixmap pm(size * 2, size * 2);
    pm.setDevicePixelRatio(2);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(color, cross ? 1.4 : 1.7, Qt::SolidLine, Qt::RoundCap));
    p.translate(size / 2.0, size / 2.0);
    if (cross)
        p.rotate(45);
    const double r = size * (cross ? 0.42 : 0.36);
    p.drawLine(QPointF(-r, 0), QPointF(r, 0));
    p.drawLine(QPointF(0, -r), QPointF(0, r));
    return pm;
}

QIcon glyphIcon(int size, bool cross)
{
    QIcon icon;
    icon.addPixmap(glyphPixmap(Theme::current().TextDim, size, cross), QIcon::Normal);
    icon.addPixmap(glyphPixmap(Theme::current().Text, size, cross), QIcon::Active);
    icon.addPixmap(glyphPixmap(Theme::current().TextFaint, size, cross), QIcon::Disabled);
    return icon;
}

void repolish(QWidget *w)
{
    w->style()->unpolish(w);
    w->style()->polish(w);
}

} // namespace

HeaderBar::HeaderBar(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("headerBar");
    setAttribute(Qt::WA_StyledBackground);
    setFixedHeight(52);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(20, 0, 12, 0);
    layout->setSpacing(0);

    auto *brand = new QLabel("DEEPWATER");
    brand->setObjectName("brand");
    layout->addWidget(brand);
    layout->addSpacing(36);

    m_navLayout = new QHBoxLayout;
    m_navLayout->setSpacing(4);
    layout->addLayout(m_navLayout);
    layout->addSpacing(6);

    m_addButton = new QToolButton;
    m_addButton->setObjectName("headerIconButton");
    m_addButton->setToolTip(tr("Show a hidden tab"));
    m_addButton->setCursor(Qt::PointingHandCursor);
    m_addButton->setFocusPolicy(Qt::NoFocus);
    m_addButton->setFixedSize(30, 30);
    m_addButton->setIcon(glyphIcon(16, false));
    m_addButton->setIconSize(QSize(16, 16));
    m_addButton->hide();
    connect(m_addButton, &QToolButton::clicked, this, &HeaderBar::showHiddenMenu);
    layout->addWidget(m_addButton);
    layout->addSpacing(4);

    m_editButton = new QToolButton;
    m_editButton->setObjectName("headerIconButton");
    m_editButton->setToolTip(tr("Edit tabs"));
    m_editButton->setCursor(Qt::PointingHandCursor);
    m_editButton->setCheckable(true);
    m_editButton->setFocusPolicy(Qt::NoFocus);
    m_editButton->setFixedSize(30, 30);
    QIcon edit;
    edit.addPixmap(editPixmap(Theme::current().TextDim, false), QIcon::Normal, QIcon::Off);
    edit.addPixmap(editPixmap(Theme::current().Text, false), QIcon::Active, QIcon::Off);
    edit.addPixmap(editPixmap(Theme::current().Accent, true), QIcon::Normal, QIcon::On);
    edit.addPixmap(editPixmap(Theme::current().Accent, true), QIcon::Active, QIcon::On);
    m_editButton->setIcon(edit);
    m_editButton->setIconSize(QSize(16, 16));
    connect(m_editButton, &QToolButton::toggled, this, &HeaderBar::setEditing);
    layout->addWidget(m_editButton);
    layout->addStretch();

    m_navGroup = new QButtonGroup(this);
    m_navGroup->setExclusive(true);

    m_clock = new QLabel;
    m_clock->setObjectName("headerClock");
    layout->addWidget(m_clock);
    layout->addSpacing(12);

    m_devButton = new QToolButton;
    m_devButton->setObjectName("headerIconButton");
    m_devButton->setToolTip(tr("Developer tools"));
    m_devButton->setCursor(Qt::PointingHandCursor);
    m_devButton->setCheckable(true);
    m_devButton->setFocusPolicy(Qt::NoFocus);
    m_devButton->setFixedSize(34, 34);
    QIcon dev;
    dev.addPixmap(devPixmap(Theme::current().TextDim), QIcon::Normal, QIcon::Off);
    dev.addPixmap(devPixmap(Theme::current().Text), QIcon::Active, QIcon::Off);
    dev.addPixmap(devPixmap(Theme::current().Accent), QIcon::Normal, QIcon::On);
    dev.addPixmap(devPixmap(Theme::current().Accent), QIcon::Active, QIcon::On);
    m_devButton->setIcon(dev);
    m_devButton->setIconSize(QSize(18, 18));
    connect(m_devButton, &QToolButton::toggled, this, &HeaderBar::devToolsToggled);
    layout->addWidget(m_devButton);
    layout->addSpacing(2);

    auto *settings = new QToolButton;
    settings->setObjectName("headerIconButton");
    settings->setToolTip(tr("Settings"));
    settings->setCursor(Qt::PointingHandCursor);
    settings->setFixedSize(34, 34);
    QIcon gear;
    gear.addPixmap(gearPixmap(Theme::current().TextDim), QIcon::Normal);
    gear.addPixmap(gearPixmap(Theme::current().Text), QIcon::Active);
    settings->setIcon(gear);
    settings->setIconSize(QSize(18, 18));
    connect(settings, &QToolButton::clicked, this, &HeaderBar::settingsRequested);
    layout->addWidget(settings);

    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &HeaderBar::updateClock);
    timer->start(1000);
    updateClock();
}

void HeaderBar::setDevToolsChecked(bool on)
{
    const QSignalBlocker blocker(m_devButton);
    m_devButton->setChecked(on);
}

void HeaderBar::addNavItem(const QString &id, const QString &label)
{
    auto *b = new QToolButton;
    b->setObjectName("navButton");
    b->setText(label);
    b->setCheckable(true);
    b->setCursor(Qt::PointingHandCursor);
    b->setFixedHeight(52);
    m_navGroup->addButton(b);
    m_navLayout->addWidget(b);
    m_navButtons.insert(id, b);
    m_navOrder.append(id);
    if (m_navButtons.size() == 1)
        b->setChecked(true);
    b->installEventFilter(this);
    connect(b, &QToolButton::clicked, this, [this, id] { emit navigationRequested(id); });

    auto *remove = new QToolButton(b);
    remove->setObjectName("navRemoveButton");
    remove->setToolTip(tr("Hide this tab"));
    remove->setCursor(Qt::PointingHandCursor);
    remove->setFocusPolicy(Qt::NoFocus);
    remove->setFixedSize(16, 16);
    remove->setIcon(glyphIcon(10, true));
    remove->setIconSize(QSize(10, 10));
    remove->hide();
    m_removeButtons.insert(id, remove);
    connect(remove, &QToolButton::clicked, this, [this, id] {
        setNavItemHidden(id, true);
        emit navHiddenChanged(hiddenNavItems());
    });
}

void HeaderBar::setNavOrder(const QStringList &ids)
{
    QStringList order;
    for (const QString &id : ids)
        if (m_navButtons.contains(id) && !order.contains(id))
            order.append(id);
    // A tab the saved order doesn't know (added since) takes its default place: after the tab before it.
    for (int i = 0; i < m_navOrder.size(); ++i) {
        const QString &id = m_navOrder.at(i);
        if (order.contains(id))
            continue;
        const int after = i > 0 ? int(order.indexOf(m_navOrder.at(i - 1))) : -1;
        order.insert(after + 1, id);
    }
    m_navOrder = order;
    for (int i = 0; i < m_navOrder.size(); ++i)
        m_navLayout->insertWidget(i, m_navButtons.value(m_navOrder.at(i)));
}

void HeaderBar::setNavHidden(const QStringList &ids)
{
    for (const QString &id : std::as_const(m_navOrder))
        setNavItemHidden(id, ids.contains(id));
}

void HeaderBar::setNavItemHidden(const QString &id, bool hidden)
{
    QToolButton *b = m_navButtons.value(id);
    if (!b || b->isHidden() == hidden)
        return;
    if (hidden && hiddenNavItems().size() == m_navOrder.size() - 1)
        return;   // keep the last visible tab
    b->setVisible(!hidden);
    // Hiding the open page moves to the first visible tab.
    if (hidden && b->isChecked()) {
        for (const QString &other : std::as_const(m_navOrder)) {
            if (!m_navButtons.value(other)->isHidden()) {
                emit navigationRequested(other);
                break;
            }
        }
    }
    updateEditControls();
}

QStringList HeaderBar::hiddenNavItems() const
{
    QStringList ids;
    for (const QString &id : m_navOrder)
        if (m_navButtons.value(id)->isHidden())
            ids.append(id);
    return ids;
}

void HeaderBar::updateEditControls()
{
    const QStringList hidden = hiddenNavItems();
    const bool canRemove = hidden.size() < m_navOrder.size() - 1;
    for (QToolButton *remove : std::as_const(m_removeButtons))
        remove->setVisible(m_editing && canRemove);
    m_addButton->setVisible(m_editing);
    m_addButton->setEnabled(!hidden.isEmpty());
}

// Floating vertical list of the hidden tabs; picking one shows it again at its place.
void HeaderBar::showHiddenMenu()
{
    auto *menu = new QMenu(this);
    menu->setObjectName("navHiddenMenu");
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->setAttribute(Qt::WA_TranslucentBackground);   // rounded corners (style.qss)
    for (const QString &id : hiddenNavItems()) {
        menu->addAction(m_navButtons.value(id)->text(), this, [this, id] {
            setNavItemHidden(id, false);
            emit navHiddenChanged(hiddenNavItems());
        });
    }
    menu->popup(m_addButton->mapToGlobal(QPoint(0, m_addButton->height() + 8)));
}

void HeaderBar::setEditing(bool editing)
{
    m_editing = editing;
    endDrag();
    m_editButton->setToolTip(editing ? tr("Done") : tr("Edit tabs"));
    for (QToolButton *b : std::as_const(m_navButtons)) {
        b->setProperty("editing", editing);
        b->setCursor(editing ? Qt::OpenHandCursor : Qt::PointingHandCursor);
        repolish(b);
    }
    updateEditControls();
}

void HeaderBar::moveNavItem(int from, int to)
{
    m_navOrder.move(from, to);
    QWidget *b = m_navButtons.value(m_navOrder.at(to));
    m_navLayout->removeWidget(b);
    m_navLayout->insertWidget(to, b);
    layout()->activate();   // apply the new geometry before the next mouse move is measured
}

void HeaderBar::endDrag()
{
    if (!m_dragButton)
        return;
    m_dragButton->setProperty("dragging", false);
    m_dragButton->setCursor(m_editing ? Qt::OpenHandCursor : Qt::PointingHandCursor);
    repolish(m_dragButton);
    m_dragButton = nullptr;
}

// In edit mode the tabs' mouse events are handled here: a press starts a drag, moving past
// a neighbour's center swaps with it, and a release without movement still opens the page.
bool HeaderBar::eventFilter(QObject *watched, QEvent *event)
{
    auto *b = qobject_cast<QToolButton *>(watched);
    if (b && event->type() == QEvent::Resize) {
        // The × sits at the right end of the tab chip (vertically centered, see style.qss).
        QToolButton *remove = m_removeButtons.value(m_navButtons.key(b));
        remove->move(b->width() - remove->width() - 8, (b->height() - remove->height()) / 2);
    }
    if (!m_editing || !b)
        return QWidget::eventFilter(watched, event);

    switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonDblClick: {
        auto *me = static_cast<QMouseEvent *>(event);
        if (me->button() != Qt::LeftButton)
            return true;
        m_dragButton = b;
        m_pressX = me->globalPosition().x();
        m_dragMoved = false;
        b->setProperty("dragging", true);
        b->setCursor(Qt::ClosedHandCursor);
        repolish(b);
        return true;
    }
    case QEvent::MouseMove: {
        if (b != m_dragButton)
            return true;
        auto *me = static_cast<QMouseEvent *>(event);
        const int gx = me->globalPosition().x();
        if (!m_dragMoved && qAbs(gx - m_pressX) < QApplication::startDragDistance())
            return true;
        m_dragMoved = true;
        const int x = mapFromGlobal(me->globalPosition().toPoint()).x();
        // Hidden tabs are skipped: the dragged tab jumps to the next visible neighbour.
        auto neighbour = [this](int i, int step) {
            for (i += step; i >= 0 && i < m_navOrder.size(); i += step)
                if (!m_navButtons.value(m_navOrder.at(i))->isHidden())
                    return i;
            return -1;
        };
        int i = m_navOrder.indexOf(m_navButtons.key(b));
        for (int j; (j = neighbour(i, -1)) >= 0
                    && x < m_navButtons.value(m_navOrder.at(j))->geometry().center().x();
             i = j)
            moveNavItem(i, j);
        for (int j; (j = neighbour(i, +1)) >= 0
                    && x > m_navButtons.value(m_navOrder.at(j))->geometry().center().x();
             i = j)
            moveNavItem(i, j);
        return true;
    }
    case QEvent::MouseButtonRelease: {
        if (b != m_dragButton)
            return true;
        const bool moved = m_dragMoved;
        const QString id = m_navButtons.key(b);
        endDrag();
        if (moved)
            emit navOrderChanged(m_navOrder);
        else
            emit navigationRequested(id);
        return true;
    }
    default:
        return QWidget::eventFilter(watched, event);
    }
}

void HeaderBar::setCurrent(const QString &id)
{
    if (QToolButton *b = m_navButtons.value(id))
        b->setChecked(true);
}

void HeaderBar::updateClock()
{
    m_clock->setText(QLocale().toString(QDateTime::currentDateTimeUtc(), "ddd d MMM  HH:mm:ss")
                     + " UTC");
}
