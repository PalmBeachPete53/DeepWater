#pragma once

// Custom indicators: JavaScript files in the custom/ folder, in quant-terminal's own format
// (docs/custom-indicators.md): `export default { name, inputs, outputs, calc(bars, p, ta) { ... } }`.
// They run unchanged on a QJSEngine (one engine per thread, since compute() is called from the
// UI thread and from backend worker threads). `ta` exposes the same helpers as the JS `ta` object.

#include "Indicators.h"

namespace qtm {

// Imports the module, validates it with the JS rules (validateIndicator, including the trial run)
// and registers it as `id` (custom:<file name without .js>). Throws qtm::Error with the same
// message the Node server would record for that file (import error text or format problems).
IndicatorDefPtr loadCustomIndicator(const QString &path, const QString &id);

// JS indicatorProblems((await import(path)).default, { run }): the format problems of a file's
// default export as it stands (no defaults filled in). Throws qtm::Error when the import fails.
QStringList customIndicatorProblems(const QString &path, bool run = true);

// The custom/ folder also takes Pine Script sources (.pine), run by PineTS (PineIndicators.h).
bool isCustomIndicatorFile(const QString &fileName);          // name.js or name.pine
QString customIndicatorBaseName(const QString &fileName);     // the name without .js / .pine
// loadCustomIndicator or loadPineIndicator by the file's extension. Throws qtm::Error.
IndicatorDefPtr loadCustomIndicatorFile(const QString &path, const QString &id);

} // namespace qtm
