#pragma once

// Internal model of the chart engine. It follows the structure of Lightweight Charts v5.2.1
// (TimeScale, PriceScale, Pane, Series, Crosshair, chart widget layout) so that behaviour and
// numbers match; comments name the JS concepts they come from.

#include "Chart.h"

#include <QElapsedTimer>
#include <QFont>
#include <QFontMetricsF>
#include <QHash>
#include <QTimer>

#include <cmath>
#include <map>
#include <memory>
#include <vector>

#include <functional>

namespace lw::detail {

struct ChartPrivate;
class PaneImpl;
class SeriesImpl;
struct PriceScaleModel;

inline double jsRound(double v) { return std::floor(v + 0.5); }   // Math.round

// ---------------------------------------------------------------- formatting (ChartFormat.cpp)

QString jsNumber(double v);              // Number#toString
QString jsToFixed(double v, int digits); // Number#toFixed

// PriceFormatter (price), PercentageFormatter and VolumeFormatter of Lightweight Charts.
struct NumberFormatter {
    enum Kind { Price, Percent, Volume };
    Kind kind = Price;
    double priceScale = 100;   // 10^precision
    double minMove = 1;        // minMove * priceScale
    int fractional = 2;
    int volumePrecision = 2;
    static NumberFormatter price(double priceScale, double minMove);
    static NumberFormatter percent(double priceScale = 100);
    static NumberFormatter volume(int precision);
    static NumberFormatter fromFormat(const PriceFormat &f);
    QString format(double v) const;

private:
    QString formatPrice(double v) const;
    QString formatVolume(double v) const;
};

struct UtcParts {
    int year = 1970, month = 1, day = 1, hour = 0, minute = 0, second = 0;
};
UtcParts utcParts(double timeSec);
int timeWeight(double cur, double prev);   // weight of a time point vs the previous one
enum class TickMarkType { Year, Month, DayOfMonth, Time, TimeWithSeconds };
TickMarkType tickMarkType(int weight, bool timeVisible, bool secondsVisible);
QString defaultTickMarkLabel(double time, TickMarkType type);
QString defaultTimeLabel(double time, bool timeVisible, bool secondsVisible);

// ---------------------------------------------------------------- text measurement

// Canvas-like text for one font. Widths are measured with digits replaced by 0 (as Lightweight
// Charts' TextWidthCache does) so labels don't jitter. On macOS the text is shaped with CoreText,
// which gives the same advances and kerning as the browser (Qt adds tracking and drops kerning
// for the system font); glyphs are then drawn by Qt at those positions.
// Test hook: when set, every label the chart draws is reported (text, left end of its baseline in
// device pixels, bold, colour). Used by the chart demo to compare labels with Lightweight Charts.
extern std::function<void(const QString &, const QPointF &, bool, const QColor &)> textTrace;

struct TextMetrics {
    TextMetrics() = default;
    ~TextMetrics();
    TextMetrics(const TextMetrics &) = delete;
    TextMetrics &operator=(const TextMetrics &) = delete;

    QFont font;
    QFontMetricsF fm{QFont()};
    double middleOffset = 0;   // alphabetic baseline below the canvas "middle" baseline
    void setFont(const QFont &f);
    double width(const QString &text, bool allDigits = false);       // measureText().width
    double centerOffset(const QString &text, bool allDigits = false); // baseline below glyph centre
    double rawWidth(const QString &text);
    // fillText with textBaseline "alphabetic" at the given baseline origin
    void draw(QPainter &p, const QPointF &origin, const QString &text, const QColor &color);

