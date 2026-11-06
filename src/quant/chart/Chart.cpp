// The chart widget: layout (ChartWidget._adjustSizeImpl), mouse and wheel handling (PaneWidget,
// PriceAxisWidget, TimeAxisWidget, PaneSeparator, MouseEventHandler), crosshair, and the public
// API objects.

#include "ChartPrivate.h"

#include <QFontDatabase>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include <algorithm>

namespace lw {

using namespace detail;

namespace {
SeriesImpl *impl(Series *s) { return static_cast<SeriesImpl *>(s); }
const SeriesImpl *impl(const Series *s) { return static_cast<const SeriesImpl *>(s); }
} // namespace

} // namespace lw

namespace lw::detail {

// ================================================================ helpers

double ChartPrivate::effectiveDpr() const
{
    if (paintDpr > 0)
        return paintDpr;
    const QVariant v = q->property("lwDevicePixelRatio");   // test hook for screenshots
    if (v.isValid() && v.toDouble() > 0)
        return v.toDouble();
    return q->devicePixelRatioF();
}

SeriesOptions ChartPrivate::resolveOptions(SeriesOptions o, SeriesType type)
{
    if (!o.color.isValid())
        o.color = type == SeriesType::Histogram ? QColor("#26a69a") : QColor("#2196f3");
    if (o.priceScaleId.isEmpty())
        o.priceScaleId = QStringLiteral("right");
    return o;
}

int ChartPrivate::crosshairPaneIndex() const
{
    return ch.pane ? paneIndexOf(ch.pane) : -1;
}

void ChartPrivate::request()
{
    dirty = true;
    q->update();
}

bool ChartPrivate::canFlush() const
{
    return q->isVisible() || q->testAttribute(Qt::WA_Resized);
}

void ChartPrivate::queueTimeOp(std::function<void()> op)
{
    pendingOps.push_back(std::move(op));
    request();
}

void ChartPrivate::updateFonts()
{
    if (fontSizeCached == opts.fontSize && fontFamilyCached == opts.fontFamily)
        return;
    QFont f = opts.fontFamily.isEmpty() ? QFontDatabase::systemFont(QFontDatabase::GeneralFont)
                                        : QFont(opts.fontFamily);
    f.setPixelSize(opts.fontSize);
    f.setStyleStrategy(QFont::PreferAntialias);
    text.setFont(f);
    QFont b = f;
    b.setBold(true);
    bold.setFont(b);
    fontSizeCached = opts.fontSize;
    fontFamilyCached = opts.fontFamily;
    for (auto &p : panes)
        for (auto *sc : p->allScales())
            sc->marksValid = false;
}

void ChartPrivate::autoscaleAll()
{
    int from = 0, to = 0;
    const bool have = ts.strictRange(from, to);
    for (auto &p : panes)
        for (PriceScaleModel *sc : p->allScales()) {
            if (sc->sources.isEmpty())
                continue;
            if (!have)
                continue;
            if (sc->opts.autoScale || !sc->range) {
                sc->autoscaleFor(std::make_pair(from, to));
            }
        }
}

double ChartPrivate::optimalAxisWidth(PaneImpl *pane)
{
    PriceScaleModel *sc = pane->right.get();
    if (!sc->opts.visible)
        return 0;
    double t = 0;
    const auto &marks = sc->tickMarks();
    if (!marks.isEmpty())
        t = std::max(text.width(marks.first().label), text.width(marks.last().label));
    for (const AxisLabel &l : axisLabels(pane, sc))
        if (l.axisVisible)
            t = std::max({t, text.width(l.axisText), l.axisDetail.isEmpty() ? 0.0 : text.width(l.axisDetail)});
    const auto fv = sc->firstValue();
    if (fv && opts.crosshairMode != CrosshairMode::Hidden && pane->height > 0 && !sc->isEmpty()) {
        const double a = sc->coordinateToPrice(1, *fv);
        const double b = sc->coordinateToPrice(pane->height - 2, *fv);
        t = std::max({t,
                      text.width(sc->formatPrice(std::floor(std::min(a, b)) + 0.11111111111111, *fv)),
                      text.width(sc->formatPrice(std::ceil(std::max(a, b)) - 0.11111111111111, *fv))});
    }
    const double l = t > 0 ? t : 34;
    const double fs = opts.fontSize;
    const int res = int(std::ceil(1 + 5 + 2 * (fs / 12 * 5) + 5 + l));
    return res + res % 2;
}

void ChartPrivate::layout(bool full)
{
    updateFonts();
    const double dpr = effectiveDpr();
    layoutDpr = dpr;
    int W = int(std::floor(q->width())), H = int(std::floor(q->height()));
    W -= W % 2;
    H -= H % 2;
    mediaW = W;
    mediaH = H;
    const double fs = opts.fontSize;
    int o = int(std::ceil(1 + 5 + fs + 3 * fs / 12 + 3 * fs / 12 + 4 * fs / 12));
    o += o % 2;
    timeAxisHeight = o;
    const bool axisVisible = !panes.empty() && panes[0]->right->opts.visible;

    auto apply = [&](double aw) {
        paneWidth = std::max(W - aw, 0.0);
        double totalStretch = 0;
        for (auto &p : panes)
            totalStretch += p->stretch;
        const double seps = double(panes.size()) - 1;
        const double c = H < seps + o ? 0 : H - (seps + o);
        const double per = totalStretch > 0 ? c / totalStretch : 0;
        double acc = 0, top = 0;
        for (size_t i = 0; i < panes.size(); ++i) {
            PaneImpl *p = panes[i].get();
            double h = i + 1 == panes.size() ? std::ceil((c - acc) * dpr) / dpr
                                             : jsRound(p->stretch * per * dpr) / dpr;
            h = std::max(h, 2.0);
            acc += h;
            p->top = top;
            p->height = h;
            top += h + 1;
            for (PriceScaleModel *sc : p->allScales())
                sc->setHeight(h);
        }
        if (paneWidth > 0)
            ts.setWidth(paneWidth);
    };

    // ChartWidget._adjustSizeImpl: the axis width is the widest label right now (with the price
    // range of the previous view), once per full frame; it doesn't look ahead
    auto adjust = [&] {
        double need = 0;
        if (axisVisible)
            for (auto &p : panes)
                need = std::max(need, optimalAxisWidth(p.get()));
        need += int(need) % 2;
        axisWidth = need;
        apply(axisWidth);
    };
    // a price axis widget starts with 0 as the width of its last marks
    for (auto &p : panes)
        if (p->am < 0)
            p->am = 0;
    if (!sized && W > 0) {
        // createChart lays the chart out before it has a price axis, so the first data comes in
        // with the default view over the whole width
        sized = true;
        axisWidth = 0;
        apply(axisWidth);
        autoscaleAll();
        full = true;
    }
    // one animation frame (ChartWidget._drawImpl): full -> adjust the size; autoscale; time scale
    // operations; price axis marks. A marks rebuild that needs a wider axis than that widget had so
    // far asks for a full update (PriceAxisWidget._onMarksChanged); asked for during the frame, it
    // is merged and the frame's operations run again with the new size; asked for while painting,
    // it's the next frame, run here right away so a chart is drawn as the browser shows it settled.
    auto ops = canFlush() ? std::move(pendingOps) : std::vector<std::function<void()>>();
    if (canFlush())
        pendingOps.clear();
    for (int frame = 0; frame < 4; ++frame) {
        inFrame = true;
        fullRequested = false;
        if (full)
            adjust();
        else
            apply(axisWidth);
        autoscaleAll();
        for (auto &op : ops)
            op();
        autoscaleAll();
        for (auto &p : panes)
            if (p->right->opts.visible)
                p->right->tickMarks();
        if (fullRequested) {
            fullRequested = false;
            adjust();
            autoscaleAll();
            for (auto &op : ops)
                op();
            autoscaleAll();
        }
        inFrame = false;
        // painting: the series read their price ranges, then the axes their marks
        for (auto &p : panes)
            for (PriceScaleModel *sc : p->allScales())
                sc->ensureRange();
        for (auto &p : panes)
            for (PriceScaleModel *sc : p->allScales())
                sc->tickMarks();
        if (!fullRequested)
            break;
        full = true;
        ops.clear();
    }
    fullRequested = false;
}

// PriceAxisWidget._onMarksChanged: the axis asks for a full update when it needs more room than
// its last marks did
void ChartPrivate::marksRebuilt(PriceScaleModel *sc)
{
    PaneImpl *pane = sc->pane;
    if (!pane || sc != pane->right.get() || !sc->opts.visible || pane->am < 0)
        return;
    const double t = optimalAxisWidth(pane);
    if (pane->am < t) {
        if (inFrame || updating)
            fullRequested = true;
        else {
            fullLayoutNeeded = true;
            request();
        }
    }
    pane->am = t;
}

void ChartPrivate::ensureUpdated()
{
    if (updating)
        return;
    if (!dirty && layoutDpr == effectiveDpr() && (pendingOps.empty() || !canFlush()))
        return;
    updating = true;
    layout(fullLayoutNeeded);
    fullLayoutNeeded = false;
    dirty = false;
    // keep the crosshair on the bar under the mouse after scrolling / new data (ChartModel.Ra)
    if (ch.visible && ch.pane && std::isfinite(ch.rawX)) {
        setCrosshair(ch.rawX, ch.rawY, ch.pane, false);
    }
    updating = false;
}

void ChartPrivate::scheduleCrosshairEmit()
{
    if (!ch.visible || emitScheduled)
        return;
    emitScheduled = true;
    QMetaObject::invokeMethod(
        q,
        [this] {
            emitScheduled = false;
            if (!ch.visible || !ch.pane)
                return;
            ensureUpdated();
            if (!ch.visible || !ch.pane)
                return;
            emit q->crosshairMoved(params(ch.index, QPointF(ch.rawX, ch.rawY), paneIndexOf(ch.pane)));
        },
        Qt::QueuedConnection);
}

// ================================================================ crosshair

MouseEventParams ChartPrivate::params(std::optional<int> logical, std::optional<QPointF> point,
                                      int pane)
{
    MouseEventParams mp;
    mp.point = point;
    mp.paneIndex = std::max(0, pane);
    if (logical) {
        mp.logical = *logical;
        if (*logical >= 0 && *logical < ts.points.size())
            mp.time = ts.points[*logical].time;
    }
    return mp;
}

double ChartPrivate::magnet(double price, int index, PaneImpl *pane)
{
    if (opts.crosshairMode != CrosshairMode::Magnet)
        return price;
    PriceScaleModel *e = pane->defaultScale();
    const auto r = e->firstValue();
    if (!r)
        return price;
    const double h = e->priceToCoordinate(price, *r);
    QVector<double> cands;
    for (SeriesImpl *s : pane->series) {
        if (s->isOverlay() || !s->opts.visible)
            continue;
        PriceScaleModel *sc = s->scale;
        if (sc->isEmpty())
            continue;
        const Row *row = s->rowAt(index);
        const auto fv = s->firstValue();
        if (!row || !fv)
            continue;
        cands.push_back(sc->priceToCoordinate(row->c, fv->first));
    }
    if (cands.isEmpty())
        return price;
    std::sort(cands.begin(), cands.end(),
              [h](double a, double b) { return std::abs(a - h) < std::abs(b - h); });
    return e->coordinateToPrice(cands.first(), *r);
}

void ChartPrivate::setCrosshair(double x, double y, PaneImpl *pane, bool emitEvent)
{
    ch.rawX = x;
    ch.rawY = y;
    int h = ts.coordinateToIndex(x);
    int from, to;
    if (ts.strictRange(from, to))
        h = std::min(std::max(from, h), to);
    PriceScaleModel *l = pane->defaultScale();
    const auto o = l->firstValue();
    double price = NaN;
    if (o)
        price = l->coordinateToPrice(y, *o);
    price = magnet(price, h, pane);
    ch.visible = true;
    ch.index = h;
    ch.x = ts.isEmpty() ? NaN : ts.coordOf(h);
    ch.pane = pane;
    if (!l->isEmpty() && o) {
        ch.price = price;
        ch.y = l->priceToCoordinate(price, *o);
    } else {
        ch.price = NaN;
        ch.y = NaN;
    }
    if (emitEvent)
        emit q->crosshairMoved(params(h, QPointF(x, y), paneIndexOf(pane)));
}

void ChartPrivate::clearCrosshair(bool emitEvent)
{
    const bool was = ch.visible;
    ch = CrosshairState();
    if (emitEvent && was) {
        MouseEventParams mp;
        emit q->crosshairMoved(mp);
    }
}

void ChartPrivate::refreshCrosshair()
{
    if (ch.visible && ch.pane)
        setCrosshair(ch.rawX, ch.rawY, ch.pane, false);
}

// ================================================================ series removal

void ChartPrivate::removeSeriesInternal(SeriesImpl *s, bool allowPaneRemoval)
{
    setSeriesData(s, {});
    PaneImpl *pane = s->pane;
    for (Primitive *prim : s->primitives)
        prim->detached();
    s->primitives.clear();
    detachFromScale(s);
    if (pane) {
        pane->series.removeAll(s);
        renumber(pane);
        for (PriceScaleModel *sc : pane->allScales())
            sc->updateFormatter();
    }
    for (auto it = series.begin(); it != series.end(); ++it)
        if (it->get() == s) {
            series.erase(it);
            break;
        }
    if (allowPaneRemoval && pane && pane->series.isEmpty() && panes.size() > 1)
        removePaneObject(pane);
    invalidateAll();
    request();
}

// ================================================================ animation

void ChartPrivate::startAnimation(std::function<bool(double)> anim)
{
    animation = std::move(anim);
    if (!animTimer.isActive())
        animTimer.start();
}

void ChartPrivate::stopAnimation()
{
    animation = nullptr;
    kinetic.reset();
    animTimer.stop();
}

void ChartPrivate::animationTick()
{
    if (!animation) {
        animTimer.stop();
        return;
    }
    const double now = double(clock.nsecsElapsed()) / 1e6;
    const bool done = animation(now);
    invalidateScales();
    request();
    scheduleCrosshairEmit();
    if (done)
        stopAnimation();
}

// ================================================================ interaction

ChartPrivate::Hit ChartPrivate::hitTest(const QPointF &pos) const
{
    Hit h;
    if (pos.x() < 0 || pos.y() < 0 || pos.x() >= mediaW || pos.y() >= mediaH || panes.empty())
        return h;
    if (opts.enablePaneResize) {
        for (size_t k = 0; k + 1 < panes.size(); ++k) {
            const double sepTop = panes[k]->top + panes[k]->height;
            if (pos.y() >= sepTop - 4 && pos.y() < sepTop + 5) {
                h.area = Area::Separator;
                h.pane = int(k);
                h.local = QPointF(pos.x(), pos.y() - sepTop);
                return h;
            }
        }
    }
    const double timeTop = mediaH - timeAxisHeight;
    if (pos.y() >= timeTop) {
        h.area = pos.x() < paneWidth ? Area::TimeAxis : Area::Corner;
        h.local = QPointF(pos.x(), pos.y() - timeTop);
        return h;
    }
    for (size_t i = 0; i < panes.size(); ++i) {
        const PaneImpl *p = panes[i].get();
        if (pos.y() >= p->top && pos.y() < p->top + p->height) {
            h.pane = int(i);
            if (pos.x() < paneWidth) {
                h.area = Area::Pane;
                h.local = QPointF(pos.x(), pos.y() - p->top);
            } else {
                h.area = Area::PriceAxis;
                h.local = QPointF(pos.x() - paneWidth, pos.y() - p->top);
            }
            return h;
        }
    }
    return h;
}

void ChartPrivate::updateCursor(const Hit &hit)
{
    Qt::CursorShape shape = Qt::ArrowCursor;
    if (hit.area == Area::PriceAxis && opts.handleScale && hit.pane >= 0) {
        PriceScaleModel *sc = panes[hit.pane]->right.get();
        if (!sc->isPercentage() && !sc->isIndexed())
            shape = Qt::SizeVerCursor;
    } else if (hit.area == Area::TimeAxis && opts.handleScale) {
        shape = Qt::SizeHorCursor;
    } else if (hit.area == Area::Separator || dragSeparator >= 0) {
        shape = Qt::SplitVCursor;
    }
    // Over the plain plot, inherit the parent's cursor (the app shows a crosshair while a drawing
    // tool is active, the way the page's body cursor shows through the chart).
    if (shape == Qt::ArrowCursor) {
        if (q->testAttribute(Qt::WA_SetCursor))
            q->unsetCursor();
    } else if (q->cursor().shape() != shape || !q->testAttribute(Qt::WA_SetCursor)) {
        q->setCursor(shape);
    }
}

void ChartPrivate::hover(const QPointF &pos)
{
    ensureUpdated();
    const Hit hit = hitTest(pos);
    const int sep = hit.area == Area::Separator ? hit.pane : -1;
    if (sep != hoverSeparator) {
        hoverSeparator = sep;
        q->update();
    }
    if (hit.area == Area::Pane) {
        PaneImpl *pane = panes[hit.pane].get();
        const double x = std::max(0.0, std::min(hit.local.x(), paneWidth - 1));
        const double y = std::max(0.0, std::min(hit.local.y(), pane->height - 1));
        mouseInPane = true;
        setCrosshair(x, y, pane, true);
        request();
    } else if (mouseInPane) {
        mouseInPane = false;
        clearCrosshair(true);
        request();
    }
    updateCursor(hit);
}

void ChartPrivate::leave()
{
    if (pressed)
        return;
    if (hoverSeparator >= 0) {
        hoverSeparator = -1;
        q->update();
    }
    if (mouseInPane) {
        mouseInPane = false;
        clearCrosshair(true);
        request();
    }
}

void ChartPrivate::press(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton)
        return;
    ensureUpdated();
    const QPointF pos = e->position();
    const Hit hit = hitTest(pos);
    pressHit = hit;
    pressPos = pos;
    pressed = true;
    dragging = false;
    scrollStarted = false;
    scrollAnchor.reset();
    kinetic.reset();
    // double-click window: 500 ms from the first press
    if (!clickWindow || clickTimer.elapsed() > 500) {
        clickWindow = true;
        clickCount = 0;
        firstPressPos = pos;
        clickTimer.start();
    }
    switch (hit.area) {
    case Area::Pane: {
        stopAnimation();
        PaneImpl *pane = panes[hit.pane].get();
        const double x = std::max(0.0, std::min(hit.local.x(), paneWidth - 1));
        const double y = std::max(0.0, std::min(hit.local.y(), pane->height - 1));
        mouseInPane = true;
        setCrosshair(x, y, pane, true);
        request();
        break;
    }
    case Area::PriceAxis: {
        PriceScaleModel *sc = panes[hit.pane]->right.get();
        if (opts.handleScale && !sc->isEmpty()) {
            priceScaleDragged = true;
            sc->startScale(hit.local.y());
        }
        break;
    }
    case Area::TimeAxis:
        if (opts.handleScale && !ts.isEmpty()) {
            stopAnimation();
            ts.startScale(hit.local.x());
        }
        break;
    case Area::Separator: {
        PaneImpl *a = panes[hit.pane].get();
        PaneImpl *b = panes[hit.pane + 1].get();
        const double total = a->stretch + b->stretch;
        const double perPx = total / (a->height + b->height);
        const double minS = 30 * perPx;
        if (total > 2 * minS) {
            dragSeparator = hit.pane;
            sepStartY = pos.y();
            sepStartStretch = a->stretch;
            sepTotal = total;
            sepPerPx = perPx;
            sepMin = minS;
        }
        break;
    }
    default:
        break;
    }
}

