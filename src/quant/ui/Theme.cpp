#include "Theme.h"

#include "../core/JsCompat.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QHash>
#include <QLocale>
#include <QPainter>
#include <QPixmap>
#include <QRegularExpression>
#include <QStyle>
#include <QSvgRenderer>
#include <QVariant>
#include <QWidget>

#include <cmath>

namespace qtm::theme {

namespace {

// The CSS variables of app.css, as written there.
const QHash<QString, QString> &darkVars()
{
    static const QHash<QString, QString> v = {
        {"--bg", "#131722"},         {"--panel", "#1e222d"},  {"--panel-2", "#2a2e39"},
        {"--border", "#2a2e39"},     {"--text", "#d1d4dc"},   {"--muted", "#787b86"},
        {"--accent", "#2962ff"},     {"--accent-soft", "rgba(41, 98, 255, 0.18)"},
        {"--up", "#089981"},         {"--down", "#f23645"},   {"--hover", "#2a2e39"},
        {"--grid", "rgba(42, 46, 57, 0.6)"},                  {"--input", "#131722"},
        {"--gold", "#d4a514"},       {"--gold-soft", "rgba(212, 165, 20, 0.16)"},
        {"--scroll", "#363a45"},
        // fixed in the original (Lightweight Charts defaults, white button text ...)
        {"--on-accent", "#ffffff"},  {"--tooltip", "#1e222d"}, {"--tooltip-border", "#2a2e39"},
        {"--crosshair", "#9598a1"},  {"--crosshair-label", "#131722"},  {"--short", "#ff6d00"},
    };
    return v;
}

const QHash<QString, QString> &lightVars()
{
    static const QHash<QString, QString> v = {
        {"--bg", "#ffffff"},         {"--panel", "#f8f9fd"},  {"--panel-2", "#eef0f6"},
        {"--border", "#e0e3eb"},     {"--text", "#131722"},   {"--muted", "#6a6d78"},
        {"--accent", "#2962ff"},     {"--accent-soft", "rgba(41, 98, 255, 0.12)"},
        {"--up", "#089981"},         {"--down", "#f23645"},   {"--hover", "#eef0f6"},
        {"--grid", "rgba(224, 227, 235, 0.7)"},               {"--input", "#ffffff"},
        {"--gold", "#b8860b"},       {"--gold-soft", "rgba(184, 134, 11, 0.14)"},
        {"--scroll", "#c9cdd6"},
        {"--on-accent", "#ffffff"},  {"--tooltip", "#f8f9fd"}, {"--tooltip-border", "#e0e3eb"},
        {"--crosshair", "#9598a1"},  {"--crosshair-label", "#131722"},  {"--short", "#ff6d00"},
    };
    return v;
}

QString g_current = "dark";
// The host palette (setPalette), merged over the built-in one of the same lightness.
QHash<QString, QString> g_palette;
bool g_paletteLight = false;

const QHash<QString, QString> &vars()
{
    if (!g_palette.isEmpty())
        return g_palette;
    return g_current == "light" ? lightVars() : darkVars();
}

// QSS takes rgba() with 0-255 alpha or a fraction; normalise to #aarrggbb-free rgba(r,g,b,a 0-255).
QString qssColor(const QString &cssValue)
{
    const QColor c = parseColor(cssValue);
    if (c.alpha() == 255)
        return c.name();
    return QString("rgba(%1, %2, %3, %4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(c.alpha());
}

// The stylesheet: app.css rules for the widgets the port builds. @var tokens are replaced with
// the current theme's colours. Selectors use the "cls" property for CSS classes.
const char *const Template = R"QSS(
#qtmRoot, #qtmRoot QWidget { color: @text; font-size: 13px; }
#qtmRoot { background: @bg; }
#qtmRoot QToolTip { background: @tooltip; color: @text; border: 1px solid @tooltipBorder; padding: 4px 6px; }

/* scrollbars: thin, transparent track */
#qtmRoot QScrollBar:vertical { background: transparent; width: 8px; margin: 0; }
#qtmRoot QScrollBar:horizontal { background: transparent; height: 8px; margin: 0; }
#qtmRoot QScrollBar::handle { background: @scroll; border-radius: 4px; min-height: 24px; min-width: 24px; }
#qtmRoot QScrollBar::add-line, #qtmRoot QScrollBar::sub-line { width: 0; height: 0; background: none; }
#qtmRoot QScrollBar::add-page, #qtmRoot QScrollBar::sub-page { background: none; }
#qtmRoot QScrollArea { background: transparent; border: 0; }
#qtmRoot QScrollArea > QWidget > QWidget { background: transparent; }

/* text helpers */
#qtmRoot QLabel[cls~="muted"], #qtmRoot QLabel[cls~="hint-text"] { color: @muted; }
#qtmRoot QLabel[cls~="hint-text"] { font-size: 12px; }
#qtmRoot QLabel[cls~="up"] { color: @up; }
#qtmRoot QLabel[cls~="down"] { color: @down; }
#qtmRoot QLabel[cls~="gold-text"] { color: @gold; }
#qtmRoot QLabel[cls~="h4"] { font-size: 12px; font-weight: 600; color: @muted; }
#qtmRoot QLabel[cls~="link"] { color: @accent; }
#qtmRoot QLabel[cls~="field-label"] { font-size: 11px; color: @muted; }

/* top bar buttons (.tb) */
#qtmRoot QPushButton[cls~="tb"], #qtmRoot QToolButton[cls~="tb"] {
    background: transparent; border: 0; border-radius: 4px; padding: 0 9px; min-height: 32px; max-height: 32px;
}
#qtmRoot QPushButton[cls~="tb"]:hover, #qtmRoot QToolButton[cls~="tb"]:hover,
#qtmRoot QPushButton[cls~="tb"][cls~="active"], #qtmRoot QToolButton[cls~="tb"][cls~="active"] { background: @hover; }
#qtmRoot QPushButton[cls~="tb"][cls~="on"], #qtmRoot QToolButton[cls~="tb"][cls~="on"] { color: @accent; }
#qtmRoot QPushButton[cls~="tb"][cls~="icon"], #qtmRoot QToolButton[cls~="tb"][cls~="icon"] { padding: 0 7px; }
#qtmRoot QPushButton[cls~="symbol-btn"] { font-weight: 600; font-size: 14px; }

/* buttons (.btn) */
#qtmRoot QPushButton[cls~="btn"] {
    min-height: 26px; max-height: 26px; padding: 0 12px; border-radius: 4px;
    border: 1px solid @border; background: @panel; color: @text;
}
#qtmRoot QPushButton[cls~="btn"]:hover { background: @hover; }
#qtmRoot QPushButton[cls~="btn"][cls~="primary"] { background: @accent; border-color: @accent; color: @onAccent; }
#qtmRoot QPushButton[cls~="btn"][cls~="primary"]:hover { background: @accentHover; }
#qtmRoot QPushButton[cls~="btn"][cls~="small"] { min-height: 22px; max-height: 22px; padding: 0 8px; font-size: 12px; }
#qtmRoot QPushButton[cls~="btn"][cls~="danger"] { color: @down; }
#qtmRoot QPushButton[cls~="btn"][cls~="fieldbtn"] { color: @muted; }   /* a button inside a .field inherits its colour */
#qtmRoot QPushButton[cls~="btn"][cls~="gold"] { background: @gold; border-color: @gold; color: #1a1405; font-weight: 600; }

/* inputs (.in, .field input/select, .search-in) */
#qtmRoot QLineEdit[cls~="in"], #qtmRoot QComboBox[cls~="in"], #qtmRoot QSpinBox[cls~="in"],
#qtmRoot QDoubleSpinBox[cls~="in"], #qtmRoot QDateTimeEdit[cls~="in"] {
    min-height: 26px; max-height: 26px; border: 1px solid @border; border-radius: 4px;
    background: @input; padding: 0 7px; color: @text; selection-background-color: @accent; selection-color: @onAccent;
}
#qtmRoot QPlainTextEdit[cls~="in"], #qtmRoot QTextEdit[cls~="in"] {
    border: 1px solid @border; border-radius: 4px; background: @input; padding: 8px; color: @text;
    font-family: Menlo, "SF Mono", monospace; font-size: 12px; selection-background-color: @accent; selection-color: @onAccent;
}
#qtmRoot QLineEdit[cls~="search-in"] {
    min-height: 36px; max-height: 36px; font-size: 15px; border: 1px solid @border; border-radius: 6px;
    background: @input; padding: 0 12px; color: @text; selection-background-color: @accent; selection-color: @onAccent;
}
#qtmRoot QComboBox[cls~="in"]::drop-down, #qtmRoot QComboBox[cls~="tb"]::drop-down { border: 0; width: 18px; }
#qtmRoot QComboBox QAbstractItemView { background: @panel; color: @text; border: 1px solid @border;
    selection-background-color: @hover; selection-color: @text; outline: 0; }

