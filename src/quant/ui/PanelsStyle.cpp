#include "Theme.h"

namespace qtm::theme {

// Stylesheet rules for the side and bottom panels (owned by the panels port). Most of the panels
// paint themselves (ui/PanelsKit.cpp: text blocks, tables, stats, selects, boxes); these rules
// cover the Qt input widgets they use.
QString panelsStyleSheet()
{
    QString s = QStringLiteral(R"QSS(
/* input.in / .field input (height 28, padding 0 7px: QLineEdit adds 2px of its own) */
#qtmRoot QLineEdit[cls~="pin"] {
    min-height: 26px; max-height: 26px; border: 1px solid @border; border-radius: 4px;
    background: @input; padding: 0 5px; color: @text; selection-background-color: @accent; selection-color: @onAccent;
}
/* .field input / select inherit the label's 11px */
#qtmRoot QLineEdit[cls~="f11"] { font-size: 11px; }
/* .rule input: height 26, padding 0 5px */
#qtmRoot QLineEdit[cls~="rin"] {
    min-height: 24px; max-height: 24px; border: 1px solid @border; border-radius: 4px;
    background: @input; padding: 0 3px; color: @text; selection-background-color: @accent; selection-color: @onAccent;
}
/* buttons keep their colours when disabled (button { color: inherit } beats the UA :disabled rule) */
#qtmRoot QPushButton[cls~="btn"]:disabled { color: @text; }
#qtmRoot QPushButton[cls~="btn"][cls~="primary"]:disabled { color: @onAccent; }
#qtmRoot QPushButton[cls~="btn"][cls~="gold"]:disabled { color: #1a1405; }
#qtmRoot QPushButton[cls~="btn"][cls~="gold"]:hover { background: @goldHover; border-color: @goldHover; }
/* textarea.in (the indicator editor, the JSON modal): padding 8px; QTextEdit puts a fixed line
   height's extra space under the text, CSS splits it, hence 9.5px on top */
#qtmRoot QTextEdit[cls~="code"] {
    border: 1px solid @border; border-radius: 4px; background: @input; color: @text;
    padding: 9px 8px 7px 8px; selection-background-color: @accent; selection-color: @onAccent;
}
)QSS");
    QColor gold = color("--gold");
    gold = QColor::fromHsvF(gold.hsvHueF(), gold.hsvSaturationF(), std::min(1.0f, gold.valueF() * 1.08f));
    s.replace("@goldHover", gold.name());
    const QStringList names = {"accent", "border", "input", "text"};
    s.replace("@onAccent", color("--on-accent").name());
    for (const QString &n : names) {
        const QColor c = color("--" + n);
        s.replace("@" + n, c.alpha() == 255 ? c.name()
                                            : QString("rgba(%1, %2, %3, %4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(c.alpha()));
    }
    return s;
}

} // namespace qtm::theme
