#pragma once

// Pine Script files (.pine) in the custom/ folder, as chart indicators: the script's declaration,
// inputs and plots become an IndicatorDef (name, inputs, outputs, levels), and each compute runs the
// script on the chart's bars with PineTS (PineScript.h). The file is read again when it changes.

#include "Indicators.h"

namespace qtm {

// Runs the script once on test bars to read its declaration, inputs and plots, then registers it
// as `id` (custom:<file name without .pine>). Throws qtm::Error with PineTS's message (or the
// Node.js problem) when it can't.
IndicatorDefPtr loadPineIndicator(const QString &path, const QString &id);

// The JavaScript PineTS runs for the file (pine::transpile on its contents). Throws qtm::Error.
QString pineIndicatorJs(const QString &path);

} // namespace qtm
