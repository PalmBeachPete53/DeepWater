#pragma once

// JavaScript semantics the backend code relies on, so the port reads like the JS: Number(),
// String(), truthiness, JSON.stringify (number format, escaping, indentation), JSON.parse,
// Object.keys order (QJsonObject sorts its keys; a few routes show keys in upstream order),
// encodeURIComponent / decodeURIComponent, Date.parse and Date#toISOString.

#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>

namespace qtm::jsv {

// ---------- values ----------
double toNumber(const QJsonValue &v);          // unary + / Number(v)
double toNumber(const QString &s);             // Number("...")
double parseFloat(const QString &s);           // parseFloat("...")
QString toString(const QJsonValue &v);         // String(v), `${v}`
QString numberToString(double d);              // Number#toString()
bool truthy(const QJsonValue &v);              // !!v
QString trim(const QString &s);                // String#trim()
// What JS's \s matches (Unicode spaces and line terminators), for a regex character class:
// "[" JS_SPACE "]" is JS's \s, "[^" JS_SPACE "]" its \S. PCRE's \s is ASCII only.
#define QTM_JS_SPACE "\\t\\n\\x{0B}\\f\\r \\x{A0}\\x{1680}\\x{2000}-\\x{200A}\\x{2028}\\x{2029}\\x{202F}\\x{205F}\\x{3000}\\x{FEFF}"
bool nullish(const QJsonValue &v);             // v == null (null or undefined)
inline QJsonValue orElse(const QJsonValue &v, const QJsonValue &fallback)   // v || fallback
{
    return truthy(v) ? v : fallback;
}
inline QJsonValue coalesce(const QJsonValue &v, const QJsonValue &fallback) // v ?? fallback
{
    return nullish(v) ? fallback : v;
}
// A JS number as JSON: whole numbers stay integers (as JS prints them), the rest doubles.
QJsonValue num(double d);

// ---------- JSON ----------
QByteArray stringify(const QJsonValue &v, int indent = 0);   // JSON.stringify(v, null, indent)
QString quote(const QString &s);                             // JSON.stringify("...")
QString numbersJson(const QVector<double> &values);          // JSON.stringify([1, 2.5, ...])
// JSON.parse: any JSON text, scalars included. ok=false on a syntax error.
QJsonValue parse(const QByteArray &text, bool *ok = nullptr);
// Object.keys() of the object at `path` inside the raw JSON text, in JS order (array-index keys
// first, ascending, then the rest in source order). Empty when there is no object there.
QStringList keyOrder(const QByteArray &json, const QStringList &path);

// ---------- URLs ----------
QString encodeURIComponent(const QString &s);
// Throws qtm::Error("URI malformed") like JS's URIError.
QString decodeURIComponent(const QString &s);
// URLSearchParams decoding of one component: '+' is a space, bad escapes stay as they are.
QString decodeFormComponent(const QString &s);

// ---------- dates ----------
double nowMs();                       // Date.now()
double dateParse(const QString &s);   // Date.parse(): ms since 1970, NaN when unreadable
QString isoString(double ms);         // new Date(ms).toISOString()
inline QString isoNow() { return isoString(nowMs()); }
// Date.UTC(y, m, d, h, mi, s, ms) with JS's month/day overflow and the 0-99 -> 1900s rule.
double dateUtc(double y, double m, double d = 1, double h = 0, double mi = 0, double s = 0, double ms = 0);
int utcDay(double ms);                // getUTCDay()
int utcHours(double ms);
int utcMinutes(double ms);
int utcFullYear(double ms);

// Math.round (half up toward +Infinity).
double round(double x);

} // namespace qtm::jsv