void ChartPrivate::move(QMouseEvent *e)
{
    const QPointF pos = e->position();
    if (!pressed || !(e->buttons() & Qt::LeftButton)) {
        if (pressed && !(e->buttons() & Qt::LeftButton))
            pressed = false;
        hover(pos);
        return;
    }
    if (!dragging) {
        const double dist = std::abs(pos.x() - pressPos.x()) + std::abs(pos.y() - pressPos.y());
        if (dist < 5)
            return;
        dragging = true;
        clickWindow = false;   // a drag cancels the double click
    }
    ensureUpdated();
    switch (pressHit.area) {
    case Area::Pane: {
        if (pressHit.pane >= int(panes.size()))
            break;
        PaneImpl *pane = panes[pressHit.pane].get();
        const QPointF local(pos.x(), pos.y() - pane->top);
        // PaneWidget._pressedMouseMoveEvent: scroll once the mouse really moved
        if (!ts.isEmpty() && opts.handleScroll) {
            PriceScaleModel *h = pane->defaultScale();
            const double now = double(clock.nsecsElapsed()) / 1e6;
            if (!scrollAnchor)
                scrollAnchor = pos;
            if (!scrollStarted && *scrollAnchor != pos) {
                if (opts.kineticScrollMouse) {
                    const double bs = ts.barSpacing;
                    kinetic = KineticAnimation();
                    kinetic->minSpeed = 0.2 / bs;
                    kinetic->maxSpeed = 7 / bs;
                    kinetic->dumping = 0.997;
                    kinetic->minMove = 15 / bs;
                    kinetic->addPosition(ts.rightOffset, now);
                } else {
                    kinetic.reset();
                }
                if (!h->isEmpty() && !h->opts.autoScale)
                    h->startScroll(local.y());
                ts.startScroll(local.x());
                scrollStarted = true;
            }
            if (scrollStarted) {
                if (!h->isEmpty() && !h->opts.autoScale)
                    h->scrollTo(local.y());
                ts.scrollTo(local.x());
                if (kinetic)
                    kinetic->addPosition(ts.rightOffset, now);
                invalidateScales();
                dirty = true;
                ensureUpdated();
            }
        }
        const double x = std::max(0.0, std::min(local.x(), paneWidth - 1));
        const double y = std::max(0.0, std::min(local.y(), pane->height - 1));
        setCrosshair(x, y, pane, true);
        request();
        break;
    }
    case Area::PriceAxis: {
        if (!opts.handleScale || pressHit.pane >= int(panes.size()))
            break;
        PaneImpl *pane = panes[pressHit.pane].get();
        pane->right->scaleTo(pos.y() - pane->top);
        refreshCrosshair();
        request();
        scheduleCrosshairEmit();
        break;
    }
    case Area::TimeAxis:
        if (!opts.handleScale || ts.isEmpty())
            break;
        ts.scaleTo(pos.x());
        invalidateScales();
        request();
        scheduleCrosshairEmit();
        break;
    case Area::Separator:
        if (dragSeparator >= 0 && dragSeparator + 1 < int(panes.size())) {
            const double n = (pos.y() - sepStartY) * sepPerPx;
            const double s = std::clamp(sepStartStretch + n, sepMin, sepTotal - sepMin);
            panes[dragSeparator]->stretch = s;
            panes[dragSeparator + 1]->stretch = sepTotal - s;
            fullLayoutNeeded = true;
            invalidateScales();
            request();
        }
        break;
    default:
        break;
    }
}

