#pragma once

#include <QString>

// JavaScript evaluated on each custom indicator engine (QJSEngine) before a file is imported.

namespace qtm::detail {

// The built-ins V4 (Qt 6.12) lacks next to Node, spec-faithful, and fixes to the natives that don't
// follow the spec (Array.prototype.sort is not stable, Object.assign skips symbols, ...): core/js/qtm-polyfills.js.
const QString &jsPolyfills();
// The global `__qtm` object: the helpers that code rewritten by JsTransform calls (core/js/qtm-runtime.js).
const QString &jsRuntime();

} // namespace qtm::detail