/* tabs (.tabs button) */
#qtmRoot QWidget[cls~="tabs"] { border-bottom: 1px solid @border; }
#qtmRoot QPushButton[cls~="tab"] {
    border: 0; background: transparent; min-height: 30px; max-height: 30px; padding: 0 10px;
    border-radius: 4px; color: @muted;
}
#qtmRoot QPushButton[cls~="tab"]:hover { background: @hover; color: @text; }
#qtmRoot QPushButton[cls~="tab"][cls~="active"] { background: @hover; color: @text; }
#qtmRoot QPushButton[cls~="tab"][cls~="ptq"] { color: #6cb8ff; font-weight: 600; }

/* cards, stats */
#qtmRoot QFrame[cls~="card"] { border: 1px solid @border; border-radius: 6px; background: @panel; }
#qtmRoot QFrame[cls~="stat"] { border: 1px solid @border; border-radius: 6px; background: @panel; }
#qtmRoot QLabel[cls~="stat-k"] { color: @muted; font-size: 11px; }
#qtmRoot QLabel[cls~="stat-v"] { font-size: 16px; font-weight: 600; }
#qtmRoot QFrame[cls~="gold-box"] { border: 1.5px solid @gold; border-radius: 8px; background: @panel; }
#qtmRoot QLabel[cls~="gold-badge"] { background: @gold; color: #1a1405; font-weight: 700; font-size: 11px;
    padding: 1px 7px; border-radius: 3px; }
