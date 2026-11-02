// Rendering. Everything is drawn in bitmap (device pixel) coordinates with the same rounding as
// Lightweight Charts' renderers, and text in media coordinates, so the output matches the canvas
// version pixel for pixel as far as fonts allow.

#include "ChartPrivate.h"

#include <QPainter>
#include <QPainterPath>

#include <algorithm>

namespace lw::detail {

namespace {

const QColor kBaseLineColor("#B2B5BE");

void fillRectI(QPainter &p, int x, int y, int w, int h, const QColor &c)
{
    if (w < 0) {
        x += w;
        w = -w;
    }
    if (h < 0) {
        y += h;
        h = -h;
    }
    if (w == 0 || h == 0 || c.alpha() == 0)
        return;
    p.fillRect(QRect(x, y, w, h), c);
}

QVector<qreal> dashFor(LineStyle style)
{
    switch (style) {
    case LineStyle::Dotted:
        return {1, 1};
    case LineStyle::Dashed:
        return {2, 2};
    case LineStyle::LargeDashed:
        return {6, 6};
    case LineStyle::SparseDotted:
        return {1, 4};
    default:
        return {};
    }
}

double dashTotal(LineStyle style, double width)
{
    double t = 0;
    for (qreal v : dashFor(style))
        t += v * width;
    return t;
}

QPen makePen(const QBrush &brush, double width, LineStyle style, double dashOffsetPx = 0)
{
    QPen pen(brush, width, Qt::SolidLine, Qt::FlatCap, Qt::BevelJoin);
    const auto dash = dashFor(style);
    if (!dash.isEmpty() && width > 0) {
        pen.setDashPattern(dash);
        pen.setDashOffset(dashOffsetPx / width);
    }
    return pen;
}

// drawHorizontalLine of fancy-canvas helpers: half-pixel offset for odd widths
void hLine(QPainter &p, int y, double x0, double x1, const QColor &color, int width, LineStyle style)
{
    if (width <= 0 || color.alpha() == 0)
        return;
    p.setPen(makePen(color, width, style));
    const double off = width % 2 ? 0.5 : 0;
    p.drawLine(QPointF(x0, y + off), QPointF(x1, y + off));
}

void vLine(QPainter &p, int x, double y0, double y1, const QColor &color, int width, LineStyle style)
{
    if (width <= 0 || color.alpha() == 0)
        return;
    p.setPen(makePen(color, width, style));
    const double off = width % 2 ? 0.5 : 0;
    p.drawLine(QPointF(x + off, y0), QPointF(x + off, y1));
}

QColor opaque(const QColor &c)
{
    QColor o = c;
    o.setAlpha(255);
    return o;
}

QColor contrastText(const QColor &c)
{
    const double lum = 0.199 * c.red() + 0.687 * c.green() + 0.114 * c.blue();
    return lum > 160 ? QColor(Qt::black) : QColor(Qt::white);
}

// Text on an axis label: the chart's own rule when it has one.
QColor labelText(const ChartOptions &o, const QColor &background)
{
    return o.labelTextColor ? o.labelTextColor(background) : contrastText(background);
}

void drawTextMedia(QPainter &p, double hpr, double vpr, TextMetrics &tm, const QColor &color,
                   const QPointF &baseline, const QString &text)
{
    p.save();
    p.scale(hpr, vpr);
    tm.draw(p, baseline, text, color);
    p.restore();
}

// Rounded rectangle fill with per-corner radii [tl, tr, br, bl] (canvas roundRect).
void fillRoundRect(QPainter &p, double x, double y, double w, double h, const QColor &color,
                   const std::array<double, 4> &r)
{
    if (color.alpha() == 0 || w == 0 || h == 0)
        return;
    QPainterPath path;
    path.moveTo(x + r[0], y);
    path.lineTo(x + w - r[1], y);
    if (r[1] > 0)
        path.arcTo(QRectF(x + w - 2 * r[1], y, 2 * r[1], 2 * r[1]), 90, -90);
    path.lineTo(x + w, y + h - r[2]);
    if (r[2] > 0)
        path.arcTo(QRectF(x + w - 2 * r[2], y + h - 2 * r[2], 2 * r[2], 2 * r[2]), 0, -90);
    path.lineTo(x + r[3], y + h);
    if (r[3] > 0)
        path.arcTo(QRectF(x, y + h - 2 * r[3], 2 * r[3], 2 * r[3]), 270, -90);
    path.lineTo(x, y + r[0]);
    if (r[0] > 0)
        path.arcTo(QRectF(x, y, 2 * r[0], 2 * r[0]), 180, -90);
    path.closeSubpath();
    p.fillPath(path, color);
}

int oddCeil(double t)
{
    const int i = int(std::ceil(t));
    return i % 2 == 0 ? i - 1 : i;
}

int shapeSize(SeriesMarker::Shape shape, double size)
{
    const double k = shape == SeriesMarker::Circle ? 0.8 : shape == SeriesMarker::Square ? 0.7 : 1;
    return oddCeil(std::max(size, 12.0) * k);
}

// A path that behaves like a canvas path: lineTo on an empty path starts it.
struct CanvasPath {
    QPainterPath path;
    bool empty = true;
    void moveTo(double x, double y)
    {
        path.moveTo(x, y);
        empty = false;
    }
    void lineTo(double x, double y)
    {
        if (empty)
            moveTo(x, y);
        else
            path.lineTo(x, y);
    }
    void cubicTo(double x1, double y1, double x2, double y2, double x, double y)
    {
        if (empty)
            moveTo(x1, y1);
        path.cubicTo(x1, y1, x2, y2, x, y);
    }
    void reset()
    {
        path = QPainterPath();
        empty = true;
    }
};

struct LinePt {
    double x = 0, y = 0;
    QColor color;
};

// Control points of the curved line between points i and n (LWC getControlPoints)
std::pair<QPointF, QPointF> controlPoints(const QVector<LinePt> &t, int i, int n)
{
    const int s = std::max(0, i - 1), e = std::min<int>(t.size() - 1, n + 1);
    const QPointF a(t[i].x + (t[n].x - t[s].x) / 6, t[i].y + (t[n].y - t[s].y) / 6);
    const QPointF b(t[n].x - (t[e].x - t[i].x) / 6, t[n].y - (t[e].y - t[i].y) / 6);
    return {a, b};
}

// walkLine: the canvas line walker of Lightweight Charts (walkLine). finish() strokes or fills the
// current path from start to end point with the segment colour, then a new path begins.
template <typename Finish>
void walkLine(const QVector<LinePt> &pts, int from, int to, LineType type, double barSpacing,
              double hpr, double vpr, double dashLen, CanvasPath &path, double &dashOffset,
              Finish finish, int chunk = 0)
{
    if (pts.isEmpty() || from >= pts.size() || to <= 0 || from >= to)
        return;
    const LinePt *u = &pts[from];
    QColor c = u->color;
    LinePt d = *u;
    if (to - from < 2) {
        const double half = barSpacing / 2;
        path.reset();
        const LinePt a{u->x - half, u->y, c}, b{u->x + half, u->y, c};
        path.moveTo(a.x * hpr, a.y * vpr);
        path.lineTo(b.x * hpr, b.y * vpr);
        finish(c, a, b);
        return;
    }
    const bool dashed = dashLen > 0;
    double f = 0;
    auto newSegment = [&](const QColor &col, const LinePt &n) {
        finish(c, d, n);
        path.reset();
        c = col;
        d = n;
        if (dashed) {
            const double t = std::fmod(f, dashLen);
            dashOffset = t;
            f = t;
        }
    };
    path.reset();
    path.moveTo(u->x * hpr, u->y * vpr);
    LinePt v = d;
    int segmentPoints = 0;
    for (int h = from + 1; h < to; ++h) {
        v = pts[h];
        const double sx = v.x * hpr, sy = v.y * vpr;
        const QColor &uc = v.color;
        switch (type) {
        case LineType::Simple:
            path.lineTo(sx, sy);
            if (dashed)
                f += std::hypot(sx - pts[h - 1].x * hpr, sy - pts[h - 1].y * vpr);
            break;
        case LineType::WithSteps: {
            const LinePt &prev = pts[h - 1];
            const double ny = prev.y * vpr;
            path.lineTo(sx, ny);
            if (dashed)
                f += std::abs(v.x - prev.x) * hpr;
            if (uc != c) {
                newSegment(uc, v);
                path.lineTo(sx, ny);
            }
            path.lineTo(sx, sy);
            if (dashed)
                f += std::abs(v.y - prev.y) * vpr;
            break;
        }
        case LineType::Curved: {
            const auto [c1, c2] = controlPoints(pts, h - 1, h);
            path.cubicTo(c1.x() * hpr, c1.y() * vpr, c2.x() * hpr, c2.y() * vpr, sx, sy);
            if (dashed)
                f += std::hypot(sx - pts[h - 1].x * hpr, sy - pts[h - 1].y * vpr);
            break;
        }
        }
        if (type != LineType::WithSteps && uc != c) {
            newSegment(uc, v);
            path.moveTo(sx, sy);
            segmentPoints = 0;
        } else if (chunk > 0 && ++segmentPoints >= chunk) {
            // Qt's stroker gets slow on long self-crossing paths: stroke in pieces
            newSegment(c, v);
            path.moveTo(sx, sy);
            segmentPoints = 0;
        }
    }
    const bool same = d.x == v.x && d.y == v.y && d.color == v.color;
    if (!same || type == LineType::WithSteps)
        finish(c, d, v);
    if (dashed)
        dashOffset = 0;
}

// visible rows of a series: [from, to) positions, optionally extended by one on each side
void visibleRows(const SeriesImpl *s, int from, int to, bool extend, int &a, int &b)
{
    const auto &rows = s->rows;
    auto lo = std::lower_bound(rows.begin(), rows.end(), from,
                               [](const Row &r, int i) { return r.index < i; });
    auto hi = std::upper_bound(rows.begin(), rows.end(), to,
                               [](int i, const Row &r) { return i < r.index; });
    const int r = int(lo - rows.begin()), h = int(hi - rows.begin());
    a = r;
    b = h;
    if (!extend)
        return;
    if (r > 0 && r < rows.size() && rows[r].index >= from)
        a = r - 1;
    if (h > 0 && h < rows.size() && rows[h - 1].index <= to)
        b = h + 1;
}

// Solid rectangles in bitmap pixels, batched per colour (much cheaper than one fillRect each).
struct RectBatch {
    QPainter &p;
    QColor color;
    QVector<QRect> rects;
    explicit RectBatch(QPainter &painter) : p(painter) {}
    ~RectBatch() { flush(); }
    void add(int x, int y, int w, int h, const QColor &c)
    {
        if (w < 0) {
            x += w;
            w = -w;
        }
        if (h < 0) {
            y += h;
            h = -h;
        }
        if (w == 0 || h == 0 || c.alpha() == 0)
            return;
        if (c != color)
            flush();
        color = c;
        rects.push_back(QRect(x, y, w, h));
    }
    void flush()
    {
        if (rects.isEmpty())
            return;
        const bool aa = p.testRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::Antialiasing, false);
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawRects(rects);
        p.setBrush(Qt::NoBrush);
        p.setRenderHint(QPainter::Antialiasing, aa);
        rects.clear();
    }
};

struct PaneCtx {
    QPainter &p;
    double hpr, vpr;
    int bw, bh;           // bitmap size of the pane
    double mw, mh;        // media size
};

} // namespace

