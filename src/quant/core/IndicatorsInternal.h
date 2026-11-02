#pragma once

// Internals of the indicators.js port shared between Indicators*.cpp and CustomIndicators.cpp:
// the non-exported JS helpers (macdFrom, dmiOf, psarOf, supertrendOf, ...), the parameter reader
// and the registry insert used once a definition has been checked.

#include "Indicators.h"
#include "JsCompat.h"

#include <QJsonArray>

namespace qtm::detail {

// src[i] with JS semantics for a missing index: undefined, which behaves like NaN in arithmetic
// and comparisons.
inline double at(const Values &v, qint64 i)
{
    return (i >= 0 && i < v.size()) ? v[int(i)] : NaN;
}
inline Values filled(qsizetype n, double v = NaN)
{
    return Values(n, v);
}
// A JS number used as a window length / loop bound. NaN never occurs with sane params; 0 is the
// least surprising stand-in.
inline int toInt(double v)
{
    if (std::isnan(v)) return 0;
    if (v > 1e9) return 1000000000;
    if (v < -1e9) return -1000000000;
    return int(std::trunc(v));
}
// JS Number(x) for a JSON value: numbers as is, strings parsed, true/false 1/0, null 0, undefined NaN.
double jsonNumber(const QJsonValue &v);
// JS truthiness of a JSON value.
bool jsonTruthy(const QJsonValue &v);

// The `p` object handed to calc(): defaults merged under the caller's params.
struct Params {
    const QJsonObject &o;
    double num(const QString &k) const { return jsonNumber(o.value(k)); }
    int len(const QString &k) const { return toInt(num(k)); }
    QString str(const QString &k) const { return o.value(k).toVariant().toString(); }
    QJsonValue raw(const QString &k) const { return o.value(k); }
};

Values stochOf(const Values &src, const Values &high, const Values &low, int n);
Values cum(const Values &src);
Values shiftForward(const Values &src, int k);
Values whole(const Values &out, const Values &src, int n);
double pearson(const Values &x, const Values &y);
Values avgRanks(const Values &v);
Values percentRank(const Values &src, int n);
Values updownStreak(const Values &src);
Values swma(const Values &src);
Values extremeBars(const Values &src, int n, bool higher);
bool sessionBreak(bool hadKey, int start, int i);

QVector<IndicatorMarker> candlePatterns(const Bars &bars);
QVector<IndicatorMarker> pivotMarkers(const Bars &bars, int left, int right);
QVector<IndicatorMarker> fractalMarkers(const Bars &bars, int n);

struct Macd {
    Values macd, signal, hist;
    QStringList colors;
};
Macd macdFrom(const Values &s, int fast, int slow, int signal);
struct Dmi {
    Values plus, minus, adx;
};
Dmi dmiOf(const Bars &bars, int diLen, int adxLen);
Values psarOf(const Bars &bars, double start, double inc, double max);
struct Supertrend {
    Values st, dir;
};
Supertrend supertrendOf(const Bars &bars, double factor, int atrLen);
struct Vstop {
    Values up, down;
};
Vstop vstopOf(const Bars &bars, int length, double factor);
Values zigzagOf(const Bars &bars, double deviation, double depth);
IndicatorResult pivotsOf(const Bars &bars, const QString &period);

// The 87 built-ins, in JS order.
QVector<IndicatorDef> builtinIndicators();
// 300 made-up daily bars, the same every time, for the trial run (JS trialBars).
Bars trialBars(int n = 300);
// Adds or replaces (in place) a definition that has already been checked.
IndicatorDefPtr insertIndicator(IndicatorDef def);

// JSON shape of a result, as the JS object would serialize (arrays, markers, colors, breaks, extras).
QJsonObject markerToJson(const IndicatorMarker &m);

} // namespace qtm::detail
