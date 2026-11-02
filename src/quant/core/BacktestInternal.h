#pragma once

// Internals of the backtest.js port shared with Quant.cpp: the backtest as plain C++ data (the
// quant checks run it ~27 times and only need returns and a few stats, not the JSON), and the
// bits of JS value semantics both files need.

#include "Backtest.h"
#include "JsCompat.h"

#include <QJsonValue>

namespace qtm::bt {

// String(v) / `${v}` for a JSON value.
QString jsString(const QJsonValue &v);
// Number(v) for a JSON value (undefined -> NaN, null -> 0, strings parsed the JS way).
double jsNumber(const QJsonValue &v);
// Number(s) for a string.
double stringToNumber(const QString &s);
// JS truthiness of a JSON value.
bool truthy(const QJsonValue &v);
// a ?? b
inline QJsonValue orDefault(const QJsonValue &a, const QJsonValue &b)
{
    return (a.isNull() || a.isUndefined()) ? b : a;
}

struct Trade {
    int side = 1;
    double entryTime = 0, exitTime = 0, entryPrice = 0, exitPrice = 0, qty = 0, pnl = 0, pnlPct = 0;
    int bars = 0;
    QString entryReason, exitReason;
};

struct Run {
    QVector<Trade> trades;
    Values eqTime, eqValue;
    QJsonObject stats;
};

Run run(const Bars &bars, const QJsonObject &strategy, const Bars *subBars);
QJsonObject toJson(const Run &r);

} // namespace qtm::bt
