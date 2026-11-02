// The built-in indicators: inputs (shown in their settings), outputs (one chart series each) and
// calc(). overlay: true draws on the price pane; false gets its own pane. Same order, ids, labels,
// defaults and colours as the INDICATORS array of quant-terminal/public/lib/indicators.js.
#include "IndicatorsInternal.h"

#include "Error.h"

#include <QJsonArray>

#include <algorithm>

#if defined(__clang__)
#pragma clang fp contract(off)
#endif

namespace qtm::detail {

namespace {

using namespace ta;

const QJsonValue Undef(QJsonValue::Undefined);

IndicatorInput input(const QString &key, const QString &label, const QString &type, const QJsonValue &def,
                     const QJsonValue &min = Undef, const QJsonValue &max = Undef)
{
    IndicatorInput i;
    i.key = key;
    i.label = label;
    i.type = type;
    i.def = def;
    i.min = min;
    i.max = max;
    return i;
}
IndicatorInput intIn(const QString &key, const QString &label, double def, const QJsonValue &min = Undef)
{
    return input(key, label, QStringLiteral("int"), def, min);
}
IndicatorInput floatIn(const QString &key, const QString &label, double def)
{
    return input(key, label, QStringLiteral("float"), def);
}
IndicatorInput selectIn(const QString &key, const QString &label, const QStringList &options, const QString &def)
{
    IndicatorInput i = input(key, label, QStringLiteral("select"), def);
    i.options = options;
    return i;
}
IndicatorInput src(const QString &def = QStringLiteral("close"))
{
    return input(QStringLiteral("source"), QStringLiteral("Source"), QStringLiteral("source"), def);
}
IndicatorInput len(double d, const QString &label = QStringLiteral("Length"))
{
    return intIn(QStringLiteral("length"), label, d, 1);
}
IndicatorOutput line(const QString &key, const QString &label, const QString &color, int lineWidth = 0,
                     bool dots = false)
{
    IndicatorOutput o;
    o.key = key;
    o.label = label;
    o.kind = QStringLiteral("line");
    o.color = color;
    o.lineWidth = lineWidth;
    o.dots = dots;
    return o;
}
IndicatorOutput hist(const QString &key, const QString &label, const QString &color)
{
    IndicatorOutput o = line(key, label, color);
    o.kind = QStringLiteral("histogram");
    return o;
}

const QString MA_COLORS = QStringLiteral("#2962ff");

// Guppy: six fast EMAs in shades of cyan, six slow ones in shades of red.
const QVector<QPair<int, QString>> &guppy()
{
    static const QVector<QPair<int, QString>> g {
        { 3, "#80deea" },  { 5, "#4dd0e1" },  { 8, "#26c6da" },  { 10, "#00bcd4" },
        { 12, "#00acc1" }, { 15, "#0097a7" }, { 30, "#ef9a9a" }, { 35, "#e57373" },
        { 40, "#ef5350" }, { 45, "#e53935" }, { 50, "#d32f2f" }, { 60, "#b71c1c" },
    };
    return g;
}

IndicatorResult res(std::initializer_list<std::pair<QString, Values>> values)
{
    IndicatorResult r;
    for (const auto &kv : values) r.values.insert(kv.first, kv.second);
    return r;
}

IndicatorDef def(const QString &id, const QString &name, const QString &group, bool overlay,
                 QVector<IndicatorInput> inputs, QVector<IndicatorOutput> outputs,
                 std::function<IndicatorResult(const Bars &, const Params &)> calc, QVector<double> levels = {})
{
    IndicatorDef d;
    d.id = id;
    d.name = name;
    d.group = group;
    d.overlay = overlay;
    d.inputs = std::move(inputs);
    d.outputs = std::move(outputs);
    d.levels = std::move(levels);
    d.calc = [calc](const Bars &bars, const QJsonObject &params) { return calc(bars, Params { params }); };
    return d;
}

using MaFn = std::function<Values(const Values &s, int n, const Params &p, const Bars &bars)>;
IndicatorDef maDef(const QString &id, const QString &name, MaFn fn, QVector<IndicatorInput> extraInputs = {})
{
    QVector<IndicatorInput> inputs { len(id == QLatin1String("ema") ? 9 : 20), src() };
    inputs += extraInputs;
    return def(id, name, QStringLiteral("Moving averages"), true, inputs, { line("value", name, MA_COLORS) },
               [fn](const Bars &bars, const Params &p) {
                   return res({ { "value", fn(source(bars, p.str("source")), p.len("length"), p, bars) } });
               });
}

Values map(const Values &a, const std::function<double(double, int)> &f)
{
    Values out(a.size());
    for (int i = 0; i < a.size(); ++i) out[i] = f(a[i], i);
    return out;
}
// c.slice(from, to) for 0 <= from (empty when from >= to).
Values window(const Values &c, int from, int to)
{
    return from < to ? c.mid(from, to - from) : Values();
}
Values mapBars(const Bars &bars, const std::function<double(const Bar &, int)> &f)
{
    Values out(bars.size());
    for (int i = 0; i < bars.size(); ++i) out[i] = f(bars[i], i);
    return out;
}
// c.map((v, i) => (i >= n ? (100 * (v - c[i - n])) / c[i - n] : N))
Values rocOf(const Values &c, int n)
{
    return map(c, [&](double v, int i) { return i >= n ? (100 * (v - at(c, qint64(i) - n))) / at(c, qint64(i) - n) : NaN; });
}
// (b.high === b.low ? 0 : ((2 * b.close - b.low - b.high) / (b.high - b.low)) * (b.volume || 0))
double adOf(const Bar &b)
{
    return b.high == b.low ? 0 : ((2 * b.close - b.low - b.high) / (b.high - b.low)) * js::orZero(b.volume);
}

IndicatorMarker mk(double time, const char *position, const char *shape, const char *color, const char *text)
{
    IndicatorMarker m;
    m.time = time;
    m.position = QLatin1String(position);
    m.shape = QLatin1String(shape);
    m.color = QLatin1String(color);
    m.text = QLatin1String(text);
    return m;
}

const QString VOL_UP = QStringLiteral("rgba(38,166,154,0.6)"), VOL_DOWN = QStringLiteral("rgba(239,83,80,0.6)");

} // namespace

QVector<IndicatorDef> builtinIndicators()
{
    QVector<IndicatorDef> L;
    const QString MA = QStringLiteral("Moving averages"), TREND = QStringLiteral("Trend"),
                  VOLAT = QStringLiteral("Volatility"), MOM = QStringLiteral("Momentum"), VOL = QStringLiteral("Volume");

    L << maDef("sma", "Moving Average (SMA)", [](const Values &s, int n, const Params &, const Bars &) { return sma(s, n); });
    L << maDef("ema", "Exponential Moving Average (EMA)", [](const Values &s, int n, const Params &, const Bars &) { return ema(s, n); });
    L << maDef("wma", "Weighted Moving Average (WMA)", [](const Values &s, int n, const Params &, const Bars &) { return wma(s, n); });
    L << maDef("vwma", "Volume Weighted Moving Average (VWMA)",
               [](const Values &s, int n, const Params &, const Bars &bars) { return vwma(s, source(bars, "volume"), n); });
    L << maDef("hma", "Hull Moving Average (HMA)", [](const Values &s, int n, const Params &, const Bars &) { return hma(s, n); });
    L << maDef("dema", "Double EMA (DEMA)", [](const Values &s, int n, const Params &, const Bars &) { return dema(s, n); });
    L << maDef("tema", "Triple EMA (TEMA)", [](const Values &s, int n, const Params &, const Bars &) { return tema(s, n); });
    L << maDef("smma", "Smoothed Moving Average (SMMA / RMA)", [](const Values &s, int n, const Params &, const Bars &) { return rma(s, n); });
    L << maDef("alma", "Arnaud Legoux Moving Average (ALMA)",
               [](const Values &s, int n, const Params &p, const Bars &) { return alma(s, n, p.num("offset"), p.num("sigma")); },
               { floatIn("offset", "Offset", 0.85), floatIn("sigma", "Sigma", 6) });
    L << maDef("lsma", "Least Squares Moving Average (LSMA)", [](const Values &s, int n, const Params &, const Bars &) { return linreg(s, n, 0); });

    L << def("mcginley", "McGinley Dynamic", MA, true, { len(14), src() }, { line("value", "McGinley Dynamic", MA_COLORS) },
             [](const Bars &bars, const Params &p) {
                 // Starts on the first EMA, then chases price faster when price runs away from it.
                 const Values s = source(bars, p.str("source")), e = ema(s, p.len("length"));
                 const double L = p.num("length");
                 double prev = NaN;
                 return res({ { "value", map(s, [&](double v, int i) {
                                    prev = isNum(prev) ? prev + (v - prev) / (L * std::pow(v / prev, 4)) : e[i];
                                    return prev;
                                }) } });
             });

    L << def("macross", "MA Cross", MA, true,
             { intIn("fast", "Fast length", 9, 1), intIn("slow", "Slow length", 21, 1),
               selectIn("type", "Type", { "EMA", "SMA" }, "EMA"), src() },
             { line("fast", "Fast", "#2962ff"), line("slow", "Slow", "#ff6d00") },
             [](const Bars &bars, const Params &p) {
                 const Values s = source(bars, p.str("source"));
                 const bool isSma = p.raw("type") == QJsonValue(QStringLiteral("SMA"));
                 const Values fast = isSma ? sma(s, p.len("fast")) : ema(s, p.len("fast"));
                 const Values slow = isSma ? sma(s, p.len("slow")) : ema(s, p.len("slow"));
                 const auto up = crossover(fast, slow), dn = crossunder(fast, slow);
                 IndicatorResult r = res({ { "fast", fast }, { "slow", slow } });
                 r.hasMarkers = true;
                 for (int i = 0; i < bars.size(); ++i) {
                     if (up[i]) r.markers << mk(bars[i].time, "belowBar", "arrowUp", "#26a69a", "Cross up");
                     if (dn[i]) r.markers << mk(bars[i].time, "aboveBar", "arrowDown", "#ef5350", "Cross down");
                 }
                 return r;
             });

    L << def("ribbon", "Moving Average Ribbon", MA, true, { src() },
             { line("m20", "MA 20", "#f6c309"), line("m50", "MA 50", "#fb9800"), line("m100", "MA 100", "#fb6500"),
               line("m200", "MA 200", "#f60c0c") },
             [](const Bars &bars, const Params &p) {
                 const Values s = source(bars, p.str("source"));
                 return res({ { "m20", ema(s, 20) }, { "m50", ema(s, 50) }, { "m100", ema(s, 100) }, { "m200", ema(s, 200) } });
             });

    {
        QVector<IndicatorOutput> outs;
        for (const auto &[n, color] : guppy()) outs << line(QStringLiteral("ema%1").arg(n), QStringLiteral("EMA %1").arg(n), color, 1);
        L << def("guppy", "Guppy Multiple Moving Average", MA, true, {}, outs, [](const Bars &bars, const Params &) {
            const Values c = source(bars, "close");
            IndicatorResult r;
            for (const auto &g : guppy()) r.values.insert(QStringLiteral("ema%1").arg(g.first), ema(c, g.first));
            return r;
        });
    }

    L << def("vwap", "Volume Weighted Average Price (VWAP)", VOL, true,
             { src("hlc3"), selectIn("anchor", "Anchor", { "day", "week", "month", "year" }, "day") },
             { line("value", "VWAP", "#2962ff", 2) },
             [](const Bars &bars, const Params &p) {
                 const Values s = source(bars, p.str("source"));
                 const QString anchor = jsonTruthy(p.raw("anchor")) ? p.str("anchor") : QStringLiteral("day");
                 QString key;
                 bool hasKey = false;
                 int start = 0;
                 double pv = 0, v = 0;
                 QVector<bool> breaks(bars.size(), false);
                 const Values value = mapBars(bars, [&](const Bar &b, int i) {
                     const QString k = periodKey(b.time, anchor);
                     if (!hasKey || k != key) {
                         breaks[i] = sessionBreak(hasKey, start, i);   // new session: don't join it to yesterday's line
                         key = k;
                         hasKey = true;
                         start = i;
                         pv = 0;
                         v = 0;
                     }
                     const double vol = js::orZero(b.volume);
                     pv += s[i] * vol;
                     v += vol;
                     return js::truthy(v) ? pv / v : s[i];
                 });
                 IndicatorResult r = res({ { "value", value } });
                 r.breaks = breaks;
                 return r;
             });

    // VWAP from a bar you pick instead of each session's start. Adding it from the picker asks for a click on the
    // chart; the settings take a date and time. Bands are volume-weighted standard deviations around it.
    L << def("avwap", "Anchored VWAP", VOL, true,
             { input("anchor", "Anchor", "time", 0), src("hlc3"), floatIn("band1", "Band 1 (StdDev, 0 = off)", 1),
               floatIn("band2", "Band 2 (StdDev, 0 = off)", 2) },
             { line("value", "Anchored VWAP", "#ff9800", 2), line("upper1", "Upper band 1", "#26a69a"),
               line("lower1", "Lower band 1", "#26a69a"), line("upper2", "Upper band 2", "#7e57c2"),
               line("lower2", "Lower band 2", "#7e57c2") },
             [](const Bars &bars, const Params &p) {
                 const Values s = source(bars, p.str("source"));
                 const int n = bars.size();
                 // No anchor yet: start 150 bars back so the line shows straight away. An anchor before the loaded
                 // history starts at the first bar; one after the last bar draws nothing.
                 const double anchor = p.num("anchor");
                 int a;
                 if (!jsonTruthy(p.raw("anchor"))) a = std::max(0, n - 150);
                 else if (n && anchor <= bars[0].time) a = 0;
                 else {
                     a = -1;
                     for (int i = 0; i < n; ++i)
                         if (bars[i].time >= anchor) {
                             a = i;
                             break;
                         }
                     if (a < 0) a = n;
                 }
                 const double b1 = p.num("band1"), b2 = p.num("band2");
                 Values value, upper1, lower1, upper2, lower2;
                 double pv = 0, pv2 = 0, v = 0;
                 for (int i = 0; i < n; ++i) {
                     if (i < a) {
                         for (Values *o : { &value, &upper1, &lower1, &upper2, &lower2 }) o->push_back(NaN);
                         continue;
                     }
                     const double vol = js::orZero(bars[i].volume);
                     pv += s[i] * vol;
                     pv2 += s[i] * s[i] * vol;
                     v += vol;
                     const double vw = js::truthy(v) ? pv / v : s[i];
                     const double sd = js::truthy(v) ? std::sqrt(js::max(0, pv2 / v - vw * vw)) : 0;
                     value.push_back(vw);
                     upper1.push_back(js::truthy(b1) ? vw + b1 * sd : NaN);
                     lower1.push_back(js::truthy(b1) ? vw - b1 * sd : NaN);
                     upper2.push_back(js::truthy(b2) ? vw + b2 * sd : NaN);
                     lower2.push_back(js::truthy(b2) ? vw - b2 * sd : NaN);
                 }
                 return res({ { "value", value }, { "upper1", upper1 }, { "lower1", lower1 }, { "upper2", upper2 }, { "lower2", lower2 } });
             });

    L << def("twap", "Time Weighted Average Price (TWAP)", VOL, true,
             { selectIn("anchor", "Anchor", { "day", "week", "month" }, "day") }, { line("value", "TWAP", "#ff6d00", 2) },
             [](const Bars &bars, const Params &p) {
                 // Plain average of ohlc4 since the period began, volume ignored. Starts over at each new period.
                 const Values s = source(bars, "ohlc4");
                 const QString anchor = p.str("anchor");
                 QString key;
                 bool hasKey = false;
                 int start = 0;
                 double total = 0, count = 0;
                 QVector<bool> breaks(bars.size(), false);
                 const Values value = mapBars(bars, [&](const Bar &b, int i) {
                     const QString k = periodKey(b.time, anchor);
                     if (!hasKey || k != key) {
                         breaks[i] = sessionBreak(hasKey, start, i);
                         key = k;
                         hasKey = true;
                         start = i;
                         total = 0;
                         count = 0;
                     }
                     total += s[i];
                     count++;
                     return total / count;
                 });
                 IndicatorResult r = res({ { "value", value } });
                 r.breaks = breaks;
                 return r;
             });

    L << def("bb", "Bollinger Bands", VOLAT, true, { len(20), src(), floatIn("mult", "StdDev", 2) },
             { line("upper", "Upper", "#f23645"), line("basis", "Basis", "#2962ff"), line("lower", "Lower", "#089981") },
             [](const Bars &bars, const Params &p) {
                 const Values s = source(bars, p.str("source")), basis = sma(s, p.len("length")), sd = stdev(s, p.len("length"));
                 const double m = p.num("mult");
                 return res({ { "basis", basis },
                              { "upper", map(basis, [&](double v, int i) { return v + m * sd[i]; }) },
                              { "lower", map(basis, [&](double v, int i) { return v - m * sd[i]; }) } });
             });

    L << def("bbpb", "Bollinger Bands %B", VOLAT, false, { len(20), src(), floatIn("mult", "StdDev", 2) },
             { line("value", "%B", "#22ab94") },
             [](const Bars &bars, const Params &p) {
                 const Values s = source(bars, p.str("source")), basis = sma(s, p.len("length")), sd = stdev(s, p.len("length"));
                 const double m = p.num("mult");
                 return res({ { "value", map(s, [&](double v, int i) { return (v - (basis[i] - m * sd[i])) / (2 * m * sd[i]); }) } });
             },
             { 0, 0.5, 1 });

    L << def("bbw", "Bollinger BandWidth", VOLAT, false, { len(20), src(), floatIn("mult", "StdDev", 2) },
             { line("value", "BBW", "#2962ff") },
             [](const Bars &bars, const Params &p) {
                 const Values s = source(bars, p.str("source")), basis = sma(s, p.len("length")), sd = stdev(s, p.len("length"));
                 const double m = p.num("mult");
                 return res({ { "value", map(basis, [&](double b, int i) { return ((2 * m * sd[i]) / b) * 100; }) } });
             });

    L << def("bbtrend", "BBTrend", VOLAT, false,
             { intIn("short", "Short length", 20), intIn("long", "Long length", 50), floatIn("mult", "StdDev", 2) },
             { hist("value", "BBTrend", "#26a69a") },
             [](const Bars &bars, const Params &p) {
                 const Values c = source(bars, "close");
                 const double m = p.num("mult");
                 struct Bands {
                     Values mid, upper, lower;
                 };
                 auto bands = [&](int n) {
                     Bands b;
                     b.mid = sma(c, n);
                     const Values sd = stdev(c, n);
                     b.upper = map(b.mid, [&](double v, int i) { return v + m * sd[i]; });
                     b.lower = map(b.mid, [&](double v, int i) { return v - m * sd[i]; });
                     return b;
                 };
                 const Bands s = bands(p.len("short")), l = bands(p.len("long"));
                 const Values value = map(s.mid, [&](double mm, int i) {
                     return ((std::fabs(s.lower[i] - l.lower[i]) - std::fabs(s.upper[i] - l.upper[i])) / mm) * 100;
                 });
                 IndicatorResult r = res({ { "value", value } });
                 for (double v : value) r.colors << (v >= 0 ? QStringLiteral("#26a69a") : QStringLiteral("#ef5350"));
                 return r;
             },
             { 0 });

    L << def("kc", "Keltner Channels", VOLAT, true,
             { len(20), floatIn("mult", "Multiplier", 2), intIn("atrLength", "ATR length", 10), src() },
             { line("upper", "Upper", "#2962ff"), line("basis", "Basis", "#2962ff"), line("lower", "Lower", "#2962ff") },
             [](const Bars &bars, const Params &p) {
                 const Values basis = ema(source(bars, p.str("source")), p.len("length")), a = atr(bars, p.len("atrLength"));
                 const double m = p.num("mult");
                 return res({ { "basis", basis },
                              { "upper", map(basis, [&](double v, int i) { return v + m * a[i]; }) },
                              { "lower", map(basis, [&](double v, int i) { return v - m * a[i]; }) } });
             });

    L << def("donchian", "Donchian Channels", VOLAT, true, { len(20) },
             { line("upper", "Upper", "#2962ff"), line("basis", "Basis", "#ff6d00"), line("lower", "Lower", "#2962ff") },
             [](const Bars &bars, const Params &p) {
                 const Values upper = highest(source(bars, "high"), p.len("length")),
                              lower = lowest(source(bars, "low"), p.len("length"));
                 return res({ { "upper", upper }, { "lower", lower },
                              { "basis", map(upper, [&](double v, int i) { return (v + lower[i]) / 2; }) } });
             });

    L << def("envelope", "Envelope", VOLAT, true, { len(20), floatIn("percent", "Percent", 10), src() },
             { line("upper", "Upper", "#2962ff"), line("basis", "Basis", "#ff6d00"), line("lower", "Lower", "#2962ff") },
             [](const Bars &bars, const Params &p) {
                 const Values basis = sma(source(bars, p.str("source")), p.len("length"));
                 const double k = p.num("percent") / 100;
                 return res({ { "basis", basis }, { "upper", map(basis, [&](double v, int) { return v * (1 + k); }) },
                              { "lower", map(basis, [&](double v, int) { return v * (1 - k); }) } });
             });

    L << def("atr", "Average True Range (ATR)", VOLAT, false, { len(14) }, { line("value", "ATR", "#b71c1c") },
             [](const Bars &bars, const Params &p) { return res({ { "value", atr(bars, p.len("length")) } }); });

    L << def("stdev", "Standard Deviation", VOLAT, false, { len(20), src() }, { line("value", "StdDev", "#2962ff") },
             [](const Bars &bars, const Params &p) {
                 return res({ { "value", stdev(source(bars, p.str("source")), p.len("length")) } });
             });

    L << def("hv", "Historical Volatility", VOLAT, false, { len(10), intIn("annual", "Periods per year", 365) },
             { line("value", "HV", "#2962ff") },
             [](const Bars &bars, const Params &p) {
                 const Values lr = mapBars(bars, [&](const Bar &b, int i) { return i ? js::log(b.close / bars[i - 1].close) : NaN; });
                 const double k = std::sqrt(p.num("annual"));
                 return res({ { "value", map(stdev(lr, p.len("length")), [&](double v, int) { return 100 * v * k; }) } });
             });

    L << def("rvi", "Relative Volatility Index", VOLAT, false,
             { len(10, "StdDev length"), intIn("smoothing", "Smoothing length", 14) }, { line("value", "RVI", "#7e57c2") },
             [](const Bars &bars, const Params &p) {
                 // Like RSI, but the up and down halves are the bar-by-bar standard deviation instead of the price change.
                 const Values c = source(bars, "close"), ch = change(c), sd = stdev(c, p.len("length"));
                 const Values up = ema(map(ch, [&](double v, int i) { return v <= 0 ? 0 : sd[i]; }), p.len("smoothing"));
                 const Values down = ema(map(ch, [&](double v, int i) { return v > 0 ? 0 : sd[i]; }), p.len("smoothing"));
                 return res({ { "value", map(up, [&](double u, int i) { return (100 * u) / (u + down[i]); }) } });
             });

    L << def("rsi", "Relative Strength Index (RSI)", MOM, false,
             { len(14), src(), intIn("maLength", "Smoothing (SMA, 0 = off)", 14, 0) },
             { line("value", "RSI", "#7e57c2"), line("ma", "RSI-based MA", "#f7c600", 1) },
             [](const Bars &bars, const Params &p) {
                 const Values value = rsi(source(bars, p.str("source")), p.len("length"));
                 return res({ { "value", value },
                              { "ma", p.num("maLength") > 0 ? sma(value, p.len("maLength")) : filled(value.size()) } });
             },
             { 30, 50, 70 });

    L << def("crsi", "Connors RSI", MOM, false,
             { intIn("rsiLength", "RSI length", 3), intIn("streakLength", "Up/Down length", 2),
               intIn("rankLength", "ROC percent rank", 100) },
             { line("value", "CRSI", "#2962ff") },
             [](const Bars &bars, const Params &p) {
                 // The average of three readings: RSI of price, RSI of the up/down streak, and where today's 1-bar ROC ranks.
                 const Values c = source(bars, "close");
                 const Values price = rsi(c, p.len("rsiLength")), streak = rsi(updownStreak(c), p.len("streakLength"));
                 const Values rank = percentRank(map(c, [&](double v, int i) { return i ? (100 * (v - c[i - 1])) / c[i - 1] : NaN; }),
                                                 p.len("rankLength"));
                 return res({ { "value", map(price, [&](double v, int i) { return (v + streak[i] + rank[i]) / 3; }) } });
             },
             { 30, 70 });

    L << def("stoch", "Stochastic", MOM, false,
             { intIn("k", "%K length", 14), intIn("smoothK", "%K smoothing", 1), intIn("d", "%D smoothing", 3) },
             { line("k", "%K", "#2962ff"), line("d", "%D", "#ff6d00") },
             [](const Bars &bars, const Params &p) {
                 const Values k = sma(stochOf(source(bars, "close"), source(bars, "high"), source(bars, "low"), p.len("k")),
                                      p.len("smoothK"));
                 return res({ { "k", k }, { "d", sma(k, p.len("d")) } });
             },
             { 20, 50, 80 });

    L << def("stochrsi", "Stochastic RSI", MOM, false,
             { intIn("smoothK", "K", 3), intIn("smoothD", "D", 3), intIn("rsiLength", "RSI length", 14),
               intIn("stochLength", "Stochastic length", 14), src() },
             { line("k", "K", "#2962ff"), line("d", "D", "#ff6d00") },
             [](const Bars &bars, const Params &p) {
                 const Values r = rsi(source(bars, p.str("source")), p.len("rsiLength"));
                 const Values k = sma(stochOf(r, r, r, p.len("stochLength")), p.len("smoothK"));
                 return res({ { "k", k }, { "d", sma(k, p.len("smoothD")) } });
             },
             { 20, 50, 80 });

    L << def("macd", "MACD", MOM, false,
             { intIn("fast", "Fast length", 12), intIn("slow", "Slow length", 26), intIn("signal", "Signal smoothing", 9), src() },
             { hist("hist", "Histogram", "#26a69a"), line("macd", "MACD", "#2962ff"), line("signal", "Signal", "#ff6d00") },
             [](const Bars &bars, const Params &p) {
                 const Macd m = macdFrom(source(bars, p.str("source")), p.len("fast"), p.len("slow"), p.len("signal"));
                 IndicatorResult r = res({ { "macd", m.macd }, { "signal", m.signal }, { "hist", m.hist } });
                 r.colors = m.colors;
                 return r;
             });

    L << def("cci", "Commodity Channel Index (CCI)", MOM, false, { len(20), src("hlc3") }, { line("value", "CCI", "#2962ff") },
             [](const Bars &bars, const Params &p) {
                 const Values s = source(bars, p.str("source")), m = sma(s, p.len("length")), d = dev(s, p.len("length"));
                 return res({ { "value", map(s, [&](double v, int i) { return (v - m[i]) / (0.015 * d[i]); }) } });
             },
             { -100, 0, 100 });

    L << def("willr", "Williams %R", MOM, false, { len(14) }, { line("value", "%R", "#7e57c2") },
             [](const Bars &bars, const Params &p) {
                 const Values hh = highest(source(bars, "high"), p.len("length")), ll = lowest(source(bars, "low"), p.len("length"));
                 return res({ { "value", mapBars(bars, [&](const Bar &b, int i) { return (100 * (b.close - hh[i])) / (hh[i] - ll[i]); }) } });
             },
             { -80, -50, -20 });

    L << def("fisher", "Fisher Transform", MOM, false, { len(9) },
             { line("fisher", "Fisher", "#2962ff"), line("trigger", "Trigger", "#ff6d00") },
             [](const Bars &bars, const Params &p) {
                 const int n = p.len("length");
                 const Values h2 = source(bars, "hl2"), hi = highest(h2, n), lo = lowest(h2, n);
                 Values fisher = filled(bars.size());
                 double value = 0, fish = 0;
                 for (int i = n - 1; i < bars.size(); ++i) {
                     // A flat window counts as 0 on the 0..1 scale. (Before the first bar both sides are undefined.)
                     const double ratio = (i < 0 || hi[i] == lo[i]) ? 0 : (at(h2, i) - lo[i]) / (hi[i] - lo[i]);
                     // TradingView snaps anything past 0.99 to 0.999 rather than clamping it.
                     const double v = 0.66 * (ratio - 0.5) + 0.67 * value;
                     value = v > 0.99 ? 0.999 : v < -0.99 ? -0.999 : v;
                     fish = 0.5 * js::log((1 + value) / (1 - value)) + 0.5 * fish;
                     if (i >= 0) fisher[i] = fish;
                 }
                 return res({ { "fisher", fisher }, { "trigger", map(fisher, [&](double, int i) { return i ? fisher[i - 1] : NaN; }) } });
             },
             { -1.5, -0.75, 0, 0.75, 1.5 });

    L << def("roc", "Rate Of Change (ROC)", MOM, false, { len(9), src() }, { line("value", "ROC", "#2962ff") },
             [](const Bars &bars, const Params &p) { return res({ { "value", rocOf(source(bars, p.str("source")), p.len("length")) } }); },
             { 0 });

    L << def("mom", "Momentum", MOM, false, { len(10), src() }, { line("value", "Momentum", "#2962ff") },
             [](const Bars &bars, const Params &p) { return res({ { "value", change(source(bars, p.str("source")), p.len("length")) } }); },
             { 0 });

    L << def("mfi", "Money Flow Index (MFI)", VOL, false, { len(14) }, { line("value", "MFI", "#7e57c2") },
             [](const Bars &bars, const Params &p) {
                 const Values tp = source(bars, "hlc3"), ch = change(tp);
                 const Values up = sum(mapBars(bars, [&](const Bar &b, int i) {
                                           return isNum(ch[i]) ? (ch[i] <= 0 ? 0 : js::orZero(b.volume) * tp[i]) : NaN;
                                       }),
                                       p.len("length"));
                 const Values dn = sum(mapBars(bars, [&](const Bar &b, int i) {
                                           return isNum(ch[i]) ? (ch[i] >= 0 ? 0 : js::orZero(b.volume) * tp[i]) : NaN;
                                       }),
                                       p.len("length"));
                 return res({ { "value", map(up, [&](double u, int i) { return dn[i] == 0 ? 100 : 100 - 100 / (1 + u / dn[i]); }) } });
             },
             { 20, 50, 80 });

    L << def("cmo", "Chande Momentum Oscillator", MOM, false, { len(9), src() }, { line("value", "CMO", "#2962ff") },
             [](const Bars &bars, const Params &p) {
                 const Values ch = change(source(bars, p.str("source")));
                 const Values su = sum(map(ch, [](double v, int) { return isNum(v) ? js::max(v, 0) : NaN; }), p.len("length"));
                 const Values sd = sum(map(ch, [](double v, int) { return isNum(v) ? js::max(-v, 0) : NaN; }), p.len("length"));
                 return res({ { "value", map(su, [&](double u, int i) { return (100 * (u - sd[i])) / (u + sd[i]); }) } });
             },
             { -50, 0, 50 });

    L << def("rci", "Rank Correlation Index", MOM, false, { len(9) }, { line("value", "RCI", "#2962ff") },
             [](const Bars &bars, const Params &p) {
                 // Spearman correlation of time order against price order over the window, x100. Ties share an average rank.
                 const int n = p.len("length");
                 const Values c = source(bars, "close");
                 Values time(std::max(0, n));
                 for (int k = 0; k < time.size(); ++k) time[k] = k + 1;
                 return res({ { "value", map(c, [&](double, int i) {
                                    if (i < n - 1) return NaN;
                                    const Values w = window(c, i - n + 1, i + 1);
                                    for (double x : w)
                                        if (!isNum(x)) return NaN;
                                    return 100 * pearson(time, avgRanks(w));
                                }) } });
             },
             { 0 });

    L << def("trix", "TRIX", MOM, false, { len(18) }, { line("value", "TRIX", "#f23645") },
             [](const Bars &bars, const Params &p) {
                 const int n = p.len("length");
                 const Values l = mapBars(bars, [](const Bar &b, int) { return js::log(b.close); });
                 return res({ { "value", map(change(ema(ema(ema(l, n), n), n)), [](double v, int) { return 10000 * v; }) } });
             },
             { 0 });

    L << def("ao", "Awesome Oscillator", MOM, false, {}, { hist("value", "AO", "#26a69a") },
             [](const Bars &bars, const Params &) {
                 const Values hl2 = source(bars, "hl2"), slow = sma(hl2, 34);
                 const Values v = map(sma(hl2, 5), [&](double x, int i) { return x - slow[i]; });
                 IndicatorResult r = res({ { "value", v } });
                 for (int i = 0; i < v.size(); ++i)
                     r.colors << ((i && v[i] >= v[i - 1]) ? QStringLiteral("#26a69a") : QStringLiteral("#ef5350"));
                 return r;
             });

    L << def("uo", "Ultimate Oscillator", MOM, false,
             { intIn("fast", "Fast", 7), intIn("middle", "Middle", 14), intIn("slow", "Slow", 28) },
             { line("value", "UO", "#f23645") },
             [](const Bars &bars, const Params &p) {
                 const Values bp = mapBars(bars, [&](const Bar &b, int i) { return i ? b.close - js::min(b.low, bars[i - 1].close) : NaN; });
                 const Values trr = mapBars(bars, [&](const Bar &b, int i) {
                     return i ? js::max(b.high, bars[i - 1].close) - js::min(b.low, bars[i - 1].close) : NaN;
                 });
                 auto avg = [&](int n) {
                     const Values t = sum(trr, n);
                     return map(sum(bp, n), [&](double v, int i) { return v / t[i]; });
                 };
                 const Values a = avg(p.len("fast")), b = avg(p.len("middle")), c = avg(p.len("slow"));
                 return res({ { "value", map(a, [&](double v, int i) { return (100 * (4 * v + 2 * b[i] + c[i])) / 7; }) } });
             },
             { 30, 50, 70 });

    L << def("tsi", "True Strength Index", MOM, false,
             { intIn("long", "Long length", 25), intIn("short", "Short length", 13), intIn("signal", "Signal length", 13) },
             { line("value", "TSI", "#2962ff"), line("signal", "Signal", "#f23645") },
             [](const Bars &bars, const Params &p) {
                 const Values pc = change(source(bars, "close"));
                 const Values num = ema(ema(pc, p.len("long")), p.len("short"));
                 const Values den = ema(ema(map(pc, [](double v, int) { return std::fabs(v); }), p.len("long")), p.len("short"));
                 const Values value = map(num, [&](double v, int i) { return (100 * v) / den[i]; });
                 return res({ { "value", value }, { "signal", ema(value, p.len("signal")) } });
             },
             { 0 });

    L << def("smiergodic", "SMI Ergodic Indicator/Oscillator", MOM, false,
             { intIn("long", "Long length", 20), intIn("short", "Short length", 5), intIn("signal", "Signal length", 5) },
             { hist("osc", "Oscillator", "#26a69a"), line("smi", "SMI", "#2962ff"), line("signal", "Signal", "#ff6d00") },
             [](const Bars &bars, const Params &p) {
                 // The true strength index on a -1..1 scale (not x100).
                 const Values pc = change(source(bars, "close"));
                 const Values num = ema(ema(pc, p.len("short")), p.len("long"));
                 const Values den = ema(ema(map(pc, [](double v, int) { return std::fabs(v); }), p.len("short")), p.len("long"));
                 const Values smi = map(num, [&](double v, int i) { return v / den[i]; });
                 const Values signal = ema(smi, p.len("signal"));
                 return res({ { "smi", smi }, { "signal", signal }, { "osc", map(smi, [&](double v, int i) { return v - signal[i]; }) } });
             },
             { 0 });

    L << def("ppo", "Price Oscillator (PPO)", MOM, false,
             { intIn("fast", "Fast", 12), intIn("slow", "Slow", 26), intIn("signal", "Signal", 9), src() },
             { line("value", "PPO", "#2962ff"), line("signal", "Signal", "#ff6d00") },
             [](const Bars &bars, const Params &p) {
                 const Values s = source(bars, p.str("source")), f = ema(s, p.len("fast")), sl = ema(s, p.len("slow"));
                 const Values value = map(f, [&](double v, int i) { return ((v - sl[i]) / sl[i]) * 100; });
                 return res({ { "value", value }, { "signal", ema(value, p.len("signal")) } });
             },
             { 0 });

    L << def("coppock", "Coppock Curve", MOM, false,
             { intIn("wma", "WMA length", 10), intIn("long", "Long ROC", 14), intIn("short", "Short ROC", 11) },
             { line("value", "Coppock", "#2962ff") },
             [](const Bars &bars, const Params &p) {
                 const Values c = source(bars, "close"), l = rocOf(c, p.len("long")), s = rocOf(c, p.len("short"));
                 return res({ { "value", wma(map(l, [&](double v, int i) { return v + s[i]; }), p.len("wma")) } });
             },
             { 0 });

    L << def("kst", "Know Sure Thing", MOM, false,
             { intIn("roc1", "ROC 1", 10), intIn("roc2", "ROC 2", 15), intIn("roc3", "ROC 3", 20), intIn("roc4", "ROC 4", 30),
               intIn("sma1", "SMA 1", 10), intIn("sma2", "SMA 2", 10), intIn("sma3", "SMA 3", 10), intIn("sma4", "SMA 4", 15),
               intIn("signal", "Signal length", 9) },
             { line("kst", "KST", "#089981"), line("signal", "Signal", "#f23645") },
             [](const Bars &bars, const Params &p) {
                 const Values c = source(bars, "close");
                 auto smaRoc = [&](int r, int s) { return sma(rocOf(c, r), s); };
                 const Values a = smaRoc(p.len("roc1"), p.len("sma1")), b = smaRoc(p.len("roc2"), p.len("sma2")),
                              d = smaRoc(p.len("roc3"), p.len("sma3")), e = smaRoc(p.len("roc4"), p.len("sma4"));
                 const Values kst = map(a, [&](double v, int i) { return v + 2 * b[i] + 3 * d[i] + 4 * e[i]; });
                 return res({ { "kst", kst }, { "signal", sma(kst, p.len("signal")) } });
             },
             { 0 });

    L << def("dpo", "Detrended Price Oscillator", MOM, false,
             { len(21, "Period"), selectIn("centered", "Centered", { "Off", "On" }, "Off") }, { line("value", "DPO", "#43a047") },
             [](const Bars &bars, const Params &p) {
                 const Values c = source(bars, "close");
                 const int back = int(std::floor(p.num("length") / 2)) + 1;
                 const Values ma = sma(c, p.len("length"));
                 // Off: today's close against the average from `back` bars ago. On: each close against the average centred
                 // on it, which means looking `back` bars ahead, so the whole line is drawn `back` bars earlier.
                 if (p.raw("centered") == QJsonValue(QStringLiteral("On")))
                     return res({ { "value", shiftForward(map(ma, [&](double m, int i) { return at(c, qint64(i) - back) - m; }), -back) } });
                 return res({ { "value", map(c, [&](double v, int i) { return v - at(ma, qint64(i) - back); }) } });
             },
             { 0 });

    L << def("elder", "Elder Ray (Bull/Bear Power)", MOM, false, { len(13) },
             { hist("bull", "Bull power", "#26a69a"), hist("bear", "Bear power", "#ef5350") },
             [](const Bars &bars, const Params &p) {
                 const Values e = ema(source(bars, "close"), p.len("length"));
                 return res({ { "bull", mapBars(bars, [&](const Bar &b, int i) { return b.high - e[i]; }) },
                              { "bear", mapBars(bars, [&](const Bar &b, int i) { return b.low - e[i]; }) } });
             },
             { 0 });

    L << def("bop", "Balance of Power", MOM, false, {}, { line("value", "BOP", "#f23645") },
             [](const Bars &bars, const Params &) {
                 return res({ { "value", mapBars(bars, [](const Bar &b, int) { return b.high == b.low ? 0 : (b.close - b.open) / (b.high - b.low); }) } });
             },
             { 0 });

    L << def("rvgi", "Relative Vigor Index", MOM, false, { len(10) },
             { line("value", "RVGI", "#089981"), line("signal", "Signal", "#f23645") },
             [](const Bars &bars, const Params &p) {
                 const Values num = swma(mapBars(bars, [](const Bar &b, int) { return b.close - b.open; }));
                 const Values den = swma(mapBars(bars, [](const Bar &b, int) { return b.high - b.low; }));
                 const Values sn = sum(num, p.len("length")), sd = sum(den, p.len("length"));
                 const Values value = map(sn, [&](double v, int i) { return v / sd[i]; });
                 return res({ { "value", value }, { "signal", swma(value) } });
             },
             { 0 });

    L << def("dmi", "Directional Movement (DMI / ADX)", TREND, false,
             { intIn("diLength", "DI length", 14), intIn("adxSmoothing", "ADX smoothing", 14) },
             { line("adx", "ADX", "#f23645"), line("plus", "+DI", "#2962ff"), line("minus", "-DI", "#ff6d00") },
             [](const Bars &bars, const Params &p) {
                 const Dmi d = dmiOf(bars, p.len("diLength"), p.len("adxSmoothing"));
                 return res({ { "plus", d.plus }, { "minus", d.minus }, { "adx", d.adx } });
             });

    L << def("adx", "Average Directional Index (ADX)", TREND, false,
             { intIn("diLength", "DI length", 14), intIn("adxSmoothing", "ADX smoothing", 14) }, { line("adx", "ADX", "#f23645") },
             [](const Bars &bars, const Params &p) { return res({ { "adx", dmiOf(bars, p.len("diLength"), p.len("adxSmoothing")).adx } }); },
             { 20, 25 });

    L << def("aroon", "Aroon", TREND, false, { len(14) },
             { line("up", "Aroon up", "#ff6d00"), line("down", "Aroon down", "#2962ff") },
             [](const Bars &bars, const Params &p) {
                 const double n = p.num("length");
                 const Values hb = extremeBars(source(bars, "high"), p.len("length") + 1, true);
                 const Values lb = extremeBars(source(bars, "low"), p.len("length") + 1, false);
                 return res({ { "up", map(hb, [&](double v, int) { return (100 * (v + n)) / n; }) },
                              { "down", map(lb, [&](double v, int) { return (100 * (v + n)) / n; }) } });
             });

    L << def("vortex", "Vortex Indicator", TREND, false, { len(14) },
             { line("plus", "VI +", "#2962ff"), line("minus", "VI -", "#e91e63") },
             [](const Bars &bars, const Params &p) {
                 const int n = p.len("length");
                 const Values vmp = sum(mapBars(bars, [&](const Bar &b, int i) { return i ? std::fabs(b.high - bars[i - 1].low) : NaN; }), n);
                 const Values vmm = sum(mapBars(bars, [&](const Bar &b, int i) { return i ? std::fabs(b.low - bars[i - 1].high) : NaN; }), n);
                 const Values str = sum(map(tr(bars), [](double v, int i) { return i ? v : NaN; }), n);
                 return res({ { "plus", map(vmp, [&](double v, int i) { return v / str[i]; }) },
                              { "minus", map(vmm, [&](double v, int i) { return v / str[i]; }) } });
             });

    L << def("chop", "Choppiness Index", TREND, false, { len(14) }, { line("value", "CHOP", "#2962ff") },
             [](const Bars &bars, const Params &p) {
                 const int n = p.len("length");
                 const Values s = sum(tr(bars), n), hh = highest(source(bars, "high"), n), ll = lowest(source(bars, "low"), n);
                 const double d = js::log10(p.num("length"));
                 return res({ { "value", map(s, [&](double v, int i) { return (100 * js::log10(v / (hh[i] - ll[i]))) / d; }) } });
             },
             { 38.2, 61.8 });

    L << def("chopzone", "Chop Zone", TREND, false, {}, { hist("value", "Chop Zone", "#fdd835") },
             [](const Bars &bars, const Params &) {
                 // Colours the bar by the angle of the 34 EMA: cyan and green for a rising trend, orange and red for a
                 // falling one, yellow when it is going nowhere.
                 const Values e = ema(source(bars, "close"), 34), avg = source(bars, "hlc3"),
                              hh = highest(source(bars, "high"), 30), ll = lowest(source(bars, "low"), 30);
                 QStringList colors;
                 for (int i = 0; i < bars.size(); ++i) colors << QStringLiteral("#FDD835");
                 const Values value = mapBars(bars, [&](const Bar &, int i) {
                     if (!i || !isNum(e[i - 1]) || !isNum(e[i])) return NaN;
                     const double span = (25 / (hh[i] - ll[i])) * ll[i], y2 = ((e[i - 1] - e[i]) / avg[i]) * span;
                     const double a1 = js::round((180 * std::acos(1 / std::sqrt(1 + y2 * y2))) / M_PI);
                     const double a = y2 > 0 ? -a1 : a1;
                     colors[i] = a >= 5        ? "#26C6DA"
                                 : a >= 3.57   ? "#43A047"
                                 : a >= 2.14   ? "#A5D6A7"
                                 : a >= 0.71   ? "#009688"
                                 : a <= -5     ? "#D50000"
                                 : a <= -3.57  ? "#E91E63"
                                 : a <= -2.14  ? "#FF6D00"
                                 : a <= -0.71  ? "#FFB74D"
                                               : "#FDD835";
                     return 1.0;
                 });
                 IndicatorResult r = res({ { "value", value } });
                 r.colors = colors;
                 return r;
             });

    L << def("trendstrength", "Trend Strength Index", TREND, false, { len(14) }, { line("value", "Trend strength", "#2962ff") },
             [](const Bars &bars, const Params &p) {
                 // Correlation of price with the bar count: +1 a straight climb, -1 a straight fall, 0 no trend.
                 const int n = p.len("length");
                 const Values c = source(bars, "close");
                 Values idx(std::max(0, n));
                 for (int k = 0; k < idx.size(); ++k) idx[k] = k;
                 return res({ { "value", map(c, [&](double, int i) {
                                    if (i < n - 1) return NaN;
                                    const Values w = window(c, i - n + 1, i + 1);
                                    for (double x : w)
                                        if (!isNum(x)) return NaN;
                                    return pearson(idx, w);
                                }) } });
             },
             { 0 });

    L << def("mass", "Mass Index", TREND, false, { len(10) }, { line("value", "Mass", "#2962ff") },
             [](const Bars &bars, const Params &p) {
                 const Values span = mapBars(bars, [](const Bar &b, int) { return b.high - b.low; });
                 const Values e1 = ema(span, 9), e2 = ema(e1, 9);
                 return res({ { "value", sum(map(e1, [&](double v, int i) { return v / e2[i]; }), p.len("length")) } });
             });

    L << def("psar", "Parabolic SAR", TREND, true,
             { floatIn("start", "Start", 0.02), floatIn("inc", "Increment", 0.02), floatIn("max", "Max value", 0.2) },
             { line("value", "SAR", "#2962ff", 0, true) },
             [](const Bars &bars, const Params &p) {
                 return res({ { "value", psarOf(bars, p.num("start"), p.num("inc"), p.num("max")) } });
             });

    L << def("supertrend", "Supertrend", TREND, true,
             { intIn("atrLength", "ATR length", 10), floatIn("factor", "Factor", 3) },
             { line("up", "Up trend", "#26a69a", 2), line("down", "Down trend", "#ef5350", 2) },
             [](const Bars &bars, const Params &p) {
                 const Supertrend s = supertrendOf(bars, p.num("factor"), p.len("atrLength"));
                 return res({ { "up", map(s.st, [&](double v, int i) { return s.dir[i] < 0 ? v : NaN; }) },
                              { "down", map(s.st, [&](double v, int i) { return s.dir[i] > 0 ? v : NaN; }) },
                              { "direction", s.dir } });
             });

    L << def("vstop", "Volatility Stop", TREND, true, { len(20, "ATR length"), floatIn("factor", "Factor", 2) },
             { line("up", "Up trend", "#26a69a", 2), line("down", "Down trend", "#ef5350", 2) },
             [](const Bars &bars, const Params &p) {
                 const Vstop v = vstopOf(bars, p.len("length"), p.num("factor"));
                 return res({ { "up", v.up }, { "down", v.down } });
             });

    L << def("cks", "Chande Kroll Stop", TREND, true,
             { intIn("p", "ATR length (p)", 10), floatIn("x", "ATR multiplier (x)", 1), intIn("q", "Stop length (q)", 9) },
             { line("stopLong", "Stop long", "#26a69a"), line("stopShort", "Stop short", "#ef5350") },
             [](const Bars &bars, const Params &prm) {
                 const int p = prm.len("p"), q = prm.len("q");
                 const double x = prm.num("x");
                 const Values a = atr(bars, p), hh = highest(source(bars, "high"), p), ll = lowest(source(bars, "low"), p);
                 const Values firstHigh = map(hh, [&](double v, int i) { return v - x * a[i]; });
                 const Values firstLow = map(ll, [&](double v, int i) { return v + x * a[i]; });
                 return res({ { "stopShort", whole(highest(firstHigh, q), firstHigh, q) },
                              { "stopLong", whole(lowest(firstLow, q), firstLow, q) } });
             });

    L << def("ichimoku", "Ichimoku Cloud", TREND, true,
             { intIn("conversion", "Conversion line", 9), intIn("base", "Base line", 26), intIn("spanB", "Leading span B", 52),
               intIn("displacement", "Lagging span", 26) },
             { line("conversion", "Conversion", "#2962ff"), line("base", "Base", "#b71c1c"), line("lagging", "Lagging span", "#43a047"),
               line("spanA", "Leading span A", "#a5d6a7"), line("spanB", "Leading span B", "#ef9a9a") },
             [](const Bars &bars, const Params &p) {
                 const Values hi = source(bars, "high"), lo = source(bars, "low");
                 auto mid = [&](int n) {
                     const Values ll = lowest(lo, n);
                     return map(highest(hi, n), [&](double v, int i) { return (v + ll[i]) / 2; });
                 };
                 const Values conversion = mid(p.len("conversion")), base = mid(p.len("base"));
                 const Values a = map(conversion, [&](double v, int i) { return (v + base[i]) / 2; }), b = mid(p.len("spanB"));
                 const double offD = p.num("displacement") - 1;
                 const int off = toInt(offD);
                 IndicatorResult r = res({ { "conversion", conversion }, { "base", base },
                                           { "lagging", shiftForward(source(bars, "close"), -off) },
                                           { "spanA", shiftForward(a, off) }, { "spanB", shiftForward(b, off) } });
                 r.extra.insert("extendBars", offD);
                 return r;
             });

    L << def("alligator", "Williams Alligator", TREND, true,
             { intIn("jawLength", "Jaw length", 13), intIn("jawOffset", "Jaw offset", 8), intIn("teethLength", "Teeth length", 8),
               intIn("teethOffset", "Teeth offset", 5), intIn("lipsLength", "Lips length", 5), intIn("lipsOffset", "Lips offset", 3) },
             { line("jaw", "Jaw", "#2962ff"), line("teeth", "Teeth", "#e91e63"), line("lips", "Lips", "#66bb6a") },
             [](const Bars &bars, const Params &p) {
                 // Three smoothed averages of the bar midpoint, each drawn a few bars ahead of the bar it was computed on.
                 const Values h2 = source(bars, "hl2");
                 IndicatorResult r = res({ { "jaw", shiftForward(rma(h2, p.len("jawLength")), p.len("jawOffset")) },
                                           { "teeth", shiftForward(rma(h2, p.len("teethLength")), p.len("teethOffset")) },
                                           { "lips", shiftForward(rma(h2, p.len("lipsLength")), p.len("lipsOffset")) } });
                 r.extra.insert("extendBars", js::max(p.num("jawOffset"), p.num("teethOffset"), p.num("lipsOffset")));
                 return r;
             });

    {
        IndicatorDef d = def("pivots", "Pivot Points Standard", TREND, true,
                             { selectIn("period", "Pivot period", { "auto", "day", "week", "month", "year" }, "auto") },
                             { line("r3", "R3", "#ef5350", 1), line("r2", "R2", "#ef5350", 1), line("r1", "R1", "#ef5350", 1),
                               line("p", "P", "#ff9800", 1), line("s1", "S1", "#26a69a", 1), line("s2", "S2", "#26a69a", 1),
                               line("s3", "S3", "#26a69a", 1) },
                             [](const Bars &bars, const Params &p) { return pivotsOf(bars, p.str("period")); });
        d.noAutoscale = true;
        L << d;
    }

    {
        IndicatorDef d = def("pdhl", "Previous Day High/Low", TREND, true,
                             { selectIn("period", "Previous", { "day", "week", "month" }, "day") },
                             { line("high", "Prev high", "#ef5350", 1), line("low", "Prev low", "#26a69a", 1) },
                             [](const Bars &bars, const Params &p) {
                                 // Each bar shows the high and low of the whole period before the one it's in.
                                 const int n = bars.size();
                                 Values high = filled(n), low = filled(n);
                                 QVector<bool> breaks(n, false);
                                 const QString period = p.str("period");
                                 QString key;
                                 bool hasKey = false, hasCur = false, hasPrev = false;
                                 double curH = 0, curL = 0, prevH = 0, prevL = 0;
                                 for (int i = 0; i < n; ++i) {
                                     const Bar &b = bars[i];
                                     const QString k = periodKey(b.time, period);
                                     if (!hasKey || k != key) {
                                         if (hasCur) {
                                             prevH = curH;
                                             prevL = curL;
                                         }
                                         hasPrev = hasCur;
                                         curH = b.high;
                                         curL = b.low;
                                         hasCur = true;
                                         key = k;
                                         hasKey = true;
                                         breaks[i] = true;
                                     } else {
                                         curH = js::max(curH, b.high);
                                         curL = js::min(curL, b.low);
                                     }
                                     if (hasPrev) {
                                         high[i] = prevH;
                                         low[i] = prevL;
                                     }
                                 }
                                 IndicatorResult r = res({ { "high", high }, { "low", low } });
                                 r.breaks = breaks;
                                 return r;
                             });
        d.noAutoscale = true;
        L << d;
    }

    L << def("pivotshl", "Pivot Points High Low", TREND, true,
             { intIn("left", "Bars to the left", 10), intIn("right", "Bars to the right", 10) }, {},
             [](const Bars &bars, const Params &p) {
                 IndicatorResult r;
                 r.markers = pivotMarkers(bars, p.len("left"), p.len("right"));
                 r.hasMarkers = true;
                 return r;
             });

    L << def("zigzag", "Zig Zag", TREND, true, { floatIn("deviation", "Deviation (%)", 5), intIn("depth", "Depth", 10) },
             { line("value", "Zig Zag", "#2962ff", 2) },
             [](const Bars &bars, const Params &p) { return res({ { "value", zigzagOf(bars, p.num("deviation"), p.num("depth")) } }); });

    {
        IndicatorDef d = def("volume", "Volume", VOL, false, { intIn("maLength", "MA length (0 = off)", 20, 0) },
                             { hist("value", "Volume", "#26a69a"), line("ma", "Volume MA", "#2962ff", 1) },
                             [](const Bars &bars, const Params &p) {
                                 const Values v = source(bars, "volume");
                                 IndicatorResult r = res({ { "value", v },
                                                           { "ma", p.num("maLength") > 0 ? sma(v, p.len("maLength")) : filled(v.size()) } });
                                 for (const Bar &b : bars) r.colors << (b.close >= b.open ? VOL_UP : VOL_DOWN);
                                 return r;
                             });
        d.volumeFormat = true;
        L << d;
    }

    {
        IndicatorDef d = def("obv", "On Balance Volume (OBV)", VOL, false, {}, { line("value", "OBV", "#2962ff") },
                             [](const Bars &bars, const Params &) {
                                 return res({ { "value", cum(mapBars(bars, [&](const Bar &b, int i) {
                                                    return i ? js::sign(b.close - bars[i - 1].close) * js::orZero(b.volume) : 0;
                                                })) } });
                             });
        d.volumeFormat = true;
        L << d;
    }

    L << def("cmf", "Chaikin Money Flow", VOL, false, { len(20) }, { line("value", "CMF", "#43a047") },
             [](const Bars &bars, const Params &p) {
                 const Values ad = mapBars(bars, [](const Bar &b, int) { return adOf(b); });
                 const Values sv = sum(source(bars, "volume"), p.len("length"));
                 return res({ { "value", map(sum(ad, p.len("length")), [&](double v, int i) { return js::truthy(sv[i]) ? v / sv[i] : 0; }) } });
             },
             { 0 });

    {
        IndicatorDef d = def("ad", "Accumulation/Distribution", VOL, false, {}, { line("value", "A/D", "#999915") },
                             [](const Bars &bars, const Params &) {
                                 return res({ { "value", cum(mapBars(bars, [](const Bar &b, int) { return adOf(b); })) } });
                             });
        d.volumeFormat = true;
        L << d;
    }

    {
        IndicatorDef d = def("chaikinosc", "Chaikin Oscillator", VOL, false,
                             { intIn("fast", "Fast length", 3), intIn("slow", "Slow length", 10) },
                             { line("value", "Chaikin Osc", "#2962ff") },
                             [](const Bars &bars, const Params &p) {
                                 const Values ad = compute(QStringLiteral("ad"), bars).values.value(QStringLiteral("value"));
                                 const Values fast = ema(ad, p.len("fast")), slow = ema(ad, p.len("slow"));
                                 return res({ { "value", map(fast, [&](double v, int i) { return v - at(slow, i); }) } });
                             },
                             { 0 });
        d.volumeFormat = true;
        L << d;
    }

    {
        IndicatorDef d = def("pvt", "Price Volume Trend", VOL, false, {}, { line("value", "PVT", "#2962ff") },
                             [](const Bars &bars, const Params &) {
                                 return res({ { "value", cum(mapBars(bars, [&](const Bar &b, int i) {
                                                    return i ? ((b.close - bars[i - 1].close) / bars[i - 1].close) * js::orZero(b.volume) : 0;
                                                })) } });
                             });
        d.volumeFormat = true;
        L << d;
    }

    L << def("eom", "Ease of Movement", VOL, false, { len(14), intIn("divisor", "Divisor", 10000) },
             { line("value", "EOM", "#43a047") },
             [](const Bars &bars, const Params &p) {
                 const Values ch = change(source(bars, "hl2"));
                 const double divisor = p.num("divisor");
                 return res({ { "value", sma(mapBars(bars, [&](const Bar &b, int i) {
                                                 return js::truthy(b.volume) ? (divisor * ch[i] * (b.high - b.low)) / b.volume : 0;
                                             }),
                                             p.len("length")) } });
             },
             { 0 });

    {
        IndicatorDef d = def("efi", "Elder Force Index", VOL, false, { len(13) }, { line("value", "EFI", "#26a69a") },
                             [](const Bars &bars, const Params &p) {
                                 const Values ch = change(source(bars, "close")), vol = source(bars, "volume");
                                 return res({ { "value", ema(map(ch, [&](double v, int i) { return v * vol[i]; }), p.len("length")) } });
                             },
                             { 0 });
        d.volumeFormat = true;
        L << d;
    }

    {
        IndicatorDef d = def("kvo", "Klinger Oscillator", VOL, false,
                             { intIn("fast", "Fast length", 34), intIn("slow", "Slow length", 55), intIn("signal", "Signal length", 13) },
                             { line("kvo", "KVO", "#2962ff"), line("signal", "Signal", "#43a047") },
                             [](const Bars &bars, const Params &p) {
                                 // Volume counts for the bar when the typical price rose or held, against it when the price fell.
                                 const Values ch = change(source(bars, "hlc3")), vol = source(bars, "volume");
                                 const Values sv = map(ch, [&](double v, int i) { return v >= 0 ? vol[i] : -vol[i]; });
                                 const Values f = ema(sv, p.len("fast")), s = ema(sv, p.len("slow"));
                                 const Values kvo = map(f, [&](double v, int i) { return v - s[i]; });
                                 return res({ { "kvo", kvo }, { "signal", ema(kvo, p.len("signal")) } });
                             },
                             { 0 });
        d.volumeFormat = true;
        L << d;
    }

    L << def("volosc", "Volume Oscillator", VOL, false, { intIn("short", "Short length", 5), intIn("long", "Long length", 10) },
             { line("value", "Volume Osc", "#43a047") },
             [](const Bars &bars, const Params &p) {
                 const Values vol = source(bars, "volume"), s = ema(vol, p.len("short")), l = ema(vol, p.len("long"));
                 return res({ { "value", map(s, [&](double v, int i) { return (100 * (v - l[i])) / l[i]; }) } });
             },
             { 0 });

    {
        IndicatorDef d = def("netvol", "Net Volume", VOL, false, {}, { hist("value", "Net volume", "#26a69a") },
                             [](const Bars &bars, const Params &) {
                                 const Values ch = change(source(bars, "close")), vol = source(bars, "volume");
                                 const Values value = map(ch, [&](double v, int i) { return v > 0 ? vol[i] : v < 0 ? -vol[i] : 0; });
                                 IndicatorResult r = res({ { "value", value } });
                                 for (double v : value) r.colors << (v >= 0 ? VOL_UP : VOL_DOWN);
                                 return r;
                             });
        d.volumeFormat = true;
        L << d;
    }

    // Lewis's hedge fund method: label each bar bull, bear or sideways by its trailing return,
    // count how often each state follows each other one, and read tomorrow's odds from today's
    // row. Walk-forward: each bar only uses transitions that had already happened.
    L << def("markov", "Markov Regime (bull, bear, sideways)", "Quant (bonus)", false,
             { intIn("lookback", "Trailing return (bars)", 20, 2), floatIn("threshold", "Bull/bear threshold %", 5),
               intIn("minHistory", "Transitions before odds show", 50, 5) },
             { hist("signal", "Bull minus bear odds", "#9598a1"), line("pbull", "Odds of bull next", "#26a69a"),
               line("pbear", "Odds of bear next", "#ef5350") },
             [](const Bars &bars, const Params &p) {
                 const int n = bars.size(), lookback = p.len("lookback");
                 const double th = p.num("threshold") / 100, minHistory = p.num("minHistory");
                 Values state = filled(n), signal = filled(n), pbull = filled(n), pbear = filled(n);
                 QStringList colors;
                 for (int i = 0; i < n; ++i) colors << QStringLiteral("#9598a1");
                 double counts[3][3] = { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } };   // rows: from bear, sideways, bull
                 double seen = 0;
                 // A negative lookback starts before the first bar, where the JS reads a property of undefined.
                 if (lookback < 0) throw Error(QStringLiteral("Cannot read properties of undefined (reading 'close')"));
                 for (int i = lookback; i < n; ++i) {
                     const double rr = bars[i].close / bars[i - lookback].close - 1;
                     state[i] = rr >= th ? 1 : rr <= -th ? -1 : 0;
                     if (i - 1 >= 0 && std::isfinite(state[i - 1])) {
                         counts[int(state[i - 1]) + 1][int(state[i]) + 1]++;
                         seen++;
                     }
                     const double *row = counts[int(state[i]) + 1];
                     const double total = row[0] + row[1] + row[2];
                     colors[i] = state[i] == 1 ? "#26a69a" : state[i] == -1 ? "#ef5350" : "#9598a1";
                     if (seen >= minHistory && total > 0) {
                         pbull[i] = row[2] / total;
                         pbear[i] = row[0] / total;
                         signal[i] = pbull[i] - pbear[i];
                     }
                 }
                 IndicatorResult r = res({ { "signal", signal }, { "pbull", pbull }, { "pbear", pbear }, { "state", state } });
                 r.colors = colors;
                 return r;
             },
             { 0 });

