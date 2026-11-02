// Quant checks for a strategy tester result: is the backtest real, or luck and overfitting?
// The methods are the published ones quant desks use (and PTQ runs):
//   - walk-forward: pick the best settings on the past only, then score them on the next stretch
//   - probability of backtest overfitting (PBO) by combinatorially symmetric cross-validation
//   - probabilistic and deflated Sharpe ratio (Bailey & Lopez de Prado), for skew, fat tails and
//     the number of versions tried
//   - cost stress: double the fees, triple the slippage (at least 0.05% a side, so zero isn't left at zero)
//   - Monte Carlo: reshuffle the trades thousands of times to see the spread of outcomes
//   - parameter robustness: do nearby settings hold up, or is it one lucky number?
// Each variant's returns come from one full backtest: signals only use past bars, so a variant's
// return on any bar is the same whether or not later bars exist. Slicing them is look-ahead free.
//
// Port of quant-terminal/public/lib/quant.js, same arithmetic in the same order (bit-exact with Node).

#include "Quant.h"
#include "BacktestInternal.h"
#include "Error.h"
#include "Indicators.h"
#include "JsCompat.h"

#include <QJsonArray>
#include <QRegularExpression>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace qtm {

using bt::jsNumber;
using bt::jsString;

// ---------- maths ----------
double normCdf(double x)
{
    // Abramowitz & Stegun 7.1.26 on erf, accurate to about 1e-7
    const double t = 1 / (1 + (0.3275911 * std::fabs(x)) / M_SQRT2);
    const double y =
        1 - ((((1.061405429 * t - 1.453152027) * t + 1.421413741) * t - 0.284496736) * t + 0.254829592) * t *
                js::exp(-(x * x) / 2);
    return x >= 0 ? (1 + y) / 2 : (1 - y) / 2;
}

