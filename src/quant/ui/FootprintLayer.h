#pragma once

// The footprint chart's drawing (a Deepwater addition, not in Quant Terminal): a series primitive over the main
// series, which is left transparent (it still sets the price scale and the crosshair). It only reads the feed's
// book (TradeFeed) and the pane's bars; every number comes from core/Footprint.
//
// How much it draws depends on the zoom:
//   Text     the numbers fit in a row: "bid x ask" per row
//   Colors   rows of at least ~2 px: the same rows as coloured cells, no numbers
//   Candles  plain candles
// In every detail level each candle keeps a thin OHLC candle on its left, its POC is outlined, imbalances and
// stacked imbalance zones are coloured, and a strip at the bottom of the pane has each candle's delta and volume.

#include "../chart/Chart.h"
#include "../core/Footprint.h"

#include <QColor>
#include <QHash>
#include <QJsonObject>
#include <QStaticText>
#include <QTimer>

#include <memory>
#include <unordered_map>

namespace qtm {

class ChartPane;
class TradeFeed;

class FootprintLayer : public lw::Primitive
{
public:
    enum class Mode { BidAsk, Delta, Profile };
    enum class Detail { Text, Colors, Candles };
    // Saved with the pane (PaneConfig::footprint), as
    //   { mode: "bidask" | "delta" | "profile", rows: ticks (0 = Auto), ratio, minVolume: number | "auto",
    //     stack, budget: trades of history, buyColor, sellColor, pocColor: "#rrggbb" (absent = the theme's) }
    // with every key optional.
    struct Settings {
        Mode mode = Mode::BidAsk;
        int rowTicks = 0;   // 0 = Auto: rows of about 14 px at the current zoom (a 1-2-5 number of ticks)
        footprint::Params params = defaultParams();
        int budget = 300000;
        QColor buy, sell, poc;   // invalid = the theme's up / down, and orange

        static footprint::Params defaultParams()
        {
            footprint::Params p;
            p.autoMinVolume = true;
            return p;
        }
        static Settings fromJson(const QJsonObject &o);
        QJsonObject toJson() const;   // only what differs from the defaults
    };

    static constexpr double STRIP_HEIGHT = 30;   // the delta / volume strip at the bottom of the pane

    explicit FootprintLayer(ChartPane *pane);

    void setFeed(TradeFeed *feed);   // not owned; null = no trades (plain candles)
    void setSettings(const Settings &s);
    const Settings &settings() const { return m_settings; }
    // Trades came in: redraw, at most ~15 times a second.
    void scheduleUpdate();
    Detail lastDetail() const { return m_lastDetail; }

    ZOrder zOrder() const override { return ZOrder::Normal; }
    void draw(QPainter &p, const QSizeF &size) override;

private:
    struct Cached {
        uint64_t version = 0;
        int rowTicks = 0;
        footprint::Analysis a;
    };
    const footprint::Analysis &analysisOf(double time, const footprint::CandleFootprint &c);
    const QStaticText &label(const QString &s, const QFont &font, bool bold);

    ChartPane *m_pane;
    TradeFeed *m_feed = nullptr;
    Settings m_settings;
    std::unordered_map<double, Cached> m_cache;   // references stay valid as it grows (the frame keeps them)
    QHash<QString, QStaticText> m_labels;
    std::unique_ptr<QTimer> m_timer;
    std::unique_ptr<QTimer> m_regroup;   // Auto rows: regroup once the zoom has settled
    int m_wantRows = 0;
    Detail m_lastDetail = Detail::Candles;
};


} // namespace qtm
