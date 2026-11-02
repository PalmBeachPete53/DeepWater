// Technical indicators: sources, the ta.* building blocks and the helpers behind the built-ins.
// Port of the first half of quant-terminal/public/lib/indicators.js. Formulas follow the Pine
// Script reference (ta.* functions) so values line up with TradingView's built-ins: RMA-based
// RSI/ATR/ADX, SMA-seeded EMA, population stdev. Every series is the same length as the bars;
// missing values are NaN.
#include "IndicatorsInternal.h"

#include "Error.h"

#include <QDateTime>

#include <algorithm>

#if defined(__clang__)
#pragma clang fp contract(off)
#endif

namespace qtm {

using detail::at;
using detail::filled;

// ---------- sources ----------
namespace ta {

Values source(const Bars &bars, const QString &name)
{
    Values out;
    out.reserve(bars.size());
    if (name == QLatin1String("open"))
        for (const Bar &b : bars) out.push_back(b.open);
    else if (name == QLatin1String("high"))
        for (const Bar &b : bars) out.push_back(b.high);
    else if (name == QLatin1String("low"))
        for (const Bar &b : bars) out.push_back(b.low);
    else if (name == QLatin1String("volume"))
        for (const Bar &b : bars) out.push_back(b.volume);   // b.volume ?? 0: a missing volume is 0 in Bar
    else if (name == QLatin1String("hl2"))
        for (const Bar &b : bars) out.push_back((b.high + b.low) / 2);
    else if (name == QLatin1String("hlc3"))
        for (const Bar &b : bars) out.push_back((b.high + b.low + b.close) / 3);
    else if (name == QLatin1String("ohlc4"))
        for (const Bar &b : bars) out.push_back((b.open + b.high + b.low + b.close) / 4);
    else if (name == QLatin1String("hlcc4"))
        for (const Bar &b : bars) out.push_back((b.high + b.low + 2 * b.close) / 4);
    else
        for (const Bar &b : bars) out.push_back(b.close);
    return out;
}

// ---------- building blocks (ta.*) ----------
Values change(const Values &src, int n)
{
    Values out(src.size());
    for (int i = 0; i < src.size(); ++i) out[i] = i >= n ? src[i] - at(src, qint64(i) - n) : NaN;
    return out;
}

Values sum(const Values &src, int n)
{
    Values out = filled(src.size());
    double s = 0;
    int valid = 0;
    for (int i = 0; i < src.size(); ++i) {
        if (isNum(src[i])) {
            s += src[i];
            valid++;
        }
        if (i >= n && isNum(at(src, qint64(i) - n))) {
            s -= at(src, qint64(i) - n);
            valid--;
        }
        if (i >= n - 1 && valid == n) out[i] = s;
    }
    return out;
}

Values sma(const Values &src, int n)
{
    Values out = sum(src, n);
    for (double &v : out) v = v / n;
    return out;
}

// Shared core for EMA and RMA: seeded with the SMA of the first n valid values, as Pine does.
static Values smoothed(const Values &src, int n, double alpha)
{
    Values out = filled(src.size());
    double prev = NaN, seedSum = 0;
    int seedCount = 0;
    for (int i = 0; i < src.size(); ++i) {
        const double v = src[i];
        if (!isNum(v)) continue;
        if (!isNum(prev)) {
            seedSum += v;
            seedCount++;
            if (seedCount == n) {
                prev = seedSum / n;
                out[i] = prev;
            }
            continue;
        }
        prev = alpha * v + (1 - alpha) * prev;
        out[i] = prev;
    }
    return out;
}

Values ema(const Values &src, int n)
{
    return smoothed(src, n, 2.0 / (double(n) + 1));
}
Values rma(const Values &src, int n)
{
    return smoothed(src, n, 1.0 / double(n));
}

Values wma(const Values &src, int n)
{
    Values out = filled(src.size());
    const double denom = (double(n) * (double(n) + 1)) / 2;
    for (int i = std::max(0, n - 1); i < src.size(); ++i) {
        double s = 0;
        bool ok = true;
        for (int j = 0; j < n; ++j) {
            const double v = at(src, qint64(i) - j);
            if (!isNum(v)) {
                ok = false;
                break;
            }
            s += v * (n - j);
        }
        if (ok) out[i] = s / denom;
    }
    return out;
}

Values vwma(const Values &src, const Values &vol, int n)
{
    Values pvIn(src.size());
    for (int i = 0; i < src.size(); ++i) pvIn[i] = src[i] * at(vol, i);
    const Values pv = sma(pvIn, n);
    const Values vv = sma(vol, n);
    Values out(pv.size());
    for (int i = 0; i < pv.size(); ++i) {
        const double d = at(vv, i);
        out[i] = js::truthy(d) ? pv[i] / d : NaN;
    }
    return out;
}

Values hma(const Values &src, int n)
{
    if (n < 0) return filled(src.size());   // the last window is sqrt(n) = NaN long: no values at all
    const Values half = wma(src, std::max(1, int(std::floor(n / 2.0))));
    const Values full = wma(src, n);
    Values d(half.size());
    for (int i = 0; i < half.size(); ++i) d[i] = 2 * half[i] - at(full, i);
    return wma(d, std::max(1, int(std::floor(std::sqrt(double(n))))));
}

Values dema(const Values &src, int n)
{
    const Values e1 = ema(src, n), e2 = ema(e1, n);
    Values out(e1.size());
    for (int i = 0; i < e1.size(); ++i) out[i] = 2 * e1[i] - e2[i];
    return out;
}

Values tema(const Values &src, int n)
{
    const Values e1 = ema(src, n), e2 = ema(e1, n), e3 = ema(e2, n);
    Values out(e1.size());
    for (int i = 0; i < e1.size(); ++i) out[i] = 3 * (e1[i] - e2[i]) + e3[i];
    return out;
}

Values alma(const Values &src, int n, double offset, double sigma)
{
    const double m = offset * (double(n) - 1), s = double(n) / sigma;
    QVector<double> w(std::max(0, n));
    for (int i = 0; i < w.size(); ++i) {
        const double d = i - m;
        w[i] = js::exp(-(d * d) / (2 * s * s));
    }
    double norm = 0;
    for (double x : w) norm += x;
    Values out = filled(src.size());
    for (int t = std::max(0, n - 1); t < src.size(); ++t) {
        double acc = 0;
        bool ok = true;
        for (int i = 0; i < n; ++i) {
            const double v = at(src, qint64(t) - (n - 1 - i));
            if (!isNum(v)) {
                ok = false;
                break;
            }
            acc += v * w[i];
        }
        if (ok) out[t] = acc / norm;
    }
    return out;
}

Values linreg(const Values &src, int n, int offset)
{
    Values out = filled(src.size());
    for (int t = std::max(0, n - 1); t < src.size(); ++t) {
        double sx = 0, sy = 0, sxy = 0, sxx = 0;
        bool ok = true;
        for (int i = 0; i < n; ++i) {
            const double y = at(src, qint64(t) - n + 1 + i);
            if (!isNum(y)) {
                ok = false;
                break;
            }
            sx += i;
            sy += y;
            sxy += i * y;
            sxx += double(i) * i;
        }
        if (!ok) continue;
        const double slope = (n * sxy - sx * sy) / (n * sxx - sx * sx);
        const double intercept = (sy - slope * sx) / n;
        out[t] = intercept + slope * (double(n) - 1 - offset);
    }
    return out;
}

Values stdev(const Values &src, int n)
{
    Values sq(src.size());
    for (int i = 0; i < src.size(); ++i) sq[i] = src[i] * src[i];
    const Values m = sma(src, n), m2 = sma(sq, n);
    Values out(m.size());
    for (int i = 0; i < m.size(); ++i) out[i] = std::sqrt(js::max(0, m2[i] - m[i] * m[i]));
    return out;
}

// Mean absolute deviation (ta.dev), used by CCI.
Values dev(const Values &src, int n)
{
    const Values m = sma(src, n);
    Values out(m.size());
    for (int i = 0; i < m.size(); ++i) {
        const double mean = m[i];
        if (!isNum(mean)) {
            out[i] = NaN;
            continue;
        }
        double s = 0;
        for (int j = 0; j < n; ++j) s += std::fabs(at(src, qint64(i) - j) - mean);
        out[i] = s / n;
    }
    return out;
}

Values highest(const Values &src, int n)
{
    Values out = filled(src.size());
    for (int i = std::max(0, n - 1); i < src.size(); ++i) {
        double h = -js::Inf;
        for (int j = 0; j < n; ++j)
            if (src[i - j] > h) h = src[i - j];
        out[i] = h;
    }
    return out;
}

Values lowest(const Values &src, int n)
{
    Values out = filled(src.size());
    for (int i = std::max(0, n - 1); i < src.size(); ++i) {
        double l = js::Inf;
        for (int j = 0; j < n; ++j)
            if (src[i - j] < l) l = src[i - j];
        out[i] = l;
    }
    return out;
}

Values tr(const Bars &bars)
{
    Values out(bars.size());
    for (int i = 0; i < bars.size(); ++i) {
        const Bar &b = bars[i];
        out[i] = i == 0 ? b.high - b.low
                        : js::max(b.high - b.low, std::fabs(b.high - bars[i - 1].close),
                                  std::fabs(b.low - bars[i - 1].close));
    }
    return out;
}

Values atr(const Bars &bars, int n)
{
    return rma(tr(bars), n);
}

QVector<bool> crossover(const Values &a, const Values &b)
{
    QVector<bool> out(a.size());
    for (int i = 0; i < a.size(); ++i) out[i] = i > 0 && a[i] > at(b, i) && a[i - 1] <= at(b, i - 1);
    return out;
}
QVector<bool> crossover(const Values &a, double b)
{
    QVector<bool> out(a.size());
    for (int i = 0; i < a.size(); ++i) out[i] = i > 0 && a[i] > b && a[i - 1] <= b;
    return out;
}
QVector<bool> crossunder(const Values &a, const Values &b)
{
    QVector<bool> out(a.size());
    for (int i = 0; i < a.size(); ++i) out[i] = i > 0 && a[i] < at(b, i) && a[i - 1] >= at(b, i - 1);
    return out;
}
QVector<bool> crossunder(const Values &a, double b)
{
    QVector<bool> out(a.size());
    for (int i = 0; i < a.size(); ++i) out[i] = i > 0 && a[i] < b && a[i - 1] >= b;
    return out;
}

// JS rsiOf
Values rsi(const Values &src, int n)
{
    const Values ch = change(src);
    Values u(ch.size()), d(ch.size());
    for (int i = 0; i < ch.size(); ++i) {
        u[i] = isNum(ch[i]) ? js::max(ch[i], 0) : NaN;
        d[i] = isNum(ch[i]) ? js::max(-ch[i], 0) : NaN;
    }
    const Values up = rma(u, n), dn = rma(d, n);
    Values out(up.size());
    for (int i = 0; i < up.size(); ++i) {
        const double uu = up[i], dd = dn[i];
        if (!isNum(uu) || !isNum(dd)) out[i] = NaN;
        else if (dd == 0) out[i] = 100;
        else if (uu == 0) out[i] = 0;
        else out[i] = 100 - 100 / (1 + uu / dd);
    }
    return out;
}

} // namespace ta

namespace detail {

double jsonNumber(const QJsonValue &v)
{
    switch (v.type()) {
    case QJsonValue::Double: return v.toDouble();
    case QJsonValue::Bool: return v.toBool() ? 1 : 0;
    case QJsonValue::Null: return 0;
    case QJsonValue::String: {
        const QString s = v.toString().trimmed();
        if (s.isEmpty()) return 0;
        if (s == QLatin1String("Infinity") || s == QLatin1String("+Infinity")) return js::Inf;
        if (s == QLatin1String("-Infinity")) return -js::Inf;
        bool ok = false;
        double d;
        if (s.startsWith(QLatin1String("0x")) || s.startsWith(QLatin1String("0X")))
            d = double(s.mid(2).toULongLong(&ok, 16));
        else
            d = s.toDouble(&ok);
        return ok ? d : NaN;
    }
    default: return NaN;
    }
}

bool jsonTruthy(const QJsonValue &v)
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

Values stochOf(const Values &src, const Values &high, const Values &low, int n)
{
    const Values hh = ta::highest(high, n), ll = ta::lowest(low, n);
    Values out(src.size());
    for (int i = 0; i < src.size(); ++i) {
        const double h = at(hh, i), l = at(ll, i);
        out[i] = h - l == 0 ? NaN : (100 * (src[i] - l)) / (h - l);
    }
    return out;
}

Values cum(const Values &src)
{
    Values out(src.size());
    double s = 0;
    for (int i = 0; i < src.size(); ++i) out[i] = (s += isNum(src[i]) ? src[i] : 0);
    return out;
}

// Value computed at bar i is drawn at bar i + k (k may be negative).
Values shiftForward(const Values &src, int k)
{
    Values out = filled(src.size() + std::max(0, k));
    for (int i = 0; i < src.size(); ++i)
        if (i + k >= 0) out[i + k] = src[i];
    return out;
}

// highest/lowest skip a NaN inside their window; Pine gives na until the whole window is real.
Values whole(const Values &out, const Values &src, int n)
{
    Values r(out.size());
    for (int i = 0; i < out.size(); ++i) {
        double v = out[i];
        for (int j = 0; j < n; ++j)
            if (!isNum(at(src, qint64(i) - j))) {
                v = NaN;
                break;
            }
        r[i] = v;
    }
    return r;
}

// Pearson correlation of two equal-length arrays (ta.correlation). NaN when either one is flat.
double pearson(const Values &x, const Values &y)
{
    const int n = x.size();
    double sx = 0, sy = 0;
    for (int i = 0; i < n; ++i) {
        sx += x[i];
        sy += at(y, i);
    }
    const double mx = sx / n, my = sy / n;
    double sxy = 0, sxx = 0, syy = 0;
    for (int i = 0; i < n; ++i) {
        const double dx = x[i] - mx, dy = at(y, i) - my;
        sxy += dx * dy;
        sxx += dx * dx;
        syy += dy * dy;
    }
    return js::truthy(sxx) && js::truthy(syy) ? sxy / std::sqrt(sxx * syy) : NaN;
}

// Ranks 1..n from lowest to highest; tied values share the average of the ranks they span.
Values avgRanks(const Values &v)
{
    Values out(v.size());
    for (int i = 0; i < v.size(); ++i) {
        const double a = v[i];
        int less = 0, eq = 0;
        for (double b : v) {
            if (b < a) less++;
            else if (b == a) eq++;
        }
        out[i] = less + (eq + 1) / 2.0;
    }
    return out;
}

// Share of the n values before this one that are lower or equal, in percent (ta.percentrank).
Values percentRank(const Values &src, int n)
{
    Values out(src.size());
    for (int i = 0; i < src.size(); ++i) {
        const double v = src[i];
        if (i < n || !isNum(v)) {
            out[i] = NaN;
            continue;
        }
        int less = 0;
        bool bad = false;
        for (int j = 1; j <= n; ++j) {
            const double w = at(src, qint64(i) - j);
            if (!isNum(w)) {
                bad = true;
                break;
            }
            if (w <= v) less++;
        }
        out[i] = bad ? NaN : (100.0 * less) / n;
    }
    return out;
}

// Consecutive-close streak: +1, +2, ... on up closes, -1, -2, ... on down closes, 0 when unchanged.
Values updownStreak(const Values &src)
{
    Values out(src.size());
    double ud = 0;
    for (int i = 0; i < src.size(); ++i) {
        const double v = src[i];
        if (i == 0 || v == src[i - 1]) ud = 0;
        else if (v > src[i - 1]) ud = ud <= 0 ? 1 : ud + 1;
        else ud = ud >= 0 ? -1 : ud - 1;
        out[i] = ud;
    }
    return out;
}

// ta.swma: weights 1, 2, 2, 1 over the last four values.
Values swma(const Values &src)
{
    Values out(src.size());
    for (int i = 0; i < src.size(); ++i)
        out[i] = i >= 3 ? (src[i - 3] + 2 * src[i - 2] + 2 * src[i - 1] + src[i]) / 6 : NaN;
    return out;
}

// Offset (0 or negative) of the highest/lowest value within the last n bars, like ta.highestbars.
Values extremeBars(const Values &src, int n, bool higher)
{
    Values out = filled(src.size());
    for (int i = std::max(0, n - 1); i < src.size(); ++i) {
        double best = src[i];
        int off = 0;
        for (int j = 1; j < n; ++j) {
            const double v = src[i - j];
            if (higher ? v > best : v < best) {
                best = v;
                off = -j;
            }
        }
        out[i] = off;
    }
    return out;
}

// Break the line at a new period only when the last one had more than one bar. On a daily or
// longer chart with a daily anchor every bar is its own period, and breaking each one hid the line.
bool sessionBreak(bool hadKey, int start, int i)
{
    return hadKey && i - start > 1;
}

// ---------- candle patterns ----------
static IndicatorMarker marker(double time, const char *position, const char *shape, const char *color,
                              const QString &text = {})
{
    IndicatorMarker m;
    m.time = time;
    m.position = QLatin1String(position);
    m.shape = QLatin1String(shape);
    m.color = QLatin1String(color);
    m.text = text;
    return m;
}

static double orElse(double x, double d)
{
    return js::truthy(x) ? x : d;
}

QVector<IndicatorMarker> candlePatterns(const Bars &bars)
{
    QVector<IndicatorMarker> markers;
    for (int i = 2; i < bars.size(); ++i) {
        const Bar &b = bars[i], &p = bars[i - 1], &pp = bars[i - 2];
        const double body = std::fabs(b.close - b.open), range = orElse(b.high - b.low, 1e-12);
        const double upper = b.high - js::max(b.open, b.close), lower = js::min(b.open, b.close) - b.low;
        const double pBody = std::fabs(p.close - p.open);
        const bool bull = b.close > b.open, pBull = p.close > p.open;
        QString name;
        bool up = true;
        if (!pBull && bull && b.open <= p.close && b.close >= p.open && body > pBody)
            name = QStringLiteral("Engulfing");
        else if (pBull && !bull && b.open >= p.close && b.close <= p.open && body > pBody) {
            name = QStringLiteral("Engulfing");
            up = false;
        } else if (lower >= 2 * body && upper <= body * 0.5 && body / range < 0.35)
            name = QStringLiteral("Hammer");
        else if (upper >= 2 * body && lower <= body * 0.5 && body / range < 0.35) {
            name = QStringLiteral("Star");
            up = false;
        } else if (pp.close < pp.open && std::fabs(p.close - p.open) / orElse(p.high - p.low, 1) < 0.3 && bull
                   && b.close > (pp.open + pp.close) / 2)
            name = QStringLiteral("Star");
        else if (pp.close > pp.open && std::fabs(p.close - p.open) / orElse(p.high - p.low, 1) < 0.3 && !bull
                 && b.close < (pp.open + pp.close) / 2) {
            name = QStringLiteral("Star");
            up = false;
        }
        if (!name.isEmpty())
            markers.push_back(marker(b.time, up ? "belowBar" : "aboveBar", up ? "arrowUp" : "arrowDown",
                                     up ? "#26a69a" : "#ef5350", name));
    }
    return markers;
}

// Pivot highs and lows like ta.pivothigh / ta.pivotlow: a bar is a pivot once `right` later bars have closed and
// nothing in its window of left + right + 1 bars beats it. Ties go to the earlier bar: the pivot must be strictly
// beyond the `left` bars before it and at least level with the `right` bars after it. The label sits on the pivot
// bar itself and shows its price.
QVector<IndicatorMarker> pivotMarkers(const Bars &bars, int left, int right)
{
    QVector<IndicatorMarker> markers;
    for (int c = left; qint64(c) + right < bars.size(); ++c) {
        // Negative settings reach past the bars, where the JS reads a property of undefined.
        if (c < 0 || c >= bars.size())
            throw Error(QStringLiteral("Cannot read properties of undefined (reading '%1')")
                            .arg(left >= 1 || right >= 1 ? QStringLiteral("high") : QStringLiteral("time")));
        const Bar &b = bars[c];
        bool isHigh = true, isLow = true;
        for (int j = 1; j <= left; ++j) {
            if (bars[c - j].high >= b.high) isHigh = false;
            if (bars[c - j].low <= b.low) isLow = false;
        }
        for (int j = 1; j <= right; ++j) {
            if (bars[c + j].high > b.high) isHigh = false;
            if (bars[c + j].low < b.low) isLow = false;
        }
        if (isHigh) markers.push_back(marker(b.time, "aboveBar", "circle", "#ef5350", js::numberToString(b.high)));
        if (isLow) markers.push_back(marker(b.time, "belowBar", "circle", "#26a69a", js::numberToString(b.low)));
    }
    return markers;
}

// Williams fractals with the rules of TradingView's built-in: the n bars after the fractal bar must be strictly
// lower (higher for a down fractal); before it there must be n strictly lower bars, optionally behind up to four
// bars that only match it. Marked on the fractal bar, so the newest n bars never carry one.
QVector<IndicatorMarker> fractalMarkers(const Bars &bars, int n)
{
    QVector<IndicatorMarker> markers;
    // val(i) is "more extreme is higher": highs for an up fractal, negated lows for a down fractal.
    auto high = [&](qint64 i) { return (i >= 0 && i < bars.size()) ? bars[int(i)].high : NaN; };
    auto low = [&](qint64 i) { return (i >= 0 && i < bars.size()) ? -bars[int(i)].low : NaN; };
    auto isFractal = [&](int c, auto val) {
        const double x = val(c);
        for (int j = 1; j <= n; ++j)
            if (!(val(qint64(c) + j) < x)) return false;
        for (int k = 0; k <= 4; ++k) {
            bool ok = true;
            for (int j = 1; j <= k; ++j)
                if (!(val(qint64(c) - j) <= x)) ok = false;
            for (int j = 1; j <= n && ok; ++j)
                if (!(val(qint64(c) - k - j) < x)) ok = false;
            if (ok) return true;
        }
        return false;
    };
    if (n < 0 && 2 * qint64(n) < bars.size()) throw Error(QStringLiteral("Cannot read properties of undefined (reading 'time')"));
    for (int c = std::max(0, n); c + n < bars.size(); ++c) {
        if (isFractal(c, high)) markers.push_back(marker(bars[c].time, "aboveBar", "arrowDown", "#ef5350"));
        if (isFractal(c, low)) markers.push_back(marker(bars[c].time, "belowBar", "arrowUp", "#26a69a"));
    }
    return markers;
}

Macd macdFrom(const Values &s, int fast, int slow, int signal)
{
    const Values f = ta::ema(s, fast), sl = ta::ema(s, slow);
    Macd r;
    r.macd.resize(f.size());
    for (int i = 0; i < f.size(); ++i) r.macd[i] = f[i] - sl[i];
    r.signal = ta::ema(r.macd, signal);
    r.hist.resize(r.macd.size());
    for (int i = 0; i < r.macd.size(); ++i) r.hist[i] = r.macd[i] - r.signal[i];
    r.colors.reserve(r.hist.size());
    for (int i = 0; i < r.hist.size(); ++i) {
        const double v = r.hist[i], prev = i ? r.hist[i - 1] : 0;
        if (v >= 0) r.colors << (v >= prev ? QStringLiteral("#26a69a") : QStringLiteral("#b2dfdb"));
        else r.colors << (v <= prev ? QStringLiteral("#ef5350") : QStringLiteral("#ffcdd2"));
    }
    return r;
}

Dmi dmiOf(const Bars &bars, int diLen, int adxLen)
{
    const Values t = ta::tr(bars);
    const int n = bars.size();
    Values plusDM(n), minusDM(n), tt(n);
    for (int i = 0; i < n; ++i) {
        if (!i) {
            plusDM[i] = minusDM[i] = tt[i] = NaN;
            continue;
        }
        const double up = bars[i].high - bars[i - 1].high, down = bars[i - 1].low - bars[i].low;
        plusDM[i] = up > down && up > 0 ? up : 0;
        minusDM[i] = down > up && down > 0 ? down : 0;
        tt[i] = t[i];
    }
    const Values trur = ta::rma(tt, diLen);
    Dmi r;
    r.plus = ta::rma(plusDM, diLen);
    r.minus = ta::rma(minusDM, diLen);
    for (int i = 0; i < n; ++i) {
        r.plus[i] = (100 * r.plus[i]) / trur[i];
        r.minus[i] = (100 * r.minus[i]) / trur[i];
    }
    Values dx(n);
    for (int i = 0; i < n; ++i) {
        const double s = r.plus[i] + r.minus[i];
        dx[i] = std::fabs(r.plus[i] - r.minus[i]) / (s == 0 ? 1 : s);
    }
    r.adx = ta::rma(dx, adxLen);
    for (double &v : r.adx) v = 100 * v;
    return r;
}

Values psarOf(const Bars &bars, double start, double inc, double max)
{
    Values out = filled(bars.size());
    if (bars.size() < 2) return out;
    bool up = bars[1].close >= bars[0].close;
    double af = start, ep = up ? bars[0].high : bars[0].low, sar = up ? bars[0].low : bars[0].high;
    for (int i = 1; i < bars.size(); ++i) {
        const Bar &b = bars[i], &p = bars[i - 1];
        sar = sar + af * (ep - sar);
        if (up) {
            sar = js::min(sar, p.low, i > 1 ? bars[i - 2].low : p.low);
            if (b.low < sar) {
                up = false;
                sar = ep;
                ep = b.low;
                af = start;
            } else if (b.high > ep) {
                ep = b.high;
                af = js::min(af + inc, max);
            }
        } else {
            sar = js::max(sar, p.high, i > 1 ? bars[i - 2].high : p.high);
            if (b.high > sar) {
                up = true;
                sar = ep;
                ep = b.high;
                af = start;
            } else if (b.low < ep) {
                ep = b.low;
                af = js::min(af + inc, max);
            }
        }
        out[i] = sar;
    }
    return out;
}

Supertrend supertrendOf(const Bars &bars, double factor, int atrLen)
{
    const Values a = ta::atr(bars, atrLen);
    Supertrend r { filled(bars.size()), filled(bars.size()) };
    double prevUpper = NaN, prevLower = NaN, prevSt = NaN;
    for (int i = 0; i < bars.size(); ++i) {
        const Bar &b = bars[i];
        const double mid = (b.high + b.low) / 2;
        if (!isNum(a[i])) continue;
        double upper = mid + factor * a[i], lower = mid - factor * a[i];
        const double prevClose = i ? bars[i - 1].close : NaN;
        if (isNum(prevLower)) lower = lower > prevLower || prevClose < prevLower ? lower : prevLower;
        if (isNum(prevUpper)) upper = upper < prevUpper || prevClose > prevUpper ? upper : prevUpper;
        int d;
        if (!isNum(prevSt)) d = 1;
        else if (prevSt == prevUpper) d = b.close > upper ? -1 : 1;
        else d = b.close < lower ? 1 : -1;
        r.st[i] = d == -1 ? lower : upper;
        r.dir[i] = d;
        prevUpper = upper;
        prevLower = lower;
        prevSt = r.st[i];
    }
    return r;
}

// Volatility Stop: a trailing stop that rides the best close of the trend minus a multiple of ATR, and flips
// (restarting from the closing price) when the close crosses it. While ATR is still warming up it uses the raw true range.
Vstop vstopOf(const Bars &bars, int length, double factor)
{
    const Values a = ta::atr(bars, length), t = ta::tr(bars);
    Vstop r { filled(bars.size()), filled(bars.size()) };
    double max = NaN, min = NaN, stop = NaN;
    bool uptrend = true;
    for (int i = 0; i < bars.size(); ++i) {
        const Bar &b = bars[i];
        const double m = isNum(a[i]) ? a[i] * factor : t[i];
        max = i ? js::max(max, b.close) : b.close;
        min = i ? js::min(min, b.close) : b.close;
        stop = !isNum(stop) ? b.close : uptrend ? js::max(stop, max - m) : js::min(stop, min + m);
        const bool was = uptrend;
        uptrend = b.close - stop >= 0;
        if (uptrend != was) {
            max = min = b.close;
            stop = uptrend ? max - m : min + m;
        }
        (uptrend ? r.up : r.down)[i] = stop;
    }
    return r;
}

// Zig Zag: follow the current leg's extreme and call it a pivot once price has reversed `deviation` percent from it,
// provided the extreme is at least `depth` bars after the previous pivot. A move through the previous pivot's
// price before that happens means the pivot was not the end of its swing, so it moves to the new extreme.
// The line runs straight between pivots and the unconfirmed last leg ends at its latest extreme.
Values zigzagOf(const Bars &bars, double deviation, double depth)
{
    const int n = bars.size();
    Values out = filled(n);
    const double dev = deviation / 100;
    struct Piv {
        int i;
        double price;
    };
    QVector<Piv> piv;   // confirmed pivots, alternating high and low
    int dir = 0;        // 0 no leg yet, 1 rising (tracking the high), -1 falling (tracking the low)
    int ext = 0;        // bar of the current leg's extreme
    int hiI = 0, loI = 0;   // running high and low while no leg has been confirmed
    for (int i = 1; i < n; ++i) {
        const Bar &b = bars[i];
        if (dir == 0) {
            if (b.high > bars[hiI].high) hiI = i;
            if (b.low < bars[loI].low) loI = i;
            if (i > hiI && b.low <= bars[hiI].high * (1 - dev)) {
                piv.push_back({ hiI, bars[hiI].high });
                dir = -1;
                ext = i;
            } else if (i > loI && b.high >= bars[loI].low * (1 + dev)) {
                piv.push_back({ loI, bars[loI].low });
                dir = 1;
                ext = i;
            }
            continue;
        }
        const Piv last = piv.last();
        if (dir == 1) {
            if (b.high > bars[ext].high) ext = i;
            else if (b.low <= bars[ext].high * (1 - dev) && ext - last.i >= depth) {
                piv.push_back({ ext, bars[ext].high });
                dir = -1;
                ext = i;
            } else if (b.low < last.price) {
                piv.last() = { i, b.low };
                ext = i;
            }
        } else if (b.low < bars[ext].low)
            ext = i;
        else if (b.high >= bars[ext].low * (1 + dev) && ext - last.i >= depth) {
            piv.push_back({ ext, bars[ext].low });
            dir = 1;
            ext = i;
        } else if (b.high > last.price) {
            piv.last() = { i, b.high };
            ext = i;
        }
    }
    if (dir != 0 && ext > piv.last().i) piv.push_back({ ext, dir == 1 ? bars[ext].high : bars[ext].low });
    for (int k = 0; k < piv.size(); ++k) {
        const Piv &a = piv[k];
        out[a.i] = a.price;
        if (k + 1 < piv.size()) {
            const Piv &b = piv[k + 1];
            for (int i = a.i + 1; i < b.i; ++i)
                out[i] = a.price + ((b.price - a.price) * (i - a.i)) / (b.i - a.i);
        }
    }
    return out;
}

IndicatorResult pivotsOf(const Bars &bars, const QString &period)
{
    QString per = period;
    if (per == QLatin1String("auto")) {
        const double step = bars.size() > 1 ? bars[1].time - bars[0].time : 86400;
        per = step <= 900 ? QStringLiteral("day")
              : step < 86400 ? QStringLiteral("week")
              : step < 7 * 86400 ? QStringLiteral("month")
                                 : QStringLiteral("year");
    }
    const int n = bars.size();
    Values P = filled(n), r1 = filled(n), s1 = filled(n), r2 = filled(n), s2 = filled(n), r3 = filled(n),
           s3 = filled(n);
    QVector<bool> colorsBreak(n, false);
    QString key;
    bool hasKey = false, hasCur = false, hasPrev = false;
    struct HLC {
        double h, l, c;
    } cur {}, prev {};
    for (int i = 0; i < n; ++i) {
        const Bar &b = bars[i];
        const QString k = periodKey(b.time, per);
        if (!hasKey || k != key) {
            if (hasCur) {
                prev = cur;
                hasPrev = true;
            }
            cur = { b.high, b.low, b.close };
            hasCur = true;
            key = k;
            hasKey = true;
            colorsBreak[i] = true;
        } else {
            cur.h = js::max(cur.h, b.high);
            cur.l = js::min(cur.l, b.low);
            cur.c = b.close;
        }
        if (hasPrev) {
            const double pv = (prev.h + prev.l + prev.c) / 3;
            P[i] = pv;
            r1[i] = 2 * pv - prev.l;
            s1[i] = 2 * pv - prev.h;
            r2[i] = pv + (prev.h - prev.l);
            s2[i] = pv - (prev.h - prev.l);
            r3[i] = prev.h + 2 * (pv - prev.l);
            s3[i] = prev.l - 2 * (prev.h - pv);
        }
    }
    IndicatorResult r;
    r.values.insert(QStringLiteral("p"), P);
    r.values.insert(QStringLiteral("r1"), r1);
    r.values.insert(QStringLiteral("s1"), s1);
    r.values.insert(QStringLiteral("r2"), r2);
    r.values.insert(QStringLiteral("s2"), s2);
    r.values.insert(QStringLiteral("r3"), r3);
    r.values.insert(QStringLiteral("s3"), s3);
    r.breaks = colorsBreak;
    return r;
}

// 300 made-up daily bars, the same every time, for the trial run.
Bars trialBars(int n)
{
    Bars out;
    double price = 100, seed = 42;
    auto rand = [&]() { return (seed = std::fmod(seed * 16807, 2147483647)) / 2147483647; };
    for (int i = 0; i < n; ++i) {
        const double open = price;
        price = js::max(1, price * (1 + (rand() - 0.5) * 0.04));
        const double high = js::max(open, price) * (1 + rand() * 0.01);
        const double low = js::min(open, price) * (1 - rand() * 0.01);
        Bar b;
        b.time = 1577836800 + double(i) * 86400;
        b.open = open;
        b.high = high;
        b.low = low;
        b.close = price;
        b.volume = js::round(1000 + rand() * 9000);
        out.push_back(b);
    }
    return out;
}

QJsonObject markerToJson(const IndicatorMarker &m)
{
    QJsonObject o { { "time", m.time }, { "position", m.position }, { "shape", m.shape } };
    if (!m.color.isEmpty()) o.insert("color", m.color);
    if (!m.text.isEmpty()) o.insert("text", m.text);
    return o;
}

} // namespace detail

// Period key used by VWAP resets and pivot points. Times are UTC seconds.
QString periodKey(double t, const QString &period)
{
    const js::UtcParts d = js::utcParts(t * 1000);
    if (period == QLatin1String("day"))
        return js::numberToString(d.year) + QLatin1Char('-') + js::numberToString(d.month) + QLatin1Char('-')
               + js::numberToString(d.day);
    if (period == QLatin1String("month")) return js::numberToString(d.year) + QLatin1Char('-') + js::numberToString(d.month);
    if (period == QLatin1String("year")) return js::numberToString(d.year);
    // ISO week, Monday start
    const double day = std::floor(t / 86400);
    return QLatin1Char('w') + js::numberToString(std::floor((day + 3) / 7));
}

} // namespace qtm
