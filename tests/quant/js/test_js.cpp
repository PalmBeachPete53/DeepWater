// qtm_test_js: modern JavaScript in custom indicators, checked against Node (reference/reference.json,
// written by make_reference.mjs).
//
//  1. The probe: every ES2015..ES2025 feature of features.mjs run on a bare QJSEngine, to list what
//     V4 lacks on its own (informational).
//  2. The same probes through the custom indicator loader (rewrites, polyfills): each must give
//     what Node gives, except the features refused on purpose (with their message) and the known
//     gaps listed below.
//  3. The custom indicators in indicators/: definitions and every compute, bit for bit.
//  4. Error messages (syntax errors next to modern syntax, runtime errors), as Node words them.
//  5. Rewritten code keeps its line structure; computing from several threads at once.
#include "core/CustomIndicators.h"
#include "core/Error.h"
#include "core/Indicators.h"
#include "core/JsTransform.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJSEngine>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QRegularExpression>
#include <QTemporaryDir>

#include <atomic>
#include <cstring>
#include <functional>
#include <iostream>
#include <thread>

using namespace qtm;

namespace {

// ---------------------------------------------------------------- bookkeeping (as in test_core.cpp)
struct Stats {
    qint64 checks = 0, fails = 0;
    QMap<QString, qint64> checksBy;
    QMap<QString, int> failsBy;
} S;
QString CAT = "misc";

void check(bool ok, const QString &msg)
{
    S.checks++;
    S.checksBy[CAT]++;
    if (ok) return;
    S.fails++;
    if (S.failsBy[CAT]++ < 15) std::cout << "FAIL [" << CAT.toStdString() << "] " << msg.toStdString() << "\n";
}
void checkStr(const QString &want, const QString &got, const QString &ctx)
{
    check(want == got, QStringLiteral("%1: want \"%2\", got \"%3\"").arg(ctx, want, got));
}
QString num(double v)
{
    return QString::number(v, 'g', 17);
}
// Bit for bit, NaN equal to NaN.
void checkNum(double want, double got, const QString &ctx)
{
    const bool same = (std::isnan(want) && std::isnan(got)) || std::memcmp(&want, &got, sizeof want) == 0;
    check(same, QStringLiteral("%1: want %2, got %3").arg(ctx, num(want), num(got)));
}
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
    const Values t = f64(o["time"]), op = f64(o["open"]), h = f64(o["high"]), l = f64(o["low"]), c = f64(o["close"]), v = f64(o["volume"]);
    Bars out(t.size());
    for (int i = 0; i < t.size(); ++i) {
        out[i].time = t[i];
        out[i].open = op[i];
        out[i].high = h[i];
        out[i].low = l[i];
        out[i].close = c[i];
        out[i].volume = v[i];
    }
    return out;
}
QString compact(const QJsonValue &v)
{
    return QString::fromUtf8(QJsonDocument(QJsonArray { v }).toJson(QJsonDocument::Compact));
}
// Deep comparison; NaN in a C++ result is null in Node's JSON.
void checkJson(const QJsonValue &want, const QJsonValue &got, const QString &ctx)
{
    if (want.isString() && want.toString().startsWith(QLatin1String("f64:"))) return checkNum(jnum(want), jnum(got), ctx);
    if (want.isNull()) return check(got.isNull() || (got.isDouble() && std::isnan(got.toDouble())), ctx + ": want null, got " + compact(got));
    if (want.isDouble()) {
        if (!got.isDouble()) return check(false, ctx + ": want a number, got " + compact(got));
        return checkNum(want.toDouble(), got.toDouble(), ctx);
    }
    if (want.isArray()) {
        const QJsonArray a = want.toArray(), b = got.toArray();
        if (!got.isArray() || a.size() != b.size()) return check(false, ctx + ": want " + compact(want) + ", got " + compact(got));
        for (int i = 0; i < a.size(); ++i) checkJson(a[i], b[i], QStringLiteral("%1[%2]").arg(ctx).arg(i));
        return;
    }
    if (want.isObject()) {
        if (!got.isObject()) return check(false, ctx + ": want an object, got " + compact(got));
        const QJsonObject a = want.toObject(), b = got.toObject();
        check(a.keys() == b.keys(), QStringLiteral("%1: keys %2 vs %3").arg(ctx, a.keys().join(','), b.keys().join(',')));
        for (const QString &k : a.keys()) checkJson(a[k], b.value(k), ctx + "." + k);
        return;
    }
    check(want == got, ctx + ": want " + compact(want) + ", got " + compact(got));
}
void checkResult(const QJsonObject &want, const IndicatorResult &got, const QString &ctx)
{
    const QJsonObject wv = want["values"].toObject();
    QStringList gk = got.values.keys();
    std::sort(gk.begin(), gk.end());
    check(wv.keys() == gk, QStringLiteral("%1: value keys %2 vs %3").arg(ctx, wv.keys().join(','), gk.join(',')));
    for (const QString &k : wv.keys()) {
        const Values a = f64(wv[k]), b = got.values.value(k);
        if (a.size() != b.size()) {
            check(false, QStringLiteral("%1.%2: %3 values, got %4").arg(ctx, k).arg(a.size()).arg(b.size()));
            continue;
        }
        for (int i = 0; i < a.size(); ++i) checkNum(a[i], b[i], QStringLiteral("%1.%2[%3]").arg(ctx, k).arg(i));
    }
    check(want.contains("markers") == got.hasMarkers, ctx + ": markers present differs");
    const QJsonArray wm = want["markers"].toArray();
    if (wm.size() != got.markers.size()) check(false, QStringLiteral("%1: %2 markers, got %3").arg(ctx).arg(wm.size()).arg(got.markers.size()));
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
    QStringList wc;
    for (const QJsonValue &c : want["colors"].toArray()) wc << c.toString();
    check(wc == got.colors, ctx + ": colors differ");
    checkJson(want["extra"], got.extra, ctx + ".extra");
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
QString readFile(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
}

const QString ROOT = QStringLiteral(QTM_ROOT_DIR);
QMap<QString, Bars> SETS;

QString refused(const QString &what, int line)
{
    return QStringLiteral("!! %1 is not supported by this app's JavaScript engine (line %2)").arg(what).arg(line);
}

// Features refused on purpose: no faithful emulation exists. The loader says what and where.
const QMap<QString, QString> REFUSED {
    { "BigInt", refused("BigInt (10n)", 2) },
    { "top-level await", refused("Top-level await", 1) },
    { "regex lookbehind", refused("Regular expression lookbehind ((?<= ...) and (?<! ...))", 2) },
    { "regex unicode property escapes", refused("Unicode property escapes in regular expressions (\\p{...})", 2) },
    { "regex match indices (d flag)", refused("The regular expression flag 'd'", 2) },
    { "regex v flag", refused("The regular expression flag 'v'", 2) },
    { "regex modifiers and duplicate named groups", refused("Regular expression modifiers ((?i: ...))", 2) },
};
// Built-ins V4 lacks that can't be added in JavaScript (they need engine support); the probe
// checks that they are still missing, so this list stays true.
const QMap<QString, QString> GAPS {
    { "SharedArrayBuffer and Atomics", "shared memory: V4's typed arrays don't accept a SharedArrayBuffer" },
    { "ArrayBuffer resize and transfer", "resizable and detachable buffers need engine support" },
    { "Atomics.waitAsync", "no shared memory" },
    { "Float16Array and Math.f16round", "a new typed array kind needs engine support (Math.f16round alone is there)" },
    { "Intl.NumberFormat and toLocaleString", "no Intl (ICU) in V4; toLocaleString formats like toString" },
};

// ---------------------------------------------------------------- 1. the probe
void rawProbe(const QJsonArray &features)
{
    CAT = "probe";
    QString ser = readFile(ROOT + "/tests/quant/js/reference/features/ser.js");
    ser.remove(QStringLiteral("export { ser };"));
    QStringList lacking;
    int same = 0;
    for (const QJsonValue &v : features) {
        const QJsonObject f = v.toObject();
        QJSEngine e;
        const QJSValue serFn = e.evaluate("(function () {" + ser + "; return ser; })()");
        QJSValue box = e.newObject();
        box.setProperty("r", "pending");
        const QJSValue m = e.importModule(ROOT + "/" + f["probe"].toString());
        QString got;
        if (m.isError()) {
            got = "!! " + m.property("message").toString();
        } else {
            e.evaluate("(function (d, box, ser) { if (d && typeof d.then === 'function') d.then(function (v) { box.r = ser(v); }, "
                       "function (x) { box.r = '!! rejected ' + ser(x); }); else box.r = ser(d); })")
                .call({ m.property("default"), box, serFn });
            QElapsedTimer t;
            t.start();
            while (box.property("r").toString() == QLatin1String("pending") && t.elapsed() < 300) QCoreApplication::processEvents();
            got = box.property("r").toString();
        }
        if (got == f["want"].toString()) same++;
        else lacking << QStringLiteral("ES%1 %2").arg(f["es"].toInt() ? QString::number(f["es"].toInt()) : QStringLiteral("-"), f["name"].toString());
    }
    std::cout << "probe: a bare QJSEngine gives what Node gives for " << same << " of " << features.size() << " features; it lacks or gets wrong:\n";
    for (const QString &l : lacking) std::cout << "  " << l.toStdString() << "\n";
    check(same < features.size(), "the probe found nothing missing: is the engine new?");
}

// ---------------------------------------------------------------- 2. probes through the loader
void loaderProbes(const QJsonArray &features)
{
    CAT = "features";
    int same = 0, refusedOk = 0, gaps = 0;
    for (int i = 0; i < features.size(); ++i) {
        const QJsonObject f = features[i].toObject();
        const QString name = f["name"].toString(), id = QStringLiteral("custom:wrap%1").arg(i);
        QString got;
        const QString err = errorOf([&] { loadCustomIndicator(ROOT + "/" + f["wrap"].toString(), id); });
        if (!err.isEmpty()) {
            got = "!! " + err;
        } else {
            QElapsedTimer t;
            t.start();
            for (;;) {
                got = compute(id, SETS["short"], {}).extra.value("probe").toString();
                if (got != QLatin1String("pending") || t.elapsed() > 1000) break;
                QCoreApplication::processEvents();
            }
        }
        const QString want = f["want"].toString();
        if (REFUSED.contains(name)) {
            checkStr(REFUSED[name], got, name + " (refused)");
            refusedOk += got == REFUSED[name];
        } else if (GAPS.contains(name)) {
            check(got != want, name + ": listed as a gap but works now");
            gaps++;
        } else {
            checkStr(want, got, name);
            same += got == want;
        }
    }
    std::cout << "loader: " << same << " features give what Node gives, " << refusedOk << " refused with a message, " << gaps
              << " known gaps, of " << features.size() << "\n";
}

// ---------------------------------------------------------------- 3. indicators
void indicatorFiles(const QJsonArray &indicators)
{
    CAT = "indicators";
    for (const QJsonValue &v : indicators) {
        const QJsonObject ind = v.toObject();
        const QString file = ind["file"].toString(), id = "custom:" + QString(file).remove(QRegularExpression("\\.js$"));
        const QString err = errorOf([&] { loadCustomIndicator(ROOT + "/" + ind["path"].toString(), id); });
        checkStr(ind["error"].toString(), err, file + " load");
        if (!err.isEmpty()) continue;
        const IndicatorDefPtr d = indicatorById(id);
        const QJsonObject wd = ind["def"].toObject();
        checkStr(wd["name"].toString(), d->name, file + " name");
        QStringList outs;
        for (const IndicatorOutput &o : d->outputs) outs << o.key;
        QStringList wantOuts;
        for (const QJsonValue &o : wd["outputs"].toArray()) wantOuts << o.toString();
        check(outs == wantOuts, file + " outputs");
        for (const QJsonValue &cv : ind["computes"].toArray()) {
            const QJsonObject c = cv.toObject();
            const QJsonObject params = c["params"].toObject();
            const QString ctx = QStringLiteral("%1/%2 %3").arg(file, c["set"].toString(), QString::fromUtf8(QJsonDocument(params).toJson(QJsonDocument::Compact)));
            try {
                const IndicatorResult r = compute(id, SETS[c["set"].toString()], params);
                if (c.contains("error")) check(false, ctx + ": expected error " + c["error"].toString());
                else checkResult(c["result"].toObject(), r, ctx);
            } catch (const Error &e) {
                if (c.contains("error")) checkStr(c["error"].toString(), e.message(), ctx + " error");
                else check(false, ctx + ": threw " + e.message());
            }
        }
        std::cout << "  " << file.toStdString() << ": " << ind["computes"].toArray().size() << " computes\n";
    }
}

// ---------------------------------------------------------------- 4. errors
void errorFiles(const QJsonArray &errors)
{
    CAT = "errors";
    for (const QJsonValue &v : errors) {
        const QJsonObject e = v.toObject();
        const QString file = e["file"].toString(), id = "custom:" + QString(file).remove(QRegularExpression("\\.js$"));
        checkStr(e["error"].toString(), errorOf([&] { loadCustomIndicator(ROOT + "/" + e["path"].toString(), id); }), file);
    }
}

// ---------------------------------------------------------------- 5. lines, threads, reloads
void lineStructure(const QJsonObject &ref)
{
    CAT = "lines";
    QStringList files;
    for (const QJsonValue &v : ref["features"].toArray()) files << v.toObject()["probe"].toString();
    for (const QJsonValue &v : ref["indicators"].toArray()) files << v.toObject()["path"].toString();
    files << "tests/quant/js/indicators/lib/format.js";
    int changed = 0;
    for (const QString &f : files) {
        const QString src = readFile(ROOT + "/" + f);
        const js::Transformed t = js::transformModule(src, "file://" + ROOT + "/" + f);
        check(t.parsed, f + ": parsed");
        changed += t.changed;
        check(src.count(QLatin1Char('\n')) == t.code.count(QLatin1Char('\n')), f + ": line count kept");
    }
    // Errors point at the user's lines: lineNumber of errors made after and inside rewritten code.
    QTemporaryDir dir;
    const QString path = dir.filePath("lines.js");
    QFile out(path);
    if (!out.open(QIODevice::WriteOnly))
        qFatal("cannot write %s", qPrintable(path));
    out.write("class A {\n"                                              // 1
              "  #x = 1;\n"                                              // 2
              "  static s = { ...{ a: 1 } };\n"                          // 3
              "  static { this.t = 2; }\n"                               // 4
              "  get x() { return this.#x; }\n"                          // 5
              "}\n"                                                      // 6
              "const { a, ...rest } = { a: 1, b: 2 };\n"                 // 7
              "let q = null; q ?\?= 1;\n"                                 // 8
              "const f = async ({ k, ...r }, [m] = [1]) => {\n"          // 9
              "  await null;\n"                                          // 10
              "  return r;\n"                                            // 11
              "};\n"                                                     // 12
              "const o = {\n"                                            // 13
              "  ...rest,\n"                                             // 14
              "  z: 1,\n"                                                // 15
              "};\n"                                                     // 16
              "const lines = [new Error().lineNumber];\n"                // 17
              "function g({ p, ...others }, { u }) {\n"                  // 18
              "  return new Error().lineNumber;\n"                       // 19
              "}\n"                                                      // 20
              "export default { name: 'L', calc(bars) {\n"               // 21
              "  return { value: bars.map(() => 0), info: [lines[0], g({ p: 1 }, { u: 2 }), new Error().lineNumber, new A().x] };\n"   // 22
              "} };\n");
    out.close();
    Values info;
    const QString err = errorOf([&] {
        loadCustomIndicator(path, "custom:lines");
        info = compute("custom:lines", SETS["short"], {}).values.value("info");
    });
    checkStr(QString(), err, "lines.js loads");
    QStringList got;
    for (double v : info) got << num(v);
    checkStr("17,19,22,1", got.join(','), "error line numbers after rewritten code");
    std::cout << "lines: " << changed << " of " << files.size() << " files rewritten, line structure kept\n";
    // A file that needs nothing is left alone; a syntax error is left to the engine.
    const js::Transformed plain = js::transformModule("export default { name: 'x', calc: (b) => ({ value: b.map((x) => x.close) }) };\n", "");
    check(plain.parsed && !plain.changed, "plain file untouched");
    const js::Transformed bad = js::transformModule("const a = { ...b };\nconst c = ;\n", "");
    check(!bad.parsed && bad.errorOffset == 30, QStringLiteral("syntax error offset %1").arg(bad.errorOffset));
    checkStr("Unexpected token ';'", js::syntaxErrorAt("const c = ;", 10), "syntaxErrorAt");
}

void threads(const QJsonArray &indicators)
{
    CAT = "threads";
    const Bars bars = SETS["btc"];
    QStringList ids;
    QMap<QString, IndicatorResult> want;
    for (const QJsonValue &v : indicators) {
        const QString id = "custom:" + v.toObject()["file"].toString().remove(QRegularExpression("\\.js$"));
        ids << id;
        want[id] = compute(id, bars);
    }
    std::atomic<int> bad { 0 }, runs { 0 };
    auto worker = [&](int seed) {
        for (int k = 0; k < 8; ++k) {
            const QString id = ids[(k + seed) % ids.size()];
            try {
                const IndicatorResult r = compute(id, bars);
                const IndicatorResult &w = want[id];
                for (auto it = w.values.begin(); it != w.values.end(); ++it) {
                    const Values &a = it.value(), &b = r.values.value(it.key());
                    bool same = a.size() == b.size();
                    for (int i = 0; same && i < a.size(); ++i) same = (std::isnan(a[i]) && std::isnan(b[i])) || a[i] == b[i];
                    if (!same) bad++;
                }
                if (r.markers.size() != w.markers.size() || r.extra != w.extra) bad++;
                runs++;
            } catch (const Error &e) {
                std::cout << "thread error: " << e.message().toStdString() << "\n";
                bad++;
            }
        }
    };
    std::vector<std::thread> pool;
    for (int t = 0; t < 4; ++t) pool.emplace_back(worker, t);
    for (auto &t : pool) t.join();
    check(bad == 0 && runs == 32, QStringLiteral("compute from four threads at once: %1 mismatches, %2 runs").arg(int(bad)).arg(int(runs)));
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QElapsedTimer timer;
    timer.start();
    const QByteArray raw = readFile(ROOT + "/tests/quant/js/reference/reference.json").toUtf8();
    const QJsonObject ref = QJsonDocument::fromJson(raw).object();
    if (ref.isEmpty()) {
        std::cout << "missing reference: run node tests/quant/js/make_reference.mjs\n";
        return 2;
    }
    const QJsonObject sets = ref["sets"].toObject();
    for (auto it = sets.begin(); it != sets.end(); ++it) SETS[it.key()] = barsFrom(it.value().toObject());

    rawProbe(ref["features"].toArray());
    loaderProbes(ref["features"].toArray());
    indicatorFiles(ref["indicators"].toArray());
    errorFiles(ref["errors"].toArray());
    lineStructure(ref);
    threads(ref["indicators"].toArray());

    std::cout << "\nchecks by area:\n";
    for (auto it = S.checksBy.begin(); it != S.checksBy.end(); ++it)
        std::cout << "  " << it.key().toStdString() << ": " << it.value() << " checks, " << S.failsBy.value(it.key()) << " failed\n";
    std::cout << "\n" << S.checks << " checks, " << S.fails << " failed (" << timer.elapsed() << " ms)\n";
    return S.fails ? 1 : 0;
}
