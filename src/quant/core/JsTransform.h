#pragma once

// Source-to-source lowering of custom indicator modules for QJSEngine (Qt's V4 engine).
//
// Node runs custom indicators on V8, which accepts all of modern JavaScript. V4 (Qt 6.12) lacks a
// part of ES2018..ES2022 syntax, so a file that Node loads would fail to import here. Before a
// module is imported it is parsed (a full ES2025 parser over the tokens, not pattern matching) and
// the syntax V4 lacks is rewritten into equivalent code that V4 runs, calling helpers of the global
// `__qtm` object (installed by CustomIndicators.cpp next to the built-in polyfills):
//
//   object spread  { a, ...x }            -> __qtm.spreadProps(...) (CopyDataProperties semantics)
//   `__proto__: v` in object literals (V4 makes it an own property instead of the prototype)
//   object rest    const { a, ...r } = o  -> flattened destructuring + __qtm.rest(o, ["a"])
//                  (declarations, assignments, parameters, for-in/of heads, catch, nested patterns)
//   logical assignment  a.b ??= c         -> ((t = a).b ?? (t.b = c)), `a` evaluated once
//   class fields, private members, static blocks, `#x in o`
//   async functions / arrows / methods, await, async generators, for await
//   optional catch binding, export * as ns, import.meta, dynamic import()
//   regular expression named groups and the s flag (via __qtm.regex)
//   two or more destructuring parameters (a V4 bug binds them to the wrong argument)
//   `return (x = f())` and `(v) => (o.p = f(v))` (a V4 bug compiles them as tail calls and drops
//   the store)
//
// Every rewrite keeps the line structure: code after a rewritten construct stays on its line, so
// errors and stack traces point at the user's lines.
//
// What can't be emulated correctly is refused with a message naming the feature and the line:
// BigInt, top-level await, and regular expression features V4's regex engine lacks (lookbehind,
// \p{...}, the d and v flags, inline modifiers, duplicate group names).

#include <QString>
#include <QStringList>

namespace qtm::js {

struct Transformed {
    QString code;        // the source to import (the original when nothing needed rewriting)
    bool changed = false;
    bool parsed = false; // false: the parser stopped at a syntax error; code is the original
    int errorOffset = -1;   // where the parser stopped (parsed == false)
    QString unsupported; // non-empty: refuse the file with this message
    QStringList imports; // static import specifiers (import ... from, export ... from), in order
};

// `url` is the module's own file URL (for import.meta.url and dynamic import()).
Transformed transformModule(const QString &source, const QString &url);

// The Node (V8) wording of a syntax error at `offset` in `source`, as V8 words the common cases
// ("Unexpected token ';'", "Unexpected identifier 'x'", "Unexpected end of input", ...).
QString syntaxErrorAt(const QString &source, int offset);

} // namespace qtm::js
