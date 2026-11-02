// Timeframes, resampling and chart-type transforms. Port of quant-terminal/public/lib/series.js.
#include "Series.h"

#include "Error.h"
#include "JsCompat.h"

#include <QRegularExpression>

#include <algorithm>

#if defined(__clang__)
#pragma clang fp contract(off)
#endif

namespace qtm {

namespace {

qint64 unitSec(QChar u)
{
    switch (u.unicode()) {
    case 's': return 1;
    case 'm': return 60;
    case 'h': return 3600;
    case 'D': return 86400;
    case 'W': return 604800;
    case 'M': return 2592000;
    }
    return 0;
}

// JS `%` on integers keeps the sign of the dividend, like C++.
qint64 floorDiv(qint64 a, qint64 b)
{
    qint64 q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) --q;
    return q;
}

} // namespace

// "15m" -> { n: 15, unit: 'm', seconds: 900 }. Accepts 1s..59s, minutes, hours, D, W, M.
Tf parseTf(const QString &tf)
{
    static const QRegularExpression re(QStringLiteral("^([0-9]+)?([smhDWM])$"));
    const auto m = re.match(tf.trimmed());
    if (!m.hasMatch())
        throw Error(QStringLiteral("Bad timeframe \"%1\". Use forms like 1s, 5m, 4h, 1D, 1W, 1M.").arg(tf));
    const double n = m.captured(1).isEmpty() ? 1 : m.captured(1).toDouble();
    if (n < 1) throw Error(QStringLiteral("Bad timeframe \"%1\"").arg(tf));
    Tf r;
    r.n = int(n);
    r.unit = m.captured(2).at(0);
    r.seconds = qint64(n) * unitSec(r.unit);
    return r;
}

bool isIntraday(const QString &tf)
{
    return parseTf(tf).seconds < 86400;
}

// Start of the bucket a bar at time t (UTC seconds) belongs to.
qint64 bucketStart(qint64 t, const QString &tf)
{
    const Tf p = parseTf(tf);
    const qint64 n = p.n;
    if (p.unit == QLatin1Char('M')) {
        const js::UtcParts d = js::utcParts(double(t) * 1000);
        const qint64 months = qint64(d.year) * 12 + qint64(d.month);
        const qint64 start = months - (months % n);
        return qint64(js::dateUtc(double(floorDiv(start, 12)), double(start % 12), 1) / 1000);
    }
    if (p.unit == QLatin1Char('W')) {
        const qint64 day = floorDiv(t, 86400);
        const qint64 monday = day - ((day + 3) % 7);   // 1970-01-01 was a Thursday
        const qint64 week = floorDiv(monday, 7);
        return (monday - (week % n) * 7) * 86400;
    }
    const qint64 size = n * unitSec(p.unit);
    return floorDiv(t, size) * size;
}

// Merge bars into a larger timeframe. Bars must be sorted ascending.
Bars resample(const Bars &bars, const QString &tf)
{
    Bars out;
    Bar *cur = nullptr;
    for (const Bar &b : bars) {
        const qint64 start = bucketStart(qint64(b.time), tf);
        if (!cur || cur->time != double(start)) {
            Bar n;
            n.time = double(start);
            n.open = b.open;
            n.high = b.high;
            n.low = b.low;
            n.close = b.close;
            n.volume = js::orZero(b.volume);
            out.push_back(n);
            cur = &out.last();
        } else {
            cur->high = js::max(cur->high, b.high);
            cur->low = js::min(cur->low, b.low);
            cur->close = b.close;
            cur->volume += js::orZero(b.volume);
        }
    }
    return out;
}