#qtmRoot QLabel[cls~="next-step"] { font-size: 12px; color: @muted; border-left: 2px solid @accent; padding: 4px 10px; }

/* tables (table.t) */
#qtmRoot QTableWidget[cls~="t"], #qtmRoot QTableView[cls~="t"] {
    background: transparent; border: 0; gridline-color: transparent; font-size: 12px; outline: 0;
    selection-background-color: transparent; selection-color: @text;
}
#qtmRoot QTableWidget[cls~="t"]::item, #qtmRoot QTableView[cls~="t"]::item { border-bottom: 1px solid @border; padding: 0 8px; }
#qtmRoot QTableWidget[cls~="t"]::item:hover, #qtmRoot QTableView[cls~="t"]::item:hover { background: @hover; }
#qtmRoot QHeaderView::section { background: @bg; color: @muted; font-weight: 500; font-size: 12px; padding: 6px 8px;
    border: 0; border-bottom: 1px solid @border; }

/* rule builder (.rule) */
#qtmRoot QFrame[cls~="rule"] { background: @panel; border: 1px solid @border; border-radius: 6px; }
#qtmRoot QFrame[cls~="rule"] QComboBox, #qtmRoot QFrame[cls~="rule"] QLineEdit {
    min-height: 24px; max-height: 24px; border: 1px solid @border; border-radius: 4px; background: @input; padding: 0 5px;
}