// ================================================================ series renderers

static void drawLineLike(PaneCtx &c, ChartPrivate *d, SeriesImpl *s, double base, int from, int to)
{
    Q_UNUSED(d);
    const auto &opts = s->opts;
    PriceScaleModel *sc = s->scale;
    int a, b;
    visibleRows(s, from, to, true, a, b);
    if (a >= b)
        return;
    // the points of the visible range (indexes relative to a)
    QVector<LinePt> pts;
    pts.reserve(b - a);
    for (int i = a; i < b; ++i) {
        const Row &r = s->rows[i];
        LinePt lp;
        lp.x = d->ts.coordOf(r.index);
        lp.y = sc->priceToCoordinate(r.c, base);
        if (s->stype == SeriesType::Line)
            lp.color = r.color.isValid() ? r.color : opts.color;
        else if (s->stype == SeriesType::Area)
            lp.color = opts.lineColor;
        else
            lp.color = QColor(1, 2, 3);   // baseline: one gradient for all points
        pts.push_back(lp);
    }
    const double bs = d->ts.barSpacing;
    QPainter &p = c.p;
    CanvasPath path;
    double dashOffset = 0;
    const int n = pts.size();

    // fills
    if (s->stype == SeriesType::Area) {
        QLinearGradient g(0, 0, 0, c.bh);
        g.setColorAt(0, opts.topColor);
        g.setColorAt(1, opts.bottomColor);
        const double baseY = c.mh;
        walkLine(pts, 0, n, opts.lineType, bs, c.hpr, c.vpr, 0, path, dashOffset,
                 [&](const QColor &, const LinePt &st, const LinePt &en) {
                     path.lineTo(en.x * c.hpr, baseY * c.vpr);
                     path.lineTo(st.x * c.hpr, baseY * c.vpr);
                     path.path.closeSubpath();
                     p.fillPath(path.path, g);
                 });
    } else if (s->stype == SeriesType::Baseline) {
        const double baseY = sc->priceToCoordinate(opts.baseValue, base);
        const double split = baseY * c.vpr;
        walkLine(pts, 0, n, opts.lineType, bs, c.hpr, c.vpr, 0, path, dashOffset,
                 [&](const QColor &, const LinePt &st, const LinePt &en) {
                     path.lineTo(en.x * c.hpr, baseY * c.vpr);
                     path.lineTo(st.x * c.hpr, baseY * c.vpr);
                     path.path.closeSubpath();
                     // gradient 0..height with a hard stop at the base line
                     p.save();
                     p.setClipRect(QRectF(0, -1e6, c.bw, split + 1e6), Qt::IntersectClip);
                     QLinearGradient top(0, 0, 0, split);
                     top.setColorAt(0, opts.topFillColor1);
                     top.setColorAt(1, opts.topFillColor2);
                     p.fillPath(path.path, top);
                     p.restore();
                     p.save();
                     p.setClipRect(QRectF(0, split, c.bw, 1e6), Qt::IntersectClip);
                     QLinearGradient bottom(0, split, 0, c.bh);
                     bottom.setColorAt(0, opts.bottomFillColor1);
                     bottom.setColorAt(1, opts.bottomFillColor2);
                     p.fillPath(path.path, bottom);
                     p.restore();
                 });
    }

    // line
    const bool lineVisible = opts.lineVisible;
    const double lw = opts.lineWidth * c.vpr;
    if (lineVisible) {
        const double dl = dashTotal(opts.lineStyle, lw);
        double baseSplit = 0;
        if (s->stype == SeriesType::Baseline)
            baseSplit = sc->priceToCoordinate(opts.baseValue, base) * c.vpr;
        walkLine(pts, 0, n, opts.lineType, bs, c.hpr, c.vpr, dl, path, dashOffset,
                 [&](const QColor &col, const LinePt &, const LinePt &) {
                     p.setBrush(Qt::NoBrush);
                     if (s->stype == SeriesType::Baseline) {
                         p.save();
                         p.setClipRect(QRectF(-1e6, -1e6, 2e6, baseSplit + 1e6), Qt::IntersectClip);
                         p.strokePath(path.path, makePen(opts.topLineColor, lw, opts.lineStyle, dashOffset));
                         p.restore();
                         p.save();
                         p.setClipRect(QRectF(-1e6, baseSplit, 2e6, 1e6), Qt::IntersectClip);
                         p.strokePath(path.path, makePen(opts.bottomLineColor, lw, opts.lineStyle, dashOffset));
                         p.restore();
                     } else if (col.alpha() > 0) {
                         p.strokePath(path.path, makePen(col, lw, opts.lineStyle, dashOffset));
                     }
                 }, opts.lineType == LineType::Simple && opts.lineStyle == LineStyle::Solid ? 16 : 0);
    }

    // point markers
    if (opts.pointMarkersVisible) {
        const double radius = (std::isnan(opts.pointMarkersRadius) || opts.pointMarkersRadius == 0)
                                  ? opts.lineWidth / 2.0 + 2
                                  : opts.pointMarkersRadius;
        const double o = (std::max(1, int(std::floor(c.hpr))) % 2) / 2.0;
        const double rr = radius * c.vpr + o;
        p.setPen(Qt::NoPen);
        for (int i = n - 1; i >= 0; --i) {
            const LinePt &lp = pts[i];
            QColor col = lp.color;
            if (s->stype == SeriesType::Baseline)
                col = lp.y * c.vpr < (sc->priceToCoordinate(opts.baseValue, base) * c.vpr)
                          ? opts.topLineColor
                          : opts.bottomLineColor;
            if (col.alpha() == 0)
                continue;
            p.setBrush(col);
            p.drawEllipse(QPointF(jsRound(lp.x * c.hpr) + o, lp.y * c.vpr), rr, rr);
        }
        p.setBrush(Qt::NoBrush);
    }
}

