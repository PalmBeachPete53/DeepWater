#include "SideFlap.h"

#include "Theme.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolButton>

SideFlap::SideFlap(const QString &label, QWidget *parent)
    : QWidget(parent)
    , m_label(label)
{
    setFixedSize(Width, Height);
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover);
}

void SideFlap::setOpen(bool open)
{
    m_open = open;
    update();
}

void SideFlap::paintEvent(QPaintEvent *)
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
    p.drawText(QRectF(-30, -8, 60, 16), Qt::AlignCenter, m_label);
}

void SideFlap::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && rect().contains(e->pos()))
        emit clicked();
}

namespace {

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

QToolButton *sidePanelCloseButton(QWidget *parent)
{
    auto *close = new QToolButton(parent);
    close->setObjectName("devIcon");
    close->setCursor(Qt::PointingHandCursor);
    close->setFocusPolicy(Qt::NoFocus);
    close->setFixedSize(26, 26);
    QIcon cross;
    cross.addPixmap(crossPixmap(Theme::current().TextDim), QIcon::Normal);
    cross.addPixmap(crossPixmap(Theme::current().Text), QIcon::Active);
    close->setIcon(cross);
    close->setIconSize(QSize(12, 12));
    return close;
}
