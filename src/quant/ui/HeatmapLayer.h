#pragma once

// The liquidity heatmap on the chart (a Deepwater addition, not in Quant Terminal): the "heatmap" indicator. The depth
// feed (server/DepthStream, ui/DepthFeed) fills a store of columns and its time pyramid; this draws them under the candles, and the
// current book (the newest column) as a compact DOM at the right edge.
//
// The chart is painted with QPainter (no GPU), so the heatmap is an image, a "texture" of texels in time × price, kept
// between frames. Rows are first grouped (the setting: 1, 2, 4… storage rows summed into one), then a texel is `c`
// samples × `r` groups, powers of two chosen so a texel is about a device pixel; when several samples or groups fall
// in one texel they are aggregated (max or mean, the setting). Zoomed out, whole pyramid columns (4^L samples, already
// reduced) are read instead of the samples, so a view of hours reads thousands of columns. The texture covers the
// visible window and a margin around it; scrolling inside it, or zooming within the same c and r, only changes where it
// is drawn (QPainter scales it, no smoothing). New samples only redo their own texel column. It is rebuilt when the
// view leaves it, when c or r change, or when the colour scale moves by more than 8 % (the reference size is a
// percentile of the visible texels).
//
// The texture outlives the layers (ChartPane rebuilds them with the indicators, every 1.5 s live): one per indicator.

#include "../chart/Chart.h"
#include "../core/Heatmap.h"
#include "../core/HeatmapEvents.h"
#include "BarAxis.h"

#include <QElapsedTimer>
#include <QImage>
#include <QJsonObject>
#include <QTimer>

#include <array>
#include <deque>
#include <memory>
#include <vector>

namespace qtm {

class ChartPane;
class DepthFeed;
class TradeFeed;

struct HeatmapSettings {
    int intervalMs = 250;
    int historyMinutes = 120;
    int group = 1;                       // storage rows summed into one price row
    heatmap::Agg agg = heatmap::Agg::Max;
    bool dom = true;                     // the current book at the right edge
    int stableMs = 0;                    // > 0: only the liquidity that rested this long (an approximation)
    bool events = true;                  // mark pulled and absorbed liquidity
    bool record = false;                 // write the order book to disk (the first heatmap's)
    int keepDays = 7;
    heatmap::EventParams eventParams;    // its thresholds (the first heatmap's)
    bool debug = false;
    heatmap::Look look;
    heatmap::PaletteKind palette = heatmap::PaletteKind::Deep;
    heatmap::Rgb low{30, 58, 138}, mid{34, 211, 238}, high{253, 224, 71};   // the Custom palette's
    // `colors`: the indicator's colours (Settings › Colours), for the Custom palette.
    static HeatmapSettings fromJson(const QJsonObject &o, const QHash<QString, QString> &colors = {});
};

class HeatmapTexture
{
public:
    // What the chart shows: UTC times, prices (bottom, top), the pane in device pixels.
    struct View {
        double t0 = 0, t1 = 0, p0 = 0, p1 = 0;
        int devW = 0, devH = 0;
        QRgb background = 0;   // the chart's: the texture is opaque, blended over it (a plain copy to draw, no blending)
        bool operator==(const View &o) const = default;
    };
    // Up to date with the feed and the view (cheap when nothing changed). False when there is nothing to draw.
    bool update(const DepthFeed &feed, const View &v, const HeatmapSettings &s);

    const QImage &image() const { return m_img; }
    double texelSeconds() const { return m_texelSec; }
    double texelPrice() const { return m_rowPrice * m_g * m_r; }
    int64_t j0() const { return m_J0; }   // the image's first texel column (time = j × texelSeconds)
    int64_t k0() const { return m_K0; }   // its lowest texel row (price = k × texelPrice), at the image's bottom
    int c() const { return m_c; }
    int r() const { return m_r; }
    int level() const { return m_level; }   // the pyramid level read (0 = the samples)

    // For the debug overlay.
    struct Stats {
        double lastRebuildMs = 0, lastUpdateMs = 0;
        int rebuilds = 0;
        size_t columnsRead = 0;   // by the last rebuild
        size_t bytes = 0;
    };
    const Stats &stats() const { return m_stats; }

private:
    void rebuild(const DepthFeed &feed);
    void addColumn(const heatmap::ColumnMeta &m, const heatmap::Code *rows, int band, std::vector<int64_t> *touched);
    void finish(int64_t j);   // texel values → codes for one texel column
    void colourColumn(int64_t j);
    void colourAll();
    double visiblePercentile(const View &v, double p);