void ChartPrivate::release(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton || !pressed)
        return;
    pressed = false;
    const QPointF pos = e->position();
    switch (pressHit.area) {
    case Area::Pane:
        if (scrollStarted && pressHit.pane < int(panes.size())) {
            PriceScaleModel *h = panes[pressHit.pane]->defaultScale();
            if (!h->opts.autoScale)
                h->endScroll();
            ts.endScroll();
            if (kinetic) {
                const double now = double(clock.nsecsElapsed()) / 1e6;
                kinetic->finish(ts.rightOffset, now);
                if (!kinetic->finished(now)) {
                    KineticAnimation k = *kinetic;
                    startAnimation([this, k](double t) {
                        if (k.finished(t))
                            return true;
                        ts.setRightOffset(k.positionAt(t));
                        return false;
                    });
                } else {
                    kinetic.reset();
                }
            }
        }
        scrollStarted = false;
        scrollAnchor.reset();
        break;
    case Area::PriceAxis:
        if (pressHit.pane < int(panes.size()))
            panes[pressHit.pane]->right->endScale();
        priceScaleDragged = false;
        break;
    case Area::TimeAxis:
        ts.endScale();
        break;
    case Area::Separator:
        dragSeparator = -1;
        break;
    default:
        break;
    }
    // clicks and double clicks (MouseEventHandler._mouseUpHandler)
    ++clickCount;
    if (clickWindow && clickTimer.elapsed() <= 500 && clickCount > 1) {
        const double dist =
            std::abs(pos.x() - firstPressPos.x()) + std::abs(pos.y() - firstPressPos.y());
        if (dist < 5 && !dragging)
            doubleClick(pressHit);
        clickWindow = false;
        clickCount = 0;
    } else if (!dragging) {
        click(pressHit);
        Q_UNUSED(pos);
    }
    if (dragging && !clickWindow)
        clickCount = 0;
    dragging = false;
    invalidateScales();
    request();
    hover(pos);
}

