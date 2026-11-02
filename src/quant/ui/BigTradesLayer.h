#pragma once

// Big Trades on the chart (a Deepwater addition, not in Quant Terminal): the "bigtrades" indicator. Each instance has
// a model (its detector run over the pane's trade tape, core/BigTrades) and a layer (a series primitive drawing the
// bubbles). The model outlives the layer: ChartPane rebuilds layers with the indicators (every 1.5 s live), the model
// only when its settings change.

#include "../chart/Chart.h"
#include "../core/BigTrades.h"

#include <QColor>
#include <QElapsedTimer>
#include <QHash>
#include <QJsonObject>
#include <QPixmap>
#include <QTimer>

#include <functional>
#include <memory>
#include <vector>

template <typename T> class QFutureWatcher;

namespace qtm {

class ChartPane;
class TradeFeed;

class BigTradesModel
{
public:
    // The indicator's parameters (see the "bigtrades" definition in core/IndicatorsDefs.cpp).
    struct Params {
        bigtrades::AggregationParams agg;
        bigtrades::ThresholdParams thr;
        static Params fromJson(const QJsonObject &o);
    };
    // The settings that change which trades are big (the others only change how they look).
    static QJsonObject detectionParams(const QJsonObject &all);

    explicit BigTradesModel(const QJsonObject &params);
    ~BigTradesModel();
    // Called when a rebuild done in the background has gone in (the pane redraws and updates the legend).
    std::function<void()> onRebuilt;

    const QJsonObject &json() const { return m_json; }
    const Params &params() const { return m_p; }
    // Up to date with the feed: everything again for a new tape (a new feed, a reset) once its history is complete,
    // otherwise only the trades since last time; then the open aggregate once its window is over. `now` is the
    // exchange time it's safe to close aggregates at (a little behind the clock: trades arrive late). True when big
    // trades were added.
    bool sync(const TradeFeed *feed, double now);

    const std::vector<bigtrades::BigTrade> &trades() const { return m_trades; }   // by time
    // The biggest aggregate so far, big or not: bubble sizes are relative to it, the same for every tranche.
    double refVolume() const { return m_det.maxVolume(); }
    bigtrades::Threshold threshold() const { return m_det.threshold(); }
    bool built() const { return m_built; }
    bool complete() const { return m_complete; }   // built over the whole history (not just what had come in)
    bool approximate() const { return m_approx; }

private:
    void push(const bigtrades::BigTrade &b);
    // Everything again over the tape, on a worker thread (300,000 trades take ~80 ms): the old results stay on show
    // until the new ones are in, and live trades that came meanwhile are added after.
    struct Rebuild {
        bigtrades::Detector det;
        std::vector<bigtrades::BigTrade> trades;
        qint64 lastId = -1;
        quint64 gen = 0;
        bool complete = false;
    };
    void startRebuild(const TradeFeed *feed);
    void finishRebuild();
    std::unique_ptr<QFutureWatcher<Rebuild>> m_rebuild;

    QJsonObject m_json;
    Params m_p;
    bigtrades::Detector m_det;
    std::vector<bigtrades::BigTrade> m_trades;
    bool m_built = false, m_complete = false, m_approx = false;
    QElapsedTimer m_sinceBuild;
    quint64 m_gen = 0;
    qint64 m_lastId = -1;
};

class BigTradesLayer : public lw::Primitive
{
public:
    // How the bubbles look (the indicator's settings and colours): changing them only redraws, the model stays.
    struct Style {
        enum class Shape { Circle, Square, Diamond, Text };
        Shape shape = Shape::Circle;
        bigtrades::BubbleStyle bubble;   // radii are half the sizes set
        bool showVolume = false;
        QColor buy, sell;                // invalid = the theme's up / down
        static Style fromJson(const QJsonObject &params, const QHash<QString, QString> &colors);
    };

    BigTradesLayer(ChartPane *pane, std::shared_ptr<BigTradesModel> model, const Style &style);

    ZOrder zOrder() const override { return ZOrder::Normal; }
    void draw(QPainter &p, const QSizeF &size) override;
    void scheduleUpdate();   // at most ~15 redraws a second

    // The bubble under the mouse at the last draw (the topmost), for the tooltip.
    const bigtrades::BigTrade *hovered() const;
    QPointF hoveredAt() const { return m_hoverAt; }
    double hoveredRadius() const { return m_hoverR; }

private:
    // A bubble drawn once per shape, colour, radius (to half a pixel) and opacity (to 1/32), then copied: an
    // antialiased, translucent shape drawn each time cost ~40 µs a bubble on a Retina screen.
    const QPixmap &sprite(Style::Shape shape, const QColor &color, double radius, double alpha, double dpr);
    QHash<quint64, QPixmap> m_sprites;

    ChartPane *m_pane;
    std::shared_ptr<BigTradesModel> m_model;
    Style m_style;
    std::unique_ptr<QTimer> m_timer;
    long m_hover = -1;   // index in the model's trades
    QPointF m_hoverAt;
    double m_hoverR = 0;
};

// The tooltip of the bubble under the mouse, over everything (one for all the Big Trades layers of a pane).
class BigTradesTooltip : public lw::Primitive
{
public:
    explicit BigTradesTooltip(ChartPane *pane) : m_pane(pane) {}
    ZOrder zOrder() const override { return ZOrder::Top; }
    void draw(QPainter &p, const QSizeF &size) override;

private:
    ChartPane *m_pane;
};

} // namespace qtm
