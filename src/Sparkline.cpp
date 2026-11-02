#include "Sparkline.h"

#include "Theme.h"

#include <QLocale>
#include <QPainter>
#include <QPainterPath>

Sparkline::Sparkline(QWidget *parent)
    : QWidget(parent)
    , m_color(Theme::current().Text)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
}

void Sparkline::setValues(const QVector<double> &values)
{
    m_values = values;
    update();
}

void Sparkline::setColor(const QColor &color)
{
    m_color = color;
    update();
}

void Sparkline::setDetailed(bool detailed)
{
    m_detailed = detailed;
    setMinimumHeight(detailed ? 140 : 22);
    updateGeometry();
    update();
}

QSize Sparkline::sizeHint() const
{
    return m_detailed ? QSize(280, 150) : QSize(70, 24);
}

void Sparkline::paintEvent(QPaintEvent *)
{
    if (m_values.size() < 2)
        return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const auto [minIt, maxIt] = std::minmax_element(m_values.begin(), m_values.end());
    const double lo = *minIt, hi = *maxIt;
    const double span = hi - lo > 1e-12 ? hi - lo : 1.0;

    const QMarginsF margins = m_detailed ? QMarginsF(2, 18, 2, 18) : QMarginsF(1, 2, 1, 2);
    const QRectF r = QRectF(rect()).marginsRemoved(margins);
    auto pt = [&](int i) {
        return QPointF(r.left() + r.width() * i / (m_values.size() - 1),
                       r.bottom() - (m_values[i] - lo) / span * r.height());
    };

    QPainterPath line;
    line.moveTo(pt(0));
    for (int i = 1; i < m_values.size(); ++i)
        line.lineTo(pt(i));

    if (m_detailed) {
        QPainterPath area = line;
        area.lineTo(r.right(), r.bottom());
        area.lineTo(r.left(), r.bottom());
        area.closeSubpath();
        QLinearGradient g(r.topLeft(), r.bottomLeft());
        QColor top = m_color; top.setAlpha(70);
        QColor bottom = m_color; bottom.setAlpha(0);
        g.setColorAt(0, top);
        g.setColorAt(1, bottom);
        p.fillPath(area, g);

        // Dashed baseline at the opening value.
        QPen base(Theme::current().TextDim, 1, Qt::DashLine);
        p.setPen(base);
        const double y0 = pt(0).y();
        p.drawLine(QPointF(r.left(), y0), QPointF(r.right(), y0));

        QFont f = font();
        f.setPointSizeF(10);
        p.setFont(f);
        p.setPen(Theme::current().TextDim);
        p.drawText(QRectF(0, 0, width(), 16), Qt::AlignRight | Qt::AlignTop,
                   tr("H %1").arg(QLocale().toString(hi, 'f', hi < 100 ? 2 : 1)));
        p.drawText(QRectF(0, height() - 16, width(), 16), Qt::AlignRight | Qt::AlignBottom,
                   tr("L %1").arg(QLocale().toString(lo, 'f', lo < 100 ? 2 : 1)));
    }

    p.setPen(QPen(m_color, m_detailed ? 1.8 : 1.3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawPath(line);

    // Mark the latest value.
    p.setPen(Qt::NoPen);
    p.setBrush(m_color);
    p.drawEllipse(pt(m_values.size() - 1), m_detailed ? 3.5 : 2.0, m_detailed ? 3.5 : 2.0);
}