    L << def("vpvr", "Volume Profile, visible range (VPVR)", VOL, true,
             { intIn("rows", "Rows", 24, 4), intIn("width", "Width (% of chart)", 30, 5), intIn("valueArea", "Value area %", 70, 10) },
             {},
             [](const Bars &, const Params &p) {
                 IndicatorResult r;
                 r.extra.insert("volumeProfile",
                                QJsonObject { { "rows", p.raw("rows") }, { "width", p.raw("width") }, { "valueArea", p.raw("valueArea") } });
                 return r;
             });

    L << def("svp", "Session Volume Profile (SVP)", VOL, true,
             { selectIn("period", "Session", { "Auto", "Day", "Week", "Month" }, "Auto"), intIn("rows", "Rows", 24, 4),
               intIn("width", "Width (% of session)", 70, 5), intIn("valueArea", "Value area %", 70, 10) },
             {},
             [](const Bars &, const Params &p) {
                 IndicatorResult r;
                 r.extra.insert("sessionProfile", QJsonObject { { "period", p.raw("period") }, { "rows", p.raw("rows") },
                                                                { "width", p.raw("width") }, { "valueArea", p.raw("valueArea") } });
                 return r;
             });

    L << def("patterns", "Candlestick Patterns", "Patterns", true, {}, {}, [](const Bars &bars, const Params &) {
        IndicatorResult r;
        r.markers = candlePatterns(bars);
        r.hasMarkers = true;
        return r;
    });

