// qtm_test_pinets: Pine Script through PineTS (third_party/pinets) on Node.js.
//
//  1. The bridge starts, answers, and reports its versions.
//  2. transpile(): the JavaScript PineTS runs for a script.
//  3. run(): SMA, EMA, RSI, ATR and crossovers of a script against the C++ ta.* port, input
//     overrides, plot metadata (styles, colours, per-bar colours, marks, hlines), warnings.
//  4. loadPineIndicator(): a .pine file as a chart indicator (definition, compute, params, a file
//     edit picked up by the next compute), and compute from several threads at once.
//  5. Errors: syntax errors, unknown functions, missing files, scripts without plots.
#include "core/CustomIndicators.h"
#include "core/Error.h"
#include "core/Indicators.h"
#include "core/PineIndicators.h"
#include "core/PineScript.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QThread>

#include <atomic>
#include <cmath>
#include <functional>
#include <iostream>
#include <thread>

using namespace qtm;

namespace {

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
    S.failsBy[CAT]++;
    if (S.failsBy[CAT] <= 12) std::cerr << "  FAIL [" << CAT.toStdString() << "] " << msg.toStdString() << "\n";
}

const QString ROOT = QStringLiteral(QTM_ROOT_DIR);

Bars loadBars(const QString &path, int n = 0)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    const QJsonArray a = QJsonDocument::fromJson(f.readAll()).array();
    Bars out;
    for (const QJsonValue &v : a) {
        const QJsonObject o = v.toObject();
        Bar b;
        b.time = o["time"].toDouble();
        b.open = o["open"].toDouble();
        b.high = o["high"].toDouble();
        b.low = o["low"].toDouble();
        b.close = o["close"].toDouble();
        b.volume = o["volume"].toDouble();
        out.push_back(b);
        if (n && out.size() >= n) break;
    }
    return out;
}

QString readFile(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
}

// Same numbers, allowing for the rounding PineTS applies and the different summation orders:
// from `from` on (indicators warm up differently), within 1e-7 of the value, NaN only where both are.
int sameSeries(const Values &a, const Values &b, int from, const QString &what, double tol = 1e-7)
{
    check(a.size() == b.size(), QStringLiteral("%1: %2 values vs %3").arg(what).arg(a.size()).arg(b.size()));
    int bad = 0;
    for (int i = from; i < std::min(a.size(), b.size()); ++i) {
        const bool na = std::isnan(a[i]), nb = std::isnan(b[i]);
        if (na != nb || (!na && std::abs(a[i] - b[i]) > tol * std::max(1.0, std::abs(b[i])))) {
            if (bad++ < 3) check(false, QStringLiteral("%1[%2]: %3 vs %4").arg(what).arg(i).arg(a[i]).arg(b[i]));
        }
    }
    check(bad == 0, QStringLiteral("%1: %2 bars differ").arg(what).arg(bad));
    return bad;
}

const pine::Plot *plotNamed(const pine::Result &r, const QString &key)
{
    for (const pine::Plot &p : r.plots)
        if (p.key == key) return &p;
    return nullptr;
}

// ---------------------------------------------------------------- 1. the bridge
bool bridge()
{
    CAT = "bridge";
    QString why;
    const bool ok = pine::available(&why);
    if (!ok) {
        std::cout << "SKIPPED: " << why.toStdString() << "\n";
        return false;
    }
    std::cout << "  node " << pine::nodePath().toStdString() << " " << pine::nodeVersion().toStdString() << ", PineTS "
              << pine::pinetsVersion().toStdString() << ", runtime " << pine::runtimeDir().toStdString() << "\n";
    check(pine::nodeVersion().startsWith('v'), "node version");
    check(QFile::exists(pine::runtimeDir() + "/pinets.min.es.js") && QFile::exists(pine::runtimeDir() + "/deepwater-pinets.mjs"),
          "runtime files installed");
    return true;
}

// ---------------------------------------------------------------- 2. transpile
void transpile()
{
    CAT = "transpile";
    const QString src = readFile(ROOT + "/tests/quant/fixtures/pine/demo-ma.pine");
    check(!src.isEmpty(), "fixture read");
    QElapsedTimer t;
    t.start();
    const QString js = pine::transpile(src);
    std::cout << "  transpile: " << t.elapsed() << " ms, " << js.size() << " chars\n";
    check(js.startsWith("async $ =>"), "transpiled code is the PineTS function: " + js.left(40));
    check(js.contains("ta.sma(") && js.contains("ta.ema(") && js.contains("indicator("), "transpiled code calls the Pine API");
    check(js.contains("'Demo MA'"), "transpiled code keeps the title");
    check(js.count('\n') > 20, "transpiled code is formatted on lines");
}

