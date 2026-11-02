// The liquidity heatmap on the chart (see HeatmapLayer.h). A Deepwater addition.
#include "HeatmapLayer.h"

#include "ChartPane.h"
#include "DepthFeed.h"
#include "TradeFeed.h"
#include "Theme.h"

#include "../core/Footprint.h"
#include "../core/Series.h"

#include <QFileInfo>
#include <QFontMetricsF>
#include <QLocale>
#include <QPainter>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/resource.h>
#endif

#include <algorithm>
#include <cmath>

namespace qtm {

using namespace heatmap;

namespace {
int pow2ceil(double x)
{
    int c = 1;
    while (c < x && c < (1 << 24)) c <<= 1;
    return c;
}
int64_t ffloor(double x) { return int64_t(std::floor(x)); }
int64_t fceil(double x) { return int64_t(std::ceil(x)); }
bool sameLook(const Look &a, const Look &b)
{
    return a.percentile == b.percentile && a.gain == b.gain && a.gamma == b.gamma && a.threshold == b.threshold;
}
} // namespace

// ---------------------------------------------------------------- settings

HeatmapSettings HeatmapSettings::fromJson(const QJsonObject &o, const QHash<QString, QString> &colors)
{
    HeatmapSettings s;
    const QString iv = o.value("interval").toString();
    s.intervalMs = iv == "100 ms" ? 100 : iv == "500 ms" ? 500 : iv == "1 s" ? 1000 : 250;
    s.historyMinutes = std::clamp(int(o.value("history").toDouble(120)), 1, 24 * 60);
    const QJsonValue g = o.value("group");
    s.group = std::clamp(g.isString() ? g.toString().toInt() : g.toInt(1), 1, 1000);
    if (s.group < 1) s.group = 1;
    s.agg = o.value("agg").toString() == "Mean" ? Agg::Mean : Agg::Max;
    s.dom = o.value("dom").toString() != "Off";
    const QString pal = o.value("palette").toString();
    s.palette = pal.startsWith("Thermal") ? PaletteKind::Thermal
                : pal.startsWith("Mono")  ? PaletteKind::Mono
                : pal.startsWith("Custom") ? PaletteKind::Custom
                                           : PaletteKind::Deep;
    auto num = [&](const char *k, double def, double lo, double hi) {
        const QJsonValue v = o.value(k);
        const double x = v.isString() ? v.toString().toDouble() : v.toDouble(def);
        return std::isfinite(x) ? std::clamp(x, lo, hi) : def;
    };
    s.look.gain = num("gain", 1, 0.05, 50);
    s.look.gamma = num("gamma", 1, 0.1, 10);
    s.look.threshold = num("threshold", 0.02, 0, 1);
    s.look.percentile = num("percentile", 95, 50, 100);
    s.eventParams.bigFactor = num("pulledSize", 5, 1, 100);
    s.eventParams.absorbRatio = num("absorbRatio", 1, 0.1, 100);
    s.keepDays = int(num("recordDays", 7, 1, 3650));
    auto colour = [&](const char *key, Rgb def) {
        const QColor c(colors.value(key));
        return c.isValid() ? Rgb{uint8_t(c.red()), uint8_t(c.green()), uint8_t(c.blue())} : def;
    };
    s.low = colour("low", s.low);
    s.mid = colour("mid", s.mid);
    s.high = colour("high", s.high);
    s.debug = o.value("debug").toString() == "On";
    const QString st = o.value("stable").toString();
    s.stableMs = st == "500 ms" ? 500 : st == "1 s" ? 1000 : st == "2 s" ? 2000 : st == "5 s" ? 5000 : st == "10 s" ? 10000 : 0;
    s.events = o.value("events").toString() != "Off";
    s.record = o.value("record").toString() == "On";
    return s;
}

// ---------------------------------------------------------------- texture

bool HeatmapTexture::update(const DepthFeed &feed, const View &v, const HeatmapSettings &s)
{
    QElapsedTimer t;
    t.start();
    const ColumnStore *st = feed.store();
    const double iv = feed.params().intervalMs / 1000.0;
    const double rowPrice = feed.rowPrice();
    if (!st || st->empty() || v.devW <= 0 || v.devH <= 0 || !(v.t1 > v.t0) || !(v.p1 > v.p0) || !(rowPrice > 0))
        return false;
    // The palette, blended over the chart's background: a new one (theme, setting) recolours everything.
    const Rgb bg{uint8_t(qRed(v.background)), uint8_t(qGreen(v.background)), uint8_t(qBlue(v.background))};
    std::vector<PaletteStop> stops = paletteStops(s.palette, bg, s.low, s.mid, s.high);
    const bool bgChanged = v.background != m_paletteBg || stops != m_paletteStops;
    if (bgChanged) {
        m_palette = buildPalette(stops, bg);
        m_paletteStops = std::move(stops);
        m_paletteBg = v.background;
    }

    // A texel is about a device pixel: c samples × r groups of `g` rows, powers of two.
    const int g = std::max(1, s.group);
    const int c = pow2ceil((v.t1 - v.t0) / iv / v.devW);
    const int r = pow2ceil((v.p1 - v.p0) / (rowPrice * g) / v.devH);
    const double texelSec = c * iv, texelPrice = r * g * rowPrice;
    const int64_t vj0 = ffloor(v.t0 / texelSec), vj1 = fceil(v.t1 / texelSec);
    const int64_t vk0 = ffloor(v.p0 / texelPrice), vk1 = fceil(v.p1 / texelPrice);

    bool rebuildNow = feed.storeId() != m_storeId || c != m_c || r != m_r || g != m_g || s.agg != m_agg
                      || rowPrice != m_rowPrice || vj0 < m_wantJ0 || vj1 > m_wantJ1 || vk0 < m_wantK0 || vk1 > m_wantK1;
    const bool dataChanged = st->endIndex() != m_seen;
    std::vector<int64_t> touched;
    if (!rebuildNow && dataChanged) {
        for (uint64_t i = std::max(m_seen, st->firstIndex()); i < st->endIndex() && !m_needRebuild; ++i)
            addColumn(st->meta(i), st->rows(i), st->bandRows(), &touched);
        m_seen = st->endIndex();
        rebuildNow = m_needRebuild;
    }
    if (rebuildNow) {
        m_storeId = feed.storeId();
        m_c = c;
        m_r = r;
        m_g = g;
        m_agg = s.agg;
        m_rowPrice = rowPrice;
        m_texelSec = texelSec;
        // The window: the view and half of it again on each side in time, a quarter above and below in price.
        const int64_t wj = vj1 - vj0, wk = vk1 - vk0;
        m_wantJ0 = vj0 - wj / 2 - 1;
        m_wantJ1 = vj1 + wj / 2 + 1;
        m_wantK0 = vk0 - wk / 4 - 1;
        m_wantK1 = vk1 + wk / 4 + 1;
        rebuild(feed);
        m_stats.lastRebuildMs = double(t.nsecsElapsed()) / 1e6;
        ++m_stats.rebuilds;
    } else {
        for (int64_t j : touched) finish(j);
    }
    if (m_W <= 0 || m_H <= 0) return false;

    // The colour scale follows the visible texels; small moves keep it (no flicker), new samples colour their column.
    if (rebuildNow || dataChanged || !(v == m_view) || !sameLook(s.look, m_look) || bgChanged) {
        const double ref = visiblePercentile(v, s.look.percentile);
        const bool lookChanged = !sameLook(s.look, m_look);
        m_view = v;
        m_look = s.look;
        if (ref > 0 && (!m_lutBuilt || lookChanged || std::fabs(ref / m_lut.reference() - 1) > 0.08)) {
            m_lut.build(ref, s.look);
            m_lutBuilt = true;
            colourAll();
        } else if (rebuildNow || bgChanged) {
            colourAll();
        } else {
            for (int64_t j : touched) colourColumn(j);
        }
    }
    m_stats.bytes = size_t(m_img.sizeInBytes()) + m_codes.capacity() * sizeof(Code) + m_val.capacity() * sizeof(float);
    m_stats.lastUpdateMs = double(t.nsecsElapsed()) / 1e6;
    return true;
}

void HeatmapTexture::rebuild(const DepthFeed &feed)
{
    m_needRebuild = false;
    const ColumnStore &st = *feed.store();
    const double tA = double(m_wantJ0) * m_texelSec, tB = double(m_wantJ1) * m_texelSec;
    // Zoomed out: the pyramid level whose columns are whole texels (4^L ≤ c samples), when it aggregates the same way;
    // its open bucket and anything after are read from the samples.
    const ColumnPyramid *pyr = feed.pyramid();
    m_level = 0;
    if (pyr && pyr->agg() == m_agg)
        for (int L = pyr->levels(); L >= 1; --L)
            if (pyr->samplesPer(L) <= m_c) {
                m_level = L;
                break;
            }
    const ColumnStore *lvl = m_level ? &pyr->level(m_level) : nullptr;
    const double split = m_level ? std::clamp(pyr->closedUntil(m_level), tA, tB) : tA;
    const uint64_t la = lvl ? lvl->lowerBound(tA) : 0, lb = lvl ? lvl->lowerBound(split) : 0;
    const uint64_t sa = st.lowerBound(split), sb = st.lowerBound(tB);

    // The rows the columns read reach, so the texture is no taller than the data.
    int64_t lo = INT64_MAX, hi = INT64_MIN;
    double firstTime = INFINITY;
    auto extent = [&](const ColumnStore &s, uint64_t a, uint64_t b) {
        for (uint64_t i = a; i < b; ++i) {
            lo = std::min(lo, s.meta(i).anchorRow);
            hi = std::max(hi, s.meta(i).anchorRow + s.bandRows());
        }
        if (a < b) firstTime = std::min(firstTime, s.meta(a).time);
    };
    if (lvl) extent(*lvl, la, lb);
    extent(st, sa, sb);
    m_seen = st.endIndex();
    m_stats.columnsRead = size_t(lb - la) + size_t(sb - sa);
    if (lo > hi) {   // no column in the window
        m_W = m_H = 0;
        m_img = QImage();
        return;
    }
    m_J0 = std::max(m_wantJ0, ffloor(firstTime / m_texelSec));
    m_J1 = m_wantJ1;
    const int64_t rows = int64_t(m_g) * m_r;
    m_K0 = std::max(m_wantK0, floorDiv(lo, rows));
    m_K1 = std::min(m_wantK1, floorDiv(hi - 1, rows) + 1);
    m_W = int(std::max<int64_t>(0, m_J1 - m_J0));
    m_H = int(std::max<int64_t>(0, m_K1 - m_K0));
    if (m_W <= 0 || m_H <= 0) {
        m_img = QImage();
        return;
    }
    const size_t n = size_t(m_W) * size_t(m_H);
    m_val.assign(n, 0.f);
    m_codes.assign(n, 0);
    m_count.assign(size_t(m_W), 0);
    if (m_img.width() != m_W || m_img.height() != m_H) m_img = QImage(m_W, m_H, QImage::Format_RGB32);
    if (lvl)
        for (uint64_t i = la; i < lb; ++i) addColumn(lvl->meta(i), lvl->rows(i), lvl->bandRows(), nullptr);
    for (uint64_t i = sa; i < sb; ++i) addColumn(st.meta(i), st.rows(i), st.bandRows(), nullptr);
    for (int64_t j = 0; j < m_W; ++j) finish(j);
    m_needRebuild = false;   // columns outside the window were just not wanted
}

// One column (a sample, or a pyramid column of `count` samples) into its texel column: its rows grouped (summed by
// g), then each group into its texel row, by max or by sum × samples (the mean comes out in finish()).
void HeatmapTexture::addColumn(const ColumnMeta &m, const Code *rows, int band, std::vector<int64_t> *touched)
{
    const int64_t j = ffloor(m.time / m_texelSec) - m_J0;
    if (j < 0) return;
    if (j >= m_W) {   // past the texture (live, the window's right margin is used up)
        if (touched) m_needRebuild = true;
        return;
    }
    const int64_t first = groupRows(rows, band, m.anchorRow, m_g, m_groups);
    const bool mean = m_agg == Agg::Mean;
    for (size_t q = 0; q < m_groups.size(); ++q) {
        const double v = m_groups[q];
        if (!(v > 0)) continue;
        const int64_t kk = floorDiv(first + int64_t(q), m_r);
        const int64_t k = kk - m_K0;
        if (k < 0 || k >= m_H) {
            // The price moved past the texture's rows, into the window: the texture grows (rebuilt).
            if (touched && kk >= m_wantK0 && kk < m_wantK1) m_needRebuild = true;
            continue;
        }
        float &cell = m_val[size_t(k) * size_t(m_W) + size_t(j)];
        if (mean)
            cell += float(v * m.count);
        else if (v > cell)
            cell = float(v);
    }
    if (mean) m_count[size_t(j)] += m.count;
    if (touched && (touched->empty() || touched->back() != j)) touched->push_back(j);
}

void HeatmapTexture::finish(int64_t j)
{
    if (j < 0 || j >= m_W) return;
    const bool mean = m_agg == Agg::Mean;
    const double div = mean ? double(m_count[size_t(j)]) * m_r : 1;
    for (int k = 0; k < m_H; ++k) {
        const size_t i = size_t(k) * size_t(m_W) + size_t(j);
        const float v = m_val[i];
        m_codes[i] = v > 0 && div > 0 ? encode(v / div) : 0;
    }
}

void HeatmapTexture::colourColumn(int64_t j)
{
    if (j < 0 || j >= m_W) return;
    for (int k = 0; k < m_H; ++k) {
        auto *line = reinterpret_cast<QRgb *>(m_img.scanLine(m_H - 1 - k));
        line[j] = QRgb(m_palette[m_lut[m_codes[size_t(k) * size_t(m_W) + size_t(j)]]]);
    }
}

void HeatmapTexture::colourAll()
{
    for (int k = 0; k < m_H; ++k) {
        auto *line = reinterpret_cast<QRgb *>(m_img.scanLine(m_H - 1 - k));
        const Code *codes = m_codes.data() + size_t(k) * size_t(m_W);
        for (int j = 0; j < m_W; ++j) line[j] = QRgb(m_palette[m_lut[codes[j]]]);
    }
}

double HeatmapTexture::visiblePercentile(const View &v, double p)
{
    const int64_t j0 = std::clamp<int64_t>(ffloor(v.t0 / m_texelSec) - m_J0, 0, m_W);
    const int64_t j1 = std::clamp<int64_t>(fceil(v.t1 / m_texelSec) - m_J0, 0, m_W);
    const double tp = texelPrice();
    const int64_t k0 = std::clamp<int64_t>(ffloor(v.p0 / tp) - m_K0, 0, m_H);
    const int64_t k1 = std::clamp<int64_t>(fceil(v.p1 / tp) - m_K0, 0, m_H);
    static thread_local CodeHistogram h;
    h.clear();
    for (int64_t k = k0; k < k1; ++k) h.add(m_codes.data() + size_t(k) * size_t(m_W) + size_t(j0), size_t(j1 - j0));
    return h.percentile(p);
}

// ---------------------------------------------------------------- pulled / absorbed

size_t HeatmapEvents::count(LiquidityEvent::Kind k) const
{
    if (!m_det) return 0;
    return size_t(std::count_if(m_det->events().begin(), m_det->events().end(), [k](const LiquidityEvent &e) { return e.kind == k; }));
}

void HeatmapEvents::sync(const DepthFeed *depth, const TradeFeed *trades, const EventParams &p)
{
    QElapsedTimer timer;
    timer.start();
    struct Done {
        HeatmapEvents *e;
        QElapsedTimer *t;
        ~Done()
        {
            e->m_syncMs = double(t->nsecsElapsed()) / 1e6;
            e->m_syncMax = std::max(e->m_syncMax, e->m_syncMs);
        }
    } done{this, &timer};
    const ColumnStore *st = depth ? depth->rawStore() : nullptr;
    if (!st || st->empty() || !(depth->tickSize() > 0)) return;
    if (!m_det || m_rawId != depth->rawId()) {
        m_det = std::make_unique<EventDetector>(depth->rowTicks(), p);
        m_rawId = depth->rawId();
        m_nextCol = st->firstIndex();
        m_lastId = -1;
        m_lastTradeTime = -INFINITY;
    }
    m_det->setParams(p);
    m_nextCol = std::max(m_nextCol, st->firstIndex());
    const double tick = depth->tickSize();
    if (trades && m_lastId < 0 && m_nextCol < st->endIndex()) {
        // Not started yet: wait until the trades reach back to the first sample to read (the history loads newest
        // first, page by page); samples from before the trades loaded (the history stopped earlier) are read without.
        const std::deque<TradeFeed::TapeTrade> &tape = trades->tape();
        const double first = st->meta(m_nextCol).time;
        if (!trades->ready() || (trades->loadingHistory() && (tape.empty() || tape.front().t.time > first))) return;
    }
    if (trades && trades->ready()) {
        const std::deque<TradeFeed::TapeTrade> &tape = trades->tape();
        if (trades->tapeGeneration() != m_tapeGen) {
            m_tapeGen = trades->tapeGeneration();
            m_lastId = -1;
        }
        // From the first trade not given yet: by id, or at first by time (from the first sample still to come).
        auto it = m_lastId >= 0 ? std::upper_bound(tape.begin(), tape.end(), m_lastId,
                                                   [](qint64 id, const TradeFeed::TapeTrade &t) { return id < t.id; })
                                : std::lower_bound(tape.begin(), tape.end(), m_nextCol < st->endIndex() ? st->meta(m_nextCol).time : INFINITY,
                                                   [](const TradeFeed::TapeTrade &t, double time) { return t.t.time < time; });
        for (; it != tape.end(); ++it) {
            m_det->addTrade(it->t.time, std::llround(it->t.price / tick), it->t.qty, it->t.side);
            m_lastId = it->id;
            m_lastTradeTime = std::max(m_lastTradeTime, it->t.time);
        }
    }
    const uint64_t end = st->endIndex();
    while (m_nextCol < end) {
        const ColumnMeta &m = st->meta(m_nextCol);
        if (!(m.time <= m_lastTradeTime || m_nextCol + 8 < end)) break;
        m_det->addColumn(m, st->rows(m_nextCol), st->bandRows());
        ++m_nextCol;
    }
    m_det->dropBefore(st->meta(st->firstIndex()).time);
}

// Pulled: a dashed line at the level's row from when it went to when the price got there, a cross where it went.
// Absorbed: a bar at the level's price over the time it held, a diamond where the price left it.
void HeatmapLayer::drawEvents(QPainter &p, const QSizeF &size)
{
    if (!m_settings.events || !m_events || !m_events->detector() || !m_feed || !m_series) return;
    TimeMap tm;
    if (!timeMap(tm)) return;
    const double rowPrice = m_feed->rowPrice(), tick = m_feed->tickSize();
    const QColor pulled("#ff9100"), absorbed("#e040fb");
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    // Not past the last bar (replay: what happened after the cursor isn't known yet).
    const double end = tm.bars->last().time + tm.tf;
    for (const LiquidityEvent &e : m_events->detector()->events()) {
        if (e.t1 < tm.t0 || e.t0 > tm.t1 || e.t1 > end) continue;
        const bool isPulled = e.kind == LiquidityEvent::Kind::Pulled;
        const double price = isPulled ? (double(e.row) + 0.5) * rowPrice : double(e.tick) * tick;
        const auto y = m_series->priceToCoordinate(price);
        if (!y || *y < -8 || *y > size.height() + 8) continue;
        const double x0 = tm.xOfTime(e.t0), x1 = std::max(x0 + 2, tm.xOfTime(e.t1));
        if (isPulled) {
            QPen pen(pulled, 1.2, Qt::DashLine);
            p.setPen(pen);
            p.drawLine(QPointF(x0, *y), QPointF(x1, *y));
            p.setPen(QPen(pulled, 2));
            p.drawLine(QPointF(x0 - 4, *y - 4), QPointF(x0 + 4, *y + 4));
            p.drawLine(QPointF(x0 - 4, *y + 4), QPointF(x0 + 4, *y - 4));
        } else {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(absorbed.red(), absorbed.green(), absorbed.blue(), 210));
            p.drawRect(QRectF(x0, *y - 2, x1 - x0, 4));
            QPolygonF d;
            d << QPointF(x1, *y - 5) << QPointF(x1 + 5, *y) << QPointF(x1, *y + 5) << QPointF(x1 - 5, *y);
            p.setBrush(absorbed);
            p.setPen(QPen(theme::color("--bg"), 1));
            p.drawPolygon(d);
        }
    }
    p.restore();
}

