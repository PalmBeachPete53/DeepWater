#include "Widgets.h"

#include "Text.h"
#include "Theme.h"

#include <QBoxLayout>
#include <QColorDialog>
#include <QEvent>
#include <QFontMetricsF>
#include <QHash>
#include <QPainter>
#include <QPainterPath>
#include <QStandardItemModel>
#include <QStyleOptionComboBox>

namespace qtm {

namespace {

QColor resolve(const QString &v)
{
    if (v.startsWith("--"))
        return theme::color(v);
    return theme::parseColor(v);
}

// Icons are SVG renders; cache them by markup, colour, size and pixel ratio.
QPixmap cachedIcon(const QString &svg, const QColor &color, int size, qreal dpr)
{
    static QHash<QString, QPixmap> cache;
    const QString key = QString("%1|%2|%3|%4").arg(svg).arg(color.name(QColor::HexArgb)).arg(size).arg(dpr);
    auto it = cache.find(key);
    if (it != cache.end())
        return *it;
    if (cache.size() > 2000)
        cache.clear();
    QPixmap pm = theme::svgPixmap(svg, color, size, dpr);
    cache.insert(key, pm);
    return pm;
}

} // namespace

double textWidth(const QFont &font, const QString &text)
{
    return text::width(font, text);
}

// ---------------------------------------------------------------- Btn

Btn::Btn(const BtnStyle &style, const QString &iconSvg, const QString &text, QWidget *parent)
    : QAbstractButton(parent), m_style(style), m_svg(iconSvg), m_text(text)
{
    setAttribute(Qt::WA_Hover);
    setFocusPolicy(Qt::NoFocus);
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void Btn::setActive(bool on)
{
    if (m_active == on) return;
    m_active = on;
    update();
}

void Btn::setOn(bool on)
{
    if (m_on == on) return;
    m_on = on;
    update();
}

void Btn::setIconSvg(const QString &svg)
{
    m_svg = svg;
    updateGeometry();
    update();
}

void Btn::setLabel(const QString &text)
{
    if (m_text == text) return;
    m_text = text;
    updateGeometry();
    update();
}

void Btn::setIconRotated(bool on)
{
    m_rotated = on;
    update();
}

QSize Btn::sizeHint() const
{
    if (m_style.fixedWidth)
        return {m_style.fixedWidth, m_style.height};
    double w = m_style.padL + m_style.padR;
    if (!m_svg.isEmpty())
        w += m_style.iconSize;
    if (!m_text.isEmpty()) {
        if (!m_svg.isEmpty())
            w += m_style.gap;
        w += textWidth(theme::font(m_style.fontPx, m_style.weight), m_text);
    }
    return {std::max(m_style.minWidth, int(std::ceil(w))), m_style.height};
}

bool Btn::event(QEvent *e)
{
    if (e->type() == QEvent::HoverEnter || e->type() == QEvent::HoverLeave)
        update();
    return QAbstractButton::event(e);
}

void Btn::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const bool hover = underMouse() && isEnabled();
    QString bg;
    if (m_active) bg = m_style.activeBg;
    else if (hover) bg = m_style.hoverBg;
    if (!bg.isEmpty()) {
        p.setPen(Qt::NoPen);
        p.setBrush(resolve(bg));
        p.drawRoundedRect(QRectF(rect()), m_style.radius, m_style.radius);
    }
    QString c = m_style.color;
    if (hover && !m_style.hoverColor.isEmpty()) c = m_style.hoverColor;
    if (m_active && !m_style.activeColor.isEmpty()) c = m_style.activeColor;
    if (m_on) c = m_style.onColor;
    const QColor color = resolve(c);
    const QFont f = theme::font(m_style.fontPx, m_style.weight);
    double content = 0;
    if (!m_svg.isEmpty()) content += m_style.iconSize;
    const double tw = m_text.isEmpty() ? 0 : textWidth(f, m_text);
    if (!m_text.isEmpty()) content += (m_svg.isEmpty() ? 0 : m_style.gap) + tw;
    // Buttons centre their content (the padding is symmetric in every rule used here).
    double x = (width() - content) / 2.0;
    if (!m_style.centered && !m_style.fixedWidth && width() <= sizeHint().width())
        x = m_style.padL;
    if (!m_svg.isEmpty()) {
        const qreal dpr = devicePixelRatioF();
        const QPixmap pm = cachedIcon(m_svg, color, m_style.iconSize, dpr);
        const double y = (height() - m_style.iconSize) / 2.0;
        if (m_rotated) {
            p.save();
            p.translate(x + m_style.iconSize / 2.0, y + m_style.iconSize / 2.0);
            p.rotate(180);
            p.drawPixmap(QPointF(-m_style.iconSize / 2.0, -m_style.iconSize / 2.0), pm);
            p.restore();
        } else {
            p.drawPixmap(QPointF(x, y), pm);
        }
        x += m_style.iconSize + m_style.gap;
    }
    if (!m_text.isEmpty()) {
        const double a = text::ascent(f), d = text::descent(f);
        text::draw(p, QPointF(x, (height() - (a + d)) / 2 + a), f, m_text, color);
    }
}

// ---------------------------------------------------------------- Select

Select::Select(QWidget *parent) : QComboBox(parent)
{
    setAttribute(Qt::WA_Hover);
    setFocusPolicy(Qt::ClickFocus);
    setCursor(Qt::PointingHandCursor);
    setSizeAdjustPolicy(QComboBox::AdjustToContents);
    setModel(new QStandardItemModel(this));
}

void Select::addOption(const QString &label, const QString &value)
{
    addItem(label, value.isNull() ? label : value);
}

void Select::addGroup(const QString &label)
{
    auto *m = static_cast<QStandardItemModel *>(model());
    auto *item = new QStandardItem(label);
    item->setFlags(Qt::NoItemFlags);
    QFont f = theme::font(fontPx, 700);
    item->setFont(f);
    item->setData(QString(), Qt::UserRole);
    item->setData(true, Qt::UserRole + 1);
    m->appendRow(item);
}

QString Select::value() const
{
    return currentData(Qt::UserRole).toString();
}

void Select::setValue(const QString &value)
{
    for (int i = 0; i < count(); ++i)
        if (itemData(i, Qt::UserRole).toString() == value && !itemData(i, Qt::UserRole + 1).toBool()) {
            setCurrentIndex(i);
            return;
        }
}

QSize Select::sizeHint() const
{
    const QFont f = theme::font(fontPx);
    double w = 0;
    for (int i = 0; i < count(); ++i)
        w = std::max(w, textWidth(f, itemText(i)));
    return {int(std::ceil(w)) + padLeft + padRight, height() > 0 ? QComboBox::sizeHint().height() : 28};
}

QSize Select::minimumSizeHint() const
{
    return sizeHint();
}

void Select::paintEvent(QPaintEvent *)
{
    // The box itself (background, border) comes from the stylesheet via the frame; we paint the
    // text and the chevron ourselves so they sit where the CSS puts them.
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QStyleOptionComboBox opt;
    initStyleOption(&opt);
    opt.currentText.clear();
    opt.currentIcon = QIcon();
    style()->drawComplexControl(QStyle::CC_ComboBox, &opt, &p, this);
    {
        const QFont f = theme::font(fontPx);
        const double a = text::ascent(f), d = text::descent(f);
        p.save();
        p.setClipRect(QRectF(0, 0, width() - padRight + 6, height()));
        text::draw(p, QPointF(padLeft, (height() - (a + d)) / 2 + a), f, currentText(),
                   property("textColor").isValid() ? resolve(property("textColor").toString()) : theme::color("--text"));
        p.restore();
    }
    // Chevron: linear-gradient(45deg, transparent 50%, muted 50%) at (right-12, top) and
    // linear-gradient(135deg, muted 50%, transparent 50%) at (right-8, top), each 4x4.
    const double top = m_chevTop ? m_chevTop : (height() - 4) / 2.0;
    const double x1 = width() - m_chevRight, x2 = width() - m_chevRight + 4;
    if (m_nativeArrow) {
        // Chrome's own select arrow (appearance: auto): a small chevron 8px from the right.
        const double cx = width() - 7.5, cy = height() / 2.0 + 0.5;
        QPen pen(theme::color("--text"), 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        QPainterPath a;
        a.moveTo(cx - 4, cy - 2);
        a.lineTo(cx, cy + 2);
        a.lineTo(cx + 4, cy - 2);
        p.drawPath(a);
        return;
    }
    // The gradients draw a down-pointing "v": first square's lower-right triangle, second's lower-left.
    QPainterPath v;
    v.moveTo(x1, top);
    v.lineTo(x1 + 4, top + 4);
    v.lineTo(x1 + 4, top);
    v.closeSubpath();
    v.moveTo(x2 + 4, top);
    v.lineTo(x2, top + 4);
    v.lineTo(x2, top);
    v.closeSubpath();
    p.setPen(Qt::NoPen);
    p.setBrush(theme::color("--muted"));
    p.setRenderHint(QPainter::Antialiasing, false);
    p.drawPath(v);
}

// ---------------------------------------------------------------- ColorInput

ColorInput::ColorInput(const QString &value, QWidget *parent) : QAbstractButton(parent), m_value(value)
{
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
    setMinimumHeight(28);
    setMaximumHeight(28);
    connect(this, &QAbstractButton::clicked, this, [this] {
        const QColor c = QColorDialog::getColor(QColor(m_value), window(), QString());
        if (c.isValid()) {
            m_value = c.name();
            update();
        }
    });
}

void ColorInput::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    p.setPen(theme::color("--border"));
    p.setBrush(theme::color("--input"));
    p.drawRoundedRect(r, 4, 4);
    // Chrome's swatch: the colour inside the padding, with a thin grey outline.
    const QRectF sw = QRectF(rect()).adjusted(10, 6, -10, -6);
    p.setPen(QColor(119, 119, 119));
    p.setBrush(QColor(m_value));
    p.setRenderHint(QPainter::Antialiasing, false);
    p.drawRect(sw.adjusted(0, 0, -1, -1));
}

// ---------------------------------------------------------------- helpers

QFrame *box(const QString &cls, QWidget *parent)
{
    auto *f = new QFrame(parent);
    theme::setCls(f, cls);
    return f;
}

QLabel *label(const QString &text, const QString &cls, bool rich, QWidget *parent)
{
    auto *l = new QLabel(parent);
    l->setTextFormat(rich ? Qt::RichText : Qt::PlainText);
    l->setText(text);
    if (!cls.isEmpty())
        theme::setCls(l, cls);
    return l;
}

QWidget *field(const QString &title, QWidget *control, QWidget *extra)
{
    auto *w = new QWidget;
    w->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);   // 1fr columns: equal widths
    auto *v = new QVBoxLayout(w);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(3);
    auto *t = label(title, "field-label");
    t->setFixedHeight(15);   // 11px * line-height 1.4
    v->addWidget(t);
    // .field inputs inherit the field's 11px font
    theme::setCls(control, (control->property("cls").toString() + " fieldctl").trimmed());
    if (auto *s = qobject_cast<Select *>(control)) {
        s->fontPx = 11;
        s->setNativeArrow(true);
    }
    control->setFixedHeight(28);
    control->setMinimumWidth(0);
    v->addWidget(control);
    if (extra)
        v->addWidget(extra);   // a flex column: the extra button stretches to the field's width
    return w;
}

} // namespace qtm