static void drawHistogram(PaneCtx &c, ChartPrivate *d, SeriesImpl *s, double base, int from, int to)
{
    int a, b;
    visibleRows(s, from, to, true, a, b);
    if (a >= b)
        return;
    RectBatch rb(c.p);
    PriceScaleModel *sc = s->scale;
    const double hpr = c.hpr, vpr = c.vpr;
    const double bs = d->ts.barSpacing;
    const double baseCoord = sc->priceToCoordinate(0, base);
    // column geometry (HistogramSeriesRenderer._fillPrecalculatedCache)
    const int gap = std::ceil(bs * hpr) <= 1 ? 0 : std::max(1, int(std::floor(hpr)));
    const int width = int(jsRound(bs * hpr)) - gap;
    struct Col { int left, right, center; double exact; int index; };
    QVector<Col> cols(b - a);
    for (int i = a; i < b; ++i) {
        const double x = d->ts.coordOf(s->rows[i].index);
        const int e = int(jsRound(x * hpr));
        Col col;
        if (width % 2) {
            const int t = (width - 1) / 2;
            col.left = e - t;
            col.right = e + t;
        } else {
            const int t = width / 2;
            col.left = e - t;
            col.right = e + t - 1;
        }
        col.center = e;
        col.exact = x * hpr;
        col.index = s->rows[i].index;
        cols[i - a] = col;
    }
    for (int i = 1; i < cols.size(); ++i) {
        Col &n = cols[i];
        Col &prev = cols[i - 1];
        if (n.index == prev.index + 1 && n.left - prev.right != gap + 1) {
            if (prev.center > prev.exact)
                prev.right = n.left - gap - 1;
            else
                n.left = prev.right + gap + 1;
        }
    }
    int minWidth = int(std::ceil(bs * hpr));
    for (Col &col : cols) {
        if (col.right < col.left)
            col.right = col.left;
        minWidth = std::min(minWidth, col.right - col.left + 1);
    }
    if (gap > 0 && minWidth < 4)
        for (Col &col : cols)
            if (col.right - col.left + 1 > minWidth) {
                if (col.center > col.exact)
                    col.right -= 1;
                else
                    col.left += 1;
            }
    const int lineW = std::max(1, int(std::floor(vpr)));
    const int e = int(jsRound(baseCoord * vpr)) - lineW / 2;
    const int r = e + lineW;
    for (int i = a; i < b; ++i) {
        const Row &row = s->rows[i];
        const Col &col = cols[i - a];
        const int l = int(jsRound(sc->priceToCoordinate(row.c, base) * vpr));
        int top, bottom;
        if (l <= e) {
            top = l;
            bottom = r;
        } else {
            top = e;
            bottom = l - lineW / 2 + lineW;
        }
        const QColor color = row.color.isValid() ? row.color : s->opts.color;
        rb.add(col.left, top, col.right - col.left + 1, bottom - top, color);
    }
}

static void drawBars(PaneCtx &c, ChartPrivate *d, SeriesImpl *s, double base, int from, int to)
{
    int a, b;
    visibleRows(s, from, to, false, a, b);
    if (a >= b)
        return;
    RectBatch rb(c.p);
    PriceScaleModel *sc = s->scale;
    const double hpr = c.hpr, vpr = c.vpr;
    const double bs = d->ts.barSpacing;
    int barWidth = std::max(int(std::floor(hpr)), int(std::floor(0.3 * bs * hpr)));
    if (barWidth >= 2 && std::max(1, int(std::floor(hpr))) % 2 != barWidth % 2)
        --barWidth;
    const int lineW = s->opts.thinBars ? std::min(barWidth, int(std::floor(hpr))) : barWidth;
    const bool ticks = lineW <= barWidth && bs >= std::floor(1.5 * hpr);
    for (int i = a; i < b; ++i) {
        const Row &r = s->rows[i];
        const QColor color = s->barColor(r);
        const int half = int(std::floor(0.5 * lineW));
        const int l = int(jsRound(d->ts.coordOf(r.index) * hpr));
        const int o = l - half;
        const int u = o + lineW - 1;
        const double hy = sc->priceToCoordinate(r.h, base), ly = sc->priceToCoordinate(r.l, base);
        const double cmin = std::min(hy, ly), cmax = std::max(hy, ly);
        const int f = int(jsRound(cmin * vpr)) - half;
        const int pp = int(jsRound(cmax * vpr)) + half;
        const int v = std::max(pp - f, lineW);
        rb.add(o, f, lineW, v, color);
        const int m = int(std::ceil(1.5 * barWidth));
        if (ticks) {
            if (s->opts.openVisible) {
                const int x0 = l - m;
                int top = std::max(f, int(jsRound(sc->priceToCoordinate(r.o, base) * vpr)) - half);
                int bottom = top + lineW - 1;
                if (bottom > f + v - 1) {
                    bottom = f + v - 1;
                    top = bottom - lineW + 1;
                }
                rb.add(x0, top, o - x0, bottom - top + 1, color);
            }
            const int x1 = l + m;
            int top = std::max(f, int(jsRound(sc->priceToCoordinate(r.c, base) * vpr)) - half);
            int bottom = top + lineW - 1;
            if (bottom > f + v - 1) {
                bottom = f + v - 1;
                top = bottom - lineW + 1;
            }
            rb.add(u + 1, top, x1 - u, bottom - top + 1, color);
        }
    }
}