    QImage m_img;
    std::vector<float> m_val;       // W × H from the bottom row up: max, or Σ value × samples (mean)
    std::vector<uint32_t> m_count;  // samples per texel column (mean)
    std::vector<heatmap::Code> m_codes;
    std::vector<double> m_groups;   // scratch
    int m_W = 0, m_H = 0;
    int64_t m_J0 = 0, m_J1 = 0, m_K0 = 0, m_K1 = 0;
    int m_c = 0, m_r = 0, m_g = 1, m_level = 0;
    heatmap::Agg m_agg = heatmap::Agg::Max;
    double m_texelSec = 0, m_rowPrice = 0;
    quint64 m_storeId = 0;
    uint64_t m_seen = 0;    // the store's samples up to here are in the texture
    int64_t m_wantJ0 = 0, m_wantJ1 = 0, m_wantK0 = 0, m_wantK1 = 0;   // the window asked for (the texture is that ∩ data)
    bool m_needRebuild = false;
    View m_view;
    heatmap::Look m_look;
    heatmap::IntensityLut m_lut;
    bool m_lutBuilt = false;
    std::array<uint32_t, 256> m_palette{};
    std::vector<heatmap::PaletteStop> m_paletteStops;   // what the palette was built from, and over which background
    QRgb m_paletteBg = 1;
    Stats m_stats;
};

// Pulled and absorbed liquidity for one pane: core/HeatmapEvents fed with the depth feed's samples (not the filtered
// ones) and the pane's trades, in time order. It starts once the trades' history reaches back to the first sample
// (samples loaded from recordings are older than the live trades); then a sample goes in once the trades have caught
// up with it (or it is 2 s old: a quiet market).
class HeatmapEvents
{
public:
    void sync(const DepthFeed *depth, const TradeFeed *trades, const heatmap::EventParams &p);
    double lastSyncMs() const { return m_syncMs; }
    double maxSyncMs() const { return m_syncMax; }
    const heatmap::EventDetector *detector() const { return m_det.get(); }
    size_t count(heatmap::LiquidityEvent::Kind k) const;

private:
    std::unique_ptr<heatmap::EventDetector> m_det;
    quint64 m_rawId = 0, m_tapeGen = 0;
    uint64_t m_nextCol = 0;
    qint64 m_lastId = -1;
    double m_lastTradeTime = -INFINITY;
    double m_syncMs = 0, m_syncMax = 0;
};


class HeatmapLayer : public lw::Primitive
{
public:
    HeatmapLayer(ChartPane *pane, std::shared_ptr<HeatmapTexture> texture, const HeatmapSettings &s);
    ~HeatmapLayer() override;

    void setFeed(DepthFeed *feed) { m_feed = feed; }   // not owned; null = nothing drawn
    void setEvents(const HeatmapEvents *events) { m_events = events; }   // not owned; null = no markers
    const HeatmapSettings &settings() const { return m_settings; }
    void scheduleUpdate();   // at most ~60 redraws a second

    ZOrder zOrder() const override { return ZOrder::Bottom; }   // under the grid and the candles
    void draw(QPainter &p, const QSizeF &size) override;

    // Also attach: the DOM at the right edge (over the candles) and the debug overlay (over everything).
    lw::Primitive *dom() const { return m_dom.get(); }
    lw::Primitive *overlay() const { return m_overlay.get(); }

    static constexpr double DOM_WIDTH = 96, DOM_MIN_WIDTH = 56;

private:
    class Overlay;
    class Dom;
    friend class Overlay;
    friend class Dom;
    void drawOverlay(QPainter &p, const QSizeF &size);
    void drawDom(QPainter &p, const QSizeF &size);
    void drawEvents(QPainter &p, const QSizeF &size);
    // Where a time is on the chart: bar i spans logical i − ½ … i + ½ and its slot [time, time + tf). False when the
    // chart has no time-based bars to place things on.
    struct TimeMap {
        BarAxis ax;   // each bar at its own index on the chart (a compared symbol adds times)
        const Bars *bars = nullptr;
        int n = 0, from = 0, to = 0;
        double bs = 1, tf = 60;
        double t0 = 0, t1 = 0;   // the visible bars' times
        double xOfTime(double t) const { return ax.xOfTime(t); }
        double leftOf(int i) const { return ax.xOfBar(i) - bs / 2; }
        double rightOf(int i) const { return ax.xOfBar(i) + bs / 2; }
    };
    bool timeMap(TimeMap &m) const;

    ChartPane *m_pane;
    std::shared_ptr<HeatmapTexture> m_tex;
    HeatmapSettings m_settings;
    DepthFeed *m_feed = nullptr;
    const HeatmapEvents *m_events = nullptr;
    std::unique_ptr<QTimer> m_timer;
    std::unique_ptr<lw::Primitive> m_dom, m_overlay;
    // Measures.
    double m_frameMs = 0, m_domMs = 0;
    uint64_t m_lastEnd = 0;
    std::deque<double> m_latency;   // column made → drawn, ms
    double m_cpuApp = 0, m_cpuAt = -1, m_cpuWall = 0;
    QElapsedTimer m_cpuClock;
};

} // namespace qtm
