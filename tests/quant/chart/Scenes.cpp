#include "Scenes.h"

#include <QDateTime>
#include <QLocale>
#include <QTimeZone>
#include <QPainter>
#include <QPainterPath>
#include <QStringList>

#include <cmath>
#include <cstdint>

using namespace lw;

namespace {

// mulberry32
struct Rng {
    std::uint32_t a;
    double next()
    {
        a += 0x6D2B79F5u;
        std::uint32_t t = a;
        t = (t ^ (t >> 15)) * (t | 1u);
        t ^= t + (t ^ (t >> 7)) * (t | 61u);
        return double((t ^ (t >> 14))) / 4294967296.0;
    }
    double normal()
    {
        const double u = std::max(next(), 1e-12), v = next();
        return std::sqrt(-2 * std::log(u)) * std::cos(2 * M_PI * v);
    }
};

double r2(double v) { return std::round(v * 100) / 100; }

QVector<Bar> walk(std::uint32_t seed, double start, int n, double t0, double step, double vol,
                  double drift = 0, int decimals = 2)
{
    Rng rng{seed};
    QVector<Bar> out;
    double close = start;
    const double k = std::pow(10, decimals);
    auto rd = [k](double v) { return std::round(v * k) / k; };
    for (int i = 0; i < n; ++i) {
        const double open = close * (1 + rng.normal() * vol * 0.15);
        const double c = open * (1 + drift + rng.normal() * vol);
        const double hi = std::max(open, c) * (1 + std::abs(rng.normal()) * vol * 0.5);
        const double lo = std::min(open, c) * (1 - std::abs(rng.normal()) * vol * 0.5);
        const double v = std::round(1000 + rng.next() * 4000 + std::abs(c - open) / c * 200000);
        out.push_back({t0 + i * step, rd(open), rd(hi), rd(lo), rd(c), v});
        close = c;
    }
    return out;
}

QVector<double> ema(const QVector<Bar> &b, int n)
{
    QVector<double> out(b.size(), NaN);
    const double k = 2.0 / (n + 1);
    double prev = 0;
    for (int i = 0; i < b.size(); ++i) {
        if (i < n - 1) {
            prev += b[i].close;
            continue;
        }
        if (i == n - 1) {
            prev = (prev + b[i].close) / n;
        } else {
            prev = b[i].close * k + prev * (1 - k);
        }
        out[i] = prev;
    }
    return out;
}

QVector<double> rsi(const QVector<Bar> &b, int n)
{
    QVector<double> out(b.size(), NaN);
    double g = 0, l = 0;
    for (int i = 1; i < b.size(); ++i) {
        const double ch = b[i].close - b[i - 1].close;
        const double up = std::max(ch, 0.0), dn = std::max(-ch, 0.0);
        if (i <= n) {
            g += up;
            l += dn;
            if (i == n) {
                g /= n;
                l /= n;
                out[i] = l == 0 ? 100 : 100 - 100 / (1 + g / l);
            }
        } else {
            g = (g * (n - 1) + up) / n;
            l = (l * (n - 1) + dn) / n;
            out[i] = l == 0 ? 100 : 100 - 100 / (1 + g / l);
        }
    }
    return out;
}

QString num(double v)
{
    if (!std::isfinite(v))
        return QStringLiteral("null");
    return QString::number(v, 'g', 17);
}

QString barsJs(const QVector<Bar> &b)
{
    QStringList parts;
    for (const Bar &x : b)
        parts << QStringLiteral("[%1,%2,%3,%4,%5,%6]")
                     .arg(num(x.time), num(x.open), num(x.high), num(x.low), num(x.close), num(x.volume));
    return '[' + parts.join(',') + ']';
}

QString valuesJs(const QVector<double> &v)
{
    QStringList parts;
    for (double x : v)
        parts << num(x);
    return '[' + parts.join(',') + ']';
}

// --- theme (public/css/app.css, dark)
const QColor kBg("#131722"), kText("#d1d4dc"), kBorder("#2a2e39"), kUp("#089981"),
    kDown("#f23645"), kAccent("#2962ff"), kMuted("#787b86");
const QColor kGrid = cssColor("rgba(42,46,57,0.6)");
const QColor kAccentSoft = cssColor("rgba(41,98,255,0.18)");

ChartOptions themeOptions(int fontSize = 11)
{
    ChartOptions o;
    o.background = kBg;
    o.textColor = kText;
    o.fontSize = fontSize;
    o.separatorColor = kBorder;
    o.separatorHoverColor = kAccentSoft;
    o.enablePaneResize = true;
    o.vertLinesColor = kGrid;
    o.horzLinesColor = kGrid;
    o.rightPriceScaleBorderColor = kBorder;
    o.timeScaleBorderColor = kBorder;
    return o;
}

// chart.js createChart()
ChartOptions mainOptions()
{
    ChartOptions o = themeOptions(11);
    o.crosshairMode = CrosshairMode::Normal;
    o.rightOffset = 8;
    o.timeVisible = true;
    o.secondsVisible = false;
    o.priceFormatter = [](double p) { return fmtPrice(p); };
    return o;
}

QVector<DataItem> ohlc(const QVector<Bar> &bars)
{
    QVector<DataItem> out;
    for (const Bar &b : bars) {
        DataItem d;
        d.time = b.time;
        d.open = b.open;
        d.high = b.high;
        d.low = b.low;
        d.close = b.close;
        out.push_back(d);
    }
    return out;
}

QVector<DataItem> closes(const QVector<Bar> &bars)
{
    QVector<DataItem> out;
    for (const Bar &b : bars) {
        DataItem d;
        d.time = b.time;
        d.value = b.close;
        out.push_back(d);
    }
    return out;
}

// chart.js lineData(): gaps hidden with transparent points
QVector<DataItem> lineData(const QVector<Bar> &bars, const QVector<double> &vals)
{
    QVector<DataItem> out;
    const QColor clear(0, 0, 0, 0);
    QVector<double> next(vals.size(), NaN);
    double nv = NaN;
    for (int i = vals.size() - 1; i >= 0; --i) {
        if (std::isfinite(vals[i]))
            nv = vals[i];
        next[i] = nv;
    }
    bool started = false;
    for (int i = 0; i < vals.size(); ++i) {
        DataItem d;
        d.time = bars[i].time;
        if (!std::isfinite(vals[i])) {
            if (started && std::isfinite(next[i])) {
                d.value = next[i];
                d.color = clear;
            }
            out.push_back(d);
            continue;
        }
        started = true;
        const bool gapAhead = i + 1 < vals.size() && !std::isfinite(vals[i + 1]);
        d.value = vals[i];
        if (gapAhead)
            d.color = clear;
        out.push_back(d);
    }
    return out;
}

void setPrecision(Series *s, double last)
{
    const int precision = last >= 100 ? 2 : last >= 1 ? 4 : last >= 0.01 ? 6 : 8;
    auto o = s->options();
    o.priceFormat.type = PriceFormatType::Price;
    o.priceFormat.precision = precision;
    o.priceFormat.minMove = 1 / std::pow(10, precision);
    s->applyOptions(o);
}

Series *addVolume(Chart *c, const QVector<Bar> &bars)
{
    SeriesOptions vo;
    vo.priceScaleId = "vol";
    vo.priceFormat.type = PriceFormatType::Volume;
    vo.lastValueVisible = false;
    vo.priceLineVisible = false;
    Series *v = c->addSeries(SeriesType::Histogram, vo);
    auto ps = c->priceScale("vol")->options();
    ps.scaleMarginTop = 0.82;
    ps.scaleMarginBottom = 0;
    c->priceScale("vol")->applyOptions(ps);
    QVector<DataItem> data;
    for (const Bar &b : bars) {
        DataItem d;
        d.time = b.time;
        d.value = b.volume;
        d.color = b.close >= b.open ? cssColor("rgba(8,153,129,0.45)") : cssColor("rgba(242,54,69,0.45)");
        data.push_back(d);
    }
    v->setData(data);
    return v;
}

Series *addCandles(Chart *c, const QVector<Bar> &bars)
{
    SeriesOptions so;
    so.upColor = kUp;
    so.downColor = kDown;
    so.borderVisible = false;
    so.wickUpColor = kUp;
    so.wickDownColor = kDown;
    Series *s = c->addSeries(SeriesType::Candlestick, so);
    setPrecision(s, bars.last().close);
    s->setData(ohlc(bars));
    return s;
}

void initialRange(Chart *c, int n)
{
    c->timeScale()->setVisibleLogicalRange({double(std::max(0, n - 160)), double(n + 8)});
}

// A trend line drawn like drawings.js (top z-order, media coordinates)
class TrendLine : public Primitive
{
public:
    TrendLine(Chart *c, double t1, double p1, double t2, double p2)
        : m_chart(c), m_t1(t1), m_p1(p1), m_t2(t2), m_p2(p2) {}
    ZOrder zOrder() const override { return ZOrder::Top; }
    void draw(QPainter &p, const QSizeF &) override
    {
        if (!m_series)
            return;
        auto x1 = m_chart->timeScale()->logicalToCoordinate(m_t1);
        auto x2 = m_chart->timeScale()->logicalToCoordinate(m_t2);
        auto y1 = m_series->priceToCoordinate(m_p1), y2 = m_series->priceToCoordinate(m_p2);
        if (!x1 || !x2 || !y1 || !y2)
            return;
        p.setPen(QPen(QColor("#2962ff"), 1.5));
        p.drawLine(QPointF(*x1, *y1), QPointF(*x2, *y2));
        p.setBrush(QColor("#2962ff"));
        p.drawEllipse(QPointF(*x1, *y1), 3, 3);
        p.drawEllipse(QPointF(*x2, *y2), 3, 3);
    }

private:
    Chart *m_chart;
    double m_t1, m_p1, m_t2, m_p2;   // logical index and price
};

// A bottom z-order block like volume-profile.js
class Block : public Primitive
{
public:
    explicit Block(double lo, double hi) : m_lo(lo), m_hi(hi) {}
    ZOrder zOrder() const override { return ZOrder::Bottom; }
    void draw(QPainter &p, const QSizeF &size) override
    {
        if (!m_series)
            return;
        auto a = m_series->priceToCoordinate(m_hi), b = m_series->priceToCoordinate(m_lo);
        if (!a || !b)
            return;
        const double w = size.width() * 0.3;
        p.fillRect(QRectF(size.width() - w, *a, w, *b - *a), cssColor("rgba(8,153,129,0.45)"));
        p.setPen(QPen(QColor("#ff9800"), 1, Qt::CustomDashLine));
        QPen pen(QColor("#ff9800"), 1);
        pen.setDashPattern({4, 3});
        p.setPen(pen);
        const double y = (*a + *b) / 2;
        p.drawLine(QPointF(0, y), QPointF(size.width(), y));
    }

private:
    double m_lo, m_hi;
};

QVector<SeriesMarker> makeMarkers(const SceneData &d)
{
    QVector<SeriesMarker> m;
    const int n = d.daily.size();
    for (int i = n - 150; i < n; ++i) {
        if (!std::isfinite(d.ema[i]) || !std::isfinite(d.ema[i - 1]))
            continue;
        const bool upCross = d.daily[i - 1].close < d.ema[i - 1] && d.daily[i].close >= d.ema[i];
        const bool dnCross = d.daily[i - 1].close > d.ema[i - 1] && d.daily[i].close <= d.ema[i];
        if (upCross) {
            SeriesMarker x;
            x.time = d.daily[i].time;
            x.position = SeriesMarker::BelowBar;
            x.shape = SeriesMarker::ArrowUp;
            x.color = kUp;
            x.text = "Buy";
            m.push_back(x);
        } else if (dnCross) {
            SeriesMarker x;
            x.time = d.daily[i].time;
            x.position = SeriesMarker::AboveBar;
            x.shape = SeriesMarker::ArrowDown;
            x.color = kDown;
            x.text = "Sell";
            m.push_back(x);
        }
    }
    SeriesMarker c;
    c.time = d.daily[n - 40].time;
    c.position = SeriesMarker::InBar;
    c.shape = SeriesMarker::Circle;
    c.color = QColor("#ff9800");
    m.push_back(c);
    SeriesMarker s;
    s.time = d.daily[n - 20].time;
    s.position = SeriesMarker::AboveBar;
    s.shape = SeriesMarker::Square;
    s.color = QColor("#e91e63");
    s.text = "Note";
    m.push_back(s);
    std::sort(m.begin(), m.end(), [](const SeriesMarker &a, const SeriesMarker &b) { return a.time < b.time; });
    return m;
}

Chart *mainChart(const SceneData &d, bool withRsi)
{
    Chart *c = new Chart(mainOptions());
    Series *main = addCandles(c, d.daily);
    addVolume(c, d.daily);
    // EMA 20 overlay (renderIndicators, pane 0)
    SeriesOptions eo;
    eo.color = QColor("#f7a21b");
    eo.priceLineVisible = false;
    eo.lastValueVisible = false;
    eo.crosshairMarkerVisible = false;
    eo.lineWidth = 1;
    Series *e = c->addSeries(SeriesType::Line, eo, 0);
    e->setData(lineData(d.daily, d.ema));
    if (withRsi) {
        SeriesOptions ro;
        ro.color = QColor("#7e57c2");
        ro.priceLineVisible = false;
        ro.lastValueVisible = true;
        ro.crosshairMarkerVisible = false;
        ro.lineWidth = 1;
        Series *r = c->addSeries(SeriesType::Line, ro, 1);
        r->setData(lineData(d.daily, d.rsi));
        for (double lv : {70.0, 50.0, 30.0}) {
            PriceLineOptions po;
            po.price = lv;
            po.color = kMuted;
            po.lineWidth = 1;
            po.lineStyle = LineStyle::Dashed;
            po.axisLabelVisible = false;
            r->createPriceLine(po);
        }
        const auto ps = c->panes();
        for (int i = 0; i < ps.size(); ++i)
            ps[i]->setStretchFactor(i == 0 ? 1 : 0.28);
    }
    main->setMarkers(makeMarkers(d));
    // render(): timeScale options, price scale mode, initial range
    auto o = c->options();
    o.timeVisible = false;
    o.secondsVisible = false;
    c->applyOptions(o);
    initialRange(c, d.daily.size());
    return c;
}

Chart *typeChart(const SceneData &d, const QString &type)
{
    Chart *c = new Chart(mainOptions());
    Series *s = nullptr;
    SeriesOptions so;
    if (type == "bars") {
        so.upColor = kUp;
        so.downColor = kDown;
        s = c->addSeries(SeriesType::Bar, so);
        setPrecision(s, d.daily.last().close);
        s->setData(ohlc(d.daily));
    } else if (type == "hollow") {
        so.upColor = QColor(0, 0, 0, 0);
        so.downColor = kDown;
        so.borderUpColor = kUp;
        so.borderDownColor = kDown;
        so.wickUpColor = kUp;
        so.wickDownColor = kDown;
        so.borderVisible = true;
        s = c->addSeries(SeriesType::Candlestick, so);
        setPrecision(s, d.daily.last().close);
        s->setData(ohlc(d.daily));
    } else if (type == "line" || type == "step" || type == "linemarkers") {
        so.color = kAccent;
        so.lineWidth = 2;
        so.lineType = type == "step" ? LineType::WithSteps : LineType::Simple;
        so.pointMarkersVisible = type == "linemarkers";
        s = c->addSeries(SeriesType::Line, so);
        setPrecision(s, d.daily.last().close);
        s->setData(closes(d.daily));
    } else if (type == "area") {
        so.lineColor = kAccent;
        so.topColor = cssColor("rgba(41,98,255,0.35)");
        so.bottomColor = cssColor("rgba(41,98,255,0.02)");
        so.lineWidth = 2;
        s = c->addSeries(SeriesType::Area, so);
        setPrecision(s, d.daily.last().close);
        s->setData(closes(d.daily));
    } else if (type == "baseline") {
        so.topLineColor = kUp;
        so.bottomLineColor = kDown;
        so.topFillColor1 = cssColor("rgba(8,153,129,0.25)");
        so.topFillColor2 = cssColor("rgba(8,153,129,0.02)");
        so.bottomFillColor1 = cssColor("rgba(242,54,69,0.02)");
        so.bottomFillColor2 = cssColor("rgba(242,54,69,0.25)");
        s = c->addSeries(SeriesType::Baseline, so);
        setPrecision(s, d.daily.last().close);
        auto o = s->options();
        o.baseValue = d.daily[std::max(0, int(d.daily.size()) - 150)].close;
        s->applyOptions(o);
        s->setData(closes(d.daily));
    } else if (type == "columns") {
        s = c->addSeries(SeriesType::Histogram, so);
        setPrecision(s, d.daily.last().close);
        QVector<DataItem> data;
        for (const Bar &b : d.daily) {
            DataItem x;
            x.time = b.time;
            x.value = b.close;
            x.color = b.close >= b.open ? kUp : kDown;
            data.push_back(x);
        }
        s->setData(data);
    }
    if (type == "bars" || type == "hollow")
        addVolume(c, d.daily);
    auto o = c->options();
    o.timeVisible = false;
    c->applyOptions(o);
    initialRange(c, d.daily.size());
    return c;
}

} // namespace

