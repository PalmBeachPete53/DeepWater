#pragma once

// Small widgets shared by the app port: painted buttons that reproduce the CSS button rules exactly
// (.tb, .tf-group button, .drawbar button, legend action buttons), a select with the app's chevron,
// and a few helpers for building forms like the HTML ones.

#include <QAbstractButton>
#include <QComboBox>
#include <QFrame>
#include <QLabel>
#include <QString>

class QBoxLayout;

namespace qtm {

// The CSS of one kind of button. Colours are CSS variable names ("--text") or CSS colours.
struct BtnStyle {
    int height = 32;
    int padL = 9, padR = 9;
    int minWidth = 0;
    int fixedWidth = 0;          // 0 = from the content
    double radius = 4;
    int gap = 6;                 // between icon and text
    double fontPx = 13;
    int weight = 400;
    int iconSize = 18;
    QString color = "--text";
    QString hoverBg = "--hover";
    QString hoverColor;          // empty = color
    QString activeBg = "--hover";
    QString activeColor;         // empty = color
    QString onColor = "--accent";
    bool centered = false;       // grid / place-items: center
};

// A button painted like the HTML one: optional inline SVG icon (24x24 viewBox) and text.
// Classes: "active" (pressed look) and "on" (accent text), as in the CSS.
class Btn : public QAbstractButton
{
    Q_OBJECT
public:
    Btn(const BtnStyle &style, const QString &iconSvg, const QString &text, QWidget *parent = nullptr);
    void setActive(bool on);
    void setOn(bool on);
    bool isActive() const { return m_active; }
    void setIconSvg(const QString &svg);
    void setLabel(const QString &text);
    QString label() const { return m_text; }
    BtnStyle &btnStyle() { return m_style; }
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return sizeHint(); }
    // Rotate the icon 180 degrees (.bottom.collapsed #bottomToggle svg).
    void setIconRotated(bool on);

protected:
    void paintEvent(QPaintEvent *) override;
    bool event(QEvent *e) override;

private:
    BtnStyle m_style;
    QString m_svg;
    QString m_text;
    bool m_active = false;
    bool m_on = false;
    bool m_rotated = false;
};

// <select class="tb"> / plain <select>: the native-looking box with the app's chevron
// (two 4px gradient triangles at right-12/right-8, 14px from the top for .tb).
class Select : public QComboBox
{
    Q_OBJECT
public:
    explicit Select(QWidget *parent = nullptr);
    // Chevron position from the right edge, and its top (0 = vertically centred).
    void setChevron(int rightOffset, int top) { m_chevRight = rightOffset; m_chevTop = top; update(); }
    void addOption(const QString &label, const QString &value = QString());   // value defaults to label
    void addGroup(const QString &label);   // <optgroup label>: a disabled header row
    QString value() const;
    void setValue(const QString &value);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    int padLeft = 9, padRight = 22;
    double fontPx = 13;
    // A plain <select> (not .tb): Chrome draws its own arrow and pads the text by 4 more px.
    void setNativeArrow(bool on) { m_nativeArrow = on; if (on) { padLeft = 12; padRight = 24; } update(); }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    int m_chevRight = 12;
    int m_chevTop = 14;
    bool m_nativeArrow = false;
};

// <input type="color">: a swatch button that opens a colour picker.
class ColorInput : public QAbstractButton
{
    Q_OBJECT
public:
    explicit ColorInput(const QString &value, QWidget *parent = nullptr);
    QString value() const { return m_value; }
    void setValue(const QString &v) { m_value = v; update(); }
    QSize sizeHint() const override { return {70, 28}; }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QString m_value;
};

// A box that paints its CSS background/border via the stylesheet (QFrame with a class).
QFrame *box(const QString &cls, QWidget *parent = nullptr);
// A label with a class, rich text off unless `rich`.
QLabel *label(const QString &text, const QString &cls = QString(), bool rich = false, QWidget *parent = nullptr);
// <label class="field">Title<control></label>: a column with the 11px muted title, gap 3.
QWidget *field(const QString &title, QWidget *control, QWidget *extra = nullptr);

// Pixel size of text in a font (canvas measureText / CSS width of a text run).
double textWidth(const QFont &font, const QString &text);

} // namespace qtm