void ChartPrivate::click(const Hit &hit)
{
    if (hit.area != Area::Pane || hit.pane >= int(panes.size()))
        return;
    PaneImpl *pane = panes[hit.pane].get();
    // the click is reported where the button went up, relative to the pressed pane
    const QPointF local(lastRelease.x(), lastRelease.y() - pane->top);
    const double x = std::max(0.0, std::min(local.x(), paneWidth - 1));
    const double y = std::max(0.0, std::min(local.y(), pane->height - 1));
    setCrosshair(x, y, pane, false);
    std::optional<int> logical;
    if (!ts.isEmpty())
        logical = ts.coordinateToIndex(local.x());
    emit q->clicked(params(logical, local, hit.pane));
}

void ChartPrivate::doubleClick(const Hit &hit)
{
    switch (hit.area) {
    case Area::PriceAxis:
        if (opts.handleScale && hit.pane < int(panes.size())) {
            PriceScaleModel *sc = panes[hit.pane]->right.get();
            sc->opts.autoScale = true;
            sc->rangeValid = false;
            invalidateScales();
            request();
        }
        break;
    case Area::TimeAxis:
        if (opts.handleScale) {
            stopAnimation();
            ts.reset();
            invalidateScales();
            request();
            scheduleCrosshairEmit();
        }
        break;
    default:
        break;
    }
}

