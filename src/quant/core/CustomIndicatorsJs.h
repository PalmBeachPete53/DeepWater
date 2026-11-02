#pragma once

// JavaScript evaluated on each custom indicator engine (QJSEngine) before a file is imported.

namespace qtm::detail {

// The built-ins V4 (Qt 6.12) lacks next to Node, spec-faithful, and fixes to the natives that don't
// follow the spec (Array.prototype.sort is not stable, Object.assign skips symbols, ...).
extern const char *const JS_POLYFILLS;
// The global `__qtm` object: the helpers that code rewritten by JsTransform calls.
extern const char *const JS_RUNTIME;

} // namespace qtm::detail
