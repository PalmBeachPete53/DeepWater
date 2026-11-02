// Strategy tester. A strategy is plain JSON so the UI, alerts and your AI agent can all write one.
//
// {
//   name: 'EMA cross',
//   indicators: { fast: { id: 'ema', params: { length: 9 } }, slow: { id: 'ema', params: { length: 21 } } },
//   entryLong:  [ { left: 'fast', op: 'crossesAbove', right: 'slow' } ],   // all must be true
//   exitLong:   [ { left: 'fast', op: 'crossesBelow', right: 'slow' } ],
//   entryShort: [], exitShort: [],
//   exitMatch: 'all',        // 'any': exit on whichever exit condition fires first
//   maxBarsInTrade: 0,       // time exit: close after this many bars if nothing else has (0 = off)
//   stopLossPct: 0, takeProfitPct: 0, trailingStopPct: 0,
//   initialCapital: 10000, positionPct: 100, feePct: 0.1, slippagePct: 0.05,
//   spreadPct: 0,            // bid to ask: buys fill at the ask, sells at the bid, stops trigger on that side
//   intrabar: 'worst',       // how stops and targets fill inside a bar, see FILLS below
// }
//
// Operands: a number, a price field (open/high/low/close/volume/hl2/hlc3/ohlc4), an indicator key
// ('fast'), or an indicator output ('macd.signal'). Signals are read on a bar's close and filled at
// the NEXT bar's open, so the test never trades on information it could not have had.
//
// Port of quant-terminal/public/lib/backtest.js: same arithmetic in the same order (results match
// Node bit for bit), same messages.

#include "Backtest.h"
#include "BacktestInternal.h"
#include "Error.h"
#include "Indicators.h"
#include "JsCompat.h"

#include <QJsonDocument>
#include <QRegularExpression>

#include <cmath>
#include <optional>