static void drawCandles(PaneCtx &c, ChartPrivate *d, SeriesImpl *s, double base, int from, int to)
{
    int a, b;
    visibleRows(s, from, to, false, a, b);
    if (a >= b)
        return;
    RectBatch rb(c.p);
    PriceScaleModel *sc = s->scale;
    const auto &opts = s->opts;
    const double hpr = c.hpr, vpr = c.vpr;
    const double bs = d->ts.barSpacing;
    // optimalCandlestickWidth
    int barWidth;
    if (bs >= 2.5 && bs <= 4) {
        barWidth = int(std::floor(3 * hpr));
    } else {
        const double coeff = 1 - 0.2 * std::atan(std::max(4.0, bs) - 4) / (0.5 * M_PI);
        const int res = int(std::floor(bs * coeff * hpr));
        const int scaled = int(std::floor(bs * hpr));
        barWidth = std::max(int(std::floor(hpr)), std::min(res, scaled));
    }
    if (barWidth >= 2 && int(std::floor(hpr)) % 2 != barWidth % 2)
        --barWidth;

    struct Geo { int x; double yo, yc, yh, yl; QColor body, border, wick; };
    QVector<Geo> geo(b - a);
    for (int i = a; i < b; ++i) {
        const Row &r = s->rows[i];
        const bool up = r.o <= r.c;
        Geo g;
        g.x = int(jsRound(d->ts.coordOf(r.index) * hpr));
        g.yo = sc->priceToCoordinate(r.o, base);
        g.yc = sc->priceToCoordinate(r.c, base);
        g.yh = sc->priceToCoordinate(r.h, base);
        g.yl = sc->priceToCoordinate(r.l, base);
        geo[i - a] = g;
        Geo &gg = geo[i - a];
        gg.body = r.color.isValid() ? r.color : (up ? opts.upColor : opts.downColor);
        gg.border = r.border.isValid() ? r.border : (up ? opts.borderUpColor : opts.borderDownColor);
        gg.wick = r.wick.isValid() ? r.wick : (up ? opts.wickUpColor : opts.wickDownColor);
    }

    auto borderWidthFn = [&]() {
        int bw = int(std::floor(1 * hpr));
        if (barWidth <= 2 * bw)
            bw = int(std::floor(0.5 * (barWidth - 1)));
        const int res = std::max(int(std::floor(hpr)), bw);
        return barWidth <= 2 * res ? std::max(int(std::floor(hpr)), int(std::floor(1 * hpr))) : res;
    };

    // wicks
    if (opts.wickVisible) {
        int ww = std::min(int(std::floor(hpr)), int(std::floor(bs * hpr)));
        ww = std::max(int(std::floor(hpr)), std::min(ww, barWidth));
        const int half = int(std::floor(0.5 * ww));
        std::optional<int> prevEdge;
        for (int i = a; i < b; ++i) {
            const Row &r = s->rows[i];
            const Geo &g = geo[i - a];
            const int top = int(jsRound(std::min(g.yo, g.yc) * vpr));
            const int bottom = int(jsRound(std::max(g.yo, g.yc) * vpr));
            const int high = int(jsRound(g.yh * vpr));
            const int low = int(jsRound(g.yl * vpr));
            int left = int(jsRound(hpr * d->ts.coordOf(r.index))) - half;
            const int right = left + ww - 1;
            if (prevEdge) {
                left = std::max(*prevEdge + 1, left);
                left = std::min(left, right);
            }
            const int w = right - left + 1;
            rb.add(left, high, w, top - high, g.wick);
            rb.add(left, bottom + 1, w, low - bottom, g.wick);
            prevEdge = right;
        }
    }
    // borders
    const int bw = borderWidthFn();
    if (opts.borderVisible) {
        std::optional<int> prevEdge;
        for (int i = a; i < b; ++i) {
            const Row &r = s->rows[i];
            const Geo &g = geo[i - a];
            int left = int(jsRound(d->ts.coordOf(r.index) * hpr)) - int(std::floor(0.5 * barWidth));
            const int right = left + barWidth - 1;
            const int top = int(jsRound(std::min(g.yo, g.yc) * vpr));
            const int bottom = int(jsRound(std::max(g.yo, g.yc) * vpr));
            if (prevEdge) {
                left = std::max(*prevEdge + 1, left);
                left = std::min(left, right);
            }
            if (bs * hpr > 2 * bw) {
                // fillRectInnerBorder
                const int w = right - left + 1, h = bottom - top + 1;
                rb.add(left + bw, top, w - 2 * bw, bw, g.border);
                rb.add(left + bw, top + h - bw, w - 2 * bw, bw, g.border);
                rb.add(left, top, bw, h, g.border);
                rb.add(left + w - bw, top, bw, h, g.border);
            } else {
                rb.add(left, top, right - left + 1, bottom - top + 1, g.border);
            }
            prevEdge = right;
        }
    }
    // bodies
    if (!opts.borderVisible || barWidth > 2 * bw) {
        for (int i = a; i < b; ++i) {
            const Row &r = s->rows[i];
            const Geo &g = geo[i - a];
            int top = int(jsRound(std::min(g.yo, g.yc) * vpr));
            int bottom = int(jsRound(std::max(g.yo, g.yc) * vpr));
            int left = int(jsRound(d->ts.coordOf(r.index) * hpr)) - int(std::floor(0.5 * barWidth));
            int right = left + barWidth - 1;
            if (opts.borderVisible) {
                left += bw;
                top += bw;
                right -= bw;
                bottom -= bw;
            }
            if (top > bottom)
                continue;
            rb.add(left, top, right - left + 1, bottom - top + 1, g.body);
        }
    }
}