void ChartPrivate::wheel(QWheelEvent *e)
{
    double dx, dy;
    if (!e->pixelDelta().isNull()) {
        dx = -e->pixelDelta().x();
        dy = -e->pixelDelta().y();
    } else {
        dx = -e->angleDelta().x() / 120.0 * 100;
        dy = -e->angleDelta().y() / 120.0 * 100;
    }
    if (!((dx != 0 && opts.handleScroll) || (dy != 0 && opts.handleScale))) {
        e->ignore();
        return;
    }
    e->accept();
    ensureUpdated();
    const double n = dx / 100;
    const double s = -dy / 100;
    bool changed = false;
    if (s != 0 && opts.handleScale && !ts.isEmpty()) {
        stopAnimation();
        const double scale = (s > 0 ? 1 : -1) * std::min(1.0, std::abs(s));
        double x = e->position().x();
        x = std::max(1.0, std::min(x, ts.width));
        ts.zoom(x, scale);
        changed = true;
    }
    if (n != 0 && opts.handleScroll && !ts.isEmpty()) {
        stopAnimation();
        ts.startScroll(0);
        ts.scrollTo(-80 * n);
        ts.endScroll();
        changed = true;
    }
    if (changed) {
        invalidateScales();
        request();
        ensureUpdated();
        if (ch.visible)
            emit q->crosshairMoved(params(ch.index, QPointF(ch.rawX, ch.rawY), paneIndexOf(ch.pane)));
    }
}

} // namespace lw::detail

namespace lw {

// ================================================================ Chart

Chart::Chart(const ChartOptions &options, QWidget *parent) : QWidget(parent), d(new ChartPrivate(this))
{
    d->opts = options;
    d->rightDefaults.borderVisible = options.rightPriceScaleBorderVisible;
    d->rightDefaults.borderColor = options.rightPriceScaleBorderColor;
    d->ts.barSpacing = options.barSpacing;
    d->ts.rightOffset = options.rightOffset;
    PaneImpl *p = d->createPane();
    p->stretch = 2;   // the default pane (ChartModel constructor)
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent);
    d->animTimer.setInterval(16);
    connect(&d->animTimer, &QTimer::timeout, this, [this] { d->animationTick(); });
}

