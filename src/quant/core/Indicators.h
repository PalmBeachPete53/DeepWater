#pragma once

// Port of quant-terminal/public/lib/indicators.js: the 87 built-in indicators, the ta.* building
// blocks, the registry (built-ins + custom indicators) and the custom-indicator format checks.
// Values must match the JS implementation exactly (the port is checked against Node).

#include "Bar.h"

#include <QHash>
#include <QJsonObject>
#include <QJsonValue>
#include <QStringList>

#include <functional>
#include <memory>

namespace qtm {

// ---------------------------------------------------------------- definitions

struct IndicatorInput {
    QString key;
    QString label;
    QString type;          // "int" | "float" | "select" | "source" | "time"
    QJsonValue def;        // JS `default`: number or string
    QStringList options;   // select only
    QJsonValue min;        // undefined (QJsonValue::Undefined) when not set
    QJsonValue max;
};

struct IndicatorOutput {
    QString key;
    QString label;
    QString kind = "line"; // "line" | "histogram"
    QString color;         // CSS colour as written in the JS ("#2962ff", "rgba(...)")
    int lineWidth = 0;     // 0 = not set (chart uses 1)
    bool dots = false;     // drawn as dots instead of a line
};

struct IndicatorMarker {
    double time = 0;       // bar time, UTC seconds
    QString position;      // "aboveBar" | "belowBar" | "inBar"
    QString shape;         // "arrowUp" | "arrowDown" | "circle" | "square"
    QString color;
    QString text;
};

// What calc() returns. JS returns a plain object; arrays of numbers go in `values` (by key,
// including arrays that are not declared outputs such as Supertrend's "direction"), and the
// special keys get their own fields. Arrays can be longer than the bars (shifted indicators that
// extend into the future, e.g. Ichimoku, Alligator).
struct IndicatorResult {
    QHash<QString, Values> values;
    QVector<IndicatorMarker> markers;
    bool hasMarkers = false;
    QStringList colors;            // res.colors: per-bar histogram colours ("" = default)
    QVector<bool> breaks;          // res.breaks: true where the line must not join the previous point
    QJsonObject extra;             // any other key, as JSON: volumeProfile, sessionProfile, extendBars, ...
    // Set by the chart, never by calc():
    QString error;
    bool pending = false;

    bool has(const QString &key) const { return values.contains(key); }
};

using IndicatorCalc = std::function<IndicatorResult(const Bars &bars, const QJsonObject &params)>;

struct IndicatorDef {
    QString id;
    QString name;
    QString group;                 // "Moving averages", "Trend", ..., "Quant (bonus)", "Custom"
    bool overlay = true;
    QVector<IndicatorInput> inputs;
    QVector<IndicatorOutput> outputs;
    QVector<double> levels;
    bool noAutoscale = false;
    bool volumeFormat = false;
    IndicatorCalc calc;            // params arrive merged with defaults
    QString file;                  // custom indicators: absolute path of the .js file
};

using IndicatorDefPtr = std::shared_ptr<const IndicatorDef>;

// ---------------------------------------------------------------- registry (thread-safe)

// Every indicator in JS INDICATORS order (built-ins, then custom ones in registration order).
QVector<IndicatorDefPtr> indicators();
IndicatorDefPtr indicatorById(const QString &id);      // null when unknown (JS BY_ID[id])
QJsonObject indicatorDefaults(const QString &id);       // JS defaults(id)
// JS compute(id, bars, params): defaults merged under params. Throws qtm::Error("Unknown indicator: id").
IndicatorResult compute(const QString &id, const Bars &bars, const QJsonObject &params = {});

// Custom indicator format checks (JS indicatorProblems / validateIndicator / registerIndicator).
QStringList indicatorProblems(const IndicatorDef &def, bool run = true);
void validateIndicator(const IndicatorDef &def);         // throws qtm::Error with the JS message
IndicatorDefPtr registerIndicator(IndicatorDef def);     // fills JS defaults, validates, adds or replaces

// Period key used by VWAP resets, pivots and session volume profiles (JS periodKey).
QString periodKey(double t, const QString &period);

// ---------------------------------------------------------------- ta.* building blocks
// Same names, defaults and NaN behaviour as the JS exports.
namespace ta {
Values source(const Bars &bars, const QString &name = "close");
Values change(const Values &src, int n = 1);
Values sum(const Values &src, int n);
Values sma(const Values &src, int n);
Values ema(const Values &src, int n);
Values rma(const Values &src, int n);
Values wma(const Values &src, int n);
Values vwma(const Values &src, const Values &vol, int n);
Values hma(const Values &src, int n);
Values dema(const Values &src, int n);
Values tema(const Values &src, int n);
Values alma(const Values &src, int n, double offset = 0.85, double sigma = 6);
Values linreg(const Values &src, int n, int offset = 0);
Values stdev(const Values &src, int n);
Values dev(const Values &src, int n);
Values highest(const Values &src, int n);
Values lowest(const Values &src, int n);
Values tr(const Bars &bars);
Values atr(const Bars &bars, int n = 14);
QVector<bool> crossover(const Values &a, const Values &b);
QVector<bool> crossover(const Values &a, double b);
QVector<bool> crossunder(const Values &a, const Values &b);
QVector<bool> crossunder(const Values &a, double b);
Values rsi(const Values &src, int n = 14);
} // namespace ta

} // namespace qtm
