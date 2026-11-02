#pragma once

// Port of quant-terminal/public/lib/backtest.js. Strategies stay plain JSON, exactly the JS
// format (README "Strategy format"), and results come back as JSON in the JS object shape, so
// the tester UI, alerts, screener scans and the HTTP API read the same fields as in JS.
//
// NaN and Infinity in results: JSON text can't hold them, but QJsonValue can. A JS number that
// is NaN/Infinity/-Infinity is stored as that double in the QJsonObject/QJsonArray (Qt keeps it:
// isDouble() is true and toDouble() gives it back), so in-process readers (the UI) see exactly
// the JS value. Turned into text (QJsonDocument::toJson), Qt writes them as null, which is what
// JSON.stringify does, so the HTTP API sends the same bytes as the JS server. A JS `null` is a
// QJsonValue::Null (e.g. stats.profitFactor when there are wins and no losses, sharpe with a
// flat equity curve), and a JS `undefined` field is left out (as JSON.stringify does).
// Only -0 doesn't survive (Qt stores it as 0); JS shows -0 as "0" everywhere anyway.

#include "Bar.h"
#include "Indicators.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>

#include <functional>
#include <memory>

namespace qtm {

extern const QStringList OPS;        // ">", "<", ">=", "<=", "crossesAbove", "crossesBelow", "crosses"
extern const QStringList INTRABAR;   // "worst", "ohlc", "magnifier"

// JS linePrice({ t1, p1, t2, p2, extend }) -> price at time t, NaN where the line doesn't reach.
std::function<double(double)> linePrice(const QJsonObject &line);

// The JS `cache` object handed to resolveSeries/conditionFn: price fields by name, indicator
// results by "@key". Share one between several conditionFn() calls on the same bars to compute
// each indicator once, as JS does when it passes the same object (an entry is never recomputed,
// even if a later call defines the key differently: same as JS). Not thread-safe.
struct ConditionCache {
    QHash<QString, Values> prices;               // cache["close"], ...
    QHash<QString, IndicatorResult> results;     // cache["@fast"], ...
};
using ConditionCachePtr = std::shared_ptr<ConditionCache>;

// JS resolveSeries(bars, indicators, cache): operand -> (i) -> value (NaN where JS has
// undefined/NaN). Throws qtm::Error("Unknown operand ...") / ("Indicator ... has no output ...").
using SeriesFn = std::function<double(int)>;
std::function<SeriesFn(const QJsonValue &operand)> resolveSeries(const Bars &bars, const QJsonObject &indicators,
                                                                 ConditionCachePtr cache = nullptr);

// JS conditionFn(bars, indicators, cache): make(rule) -> (i) -> bool. The cache is shared between
// rules built from the same factory (one compute per indicator key); pass your own to share it
// between factories too. A bad operator throws when a rule is evaluated (as in JS), a bad operand
// when the rule is made.
using Condition = std::function<bool(int)>;
using ConditionFactory = std::function<Condition(const QJsonObject &rule)>;
ConditionFactory conditionFn(const Bars &bars, const QJsonObject &indicators);
ConditionFactory conditionFn(const Bars &bars, const QJsonObject &indicators, ConditionCachePtr cache);

struct MagnifierTf {
    QString tf;
    qint64 seconds = 0;
};
// JS magnifierTf: empty tf when there is none.
MagnifierTf magnifierTf(qint64 tfSeconds, double spanSeconds, int max = 20000);

// JS backtest(bars, strategy, { subBars }): { stats, trades, equity } with the JS field names.
// Throws qtm::Error with the JS messages on a bad strategy. subBars may be null.
QJsonObject backtest(const Bars &bars, const QJsonObject &strategy, const Bars *subBars = nullptr);
QString tradesCsv(const QJsonArray &trades);

// JS EXAMPLE_STRATEGIES, in their JS order.
QVector<QPair<QString, QJsonObject>> exampleStrategies();

} // namespace qtm
