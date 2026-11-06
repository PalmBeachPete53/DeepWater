#pragma once

// A QPainter re-implementation of the subset of TradingView Lightweight Charts (v5) that
// quant-terminal uses (public/vendor/lightweight-charts.standalone.production.js). It must look
// and behave like Lightweight Charts: same layout, axis labels, tick marks, crosshair, last-value
// labels, price lines, markers, scrolling (drag, wheel, kinetic), zooming (wheel, axis drag),
// autoscale, pane separators, log/percentage scales. The API mirrors the JS one so the port of
// chart.js, panels.js and research.js reads like the original.
//
// Times are chart times in seconds (the JS `time` field, already shifted to local time by the
// caller when needed). Coordinates are in logical pixels of the chart widget, relative to the
// pane they belong to (as in Lightweight Charts' media coordinate space).
//
// Things to know when porting JS code to this API (they follow Lightweight Charts v5.2.1):
//
// - Options structs are complete values, not partial objects. To change one field do
//   `auto o = x->options(); o.field = v; x->applyOptions(o);` (JS `applyOptions({field: v})`).
//   Chart::applyOptions only touches the time scale's bar spacing / right offset when those
//   values differ from the current options (JS only applies them when present).
// - Data must be sorted by time (like JS). A whitespace item ({time} only) keeps its time slot.
//   Lines join over whitespace (JS behaviour); hide a segment with a transparent point colour:
//   a segment takes the colour of the point it starts from.
// - Logical indexes are indexes in the union of all series' times (whitespace included).
//   TimeScale::logicalToCoordinate returns 0 for a non-integer logical (as Lightweight Charts
//   does: it only accepts whole indexes). coordinateToLogical returns a whole index (the bar
//   under x). getVisibleLogicalRange returns fractional bounds.
// - Time scale operations (setVisibleLogicalRange, fitContent, scrollToRealTime ...) issued
//   before the widget is shown (or resized with resize()) are queued and applied once it has a
//   size, like the JS chart applies them on the next animation frame.
// - The last-value label shows the last *visible* value; the dashed price line shows the last
//   bar. Price scales are per pane: priceScale("right", i). Applying options to the "right"
//   scale of any pane also becomes the default for panes created later (JS merges them into
//   chart options), so set the mode before adding indicator panes if that matters.
// - A pane that loses its last series is removed (unless it is the only pane); pane indexes
//   then shift. Pane* pointers stay valid until their pane is removed. removePane() also
//   removes the series in that pane. The first pane starts with stretch factor 2, new panes 1.
// - Overlay price scales (any id other than "right"/"left") are created by addSeries and
//   deleted with their last series, so set their options after adding the series (as JS does).
// - Series markers snap to the nearest data item of the series and add vertical margins to
//   autoscale (aboveBar / belowBar), like createSeriesMarkers.
// - Clicks: like Lightweight Charts, a second click within 500 ms of the first press is a
//   double click and does not emit clicked(); a press that moves 5 px or more is a drag.
// - Wheel: vertical wheel zooms around the cursor, horizontal wheel scrolls. Kinetic scrolling
//   after a mouse drag is off by default (JS kineticScroll.mouse = false); trackpad momentum
//   comes through the wheel events.
// - takeScreenshot() renders without the crosshair but with the top layer (drawings), like
//   JS takeScreenshot(true). The image has the widget's device pixel ratio.
// - Signals are emitted synchronously from mouse handling; crosshair updates caused by data or
//   scrolling (JS re-emits them too) arrive through a queued call.
// - Text is shaped with CoreText on macOS so label widths and glyph positions match the browser.

#include <QColor>
#include <QFont>
#include <QImage>
#include <QPointF>
#include <QPointer>
#include <QSizeF>
#include <QString>
#include <QVector>
#include <QWidget>

#include <functional>
#include <limits>
#include <memory>
#include <optional>

class QPainter;

