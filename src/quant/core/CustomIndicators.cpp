// Custom indicators: JS files in the custom/ folder, run unchanged on a QJSEngine.
// Port of how quant-terminal loads them (server.js loadCustom, app.js importCustom):
//   registerIndicator({ ...(await import(file)).default, id: `custom:${name}` })
// with the format checks of indicators.js indicatorProblems done on the JS object itself, and a
// `ta` object exposing the same helpers as the JS one, backed by the C++ port.
//
// QJSEngine objects belong to one thread, and compute() runs on the UI thread and on backend
// workers, so every thread keeps its own engine per file, imported lazily and re-imported when the
// file's modification time changes.
#include "CustomIndicators.h"

#include "CustomIndicatorsJs.h"
#include "Error.h"
#include "IndicatorsInternal.h"
#include "JsTransform.h"
#include "PineIndicators.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QUrl>
#include <QJSEngine>
#include <QJSValue>
#include <QJsonArray>
#include <QRegularExpression>

#include <memory>

#if defined(__clang__)
#pragma clang fp contract(off)
#endif

namespace qtm {

namespace {

// ---------------------------------------------------------------- JS glue
// The built-ins V4 lacks and the helpers of rewritten code: CustomIndicatorsJs.cpp.
using detail::jsPolyfills;
using detail::jsRuntime;

// Calls fn with `self` as this, catching what it throws: { ok, value } or { ok: false, error }.
// (QJSValue::call hands back a thrown value without saying it was thrown.)
const char *TRY_CALL = R"JS(
(function (fn, self, args) {
  try { return { ok: true, value: fn.apply(self, args) }; } catch (e) { return { ok: false, error: e }; }
})
)JS";

// registerIndicator({ ...mod.default, id }) fills these in before the checks.
const char *REGISTER_DEFAULTS = R"JS(
(function (d, id) {
  var def = Object.assign({}, d, { id: id });
  return Object.assign({ group: "Custom", overlay: true, inputs: [],
    outputs: [{ key: "value", label: def.name || def.id, kind: "line", color: "#ff9800" }] }, def);
})
)JS";

// The `ta` object handed to calc(): the same keys as indicators.js `ta`, forwarding to C++.
const char *TA_OBJECT = R"JS(
(function (B) {
  return {
    sma: function (src, n) { return B.sma(src, n); },
    ema: function (src, n) { return B.ema(src, n); },
    rma: function (src, n) { return B.rma(src, n); },
    wma: function (src, n) { return B.wma(src, n); },
    vwma: function (src, vol, n) { return B.vwma(src, vol, n); },
    hma: function (src, n) { return B.hma(src, n); },
    dema: function (src, n) { return B.dema(src, n); },
    tema: function (src, n) { return B.tema(src, n); },
    alma: function (src, n, offset, sigma) { return B.alma(src, n, offset, sigma); },
    linreg: function (src, n, offset) { return B.linreg(src, n, offset); },
    stdev: function (src, n) { return B.stdev(src, n); },
    dev: function (src, n) { return B.dev(src, n); },
    highest: function (src, n) { return B.highest(src, n); },
    lowest: function (src, n) { return B.lowest(src, n); },
    change: function (src, n) { return B.change(src, n); },
    sum: function (src, n) { return B.sum(src, n); },
    rsi: function (src, n) { return B.rsi(src, n); },
    atr: function (bars, n) { return B.atr(bars, n); },
    tr: function (bars) { return B.tr(bars); },
    crossover: function (a, b) { return B.crossover(a, b); },
    crossunder: function (a, b) { return B.crossunder(a, b); },
    compute: function (id, bars, params) { return B.compute(id, bars, params); },
    source: function (bars, name) { return B.source(bars, name); },
    macd: function (s, fast, slow, signal) { return B.macd(s, fast, slow, signal); },
    supertrend: function (bars, factor, len) { return B.supertrend(bars, factor, len); },
    psar: function (bars, start, inc, max) { return B.psar(bars, start, inc, max); },
    dmi: function (bars, diLen, adxLen) { return B.dmi(bars, diLen, adxLen); }
  };
})
)JS";

// ---------------------------------------------------------------- V4 -> Node error texts
// The import and runtime errors come from Qt's engine, not V8. Reword the common ones the way
// Node words them, so the message the app shows reads the same.
QString nodeMessage(QString msg)
{
    // "Importing module failed:\n    file:///x.js:3:55: Unexpected token `;'\n ..." -> "Unexpected token `;'"
    if (msg.startsWith(QLatin1String("Importing module failed:"))) {
        const QStringList lines = msg.split(QLatin1Char('\n'));
        if (lines.size() > 1) {
            static const QRegularExpression loc(QStringLiteral("^\\s*\\S*:\\d+:\\d+: "));
            msg = lines[1];
            msg.remove(loc);
            msg = msg.trimmed();
        }
    }
    // V4 quotes tokens as `x', V8 as 'x'.
    static const QRegularExpression tick(QStringLiteral("`([^`']*)'"));
    msg.replace(tick, QStringLiteral("'\\1'"));
    static const QRegularExpression readProp(QStringLiteral("^Cannot (?:read property|call method) '([^']*)' of (undefined|null)$"));
    if (auto m = readProp.match(msg); m.hasMatch())
        return QStringLiteral("Cannot read properties of %1 (reading '%2')").arg(m.captured(2), m.captured(1));
    static const QRegularExpression notFn(QStringLiteral("^Property '([^']*)' of object .* is not a function$"));
    if (auto m = notFn.match(msg); m.hasMatch()) return QStringLiteral("%1 is not a function").arg(m.captured(1));
    return msg;
}

QString errorMessage(const QJSValue &e)
{
    // `${e.message}`: an Error's message; undefined for a thrown string or number.
    return e.property(QStringLiteral("message")).toString();
}

// ---------------------------------------------------------------- per-thread engines

struct Module;
QString typeOf(const QJSValue &v)
{
    if (v.isUndefined()) return QStringLiteral("undefined");
    if (v.isNull()) return QStringLiteral("object");
    if (v.isBool()) return QStringLiteral("boolean");
    if (v.isNumber()) return QStringLiteral("number");
    if (v.isString()) return QStringLiteral("string");
    if (v.isCallable()) return QStringLiteral("function");
    return QStringLiteral("object");
}

// The bars handed to calc() as JS objects, so ta.source(bars) and friends can skip converting
// them back (strict equality with the array we built).
struct BarsFrame {
    QJSEngine *engine;
    QJSValue js;
    const Bars *bars;
};
thread_local QVector<BarsFrame> tlsBarsFrames;
struct BarsFrameScope {
    BarsFrameScope(QJSEngine *engine, const QJSValue &js, const Bars *bars) { tlsBarsFrames.push_back({ engine, js, bars }); }
    ~BarsFrameScope() { tlsBarsFrames.pop_back(); }
};