// ---------------------------------------------------------------- the layer

class HeatmapLayer::Overlay : public lw::Primitive
{
public:
    explicit Overlay(HeatmapLayer *l) : m_l(l) {}
    ZOrder zOrder() const override { return ZOrder::Top; }
    void draw(QPainter &p, const QSizeF &size) override { m_l->drawOverlay(p, size); }

private:
    HeatmapLayer *m_l;
};

class HeatmapLayer::Dom : public lw::Primitive
{
public:
    explicit Dom(HeatmapLayer *l) : m_l(l) {}
    ZOrder zOrder() const override { return ZOrder::Normal; }   // over the candles, under the crosshair
    void draw(QPainter &p, const QSizeF &size) override
    {
        m_l->drawEvents(p, size);
        m_l->drawDom(p, size);
    }

private:
    HeatmapLayer *m_l;
};

HeatmapLayer::HeatmapLayer(ChartPane *pane, std::shared_ptr<HeatmapTexture> texture, const HeatmapSettings &s)
    : m_pane(pane), m_tex(std::move(texture)), m_settings(s), m_dom(std::make_unique<Dom>(this)),
      m_overlay(std::make_unique<Overlay>(this))
{
    m_cpuClock.start();
}

HeatmapLayer::~HeatmapLayer() = default;