// Pick the largest native timeframe that divides the requested one.
QString nativeBase(const QString &tf, const QStringList &natives)
{
    const Tf want = parseTf(tf);
    if (natives.contains(tf)) return tf;
    struct C {
        QString tf;
        Tf p;
    };
    QVector<C> cands;
    for (const QString &n : natives) cands.push_back({ n, parseTf(n) });
    QVector<C> keep;
    for (const C &c : cands) {
        bool ok;
        if (want.unit == QLatin1Char('M'))
            ok = c.p.unit == QLatin1Char('M') ? want.n % c.p.n == 0 : c.p.seconds <= 86400;
        else if (want.unit == QLatin1Char('W'))
            ok = c.p.unit == QLatin1Char('W') ? want.n % c.p.n == 0
                                              : c.p.seconds <= 86400 && 86400 % c.p.seconds == 0;
        else
            ok = c.p.unit != QLatin1Char('M') && c.p.unit != QLatin1Char('W') && c.p.seconds <= want.seconds
                 && want.seconds % c.p.seconds == 0;
        if (ok) keep.push_back(c);
    }
    std::stable_sort(keep.begin(), keep.end(), [](const C &a, const C &b) { return b.p.seconds < a.p.seconds; });
    return keep.isEmpty() ? QString() : keep.first().tf;
}

Bars heikinAshi(const Bars &bars)
{
    Bars out;
    out.reserve(bars.size());
    for (int i = 0; i < bars.size(); ++i) {
        const Bar &b = bars[i];
        const double close = (b.open + b.high + b.low + b.close) / 4;
        const double open = i == 0 ? (b.open + b.close) / 2 : (out[i - 1].open + out[i - 1].close) / 2;
        Bar h;
        h.time = b.time;
        h.open = open;
        h.close = close;
        h.high = js::max(b.high, open, close);
        h.low = js::min(b.low, open, close);
        h.volume = b.volume;
        out.push_back(h);
    }
    return out;
}

// Renko from closes. boxSize 0 = ATR(14) of the last bar. Bricks get unique, increasing times.
Bars renko(const Bars &bars, double boxSize)
{
    if (bars.size() < 2) return {};
    double box = boxSize;
    if (!js::truthy(box)) {
        double s = 0;
        const int n = std::min<int>(14, bars.size() - 1);
        for (int i = bars.size() - n; i < bars.size(); ++i) {
            const Bar &p = bars[i - 1], &b = bars[i];
            s += js::max(b.high - b.low, std::fabs(b.high - p.close), std::fabs(b.low - p.close));
        }
        box = s / n;
        if (!js::truthy(box)) box = bars.last().close * 0.01;
    }
    Bars out;
    double base = bars[0].close, lastTime = -js::Inf;
    auto push = [&](double t, double open, double close) {
        const double time = js::max(t, lastTime + 1);
        lastTime = time;
        Bar b;
        b.time = time;
        b.open = open;
        b.close = close;
        b.high = js::max(open, close);
        b.low = js::min(open, close);
        out.push_back(b);
    };
    int dir = 0;
    for (const Bar &b : bars) {
        while (true) {
            if (b.close >= base + box * (dir == -1 ? 2 : 1)) {
                const double open = dir == -1 ? base + box : base;
                push(b.time, open, open + box);
                base = open + box;
                dir = 1;
            } else if (b.close <= base - box * (dir == 1 ? 2 : 1)) {
                const double open = dir == 1 ? base - box : base;
                push(b.time, open, open - box);
                base = open - box;
                dir = -1;
            } else
                break;
        }
    }
    return out;
}

// Three-line break.
Bars lineBreak(const Bars &bars, int lines)
{
    Bars out;
    double lastTime = -js::Inf;
    for (const Bar &b : bars) {
        if (out.isEmpty()) {
            Bar f;
            f.time = b.time;
            f.open = b.open;
            f.close = b.close;
            f.high = js::max(b.open, b.close);
            f.low = js::min(b.open, b.close);
            out.push_back(f);
            lastTime = b.time;
            continue;
        }
        // out.slice(-lines): the last `lines` bricks (all of them when lines <= 0 or too many).
        const int from = lines > 0 ? std::max<int>(0, out.size() - lines) : std::min<int>(out.size(), -lines);
        double hi = -js::Inf, lo = js::Inf;
        for (int k = from; k < out.size(); ++k) {
            hi = js::max(hi, out[k].high);
            lo = js::min(lo, out[k].low);
        }
        const Bar &last = out.last();
        const bool up = last.close > last.open;
        bool brick = false;
        double bo = 0, bc = 0;
        if (b.close > (up ? last.close : hi)) {
            brick = true;
            bo = up ? last.close : last.open;
            bc = b.close;
        } else if (b.close < (up ? lo : last.close)) {
            brick = true;
            bo = up ? last.open : last.close;
            bc = b.close;
        }
        if (brick) {
            const double time = js::max(b.time, lastTime + 1);
            lastTime = time;
            Bar n;
            n.time = time;
            n.open = bo;
            n.close = bc;
            n.high = js::max(bo, bc);
            n.low = js::min(bo, bc);
            out.push_back(n);
        }
    }
    return out;
}