QString fmtPrice(double v)
{
    if (!std::isfinite(v))
        return QStringLiteral("—");
    const double a = std::abs(v);
    if (a >= 1e6) {
        if (a >= 1e12) return QString::number(v / 1e12, 'f', 2) + 'T';
        if (a >= 1e9) return QString::number(v / 1e9, 'f', 2) + 'B';
        return QString::number(v / 1e6, 'f', 2) + 'M';
    }
    const int d = a >= 1000 ? 2 : a >= 1 ? (a >= 100 ? 2 : 4) : a >= 0.01 ? 6 : 8;
    const int mn = std::min(d, 2);
    QLocale loc(QLocale::English, QLocale::UnitedStates);
    QString s = loc.toString(v, 'f', d);
    // drop trailing zeros down to the minimum fraction digits
    const int dot = s.indexOf('.');
    if (dot >= 0) {
        int keep = s.size();
        while (keep > dot + 1 + mn && s[keep - 1] == '0')
            --keep;
        s.truncate(keep);
        if (s.endsWith('.'))
            s.chop(1);
    }
    return s;
}

SceneData makeData()
{
    SceneData d;
    const double day = 86400;
    const double t0 = 1672617600;   // 2023-01-02
    d.daily = walk(7, 30000, 600, t0, day, 0.022, 0.0006);
    d.intraday = walk(11, 3500, 420, 1709575200 /* 2024-03-04 18:00 */, 300, 0.0025);
    d.growth = walk(13, 0.5, 600, t0, day, 0.03, 0.017, 6);
    d.cmpA = walk(17, 2000, 600, t0, day, 0.028, 0.0004);
    d.cmpB = walk(19, 50, 600, t0, day, 0.04, 0.001, 4);
    d.ema = ema(d.daily, 20);
    d.rsi = rsi(d.daily, 14);
    Rng rng{23};
    for (int y : {2021, 2022, 2023, 2024, 2025, 2026}) {
        SceneData::Year yr;
        yr.year = y;
        double pct = 0;
        const int days = y == 2026 ? 300 : (y % 4 == 0 ? 366 : 365);
        for (int doy = 1; doy <= days; ++doy) {
            if (doy % 7 == 6 || doy % 7 == 0)
                continue;   // weekends: no bars
            pct += rng.normal() * 1.2 + 0.05;
            yr.points.push_back({doy, std::round(pct * 100) / 100});
        }
        d.years.push_back(yr);
    }
    for (int doy = 1; doy <= 366; ++doy) {
        double sum = 0;
        int n = 0;
        for (const auto &yr : d.years) {
            if (yr.year == 2026)
                continue;
            for (const auto &p : yr.points)
                if (p.first == doy) {
                    sum += p.second;
                    ++n;
                }
        }
        if (n)
            d.average.push_back({doy, std::round(sum / n * 100) / 100});
    }
    // price axis cases: price levels (small, huge, around powers of ten), modes, pane heights
    struct Level { const char *id; double start, vol, drift; int dec; };
    const Level levels[] = {
        {"p0012", 0.0012, 0.03, 0.0005, 8},  {"p0123", 0.0123, 0.02, 0, 8},
        {"p098", 0.98, 0.012, 0.0002, 6},    {"p32", 3.2, 0.03, 0, 4},
        {"p98", 9.8, 0.01, 0.0002, 4},       {"p47", 47.3, 0.004, 0, 4},
        {"p99", 98.5, 0.008, 0.0002, 2},     {"p1234", 1234, 0.05, 0.001, 2},
        {"p9900", 9900, 0.006, 0.0002, 2},   {"p99969", 99969, 0.004, 0.0001, 2},
        {"p87k", 87000, 0.03, 0.001, 2},     {"p1m", 1.2e6, 0.02, 0.0005, 2},
        {"p004", 0.0040, 0.08, 0.002, 8},    {"pflat", 1.0001, 0.00002, 0, 6},
    };
    struct Size { const char *id; int w, h; };
    const Size sizes[] = {{"a", 900, 460}, {"b", 450, 300}, {"c", 400, 200}, {"d", 300, 140}, {"e", 380, 110}};
    std::uint32_t seed = 101;
    for (const Level &l : levels) {
        const QVector<Bar> bars = walk(seed++, l.start, 400, t0, day, l.vol, l.drift, l.dec);
        int k = 0;
        for (const Size &z : sizes) {
            SceneData::Tick t;
            t.name = QStringLiteral("tk-%1-%2").arg(l.id, z.id);
            t.width = z.w;
            t.height = z.h;
            t.type = QStringList{"candles", "line", "area", "bars", "candles"}[k];
            t.visible = k == 3 ? 0 : k == 4 ? 60 : 160;
            t.bars = bars;
            d.ticks.push_back(t);
            ++k;
        }
        // log and percentage scales, indicator panes
        SceneData::Tick t;
        t.width = 900;
        t.height = 460;
        t.type = "candles";
        t.bars = bars;
        t.name = QStringLiteral("tk-%1-log").arg(l.id);
        t.mode = 1;
        t.visible = 0;
        d.ticks.push_back(t);
        t.name = QStringLiteral("tk-%1-pct").arg(l.id);
        t.mode = 2;
        t.visible = 160;
        d.ticks.push_back(t);
        t.name = QStringLiteral("tk-%1-pcts").arg(l.id);
        t.height = 220;
        t.width = 450;
        d.ticks.push_back(t);
        t.mode = 0;
        t.width = 900;
        t.height = 560;
        const char *panes[] = {"rsi", "macd", "obv"};
        const char *pane = panes[(seed - 102) % 3];
        t.name = QStringLiteral("tk-%1-%2").arg(l.id, pane);
        t.pane = pane;
        if (t.pane == "rsi") {
            t.ind = rsi(bars, 14);
        } else if (t.pane == "macd") {
            const QVector<double> f = ema(bars, 12), s2 = ema(bars, 26);
            t.ind.resize(bars.size());
            for (int i = 0; i < bars.size(); ++i)
                t.ind[i] = f[i] - s2[i];
        } else {
            double obv = 0;
            t.ind.resize(bars.size());
            for (int i = 0; i < bars.size(); ++i) {
                if (i > 0)
                    obv += bars[i].close > bars[i - 1].close ? bars[i].volume * 1000
                           : bars[i].close < bars[i - 1].close ? -bars[i].volume * 1000 : 0;
                t.ind[i] = obv;
            }
        }
        d.ticks.push_back(t);
        {
            // small log chart, a 2x4 layout cell with fewer bars, a 4x4 cell with an RSI pane
            SceneData::Tick u;
            u.bars = bars;
            u.type = "candles";
            u.name = QStringLiteral("tk-%1-logs").arg(l.id);
            u.width = 400;
            u.height = 200;
            u.mode = 1;
            u.visible = 120;
            d.ticks.push_back(u);
            u.name = QStringLiteral("tk-%1-l8").arg(l.id);
            u.width = 480;
            u.height = 300;
            u.mode = 0;
            u.type = "area";
            u.visible = 120;
            d.ticks.push_back(u);
            u.name = QStringLiteral("tk-%1-l16").arg(l.id);
            u.width = 390;
            u.height = 230;
            u.type = "candles";
            u.visible = 60;
            u.pane = "rsi";
            u.ind = rsi(bars, 14);
            d.ticks.push_back(u);
        }
        t.name += "-s";   // 4x4 layout cell
        t.width = 400;
        t.height = 240;
        d.ticks.push_back(t);
    }
    for (int step : {300, 900, 3600, 14400, 86400, 604800}) {
        Rng r{std::uint32_t(step)};
        const double start = step >= 86400 ? 1534118400 /* 2018-08-13 */ : 1782432000 /* 2026-06-26 */;
        double e = 10000, bh = 10000;
        auto &v = d.equities[step];
        const int n = step == 604800 ? 420 : 3000;
        for (int i = 0; i < n; ++i) {
            e *= 1 + r.normal() * 0.004 + 0.0002;
            bh *= 1 + r.normal() * 0.006 + 0.0001;
            v.push_back({start + double(i) * step, std::round(e * 100) / 100, std::round(bh * 100) / 100});
        }
    }
    double eq = 10000;
    for (int i = 0; i < d.daily.size(); ++i) {
        eq *= 1 + rng.normal() * 0.01 + 0.0008;
        d.equity.push_back({d.daily[i].time, std::round(eq * 100) / 100});
    }
    return d;
}