Chart::~Chart()
{
    for (auto &s : d->series)
        for (Primitive *prim : s->primitives)
            prim->detached();
}

void Chart::applyOptions(const ChartOptions &options)
{
    const ChartOptions old = d->opts;
    d->opts = options;
    if (options.barSpacing != old.barSpacing) {
        const double v = options.barSpacing;
        d->pendingOps.push_back([dd = d.get(), v] { dd->ts.setBarSpacing(v); });
    }
    if (options.rightOffset != old.rightOffset) {
        const double v = options.rightOffset;
        d->pendingOps.push_back([dd = d.get(), v] { dd->ts.setRightOffset(v); });
    }
    d->rightDefaults.borderVisible = options.rightPriceScaleBorderVisible;
    d->rightDefaults.borderColor = options.rightPriceScaleBorderColor;
    for (auto &p : d->panes) {
        p->right->opts.borderVisible = options.rightPriceScaleBorderVisible;
        p->right->opts.borderColor = options.rightPriceScaleBorderColor;
        for (auto *sc : p->allScales())
            sc->updateFormatter();
    }
    if (options.crosshairMode == CrosshairMode::Hidden)
        d->ch.visible = false;
    d->invalidateAll();
    d->request();
}

ChartOptions Chart::options() const
{
    return d->opts;
}

Series *Chart::addSeries(SeriesType type, const SeriesOptions &options, int paneIndex)
{
    auto s = std::make_unique<SeriesImpl>();
    s->chart = d.get();
    s->stype = type;
    s->opts = ChartPrivate::resolveOptions(options, type);
    s->formatter = NumberFormatter::fromFormat(s->opts.priceFormat);
    PaneImpl *pane = d->paneAt(paneIndex);
    SeriesImpl *raw = s.get();
    d->series.push_back(std::move(s));
    pane->series.push_back(raw);
    raw->order = pane->series.size() - 1;
    d->attachToScale(raw, pane, raw->opts.priceScaleId);
    if (d->series.size() == 1)
        d->fullLayoutNeeded = true;
    d->invalidateScales();
    d->request();
    return raw;
}

void Chart::removeSeries(Series *series)
{
    if (!series)
        return;
    for (auto &s : d->series)
        if (s.get() == series) {
            d->removeSeriesInternal(impl(series), true);
            return;
        }
}

QVector<Pane *> Chart::panes() const
{
    QVector<Pane *> v;
    for (auto &p : d->panes)
        v.push_back(p.get());
    return v;
}

void Chart::removePane(int index)
{
    if (d->panes.size() == 1 || index < 0 || index >= int(d->panes.size()))
        return;
    PaneImpl *pane = d->panes[index].get();
    const QVector<SeriesImpl *> list = pane->series;
    for (SeriesImpl *s : list)
        d->removeSeriesInternal(s, false);
    d->removePaneObject(pane);
    d->invalidateAll();
    d->request();
}

TimeScale *Chart::timeScale()
{
    return &d->tsHandle;
}

PriceScale *Chart::priceScale(const QString &id, int paneIndex)
{
    auto key = std::make_pair(id, paneIndex);
    auto it = d->scaleHandles.find(key);
    if (it == d->scaleHandles.end()) {
        auto h = std::make_unique<PriceScaleHandle>();
        h->chart = d.get();
        h->id = id;
        h->paneIndex = paneIndex;
        it = d->scaleHandles.emplace(key, std::move(h)).first;
    }
    return it->second.get();
}

void Chart::setCrosshairPosition(double price, double time, Series *series)
{
    SeriesImpl *s = impl(series);
    if (!s || !s->pane)
        return;
    d->ensureUpdated();
    PaneImpl *pane = s->pane;
    PriceScaleModel *sc = pane->defaultScale();
    const auto fv = sc->firstValue();
    const auto idx = d->ts.timeToIndex(time, true);
    if (!fv || !idx)
        return;
    const double y = sc->priceToCoordinate(price, *fv);
    const double x = d->ts.coordOf(*idx);
    d->setCrosshair(x, y, pane, false);
    d->request();
}

void Chart::clearCrosshairPosition()
{
    d->clearCrosshair(false);
    d->request();
}

QImage Chart::takeScreenshot()
{
    const bool wasResized = testAttribute(Qt::WA_Resized);
    if (!wasResized && !isVisible())
        setAttribute(Qt::WA_Resized, true);   // lets queued time scale operations run
    d->ensureUpdated();
    const double dpr = d->effectiveDpr();
    const int w = int(jsRound(d->mediaW * dpr)), h = int(jsRound(d->mediaH * dpr));
    QImage img(std::max(1, w), std::max(1, h), QImage::Format_ARGB32_Premultiplied);
    img.fill(d->opts.background);
    {
        QPainter p(&img);
        d->hideCrosshairForShot = true;
        d->render(p, dpr);
        d->hideCrosshairForShot = false;
    }
    img.setDevicePixelRatio(dpr);
    if (!wasResized && !isVisible())
        setAttribute(Qt::WA_Resized, false);
    return img;
}

void Chart::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    // the device scale of the target (backing store, or an image when render()ed)
    const double dpr = p.deviceTransform().m11() > 0 ? p.deviceTransform().m11() : devicePixelRatioF();
    p.setWorldTransform(QTransform::fromScale(1 / dpr, 1 / dpr));
    d->paintDpr = dpr;
    d->render(p, dpr);
    d->paintDpr = 0;
    const auto r = timeScale()->getVisibleLogicalRange();
    const std::optional<std::pair<double, double>> now = r ? std::optional(std::pair(r->from, r->to)) : std::nullopt;
    if (now != d->emittedRange) {
        d->emittedRange = now;
        emit visibleLogicalRangeChanged();
    }
}