QString toCsv(const Bars &bars, const QVector<QPair<QString, Values>> &extraCols)
{
    QStringList head { "time", "iso", "open", "high", "low", "close", "volume" };
    for (const auto &c : extraCols) head << c.first;
    QStringList lines { head.join(QLatin1Char(',')) };
    for (int i = 0; i < bars.size(); ++i) {
        const Bar &b = bars[i];
        QStringList row { js::numberToString(b.time), js::toIsoString(b.time * 1000), js::numberToString(b.open),
                          js::numberToString(b.high), js::numberToString(b.low), js::numberToString(b.close),
                          js::numberToString(b.volume) };
        for (const auto &c : extraCols) {
            const double v = i < c.second.size() ? c.second[i] : NaN;
            row << (std::isnan(v) ? QString() : js::numberToString(v));
        }
        lines << row.join(QLatin1Char(','));
    }
    return lines.join(QLatin1Char('\n'));
}

// Average true range of the last 14 bars, the default box/range size for the price-only charts.
static double recentAtr(const Bars &bars, int n = 14)
{
    const int k = std::min<int>(n, bars.size() - 1);
    if (k < 1) return bars.isEmpty() ? 1 : bars[0].close * 0.01;
    double s = 0;
    for (int i = bars.size() - k; i < bars.size(); ++i) {
        const Bar &p = bars[i - 1], &b = bars[i];
        s += js::max(b.high - b.low, std::fabs(b.high - p.close), std::fabs(b.low - p.close));
    }
    const double r = s / k;
    return js::truthy(r) ? r : bars.last().close * 0.01;
}

namespace {
struct UniqueTimes {
    double last = -js::Inf;
    double operator()(double t) { return last = js::max(t, last + 1); }
};
} // namespace

// Range bars: a new bar starts each time price has covered `range` (default: ATR 14). Each source
// bar is walked open -> low -> high -> close (or open -> high -> low -> close on down bars).
Bars rangeBars(const Bars &bars, double range)
{
    if (bars.isEmpty()) return {};
    const double R = js::truthy(range) ? range : recentAtr(bars);
    Bars out;
    UniqueTimes stamp;
    Bar cur;
    bool have = false;
    auto emitBar = [&]() {
        Bar e = cur;
        e.time = stamp(cur.time);
        out.push_back(e);
    };
    auto fresh = [](double time, double p) {
        Bar b;
        b.time = time;
        b.open = b.high = b.low = b.close = p;
        b.volume = 0;
        return b;
    };
    auto feed = [&](double price, double time) {
        if (!have) {
            cur = fresh(time, price);
            have = true;
            return;
        }
        for (;;) {
            if (price > cur.high && price - cur.low >= R) {
                const double top = cur.low + R;
                cur.high = top;
                cur.close = top;
                emitBar();
                cur = fresh(time, top);
            } else if (price < cur.low && cur.high - price >= R) {
                const double bot = cur.high - R;
                cur.low = bot;
                cur.close = bot;
                emitBar();
                cur = fresh(time, bot);
            } else {
                cur.high = js::max(cur.high, price);
                cur.low = js::min(cur.low, price);
                cur.close = price;
                return;
            }
        }
    };
    for (const Bar &b : bars) {
        const bool upBar = b.close >= b.open;
        const double path[4] = { b.open, upBar ? b.low : b.high, upBar ? b.high : b.low, b.close };
        for (double p : path) feed(p, b.time);
        cur.volume += js::orZero(b.volume);
    }
    emitBar();
    return out;
}

