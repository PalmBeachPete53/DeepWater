#include "Drawings.h"

#include "ChartPane.h"
#include "Text.h"
#include "Theme.h"

#include "../core/JsCompat.h"

#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>

#include <cmath>

namespace qtm {

const QVector<DrawingTool> &drawingTools()
{
    static const QVector<DrawingTool> tools = {
        {"cursor", "Cursor", 0, R"(<path d="M5 3l14 7-6 2-2 6z"/>)"},
        {"trend", "Trend line", 2, R"(<path d="M4 19L20 5"/><circle cx="4" cy="19" r="1.6"/><circle cx="20" cy="5" r="1.6"/>)"},
        {"ray", "Ray", 2, R"(<path d="M4 19L21 4"/><circle cx="4" cy="19" r="1.6"/><circle cx="11" cy="13" r="1.6"/>)"},
        {"extended", "Extended line", 2, R"(<path d="M2 21L22 3"/><circle cx="8" cy="16" r="1.6"/><circle cx="16" cy="9" r="1.6"/>)"},
        {"hline", "Horizontal line", 1, R"(<path d="M3 12h18"/><circle cx="12" cy="12" r="1.6"/>)"},
        {"hray", "Horizontal ray", 1, R"(<path d="M8 12h13"/><circle cx="8" cy="12" r="1.6"/>)"},
        {"vline", "Vertical line", 1, R"(<path d="M12 3v18"/><circle cx="12" cy="12" r="1.6"/>)"},
        {"channel", "Parallel channel", 3, R"(<path d="M3 15L15 5M9 21l12-10"/>)"},
        {"rect", "Rectangle", 2, R"(<rect x="4" y="6" width="16" height="12" rx="1"/>)"},
        {"fib", "Fib retracement", 2, R"(<path d="M3 5h18M3 10h18M3 14h18M3 19h18" stroke-dasharray="2 2"/>)"},
        {"measure", "Price range / measure", 2, R"(<path d="M12 3v18M8 7l4-4 4 4M8 17l4 4 4-4"/>)"},
        {"long", "Long position", 2, R"(<rect x="4" y="5" width="16" height="7" fill="currentColor" fill-opacity=".25"/><rect x="4" y="12" width="16" height="7"/>)"},
        {"short", "Short position", 2, R"(<rect x="4" y="5" width="16" height="7"/><rect x="4" y="12" width="16" height="7" fill="currentColor" fill-opacity=".25"/>)"},
        {"text", "Text note", 1, R"(<path d="M5 6h14M12 6v13"/>)"},
    };
    return tools;
}

const DrawingTool *toolById(const QString &id)
{
    for (const auto &t : drawingTools())
        if (t.id == id) return &t;
    return nullptr;
}

const QVector<double> FIB_LEVELS = {0, 0.236, 0.382, 0.5, 0.618, 0.786, 1, 1.618};

namespace {
// The default colours are theme variables (#2962ff and #787b86 in the original theme).
const char *const LineColor = "--accent";
const char *const FibColor = "--muted";
const QString Up = "--up";
const QString Down = "--down";

// A CSS colour or a theme variable, with an alpha for the variables ("--up" at 0.18).
QColor col(const QString &css, double alpha = 1)
{
    return css.startsWith("--") ? theme::chartColor(css, alpha) : theme::parseColor(css);
}
} // namespace

// ---------------------------------------------------------------- JSON

QJsonObject Drawing::toJson() const
{
    QJsonObject o;
    o["id"] = id;
    o["type"] = type;
    QJsonArray pts;
    for (const auto &p : points)
        pts.append(QJsonObject{{"time", p.time}, {"price", p.price}});
    o["points"] = pts;
    if (text) o["text"] = *text;
    if (color) o["color"] = *color;
    if (rr) o["rr"] = *rr;
    return o;
}

Drawing Drawing::fromJson(const QJsonObject &o)
{
    Drawing d;
    d.id = o.value("id").toString();
    d.type = o.value("type").toString();
    for (const auto &v : o.value("points").toArray()) {
        const QJsonObject p = v.toObject();
        d.points.push_back({p.value("time").toDouble(), p.value("price").toDouble()});
    }
    if (o.value("text").isString()) d.text = o.value("text").toString();
    if (o.value("color").isString()) d.color = o.value("color").toString();
    if (o.value("rr").isDouble()) d.rr = o.value("rr").toDouble();
    return d;
}

QJsonArray drawingsToJson(const QVector<Drawing> &items)
{
    QJsonArray a;
    for (const auto &d : items)
        a.append(d.toJson());
    return a;
}

QVector<Drawing> drawingsFromJson(const QJsonArray &a)
{
    QVector<Drawing> out;
    for (const auto &v : a)
        out.push_back(Drawing::fromJson(v.toObject()));
    return out;
}

// ---------------------------------------------------------------- layer

DrawingLayer::DrawingLayer(ChartPane *pane) : m_pane(pane) {}

Drawing *DrawingLayer::selectedItem()
{
    if (selected.isEmpty()) return nullptr;
    for (auto &d : items)
        if (d.id == selected) return &d;
    return nullptr;
}

void DrawingLayer::setItems(const QVector<Drawing> &list)
{
    items = list;
    selected.clear();
    update();
}

std::optional<double> DrawingLayer::x(double t) const
{
    return m_pane->timeToX(t);
}

std::optional<double> DrawingLayer::y(double price) const
{
    if (!m_series) return std::nullopt;
    return m_series->priceToCoordinate(price);
}

void DrawingLayer::draw(QPainter &p, const QSizeF &size)
{
    p.save();
    QVector<const Drawing *> all;
    for (const auto &d : items) all.push_back(&d);
    if (temp) all.push_back(&*temp);
    // The canvas font is set once per frame; the text tool's 13px stays for the drawings after it.
    m_font = theme::font(11);
    for (const Drawing *d : all) {
        p.save();
        drawOne(p, size, *d, (!selected.isEmpty() && d->id == selected && d != &*temp) || (temp && d == &*temp));
        p.restore();
    }
    p.restore();
}

namespace {

// The bits of the canvas 2D state drawings.js uses.
struct Ctx {
    QPainter &p;
    QColor stroke, fill;
    double lineWidth = 1;
    QVector<qreal> dash;
    double alpha = 1;
    QFont font;

