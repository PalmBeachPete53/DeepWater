#pragma once

// Port of public/js/quantui.js: the gold "Bonus: quant checks" box under a strategy tester result.

class QWidget;

namespace qtm {

namespace panels { struct TesterResult; }

namespace quantui {

// mountQuantChecks(el, result): fills `el` (a widget with a QVBoxLayout, emptied first).
void mountQuantChecks(QWidget *el, const panels::TesterResult &result);

} // namespace quantui
} // namespace qtm