/* modal (.modal, .modal-card) */
#qtmRoot QWidget#qtmModalBackdrop { background: rgba(0, 0, 0, 115); }
#qtmRoot QWidget#qtmModalCard { background: @panel; border: 1px solid @border; border-radius: 8px; }
#qtmRoot QWidget[cls~="modal-head"] { border-bottom: 1px solid @border; }
#qtmRoot QLabel[cls~="modal-title"] { font-weight: 600; font-size: 15px; }
#qtmRoot QWidget[cls~="modal-foot"] { border-top: 1px solid @border; }

/* toasts */
#qtmRoot QFrame[cls~="toast"] { background: @panel; border: 1px solid @border; border-left: 3px solid @accent; border-radius: 6px; }
#qtmRoot QFrame[cls~="toast"][cls~="err"] { border-left-color: @down; }
#qtmRoot QFrame[cls~="toast"][cls~="alert"] { border-left-color: #f7a600; }

/* app shell (index.html) */
#qtmRoot QWidget#qtmTopbar { background: @bg; border-bottom: 1px solid @border; }
#qtmRoot QWidget#qtmDrawbar { border-right: 1px solid @border; }
#qtmRoot QWidget#qtmGrid { background: @border; }
#qtmRoot QWidget#qtmPane { background: @bg; }
#qtmRoot QLabel#qtmPaneMsg { color: @muted; background: transparent; }
#qtmRoot QLineEdit#tfCustom { border: 1px solid @border; border-radius: 4px; background: @input; padding: 0 6px;
    color: @text; selection-background-color: @accent; selection-color: @onAccent; }
#qtmRoot QComboBox { combobox-popup: 0; }
#qtmRoot QComboBox::drop-down { width: 0; border: 0; }
#qtmRoot QComboBox::down-arrow { image: none; width: 0; height: 0; }
#qtmRoot QComboBox[cls~="tb"] { background: transparent; border: 0; border-radius: 4px; padding: 0; color: @text; }
#qtmRoot QComboBox[cls~="tb"]:hover { background: @hover; }
#qtmRoot QCheckBox[cls~="check"] { color: @muted; padding: 0 9px 0 13px; border-radius: 4px; spacing: 8px; background: transparent; }
#qtmRoot QCheckBox[cls~="check"]:hover { background: @hover; }
#qtmRoot QFrame[cls~="replay-bar"] { background: @panel; border: 1px solid @border; border-radius: 6px; }
#qtmRoot QLabel[cls~="hint"] { color: @muted; padding: 0 6px; background: transparent; }
#qtmRoot QCheckBox[cls~="hint"] { color: @muted; padding: 0 6px 0 10px; spacing: 8px; background: transparent; }
#qtmRoot QComboBox#qtmReplaySpeed { background: transparent; border: 0; border-radius: 4px; padding: 0; color: @text; }
#qtmRoot QFrame[cls~="item"] { border-radius: 4px; background: transparent; }
#qtmRoot QFrame[cls~="item"]:hover, #qtmRoot QFrame[cls~="item"][cls~="sel"] { background: @hover; }
#qtmRoot QLabel[cls~="meta"] { color: @muted; font-size: 12px; background: transparent; }
#qtmRoot QLabel[cls~="group"] { color: @muted; font-size: 11px; }
#qtmRoot QLabel[cls~="group"][cls~="gold-text"] { color: @gold; }
#qtmRoot QLabel[cls~="toast-text"] { background: transparent; }
#qtmRoot QWidget#qtmModalBody { background: transparent; }
#qtmRoot QLineEdit[cls~="fieldctl"], #qtmRoot QDateTimeEdit[cls~="fieldctl"] { font-size: 11px; }
#qtmRoot QComboBox[cls~="in"] { min-height: 26px; max-height: 26px; border: 1px solid @border; border-radius: 4px;
    background: @input; padding: 0; color: @text; }