    struct Shaped {
        double width = 0;
        double top = 0, bottom = 0;   // glyph bounds above / below the baseline
        QVector<quint32> glyphs;
        QVector<QPointF> positions;
    };
    const Shaped &shape(const QString &text);
    void *ctFont = nullptr;   // CTFontRef on macOS
    QHash<QString, Shaped> shapes;
};

// ---------------------------------------------------------------- time scale

struct TimePoint {
    double time = 0;
    int weight = 0;
    int refs = 0;   // how many series (including whitespace) use this time
};

struct TimeMark {
    double coord = 0;
    QString label;
    int weight = 0;
    bool needAlignCoordinate = false;
};

struct TimeScaleModel {
    ChartPrivate *chart = nullptr;
    double width = 0;             // k_
    double barSpacing = 6;        // oc
    double rightOffset = 0;       // lc
    std::optional<int> baseIndex; // qu
    QVector<TimePoint> points;

    // drag state (startScroll / startScale) and the snapshot they work from
    std::optional<double> scrollStartX;
    std::optional<double> scaleStartX;
    bool haveSnapshot = false;
    double snapBarSpacing = 0, snapRightOffset = 0;

    // tick marks by weight, rebuilt when points change
    std::map<int, QVector<int>, std::greater<int>> marksByWeight;
    int marksCacheKey = -1;
    QVector<int> marksCache;
    bool marksByWeightDirty = true;

    bool isEmpty() const { return width == 0 || points.isEmpty() || !baseIndex; }
    int base() const { return baseIndex.value_or(0); }
    double indexToCoordinate(double index) const;   // 0 for non-integers, as LWC
    double coordOf(int index) const;                // same, no checks
    double floatIndex(double x) const;              // _coordinateToFloatIndex
    int coordinateToIndex(double x) const { return int(std::ceil(floatIndex(x))); }
    bool logicalRange(double &from, double &to) const;   // visible logical range
    bool strictRange(int &from, int &to) const;          // floor / ceil of it

    void setWidth(double w);
    void setBarSpacing(double v);
    void setRightOffset(double v);
    void correctBarSpacing();
    void correctOffset();
    double minRightOffset() const;
    double maxRightOffset() const;
    double maxBarSpacing() const;
    void zoom(double x, double scale);
    void startScroll(double x);
    void scrollTo(double x);
    void endScroll();
    void startScale(double x);
    void scaleTo(double x);
    void endScale();
    void setLogicalRange(double from, double to, bool useOptionOffset);
    void fitContent();
    void reset();
    void setBaseIndex(std::optional<int> base);
    void rebuildMarksByWeight();
    QVector<int> tickIndexes(double spacing, double maxLabelWidth);
    std::optional<int> timeToIndex(double time, bool findNearest) const;
};

// ---------------------------------------------------------------- price scale

struct PriceRange {
    double min = 0, max = 0;
    double length() const { return max - min; }
    bool isEmpty() const { return max == min || std::isnan(min) || std::isnan(max); }
    bool operator==(const PriceRange &o) const { return min == o.min && max == o.max; }
    void scaleAroundCenter(double coeff);
    void shift(double delta) { min += delta; max += delta; }
    PriceRange merged(const PriceRange &o) const;
};

struct LogFormula {
    double logicalOffset = 4;
    double coordOffset = 1e-4;
    bool operator==(const LogFormula &o) const
    {
        return logicalOffset == o.logicalOffset && coordOffset == o.coordOffset;
    }
};

struct PriceMark {
    double coord = 0;
    QString label;
    double logical = 0;
};

struct PriceScaleModel {
    ChartPrivate *chart = nullptr;
    PaneImpl *pane = nullptr;
    QString id;
    PriceScaleOptions opts;
    double height = 0;
    std::optional<PriceRange> range;   // internal units (log10 space in log mode)
    LogFormula logFormula;
    double marginAbove = 0, marginBelow = 0;   // px, from markers (autoscale info margins)
    QVector<SeriesImpl *> sources;             // in attach order
    bool rangeValid = false;                   // autoscale computed for visibleRange
    std::optional<std::pair<int, int>> visibleRange;
    std::optional<double> scaleStartPoint;
    std::optional<PriceRange> dragStartRange;
    std::optional<double> scrollStartPoint;
    QVector<PriceMark> marks;
    bool marksValid = false;
    NumberFormatter formatter;
    int tickBase = 100;
    SeriesImpl *mainSource = nullptr;