QString exportJs(const SceneData &d)
{
    QString s = QStringLiteral("window.DATA = {\n");
    s += "daily: " + barsJs(d.daily) + ",\n";
    s += "intraday: " + barsJs(d.intraday) + ",\n";
    s += "growth: " + barsJs(d.growth) + ",\n";
    s += "cmpA: " + barsJs(d.cmpA) + ",\n";
    s += "cmpB: " + barsJs(d.cmpB) + ",\n";
    s += "ema: " + valuesJs(d.ema) + ",\n";
    s += "rsi: " + valuesJs(d.rsi) + ",\n";
    QStringList years;
    for (const auto &y : d.years) {
        QStringList pts;
        for (const auto &p : y.points)
            pts << QStringLiteral("[%1,%2]").arg(p.first).arg(num(p.second));
        years << QStringLiteral("{year:%1,points:[%2]}").arg(y.year).arg(pts.join(','));
    }
    s += "years: [" + years.join(",\n") + "],\n";
    QStringList avg;
    for (const auto &p : d.average)
        avg << QStringLiteral("[%1,%2]").arg(p.first).arg(num(p.second));
    s += "average: [" + avg.join(',') + "],\n";
    QStringList eq;
    for (const auto &p : d.equity)
        eq << QStringLiteral("[%1,%2]").arg(num(p.first), num(p.second));
    s += "equity: [" + eq.join(',') + "],\n";
    QStringList tk;
    for (const auto &t : d.ticks)
        tk << QStringLiteral("\"%1\": {type:\"%2\",mode:%3,pane:\"%4\",visible:%5,bars:%6,ind:%7}")
                  .arg(t.name, t.type).arg(t.mode).arg(t.pane).arg(t.visible).arg(barsJs(t.bars), valuesJs(t.ind));
    s += "ticks: {" + tk.join(",\n") + "},\n";
    QStringList eqs;
    for (auto it = d.equities.cbegin(); it != d.equities.cend(); ++it) {
        QStringList pts;
        for (const auto &p : it.value())
            pts << QStringLiteral("[%1,%2,%3]").arg(num(p[0]), num(p[1]), num(p[2]));
        eqs << QStringLiteral("\"%1\": [%2]").arg(it.key()).arg(pts.join(','));
    }
    s += "equities: {" + eqs.join(",\n") + "}\n};\n";
    return s;
}

