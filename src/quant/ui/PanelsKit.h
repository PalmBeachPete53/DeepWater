#pragma once

// Small widget kit for the panels port: the pieces of HTML/CSS that panels.js, research.js,
// quantui.js and ptq.js build with innerHTML, done as widgets that lay out and paint like the
// browser does with app.css (line-height 1.4, flex rows that wrap, table.t, .stats grid, <select>).
//
// Rich text: the painted text widgets take a small subset of HTML, the one the JS writes:
// <b>, <strong>, <i>, <em>, <code>, <br>, <a class="link" href="...">, <span class="up|down|muted|...">
// and the entities &amp; &lt; &gt; &quot; &#39; &times; &#8599; &nbsp;. Class names map to the
// CSS colours (.up, .down, .muted, .hint-text, .warn, .gold-text, .impact-High ...).

#include <QColor>
#include <QComboBox>
#include <QFont>
#include <QJsonValue>
#include <QLayout>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QTextEdit>
#include <QTextLayout>
#include <QVector>
#include <QWidget>

#include <functional>

class QBoxLayout;
class QVBoxLayout;
class QHBoxLayout;

namespace qtm::kit {

// ---------------------------------------------------------------- colours and fonts

// The CSS variables in effect for a subtree: the theme's, or PTQ's brand overrides (.ptq-brand).
struct Pal {
    QColor bg, panel, border, text, muted, accent, up, down, hover, input, gold, warn;
    bool brand = false;
};
Pal pal(bool brand = false);
// A colour for a CSS class name ("up", "down", "muted", "hint-text", "warn", "impact-High" ...),
// invalid when the class sets no colour.
QColor clsColor(const QString &cls, const Pal &p);

// The body font (-apple-system) at a CSS pixel size and weight (400, 500, 600, 700).
QFont uiFont(double px, int weight = 400);
QFont monoFont(double px);        // ui-monospace, SFMono-Regular, Menlo
// PTQ brand fonts with the CSS fallbacks when they are not installed.
QFont brandSans(double px, int weight = 400);    // "Instrument Sans", "Helvetica Neue", Arial
QFont brandSerif(double px);                     // "Instrument Serif" italic, Georgia
QFont brandMono(double px);                      // "Fragment Mono", ui-monospace

// ---------------------------------------------------------------- rich text

struct Rich {
    QString text;
    QVector<QTextLayout::FormatRange> formats;
    struct Link { int start, length; QString href; };
    QVector<Link> links;
    struct Code { int start, length; };
    QVector<Code> codes;     // <code> runs: painted with a rounded background
};
// Parses the HTML subset into text + formats. `base` is the font, `color` the default colour.
Rich parseRich(const QString &html, const QFont &base, const QColor &color, const Pal &p);

// Text style of a block (CSS of the element the JS writes).
struct TextStyle {
    QFont font = uiFont(13);
    QColor color;               // invalid = pal.text
    double lineHeight = 1.4;    // multiple of the font's pixel size (CSS line-height)
    bool wrap = true;           // white-space: normal
    int clamp = 0;              // -webkit-line-clamp
    bool ellipsis = false;      // text-overflow: ellipsis (single line)
    Qt::Alignment align = Qt::AlignLeft;
    bool upper = false;         // text-transform: uppercase
    bool brand = false;         // inside .ptq-brand
    QColor codeBg;              // <code> background (invalid = pal.hover)
    QFont codeFont = monoFont(12);
};

// A block of rich text, laid out like the browser (line boxes of lineHeight * px with the glyphs
// centred in them). Height follows the width. Links emit linkActivated(href).
class Text : public QWidget
{
    Q_OBJECT
public:
    Text(const QString &html, const TextStyle &style, QWidget *parent = nullptr);
    void setHtml(const QString &html);
    QString html() const { return m_html; }
    void setStyle(const TextStyle &style);
    const TextStyle &textStyle() const { return m_style; }
    void setColor(const QColor &c);