    L << def("fractals", "Williams Fractals", "Patterns", true, { intIn("periods", "Periods", 2) }, {},
             [](const Bars &bars, const Params &p) {
                 IndicatorResult r;
                 r.markers = fractalMarkers(bars, p.len("periods"));
                 r.hasMarkers = true;
                 return r;
             });

    // Deepwater addition, not in Quant Terminal (and so the last built-in, after the ported ones): large aggressive
    // orders from the chart's Binance trades, drawn by ui/BigTradesLayer. The calculation only hands the settings over.
    L << def("bigtrades", "Big Trades", VOL, true,
             { selectIn("mode", "Threshold", { "Auto", "Manual" }, "Auto"),
               floatIn("min", "Min volume (manual; auto's fallback)", 0), floatIn("max", "Max volume (0 = none)", 0),
               floatIn("percentile", "Auto: percentile", 99), intIn("window", "Auto: window (minutes)", 30, 1),
               selectIn("volatility", "Auto: volatility adjustment", { "On", "Off" }, "On"),
               intIn("aggWindow", "Aggregation window (ms)", 50, 1),
               selectIn("aggTime", "Aggregate", { "Within the window", "Same timestamp only" }, "Within the window"),
               selectIn("aggPrice", "Prices", { "Same price", "Sweep (one direction)" }, "Same price"),
               selectIn("shape", "Shape", { "Circle", "Square", "Diamond", "Text" }, "Circle"),
               intIn("minSize", "Min size (px)", 8, 2), intIn("maxSize", "Max size (px)", 48, 2),
               floatIn("minOpacity", "Min opacity", 0.35),
               selectIn("showVolume", "Volume in the bubble", { "Off", "On" }, "Off") },
             // Colours only (Settings › Colours): no values, so no series.
             { line("buy", "Aggressive buys", "#26a69a"), line("sell", "Aggressive sells", "#ef5350") },
             [](const Bars &, const Params &p) {
                 IndicatorResult r;
                 QJsonObject o;
                 for (const char *k : { "mode", "min", "max", "percentile", "window", "volatility", "aggWindow", "aggTime", "aggPrice",
                                        "shape", "minSize", "maxSize", "minOpacity", "showVolume" })
                     o.insert(k, p.raw(k));
                 r.extra.insert("bigTrades", o);
                 return r;
             });