void Chart::resizeEvent(QResizeEvent *)
{
    d->fullLayoutNeeded = true;
    d->invalidateScales();
    d->request();
}

void Chart::showEvent(QShowEvent *)
{
    d->request();
}

void Chart::mousePressEvent(QMouseEvent *e)
{
    d->lastRelease = e->position();
    d->press(e);
}

void Chart::mouseDoubleClickEvent(QMouseEvent *e)
{
    // Qt sends this instead of the second press; the click logic counts presses itself
    d->lastRelease = e->position();
    d->press(e);
}

void Chart::mouseMoveEvent(QMouseEvent *e)
{
    d->move(e);
}

void Chart::mouseReleaseEvent(QMouseEvent *e)
{
    d->lastRelease = e->position();
    d->release(e);
}

void Chart::wheelEvent(QWheelEvent *e)
{
    d->wheel(e);
}

void Chart::leaveEvent(QEvent *)
{
    d->leave();
}

void Chart::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::DevicePixelRatioChange || e->type() == QEvent::ScreenChangeInternal) {
        d->fullLayoutNeeded = true;
        d->request();
    }
    QWidget::changeEvent(e);
}

// ================================================================ Series

SeriesType Series::type() const { return impl(this)->stype; }

void Series::setData(const QVector<DataItem> &data)
{
    SeriesImpl *s = impl(this);
    s->chart->setSeriesData(s, data);
    s->chart->request();
    s->chart->scheduleCrosshairEmit();
}

void Series::update(const DataItem &item)
{
    SeriesImpl *s = impl(this);
    s->chart->updateSeriesItem(s, item);
    s->chart->request();
    s->chart->scheduleCrosshairEmit();
}

QVector<DataItem> Series::data() const
{
    QVector<DataItem> out;
    for (const auto &it : impl(this)->items)
        if (!it.whitespace())
            out.push_back(it);
    return out;
}

void Series::setLastValueDetail(const QString &text)
{
    SeriesImpl *s = impl(this);
    s->opts.lastValueDetail = text;
    s->chart->request();
}

void Series::applyOptions(const SeriesOptions &options)
{
    SeriesImpl *s = impl(this);
    ChartPrivate *d = s->chart;
    const SeriesOptions o = ChartPrivate::resolveOptions(options, s->stype);
    const bool scaleChanged = o.priceScaleId != s->opts.priceScaleId;
    const PriceFormat oldFmt = s->opts.priceFormat;
    s->opts = o;
    if (oldFmt.type != o.priceFormat.type || oldFmt.precision != o.priceFormat.precision
        || oldFmt.minMove != o.priceFormat.minMove)
        s->formatter = NumberFormatter::fromFormat(o.priceFormat);
    if (scaleChanged && s->pane) {
        PaneImpl *pane = s->pane;
        d->detachFromScale(s);
        d->attachToScale(s, pane, o.priceScaleId);
    }
    if (s->scale)
        s->scale->updateFormatter();
    s->markersDirty = true;
    d->invalidateScales();
    d->request();
}

SeriesOptions Series::options() const { return impl(this)->opts; }

PriceLine *Series::createPriceLine(const PriceLineOptions &options)
{
    SeriesImpl *s = impl(this);
    auto pl = std::make_unique<PriceLineImpl>();
    pl->series = s;
    pl->opts = options;
    PriceLine *raw = pl.get();
    s->priceLines.push_back(std::move(pl));
    s->chart->invalidateScales();
    s->chart->request();
    return raw;
}

void Series::removePriceLine(PriceLine *line)
{
    SeriesImpl *s = impl(this);
    for (auto it = s->priceLines.begin(); it != s->priceLines.end(); ++it)
        if (it->get() == line) {
            s->priceLines.erase(it);
            break;
        }
    s->chart->invalidateScales();
    s->chart->request();
}

std::optional<double> Series::priceToCoordinate(double price) const
{
    const SeriesImpl *s = impl(this);
    s->chart->ensureUpdated();
    const auto fv = s->firstValue();
    if (!fv || !s->scale)
        return std::nullopt;
    return s->scale->priceToCoordinate(price, fv->first);
}

std::optional<double> Series::coordinateToPrice(double y) const
{
    const SeriesImpl *s = impl(this);
    s->chart->ensureUpdated();
    const auto fv = s->firstValue();
    if (!fv || !s->scale)
        return std::nullopt;
    return s->scale->coordinateToPrice(y, fv->first);
}

void Series::attachPrimitive(Primitive *primitive)
{
    SeriesImpl *s = impl(this);
    if (!primitive || s->primitives.contains(primitive))
        return;
    s->primitives.push_back(primitive);
    ChartPrivate *d = s->chart;
    QPointer<Chart> chart(d->q);
    primitive->attached(this, [chart] {
        if (chart)
            chart->update();
    });
    d->request();
}

void Series::detachPrimitive(Primitive *primitive)
{
    SeriesImpl *s = impl(this);
    if (s->primitives.removeAll(primitive) > 0)
        primitive->detached();
    s->chart->request();
}

void Series::setMarkers(const QVector<SeriesMarker> &markers)
{
    SeriesImpl *s = impl(this);
    s->markers = markers;
    s->markersDirty = true;
    s->chart->invalidateScales();
    s->chart->request();
}

int Series::paneIndex() const
{
    const SeriesImpl *s = impl(this);
    return s->chart->paneIndexOf(s->pane);
}

// ================================================================ PriceLine

void PriceLine::applyOptions(const PriceLineOptions &options)
{
    auto *pl = static_cast<PriceLineImpl *>(this);
    pl->opts = options;
    pl->series->chart->invalidateScales();
    pl->series->chart->request();
}