// ---------------------------------------------------------------- 3. run
void runScript(const Bars &bars)
{
    CAT = "run";
    const QString src = readFile(ROOT + "/tests/quant/fixtures/pine/demo-ma.pine");
    QElapsedTimer t;
    t.start();
    const pine::Result r = pine::run(src, bars);
    std::cout << "  run: " << bars.size() << " bars in " << t.elapsed() << " ms, " << r.plots.size() << " plots\n";
    check(r.declaration.type == "indicator", "declaration type: " + r.declaration.type);
    check(r.declaration.version == 6, QStringLiteral("version 6: %1").arg(r.declaration.version));
    check(r.declaration.title == "Demo MA" && r.declaration.shorttitle == "DMA" && r.declaration.overlay, "declaration title/overlay");
    check(r.warnings.isEmpty(), "no warnings: " + r.warnings.join("; "));

    // inputs as scanned
    check(r.inputs.size() == 4, QStringLiteral("4 inputs: %1").arg(r.inputs.size()));
    if (r.inputs.size() == 4) {
        check(r.inputs[0].id == "in_0" && r.inputs[0].varId == "len" && r.inputs[0].type == "int" && r.inputs[0].defval.toDouble() == 20
                  && r.inputs[0].minval.toDouble() == 1 && r.inputs[0].maxval.toDouble() == 500,
              "input Length");
        check(r.inputs[1].type == "source" && r.inputs[1].defval.toString() == "close" && r.inputs[1].options.size() == 8, "input Source");
        check(r.inputs[2].type == "bool" && r.inputs[2].defval.toBool() == false, "input Use EMA");
        check(r.inputs[3].type == "string" && r.inputs[3].options.size() == 2 && r.inputs[3].step.isUndefined(), "input Mode");
    }

    // plots against the C++ port
    const Values close = ta::source(bars, "close");
    const pine::Plot *ma = plotNamed(r, "MA");
    check(ma != nullptr, "plot MA");
    if (ma) {
        check(ma->style == "line" && ma->color == "#2962FF" && ma->linewidth == 2 && ma->overlay && !ma->isMarks(), "MA metadata");
        sameSeries(ma->values, ta::sma(close, 20), 0, "sma(20)");
        check(ma->colors.isEmpty(), "MA has one colour");
    }
    const pine::Plot *rsi = plotNamed(r, "RSI");
    check(rsi != nullptr, "plot RSI");
    if (rsi) {
        check(rsi->style == "histogram", "RSI style: " + rsi->style);
        sameSeries(rsi->values, ta::rsi(close, 14), 100, "rsi(14)", 1e-6);
        check(rsi->colors.size() == bars.size(), QStringLiteral("RSI per-bar colours: %1").arg(rsi->colors.size()));
        if (rsi->colors.size() == bars.size()) {
            int bad = 0;
            for (int i = 0; i < bars.size(); ++i)
                if (rsi->colors[i] != (bars[i].close > bars[i].open ? "#4CAF50" : "#F23645")) bad++;
            check(bad == 0, QStringLiteral("RSI colours follow close > open (%1 differ)").arg(bad));
        }
    }
    const pine::Plot *up = plotNamed(r, "Cross up");
    check(up != nullptr, "plot Cross up");
    if (up) {
        check(up->isMarks() && up->style == "shape" && up->location == "BelowBar" && up->shape == "shape_triangle_up"
                  && up->color == "#4CAF50" && up->text == "up",
              "Cross up metadata");
        const QVector<bool> want = ta::crossover(close, ta::sma(close, 20));
        int bad = 0, count = 0;
        for (int i = 0; i < bars.size(); ++i) {
            if (up->flags.value(i) != want[i]) bad++;
            if (want[i]) count++;
        }
        check(bad == 0 && count > 5, QStringLiteral("Cross up flags match ta.crossover (%1 differ, %2 crosses)").arg(bad).arg(count));
    }
    const pine::Plot *down = plotNamed(r, "Cross down");
    check(down && down->style == "char" && down->ch == "x" && down->location == "AboveBar", "Cross down metadata");
    const pine::Plot *mid = plotNamed(r, "Mid");
    check(mid && mid->style == "hline" && mid->values.size() == bars.size() && mid->values[0] == 50, "hline Mid");

    // input overrides: by id, by variable, by title
    const pine::Result r5 = pine::run(src, bars, QJsonObject { { "in_0", 5 } });
    const pine::Plot *ma5 = plotNamed(r5, "MA");
    if (ma5) sameSeries(ma5->values, ta::sma(close, 5), 0, "sma(5) by id");
    const pine::Result rEma = pine::run(src, bars, QJsonObject { { "len", 10 }, { "Use EMA", true } });
    if (const pine::Plot *p = plotNamed(rEma, "MA")) sameSeries(p->values, ta::ema(close, 10), 60, "ema(10) by variable + title", 1e-6);
    const pine::Result rSrc = pine::run(src, bars, QJsonObject { { "Source", "hl2" } });
    if (const pine::Plot *p = plotNamed(rSrc, "MA")) sameSeries(p->values, ta::sma(ta::source(bars, "hl2"), 20), 0, "sma on hl2");

    // more of ta.*: ATR, highest/lowest, stdev, macd
    const pine::Result r2 = pine::run(
        "//@version=6\nindicator(\"More\")\nplot(ta.atr(14), \"ATR\")\nplot(ta.highest(high, 50), \"HH\")\nplot(ta.lowest(low, 50), \"LL\")\n"
        "plot(ta.stdev(close, 20), \"SD\")\n[m, s, h] = ta.macd(close, 12, 26, 9)\nplot(m, \"MACD\")\nplot(ta.wma(close, 9), \"WMA\")\n",
        bars);
    check(r2.plots.size() == 6, QStringLiteral("6 plots: %1").arg(r2.plots.size()));
    check(!r2.declaration.overlay && r2.declaration.title == "More", "declaration More");
    if (const pine::Plot *p = plotNamed(r2, "ATR")) sameSeries(p->values, ta::atr(bars, 14), 100, "atr(14)", 1e-6);
    if (const pine::Plot *p = plotNamed(r2, "HH")) sameSeries(p->values, ta::highest(ta::source(bars, "high"), 50), 0, "highest(50)");
    if (const pine::Plot *p = plotNamed(r2, "LL")) sameSeries(p->values, ta::lowest(ta::source(bars, "low"), 50), 0, "lowest(50)");
    if (const pine::Plot *p = plotNamed(r2, "SD")) sameSeries(p->values, ta::stdev(close, 20), 0, "stdev(20)", 1e-6);
    if (const pine::Plot *p = plotNamed(r2, "WMA")) sameSeries(p->values, ta::wma(close, 9), 0, "wma(9)");
    if (const pine::Plot *p = plotNamed(r2, "MACD")) {
        const Values macd = [&] {
            Values out(bars.size());
            const Values f = ta::ema(close, 12), s = ta::ema(close, 26);
            for (int i = 0; i < bars.size(); ++i) out[i] = f[i] - s[i];
            return out;
        }();
        sameSeries(p->values, macd, 150, "macd line", 1e-6);
    }

    // timing on 5000 bars
    Bars big;
    for (int i = 0; i < 5000; ++i) {
        Bar b = bars[i % bars.size()];
        b.time = 1500000000 + i * 86400.0;
        big.push_back(b);
    }
    t.restart();
    const pine::Result rb = pine::run(src, big);
    std::cout << "  run: 5000 bars in " << t.elapsed() << " ms\n";
    check(rb.plots.size() == r.plots.size() && plotNamed(rb, "MA")->values.size() == 5000, "5000 bars");

    // no bars
    const pine::Result r0 = pine::run(src, Bars());
    check(r0.plots.isEmpty() && r0.declaration.type == "indicator", "no bars: no plots");
}

