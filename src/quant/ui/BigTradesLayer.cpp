// Big Trades on the chart (see BigTradesLayer.h). A Deepwater addition.
#include "BigTradesLayer.h"

#include "BarAxis.h"
#include "ChartPane.h"
#include "TradeFeed.h"
#include "Theme.h"

#include "../core/Series.h"

#include <QElapsedTimer>
#include <QFontMetricsF>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>
#include <QPainter>
#include <QPainterPath>
#include <QTimeZone>

#include <algorithm>
#include <cmath>

namespace qtm {

using namespace bigtrades;

// ---------------------------------------------------------------- model

BigTradesModel::Params BigTradesModel::Params::fromJson(const QJsonObject &o)
{
    Params p;
    p.thr.automatic = o.value("mode").toString() != "Manual";
    p.thr.minVolume = std::max(0.0, o.value("min").toDouble());
    p.thr.maxVolume = std::max(0.0, o.value("max").toDouble());
    if (const double pc = o.value("percentile").toDouble(); pc > 0) p.thr.percentile = std::clamp(pc, 50.0, 99.99);
    if (const double w = o.value("window").toDouble(); w > 0) p.thr.windowMinutes = std::clamp(w, 1.0, 1440.0);
    p.thr.volatility = o.value("volatility").toString() != "Off";
    if (const double w = o.value("aggWindow").toDouble(); w > 0) p.agg.windowMs = std::clamp(w, 1.0, 60000.0);
    p.agg.exactTimestamp = o.value("aggTime").toString() == "Same timestamp only";
    p.agg.priceMode = o.value("aggPrice").toString().startsWith("Sweep") ? PriceMode::Sweep : PriceMode::SamePrice;
    return p;
}

QJsonObject BigTradesModel::detectionParams(const QJsonObject &all)
{
    QJsonObject o;
    for (const char *k : {"mode", "min", "max", "percentile", "window", "volatility", "aggWindow", "aggTime", "aggPrice"})
        if (all.contains(k)) o.insert(k, all.value(k));
    return o;
}

BigTradesModel::BigTradesModel(const QJsonObject &params)
    : m_json(params), m_p(Params::fromJson(params)), m_det(m_p.agg, m_p.thr)
{
}

void BigTradesModel::push(const BigTrade &b)
{
    m_trades.push_back(b);
    m_approx = m_approx || b.a.approximate;
}

bool BigTradesModel::sync(const TradeFeed *feed, double now)
{
    const size_t before = m_trades.size();
    if (!feed || !feed->ready()) return false;
    const TradeFeed::Tape tape = feed->tape();
    // The history arrives newest first and the detection reads trades in time order: while it loads, everything is run
    // again every 2 s over what has come in (bubbles appear from the newest back), and once more when it's complete.
    const bool loading = feed->loadingHistory();
    const bool fresh = m_gen != feed->tapeGeneration() || !m_built;
    const bool grown = m_built && !m_complete && (!loading || m_sinceBuild.elapsed() > 2000);
    const bool running = m_rebuild && m_rebuild->isRunning();
    if ((fresh || grown) && !tape.empty() && !running) startRebuild(feed);
    if (!m_built || m_gen != feed->tapeGeneration()) return false;
    if (!tape.empty() && tape.back().id > m_lastId && tape.front().id <= m_lastId + 1) {   // live: the new trades only
        size_t i = tape.size();
        while (i > 0 && tape[i - 1].id > m_lastId) --i;
        for (; i < tape.size(); ++i)
            if (auto b = m_det.add(tape[i].t, {tape[i].t.side, false})) push(*b);
        m_lastId = tape.back().id;
    }
    if (auto b = m_det.flushIfDone(now)) push(*b);
    return m_trades.size() != before;
}

BigTradesModel::~BigTradesModel() = default;   // a rebuild still running finishes on its own copy, unread

void BigTradesModel::startRebuild(const TradeFeed *feed)
{
    // A copy of the tape (12 MB for 300,000 trades, a few ms): the feed keeps changing meanwhile.
    const TradeFeed::Tape view = feed->tape();
    std::vector<TradeFeed::TapeTrade> tape(view.begin(), view.end());
    const Params p = m_p;
    const quint64 gen = feed->tapeGeneration();
    const bool complete = !feed->loadingHistory();
    if (!m_rebuild) {
        m_rebuild = std::make_unique<QFutureWatcher<Rebuild>>();
        QObject::connect(m_rebuild.get(), &QFutureWatcher<Rebuild>::finished, [this] { finishRebuild(); });
    }
    m_sinceBuild.start();
    m_rebuild->setFuture(QtConcurrent::run([tape = std::move(tape), p, gen, complete] {
        Rebuild r;
        r.det = Detector(p.agg, p.thr);
        for (const TradeFeed::TapeTrade &x : tape)
            if (auto b = r.det.add(x.t, {x.t.side, false})) r.trades.push_back(*b);
        r.lastId = tape.empty() ? -1 : tape.back().id;
        r.gen = gen;
        r.complete = complete;
        return r;
    }));
}

void BigTradesModel::finishRebuild()
{
    Rebuild r = m_rebuild->result();
    m_det = std::move(r.det);
    m_trades = std::move(r.trades);
    m_approx = false;
    for (const BigTrade &b : m_trades) m_approx = m_approx || b.a.approximate;
    m_lastId = r.lastId;
    m_gen = r.gen;
    m_built = true;
    m_complete = r.complete;
    if (onRebuilt) onRebuilt();
}

// ---------------------------------------------------------------- layer

BigTradesLayer::Style BigTradesLayer::Style::fromJson(const QJsonObject &o, const QHash<QString, QString> &colors)
{
    Style s;
    const QString shape = o.value("shape").toString();
    s.shape = shape == "Square" ? Shape::Square : shape == "Diamond" ? Shape::Diamond : shape == "Text" ? Shape::Text : Shape::Circle;
    const double minSize = o.value("minSize").toDouble(8), maxSize = o.value("maxSize").toDouble(48);
    s.bubble.minRadius = std::clamp(minSize, 2.0, 200.0) / 2;
    s.bubble.maxRadius = std::max(s.bubble.minRadius, std::clamp(maxSize, 2.0, 200.0) / 2);
    s.bubble.minOpacity = std::clamp(o.value("minOpacity").toDouble(0.35), 0.05, 1.0);
    s.showVolume = o.value("showVolume").toString() == "On";
    // A colour left at the definition's default follows the theme (the settings dialog saves the defaults too).
    auto colour = [&](const char *key, const char *def) {
        const QString c = colors.value(key);
        return c.isEmpty() || c.compare(def, Qt::CaseInsensitive) == 0 ? QColor() : QColor(c);
    };
    s.buy = colour("buy", "#26a69a");
    s.sell = colour("sell", "#ef5350");
    return s;
}

BigTradesLayer::BigTradesLayer(ChartPane *pane, std::shared_ptr<BigTradesModel> model, const Style &style)
    : m_pane(pane), m_model(std::move(model)), m_style(style), m_timer(std::make_unique<QTimer>())
{
    m_timer->setSingleShot(true);
    m_timer->setInterval(66);
    QObject::connect(m_timer.get(), &QTimer::timeout, [this] { requestUpdate(); });
}

void BigTradesLayer::scheduleUpdate()
{
    if (!m_timer->isActive()) m_timer->start();
}

const BigTrade *BigTradesLayer::hovered() const
{
    const auto &all = m_model->trades();
    return m_hover >= 0 && size_t(m_hover) < all.size() ? &all[size_t(m_hover)] : nullptr;
}

const QPixmap &BigTradesLayer::sprite(Style::Shape shape, const QColor &color, double radius, double alpha, double dpr)
{
    const int rq = int(std::lround(radius * 2)), aq = int(std::lround(alpha * 32)), dq = int(std::lround(dpr * 4));
    const quint64 key = (quint64(color.rgb() & 0xffffff) << 32) | (quint64(int(shape) & 0x3) << 30) | (quint64(rq & 0xfff) << 16)
                        | (quint64(aq & 0x3f) << 8) | quint64(dq & 0xff);
    auto it = m_sprites.find(key);
    if (it != m_sprites.end()) return *it;
    if (m_sprites.size() > 2000) m_sprites.clear();
    const double r = rq / 2.0, a = aq / 32.0;
    const int side = int(std::ceil((2 * r + 2) * dpr));
    QPixmap pm(side, side);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter q(&pm);
    q.setRenderHint(QPainter::Antialiasing, true);
    QColor fill = color, pen = color;
    fill.setAlphaF(a * 0.8);
    pen.setAlphaF(std::min(1.0, a + 0.2));
    q.setPen(QPen(pen, 1));
    q.setBrush(fill);
    const double c = side / dpr / 2;
    switch (shape) {
    case Style::Shape::Square:
        q.drawRect(QRectF(c - r * 0.89, c - r * 0.89, r * 1.78, r * 1.78));   // the area of the circle of radius r
        break;
    case Style::Shape::Diamond: {
        const double d = r * 1.25;   // the area of the circle of radius r
        q.drawPolygon(QPolygonF{QPointF(c, c - d), QPointF(c + d, c), QPointF(c, c + d), QPointF(c - d, c)});
        break;
    }
    default:
        q.drawEllipse(QPointF(c, c), r, r);
    }
    q.end();
    return *m_sprites.insert(key, pm);
}

void BigTradesLayer::draw(QPainter &p, const QSizeF &size)
{
    m_hover = -1;
    const std::vector<BigTrade> &all = m_model->trades();
    const Bars &bars = m_pane->shown;
    if (all.empty() || bars.isEmpty() || !m_series) return;
    // Bubbles sit at a time: charts whose bars aren't time slices (Renko, Kagi…) have nowhere to put them.
    static const QStringList timeless = {"renko", "linebreak", "range", "kagi", "pnf"};
    if (timeless.contains(m_pane->cfg.type)) return;
    double tf = 60;
    try {
        tf = double(parseTf(m_pane->cfg.tf).seconds);
    } catch (...) {
    }
    // Each bar at its own index on the chart (a compared symbol adds times: bar i isn't index i).
    BarAxis axis;
    if (!axis.build(m_pane->chart->timeScale(), bars, [this](double t) { return m_pane->toChart(t); }, tf)) return;

    // Only the visible ones (binary search on time), the biggest drawn last; at most 4000 (the biggest).
    const double t0 = axis.t0, t1 = axis.t1;
    auto lo = std::lower_bound(all.begin(), all.end(), t0, [](const BigTrade &b, double t) { return b.a.time < t; });
    auto hi = std::lower_bound(lo, all.end(), t1, [](const BigTrade &b, double t) { return b.a.time < t; });
    std::vector<const BigTrade *> vis;
    vis.reserve(size_t(hi - lo));
    for (auto it = lo; it != hi; ++it) vis.push_back(&*it);
    constexpr size_t MAX_BUBBLES = 4000;
    if (vis.size() > MAX_BUBBLES) {
        std::nth_element(vis.begin(), vis.end() - MAX_BUBBLES, vis.end(),
                         [](const BigTrade *a, const BigTrade *b) { return a->a.volume < b->a.volume; });
        vis.erase(vis.begin(), vis.end() - MAX_BUBBLES);
    }
    std::sort(vis.begin(), vis.end(), [](const BigTrade *a, const BigTrade *b) { return a->a.volume < b->a.volume; });

    const BubbleStyle &style = m_style.bubble;
    const double ref = m_model->refVolume();
    const QColor up = m_style.buy.isValid() ? m_style.buy : theme::chartColor("--up");
    const QColor down = m_style.sell.isValid() ? m_style.sell : theme::chartColor("--down");
    const double dpr = p.device() ? p.device()->devicePixelRatioF() : 1;
    const std::optional<QPointF> mouse = m_pane->bigTradesMouse;
    const QFont volFont = theme::font(10, 600);
    const QFontMetricsF vfm(volFont);
    p.save();
    p.setClipRect(QRectF(0, 0, size.width(), size.height()));
    for (const BigTrade *b : vis) {
        // The bar holding the trade, then the trade's place inside it.
        if (axis.barAt(b->a.time) < axis.from) continue;
        const double x = axis.xOfTime(b->a.time);
        const auto y = m_series->priceToCoordinate(b->a.vwap());
        if (!y) continue;
        const double r = bubbleRadius(b->a.volume, ref, style);
        if (x + r < 0 || x - r > size.width() || *y + r < 0 || *y - r > size.height()) continue;
        const QColor c = b->a.side == footprint::Side::Buy ? up : down;
        const double alpha = bubbleOpacity(b->a.volume, ref, style);
        const QString vol = m_style.shape == Style::Shape::Text || m_style.showVolume ? footprint::formatVolume(b->a.volume) : QString();
        double hitR = r;
        if (m_style.shape == Style::Shape::Text) {
            // the volume in figures, its size following the bubble's, on a backing so it reads over the candles
            QFont f = theme::font(std::clamp(r * 0.7, 10.0, 24.0), 600);
            const QFontMetricsF fm(f);
            QColor tc = c;
            tc.setAlphaF(std::max(0.8, alpha));
            const QRectF box(x - fm.horizontalAdvance(vol) / 2 - 3, *y - fm.height() / 2, fm.horizontalAdvance(vol) + 6, fm.height());
            p.setRenderHint(QPainter::Antialiasing, true);
            p.setPen(Qt::NoPen);
            p.setBrush(theme::chartColor("--bg", 0.75));
            p.drawRoundedRect(box, 3, 3);
            p.setFont(f);
            p.setPen(tc);
            p.drawText(box, Qt::AlignCenter, vol);
            hitR = std::max(box.width(), box.height()) / 2;
        } else {
            const QPixmap &pm = sprite(m_style.shape, c, r, alpha, dpr);
            const double half = pm.width() / pm.devicePixelRatio() / 2;
            p.drawPixmap(QPointF(x - half, *y - half), pm);
            // the volume inside, when it fits
            if (m_style.showVolume && vfm.horizontalAdvance(vol) + 4 <= 2 * r && vfm.height() <= 2 * r) {
                p.setFont(volFont);
                p.setPen(QColor(255, 255, 255, 235));
                p.drawText(QRectF(x - r, *y - r, 2 * r, 2 * r), Qt::AlignCenter, vol);
            }
        }
        // Drawn biggest last, so the last one under the mouse is the one on top.
        if (mouse && QLineF(*mouse, QPointF(x, *y)).length() <= std::max(hitR, 4.0)) {
            m_hover = long(b - all.data());
            m_hoverAt = QPointF(x, *y);
            m_hoverR = hitR;
        }
    }
    p.restore();
}

// ---------------------------------------------------------------- tooltip

void BigTradesTooltip::draw(QPainter &p, const QSizeF &size)
{
    // The topmost layer with a bubble under the mouse (layers draw in order: the last one is on top).
    const BigTradesLayer *layer = nullptr;
    for (const auto &l : m_pane->bigTradesLayers())
        if (l->hovered()) layer = l.get();
    if (!layer) return;
    const BigTrade &b = *layer->hovered();
    const bool utc = m_pane->app->settings().utc;
    const qint64 ms = qint64(std::llround(b.a.time * 1000));
    const QDateTime t = utc ? QDateTime::fromMSecsSinceEpoch(ms, QTimeZone::UTC) : QDateTime::fromMSecsSinceEpoch(ms);
    const bool buy = b.a.side == footprint::Side::Buy;
    QStringList lines;
    lines << t.toString(t.date() == QDate::currentDate() ? "HH:mm:ss.zzz" : "d MMM HH:mm:ss.zzz") + (utc ? " UTC" : "");
    lines << QString(buy ? "Aggressive buy" : "Aggressive sell") + (b.a.approximate ? " (side estimated)" : "");
    lines << "Volume  " + footprint::formatVolume(b.a.volume);
    lines << "Avg price  " + theme::fmtPrice(b.a.vwap());
    if (b.a.maxPrice > b.a.minPrice)
        lines << QString::fromUtf8("Range  %1 – %2").arg(theme::fmtPrice(b.a.minPrice), theme::fmtPrice(b.a.maxPrice));
    lines << QString("Executions  %1").arg(b.a.executions);
    if (b.a.lastTime > b.a.time) lines << QString("Over  %1 ms").arg(std::llround((b.a.lastTime - b.a.time) * 1000));

    const QFont f = theme::font(11), bold = theme::font(11, 600);
    const QFontMetricsF fm(f), bm(bold);
    double w = 0;
    for (int i = 0; i < lines.size(); ++i) w = std::max(w, (i == 1 ? bm : fm).horizontalAdvance(lines[i]));
    const double lh = fm.height() + 2, pad = 8;
    const QSizeF box(w + 2 * pad, lines.size() * lh + 2 * pad - 2);
    const QPointF at = layer->hoveredAt();
    const double r = layer->hoveredRadius();
    QPointF tl(at.x() + r + 8, at.y() - box.height() / 2);
    if (tl.x() + box.width() > size.width() - 4) tl.setX(at.x() - r - 8 - box.width());
    tl.setY(std::clamp(tl.y(), 4.0, std::max(4.0, size.height() - box.height() - 4)));
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(theme::chartColor("--border"), 1));
    p.setBrush(theme::chartColor("--bg", 0.96));
    p.drawRoundedRect(QRectF(tl, box), 4, 4);
    for (int i = 0; i < lines.size(); ++i) {
        p.setFont(i == 1 ? bold : f);
        p.setPen(i == 1 ? theme::chartColor(buy ? "--up" : "--down") : i == 0 ? theme::chartColor("--muted") : theme::chartColor("--text"));
        p.drawText(QPointF(tl.x() + pad, tl.y() + pad + i * lh + fm.ascent()), lines[i]);
    }
    p.restore();
}

} // namespace qtm
