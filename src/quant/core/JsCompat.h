#pragma once

// Small pieces of JavaScript semantics the port needs to give the same numbers and texts as the
// original: Math.round/max/min (NaN-propagating), Number#toString, Date in UTC, toISOString.
// Shared by the qtm_core sources (series, indicators, seasonals); usable by anyone porting JS.

#include <QString>

#include <cmath>
#include <limits>

// Bit-exact results need the same floating-point operations as V8: no fused multiply-add.
// (Clang contracts a*b+c into an FMA by default on arm64.) Every core .cpp includes this.
#if defined(__clang__)
#pragma clang fp contract(off)
#endif

namespace qtm::js {

constexpr double Inf = std::numeric_limits<double>::infinity();

// Math.round: rounds half up, toward +Infinity (Math.round(-2.5) === -2).
inline double round(double x)
{
    if (!std::isfinite(x)) return x;
    const double f = std::floor(x);
    return x - f >= 0.5 ? f + 1 : f;
}
// Math.max / Math.min with two arguments: NaN if either is NaN.
inline double max(double a, double b)
{
    if (std::isnan(a) || std::isnan(b)) return std::numeric_limits<double>::quiet_NaN();
    if (a == b) return std::signbit(a) ? b : a;   // max(-0, 0) is 0
    return a > b ? a : b;
}
inline double min(double a, double b)
{
    if (std::isnan(a) || std::isnan(b)) return std::numeric_limits<double>::quiet_NaN();
    if (a == b) return std::signbit(a) ? a : b;   // min(-0, 0) is -0
    return a < b ? a : b;
}
inline double max(double a, double b, double c) { return max(max(a, b), c); }
inline double min(double a, double b, double c) { return min(min(a, b), c); }
inline double sign(double x) { return std::isnan(x) ? x : x > 0 ? 1 : x < 0 ? -1 : x; }
// `x || 0` for a number.
inline double orZero(double x) { return (std::isnan(x) || x == 0) ? 0 : x; }
// Truthiness of a number.
inline bool truthy(double x) { return !(std::isnan(x) || x == 0); }

// Math.log, Math.log10, Math.exp exactly as V8 computes them (fdlibm), see JsMath.cpp.
double log(double x);
double log10(double x);
double exp(double x);

// String(x) / Number#toString() for a double: shortest round-trip digits, JS exponent rules.
QString numberToString(double x);

// Date parts from a JS time value in milliseconds (after TimeClip). Invalid -> all NaN.
struct UtcParts {
    double year = 0, month = 0, day = 0, hours = 0, minutes = 0, seconds = 0, ms = 0, weekDay = 0;
    bool valid = true;
};
UtcParts utcParts(double ms);
// Date.UTC(year, month, day, ...) in milliseconds (month may be out of 0..11, it carries over).
double dateUtc(double year, double month, double day = 1, double h = 0, double m = 0, double s = 0, double ms = 0);
// new Date(ms).toISOString(); throws qtm::Error("Invalid time value") like the RangeError.
QString toIsoString(double ms);
// Number#toFixed(digits) for 0 <= digits <= 100.
QString toFixed(double x, int digits);

} // namespace qtm::js