    bool hasHeightForWidth() const override { return m_style.wrap; }
    int heightForWidth(int w) const override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    double naturalWidth() const;     // unwrapped width of the longest line

signals:
    void linkActivated(const QString &href);

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mousePressEvent(QMouseEvent *) override;

private:
    struct Laid;
    void layoutFor(double width, QVector<QTextLayout *> *out, double *height) const;
    QString linkAt(const QPoint &pos) const;
    QString m_html;
    TextStyle m_style;
    Rich m_rich;
};

// Shorthands for the common text blocks. Margins are CSS margins (top, bottom) in px.
Text *text(const QString &html, double px = 13, const QString &color = QString(), int weight = 400);
Text *hint(const QString &html, bool brand = false);                // .hint-text (12px muted)
Text *para(const QString &html, double px = 13, bool brand = false); // <p>
// <h4>: 12px 600 muted uppercase, letter-spacing .04em.
Text *h4(const QString &text, bool brand = false);

// ---------------------------------------------------------------- layout

// .row-flex: a flex row that wraps (gap 8, align-items center). Per-widget properties:
//   "flexGrow" (int)      flex: n, shares the line's free space
//   "alignSelf" ("end", "start", "stretch")
//   "marginLeftAuto" (bool) margin-left: auto (pushes the item to the end of its line)
class FlowLayout : public QLayout
{
public:
    explicit FlowLayout(QWidget *parent = nullptr, int gapX = 8, int gapY = 8);
    ~FlowLayout() override;
    void setWrap(bool on) { m_wrap = on; invalidate(); }
    void setAlignItems(Qt::Alignment a) { m_alignItems = a; invalidate(); }   // AlignVCenter, AlignTop, AlignBottom
    void addSpacing(int width);   // a gap on the line; dropped when it would open one

