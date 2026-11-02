// qtm_test_backtest: the C++ port of quant-terminal/public/lib/{backtest,quant}.js checked against
// the JS originals (reference/reference.json, written by make_reference.mjs), bit for bit, plus
// the JS test suite (test/backtest.test.js, test/quant.test.js) ported as it is.
#include "core/Backtest.h"
#include "core/Error.h"
#include "core/JsCompat.h"
#include "core/Quant.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QRegularExpression>

#include <cstring>
#include <iostream>

using namespace qtm;

// ---------------------------------------------------------------- bookkeeping
namespace {

struct Stats {
    qint64 checks = 0, fails = 0;
    QMap<QString, int> failsBy;
    QMap<QString, qint64> checksBy;
} S;
QString CAT = "misc";

void fail(const QString &msg)
{
    S.fails++;
    if (S.failsBy[CAT]++ < 15) std::cout << "FAIL [" << CAT.toStdString() << "] " << msg.toStdString() << "\n";
}
void check(bool ok, const QString &msg)
{
    S.checks++;
    S.checksBy[CAT]++;
    if (!ok) fail(msg);
}
QString num(double v)
{
    return QString::number(v, 'g', 17);
}
// Bit-exact (NaN == NaN; -0 == 0, since QJsonValue doesn't keep -0 and JS shows it as 0).
bool same(double a, double b)
{
    return (std::isnan(a) && std::isnan(b)) || a == b;
}
void checkNum(double want, double got, const QString &ctx)
{
    check(same(want, got), QStringLiteral("%1: want %2, got %3").arg(ctx, num(want), num(got)));
}
void checkStr(const QString &want, const QString &got, const QString &ctx)
{
    check(want == got, QStringLiteral("%1: want \"%2\", got \"%3\"").arg(ctx, want, got));
}

// ---------------------------------------------------------------- reference decoding
Values f64(const QJsonValue &v)
{
    const QString s = v.toString();
    if (!s.startsWith(QLatin1String("f64:"))) return {};
    const QByteArray raw = QByteArray::fromBase64(s.mid(4).toLatin1());
    Values out(raw.size() / 8);
    std::memcpy(out.data(), raw.constData(), size_t(out.size()) * 8);
    return out;
}
bool isF64(const QJsonValue &v)
{
    return v.isString() && v.toString().startsWith(QLatin1String("f64:"));
}
double jnum(const QJsonValue &v)
{
    return isF64(v) ? f64(v).value(0, NaN) : v.toDouble(NaN);
}
Values jnums(const QJsonValue &arr)
{
    Values out;
    for (const QJsonValue &x : arr.toArray()) out << jnum(x);
    return out;
}
Bars barsFrom(const QJsonObject &o)
{
    const Values t = f64(o["time"]), op = f64(o["open"]), h = f64(o["high"]), l = f64(o["low"]), c = f64(o["close"]),
                 v = f64(o["volume"]);
    Bars out(t.size());
    for (int i = 0; i < t.size(); ++i) out[i] = { t[i], op[i], h[i], l[i], c[i], v[i], {} };
    return out;
}

// A JS value dumped by enc() (numbers as f64 strings) against the C++ JSON.
void compareJson(const QJsonValue &want, const QJsonValue &got, const QString &ctx)
{
    if (isF64(want)) {
        if (!got.isDouble()) return check(false, QStringLiteral("%1: want number %2, got type %3").arg(ctx, num(jnum(want))).arg(got.type()));
        return checkNum(jnum(want), got.toDouble(), ctx);
    }
    if (want.type() != got.type())
        return check(false, QStringLiteral("%1: want type %2 (%3), got type %4 (%5)")
                                .arg(ctx)
                                .arg(want.type())
                                .arg(QString::fromUtf8(QJsonDocument(QJsonArray{ want }).toJson(QJsonDocument::Compact)))
                                .arg(got.type())
                                .arg(QString::fromUtf8(QJsonDocument(QJsonArray{ got }).toJson(QJsonDocument::Compact))));
    switch (want.type()) {
    case QJsonValue::Array: {
        const QJsonArray w = want.toArray(), g = got.toArray();
        if (w.size() != g.size()) return check(false, QStringLiteral("%1: length want %2, got %3").arg(ctx).arg(w.size()).arg(g.size()));
        for (int i = 0; i < w.size(); ++i) compareJson(w[i], g[i], QStringLiteral("%1[%2]").arg(ctx).arg(i));
        return;
    }
    case QJsonValue::Object: {
        const QJsonObject w = want.toObject(), g = got.toObject();
        check(w.keys() == g.keys(), QStringLiteral("%1: keys want %2, got %3").arg(ctx, w.keys().join(','), g.keys().join(',')));
        for (auto it = w.begin(); it != w.end(); ++it)
            if (g.contains(it.key())) compareJson(it.value(), g.value(it.key()), ctx + "." + it.key());
        return;
    }
    case QJsonValue::String: return checkStr(want.toString(), got.toString(), ctx);
    case QJsonValue::Bool: return check(want.toBool() == got.toBool(), ctx + ": bool");
    default: return check(true, ctx);
    }
}

QJsonObject REF;
QHash<QString, Bars> SETS;
const Bars *setOrNull(const QJsonValue &name)
{
    return name.isString() && SETS.contains(name.toString()) ? &SETS[name.toString()] : nullptr;
}

// ---------------------------------------------------------------- reference comparisons
void testBacktests()
{
    CAT = "backtest";
    for (const QJsonValue &cv : REF["backtests"].toArray()) {
        const QJsonObject c = cv.toObject();
        const QString name = c["name"].toString();
        const QJsonObject want = c["result"].toObject();
        QJsonObject got;
        QString error;
        try {
            got = backtest(SETS[c["set"].toString()], c["strategy"].toObject(), setOrNull(c["sub"]));
        } catch (const Error &e) {
            error = e.message();
        }
        if (want.contains("error")) {
            checkStr(want["error"].toString(), error, name + " error");
            continue;
        }
        if (!error.isEmpty()) {
            check(false, name + ": threw " + error);
            continue;
        }
        QJsonObject ok = want["ok"].toObject();
        const QString csv = ok.take("csv").toString();
        compareJson(ok, got, name);
        CAT = "tradesCsv";
        checkStr(csv, tradesCsv(got["trades"].toArray()), name + " csv");
        CAT = "backtest";
    }
}

QVector<QVector<bool>> runRules(const Bars &bars, const QJsonObject &inds, const QJsonArray &rules,
                                ConditionCachePtr cache = nullptr)
{
    const ConditionFactory make = cache ? conditionFn(bars, inds, cache) : conditionFn(bars, inds);
    QVector<QVector<bool>> out;
    for (const QJsonValue &r : rules) {
        QVector<bool> row;
        for (int i = 0; i < bars.size(); ++i) row << make(r.toObject())(i);
        out << row;
    }
    return out;
}
void compareBools(const QJsonValue &want, const QVector<QVector<bool>> &got, const QString &ctx)
{
    QJsonArray g;
    for (const auto &row : got) {
        QJsonArray r;
        for (bool b : row) r.append(b);
        g.append(r);
    }
    compareJson(want, g, ctx);
}

void testConditions()
{
    CAT = "conditionFn";
    for (const QJsonValue &cv : REF["conditions"].toArray()) {
        const QJsonObject c = cv.toObject();
        const QString name = c["name"].toString();
        const Bars &bars = SETS[c["set"].toString()];
        const QJsonObject inds = c["indicators"].toObject();
        const QJsonArray rules = c["rules"].toArray();
        if (c.contains("error")) {
            QString err;
            try {
                conditionFn(bars, inds)(rules[0].toObject())(int(bars.size()) - 1);
            } catch (const Error &e) {
                err = e.message();
            }
            checkStr(c["error"].toString(), err, name);
        } else if (c.contains("first")) {
            auto cache = std::make_shared<ConditionCache>();
            compareBools(c["first"], runRules(bars, inds, rules, cache), name + " first");
            compareBools(c["second"], runRules(bars, c["indicators2"].toObject(), rules, cache), name + " second");
            compareBools(c["fresh"], runRules(bars, c["indicators2"].toObject(), rules), name + " fresh");
        } else
            compareBools(c["out"], runRules(bars, inds, rules), name);
    }

    CAT = "linePrice";
    const Values times = jnums(REF["times"]);
    for (const QJsonValue &lv : REF["linePrices"].toArray()) {
        const QJsonObject l = lv.toObject();
        const auto at = linePrice(l["line"].toObject());
        QJsonArray got;
        for (double t : times) got.append(at(t));
        compareJson(l["out"], got, QString::fromUtf8(QJsonDocument(l["line"].toObject()).toJson(QJsonDocument::Compact)));
    }

    CAT = "magnifierTf";
    for (const QJsonValue &mv : REF["magnifier"].toArray()) {
        const QJsonObject m = mv.toObject();
        const MagnifierTf got = magnifierTf(m["tf"].toInteger(), jnum(m["span"]), m["max"].toInt());
        const QString ctx = QStringLiteral("tf %1 span %2 max %3").arg(m["tf"].toInteger()).arg(num(jnum(m["span"]))).arg(m["max"].toInt());
        if (m["out"].isNull()) check(got.tf.isEmpty(), ctx + ": want null, got " + got.tf);
        else {
            checkStr(m["out"]["tf"].toString(), got.tf, ctx);
            checkNum(jnum(m["out"]["seconds"]), double(got.seconds), ctx);
        }
    }

    CAT = "examples";
    const auto ex = exampleStrategies();
    const QJsonArray want = REF["examples"].toArray();
    check(ex.size() == want.size(), "count");
    for (int i = 0; i < std::min<qsizetype>(ex.size(), want.size()); ++i) {
        checkStr(want[i]["name"].toString(), ex[i].first, "name");
        check(want[i]["strategy"].toObject() == ex[i].second, ex[i].first + ": strategy differs");
    }
    QStringList ops, ib;
    for (const QJsonValue &v : REF["ops"].toArray()) ops << v.toString();
    for (const QJsonValue &v : REF["intrabar"].toArray()) ib << v.toString();
    check(ops == OPS, "OPS");
    check(ib == INTRABAR, "INTRABAR");
}

QJsonValue opt(const std::optional<double> &v)
{
    return v ? QJsonValue(*v) : QJsonValue(QJsonValue::Null);
}

void testMaths()
{
    CAT = "quant maths";
    const QJsonObject m = REF["maths"].toObject();
    const Values xs = jnums(m["xs"]), ps = jnums(m["ps"]);
    {
        QJsonArray a, b;
        for (double x : xs) a.append(normCdf(x));
        for (double p : ps) b.append(normInv(p));
        compareJson(m["normCdf"], a, "normCdf");
        compareJson(m["normInv"], b, "normInv");
    }
    const QJsonObject series = m["series"].toObject();
    for (auto it = series.begin(); it != series.end(); ++it) {
        const QJsonObject s = it.value().toObject();
        const Values r = jnums(s["r"]);
        const Moments mo = moments(r);
        compareJson(s["moments"], QJsonObject{ { "mean", mo.mean }, { "sd", mo.sd }, { "skew", mo.skew }, { "kurt", mo.kurt } },
                    it.key() + " moments");
        compareJson(s["sharpe"], opt(sharpe(r)), it.key() + " sharpe");
        compareJson(s["psr0"], opt(psr(r)), it.key() + " psr");
        compareJson(s["psr1"], opt(psr(r, 0.05)), it.key() + " psr 0.05");
    }
    for (const QJsonValue &e : m["ems"].toArray())
        compareJson(e["out"], expectedMaxSharpe(jnum(e["n"]), jnum(e["v"])), QStringLiteral("expectedMaxSharpe(%1, %2)").arg(num(jnum(e["n"])), num(jnum(e["v"]))));
    for (const QJsonValue &pv : m["pbo"].toArray()) {
        QVector<Values> matrix;
        for (const QJsonValue &row : pv["matrix"].toArray()) matrix << jnums(row);
        compareJson(pv["out"], pbo(matrix), "pbo " + pv["name"].toString());
        compareJson(pv["out6"], pbo(matrix, 6), "pbo/6 " + pv["name"].toString());
    }
    for (const QJsonValue &mv : m["mc"].toArray()) {
        const QJsonObject c = mv.toObject();
        compareJson(c["out"], monteCarlo(jnums(c["trades"]), c["runs"].toInt(), jnum(c["seed"])),
                    QStringLiteral("monteCarlo runs %1 seed %2").arg(c["runs"].toInt()).arg(num(jnum(c["seed"]))));
    }
}

void testQuant()
{
    CAT = "quantChecks";
    int defaultOrderCases = 0;
    for (const QJsonValue &cv : REF["quant"].toArray()) {
        const QJsonObject c = cv.toObject();
        const QString name = c["name"].toString();
        const QJsonObject strategy = c["strategy"].toObject();
        QStringList order;
        for (const QJsonValue &k : c["order"].toArray()) order << k.toString();
        QuantOptions o;
        o.mcRuns = c.contains("mcRuns") ? c["mcRuns"].toInt() : 2000;
        o.subBars = setOrNull(c["sub"]);
        o.indicatorOrder = order;
        const QJsonObject want = c["result"].toObject();
        auto runIt = [&](const QuantOptions &opts, const QString &ctx) {
            QJsonObject got;
            QString error;
            try {
                got = quantChecks(SETS[c["set"].toString()], strategy, opts);
            } catch (const Error &e) {
                error = e.message();
            }
            if (want.contains("error")) return checkStr(want["error"].toString(), error, ctx + " error");
            if (!error.isEmpty()) return check(false, ctx + ": threw " + error);
            compareJson(want["ok"], got, ctx);
        };
        runIt(o, name);
        // The order the tester UI builds is the default: same result without being told the order.
        if (indicatorOrder(strategy) == order) {
            defaultOrderCases++;
            QuantOptions d = o;
            d.indicatorOrder.clear();
            if (c.contains("mcRuns")) runIt(d, name + " (default order)");
            else {
                CAT = "quantChecks (default args)";
                QJsonObject got = quantChecks(SETS[c["set"].toString()], strategy);
                compareJson(want["ok"], got, name + " (default args)");
                CAT = "quantChecks";
            }
        }
    }
    CAT = "quantChecks";
    check(defaultOrderCases >= 30, QStringLiteral("default order matches JS in %1 cases").arg(defaultOrderCases));
}

// ---------------------------------------------------------------- JS test suite, ported
Bars fixtureBars()
{
    QFile f(QStringLiteral(QTM_FIXTURES "/btc-1d.json"));
    if (!f.open(QIODevice::ReadOnly)) return {};
    Bars out;
    for (const QJsonValue &v : QJsonDocument::fromJson(f.readAll()).array()) {
        const QJsonObject o = v.toObject();
        out.push_back({ o["time"].toDouble(), o["open"].toDouble(), o["high"].toDouble(), o["low"].toDouble(),
                        o["close"].toDouble(), o["volume"].toDouble(), {} });
    }
    return out;
}
QJsonObject example(const QString &name)
{
    for (const auto &[n, s] : exampleStrategies())
        if (n == name) return s;
    return {};
}
QJsonObject merged(QJsonObject a, const QJsonObject &b)
{
    for (auto it = b.begin(); it != b.end(); ++it) a.insert(it.key(), it.value());
    return a;
}
QJsonArray rules(const char *json)
{
    return QJsonDocument::fromJson(json).array();
}
Bar mk(double t, double o, double h, double l, double c)
{
    return { t, o, h, l, c, 1, {} };
}

void testJsSuite()
{
    const Bars bars = fixtureBars();
    CAT = "backtest.test.js";
    {   // fills on the next bar open, never the signal bar
        const QJsonObject r = backtest(bars, example("EMA cross (9/21)"));
        check(r["trades"].toArray().size() > 5, "more than 5 trades");
        for (const QJsonValue &t : r["trades"].toArray()) {
            int i = -1;
            for (int k = 0; k < bars.size(); ++k)
                if (bars[k].time == t["entryTime"].toDouble()) {
                    i = k;
                    break;
                }
            check(i > 0, "entry bar found");
            if (i > 0) check(std::fabs(t["entryPrice"].toDouble() - bars[i].open * 1.0005) < 1e-6, "entry = open x (1 + slippage)");
        }
    }
    {   // equity curve accounts for every trade
        const QJsonObject r = backtest(bars, merged(example("RSI mean reversion (30/55)"), { { "initialCapital", 10000 } }));
        double sum = 0;
        for (const QJsonValue &t : r["trades"].toArray()) sum += t["pnl"].toDouble();
        check(std::fabs(10000 + sum - r["equity"].toArray().last()["value"].toDouble()) < 1e-6, "equity = capital + pnl");
        check(r["stats"]["totalTrades"].toInt() == r["trades"].toArray().size(), "totalTrades");
    }
    {   // stops cap the loss of a losing trade
        const QJsonObject r = backtest(bars, merged(example("EMA cross (9/21)"), { { "stopLossPct", 2 }, { "feePct", 0 }, { "slippagePct", 0 } }));
        int stopped = 0;
        for (const QJsonValue &t : r["trades"].toArray())
            if (t["exitReason"].toString() == "Stop") {
                stopped++;
                check(t["pnlPct"].toDouble() >= -2 - 15, "gap through the stop is the only way past -2%");
            }
        check(stopped > 0, "some stops");
    }
    {   // shorts make money when price falls
        Bars down;
        for (int i = 0; i < 60; ++i) down.push_back(mk(i * 86400, 100 - i, 101 - i, 99 - i, 100 - i));
        const QJsonObject r = backtest(down, QJsonObject{ { "indicators", QJsonObject() },
                                                          { "entryShort", rules(R"([{"left":"close","op":"<","right":95}])") },
                                                          { "exitShort", rules(R"([{"left":"close","op":"<","right":50}])") },
                                                          { "feePct", 0 }, { "slippagePct", 0 } });
        check(r["trades"][0]["side"].toString() == "short", "short side");
        check(r["trades"][0]["pnl"].toDouble() > 0, "short pnl > 0");
    }
    for (const auto &[name, s] : exampleStrategies()) {   // every example strategy runs and exports
        const QJsonObject r = backtest(bars, s);
        check(r["equity"].toArray().size() == bars.size(), name + " equity length");
        check(tradesCsv(r["trades"].toArray()).startsWith("side,"), name + " csv");
    }
    {   // a drawn line gives its price at any time, within its reach
        auto line = [](const char *ext) { return QJsonObject{ { "t1", 100 }, { "p1", 10 }, { "t2", 200 }, { "p2", 20 }, { "extend", ext } }; };
        check(linePrice(line("none"))(150) == 15, "trend 150");
        check(std::isnan(linePrice(line("none"))(250)), "a trend line stops at its second point");
        check(linePrice(line("right"))(300) == 30, "ray 300");
        check(std::isnan(linePrice(line("right"))(50)), "a ray has nothing behind its start");
        check(linePrice(line("both"))(0) == 0, "extended 0");
    }
    {   // price crossing a drawn line is detected bar by bar
        Bars series;
        const double closes[] = { 5, 6, 7, 12, 13 };
        for (int i = 0; i < 5; ++i) series.push_back(mk(i * 10, closes[i], closes[i], closes[i], closes[i]));
        const QJsonObject rising{ { "line", QJsonObject{ { "t1", 0 }, { "p1", 8 }, { "t2", 40 }, { "p2", 12 }, { "extend", "right" } } } };
        const Condition test = conditionFn(series, {})(QJsonObject{ { "left", "close" }, { "op", "crossesAbove" }, { "right", rising } });
        QVector<bool> got;
        for (int i = 0; i < 5; ++i) got << test(i);
        check(got == QVector<bool>({ false, false, false, true, false }), "line crossing");
    }
    auto flat = [](const Values &closes) {
        Bars b;
        for (int i = 0; i < closes.size(); ++i) b.push_back(mk(1700000000 + i * 86400, closes[i], closes[i], closes[i], closes[i]));
        return b;
    };
    const QJsonObject noCosts{ { "feePct", 0 }, { "slippagePct", 0 } };
    {   // exitMatch any exits on whichever exit condition fires first; all waits for both
        const Bars b = flat({ 100, 100, 120, 130, 125, 90, 80, 80 });
        const QJsonObject s = merged(noCosts, { { "entryLong", rules(R"([{"left":"close","op":">","right":110}])") },
                                                { "exitLong", rules(R"([{"left":"close","op":"<","right":128},{"left":"close","op":"<","right":100}])") } });
        const QJsonObject any = merged(s, { { "exitMatch", "any" } });
        check(backtest(b, any)["trades"][0]["exitTime"].toDouble() == b[5].time, "any exits at bar 5");
        check(backtest(b, s)["trades"][0]["exitTime"].toDouble() == b[6].time, "all exits at bar 6");
        check(backtest(b, any)["trades"][0]["exitReason"].toString() == "Exit long", "exit reason");
    }
    {   // maxBarsInTrade closes a trade after that many bars unless an exit fires first
        Values c;
        for (int i = 0; i < 30; ++i) c << 100 + i;
        const Bars b = flat(c);
        const QJsonObject s = merged(noCosts, { { "entryLong", rules(R"([{"left":"close","op":">","right":0}])") }, { "maxBarsInTrade", 5 } });
        const QJsonArray trades = backtest(b, s)["trades"].toArray();
        check(trades.size() > 2, "several trades");
        for (int i = 0; i + 1 < trades.size(); ++i) {
            check(trades[i]["bars"].toInt() == 5, "5 bars");
            check(trades[i]["exitReason"].toString() == "Time exit", "time exit");
        }
        const QJsonValue early = backtest(b, merged(s, { { "exitLong", rules(R"([{"left":"close","op":">=","right":103}])") } }))["trades"][0];
        check(early["exitReason"].toString() == "Exit long", "early exit reason");
        check(early["bars"].toInt() < 5, "early exit bars");
        check(backtest(b, merged(s, { { "maxBarsInTrade", 0 } }))["trades"].toArray().size() == 1, "0 turns it off");
    }
    auto day = [](int i, double o, double h, double l, double c) { return mk(1700000000 + i * 86400, o, h, l, c); };
    const QJsonArray always = rules(R"([{"left":"close","op":">","right":0}])");
    {   // the spread fills buys at the ask and sells at the bid
        const Bars b = { day(0, 100, 100, 100, 100), day(1, 100, 100, 100, 100), day(2, 100, 100, 100, 100), day(3, 100, 100, 100, 100) };
        const QJsonObject s = merged(noCosts, { { "spreadPct", 0.2 }, { "entryLong", always }, { "exitLong", always } });
        const QJsonValue t = backtest(b, s)["trades"][0];
        check(std::fabs(t["entryPrice"].toDouble() - 100.1) < 1e-9, "ask");
        check(std::fabs(t["exitPrice"].toDouble() - 99.9) < 1e-9, "bid");
        check(backtest(b, s).value("stats")["feesAndSlippage"].toString().contains(QRegularExpression("0.2% spread")), "spread text");
    }
    {   // a long's stop triggers on the bid
        const Bars b = { day(0, 100, 100, 100, 100), day(1, 100, 100, 98.15, 99), day(2, 99, 99, 99, 99) };
        const QJsonObject s = merged(noCosts, { { "stopLossPct", 2 }, { "entryLong", always } });
        check(backtest(b, s)["trades"][0]["exitReason"].toString() == "End of data", "mid doesn't stop");
        check(backtest(b, merged(s, { { "spreadPct", 0.2 } }))["trades"][0]["exitReason"].toString() == "Stop", "bid stops");
    }
    {   // fills inside a bar: cautious, TradingView path, and the bar magnifier
        const Bars b = { day(0, 100, 100, 100, 100), day(1, 100, 102.5, 97, 100), day(2, 100, 100, 100, 100) };
        const QJsonObject s = merged(noCosts, { { "stopLossPct", 2 }, { "takeProfitPct", 2 }, { "entryLong", always } });
        check(backtest(b, s)["trades"][0]["exitReason"].toString() == "Stop", "worst");
        check(backtest(b, merged(s, { { "intrabar", "ohlc" } }))["trades"][0]["exitReason"].toString() == "Target", "ohlc");
        const Bars sub = { mk(b[1].time, 100, 100, 97, 97.5), mk(b[1].time + 43200, 97.5, 102.5, 97.5, 100) };
        const QJsonObject m = backtest(b, merged(s, { { "intrabar", "magnifier" } }), &sub);
        check(m["trades"][0]["exitReason"].toString() == "Stop", "magnifier stop");
        check(m["trades"][0]["exitPrice"].toDouble() == 98, "magnifier price");
        check(m["stats"]["intrabarCovered"].toInt() == 1, "covered");
        check(m["stats"]["fills"].toString().contains("inside 1 of 3 bars, the rest on the OHLC path"), "fills text");
        const Bars up = { mk(b[1].time, 100, 102.5, 100, 101), mk(b[1].time + 43200, 101, 101, 97, 100) };
        check(backtest(b, merged(s, { { "intrabar", "magnifier" } }), &up)["trades"][0]["exitReason"].toString() == "Target", "magnifier target");
    }
    {   // a bar that opens past the stop fills at the open on every fill setting
        const Bars b = { day(0, 100, 100, 100, 100), day(1, 100, 100, 99, 99), day(2, 95, 96, 94, 95), day(3, 95, 95, 95, 95) };
        const QJsonObject s = merged(noCosts, { { "stopLossPct", 2 }, { "entryLong", always } });
        for (const QString &ib : INTRABAR) {
            const QJsonValue t = backtest(b, merged(s, { { "intrabar", ib } }))["trades"][0];
            check(t["exitReason"].toString() == "Stop", ib + " stop");
            check(t["exitPrice"].toDouble() == 95, ib + " at the open");
        }
    }
    {   // the magnifier picks the finest timeframe that covers the test in 20,000 bars
        check(magnifierTf(14400, 1000 * 14400).tf == "15m", "15m");
        check(magnifierTf(86400, 1000 * 86400).tf == "4h", "4h");
        check(magnifierTf(3600, 300 * 3600).tf == "1m", "1m");
        check(magnifierTf(3600, 500 * 3600).tf == "5m", "5m");
        check(magnifierTf(60, 1000 * 60).tf.isEmpty(), "none");
    }

    CAT = "quant.test.js";
    {   // normal distribution helpers match known values
        check(std::fabs(normCdf(0) - 0.5) < 1e-7, "normCdf(0)");
        check(std::fabs(normCdf(1.96) - 0.9750021) < 1e-6, "normCdf(1.96)");
        check(std::fabs(normInv(0.975) - 1.959964) < 1e-6, "normInv(0.975)");
        for (double p : { 0.001, 0.2, 0.5, 0.8, 0.999 }) check(std::fabs(normCdf(normInv(p)) - p) < 1e-6, "round trip");
    }
    auto seeded = [](double s) {
        return [s]() mutable { return (s = std::fmod(s * 16807, 2147483647)) / 2147483647 - 0.5; };
    };
    {   // PSR
        Values steady, noise;
        for (int i = 0; i < 500; ++i) steady << 0.002 + 0.001 * std::sin(i);
        check(*psr(steady) > 0.999, "steady PSR");
        auto r = seeded(42);
        for (int i = 0; i < 2000; ++i) noise << r() * 0.02;
        const double p = *psr(noise);
        check(p > 0.05 && p < 0.95, QStringLiteral("noise PSR %1").arg(p));
    }
    {   // expected best Sharpe grows with trials
        check(expectedMaxSharpe(100, 0.01) > expectedMaxSharpe(10, 0.01), "grows");
        check(expectedMaxSharpe(1, 0.01) == 0, "one trial");
    }
    {   // PBO
        const int T = 400;
        Values good;
        for (int i = 0; i < T; ++i) good << 0.01 + 0.002 * std::sin(i);
        QVector<Values> m{ good };
        for (int k = 0; k < 5; ++k) {
            Values o;
            for (int i = 0; i < T; ++i) o << 0.001 * std::sin(i * (k + 2));
            m << o;
        }
        check(pbo(m)["pbo"].toDouble() < 0.1, "better everywhere is not overfit");
        auto r = seeded(7);
        QVector<Values> noise;
        for (int k = 0; k < 12; ++k) {
            Values o;
            for (int i = 0; i < T; ++i) o << r();
            noise << o;
        }
        const double p = pbo(noise)["pbo"].toDouble();
        check(p > 0.2 && p < 0.8, QStringLiteral("noise PBO %1").arg(p));
        check(pbo({ good }).isNull(), "needs at least two variants");
    }
    {   // Monte Carlo is reproducible and ordered
        const Values trades = { 5, -2, 3, -1, 8, -4, 2, 1, -3, 6 };
        const QJsonObject a = monteCarlo(trades).toObject(), b = monteCarlo(trades).toObject();
        check(a == b, "reproducible");
        check(a["p5"].toDouble() <= a["p50"].toDouble() && a["p50"].toDouble() <= a["p95"].toDouble(), "ordered");
        check(a["lossProb"].toDouble() >= 0 && a["lossProb"].toDouble() <= 100, "lossProb range");
    }
    {   // quant checks run on a real strategy and report every gate
        const QJsonObject r = quantChecks(bars, example("EMA cross (9/21)"), 300);
        check(r["variants"].toArray().size() == 25, "two lengths x five settings");
        QStringList keys;
        for (const QJsonValue &g : r["gates"].toArray()) keys << g["key"].toString();
        keys.sort();
        check(keys == QStringList({ "cost", "dsr", "pbo", "robust", "wf" }), "gates");
        check(r["pbo"]["pbo"].toDouble() >= 0 && r["pbo"]["pbo"].toDouble() <= 1, "pbo range");
        check(r["pbo"]["splits"].toInt() == 70, "70 splits");
        check(r["walkForward"]["folds"].toArray().size() == 5, "5 folds");
        check(r["dsr"].toDouble() <= r["psr"].toDouble() + 1e-12, "deflating can only lower the probability");
    }
    {   // a strategy with no tunable lengths still gets the checks that apply
        const QJsonObject r = quantChecks(bars, QJsonObject{ { "indicators", QJsonObject() },
                                                             { "entryLong", rules(R"([{"left":"close","op":"crossesAbove","right":30000}])") },
                                                             { "exitLong", rules(R"([{"left":"close","op":"crossesBelow","right":30000}])") } },
                                          100);
        check(r["variants"].toArray().size() == 1, "one variant");
        bool cost = false;
        for (const QJsonValue &g : r["gates"].toArray()) cost |= g["key"].toString() == "cost";
        check(cost, "cost gate");
        check(r["pbo"].isNull(), "no pbo");
    }
    {   // Monte Carlo scales trades by position size; cost stress adds slippage when it is 0
        const QJsonObject strat = merged(example("EMA cross (9/21)"), { { "slippagePct", 0 } });
        const QJsonObject full = quantChecks(bars, merged(strat, { { "positionPct", 100 } }), 300);
        const QJsonObject part = quantChecks(bars, merged(strat, { { "positionPct", 20 } }), 300);
        check(std::fabs(part["monteCarlo"]["ddP50"].toDouble()) < std::fabs(full["monteCarlo"]["ddP50"].toDouble()) * 0.4,
              "20% sizing gives a much smaller drawdown");
        const QJsonObject nc = quantChecks(bars, merged(strat, { { "feePct", 0 } }), 100);
        check(nc["costStress"].toDouble() < nc["base"]["netPct"].toDouble(), "stress still costs something");
        QString v;
        for (const QJsonValue &g : nc["gates"].toArray())
            if (g["key"].toString() == "cost") v = g["value"].toString();
        check(v.contains("0.05% slippage"), "0.05% slippage: " + v);
    }
}

// NaN/Infinity survive in the JSON object and turn into null as text, as JSON.stringify does.
void testNonFinite()
{
    CAT = "non-finite";
    Bars same;
    for (int i = 0; i < 300; ++i) same.push_back(mk(1700000000, 100 + (i % 7), 101 + (i % 7), 99 + (i % 7), 100 + (i % 5)));
    const QJsonObject q = quantChecks(same, example("EMA cross (9/21)"), 50);
    check(q["base"]["sharpe"].isDouble(), "kept as a double");
    const QJsonDocument doc(q);
    const QJsonObject back = QJsonDocument::fromJson(doc.toJson()).object();
    check(back["sr0"].isNull() == !std::isfinite(q["sr0"].toDouble()), "non-finite -> null in text");
}

double timeIt(const std::function<void()> &f, int reps)
{
    f();
    QElapsedTimer t;
    t.start();
    for (int i = 0; i < reps; ++i) f();
    return double(t.nsecsElapsed()) / 1e6 / reps;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QFile f(QStringLiteral(QTM_REF));
    if (!f.open(QIODevice::ReadOnly)) {
        std::cout << "no reference (run node tests/quant/backtest/make_reference.mjs)\n";
        return 1;
    }
    REF = QJsonDocument::fromJson(f.readAll()).object();
    const QJsonObject sets = REF["sets"].toObject();
    for (auto it = sets.begin(); it != sets.end(); ++it) SETS.insert(it.key(), barsFrom(it.value().toObject()));

    QElapsedTimer total;
    total.start();
    testBacktests();
    testConditions();
    testMaths();
    testQuant();
    testJsSuite();
    testNonFinite();

    const QJsonObject ema = example("EMA cross (9/21)");
    const double ms = timeIt([&] { quantChecks(SETS["big"], ema); }, 5);
    const double btMs = timeIt([&] { backtest(SETS["big"], ema); }, 20);
    std::cout << "\nquantChecks on 3000 bars: " << ms << " ms (Node " << REF.value("timing")["quantChecks3000ms"].toDouble()
              << " ms); one backtest on 3000 bars: " << btMs << " ms\n";

    std::cout << "\nchecks by area:\n";
    for (auto it = S.checksBy.begin(); it != S.checksBy.end(); ++it)
        std::cout << "  " << it.key().toStdString() << ": " << it.value() << " checks, " << S.failsBy.value(it.key())
                  << " failed\n";
    std::cout << S.checks << " checks, " << S.fails << " failed (" << total.elapsed() << " ms)\n";
    return S.fails ? 1 : 0;
}
