#pragma once

// Quant Terminal look: the CSS variables of quant-terminal/public/css/app.css (dark and light),
// the stylesheet that reproduces app.css with Qt style sheets, and helpers shared by every UI file.
//
// Class convention: a widget that has CSS classes in the original gets a dynamic property "cls"
// with the same space-separated class names (setCls(w, "btn primary")). The QSS selects them with
// [cls~="btn"], so the port reads like the HTML: <button class="btn small danger"> becomes
// setCls(button, "btn small danger"). Object names mirror HTML ids where the CSS styles an id.

#include <QColor>
#include <QFont>
#include <QHash>
#include <QIcon>
#include <QPixmap>
#include <QString>

class QWidget;

namespace qtm::theme {

// "dark" or "light" (app.settings.theme). While a host palette is set, current() is "light" or
// "dark" after that palette and setCurrent() only records the choice for when it is cleared.
void setCurrent(const QString &name);
QString current();

// A palette from the host app (deepwater's themes) replacing the built-in TradingView ones:
// CSS colour text keyed by the variable names below; a missing name keeps the built-in value.
// `light` says whether it is a light theme (shadows, checkboxes). After a change, ask the App to
// re-apply it (App::refreshTheme). clearPalette() goes back to the built-in dark / light.
void setPalette(const QHash<QString, QString> &vars, bool light);
void clearPalette();
bool hasPalette();

// A CSS variable of the current theme, e.g. color("--bg"), color("--accent-soft").
// Names: --bg --panel --panel-2 --border --text --muted --accent --accent-soft --up --down --hover
//        --grid --input --gold --gold-soft --scroll
// and, not in app.css (the port's fixed colours, which a host palette can change):
//        --on-accent (text on --accent), --tooltip --tooltip-border, --crosshair (lines)
//        --crosshair-label (label background), --short (tester short entries)
QColor color(const QString &var);
// color(var) with alpha `a` (0-1), for the translucent up / down / accent fills.
QColor color(const QString &var, double a);
// Text on a coloured surface (labels on up / down / accent boxes): white, or the theme's dark
// colour when that reads better. Always white with the built-in palettes, as in the original.
QColor textOn(const QColor &background);
// The drop shadow of modals, toasts and the replay bar.
QColor shadowColor();
// The same value as CSS text ("#131722", "rgba(41, 98, 255, 0.18)"), for rich text and charts.
QString css(const QString &var);
// Parses CSS colour text: #rgb, #rrggbb, #rrggbbaa, rgb(), rgba() and the named colours used.
QColor parseColor(const QString &cssColor);

// The full stylesheet for the terminal (app.css), built for the current theme. It appends
// panelsStyleSheet() (ui/PanelsStyle.cpp, owned by the panels port) for the panel-specific rules.
QString styleSheet();
QString panelsStyleSheet();

// CSS class helpers (re-polish so the stylesheet updates).
void setCls(QWidget *w, const QString &classes);
void toggleCls(QWidget *w, const QString &cls, bool on);
bool hasCls(const QWidget *w, const QString &cls);

// An icon from the inline SVG markup used in the JS (the children of <svg viewBox="0 0 24 24">),
// drawn like the CSS does: no fill, stroke = colour, stroke-width 1.6, round caps and joins.
QIcon svgIcon(const QString &innerSvg, const QColor &color, int size = 18);
// The same drawing as a pixmap of size x size logical pixels at device pixel ratio `dpr`.
QPixmap svgPixmap(const QString &innerSvg, const QColor &color, int size, qreal dpr);

// The body font (-apple-system: the system UI font) at a CSS pixel size and weight (400, 600 ...).
QFont font(double px, int weight = 400);
// ui-monospace (SF Mono / Menlo).
QFont monoFont(double px);

// Number formatting from public/js/api.js (fmtPrice, fmtNum, fmtPct) and esc().
QString fmtPrice(double v);
QString fmtNum(double v, int decimals = 2);
QString fmtPct(double v);
QString esc(const QString &s);
// "BINANCE:BTCUSDT" -> "BTCUSDT"; spreads -> "BTCUSDT/ETHUSDT" (api.js ticker); exchange prefix.
QString ticker(const QString &id);
QString exchange(const QString &id);

} // namespace qtm::theme