void HeatmapLayer::scheduleUpdate()
{
    if (!m_timer) {
        m_timer = std::make_unique<QTimer>();
        m_timer->setSingleShot(true);
        m_timer->setInterval(16);   // the screen's rate at most
        QObject::connect(m_timer.get(), &QTimer::timeout, [this] { requestUpdate(); });   // repaints the overlay too
    }
    if (!m_timer->isActive()) m_timer->start();
}

bool HeatmapLayer::timeMap(TimeMap &m) const
{
    static const QStringList timeless = {"renko", "linebreak", "range", "kagi", "pnf"};
    if (timeless.contains(m_pane->cfg.type)) return false;
    const Bars &bars = m_pane->shown;
    if (bars.isEmpty()) return false;
    try {
        m.tf = double(parseTf(m_pane->cfg.tf).seconds);
    } catch (...) {
        m.tf = 60;
    }
    if (!m.ax.build(m_pane->chart->timeScale(), bars, [this](double t) { return m_pane->toChart(t); }, m.tf)) return false;
    m.bars = &bars;
    m.n = m.ax.n;
    m.from = m.ax.from;
    m.to = m.ax.to;
    m.bs = m.ax.bs;
    m.t0 = m.ax.t0;
    m.t1 = m.ax.t1;
    return true;
}