double normInv(double p)
{
    // Acklam's rational approximation, accurate to about 1e-9
    if (p <= 0) return -js::Inf;
    if (p >= 1) return js::Inf;
    static const double a[] = { -39.69683028665376, 220.9460984245205,  -275.9285104469687,
                                138.357751867269,   -30.66479806614716, 2.506628277459239 };
    static const double b[] = { -54.47609879822406, 161.5858368580409, -155.6989798598866, 66.80131188771972,
                                -13.28068155288572 };
    static const double c[] = { -0.007784894002430293, -0.3223964580411365, -2.400758277161838,
                                -2.549732539343734,    4.374664141464968,   2.938163982698783 };
    static const double d[] = { 0.007784695709041462, 0.3224671290700398, 2.445134137142996, 3.754408661907416 };
    const double lo = 0.02425, hi = 1 - lo;
    if (p < lo) {
        const double q = std::sqrt(-2 * js::log(p));
        return (((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q + c[5]) /
               ((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1);
    }
    if (p > hi) {
        const double q = std::sqrt(-2 * js::log(1 - p));
        return -(((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q + c[5]) /
               ((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1);
    }
    const double q = p - 0.5, r = q * q;
    return ((((((a[0] * r + a[1]) * r + a[2]) * r + a[3]) * r + a[4]) * r + a[5]) * q) /
           (((((b[0] * r + b[1]) * r + b[2]) * r + b[3]) * r + b[4]) * r + 1);
}

namespace {

Moments momentsOf(const double *r, qsizetype n)
{
    double sum = 0;
    for (qsizetype i = 0; i < n; ++i) sum = sum + r[i];
    const double mean = sum / double(n);
    double m2 = 0, m3 = 0, m4 = 0;
    for (qsizetype i = 0; i < n; ++i) {
        const double d = r[i] - mean;
        m2 += d * d;
        m3 += d * d * d;
        m4 += d * d * d * d;
    }
    m2 /= double(n);
    m3 /= double(n);
    m4 /= double(n);
    const double sd = std::sqrt(m2);
    return { mean, sd, sd > 0 ? m3 / std::pow(sd, 3.0) : 0, sd > 0 ? m4 / std::pow(sd, 4.0) : 3 };
}

std::optional<double> sharpeOf(const double *r, qsizetype n)
{
    if (n < 2) return std::nullopt;
    const Moments m = momentsOf(r, n);
    return m.sd > 0 ? std::optional<double>(m.mean / m.sd) : std::nullopt;
}

double sharpeOr(const double *r, qsizetype n, double fallback)
{
    const auto s = sharpeOf(r, n);
    return s ? *s : fallback;
}

QJsonValue numOrNull(const std::optional<double> &v)
{
    return v ? QJsonValue(*v) : QJsonValue(QJsonValue::Null);
}

// Math.max(...xs) then xs.indexOf(it): the first index of the largest (no NaN can occur here).
int argMax(const Values &xs)
{
    double m = -js::Inf;
    for (double x : xs) m = js::max(m, x);
    for (int i = 0; i < xs.size(); ++i)
        if (xs[i] == m) return i;
    return -1;
}

struct Pbo {
    double pbo;
    int splits;
};

std::optional<Pbo> pboOf(const QVector<Values> &matrix, int blocks)
{
    const int V = int(matrix.size());
    const int T = V ? int(matrix[0].size()) : 0;
    if (V < 2 || T < blocks * 5) return std::nullopt;
    const int size = int(std::floor(double(T) / blocks));
    QVector<QVector<int>> combos;
    QVector<int> chosen;
    std::function<void(int)> pick = [&](int start) {
        if (double(chosen.size()) == double(blocks) / 2) {
            combos.push_back(chosen);
            return;
        }
        for (int b = start; b < blocks; b++) {
            chosen.push_back(b);
            pick(b + 1);
            chosen.pop_back();
        }
    };
    pick(0);
    // sharpe(slice(r, ids)) without building the slice: the same sums in the same order.
    std::vector<double> buf;
    auto sliceSharpe = [&](const Values &r, const QVector<int> &ids) {
        buf.resize(size_t(ids.size()) * size_t(size));
        double *out = buf.data();
        for (int b : ids) {
            const int from = std::min(int(r.size()), b * size), to = std::min(int(r.size()), (b + 1) * size);
            if (to > from) std::memcpy(out, r.constData() + from, size_t(to - from) * sizeof(double));
            out += std::max(0, to - from);
        }
        return sharpeOr(buf.data(), out - buf.data(), -js::Inf);
    };
    int below = 0, counted = 0;
    Values isS(V), oosS(V);
    for (const QVector<int> &is : combos) {
        QVector<int> oos;
        for (int b = 0; b < blocks; ++b)
            if (!is.contains(b)) oos << b;
        for (int v = 0; v < V; ++v) isS[v] = sliceSharpe(matrix[v], is);
        const int best = argMax(isS);
        for (int v = 0; v < V; ++v) oosS[v] = sliceSharpe(matrix[v], oos);
        int rank = 1;   // 1 = worst
        for (double s : oosS)
            if (s < oosS[best]) rank++;
        const double w = double(rank) / (V + 1);
        counted++;
        if (js::log(w / (1 - w)) <= 0) below++;
    }
    return Pbo{ double(below) / counted, counted };
}

// Seeded random numbers, so the same strategy always shows the same Monte Carlo (mulberry32).
struct Rng {
    quint32 a;
    double operator()()
    {
        a = a + 0x6d2b79f5u;
        quint32 t = a;
        t = (t ^ (t >> 15)) * (t | 1u);
        t ^= t + (t ^ (t >> 7)) * (t | 61u);
        return double(t ^ (t >> 14)) / 4294967296.0;
    }
};

quint32 toUint32(double x)
{
    if (!std::isfinite(x)) return 0;
    double m = std::fmod(std::trunc(x), 4294967296.0);
    if (m < 0) m += 4294967296.0;
    return quint32(m);
}

} // namespace

Moments moments(const Values &r)
{
    return momentsOf(r.constData(), r.size());
}

std::optional<double> sharpe(const Values &r)
{
    return sharpeOf(r.constData(), r.size());
}

// Probability the true (per-period) Sharpe beats `benchmark`, given the sample's skew and kurtosis.
std::optional<double> psr(const Values &r, double benchmark)
{
    if (r.size() < 10) return std::nullopt;
    const Moments m = moments(r);
    if (!(m.sd > 0)) return std::nullopt;
    const double sr = m.mean / m.sd;
    const double denom = std::sqrt(js::max(1e-12, 1 - m.skew * sr + ((m.kurt - 1) / 4) * sr * sr));
    return normCdf(((sr - benchmark) * std::sqrt(double(r.size() - 1))) / denom);
}

// The best Sharpe you'd expect from n tries of pure noise with this spread of Sharpes.
double expectedMaxSharpe(double n, double variance)
{
    if (n < 2 || !(variance > 0)) return 0;
    const double g = 0.5772156649015329;
    return std::sqrt(variance) * ((1 - g) * normInv(1 - 1 / n) + g * normInv(1 - 1 / (n * M_E)));
}

// Probability of backtest overfitting: split the history into `blocks`, try every half/half split,
// pick the best variant on one half and see where it ranks on the other.
QJsonValue pbo(const QVector<Values> &matrix, int blocks)
{
    const auto p = pboOf(matrix, blocks);
    if (!p) return QJsonValue(QJsonValue::Null);
    return QJsonObject{ { "pbo", p->pbo }, { "splits", p->splits } };
}

QJsonValue monteCarlo(const Values &tradePcts, int runs, double seed)
{
    if (tradePcts.size() < 5) return QJsonValue(QJsonValue::Null);
    Rng rand{ toUint32(seed) };
    Values finals, dds;
    finals.reserve(std::max(0, runs));
    dds.reserve(std::max(0, runs));
    const int n = int(tradePcts.size());
    for (int k = 0; k < runs; k++) {
        double eq = 1, peak = 1, dd = 0;
        for (int i = 0; i < n; i++) {
            eq *= 1 + tradePcts[int(std::floor(rand() * n))] / 100;
            peak = js::max(peak, eq);
            dd = js::max(dd, (peak - eq) / peak);
        }
        finals.push_back((eq - 1) * 100);
        dds.push_back(dd * 100);
    }
    std::sort(finals.begin(), finals.end());
    std::sort(dds.begin(), dds.end());
    QJsonObject out{ { "runs", runs } };
    auto q = [&](const char *key, const Values &s, double p) {
        const qsizetype i = std::min<qsizetype>(s.size() - 1, qsizetype(std::floor(p * s.size())));
        if (i >= 0) out.insert(key, s[i]);   // JS: undefined (left out) when there are no runs
    };
    q("p5", finals, 0.05);
    q("p50", finals, 0.5);
    q("p95", finals, 0.95);
    q("ddP50", dds, 0.5);
    q("ddP95", dds, 0.95);
    int losses = 0;
    for (double x : finals)
        if (x < 0) losses++;
    out.insert("lossProb", (double(losses) / runs) * 100);
    return out;
}

// ---------- the checks ----------
namespace {

const QStringList PRICE_FIELDS = { "open", "high", "low", "close", "volume", "hl2", "hlc3", "ohlc4", "hlcc4" };

Values perBar(const Values &equity)
{
    Values out(std::max<qsizetype>(0, equity.size() - 1));
    for (int i = 0; i < out.size(); ++i) out[i] = equity[i + 1] / equity[i] - 1;
    return out;
}

double periodsPerYear(const Bars &bars)
{
    return bars.size() > 1 ? (365 * 86400 * double(bars.size() - 1)) / (bars.last().time - bars[0].time) : 252;
}

// A key JS orders first: a canonical array index ("0", "17", below 2^32 - 1).
bool isIndexKey(const QString &k)
{
    if (k.isEmpty() || k.size() > 10) return false;
    for (QChar c : k)
        if (c < QLatin1Char('0') || c > QLatin1Char('9')) return false;
    if (k.size() > 1 && k[0] == QLatin1Char('0')) return false;
    return k.toULongLong() < 4294967295ull;
}

struct Tunable {
    QString key, param, label;
    QJsonValue base;
    double baseNum;
};

// Up to two whole-number settings to vary (indicator lengths and the like).
QVector<Tunable> tunables(const QJsonObject &strategy, const QStringList &order)
{
    QVector<Tunable> out;
    const QJsonObject inds = strategy.value("indicators").toObject();
    static const QRegularExpression PAREN(QStringLiteral(" \\(.*\\)\\z"));
    for (const QString &key : order) {
        const QJsonValue spec = inds.value(key);
        if (spec.isNull()) throw Error(QStringLiteral("Cannot read properties of null (reading 'id')"));
        const QJsonObject so = spec.toObject();
        const IndicatorDefPtr def = spec.isObject() ? indicatorById(jsString(so.value("id"))) : nullptr;
        if (!def) continue;
        QJsonObject p = indicatorDefaults(def->id);
        const QJsonValue params = so.value("params");
        if (bt::truthy(params)) {
            const QJsonObject po = params.toObject();
            for (auto it = po.begin(); it != po.end(); ++it) p.insert(it.key(), it.value());
        }
        for (const IndicatorInput &inp : def->inputs) {
            const QJsonValue v = p.value(inp.key);
            if (inp.type != QLatin1String("int") || !(jsNumber(v) >= 2)) continue;
            QString name = def->name;
            name.replace(PAREN, QString());
            out.push_back({ key, inp.key, name + QLatin1Char(' ') + inp.label.toLower(), v, jsNumber(v) });
            if (out.size() == 2) return out;
        }
    }
    return out;
}

QJsonObject withParams(const QJsonObject &strategy, const QVector<Tunable> &tun, const Values &vals)
{
    QJsonObject s = strategy;
    QJsonObject inds = s.value("indicators").toObject();
    for (int k = 0; k < tun.size(); ++k) {
        QJsonObject spec = inds.value(tun[k].key).toObject();
        const QJsonValue pv = spec.value("params");
        QJsonObject params = bt::truthy(pv) ? pv.toObject() : QJsonObject();
        params.insert(tun[k].param, vals[k]);
        spec.insert("params", params);
        inds.insert(tun[k].key, spec);
    }
    s.insert("indicators", inds);
    return s;
}

QJsonArray numArray(const Values &v)
{
    QJsonArray a;
    for (double x : v) a.append(x);
    return a;
}

} // namespace

QStringList indicatorOrder(const QJsonObject &strategy, const QStringList &known)
{
    const QJsonObject inds = strategy.value("indicators").toObject();
    QStringList keys;
    auto add = [&](const QString &k) {
        if (inds.contains(k) && !keys.contains(k)) keys << k;
    };
    if (!known.isEmpty()) {
        for (const QString &k : known) add(k);
    } else {
        for (const char *group : { "entryLong", "exitLong", "entryShort", "exitShort" })
            for (const QJsonValue &rule : strategy.value(group).toArray())
                for (const char *side : { "left", "right" }) {
                    const QJsonValue o = rule.toObject().value(side);
                    if (!o.isString()) continue;
                    const QString s = o.toString();
                    if (PRICE_FIELDS.contains(s) || (!s.trimmed().isEmpty() && !std::isnan(bt::stringToNumber(s))))
                        continue;
                    add(s.section(QLatin1Char('.'), 0, 0));
                }
    }
    for (auto it = inds.begin(); it != inds.end(); ++it) add(it.key());
    QStringList index, rest;
    for (const QString &k : keys) (isIndexKey(k) ? index : rest) << k;
    std::sort(index.begin(), index.end(), [](const QString &a, const QString &b) {
        return a.toULongLong() < b.toULongLong();
    });
    return index + rest;
}

QJsonObject quantChecks(const Bars &bars, const QJsonObject &strategy, int mcRuns, const Bars *subBars)
{
    QuantOptions o;
    o.mcRuns = mcRuns;
    o.subBars = subBars;
    return quantChecks(bars, strategy, o);
}

QJsonObject quantChecks(const Bars &bars, const QJsonObject &strategy, const QuantOptions &options)
{
    const Bars *subBars = options.subBars;
    const QJsonValue null(QJsonValue::Null);
    const double ppy = periodsPerYear(bars), ann = std::sqrt(ppy);
    const bt::Run base = bt::run(bars, strategy, subBars);
    const Values baseR = perBar(base.eqValue);
    const QVector<Tunable> tun = tunables(strategy, indicatorOrder(strategy, options.indicatorOrder));
    static const double MULT[] = { 0.6, 0.8, 1, 1.2, 1.4 };
    QVector<Values> axes;
    for (const Tunable &t : tun) {
        Values axis;
        for (double m : MULT) {
            const double v = js::max(2, js::round(t.baseNum * m));
            if (!std::any_of(axis.begin(), axis.end(), [v](double x) { return x == v || (std::isnan(x) && std::isnan(v)); }))
                axis << v;
        }
        axes << axis;
    }
    QVector<Values> grid;
    if (tun.isEmpty()) grid = { Values() };
    else if (tun.size() == 1)
        for (double v : axes[0]) grid.push_back({ v });
    else
        for (double a : axes[0])
            for (double b : axes[1]) grid.push_back({ a, b });

    struct Variant {
        Values values;
        bool isBase;
        Values returns;
        std::optional<double> sharpe;
        QJsonValue netPct, trades;
    };
    QVector<Variant> variants;
    for (const Values &vals : grid) {
        bool isBase = true;
        for (int k = 0; k < tun.size(); ++k)
            if (!(tun[k].base.isDouble() && tun[k].base.toDouble() == vals[k])) isBase = false;
        const bt::Run r = isBase ? base : bt::run(bars, withParams(strategy, tun, vals), subBars);
        Values ret = perBar(r.eqValue);
        const auto sr = sharpe(ret);
        variants.push_back({ vals, isBase, std::move(ret), sr, r.stats.value("netProfitPct"), r.stats.value("totalTrades") });
    }
    QVector<Values> matrix;
    for (const Variant &v : variants) matrix << v.returns;
    QJsonArray gates;
    auto gate = [&](const char *key, const char *label, const QString &value, bool pass) {
        gates.append(QJsonObject{ { "key", key }, { "label", label }, { "value", value }, { "pass", pass } });
    };

    // walk-forward: anchored, 5 test windows after the first 30% of history
    QJsonValue walkForward = null;
    if (variants.size() > 1 && baseR.size() > 100) {
        const int T = int(baseR.size()), start = int(std::floor(T * 0.3)), folds = 5;
        const int size = int(std::floor(double(T - start) / folds));
        Values oos, isChosen;
        QJsonArray rows;
        Values trainS(matrix.size());
        for (int k = 0; k < folds; k++) {
            const int s = start + k * size, e = k == folds - 1 ? T : s + size;
            for (int v = 0; v < matrix.size(); ++v)
                trainS[v] = sharpeOr(matrix[v].constData(), std::min<qsizetype>(s, matrix[v].size()), -js::Inf);
            const int best = argMax(trainS);
            const Values test = matrix[best].mid(s, e - s);
            oos += test;
            isChosen << trainS[best];
            QJsonObject row;
            if (s + 1 >= 0 && s + 1 < bars.size()) row.insert("from", bars[s + 1].time);
            if (e >= 0 && e < bars.size()) row.insert("to", bars[e].time);
            row.insert("chosen", numArray(variants[best].values));
            row.insert("testSharpe", sharpeOr(test.constData(), test.size(), 0) * ann);
            double acc = 1;
            for (double x : test) acc = acc * (1 + x);
            row.insert("testReturnPct", (acc - 1) * 100);
            rows.append(row);
        }
        double isSum = 0;
        for (double x : isChosen) isSum = isSum + x;
        const double oosS = sharpeOr(oos.constData(), oos.size(), 0) * ann, isS = (isSum / folds) * ann;
        int positive = 0;
        for (const QJsonValue &r : rows)
            if (r.toObject().value("testReturnPct").toDouble() > 0) positive++;
        walkForward = QJsonObject{
            { "folds", rows },
            { "oosSharpe", oosS },
            { "isSharpe", isS },
            { "efficiency", isS > 0 ? QJsonValue(oosS / isS) : null },
            { "positiveFolds", positive },
        };
        gate("wf", "Walk-forward",
             QStringLiteral("%1 Sharpe on data the settings were never picked on (needs above 0)").arg(js::toFixed(oosS, 2)),
             oosS > 0);
    }

    // overfitting
    const std::optional<Pbo> overfit = variants.size() > 2 ? pboOf(matrix, 8) : std::nullopt;
    if (overfit)
        gate("pbo", "Overfitting (PBO)",
             QStringLiteral("%1% chance the best-looking settings are below average on unseen data (needs under 50%)")
                 .arg(js::toFixed(overfit->pbo * 100, 1)),
             overfit->pbo < 0.5);

    // Sharpe significance
    Values srs;
    for (const Variant &v : variants)
        if (v.sharpe) srs << *v.sharpe;
    const double varSr =
        srs.size() > 1 ? std::pow(moments(srs).sd, 2.0) * (double(srs.size()) / double(srs.size() - 1)) : 0;
    const double sr0 = expectedMaxSharpe(std::max<double>(srs.size(), 1), varSr);
    const std::optional<double> probSharpe = psr(baseR, 0), deflated = psr(baseR, sr0);
    if (deflated)
        gate("dsr", "Deflated Sharpe",
             QStringLiteral("%1 probability the edge is real after %2 version%3 tried (needs above 0.95)")
                 .arg(js::toFixed(*deflated, 3))
                 .arg(srs.size())
                 .arg(srs.size() == 1 ? QString() : QStringLiteral("s")),
             *deflated > 0.95);

    // costs
    const double stressSlip = js::max(jsNumber(bt::orDefault(strategy.value("slippagePct"), 0.05)) * 3, 0.05);
    QJsonObject stressedStrategy = strategy;
    stressedStrategy.insert("feePct", jsNumber(bt::orDefault(strategy.value("feePct"), 0.1)) * 2);
    stressedStrategy.insert("slippagePct", stressSlip);
    stressedStrategy.insert("spreadPct", jsNumber(bt::orDefault(strategy.value("spreadPct"), 0)) * 2);
    const bt::Run stressed = bt::run(bars, stressedStrategy, subBars);
    const QJsonValue stressedPct = stressed.stats.value("netProfitPct");
    gate("cost", "Cost stress",
         QStringLiteral("%1%2% with double fees and %3% slippage a side%4 (needs above 0)")
             .arg(jsNumber(stressedPct) >= 0 ? QStringLiteral("+") : QString(), jsString(stressedPct),
                  js::numberToString(bt::stringToNumber(js::toFixed(stressSlip, 3))),
                  bt::truthy(strategy.value("spreadPct")) ? QStringLiteral(" and double the spread") : QString()),
         jsNumber(stressedPct) > 0);

    // robustness: share of neighbouring settings that keep at least half the base Sharpe
    QJsonValue robustness = null;
    if (variants.size() > 1) {
        double b = 0;
        for (const Variant &v : variants)
            if (v.isBase) {
                b = v.sharpe ? *v.sharpe : 0;
                break;
            }
        int others = 0, ok = 0;
        for (const Variant &v : variants) {
            if (v.isBase) continue;
            others++;
            if (v.sharpe && b > 0 && *v.sharpe >= b * 0.5) ok++;
        }
        const double share = (double(ok) / others) * 100;
        QJsonArray params;
        for (int k = 0; k < tun.size(); ++k)
            params.append(QJsonObject{ { "label", tun[k].label }, { "values", numArray(axes[k]) }, { "base", tun[k].base } });
        robustness = QJsonObject{ { "share", share }, { "params", params } };
        gate("robust", "Nearby settings",
             QStringLiteral("%1% of nearby settings keep at least half the Sharpe (needs 60%+)").arg(js::toFixed(share, 0)),
             share >= 60);
    }

    // a trade's % is on the money in it; the account only moves by the share positionPct put in
    const double sizePct = jsNumber(bt::orDefault(strategy.value("positionPct"), 100));
    Values pcts;
    for (const bt::Trade &t : base.trades) pcts << (t.pnlPct * sizePct) / 100;
    const QJsonValue mc = monteCarlo(pcts, options.mcRuns);

    QJsonArray variantsOut, tunablesOut;
    for (const Variant &v : variants)
        variantsOut.append(QJsonObject{
            { "values", numArray(v.values) },
            { "isBase", v.isBase },
            { "sharpe", v.sharpe ? QJsonValue(*v.sharpe * ann) : null },
            { "netPct", v.netPct },
            { "trades", v.trades },
        });
    for (int k = 0; k < tun.size(); ++k)
        tunablesOut.append(QJsonObject{ { "label", tun[k].label }, { "base", tun[k].base }, { "values", numArray(axes[k]) } });
    int passed = 0;
    for (const QJsonValue &g : gates)
        if (g.toObject().value("pass").toBool()) passed++;
    const auto baseSharpe = sharpe(baseR);
    return QJsonObject{
        { "base", QJsonObject{ { "sharpe", (baseSharpe ? *baseSharpe : 0) * ann },
                               { "netPct", base.stats.value("netProfitPct") },
                               { "trades", base.stats.value("totalTrades") } } },
        { "variants", variantsOut },
        { "tunables", tunablesOut },
        { "walkForward", walkForward },
        { "pbo", overfit ? QJsonValue(QJsonObject{ { "pbo", overfit->pbo }, { "splits", overfit->splits } }) : null },
        { "psr", numOrNull(probSharpe) },
        { "dsr", numOrNull(deflated) },
        { "sr0", sr0 * ann },
        { "trials", int(srs.size()) },
        { "costStress", stressedPct },
        { "robustness", robustness },
        { "monteCarlo", mc },
        { "gates", gates },
        { "passed", passed },
    };
}

} // namespace qtm
