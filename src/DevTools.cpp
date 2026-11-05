#include "DevTools.h"

#include "CodeGraphView.h"
#include "SideFlap.h"
#include "Theme.h"

#include <QEvent>
#include <QRegion>
#include <QFrame>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariantAnimation>


DevToolsDock::DevToolsDock(QWidget *host, int topOffset)
    : QWidget(host), m_host(host), m_topOffset(topOffset)
{
    setObjectName("devToolsDock");

    auto *flap = new SideFlap(QStringLiteral("DEV"), this);
    m_flap = flap;
    connect(flap, &SideFlap::clicked, this, [this] { setOpen(!m_open); });

    m_panel = new QFrame(this);
    m_panel->setObjectName("devPanel");
    auto *layout = new QVBoxLayout(m_panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *tabs = new QWidget;
    tabs->setObjectName("devTabBar");
    tabs->setFixedHeight(36);
    auto *tabRow = new QHBoxLayout(tabs);
    tabRow->setContentsMargins(10, 0, 6, 0);
    tabRow->setSpacing(2);
    auto *codebase = new QToolButton;
    codebase->setObjectName("devTab");
    codebase->setText(tr("CODEBASE"));
    codebase->setCheckable(true);
    codebase->setChecked(true);
    codebase->setCursor(Qt::PointingHandCursor);
    codebase->setFixedHeight(36);
    tabRow->addWidget(codebase);
    tabRow->addStretch();
    auto *close = sidePanelCloseButton();
    close->setToolTip(tr("Close"));
    connect(close, &QToolButton::clicked, this, [this] { setOpen(false); });
    tabRow->addWidget(close);
    layout->addWidget(tabs);

    m_stack = new QStackedWidget;
    m_codebase = new CodeGraphView;
    m_stack->addWidget(m_codebase);
    layout->addWidget(m_stack, 1);

    m_anim = new QVariantAnimation(this);
    m_anim->setDuration(220);
    m_anim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        m_shown = v.toDouble();
        relayout();
    });

    host->installEventFilter(this);
    relayout();
}

void DevToolsDock::setOpen(bool open, bool animate)
{
    if (open != m_open) {
        m_open = open;
        static_cast<SideFlap *>(m_flap)->setOpen(open);
        emit openChanged(open);
    }
    m_anim->stop();
    if (animate) {
        m_anim->setStartValue(m_shown);
        m_anim->setEndValue(open ? 1.0 : 0.0);
        m_anim->start();
    } else {
        m_shown = open ? 1.0 : 0.0;
        relayout();
    }
}

bool DevToolsDock::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_host && event->type() == QEvent::Resize)
        relayout();
    return QWidget::eventFilter(watched, event);
}

void DevToolsDock::relayout()
{
    // The panel spans the whole window; only the flap stays visible when closed.
    const int panelShown = int(qMax(200, m_host->width() - SideFlap::Width) * m_shown + 0.5);
    const int h = m_host->height() - m_topOffset;
    setGeometry(m_host->width() - SideFlap::Width - panelShown, m_topOffset, SideFlap::Width + panelShown, h);
    // Open, the panel also covers the flap's column; the flap stays on top of it.
    m_panel->setGeometry(SideFlap::Width - int(SideFlap::Width * m_shown + 0.5), 0, m_host->width(), h);
    m_flap->move(0, SideFlap::Top + int(66 * m_shown));   // open: below the panel's tool row
    m_flap->raise();
    // The dock is as high as the page area but only its flap (and its panel while open) takes the mouse: the rest of
    // its column lets clicks through, to the AI panel's flap under it or the page.
    QRegion region(m_flap->geometry());
    if (m_shown > 0) region += m_panel->geometry();
    setMask(region);
    raise();
}