static void drawMarkers(PaneCtx &c, ChartPrivate *d, SeriesImpl *s)
{
    if (s->markers.isEmpty())
        return;
    s->layoutMarkers();
    double lf, lt;
    if (!d->ts.logicalRange(lf, lt))
        return;
    if (s->rows.isEmpty())
        return;
    const int from = int(std::floor(lf)), to = int(std::ceil(lt));
    auto &items = s->markerItems;
    // visible markers, extended by one on each side
    auto lo = std::lower_bound(items.begin(), items.end(), from,
                               [](const MarkerItem &m, int i) { return m.index < i; });
    auto hi = std::upper_bound(items.begin(), items.end(), to,
                               [](int i, const MarkerItem &m) { return i < m.index; });
    int a = int(lo - items.begin()), b = int(hi - items.begin());
    if (a > 0 && a < items.size() && items[a].index >= from)
        --a;
    if (b > 0 && b < items.size() && items[b - 1].index <= to)
        ++b;
    QPainter &p = c.p;
    const double hpr = c.hpr, vpr = c.vpr;
    for (int i = a; i < b; ++i) {
        MarkerItem &it = items[i];
        if (it.size == 0 && !it.hasText)
            continue;
        const QColor color = it.m.color.isValid() ? it.m.color : QColor(Qt::black);
        if (it.hasText) {
            const double w = d->text.width(it.m.text);
            const double tx = it.x - w / 2;
            drawTextMedia(p, hpr, vpr, d->text, color,
                          QPointF(tx, it.textY + d->text.middleOffset), it.m.text);
        }
        if (it.size == 0)
            continue;
        const double cx = jsRound(it.x * hpr) + (std::max(1, int(std::floor(hpr))) % 2) / 2.0;
        const double cy = it.y * vpr;
        const double k = hpr;
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        switch (it.m.shape) {
        case SeriesMarker::ArrowUp:
        case SeriesMarker::ArrowDown: {
            const bool up = it.m.shape == SeriesMarker::ArrowUp;
            const double e = ((shapeSize(SeriesMarker::ArrowUp, it.size) - 1) / 2.0) * k;
            const double r = ((oddCeil(it.size / 2) - 1) / 2.0) * k;
            QPainterPath path;
            if (up) {
                path.moveTo(cx - e, cy);
                path.lineTo(cx, cy - e);
                path.lineTo(cx + e, cy);
                path.lineTo(cx + r, cy);
                path.lineTo(cx + r, cy + e);
                path.lineTo(cx - r, cy + e);
                path.lineTo(cx - r, cy);
            } else {
                path.moveTo(cx - e, cy);
                path.lineTo(cx, cy + e);
                path.lineTo(cx + e, cy);
                path.lineTo(cx + r, cy);
                path.lineTo(cx + r, cy - e);
                path.lineTo(cx - r, cy - e);
                path.lineTo(cx - r, cy);
            }
            path.closeSubpath();
            p.fillPath(path, color);
            break;
        }
        case SeriesMarker::Circle: {
            const double rr = (shapeSize(SeriesMarker::Circle, it.size) - 1) / 2.0 * k;
            p.drawEllipse(QPointF(cx, cy), rr, rr);
            break;
        }
        case SeriesMarker::Square: {
            const int sz = shapeSize(SeriesMarker::Square, it.size);
            const double e = ((sz - 1) * k) / 2;
            p.fillRect(QRectF(cx - e, cy - e, sz * k, sz * k), color);
            break;
        }
        }
        p.setBrush(Qt::NoBrush);
    }
}

static void drawPrimitives(QPainter &p, double hpr, double vpr, double mw, double mh,
                           SeriesImpl *s, Primitive::ZOrder z)
{
    for (Primitive *prim : s->primitives) {
        if (prim->zOrder() != z)
            continue;
        p.save();
        p.scale(hpr, vpr);
        p.setClipRect(QRectF(0, 0, mw, mh), Qt::IntersectClip);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::TextAntialiasing, true);
        prim->draw(p, QSizeF(mw, mh));
        p.restore();
    }
}

// ================================================================ price axis labels

double AxisLabel::heightOf(double fs) const
{
    return (axisVisible || paneVisible) ? fs + 2 * (2.5 / 12 * fs) : 0;
}

// PriceAxisViewRenderer.draw for one label part. alignRight: the label hugs the right edge of
// the target (pane labels); otherwise the left edge (labels on a right price axis).
static void drawAxisLabelPart(QPainter &p, TextMetrics &tm, double fs, const AxisLabel &l,
                              bool paneLabel, bool alignRight, double hpr, double vpr,
                              double mediaWidth, int bitmapWidth, const QColor &chartBg)
{
    const QString &text = paneLabel ? l.paneText : l.axisText;
    if (!(paneLabel ? l.paneVisible : l.axisVisible) || text.isEmpty())
        return;
    const double borderSize = 1, tickLength = 5;
    const double padTop = 2.5 / 12 * fs + l.extraPadding;
    const double padBottom = padTop;
    const double padInner = fs / 12 * tickLength, padOuter = padInner;
    const double o = paneLabel ? 0 : tickLength;
    const double border = paneLabel ? borderSize : 0;
    const double w = std::ceil(tm.width(text));
    const double g = fs + padTop + padBottom;
    const double M = borderSize + padInner + padOuter + w + o;
    const int b = std::max(1, int(std::floor(vpr)));
    int S = int(jsRound(g * vpr));
    if (S % 2 != b % 2)
        S += 1;
    const int x = border > 0 ? std::max(1, int(std::floor(border * hpr))) : 0;
    const int C = int(jsRound(M * hpr));
    const double P = l.coord();
    const int k = int(jsRound(P * vpr)) - int(std::floor(0.5 * vpr));
    const int T = int(std::floor(k + b / 2.0 - S / 2.0));
    const int R = T + S;
    const double I = alignRight ? mediaWidth - border : border;
    const int V = alignRight ? bitmapWidth - x : x;
    double textX;
    const double radius = 2 * hpr;
    if (alignRight) {
        const int E = V - C;
        textX = I - o - padInner - border;
        fillRoundRect(p, E, T, C, S, l.background, {radius, 0, 0, radius});
    } else {
        textX = I + o + padInner;
        fillRoundRect(p, V, T, C, S, l.background, {0, radius, radius, 0});
    }
    if (paneLabel)   // a strip of chart background between the label and the axis
        fillRectI(p, alignRight ? bitmapWidth - x : 0, T, x, R - T, chartBg);
    const double cy = (T / vpr + R / vpr) / 2;
    const double baseline = cy + tm.centerOffset(text);
    const double tw = tm.rawWidth(text);
    drawTextMedia(p, hpr, vpr, tm, l.textColor,
                  QPointF(alignRight ? textX - tw : textX, baseline), text);
}

