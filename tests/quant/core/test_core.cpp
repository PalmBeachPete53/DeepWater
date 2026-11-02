// qtm_test_core: the C++ port of quant-terminal/public/lib/{series,indicators,seasonals}.js and the
// custom indicator loader, checked against the JS originals (reference/reference.json, written by
// make_reference.mjs), plus the relevant cases of the JS test suite (test/indicators.test.js,
// test/reference.test.js, test/series.test.js) ported as they are.
#include "core/CustomIndicators.h"
#include "core/Error.h"
#include "core/Indicators.h"
#include "core/JsCompat.h"
#include "core/Seasonals.h"
#include "core/Series.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QThread>

#include <atomic>
#include <cstring>
#include <iostream>
#include <thread>

using namespace qtm;

// ---------------------------------------------------------------- bookkeeping
namespace {

struct Stats {
    qint64 checks = 0, exact = 0, near = 0, fails = 0;
    QMap<QString, int> failsBy;
    QMap<QString, qint64> checksBy;
    QMap<QString, int> nearBy;
} S;

QString CAT = "misc";

void fail(const QString &msg)
{
    S.fails++;
    if (S.failsBy[CAT]++ < 12) std::cout << "FAIL [" << CAT.toStdString() << "] " << msg.toStdString() << "\n";
}
void check(bool ok, const QString &msg)
{
    S.checks++;
    S.checksBy[CAT]++;
    if (ok) S.exact++;
    else fail(msg);
}
QString num(double v)
{
    return QString::number(v, 'g', 17);
}
// Same number: bit-exact (NaN == NaN, -0 == 0) counts as exact, within 1e-9 relative as near.
void checkNum(double want, double got, const QString &ctx)
{
    S.checks++;
    S.checksBy[CAT]++;
    if ((std::isnan(want) && std::isnan(got)) || want == got) {
        S.exact++;
        return;
    }
    if (std::isfinite(want) && std::isfinite(got) && std::fabs(want - got) <= 1e-9 * std::max(1.0, std::fabs(want))) {
        S.near++;
        S.nearBy[ctx.section(QLatin1Char(' '), 0, 0).section(QLatin1Char('/'), 0, 0)]++;
        if (S.near <= 10) std::cout << "near [" << CAT.toStdString() << "] " << ctx.toStdString() << ": " << num(want).toStdString()
                                    << " vs " << num(got).toStdString() << "\n";
        return;
    }
    fail(QStringLiteral("%1: want %2, got %3").arg(ctx, num(want), num(got)));
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
double jnum(const QJsonValue &v)
{
    if (v.isString() && v.toString().startsWith(QLatin1String("f64:"))) return f64(v).value(0, NaN);
    if (v.isNull() || v.isUndefined()) return NaN;
    return v.toDouble();
}
Bars barsFrom(const QJsonObject &o)
{
    const Values t = f64(o["time"]), op = f64(o["open"]), h = f64(o["high"]), l = f64(o["low"]), c = f64(o["close"]),
                 v = f64(o["volume"]);
    const QJsonArray colors = o["color"].toArray();
    Bars out(t.size());
    for (int i = 0; i < t.size(); ++i) {
        out[i].time = t[i];
        out[i].open = op[i];
        out[i].high = h[i];
        out[i].low = l[i];
        out[i].close = c[i];
        out[i].volume = v[i];
        out[i].color = colors.at(i).toString();
    }
    return out;
}
void checkValues(const Values &want, const Values &got, const QString &ctx)
{
    if (want.size() != got.size()) {
        check(false, QStringLiteral("%1: length want %2, got %3").arg(ctx).arg(want.size()).arg(got.size()));
        return;
    }
    for (int i = 0; i < want.size(); ++i) checkNum(want[i], got[i], QStringLiteral("%1[%2]").arg(ctx).arg(i));
}
void checkBars(const QJsonObject &want, const Bars &got, const QString &ctx)
{
    const Bars w = barsFrom(want);
    if (w.size() != got.size()) {
        check(false, QStringLiteral("%1: %2 bars, got %3").arg(ctx).arg(w.size()).arg(got.size()));
        return;
    }
    for (int i = 0; i < w.size(); ++i) {
        const QString c = QStringLiteral("%1[%2]").arg(ctx).arg(i);
        checkNum(w[i].time, got[i].time, c + ".time");
        checkNum(w[i].open, got[i].open, c + ".open");
        checkNum(w[i].high, got[i].high, c + ".high");
        checkNum(w[i].low, got[i].low, c + ".low");
        checkNum(w[i].close, got[i].close, c + ".close");
        checkNum(w[i].volume, got[i].volume, c + ".volume");
        checkStr(w[i].color, got[i].color, c + ".color");
    }
}
// Deep JSON comparison; numbers may be "f64:" strings on the reference side, NaN is null in JSON.
void checkJson(const QJsonValue &want, const QJsonValue &got, const QString &ctx)
{
    if (want.isString() && want.toString().startsWith(QLatin1String("f64:"))) {
        checkNum(jnum(want), jnum(got), ctx);
        return;
    }
    if (want.isNull()) {
        check(got.isNull() || (got.isDouble() && std::isnan(got.toDouble())), ctx + ": want null, got " + QJsonDocument(QJsonArray { got }).toJson(QJsonDocument::Compact));
        return;
    }
    if (want.isDouble()) {
        if (!got.isDouble()) {
            check(false, ctx + ": want a number, got " + QJsonDocument(QJsonArray { got }).toJson(QJsonDocument::Compact));
            return;
        }
        checkNum(want.toDouble(), got.toDouble(), ctx);
        return;
    }
    if (want.isArray()) {
        const QJsonArray a = want.toArray(), b = got.toArray();
        if (!got.isArray() || a.size() != b.size()) {
            check(false, QStringLiteral("%1: array of %2, got %3").arg(ctx).arg(a.size()).arg(got.isArray() ? b.size() : -1));
            return;
        }
        for (int i = 0; i < a.size(); ++i) checkJson(a[i], b[i], QStringLiteral("%1[%2]").arg(ctx).arg(i));
        return;
    }
    if (want.isObject()) {
        const QJsonObject a = want.toObject(), b = got.toObject();
        if (!got.isObject()) {
            check(false, ctx + ": want an object");
            return;
        }
        QStringList ka = a.keys(), kb = b.keys();
        check(ka == kb, QStringLiteral("%1: keys %2 vs %3").arg(ctx, ka.join(','), kb.join(',')));
        for (const QString &k : ka) checkJson(a[k], b.value(k), ctx + "." + k);
        return;
    }
    check(want == got, ctx + ": want " + QJsonDocument(QJsonArray { want }).toJson(QJsonDocument::Compact) + ", got "
                           + QJsonDocument(QJsonArray { got }).toJson(QJsonDocument::Compact));
}
QJsonValue markersJson(const IndicatorResult &r)
{
    QJsonArray a;
    for (const IndicatorMarker &m : r.markers)
        a.append(QJsonObject { { "time", m.time }, { "position", m.position }, { "shape", m.shape }, { "color", m.color }, { "text", m.text } });
    return a;
}
void checkResult(const QJsonObject &want, const IndicatorResult &got, const QString &ctx)
{
    const QJsonObject wv = want["values"].toObject();
    QStringList wk = wv.keys(), gk = got.values.keys();
    std::sort(gk.begin(), gk.end());
    check(wk == gk, QStringLiteral("%1: value keys %2 vs %3").arg(ctx, wk.join(','), gk.join(',')));
    for (const QString &k : wk) checkValues(f64(wv[k]), got.values.value(k), ctx + "." + k);
    check(want.contains("markers") == got.hasMarkers, ctx + ": markers present differs");
    if (want.contains("markers")) {
        const QJsonArray wm = want["markers"].toArray();
        if (wm.size() != got.markers.size())
            check(false, QStringLiteral("%1: %2 markers, got %3").arg(ctx).arg(wm.size()).arg(got.markers.size()));
        else
            for (int i = 0; i < wm.size(); ++i) {
                const QJsonObject m = wm[i].toObject();
                const IndicatorMarker &g = got.markers[i];
                const QString c = QStringLiteral("%1.markers[%2]").arg(ctx).arg(i);
                checkNum(jnum(m["time"]), g.time, c + ".time");
                checkStr(m["position"].toString(), g.position, c + ".position");
                checkStr(m["shape"].toString(), g.shape, c + ".shape");
                checkStr(m["color"].toString(), g.color, c + ".color");
                checkStr(m["text"].toString(), g.text, c + ".text");
            }
    }
    QStringList wc;
    for (const QJsonValue &c : want["colors"].toArray()) wc << c.toString();
    check(wc == got.colors, ctx + ": colors differ" + (wc.size() != got.colors.size() ? QStringLiteral(" (sizes %1 vs %2)").arg(wc.size()).arg(got.colors.size()) : QString()));
    QVector<bool> wb;
    for (const QJsonValue &b : want["breaks"].toArray()) wb << b.toBool();
    check(wb == got.breaks, ctx + ": breaks differ");
    checkJson(want["extra"], got.extra, ctx + ".extra");
}
void checkDef(const QJsonObject &w, const IndicatorDef &d, const QString &ctx)
{
    checkStr(w["id"].toString(), d.id, ctx + ".id");
    checkStr(w["name"].toString(), d.name, ctx + ".name");
    checkStr(w["group"].toString(), d.group, ctx + ".group");
    check(w["overlay"].toBool() == d.overlay, ctx + ".overlay");
    const QJsonArray ins = w["inputs"].toArray();
    check(ins.size() == d.inputs.size(), ctx + ".inputs size");
    for (int i = 0; i < std::min<int>(ins.size(), d.inputs.size()); ++i) {
        const QJsonObject a = ins[i].toObject();
        const IndicatorInput &b = d.inputs[i];
        const QString c = QStringLiteral("%1.inputs[%2]").arg(ctx).arg(i);
        checkStr(a["key"].toString(), b.key, c + ".key");
        checkStr(a["label"].toString(), b.label, c + ".label");
        checkStr(a["type"].toString(), b.type, c + ".type");
        checkJson(a["default"], b.def, c + ".default");
        QStringList opts;
        for (const QJsonValue &o : a["options"].toArray()) opts << o.toString();
        check(opts == b.options, c + ".options");
        check(a.contains("min") == !b.min.isUndefined(), c + ".min presence");
        if (a.contains("min")) checkJson(a["min"], b.min, c + ".min");
        check(a.contains("max") == !b.max.isUndefined(), c + ".max presence");
        if (a.contains("max")) checkJson(a["max"], b.max, c + ".max");
    }
    const QJsonArray outs = w["outputs"].toArray();
    check(outs.size() == d.outputs.size(), ctx + ".outputs size");
    for (int i = 0; i < std::min<int>(outs.size(), d.outputs.size()); ++i) {
        const QJsonObject a = outs[i].toObject();
        const IndicatorOutput &b = d.outputs[i];
        const QString c = QStringLiteral("%1.outputs[%2]").arg(ctx).arg(i);
        checkStr(a["key"].toString(), b.key, c + ".key");
        checkStr(a["label"].toString(), b.label, c + ".label");
        checkStr(a["kind"].toString(), b.kind, c + ".kind");
        checkStr(a["color"].toString(), b.color, c + ".color");
        check(a["lineWidth"].toInt() == b.lineWidth, c + ".lineWidth");
        check(a["dots"].toBool() == b.dots, c + ".dots");
    }
    Values lv;
    for (const QJsonValue &l : w["levels"].toArray()) lv << jnum(l);
    checkValues(lv, d.levels, ctx + ".levels");
    check(w["noAutoscale"].toBool() == d.noAutoscale, ctx + ".noAutoscale");
    check(w["volumeFormat"].toBool() == d.volumeFormat, ctx + ".volumeFormat");
}

QMap<QString, Bars> SETS;

void runComputes(const QJsonArray &computes, const QString &prefix)
{
    for (const QJsonValue &cv : computes) {
        const QJsonObject c = cv.toObject();
        const QString id = c["id"].toString(), set = c["set"].toString();
        const QJsonObject params = c["params"].toObject();
        const QString ctx = QStringLiteral("%1%2/%3 %4").arg(prefix, id, set, QString::fromUtf8(QJsonDocument(params).toJson(QJsonDocument::Compact)));
        if (!SETS.contains(set)) {
            check(false, ctx + ": unknown set");
            continue;
        }
        try {
            const IndicatorResult r = compute(id, SETS[set], params);
            if (c.contains("error")) check(false, ctx + ": expected error " + c["error"].toString());
            else checkResult(c["result"].toObject(), r, ctx);
        } catch (const Error &e) {
            if (c.contains("error")) checkStr(c["error"].toString(), e.message(), ctx + " error");
            else check(false, ctx + ": threw " + e.message());
        }
    }
}

QString errorOf(const std::function<void()> &f)
{
    try {
        f();
    } catch (const Error &e) {
        return e.message();
    }
    return QString();
}

} // namespace

// ---------------------------------------------------------------- reference checks
static void checkReference(const QJsonObject &ref)
{
    const QJsonObject sets = ref["sets"].toObject();
    for (auto it = sets.begin(); it != sets.end(); ++it) SETS.insert(it.key(), barsFrom(it.value().toObject()));
    const Bars &btc = SETS["btc"];

    CAT = "registry";
    const QJsonArray defs = ref["indicators"].toArray();
    const QVector<IndicatorDefPtr> all = indicators();
    check(all.size() >= defs.size(), QStringLiteral("%1 indicators, want %2").arg(all.size()).arg(defs.size()));
    for (int i = 0; i < defs.size() && i < all.size(); ++i) checkDef(defs[i].toObject(), *all[i], "def " + defs[i].toObject()["id"].toString());
    const QJsonObject defaults = ref["defaults"].toObject();
    for (auto it = defaults.begin(); it != defaults.end(); ++it) checkJson(it.value(), indicatorDefaults(it.key()), "defaults " + it.key());
    checkStr(ref["unknown"].toString(), errorOf([&] { compute("nope", btc); }), "unknown indicator");
    check(!indicatorById("nope"), "indicatorById unknown");

    CAT = "indicators";
    runComputes(ref["computes"].toArray(), "");

    CAT = "ta";
    const QJsonObject taRef = ref["ta"].toObject();
    const Values holey = f64(taRef["holey"]), close120 = f64(taRef["close120"]), vol120 = f64(taRef["volume120"]);
    for (const QJsonValue &cv : taRef["cases"].toArray()) {
        const QJsonObject c = cv.toObject();
        const QString name = c["name"].toString();
        const QJsonArray args = c["args"].toArray();
        const QString ctx = name + " " + QJsonDocument(args).toJson(QJsonDocument::Compact);
        const Values &src = args[0].toString() == "holey" ? holey : close120;
        const int n = args.size() > 1 ? args[1].toInt() : 0;
        Values got;
        QVector<bool> gotB;
        bool isBool = false;
        if (name == "sma") got = ta::sma(src, n);
        else if (name == "ema") got = ta::ema(src, n);
        else if (name == "rma") got = ta::rma(src, n);
        else if (name == "wma") got = ta::wma(src, n);
        else if (name == "hma") got = ta::hma(src, n);
        else if (name == "dema") got = ta::dema(src, n);
        else if (name == "tema") got = ta::tema(src, n);
        else if (name == "alma") got = args.size() > 2 ? ta::alma(src, n, args[2].toDouble(), args[3].toDouble()) : ta::alma(src, n);
        else if (name == "linreg") got = ta::linreg(src, n, args.size() > 2 ? args[2].toInt() : 0);
        else if (name == "stdev") got = ta::stdev(src, n);
        else if (name == "dev") got = ta::dev(src, n);
        else if (name == "highest") got = ta::highest(src, n);
        else if (name == "lowest") got = ta::lowest(src, n);
        else if (name == "change") got = ta::change(src, n);
        else if (name == "sum") got = ta::sum(src, n);
        else if (name == "rsi") got = ta::rsi(src, n);
        else if (name == "vwma") got = ta::vwma(src, vol120, n);
        else if (name == "crossover" || name == "crossunder") {
            isBool = true;
            const bool over = name == "crossover";
            if (args.size() > 1) gotB = over ? ta::crossover(holey, args[1].toDouble()) : ta::crossunder(holey, args[1].toDouble());
            else {
                const Values fast = ta::ema(holey, 5), slow = ta::sma(holey, 9);
                gotB = over ? ta::crossover(fast, slow) : ta::crossunder(fast, slow);
            }
        } else if (name == "tr") got = ta::tr(SETS[args[0].toString()]);
        else if (name == "atr") got = ta::atr(SETS[args[0].toString()], args[1].toInt());
        else if (name == "source") got = ta::source(SETS[args[0].toString()], args[1].toString());
        else {
            check(false, "unknown ta case " + name);
            continue;
        }
        if (isBool)
            for (bool b : gotB) got << (b ? 1 : 0);
        checkValues(f64(c["value"]), got, ctx);
    }

    CAT = "periodKey";
    for (const QJsonValue &pv : ref["periodKeys"].toArray()) {
        const QJsonObject p = pv.toObject();
        const double t = jnum(p["t"]);
        checkStr(p["key"].toString(), periodKey(t, p["period"].toString()), QStringLiteral("periodKey(%1, %2)").arg(num(t), p["period"].toString()));
    }

    CAT = "series";
    const QJsonObject series = ref["series"].toObject();
    for (const QJsonValue &v : series["parseTf"].toArray()) {
        const QJsonObject c = v.toObject();
        const QString tf = c["tf"].toString();
        if (c.contains("error")) {
            checkStr(c["error"].toString(), errorOf([&] { parseTf(tf); }), "parseTf(" + tf + ")");
            checkStr(c["error"].toString(), errorOf([&] { isIntraday(tf); }), "isIntraday(" + tf + ")");
        } else {
            const Tf r = parseTf(tf);
            check(r.n == c["n"].toInt() && r.unit == c["unit"].toString().at(0) && r.seconds == qint64(c["seconds"].toDouble()),
                  "parseTf(" + tf + ")");
            check(isIntraday(tf) == c["intraday"].toBool(), "isIntraday(" + tf + ")");
        }
    }
    for (const QJsonValue &v : series["bucketStart"].toArray()) {
        const QJsonObject c = v.toObject();
        const qint64 t = qint64(c["t"].toDouble());
        const QString tf = c["tf"].toString(), ctx = QStringLiteral("bucketStart(%1, %2)").arg(t).arg(tf);
        if (c.contains("error")) checkStr(c["error"].toString(), errorOf([&] { bucketStart(t, tf); }), ctx);
        else checkNum(c["value"].toDouble(), double(bucketStart(t, tf)), ctx);
    }
    for (const QJsonValue &v : series["resample"].toArray()) {
        const QJsonObject c = v.toObject();
        const QString set = c["set"].toString();
        checkBars(c["bars"].toObject(), resample(set == "empty" ? Bars() : SETS[set], c["tf"].toString()), "resample " + set + " " + c["tf"].toString());
    }
    for (const QJsonValue &v : series["nativeBase"].toArray()) {
        const QJsonObject c = v.toObject();
        QStringList natives;
        for (const QJsonValue &n : c["natives"].toArray()) natives << n.toString();
        const QString tf = c["tf"].toString(), ctx = "nativeBase(" + tf + ", " + natives.join(' ') + ")";
        if (c.contains("error")) checkStr(c["error"].toString(), errorOf([&] { nativeBase(tf, natives); }), ctx);
        else checkStr(c["value"].isNull() ? QString() : c["value"].toString(), nativeBase(tf, natives), ctx);
    }
    for (const QJsonValue &v : series["transforms"].toArray()) {
        const QJsonObject c = v.toObject();
        const QString fn = c["fn"].toString(), set = c["set"].toString();
        const QJsonArray a = c["args"].toArray();
        const Bars &b = SETS[set];
        const QString ctx = fn + " " + set + " " + QJsonDocument(a).toJson(QJsonDocument::Compact);
        Bars got;
        if (fn == "heikinAshi") got = heikinAshi(b);
        else if (fn == "renko") got = a.isEmpty() ? renko(b) : renko(b, a[0].toDouble());
        else if (fn == "lineBreak") got = a.isEmpty() ? lineBreak(b) : lineBreak(b, a[0].toInt());
        else if (fn == "rangeBars") got = a.isEmpty() ? rangeBars(b) : rangeBars(b, a[0].toDouble());
        else if (fn == "kagi") got = a.isEmpty() ? kagi(b) : kagi(b, a[0].toDouble());
        else if (fn == "pointFigure") got = a.isEmpty() ? pointFigure(b) : pointFigure(b, a[0].toDouble(), a[1].toInt());
        checkBars(c["bars"].toObject(), got, ctx);
    }
    int csvN = 0;
    for (const QJsonValue &v : series["toCsv"].toArray()) {
        const QJsonObject c = v.toObject();
        QVector<QPair<QString, Values>> cols;
        for (const QJsonValue &col : c["cols"].toArray()) cols.push_back({ col.toArray()[0].toString(), f64(col.toArray()[1]) });
        Bars b = barsFrom(c["bars"].toObject());
        checkStr(c["csv"].toString(), toCsv(b, cols), QStringLiteral("toCsv #%1").arg(csvN++));
    }

    CAT = "seasonals";
    if (ref["thisYear"].toInt() != QDateTime::currentDateTimeUtc().date().year())
        std::cout << "note: reference made in " << ref["thisYear"].toInt() << ", the current year changes `complete`\n";
    for (const QJsonValue &v : ref["seasonals"].toArray()) {
        const QJsonObject c = v.toObject();
        const QString set = c["set"].toString();
        checkJson(c["value"], seasonals(SETS[set]), "seasonals " + set);
    }

    CAT = "custom";
    const QString root = QStringLiteral(QTM_ROOT_DIR);
    for (const QJsonValue &v : ref["custom"].toArray()) {
        const QJsonObject c = v.toObject();
        const QString file = c["file"].toString(), path = root + "/" + c["path"].toString();
        const QString id = "custom:" + QString(file).remove(QRegularExpression("\\.js$"));
        QString err;
        IndicatorDefPtr d;
        try {
            d = loadCustomIndicator(path, id);
        } catch (const Error &e) {
            err = e.message();
        }
        checkStr(c["error"].isNull() ? QString() : c["error"].toString(), err, "load " + file);
        if (d) {
            checkDef(c["def"].toObject(), *d, "custom def " + file);
            check(indicatorById(id) == d, "registered " + id);
            QElapsedTimer t;
            t.start();
            runComputes(c["computes"].toArray(), file + ": ");
            std::cout << "  " << file.toStdString() << ": " << c["computes"].toArray().size() << " runs in " << t.elapsed() << " ms\n";
        }
    }

    CAT = "problems";
    for (const QJsonValue &v : ref["problems"].toArray()) {
        const QJsonObject c = v.toObject();
        const QString file = c["file"].toString(), path = root + "/" + c["path"].toString();
        const QString id = "custom:" + QString(file).remove(QRegularExpression("\\.js$"));
        if (c.contains("importError")) {
            checkStr(c["importError"].toString(), errorOf([&] { customIndicatorProblems(path); }), "import " + file);
        } else {
            QStringList want, wantNoRun;
            for (const QJsonValue &p : c["problems"].toArray()) want << p.toString();
            for (const QJsonValue &p : c["problemsNoRun"].toArray()) wantNoRun << p.toString();
            QStringList got, gotNoRun;
            try {
                got = customIndicatorProblems(path, true);
                gotNoRun = customIndicatorProblems(path, false);
            } catch (const Error &e) {
                got << "threw: " + e.message();
            }
            checkStr(want.join(" | "), got.join(" | "), "problems " + file);
            checkStr(wantNoRun.join(" | "), gotNoRun.join(" | "), "problems (no run) " + file);
        }
        QString err;
        IndicatorDefPtr d;
        try {
            d = loadCustomIndicator(path, id);
        } catch (const Error &e) {
            err = e.message();
        }
        checkStr(c["loadError"].isNull() ? QString() : c["loadError"].toString(), err, "load " + file);
        if (!err.isEmpty()) check(!indicatorById(id), "a refused indicator is not registered: " + id);
        if (d && c.contains("def")) {
            checkDef(c["def"].toObject(), *d, "def " + file);
            runComputes(c["computes"].toArray(), file + ": ");
        }
    }
}

// ---------------------------------------------------------------- ported JS unit tests
static bool near(double a, double b, double tol = 1e-6)
{
    return std::fabs(a - b) <= tol * std::max(1.0, std::fabs(b));
}

static Bars mkBars(const QVector<double> &closes)
{
    Bars out;
    for (int i = 0; i < closes.size(); ++i) {
        Bar b;
        b.time = 1700000000 + i * 86400.0;
        b.open = b.high = b.low = b.close = closes[i];
        b.volume = 1;
        out << b;
    }
    return out;
}
// reference.test.js hand(): highs and lows only matter, the rest follows from them.
static Bars hand(const QVector<double> &highs, const QVector<double> &lows)
{
    Bars out;
    for (int i = 0; i < highs.size(); ++i) {
        Bar b;
        b.time = 1700000000 + i * 86400.0;
        b.open = (highs[i] + lows[i]) / 2;
        b.high = highs[i];
        b.low = lows[i];
        b.close = (highs[i] + lows[i]) / 2;
        b.volume = 1;
        out << b;
    }
    return out;
}
static int indexOfTime(const Bars &b, double t)
{
    for (int i = 0; i < b.size(); ++i)
        if (b[i].time == t) return i;
    return -1;
}

static void portedTests()
{
    const Bars &bars = SETS["btc"];
    const Values close = ta::source(bars, "close");
    const int N = bars.size();

    CAT = "indicators.test";
    {
        const Values s = ta::sma(close, 20);
        for (int i : { 19, 100, 999 }) {
            double sum = 0;
            for (int k = i - 19; k <= i; ++k) sum += close[k];
            check(near(s[i], sum / 20), QStringLiteral("SMA plain average at %1").arg(i));
        }
        check(std::isnan(s[18]), "SMA NaN at 18");
    }
    auto naiveEma = [](const Values &src, int n) {
        Values out;
        double prev = 0;
        for (int i = 0; i < src.size(); ++i) {
            if (i < n - 1) out << NaN;
            else if (i == n - 1) {
                double s = 0;
                for (int k = 0; k < n; ++k) s += src[k];
                prev = s / n;
                out << prev;
            } else {
                prev = (src[i] - prev) * (2.0 / (n + 1)) + prev;
                out << prev;
            }
        }
        return out;
    };
    {
        const Values a = ta::ema(close, 21), b = naiveEma(close, 21);
        for (int i : { 20, 21, 500, 999 }) check(near(a[i], b[i]), QStringLiteral("EMA at %1").arg(i));
    }
    {
        // Wilder's original RSI.
        const int n = 14;
        Values w = Values(N, NaN);
        double g = 0, l = 0;
        for (int i = 1; i <= n; ++i) {
            const double d = close[i] - close[i - 1];
            g += std::max(d, 0.0);
            l += std::max(-d, 0.0);
        }
        g /= n;
        l /= n;
        w[n] = 100 - 100 / (1 + g / l);
        for (int i = n + 1; i < N; ++i) {
            const double d = close[i] - close[i - 1];
            g = (g * (n - 1) + std::max(d, 0.0)) / n;
            l = (l * (n - 1) + std::max(-d, 0.0)) / n;
            w[i] = 100 - 100 / (1 + g / l);
        }
        const Values a = compute("rsi", bars, { { "length", 14 } }).values["value"];
        check(std::isnan(a[13]), "RSI NaN at 13");
        for (int i : { 14, 15, 200, 999 }) check(near(a[i], w[i]), QStringLiteral("RSI Wilder at %1").arg(i));
        bool inRange = true;
        for (int i = 14; i < N; ++i) inRange = inRange && a[i] >= 0 && a[i] <= 100;
        check(inRange, "RSI within 0..100");
    }
    {
        const Values trs = ta::tr(bars);
        Values w(N, NaN);
        double a = 0;
        for (int k = 0; k < 14; ++k) a += trs[k];
        a /= 14;
        w[13] = a;
        for (int i = 14; i < N; ++i) {
            a = (a * 13 + trs[i]) / 14;
            w[i] = a;
        }
        const Values got = ta::atr(bars, 14);
        for (int i : { 13, 14, 500, 999 }) check(near(got[i], w[i]), QStringLiteral("ATR Wilder at %1").arg(i));
    }
    {
        const IndicatorResult m = compute("macd", bars);
        const Values f = naiveEma(close, 12), s = naiveEma(close, 26);
        check(near(m.values["macd"][999], f[999] - s[999]), "MACD line");
        check(near(m.values["hist"][999], m.values["macd"][999] - m.values["signal"][999]), "MACD histogram");
    }
    {
        const IndicatorResult r = compute("bb", bars, { { "length", 20 }, { "mult", 2 } });
        double mean = 0, var = 0;
        for (int i = 980; i < 1000; ++i) mean += close[i];
        mean /= 20;
        for (int i = 980; i < 1000; ++i) var += (close[i] - mean) * (close[i] - mean);
        const double sd = std::sqrt(var / 20);
        check(near(r.values["basis"][999], mean), "BB basis");
        check(near(r.values["upper"][999], mean + 2 * sd, 1e-5), "BB upper uses population stdev");
    }
    {
        int builtins = 0;
        for (const IndicatorDefPtr &def : indicators()) {
            if (def->group == "Custom") continue;
            builtins++;
            const IndicatorResult out = compute(def->id, bars);
            if (def->id == "bigtrades" || def->id == "heatmap") continue;   // Deepwater's: drawn from trades / the order book
            for (const IndicatorOutput &o : def->outputs) {
                check(out.values.contains(o.key), def->id + "." + o.key + " missing");
                const Values v = out.values.value(o.key);
                check(v.size() >= N, def->id + "." + o.key + " too short");
                bool any = false;
                for (int i = 0; i < std::min<int>(N, v.size()); ++i) any = any || std::isfinite(v[i]);
                check(any, def->id + "." + o.key + " has no values");
            }
        }
        check(builtins == 89, QStringLiteral("%1 built-in indicators").arg(builtins));   // 87 ported + Deepwater's bigtrades, heatmap
    }
    {
        const IndicatorResult r = compute("supertrend", bars);
        const Values &dir = r.values["direction"], &up = r.values["up"];
        int flips = 0;
        for (int i = 1; i < N; ++i)
            if (dir[i] != dir[i - 1] && std::isfinite(dir[i - 1])) flips++;
        check(flips > 3, "supertrend flips");
        bool ok = true;
        for (int i = 0; i < N; ++i)
            if (std::isfinite(up[i])) ok = ok && up[i] <= bars[i].high * 1.0001;
        check(ok, "supertrend stays below the highs in an up trend");
    }
    {
        // custom indicators can be registered and computed (C++ definitions)
        IndicatorDef d;
        d.id = "custom:test";
        d.name = "Test";
        d.calc = [](const Bars &b, const QJsonObject &) {
            IndicatorResult r;
            r.values["value"] = ta::sma(ta::source(b), 3);
            return r;
        };
        const IndicatorDefPtr reg = registerIndicator(d);
        check(reg->group == "Custom" && reg->overlay && reg->outputs.size() == 1 && reg->outputs[0].label == "Test"
                  && reg->outputs[0].color == "#ff9800",
              "registerIndicator fills the JS defaults");
        const Values v = compute("custom:test", bars).values["value"];
        check(near(v[2], (close[0] + close[1] + close[2]) / 3), "custom:test computes");
        check(indicators().last()->id == "custom:test", "custom indicators come after the built-ins");
        const int count = indicators().size();
        d.name = "Test 2";
        registerIndicator(d);
        check(indicators().size() == count && indicatorById("custom:test")->name == "Test 2", "re-registering replaces in place");
        check(errorOf([] { registerIndicator(IndicatorDef {}); }) == "A custom indicator needs an id", "a custom indicator needs an id");
    }
    {
        // a malformed definition is refused, with each problem named (typed version of the JS cases)
        IndicatorDef good;
        good.name = "Ok";
        good.inputs = { IndicatorInput { "length", "", "int", 14, {}, QJsonValue(QJsonValue::Undefined), QJsonValue(QJsonValue::Undefined) } };
        IndicatorOutput out;
        out.key = "value";
        out.color = "#2962ff";
        good.outputs = { out };
        good.calc = [](const Bars &b, const QJsonObject &p) {
            IndicatorResult r;
            r.values["value"] = ta::sma(ta::source(b), p["length"].toInt());
            return r;
        };
        check(indicatorProblems(good).isEmpty(), "good typed def has no problems: " + indicatorProblems(good).join("; "));
        auto expect = [&](IndicatorDef d, const QString &re, const QString &what) {
            const QString all = indicatorProblems(d).join("\n");
            check(QRegularExpression(re).match(all).hasMatch(), what + ": got " + all);
        };
        IndicatorDef d = good;
        d.calc = nullptr;
        expect(d, "needs a `calc\\(bars, p, ta\\)` function", "no calc");
        d = good;
        d.name = "";
        expect(d, "needs a `name`", "no name");
        d = good;
        d.inputs[0].def = 2.5;
        expect(d, "input \"length\": an int input needs a whole-number default", "int default");
        d = good;
        d.inputs = { IndicatorInput { "ma", "", "select", "EMA", { "SMA" }, QJsonValue(QJsonValue::Undefined), QJsonValue(QJsonValue::Undefined) } };
        expect(d, "input \"ma\": its default must be one of its options", "select default");
        d = good;
        d.inputs = { IndicatorInput { "src", "", "source", "close", {}, QJsonValue(QJsonValue::Undefined), QJsonValue(QJsonValue::Undefined) } };
        expect(d, "a source input's key must be \"source\"", "source key");
        d = good;
        d.inputs = { IndicatorInput { "on", "", "bool", true, {}, QJsonValue(QJsonValue::Undefined), QJsonValue(QJsonValue::Undefined) } };
        expect(d, "`type` must be one of int, float, select, source, time", "bad type");
        d = good;
        d.outputs[0].kind = "area";
        expect(d, "`kind` must be \"line\" or \"histogram\"", "kind");
        d = good;
        d.outputs = { out, out };
        d.outputs[0].key = d.outputs[1].key = "a";
        d.calc = [](const Bars &b, const QJsonObject &) {
            IndicatorResult r;
            r.values["a"] = Values(b.size(), 1);
            return r;
        };
        expect(d, "output \"a\": the key is used twice", "dup output");
        d = good;
        d.calc = [](const Bars &b, const QJsonObject &) {
            IndicatorResult r;
            r.values["value"] = ta::source(b.mid(1), "close");
            return r;
        };
        expect(d, "output \"value\": calc\\(\\) returned 299 values, but there are 300 bars", "short output");
        d = good;
        d.calc = [](const Bars &, const QJsonObject &) { return IndicatorResult(); };
        expect(d, "output \"value\": calc\\(\\) returned no list for it", "no list");
        d = good;
        d.calc = [](const Bars &, const QJsonObject &) -> IndicatorResult { throw Error("boom"); };
        expect(d, "calc\\(\\) stopped with an error on test bars: boom", "throws");
        d = good;
        d.calc = [](const Bars &b, const QJsonObject &) {
            IndicatorResult r;
            r.values["value"] = ta::source(b);
            r.hasMarkers = true;
            IndicatorMarker m;
            m.time = b[0].time;
            m.position = "top";
            m.shape = "arrowUp";
            r.markers << m;
            return r;
        };
        expect(d, "markers: each needs", "markers");
        d = good;
        d.levels = { 70, NaN };
        expect(d, "`levels` must be a list of numbers", "levels");
        d = good;
        d.id = "custom:test-broken";
        d.calc = [](const Bars &, const QJsonObject &) { return IndicatorResult(); };
        const QString err = errorOf([&] { registerIndicator(d); });
        check(err.startsWith("This indicator doesn't follow the format (docs/custom-indicators.md):\n- output \"value\""), "refused: " + err);
        check(!indicatorById("custom:test-broken"), "a refused indicator is not registered");
        d = good;
        d.id = "custom:test-ok";
        check(registerIndicator(d)->id == "custom:test-ok", "registerIndicator returns the definition");
    }

    CAT = "reference.test";
    {
        // previous day high/low on intraday bars uses the whole previous day
        const double t0 = js::dateUtc(2026, 0, 5) / 1000;
        const QVector<QPair<double, double>> hl { { 10, 5 }, { 12, 7 }, { 11, 6 }, { 9, 4 }, { 20, 15 }, { 18, 16 }, { 17, 14 }, { 19, 13 }, { 30, 25 } };
        Bars six;
        for (int i = 0; i < hl.size(); ++i) {
            Bar b;
            b.time = t0 + i * 6 * 3600;
            b.open = b.low = hl[i].second;
            b.high = b.close = hl[i].first;
            b.volume = 1;
            six << b;
        }
        const IndicatorResult r = compute("pdhl", six);
        bool ok = true;
        for (int i = 0; i < 4; ++i) ok = ok && std::isnan(r.values["high"][i]);
        check(ok, "pdhl: nothing before day two");
        check((r.values["high"].mid(4, 4) == Values { 12, 12, 12, 12 }) && (r.values["low"].mid(4, 4) == Values { 4, 4, 4, 4 }), "pdhl day two");
        check(r.values["high"][8] == 20 && r.values["low"][8] == 13, "pdhl day three");
        QVector<int> idx;
        for (int i = 0; i < r.breaks.size(); ++i)
            if (r.breaks[i]) idx << i;
        check(idx == QVector<int> { 0, 4, 8 }, "pdhl breaks");
        const IndicatorResult w = compute("pdhl", six, { { "period", "week" } });
        ok = true;
        for (double x : w.values["high"]) ok = ok && std::isnan(x);
        check(ok, "pdhl: one week of bars has no previous week");
    }
    {
        // TWAP averages ohlc4 since the period began and starts over each period
        const double t0 = js::dateUtc(2026, 0, 5) / 1000;
        const QVector<double> px { 10, 20, 30, 40, 100, 200, 300, 400, 1000 }, hours { 0, 6, 12, 18, 24, 30, 36, 42, 7 * 24 };
        Bars six;
        for (int i = 0; i < px.size(); ++i) {
            Bar b;
            b.time = t0 + hours[i] * 3600;
            b.open = b.high = b.low = b.close = px[i];
            b.volume = 1;
            six << b;
        }
        auto idx = [](const IndicatorResult &r) {
            QVector<int> out;
            for (int i = 0; i < r.breaks.size(); ++i)
                if (r.breaks[i]) out << i;
            return out;
        };
        const IndicatorResult day = compute("twap", six);
        check(day.values["value"] == Values({ 10, 15, 20, 25, 100, 150, 200, 250, 1000 }), "twap day values");
        check(idx(day) == QVector<int> { 4, 8 }, "twap day breaks");
        const IndicatorResult week = compute("twap", six, { { "anchor", "week" } });
        Values wr;
        for (int i = 0; i < 8; ++i) wr << js::round(week.values["value"][i] * 1e6) / 1e6;
        check(wr == Values({ 10, 15, 20, 25, 40, 66.666667, 100, 137.5 }) && week.values["value"][8] == 1000, "twap week values");
        check(idx(week) == QVector<int> { 8 }, "twap week breaks");
        const IndicatorResult month = compute("twap", six, { { "anchor", "month" } });
        check(near(month.values["value"][8], 2100.0 / 9, 1e-7) && idx(month).isEmpty(), "twap month");
    }
    {
        // VWAP on daily bars draws a joined line, and an anchor makes it run over the week
        const double t0 = js::dateUtc(2026, 0, 5) / 1000;
        Bars daily;
        for (int i = 0; i < 6; ++i) {
            Bar b;
            b.time = t0 + i * 86400;
            b.open = b.high = b.low = b.close = 10 * (i + 1);
            b.volume = 1;
            daily << b;
        }
        const IndicatorResult day = compute("vwap", daily);
        check(day.values["value"] == Values({ 10, 20, 30, 40, 50, 60 }) && !day.breaks.contains(true), "vwap daily");
        check(compute("vwap", daily, { { "anchor", "week" } }).values["value"] == Values({ 10, 15, 20, 25, 30, 35 }), "vwap weekly anchor");
        Bars hourly;
        const QVector<double> h { 0, 6, 24, 30 };
        for (int i = 0; i < 4; ++i) {
            Bar b;
            b.time = t0 + h[i] * 3600;
            b.open = b.high = b.low = b.close = i + 1;
            b.volume = 1;
            hourly << b;
        }
        check(compute("vwap", hourly).breaks == QVector<bool>({ false, false, true, false }), "vwap hourly breaks");
    }
    {
        const Bars b = hand({ 5, 6, 9, 7, 6, 6, 8, 10, 10, 7, 7 }, { 4, 5, 8, 6, 5, 5, 7, 9, 3, 6, 6 });
        QStringList got;
        for (const IndicatorMarker &m : compute("pivotshl", b, { { "left", 2 }, { "right", 2 } }).markers)
            got << QStringLiteral("%1 %2 %3").arg(indexOfTime(b, m.time)).arg(m.position, m.text);
        checkStr("2 aboveBar 9|4 belowBar 5|7 aboveBar 10|8 belowBar 3", got.join('|'), "pivots high low, ties to the earlier bar");
        const QVector<IndicatorMarker> m = compute("pivotshl", bars).markers;
        check(m.size() > 10, "pivots on real data");
        bool ok = true;
        for (const IndicatorMarker &k : m) {
            const int c = indexOfTime(bars, k.time);
            ok = ok && c >= 10 && c + 10 < N;
            double hi = -js::Inf, lo = js::Inf;
            for (int i = std::max(0, c - 10); i < std::min(N, c + 11); ++i) {
                hi = std::max(hi, bars[i].high);
                lo = std::min(lo, bars[i].low);
            }
            ok = ok && (k.position == "aboveBar" ? hi : lo) == k.text.toDouble();
        }
        check(ok, "each pivot is the extreme of its window");
    }
    {
        const Bars b = hand({ 15, 16, 19, 17, 16, 16, 18, 18, 17, 16, 15, 14 }, { 6, 5, 4, 5, 3, 4, 5, 5, 2, 2, 4, 5 });
        QStringList got;
        for (const IndicatorMarker &m : compute("fractals", b).markers)
            got << QStringLiteral("%1 %2 %3").arg(indexOfTime(b, m.time)).arg(m.position, m.shape);
        checkStr("2 aboveBar arrowDown|4 belowBar arrowUp|7 aboveBar arrowDown|9 belowBar arrowUp", got.join('|'), "Williams fractals");
        const QVector<IndicatorMarker> real = compute("fractals", bars).markers;
        bool ok = real.size() > 50;
        for (const IndicatorMarker &m : real) ok = ok && indexOfTime(bars, m.time) <= N - 3;
        check(ok, "fractals on real data");
    }
    {
        auto z = [](const QVector<double> &p, double deviation, double depth) {
            return compute("zigzag", hand(p, p), { { "deviation", deviation }, { "depth", depth } }).values["value"];
        };
        auto same = [](const Values &got, const Values &want, const QString &what) {
            bool ok = got.size() == want.size();
            for (int i = 0; ok && i < want.size(); ++i)
                ok = std::isnan(want[i]) ? std::isnan(got[i]) : near(got[i], want[i], 1e-7);
            check(ok, "zigzag " + what);
        };
        const double n = NaN;
        same(z({ 100, 102, 105, 109, 107, 103, 97, 94, 99, 105, 111, 117, 114, 108, 103, 99, 100, 101 }, 10, 3),
             { n, n, n, 109, 105.25, 101.5, 97.75, 94, 99.75, 105.5, 111.25, 117, 112.5, 108, 103.5, 99, n, n }, "legs");
        same(z({ 100, 120, 105, 130, 100 }, 10, 3), { 100, 110, 120, 130, 100 }, "depth 3");
        same(z({ 100, 120, 105, 130, 100 }, 10, 1), { 100, 120, 105, 130, 100 }, "depth 1");
        same(z({ 100, 120, 90, 95, 100, 105 }, 10, 3), { n, n, 90, 95, 100, 105 }, "moves the low");
        same(z({ 100, 120, 90, 95, 100, 105 }, 10, 1), { 100, 120, 90, 95, 100, 105 }, "moves the low depth 1");
        same(z({ 100, 80, 110, 105, 100, 95 }, 10, 3), { n, n, 110, 105, 100, 95 }, "moves the high");
        same(z({ 100, 80, 110, 105, 100, 95 }, 10, 1), { 100, 80, 110, 105, 100, 95 }, "moves the high depth 1");
        same(z({ 100, 101, 102, 101, 100 }, 10, 3), { n, n, n, n, n }, "no swing");
    }
    {
        QVector<double> up, down;
        for (int i = 0; i < 80; ++i) {
            up << 100 + i;
            down << 200 - i;
        }
        const double limit = 2 * std::atanh(0.999);
        check(near(compute("fisher", hand(up, up)).values["fisher"][79], limit, 1e-7), "fisher settles up");
        check(near(compute("fisher", hand(down, down)).values["fisher"][79], -limit, 1e-7), "fisher settles down");
    }
    {
        const Values crsi = compute("crsi", mkBars({ 100, 110, 100, 110, 100, 110, 100, 110 }), { { "rankLength", 2 } }).values["value"];
        const Values price = compute("rsi", mkBars({ 100, 110, 100, 110, 100, 110, 100, 110 }), { { "length", 3 }, { "maLength", 0 } }).values["value"];
        const Values run = compute("rsi", mkBars({ 100, 101, 99, 101, 99, 101, 99, 101 }), { { "length", 2 }, { "maLength", 0 } }).values["value"];
        check(near(3 * crsi[5] - price[5] - run[5], 100, 1e-7), "crsi percent rank 100");
        check(near(3 * crsi[6] - price[6] - run[6], 50, 1e-7), "crsi percent rank 50");
    }
    {
        check(near(compute("rci", mkBars({ 1, 2, 2, 3 }), { { "length", 4 } }).values["value"][3], 100 * std::sqrt(0.9), 1e-7), "rci ties");
        check(near(compute("rci", mkBars({ 1, 2, 3, 4 }), { { "length", 4 } }).values["value"][3], 100, 1e-7), "rci up");
        check(near(compute("rci", mkBars({ 4, 3, 2, 1 }), { { "length", 4 } }).values["value"][3], -100, 1e-7), "rci down");
    }
    {
        const IndicatorResult r = compute("cks", bars);
        auto first = [](const Values &v) {
            for (int i = 0; i < v.size(); ++i)
                if (std::isfinite(v[i])) return i;
            return -1;
        };
        check(first(r.values["stopShort"]) == 17 && first(r.values["stopLong"]) == 17, "cks starts at bar 17");
    }
    {
        const IndicatorResult r = compute("alligator", bars);
        check(r.extra["extendBars"].toDouble() == 8, "alligator extendBars");
        check(r.values["jaw"].size() == N + 8 && r.values["teeth"].size() == N + 5 && r.values["lips"].size() == N + 3, "alligator lengths");
        const Values hl2 = ta::source(bars, "hl2");
        bool ok = true;
        for (auto [key, len, k] : { std::tuple<const char *, int, int> { "jaw", 13, 8 }, { "teeth", 8, 5 }, { "lips", 5, 3 } }) {
            const Values line = ta::rma(hl2, len);
            for (int j = 0; j < k; ++j) ok = ok && near(r.values[key][N + j], line[N - k + j], 1e-7);
        }
        check(ok, "alligator tails");
    }
    {
        const Values ma = ta::sma(close, 21), v = compute("dpo", bars, { { "centered", "On" } }).values["value"];
        bool ok = v.size() == N;
        for (int i = N - 400; i < N - 11; ++i) ok = ok && near(v[i], close[i] - ma[i + 11], 1e-7);
        for (int i = N - 11; i < N; ++i) ok = ok && std::isnan(v[i]);
        check(ok, "dpo centered");
    }
    {
        const int a = 200;
        const IndicatorResult r = compute("avwap", bars, { { "anchor", bars[a].time }, { "source", "hlc3" }, { "band1", 1 }, { "band2", 2 } });
        bool ok = true;
        for (int i = 0; i < a; ++i) ok = ok && std::isnan(r.values["value"][i]);
        auto h = [](const Bar &b) { return (b.high + b.low + b.close) / 3; };
        ok = ok && std::fabs(r.values["value"][a] - h(bars[a])) < 1e-9;
        double pv = 0, pv2 = 0, v = 0;
        for (int i = a; i < a + 60; ++i) {
            pv += h(bars[i]) * bars[i].volume;
            pv2 += h(bars[i]) * h(bars[i]) * bars[i].volume;
            v += bars[i].volume;
            const double vw = pv / v, sd = std::sqrt(std::max(0.0, pv2 / v - vw * vw));
            ok = ok && std::fabs(r.values["value"][i] - vw) < 1e-6 * vw && std::fabs(r.values["upper1"][i] - (vw + sd)) < 1e-6 * vw
                 && std::fabs(r.values["lower2"][i] - (vw - 2 * sd)) < 1e-6 * vw;
        }
        check(ok, "avwap from the anchor");
        const IndicatorResult later = compute("avwap", bars, { { "anchor", bars.last().time + 86400 }, { "band1", 1 }, { "band2", 0 } });
        ok = true;
        for (double x : later.values["value"]) ok = ok && std::isnan(x);
        check(ok, "an anchor after the last bar draws nothing");
        const IndicatorResult none = compute("avwap", bars, { { "anchor", 0 }, { "band1", 0 }, { "band2", 0 } });
        int first = -1;
        for (int i = 0; i < N && first < 0; ++i)
            if (!std::isnan(none.values["value"][i])) first = i;
        check(first == N - 150, "no anchor yet: starts 150 bars back");
    }
    {
        const IndicatorResult r = compute("markov", bars, { { "lookback", 20 }, { "threshold", 5 }, { "minHistory", 50 } });
        bool ok = true;
        for (int i = 20; i < N; ++i) {
            const double ret = bars[i].close / bars[i - 20].close - 1;
            ok = ok && r.values["state"][i] == (ret >= 0.05 ? 1 : ret <= -0.05 ? -1 : 0);
        }
        const IndicatorResult cut = compute("markov", bars.mid(0, 600), { { "lookback", 20 }, { "threshold", 5 }, { "minHistory", 50 } });
        for (int i = 0; i < 600; ++i)
            if (std::isfinite(cut.values["signal"][i])) ok = ok && std::fabs(cut.values["signal"][i] - r.values["signal"][i]) < 1e-12;
        check(ok, "markov states and no look-ahead");
    }

    CAT = "series.test";
    {
        check(parseTf("15m").seconds == 900 && parseTf("4h").seconds == 14400 && parseTf("1D").seconds == 86400, "parses timeframes");
        check(!errorOf([] { parseTf("banana"); }).isEmpty(), "parseTf banana throws");
        const qint64 t = qint64(js::dateUtc(2026, 9, 15, 13) / 1000);
        checkStr("2026-10-12T00:00:00.000Z", js::toIsoString(double(bucketStart(t, "1W")) * 1000), "weekly buckets start on Monday");
        checkStr("2026-10-01T00:00:00.000Z", js::toIsoString(double(bucketStart(t, "1M")) * 1000), "monthly buckets");
        const Bars w = resample(bars, "1W");
        const Bar first = w[1];
        Bars members;
        for (const Bar &b : bars)
            if (bucketStart(qint64(b.time), "1W") == qint64(first.time)) members << b;
        double hi = -js::Inf, vol = 0;
        for (const Bar &b : members) {
            hi = std::max(hi, b.high);
            vol += b.volume;
        }
        check(first.open == members[0].open && first.close == members.last().close && first.high == hi && std::fabs(first.volume - vol) < 1e-6,
              "resampling keeps OHLC semantics");
        const QStringList n { "1m", "5m", "15m", "1h", "4h", "1D", "1W", "1M" };
        check(nativeBase("7m", n) == "1m" && nativeBase("2h", n) == "1h" && nativeBase("3D", n) == "1D" && nativeBase("1W", n) == "1W"
                  && nativeBase("3M", n) == "1M",
              "native base timeframe");
        for (const Bars &out : { heikinAshi(bars), renko(bars), lineBreak(bars) }) {
            bool ok = out.size() > 10;
            for (int i = 1; i < out.size(); ++i) ok = ok && out[i].time > out[i - 1].time;
            check(ok, "transforms produce increasing times");
        }
        const QString csv = toCsv(bars.mid(0, 5), { { "rsi", { 1, 2, 3, NaN, 5 } } });
        check(csv.split('\n').size() == 6 && csv.split('\n')[0].endsWith("rsi"), "CSV export");
        const Bars r = rangeBars(bars, 2000);
        bool ok = r.size() > 50;
        for (int i = 0; i + 1 < r.size(); ++i) ok = ok && std::fabs(r[i].high - r[i].low - 2000) < 1e-6;
        for (int i = 1; i < r.size(); ++i) ok = ok && r[i].time > r[i - 1].time;
        check(ok, "range bars span the range");
        const Bars k = kagi(bars, 3000);
        ok = k.size() > 10;
        for (int i = 1; i < k.size(); ++i) ok = ok && k[i].time > k[i - 1].time;
        for (int i = 2; i < k.size() - 1; ++i) {
            const double a = k[i - 1].close - k[i - 2].close, b = k[i].close - k[i - 1].close;
            ok = ok && a * b < 0 && std::fabs(b) >= 3000 - 1e-6;
        }
        check(ok, "kagi turns alternate");
        const Bars p = pointFigure(bars, 1000, 3);
        ok = p.size() > 10;
        for (int i = 1; i < p.size(); ++i) ok = ok && (p[i].close > p[i].open) != (p[i - 1].close > p[i - 1].open) && p[i].time > p[i - 1].time;
        for (int i = 1; i + 1 < p.size(); ++i) ok = ok && js::round((p[i].high - p[i].low) / 1000) + 1 >= 3;
        check(ok, "point and figure columns alternate");
        const QJsonObject s = seasonals(bars);
        QJsonObject y;
        for (const QJsonValue &v : s["years"].toArray())
            if (v.toObject()["complete"].toBool()) {
                y = v.toObject();
                break;
            }
        check(!y.isEmpty() && s["years"].toArray().size() >= 3 && s["average"].toArray().size() == 366, "seasonals complete year");
    }
}

// ---------------------------------------------------------------- threads and reloading
static void threadTests()
{
    CAT = "threads";
    const QString root = QStringLiteral(QTM_ROOT_DIR);
    const Bars bars = SETS["btc"];
    const QString id = "custom:ta-tour";
    // Single-thread results first.
    const IndicatorResult want = compute(id, bars);
    const IndicatorResult wantAlert = compute("custom:alert-example", bars);
    std::atomic<int> bad { 0 }, runs { 0 };
    auto worker = [&](int seed) {
        for (int k = 0; k < 6; ++k) {
            try {
                const bool tour = (k + seed) % 2 == 0;
                const IndicatorResult r = compute(tour ? id : QStringLiteral("custom:alert-example"), bars);
                const IndicatorResult &w = tour ? want : wantAlert;
                for (auto it = w.values.begin(); it != w.values.end(); ++it) {
                    const Values &a = it.value(), &b = r.values.value(it.key());
                    bool same = a.size() == b.size();
                    for (int i = 0; same && i < a.size(); ++i) same = (std::isnan(a[i]) && std::isnan(b[i])) || a[i] == b[i];
                    if (!same) bad++;
                }
                if (r.markers.size() != w.markers.size()) bad++;
                compute("macd", bars);   // built-ins at the same time
                runs++;
            } catch (const Error &e) {
                std::cout << "thread error: " << e.message().toStdString() << "\n";
                bad++;
            }
        }
    };
    std::thread a(worker, 0), b(worker, 1), c(worker, 2);
    // Registration while the others compute.
    for (int i = 0; i < 20; ++i) {
        IndicatorDef d;
        d.id = QStringLiteral("custom:race%1").arg(i % 3);
        d.name = "Race";
        d.calc = [](const Bars &bb, const QJsonObject &) {
            IndicatorResult r;
            r.values["value"] = ta::source(bb);
            return r;
        };
        registerIndicator(d);
    }
    a.join();
    b.join();
    c.join();
    check(bad == 0 && runs == 18, QStringLiteral("compute from three threads at once: %1 mismatches, %2 runs").arg(int(bad)).arg(int(runs)));

    // How long a custom indicator takes on a long chart (informational).
    {
        Bars big;
        for (int k = 0; k < 5; ++k)
            for (Bar b : bars) {
                b.time += k * 1000 * 86400.0;
                big << b;
            }
        for (const QString &cid : { QStringLiteral("custom:alert-example"), QStringLiteral("custom:ema-ribbon-squeeze"), id, QStringLiteral("macd") }) {
            QElapsedTimer t;
            t.start();
            compute(cid, big);
            std::cout << "  " << cid.toStdString() << " on " << big.size() << " bars: " << t.nsecsElapsed() / 1000 << " us\n";
        }
    }

    // Re-import when the file changes, on every thread.
    QTemporaryDir dir;
    const QString path = dir.filePath("reload.js");
    auto write = [&](const QString &code) {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) fail("cannot write " + path);
        f.write(code.toUtf8());
        f.close();
    };
    const QString tmpl = "export default { name: 'Reload', calc(bars) { return { value: bars.map(() => %1) }; } };\n";
    write(tmpl.arg(1));
    loadCustomIndicator(path, "custom:reload");
    check(compute("custom:reload", bars).values["value"][0] == 1, "first version");
    double other = 0;
    std::thread t1([&] { other = compute("custom:reload", bars).values["value"][0]; });
    t1.join();
    check(other == 1, "first version on another thread");
    QThread::msleep(20);
    write(tmpl.arg(22));
    QFile(path).setFileTime(QDateTime::currentDateTime().addSecs(5), QFileDevice::FileModificationTime);
    check(compute("custom:reload", bars).values["value"][0] == 22, "re-imported after the file changed");
    std::thread t2([&] { other = compute("custom:reload", bars).values["value"][0]; });
    t2.join();
    check(other == 22, "re-imported on another thread too");
    // A syntax error: refused with the error Node would give.
    write("export default {\n  name: 'x',\n  calc(bars) { return { value: bars.map(b => b.close) ; },\n};\n");
    QFile(path).setFileTime(QDateTime::currentDateTime().addSecs(10), QFileDevice::FileModificationTime);
    checkStr("Unexpected token ';'", errorOf([&] { loadCustomIndicator(path, "custom:reload"); }), "syntax error text");
    // Refused, so the registered version keeps working, as in Node.
    check(compute("custom:reload", bars).values["value"][0] == 22, "a broken save keeps the loaded version");
    checkStr("Cannot find module '" + dir.filePath("missing.js") + "'", errorOf([&] { loadCustomIndicator(dir.filePath("missing.js"), "custom:missing"); }),
             "missing file");
    // A runtime error inside calc reads like V8's.
    write("export default { name: 'Boom', calc(bars) { const o = undefined; return { value: bars.map(() => o.x) }; } };\n");
    QFile(path).setFileTime(QDateTime::currentDateTime().addSecs(15), QFileDevice::FileModificationTime);
    checkStr("This indicator doesn't follow the format (docs/custom-indicators.md):\n- calc() stopped with an error on test bars: Cannot read "
             "properties of undefined (reading 'x')",
             errorOf([&] { loadCustomIndicator(path, "custom:boom"); }), "runtime error text");
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QElapsedTimer timer;
    timer.start();
    QFile f(QStringLiteral(QTM_REF_DIR "/reference.json"));
    if (!f.open(QIODevice::ReadOnly)) {
        std::cout << "missing " << f.fileName().toStdString() << ": run node tests/quant/core/make_reference.mjs\n";
        return 2;
    }
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &pe);
    if (doc.isNull()) {
        std::cout << "bad reference.json: " << pe.errorString().toStdString() << "\n";
        return 2;
    }
    std::cout << "reference loaded in " << timer.elapsed() << " ms\n";
    checkReference(doc.object());
    portedTests();
    threadTests();

    std::cout << "\nchecks by area:\n";
    for (auto it = S.checksBy.begin(); it != S.checksBy.end(); ++it)
        std::cout << "  " << it.key().toStdString() << ": " << it.value() << " checks, " << S.failsBy.value(it.key()) << " failed\n";
    if (!S.nearBy.isEmpty()) {
        std::cout << "within 1e-9 but not bit-exact, by indicator / function:\n";
        for (auto it = S.nearBy.begin(); it != S.nearBy.end(); ++it) std::cout << "  " << it.key().toStdString() << ": " << it.value() << "\n";
    }
    std::cout << "\n" << S.checks << " checks: " << S.exact << " exact, " << S.near << " within 1e-9, " << S.fails << " failed ("
              << timer.elapsed() << " ms)\n";
    return S.fails ? 1 : 0;
}
