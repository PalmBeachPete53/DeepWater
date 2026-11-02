#include "Ptq.h"

#include "../core/Indicators.h"
#include "Async.h"
#include "Http.h"
#include "Json.h"

#include <QDir>
#include <QFile>
#include <QRandomGenerator>
#include <QRegularExpression>

#include <cmath>

namespace qtm {

const QString Ptq::SITE = QStringLiteral(
    "https://www.part-timequant.com/?utm_source=quant-terminal&utm_medium=app&utm_campaign=ptq-connector");

Ptq::Ptq(const QString &url, const QString &home) : m_url(url), m_home(home)
{
    m_url.replace(QRegularExpression(QStringLiteral("/+$")), QString());
}

Ptq Ptq::fromEnv()
{
    const QString url = qEnvironmentVariable("PTQ_URL");
    return Ptq(url.isEmpty() ? QStringLiteral("http://127.0.0.1:8787") : url, qEnvironmentVariable("PTQ_HOME"));
}

// ---------- licence lease (written by `ptq activate` on this machine) ----------
Ptq::Lease Ptq::leaseInfo() const
{
    QStringList homes;
    if (!m_home.isEmpty()) homes << m_home;
    homes << QDir::home().filePath(".ptq") << QDir::home().filePath(".trading-os");
    for (const QString &home : homes) {
        QFile file(QDir(home).filePath("license.json"));
        if (!file.open(QIODevice::ReadOnly)) continue;
        bool ok = false;
        const QJsonValue f = jsv::parse(file.readAll(), &ok);
        if (!ok) continue;
        Lease l;
        l.lease = f["lease"].isString() ? f["lease"].toString() : QString();
        const double exp = jsv::toNumber(f["exp"]);
        l.exp = (exp != 0 && !std::isnan(exp)) ? QJsonValue(exp) : QJsonValue(QJsonValue::Null);
        l.activated = jsv::truthy(f["key"]);
        return l;
    }
    return { QString(), QJsonValue(QJsonValue::Null), false };
}

static PtqError fail(const QString &message, int status, const QString &code)
{
    return PtqError(message, status, code);
}

QJsonValue Ptq::call(const QByteArray &method, const QString &p, const QJsonValue &body, int timeoutMs) const
{
    const Lease lease = leaseInfo();
    http::FetchOptions o;
    o.method = method;
    if (!body.isUndefined()) {
        o.headers << qMakePair(QByteArray("content-type"), QByteArray("application/json"));
        o.body = jsv::stringify(body);
        o.hasBody = true;
    }
    if (!lease.lease.isEmpty()) o.headers << qMakePair(QByteArray("authorization"), ("Bearer " + lease.lease).toUtf8());
    o.timeoutMs = timeoutMs;
    http::FetchResponse res;
    try {
        res = http::fetch(m_url + "/api" + p, o);
    } catch (const http::FetchError &e) {
        if (e.timeout())
            throw fail("PTQ took too long to answer. It may still be working: check PTQ itself.", 504, "timeout");
        throw fail("PTQ isn't running on this computer. Start it with `ptq serve`, then try again.", 503, "not_running");
    }
    bool ok = false;
    QJsonValue j = jsv::parse(res.body, &ok);
    if (!ok) j = QJsonObject();
    const QJsonValue err = j["error"];
    const QString errText = jsv::truthy(err) ? jsv::toString(err) : QString();
    if (res.status == 401) {
        if (errText.contains("expired"))
            throw fail("Your PTQ licence lease has run out. Open PTQ or run `ptq status` to refresh it, then try again.", 401, "licence");
        throw fail("PTQ needs your licence on this computer. Run `ptq activate <your key>`, then try again.", 401, "licence");
    }
    static const QRegularExpression refused(QStringLiteral("refused|not found|unavailable"), QRegularExpression::CaseInsensitiveOption);
    if (res.status == 402 && !refused.match(errText).hasMatch())
        throw fail("PTQ couldn't charge this run: " + (errText.isEmpty() ? QStringLiteral("out of credits") : errText), 402, "credits");
    if (res.status == 402) {
        QString m = jsv::toString(err);
        m.replace(QRegularExpression(QStringLiteral("engine 422: .*"), QRegularExpression::DotMatchesEverythingOption),
                  "its engine can't run these settings.");
        throw fail(m, 400, "ptq");
    }
    if (!res.ok())
        throw fail(errText.isEmpty() ? QStringLiteral("PTQ answered HTTP %1").arg(res.status) : errText,
                   res.status >= 500 ? 502 : 400, "ptq");
    return j;
}

QJsonObject Ptq::status() const
{
    const Lease lic = leaseInfo();
    QJsonObject base {
        { "url", m_url },
        { "site", SITE },
        { "licensed", !lic.lease.isEmpty() },
        { "leaseExpires", lic.exp },
    };
    try {
        const QJsonValue h = call("GET", "/health", QJsonValue(QJsonValue::Undefined), 2500);
        base.insert("connected", true);
        if (!h["version"].isUndefined()) base.insert("version", h["version"]);
        if (!h["engine"].isUndefined()) base.insert("engine", h["engine"]);
    } catch (const Error &e) {
        base.insert("connected", false);
        base.insert("reason", e.message());
    }
    return base;
}

// ---------- strategy translation: tester JSON -> PTQ composer manifest ----------
static const QStringList PRICE = { "close", "open", "high", "low", "volume" };

static QString opFor(const QJsonValue &op)
{
    if (!op.isString()) return {};
    static const QHash<QString, QString> OPS = {
        { ">", ">" }, { "<", "<" }, { ">=", ">=" }, { "<=", "<=" }, { "crossesAbove", "crosses_above" }, { "crossesBelow", "crosses_below" },
    };
    return OPS.value(op.toString());
}

static const QString SUPPORTED = QStringLiteral(
    "SMA, EMA, RSI, ATR, ROC, standard deviation, Bollinger Bands, MACD, Donchian highs and lows, MA Cross and volume");

using Params = QVector<QPair<QString, QJsonValue>>;

static QString outOfBounds(const Params &params)
{
    static const QHash<QString, QPair<double, double>> BOUNDS = {
        { "length", { 2, 400 } }, { "fast", { 2, 200 } }, { "slow", { 3, 400 } }, { "signal", { 2, 100 } }, { "mult", { 0.1, 10 } },
    };
    for (const auto &[k, v] : params) {
        if (!BOUNDS.contains(k)) continue;
        const auto b = BOUNDS.value(k);
        const double x = jsv::toNumber(v);
        if (!(x >= b.first && x <= b.second))
            return QStringLiteral("%1 %2 is outside PTQ's range (%3 to %4)")
                .arg(k, jsv::toString(v), jsv::numberToString(b.first), jsv::numberToString(b.second));
    }
    return {};
}

// One operand -> { term } or { error }, plus optional notes.
PtqTerm Ptq::toTerm(const QJsonValue &operand, const QJsonObject &indicators)
{
    PtqTerm r;
    if (operand.isDouble()
        || (operand.isString() && !jsv::trim(operand.toString()).isEmpty() && !std::isnan(jsv::toNumber(operand)))) {
        r.term = { { "indicator", "constant" }, { "value", jsv::toNumber(operand) } };
        return r;
    }
    if (operand.isString() && PRICE.contains(operand.toString())) {
        r.term = { { "indicator", operand.toString() } };
        return r;
    }
    if (operand.isString() && QStringList { "hl2", "hlc3", "ohlc4", "hlcc4" }.contains(operand.toString())) {
        r.error = QStringLiteral("PTQ compares close, open, high, low or volume, not %1").arg(operand.toString());
        return r;
    }
    const QString text = jsv::toString(operand);
    const QStringList parts = text.split('.');
    const QString key = parts[0];
    const QString outRaw = parts.size() > 1 ? parts[1] : QString();
    const QJsonValue spec = indicators.value(key);
    const IndicatorDefPtr def = jsv::truthy(spec) ? indicatorById(jsv::toString(spec["id"])) : nullptr;
    if (!def) {
        r.error = QStringLiteral("\"%1\" isn't a known indicator").arg(text);
        return r;
    }
    QJsonObject p = indicatorDefaults(def->id);
    const QJsonObject sp = jsv::orElse(spec["params"], QJsonObject()).toObject();
    for (auto it = sp.begin(); it != sp.end(); ++it) p.insert(it.key(), it.value());
    QString out = outRaw;
    if (out.isEmpty()) out = jsv::truthy(spec["output"]) ? jsv::toString(spec["output"]) : QString();
    if (out.isEmpty() && !def->outputs.isEmpty()) out = def->outputs[0].key;
    if (jsv::truthy(p.value("source")) && p.value("source") != QJsonValue("close")) {
        r.error = QStringLiteral("%1 on %2: PTQ computes indicators on the close").arg(def->name, jsv::toString(p.value("source")));
        return r;
    }
    auto t = [&](const QString &indicator, const Params &params, const QStringList &notes = {}) {
        PtqTerm x;
        const QString bad = outOfBounds(params);
        if (!bad.isEmpty()) {
            x.error = def->name + ": " + bad;
            return x;
        }
        QJsonObject po;
        for (const auto &[k, v] : params)
            if (!v.isUndefined()) po.insert(k, v);
        x.term = { { "indicator", indicator }, { "params", po } };
        x.notes = notes;
        return x;
    };
    const QString id = def->id;
    if (id == "sma" || id == "ema" || id == "rsi" || id == "atr" || id == "roc" || id == "stdev")
        return t(id, { { "length", p.value("length") } });
    if (id == "bb") {
        if (out == "upper") return t("bb_upper", { { "length", p.value("length") }, { "mult", p.value("mult") } });
        if (out == "lower") return t("bb_lower", { { "length", p.value("length") }, { "mult", p.value("mult") } });
        return t("sma", { { "length", p.value("length") } });
    }
    if (id == "macd") {
        if (out == "signal")
            return t("macd_signal", { { "fast", p.value("fast") }, { "slow", p.value("slow") }, { "signal", p.value("signal") } });
        if (out == "hist")
            return t("macd_hist", { { "fast", p.value("fast") }, { "slow", p.value("slow") }, { "signal", p.value("signal") } });
        return t("macd", { { "fast", p.value("fast") }, { "slow", p.value("slow") } });
    }
    if (id == "donchian") {
        const QString note = "PTQ's channel uses the bars before the current one, so a breakout means beating the previous high or low.";
        if (out == "upper") return t("highest", { { "length", p.value("length") } }, { note });
        if (out == "lower") return t("lowest", { { "length", p.value("length") } }, { note });
        r.error = "Donchian middle line isn't in PTQ's strategy builder yet";
        return r;
    }
    if (id == "macross") {
        const QString kind = p.value("type") == QJsonValue("SMA") ? "sma" : "ema";
        return t(kind, { { "length", out == "slow" ? p.value("slow") : p.value("fast") } });
    }
    if (id == "volume") {
        if (out == "ma") return t("volume_sma", { { "length", jsv::orElse(p.value("maLength"), 20) } });
        r.term = { { "indicator", "volume" } };
        return r;
    }
    r.error = QStringLiteral("%1 isn't in PTQ's strategy builder yet (it takes %2)").arg(def->name, SUPPORTED);
    return r;
}

static QJsonValue group(const QJsonArray &rules, const QJsonObject &indicators, QStringList &problems, QStringList &notes,
                        const QString &label, const QString &op = "AND")
{
    QJsonArray conditions;
    for (int i = 0; i < rules.size(); ++i) {
        const QJsonValue r = rules[i];
        const QString cmp = opFor(r["op"]);
        if (cmp.isEmpty()) {
            problems << QStringLiteral("%1 condition %2: \"%3\" isn't one PTQ uses (it takes above, below, crosses above, crosses below)")
                            .arg(label).arg(i + 1).arg(jsv::toString(r["op"]));
            continue;
        }
        const PtqTerm L = Ptq::toTerm(r["left"], indicators), R = Ptq::toTerm(r["right"], indicators);
        for (const PtqTerm *x : { &L, &R }) {
            if (!x->error.isEmpty()) problems << QStringLiteral("%1 condition %2: %3").arg(label).arg(i + 1).arg(x->error);
            for (const auto &n : x->notes)
                if (!notes.contains(n)) notes << n;
        }
        if (L.error.isEmpty() && R.error.isEmpty())
            conditions.append(QJsonObject { { "kind", "comparison" }, { "left", L.term }, { "op", cmp }, { "right", R.term } });
    }
    if (conditions.isEmpty()) return QJsonValue(QJsonValue::Null);
    return QJsonObject { { "kind", "group" }, { "op", op }, { "conditions", conditions } };
}

PtqTranslation Ptq::translate(const QJsonObject &strategy, const QJsonValue &tf, const QJsonValue &name)
{
    QJsonObject s { { "entryLong", QJsonArray() }, { "exitLong", QJsonArray() }, { "entryShort", QJsonArray() },
                    { "exitShort", QJsonArray() }, { "indicators", QJsonObject() } };
    for (auto it = strategy.begin(); it != strategy.end(); ++it) s.insert(it.key(), it.value());
    PtqTranslation out;
    QStringList &problems = out.problems, &notes = out.notes;
    const QJsonArray entryLong = s.value("entryLong").toArray(), entryShort = s.value("entryShort").toArray();
    QString direction = "long";
    QJsonArray entryRules = entryLong, exitRules = s.value("exitLong").toArray();
    if (entryLong.isEmpty() && !entryShort.isEmpty()) {
        direction = "short";
        entryRules = entryShort;
        exitRules = s.value("exitShort").toArray();
    }
    if (!entryLong.isEmpty() && !entryShort.isEmpty())
        notes << "PTQ runs one direction per strategy, so this sends the long side.";
    if (entryRules.isEmpty()) problems << "Add at least one entry condition.";
    static const QHash<QString, QString> TF = { { "1m", "1m" }, { "5m", "5m" }, { "15m", "15m" }, { "1h", "1h" }, { "4h", "4h" }, { "1D", "1d" } };
    const QString tfs = tf.isString() ? tf.toString() : QString();
    const QString timeframe = TF.value(tfs);
    if (timeframe.isEmpty()) problems << QStringLiteral("PTQ tests on 1m, 5m, 15m, 1h, 4h or 1D, not %1.").arg(jsv::toString(tf));
    const QJsonObject indicators = s.value("indicators").toObject();
    const QJsonValue entry = group(entryRules, indicators, problems, notes, "Entry");
    const QJsonValue exit = group(exitRules, indicators, problems, notes, "Exit", s.value("exitMatch") == QJsonValue("any") ? "OR" : "AND");
    // PTQ caps a trade by hours, not bars; round up so it never exits earlier than the test did here.
    static const QHash<QString, double> HOURS = { { "1m", 1.0 / 60 }, { "5m", 1.0 / 12 }, { "15m", 0.25 }, { "1h", 1 }, { "4h", 4 }, { "1D", 24 } };
    const double hoursPerBar = HOURS.value(tfs, 0);
    const double maxBars = jsv::toNumber(s.value("maxBarsInTrade"));
    const double maxHoldingHours = maxBars > 0 && hoursPerBar ? std::ceil(maxBars * hoursPerBar) : 0;
    if (maxHoldingHours && maxHoldingHours != maxBars * hoursPerBar)
        notes << QStringLiteral("PTQ holds by whole hours, so the %1-bar time exit is sent as %2 hours.")
                     .arg(jsv::toString(s.value("maxBarsInTrade")), jsv::numberToString(maxHoldingHours));
    if (jsv::truthy(s.value("trailingStopPct")))
        notes << "PTQ sizes and stops each trade by its own risk settings; the trailing stop isn't sent.";
    if (jsv::truthy(s.value("stopLossPct")))
        notes << "PTQ stops each trade at 2 ATR under its own risk settings; the stop-loss % isn't sent.";
    if (jsv::truthy(s.value("spreadPct"))) notes << "PTQ applies its own trading costs; the spread isn't sent.";
    if (jsv::truthy(s.value("takeProfitPct")))
        notes << "PTQ sets the take-profit from its own risk settings; the take-profit % isn't sent.";
    QJsonObject composed { { "entry", entry } };
    if (!exit.isNull()) composed.insert("exit", exit);
    const quint64 r1 = QRandomGenerator::system()->generate64();
    const QString hex = QStringLiteral("%1").arg(r1 & 0xFFFFFFFFFFULL, 10, 16, QChar('0'));
    QJsonObject manifest {
        { "id", "strat_qt_" + hex },
        { "name", QStringLiteral("Quant Terminal: %1").arg(jsv::truthy(name) ? jsv::toString(name) : QStringLiteral("strategy")).left(80) },
        { "version", "1.0.0" },
        { "status", "research" },
        { "assetClass", QJsonValue(QJsonValue::Null) },
        { "instrument", QJsonValue(QJsonValue::Null) },
        { "direction", direction },
        { "entryLogic", "composed" },
        { "creationMode", "composer" },
        { "composed", composed },
        { "session", QJsonObject { { "enabled", false }, { "label", "24/7" } } },
        { "newsGuard", false },
        { "maxHoldingHours", maxHoldingHours },
        { "risk", QJsonObject { { "riskPerTradePct", 0.5 }, { "stopAtr", 2 }, { "maxTradesPerDay", 3 }, { "requireBracket", true } } },
        { "validation", QJsonObject { { "walkForward", true }, { "transactionCosts", true }, { "overfittingCheck", true }, { "minOutOfSampleMonths", 12 } } },
        { "createdAt", 0 },
        { "updatedAt", 0 },
    };
    if (!timeframe.isEmpty()) manifest.insert("signalTimeframe", timeframe);
    out.manifest = manifest;
    return out;
}

// Terminal symbol -> PTQ market id, checked against PTQ's own catalogue.
QJsonObject Ptq::resolveInstrument(const QString &symbolId) const
{
    const QStringList parts = symbolId.split(':');
    const QString ex = parts[0];
    const QString ticker = parts.size() > 1 ? parts[1] : QString();
    QString assetClass, candidate, q;
    if (QStringList { "BINANCE", "COINBASE", "KRAKEN" }.contains(ex)) {
        QString base = ticker;
        base.remove('-');
        base.replace(QRegularExpression(QStringLiteral("(USDT|USDC|FDUSD|USD)$")), QString());
        base.replace(QRegularExpression(QStringLiteral("^XBT$")), "BTC");
        assetClass = "crypto";
        q = base + "USDT";
        candidate = "BITGET:SPOT:" + base + "USDT";
    } else if (ex == "US") {
        assetClass = "equities";
        q = ticker;
        candidate = "YAHOO:STOCKS:" + ticker;
    } else if (ex == "FX") {
        assetClass = "fx";
        q = ticker;
        candidate = "YAHOO:FX:" + ticker + "=X";
    } else {
        throw fail(QStringLiteral("PTQ can't test %1 from here. Use a crypto pair, a US stock or a forex pair.").arg(symbolId), 422, "instrument");
    }
    const QJsonValue found = call("GET", QStringLiteral("/markets?q=%1&assetClass=%2&limit=50").arg(jsv::encodeURIComponent(q), assetClass));
    QJsonValue item;
    for (const QJsonValue x : jsv::orElse(found["items"], QJsonArray()).toArray())
        if (x["id"] == QJsonValue(candidate)) {
            item = x;
            break;
        }
    if (!jsv::truthy(item))
        throw fail(QStringLiteral("PTQ's market list has no %1, so it can't test this symbol.").arg(candidate), 422, "instrument");
    const QJsonValue history = jsv::orElse(item["history"], QJsonObject());
    if (history["status"] != QJsonValue("available"))
        throw fail(QStringLiteral("PTQ has no price history for %1 (%2).")
                       .arg(candidate, jsv::truthy(history["reason"]) ? jsv::toString(history["reason"]) : QStringLiteral("unavailable")),
                   422, "instrument");
    return { { "id", candidate }, { "assetClass", assetClass } };
}

// ---------- the actions the terminal offers ----------
QJsonObject Ptq::check(const QJsonObject &body) const
{
    PtqTranslation tr = translate(body.value("strategy").toObject(), body.value("tf"), body.value("name"));
    if (!tr.problems.isEmpty())
        throw PtqError("PTQ can't test this strategy as written.", 422, "translate", tr.problems, tr.notes);
    const QJsonObject inst = resolveInstrument(jsv::toString(body.value("symbol")));
    tr.manifest.insert("instrument", inst.value("id"));
    tr.manifest.insert("assetClass", inst.value("assetClass"));
    const QJsonValue saved = call("POST", "/strategies", tr.manifest);
    const QJsonValue run = call("POST", "/backtest/run", QJsonObject { { "strategyId", saved["id"] } }, 240000);
    QJsonValue validation(QJsonValue::Null), validationError(QJsonValue::Null);
    try {
        const QJsonValue v = call("POST", QStringLiteral("/backtest/%1/validate").arg(jsv::toString(run["id"])),
                                  QJsonValue(QJsonValue::Undefined), 240000);
        QJsonArray gates;
        for (const QJsonValue g : jsv::orElse(v["gates"], QJsonArray()).toArray()) {
            QJsonObject o;
            for (const char *k : { "key", "label", "value", "pass" })
                if (!g[k].isUndefined()) o.insert(k, g[k]);
            gates.append(o);
        }
        QJsonObject vo { { "gates", gates } };
        for (const char *k : { "verdict", "pboPct", "deflatedSharpe" })
            if (!v[k].isUndefined()) vo.insert(k, v[k]);
        validation = jsv::truthy(v) ? QJsonValue(vo) : v;
    } catch (const Error &e) {
        validationError = e.message();
    }
    QJsonObject out {
        { "instrument", inst.value("id") },
        { "metrics", jsv::orElse(run["metrics"], QJsonObject()) },
        { "validation", validation },
        { "validationError", validationError },
        { "notes", QJsonArray::fromStringList(tr.notes) },
        { "links", QJsonObject {
              { "strategy", QStringLiteral("%1/strategy?strategyId=%2").arg(m_url, jsv::encodeURIComponent(jsv::toString(saved["id"]))) },
              { "autopilot", m_url + "/autopilot" },
              { "integrations", m_url + "/integrations" },
          } },
    };
    if (!saved["id"].isUndefined()) out.insert("strategyId", saved["id"]);
    if (!saved["name"].isUndefined()) out.insert("strategyName", saved["name"]);
    if (tr.manifest.contains("signalTimeframe")) out.insert("timeframe", tr.manifest.value("signalTimeframe"));
    if (!run["provenance"].isUndefined()) out.insert("provenance", run["provenance"]);
    return out;
}

QJsonObject Ptq::overview() const
{
    const QStringList paths = { "/portfolio", "/execution/strategy-arms", "/autopilot", "/execution/kill-switch", "/journal" };
    const auto got = async::all(int(paths.size()), [&](int i) -> QJsonValue {
        try {
            return call("GET", paths[i]);
        } catch (const PtqError &e) {
            return QJsonObject { { "error", e.message() }, { "code", e.code() } };
        } catch (const Error &e) {
            return QJsonObject { { "error", e.message() } };
        }
    });
    QJsonValue journal = got[4];
    if (journal.isArray()) {
        QJsonArray a;
        for (const QJsonValue x : journal.toArray())
            if (a.size() < 50) a.append(x);
        journal = a;
    }
    return {
        { "portfolio", got[0] },
        { "arms", got[1] },
        { "autopilot", got[2] },
        { "killSwitch", got[3] },
        { "journal", journal },
        { "links", QJsonObject { { "app", m_url }, { "autopilot", m_url + "/autopilot" }, { "integrations", m_url + "/integrations" },
                                 { "journal", m_url + "/journal" } } },
    };
}

// Only ever reduces what PTQ does: engage the stop, or disarm one strategy.
QJsonValue Ptq::engageKillSwitch() const
{
    return call("POST", "/execution/kill-switch", QJsonObject { { "engaged", true } });
}

QJsonValue Ptq::disarm(const QString &id) const
{
    return call("POST", QStringLiteral("/execution/strategy-arms/%1/disarm").arg(jsv::encodeURIComponent(id)), QJsonObject());
}

} // namespace qtm