// Series last-value labels and price line labels of one price scale (PriceAxisWidget._updateLabels)
QVector<AxisLabel> ChartPrivate::axisLabels(PaneImpl *pane, PriceScaleModel *scale)
{
    QVector<AxisLabel> out;
    QVector<SeriesImpl *> list;
    for (SeriesImpl *s : pane->series)
        if (s->scale == scale)
            list.push_back(s);
    if (scale == pane->right.get() && scale->opts.visible)
        for (SeriesImpl *s : pane->series)
            if (s->isOverlay())
                list.push_back(s);
    for (SeriesImpl *s : list) {
        PriceScaleModel *sc = s->scale;
        const auto fv = s->firstValue();
        // last value
        AxisLabel lv;
        lv.owner = s;
        do {
            if (!s->opts.visible || !fv || ts.isEmpty() || sc->isEmpty() || s->rows.isEmpty())
                break;
            int from, to;
            if (!ts.strictRange(from, to))
                break;
            const int ri = s->rowAtOrBefore(to);
            if (ri < 0)
                break;
            const Row &r = s->rows[ri];
            const QColor color = s->opts.priceLineColor.isValid() ? s->opts.priceLineColor
                                                                  : s->barColor(r);
            lv.coordinate = sc->priceToCoordinate(r.c, fv->first);
            if (s->opts.lastValueVisible) {
                lv.axisText = sc->formatPrice(r.c, fv->first);
                lv.axisVisible = !lv.axisText.isEmpty();
            }
            if (!s->opts.title.isEmpty()) {
                lv.paneText = s->opts.title;
                lv.paneVisible = !isOverlayScale(sc);
            }
            lv.background = opaque(color);
            lv.textColor = labelText(opts, lv.background);
        } while (false);
        out.push_back(lv);
        // price lines
        for (const auto &pl : s->priceLines) {
            AxisLabel l;
            l.owner = s;
            const auto &po = pl->opts;
            if (po.axisLabelVisible && s->opts.visible && fv && !ts.isEmpty() && !sc->isEmpty()) {
                l.coordinate = sc->priceToCoordinate(po.price, fv->first);
                if (!po.title.isEmpty()) {
                    l.paneText = po.title;
                    l.paneVisible = !isOverlayScale(sc);
                }
                l.axisText = sc->formatPrice(po.price, fv->first);
                l.axisVisible = true;
                l.background = opaque(pl->color());
                l.textColor = labelText(opts, l.background);
            }
            out.push_back(l);
        }
    }
    return out;
}

bool ChartPrivate::isOverlayScale(const PriceScaleModel *sc) const
{
    return sc && sc->id != QLatin1String("right") && sc->id != QLatin1String("left");
}

// PriceAxisWidget._fixLabelOverlap + the label spreading of Lightweight Charts
void ChartPrivate::layoutLabels(QVector<AxisLabel *> labels, double height, double centre)
{
    const double fs = opts.fontSize;
    QVector<AxisLabel *> top, bottom;
    for (AxisLabel *l : labels)
        (l->coordinate <= centre ? top : bottom).push_back(l);
    std::sort(top.begin(), top.end(),
              [](AxisLabel *a, AxisLabel *b) { return b->coordinate < a->coordinate; });
    if (!top.isEmpty() && !bottom.isEmpty())
        bottom.push_back(top.first());
    std::sort(bottom.begin(), bottom.end(),
              [](AxisLabel *a, AxisLabel *b) { return a->coordinate < b->coordinate; });
    for (AxisLabel *l : labels) {
        const double half = std::floor(l->heightOf(fs) / 2);
        const double s = l->coordinate;
        if (s > -half && s < half)
            l->shifted = half;
        if (s > height - half && s < height + half)
            l->shifted = height - half;
    }
    auto spread = [&](QVector<AxisLabel *> &t, int dir) {
        if (t.isEmpty())
            return;
        int e = 0;
        const double r0 = t[0]->heightOf(fs);
        double h = dir == 1 ? height / 2 - (t[0]->coord() - r0 / 2) : t[0]->coord() - r0 / 2 - height / 2;
        h = std::max(0.0, h);
        for (int r = 1; r < t.size(); ++r) {
            AxisLabel *a = t[r];
            AxisLabel *l = t[r - 1];
            const double o = l->heightOf(fs);
            const double cur = a->coord(), prev = l->coord();
            if (dir == 1 ? cur > prev - o : cur < prev + o) {
                const double s = prev - o * dir;
                a->shifted = s;
                const double edge = s - dir * o / 2;
                if ((dir == 1 ? edge < 0 : edge > height) && h > 0) {
                    const double over = dir == 1 ? -1 - edge : edge - height;
                    const double move = std::min(over, h);
                    for (int k = e; k < t.size(); ++k)
                        t[k]->shifted = t[k]->coord() + dir * move;
                    h -= move;
                }
            } else {
                e = r;
                h = dir == 1 ? prev - o - cur : cur - (prev + o);
            }
        }
    };
    spread(top, 1);
    spread(bottom, -1);
}

// ================================================================ pane

void ChartPrivate::drawPane(QPainter &p, PaneImpl *pane, int paneIndex, double dpr)
{
    Q_UNUSED(paneIndex);
    const int y0 = int(jsRound(pane->top * dpr));
    const int bh = int(jsRound((pane->top + pane->height) * dpr)) - y0;
    const int bw = int(jsRound(paneWidth * dpr));
    if (bw <= 0 || bh <= 0 || pane->height <= 0 || paneWidth <= 0)
        return;
    p.save();
    p.translate(0, y0);
    p.setClipRect(QRect(0, 0, bw, bh));
    PaneCtx c{p, bw / paneWidth, bh / pane->height, bw, bh, paneWidth, pane->height};
    const double hpr = c.hpr, vpr = c.vpr;
    p.fillRect(QRect(0, 0, bw, bh), opts.background);

    // bottom primitives
    for (SeriesImpl *s : pane->series)
        drawPrimitives(p, hpr, vpr, c.mw, c.mh, s, Primitive::ZOrder::Bottom);

    // grid
    {
        const int lw = std::max(1, int(std::floor(hpr)));
        if (opts.vertLinesVisible) {
            for (const TimeMark &m : timeMarks())
                vLine(p, int(jsRound(m.coord * hpr)), -lw, bh + lw, opts.vertLinesColor, lw,
                      LineStyle::Solid);
        }
        if (opts.horzLinesVisible) {
            PriceScaleModel *sc = pane->defaultScale();
            for (const PriceMark &m : sc->tickMarks())
                hLine(p, int(jsRound(m.coord * vpr)), -lw, bw + lw, opts.horzLinesColor, lw,
                      LineStyle::Solid);
        }
    }

    int from = 0, to = -1;
    const bool haveRange = ts.strictRange(from, to);
    for (SeriesImpl *s : pane->series) {
        if (!s->opts.visible || !s->scale)
            continue;
        PriceScaleModel *sc = s->scale;
        const auto fv = s->firstValue();
        if (haveRange && fv && !sc->isEmpty()) {
            const double base = fv->first;
            // base line in percentage / indexed modes
            if (!s->isOverlay() && (sc->isPercentage() || sc->isIndexed()))
                hLine(p, int(jsRound(sc->priceToCoordinate(base, base) * vpr)), 0, bw,
                      kBaseLineColor, int(std::floor(1 * hpr)), LineStyle::Solid);
            switch (s->stype) {
            case SeriesType::Line:
            case SeriesType::Area:
            case SeriesType::Baseline:
                drawLineLike(c, this, s, base, from, to);
                break;
            case SeriesType::Histogram:
                drawHistogram(c, this, s, base, from, to);
                break;
            case SeriesType::Bar:
                drawBars(c, this, s, base, from, to);
                break;
            case SeriesType::Candlestick:
                drawCandles(c, this, s, base, from, to);
                break;
            }
            // last price line (last bar)
            if (s->opts.priceLineVisible && !s->rows.isEmpty()) {
                const Row &r = s->rows.last();
                const QColor col = s->opts.priceLineColor.isValid() ? s->opts.priceLineColor
                                                                    : s->barColor(r);
                const int y = int(jsRound(sc->priceToCoordinate(r.c, base) * vpr));
                if (y >= 0 && y <= bh)
                    hLine(p, y, 0, bw, col, int(std::floor(s->opts.priceLineWidth * hpr)),
                          s->opts.priceLineStyle);
            }
            // price lines
            for (const auto &pl : s->priceLines) {
                if (!pl->opts.lineVisible)
                    continue;
                const int y = int(jsRound(sc->priceToCoordinate(pl->opts.price, base) * vpr));
                if (y >= 0 && y <= bh)
                    hLine(p, y, 0, bw, pl->color(), int(std::floor(pl->opts.lineWidth * hpr)),
                          pl->opts.lineStyle);
            }
            drawMarkers(c, this, s);
        }
        drawPrimitives(p, hpr, vpr, c.mw, c.mh, s, Primitive::ZOrder::Normal);
    }

    // crosshair
    if (ch.visible && !hideCrosshairForShot && opts.crosshairMode != CrosshairMode::Hidden) {
        const int lw = int(std::floor(1 * hpr));
        const int x = int(jsRound(ch.x * hpr));
        if (std::isfinite(ch.x) && x >= 0)
            vLine(p, x, 0, bh, opts.crosshairLineColor, lw, LineStyle::LargeDashed);
        if (ch.pane == pane && std::isfinite(ch.y)) {
            const int y = int(jsRound(ch.y * vpr));
            if (y >= 0)
                hLine(p, y, 0, bw, opts.crosshairLineColor, int(std::floor(1 * vpr)), LineStyle::LargeDashed);
        }
        // crosshair markers of line-like series
        int sf, st;
        if (ts.strictRange(sf, st) && ch.index >= sf && ch.index <= st) {
            for (SeriesImpl *s : pane->series) {
                if (!s->isLineLike() || !s->opts.crosshairMarkerVisible || !s->opts.visible)
                    continue;
                const Row *r = s->rowAt(ch.index);
                const auto fv = s->firstValue();
                if (!r || !fv)
                    continue;
                const double y = s->scale->priceToCoordinate(r->c, fv->first);
                const double o = (std::max(1, int(std::floor(hpr))) % 2) / 2.0;
                const double cx = jsRound(ts.coordOf(ch.index) * hpr) + o;
                const double cy = y * vpr;
                QColor fill = s->stype == SeriesType::Line
                                  ? (r->color.isValid() ? r->color : s->opts.color)
                                  : s->barColor(*r);
                p.setPen(Qt::NoPen);
                p.setBrush(opts.background);
                p.drawEllipse(QPointF(cx, cy), (4 + 2) * vpr + o, (4 + 2) * vpr + o);
                p.setBrush(fill);
                p.drawEllipse(QPointF(cx, cy), 4 * vpr + o, 4 * vpr + o);
                p.setBrush(Qt::NoBrush);
            }
        }
    }

    // top primitives
    for (SeriesImpl *s : pane->series)
        drawPrimitives(p, hpr, vpr, c.mw, c.mh, s, Primitive::ZOrder::Top);

    // pane labels (series titles, price line titles)
    for (AxisLabel &l : paneLabels)
        drawAxisLabelPart(p, text, opts.fontSize, l, true, true, hpr, vpr, paneWidth, bw,
                          opts.background);
    p.restore();
}

