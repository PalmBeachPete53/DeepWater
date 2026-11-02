#pragma once

// Port of quant-terminal/public/lib/quant.js: the "quant checks" run on a strategy (deflated
// Sharpe, walk-forward, PBO, Monte Carlo, cost stress, nearby settings ...). Same JSON shape as JS
// (NaN/Infinity/null/undefined carried as described in Backtest.h).

#include "Bar.h"

#include <QJsonObject>
#include <QJsonValue>
#include <QStringList>
#include <QVector>

#include <optional>

namespace qtm {

double normCdf(double x);
double normInv(double p);

// JS moments(r): mean, sd, skew, kurt.
struct Moments {
    double mean = 0, sd = 0, skew = 0, kurt = 0;
};
Moments moments(const Values &r);
// JS sharpe(r) / psr(r, benchmark): nullopt where JS returns null.
std::optional<double> sharpe(const Values &r);
std::optional<double> psr(const Values &r, double benchmark = 0);
double expectedMaxSharpe(double n, double variance);
// JS pbo(matrix, blocks): null or { pbo, splits }.
QJsonValue pbo(const QVector<Values> &matrix, int blocks = 8);
// JS monteCarlo(tradePcts, runs, seed): null or { runs, p5, p50, p95, ddP50, ddP95, lossProb }.
QJsonValue monteCarlo(const Values &tradePcts, int runs = 2000, double seed = 7);

// JS quantChecks(bars, strategy, { mcRuns, subBars }).
QJsonObject quantChecks(const Bars &bars, const QJsonObject &strategy, int mcRuns = 2000,
                        const Bars *subBars = nullptr);

// The order of strategy.indicators matters to the checks (JS walks Object.entries(indicators) to
// pick the first two whole-number settings), and a QJsonObject doesn't keep it. Pass the keys in
// their JS order when you know it (as written in the JSON text, or as the tester built them).
// Without it, the order is the one the tester UI gives them: first use in entryLong, exitLong,
// entryShort, exitShort (left operand, then right), then any unused keys. Either way, keys that
// are array indexes ("0", "12") come first in numeric order, as in JS.
struct QuantOptions {
    int mcRuns = 2000;
    const Bars *subBars = nullptr;
    QStringList indicatorOrder;
};
QJsonObject quantChecks(const Bars &bars, const QJsonObject &strategy, const QuantOptions &options);
// The order used for strategy.indicators (see QuantOptions).
QStringList indicatorOrder(const QJsonObject &strategy, const QStringList &known = {});

} // namespace qtm