void HeatmapLayer::draw(QPainter &p, const QSizeF &size)
{
    QElapsedTimer ft;
    ft.start();
    m_frameMs = 0;
    if (!m_feed || !m_feed->store() || !m_series) return;
    TimeMap tm;
    if (!timeMap(tm)) return;
    const Bars &bars = *tm.bars;
    const int from = tm.from, to = tm.to;
    const double tf = tm.tf;
    const auto pTop = m_series->coordinateToPrice(0), pBot = m_series->coordinateToPrice(size.height());
    if (!pTop || !pBot) return;
    const QTransform tr = p.transform();
    HeatmapTexture::View v;
    v.t0 = tm.t0;
    v.t1 = tm.t1;
    v.p0 = std::min(*pTop, *pBot);
    v.p1 = std::max(*pTop, *pBot);
    v.devW = int(std::lround(size.width() * tr.m11()));
    v.devH = int(std::lround(size.height() * tr.m22()));
    v.background = m_pane->chart->options().background.rgb();
    if (!m_tex->update(*m_feed, v, m_settings)) {
        m_frameMs = double(ft.nsecsElapsed()) / 1e6;
        return;
    }
    const QImage &img = m_tex->image();
    const double texelSec = m_tex->texelSeconds(), texelPrice = m_tex->texelPrice();
    const auto yTop = m_series->priceToCoordinate(double(m_tex->k0() + img.height()) * texelPrice);
    const auto yBot = m_series->priceToCoordinate(double(m_tex->k0()) * texelPrice);
    if (!yTop || !yBot) return;

    p.save();
    p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    p.setRenderHint(QPainter::Antialiasing, false);
    // One draw per run of bars consecutive in time and side by side on the chart: inside a run time is linear in x; a
    // gap (no bar for a while, or a compared symbol's times between two bars) ends the run.
    int a = from;
    for (int i = from; i <= to; ++i) {
        if (i < to && std::fabs(bars[i + 1].time - bars[i].time - tf) < 1e-6 && tm.ax.adjacent(i)) continue;
        const double ta = bars[a].time, tb = bars[i].time + tf;
        double x0 = tm.leftOf(a), x1 = tm.rightOf(i);
        double s0 = ta / texelSec - double(m_tex->j0()), s1 = tb / texelSec - double(m_tex->j0());
        // Clip to the texture, the target with it.
        const double W = img.width();
        if (s1 > W) {
            x1 = x0 + (x1 - x0) * (W - s0) / (s1 - s0);
            s1 = W;
        }
        if (s0 < 0) {
            x0 = x0 + (x1 - x0) * (0 - s0) / (s1 - s0);
            s0 = 0;
        }
        if (s1 > s0 && x1 > x0)
            p.drawImage(QRectF(x0, *yTop, x1 - x0, *yBot - *yTop), img, QRectF(s0, 0, s1 - s0, img.height()));
        a = i + 1;
    }
    p.restore();

    // Column made on the feed's thread → on screen.
    const uint64_t end = m_feed->store()->endIndex();
    if (end != m_lastEnd) {
        if (m_lastEnd && !std::isnan(m_feed->newestMadeAtMs())) {
            m_latency.push_back(DepthFeed::steadyMs() - m_feed->newestMadeAtMs());
            if (m_latency.size() > 20) m_latency.pop_front();
        }
        m_lastEnd = end;
    }
    m_frameMs = double(ft.nsecsElapsed()) / 1e6;
}