class TaBridge : public QObject
{
    Q_OBJECT
public:
    TaBridge(QJSEngine *engine, QJSValue keys) : e(engine), m_keys(std::move(keys)) { }

    QJSEngine *e;

    // ---- conversions
    static double num(const QJSValue &v) { return v.isNumber() ? v.toNumber() : NaN; }
    static int len(const QJSValue &v, int def)
    {
        return v.isUndefined() ? def : detail::toInt(v.toNumber());
    }
    static double dbl(const QJSValue &v, double def) { return v.isUndefined() ? def : v.toNumber(); }

    // A JS array (or array-like) of numbers; anything that isn't a number is NaN, which is how the JS
    // helpers treat it (they test isNum).
    static Values values(const QJSValue &v)
    {
        Values out;
        if (!v.isObject()) return out;
        const int n = detail::toInt(v.property(QStringLiteral("length")).toNumber());
        out.resize(std::max(0, n));
        for (int i = 0; i < out.size(); ++i) out[i] = num(v.property(quint32(i)));
        return out;
    }
    Bars bars(const QJSValue &v) const
    {
        for (int k = tlsBarsFrames.size() - 1; k >= 0; --k)
            if (tlsBarsFrames[k].engine == e && tlsBarsFrames[k].js.strictlyEquals(v)) return *tlsBarsFrames[k].bars;
        Bars out;
        if (!v.isObject()) return out;
        const int n = detail::toInt(v.property(QStringLiteral("length")).toNumber());
        out.reserve(std::max(0, n));
        static const QString T("time"), O("open"), H("high"), L("low"), C("close"), V("volume");
        for (int i = 0; i < n; ++i) {
            const QJSValue b = v.property(quint32(i));
            Bar bar;
            bar.time = b.property(T).toNumber();
            bar.open = b.property(O).toNumber();
            bar.high = b.property(H).toNumber();
            bar.low = b.property(L).toNumber();
            bar.close = b.property(C).toNumber();
            const QJSValue vol = b.property(V);
            bar.volume = (vol.isUndefined() || vol.isNull()) ? 0 : vol.toNumber();
            out.push_back(bar);
        }
        return out;
    }
    QJSValue js(const Values &v) const
    {
        QJSValue a = e->newArray(uint(v.size()));
        for (int i = 0; i < v.size(); ++i) a.setProperty(quint32(i), v[i]);
        return a;
    }
    QJSValue js(const QVector<bool> &v) const
    {
        QJSValue a = e->newArray(uint(v.size()));
        for (int i = 0; i < v.size(); ++i) a.setProperty(quint32(i), bool(v[i]));
        return a;
    }
    QJSValue js(const QStringList &v) const
    {
        QJSValue a = e->newArray(uint(v.size()));
        for (int i = 0; i < v.size(); ++i) a.setProperty(quint32(i), v[i]);
        return a;
    }
    QJSValue js(const QJsonValue &v) const
    {
        switch (v.type()) {
        case QJsonValue::Null: return QJSValue(QJSValue::NullValue);
        case QJsonValue::Bool: return QJSValue(v.toBool());
        case QJsonValue::Double: return QJSValue(v.toDouble());
        case QJsonValue::String: return QJSValue(v.toString());
        case QJsonValue::Array: {
            const QJsonArray arr = v.toArray();
            QJSValue a = e->newArray(uint(arr.size()));
            for (int i = 0; i < arr.size(); ++i) a.setProperty(quint32(i), js(arr[i]));
            return a;
        }
        case QJsonValue::Object: {
            const QJsonObject o = v.toObject();
            QJSValue r = e->newObject();
            for (auto it = o.begin(); it != o.end(); ++it) r.setProperty(it.key(), js(it.value()));
            return r;
        }
        default: return QJSValue(QJSValue::UndefinedValue);
        }
    }
    QJsonValue json(const QJSValue &v, int depth = 0) const
    {
        if (depth > 64) return QJsonValue(QJsonValue::Null);
        if (v.isNull()) return QJsonValue(QJsonValue::Null);
        if (v.isBool()) return v.toBool();
        if (v.isNumber()) return v.toNumber();
        if (v.isString()) return v.toString();
        if (v.isUndefined() || v.isCallable()) return QJsonValue(QJsonValue::Undefined);
        if (v.isArray()) {
            QJsonArray a;
            const int n = detail::toInt(v.property(QStringLiteral("length")).toNumber());
            for (int i = 0; i < n; ++i) {
                const QJsonValue x = json(v.property(quint32(i)), depth + 1);
                a.append(x.isUndefined() ? QJsonValue(QJsonValue::Null) : x);   // JSON.stringify writes null
            }
            return a;
        }
        if (v.isObject()) {
            QJsonObject o;
            const QJSValue keys = m_keys.call({ v });
            const int n = keys.property(QStringLiteral("length")).toInt();
            for (int i = 0; i < n; ++i) {
                const QString k = keys.property(quint32(i)).toString();
                const QJsonValue x = json(v.property(k), depth + 1);
                if (!x.isUndefined()) o.insert(k, x);
            }
            return o;
        }
        return QJsonValue(QJsonValue::Undefined);
    }
    QStringList keys(const QJSValue &o) const
    {
        QStringList out;
        if (!o.isObject()) return out;
        const QJSValue k = m_keys.call({ o });
        const int n = k.property(QStringLiteral("length")).toInt();
        for (int i = 0; i < n; ++i) out << k.property(quint32(i)).toString();
        return out;
    }
    QJSValue jsMarker(const IndicatorMarker &m) const { return js(QJsonValue(detail::markerToJson(m))); }
    // The object ta.compute(id, ...) returns. Its keys come in the order the JS calc() creates them
    // (spread, Object.keys and for-in show it): RESULT_ORDER for the built-ins whose order is not
    // their outputs', the outputs' order otherwise, then any other key.
    QJSValue jsResult(const IndicatorResult &r, const QString &id) const
    {
        static const QHash<QString, QStringList> RESULT_ORDER {
    { "macross", { "fast", "slow", "markers" } },
    { "vwap", { "value", "breaks" } },
    { "twap", { "value", "breaks" } },
    { "bb", { "basis", "upper", "lower" } },
    { "bbtrend", { "value", "colors" } },
    { "kc", { "basis", "upper", "lower" } },
    { "donchian", { "upper", "lower", "basis" } },
    { "envelope", { "basis", "upper", "lower" } },
    { "macd", { "macd", "signal", "hist", "colors" } },
    { "ao", { "value", "colors" } },
    { "smiergodic", { "smi", "signal", "osc" } },
    { "dmi", { "plus", "minus", "adx" } },
    { "chopzone", { "value", "colors" } },
    { "supertrend", { "up", "down", "direction" } },
    { "cks", { "stopShort", "stopLong" } },
    { "ichimoku", { "conversion", "base", "lagging", "spanA", "spanB", "extendBars" } },
    { "alligator", { "jaw", "teeth", "lips", "extendBars" } },
    { "pivots", { "p", "r1", "s1", "r2", "s2", "r3", "s3", "breaks" } },
    { "pdhl", { "high", "low", "breaks" } },
    { "pivotshl", { "markers" } },
    { "volume", { "value", "colors", "ma" } },
    { "netvol", { "value", "colors" } },
    { "markov", { "signal", "pbull", "pbear", "state", "colors" } },
    { "vpvr", { "volumeProfile" } },
    { "svp", { "sessionProfile" } },
    { "patterns", { "markers" } },
    { "fractals", { "markers" } },
        };
        QStringList order = RESULT_ORDER.value(id);
        if (order.isEmpty())
            if (const IndicatorDefPtr d = indicatorById(id))
                for (const IndicatorOutput &out : d->outputs) order << out.key;
        // colors and breaks of the built-ins that always return them, even empty (no bars).
        QStringList rest = r.values.keys();
        if (r.hasMarkers) rest << QStringLiteral("markers");
        if (!r.colors.isEmpty() || order.contains(QLatin1String("colors"))) rest << QStringLiteral("colors");
        if (!r.breaks.isEmpty() || order.contains(QLatin1String("breaks"))) rest << QStringLiteral("breaks");
        rest << r.extra.keys();
        std::sort(rest.begin(), rest.end());
        QStringList keys;
        for (const QString &k : order)
            if (rest.contains(k)) keys << k;
        for (const QString &k : rest)
            if (!keys.contains(k)) keys << k;
        QJSValue o = e->newObject();
        for (const QString &k : keys) {
            if (r.values.contains(k)) {
                o.setProperty(k, js(r.values.value(k)));
            } else if (k == QLatin1String("markers") && r.hasMarkers) {
                QJSValue a = e->newArray(uint(r.markers.size()));
                for (int i = 0; i < r.markers.size(); ++i) a.setProperty(quint32(i), jsMarker(r.markers[i]));
                o.setProperty(k, a);
            } else if (k == QLatin1String("colors") && !r.extra.contains(k)) {
                o.setProperty(k, js(r.colors));
            } else if (k == QLatin1String("breaks") && !r.extra.contains(k)) {
                o.setProperty(k, js(r.breaks));
            } else {
                o.setProperty(k, js(r.extra.value(k)));
            }
        }
        return o;
    }
    // What calc() returned, as the C++ result: number arrays, markers, colors, breaks, the rest as JSON.
    IndicatorResult result(const QJSValue &res) const
    {
        IndicatorResult r;
        if (!res.isObject() || res.isCallable()) return r;
        for (const QString &k : keys(res)) {
            const QJSValue v = res.property(k);
            if (k == QLatin1String("markers") && v.isArray()) {
                r.hasMarkers = true;
                const int n = v.property(QStringLiteral("length")).toInt();
                for (int i = 0; i < n; ++i) {
                    const QJSValue m = v.property(quint32(i));
                    IndicatorMarker mk;
                    auto str = [&](const char *name) {
                        const QJSValue s = m.property(QLatin1String(name));
                        return (s.isUndefined() || s.isNull()) ? QString() : s.toString();
                    };
                    mk.time = m.property(QStringLiteral("time")).toNumber();
                    mk.position = str("position");
                    mk.shape = str("shape");
                    mk.color = str("color");
                    mk.text = str("text");
                    r.markers.push_back(mk);
                }
                continue;
            }
            if (k == QLatin1String("colors") && v.isArray()) {
                const int n = v.property(QStringLiteral("length")).toInt();
                for (int i = 0; i < n; ++i) {
                    const QJSValue c = v.property(quint32(i));
                    r.colors << ((c.isUndefined() || c.isNull()) ? QString() : c.toString());
                }
                continue;
            }
            if (k == QLatin1String("breaks") && v.isArray()) {
                const int n = v.property(QStringLiteral("length")).toInt();
                for (int i = 0; i < n; ++i) r.breaks << v.property(quint32(i)).toBool();
                continue;
            }
            if (v.isArray()) {
                // Numbers stay numbers, null/undefined become NaN, booleans 1/0 (what JS comparisons see).
                const int n = v.property(QStringLiteral("length")).toInt();
                Values vals(n);
                bool numeric = true;
                for (int i = 0; i < n && numeric; ++i) {
                    const QJSValue x = v.property(quint32(i));
                    if (x.isNumber()) vals[i] = x.toNumber();
                    else if (x.isBool()) vals[i] = x.toBool() ? 1 : 0;
                    else if (x.isUndefined() || x.isNull()) vals[i] = NaN;
                    else numeric = false;
                }
                if (numeric) {
                    r.values.insert(k, vals);
                    continue;
                }
            }
            const QJsonValue j = json(v);
            if (!j.isUndefined()) r.extra.insert(k, j);
        }
        return r;
    }