namespace qtm {

namespace {
const QStringList PRICE_FIELDS = { "open", "high", "low", "close", "volume", "hl2", "hlc3", "ohlc4", "hlcc4" };
}

const QStringList OPS = { ">", "<", ">=", "<=", "crossesAbove", "crossesBelow", "crosses" };
const QStringList INTRABAR = { "worst", "ohlc", "magnifier" };

// ---------------------------------------------------------------- JS value semantics
namespace bt {

QString jsString(const QJsonValue &v)
{
    switch (v.type()) {
    case QJsonValue::Double: return js::numberToString(v.toDouble());
    case QJsonValue::String: return v.toString();
    case QJsonValue::Bool: return v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    case QJsonValue::Null: return QStringLiteral("null");
    case QJsonValue::Array: {
        QStringList parts;
        for (const QJsonValue &x : v.toArray()) parts << ((x.isNull() || x.isUndefined()) ? QString() : jsString(x));
        return parts.join(QLatin1Char(','));
    }
    case QJsonValue::Object: return QStringLiteral("[object Object]");
    default: return QStringLiteral("undefined");
    }
}

static bool isJsSpace(QChar c)
{
    return c.isSpace() || c == QChar(0xFEFF) || c == QChar(0x2028) || c == QChar(0x2029);
}

double stringToNumber(const QString &str)
{
    qsizetype a = 0, b = str.size();
    while (a < b && isJsSpace(str[a])) ++a;
    while (b > a && isJsSpace(str[b - 1])) --b;
    const QString s = str.mid(a, b - a);
    if (s.isEmpty()) return 0;
    if (s == QLatin1String("Infinity") || s == QLatin1String("+Infinity")) return js::Inf;
    if (s == QLatin1String("-Infinity")) return -js::Inf;
    if (s.size() > 2 && s[0] == QLatin1Char('0')) {
        const QChar k = s[1].toLower();
        const int base = k == QLatin1Char('x') ? 16 : k == QLatin1Char('o') ? 8 : k == QLatin1Char('b') ? 2 : 0;
        if (base) {
            double out = 0;
            for (qsizetype i = 2; i < s.size(); ++i) {
                const int d = s[i].isDigit() ? s[i].digitValue()
                              : (s[i].toLower() >= QLatin1Char('a') && s[i].toLower() <= QLatin1Char('f'))
                                  ? s[i].toLower().unicode() - 'a' + 10
                                  : 99;
                if (d >= base) return NaN;
                out = out * base + d;
            }
            return out;
        }
    }
    static const QRegularExpression DEC(QStringLiteral("^[+-]?(\\d+\\.?\\d*|\\.\\d+)([eE][+-]?\\d+)?$"));
    if (!DEC.match(s).hasMatch()) return NaN;
    return s.toDouble();   // QString::toDouble is locale-independent and correctly rounded
}

double jsNumber(const QJsonValue &v)
{
    switch (v.type()) {
    case QJsonValue::Double: return v.toDouble();
    case QJsonValue::Bool: return v.toBool() ? 1 : 0;
    case QJsonValue::Null: return 0;
    case QJsonValue::String: return stringToNumber(v.toString());
    case QJsonValue::Array: {
        const QJsonArray a = v.toArray();
        if (a.isEmpty()) return 0;
        if (a.size() == 1) return stringToNumber(jsString(a.at(0)));
        return NaN;
    }
    default: return NaN;
    }
}

bool truthy(const QJsonValue &v)
{
    switch (v.type()) {
    case QJsonValue::Double: return js::truthy(v.toDouble());
    case QJsonValue::Bool: return v.toBool();
    case QJsonValue::String: return !v.toString().isEmpty();
    case QJsonValue::Array:
    case QJsonValue::Object: return true;
    default: return false;
    }
}

// a === b for JSON values (objects and arrays are never the same object here).
static bool strictEqual(const QJsonValue &a, const QJsonValue &b)
{
    if (a.type() != b.type()) return false;
    switch (a.type()) {
    case QJsonValue::Double: return a.toDouble() == b.toDouble();
    case QJsonValue::String: return a.toString() == b.toString();
    case QJsonValue::Bool: return a.toBool() == b.toBool();
    case QJsonValue::Null:
    case QJsonValue::Undefined: return true;
    default: return false;
    }
}

} // namespace bt

using bt::jsNumber;
using bt::jsString;

// A drawn line as an operand: { line: { t1, p1, t2, p2, extend: 'none' | 'right' | 'both' } }.
// Its price at time t, or NaN where the line doesn't reach (a trend line outside its two points,
// a ray behind its start).
std::function<double(double)> linePrice(const QJsonObject &line)
{
    const QJsonValue T1 = line.value("t1"), T2 = line.value("t2");
    const double t1 = jsNumber(T1), p1 = jsNumber(line.value("p1"));
    const double t2 = jsNumber(T2), p2 = jsNumber(line.value("p2"));
    const QJsonValue ext = line.value("extend");
    const bool none = ext.isUndefined() || (ext.isString() && ext.toString() == QLatin1String("none"));
    const bool right = ext.isString() && ext.toString() == QLatin1String("right");
    const bool differ = !bt::strictEqual(T2, T1);
    const double slope = differ ? (p2 - p1) / (t2 - t1) : 0;
    const double lo = js::min(t1, t2), hi = js::max(t1, t2);
    return [=](double t) -> double {
        if (none && (t < lo || t > hi)) return NaN;
        if (right && differ && (t - t1) * (t2 - t1) < 0) return NaN;
        return p1 + slope * (t - t1);
    };
}

namespace {

SeriesFn fromValues(const Values &v)
{
    return [v](int i) { return i >= 0 && i < v.size() ? v[i] : NaN; };
}

// res[outKey] when it's an array, as numbers (JS compares the elements as numbers).
std::optional<Values> outputArray(const IndicatorResult &res, const QString &key)
{
    if (res.values.contains(key)) return res.values.value(key);
    if (key == QLatin1String("markers") && res.hasMarkers) return Values(res.markers.size(), NaN);
    if (key == QLatin1String("colors") && !res.colors.isEmpty()) {
        Values out;
        for (const QString &c : res.colors) out << bt::stringToNumber(c);
        return out;
    }
    if (key == QLatin1String("breaks") && !res.breaks.isEmpty()) {
        Values out;
        for (bool b : res.breaks) out << (b ? 1 : 0);
        return out;
    }
    const QJsonValue x = res.extra.value(key);
    if (x.isArray()) {
        Values out;
        for (const QJsonValue &e : x.toArray()) out << (e.isObject() ? NaN : jsNumber(e));
        return out;
    }
    return std::nullopt;
}

} // namespace

std::function<SeriesFn(const QJsonValue &)> resolveSeries(const Bars &bars, const QJsonObject &indicators,
                                                         ConditionCachePtr cache)
{
    if (!cache) cache = std::make_shared<ConditionCache>();
    return [bars, indicators, cache](const QJsonValue &operand) -> SeriesFn {
        if (operand.isObject() && bt::truthy(operand.toObject().value("line"))) {
            const auto at = linePrice(operand.toObject().value("line").toObject());
            return [bars, at](int i) { return i >= 0 && i < bars.size() ? at(bars[i].time) : NaN; };
        }
        if (operand.isDouble()) {
            const double v = operand.toDouble();
            return [v](int) { return v; };
        }
        if (operand.isString()) {
            const QString s = operand.toString();
            const double n = bt::stringToNumber(s);
            if (!s.trimmed().isEmpty() && !std::isnan(n)) return [n](int) { return n; };
            if (PRICE_FIELDS.contains(s)) {
                if (!cache->prices.contains(s)) cache->prices.insert(s, ta::source(bars, s));
                return fromValues(cache->prices.value(s));
            }
        }
        const QString text = jsString(operand);
        const QStringList parts = text.split(QLatin1Char('.'));
        const QString key = parts[0], out = parts.value(1);
        const QJsonValue spec = indicators.value(key);
        if (!bt::truthy(spec))
            throw Error(QStringLiteral("Unknown operand \"%1\". Define it under indicators or use a price field or number.")
                            .arg(text));
        const QJsonObject so = spec.toObject();
        const QString id = jsString(so.value("id"));
        const QString ck = QLatin1Char('@') + key;
        if (!cache->results.contains(ck)) {
            const QJsonValue params = so.value("params");
            cache->results.insert(ck, compute(id, bars, bt::truthy(params) ? params.toObject() : QJsonObject()));
        }
        const IndicatorResult &res = cache->results[ck];
        QString outKey = out;
        if (outKey.isEmpty() && bt::truthy(so.value("output"))) outKey = jsString(so.value("output"));
        if (outKey.isEmpty()) {
            const IndicatorDefPtr def = indicatorById(id);
            if (!def) throw Error(QStringLiteral("Cannot read properties of undefined (reading 'outputs')"));
            outKey = !def->outputs.isEmpty() && !def->outputs[0].key.isEmpty() ? def->outputs[0].key : QStringLiteral("value");
        }
        const std::optional<Values> s = outputArray(res, outKey);
        if (!s) throw Error(QStringLiteral("Indicator \"%1\" (%2) has no output \"%3\"").arg(key, id, outKey));
        return fromValues(*s);
    };
}

ConditionFactory conditionFn(const Bars &bars, const QJsonObject &indicators)
{
    return conditionFn(bars, indicators, std::make_shared<ConditionCache>());
}

ConditionFactory conditionFn(const Bars &bars, const QJsonObject &indicators, ConditionCachePtr cache)
{
    const auto get = resolveSeries(bars, indicators, std::move(cache));
    return [get](const QJsonObject &rule) -> Condition {
        const SeriesFn L = get(rule.value("left")), R = get(rule.value("right"));
        const QJsonValue opv = rule.value("op");
        enum { GT, LT, GE, LE, XA, XB, X, BAD } op = BAD;
        if (opv.isString()) {
            const int k = OPS.indexOf(opv.toString());
            if (k >= 0) op = decltype(op)(k);
        }
        const QString opText = jsString(opv);
        return [L, R, op, opText](int i) -> bool {
            const double l = L(i), r = R(i);
            if (std::isnan(l) || std::isnan(r)) return false;
            switch (op) {
            case GT: return l > r;
            case LT: return l < r;
            case GE: return l >= r;
            case LE: return l <= r;
            case XA: return i > 0 && l > r && L(i - 1) <= R(i - 1);
            case XB: return i > 0 && l < r && L(i - 1) >= R(i - 1);
            case X: return i > 0 && ((l > r && L(i - 1) <= R(i - 1)) || (l < r && L(i - 1) >= R(i - 1)));
            default: throw Error(QStringLiteral("Unknown operator \"%1\"").arg(opText));
            }
        };
    };
}

// FILLS: a bar only gives open, high, low and close, so where a stop or target filled inside it is a guess.
//   'worst'      stop first whenever a bar touches both (cautious)
//   'ohlc'       TradingView's broker emulator: open, then whichever of high or low is nearer the open, then the
//                other, then close
//   'magnifier'  TradingView's bar magnifier: walk the real lower-timeframe bars inside each bar, each on the
//                'ohlc' path. Pass them as backtest(bars, strategy, { subBars }); bars without any use 'ohlc'.

// The lower timeframe the magnifier uses: the finest that covers the whole test in at most `max` bars.
MagnifierTf magnifierTf(qint64 tfSeconds, double spanSeconds, int max)
{
    static const QVector<QPair<QString, qint64>> TFS = {
        { "1m", 60 }, { "5m", 300 }, { "15m", 900 }, { "1h", 3600 }, { "4h", 14400 }
    };
    for (const auto &[tf, sec] : TFS)
        if (sec < tfSeconds && spanSeconds / double(sec) <= max) return { tf, sec };
    return {};
}

namespace bt {
namespace {

using Fns = QVector<Condition>;
bool allOf(const Fns &fns, int i)
{
    if (fns.isEmpty()) return false;
    for (const Condition &f : fns)
        if (!f(i)) return false;
    return true;
}
bool anyOf(const Fns &fns, int i)
{
    for (const Condition &f : fns)
        if (f(i)) return true;
    return false;
}

double periodsPerYear(const Bars &bars)
{
    if (bars.size() < 2) return 252;
    const double step = (bars.last().time - bars[0].time) / (bars.size() - 1);
    return (365 * 86400) / step;
}

// s.entryLong.map(cond), with the TypeErrors JS gives for a non-array.
Fns mapRules(const QJsonObject &s, const QString &name, const ConditionFactory &cond)
{
    const QJsonValue v = s.value(name);
    if (v.isNull()) throw Error(QStringLiteral("Cannot read properties of null (reading 'map')"));
    if (!v.isArray()) throw Error(QStringLiteral("s.%1.map is not a function").arg(name));
    Fns out;
    for (const QJsonValue &rule : v.toArray()) {
        if (rule.isNull()) throw Error(QStringLiteral("Cannot read properties of null (reading 'left')"));
        out << cond(rule.toObject());
    }
    return out;
}

QJsonValue roundOrNull(double v, int d = 2)
{
    return std::isfinite(v) ? QJsonValue(js::toFixed(v, d).toDouble()) : QJsonValue(QJsonValue::Null);
}

QJsonObject stats(const Bars &bars, const QVector<Trade> &trades, const Values &equity, const QJsonObject &s)
{
    const double start = jsNumber(s.value("initialCapital"));
    const double end = equity.isEmpty() ? start : equity.last();
    double peak = -js::Inf, maxDd = 0;
    for (double e : equity) {
        peak = js::max(peak, e);
        maxDd = js::max(maxDd, (peak - e) / peak);
    }
    const int nr = std::max(0, int(equity.size()) - 1);
    Values rets(nr);
    for (int i = 0; i < nr; ++i) rets[i] = equity[i + 1] / equity[i] - 1;
    const double div = nr ? nr : 1;
    double acc = 0;
    for (double r : rets) acc = acc + r;
    const double mean = acc / div;
    acc = 0;
    for (double r : rets) acc = acc + std::pow(r - mean, 2.0);
    const double sd = std::sqrt(acc / div);
    acc = 0;
    for (double r : rets) acc = acc + std::pow(js::min(r, 0), 2.0);
    const double downside = std::sqrt(acc / div);
    const double ppy = periodsPerYear(bars);
    int nWins = 0, nLosses = 0;
    double grossWin = 0, lossSum = 0, winPct = 0, lossPct = 0, allPct = 0;
    double largestWin = -js::Inf, largestLoss = js::Inf;
    for (const Trade &t : trades)
        if (t.pnl > 0) {
            nWins++;
            grossWin = grossWin + t.pnl;
            winPct = winPct + t.pnlPct;
            largestWin = js::max(largestWin, t.pnl);
        }
    for (const Trade &t : trades)
        if (t.pnl <= 0) {
            nLosses++;
            lossSum = lossSum + t.pnl;
            lossPct = lossPct + t.pnlPct;
            largestLoss = js::min(largestLoss, t.pnl);
        }
    const double grossLoss = -lossSum;
    const double years = bars.size() > 1 ? (bars.last().time - bars[0].time) / (365 * 86400) : 0;
    double inMarket = 0;
    for (const Trade &t : trades) inMarket = inMarket + t.bars;
    for (const Trade &t : trades) allPct = allPct + t.pnlPct;
    const int n = int(trades.size());
    const QJsonValue null(QJsonValue::Null);

    QJsonObject o;
    o.insert("netProfit", roundOrNull(end - start));
    o.insert("netProfitPct", roundOrNull(((end - start) / start) * 100));
    o.insert("buyHoldPct", bars.size() ? roundOrNull(((bars.last().close - bars[0].close) / bars[0].close) * 100) : null);
    o.insert("cagrPct", years > 0 && end > 0 ? roundOrNull((std::pow(end / start, 1 / years) - 1) * 100) : null);
    o.insert("totalTrades", n);
    o.insert("winRatePct", n ? roundOrNull((double(nWins) / n) * 100) : null);
    o.insert("profitFactor", grossLoss > 0 ? roundOrNull(grossWin / grossLoss) : nWins ? null : QJsonValue(0));
    o.insert("maxDrawdownPct", roundOrNull(maxDd * 100));
    o.insert("avgTradePct", n ? roundOrNull(allPct / n) : null);
    o.insert("avgWinPct", nWins ? roundOrNull(winPct / nWins) : null);
    o.insert("avgLossPct", nLosses ? roundOrNull(lossPct / nLosses) : null);
    o.insert("largestWin", nWins ? roundOrNull(largestWin) : null);
    o.insert("largestLoss", nLosses ? roundOrNull(largestLoss) : null);
    o.insert("sharpe", sd > 0 ? roundOrNull((mean / sd) * std::sqrt(ppy)) : null);
    o.insert("sortino", downside > 0 ? roundOrNull((mean / downside) * std::sqrt(ppy)) : null);
    o.insert("exposurePct", bars.size() ? roundOrNull((inMarket / bars.size()) * 100) : null);
    o.insert("feesAndSlippage", QStringLiteral("%1% fee + %2% slippage per side%3")
                                    .arg(jsString(s.value("feePct")), jsString(s.value("slippagePct")),
                                         truthy(s.value("spreadPct"))
                                             ? QStringLiteral(" + %1% spread").arg(jsString(s.value("spreadPct")))
                                             : QString()));
    return o;
}

} // namespace

Run run(const Bars &bars, const QJsonObject &strategy, const Bars *subBars)
{
    QJsonObject s{
        { "initialCapital", 10000 }, { "positionPct", 100 }, { "feePct", 0.1 },     { "slippagePct", 0.05 },
        { "spreadPct", 0 },          { "intrabar", "worst" }, { "stopLossPct", 0 }, { "takeProfitPct", 0 },
        { "trailingStopPct", 0 },    { "exitMatch", "all" },  { "maxBarsInTrade", 0 }, { "entryLong", QJsonArray() },
        { "exitLong", QJsonArray() }, { "entryShort", QJsonArray() }, { "exitShort", QJsonArray() },
    };
    for (auto it = strategy.begin(); it != strategy.end(); ++it) s.insert(it.key(), it.value());

    const QJsonValue ind = s.value("indicators");
    const ConditionFactory cond = conditionFn(bars, truthy(ind) ? ind.toObject() : QJsonObject());
    const bool anyExit = s.value("exitMatch").toString() == QLatin1String("any") && s.value("exitMatch").isString();
    const Fns eLf = mapRules(s, "entryLong", cond), xLf = mapRules(s, "exitLong", cond);
    const Fns eSf = mapRules(s, "entryShort", cond), xSf = mapRules(s, "exitShort", cond);
    auto eL = [&](int i) { return allOf(eLf, i); };
    auto eS = [&](int i) { return allOf(eSf, i); };
    auto xL = [&](int i) { return anyExit ? anyOf(xLf, i) : allOf(xLf, i); };
    auto xS = [&](int i) { return anyExit ? anyOf(xSf, i) : allOf(xSf, i); };
    const double fee = jsNumber(s.value("feePct")) / 100, slip = jsNumber(s.value("slippagePct")) / 100,
                 half = jsNumber(s.value("spreadPct")) / 200;
    const QString intrabar = s.value("intrabar").isString() ? s.value("intrabar").toString() : QString();
    const bool magnifier = intrabar == QLatin1String("magnifier"), ohlc = intrabar == QLatin1String("ohlc");
    const double positionPct = jsNumber(s.value("positionPct"));
    const bool hasStop = truthy(s.value("stopLossPct")), hasTarget = truthy(s.value("takeProfitPct"));
    const double stopLossPct = jsNumber(s.value("stopLossPct")), takeProfitPct = jsNumber(s.value("takeProfitPct"));
    const bool trailing = truthy(s.value("trailingStopPct"));
    const double trailingStopPct = jsNumber(s.value("trailingStopPct"));
    const double maxBarsInTrade = jsNumber(s.value("maxBarsInTrade"));

    // Sub-bars inside each bar, for the magnifier: subAt[i] = [from, to) into subBars.
    QVector<QPair<int, int>> subAt;
    int covered = 0;
    const int n = int(bars.size());
    if (magnifier && subBars && !subBars->isEmpty()) {
        const Bars &sb = *subBars;
        int j = 0;
        for (int i = 0; i < n; i++) {
            const double end = i + 1 < n ? bars[i + 1].time
                                         : bars[i].time + (i ? bars[i].time - bars[i - 1].time : 86400);
            while (j < sb.size() && sb[j].time < bars[i].time) j++;
            const int from = j;
            while (j < sb.size() && sb[j].time < end) j++;
            subAt.push_back({ from, j });
            if (j > from) covered++;
        }
    }

    struct Pos {
        int side;
        double qty, entry, entryTime;
        int entryIndex;
        QString reason;
        double peak;
        std::optional<double> stop, target;
    };
    double cash = jsNumber(s.value("initialCapital"));
    std::optional<Pos> pos;
    Run out;
    out.eqTime.reserve(n);
    out.eqValue.reserve(n);
    struct Pending {
        bool close = false;
        int side = 0;
        QString reason;
    };
    std::optional<Pending> pending;

    // What the position would really fill at: the ask to buy, the bid to sell (price is the mid).
    auto exitQuote = [&](double price) { return price * (1 - pos->side * half); };
    auto openPos = [&](int i, int side, double price, const QString &reason) {
        const double fill = price * (1 + side * half) * (1 + side * slip);
        const double alloc = (cash * positionPct) / 100;
        const double qty = alloc / (fill * (1 + fee));
        if (qty <= 0) return;
        cash -= qty * fill * (1 + fee);
        Pos p{ side, qty, fill, bars[i].time, i, reason, fill, std::nullopt, std::nullopt };
        if (hasStop) p.stop = fill * (1 - (side * stopLossPct) / 100);
        if (hasTarget) p.target = fill * (1 + (side * takeProfitPct) / 100);
        pos = p;
    };
    auto closePos = [&](int i, double price, const QString &reason) {
        const double fill = price * (1 - pos->side * slip);
        const double proceeds = pos->side == 1 ? pos->qty * fill : pos->qty * (2 * pos->entry - fill);
        const double net = proceeds * (1 - fee);
        const double cost = pos->qty * pos->entry * (1 + fee);
        cash += net;
        Trade t;
        t.side = pos->side;
        t.entryTime = pos->entryTime;
        t.exitTime = bars[i].time;
        t.entryPrice = pos->entry;
        t.exitPrice = fill;
        t.qty = pos->qty;
        t.pnl = net - cost;
        t.pnlPct = ((net - cost) / cost) * 100;
        t.bars = i - pos->entryIndex;
        t.entryReason = pos->reason;
        t.exitReason = reason;
        out.trades.push_back(t);
        pos.reset();
    };

    auto trail = [&](double p) {
        pos->peak = pos->side == 1 ? js::max(pos->peak, p) : js::min(pos->peak, p);
        const double t = pos->peak * (1 - (pos->side * trailingStopPct) / 100);
        pos->stop = !pos->stop ? t : pos->side == 1 ? js::max(*pos->stop, t) : js::min(*pos->stop, t);
    };
    auto beyond = [&](double p, const std::optional<double> &level, bool favour) {
        return level && ((pos->side == 1) == favour ? p >= *level : p <= *level);
    };
    // 'worst': one look at the bar, stop first when both are touched.
    auto worstExit = [&](int i, const Bar &b) {
        const double hi = exitQuote(b.high), lo = exitQuote(b.low), op = exitQuote(b.open);
        if (trailing) trail(pos->side == 1 ? hi : lo);
        if (beyond(pos->side == 1 ? lo : hi, pos->stop, false))
            closePos(i, pos->side == 1 ? js::min(*pos->stop, op) : js::max(*pos->stop, op), QStringLiteral("Stop"));
        else if (beyond(pos->side == 1 ? hi : lo, pos->target, true))
            closePos(i, pos->side == 1 ? js::max(*pos->target, op) : js::min(*pos->target, op), QStringLiteral("Target"));
    };
    // 'ohlc' and 'magnifier': walk each segment's open, near extreme, far extreme, close in order.
    auto walkExit = [&](int i, const Bar *segs, int count) {
        for (int g = 0; g < count; ++g) {
            const Bar &x = segs[g];
            double pts[4];
            if (std::fabs(x.open - x.high) < std::fabs(x.open - x.low)) {
                pts[0] = x.open, pts[1] = x.high, pts[2] = x.low, pts[3] = x.close;
            } else {
                pts[0] = x.open, pts[1] = x.low, pts[2] = x.high, pts[3] = x.close;
            }
            for (double &p : pts) p = exitQuote(p);
            for (int k = 0; k < 4; k++) {
                const double p = pts[k];
                if (k == 0) {
                    // A segment that opens past a level gapped through it: filled at the open.
                    if (trailing) trail(p);
                    if (beyond(p, pos->stop, false)) return closePos(i, p, QStringLiteral("Stop"));
                    if (beyond(p, pos->target, true)) return closePos(i, p, QStringLiteral("Target"));
                    continue;
                }
                const bool favour = pos->side == 1 ? p > pts[k - 1] : p < pts[k - 1];
                if (favour) {
                    if (beyond(p, pos->target, true)) return closePos(i, *pos->target, QStringLiteral("Target"));
                    if (trailing) trail(p);
                } else if (beyond(p, pos->stop, false))
                    return closePos(i, *pos->stop, QStringLiteral("Stop"));
            }
        }
    };

    for (int i = 0; i < n; i++) {
        const Bar &b = bars[i];
        // 1. fill orders decided on the previous close
        if (pending) {
            if (pending->close && pos) closePos(i, exitQuote(b.open), pending->reason);
            if (pending->side && !pos) openPos(i, pending->side, b.open, pending->reason);
            pending.reset();
        }
        // 2. stops and targets inside this bar (see FILLS)
        if (pos && (pos->stop || pos->target || trailing)) {
            if (ohlc || magnifier) {
                const auto [from, to] = i < subAt.size() ? subAt[i] : QPair<int, int>(0, 0);
                if (to > from) walkExit(i, subBars->constData() + from, to - from);
                else walkExit(i, &b, 1);
            } else
                worstExit(i, b);
        }
        // 3. signals on this close -> orders for the next open
        if (i < n - 1) {
            if (pos && pos->side == 1 && (xL(i) || eS(i))) {
                const QString reason = xL(i) ? QStringLiteral("Exit long") : QStringLiteral("Reverse");
                pending = Pending{ true, eS(i) ? -1 : 0, reason };
            } else if (pos && pos->side == -1 && (xS(i) || eL(i))) {
                const QString reason = xS(i) ? QStringLiteral("Exit short") : QStringLiteral("Reverse");
                pending = Pending{ true, eL(i) ? 1 : 0, reason };
            }
            // Time exit: the position has been open maxBarsInTrade bars at this close, so it goes at the next open.
            else if (pos && maxBarsInTrade > 0 && i - pos->entryIndex + 1 >= maxBarsInTrade)
                pending = Pending{ true, 0, QStringLiteral("Time exit") };
            else if (!pos && eL(i))
                pending = Pending{ false, 1, QStringLiteral("Long entry") };
            else if (!pos && eS(i))
                pending = Pending{ false, -1, QStringLiteral("Short entry") };
        }
        // 4. mark to market
        const double mtm = pos ? pos->side == 1 ? pos->qty * b.close : pos->qty * (2 * pos->entry - b.close) : 0;
        out.eqTime.push_back(b.time);
        out.eqValue.push_back(cash + mtm);
    }
    if (pos) closePos(n - 1, exitQuote(bars.last().close), QStringLiteral("End of data"));
    if (!out.eqValue.isEmpty()) out.eqValue.last() = cash;

    out.stats = stats(bars, out.trades, out.eqValue, s);
    out.stats.insert("fills",
                     magnifier ? QStringLiteral("Bar magnifier: lower-timeframe bars inside %1 of %2 bars%3")
                                     .arg(covered)
                                     .arg(n)
                                     .arg(covered < n ? QStringLiteral(", the rest on the OHLC path") : QString())
                     : ohlc ? QStringLiteral("OHLC path: open, nearer extreme, far extreme, close")
                            : QStringLiteral("Stop first when a bar touches both"));
    out.stats.insert("intrabarCovered", covered);
    return out;
}

QJsonObject toJson(const Run &r)
{
    QJsonArray trades, equity;
    for (const Trade &t : r.trades)
        trades.append(QJsonObject{
            { "side", t.side == 1 ? "long" : "short" },
            { "entryTime", t.entryTime },
            { "exitTime", t.exitTime },
            { "entryPrice", t.entryPrice },
            { "exitPrice", t.exitPrice },
            { "qty", t.qty },
            { "pnl", t.pnl },
            { "pnlPct", t.pnlPct },
            { "bars", t.bars },
            { "entryReason", t.entryReason },
            { "exitReason", t.exitReason },
        });
    for (int i = 0; i < r.eqTime.size(); ++i)
        equity.append(QJsonObject{ { "time", r.eqTime[i] }, { "value", r.eqValue[i] } });
    return QJsonObject{ { "trades", trades }, { "equity", equity }, { "stats", r.stats } };
}

} // namespace bt

QJsonObject backtest(const Bars &bars, const QJsonObject &strategy, const Bars *subBars)
{
    return bt::toJson(bt::run(bars, strategy, subBars));
}

QString tradesCsv(const QJsonArray &trades)
{
    QStringList lines{ QStringLiteral(
        "side,entry_time,exit_time,entry_price,exit_price,qty,pnl,pnl_pct,bars,entry_reason,exit_reason") };
    auto iso = [](const QJsonValue &t) { return js::toIsoString(jsNumber(t) * 1000); };
    auto cell = [](const QJsonValue &v) { return (v.isNull() || v.isUndefined()) ? QString() : jsString(v); };
    for (const QJsonValue &tv : trades) {
        const QJsonObject t = tv.toObject();
        lines << QStringList{
            cell(t.value("side")),
            iso(t.value("entryTime")),
            iso(t.value("exitTime")),
            cell(t.value("entryPrice")),
            cell(t.value("exitPrice")),
            cell(t.value("qty")),
            js::toFixed(t.value("pnl").toDouble(), 2),
            js::toFixed(t.value("pnlPct").toDouble(), 2),
            cell(t.value("bars")),
            cell(t.value("entryReason")),
            cell(t.value("exitReason")),
        }.join(QLatin1Char(','));
    }
    return lines.join(QLatin1Char('\n'));
}

QVector<QPair<QString, QJsonObject>> exampleStrategies()
{
    static const char *const JSON[][2] = {
        { "EMA cross (9/21)", R"({
            "indicators": { "fast": { "id": "ema", "params": { "length": 9 } }, "slow": { "id": "ema", "params": { "length": 21 } } },
            "entryLong": [{ "left": "fast", "op": "crossesAbove", "right": "slow" }],
            "exitLong": [{ "left": "fast", "op": "crossesBelow", "right": "slow" }] })" },
        { "RSI mean reversion (30/55)", R"({
            "indicators": { "rsi": { "id": "rsi", "params": { "length": 14 } } },
            "entryLong": [{ "left": "rsi", "op": "crossesAbove", "right": 30 }],
            "exitLong": [{ "left": "rsi", "op": "crossesAbove", "right": 55 }],
            "stopLossPct": 8 })" },
        { "Supertrend follow", R"({
            "indicators": { "st": { "id": "supertrend", "params": { "atrLength": 10, "factor": 3 }, "output": "direction" } },
            "entryLong": [{ "left": "st", "op": "<", "right": 0 }],
            "exitLong": [{ "left": "st", "op": ">", "right": 0 }] })" },
        { "MACD + 200 EMA trend filter", R"({
            "indicators": { "macd": { "id": "macd", "params": {} }, "trend": { "id": "ema", "params": { "length": 200 } } },
            "entryLong": [{ "left": "macd.macd", "op": "crossesAbove", "right": "macd.signal" },
                          { "left": "close", "op": ">", "right": "trend" }],
            "exitLong": [{ "left": "macd.macd", "op": "crossesBelow", "right": "macd.signal" }] })" },
        { "Bollinger breakout", R"({
            "indicators": { "bb": { "id": "bb", "params": { "length": 20, "mult": 2 } } },
            "entryLong": [{ "left": "close", "op": "crossesAbove", "right": "bb.upper" }],
            "exitLong": [{ "left": "close", "op": "crossesBelow", "right": "bb.basis" }],
            "trailingStopPct": 10 })" },
    };
    QVector<QPair<QString, QJsonObject>> out;
    for (const auto &e : JSON) out.push_back({ QString::fromUtf8(e[0]), QJsonDocument::fromJson(e[1]).object() });
    return out;
}

} // namespace qtm