// Kagi: turning points of the close, reversing after `reversal` (default ATR 14). Yang (green)
// once the line breaks the previous high, yin (red) once it breaks the previous low.
Bars kagi(const Bars &bars, double reversal)
{
    if (bars.size() < 2) return {};
    const double R = js::truthy(reversal) ? reversal : recentAtr(bars);
    UniqueTimes stamp;
    struct Pt {
        double time, value;
        bool yang;
    };
    QVector<Pt> pts;
    int dir = 0;
    double extreme = bars[0].close, extremeTime = bars[0].time;
    bool yang = true;
    double lastHigh = js::Inf, lastLow = -js::Inf;
    auto push = [&](double time, double v) { pts.push_back({ stamp(time), v, yang }); };
    push(bars[0].time, bars[0].close);
    for (int i = 1; i < bars.size(); ++i) {
        const Bar &b = bars[i];
        const double c = b.close;
        if (dir >= 0 && c > extreme) {
            extreme = c;
            extremeTime = b.time;
            if (dir == 0) dir = 1;
            if (c > lastHigh) yang = true;
            continue;
        }
        if (dir <= 0 && c < extreme) {
            extreme = c;
            extremeTime = b.time;
            if (dir == 0) dir = -1;
            if (c < lastLow) yang = false;
            continue;
        }
        if (dir == 1 && extreme - c >= R) {
            push(extremeTime, extreme);
            lastHigh = extreme;
            dir = -1;
            extreme = c;
            extremeTime = b.time;
            if (c < lastLow) yang = false;
        } else if (dir == -1 && c - extreme >= R) {
            push(extremeTime, extreme);
            lastLow = extreme;
            dir = 1;
            extreme = c;
            extremeTime = b.time;
            if (c > lastHigh) yang = true;
        }
    }
    push(extremeTime, extreme);
    Bars out;
    out.reserve(pts.size());
    for (const Pt &p : pts) {
        Bar b;
        b.time = p.time;
        b.open = b.high = b.low = b.close = p.value;
        b.color = p.yang ? QStringLiteral("#089981") : QStringLiteral("#f23645");
        out.push_back(b);
    }
    return out;
}

// Point & figure from closes: X columns up, O columns down, a box of `box` (default ATR 14) and a
// `reversal`-box turn. Each column comes back as one candle spanning the column.
Bars pointFigure(const Bars &bars, double box, int reversal)
{
    if (bars.size() < 2) return {};
    const double B = js::truthy(box) ? box : recentAtr(bars);
    UniqueTimes stamp;
    auto fl = [B](double v) { return std::floor(v / B + 1e-9) * B; };
    auto ce = [B](double v) { return std::ceil(v / B - 1e-9) * B; };
    struct Col {
        int dir;
        double lo, hi, time;
    };
    QVector<Col> cols;
    const double base = fl(bars[0].close);
    Col col {};
    bool have = false;
    for (const Bar &b : bars) {
        const double c = b.close;
        if (!have) {
            if (c >= base + B) {
                col = { 1, base, fl(c), b.time };
                have = true;
            } else if (c <= base - B) {
                col = { -1, ce(c), base, b.time };
                have = true;
            }
            continue;
        }
        if (col.dir == 1) {
            if (c >= col.hi + B) col.hi = fl(c);
            else if (c <= col.hi - reversal * B) {
                cols.push_back(col);
                col = { -1, ce(c), col.hi - B, b.time };
            }
        } else if (c <= col.lo - B)
            col.lo = ce(c);
        else if (c >= col.lo + reversal * B) {
            cols.push_back(col);
            col = { 1, col.lo + B, fl(c), b.time };
        }
    }
    if (have) cols.push_back(col);
    Bars out;
    out.reserve(cols.size());
    for (const Col &k : cols) {
        Bar b;
        b.time = stamp(k.time);
        b.open = k.dir == 1 ? k.lo : k.hi;
        b.close = k.dir == 1 ? k.hi : k.lo;
        b.high = k.hi;
        b.low = k.lo;
        out.push_back(b);
    }
    return out;
}

} // namespace qtm