    // ---- the ta helpers (indicators.js `ta`)
    Q_INVOKABLE QJSValue sma(const QJSValue &s, const QJSValue &n) { return js(ta::sma(values(s), len(n, 0))); }
    Q_INVOKABLE QJSValue ema(const QJSValue &s, const QJSValue &n) { return js(ta::ema(values(s), len(n, 0))); }
    Q_INVOKABLE QJSValue rma(const QJSValue &s, const QJSValue &n) { return js(ta::rma(values(s), len(n, 0))); }
    Q_INVOKABLE QJSValue wma(const QJSValue &s, const QJSValue &n) { return js(ta::wma(values(s), len(n, 0))); }
    Q_INVOKABLE QJSValue vwma(const QJSValue &s, const QJSValue &vol, const QJSValue &n)
    {
        return js(ta::vwma(values(s), values(vol), len(n, 0)));
    }
    Q_INVOKABLE QJSValue hma(const QJSValue &s, const QJSValue &n) { return js(ta::hma(values(s), len(n, 0))); }
    Q_INVOKABLE QJSValue dema(const QJSValue &s, const QJSValue &n) { return js(ta::dema(values(s), len(n, 0))); }
    Q_INVOKABLE QJSValue tema(const QJSValue &s, const QJSValue &n) { return js(ta::tema(values(s), len(n, 0))); }
    Q_INVOKABLE QJSValue alma(const QJSValue &s, const QJSValue &n, const QJSValue &offset, const QJSValue &sigma)
    {
        return js(ta::alma(values(s), len(n, 0), dbl(offset, 0.85), dbl(sigma, 6)));
    }
    Q_INVOKABLE QJSValue linreg(const QJSValue &s, const QJSValue &n, const QJSValue &offset)
    {
        return js(ta::linreg(values(s), len(n, 0), len(offset, 0)));
    }
    Q_INVOKABLE QJSValue stdev(const QJSValue &s, const QJSValue &n) { return js(ta::stdev(values(s), len(n, 0))); }
    Q_INVOKABLE QJSValue dev(const QJSValue &s, const QJSValue &n) { return js(ta::dev(values(s), len(n, 0))); }
    Q_INVOKABLE QJSValue highest(const QJSValue &s, const QJSValue &n) { return js(ta::highest(values(s), len(n, 0))); }
    Q_INVOKABLE QJSValue lowest(const QJSValue &s, const QJSValue &n) { return js(ta::lowest(values(s), len(n, 0))); }
    Q_INVOKABLE QJSValue change(const QJSValue &s, const QJSValue &n) { return js(ta::change(values(s), len(n, 1))); }
    Q_INVOKABLE QJSValue sum(const QJSValue &s, const QJSValue &n) { return js(ta::sum(values(s), len(n, 0))); }
    Q_INVOKABLE QJSValue rsi(const QJSValue &s, const QJSValue &n) { return js(ta::rsi(values(s), len(n, 14))); }
    Q_INVOKABLE QJSValue atr(const QJSValue &b, const QJSValue &n) { return js(ta::atr(bars(b), len(n, 14))); }
    Q_INVOKABLE QJSValue tr(const QJSValue &b) { return js(ta::tr(bars(b))); }
    Q_INVOKABLE QJSValue crossover(const QJSValue &a, const QJSValue &b)
    {
        return b.isArray() ? js(ta::crossover(values(a), values(b))) : js(ta::crossover(values(a), b.toNumber()));
    }
    Q_INVOKABLE QJSValue crossunder(const QJSValue &a, const QJSValue &b)
    {
        return b.isArray() ? js(ta::crossunder(values(a), values(b))) : js(ta::crossunder(values(a), b.toNumber()));
    }
    Q_INVOKABLE QJSValue compute(const QJSValue &id, const QJSValue &b, const QJSValue &params)
    {
        try {
            const QJsonValue p = json(params);
            return jsResult(qtm::compute(id.toString(), bars(b), p.toObject()), id.toString());
        } catch (const Error &err) {
            return throwError(err.message());
        } catch (const std::exception &err) {
            return throwError(QString::fromUtf8(err.what()));
        }
    }
    Q_INVOKABLE QJSValue source(const QJSValue &b, const QJSValue &name)
    {
        return js(ta::source(bars(b), name.isUndefined() ? QStringLiteral("close") : name.toString()));
    }
    Q_INVOKABLE QJSValue macd(const QJSValue &s, const QJSValue &fast, const QJSValue &slow, const QJSValue &signal)
    {
        const detail::Macd m = detail::macdFrom(values(s), len(fast, 12), len(slow, 26), len(signal, 9));
        QJSValue o = e->newObject();
        o.setProperty(QStringLiteral("macd"), js(m.macd));
        o.setProperty(QStringLiteral("signal"), js(m.signal));
        o.setProperty(QStringLiteral("hist"), js(m.hist));
        o.setProperty(QStringLiteral("colors"), js(m.colors));
        return o;
    }
    Q_INVOKABLE QJSValue supertrend(const QJSValue &b, const QJSValue &factor, const QJSValue &atrLen)
    {
        const detail::Supertrend s = detail::supertrendOf(bars(b), dbl(factor, 3), len(atrLen, 10));
        QJSValue o = e->newObject();
        o.setProperty(QStringLiteral("st"), js(s.st));
        o.setProperty(QStringLiteral("dir"), js(s.dir));
        return o;
    }
    Q_INVOKABLE QJSValue psar(const QJSValue &b, const QJSValue &start, const QJSValue &inc, const QJSValue &max)
    {
        return js(detail::psarOf(bars(b), dbl(start, 0.02), dbl(inc, 0.02), dbl(max, 0.2)));
    }
    Q_INVOKABLE QJSValue dmi(const QJSValue &b, const QJSValue &diLen, const QJSValue &adxLen)
    {
        const detail::Dmi d = detail::dmiOf(bars(b), len(diLen, 14), len(adxLen, 14));
        QJSValue o = e->newObject();
        o.setProperty(QStringLiteral("plus"), js(d.plus));
        o.setProperty(QStringLiteral("minus"), js(d.minus));
        o.setProperty(QStringLiteral("adx"), js(d.adx));
        return o;
    }

private:
    QJSValue throwError(const QString &message)
    {
        e->throwError(message);
        return QJSValue(QJSValue::UndefinedValue);
    }
    QJSValue m_keys;
};

