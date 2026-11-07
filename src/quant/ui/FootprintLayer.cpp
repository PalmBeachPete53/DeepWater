// The footprint chart's drawing (see FootprintLayer.h). A Deepwater addition.
#include "FootprintLayer.h"

#include "BarAxis.h"
#include "ChartPane.h"
#include "TradeFeed.h"
#include "Theme.h"

#include <QFontMetricsF>
#include <QPainter>

#include <algorithm>
#include <cmath>

namespace qtm {

using namespace footprint;

FootprintLayer::Settings FootprintLayer::Settings::fromJson(const QJsonObject &o)
{
    Settings s;
    const QString mode = o.value("mode").toString();
    s.mode = mode == "delta" ? Mode::Delta : mode == "profile" ? Mode::Profile : Mode::BidAsk;
    s.rowTicks = std::max(0, o.value("rows").toInt());
    if (const double r = o.value("ratio").toDouble(); r >= 1) s.params.ratio = r;
    if (o.value("minVolume").isDouble() && o.value("minVolume").toDouble() >= 0) {
        s.params.minVolume = o.value("minVolume").toDouble();
        s.params.autoMinVolume = false;
    }
    if (const int n = o.value("stack").toInt(); n >= 1) s.params.stackCount = n;
    if (const int b = o.value("budget").toInt(); b > 0) s.budget = std::clamp(b, 10000, 2000000);
    auto colour = [&](const char *key) {
        const QColor c(o.value(key).toString());
        return c.isValid() ? c : QColor();
    };
    s.buy = colour("buyColor");
    s.sell = colour("sellColor");
    s.poc = colour("pocColor");
    return s;
}

QJsonObject FootprintLayer::Settings::toJson() const
{
    const Settings d;
    QJsonObject o;
    if (mode != d.mode) o["mode"] = mode == Mode::Delta ? "delta" : "profile";
    if (rowTicks != d.rowTicks) o["rows"] = rowTicks;
    if (params.ratio != d.params.ratio) o["ratio"] = params.ratio;
    if (!params.autoMinVolume) o["minVolume"] = params.minVolume;
    if (params.stackCount != d.params.stackCount) o["stack"] = params.stackCount;
    if (budget != d.budget) o["budget"] = budget;
    if (buy.isValid()) o["buyColor"] = buy.name();
    if (sell.isValid()) o["sellColor"] = sell.name();
    if (poc.isValid()) o["pocColor"] = poc.name();
    return o;
}

FootprintLayer::FootprintLayer(ChartPane *pane) : m_pane(pane), m_timer(std::make_unique<QTimer>())
{
    m_timer->setSingleShot(true);
    m_timer->setInterval(66);
    QObject::connect(m_timer.get(), &QTimer::timeout, [this] { requestUpdate(); });
    m_regroup = std::make_unique<QTimer>();
    m_regroup->setSingleShot(true);
    m_regroup->setInterval(120);
    QObject::connect(m_regroup.get(), &QTimer::timeout, [this] {
        if (!m_feed || m_settings.rowTicks > 0 || m_wantRows <= 0) return;
        m_feed->setRowTicks(m_wantRows);   // 10-25 ms for 300,000 trades
        m_cache.clear();
        m_pane->updateLegend();
        requestUpdate();
    });
}

// Auto rows: the smallest 1-2-5 number of ticks that makes rows at least `px` tall.
static int rowsFor(double pxPerTick, double px)
{
    for (double k = 1; k < 1e9; k *= 10)
        for (double c : {1.0, 2.0, 5.0})
            if (c * k * pxPerTick >= px) return int(c * k);
    return 1;
}

void FootprintLayer::setFeed(TradeFeed *feed)
{
    if (m_feed == feed) return;
    m_feed = feed;
    m_cache.clear();
    requestUpdate();
}

void FootprintLayer::setSettings(const Settings &s)
{
    m_settings = s;
    m_cache.clear();
    requestUpdate();
}

void FootprintLayer::scheduleUpdate()
{
    if (!m_timer->isActive()) m_timer->start();
}

const Analysis &FootprintLayer::analysisOf(double time, const CandleFootprint &c)
{
    const int rowTicks = m_feed->book().grid().rowTicks;
    Cached &e = m_cache[time];
    if (e.version != c.version() || e.rowTicks != rowTicks) e = Cached{c.version(), rowTicks, analyze(c, m_settings.params)};
    return e.a;
}

// Laid out once per text and font: hundreds of numbers a frame.
const QStaticText &FootprintLayer::label(const QString &s, const QFont &font, bool bold)
{
    const QString key = bold ? QStringLiteral("\x01") + s : s;
    auto it = m_labels.find(key);
    if (it == m_labels.end()) {
        if (m_labels.size() > 6000) m_labels.clear();
        QStaticText st(s);
        st.setTextFormat(Qt::PlainText);
        st.setPerformanceHint(QStaticText::AggressiveCaching);
        st.prepare(QTransform(), font);
        it = m_labels.insert(key, st);
    }
    return *it;
}

void FootprintLayer::draw(QPainter &p, const QSizeF &size)
{
    const Bars &bars = m_pane->shown;
    if (bars.isEmpty() || !m_series) return;
    // Each bar at its own index on the chart (a compared symbol adds times: bar i isn't index i).
    BarAxis axis;
    if (!axis.build(m_pane->chart->timeScale(), bars, [this](double t) { return m_pane->toChart(t); }, 60)) return;
    const int n = axis.n, from = axis.from, to = axis.to;
    const double bs = std::max(0.5, axis.bs);
    auto xOf = [&](int i) { return axis.xOfBar(i); };
    // Price to y: a straight line unless the scale is logarithmic (percentage and indexed scales are linear too),
    // so a frame asks the chart twice instead of for every candle and row.
    const bool logScale = m_pane->cfg.log && m_pane->cfg.compare.isEmpty();
    const double ref = bars[to].close;
    const double yRef = m_series->priceToCoordinate(ref).value_or(NaN);
    const double ySlope = (m_series->priceToCoordinate(ref * 1.01).value_or(NaN) - yRef) / (ref * 0.01);
    auto yOf = [&](double price) {
        return logScale || !std::isfinite(ySlope) ? m_series->priceToCoordinate(price).value_or(NaN)
                                                  : yRef + (price - ref) * ySlope;
    };

    if (m_cache.size() > 5000) m_cache.clear();   // before the frame takes references into it
    const QColor up = m_settings.buy.isValid() ? m_settings.buy : theme::chartColor("--up");
    const QColor down = m_settings.sell.isValid() ? m_settings.sell : theme::chartColor("--down");
    auto tint = [&](bool buy, double alpha) {
        QColor c = buy ? up : down;
        c.setAlphaF(std::clamp(alpha, 0.0, 1.0));
        return c;
    };
    const QColor text = theme::chartColor("--text"), muted = theme::chartColor("--muted");
    const QColor poc = m_settings.poc.isValid() ? m_settings.poc : QColor("#ff9800");

    // The candles that have a complete footprint.
    struct Item {
        int i;
        const CandleFootprint *c;
        const Analysis *a;   // only when the rows are drawn
    };
    std::vector<Item> items;
    const Book *book = m_feed && m_feed->ready() ? &m_feed->book() : nullptr;
    const TickGrid grid = book ? book->grid() : TickGrid{};
    if (book) {
        const double complete = m_feed->completeFrom();
        for (int i = from; i <= to; ++i) {
            const double t = bars[i].time;
            if (std::isnan(complete) || t < complete) continue;
            const CandleFootprint *c = book->candle(t);
            if (!c || c->empty()) continue;
            items.push_back({i, c, nullptr});
        }
    }

    // How much detail fits. Rows under a pixel, or candles narrower than 6 px, are plain candles: no analysis needed.
    const QFont font = theme::font(10), bold = theme::font(10, 600);
    const QFontMetricsF fm(font);
    const double gap = std::max(1.0, std::floor(bs * 0.04));
    const double candleW = std::clamp(std::floor(bs * 0.07), 3.0, 7.0);
    const double cellsW = bs - 2 * gap - candleW - 2;
    const bool cellsFit = cellsW >= 3;   // room for cells beside the thin candle (bs ≈ 10 px)
    const double rowPx = book ? std::fabs(yOf(ref + grid.rowHeight()) - yOf(ref)) : 0;
    double maxVol = 0, maxDelta = 0, maxCandleVol = 0, maxCandleDelta = 0;
    int64_t minRow = INT64_MAX, maxRow = INT64_MIN;
    for (const Item &it : items) {
        maxCandleVol = std::max(maxCandleVol, it.c->volume());
        maxCandleDelta = std::max(maxCandleDelta, std::fabs(it.c->delta()));
        minRow = std::min(minRow, it.c->lowRow());
        maxRow = std::max(maxRow, it.c->highRow());
    }
    // Auto rows follow the vertical zoom: rows between 11 and 32 px stay as they are (one 1-2-5 step is at most 2.5×,
    // so a finer step would still be under 14 px), others regroup once the zoom stops moving.
    if (book && m_settings.rowTicks <= 0 && cellsFit && rowPx > 0 && (rowPx < 11 || rowPx > 32)) {
        const int want = rowsFor(rowPx / grid.rowTicks, 14);
        if (want != grid.rowTicks && want != m_wantRows) {
            m_wantRows = want;
            m_regroup->start();
        }
    } else {
        m_wantRows = 0;
    }
    Detail detail = Detail::Candles;
    if (!items.empty() && rowPx >= 1 && cellsFit && maxRow - minRow < 50000) {
        for (Item &it : items) {
            it.a = &analysisOf(bars[it.i].time, *it.c);
            maxVol = std::max(maxVol, it.a->maxLevelVolume);
            maxDelta = std::max(maxDelta, it.a->maxAbsLevelDelta);
        }
        double textW = 0;
        switch (m_settings.mode) {
        case Mode::BidAsk:
            textW = 2 * fm.horizontalAdvance(footprint::formatVolume(maxVol)) + fm.horizontalAdvance(" x ") + 6;
            break;
        case Mode::Delta:
            textW = fm.horizontalAdvance("+" + footprint::formatVolume(maxDelta)) + 6;
            break;
        case Mode::Profile:
            textW = fm.horizontalAdvance(footprint::formatVolume(maxVol)) + 12;
            break;
        }
        detail = rowPx >= 11 && cellsW >= textW ? Detail::Text : Detail::Colors;
    }
    m_lastDetail = detail;

    // Row edges on screen, once for every row on show (exact on a log scale too).
    std::vector<double> rowY;
    if (detail != Detail::Candles) {
        rowY.resize(size_t(maxRow - minRow + 2));
        for (int64_t r = minRow; r <= maxRow + 1; ++r) rowY[size_t(r - minRow)] = yOf(grid.rowLow(r));
    }
    auto edge = [&](int64_t r) { return rowY[size_t(r - minRow)]; };   // bottom edge of row r (top of r - 1)

    auto plainCandle = [&](const Bar &b, double cx, double w) {
        const QColor c = b.close >= b.open ? up : down;
        const double yo = yOf(b.open), yc = yOf(b.close), yh = yOf(b.high), yl = yOf(b.low);
        if (std::isnan(yo) || std::isnan(yc) || std::isnan(yh) || std::isnan(yl)) return;
        const double wick = std::max(1.0, std::floor(std::min(w * 0.15, 2.0)));
        p.fillRect(QRectF(std::round(cx - wick / 2), yh, wick, std::max(1.0, yl - yh)), c);
        p.fillRect(QRectF(std::round(cx - w / 2), std::min(yo, yc), std::max(1.0, w), std::max(1.0, std::fabs(yc - yo))), c);
    };

    p.save();
    p.setRenderHint(QPainter::Antialiasing, false);

    // Candles without a footprint (before the history, or zoomed out): plain candles.
    {
        size_t k = 0;
        const double plainW = detail == Detail::Candles ? std::max(1.0, std::floor(bs * 0.8))
                                                        : std::max(1.0, std::min(std::floor(bs * 0.6), 12.0));
        for (int i = from; i <= to; ++i) {
            const bool hasFp = k < items.size() && items[k].i == i;
            if (hasFp) ++k;
            if (hasFp && detail != Detail::Candles) continue;
            plainCandle(bars[i], xOf(i), plainW);
        }
    }

    if (detail != Detail::Candles) {
        const double fh = fm.height();
        for (const Item &it : items) {
            const Analysis &a = *it.a;
            const CandleFootprint &c = *it.c;
            const double cx = xOf(it.i);
            const double slotL = cx - bs / 2 + gap, slotR = cx + bs / 2 - gap;
            const double cellsL = slotL + candleW + 2, cellsR = slotR;
            const double cw = cellsR - cellsL;
            if (cw < 1) {   // can't happen with cellsFit; a plain candle rather than nothing
                plainCandle(bars[it.i], cx, std::max(1.0, std::floor(bs * 0.8)));
                continue;
            }
            // the open/close: a thin candle on the left
            plainCandle(bars[it.i], slotL + candleW / 2, candleW);

            const double rowGap = rowPx >= 6 ? 1 : 0;
            for (int64_t r = a.lowRow; r <= a.highRow; ++r) {
                const double yTop = edge(r + 1), yBot = edge(r);
                const QRectF cell(cellsL, yTop, cw, std::max(1.0, yBot - yTop - rowGap));
                const Level l = c.level(r);
                const double vol = l.volume(), d = l.delta();
                if (m_settings.mode == Mode::Profile) {
                    const double share = a.maxLevelVolume > 0 ? vol / a.maxLevelVolume : 0;
                    p.fillRect(cell, theme::chartColor("--muted", 0.08));
                    p.fillRect(QRectF(cellsL, cell.top(), cw * share, cell.height()),
                               tint(d >= 0, 0.25 + 0.35 * share));
                } else {
                    const double k = a.maxAbsLevelDelta > 0 ? std::fabs(d) / a.maxAbsLevelDelta : 0;
                    const double alpha = detail == Detail::Text ? (m_settings.mode == Mode::Delta ? 0.08 + 0.5 * k : 0.05 + 0.3 * k)
                                                                : 0.12 + 0.6 * k;
                    p.fillRect(cell, tint(d >= 0, vol > 0 ? alpha : 0.03));
                }
            }
            // stacked imbalance zones, from the candle to the first later bar that trades into them
            for (const Zone &z : a.stacks) {
                const double yTop = edge(z.toRow + 1), yBot = edge(z.fromRow);
                const QColor zc = z.side == Side::Buy ? up : down;
                QColor fill = zc;
                fill.setAlphaF(0.22);
                p.fillRect(QRectF(cellsL, yTop, cw, yBot - yTop), fill);
                const double zLo = grid.rowLow(z.fromRow), zHi = grid.rowLow(z.toRow + 1);
                double endX = size.width();
                for (int j = it.i + 1; j < n; ++j)
                    if (bars[j].low < zHi && bars[j].high > zLo) {
                        endX = xOf(j) - bs / 2;
                        break;
                    }
                if (endX > cellsR) {
                    fill.setAlphaF(detail == Detail::Text ? 0.10 : 0.06);
                    p.fillRect(QRectF(cellsR, yTop, endX - cellsR, yBot - yTop), fill);
                }
                if (detail != Detail::Text) continue;   // zoomed out, the outlines would be all you see
                QPen pen(zc);
                pen.setWidthF(1);
                p.setPen(pen);
                p.setBrush(Qt::NoBrush);
                p.drawRect(QRectF(cellsL, yTop, cw - 1, std::max(1.0, yBot - yTop - 1)));
            }
            // numbers
            if (detail == Detail::Text) {
                const double mid = cellsL + cw / 2;
                for (int64_t r = a.lowRow; r <= a.highRow; ++r) {
                    const double yTop = edge(r + 1), yBot = edge(r);
                    const double ty = (yTop + yBot) / 2 - fh / 2;
                    const Level l = c.level(r);
                    if (l.volume() <= 0) continue;   // a price inside the candle that didn't trade: left blank
                    const uint8_t im = a.imbalanceAt(r);
                    auto put = [&](const QString &s, double x, int align, bool strong, const QColor &col) {
                        const QFont &f = strong ? bold : font;
                        const QStaticText &st = label(s, f, strong);
                        const double w = st.size().width();
                        const double left = align < 0 ? x - w : align == 0 ? x - w / 2 : x;
                        p.setFont(f);
                        p.setPen(col);
                        p.drawStaticText(QPointF(left, ty), st);
                    };
                    switch (m_settings.mode) {
                    case Mode::BidAsk:
                        put(footprint::formatVolume(l.bid), mid - 5, -1, im & SellImbalance, im & SellImbalance ? down : text);
                        put("x", mid, 0, false, muted);
                        put(footprint::formatVolume(l.ask), mid + 5, 1, im & BuyImbalance, im & BuyImbalance ? up : text);
                        break;
                    case Mode::Delta: {
                        const double d = l.delta();
                        const QString s = (d > 0 ? "+" : "") + footprint::formatVolume(d);
                        put(s, mid, 0, im != 0, im & BuyImbalance ? up : im & SellImbalance ? down : text);
                        break;
                    }
                    case Mode::Profile:
                        put(footprint::formatVolume(l.volume()), cellsL + 3, 1, im != 0, text);
                        break;
                    }
                }
            }
            // imbalances: a mark on the side that won (asks right, bids left)
            const double markW = std::max(1.0, std::min(2.0, std::floor(cw * 0.12)));
            for (int64_t r = a.lowRow; r <= a.highRow; ++r) {
                const uint8_t im = a.imbalanceAt(r);
                if (!im) continue;
                const double yTop = edge(r + 1), h = std::max(1.0, edge(r) - yTop - rowGap);
                if (im & BuyImbalance) p.fillRect(QRectF(cellsR - markW, yTop, markW, h), up);
                if (im & SellImbalance) p.fillRect(QRectF(cellsL, yTop, markW, h), down);
            }
            // POC
            {
                const double yTop = edge(a.pocRow + 1), yBot = edge(a.pocRow);
                QPen pen(poc);
                pen.setWidthF(1);
                p.setPen(pen);
                p.setBrush(Qt::NoBrush);
                p.drawRect(QRectF(cellsL, yTop, cw - 1, std::max(1.0, yBot - yTop - rowGap - 1)));
            }
        }
    }

    // The delta / volume strip at the bottom.
    if (!items.empty()) {
        const double top = size.height() - STRIP_HEIGHT;
        p.fillRect(QRectF(0, top, size.width(), STRIP_HEIGHT), theme::chartColor("--bg", 0.92));
        p.fillRect(QRectF(0, top, size.width(), 1), theme::chartColor("--border"));
        const double rowH = (STRIP_HEIGHT - 2) / 2;
        const QString names[] = {QString::fromUtf8("Δ"), "Vol"};
        const double namesW = std::max(fm.horizontalAdvance(names[0]), fm.horizontalAdvance(names[1])) + 8;
        p.setFont(font);
        // Numbers for every candle or for none: the widest of them decides.
        // (digits are about as wide as each other: only the longest strings are measured)
        QString longD, longV;
        if (bs - 2 * gap >= 3 * fm.horizontalAdvance('0'))
            for (const Item &it : items) {
                const double d = it.c->delta();
                const QString ds = (d > 0 ? "+" : "") + footprint::formatVolume(d), vs = footprint::formatVolume(it.c->volume());
                if (ds.size() > longD.size()) longD = ds;
                if (vs.size() > longV.size()) longV = vs;
            }
        const bool numbers = !longD.isEmpty()
                             && std::max(fm.horizontalAdvance(longD), fm.horizontalAdvance(longV)) + 4 <= bs - 2 * gap;
        for (const Item &it : items) {
            const double cx = xOf(it.i);
            if (cx - (bs - 2 * gap) / 2 < namesW) continue;   // under the row names
            const double w = bs - 2 * gap;
            const double d = it.c->delta();
            const QString ds = (d > 0 ? "+" : "") + footprint::formatVolume(d), vs = footprint::formatVolume(it.c->volume());
            if (numbers) {
                p.setPen(d >= 0 ? up : down);
                p.drawText(QRectF(cx - w / 2, top + 1, w, rowH), Qt::AlignCenter, ds);
                p.setPen(muted);
                p.drawText(QRectF(cx - w / 2, top + 1 + rowH, w, rowH), Qt::AlignCenter, vs);
            } else {
                const double k = maxCandleDelta > 0 ? std::fabs(d) / maxCandleDelta : 0;
                p.fillRect(QRectF(cx - w / 2, top + 3, std::max(1.0, w), rowH - 4),
                           tint(d >= 0, 0.2 + 0.8 * k));
                const double kv = maxCandleVol > 0 ? it.c->volume() / maxCandleVol : 0;
                p.fillRect(QRectF(cx - w / 2, top + 1 + rowH + 2, std::max(1.0, w), rowH - 4),
                           theme::chartColor("--muted", 0.15 + 0.6 * kv));
            }
        }
        // the strip's row names
        for (int k = 0; k < 2; ++k) {
            const QRectF r(0, top + 1 + k * rowH, namesW, rowH);
            p.fillRect(r, theme::chartColor("--bg"));
            p.setPen(muted);
            p.drawText(r, Qt::AlignCenter, names[k]);
        }
        // sides worked out from prices, not given by the exchange
        if (book && book->approximate()) {
            const QString s = "APPROXIMATE SIDES";
            const QFont f = theme::font(9, 600);
            const QFontMetricsF bm(f);
            const QRectF r(6, top - bm.height() - 8, bm.horizontalAdvance(s) + 10, bm.height() + 4);
            p.fillRect(r, theme::chartColor("--down", 0.85));
            p.setFont(f);
            p.setPen(Qt::white);
            p.drawText(r, Qt::AlignCenter, s);
        }
    }
    p.restore();
}

} // namespace qtm