// A price axis case (SceneData::ticks): the main chart of chart.js with one series type, an optional
// indicator pane, a price scale mode and a view like the app's initial range (or fitContent).
static Chart *tickChart(const SceneData &d, const QString &name)
{
    const SceneData::Tick *t = nullptr;
    for (const auto &x : d.ticks)
        if (x.name == name)
            t = &x;
    Chart *c = new Chart(mainOptions());
    Series *s = nullptr;
    SeriesOptions so;
    if (t->type == "candles") {
        s = addCandles(c, t->bars);
    } else if (t->type == "bars") {
        so.upColor = kUp;
        so.downColor = kDown;
        s = c->addSeries(SeriesType::Bar, so);
        setPrecision(s, t->bars.last().close);
        s->setData(ohlc(t->bars));
    } else if (t->type == "line") {
        so.color = kAccent;
        so.lineWidth = 2;
        s = c->addSeries(SeriesType::Line, so);
        setPrecision(s, t->bars.last().close);
        s->setData(closes(t->bars));
    } else {
        so.lineColor = kAccent;
        so.topColor = cssColor("rgba(41,98,255,0.35)");
        so.bottomColor = cssColor("rgba(41,98,255,0.02)");
        so.lineWidth = 2;
        s = c->addSeries(SeriesType::Area, so);
        setPrecision(s, t->bars.last().close);
        s->setData(closes(t->bars));
    }
    if (t->type == "candles" || t->type == "bars")
        addVolume(c, t->bars);
    if (!t->pane.isEmpty()) {
        SeriesOptions ro;
        ro.color = QColor("#7e57c2");
        ro.priceLineVisible = false;
        ro.lastValueVisible = true;
        ro.crosshairMarkerVisible = false;
        ro.lineWidth = 1;
        if (t->pane == "obv")
            ro.priceFormat.type = PriceFormatType::Volume;
        Series *r = c->addSeries(SeriesType::Line, ro, 1);
        r->setData(lineData(t->bars, t->ind));
        if (t->pane == "rsi")
            for (double lv : {70.0, 30.0}) {
                PriceLineOptions po;
                po.price = lv;
                po.color = kMuted;
                po.lineWidth = 1;
                po.lineStyle = LineStyle::Dashed;
                po.axisLabelVisible = false;
                r->createPriceLine(po);
            }
        const auto ps = c->panes();
        for (int i = 0; i < ps.size(); ++i)
            ps[i]->setStretchFactor(i == 0 ? 1 : 0.28);
    }
    if (t->mode) {
        auto ps = c->priceScale("right")->options();
        ps.mode = PriceScaleMode(t->mode);
        c->priceScale("right")->applyOptions(ps);
    }
    auto o = c->options();
    o.timeVisible = false;
    c->applyOptions(o);
    const int n = t->bars.size();
    if (t->visible)
        c->timeScale()->setVisibleLogicalRange({double(n - t->visible), double(n + 8)});
    else
        c->timeScale()->fitContent();
    Q_UNUSED(s);
    return c;
}