// One imported file on one thread.
struct Module;

// import() from a custom file: imports the file on the module's engine, rewritten like the others.
class ModuleHost : public QObject
{
    Q_OBJECT
public:
    explicit ModuleHost(Module *m) : m(m) { }
    Q_INVOKABLE QJSValue importModule(const QString &specifier, const QString &baseUrl);

private:
    Module *m;
};

struct Module {
    std::unique_ptr<QJSEngine> engine;   // destroyed last
    std::unique_ptr<TaBridge> bridge;
    std::unique_ptr<ModuleHost> host;
    std::unique_ptr<QTemporaryDir> staging;   // the rewritten files (see stage())
    QHash<QString, QString> staged;           // original path -> the path the engine imports
    QJSValue tryCall, registerDefaults, ta;
    QJSValue mod, full;                  // the module namespace and the registered object
    QString id;
    QDateTime mtime;
    qint64 size = -1;
    QString importError;                 // empty = imported fine

    ~Module()
    {
        full = mod = ta = registerDefaults = tryCall = QJSValue();
        bridge.reset();
        engine.reset();
        host.reset();
    }

    struct Call {
        bool ok = false;
        QJSValue value;   // the result, or what was thrown
    };
    Call call(const QJSValue &fn, const QJSValue &self, const QJSValueList &args)
    {
        QJSValue a = engine->newArray(uint(args.size()));
        for (int i = 0; i < args.size(); ++i) a.setProperty(quint32(i), args[i]);
        const QJSValue r = tryCall.call({ fn, self, a });
        if (r.isError()) return { false, r };   // the wrapper itself failed (e.g. stack overflow)
        if (r.property(QStringLiteral("ok")).toBool()) return { true, r.property(QStringLiteral("value")) };
        return { false, r.property(QStringLiteral("error")) };
    }
};