// ================================================================ price axis

void ChartPrivate::drawPriceAxis(QPainter &p, PaneImpl *pane, double dpr)
{
    PriceScaleModel *sc = pane->right.get();
    if (axisWidth <= 0 || !sc->opts.visible)
        return;
    const int x0 = int(jsRound(paneWidth * dpr));
    const int y0 = int(jsRound(pane->top * dpr));
    const int bh = int(jsRound((pane->top + pane->height) * dpr)) - y0;
    const int bw = int(jsRound((paneWidth + axisWidth) * dpr)) - x0;
    if (bw <= 0 || bh <= 0)
        return;
    const double hpr = bw / axisWidth, vpr = bh / pane->height;
    const double fs = opts.fontSize;
    p.save();
    p.translate(x0, y0);
    p.setClipRect(QRect(0, 0, bw, bh));
    p.fillRect(QRect(0, 0, bw, bh), opts.background);
    if (sc->opts.borderVisible)
        fillRectI(p, 0, 0, std::max(1, int(std::floor(1 * hpr))), bh, sc->opts.borderColor);
    // tick labels
    const auto &marks = sc->tickMarks();
    const double tx = jsRound(5 + fs / 12 * 5);
    for (int i = marks.size() - 1; i >= 0; --i) {
        const PriceMark &m = marks[i];
        drawTextMedia(p, hpr, vpr, text, opts.textColor,
                      QPointF(tx, m.coord + text.centerOffset(m.label)), m.label);
    }
    // series and price line labels
    for (AxisLabel &l : paneLabels)
        drawAxisLabelPart(p, text, fs, l, false, false, hpr, vpr, axisWidth, bw, opts.background);
    // crosshair label
    if (ch.visible && !hideCrosshairForShot && ch.pane == pane
        && opts.crosshairMode != CrosshairMode::Hidden && std::isfinite(ch.y)) {
        const auto fv = sc->firstValue();
        if (fv && !sc->isEmpty()) {
            double price = ch.price;
            if (pane->defaultScale() != sc)
                price = sc->coordinateToPrice(ch.y, *fv);
            AxisLabel l;
            l.axisVisible = true;
            l.axisText = sc->formatPrice(price, *fv);
            l.coordinate = ch.y;
            l.background = opts.crosshairLabelBackgroundColor;
            l.textColor = labelText(opts, opts.crosshairLabelBackgroundColor);
            l.extraPadding = 2.0 / 12 * fs;
            drawAxisLabelPart(p, text, fs, l, false, false, hpr, vpr, axisWidth, bw,
                              opts.background);
        }
    }
    p.restore();
}

// ================================================================ time axis

QVector<TimeMark> ChartPrivate::timeMarks()
{
    QVector<TimeMark> out;
    if (ts.isEmpty())
        return out;
    const double spacing = ts.barSpacing;
    const double maxLabelWidth = (5.0 * (opts.fontSize + 4) / 8) * 8;
    const int indexPerLabel = int(jsRound(maxLabelWidth / spacing));
    int from, to;
    ts.strictRange(from, to);
    const int first = std::max(from, from - indexPerLabel);
    const int last = std::max(to, to - indexPerLabel);
    const QVector<int> items = ts.tickIndexes(spacing, maxLabelWidth);
    const int earliest = 0 + indexPerLabel;
    const int latest = ts.points.size() - 1 - indexPerLabel;
    const bool fixed = !opts.handleScroll && !opts.handleScale;
    for (int idx : items) {
        if (!(first <= idx && idx <= last))
            continue;
        TimeMark m;
        m.coord = ts.coordOf(idx);
        m.weight = ts.points[idx].weight;
        const double time = ts.points[idx].time;
        QString label;
        if (opts.tickMarkFormatter)
            label = opts.tickMarkFormatter(time);
        if (label.isNull())
            label = defaultTickMarkLabel(time, tickMarkType(m.weight, opts.timeVisible,
                                                            opts.secondsVisible));
        m.label = label;
        if (spacing > maxLabelWidth / 2 && !fixed)
            m.needAlignCoordinate = false;
        else
            m.needAlignCoordinate = (fixed && idx <= earliest) || (fixed && idx >= latest);
        out.push_back(m);
    }
    return out;
}