    void applyPen()
    {
        QPen pen(stroke);
        pen.setWidthF(lineWidth);
        pen.setCapStyle(Qt::FlatCap);
        pen.setJoinStyle(Qt::MiterJoin);
        if (!dash.isEmpty()) {
            QVector<qreal> d;
            for (qreal v : dash) d << v / lineWidth;
            pen.setDashPattern(d);
        }
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.setOpacity(alpha);
    }
    void line(double x1, double y1, double x2, double y2)
    {
        applyPen();
        p.drawLine(QPointF(x1, y1), QPointF(x2, y2));
    }
    void fillRect(double x, double y, double w, double h)
    {
        p.setOpacity(alpha);
        p.fillRect(QRectF(x, y, w, h).normalized(), fill);
    }
    void strokeRect(double x, double y, double w, double h)
    {
        applyPen();
        p.drawRect(QRectF(x, y, w, h).normalized());
    }
    double measure(const QString &t) const { return text::width(font, t); }
    // fillText with textBaseline = "middle".
    void fillText(const QString &t, double x, double y, const QString &align)
    {
        const double w = text::width(font, t);
        double left = x;
        if (align == "center") left = x - w / 2;
        else if (align == "right") left = x - w;
        p.setOpacity(alpha);
        text::draw(p, QPointF(left, y + (text::ascent(font) - text::descent(font)) / 2), font, t, fill);
    }
};

} // namespace

void DrawingLayer::drawOne(QPainter &painter, const QSizeF &size, const Drawing &d, bool active)
{
    struct Pt { double x, y, time, price; };
    QVector<Pt> pts;
    for (const auto &p : d.points) {
        const auto px = x(p.time);
        const auto py = y(p.price);
        if (!px || !py) return;
        pts.push_back({*px, *py, p.time, p.price});
    }
    if (pts.isEmpty()) return;
    const QString color = d.color.value_or(LineColor);
    Ctx ctx{painter, col(color), col(color)};
    ctx.font = m_font;
    ctx.lineWidth = active ? 2 : 1.5;
    const Pt a = pts[0];
    const Pt b = pts.size() > 1 ? pts[1] : pts[0];
    const bool hasC = pts.size() > 2;
    const Pt c = hasC ? pts[2] : pts[0];
    auto label = [&](const QString &txt, double lx, double ly, const QString &align = "left", const QString &bg = QString(),
                     double bgAlpha = 1) {
        if (!bg.isEmpty()) {
            const double w = ctx.measure(txt) + 10;
            ctx.fill = col(bg, bgAlpha);
            ctx.fillRect(align == "center" ? lx - w / 2 : align == "right" ? lx - w : lx, ly - 9, w, 18);
            ctx.fill = theme::textOn(col(bg));   // #fff in the original
            ctx.fillText(txt, align == "center" ? lx : align == "right" ? lx - 5 : lx + 5, ly, align);
            ctx.fill = col(color);
        } else {
            ctx.fillText(txt, lx, ly, align);
        }
    };
    auto extend = [](const Pt &p, const Pt &q, bool both) -> std::pair<QPointF, QPointF> {
        const double dx = q.x - p.x, dy = q.y - p.y;
        if (dx == 0 && dy == 0) return {QPointF(p.x, p.y), QPointF(q.x, q.y)};
        const double far = 1e5;
        const double len = std::hypot(dx, dy);
        const QPointF e2(p.x + (dx / len) * far, p.y + (dy / len) * far);
        const QPointF e1 = both ? QPointF(p.x - (dx / len) * far, p.y - (dy / len) * far) : QPointF(p.x, p.y);
        return {e1, e2};
    };
    auto price = [&](double v) { return m_pane->formatPrice(v); };

    const QString &t = d.type;
    if (t == "trend") {
        ctx.line(a.x, a.y, b.x, b.y);
    } else if (t == "ray" || t == "extended") {
        const auto [p, q] = extend(a, b, t == "extended");
        ctx.line(p.x(), p.y(), q.x(), q.y());
    } else if (t == "hline") {
        ctx.line(0, a.y, size.width(), a.y);
        label(price(a.price), size.width() - 4, a.y - 10, "right");
    } else if (t == "hray") {
        ctx.line(a.x, a.y, size.width(), a.y);
        label(price(a.price), size.width() - 4, a.y - 10, "right");
    } else if (t == "vline") {
        ctx.line(a.x, 0, a.x, size.height());
    } else if (t == "channel") {
        ctx.line(a.x, a.y, b.x, b.y);
        if (hasC) {
            const double dx = (b.x - a.x) != 0 ? (b.x - a.x) : 1;
            const double off = c.y - (a.y + ((b.y - a.y) * (c.x - a.x)) / dx);
            ctx.line(a.x, a.y + off, b.x, b.y + off);
            QPainterPath path;
            path.moveTo(a.x, a.y);
            path.lineTo(b.x, b.y);
            path.lineTo(b.x, b.y + off);
            path.lineTo(a.x, a.y + off);
            path.closeSubpath();
            painter.setOpacity(0.08);
            painter.fillPath(path, ctx.fill);
            painter.setOpacity(1);
        }
    } else if (t == "rect") {
        ctx.alpha = 0.12;
        ctx.fillRect(a.x, a.y, b.x - a.x, b.y - a.y);
        ctx.alpha = 1;
        ctx.strokeRect(a.x, a.y, b.x - a.x, b.y - a.y);
    } else if (t == "fib") {
        const double x1 = std::min(a.x, b.x), x2 = std::max(a.x, b.x);
        const QString fc = d.color.value_or(FibColor);
        ctx.stroke = col(fc);
        for (double lv : FIB_LEVELS) {
            const double pv = b.price + (a.price - b.price) * lv;
            const auto yy = y(pv);
            if (!yy) continue;
            ctx.alpha = lv == 0.5 || lv == 0.618 ? 1 : 0.7;
            ctx.line(x1, *yy, x2, *yy);
            ctx.alpha = 1;
            ctx.fill = col(fc);
            label(QString("%1 (%2)").arg(js::numberToString(lv), price(pv)), x1 + 2, *yy - 8);
        }
        ctx.dash = {3, 3};
        ctx.line(a.x, a.y, b.x, b.y);
        ctx.dash.clear();
    } else if (t == "measure") {
        const bool up = b.price >= a.price;
        const QString base = up ? Up : Down;
        ctx.fill = col(base, 0.15);
        ctx.fillRect(a.x, a.y, b.x - a.x, b.y - a.y);
        ctx.stroke = col(base);
        ctx.line((a.x + b.x) / 2, a.y, (a.x + b.x) / 2, b.y);
        const double bars = js::round(m_pane->logicalOf(b.time) - m_pane->logicalOf(a.time));
        const double diff = b.price - a.price, pct = (diff / a.price) * 100;
        ctx.fill = col(base);
        label(QString("%1%2 (%3%4%)  %5 bars")
                  .arg(diff >= 0 ? "+" : "", price(diff), pct >= 0 ? "+" : "", js::toFixed(pct, 2),
                       js::numberToString(bars)),
              (a.x + b.x) / 2, b.y + (up ? -14 : 14), "center", base, 0.9);
    } else if (t == "long" || t == "short") {
        // first point = entry, second point sets the stop (its price) and the width (its time); target = 2R
        const double dir = t == "long" ? 1 : -1;
        const double rr = d.rr && js::truthy(*d.rr) ? *d.rr : 2;
        const double risk = js::truthy(std::abs(a.price - b.price)) ? std::abs(a.price - b.price) : a.price * 0.01;
        const double stop = a.price - dir * risk, target = a.price + dir * risk * rr;
        const auto ys = y(stop), yt = y(target);
        const double ysv = ys.value_or(NaN), ytv = yt.value_or(NaN);
        const double x1 = std::min(a.x, b.x), w = std::max(std::abs(b.x - a.x), 40.0);
        if (yt) {
            ctx.fill = col(Up, 0.18);
            ctx.fillRect(x1, a.y, w, ytv - a.y);
        }
        if (ys) {
            ctx.fill = col(Down, 0.18);
            ctx.fillRect(x1, a.y, w, ysv - a.y);
        }
        ctx.stroke = col(FibColor);
        ctx.line(x1, a.y, x1 + w, a.y);
        ctx.fill = col(Up);
        label(QString("Target %1 (%2%)").arg(price(target), js::toFixed(((target - a.price) / a.price) * 100 * dir, 2)),
              x1 + w / 2, ytv + (dir > 0 ? 10 : -10), "center", Up, 0.9);
        label(QString("Stop %1 (%2%)").arg(price(stop), js::toFixed(((a.price - stop) / a.price) * 100 * dir, 2)),
              x1 + w / 2, ysv + (dir > 0 ? -10 : 10), "center", Down, 0.9);
        ctx.fill = col(FibColor);
        label(QString("Entry %1  R:R %2").arg(price(a.price), js::toFixed(rr, 1)), x1 + 4, a.y - 9);
    } else if (t == "text") {
        ctx.font = m_font = theme::font(13);
        label(d.text && !d.text->isEmpty() ? *d.text : QString("Note"), a.x, a.y, "left", color);
    }
    if (active) {
        QPen pen(col(color));
        pen.setWidthF(1.5);
        painter.setOpacity(1);
        painter.setPen(pen);
        painter.setBrush(theme::hasPalette() ? theme::chartColor("--bg") : QColor("#fff"));   // handles
        for (const auto &p : pts)
            painter.drawEllipse(QPointF(p.x, p.y), 4, 4);
    }
}

QString DrawingLayer::hitTest(double px, double py) const
{
    QString best;
    double bestD = 7;
    struct P { double x, y; };
    auto segDist = [&](P p, P q) {
        const double dx = q.x - p.x, dy = q.y - p.y, l2 = dx * dx + dy * dy;
        const double t = l2 ? std::max(0.0, std::min(1.0, ((px - p.x) * dx + (py - p.y) * dy) / l2)) : 0;
        return std::hypot(px - (p.x + t * dx), py - (p.y + t * dy));
    };
    for (const auto &d : items) {
        QVector<P> pts;
        bool missing = false;
        for (const auto &p : d.points) {
            const auto xx = x(p.time);
            const auto yy = y(p.price);
            if (!xx || !yy) { missing = true; break; }
            pts.push_back({*xx, *yy});
        }
        if (missing || pts.isEmpty()) continue;
        const P a = pts[0];
        const bool hasB = pts.size() > 1;
        const P b = hasB ? pts[1] : pts[0];
        double dist = INFINITY;
        const QString &t = d.type;
        if (t == "hline") dist = std::abs(py - a.y);
        else if (t == "hray") dist = px >= a.x - 4 ? std::abs(py - a.y) : INFINITY;
        else if (t == "vline") dist = std::abs(px - a.x);
        else if (t == "text")
            dist = px >= a.x - 4 && px <= a.x + 120 && std::abs(py - a.y) < 12 ? 0 : INFINITY;
        else if (t == "rect" || t == "fib" || t == "measure" || t == "long" || t == "short") {
            const bool inside = px >= std::min(a.x, b.x) - 4 && px <= std::max({a.x, b.x, a.x + 40}) + 4
                && py >= std::min(a.y, b.y) - 40 && py <= std::max(a.y, b.y) + 40;
            dist = inside ? 3 : INFINITY;
        } else if (hasB) {
            auto far = [](P p, P q, bool both) -> std::pair<P, P> {
                const double dx = q.x - p.x, dy = q.y - p.y;
                double len = std::hypot(dx, dy);
                if (!len) len = 1;
                return {both ? P{p.x - (dx / len) * 1e5, p.y - (dy / len) * 1e5} : p,
                        P{p.x + (dx / len) * 1e5, p.y + (dy / len) * 1e5}};
            };
            const auto [p, q] = t == "ray" ? far(a, b, false) : t == "extended" ? far(a, b, true) : std::make_pair(a, b);
            dist = segDist(p, q);
        }
        if (dist < bestD) {
            bestD = dist;
            best = d.id;
        }
    }
    return best;
}

} // namespace qtm