// ---------------------------------------------------------------- modern syntax
// V4 lacks part of the syntax Node accepts (object spread and rest, class fields, async
// functions, logical assignment, ...). Each file is parsed and rewritten first (JsTransform.h).
// A file that needs no rewrite is imported as it is. Otherwise the rewritten file, and every file
// it imports with a relative path, rewritten too, go to a staging folder that mirrors their
// absolute paths, so relative imports between them keep working; the engine imports from there.

struct Source {
    QString text;
    js::Transformed t;
};

QString readSource(const QString &path, bool &ok)
{
    QFile f(path);
    ok = f.open(QIODevice::ReadOnly);
    return ok ? QString::fromUtf8(f.readAll()) : QString();
}

bool isRelative(const QString &spec)
{
    return spec.startsWith(QLatin1String("./")) || spec.startsWith(QLatin1String("../"));
}

// The files a module imports with a relative path, rewritten, depth first. Missing files are left
// to the engine, which reports them.
void collect(const QString &path, QHash<QString, Source> &graph, QStringList &order)
{
    if (graph.contains(path)) return;
    bool ok = false;
    Source s;
    s.text = readSource(path, ok);
    if (!ok) return;
    s.t = js::transformModule(s.text, QUrl::fromLocalFile(path).toString(QUrl::FullyEncoded));
    graph.insert(path, s);
    order << path;
    const QString dir = QFileInfo(path).absolutePath();
    for (const QString &spec : s.t.imports)
        if (isRelative(spec)) collect(QDir::cleanPath(QDir(dir).filePath(spec)), graph, order);
}

// The path to hand to QJSEngine::importModule for `path`, or a refusal in `error`.
QString stage(Module &m, const QString &path, QString &error)
{
    if (m.staged.contains(path)) return m.staged.value(path);
    QHash<QString, Source> graph;
    QStringList order;
    collect(path, graph, order);
    bool changed = false;
    for (const QString &p : order) {
        const Source &s = graph[p];
        if (!s.t.unsupported.isEmpty()) {
            error = s.t.unsupported;
            return path;
        }
        changed = changed || s.t.changed;
    }
    if (!changed) return path;
    if (!m.staging) {
        m.staging = std::make_unique<QTemporaryDir>(QDir::tempPath() + QStringLiteral("/qtm-custom-XXXXXX"));
        if (!m.staging->isValid()) return path;
    }
    for (const QString &p : order) {
        const QString target = m.staging->path() + p;
        if (!m.staged.contains(p)) {
            QDir().mkpath(QFileInfo(target).absolutePath());
            QFile f(target);
            if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return path;
            f.write(graph[p].t.code.toUtf8());
            m.staged.insert(p, target);
        }
    }
    return m.staged.value(path, path);
}

// V4 stops at the first construct it can't read, which in a file Node would load is modern syntax
// rather than the real mistake. Our parser accepts all of that syntax and never stops after the
// real mistake, so when V4 stopped no later than it, Node's message is the one for where our parser
// stopped. (V4 gives "file:line:col: message" lines, or one message with a lineNumber.)
QString importErrorText(const QJSValue &error, const QString &source, const js::Transformed &t)
{
    const QString v4 = errorMessage(error), msg = nodeMessage(v4);
    if (t.parsed || t.errorOffset < 0) return msg;
    int line = 1, col = 1;
    for (int i = 0; i < t.errorOffset && i < source.size(); ++i) {
        if (source[i] == QLatin1Char('\n')) {
            line++;
            col = 1;
        } else {
            col++;
        }
    }
    static const QRegularExpression loc(QStringLiteral(":(\\d+):(\\d+): "));
    const auto m = loc.match(v4);
    const QJSValue lineNumber = error.property(QStringLiteral("lineNumber"));
    if (m.hasMatch()) {
        const int v4Line = m.captured(1).toInt(), v4Col = m.captured(2).toInt();
        if (v4Line < line || (v4Line == line && v4Col <= col)) return js::syntaxErrorAt(source, t.errorOffset);
    } else if (lineNumber.isNumber() && lineNumber.toInt() <= line) {
        return js::syntaxErrorAt(source, t.errorOffset);
    }
    return msg;
}

QJSValue ModuleHost::importModule(const QString &specifier, const QString &baseUrl)
{
    QJSEngine &e = *m->engine;
    QJSValue r = e.newObject();
    const QString base = QFileInfo(QUrl(baseUrl).toLocalFile()).absolutePath();
    const QString path = specifier.startsWith(QLatin1String("file:")) ? QUrl(specifier).toLocalFile()
                         : QDir::isAbsolutePath(specifier)             ? specifier
                                                                       : QDir::cleanPath(QDir(base).filePath(specifier));
    QString error;
    const QString target = QFileInfo::exists(path) ? stage(*m, path, error) : QString();
    if (target.isEmpty() || !error.isEmpty()) {
        r.setProperty(QStringLiteral("ok"), false);
        r.setProperty(QStringLiteral("error"),
                      e.newErrorObject(QJSValue::GenericError,
                                       error.isEmpty() ? QStringLiteral("Cannot find module '%1'").arg(path) : error));
        return r;
    }
    const QJSValue ns = e.importModule(target);
    r.setProperty(QStringLiteral("ok"), !ns.isError());
    r.setProperty(ns.isError() ? QStringLiteral("error") : QStringLiteral("value"), ns);
    return r;
}

struct ThreadModules {
    QHash<QString, std::shared_ptr<Module>> byPath;
};
thread_local ThreadModules tlsModules;

struct FileStamp {
    QDateTime mtime;
    qint64 size = -1;
    bool exists = false;
};
FileStamp stampOf(const QString &path)
{
    const QFileInfo fi(path);
    return { fi.exists() ? fi.lastModified() : QDateTime(), fi.exists() ? fi.size() : -1, fi.exists() && fi.isFile() };
}

