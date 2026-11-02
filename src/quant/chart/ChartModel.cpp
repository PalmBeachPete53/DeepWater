// The chart model: time scale, price scales, series data and panes. Ported from the
// corresponding classes of Lightweight Charts v5.2.1 (TimeScale, PriceScale, PriceTickMarkBuilder,
// DataLayer, Series, Pane, ChartModel).

#include "ChartPrivate.h"

#include <QDebug>

#include <algorithm>

namespace lw::detail {

// ================================================================ time scale

double TimeScaleModel::coordOf(int index) const
{
    const double deltaFromRight = base() + rightOffset - index;
    return width - (deltaFromRight + 0.5) * barSpacing - 1;
}

double TimeScaleModel::indexToCoordinate(double index) const
{
    // Lightweight Charts only maps whole indexes; anything else lands on 0.
    if (isEmpty() || !std::isfinite(index) || std::floor(index) != index)
        return 0;
    const double deltaFromRight = base() + rightOffset - index;
    return width - (deltaFromRight + 0.5) * barSpacing - 1;
}

double TimeScaleModel::floatIndex(double x) const
{
    const double deltaFromRight = (width - 1 - x) / barSpacing;
    const double index = base() + rightOffset - deltaFromRight;
    return jsRound(index * 1000000) / 1000000;
}

bool TimeScaleModel::logicalRange(double &from, double &to) const
{
    if (isEmpty())
        return false;
    const double right = rightOffset + base();
    to = right;
    from = right - width / barSpacing + 1;
    return true;
}

bool TimeScaleModel::strictRange(int &from, int &to) const
{
    double f, t;
    if (!logicalRange(f, t))
        return false;
    from = int(std::floor(f));
    to = int(std::ceil(t));
    return true;
}

void TimeScaleModel::setWidth(double w)
{
    if (!std::isfinite(w) || w <= 0 || width == w)
        return;
    width = w;
    correctBarSpacing();
    correctOffset();
}

double TimeScaleModel::maxBarSpacing() const
{
    return 0.5 * width;   // maxBarSpacing option 0 = half the width
}

void TimeScaleModel::correctBarSpacing()
{
    const double minBs = 0.5;   // minBarSpacing; the maximum is half the width
    barSpacing = width > 0 ? std::min(std::max(barSpacing, minBs), maxBarSpacing())
                           : std::max(barSpacing, minBs);
}

double TimeScaleModel::minRightOffset() const
{
    if (points.isEmpty() || !baseIndex)
        return -std::numeric_limits<double>::infinity();
    return 0 - *baseIndex - 1 + std::min<int>(2, points.size());
}

double TimeScaleModel::maxRightOffset() const
{
    return width / barSpacing - std::min<int>(2, points.size());
}

void TimeScaleModel::correctOffset()
{
    const double mn = minRightOffset();
    if (rightOffset < mn)
        rightOffset = mn;
    // LWC knows the width from construction; here it comes with the first layout, so don't clamp
    // against a zero width (that would leave the last bars out of view).
    if (width <= 0)
        return;
    const double mx = maxRightOffset();
    if (rightOffset > mx)
        rightOffset = mx;
}

void TimeScaleModel::setBarSpacing(double v)
{
    barSpacing = v;
    correctBarSpacing();
    correctOffset();
    marksCacheKey = -1;
}

void TimeScaleModel::setRightOffset(double v)
{
    rightOffset = v;
    correctOffset();
}

void TimeScaleModel::zoom(double x, double scale)
{
    const double before = floatIndex(x);
    const double bs = barSpacing;
    setBarSpacing(bs + scale * (bs / 10));
    setRightOffset(rightOffset + (before - floatIndex(x)));
}

void TimeScaleModel::startScroll(double x)
{
    if (scrollStartX || haveSnapshot || isEmpty())
        return;
    scrollStartX = x;
    haveSnapshot = true;
    snapBarSpacing = barSpacing;
    snapRightOffset = rightOffset;
}

void TimeScaleModel::scrollTo(double x)
{
    if (!scrollStartX)
        return;
    const double shift = (*scrollStartX - x) / barSpacing;
    rightOffset = snapRightOffset + shift;
    correctOffset();
}

void TimeScaleModel::endScroll()
{
    if (!scrollStartX)
        return;
    scrollStartX.reset();
    haveSnapshot = false;
}

void TimeScaleModel::startScale(double x)
{
    if (scrollStartX)
        endScroll();
    if (scaleStartX || haveSnapshot || isEmpty())
        return;
    scaleStartX = x;
    haveSnapshot = true;
    snapBarSpacing = barSpacing;
    snapRightOffset = rightOffset;
}

void TimeScaleModel::scaleTo(double x)
{
    if (!haveSnapshot || !scaleStartX)
        return;
    const double now = std::clamp(width - x, 0.0, width);
    const double start = std::clamp(width - *scaleStartX, 0.0, width);
    if (now == 0 || start == 0)
        return;
    setBarSpacing(snapBarSpacing * now / start);
}

void TimeScaleModel::endScale()
{
    if (!scaleStartX)
        return;
    scaleStartX.reset();
    haveSnapshot = false;
}

void TimeScaleModel::setLogicalRange(double from, double to, bool useOptionOffset)
{
    const double bars = to - from + 1;
    barSpacing = width / bars;
    correctBarSpacing();
    marksCacheKey = -1;
    rightOffset = to - base();
    if (useOptionOffset && chart)
        rightOffset = chart->opts.rightOffset;
    correctOffset();
}

void TimeScaleModel::fitContent()
{
    if (points.isEmpty())
        return;
    const double off = chart ? chart->opts.rightOffset : 0;
    setLogicalRange(0, points.size() - 1 + off, true);
}

void TimeScaleModel::reset()
{
    setBarSpacing(chart ? chart->opts.barSpacing : 6);
    setRightOffset(chart ? chart->opts.rightOffset : 0);
}

void TimeScaleModel::setBaseIndex(std::optional<int> b)
{
    baseIndex = b;
    correctOffset();
}

void TimeScaleModel::rebuildMarksByWeight()
{
    marksByWeight.clear();
    for (int i = 0; i < points.size(); ++i)
        marksByWeight[points[i].weight].push_back(i);
    marksCacheKey = -1;
    marksByWeightDirty = false;
}

// TickMarks._buildMarksImpl: heavier marks first, lighter ones only where there is room.
QVector<int> TimeScaleModel::tickIndexes(double spacing, double maxLabelWidth)
{
    if (marksByWeightDirty)
        rebuildMarksByWeight();
    const int maxIndexesPerMark = int(std::ceil(maxLabelWidth / spacing));
    if (marksCacheKey == maxIndexesPerMark)
        return marksCache;
    QVector<int> marks;
    for (const auto &[weight, current] : marksByWeight) {
        Q_UNUSED(weight);
        const QVector<int> prev = marks;
        marks.clear();
        marks.reserve(prev.size() + current.size());
        int prevPointer = 0;
        double rightIndex = std::numeric_limits<double>::infinity();
        double leftIndex = -std::numeric_limits<double>::infinity();
        for (int idx : current) {
            while (prevPointer < prev.size()) {
                const int last = prev[prevPointer];
                if (last < idx) {
                    ++prevPointer;
                    marks.push_back(last);
                    leftIndex = last;
                    rightIndex = std::numeric_limits<double>::infinity();
                } else {
                    rightIndex = last;
                    break;
                }
            }
            if (rightIndex - idx >= maxIndexesPerMark && idx - leftIndex >= maxIndexesPerMark) {
                marks.push_back(idx);
                leftIndex = idx;
            }
        }
        for (; prevPointer < prev.size(); ++prevPointer)
            marks.push_back(prev[prevPointer]);
    }
    marksCacheKey = maxIndexesPerMark;
    marksCache = marks;
    return marks;
}

std::optional<int> TimeScaleModel::timeToIndex(double time, bool findNearest) const
{
    if (points.isEmpty())
        return std::nullopt;
    if (time > points.last().time)
        return findNearest ? std::optional<int>(points.size() - 1) : std::nullopt;
    auto it = std::lower_bound(points.begin(), points.end(), time,
                               [](const TimePoint &p, double t) { return p.time < t; });
    const int n = int(it - points.begin());
    if (time < points[n].time)
        return findNearest ? std::optional<int>(n) : std::nullopt;
    return n;
}

// ================================================================ price scale helpers

void PriceRange::scaleAroundCenter(double coeff)
{
    if (!std::isfinite(coeff) || max - min == 0)
        return;
    const double c = 0.5 * (max + min);
    double hi = max - c, lo = min - c;
    hi *= coeff;
    lo *= coeff;
    max = c + hi;
    min = c + lo;
}

static double mergeMin(double a, double b)
{
    const bool fa = std::isfinite(a), fb = std::isfinite(b);
    return fa && fb ? std::min(a, b) : fa ? a : fb ? b : -std::numeric_limits<double>::infinity();
}
static double mergeMax(double a, double b)
{
    const bool fa = std::isfinite(a), fb = std::isfinite(b);
    return fa && fb ? std::max(a, b) : fa ? a : fb ? b : std::numeric_limits<double>::infinity();
}

PriceRange PriceRange::merged(const PriceRange &o) const
{
    return {mergeMin(min, o.min), mergeMax(max, o.max)};
}

static double toPercent(double v, double base)
{
    const double r = 100 * (v - base) / base;
    return base < 0 ? -r : r;
}
static double toIndexed(double v, double base)
{
    const double r = 100 * (v - base) / base + 100;
    return base < 0 ? -r : r;
}
static double toLog(double v, const LogFormula &f)
{
    const double a = std::abs(v);
    if (a < 1e-15)
        return 0;
    const double r = std::log10(a + f.coordOffset) + f.logicalOffset;
    return v < 0 ? -r : r;
}
static double fromLog(double v, const LogFormula &f)
{
    const double a = std::abs(v);
    if (a < 1e-15)
        return 0;
    const double r = std::pow(10, a - f.logicalOffset) - f.coordOffset;
    return v < 0 ? -r : r;
}
static PriceRange rangeToLog(const PriceRange &r, const LogFormula &f)
{
    return {toLog(r.min, f), toLog(r.max, f)};
}
static PriceRange rangeFromLog(const PriceRange &r, const LogFormula &f)
{
    return {fromLog(r.min, f), fromLog(r.max, f)};
}
static LogFormula logFormulaFor(const std::optional<PriceRange> &r)
{
    LogFormula def;
    if (!r)
        return def;
    const double diff = std::abs(r->max - r->min);
    if (diff >= 1 || diff < 1e-15)
        return def;
    const double digits = std::ceil(std::abs(std::log10(diff)));
    LogFormula f;
    f.logicalOffset = def.logicalOffset + digits;
    f.coordOffset = 1 / std::pow(10, f.logicalOffset);
    return f;
}

// ================================================================ price scale

void PriceScaleModel::setHeight(double h)
{
    if (height == h)
        return;
    height = h;
    marksValid = false;
}

bool PriceScaleModel::isEmpty()
{
    ensureRange();
    return height == 0 || !range || range->isEmpty();
}

void PriceScaleModel::ensureRange()
{
    if (!rangeValid) {
        rangeValid = true;
        recalculate();
    }
}

void PriceScaleModel::setRange(const std::optional<PriceRange> &r)
{
    if (range.has_value() != r.has_value() || (r && !(*range == *r)))
        marksValid = false;
    range = r;
}

std::optional<double> PriceScaleModel::firstValue() const
{
    std::optional<std::pair<double, int>> best;
    for (SeriesImpl *s : sources) {
        const auto fv = s->firstValue();
        if (fv && (!best || fv->second < best->second))
            best = fv;
    }
    if (!best)
        return std::nullopt;
    return best->first;
}

double PriceScaleModel::logicalToCoordinate(double logical, double base)
{
    Q_UNUSED(base);
    ensureRange();
    if (isEmpty())
        return 0;
    const double t = isLog() && logical != 0 ? toLog(logical, logFormula) : logical;
    const PriceRange &r = *range;
    const double s = bottomMarginPx() + (internalHeight() - 1) * (t - r.min) / r.length();
    return height - 1 - s;   // invertScale is always false here
}

double PriceScaleModel::coordinateToLogical(double coord, double base)
{
    Q_UNUSED(base);
    ensureRange();
    if (isEmpty())
        return 0;
    const double inv = height - 1 - coord;
    const PriceRange &r = *range;
    const double v = r.min + r.length() * ((inv - bottomMarginPx()) / (internalHeight() - 1));
    return isLog() ? fromLog(v, logFormula) : v;
}

double PriceScaleModel::priceToCoordinate(double price, double base)
{
    if (isPercentage())
        price = toPercent(price, base);
    else if (isIndexed())
        price = toIndexed(price, base);
    return logicalToCoordinate(price, base);
}

double PriceScaleModel::coordinateToPrice(double coord, double base)
{
    double n = coordinateToLogical(coord, base);
    if (isPercentage()) {
        if (base < 0)
            n = -n;
        n = n / 100 * base + base;
    } else if (isIndexed()) {
        n -= 100;
        if (base < 0)
            n = -n;
        n = n / 100 * base + base;
    }
    return n;
}

QString PriceScaleModel::formatLogicalPlain(double v)
{
    if (chart && chart->opts.priceFormatter)
        return chart->opts.priceFormatter(v);
    return formatter.format(v);
}

QString PriceScaleModel::formatPrice(double price, double base)
{
    switch (opts.mode) {
    case PriceScaleMode::Percentage:
        return NumberFormatter::percent().format(toPercent(price, base));
    case PriceScaleMode::IndexedTo100:
        return formatter.format(toIndexed(price, base));
    default:
        return formatLogicalPlain(price);
    }
}

QString PriceScaleModel::formatLogical(double logical)
{
    switch (opts.mode) {
    case PriceScaleMode::Percentage:
        return NumberFormatter::percent().format(logical);
    case PriceScaleMode::IndexedTo100:
        return formatter.format(logical);
    default:
        return formatLogicalPlain(logical);
    }
}

void PriceScaleModel::updateFormatter()
{
    marksValid = false;
    mainSource = nullptr;
    int best = std::numeric_limits<int>::max();
    for (SeriesImpl *s : sources)
        if (s->order < best) {
            best = s->order;
            mainSource = s;
        }
    tickBase = mainSource ? int(jsRound(mainSource->minMoveBase())) : 100;
    formatter = NumberFormatter::price(100, 1);
    if (isPercentage()) {
        formatter = NumberFormatter::percent();
        tickBase = 100;
    } else if (isIndexed()) {
        formatter = NumberFormatter::price(100, 1);
        tickBase = 100;
    } else if (mainSource) {
        formatter = mainSource->formatter;
    }
}

void PriceScaleModel::autoscaleFor(std::optional<std::pair<int, int>> visible)
{
    visibleRange = visible;
    rangeValid = false;
}

// PriceScale._recalculatePriceRangeImpl
void PriceScaleModel::recalculate()
{
    if (!visibleRange)
        return;
    std::optional<PriceRange> merged;
    double above = 0, below = 0;
    for (SeriesImpl *s : sources) {
        if (!s->opts.visible)
            continue;
        const auto fv = s->firstValue();
        if (!fv)
            continue;
        if (!s->opts.autoscale)
            continue;
        auto r = s->autoscaleRange(visibleRange->first, visibleRange->second);
        if (!r)
            continue;
        switch (opts.mode) {
        case PriceScaleMode::Logarithmic:
            *r = rangeToLog(*r, logFormula);
            break;
        case PriceScaleMode::Percentage:
            *r = {toPercent(r->min, fv->first), toPercent(r->max, fv->first)};
            break;
        case PriceScaleMode::IndexedTo100:
            *r = {toIndexed(r->min, fv->first), toIndexed(r->max, fv->first)};
            break;
        default:
            break;
        }
        merged = merged ? merged->merged(*r) : *r;
        double a = 0, b = 0;
        s->markerMargins(a, b);
        above = std::max(above, a);
        below = std::max(below, b);
    }
    if (above != marginAbove || below != marginBelow) {
        marginAbove = above;
        marginBelow = below;
        marksValid = false;
    }
    if (merged) {
        PriceRange r = *merged;
        if (r.min == r.max) {
            const double minMove = (mainSource && !isPercentage() && !isIndexed())
                                       ? 1 / mainSource->minMoveBase()
                                       : 1;
            const double d = 5 * minMove;
            if (isLog())
                r = rangeFromLog(r, logFormula);
            r = {r.min - d, r.max + d};
            if (isLog())
                r = rangeToLog(r, logFormula);
        }
        if (isLog()) {
            const PriceRange raw = rangeFromLog(r, logFormula);
            const LogFormula f = logFormulaFor(raw);
            if (!(f == logFormula)) {
                std::optional<PriceRange> start;
                if (dragStartRange)
                    start = rangeFromLog(*dragStartRange, logFormula);
                logFormula = f;
                r = rangeToLog(raw, f);
                if (start)
                    dragStartRange = rangeToLog(*start, f);
            }
        }
        setRange(r);
    } else if (!range) {
        setRange(PriceRange{-0.5, 0.5});
        logFormula = logFormulaFor(std::nullopt);
    }
}

void PriceScaleModel::setMode(PriceScaleMode mode)
{
    const PriceScaleMode old = opts.mode;
    if (old == mode)
        return;
    opts.mode = mode;
    if (mode == PriceScaleMode::Percentage || mode == PriceScaleMode::IndexedTo100)
        opts.autoScale = true;
    rangeValid = false;
    if (old == PriceScaleMode::Logarithmic && range) {
        const PriceRange r = rangeFromLog(*range, logFormula);
        if (std::isfinite(r.min) && std::isfinite(r.max))
            setRange(r);
        else
            opts.autoScale = true;
    }
    if (mode == PriceScaleMode::Logarithmic && range)
        setRange(rangeToLog(*range, logFormula));
    updateFormatter();
    marksValid = false;
}

void PriceScaleModel::applyOptions(const PriceScaleOptions &o)
{
    // JS merges the options first, so a mode change through applyOptions does not convert the
    // range; it only asks for a fresh autoscale.
    const bool modeChanged = o.mode != opts.mode;
    const bool marginsChanged =
        o.scaleMarginTop != opts.scaleMarginTop || o.scaleMarginBottom != opts.scaleMarginBottom;
    opts = o;
    if (modeChanged) {
        if (o.mode == PriceScaleMode::Percentage || o.mode == PriceScaleMode::IndexedTo100)
            opts.autoScale = true;
        rangeValid = false;
    }
    if (o.autoScale)
        rangeValid = false;
    updateFormatter();
    if (marginsChanged)
        marksValid = false;
}

double PriceScaleModel::markHeight() const
{
    const int fs = chart ? chart->opts.fontSize : 12;
    return std::ceil(fs * 2.5);   // tickMarkDensity
}

// PriceTickSpanCalculator
namespace {
struct SpanCalculator {
    double base;
    std::vector<double> integral;
    std::vector<double> fractional;
    SpanCalculator(double b, std::vector<double> dividers) : base(b), integral(std::move(dividers))
    {
        auto isDecimal = [](double t) {
            if (t < 0)
                return false;
            if (t > 1e18)
                return true;
            for (double i = t; i > 1; i /= 10)
                if (std::fmod(i, 10) != 0)
                    return false;
            return true;
        };
        if (isDecimal(base)) {
            fractional = {2, 2.5, 2};
        } else {
            for (double t = base; t != 1;) {
                if (std::fmod(t, 2) == 0) {
                    fractional.push_back(2);
                    t /= 2;
                } else if (std::fmod(t, 5) == 0) {
                    fractional.push_back(2);
                    fractional.push_back(2.5);
                    t /= 5;
                } else {
                    fractional = {2, 2.5, 2};   // JS throws "unexpected base"
                    break;
                }
                if (fractional.size() > 100)
                    break;
            }
        }
    }
    static bool ge(double a, double b) { return b - a <= 1e-14; }
    double tickSpan(double high, double low, double maxTickSpan) const
    {
        const double minMovement = base == 0 ? 0 : 1 / base;
        double e = std::pow(10, std::max(0.0, std::ceil(std::log10(high - low))));
        size_t r = 0;
        double h = integral[0];
        for (int guard = 0; guard < 400; ++guard) {
            const bool t = ge(e, minMovement) && e > minMovement + 1e-14;
            const bool i = ge(e, maxTickSpan * h);
            const bool a = ge(e, 1);
            if (!(t && i && a))
                break;
            e /= h;
            h = integral[++r % integral.size()];
        }
        if (e <= minMovement + 1e-14)
            e = minMovement;
        e = std::max(1.0, e);
        if (!fractional.empty() && std::abs(e - 1) < 1e-14) {
            r = 0;
            h = fractional[0];
            for (int guard = 0; guard < 400 && ge(e, maxTickSpan * h) && e > minMovement + 1e-14;
                 ++guard) {
                e /= h;
                h = fractional[++r % fractional.size()];
            }
        }
        return e;
    }
};
} // namespace

double PriceScaleModel::tickSpan(double high, double low)
{
    const double maxTickSpan = (high - low) * markHeight() / height;
    const SpanCalculator a(tickBase, {2, 2.5, 2}), b(tickBase, {2, 2, 2.5}), c(tickBase, {2.5, 2, 2});
    return std::min({a.tickSpan(high, low, maxTickSpan), b.tickSpan(high, low, maxTickSpan),
                     c.tickSpan(high, low, maxTickSpan)});
}

// PriceScale.marks(): the cached marks stay until something resets them (a new price range once it
// is read, a new height, formatter or margins), so they can be a frame behind the time scale
const QVector<PriceMark> &PriceScaleModel::tickMarks()
{
    const bool noValue = !firstValue();
    if (marksValid && (noValue || marksNoValue == noValue))
        return marks;
    ensureRange();
    rebuildMarks();
    marksValid = true;
    marksNoValue = noValue;
    if (chart)
        chart->marksRebuilt(this);
    return marks;
}

// PriceTickMarkBuilder.rebuildTickMarks
void PriceScaleModel::rebuildMarks()
{
    marks.clear();
    const auto fv = firstValue();
    if (!fv || height <= 0 || isEmpty())
        return;
    const double base = *fv;
    const double bottom = coordinateToLogical(height - 1, base);
    const double top = coordinateToLogical(0, base);
    const double minCoord = 0, maxCoord = height - 1;
    const double high = std::max(bottom, top), low = std::min(bottom, top);
    if (high == low || !std::isfinite(high) || !std::isfinite(low))
        return;
    double span = tickSpan(high, low);
    if (!(span > 0) || !std::isfinite(span))
        return;
    double mod = std::fmod(high, span);
    mod += mod < 0 ? span : 0;
    const double sign = high >= low ? 1 : -1;
    std::optional<double> prev;
    const double mh = markHeight();
    int guard = 0;
    for (double logical = high - mod; logical > low && guard < 2000; logical -= span, ++guard) {
        const double coord = logicalToCoordinate(logical, base);
        if (prev && std::abs(coord - *prev) < mh)
            continue;
        if (coord < minCoord || coord > maxCoord)
            continue;
        marks.push_back({coord, formatLogical(logical), logical});
        prev = coord;
        if (isLog()) {
            span = tickSpan(logical * sign, low);
            if (!(span > 0) || !std::isfinite(span))
                break;
        }
    }
}

void PriceScaleModel::startScale(double y)
{
    if (isPercentage() || isIndexed())
        return;
    if (scaleStartPoint || dragStartRange || isEmpty())
        return;
    scaleStartPoint = height - y;
    dragStartRange = *range;
}

void PriceScaleModel::scaleTo(double y)
{
    if (isPercentage() || isIndexed() || !scaleStartPoint)
        return;
    opts.autoScale = false;
    y = height - y;
    if (y < 0)
        y = 0;
    double coeff = (*scaleStartPoint + 0.2 * (height - 1)) / (y + 0.2 * (height - 1));
    PriceRange r = *dragStartRange;
    coeff = std::max(coeff, 0.1);
    r.scaleAroundCenter(coeff);
    setRange(r);
    marksValid = false;
}

void PriceScaleModel::endScale()
{
    if (isPercentage() || isIndexed())
        return;
    scaleStartPoint.reset();
    dragStartRange.reset();
}

void PriceScaleModel::startScroll(double y)
{
    if (opts.autoScale)
        return;
    if (scrollStartPoint || dragStartRange || isEmpty())
        return;
    scrollStartPoint = y;
    dragStartRange = *range;
}

void PriceScaleModel::scrollTo(double y)
{
    if (opts.autoScale || !scrollStartPoint)
        return;
    const double perPx = range->length() / (internalHeight() - 1);
    const double delta = (y - *scrollStartPoint) * perPx;
    PriceRange r = *dragStartRange;
    r.shift(delta);
    setRange(r);
    marksValid = false;
}

void PriceScaleModel::endScroll()
{
    if (opts.autoScale)
        return;
    if (scrollStartPoint) {
        scrollStartPoint.reset();
        dragStartRange.reset();
    }
}

// ================================================================ series

bool SeriesImpl::isOverlay() const
{
    return scale && scale->id != QLatin1String("right") && scale->id != QLatin1String("left");
}

int SeriesImpl::rowAtOrAfter(int index) const
{
    auto it = std::lower_bound(rows.begin(), rows.end(), index,
                               [](const Row &r, int i) { return r.index < i; });
    return it == rows.end() ? -1 : int(it - rows.begin());
}

int SeriesImpl::rowAtOrBefore(int index) const
{
    auto it = std::upper_bound(rows.begin(), rows.end(), index,
                               [](int i, const Row &r) { return i < r.index; });
    return it == rows.begin() ? -1 : int(it - rows.begin()) - 1;
}

const Row *SeriesImpl::rowAt(int index) const
{
    const int i = rowAtOrAfter(index);
    return i >= 0 && rows[i].index == index ? &rows[i] : nullptr;
}

std::optional<std::pair<double, int>> SeriesImpl::firstValue() const
{
    int from, to;
    if (!chart->ts.strictRange(from, to))
        return std::nullopt;
    const int i = rowAtOrAfter(from);
    if (i < 0)
        return std::nullopt;
    return std::make_pair(rows[i].c, rows[i].index);
}

double SeriesImpl::minMoveBase() const
{
    return 1 / opts.priceFormat.minMove;
}

QColor SeriesImpl::barColor(const Row &r) const
{
    switch (stype) {
    case SeriesType::Bar:
    case SeriesType::Candlestick:
        return r.color.isValid() ? r.color : (r.o <= r.c ? opts.upColor : opts.downColor);
    case SeriesType::Area:
        return opts.lineColor;
    case SeriesType::Baseline:
        return r.c >= opts.baseValue ? opts.topLineColor : opts.bottomLineColor;
    default:
        return r.color.isValid() ? r.color : opts.color;
    }
}

std::optional<PriceRange> SeriesImpl::autoscaleRange(int from, int to) const
{
    if (rows.isEmpty())
        return std::nullopt;
    const int a = rowAtOrAfter(std::max(from, rows.first().index));
    if (a < 0)
        return std::nullopt;
    const int last = std::min(to, rows.last().index);
    double mn = std::numeric_limits<double>::infinity(), mx = -mn;
    bool any = false;
    const bool single = stype == SeriesType::Line || stype == SeriesType::Area
                        || stype == SeriesType::Baseline || stype == SeriesType::Histogram;
    for (int i = a; i < rows.size() && rows[i].index <= last; ++i) {
        const Row &r = rows[i];
        if (single) {
            if (std::isnan(r.c))
                continue;
            mn = std::min(mn, r.c);
            mx = std::max(mx, r.c);
            any = true;
        } else {
            for (double v : {r.h, r.l}) {
                if (std::isnan(v))
                    continue;
                mn = std::min(mn, v);
                mx = std::max(mx, v);
                any = true;
            }
        }
    }
    std::optional<PriceRange> res;
    if (any)
        res = PriceRange{mn, mx};
    if (stype == SeriesType::Histogram) {
        const PriceRange b{0, 0};   // base
        res = res ? res->merged(b) : b;
    }
    return res;
}

void SeriesImpl::reindexRows()
{
    rows.clear();
    rows.reserve(items.size());
    const auto &pts = chart->ts.points;
    int p = 0;
    for (const DataItem &it : items) {
        while (p < pts.size() && pts[p].time < it.time)
            ++p;
        if (p >= pts.size())
            break;
        if (it.whitespace())
            continue;
        Row r;
        r.index = p;
        if (stype == SeriesType::Candlestick || stype == SeriesType::Bar) {
            r.o = it.open;
            r.h = it.high;
            r.l = it.low;
            r.c = it.close;
            r.border = it.borderColor;
            r.wick = it.wickColor;
        } else {
            const double v = std::isnan(it.value) ? it.close : it.value;
            r.o = r.h = r.l = r.c = v;
        }
        r.color = it.color;
        rows.push_back(r);
    }
    markersDirty = true;
}

// SeriesMarkersPrimitive: markers snap to the series' data.
void SeriesImpl::rebuildMarkerItems()
{
    markerItems.clear();
    markersDirty = false;
    if (rows.isEmpty() || markers.isEmpty())
        return;
    const auto &ts = chart->ts;
    const auto firstIdx = ts.timeToIndex(chart->ts.points[rows.first().index].time, true);
    if (!firstIdx)
        return;
    for (const SeriesMarker &m : markers) {
        const auto e = ts.timeToIndex(m.time, true);
        if (!e)
            continue;
        int ri = rowAt(*e) ? rowAtOrAfter(*e) : (*e < *firstIdx ? rowAtOrAfter(*e) : rowAtOrBefore(*e));
        if (ri < 0)
            continue;
        MarkerItem item;
        item.index = rows[ri].index;
        item.time = m.time;
        item.m = m;
        markerItems.push_back(item);
    }
}

static int oddCeil(double t)   // ceiledOdd
{
    const int i = int(std::ceil(t));
    return i % 2 == 0 ? i - 1 : i;
}
static int markerSize(double barSpacing)   // size of a marker for a bar spacing (even)
{
    const int i = oddCeil(std::min(std::max(barSpacing, 12.0), 30.0));
    return i % 2 != 0 ? i - 1 : i;
}
static double markerMargin(double barSpacing)
{
    return std::max(double(oddCeil(std::min(std::max(barSpacing, 12.0), 30.0) * 0.1)), 3.0);
}

void SeriesImpl::markerMargins(double &above, double &below) const
{
    above = below = 0;
    if (markers.isEmpty())
        return;
    const double bs = chart->ts.barSpacing;
    const double n = 1.5 * markerSize(bs) + 2 * markerMargin(bs);
    bool hasAbove = false, hasBelow = false, hasIn = false;
    for (const auto &m : markers) {
        hasAbove |= m.position == SeriesMarker::AboveBar;
        hasBelow |= m.position == SeriesMarker::BelowBar;
        hasIn |= m.position == SeriesMarker::InBar;
    }
    auto pick = [&](bool own) { return own ? n : hasIn ? std::ceil(n / 2) : 0.0; };
    above = pick(hasAbove);
    below = pick(hasBelow);
}

// SeriesMarkersPaneView._updateImpl / fillSizeAndY
void SeriesImpl::layoutMarkers()
{
    if (markersDirty)
        rebuildMarkerItems();
    for (auto &it : markerItems)
        it.size = 0;
    if (markerItems.isEmpty())
        return;
    const auto &ts = chart->ts;
    const double bs = ts.barSpacing;
    const double h = markerMargin(bs);
    const double fs = chart->opts.fontSize;
    double offsetAbove = h, offsetBelow = h;
    int lastIndex = std::numeric_limits<int>::min();
    const auto fv = firstValue();
    for (auto &it : markerItems) {
        if (it.index != lastIndex) {
            offsetAbove = h;
            offsetBelow = h;
            lastIndex = it.index;
        }
        it.x = ts.coordOf(it.index);
        it.hasText = !it.m.text.isEmpty();
        const Row *r = rowAt(it.index);
        if (!r || !fv)
            continue;
        const bool ohlc = stype == SeriesType::Candlestick || stype == SeriesType::Bar;
        double price = r->c;
        if (ohlc) {
            if (it.m.position == SeriesMarker::AboveBar)
                price = r->h;
            else if (it.m.position == SeriesMarker::BelowBar)
                price = r->l;
        }
        const double coord = scale->priceToCoordinate(price, fv->first);
        const double d = std::isfinite(it.m.size) ? std::max(it.m.size, 0.0) : 1;
        const double f = markerSize(bs) * d;
        const double p = f / 2;
        it.size = f;
        switch (it.m.position) {
        case SeriesMarker::InBar:
            it.y = coord;
            if (it.hasText)
                it.textY = it.y + p + h + 0.6 * fs;
            break;
        case SeriesMarker::AboveBar:
            it.y = coord - p - offsetAbove;
            if (it.hasText) {
                it.textY = it.y - p - 0.6 * fs;
                offsetAbove += 1.2 * fs;
            }
            offsetAbove += f + h;
            break;
        case SeriesMarker::BelowBar:
            it.y = coord + p + offsetBelow;
            if (it.hasText) {
                it.textY = it.y + p + h + 0.6 * fs;
                offsetBelow += 1.2 * fs;
            }
            offsetBelow += f + h;
            break;
        }
    }
}

// ================================================================ panes

PriceScaleModel *PaneImpl::scaleById(const QString &id) const
{
    if (id == QLatin1String("right"))
        return right.get();
    if (id == QLatin1String("left"))
        return left.get();
    for (const auto &o : overlays)
        if (o.first == id)
            return o.second.get();
    return nullptr;
}

PriceScaleModel *PaneImpl::defaultScale() const
{
    if (right->opts.visible && !right->sources.isEmpty())
        return right.get();
    if (left->opts.visible && !left->sources.isEmpty())
        return left.get();
    if (!series.isEmpty() && series.first()->scale)
        return series.first()->scale;
    return right.get();
}

QVector<PriceScaleModel *> PaneImpl::allScales() const
{
    QVector<PriceScaleModel *> v{right.get(), left.get()};
    for (const auto &o : overlays)
        v.push_back(o.second.get());
    return v;
}

// ================================================================ kinetic animation

void KineticAnimation::addPosition(double pos, double time)
{
    if (p0) {
        if (p0->time == time) {
            p0->pos = pos;
            return;
        }
        if (std::abs(p0->pos - pos) < minMove)
            return;
    }
    p3 = p2;
    p2 = p1;
    p1 = p0;
    p0 = P{pos, time};
}

void KineticAnimation::finish(double pos, double time)
{
    if (!p0 || !p1)
        return;
    if (time - p0->time > 50)
        return;
    auto speedOf = [this](const P &a, const P &b) {
        const double s = (a.pos - b.pos) / (a.time - b.time);
        return (s > 0 ? 1 : s < 0 ? -1 : 0) * std::min(std::abs(s), maxSpeed);
    };
    auto sgn = [](double v) { return v > 0 ? 1 : v < 0 ? -1 : 0; };
    double total = 0;
    const double s0 = speedOf(*p0, *p1);
    std::vector<double> speeds{s0}, durs{p0->time - p1->time};
    total += durs[0];
    if (p2) {
        const double s1 = speedOf(*p1, *p2);
        if (sgn(s1) == sgn(s0)) {
            speeds.push_back(s1);
            durs.push_back(p1->time - p2->time);
            total += durs.back();
            if (p3) {
                const double s2 = speedOf(*p2, *p3);
                if (sgn(s2) == sgn(s0)) {
                    speeds.push_back(s2);
                    durs.push_back(p2->time - p3->time);
                    total += durs.back();
                }
            }
        }
    }
    double sp = 0;
    for (size_t i = 0; i < speeds.size(); ++i)
        sp += durs[i] / total * speeds[i];
    if (std::abs(sp) < minSpeed)
        return;
    start = P{pos, time};
    speed = sp;
    const double n = std::log(dumping);
    duration = std::log((1 * n) / -std::abs(sp)) / n;
}

double KineticAnimation::positionAt(double time) const
{
    const double t = time - start->time;
    return start->pos + speed * (std::pow(dumping, t) - 1) / std::log(dumping);
}

bool KineticAnimation::finished(double time) const
{
    return !start || std::min(time - start->time, duration) == duration;
}

// ================================================================ chart model

ChartPrivate::ChartPrivate(Chart *chart) : q(chart)
{
    ts.chart = this;
    tsHandle.chart = this;
    overlayDefaults.scaleMarginTop = 0.2;
    overlayDefaults.scaleMarginBottom = 0.1;
    clock.start();
}

int ChartPrivate::paneIndexOf(const PaneImpl *p) const
{
    for (size_t i = 0; i < panes.size(); ++i)
        if (panes[i].get() == p)
            return int(i);
    return -1;
}

PaneImpl *ChartPrivate::createPane()
{
    auto p = std::make_unique<PaneImpl>();
    p->chart = this;
    p->stretch = 1;
    auto mk = [&](const QString &id, const PriceScaleOptions &o) {
        auto s = std::make_unique<PriceScaleModel>();
        s->chart = this;
        s->pane = p.get();
        s->id = id;
        s->opts = o;
        s->updateFormatter();
        return s;
    };
    PriceScaleOptions leftOpts = rightDefaults;
    leftOpts.visible = false;
    p->right = mk(QStringLiteral("right"), rightDefaults);
    p->left = mk(QStringLiteral("left"), leftOpts);
    panes.push_back(std::move(p));
    fullLayoutNeeded = true;
    return panes.back().get();
}

PaneImpl *ChartPrivate::paneAt(int index)
{
    index = std::max(0, std::min<int>(int(panes.size()), index));
    if (index < int(panes.size()))
        return panes[index].get();
    return createPane();
}

void ChartPrivate::renumber(PaneImpl *p)
{
    for (int i = 0; i < p->series.size(); ++i)
        p->series[i]->order = i;
}

PriceScaleModel *ChartPrivate::attachToScale(SeriesImpl *s, PaneImpl *pane, const QString &rawId)
{
    const QString id = rawId.isEmpty() ? QStringLiteral("right") : rawId;
    PriceScaleModel *scale = pane->scaleById(id);
    if (!scale) {
        auto sc = std::make_unique<PriceScaleModel>();
        sc->chart = this;
        sc->pane = pane;
        sc->id = id;
        sc->opts = overlayDefaults;
        scale = sc.get();
        pane->overlays.emplace_back(id, std::move(sc));
    }
    s->pane = pane;
    s->scale = scale;
    scale->sources.push_back(s);
    scale->updateFormatter();
    scale->invalidate();
    return scale;
}

void ChartPrivate::detachFromScale(SeriesImpl *s)
{
    PriceScaleModel *scale = s->scale;
    if (!scale)
        return;
    scale->sources.removeAll(s);
    PaneImpl *pane = s->pane;
    if (scale->sources.isEmpty()) {
        if (scale == pane->right.get() || scale == pane->left.get()) {
            scale->opts.autoScale = true;
            scale->range.reset();
        } else {
            for (auto it = pane->overlays.begin(); it != pane->overlays.end(); ++it)
                if (it->second.get() == scale) {
                    pane->overlays.erase(it);
                    break;
                }
            scale = nullptr;
        }
    }
    if (scale) {
        scale->updateFormatter();
        scale->invalidate();
    }
    s->scale = nullptr;
}

void ChartPrivate::removePaneObject(PaneImpl *p)
{
    if (ch.pane == p) {
        ch = CrosshairState();
    }
    for (size_t i = 0; i < panes.size(); ++i)
        if (panes[i].get() == p) {
            panes.erase(panes.begin() + long(i));
            break;
        }
    fullLayoutNeeded = true;
}

std::optional<int> ChartPrivate::computeBaseIndex() const
{
    std::optional<int> b;
    for (const auto &s : series)
        if (!s->rows.isEmpty())
            b = std::max(b.value_or(0), s->rows.last().index);
    return b;
}

static int findTime(const QVector<TimePoint> &pts, double t)
{
    auto it = std::lower_bound(pts.begin(), pts.end(), t,
                               [](const TimePoint &p, double v) { return p.time < v; });
    if (it == pts.end() || it->time != t)
        return -1;
    return int(it - pts.begin());
}

static void fillWeights(QVector<TimePoint> &pts, int from)
{
    if (pts.isEmpty())
        return;
    from = std::max(0, from);
    for (int i = std::max(1, from); i < pts.size(); ++i)
        pts[i].weight = timeWeight(pts[i].time, pts[i - 1].time);
    if (from == 0 && pts.size() > 1) {
        const double avg = std::ceil((pts.last().time - pts.first().time) / (pts.size() - 1));
        pts[0].weight = timeWeight(pts[0].time, pts[0].time - avg);
    } else if (pts.size() == 1) {
        pts[0].weight = 0;
    }
}

void ChartPrivate::applyBaseIndex(std::optional<int> newBase, bool pointsChanged,
                                  std::optional<double> oldFirst)
{
    // ChartModel.updateTimeScale: keep the view still unless the last bar was in view
    const int oldBase = ts.base();
    int from, to;
    if (ts.strictRange(from, to) && oldFirst && !ts.points.isEmpty()) {
        const bool containsOldBase = from <= oldBase && oldBase <= to;
        const bool prepended = *oldFirst > ts.points.first().time;
        const bool moved = newBase && *newBase > oldBase && !prepended;
        const bool shift = containsOldBase && pointsChanged;   // shiftVisibleRangeOnNewBar
        if (moved && !shift)
            ts.setRightOffset(ts.rightOffset - (*newBase - oldBase));
    }
    ts.setBaseIndex(newBase);
}

void ChartPrivate::setSeriesData(SeriesImpl *s, QVector<DataItem> data)
{
    // the JS chart requires ascending times; keep the last of equal times
    bool sorted = true;
    for (int i = 1; i < data.size() && sorted; ++i)
        sorted = data[i - 1].time < data[i].time;
    if (!sorted) {
        std::stable_sort(data.begin(), data.end(),
                         [](const DataItem &a, const DataItem &b) { return a.time < b.time; });
        QVector<DataItem> uniq;
        uniq.reserve(data.size());
        for (const auto &it : data) {
            if (!uniq.isEmpty() && uniq.last().time == it.time)
                uniq.last() = it;
            else
                uniq.push_back(it);
        }
        data = std::move(uniq);
    }
    const std::optional<double> oldFirst =
        ts.points.isEmpty() ? std::nullopt : std::optional<double>(ts.points.first().time);
    const int oldBaseRaw = ts.base();
    Q_UNUSED(oldBaseRaw);

    bool sameTimes = s->items.size() == data.size();
    for (int i = 0; sameTimes && i < data.size(); ++i)
        sameTimes = s->items[i].time == data[i].time;

    bool pointsChanged = false;
    int firstChanged = -1;
    if (!sameTimes) {
        auto &pts = ts.points;
        for (const auto &it : s->items) {
            const int i = findTime(pts, it.time);
            if (i >= 0)
                --pts[i].refs;
        }
        QVector<double> missing;
        for (const auto &it : data) {
            const int i = findTime(pts, it.time);
            if (i >= 0)
                ++pts[i].refs;
            else
                missing.push_back(it.time);
        }
        bool removed = false;
        for (const auto &p : pts)
            if (p.refs <= 0) {
                removed = true;
                break;
            }
        if (removed || !missing.isEmpty()) {
            QVector<TimePoint> merged;
            merged.reserve(pts.size() + missing.size());
            int a = 0, b = 0;
            while (a < pts.size() || b < missing.size()) {
                if (a < pts.size() && pts[a].refs <= 0) {
                    ++a;
                    continue;
                }
                if (b >= missing.size() || (a < pts.size() && pts[a].time < missing[b])) {
                    merged.push_back(pts[a++]);
                } else {
                    TimePoint tp;
                    tp.time = missing[b++];
                    tp.refs = 1;
                    merged.push_back(tp);
                }
            }
            const int n = std::min(pts.size(), merged.size());
            firstChanged = n;
            for (int i = 0; i < n; ++i)
                if (pts[i].time != merged[i].time) {
                    firstChanged = i;
                    break;
                }
            if (firstChanged == n && pts.size() == merged.size())
                firstChanged = -1;
            pts = std::move(merged);
            if (firstChanged >= 0) {
                pointsChanged = true;
                fillWeights(pts, firstChanged);
                ts.marksByWeightDirty = true;
            }
        }
    }
    s->items = std::move(data);
    if (pointsChanged) {
        for (auto &other : series)
            other->reindexRows();
    } else {
        s->reindexRows();
    }
    applyBaseIndex(computeBaseIndex(), pointsChanged, oldFirst);
    invalidateScales();
}

void ChartPrivate::updateSeriesItem(SeriesImpl *s, const DataItem &item)
{
    if (!s->items.isEmpty() && item.time < s->items.last().time) {
        qWarning("lw::Series::update: Cannot update oldest data, last time=%g, new time=%g",
                 s->items.last().time, item.time);
        return;
    }
    const std::optional<double> oldFirst =
        ts.points.isEmpty() ? std::nullopt : std::optional<double>(ts.points.first().time);
    auto &pts = ts.points;
    int pi = findTime(pts, item.time);
    bool pointsChanged = false;
    const bool sameAsLast = !s->items.isEmpty() && s->items.last().time == item.time;
    if (pi < 0) {
        auto it = std::lower_bound(pts.begin(), pts.end(), item.time,
                                   [](const TimePoint &p, double v) { return p.time < v; });
        pi = int(it - pts.begin());
        TimePoint tp;
        tp.time = item.time;
        tp.refs = 1;
        pts.insert(pi, tp);
        fillWeights(pts, pi);
        ts.marksByWeightDirty = true;
        pointsChanged = true;
    } else if (!sameAsLast) {
        ++pts[pi].refs;
    }
    if (sameAsLast)
        s->items.last() = item;
    else
        s->items.push_back(item);
    if (pointsChanged && pi < pts.size() - 1) {
        for (auto &other : series)
            other->reindexRows();
    } else {
        // update the last row in place (DataLayer._updateLastSeriesRow)
        Row r;
        r.index = pi;
        if (s->stype == SeriesType::Candlestick || s->stype == SeriesType::Bar) {
            r.o = item.open;
            r.h = item.high;
            r.l = item.low;
            r.c = item.close;
            r.border = item.borderColor;
            r.wick = item.wickColor;
        } else {
            const double v = std::isnan(item.value) ? item.close : item.value;
            r.o = r.h = r.l = r.c = v;
        }
        r.color = item.color;
        if (s->rows.isEmpty() || s->rows.last().index < pi) {
            if (!item.whitespace())
                s->rows.push_back(r);
        } else if (!item.whitespace()) {
            s->rows.last() = r;
        } else {
            s->rows.removeLast();
        }
        s->markersDirty = true;
    }
    applyBaseIndex(computeBaseIndex(), pointsChanged, oldFirst);
    invalidateScales();
}

void ChartPrivate::invalidateScales()
{
    for (auto &p : panes)
        for (PriceScaleModel *sc : p->allScales())
            sc->invalidate();
    for (auto &s : series)
        s->markersDirty = true;
    ts.marksCacheKey = -1;
}

void ChartPrivate::invalidateAll()
{
    invalidateScales();
    for (auto &p : panes)
        for (PriceScaleModel *sc : p->allScales())
            sc->marksValid = false;
    fullLayoutNeeded = true;
}

} // namespace lw::detail