// The newest column as a compact DOM at the right edge: one bar per row of the heatmap at this zoom (the same price
// rows), its length the quantity resting there against the biggest one shown, bids in the buyers' colour, asks in the
// sellers', a row holding both (the spread, rows grouped) in grey; the quantity in figures when the row is tall enough.
void HeatmapLayer::drawDom(QPainter &p, const QSizeF &size)
{
    QElapsedTimer t;
    t.start();
    m_domMs = 0;
    if (!m_settings.dom || !m_feed || !m_series) return;
    const ColumnStore *st = m_feed->store();
    if (!st || st->empty()) return;
    // The newest column; in replay, the last one before the cursor (the end of the last bar shown).
    uint64_t last = st->endIndex() - 1;
    if (m_pane->replay && !m_pane->shown.isEmpty()) {
        double tf = 60;
        try {
            tf = double(parseTf(m_pane->cfg.tf).seconds);
        } catch (...) {
        }
        const uint64_t at = st->lowerBound(m_pane->shown.last().time + tf);
        if (at == st->firstIndex()) return;
        last = at - 1;
    }
    const ColumnMeta &m = st->meta(last);
    const Code *rows = st->rows(last);
    const double rowPrice = m_feed->rowPrice(), tick = m_feed->tickSize();
    // The heatmap's rows at this zoom (its texel rows), or the grouped rows before it has drawn.
    const int64_t per = std::max<int64_t>(1, std::llround((m_tex->texelPrice() > 0 ? m_tex->texelPrice() : rowPrice * m_settings.group) / rowPrice));
    const double domRowPrice = double(per) * rowPrice;
    std::vector<double> q;
    const int64_t first = groupRows(rows, st->bandRows(), m.anchorRow, int(per), q);
    const double bb = m.bestBid == NO_TICK ? -INFINITY : double(m.bestBid) * tick;
    const double ba = m.bestAsk == NO_TICK ? INFINITY : double(m.bestAsk) * tick;

    struct Row {
        double y0, y1, q;
        int side;   // 1 bid, −1 ask, 0 both
    };
    std::vector<Row> vis;
    double maxQ = 0;
    for (size_t i = 0; i < q.size(); ++i) {
        if (!(q[i] > 0)) continue;
        const double lo = double(first + int64_t(i)) * domRowPrice, hi = lo + domRowPrice;
        const auto yA = m_series->priceToCoordinate(hi), yB = m_series->priceToCoordinate(lo);
        if (!yA || !yB || *yB < 0 || *yA > size.height()) continue;
        const int side = hi <= bb + tick / 2 ? 1 : lo >= ba - tick / 2 ? -1 : 0;
        vis.push_back({*yA, *yB, q[i], side});
        maxQ = std::max(maxQ, q[i]);
    }
    // Right of the last bar when the chart leaves room there (at least DOM_MIN_WIDTH: the right offset, or a replay
    // cursor), so it covers no candle; otherwise at the right edge, over the newest bars.
    double x0 = size.width() - DOM_WIDTH, w = DOM_WIDTH;
    if (TimeMap tm; timeMap(tm)) {
        const double lastEnd = tm.rightOf(tm.n - 1) + 4;
        if (lastEnd >= 0 && size.width() - lastEnd >= DOM_MIN_WIDTH) {
            x0 = lastEnd;
            w = std::min(DOM_WIDTH, size.width() - lastEnd);
        }
    }
    p.save();
    p.setRenderHint(QPainter::Antialiasing, false);
    p.fillRect(QRectF(x0, 0, w, size.height()), theme::color("--bg", 0.82));
    p.setPen(QPen(theme::color("--border"), 1));
    p.drawLine(QPointF(x0 + 0.5, 0), QPointF(x0 + 0.5, size.height()));
    const QColor up = theme::color("--up", 0.75), down = theme::color("--down", 0.75), both = theme::color("--muted", 0.6);
    const QFont f = theme::font(10);
    const QFontMetricsF fm(f);
    p.setFont(f);
    for (const Row &r : vis) {
        const double h = r.y1 - r.y0;
        const double len = maxQ > 0 ? (w - 6) * r.q / maxQ : 0;
        p.fillRect(QRectF(x0 + w - len, r.y0 + (h > 3 ? 0.5 : 0), std::max(1.0, len), std::max(1.0, h > 3 ? h - 1 : h)),
                   r.side > 0 ? up : r.side < 0 ? down : both);
        if (h >= fm.height() - 1) {
            p.setPen(theme::color("--text"));
            p.drawText(QRectF(x0 + 4, r.y0, w - 8, h), Qt::AlignVCenter | Qt::AlignLeft, footprint::formatVolume(r.q));
        }
    }
    p.restore();
    m_domMs = double(t.nsecsElapsed()) / 1e6;
}