    void addItem(QLayoutItem *item) override;
    int count() const override;
    QLayoutItem *itemAt(int index) const override;
    QLayoutItem *takeAt(int index) override;
    Qt::Orientations expandingDirections() const override { return {}; }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override;
    QSize minimumSize() const override;
    QSize sizeHint() const override;
    void setGeometry(const QRect &rect) override;

private:
    int doLayout(const QRect &rect, bool testOnly) const;
    QList<QLayoutItem *> m_items;
    int m_gapX, m_gapY;
    bool m_wrap = true;
    Qt::Alignment m_alignItems = Qt::AlignVCenter;
};

// The row of a bar at the top of a page: tabs and controls side by side, on more lines when the width runs out. The bar's
// height follows through setFixedHeight rather than height-for-width: a page holding a bar in a stack would otherwise take
// the preferred height of everything in that stack as its minimum. A control pushed to the right has "marginLeftAuto".
class WrapBarLayout : public FlowLayout
{
public:
    explicit WrapBarLayout(QWidget *bar, int gap = 2, int lineGap = 0);
    bool hasHeightForWidth() const override { return false; }
    void setGeometry(const QRect &rect) override;
};

// A widget with a .row-flex layout. `margins` are the padding.
QWidget *rowFlex(QWidget *parent = nullptr, int gap = 8);
FlowLayout *flow(QWidget *w);
// A vertical block (display: block children stacked). Padding as margins, spacing 0.
QWidget *block(QWidget *parent = nullptr, const QMargins &padding = {});
QVBoxLayout *vbox(QWidget *w);
// flex: 1 spacer for row-flex.
QWidget *spacer();
// Adds `w` to a block with CSS vertical margins (no collapsing; pass the collapsed values).
void add(QWidget *blockWidget, QWidget *w, int marginTop = 0, int marginBottom = 0);
void addSpace(QWidget *blockWidget, int px);
void clear(QLayout *layout);   // deletes every item and widget

// ---------------------------------------------------------------- controls

// <button class="btn ...">
QPushButton *btn(const QString &text, const QString &cls = "btn", QWidget *parent = nullptr);

// <select>: drawn like Chrome's menulist (border, background, chevron). Options can be grouped
// (<optgroup>) and carry a value distinct from their label.
class Select : public QComboBox
{
    Q_OBJECT
public:
    enum Kind { In, Field, Rule };   // select.in / .field select (11px) / .rule select
    explicit Select(Kind kind = In, QWidget *parent = nullptr);
    void addOption(const QString &value, const QString &label);
    void addOption(const QString &valueAndLabel) { addOption(valueAndLabel, valueAndLabel); }
    void addGroup(const QString &label);   // <optgroup label>: the options added after belong to it
    void endGroup() { m_inGroup = false; }
    QString value() const;
    void setValue(const QString &value);   // no-op when missing (like el.value = x)
    void setBrand(bool on) { m_brand = on; update(); }
    void setMaxWidthPx(int w) { m_maxW = w; updateGeometry(); }
    void setFontPx(double px) { m_px = px; updateGeometry(); update(); }
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void changed(const QString &value);   // the "change" event (user picks an option)

protected:
    void paintEvent(QPaintEvent *) override;

private:
    Kind m_kind;
    double m_px;
    int m_maxW = 0;
    bool m_brand = false;
    bool m_inGroup = false;
};

// <input class="in"> / .field input / .rule input. `number` gives type=number behaviour
// (Number(value), "" when the text is not a number).
class Input : public QLineEdit
{
    Q_OBJECT
public:
    enum Kind { In, Field, Rule };
    explicit Input(Kind kind = In, bool number = false, QWidget *parent = nullptr);
    double number() const;          // JS Number(el.value): "" -> 0, invalid -> NaN (type=number gives "")
    bool isNumber() const { return m_number; }
    void setWidthPx(int w);         // style="width:...px"
    QSize sizeHint() const override;

private:
    bool m_number;
    Kind m_kind;
    int m_w = 0;
};

// <textarea class="in">: monospace 12px, line-height 1.5, soft wrap, overlay scrollbars.
// `tabIndents`: Tab inserts two spaces (the indicator editor) instead of moving the focus.
class CodeArea : public QTextEdit
{
    Q_OBJECT
public:
    explicit CodeArea(bool tabIndents = false, QWidget *parent = nullptr);
    void setCode(const QString &code);
    QString code() const { return toPlainText(); }

protected:
    void keyPressEvent(QKeyEvent *e) override;
    void insertFromMimeData(const QMimeData *source) override;

private:
    void applyLineHeight();
    bool m_tab;
};

// <label class="field">Label<input|select></label>: label text over the control, gap 3.
QWidget *field(const QString &label, QWidget *control, bool brand = false);
// The row variant: style="flex-direction:row;align-items:center;gap:6px".
QWidget *fieldRow(const QString &label, QWidget *control);

// ---------------------------------------------------------------- table.t

struct Cell {
    QString html;           // rich text (HTML subset)
    QString cls;            // "l", "up", "down", "muted", "base" ...
    QColor bg;              // style="background:..."
    QString button;         // a <button class="btn small ..."> instead of text
    QString buttonCls;      // e.g. "danger"
    QString data;           // data-* value carried by the cell's button
    int colspan = 1;
    bool bold = false;      // font-weight (tr.seas-sum, td.base)
    bool topBorder = false; // tr.seas-sum td: border-top
    bool outline = false;   // td.base in the heat table: 2px gold outline
};
struct Row {
    QVector<Cell> cells;
    QString key;            // data-s / data-t
};

// table.t: header + rows painted like the browser's auto table layout (width: 100%), rows
// highlighted on hover, header sticky inside the nearest scroll area.
class Table : public QWidget
{
    Q_OBJECT
public:
    enum Variant { Normal, Seas, Heat };
    explicit Table(QWidget *parent = nullptr);
    void setVariant(Variant v) { m_variant = v; relayout(); }
    void setWrap(bool on) { m_wrap = on; relayout(); }   // white-space: normal (tester right, gold box, ptq box)
    void setBrand(bool on) { m_brand = on; relayout(); }
    // position: sticky works against the nearest scroll container; inside .gold-box (overflow-x:
    // auto) or .ptq-box (overflow: hidden) the header never sticks.
    void setSticky(bool on) { m_sticky = on; update(); }
    void setHeader(const QVector<Cell> &cells);
    void setRows(const QVector<Row> &rows);
    const QVector<Row> &rows() const { return m_rows; }

    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int w) const override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void headerClicked(int column);
    void rowClicked(int row);
    void buttonClicked(int row, int column, const QString &data);

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void leaveEvent(QEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void showEvent(QShowEvent *) override;

private:
    struct Geo;
    struct Measure;
    void relayout();
    const Geo &geo(int width) const;
    const Measure &measure() const;
    QFont cellFont(const Cell &c, bool header) const;
    QRectF buttonRect(int row, int col, const Geo &g) const;
    double stickyOffset() const;
    Variant m_variant = Normal;
    bool m_wrap = false, m_brand = false, m_sticky = true;
    QVector<Cell> m_header;
    QVector<Row> m_rows;
    int m_hoverRow = -1;
    QPoint m_mouse;
    // Laid out for the last few widths asked (most recent first): nested layouts ask the height
    // for several widths in turn. The columns' text widths don't depend on it.
    mutable QVector<std::shared_ptr<Geo>> m_geos;
    mutable std::shared_ptr<Measure> m_measure;
    QPointer<QScrollArea> m_scroll;
};

// ---------------------------------------------------------------- .stats grid

struct Stat {
    QString k, v, cls;
};
class Stats : public QWidget
{
    Q_OBJECT
public:
    explicit Stats(QWidget *parent = nullptr);
    void setItems(const QVector<Stat> &items);
    void setMinColumn(int px) { m_min = px; updateGeometry(); update(); }   // minmax(130px, 1fr)
    void setBrand(bool on) { m_brand = on; update(); }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int w) const override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return {0, 0}; }