std::shared_ptr<Module> importFresh(const QString &path, const QString &id, const FileStamp &st)
{
    auto m = std::make_shared<Module>();
    m->engine = std::make_unique<QJSEngine>();
    m->engine->installExtensions(QJSEngine::ConsoleExtension);
    m->engine->evaluate(jsPolyfills(), QStringLiteral("qtm-polyfills.js"));
    m->engine->evaluate(jsRuntime(), QStringLiteral("qtm-runtime.js"));
    m->host = std::make_unique<ModuleHost>(m.get());
    QJSEngine::setObjectOwnership(m->host.get(), QJSEngine::CppOwnership);
    m->engine->globalObject().property(QStringLiteral("__qtm")).setProperty(QStringLiteral("host"), m->engine->newQObject(m->host.get()));
    m->tryCall = m->engine->evaluate(QString::fromUtf8(TRY_CALL));
    m->registerDefaults = m->engine->evaluate(QString::fromUtf8(REGISTER_DEFAULTS));
    m->bridge = std::make_unique<TaBridge>(m->engine.get(), m->engine->evaluate(QStringLiteral("(function (o) { return Object.keys(o); })")));
    QJSEngine::setObjectOwnership(m->bridge.get(), QJSEngine::CppOwnership);
    m->ta = m->engine->evaluate(QString::fromUtf8(TA_OBJECT)).call({ m->engine->newQObject(m->bridge.get()) });
    m->id = id;
    m->mtime = st.mtime;
    m->size = st.size;
    if (!st.exists) {
        m->importError = QStringLiteral("Cannot find module '%1'").arg(path);
        return m;
    }
    QString refused;
    const QString target = stage(*m, path, refused);
    if (!refused.isEmpty()) {
        m->importError = refused;
        return m;
    }
    m->mod = m->engine->importModule(target);
    if (m->mod.isError()) {
        bool ok = false;
        const QString source = readSource(path, ok);
        m->importError = importErrorText(m->mod, source, js::transformModule(source, QString()));
        m->mod = QJSValue();
        return m;
    }
    // registerIndicator({ ...mod.default, id })
    m->full = m->registerDefaults.call({ m->mod.property(QStringLiteral("default")), QJSValue(id) });
    return m;
}

// This thread's copy of the file, re-imported when the file changed on disk. For a compute
// (keepWorking), a file that was deleted or no longer imports keeps running the copy this thread
// already has, the way Node keeps the module it imported until the indicator is loaded again.
std::shared_ptr<Module> moduleFor(const QString &path, const QString &id, bool keepWorking)
{
    const FileStamp st = stampOf(path);
    auto &cache = tlsModules.byPath;
    std::shared_ptr<Module> old = cache.value(path);
    if (old && old->mtime == st.mtime && old->size == st.size && old->id == id) return old;   // unchanged (even if broken)
    const bool oldWorks = old && old->id == id && old->importError.isEmpty();
    if (keepWorking && oldWorks && !st.exists) return old;
    auto m = importFresh(path, id, st);
    if (oldWorks && !m->importError.isEmpty()) return keepWorking ? old : m;   // a broken save doesn't replace it
    cache.insert(path, m);
    return m;
}

// ---------------------------------------------------------------- format checks on the JS object

const QStringList INPUT_TYPES { "int", "float", "select", "source", "time" };
const QStringList PRICE_FIELDS { "close", "open", "high", "low", "hl2", "hlc3", "ohlc4", "hlcc4", "volume" };
const QStringList MARKER_POSITIONS { "aboveBar", "belowBar", "inBar" };
const QStringList MARKER_SHAPES { "arrowUp", "arrowDown", "circle", "square" };

bool isFiniteJs(const QJSValue &v)   // Number.isFinite
{
    return v.isNumber() && std::isfinite(v.toNumber());
}
bool isIntegerJs(const QJSValue &v)   // Number.isInteger
{
    return isFiniteJs(v) && std::trunc(v.toNumber()) == v.toNumber();
}
int lengthOf(const QJSValue &arr)
{
    return detail::toInt(arr.property(QStringLiteral("length")).toNumber());
}
// arr.includes(v) for a list of strings.
bool includes(const QStringList &list, const QJSValue &v)
{
    return v.isString() && list.contains(v.toString());
}

QJSValue toJsBars(QJSEngine &e, const Bars &bars)
{
    // The bars go over as one buffer of doubles and the objects are made by a JS loop: six property calls
    // from C++ per bar were most of a custom indicator's cost on every tick. Same keys in the same order,
    // the same numbers.
    static const QString Maker = QStringLiteral("__qtmBarsFromBuffer");
    QJSValue make = e.globalObject().property(Maker);
    if (!make.isCallable()) {
        make = e.evaluate(QStringLiteral(
            "(function (buffer, n) {"
            "  const f = new Float64Array(buffer);"
            "  const a = new Array(n);"
            "  for (let i = 0; i < n; ++i) {"
            "    const k = 6 * i;"
            "    a[i] = { time: f[k], open: f[k + 1], high: f[k + 2], low: f[k + 3], close: f[k + 4], volume: f[k + 5] };"
            "  }"
            "  return a;"
            "})"));
        e.globalObject().setProperty(Maker, make);
    }
    QByteArray buffer(qsizetype(bars.size()) * 6 * qsizetype(sizeof(double)), Qt::Uninitialized);
    double *f = reinterpret_cast<double *>(buffer.data());
    for (const Bar &b : bars) {
        *f++ = b.time;
        *f++ = b.open;
        *f++ = b.high;
        *f++ = b.low;
        *f++ = b.close;
        *f++ = b.volume;
    }
    return make.call({e.toScriptValue(buffer), double(bars.size())});
}