void HeatmapLayer::drawOverlay(QPainter &p, const QSizeF &size)
{
    if (!m_settings.debug || !m_feed) return;
    // The app's CPU (all threads), refreshed once a second.
    const double wall = double(m_cpuClock.nsecsElapsed()) / 1e6;
    if (wall - m_cpuWall >= 1000 || m_cpuAt < 0) {
#ifdef _WIN32
        FILETIME created, exited, kernel, user;
        GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user);
        auto ms = [](const FILETIME &t) { return double((quint64(t.dwHighDateTime) << 32) | t.dwLowDateTime) / 1e4; };
        const double cpu = ms(kernel) + ms(user);   // 100 ns units to milliseconds
#else
        rusage ru{};
        getrusage(RUSAGE_SELF, &ru);
        const double cpu = double(ru.ru_utime.tv_sec + ru.ru_stime.tv_sec) * 1e3
                           + double(ru.ru_utime.tv_usec + ru.ru_stime.tv_usec) / 1e3;
#endif
        if (m_cpuAt >= 0) m_cpuApp = 100 * (cpu - m_cpuAt) / std::max(1.0, wall - m_cpuWall);
        m_cpuAt = cpu;
        m_cpuWall = wall;
    }
    const QLocale en(QLocale::English);
    const DepthStats &st = m_feed->stats();
    const ColumnStore *s = m_feed->store();
    const HeatmapTexture::Stats &ts = m_tex->stats();
    auto mb = [](double b) { return QString::number(b / 1e6, 'f', 1) + " MB"; };
    QStringList lines;
    lines << QString("%1 · %2 · rows %3 (%4 ticks) · %5 ms")
                 .arg(m_feed->ticker(), st.us ? "binance.us" : "binance.com", theme::fmtPrice(m_feed->rowPrice()))
                 .arg(m_feed->rowTicks())
                 .arg(m_feed->params().intervalMs);
    lines << QString("book: %1 · %2 levels · snapshot %3 – %4 · %5 upd/s · %6 levels/s · gaps %7 · crossed %8 · snapshots %9")
                 .arg(st.synced ? "synced" : "NOT SYNCED", en.toString(qulonglong(st.levels)), theme::fmtPrice(st.snapshotLow),
                      theme::fmtPrice(st.snapshotHigh), QString::number(st.updatesPerSec, 'f', 1),
                      QString::number(st.levelsPerSec, 'f', 0))
                 .arg(st.gaps)
                 .arg(st.crossed)
                 .arg(st.snapshots);
    if (s) {
        const double span = s->empty() ? 0 : s->meta(s->endIndex() - 1).time - s->meta(s->firstIndex()).time;
        const ColumnPyramid *pyr = m_feed->pyramid();
        lines << QString("store: %1 / %2 columns (%3 min) · %4 (full: %5) · pyramid %6 (%7)")
                     .arg(en.toString(qulonglong(s->size())), en.toString(qulonglong(s->capacity())), QString::number(span / 60, 'f', 1),
                          mb(double(s->bytes())), mb(double(ColumnStore::bytesFor(s->bandRows(), s->capacity()))),
                          mb(pyr ? double(pyr->bytes()) : 0), pyr && pyr->agg() == Agg::Mean ? "mean" : "max");
        lines << QString("texture %1×%2 · a texel = %3 samples × %4 rows of %5 (group %6, %7) · read level %8: %9 columns · %10")
                     .arg(m_tex->image().width())
                     .arg(m_tex->image().height())
                     .arg(m_tex->c())
                     .arg(m_tex->r())
                     .arg(theme::fmtPrice(m_feed->rowPrice() * m_settings.group))
                     .arg(m_settings.group)
                     .arg(m_settings.agg == Agg::Mean ? "mean" : "max")
                     .arg(m_tex->level())
                     .arg(en.toString(qulonglong(ts.columnsRead)), mb(double(ts.bytes)));
    }
    double lat = 0, latMax = 0;
    for (double l : m_latency) {
        lat += l / double(m_latency.size());
        latMax = std::max(latMax, l);
    }
    lines << QString("cpu: app %1 % · depth thread %2 % · heatmap frame %3 ms (texture %4 ms, last rebuild %5 ms, %6 rebuilds) · DOM %7 ms")
                 .arg(QString::number(m_cpuApp, 'f', 0), QString::number(st.threadCpu, 'f', 1), QString::number(m_frameMs, 'f', 2),
                      QString::number(ts.lastUpdateMs, 'f', 2), QString::number(ts.lastRebuildMs, 'f', 1))
                 .arg(ts.rebuilds)
                 .arg(QString::number(m_domMs, 'f', 2));
    lines << QString("latency: exchange → app %1 ms (median, clocks' offset included) · column → screen %2 ms (max %3)")
                 .arg(std::isnan(st.netLatencyMs) ? QString("–") : QString::number(st.netLatencyMs, 'f', 0),
                      QString::number(lat, 'f', 1), QString::number(latMax, 'f', 1));
    if (st.recording || !st.recordNote.isEmpty() || st.loadedFiles)
        lines << QString("recording: %1 · loaded %2 file(s), %3 columns in %4 ms")
                     .arg(st.recording ? QString("%1 written this session to %2").arg(mb(double(st.recordedBytes)), QFileInfo(st.recordFile).fileName())
                                       : st.recordNote.isEmpty() ? QString("off") : st.recordNote)
                     .arg(st.loadedFiles)
                     .arg(en.toString(qulonglong(st.loadedColumns)))
                     .arg(QString::number(st.loadMs, 'f', 0));
    if (m_feed->stableSamples())
        lines << QString("stability filter: ≥ %1 ms resting (%2 samples, approximation) · %3")
                     .arg(m_settings.stableMs)
                     .arg(m_feed->stableSamples())
                     .arg(m_feed->rebuildingDisplay() ? QString("rebuilding in the background…")
                                                      : QString("rebuilt in %1 ms").arg(m_feed->lastDisplayRebuildMs(), 0, 'f', 1));
    if (m_events)
        lines << QString("events: %1 pulled · %2 absorbed · %3 touches held ≥ %4 s, best traded / resting %5 · sync %6 ms (max %7)")
                     .arg(m_events->count(LiquidityEvent::Kind::Pulled))
                     .arg(m_events->count(LiquidityEvent::Kind::Absorbed))
                     .arg(m_events->detector() ? m_events->detector()->diag().heldTouches : 0)
                     .arg(m_events->detector() ? m_events->detector()->params().absorbMinSeconds : 0)
                     .arg(QString::number(m_events->detector() ? m_events->detector()->diag().bestRatio : 0, 'f', 2),
                          QString::number(m_events->lastSyncMs(), 'f', 2), QString::number(m_events->maxSyncMs(), 'f', 1));
    if (!st.error.isEmpty()) lines << "error: " + st.error;

    const QFont f = theme::font(10.5);
    const QFontMetricsF fm(f);
    double w = 0;
    for (const QString &l : lines) w = std::max(w, fm.horizontalAdvance(l));
    const double lh = fm.height() + 1, pad = 6;
    const QRectF box(8, size.height() - lines.size() * lh - 2 * pad - 8, w + 2 * pad, lines.size() * lh + 2 * pad);
    p.save();
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 170));
    p.drawRoundedRect(box, 4, 4);
    p.setFont(f);
    p.setPen(QColor(230, 235, 240));
    for (int i = 0; i < lines.size(); ++i)
        p.drawText(QPointF(box.left() + pad, box.top() + pad + fm.ascent() + i * lh), lines[i]);
    p.restore();
}

} // namespace qtm