#qtmRoot QScrollBar::handle { background: transparent; }
#qtmRoot QScrollBar[hover="true"]::handle { background: @scroll; }

/* checkboxes */
#qtmRoot QCheckBox { spacing: 5px; }
#qtmRoot QCheckBox::indicator { width: 13px; height: 13px; image: url(@checkOff); }
#qtmRoot QCheckBox::indicator:hover { image: url(@checkOffHover); }
#qtmRoot QCheckBox::indicator:checked { image: url(@checkOn); }
#qtmRoot QCheckBox::indicator:checked:hover { image: url(@checkOnHover); }
)QSS";

} // namespace

void setCurrent(const QString &name)
{
    g_current = name == "light" ? "light" : "dark";
}

QString current()
{
    if (!g_palette.isEmpty())
        return g_paletteLight ? "light" : "dark";
    return g_current;
}

void setPalette(const QHash<QString, QString> &v, bool light)
{
    g_palette = light ? lightVars() : darkVars();
    for (auto it = v.begin(); it != v.end(); ++it)
        g_palette.insert(it.key(), it.value());
    g_paletteLight = light;
    // The port's fixed colours, from the palette when it doesn't give them.
    auto derive = [&](const char *name, const QString &value) {
        if (!v.contains(name)) g_palette.insert(name, value);
    };
    derive("--tooltip", g_palette.value("--panel"));
    derive("--tooltip-border", g_palette.value("--border"));
    derive("--crosshair", g_palette.value("--muted"));
    derive("--crosshair-label", g_palette.value("--panel-2"));
    // Some accents are light (gold, cyan): white text only where it reads.
    derive("--on-accent", textOn(color("--accent")).name());
    // Short entries: the accent's complementary hue, as the original's orange is to its blue.
    const QColor a = color("--accent");
    derive("--short", QColor::fromHslF(std::fmod(std::max(0.0f, a.hslHueF()) + 0.5f, 1.0f), a.hslSaturationF(),
                                       a.lightnessF()).name());
}

void clearPalette()
{
    g_palette.clear();
}

bool hasPalette()
{
    return !g_palette.isEmpty();
}

QColor parseColor(const QString &cssColor)
{
    const QString s = cssColor.trimmed();
    static const QRegularExpression rgba(R"(^rgba?\(\s*([\d.]+)\s*,\s*([\d.]+)\s*,\s*([\d.]+)\s*(?:,\s*([\d.]+)\s*)?\)$)");
    const auto m = rgba.match(s);
    if (m.hasMatch()) {
        QColor c(qRound(m.captured(1).toDouble()), qRound(m.captured(2).toDouble()), qRound(m.captured(3).toDouble()));
        if (!m.captured(4).isEmpty())
            c.setAlphaF(float(m.captured(4).toDouble()));
        return c;
    }
    if (s.startsWith('#') && s.size() == 9) {   // #rrggbbaa (CSS order)
        QColor c(s.left(7));
        c.setAlpha(s.mid(7, 2).toInt(nullptr, 16));
        return c;
    }
    if (s.startsWith('#') && s.size() == 5) {   // #rgba
        QColor c(QString("#%1%1%2%2%3%3").arg(s[1]).arg(s[2]).arg(s[3]));
        c.setAlpha(QString("%1%1").arg(s[4]).toInt(nullptr, 16));
        return c;
    }
    return QColor(s);   // #rgb, #rrggbb and named colours
}

QColor color(const QString &var)
{
    return parseColor(vars().value(var));
}

QString css(const QString &var)
{
    return vars().value(var);
}

QColor color(const QString &var, double a)
{
    QColor c = color(var);
    c.setAlphaF(float(a));
    return c;
}