QString ChartPrivate::formatTimeLabel(int index)
{
    if (index < 0 || index >= ts.points.size())
        return {};
    const double t = ts.points[index].time;
    if (opts.timeFormatter)
        return opts.timeFormatter(t);
    return defaultTimeLabel(t, opts.timeVisible, opts.secondsVisible);
}

void ChartPrivate::drawTimeAxis(QPainter &p, double dpr)
{
    const double topMedia = mediaH - timeAxisHeight;
    const int y0 = int(jsRound(topMedia * dpr));
    const int bh = int(jsRound(mediaH * dpr)) - y0;
    const int bw = int(jsRound(paneWidth * dpr));
    if (bh <= 0)
        return;
    const double hpr = paneWidth > 0 ? bw / paneWidth : dpr, vpr = bh / timeAxisHeight;
    const double fs = opts.fontSize;
    p.save();
    p.translate(0, y0);
    // the stub below the price axis
    const int ax = bw;
    const int aw = int(jsRound(mediaW * dpr)) - ax;
    if (aw > 0) {
        p.fillRect(QRect(ax, 0, aw, bh), opts.background);
        if (opts.timeScaleBorderVisible && axisWidth > 0)
            fillRectI(p, ax, 0, int(std::floor(1 * hpr)), int(std::floor(1 * vpr)),
                      opts.timeScaleBorderColor);
    }
    if (bw > 0) {
        p.setClipRect(QRect(0, 0, bw, bh));
        p.fillRect(QRect(0, 0, bw, bh), opts.background);
        if (opts.timeScaleBorderVisible)
            fillRectI(p, 0, 0, bw, std::max(1, int(std::floor(1 * vpr))), opts.timeScaleBorderColor);
        const QVector<TimeMark> marks = timeMarks();
        if (!marks.isEmpty()) {
            int maxWeight = marks[0].weight;
            for (const auto &m : marks)
                maxWeight = std::max(maxWeight, m.weight);
            if (maxWeight > 30 && maxWeight < 50)
                maxWeight = 30;
            const double y = 1 + 5 + 3 * fs / 12 + fs / 2;
            auto align = [&](TextMetrics &tm, double x, const QString &label) {
                const double s = tm.width(label);
                const double r = std::floor(x - s / 2) + 0.5;
                if (r < 0)
                    x += std::abs(0 - r);
                else if (r + s > paneWidth)
                    x -= std::abs(paneWidth - (r + s));
                return x;
            };
            for (int pass = 0; pass < 2; ++pass) {
                TextMetrics &tm = pass == 0 ? text : bold;
                for (const auto &m : marks) {
                    if ((pass == 0) != (m.weight < maxWeight))
                        continue;
                    const double x = m.needAlignCoordinate ? align(tm, m.coord, m.label) : m.coord;
                    const double w = tm.rawWidth(m.label);
                    drawTextMedia(p, hpr, vpr, tm, opts.textColor,
                                  QPointF(x - w / 2, y + tm.middleOffset), m.label);
                }
            }
        }
        // crosshair label
        if (ch.visible && !hideCrosshairForShot && opts.crosshairMode != CrosshairMode::Hidden
            && std::isfinite(ch.x) && ch.index >= 0 && ch.index < ts.points.size()) {
            const QString label = formatTimeLabel(ch.index);
            const double n = jsRound(text.width(label, true));
            if (n > 0) {
                const double s = 9 * fs / 12;
                const double e = n + 2 * s;
                const double r = e / 2;
                double l = ch.x;
                double o = std::floor(l - r) + 0.5;
                if (o < 0) {
                    l += std::abs(0 - o);
                    o = std::floor(l - r) + 0.5;
                } else if (o + e > paneWidth) {
                    l -= std::abs(paneWidth - (o + e));
                    o = std::floor(l - r) + 0.5;
                }
                const double end = o + e;
                const double u = std::ceil(1 + 5 + 3 * fs / 12 + fs + 3 * fs / 12);
                const double rx = jsRound(o * hpr), lx = jsRound(end * hpr);
                const double cy = jsRound(u * vpr), dd = jsRound(2 * hpr);
                QPainterPath path;
                path.moveTo(rx, 0);
                path.lineTo(rx, cy - dd);
                path.arcTo(QRectF(rx, cy - 2 * dd, 2 * dd, 2 * dd), 180, 90);
                path.lineTo(lx - dd, cy);
                path.arcTo(QRectF(lx - 2 * dd, cy - 2 * dd, 2 * dd, 2 * dd), 270, 90);
                path.lineTo(lx, 0);
                path.closeSubpath();
                p.fillPath(path, opts.crosshairLabelBackgroundColor);
                const double ty = 1 + 5 + 3 * fs / 12 + fs / 2;
                drawTextMedia(p, hpr, vpr, text, labelText(opts, opts.crosshairLabelBackgroundColor),
                              QPointF(o + s, ty + text.centerOffset(QStringLiteral("Apr0"))), label);
            }
        }
    }
    p.restore();
}

// ================================================================ whole chart

void ChartPrivate::render(QPainter &p, double dpr)
{
    ensureUpdated();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    const int W = int(std::ceil(q->width() * dpr)), H = int(std::ceil(q->height() * dpr));
    p.fillRect(QRect(0, 0, W, H), opts.background);
    if (mediaW <= 0 || mediaH <= 0)
        return;
    for (size_t i = 0; i < panes.size(); ++i) {
        PaneImpl *pane = panes[i].get();
        // labels shared by the pane (titles) and the axis (values), laid out once
        paneLabels = axisLabels(pane, pane->right.get());
        if (!paneLabels.isEmpty()) {
            // centre: the first label of the scale's first attached series (JS kl()[0])
            QVector<AxisLabel *> ptrs;
            double centre = pane->height / 2;
            SeriesImpl *first =
                pane->right->sources.isEmpty() ? nullptr : pane->right->sources.first();
            for (AxisLabel &l : paneLabels)
                if (first && l.owner == first) {
                    centre = l.coordinate;
                    break;
                }
            for (AxisLabel &l : paneLabels)
                if (l.axisVisible || l.paneVisible)
                    ptrs.push_back(&l);
            layoutLabels(ptrs, pane->height, centre);
        }
        drawPane(p, pane, int(i), dpr);
        drawPriceAxis(p, pane, dpr);
        if (i + 1 < panes.size()) {
            const double sepTop = pane->top + pane->height;
            const int y = int(jsRound(sepTop * dpr));
            fillRectI(p, 0, y, int(jsRound(mediaW * dpr)), int(jsRound(1 * dpr)), opts.separatorColor);
        }
    }
    paneLabels.clear();
    drawTimeAxis(p, dpr);
    // separator hover / drag handle
    const int sep = dragSeparator >= 0 ? dragSeparator : hoverSeparator;
    if (sep >= 0 && sep + 1 < int(panes.size()) && opts.enablePaneResize) {
        const double sepTop = panes[sep]->top + panes[sep]->height;
        p.fillRect(QRectF(0, (sepTop - 4) * dpr, mediaW * dpr, 9 * dpr), opts.separatorHoverColor);
    }
}

} // namespace lw::detail
