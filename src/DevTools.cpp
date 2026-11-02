#include "DevTools.h"

#include "CodeGraphView.h"
#include "Theme.h"

#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariantAnimation>

namespace {

constexpr int kFlapWidth = 22;
constexpr int kFlapHeight = 84;

// The flap on the page's right edge: a rounded tab with a chevron and a vertical "DEV".
class Flap : public QWidget
{
    Q_OBJECT
public:
    explicit Flap(QWidget *parent = nullptr) : QWidget(parent)
    {
        setFixedSize(kFlapWidth, kFlapHeight);
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover);
    }
    void setOpen(bool open) { m_open = open; update(); }

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent *) override
    {
        const Palette &t = Theme::current();
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QPainterPath path;
        path.addRoundedRect(QRectF(0.5, 0.5, width() + 6, height() - 1), 6, 6);   // right corners hidden
        p.setPen(QPen(t.PanelLine, 1));
        p.setBrush(underMouse() ? t.Hover : t.Panel);
        p.drawPath(path);
        const QColor ink = underMouse() ? t.Text : t.Accent;
        p.setPen(QPen(ink, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        const double cx = width() / 2.0, cy = 18;
        if (m_open)
            p.drawPolyline(QPolygonF({QPointF(cx - 3, cy - 4), QPointF(cx + 2, cy), QPointF(cx - 3, cy + 4)}));
        else
            p.drawPolyline(QPolygonF({QPointF(cx + 3, cy - 4), QPointF(cx - 2, cy), QPointF(cx + 3, cy + 4)}));
        QFont f = font();
        f.setPointSizeF(9);
        f.setBold(true);
        f.setLetterSpacing(QFont::AbsoluteSpacing, 2);
        p.setFont(f);
        p.setPen(underMouse() ? t.Text : t.TextDim);
        p.translate(cx, height() / 2.0 + 14);
        p.rotate(-90);
        p.drawText(QRectF(-30, -8, 60, 16), Qt::AlignCenter, QStringLiteral("DEV"));
    }
    void mouseReleaseEvent(QMouseEvent *e) override
    {
        if (e->button() == Qt::LeftButton && rect().contains(e->pos()))
            emit clicked();
    }
    void enterEvent(QEnterEvent *) override { update(); }
    void leaveEvent(QEvent *) override { update(); }

private:
    bool m_open = false;
};

QPixmap crossPixmap(const QColor &color)
{
    const int size = 12;
    QPixmap pm(size * 2, size * 2);
    pm.setDevicePixelRatio(2);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(color, 1.4, Qt::SolidLine, Qt::RoundCap));
    p.translate(size / 2.0, size / 2.0);
    p.rotate(45);
    p.drawLine(QPointF(-4.5, 0), QPointF(4.5, 0));
    p.drawLine(QPointF(0, -4.5), QPointF(0, 4.5));
    return pm;
}

} // namespace

DevToolsDock::DevToolsDock(QWidget *host, int topOffset)
    : QWidget(host), m_host(host), m_topOffset(topOffset)
{
    setObjectName("devToolsDock");

    auto *flap = new Flap(this);
    m_flap = flap;
    connect(flap, &Flap::clicked, this, [this] { setOpen(!m_open); });

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
    auto *close = new QToolButton;
    close->setObjectName("devIcon");
    close->setToolTip(tr("Close"));
    close->setCursor(Qt::PointingHandCursor);
    close->setFocusPolicy(Qt::NoFocus);
    close->setFixedSize(26, 26);
    QIcon cross;
    cross.addPixmap(crossPixmap(Theme::current().TextDim), QIcon::Normal);
    cross.addPixmap(crossPixmap(Theme::current().Text), QIcon::Active);
    close->setIcon(cross);
    close->setIconSize(QSize(12, 12));
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
        static_cast<Flap *>(m_flap)->setOpen(open);
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
    const int panelShown = int(qMax(200, m_host->width() - kFlapWidth) * m_shown + 0.5);
    const int h = m_host->height() - m_topOffset;
    setGeometry(m_host->width() - kFlapWidth - panelShown, m_topOffset, kFlapWidth + panelShown, h);
    // Open, the panel also covers the flap's column; the flap stays on top of it.
    m_panel->setGeometry(kFlapWidth - int(kFlapWidth * m_shown + 0.5), 0, m_host->width(), h);
    m_flap->move(0, 24 + int(66 * m_shown));   // open: below the panel's tool row
    m_flap->raise();
    raise();
}

#include "DevTools.moc"