namespace {
// WCAG relative luminance and contrast ratio.
double luminance(const QColor &c)
{
    auto ch = [](float v) { return v <= 0.03928f ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
    return 0.2126 * ch(c.redF()) + 0.7152 * ch(c.greenF()) + 0.0722 * ch(c.blueF());
}
double contrast(const QColor &a, const QColor &b)
{
    const double la = luminance(a), lb = luminance(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}
} // namespace

QColor textOn(const QColor &background)
{
    const QColor white(Qt::white);
    if (g_palette.isEmpty())
        return white;
    // The theme's dark colour: the background of a dark theme, the text of a light one.
    const QColor bg = color("--bg"), text = color("--text");
    const QColor dark = luminance(bg) < luminance(text) ? bg : text;
    return contrast(background, dark) > contrast(background, white) ? dark : white;
}

QColor shadowColor()
{
    return current() == "light" ? QColor(0, 0, 0, 38) : QColor(0, 0, 0, 115);
}

QString styleSheet()
{
    QString s = QString::fromUtf8(Template);
    // .btn.primary:hover is `filter: brightness(1.1)`.
    QColor accentHover = color("--accent");
    accentHover = QColor::fromHsvF(accentHover.hsvHueF(), accentHover.hsvSaturationF(),
                                   std::min(1.0f, accentHover.valueF() * 1.1f));
    s.replace("@accentHover", accentHover.name());
    s.replace("@onAccent", qssColor(css("--on-accent")));
    s.replace("@tooltipBorder", qssColor(css("--tooltip-border")));
    s.replace("@tooltip", qssColor(css("--tooltip")));
    // Longest names first so "@panel-2" isn't eaten by "@panel".
    const QStringList names = {"accent-soft", "gold-soft", "panel-2", "border", "accent", "scroll", "muted",
                               "input", "panel", "hover", "text", "grid", "gold", "down", "up", "bg"};
    for (const QString &n : names) {
        QString v = qssColor(css("--" + n));
        s.replace("@" + n, v);
    }
    // Chrome's checkbox (color-scheme dark / light), as image files the stylesheet can point at.
    // A host palette gets the same box in its own colours.
    const bool dark = g_current != "light";
    struct Box { const char *name; QString fill, border, check; };
    QVector<Box> boxes = {
        {"checkOff", dark ? "#3b3b3b" : "#ffffff", dark ? "#858585" : "#767676", QString()},
        {"checkOffHover", dark ? "#3b3b3b" : "#ffffff", dark ? "#acacac" : "#4f4f4f", QString()},
        {"checkOn", dark ? "#99c8ff" : "#0075ff", dark ? "#99c8ff" : "#0075ff", dark ? "#3b3b3b" : "#ffffff"},
        {"checkOnHover", dark ? "#d1e6ff" : "#005cc8", dark ? "#d1e6ff" : "#005cc8", dark ? "#3b3b3b" : "#ffffff"},
    };
    if (hasPalette()) {
        const QString input = color("--input").name(), muted = color("--muted").name(), text = color("--text").name();
        const QString accent = color("--accent").name(), on = color("--on-accent").name();
        boxes = {
            {"checkOff", input, muted, QString()},
            {"checkOffHover", input, text, QString()},
            {"checkOn", accent, accent, on},
            {"checkOnHover", accentHover.name(), accentHover.name(), on},
        };
    }
    const QString dir = QDir::tempPath() + "/qtm-ui-theme";
    QDir().mkpath(dir);
    for (const Box &b : boxes) {
        QString svg = QString("<svg xmlns='http://www.w3.org/2000/svg' width='13' height='13' viewBox='0 0 13 13'>"
                              "<rect x='0.5' y='0.5' width='12' height='12' rx='2' fill='%1' stroke='%2'/>").arg(b.fill, b.border);
        if (!b.check.isEmpty())
            svg += QString("<path d='M3 6.8l2.3 2.3L10.2 4' fill='none' stroke='%1' stroke-width='1.8' stroke-linecap='round' stroke-linejoin='round'/>").arg(b.check);
        svg += "</svg>";
        // Named after the colours, so each palette gets its own files.
        const QString file = QString("%1/%2-%3.svg").arg(dir, b.name, QString(b.fill + b.border + b.check).remove('#'));
        if (!QFile::exists(file)) {
            QFile f(file);
            if (f.open(QIODevice::WriteOnly)) f.write(svg.toUtf8());
        }
        s.replace("@" + QString(b.name) + ")", file + ")");
    }
    return s + panelsStyleSheet();
}

void setCls(QWidget *w, const QString &classes)
{
    w->setProperty("cls", classes);
    w->style()->unpolish(w);
    w->style()->polish(w);
    w->update();
}

bool hasCls(const QWidget *w, const QString &cls)
{
    return w->property("cls").toString().split(' ', Qt::SkipEmptyParts).contains(cls);
}

void toggleCls(QWidget *w, const QString &cls, bool on)
{
    QStringList list = w->property("cls").toString().split(' ', Qt::SkipEmptyParts);
    const bool has = list.contains(cls);
    if (has == on)
        return;
    if (on) list << cls;
    else list.removeAll(cls);
    setCls(w, list.join(' '));
}

QPixmap svgPixmap(const QString &innerSvg, const QColor &color, int size, qreal dpr)
{
    QString inner = innerSvg;
    inner.replace("currentColor", color.name());
    const QString svg = QString("<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 24 24\" width=\"%1\" height=\"%1\" "
                                "fill=\"none\" stroke=\"%2\" stroke-opacity=\"%3\" stroke-width=\"1.6\" "
                                "stroke-linecap=\"round\" stroke-linejoin=\"round\">%4</svg>")
                            .arg(size).arg(color.name()).arg(color.alphaF()).arg(inner);
    QSvgRenderer renderer(svg.toUtf8());
    QPixmap pm(qMax(1, qRound(size * dpr)), qMax(1, qRound(size * dpr)));
    pm.fill(Qt::transparent);
    {
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        renderer.render(&p);
    }
    pm.setDevicePixelRatio(dpr);
    return pm;
}

QFont font(double px, int weight)
{
    QFont f = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    f.setPixelSize(qRound(px));
    if (std::abs(px - qRound(px)) > 0.01)
        f.setPointSizeF(px);   // fractional CSS sizes (12.5px)
    f.setWeight(QFont::Weight(weight));
    return f;
}

QFont monoFont(double px)
{
    QFont f = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    f.setPixelSize(qRound(px));
    return f;
}

QIcon svgIcon(const QString &innerSvg, const QColor &color, int size)
{
    QString inner = innerSvg;
    inner.replace("currentColor", color.name());
    const QString svg = QString("<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 24 24\" width=\"%1\" height=\"%1\" "
                                "fill=\"none\" stroke=\"%2\" stroke-opacity=\"%3\" stroke-width=\"1.6\" "
                                "stroke-linecap=\"round\" stroke-linejoin=\"round\">%4</svg>")
                            .arg(size).arg(color.name()).arg(color.alphaF()).arg(inner);
    QSvgRenderer renderer(svg.toUtf8());
    const qreal dpr = 2.0;
    QPixmap pm(int(size * dpr), int(size * dpr));
    pm.fill(Qt::transparent);
    {
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        renderer.render(&p);
    }
    pm.setDevicePixelRatio(dpr);
    return QIcon(pm);
}

// ---------------------------------------------------------------- api.js formatting

namespace {

// Number#toLocaleString(undefined, { minimumFractionDigits: min, maximumFractionDigits: max }).
// The browser formats with its UI language (en-US for an English browser), not the region's
// number format: use the system language with its default territory.
const QLocale &uiLocale()
{
    static const QLocale l = [] {
        QLocale loc(QLocale::system().language());
        loc.setNumberOptions(QLocale::DefaultNumberOptions);
        return loc;
    }();
    return l;
}

// Intl rounds the shortest decimal form of the number, half away from zero (99969.015 ->
// "99,969.02", 1.005 -> "1.01"); QLocale and toFixed round the binary value (99969.01499...).
// Returns |v| rounded that way to `digits` decimals.
double intlRound(double v, int digits)
{
    const QString s = QString::number(std::abs(v), 'f', QLocale::FloatingPointShortest);
    const int dot = s.indexOf('.');
    if (dot < 0 || s.size() - dot - 1 <= digits) return std::abs(v);
    QString all = s.left(dot) + s.mid(dot + 1, digits);
    if (s[dot + 1 + digits] >= '5') {
        int i = all.size() - 1;
        for (; i >= 0 && all[i] == '9'; --i) all[i] = '0';
        if (i < 0) all.prepend('1');
        else all[i] = QChar(all[i].unicode() + 1);
    }
    const QString ip = all.left(all.size() - digits), fp = all.right(digits);
    return (digits ? ip + '.' + fp : ip).toDouble();
}

QString localeFixed(double v, int minDigits, int maxDigits)
{
    QString s = uiLocale().toString(std::isfinite(v) ? intlRound(v, maxDigits) : v, 'f', maxDigits);
    // toLocaleString keeps the minus of negative numbers that round to zero and of -0 ("-0.00",
    // e.g. the P&L of a flat short); QLocale drops it.
    if ((v < 0 || std::signbit(v)) && !s.startsWith(uiLocale().negativeSign())) s.prepend(uiLocale().negativeSign());
    const QChar point = uiLocale().decimalPoint().at(0);
    const int dot = s.lastIndexOf(point);
    if (dot >= 0) {
        int keep = s.size();
        while (keep > dot + 1 + minDigits && s[keep - 1] == '0')
            --keep;
        if (keep == dot + 1)
            --keep;
        s.truncate(keep);
    }
    return s;
}

// JS Number#toFixed.
QString toFixed(double v, int d)
{
    return js::toFixed(v, d);   // ties round up, like JS (not to even)
}

} // namespace

QString fmtNum(double v, int d)
{
    if (!std::isfinite(v))
        return QString::fromUtf8("—");
    const double a = std::abs(v);
    if (a >= 1e12) return toFixed(v / 1e12, 2) + "T";
    if (a >= 1e9) return toFixed(v / 1e9, 2) + "B";
    if (a >= 1e6) return toFixed(v / 1e6, 2) + "M";
    if (a >= 1e4) return toFixed(v / 1e3, 1) + "K";
    return toFixed(v, d);
}

QString fmtPrice(double v)
{
    if (!std::isfinite(v))
        return QString::fromUtf8("—");
    const double a = std::abs(v);
    if (a >= 1e6)
        return fmtNum(v);
    const int d = a >= 1000 ? 2 : a >= 1 ? (a >= 100 ? 2 : 4) : a >= 0.01 ? 6 : 8;
    return localeFixed(v, std::min(d, 2), d);
}

QString fmtPct(double v)
{
    if (!std::isfinite(v))
        return QString::fromUtf8("—");
    return QString("%1%2%").arg(v >= 0 ? "+" : "").arg(toFixed(v, 2));
}

QString esc(const QString &s)
{
    return s.toHtmlEscaped().replace('\'', "&#39;");
}

QString ticker(const QString &id)
{
    if (id.startsWith("SPREAD:")) {
        QStringList parts = id.mid(7).split(' ');
        for (QString &x : parts)
            if (x.contains(':')) x = x.split(':').last();
        return parts.join("");
    }
    return id.split(':').last();
}

QString exchange(const QString &id)
{
    return id.split(':').first();
}

} // namespace qtm::theme