namespace lw {

constexpr double NaN = std::numeric_limits<double>::quiet_NaN();

enum class SeriesType { Candlestick, Bar, Line, Area, Baseline, Histogram };
enum class LineType { Simple = 0, WithSteps = 1, Curved = 2 };
enum class LineStyle { Solid = 0, Dotted = 1, Dashed = 2, LargeDashed = 3, SparseDotted = 4 };
enum class PriceScaleMode { Normal = 0, Logarithmic = 1, Percentage = 2, IndexedTo100 = 3 };
enum class CrosshairMode { Normal = 0, Magnet = 1, Hidden = 2 };
enum class PriceFormatType { Price, Volume, Percent };

// A CSS colour string ("#rrggbb", "#rgb", "rgb(r,g,b)", "rgba(r,g,b,a)", names) as a QColor.
// Handy when porting JS code that passes CSS colours. Invalid input gives an invalid QColor.
QColor cssColor(const QString &css);

// One data item. A whitespace item (no value) keeps its time slot but draws nothing.
struct DataItem {
    double time = 0;
    double value = NaN;                   // line / area / baseline / histogram
    double open = NaN, high = NaN, low = NaN, close = NaN;   // candlestick / bar
    QColor color;                         // per-point colour (invalid = series default)
    QColor borderColor;                   // candlestick
    QColor wickColor;                     // candlestick
    bool whitespace() const;              // true when no value and no OHLC
};

struct PriceFormat {
    PriceFormatType type = PriceFormatType::Price;
    int precision = 2;
    double minMove = 0.01;
};

struct SeriesOptions {
    // common
    QString title;
    bool visible = true;
    bool lastValueVisible = true;
    QString lastValueDetail;              // a second line in the last value's label (a candle's countdown)
    bool priceLineVisible = true;
    int priceLineWidth = 1;
    LineStyle priceLineStyle = LineStyle::Dashed;
    QColor priceLineColor;                // invalid = series colour
    QString priceScaleId = "right";       // "right" or an overlay id such as "vol"
    PriceFormat priceFormat;
    bool autoscale = true;                // false = JS autoscaleInfoProvider: () => null
    // line / histogram / area line
    QColor color;                         // invalid = type default: #2196f3 line, #26a69a histogram
    int lineWidth = 3;
    LineStyle lineStyle = LineStyle::Solid;
    LineType lineType = LineType::Simple;
    bool lineVisible = true;
    bool pointMarkersVisible = false;
    double pointMarkersRadius = NaN;      // NaN = default (lineWidth / 2 + 2)
    bool crosshairMarkerVisible = true;
    // area
    QColor lineColor = QColor("#33D778");
    QColor topColor = QColor(46, 220, 135, 102);
    QColor bottomColor = QColor(40, 221, 100, 0);
    // baseline
    double baseValue = 0;
    QColor topLineColor = QColor(38, 166, 154, 255);
    QColor bottomLineColor = QColor(239, 83, 80, 255);
    QColor topFillColor1 = QColor(38, 166, 154, 71);
    QColor topFillColor2 = QColor(38, 166, 154, 13);
    QColor bottomFillColor1 = QColor(239, 83, 80, 13);
    QColor bottomFillColor2 = QColor(239, 83, 80, 71);
    // candlestick / bar
    QColor upColor = QColor("#26a69a");
    QColor downColor = QColor("#ef5350");
    bool borderVisible = true;
    QColor borderUpColor = QColor("#26a69a");
    QColor borderDownColor = QColor("#ef5350");
    bool wickVisible = true;
    QColor wickUpColor = QColor("#26a69a");
    QColor wickDownColor = QColor("#ef5350");
    bool thinBars = true;                 // bar series
    bool openVisible = true;              // bar series
};

struct PriceLineOptions {
    double price = 0;
    QColor color;                         // invalid = #FF0000 (JS default)
    int lineWidth = 1;
    LineStyle lineStyle = LineStyle::Dashed;   // JS default
    bool lineVisible = true;
    bool axisLabelVisible = true;
    QString title;
};

struct SeriesMarker {
    double time = 0;
    enum Position { AboveBar, BelowBar, InBar } position = AboveBar;
    enum Shape { ArrowUp, ArrowDown, Circle, Square } shape = ArrowUp;
    QColor color;
    QString text;
    double size = 1;
};

struct LogicalRange {
    double from = 0;
    double to = 0;
};

class Series;
class Chart;

// JS ISeriesPrimitive with one pane view. draw() gets a painter in media coordinates of the
// series' pane (origin at the pane's top-left, size = pane area without the price axis).
// The painter is antialiased and clipped to the pane. Bottom primitives are drawn under the
// grid, Normal ones above the series, Top ones above the crosshair.
class Primitive
{
public:
    enum class ZOrder { Bottom, Normal, Top };
    virtual ~Primitive() = default;
    virtual ZOrder zOrder() const { return ZOrder::Normal; }
    virtual void attached(Series *series, std::function<void()> requestUpdate)
    {
        m_series = series;
        m_requestUpdate = std::move(requestUpdate);
    }
    virtual void detached() { m_series = nullptr; }
    virtual void draw(QPainter &p, const QSizeF &mediaSize) = 0;

protected:
    void requestUpdate() { if (m_requestUpdate) m_requestUpdate(); }
    Series *m_series = nullptr;
    std::function<void()> m_requestUpdate;
};

class PriceLine
{
public:
    void applyOptions(const PriceLineOptions &options);
    PriceLineOptions options() const;

protected:
    PriceLine() = default;
    ~PriceLine() = default;
};

class Series
{
public:
    SeriesType type() const;
    void setData(const QVector<DataItem> &data);
    void update(const DataItem &item);   // replace the last item or append a newer one
    QVector<DataItem> data() const;
    void applyOptions(const SeriesOptions &options);
    SeriesOptions options() const;
    void setLastValueDetail(const QString &text);   // only redraws: applyOptions recomputes the scales
    PriceLine *createPriceLine(const PriceLineOptions &options);   // owned by the series
    void removePriceLine(PriceLine *line);
    std::optional<double> priceToCoordinate(double price) const;
    std::optional<double> coordinateToPrice(double y) const;
    void attachPrimitive(Primitive *primitive);   // not owned
    void detachPrimitive(Primitive *primitive);
    void setMarkers(const QVector<SeriesMarker> &markers);   // JS createSeriesMarkers(series).setMarkers
    int paneIndex() const;

protected:
    Series() = default;
    ~Series() = default;
};

struct PriceScaleOptions {
    PriceScaleMode mode = PriceScaleMode::Normal;
    double scaleMarginTop = 0.2;
    double scaleMarginBottom = 0.1;
    bool borderVisible = true;
    QColor borderColor = QColor("#2B2B43");
    bool visible = true;
    bool autoScale = true;   // turned off by dragging the axis, back on by double-clicking it
};

// A handle on a price scale, looked up by (id, pane index) at each call like the JS one.
class PriceScale
{
public:
    void applyOptions(const PriceScaleOptions &options);
    PriceScaleOptions options() const;

protected:
    PriceScale() = default;
    ~PriceScale() = default;
};

class TimeScale
{
public:
    std::optional<LogicalRange> getVisibleLogicalRange() const;
    void setVisibleLogicalRange(const LogicalRange &range);
    void fitContent();
    void scrollToRealTime();
    std::optional<double> logicalToCoordinate(double logical) const;
    std::optional<double> coordinateToLogical(double x) const;
    double width() const;   // plot width (without the price axis)
    // The time points (the union of every series' times: logical indexes 0 … pointCount − 1) and the logical index of a
    // time (JS timeScale().timeToIndex(time) without findNearest: none when no series has that time).
    int pointCount() const;
    std::optional<int> timeToLogical(double time) const;
    std::optional<double> pointTime(int logical) const;   // the time at an index (none outside 0 … pointCount − 1)

protected:
    TimeScale() = default;
    ~TimeScale() = default;
};

class Pane
{
public:
    int index() const;
    double getHeight() const;
    void setStretchFactor(double factor);
    double stretchFactor() const;

protected:
    Pane() = default;
    ~Pane() = default;
};

struct ChartOptions {
    // layout
    QColor background = QColor("#ffffff");
    QColor textColor = QColor("#191919");
    int fontSize = 12;
    QString fontFamily;                    // empty = system UI font (-apple-system)
    QColor separatorColor = QColor("#E0E3EB");
    QColor separatorHoverColor = QColor(178, 181, 189, 51);
    bool enablePaneResize = true;
    // grid
    bool vertLinesVisible = true;
    QColor vertLinesColor = QColor("#D6DCDE");
    bool horzLinesVisible = true;
    QColor horzLinesColor = QColor("#D6DCDE");
    // right price scale
    bool rightPriceScaleBorderVisible = true;
    QColor rightPriceScaleBorderColor = QColor("#2B2B43");
    // time scale
    bool timeScaleBorderVisible = true;
    QColor timeScaleBorderColor = QColor("#2B2B43");
    double rightOffset = 0;
    double barSpacing = 6;
    bool timeVisible = false;
    bool secondsVisible = true;
    // Custom tick label; return a null QString to use the default label for that tick.
    std::function<QString(double time)> tickMarkFormatter;   // null = Lightweight Charts default
    // crosshair (JS crosshair.vertLine / horzLine color and labelBackgroundColor; the label text
    // is black or white, whichever reads on the label)
    CrosshairMode crosshairMode = CrosshairMode::Magnet;
    QColor crosshairLineColor = QColor("#9598A1");
    QColor crosshairLabelBackgroundColor = QColor("#131722");
    // Text colour of axis labels (last value, price lines, crosshair) for a label background;
    // null = Lightweight Charts' rule (black on light backgrounds, white otherwise).
    std::function<QColor(const QColor &background)> labelTextColor;
    // localization
    std::function<QString(double price)> priceFormatter;     // null = series priceFormat
    std::function<QString(double time)> timeFormatter;       // crosshair time label; null = default
    // interaction
    bool handleScroll = true;
    bool handleScale = true;
    bool kineticScrollMouse = false;       // JS kineticScroll.mouse
};

// What subscribeCrosshairMove / subscribeClick report (JS MouseEventParams). When the mouse
// leaves the panes, crosshairMoved() is emitted with no point/time/logical (paneIndex 0).
// logical is the bar index under the mouse (clamped to the visible bars for the crosshair);
// time is set only when that index has a time point.
struct MouseEventParams {
    std::optional<QPointF> point;          // relative to the pane the mouse is in
    std::optional<double> time;
    std::optional<double> logical;
    int paneIndex = 0;
};

namespace detail { struct ChartPrivate; }

class Chart : public QWidget
{
    Q_OBJECT
public:
    explicit Chart(const ChartOptions &options, QWidget *parent = nullptr);
    ~Chart() override;

    void applyOptions(const ChartOptions &options);
    ChartOptions options() const;

    Series *addSeries(SeriesType type, const SeriesOptions &options, int paneIndex = 0);
    void removeSeries(Series *series);
    QVector<Pane *> panes() const;
    void removePane(int index);

    TimeScale *timeScale();
    PriceScale *priceScale(const QString &id, int paneIndex = 0);

    void setCrosshairPosition(double price, double time, Series *series);
    void clearCrosshairPosition();
    QImage takeScreenshot();   // the whole chart, including axes

signals:
    void crosshairMoved(const lw::MouseEventParams &params);
    void clicked(const lw::MouseEventParams &params);
    // JS subscribeVisibleLogicalRangeChange: the visible bars changed (scroll, zoom, new data, resize). Emitted after
    // the frame that shows the new range, so at most once a frame.
    void visibleLogicalRangeChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    friend struct detail::ChartPrivate;
    std::unique_ptr<detail::ChartPrivate> d;
};

} // namespace lw