    bool isLog() const { return opts.mode == PriceScaleMode::Logarithmic; }
    bool isPercentage() const { return opts.mode == PriceScaleMode::Percentage; }
    bool isIndexed() const { return opts.mode == PriceScaleMode::IndexedTo100; }
    bool isEmpty();
    void ensureRange();
    void invalidate() { rangeValid = false; }   // the marks follow once the new range is read
    bool marksNoValue = true;   // the marks were made without a first value
    void setHeight(double h);
    double topMarginPx() const { return opts.scaleMarginTop * height + marginAbove; }
    double bottomMarginPx() const { return opts.scaleMarginBottom * height + marginBelow; }
    double internalHeight() const { return height - topMarginPx() - bottomMarginPx(); }
    std::optional<double> firstValue() const;
    void setRange(const std::optional<PriceRange> &r);
    double logicalToCoordinate(double logical, double base);
    double coordinateToLogical(double coord, double base);
    double priceToCoordinate(double price, double base);
    double coordinateToPrice(double coord, double base);
    QString formatPrice(double price, double base);   // Ji
    QString formatLogical(double logical);           // ro
    QString formatLogicalPlain(double logical);      // cr
    void updateFormatter();
    void recalculate();   // autoscale
    void autoscaleFor(std::optional<std::pair<int, int>> visible);
    void applyOptions(const PriceScaleOptions &o);
    void setMode(PriceScaleMode mode);
    const QVector<PriceMark> &tickMarks();
    void rebuildMarks();
    double tickSpan(double high, double low);
    double markHeight() const;
    void startScale(double y);
    void scaleTo(double y);
    void endScale();
    void startScroll(double y);
    void scrollTo(double y);
    void endScroll();
};

// ---------------------------------------------------------------- series

struct Row {
    int index = 0;
    double o = 0, h = 0, l = 0, c = 0;
    QColor color, border, wick;
};

class PriceLineImpl final : public PriceLine
{
public:
    SeriesImpl *series = nullptr;
    PriceLineOptions opts;
    QColor color() const { return opts.color.isValid() ? opts.color : QColor("#FF0000"); }
};

// One price axis label (a JS PriceAxisView): the axis part and the pane part (title).
struct AxisLabel {
    bool axisVisible = false;
    bool paneVisible = false;
    QString axisText;
    QString axisDetail;  // a second line under axisText
    QString paneText;
    QColor background;   // opaque
    QColor textColor;
    double coordinate = 0;
    std::optional<double> fixedCoordinate;
    std::optional<double> shifted;   // set by label collision layout
    double extraPadding = 0;         // crosshair labels: 2/12 font size
    SeriesImpl *owner = nullptr;
    double heightOf(double fontSize) const;   // $t
    double coord() const
    {
        return fixedCoordinate ? *fixedCoordinate : shifted ? *shifted : coordinate;
    }
};

struct MarkerItem {   // a marker resolved to a time index and coordinates
    int index = 0;
    double time = 0;
    SeriesMarker m;
    double x = 0, y = 0, size = 0;
    bool hasText = false;
    double textX = 0, textY = 0, textWidth = 0;
};

class SeriesImpl final : public Series
{
public:
    ChartPrivate *chart = nullptr;
    SeriesType stype = SeriesType::Line;
    SeriesOptions opts;
    QVector<DataItem> items;
    QVector<Row> rows;
    PaneImpl *pane = nullptr;
    PriceScaleModel *scale = nullptr;
    int order = 0;
    std::vector<std::unique_ptr<PriceLineImpl>> priceLines;
    QVector<Primitive *> primitives;
    QVector<SeriesMarker> markers;
    QVector<MarkerItem> markerItems;   // sorted by index, rebuilt when data/markers change
    bool markersDirty = true;
    NumberFormatter formatter;