// JS indicatorProblems(def, { run }), on the object a custom file exports.
QStringList problemsOf(Module &m, const QJSValue &def, bool run)
{
    static const QRegularExpression KEY_RE(QStringLiteral("^[A-Za-z][A-Za-z0-9_]*$"));
    QStringList problems;
    if (!def.toBool() || typeOf(def) != QLatin1String("object") || def.isArray())
        return { QStringLiteral("the file must `export default { name, calc(bars, p, ta) { ... } }`") };
    const QJSValue name = def.property(QStringLiteral("name"));
    if (!name.isString() || name.toString().trimmed().isEmpty())
        problems << QStringLiteral("it needs a `name` (text)");
    else if (name.toString().size() > 80)
        problems << QStringLiteral("its `name` is longer than 80 characters");
    if (!def.property(QStringLiteral("calc")).isCallable()) problems << QStringLiteral("it needs a `calc(bars, p, ta)` function");
    const QJSValue overlay = def.property(QStringLiteral("overlay"));
    if (!overlay.isUndefined() && !overlay.isBool()) problems << QStringLiteral("`overlay` must be true or false");

    QJSValue inputs = def.property(QStringLiteral("inputs"));
    if (inputs.isUndefined() || inputs.isNull()) inputs = m.engine->newArray(0);
    QSet<QString> seen;
    const bool inputsOk = inputs.isArray();
    if (!inputsOk)
        problems << QStringLiteral("`inputs` must be a list");
    else
        for (int k = 0, n = lengthOf(inputs); k < n; ++k) {
            const QJSValue inp = inputs.property(quint32(k));
            const bool isObj = inp.toBool() && typeOf(inp) == QLatin1String("object");
            const QJSValue key = isObj ? inp.property(QStringLiteral("key")) : QJSValue();
            const QString at = key.isString() ? QStringLiteral("input \"%1\"").arg(key.toString())
                                              : QStringLiteral("input %1").arg(k + 1);
            if (!isObj) {
                problems << at + QStringLiteral(": must be an object like { key, type, default }");
                continue;
            }
            if (!key.isString() || !KEY_RE.match(key.toString()).hasMatch())
                problems << at + QStringLiteral(": `key` must be letters, digits or _ and start with a letter");
            else if (seen.contains(key.toString()))
                problems << at + QStringLiteral(": the key is used twice");
            if (key.isString()) seen.insert(key.toString());
            const QJSValue type = inp.property(QStringLiteral("type"));
            if (!includes(INPUT_TYPES, type)) {
                problems << at + QStringLiteral(": `type` must be one of ") + INPUT_TYPES.join(QStringLiteral(", "));
                continue;
            }
            const QString t = type.toString();
            const QJSValue d = inp.property(QStringLiteral("default"));
            if (t == QLatin1String("int") && !isIntegerJs(d)) problems << at + QStringLiteral(": an int input needs a whole-number default");
            if (t == QLatin1String("float") && !isFiniteJs(d)) problems << at + QStringLiteral(": a float input needs a number default");
            if (t == QLatin1String("time") && !(isFiniteJs(d) && d.toNumber() >= 0))
                problems << at + QStringLiteral(": a time input's default is a bar time in seconds, or 0");
            if (t == QLatin1String("select")) {
                const QJSValue opts = inp.property(QStringLiteral("options"));
                bool ok = opts.isArray() && lengthOf(opts) > 0;
                QStringList list;
                if (ok)
                    for (int i = 0, n = lengthOf(opts); i < n; ++i) {
                        const QJSValue o = opts.property(quint32(i));
                        if (!o.isString()) ok = false;
                        else list << o.toString();
                    }
                if (!ok) problems << at + QStringLiteral(": a select input needs `options`, a list of text choices");
                else if (!includes(list, d)) problems << at + QStringLiteral(": its default must be one of its options");
            }
            if (t == QLatin1String("source")) {
                if (!(key.isString() && key.toString() == QLatin1String("source")))
                    problems << at + QStringLiteral(": a source input's key must be \"source\"");
                if (!includes(PRICE_FIELDS, d))
                    problems << at + QStringLiteral(": a source input's default must be one of ") + PRICE_FIELDS.join(QStringLiteral(", "));
            }
            for (const char *mm : { "min", "max" }) {
                const QJSValue v = inp.property(QLatin1String(mm));
                if (!v.isUndefined() && !isFiniteJs(v)) problems << at + QStringLiteral(": `%1` must be a number").arg(QLatin1String(mm));
            }
        }

    const QJSValue outputs = def.property(QStringLiteral("outputs"));
    QSet<QString> outKeys;
    if (!outputs.isArray() || lengthOf(outputs) == 0)
        problems << QStringLiteral("`outputs` must be a list with at least one output");
    else
        for (int k = 0, n = lengthOf(outputs); k < n; ++k) {
            const QJSValue o = outputs.property(quint32(k));
            const bool isObj = o.toBool() && typeOf(o) == QLatin1String("object");
            const QJSValue key = isObj ? o.property(QStringLiteral("key")) : QJSValue();
            const QString at = key.isString() ? QStringLiteral("output \"%1\"").arg(key.toString())
                                              : QStringLiteral("output %1").arg(k + 1);
            if (!isObj) {
                problems << at + QStringLiteral(": must be an object like { key, kind, color }");
                continue;
            }
            if (!key.isString() || !KEY_RE.match(key.toString()).hasMatch())
                problems << at + QStringLiteral(": `key` must be letters, digits or _ and start with a letter");
            else if (outKeys.contains(key.toString()))
                problems << at + QStringLiteral(": the key is used twice");
            if (key.isString()) outKeys.insert(key.toString());
            const QJSValue kind = o.property(QStringLiteral("kind"));
            if (!kind.isUndefined() && !includes({ "line", "histogram" }, kind))
                problems << at + QStringLiteral(": `kind` must be \"line\" or \"histogram\"");
            const QJSValue color = o.property(QStringLiteral("color"));
            if (!color.isUndefined() && (!color.isString() || color.toString().trimmed().isEmpty()))
                problems << at + QStringLiteral(": `color` must be a colour like #2962ff");
            const QJSValue lw = o.property(QStringLiteral("lineWidth"));
            if (!lw.isUndefined() && !(isIntegerJs(lw) && lw.toNumber() >= 1 && lw.toNumber() <= 4))
                problems << at + QStringLiteral(": `lineWidth` must be 1, 2, 3 or 4");
        }
    const QJSValue levels = def.property(QStringLiteral("levels"));
    if (!levels.isUndefined()) {
        bool ok = levels.isArray();
        if (ok)
            for (int i = 0, n = lengthOf(levels); i < n; ++i)
                if (!isFiniteJs(levels.property(quint32(i)))) ok = false;
        if (!ok) problems << QStringLiteral("`levels` must be a list of numbers");
    }

    // A trial run on test bars: calc must return one value per bar for every output.
    if (run && problems.isEmpty()) {
        const Bars bars = detail::trialBars();
        const QJSValue jsBars = toJsBars(*m.engine, bars);
        QJSValue p = m.engine->newObject();
        for (int i = 0, n = lengthOf(inputs); i < n; ++i) {
            const QJSValue inp = inputs.property(quint32(i));
            p.setProperty(inp.property(QStringLiteral("key")).toString(), inp.property(QStringLiteral("default")));
        }
        Module::Call c;
        {
            BarsFrameScope frame(m.engine.get(), jsBars, &bars);
            c = m.call(def.property(QStringLiteral("calc")), def, { jsBars, p, m.ta });
        }
        if (!c.ok) problems << QStringLiteral("calc() stopped with an error on test bars: ") + nodeMessage(errorMessage(c.value));
        if (problems.isEmpty()) {
            const QJSValue res = c.value;
            if (!res.toBool() || typeOf(res) != QLatin1String("object"))
                problems << QStringLiteral("calc() must return an object, like { value: [...] }");
            else {
                for (int k = 0, n = lengthOf(outputs); k < n; ++k) {
                    const QString key = outputs.property(quint32(k)).property(QStringLiteral("key")).toString();
                    const QJSValue v = res.property(key);
                    if (!v.isArray())
                        problems << QStringLiteral("output \"%1\": calc() returned no list for it. Return { %1: [...] }").arg(key);
                    else if (lengthOf(v) != bars.size())
                        problems << QStringLiteral("output \"%1\": calc() returned %2 values, but there are %3 bars. Return one "
                                                   "value per bar (NaN where there isn't one)")
                                        .arg(key)
                                        .arg(lengthOf(v))
                                        .arg(bars.size());
                    else {
                        for (int i = 0, len = lengthOf(v); i < len; ++i) {
                            const QJSValue x = v.property(quint32(i));
                            if (!x.isUndefined() && !x.isNull() && !x.isNumber()) {
                                problems << QStringLiteral("output \"%1\": every value must be a number (or NaN/null)").arg(key);
                                break;
                            }
                        }
                    }
                }
                const QJSValue markers = res.property(QStringLiteral("markers"));
                if (!markers.isUndefined()) {
                    bool ok = markers.isArray();
                    if (ok)
                        for (int i = 0, n = lengthOf(markers); i < n && ok; ++i) {
                            const QJSValue mk = markers.property(quint32(i));
                            ok = mk.toBool() && isFiniteJs(mk.property(QStringLiteral("time")))
                                 && includes(MARKER_POSITIONS, mk.property(QStringLiteral("position")))
                                 && includes(MARKER_SHAPES, mk.property(QStringLiteral("shape")));
                        }
                    if (!ok)
                        problems << QStringLiteral("markers: each needs a time in seconds, a position (%1) and a shape (%2)")
                                        .arg(MARKER_POSITIONS.join(QStringLiteral(", ")), MARKER_SHAPES.join(QStringLiteral(", ")));
                }
            }
        }
    }
    return problems;
}

