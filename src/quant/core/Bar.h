#pragma once

// Quant Terminal (C++ port of quant-terminal/). Shared bar type.
// Mirrors the JS bar objects { time, open, high, low, close, volume } used everywhere in
// quant-terminal: `time` is UTC seconds of the bar's open; a missing volume is 0 (JS `b.volume || 0`
// / `b.volume ?? 0`). `color` is only set by chart-type transforms that colour bars (Kagi).

#include <QString>
#include <QVector>

#include <cmath>
#include <limits>

namespace qtm {

constexpr double NaN = std::numeric_limits<double>::quiet_NaN();
inline bool isNum(double v) { return !std::isnan(v); }   // JS isNum: a number that isn't NaN (Infinity counts)

struct Bar {
    double time = 0;     // UTC seconds (may be fractional only for synthetic Renko/Kagi/P&F times)
    double open = 0;
    double high = 0;
    double low = 0;
    double close = 0;
    double volume = 0;
    QString color;       // optional per-bar colour (Kagi); empty = default
};

using Bars = QVector<Bar>;
using Values = QVector<double>;   // one value per bar, NaN where missing (JS number[])

} // namespace qtm