// ---------------------------------------------------------------- 4. the chart indicator
void indicator(const Bars &bars)
{
    CAT = "indicator";
    QTemporaryDir dir;
    const QString file = dir.path() + "/demo-ma.pine";
    QFile::copy(ROOT + "/tests/quant/fixtures/pine/demo-ma.pine", file);
    check(isCustomIndicatorFile("a.pine") && isCustomIndicatorFile("a.js") && !isCustomIndicatorFile("a.txt"), "isCustomIndicatorFile");
    check(customIndicatorBaseName("demo-ma.pine") == "demo-ma" && customIndicatorBaseName("x.js") == "x", "customIndicatorBaseName");

    QElapsedTimer t;
    t.start();
    const IndicatorDefPtr def = loadCustomIndicatorFile(file, "custom:demo-ma");
    std::cout << "  load: " << t.elapsed() << " ms\n";
    check(def != nullptr, "loaded");
    if (!def) return;
    check(def->id == "custom:demo-ma" && def->name == "Demo MA" && def->group == "Custom" && def->overlay && def->file == file, "definition");
    check(indicatorById("custom:demo-ma") == def, "registered");
    check(def->inputs.size() == 4, QStringLiteral("4 chart inputs: %1").arg(def->inputs.size()));
    if (def->inputs.size() == 4) {
        check(def->inputs[0].key == "len" && def->inputs[0].type == "int" && def->inputs[0].def.toDouble() == 20
                  && def->inputs[0].min.toDouble() == 1 && def->inputs[0].max.toDouble() == 500 && def->inputs[0].label == "Length",
              "input len");
        check(def->inputs[1].key == "source" && def->inputs[1].type == "source" && def->inputs[1].def.toString() == "close", "input source");
        check(def->inputs[2].key == "useEma" && def->inputs[2].type == "select" && def->inputs[2].options == QStringList { "true", "false" }
                  && def->inputs[2].def.toString() == "false",
              "input useEma");
        check(def->inputs[3].key == "mode" && def->inputs[3].type == "select" && def->inputs[3].options == QStringList { "A", "B" }, "input mode");
    }
    check(def->outputs.size() == 2, QStringLiteral("2 outputs: %1").arg(def->outputs.size()));
    if (def->outputs.size() == 2) {
        check(def->outputs[0].key == "MA" && def->outputs[0].label == "MA" && def->outputs[0].kind == "line" && def->outputs[0].color == "#2962FF"
                  && def->outputs[0].lineWidth == 2,
              "output MA");
        check(def->outputs[1].key == "RSI" && def->outputs[1].kind == "histogram", "output RSI");
    }
    check(def->levels == QVector<double> { 50 }, "hline as a level");
    check(indicatorDefaults("custom:demo-ma").value("len").toDouble() == 20, "defaults");

    const Values close = ta::source(bars, "close");
    t.restart();
    const IndicatorResult r = compute("custom:demo-ma", bars);
    std::cout << "  compute: " << t.elapsed() << " ms\n";
    sameSeries(r.values.value("MA"), ta::sma(close, 20), 0, "compute MA");
    sameSeries(r.values.value("RSI"), ta::rsi(close, 14), 100, "compute RSI", 1e-6);
    check(r.colors.size() == bars.size(), "histogram colours per bar");
    check(r.hasMarkers && !r.markers.isEmpty(), QStringLiteral("markers: %1").arg(r.markers.size()));
    {
        const QVector<bool> upWant = ta::crossover(close, ta::sma(close, 20)), downWant = ta::crossunder(close, ta::sma(close, 20));
        int ups = 0, downs = 0, bad = 0;
        for (const IndicatorMarker &m : r.markers) {
            if (m.shape == "arrowUp" && m.position == "belowBar" && m.color == "#4CAF50" && m.text == "up") ups++;
            else if (m.shape == "circle" && m.position == "aboveBar" && m.color == "#F23645" && m.text == "x") downs++;
            else bad++;
        }
        int wantUps = 0, wantDowns = 0;
        for (int i = 0; i < bars.size(); ++i) {
            wantUps += upWant[i];
            wantDowns += downWant[i];
        }
        check(bad == 0 && ups == wantUps && downs == wantDowns,
              QStringLiteral("markers: %1 up (want %2), %3 down (want %4), %5 other").arg(ups).arg(wantUps).arg(downs).arg(wantDowns).arg(bad));
        check(std::is_sorted(r.markers.begin(), r.markers.end(), [](const IndicatorMarker &a, const IndicatorMarker &b) { return a.time < b.time; }),
              "markers in time order");
    }

    // params: numbers, the bool select, the source
    const IndicatorResult rp = compute("custom:demo-ma", bars, QJsonObject { { "len", 7 }, { "useEma", "true" }, { "source", "hl2" } });
    sameSeries(rp.values.value("MA"), ta::ema(ta::source(bars, "hl2"), 7), 60, "compute with params", 1e-6);

    // the file changes: the next compute runs the new script (same definition until reloaded)
    QThread::msleep(20);
    {
        QString src = readFile(file);
        src.replace("ta.sma(src, len)", "ta.wma(src, len)");
        QFile f(file);
        f.open(QIODevice::WriteOnly | QIODevice::Truncate);
        f.write(src.toUtf8());
    }
    const IndicatorResult re = compute("custom:demo-ma", bars);
    sameSeries(re.values.value("MA"), ta::wma(close, 20), 0, "compute after the file changed");

    // several threads at once, each with its own Node.js
    std::atomic<int> okCount { 0 }, failCount { 0 };
    std::vector<std::thread> threads;
    for (int k = 0; k < 3; ++k)
        threads.emplace_back([&] {
            try {
                const IndicatorResult x = compute("custom:demo-ma", bars, QJsonObject { { "len", 9 } });
                const Values want = ta::wma(close, 9);
                bool same = x.values.value("MA").size() == want.size();
                for (int i = 0; same && i < want.size(); ++i)
                    same = (std::isnan(want[i]) && std::isnan(x.values.value("MA")[i])) || std::abs(want[i] - x.values.value("MA")[i]) <= 1e-7 * std::max(1.0, std::abs(want[i]));
                (same ? okCount : failCount)++;
            } catch (const Error &e) {
                std::cerr << "  thread error: " << e.message().toStdString() << "\n";
                failCount++;
            }
        });
    for (auto &th : threads) th.join();
    check(okCount == 3 && failCount == 0, QStringLiteral("3 threads: %1 ok, %2 failed").arg(okCount.load()).arg(failCount.load()));

    // the JavaScript of a file
    check(pineIndicatorJs(file).contains("ta.wma("), "pineIndicatorJs");
}

