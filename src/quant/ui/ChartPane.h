#pragma once

#include "../../AiRead.h"

// Port of quant-terminal/public/js/chart.js (ChartPane). This header is the contract the panels
// rely on; the implementer adds whatever else chart.js needs (drawings, volume profiles, live
// socket/polling, legend ...) as further members.

#include "App.h"

#include <QElapsedTimer>
#include <QSet>

#include <memory>
#include <optional>

namespace lw { class Chart; class Series; struct MouseEventParams; struct SeriesOptions; }

class QLabel;
class QTimer;

namespace qtm {

class DrawingLayer;
struct Drawing;
class VolumeProfile;
struct CandleSet;
struct StoredCandles;
class TradeFeed;
class KlineSubscription;
class FootprintLayer;
class BigTradesModel;
class BigTradesLayer;
class BigTradesTooltip;
class DepthFeed;
class ChartNavigator;
class HeatmapLayer;
class HeatmapTexture;
class HeatmapEvents;
class Legend;
class LegendInput;
class Btn;
class Select;

// JS CHART_TYPES: value, label.
extern const QVector<QPair<QString, QString>> CHART_TYPES;
QString newUid();   // JS newUid(): "i" + base36 time + base36 sequence
// The legend's short name of an indicator: "Relative Strength Index (RSI)" -> "RSI".
QString shortIndicatorName(const QString &name);

class ChartPane : public QWidget, public AiReadable
{
    Q_OBJECT
public:
    QString aiRead() const override;   // read_page: its symbol, its latest bars, compared symbols and indicators
    ChartPane(App *app, const PaneConfig &cfg, QWidget *parent = nullptr);
    ~ChartPane() override;

    App *app;
    PaneConfig cfg;
    Bars bars;                         // raw bars from the provider
    Bars shown;                        // what the chart shows (chart-type transform, replay cut)
    QVector<double> times;             // chart times of `shown`
    QString provider;
    QStringList notes;
    QHash<QString, IndicatorResult> indResults;   // by indicator uid
    QVector<IndicatorMarker> extraMarkers;        // strategy tester trade markers

    struct Replay {
        int index = 1;
        bool playing = false;
        double speed = 2;
        bool follower = false;
    };
    std::optional<Replay> replay;

    lw::Chart *chart = nullptr;
    lw::Series *main = nullptr;
    DrawingLayer *drawings = nullptr;

    void load(bool keepView = false);
    void render(bool resetView);
    double lastBarLogical() const;   // the chart's index of the last bar shown (not n − 1 with a compared symbol)
    QString addIndicator(const QString &id, const QJsonObject &params = {});
    void removeIndicator(const QString &uid);
    QString indicatorParamsLabel(const IndicatorInstance &ind) const;
    void applyTheme();
    void setInteractive(bool on);
    bool deleteSelectedDrawing();
    void clearDrawings();
    void startReplay(int index);
    void replayTo(double endTime);
    void replayStep(int dir);
    void toggleReplay();
    void stopReplay();
    QImage screenshot();

    // time helpers
    double logicalOf(double utc) const;          // NaN when there are no bars (JS null)
    double timeOfLogical(double logical) const;  // NaN when there are no bars (JS null)
    std::optional<double> timeToX(double utc) const;
    void setActiveFrame(bool active);   // the accent border of .pane.active

    // ---- the rest of chart.js ----
    double shift() const;
    double step() const;
    double toChart(double t) const { return t + shift(); }
    QString formatPrice(double v) const;
    void updateLegend(std::optional<int> index = std::nullopt);
    void showMsg(const QString &text);
    void onCrosshair(const lw::MouseEventParams &p);
    void onClick(const lw::MouseEventParams &p);
    void merge(const Bars &newBars);
    void startLive();
    void stopLive();
    // While the app is hidden (App::livePaused), live updates stop; resumeLive() catches up on
    // the bars missed meanwhile, then goes live again.
    void pauseLive();
    void resumeLive();
    void destroy();
    // The footprint chart's price step (0 until Binance has said), for its settings dialog.
    double footprintTickSize() const;
    // Big Trades: where the mouse is over the main pane (the bubble under it gets a tooltip), and the layers.
    std::optional<QPointF> bigTradesMouse;
    const std::vector<std::unique_ptr<BigTradesLayer>> &bigTradesLayers() const { return btLayers; }
    // The pane is being thrown away (layout change): stop timers and sockets.
    bool destroyed = false;
    // Which load() is the latest: a caller that started one knows its own (App::paneLoadedListeners).
    int currentLoad() const { return loadToken; }

protected:
    void resizeEvent(QResizeEvent *e) override;
    bool eventFilter(QObject *o, QEvent *e) override;

private:
    friend class Legend;
    friend class LegendInput;
    struct Mtf {
        Bars bars;
        QString error;
        double at = 0;
    };