// ---------------------------------------------------------------- the C++ definition

IndicatorResult runCalc(const QString &path, const QString &id, const Bars &bars, const QJsonObject &params)
{
    const std::shared_ptr<Module> m = moduleFor(path, id, true);
    if (!m->importError.isEmpty()) throw Error(m->importError);
    const QJSValue jsBars = toJsBars(*m->engine, bars);
    const QJSValue p = m->bridge->js(QJsonValue(params));
    Module::Call c;
    {
        BarsFrameScope frame(m->engine.get(), jsBars, &bars);
        c = m->call(m->full.property(QStringLiteral("calc")), m->full, { jsBars, p, m->ta });
    }
    if (!c.ok) throw Error(nodeMessage(errorMessage(c.value)));
    return m->bridge->result(c.value);
}

IndicatorDef defFrom(Module &m, const QString &path)
{
    const QJSValue full = m.full;
    TaBridge &B = *m.bridge;
    auto str = [](const QJSValue &v) { return (v.isUndefined() || v.isNull()) ? QString() : v.toString(); };
    IndicatorDef d;
    d.id = m.id;
    d.name = str(full.property(QStringLiteral("name")));
    d.group = str(full.property(QStringLiteral("group")));
    d.overlay = full.property(QStringLiteral("overlay")).toBool();
    const QJSValue inputs = full.property(QStringLiteral("inputs"));
    if (inputs.isArray())
        for (int i = 0, n = lengthOf(inputs); i < n; ++i) {
            const QJSValue in = inputs.property(quint32(i));
            IndicatorInput x;
            x.key = str(in.property(QStringLiteral("key")));
            x.label = str(in.property(QStringLiteral("label")));
            x.type = str(in.property(QStringLiteral("type")));
            x.def = B.json(in.property(QStringLiteral("default")));
            const QJSValue opts = in.property(QStringLiteral("options"));
            if (opts.isArray())
                for (int k = 0, no = lengthOf(opts); k < no; ++k) x.options << opts.property(quint32(k)).toString();
            x.min = B.json(in.property(QStringLiteral("min")));
            x.max = B.json(in.property(QStringLiteral("max")));
            d.inputs << x;
        }
    const QJSValue outputs = full.property(QStringLiteral("outputs"));
    for (int i = 0, n = lengthOf(outputs); i < n; ++i) {
        const QJSValue o = outputs.property(quint32(i));
        IndicatorOutput x;
        x.key = str(o.property(QStringLiteral("key")));
        x.label = str(o.property(QStringLiteral("label")));
        const QJSValue kind = o.property(QStringLiteral("kind"));
        x.kind = kind.isUndefined() ? QStringLiteral("line") : kind.toString();
        x.color = str(o.property(QStringLiteral("color")));
        const QJSValue lw = o.property(QStringLiteral("lineWidth"));
        x.lineWidth = lw.isUndefined() ? 0 : lw.toInt();
        x.dots = o.property(QStringLiteral("dots")).toBool();
        d.outputs << x;
    }
    const QJSValue levels = full.property(QStringLiteral("levels"));
    if (levels.isArray())
        for (int i = 0, n = lengthOf(levels); i < n; ++i) d.levels << levels.property(quint32(i)).toNumber();
    d.noAutoscale = full.property(QStringLiteral("noAutoscale")).toBool();
    d.volumeFormat = full.property(QStringLiteral("volumeFormat")).toBool();
    d.file = QFileInfo(path).absoluteFilePath();
    const QString id = m.id, file = d.file;
    d.calc = [file, id](const Bars &bars, const QJsonObject &params) { return runCalc(file, id, bars, params); };
    return d;
}

} // namespace

IndicatorDefPtr loadCustomIndicator(const QString &path, const QString &id)
{
    const QString file = QFileInfo(path).absoluteFilePath();
    const std::shared_ptr<Module> m = moduleFor(file, id, false);
    if (!m->importError.isEmpty()) throw Error(m->importError);
    const QStringList problems = problemsOf(*m, m->full, true);
    if (!problems.isEmpty())
        throw Error(QStringLiteral("This indicator doesn't follow the format (docs/custom-indicators.md):\n- ")
                    + problems.join(QStringLiteral("\n- ")));
    return detail::insertIndicator(defFrom(*m, file));
}

QStringList customIndicatorProblems(const QString &path, bool run)
{
    const QString file = QFileInfo(path).absoluteFilePath();
    const std::shared_ptr<Module> m = moduleFor(file, QString(), false);
    if (!m->importError.isEmpty()) throw Error(m->importError);
    return problemsOf(*m, m->mod.property(QStringLiteral("default")), run);
}

bool isCustomIndicatorFile(const QString &fileName)
{
    return fileName.endsWith(QLatin1String(".js")) || fileName.endsWith(QLatin1String(".pine"));
}

QString customIndicatorBaseName(const QString &fileName)
{
    QString n = fileName;
    if (n.endsWith(QLatin1String(".js"))) n.chop(3);
    else if (n.endsWith(QLatin1String(".pine"))) n.chop(5);
    return n;
}

IndicatorDefPtr loadCustomIndicatorFile(const QString &path, const QString &id)
{
    return path.endsWith(QLatin1String(".pine")) ? loadPineIndicator(path, id) : loadCustomIndicator(path, id);
}

} // namespace qtm

#include "CustomIndicators.moc"