// ---------------------------------------------------------------- 5. errors
void errors(const Bars &bars)
{
    CAT = "errors";
    auto message = [](const std::function<void()> &f) {
        try {
            f();
        } catch (const Error &e) {
            return e.message();
        }
        return QString("(no error)");
    };
    QString m = message([&] { pine::run("//@version=6\nindicator(\"x\")\nplot(ta.sma(close, )", bars); });
    check(m.startsWith("Failed to transpile Pine Script version 6:") && m.contains("3:21"), "syntax error: " + m);
    m = message([&] { pine::run("//@version=6\nindicator(\"x\")\nplot(ta.foo(close, 2))", bars); });
    check(m == "ta.foo is not a function", "unknown function: " + m);
    m = message([&] { pine::transpile("//@version=6\nindicator(\"x\"\nplot(close)"); });
    check(m.startsWith("Failed to transpile"), "transpile error: " + m);
    m = message([&] { pine::run("//@version=6\nindicator(\"x\")\nif bar_index == 10\n    runtime.error(\"boom\")\nplot(close)", bars); });
    check(m == "boom", "runtime.error: " + m);
    m = message([&] { pine::run("//@version=6\nindicator(\"x\")\na = array.new_float(2)\nplot(array.get(a, 5))", bars); });
    check(m == "Index 5 is out of bounds, array size is 2.", "out of bounds: " + m);
    m = message([&] { loadPineIndicator("/nonexistent/x.pine", "custom:x"); });
    check(m == "Cannot find module '/nonexistent/x.pine'", "missing file: " + m);
    QTemporaryDir dir;
    {
        QFile f(dir.path() + "/noplot.pine");
        f.open(QIODevice::WriteOnly);
        f.write("//@version=6\nindicator(\"No plot\")\nplotshape(close > open, \"up\")\n");
    }
    m = message([&] { loadPineIndicator(dir.path() + "/noplot.pine", "custom:noplot"); });
    check(m.startsWith("This script plots nothing the chart can draw"), "no series plot: " + m);
    {
        QFile f(dir.path() + "/broken.pine");
        f.open(QIODevice::WriteOnly);
        f.write("//@version=6\nindicator(\"Broken\")\nplot(ta.sma(close, 20)\n");
    }
    m = message([&] { loadCustomIndicatorFile(dir.path() + "/broken.pine", "custom:broken"); });
    check(m.startsWith("Failed to transpile Pine Script version 6"), "broken file: " + m);
    check(indicatorById("custom:broken") == nullptr, "a broken file isn't registered");
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    app.setOrganizationName("Deepwater");
    app.setApplicationName("qtm_test_pinets");
    QTemporaryDir runtime;
    pine::setRuntimeDir(runtime.path() + "/pinets");

    const Bars bars = loadBars(ROOT + "/tests/quant/fixtures/btc-1d.json", 400);
    if (bars.isEmpty()) {
        std::cerr << "fixture tests/quant/fixtures/btc-1d.json not found\n";
        return 1;
    }
    std::cout << "qtm_test_pinets\n";
    if (!bridge()) return 0;
    for (const auto &section : { std::function<void()>(transpile), std::function<void()>([&] { runScript(bars); }),
                                 std::function<void()>([&] { indicator(bars); }), std::function<void()>([&] { errors(bars); }) }) {
        try {
            section();
        } catch (const Error &e) {
            check(false, "unexpected error: " + e.message());
        }
    }
    std::cout << S.checks << " checks, " << S.fails << " failed\n";
    for (auto it = S.checksBy.begin(); it != S.checksBy.end(); ++it)
        std::cout << "  " << it.key().toStdString() << ": " << it.value() << " checks, " << S.failsBy.value(it.key()) << " failed\n";
    return S.fails ? 1 : 0;
}