    void createChart();
    void makeMainSeries();
    static void mainColors(lw::SeriesOptions &o, const QString &type);
    Bars visibleBars() const;
    void renderCompare();
    void renderIndicators();
    IndicatorResult computeIndicator(const IndicatorInstance &ind, const Bars &bars);
    void loadMtf(const QString &tf);
    std::optional<std::pair<double, double>> pointAt(const lw::MouseEventParams &p) const;   // time, price
    void poll();
    void catchUp(const Bars &recent);
    void layoutOverlays();
    void buildReplayBar();
    void removeReplayBar();
    void finishTextNote(Drawing item);
    // The footprint chart type (a Deepwater addition): its trade feed and drawing follow the pane's symbol and
    // timeframe; the legend gets a line on where the trades come from.
    void syncFootprint();
    void applyFootprintRows();
    QString footprintNote() const;
    // The pane's trade feed (Binance), shared by the footprint and the Big Trades indicators: made when one of them
    // needs it, dropped when none does.
    void syncTradeFeed();
    bool wantsBigTrades() const;
    void syncBigTrades();   // the models up to the feed, the layers redrawn when something came out
    QString bigTradesNote(const QString &uid) const;
    // The liquidity heatmap's depth feed (Binance's order book): made when a visible heatmap indicator needs it.
    void syncDepthFeed();
    QString heatmapNote() const;

    lw::Series *volume = nullptr;
    QString mainType;
    QVector<QPair<QString, lw::Series *>> compareSeries;
    QHash<QString, Bars> compareData;
    QHash<QString, QVector<lw::Series *>> indSeries;
    std::vector<std::unique_ptr<VolumeProfile>> vpPrims;
    std::unique_ptr<FootprintLayer> fpLayer;
    TradeFeed *tradeFeed = nullptr;
    QString feedKey;            // symbol|tf the feed is for
    QString fpUnsupported;    // why this chart has no footprint (not Binance, timeframe)
    QHash<QString, std::shared_ptr<BigTradesModel>> btModels;   // by indicator uid: they outlive the layers
    std::vector<std::unique_ptr<BigTradesLayer>> btLayers;
    std::unique_ptr<BigTradesTooltip> btTooltip;
    DepthFeed *depthFeed = nullptr;
    QString depthKey;   // symbol|interval the feed is for
    QString hmNote;     // the legend's heatmap line last shown
    QHash<QString, std::shared_ptr<HeatmapTexture>> hmTextures;   // by indicator uid: they outlive the layers
    std::vector<std::unique_ptr<HeatmapLayer>> hmLayers;
    ChartNavigator *navigator = nullptr;      // the range slider under the chart (a child widget)
    std::unique_ptr<HeatmapEvents> hmEvents;   // pulled / absorbed liquidity (the pane's depth and trades)
    size_t hmEventsShown = 0;                  // the count the legend shows
    QTimer *btTimer = nullptr;   // closes aggregates whose window is over, while trades are quiet
    int btLegendTick = 0;
    std::unique_ptr<DrawingLayer> drawingLayer;
    QHash<QString, Mtf> mtf;
    QSet<QString> mtfLoading;
    int loadToken = 0;
    QElapsedTimer loadClock;
    void loadFull(bool keepView, int token);
    void showCandles(const CandleSet &r);
    void refreshStored(const StoredCandles &stored, int token);
    void loadedFully(const CandleSet &r);
    void noteTiming(const char *step, qint64 dataMs, qint64 renderMs);
    bool volumeOffered() const;   // this chart type has volume bars, and neither Settings nor the legend's × took them off
    QTimer *countdownTimer = nullptr;
    QString countdownShown;
    void updateCountdown();
    std::unique_ptr<KlineSubscription> klineSub;   // the live candle, from the backend (Binance pairs)
    QTimer *liveTimer = nullptr;
    QTimer *replayTimer = nullptr;
    bool livePaused = false;
    double lastFull = 0, mtfCheckedAt = 0;
    bool fullPending = false;   // a transformed chart's redraw is due (merge)
    bool markersSet = true;     // the main series has markers from renderIndicators (unknown at first: true)
    mutable QString shiftTf;     // shift(): the timeframe last asked about, and whether it is intraday
    mutable bool shiftIntraday = false;
    std::optional<int> legendIndex;

    Legend *legend = nullptr;
    LegendInput *legendInput = nullptr;
    QLabel *msgEl = nullptr;
    QWidget *frame = nullptr;
    QWidget *replayBar = nullptr;
    Btn *replayPlayBtn = nullptr;
    bool frameOn = false;
};

} // namespace qtm