    bool isLineLike() const
    {
        return stype == SeriesType::Line || stype == SeriesType::Area
               || stype == SeriesType::Baseline;
    }
    bool isOverlay() const;
    int rowAtOrAfter(int index) const;    // position in rows, -1 if none
    int rowAtOrBefore(int index) const;
    const Row *rowAt(int index) const;    // exact time index
    std::optional<std::pair<double, int>> firstValue() const;   // close and index (zt)
    QColor barColor(const Row &r) const;   // the "sh" colour of a bar
    double minMoveBase() const;            // 1 / minMove
    std::optional<PriceRange> autoscaleRange(int from, int to) const;
    void reindexRows();
    void rebuildMarkerItems();
    void layoutMarkers();
    void markerMargins(double &above, double &below) const;
};

// ---------------------------------------------------------------- pane

class PaneImpl final : public Pane
{
public:
    ChartPrivate *chart = nullptr;
    double stretch = 1;
    double top = 0;      // media
    double height = 0;   // media
    std::unique_ptr<PriceScaleModel> left, right;
    std::vector<std::pair<QString, std::unique_ptr<PriceScaleModel>>> overlays;
    QVector<SeriesImpl *> series;   // z-order
    double am = -1;   // optimal width of the right axis at its last marks (-1: no axis widget yet)
    PriceScaleModel *scaleById(const QString &id) const;
    PriceScaleModel *defaultScale() const;   // kn()
    QVector<PriceScaleModel *> allScales() const;
};

// ---------------------------------------------------------------- crosshair

struct CrosshairState {
    bool visible = false;
    PaneImpl *pane = nullptr;
    double rawX = NaN, rawY = NaN;
    int index = 0;
    double price = NaN;
    double x = NaN;   // coordinate of the index
    double y = NaN;
};

// ---------------------------------------------------------------- public handles

class TimeScaleHandle final : public TimeScale
{
public:
    ChartPrivate *chart = nullptr;
};

class PriceScaleHandle final : public PriceScale
{
public:
    ChartPrivate *chart = nullptr;
    QString id;
    int paneIndex = 0;
};

// Kinetic scroll after a drag (JS KineticAnimation).
struct KineticAnimation {
    double minSpeed = 0, maxSpeed = 0, dumping = 0.997, minMove = 0;
    struct P { double pos, time; };
    std::optional<P> p0, p1, p2, p3;
    std::optional<P> start;
    double speed = 0, duration = 0;
    void addPosition(double pos, double time);
    void finish(double pos, double time);
    double positionAt(double time) const;
    bool finished(double time) const;
};

// ---------------------------------------------------------------- chart

struct ChartPrivate {
    Chart *q = nullptr;
    ChartOptions opts;
    PriceScaleOptions rightDefaults;
    PriceScaleOptions overlayDefaults;
    std::vector<std::unique_ptr<PaneImpl>> panes;
    std::vector<std::unique_ptr<SeriesImpl>> series;   // insertion order
    TimeScaleModel ts;
    TimeScaleHandle tsHandle;
    std::map<std::pair<QString, int>, std::unique_ptr<PriceScaleHandle>> scaleHandles;

    // layout (media px)
    double mediaW = 0, mediaH = 0;
    double axisWidth = 0;
    double timeAxisHeight = 0;
    double paneWidth = 0;
    bool fullLayoutNeeded = true;
    bool sized = false;
    std::vector<std::function<void()>> pendingOps;

    // fonts
    TextMetrics text, bold;
    int fontSizeCached = -1;
    QString fontFamilyCached;

    CrosshairState ch;
    bool dirty = true;
    bool updating = false;
    bool emitScheduled = false;
    bool mouseInPane = false;
    double layoutDpr = 0;
    double paintDpr = 0;
    std::optional<std::pair<double, double>> emittedRange;   // the visible range last reported (Chart::visibleLogicalRangeChanged)
    QVector<AxisLabel> paneLabels;   // labels of the pane being painted