PriceLineOptions PriceLine::options() const
{
    return static_cast<const PriceLineImpl *>(this)->opts;
}

// ================================================================ PriceScale

void PriceScale::applyOptions(const PriceScaleOptions &options)
{
    auto *h = static_cast<PriceScaleHandle *>(this);
    ChartPrivate *d = h->chart;
    if (h->paneIndex < 0 || h->paneIndex >= int(d->panes.size()))
        return;
    PaneImpl *pane = d->panes[h->paneIndex].get();
    if (h->id == QLatin1String("right")) {
        // JS merges these into chart options: later panes get them too
        d->rightDefaults = options;
        d->opts.rightPriceScaleBorderVisible = options.borderVisible;
        d->opts.rightPriceScaleBorderColor = options.borderColor;
        pane->right->applyOptions(options);
        d->fullLayoutNeeded = true;
    } else if (PriceScaleModel *sc = pane->scaleById(h->id)) {
        sc->applyOptions(options);
    }
    d->invalidateScales();
    d->request();
}

PriceScaleOptions PriceScale::options() const
{
    auto *h = static_cast<const PriceScaleHandle *>(this);
    ChartPrivate *d = h->chart;
    if (h->paneIndex >= 0 && h->paneIndex < int(d->panes.size()))
        if (PriceScaleModel *sc = d->panes[h->paneIndex]->scaleById(h->id))
            return sc->opts;
    return h->id == QLatin1String("right") ? d->rightDefaults : d->overlayDefaults;
}

// ================================================================ TimeScale

std::optional<LogicalRange> TimeScale::getVisibleLogicalRange() const
{
    ChartPrivate *d = static_cast<const TimeScaleHandle *>(this)->chart;
    d->ensureUpdated();
    double f, t;
    if (!d->ts.logicalRange(f, t))
        return std::nullopt;
    return LogicalRange{f, t};
}

void TimeScale::setVisibleLogicalRange(const LogicalRange &range)
{
    ChartPrivate *d = static_cast<TimeScaleHandle *>(this)->chart;
    if (range.from > range.to)
        return;   // JS asserts "The from index cannot be after the to index."
    d->stopAnimation();
    d->pendingOps.clear();
    d->queueTimeOp([d, range] {
        d->ts.setLogicalRange(range.from, range.to, false);
        d->invalidateScales();
    });
    d->scheduleCrosshairEmit();
}

void TimeScale::fitContent()
{
    ChartPrivate *d = static_cast<TimeScaleHandle *>(this)->chart;
    d->stopAnimation();
    d->pendingOps.clear();
    d->queueTimeOp([d] {
        d->ts.fitContent();
        d->invalidateScales();
    });
    d->scheduleCrosshairEmit();
}

void TimeScale::scrollToRealTime()
{
    ChartPrivate *d = static_cast<TimeScaleHandle *>(this)->chart;
    d->queueTimeOp([d] {
        const double from = d->ts.rightOffset;
        const double target = d->opts.rightOffset;
        const double start = double(d->clock.nsecsElapsed()) / 1e6;
        d->startAnimation([d, from, target, start](double now) {
            const double r = (now - start) / 400;
            d->ts.setRightOffset(r >= 1 ? target : from + (target - from) * r);
            return r >= 1;
        });
    });
}

std::optional<double> TimeScale::logicalToCoordinate(double logical) const
{
    ChartPrivate *d = static_cast<const TimeScaleHandle *>(this)->chart;
    d->ensureUpdated();
    if (d->ts.isEmpty())
        return std::nullopt;
    return d->ts.indexToCoordinate(logical);
}

std::optional<double> TimeScale::coordinateToLogical(double x) const
{
    ChartPrivate *d = static_cast<const TimeScaleHandle *>(this)->chart;
    d->ensureUpdated();
    if (d->ts.isEmpty())
        return std::nullopt;
    return d->ts.coordinateToIndex(x);
}

int TimeScale::pointCount() const
{
    ChartPrivate *d = static_cast<const TimeScaleHandle *>(this)->chart;
    d->ensureUpdated();
    return int(d->ts.points.size());
}

std::optional<int> TimeScale::timeToLogical(double time) const
{
    ChartPrivate *d = static_cast<const TimeScaleHandle *>(this)->chart;
    d->ensureUpdated();
    const auto &pts = d->ts.points;
    const auto it = std::lower_bound(pts.begin(), pts.end(), time, [](const TimePoint &p, double t) { return p.time < t; });
    if (it == pts.end() || it->time != time) return std::nullopt;
    return int(it - pts.begin());
}

std::optional<double> TimeScale::pointTime(int logical) const
{
    ChartPrivate *d = static_cast<const TimeScaleHandle *>(this)->chart;
    d->ensureUpdated();
    if (logical < 0 || logical >= d->ts.points.size()) return std::nullopt;
    return d->ts.points[logical].time;
}

double TimeScale::width() const
{
    ChartPrivate *d = static_cast<const TimeScaleHandle *>(this)->chart;
    d->ensureUpdated();
    return d->paneWidth;
}

// ================================================================ Pane

int Pane::index() const
{
    auto *p = static_cast<const PaneImpl *>(this);
    return p->chart->paneIndexOf(p);
}

double Pane::getHeight() const
{
    auto *p = static_cast<const PaneImpl *>(this);
    p->chart->ensureUpdated();
    return p->height;
}

void Pane::setStretchFactor(double factor)
{
    auto *p = static_cast<PaneImpl *>(this);
    p->stretch = factor;
    p->chart->fullLayoutNeeded = true;
    p->chart->invalidateScales();
    p->chart->request();
}

double Pane::stretchFactor() const
{
    return static_cast<const PaneImpl *>(this)->stretch;
}

} // namespace lw