protected:
    void paintEvent(QPaintEvent *) override;

private:
    int columns(int w) const;
    QVector<Stat> m_items;
    int m_min = 130;
    bool m_brand = false;
};

// ---------------------------------------------------------------- containers

// A painted box: background, 1px border (or a custom width), radius, plus optional shadow ring
// (.gold-box box-shadow 0 0 0 3px). Children go in its layout, with the CSS padding as margins.
class Box : public QWidget
{
    Q_OBJECT
public:
    explicit Box(QWidget *parent = nullptr);
    QColor bg, border, ring;
    double borderWidth = 1, radius = 6, ringWidth = 0;
    bool borderLeftOnly = false;   // .ptq-arm / .next-step: border-left: 2px
    std::function<void(QPainter &, const QRectF &)> paintBackground;   // gradients
    bool hover = false;            // a.card.news:hover border colour
    QColor hoverBorder;

protected:
    void paintEvent(QPaintEvent *) override;
    void enterEvent(QEnterEvent *) override;
    void leaveEvent(QEvent *) override;

private:
    bool m_in = false;
};
// div.card (side panel cards): margin is the caller's; padding 8 10.
Box *card(QWidget *parent = nullptr);

// A scrolling column (overflow-y: auto) holding a block with `padding`.
class ScrollCol : public QScrollArea
{
    Q_OBJECT
public:
    explicit ScrollCol(const QMargins &padding = {}, QWidget *parent = nullptr);
    // The content block: its children stack from the top at their real (height-for-width) size.
    QWidget *body() const { return m_inner; }
    QVBoxLayout *layout() const;
    // fill: the content block takes at least the column's height (flex: 1 children can grow).
    void setFill(bool on);

protected:
    // The page's scrollbars are overlay ones (scrollbar-width: thin, transparent until hovered):
    // they take no space. The real bar is hidden; an overlay bar shows while the mouse is over.
    void resizeEvent(QResizeEvent *) override;
    void enterEvent(QEnterEvent *) override;
    void leaveEvent(QEvent *) override;

private:
    void placeBar();
    QWidget *m_body;
    QWidget *m_inner;
    class QScrollBar *m_bar = nullptr;
};

// .tester: a grid of minmax(330px, 420px) and 1fr, each column scrolling on its own.
class Split : public QWidget
{
    Q_OBJECT
public:
    explicit Split(QWidget *parent = nullptr, bool brand = false);
    ScrollCol *left, *right;
    double leftRatio = 0;   // > 0: two fr columns (.seas: 1.3fr / 1fr) instead of the tester grid
    QColor divider;         // border-right of the left column
    std::function<void(QPainter &, const QRectF &)> paintBackground;

protected:
    void resizeEvent(QResizeEvent *) override;
    void paintEvent(QPaintEvent *) override;
};

// ---------------------------------------------------------------- misc

// JS Date#toLocale*String in en-US, local time (what the reference browser shows).
QString localeString(double ms);                // 10/29/2026, 1:05:09 PM
QString localeDateString(double ms);            // 10/29/2026
QString localeTimeHM(double ms);                // 01:05 PM (hour: "2-digit", minute: "2-digit")
QString weekdayDayMonth(double ms, bool utc = false);   // Wednesday, Oct 28
QString monthYear(double ms);                   // Oct 2026
QString monthDayUtc(double sec);                // Jan 5 (month: "short", day: "numeric", timeZone: "UTC")
QString intLocale(double v);                    // 12,345 (Number#toLocaleString)
QString num(double v);                          // String(x)
QString numPrecision(double v, int p);          // Number(x.toPrecision(p)) as String

// window.confirm / window.prompt.
bool confirm(QWidget *parent, const QString &text);
QString prompt(QWidget *parent, const QString &text, bool *ok = nullptr);

// Opens a URL in the browser (target="_blank").
void openUrl(const QString &url);

} // namespace qtm::kit