    // interaction
    enum class Area { None, Pane, PriceAxis, TimeAxis, Separator, Corner };
    struct Hit {
        Area area = Area::None;
        int pane = -1;
        QPointF local;
    };
    Hit pressHit;
    QPointF pressPos;
    QPointF lastRelease;
    bool pressed = false;
    bool dragging = false;           // moved 5 px or more since press
    bool scrollStarted = false;      // pane scroll started
    std::optional<QPointF> scrollAnchor;   // JS Hm
    bool priceScaleDragged = false;
    int clickCount = 0;
    QElapsedTimer clickTimer;
    QPointF firstPressPos;
    bool clickWindow = false;
    int hoverSeparator = -1;
    int dragSeparator = -1;
    double sepStartY = 0, sepStartStretch = 0, sepTotal = 0, sepPerPx = 0, sepMin = 0;
    std::optional<KineticAnimation> kinetic;
    QElapsedTimer clock;
    QTimer animTimer;
    std::function<bool(double now)> animation;   // returns true when done
    bool hideCrosshairForShot = false;
    int crosshairPaneIndex() const;
    double effectiveDpr() const;
    bool isOverlayScale(const PriceScaleModel *sc) const;
    void animationTick();
    void scheduleCrosshairEmit();
    void removeSeriesInternal(SeriesImpl *s, bool allowPaneRemoval);
    static SeriesOptions resolveOptions(SeriesOptions o, SeriesType type);

    explicit ChartPrivate(Chart *chart);

    // model
    PaneImpl *paneAt(int index);
    PaneImpl *createPane();
    int paneIndexOf(const PaneImpl *p) const;
    void removePaneObject(PaneImpl *p);
    PriceScaleModel *attachToScale(SeriesImpl *s, PaneImpl *pane, const QString &id);
    void detachFromScale(SeriesImpl *s);
    void renumber(PaneImpl *p);
    void setSeriesData(SeriesImpl *s, QVector<DataItem> data);
    void updateSeriesItem(SeriesImpl *s, const DataItem &item);
    void rebuildPoints(int &firstChanged);
    std::optional<int> computeBaseIndex() const;
    void applyBaseIndex(std::optional<int> newBase, bool pointsChanged, std::optional<double> oldFirst);
    void invalidateScales();
    void invalidateAll();

    // layout & update
    bool canFlush() const;
    void queueTimeOp(std::function<void()> op);
    void ensureUpdated();
    void layout(bool full);
    double optimalAxisWidth(PaneImpl *p);
    void marksRebuilt(PriceScaleModel *sc);
    bool inFrame = false, fullRequested = false;
    void updateFonts();
    void autoscaleAll();
    void request();   // schedule a repaint

    // crosshair
    void setCrosshair(double x, double y, PaneImpl *pane, bool emitEvent);
    void clearCrosshair(bool emitEvent);
    void refreshCrosshair();
    double magnet(double price, int index, PaneImpl *pane);
    MouseEventParams params(std::optional<int> logical, std::optional<QPointF> point, int pane);

    // painting
    void render(QPainter &p, double dpr);
    void drawPane(QPainter &p, PaneImpl *pane, int index, double dpr);
    void drawPriceAxis(QPainter &p, PaneImpl *pane, double dpr);
    void drawTimeAxis(QPainter &p, double dpr);
    QVector<AxisLabel> axisLabels(PaneImpl *pane, PriceScaleModel *scale);
    void layoutLabels(QVector<AxisLabel *> labels, double height, double centre);
    QVector<TimeMark> timeMarks();

    // interaction
    Hit hitTest(const QPointF &pos) const;
    void startAnimation(std::function<bool(double)> anim);
    void stopAnimation();
    void wheel(QWheelEvent *e);
    void press(QMouseEvent *e);
    void move(QMouseEvent *e);
    void release(QMouseEvent *e);
    void hover(const QPointF &pos);
    void leave();
    void doubleClick(const Hit &hit);
    void click(const Hit &hit);
    void updateCursor(const Hit &hit);
    QString formatTimeLabel(int index);
};

} // namespace lw::detail