QVector<Scene> scenes()
{
    QVector<Scene> v;
    v.push_back({"main", 900, 560, [](const SceneData &d) { return mainChart(d, true); }});
    v.push_back({"crosshair", 900, 560, [](const SceneData &d) { return mainChart(d, true); }, 455, 180});
    v.push_back({"crosshair-rsi", 900, 560, [](const SceneData &d) { return mainChart(d, true); }, 700, 470});
    for (const char *t : {"hollow", "bars", "line", "step", "linemarkers", "area", "baseline", "columns"}) {
        const QString type = QString::fromLatin1(t);
        v.push_back({type, 900, 460, [type](const SceneData &d) { return typeChart(d, type); }});
    }
    v.push_back({"log", 900, 460, [](const SceneData &d) {
                     Chart *c = new Chart(mainOptions());
                     Series *s = addCandles(c, d.growth);
                     Q_UNUSED(s);
                     auto ps = c->priceScale("right")->options();
                     ps.mode = PriceScaleMode::Logarithmic;
                     c->priceScale("right")->applyOptions(ps);
                     auto o = c->options();
                     o.timeVisible = false;
                     c->applyOptions(o);
                     c->timeScale()->fitContent();
                     return c;
                 }});
    v.push_back({"compare", 900, 460, [](const SceneData &d) {
                     Chart *c = new Chart(mainOptions());
                     addCandles(c, d.daily);
                     addVolume(c, d.daily);
                     auto ps = c->priceScale("right")->options();
                     ps.mode = PriceScaleMode::Percentage;
                     c->priceScale("right")->applyOptions(ps);
                     const QColor palette[] = {QColor("#ff9800"), QColor("#e91e63")};
                     const QVector<Bar> *sets[] = {&d.cmpA, &d.cmpB};
                     const char *names[] = {"ETHUSDT", "SOLUSDT"};
                     for (int k = 0; k < 2; ++k) {
                         SeriesOptions so;
                         so.color = palette[k];
                         so.lineWidth = 2;
                         so.priceLineVisible = false;
                         so.title = names[k];
                         Series *s = c->addSeries(SeriesType::Line, so);
                         s->setData(closes(*sets[k]));
                     }
                     auto o = c->options();
                     o.timeVisible = false;
                     c->applyOptions(o);
                     initialRange(c, d.daily.size());
                     return c;
                 }});
    v.push_back({"primitive", 900, 460, [](const SceneData &d) {
                     Chart *c = new Chart(mainOptions());
                     Series *s = addCandles(c, d.daily);
                     addVolume(c, d.daily);
                     const int n = d.daily.size();
                     static TrendLine *tl = nullptr;
                     static Block *bl = nullptr;
                     delete tl;
                     delete bl;
                     tl = new TrendLine(c, n - 120, d.daily[n - 120].low, n - 20, d.daily[n - 20].high);
                     bl = new Block(d.daily[n - 60].close * 0.97, d.daily[n - 60].close * 1.03);
                     s->attachPrimitive(tl);
                     s->attachPrimitive(bl);
                     auto o = c->options();
                     o.timeVisible = false;
                     c->applyOptions(o);
                     initialRange(c, n);
                     return c;
                 }});
    v.push_back({"intraday", 900, 460, [](const SceneData &d) {
                     Chart *c = new Chart(mainOptions());
                     addCandles(c, d.intraday);
                     addVolume(c, d.intraday);
                     auto o = c->options();
                     o.timeVisible = true;
                     o.secondsVisible = false;
                     c->applyOptions(o);
                     initialRange(c, d.intraday.size());
                     return c;
                 }});
    v.push_back({"equity", 600, 220, [](const SceneData &d) {
                     // panels.js equity chart
                     ChartOptions o;
                     o.background = kBg;
                     o.textColor = kText;
                     o.fontSize = 10;
                     o.vertLinesVisible = false;
                     o.horzLinesColor = kGrid;
                     o.rightPriceScaleBorderVisible = false;
                     o.timeScaleBorderVisible = false;
                     Chart *c = new Chart(o);
                     SeriesOptions ao;
                     ao.lineColor = QColor("#2962ff");
                     ao.topColor = cssColor("rgba(41,98,255,.3)");
                     ao.bottomColor = cssColor("rgba(41,98,255,0)");
                     ao.lineWidth = 2;
                     ao.title = "Strategy";
                     Series *eq = c->addSeries(SeriesType::Area, ao);
                     QVector<DataItem> data;
                     for (const auto &p : d.equity) {
                         DataItem x;
                         x.time = p.first;
                         x.value = p.second;
                         data.push_back(x);
                     }
                     eq->setData(data);
                     SeriesOptions lo;
                     lo.color = QColor("#787b86");
                     lo.lineWidth = 1;
                     lo.title = "Buy & hold";
                     lo.priceLineVisible = false;
                     Series *bh = c->addSeries(SeriesType::Line, lo);
                     QVector<DataItem> bhd;
                     const double c0 = d.daily[0].close;
                     for (const Bar &b : d.daily) {
                         DataItem x;
                         x.time = b.time;
                         x.value = 10000 * b.close / c0;
                         bhd.push_back(x);
                     }
                     bh->setData(bhd);
                     c->timeScale()->fitContent();
                     return c;
                 }});
    v.push_back({"seasonals", 600, 300, [](const SceneData &d) {
                     // research.js seasonals chart
                     auto md = [](double t) {
                         static const char *m[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                                   "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
                         const QDateTime dt = QDateTime::fromSecsSinceEpoch(qint64(t), QTimeZone::UTC);
                         return QString::fromLatin1(m[dt.date().month() - 1]) + ' '
                                + QString::number(dt.date().day());
                     };
                     ChartOptions o = themeOptions(10);
                     o.tickMarkFormatter = md;
                     o.timeFormatter = md;
                     o.priceFormatter = [](double p) {
                         return QString(p >= 0 ? "+" : "") + QString::number(p, 'f', 1) + '%';
                     };
                     Chart *c = new Chart(o);
                     const double T0 = 946684800;   // 2000-01-01
                     const char *palette[] = {"#2962ff", "#ff9800", "#e91e63", "#00bcd4", "#8bc34a",
                                              "#9c27b0", "#795548", "#607d8b", "#cddc39"};
                     const auto recent = d.years.mid(std::max(0, int(d.years.size()) - 5));
                     for (int k = 0; k < recent.size(); ++k) {
                         const bool current = recent[k].year == 2026;
                         SeriesOptions so;
                         so.color = current ? kText : QColor(palette[k % 9]);
                         so.lineWidth = current ? 3 : 1;
                         so.priceLineVisible = false;
                         so.lastValueVisible = false;
                         so.crosshairMarkerVisible = false;
                         Series *s = c->addSeries(SeriesType::Line, so);
                         QVector<DataItem> data;
                         for (const auto &p : recent[k].points) {
                             DataItem x;
                             x.time = T0 + (p.first - 1) * 86400;
                             x.value = p.second;
                             data.push_back(x);
                         }
                         s->setData(data);
                     }
                     SeriesOptions ao;
                     ao.color = QColor("#9598a1");
                     ao.lineWidth = 2;
                     ao.lineStyle = LineStyle::Dashed;
                     ao.priceLineVisible = false;
                     ao.lastValueVisible = false;
                     Series *avg = c->addSeries(SeriesType::Line, ao);
                     QVector<DataItem> data;
                     for (const auto &p : d.average) {
                         DataItem x;
                         x.time = T0 + (p.first - 1) * 86400;
                         x.value = p.second;
                         data.push_back(x);
                     }
                     avg->setData(data);
                     c->timeScale()->fitContent();
                     return c;
                 }});
    v.push_back({"defaults", 700, 400, [](const SceneData &d) {
                     // Lightweight Charts defaults (light theme, magnet crosshair)
                     Chart *c = new Chart(ChartOptions());
                     Series *s = c->addSeries(SeriesType::Line, SeriesOptions());
                     s->setData(closes(d.cmpB));
                     return c;
                 }, 350, 200});
    // last price line and last value colours (candles: the last bar's up / down colour, lines: the
    // last point's colour), with whitespace at the end, per-point colours, a cut and updated series
    auto pl = [](const QString &kind) {
        return [kind](const SceneData &d) {
            Chart *c = new Chart(mainOptions());
            QVector<Bar> b = d.daily.mid(0, 300);
            Series *s = nullptr;
            if (kind == "wsend" || kind == "doji" || kind == "cut") {
                if (kind == "doji")
                    b.last().open = b.last().close;
                s = addCandles(c, b);
                if (kind == "wsend") {
                    QVector<DataItem> data = ohlc(b);
                    for (int i = 1; i <= 5; ++i) {
                        DataItem w;
                        w.time = b.last().time + i * 86400;
                        data.push_back(w);
                    }
                    s->setData(data);
                } else if (kind == "cut") {
                    // replay: the data cut to fewer bars, then the next bar comes in as an update
                    s->setData(ohlc(d.daily.mid(0, 250)));
                    DataItem u = ohlc(d.daily.mid(250, 1)).first();
                    u.close = u.open * 0.97;
                    u.low = std::min(u.low, u.close);
                    s->update(u);
                }
            } else if (kind == "hollow") {
                SeriesOptions so;
                so.upColor = QColor(0, 0, 0, 0);
                so.downColor = kDown;
                so.borderUpColor = kUp;
                so.borderDownColor = kDown;
                so.wickUpColor = kUp;
                so.wickDownColor = kDown;
                so.borderVisible = true;
                s = c->addSeries(SeriesType::Candlestick, so);
                setPrecision(s, b.last().close);
                QVector<DataItem> data = ohlc(b);
                data.last().close = data.last().open * 1.02;
                data.last().high = std::max(data.last().high, data.last().close);
                s->setData(data);
            } else {
                SeriesOptions so;
                so.color = kAccent;
                so.lineWidth = 2;
                s = c->addSeries(SeriesType::Line, so);
                setPrecision(s, b.last().close);
                QVector<DataItem> data = closes(b);
                if (kind == "linecolor") {
                    for (int i = 0; i < data.size(); ++i)
                        data[i].color = i % 2 ? QColor("#ff9800") : QColor("#e91e63");
                } else if (kind == "linegap") {
                    QVector<double> v;
                    for (const Bar &x : b)
                        v.push_back(x.close);
                    for (int i = v.size() - 6; i < v.size(); ++i)
                        v[i] = NaN;
                    data = lineData(b, v);
                }
                s->setData(data);
            }
            auto o = c->options();
            o.timeVisible = false;
            c->applyOptions(o);
            c->timeScale()->setVisibleLogicalRange({140, 310});
            return c;
        };
    };
    // the tester's equity chart (panels.js) on other bar sizes and widths
    for (int step : {300, 900, 3600, 14400, 86400, 604800})
        for (int w : {820, 560, 380})
            for (int h : {150, 60})
                v.push_back({QStringLiteral("eq-%1-%2-%3").arg(step).arg(w).arg(h), w, h, [step](const SceneData &d) {
                                 ChartOptions o;
                                 o.background = kBg;
                                 o.textColor = kText;
                                 o.fontSize = 10;
                                 o.vertLinesVisible = false;
                                 o.horzLinesColor = kGrid;
                                 o.rightPriceScaleBorderVisible = false;
                                 o.timeScaleBorderVisible = false;
                                 Chart *c = new Chart(o);
                                 SeriesOptions ao;
                                 ao.lineColor = QColor("#2962ff");
                                 ao.topColor = cssColor("rgba(41,98,255,.3)");
                                 ao.bottomColor = cssColor("rgba(41,98,255,0)");
                                 ao.lineWidth = 2;
                                 ao.title = "Strategy";
                                 Series *eq = c->addSeries(SeriesType::Area, ao);
                                 SeriesOptions lo;
                                 lo.color = QColor("#787b86");
                                 lo.lineWidth = 1;
                                 lo.title = "Buy & hold";
                                 lo.priceLineVisible = false;
                                 QVector<DataItem> a, b;
                                 for (const auto &p : d.equities[step]) {
                                     DataItem x;
                                     x.time = p[0];
                                     x.value = p[1];
                                     a.push_back(x);
                                     x.value = p[2];
                                     b.push_back(x);
                                 }
                                 eq->setData(a);
                                 Series *bh = c->addSeries(SeriesType::Line, lo);
                                 bh->setData(b);
                                 c->timeScale()->fitContent();
                                 return c;
                             }});
    for (const char *k : {"wsend", "doji", "cut", "hollow", "linecolor", "linegap"})
        v.push_back({QStringLiteral("pl-") + k, 700, 360, pl(k)});
    static const SceneData names = makeData();
    for (const auto &t : names.ticks)
        v.push_back({t.name, t.width, t.height,
                     [name = t.name](const SceneData &d) { return tickChart(d, name); }});
    return v;
}