    // Deepwater addition, not in Quant Terminal: the liquidity heatmap, the resting orders of the chart's Binance order
    // book over time (ui/DepthFeed, drawn by ui/HeatmapLayer under the candles). The calculation only hands the
    // settings over.
    L << def("heatmap", "Liquidity Heatmap", VOL, true,
             { selectIn("interval", "Sampling interval", { "100 ms", "250 ms", "500 ms", "1 s" }, "250 ms"),
               intIn("history", "History kept (minutes)", 120, 1),
               selectIn("group", "Price rows grouped (× storage row)", { "1", "2", "4", "5", "10", "20", "50" }, "1"),
               selectIn("agg", "Zoomed out, cells in one pixel", { "Max", "Mean" }, "Max"),
               selectIn("palette", "Palette", { "Deep (blue → yellow)", "Thermal (black → red → white)", "Mono", "Custom (colours below)" },
                        "Deep (blue → yellow)"),
               floatIn("gain", "Intensity (× brightness)", 1), floatIn("gamma", "Contrast (gamma: > 1 keeps the big levels)", 1),
               floatIn("threshold", "Hide under (× reference size)", 0.02),
               floatIn("percentile", "Reference size: percentile of the visible cells", 95),
               selectIn("dom", "Current book at the right edge", { "On", "Off" }, "On"),
               selectIn("stable", "Only liquidity resting at least (approximation: levels, not orders)",
                        { "Off", "500 ms", "1 s", "2 s", "5 s", "10 s" }, "Off"),
               selectIn("events", "Mark pulled / absorbed liquidity", { "On", "Off" }, "On"),
               floatIn("pulledSize", "Pulled: a level ≥ (× the median level)", 5),
               floatIn("absorbRatio", "Absorbed: traded at the touch ≥ (× its resting size)", 1),
               selectIn("bubbles", "Trades as bubbles", { "On", "Off" }, "On"),
               floatIn("bubbleMin", "Bubbles: min volume (0 = auto: the top 10 %)", 0),
               selectIn("record", "Record the order book to disk (replay later)", { "Off", "On" }, "Off"),
               intIn("recordDays", "Recordings kept (days)", 7, 1),
               selectIn("debug", "Debug overlay", { "Off", "On" }, "Off") },
             // Colours only (Settings › Colours), for the Custom palette: no values, so no series.
             { line("low", "Custom palette: little liquidity", "#1e3a8a"), line("mid", "Custom palette: middle", "#22d3ee"),
               line("high", "Custom palette: the most", "#fde047") },
             [](const Bars &, const Params &p) {
                 IndicatorResult r;
                 QJsonObject o;
                 for (const char *k : { "interval", "history", "group", "agg", "palette", "gain", "gamma", "threshold", "percentile",
                                        "dom", "stable", "events", "pulledSize", "absorbRatio", "record", "recordDays", "debug" })
                     o.insert(k, p.raw(k));
                 r.extra.insert("heatmap", o);
                 // The aggressive trades over it, as Big Trades bubbles (ui/BigTradesLayer, the pane's trade feed): every
                 // aggregate from the min volume, or the top 10 % of the last 30 minutes' (no volatility factor), small
                 // bubbles so the liquidity stays readable.
                 if (p.raw("bubbles").toString() != "Off") {
                     const double min = p.raw("bubbleMin").toDouble();
                     r.extra.insert("bigTrades", QJsonObject{
                         { "mode", min > 0 ? "Manual" : "Auto" }, { "min", std::max(0.0, min) }, { "max", 0 }, { "percentile", 90 },
                         { "window", 30 }, { "volatility", "Off" }, { "aggWindow", 50 }, { "aggTime", "Within the window" },
                         { "aggPrice", "Same price" }, { "shape", "Circle" }, { "minSize", 6 }, { "maxSize", 36 },
                         { "minOpacity", 0.45 }, { "showVolume", "Off" } });
                 }
                 return r;
             });

    return L;
}

} // namespace qtm::detail
