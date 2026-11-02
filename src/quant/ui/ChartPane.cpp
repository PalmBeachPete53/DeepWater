// One chart: candles + volume + any number of indicators, drawings, compare symbols, replay and live data.
#include "ChartPane.h"

#include "Api.h"
#include "Drawings.h"
#include "Legend.h"
#include "Text.h"
#include "Theme.h"
#include "VolumeProfile.h"
#include "TradeFeed.h"
#include "FootprintLayer.h"
#include "BigTradesLayer.h"
#include "ChartNavigator.h"
#include "DepthFeed.h"
#include "HeatmapLayer.h"
#include "Widgets.h"

#include "../chart/Chart.h"
#include "../core/Error.h"
#include "../core/JsCompat.h"
#include "../core/Series.h"

#include <QCheckBox>
#include <QDateTime>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QLocale>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QStyle>
#include <QStyleOptionButton>
#include <QTimeZone>
#include <QTimer>
#include <QWebSocket>

#include <algorithm>
#include <cmath>

namespace qtm {

const QVector<QPair<QString, QString>> CHART_TYPES = {
    {"candles", "Candles"},
    {"hollow", "Hollow candles"},
    {"bars", "Bars"},
    {"heikin", "Heikin Ashi"},
    {"line", "Line"},
    {"linemarkers", "Line with markers"},
    {"step", "Step line"},
    {"area", "Area"},
    {"baseline", "Baseline"},
    {"columns", "Columns"},
    {"renko", "Renko"},
    {"linebreak", "Line break"},
    {"range", "Range bars"},
    {"kagi", "Kagi"},
    {"pnf", "Point & figure"},
    {"footprint", "Footprint"},   // Deepwater addition: order flow from Binance trades (FootprintLayer)
};

namespace {

// Chart types built from price alone, with no meaningful volume per bar.
const QStringList PRICE_ONLY = {"renko", "linebreak", "kagi", "pnf"};
const QHash<QString, QString> BINANCE_WS_TF = {
    {"1s", "1s"}, {"1m", "1m"}, {"3m", "3m"}, {"5m", "5m"}, {"15m", "15m"}, {"30m", "30m"},
    {"1h", "1h"}, {"2h", "2h"}, {"4h", "4h"}, {"6h", "6h"}, {"8h", "8h"}, {"12h", "12h"},
    {"1D", "1d"}, {"3D", "3d"}, {"1W", "1w"}, {"1M", "1M"},
};

QColor css(const QString &name) { return theme::color(name); }
// A CSS colour, or a theme variable ("--up") so marker colours follow theme changes.
QColor col(const QString &c) { return c.startsWith("--") ? theme::color(c) : theme::parseColor(c); }

// A chart line can't skip a blank point: it joins the neighbours. A segment takes the colour of
// the point it starts from, so hiding a gap means making the point before it transparent and
// parking the gap's points (invisibly) at the next real value.
const QColor CLEAR(0, 0, 0, 0);

QVector<lw::DataItem> lineData(const Values &vals, const QVector<bool> &breaks,
                               const std::function<double(int)> &timeAt, int n)
{
    QVector<lw::DataItem> out;
    QVector<double> next(vals.size());
    double nv = NaN;
    for (int i = vals.size() - 1; i >= 0; --i) {
        if (std::isfinite(vals[i])) nv = vals[i];
        next[i] = nv;
    }
    bool started = false;
    for (int i = 0; i < vals.size(); ++i) {
        const double v = vals[i];
        lw::DataItem d;
        d.time = timeAt(i);
        if (!std::isfinite(v)) {
            if (i >= n) continue;
            if (started && std::isfinite(next[i])) {
                d.value = next[i];
                d.color = CLEAR;
            }
            out.push_back(d);
            continue;
        }
        started = true;
        const bool gapAhead = i + 1 < vals.size()
            && (!std::isfinite(vals[i + 1]) || (i + 1 < breaks.size() && breaks[i + 1]));
        d.value = v;
        if (gapAhead) d.color = CLEAR;
        out.push_back(d);
    }
    return out;
}

QVector<lw::DataItem> histData(const Values &vals, const QStringList &colors,
                               const std::function<double(int)> &timeAt, int n)
{
    QVector<lw::DataItem> out;
    for (int i = 0; i < vals.size(); ++i) {
        const double v = vals[i];
        if (i >= n && !std::isfinite(v)) continue;
        lw::DataItem d;
        d.time = timeAt(i);
        if (std::isfinite(v)) {
            d.value = v;
            if (!colors.isEmpty() && i < colors.size() && !colors[i].isEmpty())
                d.color = col(colors[i]);
        }
        out.push_back(d);
    }
    return out;
}

// Higher-timeframe values onto chart bars. A chart bar sees a higher-timeframe bar only once that
// bar has finished, except inside the newest one, where it shows the still-forming value (as
// TradingView does in real time).
IndicatorResult mapToChart(const IndicatorResult &res, const Bars &hb, const QString &htf, const Bars &cb,
                           const QString &ctf)
{
    const double hSec = parseTf(htf).seconds, cSec = parseTf(ctf).seconds;
    auto end = [&](int j) { return j + 1 < hb.size() ? hb[j + 1].time : hb[j].time + hSec; };
    QVector<int> idx(cb.size(), -1);
    int j = -1;
    for (int i = 0; i < cb.size(); ++i) {
        const double cEnd = cb[i].time + cSec;
        while (j + 1 < hb.size() && end(j + 1) <= cEnd) j++;
        idx[i] = j + 1 == hb.size() - 1 && cb[i].time >= hb.last().time ? hb.size() - 1 : j;
    }
    IndicatorResult out;
    for (auto it = res.values.begin(); it != res.values.end(); ++it) {
        const Values &v = it.value();
        if (v.size() >= hb.size()) {
            Values m(idx.size());
            for (int i = 0; i < idx.size(); ++i) m[i] = idx[i] >= 0 ? v[idx[i]] : NaN;
            out.values.insert(it.key(), m);
        } else {
            out.values.insert(it.key(), v);
        }
    }
    if (!res.colors.isEmpty()) {
        if (res.colors.size() >= hb.size()) {
            for (int x : idx) out.colors << (x >= 0 ? res.colors[x] : QString());
        } else {
            out.colors = res.colors;
        }
    }
    if (!res.breaks.isEmpty()) {
        if (res.breaks.size() >= hb.size()) {
            for (int x : idx) out.breaks << (x >= 0 ? res.breaks[x] : false);
        } else {
            out.breaks = res.breaks;
        }
    }
    out.extra = res.extra;
    return out;
}

lw::SeriesMarker toLw(const IndicatorMarker &m, double time)
{
    lw::SeriesMarker s;
    s.time = time;
    s.position = m.position == "belowBar" ? lw::SeriesMarker::BelowBar
        : m.position == "inBar"           ? lw::SeriesMarker::InBar
                                          : lw::SeriesMarker::AboveBar;
    s.shape = m.shape == "arrowDown" ? lw::SeriesMarker::ArrowDown
        : m.shape == "circle"        ? lw::SeriesMarker::Circle
        : m.shape == "square"        ? lw::SeriesMarker::Square
                                     : lw::SeriesMarker::ArrowUp;
    s.color = col(m.color);
    s.text = m.text;
    return s;
}

// The short name shown in the legend: "Relative Strength Index (RSI)" -> "RSI".
QString shortName(const QString &name)
{
    static const QRegularExpression re(R"(\(([^)]+)\)$)");
    const auto m = re.match(name);
    if (m.hasMatch()) return m.captured(1).split(" / ").first();
    return name;
}

QString jsValueText(const QJsonValue &v)
{
    if (v.isDouble()) return js::numberToString(v.toDouble());
    if (v.isString()) return v.toString();
    if (v.isBool()) return v.toBool() ? "true" : "false";
    if (v.isNull()) return "null";
    return QString();
}

// Chart types whose series take a live bar as it is (the others are rebuilt from all the bars).
bool isSimpleType(const QString &type)
{
    static const QStringList simpleTypes = {"candles", "hollow", "bars", "line", "linemarkers", "step", "area", "baseline", "columns",
                                            "footprint"};
    return simpleTypes.contains(type);
}

} // namespace

static int g_uidSeq = 0;
QString newUid()
{
    return QString("i%1%2").arg(QString::number(QDateTime::currentMSecsSinceEpoch(), 36),
                                QString::number(g_uidSeq++, 36));
}

QString shortIndicatorName(const QString &name) { return shortName(name); }

// ---------------------------------------------------------------- the accent frame (.pane.active::after)

namespace {
class ActiveFrame : public QWidget
{
public:
    using QWidget::QWidget;
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setPen(Qt::NoPen);
        p.setBrush(css("--accent"));
        const int w = width(), h = height();
        p.drawRect(0, 0, w, 1);
        p.drawRect(0, h - 1, w, 1);
        p.drawRect(0, 0, 1, h);
        p.drawRect(w - 1, 0, 1, h);
    }
};
} // namespace

// ---------------------------------------------------------------- setup

ChartPane::ChartPane(App *app_, const PaneConfig &cfg_, QWidget *parent) : QWidget(parent), app(app_), cfg(cfg_)
{
    setAttribute(Qt::WA_StyledBackground);
    setObjectName("qtmPane");
    setMinimumSize(0, 0);
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    liveTimer = new QTimer(this);
    liveTimer->setSingleShot(true);
    replayTimer = new QTimer(this);
    btTimer = new QTimer(this);
    btTimer->setInterval(250);
    connect(btTimer, &QTimer::timeout, this, [this] {
        size_t before = 0, after = 0;
        for (const auto &m : std::as_const(btModels)) before += m->trades().size();
        syncBigTrades();
        bool automatic = false;
        for (const auto &m : std::as_const(btModels)) {
            after += m->trades().size();
            automatic = automatic || m->params().thr.automatic;
        }
        // the legend: when a big trade came out, and once a second for an automatic threshold moving with the market
        if (after != before || (automatic && ++btLegendTick % 4 == 0)) updateLegend(legendIndex);
    });
    connect(replayTimer, &QTimer::timeout, this, [this] { replayStep(1); });
    createChart();
    legend = new Legend(this);
    legendInput = new LegendInput(this, legend);
    msgEl = new QLabel(this);
    msgEl->setObjectName("qtmPaneMsg");
    msgEl->setAlignment(Qt::AlignCenter);
    msgEl->setWordWrap(true);
    msgEl->setTextFormat(Qt::RichText);
    msgEl->setContentsMargins(20, 20, 20, 20);
    msgEl->setAttribute(Qt::WA_TransparentForMouseEvents);
    msgEl->hide();
    frame = new ActiveFrame(this);
    frame->setAttribute(Qt::WA_TransparentForMouseEvents);
    frame->hide();
    layoutOverlays();
}

ChartPane::~ChartPane()
{
    destroy();
    // The chart calls detached() on its primitives when it goes, and they are members of this
    // pane: delete it while they still exist.
    delete chart;
    chart = nullptr;
    main = nullptr;
}

static lw::ChartOptions themed(lw::ChartOptions o)
{
    o.background = css("--bg");
    o.textColor = css("--text");
    o.fontSize = 11;
    o.separatorColor = css("--border");
    o.separatorHoverColor = css("--accent-soft");
    o.enablePaneResize = true;
    o.vertLinesColor = css("--grid");
    o.horzLinesColor = css("--grid");
    o.rightPriceScaleBorderColor = css("--border");
    o.timeScaleBorderColor = css("--border");
    o.crosshairLineColor = css("--crosshair");
    o.crosshairLabelBackgroundColor = css("--crosshair-label");
    // A host palette's light up / down colours need dark label text where LW would put white.
    o.labelTextColor = nullptr;
    if (theme::hasPalette()) o.labelTextColor = [](const QColor &bg) { return theme::textOn(bg); };
    return o;
}

void ChartPane::createChart()
{
    lw::ChartOptions o = themed(lw::ChartOptions());
    o.crosshairMode = lw::CrosshairMode::Normal;
    o.rightOffset = 8;
    o.timeVisible = true;
    o.secondsVisible = false;
    o.priceFormatter = [](double p) { return theme::fmtPrice(p); };
    chart = new lw::Chart(o, this);
    connect(chart, &lw::Chart::crosshairMoved, this, [this](const lw::MouseEventParams &p) { onCrosshair(p); });
    connect(chart, &lw::Chart::clicked, this, [this](const lw::MouseEventParams &p) {
        if (app->tool == "cursor") onClick(p);
    });
    // Lightweight Charts drops a second click that lands within 500ms of the first (it waits for a
    // double-click), so drawing tools read native clicks instead.
    chart->installEventFilter(this);
    navigator = new ChartNavigator(chart, [this]() -> const Bars & { return shown; }, [this](double t) { return toChart(t); },
                                   this);   // the range slider under the chart
    drawingLayer = std::make_unique<DrawingLayer>(this);
    drawings = drawingLayer.get();
    makeMainSeries();
}

bool ChartPane::eventFilter(QObject *o, QEvent *e)
{
    if (o == chart && e->type() == QEvent::MouseButtonRelease && app->tool != "cursor") {
        auto *me = static_cast<QMouseEvent *>(e);
        if (me->button() == Qt::LeftButton) {
            const double x = me->position().x(), y = me->position().y();
            const auto panes = chart->panes();
            const bool inPlot = x >= 0 && x <= chart->timeScale()->width() && y >= 0
                && !panes.isEmpty() && y <= panes[0]->getHeight();
            lw::MouseEventParams p;
            if (inPlot) {
                p.point = QPointF(x, y);
                p.logical = chart->timeScale()->coordinateToLogical(x);
            }
            p.paneIndex = 0;
            // Let the chart finish its own handling first, like the DOM click comes after mouseup.
            QTimer::singleShot(0, this, [this, p] { onClick(p); });
        }
    }
    return QWidget::eventFilter(o, e);
}

void ChartPane::applyTheme()
{
    chart->applyOptions(themed(chart->options()));
    if (main) {
        lw::SeriesOptions o = main->options();
        mainColors(o, mainType);
        main->applyOptions(o);
    }
    legend->update();
    frame->update();
    if (navigator) navigator->update();
    if (replayBar) {
        for (auto *w : replayBar->findChildren<QWidget *>()) w->update();
    }
    render(false);
}

// The theme colours of the main series for chart type `t` (accent line / area, up / down candles).
void ChartPane::mainColors(lw::SeriesOptions &o, const QString &t)
{
    const QColor up = css("--up"), down = css("--down"), accent = css("--accent");
    if (t == "bars") {
        o.upColor = up;
        o.downColor = down;
    } else if (t == "line" || t == "linemarkers" || t == "step" || t == "kagi") {
        o.color = accent;
    } else if (t == "area") {
        o.lineColor = accent;
        o.topColor = theme::color("--accent", 0.35);
        o.bottomColor = theme::color("--accent", 0.02);
    } else if (t == "baseline") {
        o.topLineColor = up;
        o.bottomLineColor = down;
        o.topFillColor1 = theme::color("--up", 0.25);
        o.topFillColor2 = theme::color("--up", 0.02);
        o.bottomFillColor1 = theme::color("--down", 0.02);
        o.bottomFillColor2 = theme::color("--down", 0.25);
    } else if (t == "hollow") {
        o.upColor = QColor(0, 0, 0, 0);
        o.downColor = down;
        o.borderUpColor = up;
        o.borderDownColor = down;
        o.wickUpColor = up;
        o.wickDownColor = down;
    } else if (t == "footprint") {
        // FootprintLayer draws the candles: the series stays for the price scale, the crosshair and the last value
        // (its label goes opaque, so it keeps the up / down colour).
        auto clear = [](QColor c) {
            c.setAlpha(0);
            return c;
        };
        o.upColor = o.wickUpColor = clear(up);
        o.downColor = o.wickDownColor = clear(down);
        o.priceLineColor = accent;
    } else if (t != "columns") {
        o.upColor = up;
        o.downColor = down;
        o.wickUpColor = up;
        o.wickDownColor = down;
    }
}

void ChartPane::makeMainSeries()
{
    for (auto &vp : vpPrims)
        if (main) main->detachPrimitive(vp.get());
    vpPrims.clear();
    if (main && fpLayer) main->detachPrimitive(fpLayer.get());
    if (main) {
        for (auto &l : btLayers) main->detachPrimitive(l.get());
        if (btTooltip) main->detachPrimitive(btTooltip.get());
        for (auto &l : hmLayers) {
            main->detachPrimitive(l->overlay());
            main->detachPrimitive(l->dom());
            main->detachPrimitive(l.get());
        }
    }
    btLayers.clear();
    hmLayers.clear();
    if (main) {
        main->detachPrimitive(drawings);
        chart->removeSeries(main);
        main = nullptr;
    }
    if (volume) {
        chart->removeSeries(volume);
        volume = nullptr;
    }
    const QString t = cfg.type;
    lw::SeriesOptions o;
    o.priceLineVisible = true;
    o.lastValueVisible = true;
    mainColors(o, t);
    if (t == "bars") {
        main = chart->addSeries(lw::SeriesType::Bar, o);
    } else if (t == "line" || t == "linemarkers" || t == "step" || t == "kagi") {
        o.lineWidth = 2;
        o.lineType = t == "step" || t == "kagi" ? lw::LineType::WithSteps : lw::LineType::Simple;
        o.pointMarkersVisible = t == "linemarkers";
        main = chart->addSeries(lw::SeriesType::Line, o);
    } else if (t == "area") {
        o.lineWidth = 2;
        main = chart->addSeries(lw::SeriesType::Area, o);
    } else if (t == "baseline") {
        main = chart->addSeries(lw::SeriesType::Baseline, o);
    } else if (t == "columns") {
        main = chart->addSeries(lw::SeriesType::Histogram, o);
    } else if (t == "hollow") {
        o.borderVisible = true;
        main = chart->addSeries(lw::SeriesType::Candlestick, o);
    } else {
        o.borderVisible = false;
        main = chart->addSeries(lw::SeriesType::Candlestick, o);
    }
    if (fpLayer) main->attachPrimitive(fpLayer.get());   // under the drawings
    main->attachPrimitive(drawings);
    main->setMarkers({});
    markersSet = false;
    mainType = t;
    if (!PRICE_ONLY.contains(t) && t != "footprint" && app->settings().volume) {
        lw::SeriesOptions v;
        v.priceScaleId = "vol";
        v.priceFormat.type = lw::PriceFormatType::Volume;
        v.lastValueVisible = false;
        v.priceLineVisible = false;
        volume = chart->addSeries(lw::SeriesType::Histogram, v);
        auto *ps = chart->priceScale("vol");
        auto po = ps->options();
        po.scaleMarginTop = 0.82;
        po.scaleMarginBottom = 0;
        ps->applyOptions(po);
    }
}

// ---------------------------------------------------------------- time helpers (drawings, legend, markers all go through these)

namespace {

// QDateTime::currentDateTime().offsetFromUtc(), which goes through mktime: tens of microseconds, asked for
// every bar of every redraw (a live Renko chart spent ~100 ms a tick in it). The offset is kept until the system
// zone's next transition (daylight saving), and a minute at most (the zone itself changing).
double localOffsetNow()
{
    static qint64 validUntil = 0;
    static double offset = 0;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now >= validUntil) {
        offset = QDateTime::currentDateTime().offsetFromUtc();
        qint64 next = now + 60 * 1000;
        const QTimeZone zone = QTimeZone::systemTimeZone();
        if (zone.hasTransitions()) {
            const QTimeZone::OffsetData t = zone.nextTransition(QDateTime::fromMSecsSinceEpoch(now, QTimeZone::UTC));
            if (t.atUtc.isValid())
                next = std::min(next, t.atUtc.toMSecsSinceEpoch());
        }
        validUntil = next;
    }
    return offset;
}

} // namespace

double ChartPane::shift() const
{
    // The timeframe's kind, kept with the timeframe it was worked out for (parsing it for every bar adds up).
    if (cfg.tf != shiftTf) {
        shiftIntraday = isIntraday(cfg.tf);   // may throw on a bad timeframe, as before: nothing kept then
        shiftTf = cfg.tf;
    }
    return shiftIntraday && !app->settings().utc ? localOffsetNow() : 0;
}

double ChartPane::step() const
{
    return times.size() > 1 ? times[times.size() - 1] - times[times.size() - 2] : double(parseTf(cfg.tf).seconds);
}

double ChartPane::logicalOf(double utc) const
{
    const QVector<double> &T = times;
    const double t = utc + shift();
    const int n = T.size();
    if (!n) return NaN;
    if (t <= T[0]) return (t - T[0]) / step();
    if (t >= T[n - 1]) return n - 1 + (t - T[n - 1]) / step();
    int lo = 0, hi = n - 1;
    while (hi - lo > 1) {
        const int m = (lo + hi) >> 1;
        if (T[m] <= t) lo = m;
        else hi = m;
    }
    return lo + (t - T[lo]) / (T[hi] - T[lo]);
}

double ChartPane::timeOfLogical(double l) const
{
    const QVector<double> &T = times;
    const int n = T.size();
    if (!n) return NaN;
    double t;
    if (l <= 0) t = T[0] + l * step();
    else if (l >= n - 1) t = T[n - 1] + (l - (n - 1)) * step();
    else {
        const int i = int(std::floor(l));
        t = T[i] + (l - i) * (T[i + 1] - T[i]);
    }
    return t - shift();
}

std::optional<double> ChartPane::timeToX(double utc) const
{
    const double l = logicalOf(utc);
    if (std::isnan(l)) return std::nullopt;
    return chart->timeScale()->logicalToCoordinate(l);
}

QString ChartPane::formatPrice(double v) const
{
    return theme::fmtPrice(v);
}

// ---------------------------------------------------------------- data

void ChartPane::load(bool keepView)
{
    const int token = ++loadToken;
    stopLive();
    showMsg(QString::fromUtf8("Loading %1…").arg(theme::ticker(cfg.symbol)));
    const int limit = app->settings().bars ? app->settings().bars : 3000;
    api::candles(this, cfg.symbol, cfg.tf, limit,
        [this, token, keepView](const CandleSet &r) {
            if (token != loadToken || destroyed) return;
            cfg.symbol = r.symbol;
            provider = r.provider;
            notes = r.notes;
            bars = r.candles;
            mtf.clear();
            replay.reset();
            replayTimer->stop();
            removeReplayBar();
            drawings->setItems(drawingsFromJson(app->getDrawings(r.symbol)));
            showMsg(bars.isEmpty() ? QString("No data for this symbol and timeframe.") : QString());
            // loadCompare
            compareData.clear();
            const QStringList syms = cfg.compare;
            auto finish = [this, keepView] {
                if (destroyed) return;
                try {
                    render(!keepView);
                } catch (const Error &e) {
                    bars.clear();
                    showMsg(QString("Couldn't load %1 on %2.\n%3").arg(theme::esc(cfg.symbol), theme::esc(cfg.tf), theme::esc(e.message())));
                    return;
                }
                startLive();
                app->onPaneLoaded(this);
            };
            if (syms.isEmpty()) {
                finish();
                return;
            }
            auto left = std::make_shared<int>(syms.size());
            const int limit = app->settings().bars ? app->settings().bars : 3000;
            for (const QString &sym : syms) {
                api::candles(this, sym, cfg.tf, limit,
                    [this, sym, left, finish](const CandleSet &c) {
                        compareData[sym] = c.candles;
                        if (--*left == 0) finish();
                    },
                    [this, sym, left, finish](const QString &) {
                        compareData[sym] = Bars();
                        if (--*left == 0) finish();
                    });
            }
        },
        [this, token](const QString &message) {
            if (token != loadToken || destroyed) return;
            bars.clear();
            render(true);
            showMsg(QString("Couldn't load %1 on %2.\n%3").arg(theme::esc(cfg.symbol), theme::esc(cfg.tf), theme::esc(message)));
        });
}

void ChartPane::showMsg(const QString &text)
{
    msgEl->setVisible(!text.isEmpty());
    // line-height 1.4 of 13px
    msgEl->setText(text.isEmpty() ? QString()
                                  : QString("<div style=\"-qt-line-height-type: fixed; line-height: 18px\">%1</div>")
                                        .arg(theme::esc(text).replace("\n", "<br>")));
    msgEl->raise();
    if (frame) frame->raise();
    if (replayBar) replayBar->raise();
}

Bars ChartPane::visibleBars() const
{
    const Bars raw = replay ? bars.mid(0, replay->index + 1) : bars;
    const QString &t = cfg.type;
    if (t == "heikin") return heikinAshi(raw);
    if (t == "renko") return renko(raw);
    if (t == "linebreak") return lineBreak(raw);
    if (t == "range") return rangeBars(raw);
    if (t == "kagi") return kagi(raw);
    if (t == "pnf") return pointFigure(raw);
    return raw;
}

static lw::DataItem pointFor(const ChartPane *pane, const QString &mainType, const Bar &b)
{
    lw::DataItem d;
    d.time = pane->toChart(b.time);
    if (mainType == "line" || mainType == "linemarkers" || mainType == "step" || mainType == "area"
        || mainType == "baseline") {
        d.value = b.close;
    } else if (mainType == "kagi") {
        d.value = b.close;
        if (!b.color.isEmpty()) d.color = col(b.color);
    } else if (mainType == "columns") {
        d.value = b.close;
        d.color = b.close >= b.open ? css("--up") : css("--down");
    } else {
        d.open = b.open;
        d.high = b.high;
        d.low = b.low;
        d.close = b.close;
    }
    return d;
}

static lw::DataItem volPoint(const ChartPane *pane, const Bar &b)
{
    lw::DataItem d;
    d.time = pane->toChart(b.time);
    d.value = js::orZero(b.volume);
    d.color = theme::color(b.close >= b.open ? "--up" : "--down", 0.45);
    return d;
}

// The chart's index of the last bar shown. The chart's indexes are every series' times: a compared symbol's can sit among
// the bars or before them (US minutes going back further than the pair's), so bar i isn't index i.
double ChartPane::lastBarLogical() const
{
    if (shown.isEmpty()) return -1;
    if (const auto L = chart->timeScale()->timeToLogical(toChart(shown.last().time))) return *L;
    return shown.size() - 1;
}

void ChartPane::render(bool resetView)
{
    const auto prevRange = chart->timeScale()->getVisibleLogicalRange();
    const bool atEnd = prevRange && !shown.isEmpty() && prevRange->to >= lastBarLogical() - 2;
    const bool wantVolume = !PRICE_ONLY.contains(cfg.type) && cfg.type != "footprint" && app->settings().volume;
    if (mainType != cfg.type || (volume != nullptr) != wantVolume)
        makeMainSeries();
    shown = visibleBars();
    times.clear();
    times.reserve(shown.size());
    for (const Bar &b : shown) times.push_back(toChart(b.time));
    double last = shown.isEmpty() ? 1 : shown.last().close;
    if (!js::truthy(last)) last = 1;
    const int precision = last >= 100 ? 2 : last >= 1 ? 4 : last >= 0.01 ? 6 : 8;
    lw::SeriesOptions mo = main->options();
    mo.priceFormat.type = lw::PriceFormatType::Price;
    mo.priceFormat.precision = precision;
    mo.priceFormat.minMove = 1 / std::pow(10.0, precision);
    if (cfg.type == "baseline" && !shown.isEmpty())
        mo.baseValue = shown[std::max(0, int(shown.size()) - 150)].close;
    main->applyOptions(mo);
    QVector<lw::DataItem> data;
    data.reserve(shown.size());
    for (const Bar &b : shown) data.push_back(pointFor(this, mainType, b));
    main->setData(data);
    if (volume) {
        QVector<lw::DataItem> vd;
        vd.reserve(shown.size());
        for (const Bar &b : shown) vd.push_back(volPoint(this, b));
        volume->setData(vd);
    }
    lw::ChartOptions co = chart->options();
    co.timeVisible = isIntraday(cfg.tf);
    co.secondsVisible = parseTf(cfg.tf).seconds < 60;
    chart->applyOptions(co);
    auto *ps = chart->priceScale("right");
    auto po = ps->options();
    po.mode = !cfg.compare.isEmpty() ? lw::PriceScaleMode::Percentage
        : cfg.log                    ? lw::PriceScaleMode::Logarithmic
                                     : lw::PriceScaleMode::Normal;
    // The footprint's delta / volume strip takes the bottom of the pane (Lightweight Charts' default margin is 0.1).
    po.scaleMarginBottom = cfg.type == "footprint" ? 0.14 : 0.1;
    ps->applyOptions(po);
    syncFootprint();
    renderCompare();
    renderIndicators();
    drawings->update();
    const int n = shown.size();
    const double lastIdx = lastBarLogical();   // n − 1 without a compared symbol
    if (resetView && n)
        chart->timeScale()->setVisibleLogicalRange({std::max(0.0, lastIdx - 159), lastIdx + 9});
    if (!resetView && n && (replay || atEnd)) {
        const double w = prevRange ? std::max(20.0, prevRange->to - prevRange->from) : 160;
        chart->timeScale()->setVisibleLogicalRange({lastIdx + 9 - w, lastIdx + 9});
    }
    updateLegend();
}

void ChartPane::renderCompare()
{
    for (auto &p : compareSeries) chart->removeSeries(p.second);
    compareSeries.clear();
    static const QStringList palette = {"#ff9800", "#e91e63", "#00bcd4", "#8bc34a", "#9c27b0"};
    for (int k = 0; k < cfg.compare.size(); ++k) {
        const QString sym = cfg.compare[k];
        lw::SeriesOptions o;
        o.color = QColor(palette[k % palette.size()]);
        o.lineWidth = 2;
        o.priceLineVisible = false;
        o.title = theme::ticker(sym);
        lw::Series *s = chart->addSeries(lw::SeriesType::Line, o);
        QVector<lw::DataItem> d;
        const double lastShown = shown.isEmpty() ? NaN : shown.last().time;
        for (const Bar &b : compareData.value(sym)) {
            if (replay && !(b.time <= lastShown)) continue;
            lw::DataItem it;
            it.time = toChart(b.time);
            it.value = b.close;
            d.push_back(it);
        }
        s->setData(d);
        compareSeries.push_back({sym, s});
    }
}

// Indicator values for one instance: on the chart's bars, on another indicator's output
// (source "ind:<uid>:<output>"), or on a higher timeframe mapped back without look-ahead.
IndicatorResult ChartPane::computeIndicator(const IndicatorInstance &ind, const Bars &bars_)
{
    const QString src = jsValueText(ind.params.value("source"));
    if (!ind.tf.isEmpty() && ind.tf != cfg.tf) {
        if (!mtf.contains(ind.tf)) {
            loadMtf(ind.tf);
            IndicatorResult r;
            r.pending = true;
            return r;
        }
        const Mtf &h = mtf[ind.tf];
        if (!h.error.isEmpty()) {
            IndicatorResult r;
            r.error = h.error;
            return r;
        }
        Bars hb = h.bars;
        if (replay && !bars_.isEmpty()) {
            // replay: only higher-timeframe bars that had finished by the replayed bar
            const double cEnd = bars_.last().time + parseTf(cfg.tf).seconds, hs = parseTf(ind.tf).seconds;
            Bars f;
            for (int j = 0; j < hb.size(); ++j)
                if ((j + 1 < hb.size() ? hb[j + 1].time : hb[j].time + hs) <= cEnd) f.push_back(hb[j]);
            hb = f;
        }
        if (hb.isEmpty()) return IndicatorResult();
        QJsonObject params = ind.params;
        if (src.startsWith("ind:")) params["source"] = "close";
        const IndicatorResult res = compute(ind.id, hb, params);
        return mapToChart(res, hb, ind.tf, bars_, cfg.tf);
    }
    if (src.startsWith("ind:")) {
        const QStringList parts = src.split(':');
        const QString uid = parts.value(1), key = parts.value(2);
        if (!indResults.contains(uid) || !indResults[uid].values.contains(key)) {
            IndicatorResult r;
            r.error = "Its source indicator is missing";
            return r;
        }
        const Values vals = indResults[uid].values[key];
        Bars synth = bars_;
        for (int i = 0; i < synth.size(); ++i) {
            const double v = i < vals.size() ? vals[i] : NaN;
            synth[i].open = synth[i].high = synth[i].low = synth[i].close = v;
        }
        QJsonObject params = ind.params;
        params["source"] = "close";
        return compute(ind.id, synth, params);
    }
    return compute(ind.id, bars_, ind.params);
}

void ChartPane::loadMtf(const QString &tf)
{
    if (mtfLoading.contains(tf)) return;
    mtfLoading.insert(tf);
    try {
        if (parseTf(tf).seconds < parseTf(cfg.tf).seconds)
            throw Error(QString("Pick a timeframe at or above the chart's (%1)").arg(cfg.tf));
    } catch (const Error &e) {
        mtf[tf] = Mtf{Bars(), e.message(), double(QDateTime::currentMSecsSinceEpoch())};
        mtfLoading.remove(tf);
        // JS re-renders right away (inside this render); do it once this render is done.
        QTimer::singleShot(0, this, [this] { if (!destroyed) render(false); });
        return;
    }
    const QString sym = cfg.symbol;
    api::candles(this, sym, tf, 2000,
        [this, tf](const CandleSet &r) {
            mtf[tf] = Mtf{r.candles, QString(), double(QDateTime::currentMSecsSinceEpoch())};
            mtfLoading.remove(tf);
            if (!destroyed) render(false);
        },
        [this, tf](const QString &message) {
            mtf[tf] = Mtf{Bars(), message, double(QDateTime::currentMSecsSinceEpoch())};
            mtfLoading.remove(tf);
            if (!destroyed) render(false);
        });
}

void ChartPane::renderIndicators()
{
    // Nothing to draw and nothing drawn: a bare chart redid all of this at every live tick, and setting no
    // markers again asked for a repaint.
    if (cfg.indicators.isEmpty() && extraMarkers.isEmpty() && indSeries.isEmpty() && vpPrims.empty() && btLayers.empty()
        && btModels.isEmpty() && hmLayers.empty() && hmTextures.isEmpty() && chart->panes().size() == 1 && !markersSet) {
        indResults.clear();
        return;
    }
    for (auto it = indSeries.begin(); it != indSeries.end(); ++it)
        for (lw::Series *s : it.value()) chart->removeSeries(s);
    indSeries.clear();
    for (auto &vp : vpPrims) main->detachPrimitive(vp.get());
    vpPrims.clear();
    for (auto &l : btLayers) main->detachPrimitive(l.get());
    btLayers.clear();
    if (btTooltip) main->detachPrimitive(btTooltip.get());
    for (auto &l : hmLayers) {
        main->detachPrimitive(l->overlay());
        main->detachPrimitive(l->dom());
        main->detachPrimitive(l.get());
    }
    hmLayers.clear();
    QSet<QString> btLive, hmLive;
    while (chart->panes().size() > 1) chart->removePane(chart->panes().size() - 1);
    indResults.clear();
    const Bars &bs = shown;
    const int n = bs.size();
    auto timeAt = [this, n](int i) { return i < n ? times[i] : times[n - 1] + (i - n + 1) * step(); };
    // indicators built on another indicator's output go after their source
    QHash<QString, const IndicatorInstance *> byUid;
    for (const auto &x : cfg.indicators) byUid.insert(x.uid, &x);
    std::function<int(const IndicatorInstance &, QSet<QString> &)> depth = [&](const IndicatorInstance &x, QSet<QString> &seen) -> int {
        const QString src = jsValueText(x.params.value("source"));
        if (!src.startsWith("ind:") || seen.contains(x.uid)) return 0;
        seen.insert(x.uid);
        const IndicatorInstance *parent = byUid.value(src.split(':').value(1));
        return parent ? 1 + depth(*parent, seen) : 0;
    };
    QVector<QPair<int, const IndicatorInstance *>> ordered;
    for (const auto &x : cfg.indicators) {
        QSet<QString> seen;
        ordered.push_back({depth(x, seen), &x});
    }
    std::stable_sort(ordered.begin(), ordered.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
    int paneIdx = 0;
    QHash<QString, int> paneOf;
    QVector<IndicatorMarker> markers;
    for (const auto &o : ordered) {
        const IndicatorInstance &ind = *o.second;
        const IndicatorDefPtr def = indicatorById(ind.id);
        if (!def) continue;
        IndicatorResult res;
        try {
            if (n) res = computeIndicator(ind, bs);
        } catch (const Error &e) {
            res = IndicatorResult();
            res.error = e.message();
        } catch (const std::exception &e) {
            res = IndicatorResult();
            res.error = QString::fromUtf8(e.what());
        }
        indResults[ind.uid] = res;
        if (ind.hidden || !n) continue;
        if (res.hasMarkers && ind.tf.isEmpty()) markers += res.markers;
        if (res.extra.value("volumeProfile").isObject()) {
            auto vp = std::make_unique<VolumeProfile>(this, res.extra.value("volumeProfile").toObject());
            main->attachPrimitive(vp.get());
            vpPrims.push_back(std::move(vp));
        }
        if (res.extra.value("bigTrades").isObject()) {
            const QJsonObject all = res.extra.value("bigTrades").toObject();
            const QJsonObject params = BigTradesModel::detectionParams(all);   // the look alone doesn't rebuild
            std::shared_ptr<BigTradesModel> &model = btModels[ind.uid];
            if (!model || model->json() != params) {
                model = std::make_shared<BigTradesModel>(params);
                model->onRebuilt = [this] {
                    syncBigTrades();   // the live trades that came during the rebuild
                    for (auto &l : btLayers) l->scheduleUpdate();
                    updateLegend(legendIndex);
                };
            }
            auto layer = std::make_unique<BigTradesLayer>(this, model, BigTradesLayer::Style::fromJson(all, ind.colors));
            main->attachPrimitive(layer.get());
            btLayers.push_back(std::move(layer));
            btLive.insert(ind.uid);
        }
        if (res.extra.value("heatmap").isObject()) {
            std::shared_ptr<HeatmapTexture> &tex = hmTextures[ind.uid];
            if (!tex) tex = std::make_shared<HeatmapTexture>();
            auto layer = std::make_unique<HeatmapLayer>(this, tex, HeatmapSettings::fromJson(res.extra.value("heatmap").toObject(), ind.colors));
            main->attachPrimitive(layer.get());
            main->attachPrimitive(layer->dom());
            main->attachPrimitive(layer->overlay());
            hmLayers.push_back(std::move(layer));
            hmLive.insert(ind.uid);
        }
        if (res.extra.value("sessionProfile").isObject()) {
            auto sp = std::make_unique<SessionVolumeProfile>(this, res.extra.value("sessionProfile").toObject());
            sp->keyOf = [](double t, const QString &period) { return periodKey(t, period); };
            sp->tfSeconds = parseTf(cfg.tf).seconds;
            main->attachPrimitive(sp.get());
            vpPrims.push_back(std::move(sp));
        }
        QVector<IndicatorOutput> outs;
        for (const auto &out : def->outputs)
            if (res.values.contains(out.key)) outs.push_back(out);
        if (outs.isEmpty()) continue;
        const QString src = jsValueText(ind.params.value("source"));
        std::optional<int> parentPane;
        if (src.startsWith("ind:") && paneOf.contains(src.split(':').value(1)))
            parentPane = paneOf.value(src.split(':').value(1));
        const int pane = parentPane ? *parentPane : def->overlay ? 0 : ++paneIdx;
        paneOf[ind.uid] = pane;
        QVector<lw::Series *> list;
        for (const auto &out : outs) {
            const QString color = ind.colors.value(out.key, out.color).isEmpty() ? out.color : ind.colors.value(out.key, out.color);
            lw::SeriesOptions so;
            if (def->noAutoscale) so.autoscale = false;
            if (def->volumeFormat) so.priceFormat.type = lw::PriceFormatType::Volume;
            so.color = col(color);
            so.priceLineVisible = false;
            so.lastValueVisible = pane > 0;
            so.crosshairMarkerVisible = false;
            so.title = "";
            const Values &vals = res.values[out.key];
            lw::Series *s;
            if (out.kind == "histogram") {
                s = chart->addSeries(lw::SeriesType::Histogram, so, pane);
                s->setData(histData(vals, res.colors, timeAt, n));
            } else {
                so.lineWidth = out.lineWidth ? out.lineWidth : 1;
                if (out.dots) {
                    so.lineVisible = false;
                    so.pointMarkersVisible = true;
                    so.pointMarkersRadius = 1.5;
                }
                s = chart->addSeries(lw::SeriesType::Line, so, pane);
                s->setData(lineData(vals, res.breaks, timeAt, n));
            }
            list.push_back(s);
        }
        if (!parentPane) {
            for (double lv : def->levels) {
                lw::PriceLineOptions pl;
                pl.price = lv;
                pl.color = css("--muted");
                pl.lineWidth = 1;
                pl.lineStyle = lw::LineStyle::Dashed;
                pl.axisLabelVisible = false;
                list[0]->createPriceLine(pl);
            }
        }
        indSeries.insert(ind.uid, list);
    }
    // Big Trades: models of removed (or hidden) indicators go; the feed follows what is left.
    for (auto it = btModels.begin(); it != btModels.end();)
        it = btLive.contains(it.key()) ? std::next(it) : btModels.erase(it);
    if (!btLayers.empty()) {
        if (!btTooltip) btTooltip = std::make_unique<BigTradesTooltip>(this);
        main->attachPrimitive(btTooltip.get());
    }
    for (auto it = hmTextures.begin(); it != hmTextures.end();)
        it = hmLive.contains(it.key()) ? std::next(it) : hmTextures.erase(it);
    syncTradeFeed();
    syncBigTrades();
    syncDepthFeed();
    if (btModels.isEmpty()) btTimer->stop();
    else if (!btTimer->isActive()) btTimer->start();
    const auto panes = chart->panes();
    for (int i = 0; i < panes.size(); ++i) panes[i]->setStretchFactor(i == 0 ? 1 : 0.28);
    struct M { double time; const IndicatorMarker *m; };
    QVector<M> all;
    const double lastTime = times.isEmpty() ? NaN : times.last();
    for (const auto &m : markers) all.push_back({toChart(m.time), &m});
    for (const auto &m : extraMarkers) all.push_back({toChart(m.time), &m});
    if (replay) {
        QVector<M> kept;
        for (const auto &m : all)
            if (m.time <= lastTime) kept.push_back(m);
        all = kept;
    }
    std::stable_sort(all.begin(), all.end(), [](const M &a, const M &b) { return a.time < b.time; });
    QVector<lw::SeriesMarker> lm;
    for (const auto &m : all) lm.push_back(toLw(*m.m, m.time));
    main->setMarkers(lm);
    markersSet = !lm.isEmpty();
}

// "14 on RSI 14 (1D)": the numeric inputs, what it's built on, and its timeframe.
QString ChartPane::indicatorParamsLabel(const IndicatorInstance &ind) const
{
    const IndicatorDefPtr def = indicatorById(ind.id);
    if (!def) return QString();
    if (ind.id == "heatmap") {
        const HeatmapSettings s = HeatmapSettings::fromJson(ind.params);
        const int h = s.historyMinutes;
        QString label = QString("%1 ms · %2").arg(s.intervalMs).arg(h % 60 == 0 ? QString("%1 h").arg(h / 60) : QString("%1 min").arg(h));
        if (s.group > 1) label += QString::fromUtf8(" · rows ×%1").arg(s.group);
        if (s.agg == heatmap::Agg::Mean) label += " · mean";
        return label;
    }
    if (ind.id == "bigtrades") {   // six settings: the ones that say which trades, the legend line says the rest
        const BigTradesModel::Params p = BigTradesModel::Params::fromJson(ind.params);
        const QString max = p.thr.maxVolume > 0 ? QString::fromUtf8(" ≤ %1").arg(footprint::formatVolume(p.thr.maxVolume)) : QString();
        if (p.thr.automatic) return QString("auto p%1%2").arg(js::numberToString(p.thr.percentile), max);
        if (max.isEmpty()) return QString::fromUtf8("≥ %1").arg(footprint::formatVolume(p.thr.minVolume));
        return QString::fromUtf8("%1–%2").arg(footprint::formatVolume(p.thr.minVolume), footprint::formatVolume(p.thr.maxVolume));
    }
    QStringList nums;
    for (const auto &x : def->inputs) {
        if (x.type == "source") continue;
        QJsonValue v = ind.params.value(x.key);
        if (v.isUndefined() || v.isNull()) v = x.def;
        if (x.type != "time") {
            nums << (v.isNull() ? QString() : jsValueText(v));   // Array#join prints null as ""
            continue;
        }
        const double t = v.toDouble();
        nums << (js::truthy(t) ? QLocale(QLocale::English, QLocale::UnitedStates)
                                      .toString(QDateTime::fromSecsSinceEpoch(qint64(t)).date(), "MMM d, yyyy")
                               : QString("pick a bar"));
    }
    const QString src = jsValueText(ind.params.value("source"));
    QString on;
    if (src.startsWith("ind:")) {
        const IndicatorInstance *parent = nullptr;
        for (const auto &x : cfg.indicators)
            if (x.uid == src.split(':').value(1)) parent = &x;
        const IndicatorDefPtr pd = parent ? indicatorById(parent->id) : nullptr;
        on = pd ? QString(" on %1 %2").arg(shortName(pd->name), indicatorParamsLabel(*parent)) : QString(" on (missing)");
    }
    return nums.join(' ') + on + (!ind.tf.isEmpty() && ind.tf != cfg.tf ? QString(" (%1)").arg(ind.tf) : QString());
}

// ---------------------------------------------------------------- legend

void ChartPane::onCrosshair(const lw::MouseEventParams &p)
{
    bigTradesMouse = p.point && p.paneIndex == 0 ? std::optional<QPointF>(*p.point) : std::nullopt;   // the chart repaints anyway
    if (app->tool != "cursor" && drawings->temp && p.point) {
        const auto pt = pointAt(p);
        if (pt) {
            drawings->temp->points.last() = DrawPoint{pt->first, pt->second};
            drawings->update();
        }
    }
    std::optional<int> i;
    if (p.logical && p.point)
        i = std::max(0, std::min(int(shown.size()) - 1, int(js::round(*p.logical))));
    updateLegend(i);
    if (app->settings().syncCrosshair && p.time && p.point && p.logical)
        app->syncCrosshair(this, *p.logical);
}

void ChartPane::updateLegend(std::optional<int> index)
{
    const int n = shown.size();
    const int i = index ? *index : n - 1;
    LegendModel m;
    m.name = QString::fromUtf8("%1 · %2 · %3").arg(theme::ticker(cfg.symbol), cfg.tf,
                                                   provider.isEmpty() ? theme::exchange(cfg.symbol) : provider);
    if (i >= 0 && i < n) {
        const Bar &b = shown[i];
        m.hasBar = true;
        m.o = theme::fmtPrice(b.open);
        m.h = theme::fmtPrice(b.high);
        m.l = theme::fmtPrice(b.low);
        m.c = theme::fmtPrice(b.close);
        if (i - 1 >= 0) {
            const Bar &prev = shown[i - 1];
            const double chg = ((b.close - prev.close) / prev.close) * 100;
            m.chg = theme::fmtPct(chg);
            m.cls = chg >= 0 ? "up" : "down";
        } else {
            m.chg = theme::fmtPct(NaN);
        }
    }
    for (const QString &sym : cfg.compare)
        m.compare.push_back({sym, theme::ticker(sym) + " (compare)"});
    for (const auto &ind : cfg.indicators) {
        const IndicatorDefPtr def = indicatorById(ind.id);
        if (!def) continue;
        const IndicatorResult res = indResults.value(ind.uid);
        LegendModel::Ind row;
        row.uid = ind.uid;
        row.hidden = ind.hidden;
        row.name = shortName(def->name) + " " + indicatorParamsLabel(ind);
        if (ind.id == "bigtrades" && res.error.isEmpty()) {   // drawn from trades: its state instead of values
            if (const QString note = bigTradesNote(ind.uid); !note.isEmpty()) row.vals.push_back({note, "--muted"});
        } else if (ind.id == "heatmap" && res.error.isEmpty()) {   // drawn from the order book: its state
            QString note = heatmapNote();
            // and its trade bubbles' threshold now
            if (const std::shared_ptr<BigTradesModel> m = btModels.value(ind.uid); m && m->built()) {
                const bigtrades::Threshold t = m->threshold();
                if (std::isfinite(t.value) && t.value > 0)
                    note += QString::fromUtf8(" · bubbles ≥ %1").arg(footprint::formatVolume(t.value));
            }
            if (hmEvents && HeatmapSettings::fromJson(ind.params).events && depthFeed && depthFeed->store())
                note += QString::fromUtf8(" · %1 pulled · %2 absorbed")
                            .arg(hmEvents->count(heatmap::LiquidityEvent::Kind::Pulled))
                            .arg(hmEvents->count(heatmap::LiquidityEvent::Kind::Absorbed));
            if (const HeatmapSettings hs = HeatmapSettings::fromJson(ind.params); hs.stableMs > 0)
                note += QString::fromUtf8(" · resting ≥ %1 s (approx.)").arg(hs.stableMs / 1000.0);
            if (!note.isEmpty()) row.vals.push_back({note, "--muted"});
        } else if (!res.error.isEmpty()) {
            row.vals.push_back({res.error, "--down"});
        } else if (res.pending) {
            row.vals.push_back({QString::fromUtf8("loading…"), "--muted"});
        } else {
            for (const auto &o : def->outputs) {
                const Values v = res.values.value(o.key);
                if (i >= 0 && i < v.size() && std::isfinite(v[i]))
                    row.vals.push_back({theme::fmtPrice(v[i]), ind.colors.value(o.key).isEmpty() ? o.color : ind.colors.value(o.key)});
            }
        }
        m.inds.push_back(row);
    }
    m.notes = notes;
    if (cfg.type == "footprint") m.notes << footprintNote();
    legend->setModel(m);
}

// ---------------------------------------------------------------- indicators

QString ChartPane::addIndicator(const QString &id, const QJsonObject &params)
{
    const QString uid = newUid();
    IndicatorInstance ind;
    ind.uid = uid;
    ind.id = id;
    ind.params = params;
    cfg.indicators.push_back(ind);
    render(false);
    app->persist();
    return uid;
}

void ChartPane::removeIndicator(const QString &uid)
{
    QVector<IndicatorInstance> kept;
    for (const auto &x : cfg.indicators)
        if (x.uid != uid) kept.push_back(x);
    cfg.indicators = kept;
    render(false);
    app->persist();
}

// ---------------------------------------------------------------- drawing tools

std::optional<std::pair<double, double>> ChartPane::pointAt(const lw::MouseEventParams &p) const
{
    if (p.paneIndex) return std::nullopt;   // drawings live on the price pane only
    if (!p.point || !p.logical) return std::nullopt;
    const auto l = chart->timeScale()->coordinateToLogical(p.point->x());
    const double logical = l ? *l : *p.logical;
    const auto price = main->coordinateToPrice(p.point->y());
    const double time = timeOfLogical(logical);
    if (!price || std::isnan(time)) return std::nullopt;
    return std::make_pair(time, *price);
}

void ChartPane::onClick(const lw::MouseEventParams &p)
{
    if (destroyed) return;
    const QString tool = app->tool;
    if (tool == "anchor-pick") {
        const auto &pick = app->pendingAnchor;
        IndicatorInstance *ind = nullptr;
        if (pick && pick->pane == this)
            for (auto &x : cfg.indicators)
                if (x.uid == pick->uid) ind = &x;
        const auto pt = pointAt(p);
        if (!ind || !pt) return;
        // Snap to the raw bar under the click (works on Renko/Heikin Ashi too).
        int i = -1;
        for (int k = 0; k < bars.size(); ++k)
            if (bars[k].time > pt->first) { i = k; break; }
        i -= 1;
        if (i < 0) i = bars.size() - 1;
        ind->params[pick->key] = bars[i].time;
        app->pendingAnchor.reset();
        app->setTool("cursor");
        render(false);
        app->persist();
        app->toast("Anchored.");
        return;
    }
    if (tool == "replay-pick") {
        const auto pt = pointAt(p);
        if (!pt) return;
        // map the clicked time back to the raw bars (works for Renko/Heikin Ashi too)
        int i = -1;
        for (int k = 0; k < bars.size(); ++k)
            if (bars[k].time > pt->first) { i = k; break; }
        i -= 1;
        if (i < 0) i = bars.size() - 1;
        app->setTool("cursor");
        startReplay(std::max(1, i));
        return;
    }
    if (tool == "cursor") {
        if (!p.point) return;
        drawings->selected = drawings->hitTest(p.point->x(), p.point->y());
        drawings->update();
        return;
    }
    const DrawingTool *def = toolById(tool);
    const auto pt = pointAt(p);
    if (!def || !pt) return;
    DrawingLayer *d = drawings;
    const DrawPoint dp{pt->first, pt->second};
    bool done = false;
    if (!d->temp) {
        Drawing t;
        t.id = newUid();
        t.type = tool;
        t.points = {dp};
        if (def->points > 1) t.points.push_back(dp);
        d->temp = t;
    } else {
        d->temp->points.last() = dp;
        if (d->temp->points.size() < def->points) d->temp->points.push_back(dp);
        else done = true;
    }
    if (def->points == 1) done = true;
    if (done) {
        Drawing item = *d->temp;
        d->temp.reset();
        if (item.type == "text") {
            finishTextNote(item);
            d->update();
            return;
        }
        d->items.push_back(item);
        d->selected = item.id;
        app->saveDrawingsFrom(this, cfg.symbol, drawingsToJson(d->items));
        if (!app->settings().stayInDrawMode) app->setTool("cursor");
    }
    d->update();
}

// JS prompt("Note text:", ""): a small input card in the terminal style.
void ChartPane::finishTextNote(Drawing item)
{
    auto *body = new QWidget;
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(8);
    v->addWidget(label("Note text:"));
    auto *in = new QLineEdit;
    theme::setCls(in, "in");
    v->addWidget(in);
    auto *foot = new QWidget;
    auto *h = new QHBoxLayout(foot);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(8);
    h->addStretch(1);
    auto *cancel = new QPushButton("Cancel");
    theme::setCls(cancel, "btn");
    auto *ok = new QPushButton("OK");
    theme::setCls(ok, "btn primary");
    h->addWidget(cancel);
    h->addWidget(ok);
    ModalCard *card = app->modal("Text note", body, foot);
    auto accepted = std::make_shared<bool>(false);
    QPointer<ChartPane> self(this);
    auto accept = [this, self, in, accepted, item]() mutable {
        if (!self) return;
        const QString text = in->text();
        *accepted = true;
        app->closeModal();
        if (text.isEmpty()) {
            drawings->update();
            app->setTool("cursor");
            return;
        }
        item.text = text;
        drawings->items.push_back(item);
        drawings->selected = item.id;
        app->saveDrawingsFrom(this, cfg.symbol, drawingsToJson(drawings->items));
        if (!app->settings().stayInDrawMode) app->setTool("cursor");
        drawings->update();
    };
    QObject::connect(ok, &QPushButton::clicked, card, accept);
    QObject::connect(in, &QLineEdit::returnPressed, card, accept);
    QObject::connect(cancel, &QPushButton::clicked, card, [this] { app->closeModal(); });
    QObject::connect(card, &ModalCard::closed, this, [this, self, accepted] {
        if (!self || *accepted) return;
        drawings->update();
        app->setTool("cursor");
    });
    in->setFocus();
}

bool ChartPane::deleteSelectedDrawing()
{
    DrawingLayer *d = drawings;
    if (d->selected.isEmpty() || !d->selectedItem()) return false;
    QVector<Drawing> kept;
    for (const auto &x : d->items)
        if (x.id != d->selected) kept.push_back(x);
    d->items = kept;
    d->selected.clear();
    d->update();
    app->saveDrawingsFrom(this, cfg.symbol, drawingsToJson(d->items));
    return true;
}

void ChartPane::clearDrawings()
{
    drawings->items.clear();
    drawings->selected.clear();
    drawings->temp.reset();
    drawings->update();
    app->saveDrawingsFrom(this, cfg.symbol, QJsonArray());
}

void ChartPane::setInteractive(bool on)
{
    lw::ChartOptions o = chart->options();
    o.handleScroll = on;
    o.handleScale = on;
    chart->applyOptions(o);
    if (on && drawings->temp) {
        drawings->temp.reset();
        drawings->update();
    }
}

// ---------------------------------------------------------------- footprint (Deepwater addition)

namespace {
// Timeframes the footprint is built for: longer candles would need more trades than a history budget holds.
const QStringList FOOTPRINT_TFS = {"1m", "3m", "5m", "15m", "30m", "1h"};
} // namespace

void ChartPane::syncFootprint()
{
    const bool want = cfg.type == "footprint" && !bars.isEmpty();
    fpUnsupported.clear();
    if (want) {
        if (theme::exchange(cfg.symbol) != "BINANCE" || provider != "Binance")
            fpUnsupported = QString("The footprint needs Binance trades; this chart's data comes from %1.")
                                .arg(provider.isEmpty() ? QString("elsewhere") : provider);
        else if (!FOOTPRINT_TFS.contains(cfg.tf))
            fpUnsupported = "The footprint works on 1m to 1h charts.";
    }
    if (!want && fpLayer) {
        if (main) main->detachPrimitive(fpLayer.get());
        fpLayer.reset();
    }
    if (want) {
        if (!fpLayer) {
            fpLayer = std::make_unique<FootprintLayer>(this);
            // under the drawings, like makeMainSeries() attaches it
            main->detachPrimitive(drawings);
            main->attachPrimitive(fpLayer.get());
            main->attachPrimitive(drawings);
        }
        const FootprintLayer::Settings st = FootprintLayer::Settings::fromJson(cfg.footprint);
        if (st.toJson() != fpLayer->settings().toJson()) {
            fpLayer->setSettings(st);
            applyFootprintRows();
        }
    }
    syncTradeFeed();
}

bool ChartPane::wantsBigTrades() const
{
    for (const IndicatorInstance &ind : cfg.indicators) {
        if (ind.id == "bigtrades" && !ind.hidden) return true;
        // the liquidity heatmap's trade bubbles (on unless turned off)
        // and its pulled / absorbed marks, read from the trades too
        if (ind.id == "heatmap" && !ind.hidden
            && (jsValueText(ind.params.value("bubbles")) != "Off" || jsValueText(ind.params.value("events")) != "Off"))
            return true;
    }
    return false;
}

void ChartPane::syncTradeFeed()
{
    const bool binance = theme::exchange(cfg.symbol) == "BINANCE" && provider == "Binance";
    const bool footprint = fpLayer && fpUnsupported.isEmpty();
    const bool want = binance && !bars.isEmpty() && (footprint || wantsBigTrades());
    if (!want) {
        if (tradeFeed) {
            if (fpLayer) fpLayer->setFeed(nullptr);
            delete tradeFeed;
            tradeFeed = nullptr;
            feedKey.clear();
        }
        return;
    }
    // The history budget is the footprint's setting, for both.
    const int budget = FootprintLayer::Settings::fromJson(cfg.footprint).budget;
    const QString key = cfg.symbol + "|" + cfg.tf + "|" + QString::number(budget);
    if (!tradeFeed || feedKey != key) {
        if (fpLayer) fpLayer->setFeed(nullptr);
        delete tradeFeed;
        feedKey = key;
        tradeFeed = new TradeFeed(theme::ticker(cfg.symbol), double(parseTf(cfg.tf).seconds), bars.first().time, budget, this);
        connect(tradeFeed, &TradeFeed::changed, this, [this] {
            if (fpLayer) fpLayer->scheduleUpdate();
            syncBigTrades();
        });
        connect(tradeFeed, &TradeFeed::stateChanged, this, [this] {
            applyFootprintRows();
            syncBigTrades();
            updateLegend(legendIndex);
            if (fpLayer) fpLayer->scheduleUpdate();
        });
        if (app->livePaused() || replay) tradeFeed->stop();
    } else {
        tradeFeed->setChartStart(bars.first().time);
    }
    if (fpLayer) fpLayer->setFeed(footprint ? tradeFeed : nullptr);
}

void ChartPane::syncDepthFeed()
{
    const bool binance = theme::exchange(cfg.symbol) == "BINANCE" && provider == "Binance";
    const HeatmapLayer *first = hmLayers.empty() ? nullptr : hmLayers.front().get();
    // In replay the feed stays (its columns, from recordings too, are what the replay shows), only the live stream stops.
    const bool want = binance && first && !bars.isEmpty() && !destroyed;
    if (!want) {
        for (auto &l : hmLayers) {
            l->setFeed(nullptr);
            l->setEvents(nullptr);
        }
        hmEvents.reset();
        if (depthFeed) {
            delete depthFeed;
            depthFeed = nullptr;
            depthKey.clear();
        }
        return;
    }
    // One feed per pane: the first heatmap's sampling and history (several heatmaps only differ in their look).
    const HeatmapSettings &s = first->settings();
    const QString key = cfg.symbol + "|" + QString::number(s.intervalMs);
    if (!depthFeed || depthKey != key) {
        for (auto &l : hmLayers) l->setFeed(nullptr);
        delete depthFeed;
        depthKey = key;
        DepthFeed::Params p;
        p.intervalMs = s.intervalMs;
        p.historyMinutes = s.historyMinutes;
        p.agg = s.agg;
        p.stableMs = s.stableMs;
        p.record = s.record;
        p.keepDays = s.keepDays;
        depthFeed = new DepthFeed(theme::ticker(cfg.symbol), p, this);
        connect(depthFeed, &DepthFeed::changed, this, [this] {
            if (hmEvents) {
                hmEvents->sync(depthFeed, tradeFeed, hmLayers.empty() ? heatmap::EventParams{} : hmLayers.front()->settings().eventParams);
                // the legend's counts
                const size_t n = hmEvents->detector() ? hmEvents->detector()->events().size() : 0;
                if (n != hmEventsShown) {
                    hmEventsShown = n;
                    updateLegend(legendIndex);
                }
            }
            for (auto &l : hmLayers) l->scheduleUpdate();
        });
        // The legend line only when it says something new (stats come every half second).
        connect(depthFeed, &DepthFeed::stateChanged, this, [this] {
            const QString note = heatmapNote();
            if (hmNote != note) {
                hmNote = note;
                updateLegend(legendIndex);
            }
            for (auto &l : hmLayers)
                if (l->settings().debug) l->scheduleUpdate();
        });
        if (app->livePaused() || replay) depthFeed->stop();
    } else {
        depthFeed->setHistoryMinutes(s.historyMinutes);
        depthFeed->setAgg(s.agg);
        depthFeed->setStableMs(s.stableMs);
        depthFeed->setRecording(s.record, s.keepDays);
        if (replay) depthFeed->stop();
        else if (!app->livePaused()) depthFeed->resume();
    }
    // Pulled / absorbed liquidity: one detector per pane, when a heatmap wants the marks.
    const bool events = std::any_of(hmLayers.begin(), hmLayers.end(), [](const auto &l) { return l->settings().events; });
    if (!events) hmEvents.reset();
    else if (!hmEvents) hmEvents = std::make_unique<HeatmapEvents>();
    for (auto &l : hmLayers) {
        l->setFeed(depthFeed);
        l->setEvents(hmEvents.get());
    }
}

QString ChartPane::heatmapNote() const
{
    if (!depthFeed) {
        if (theme::exchange(cfg.symbol) != "BINANCE" || provider != "Binance") return "Binance pairs only";
        return QString();
    }
    const DepthStats &s = depthFeed->stats();
    const heatmap::ColumnStore *st = depthFeed->store();
    if (replay) return st && !st->empty() ? QString("replay: recorded depth up to the cursor") : QString("replay: no depth recorded here");
    if (!s.error.isEmpty()) return s.error;
    if (!st) return QString::fromUtf8("connecting to Binance's order book…");
    if (!s.synced) return QString::fromUtf8("syncing the order book…");
    const double span = st->empty() ? 0 : st->meta(st->endIndex() - 1).time - st->meta(st->firstIndex()).time;
    const int min = int(span / 60);
    const QString rec = s.recording ? QString::fromUtf8(" · ● REC %1 MB").arg(double(s.recordedBytes) / 1e6, 0, 'f', 1)
                        : !s.recordNote.isEmpty() ? " · not recording: " + s.recordNote : QString();
    const QString loaded = s.loadedColumns ? QString(" (%1 from recordings)").arg(s.loadedFiles == 1 ? "1 file" : QString("%1 files").arg(s.loadedFiles)) : QString();
    return QString::fromUtf8("live · %1 of depth%2 · rows of %3%4")
        .arg(min >= 60 ? QString("%1 h %2 min").arg(min / 60).arg(min % 60) : QString("%1 min").arg(min), loaded,
             theme::fmtPrice(depthFeed->rowPrice() * (hmLayers.empty() ? 1 : hmLayers.front()->settings().group)), rec);
}

void ChartPane::syncBigTrades()
{
    if (btModels.isEmpty()) return;
    // Aggregates close a second after their window: trades reach us late, and a split order would show as two.
    const double now = double(QDateTime::currentMSecsSinceEpoch()) / 1000 - 1;
    bool changed = false;
    for (const auto &m : std::as_const(btModels)) changed = m->sync(tradeFeed, now) || changed;
    if (changed)
        for (auto &l : btLayers) l->scheduleUpdate();
}

// The legend's line for a Big Trades indicator: why nothing shows, or the threshold and how many.
QString ChartPane::bigTradesNote(const QString &uid) const
{
    if (theme::exchange(cfg.symbol) != "BINANCE" || provider != "Binance")
        return QString("Binance pairs only (this chart's data comes from %1)").arg(provider.isEmpty() ? QString("elsewhere") : provider);
    const auto m = btModels.value(uid);
    if (!tradeFeed || !m) return QString();
    if (!tradeFeed->error().isEmpty()) return tradeFeed->error();
    if (!m->built() || (m->trades().empty() && !m->complete())) {
        const QString count = QLocale(QLocale::English).toString(qlonglong(tradeFeed->tape().size()));
        return QString::fromUtf8("loading trades… %1").arg(count);
    }
    const bigtrades::Threshold t = m->threshold();
    const bigtrades::ThresholdParams &p = m->params().thr;
    const QString count = QString("%1 big trades%2").arg(m->trades().size())
                              .arg(m->complete() ? QString() : QString::fromUtf8(" (loading history…)"));
    // The threshold now: what is drawn from here on (each bubble was judged against the one of its moment).
    if (t.warmingUp) {
        const QString wait = QString("warming up: %1 of %2 aggregates").arg(t.samples).arg(p.minSamples);
        return QString::fromUtf8("%1 · %2 · %3").arg(wait,
            t.value > 0 ? QString::fromUtf8("≥ %1 meanwhile").arg(footprint::formatVolume(t.value)) : QString("none shown yet"), count);
    }
    QString range = QString::fromUtf8("≥ %1").arg(footprint::formatVolume(t.value));
    if (p.maxVolume > 0) range += QString::fromUtf8(" and ≤ %1").arg(footprint::formatVolume(p.maxVolume));
    if (t.automatic) {
        QString how = QString("p%1 of %2 min = %3").arg(js::numberToString(p.percentile), js::numberToString(p.windowMinutes),
                                                       footprint::formatVolume(t.percentileValue));
        if (p.volatility)
            how += t.volatilityReady ? QString::fromUtf8(" × %1 volatility").arg(QString::number(t.volatilityFactor, 'f', 2))
                                     : QString("; volatility from %1 min of trades").arg(p.atrLong);
        range += " (" + how + ")";
    }
    return QString::fromUtf8("%1 · %2").arg(range, count);
}

double ChartPane::footprintTickSize() const
{
    return tradeFeed && tradeFeed->ready() ? tradeFeed->tickSize() : 0;
}

// The grouping: the setting, or (Auto) a start the layer then fits to the zoom.
void ChartPane::applyFootprintRows()
{
    if (!tradeFeed || !fpLayer || !(tradeFeed->tickSize() > 0)) return;
    const int set = fpLayer->settings().rowTicks;
    tradeFeed->setRowTicks(set > 0 ? set : footprint::autoRowTicks(bars, tradeFeed->tickSize()));
}

QString ChartPane::footprintNote() const
{
    if (!fpUnsupported.isEmpty()) return fpUnsupported;
    if (!tradeFeed) return QString();
    if (!tradeFeed->error().isEmpty()) return "Footprint: " + tradeFeed->error();
    if (!tradeFeed->ready()) return QString::fromUtf8("Footprint: loading Binance trades…");
    const footprint::Book &book = tradeFeed->book();
    const QString side = book.approximate() ? QString("aggressor side estimated from prices (approximate)")
                                            : QString("aggressor side from Binance (exact)");
    const QString count = QLocale(QLocale::English).toString(qlonglong(book.tradeCount()));
    const footprint::TickGrid &g = book.grid();
    const QString row = theme::fmtPrice(g.rowHeight());
    if (tradeFeed->loadingHistory())
        return QString::fromUtf8("Footprint: %1, rows of %2. Loading history… %3 trades")
            .arg(side, row, count);
    const double from = tradeFeed->completeFrom();
    QString since;
    if (!std::isnan(from) && !bars.isEmpty() && from > bars.first().time) {
        const QDateTime t = app->settings().utc ? QDateTime::fromSecsSinceEpoch(qint64(from), QTimeZone::UTC)
                                                : QDateTime::fromSecsSinceEpoch(qint64(from));
        since = QString(", from %1").arg(t.toString(t.date() == QDate::currentDate() ? "HH:mm" : "d MMM HH:mm"));
    }
    return QString("Footprint: %1, rows of %2%3 (%4 trades).").arg(side, row, since, count);
}

// ---------------------------------------------------------------- live updates

void ChartPane::startLive()
{
    stopLive();
    if (replay || bars.isEmpty() || destroyed) return;
    if (app->livePaused()) {
        livePaused = true;
        return;
    }
    const QString ex = theme::exchange(cfg.symbol);
    const QString wsTf = BINANCE_WS_TF.value(cfg.tf);
    if (ex == "BINANCE" && provider == "Binance" && !wsTf.isEmpty())
        openSocket(theme::ticker(cfg.symbol).toLower(), wsTf, 0);
    else
        poll();
}

void ChartPane::openSocket(const QString &sym, const QString &interval, int hostIdx)
{
    static const QStringList hosts = {"wss://stream.binance.com:9443/ws/", "wss://data-stream.binance.vision/ws/"};
    if (hostIdx >= hosts.size()) {
        poll();
        return;
    }
    auto *sock = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    ws = sock;
    auto opened = std::make_shared<bool>(false);
    auto closed = std::make_shared<bool>(false);
    connect(sock, &QWebSocket::connected, this, [this, opened] {
        *opened = true;
        app->setStatus("ok");
    });
    connect(sock, &QWebSocket::textMessageReceived, this, [this](const QString &m) {
        const QJsonObject k = QJsonDocument::fromJson(m.toUtf8()).object().value("k").toObject();
        if (k.isEmpty()) return;
        auto num = [&](const char *key) {
            const QJsonValue v = k.value(key);
            return v.isString() ? v.toString().toDouble() : v.toDouble();
        };
        Bar b;
        b.time = k.value("t").toDouble() / 1000;
        b.open = num("o");
        b.high = num("h");
        b.low = num("l");
        b.close = num("c");
        b.volume = num("v");
        merge({b});
    });
    auto onClose = [this, sock, opened, closed, sym, interval, hostIdx] {
        if (*closed) return;
        *closed = true;
        sock->deleteLater();
        if (ws != sock) return;
        ws = nullptr;
        if (destroyed) return;
        if (!*opened) openSocket(sym, interval, hostIdx + 1);
        else {
            liveTimer->disconnect();
            connect(liveTimer, &QTimer::timeout, this, [this] { startLive(); });
            liveTimer->start(3000);
        }
    };
    connect(sock, &QWebSocket::disconnected, this, onClose);
    connect(sock, &QWebSocket::errorOccurred, this, [sock, onClose](QAbstractSocket::SocketError) {
        if (sock->state() == QAbstractSocket::UnconnectedState) onClose();
    });
    sock->open(QUrl(hosts[hostIdx] + sym + "@kline_" + interval));
}

void ChartPane::poll()
{
    const QString ex = theme::exchange(cfg.symbol);
    const int kind = (ex == "BINANCE" || ex == "COINBASE" || ex == "KRAKEN") ? 3000 : 10000;
    liveTimer->disconnect();
    connect(liveTimer, &QTimer::timeout, this, [this, kind] {
        const int token = loadToken;
        api::candles(this, cfg.symbol, cfg.tf, 5,
            [this, kind, token](const CandleSet &r) {
                if (destroyed || token != loadToken) return;
                merge(r.candles);
                if (!ws && !replay) liveTimer->start(kind);
            },
            [this, kind, token](const QString &) {   // keep trying
                if (destroyed || token != loadToken) return;
                if (!ws && !replay) liveTimer->start(kind);
            });
    });
    liveTimer->start(kind);
}

void ChartPane::stopLive()
{
    liveTimer->stop();
    liveTimer->disconnect();
    if (ws) {
        QWebSocket *s = ws;
        ws = nullptr;
        s->close();
    }
}

void ChartPane::pauseLive()
{
    if (tradeFeed) tradeFeed->stop();
    if (depthFeed) depthFeed->stop();
    if (destroyed || replay || bars.isEmpty()) return;
    stopLive();
    livePaused = true;
}

void ChartPane::resumeLive()
{
    if (tradeFeed && !destroyed) tradeFeed->resume();
    if (depthFeed && !destroyed) depthFeed->resume();
    if (!livePaused || destroyed) return;
    livePaused = false;
    if (replay || bars.isEmpty()) return;
    // The bars missed while paused, and the last one again (it may have changed since).
    const int max = app->settings().bars ? app->settings().bars : 3000;
    double missed = max;
    try {
        if (const double tf = double(parseTf(cfg.tf).seconds); tf > 0)
            missed = (QDateTime::currentSecsSinceEpoch() - bars.last().time) / tf + 2;
    } catch (const Error &) {
    }
    const int token = loadToken;
    api::candles(this, cfg.symbol, cfg.tf, int(std::clamp(missed, 5.0, double(max))),
        [this, token](const CandleSet &r) {
            if (destroyed || token != loadToken || replay) return;
            if (!r.candles.isEmpty() && r.candles.first().time > bars.last().time) {
                load(true);   // away for longer than a full load: start over rather than leave a gap
                return;
            }
            catchUp(r.candles);
            startLive();
        },
        [this, token](const QString &) {
            if (!destroyed && token == loadToken) startLive();
        });
}

void ChartPane::catchUp(const Bars &recent)
{
    const Bar last = bars.last();
    Bars fresh;
    for (const Bar &b : recent) {
        const bool same = b.time == last.time && b.open == last.open && b.high == last.high && b.low == last.low
                          && b.close == last.close && b.volume == last.volume;
        if (b.time >= last.time && !same) fresh.push_back(b);
    }
    if (fresh.isEmpty()) return;
    if (!isSimpleType(cfg.type)) {
        merge(fresh);   // redraws the whole chart from the bars anyway
        return;
    }
    // One bar at a time, the way live updates would have come in (merge() adds the last bar of a
    // batch to a simple chart), then the indicators over them.
    for (const Bar &b : fresh) merge({b});
    lastFull = double(QDateTime::currentMSecsSinceEpoch());
    renderIndicators();
    updateLegend();
}

void ChartPane::merge(const Bars &newBars)
{
    if (bars.isEmpty() || replay) return;
    const bool simple = isSimpleType(cfg.type);
    // A simple chart shows the bars themselves: the change is made to both lists (making them share the
    // list again, as `shown = bars` did, had the next tick copy every bar).
    const bool mirror = simple && !shown.isEmpty() && !bars.isEmpty() && shown.last().time == bars.last().time;
    bool changed = false;
    for (const Bar &b : newBars) {
        const Bar &last = bars.last();
        if (b.time == last.time) {
            bars.last() = b;
            if (mirror) shown.last() = b;
            changed = true;
        } else if (b.time > last.time) {
            bars.push_back(b);
            if (mirror) shown.push_back(b);
            changed = true;
        }
    }
    if (!changed) return;
    const Bar last = bars.last();
    app->onPrice(cfg.symbol, last.close);
    if (simple) {
        if (!mirror) shown = bars;
        if (times.isEmpty() || times.last() != toChart(last.time)) times.push_back(toChart(last.time));
        main->update(pointFor(this, mainType, last));
        if (volume) volume->update(volPoint(this, last));
    }
    const double now = double(QDateTime::currentMSecsSinceEpoch());
    if (now - mtfCheckedAt > 60000) {
        mtfCheckedAt = now;
        const QStringList tfs = mtf.keys();
        for (const QString &tf : tfs)
            if (now - mtf[tf].at > 60000) loadMtf(tf);
    }
    // The indicators (and a transformed chart: Heikin Ashi, Renko…, rebuilt from all the bars) at most every
    // 1.5 s; a transformed chart gets the last tick of a burst a moment later.
    if (now - lastFull > 1500) {
        lastFull = now;
        if (simple) {
            renderIndicators();
            updateLegend();
        } else {
            render(false);
        }
    } else if (!simple && !fullPending) {
        fullPending = true;
        QTimer::singleShot(int(1500 - (now - lastFull)) + 1, this, [this] {
            fullPending = false;
            if (destroyed || replay || isSimpleType(cfg.type)) return;
            lastFull = double(QDateTime::currentMSecsSinceEpoch());
            render(false);
        });
    }
    // A chart left open keeps about the number of bars it loads (the setting): once 500 more have come in, the
    // oldest go. The view stays on the same bars (a transformed chart only drops them while at the last bar).
    const int max = app->settings().bars ? app->settings().bars : 3000;
    if (bars.size() > max + 500) {
        const auto range = chart->timeScale()->getVisibleLogicalRange();
        const double lastBefore = lastBarLogical();
        const bool atEnd = range && !shown.isEmpty() && range->to >= lastBefore - 2;
        if (atEnd || simple) {
            const int excess = int(bars.size()) - max;
            bars.remove(0, excess);
            lastFull = now;
            render(false);
            // The same bars in view: the indexes moved down by what went (bars, and compared times before them).
            const double shift = lastBefore - lastBarLogical();
            if (!atEnd && range)
                chart->timeScale()->setVisibleLogicalRange({std::max(0.0, range->from - shift), std::max(0.0, range->to - shift)});
        }
    }
}

// ---------------------------------------------------------------- bar replay

void ChartPane::removeReplayBar()
{
    if (replayBar) {
        replayBar->deleteLater();
        replayBar = nullptr;
        replayPlayBtn = nullptr;
    }
}

namespace {

// The replay bar is position:absolute at left:50% of the pane, so it may be at most half the pane
// wide before its flex items shrink (down to their min-content width): in a narrow chart (two side
// by side) "Jump to…" and "All charts" wrap at their spaces. Line height 1.4 of 13px.
constexpr double ReplayLine = 13 * 1.4;

QStringList greedyLines(const QFont &f, const QStringList &words, double width, double firstIndent = -1)
{
    // firstIndent >= 0: the first line starts with a box that wide (the checkbox) before the words.
    QStringList lines;
    QString cur;
    double curW = firstIndent >= 0 ? firstIndent : -1;
    const double space = qtm::text::width(f, " ");
    for (const QString &w : words) {
        const double ww = qtm::text::width(f, w);
        if (curW < 0) {
            cur = w;
            curW = ww;
        } else if (curW + space + ww <= width + 0.01) {
            cur += (cur.isEmpty() && lines.isEmpty() && firstIndent >= 0 ? "" : " ") + w;
            curW += space + ww;
        } else {
            lines << cur;
            cur = w;
            curW = ww;
        }
    }
    lines << cur;
    return lines;
}

// "Jump to…": when narrower than its text, the lines overflow the 28px button, centred on it.
class ReplayWrapBtn : public Btn
{
public:
    using Btn::Btn;
    int wrapWidth = 0;   // 0 = as wide as its text
    QStringList lines() const
    {
        return greedyLines(theme::font(13), label().split(' '), wrapWidth - 16);
    }
    QSize sizeHint() const override
    {
        if (!wrapWidth) return Btn::sizeHint();
        return {wrapWidth, std::max(28, int(std::ceil(lines().size() * ReplayLine)))};
    }
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent *e) override
    {
        if (!wrapWidth) return Btn::paintEvent(e);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF box(0, (height() - 28) / 2.0, width(), 28);
        if (underMouse()) {
            p.setPen(Qt::NoPen);
            p.setBrush(theme::color("--hover"));
            p.drawRoundedRect(box, 4, 4);
        }
        const QFont f = theme::font(13);
        const QStringList ls = lines();
        const double a = qtm::text::ascent(f), d = qtm::text::descent(f);
        double top = height() / 2.0 - ls.size() * ReplayLine / 2;
        for (const QString &l : ls) {
            qtm::text::draw(p, QPointF((width() - qtm::text::width(f, l)) / 2, top + (ReplayLine - (a + d)) / 2 + a), f, l,
                       theme::color("--text"));
            top += ReplayLine;
        }
    }
};

// <label class="hint"><input type="checkbox"> All charts</label>: the checkbox (13px, margins
// 3 3 3 4) and the words wrap like inline content inside padding 0 6px.
class ReplaySyncBox : public QCheckBox
{
public:
    using QCheckBox::QCheckBox;
    int wrapWidth = 0;   // 0 = on one line
    QStringList lines() const
    {
        return greedyLines(theme::font(13), text().split(' '), wrapWidth - 12, 20);
    }
    double lineHeight(int i) const { return i == 0 ? 21 : ReplayLine; }   // the checkbox's line is taller
    QSize sizeHint() const override
    {
        if (!wrapWidth) return QCheckBox::sizeHint();
        double h = 0;
        for (int i = 0; i < lines().size(); ++i) h += lineHeight(i);
        return {wrapWidth, int(std::ceil(h))};
    }
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent *e) override
    {
        if (!wrapWidth) return QCheckBox::paintEvent(e);
        QPainter p(this);
        const QFont f = theme::font(13);
        const QStringList ls = lines();
        const double a = qtm::text::ascent(f), d = qtm::text::descent(f);
        double top = (height() - sizeHint().height()) / 2.0;
        QStyleOptionButton opt;
        initStyleOption(&opt);
        opt.rect = QRect(6 + 4, int(std::round(top + 3)), 13, 13);
        style()->drawPrimitive(QStyle::PE_IndicatorCheckBox, &opt, &p, this);
        for (int i = 0; i < ls.size(); ++i) {
            const double lh = lineHeight(i);
            const double x = 6 + (i == 0 ? 20 + (ls[0].isEmpty() ? 0 : qtm::text::width(f, " ")) : 0);
            // the first line's baseline is the checkbox's bottom margin edge
            const double base = i == 0 ? top + 19 : top + (ReplayLine - (a + d)) / 2 + a;
            if (!ls[i].isEmpty()) qtm::text::draw(p, QPointF(x, base), f, ls[i], theme::color("--muted"));
            top += lh;
        }
    }
    bool hitButton(const QPoint &pos) const override { return wrapWidth ? rect().contains(pos) : QCheckBox::hitButton(pos); }
};

} // namespace

// Shrink-to-fit inside `avail` (half the pane): when the bar's natural width doesn't fit, the
// two items that can wrap shrink (flex-shrink 1, weighted by their natural width) down to their
// min-content width, like the browser's flex layout.
static void squeezeReplayBar(QWidget *bar, double avail)
{
    auto *pick = static_cast<ReplayWrapBtn *>(bar->findChild<QWidget *>("qtmReplayPick"));
    auto *sync = static_cast<ReplaySyncBox *>(bar->findChild<QWidget *>("qtmReplaySync"));
    if (!pick) return;
    pick->wrapWidth = 0;
    if (sync) sync->wrapWidth = 0;
    bar->layout()->invalidate();
    const double natural = bar->layout()->sizeHint().width();
    if (natural <= avail) {
        pick->updateGeometry();
        if (sync) sync->updateGeometry();
        return;
    }
    const QFont f = theme::font(13);
    auto longest = [&](const QString &s) {
        double m = 0;
        for (const QString &w : s.split(' ')) m = std::max(m, qtm::text::width(f, w));
        return m;
    };
    struct Item { QWidget *w; double base, min, size = 0; bool frozen = false; };
    QVector<Item> items {{pick, double(pick->sizeHint().width()), std::round(16 + longest(pick->label()))}};
    if (sync) items.push_back({sync, double(sync->sizeHint().width()), std::round(12 + std::max(20.0, longest(sync->text())))});
    double minTotal = natural;
    for (const Item &it : items) minTotal -= it.base - it.min;
    double space = std::max(avail, minTotal) - (natural - [&] { double b = 0; for (const Item &it : items) b += it.base; return b; }());
    // CSS flex-shrink: distribute the overflow by base size, freeze items at their minimum, repeat
    for (int pass = 0; pass < 3; ++pass) {
        double scaled = 0, used = 0;
        for (const Item &it : items) (it.frozen ? used += it.size : scaled += it.base);
        double total = 0;
        for (const Item &it : items) if (!it.frozen) total += it.base;
        const double overflow = total - (space - used);
        bool clamped = false;
        for (Item &it : items) {
            if (it.frozen) continue;
            it.size = it.base - overflow * it.base / scaled;
            if (it.size < it.min) { it.size = it.min; it.frozen = true; clamped = true; }
        }
        if (!clamped) break;
    }
    pick->wrapWidth = int(std::round(items[0].size));
    if (sync) sync->wrapWidth = int(std::round(items[1].size));
    pick->updateGeometry();
    if (sync) sync->updateGeometry();
    bar->layout()->invalidate();
    bar->layout()->activate();
}

static BtnStyle replayBtnStyle()
{
    BtnStyle s;
    s.height = 28;
    s.padL = s.padR = 8;
    s.radius = 4;
    s.hoverBg = "--hover";
    return s;
}

void ChartPane::buildReplayBar()
{
    auto *bar = box("replay-bar", this);
    bar->setObjectName("qtmReplayBar");
    auto *h = new QHBoxLayout(bar);
    h->setContentsMargins(7, 5, 7, 5);
    h->setSpacing(4);
    auto *hint = label("Replay", "hint");
    h->addWidget(hint);
    auto *back = new Btn(replayBtnStyle(), QString(), QString::fromUtf8("◀◀"));
    back->setToolTip("Step back");
    auto *play = new Btn(replayBtnStyle(), QString(), QString::fromUtf8("▶"));
    play->setToolTip("Play / pause");
    auto *fwd = new Btn(replayBtnStyle(), QString(), QString::fromUtf8("▶▶"));
    fwd->setToolTip("Step forward");
    replayPlayBtn = play;
    h->addWidget(back);
    h->addWidget(play);
    h->addWidget(fwd);
    auto *speed = new Select;
    speed->setObjectName("qtmReplaySpeed");
    speed->setToolTip("Speed");
    for (const char *s : {"0.5", "1", "2", "5", "10"})
        speed->addOption(QString("%1x").arg(s), s);
    speed->setValue("2");
    speed->setNativeArrow(true);   // a plain <select>: padding 0 8px plus Chrome's arrow
    speed->setFixedHeight(28);
    h->addWidget(speed);
    auto *pick = new ReplayWrapBtn(replayBtnStyle(), QString(), QString::fromUtf8("Jump to…"));
    pick->setObjectName("qtmReplayPick");
    pick->setToolTip("Pick a new start bar");
    h->addWidget(pick);
    if (app->panes.size() > 1) {
        auto *sync = new ReplaySyncBox("All charts");
        sync->setObjectName("qtmReplaySync");
        sync->setToolTip("Replay every chart in the layout to the same moment");
        sync->setChecked(app->settings().syncReplay);
        theme::setCls(sync, "hint");
        h->addWidget(sync);
        connect(sync, &QCheckBox::toggled, this, [this](bool on) {
            app->settings().syncReplay = on;
            app->persist();
            if (on) app->onReplayStart(this);
            else app->onReplayStop(this);
        });
    }
    auto *exit = new Btn(replayBtnStyle(), QString(), "Exit");
    exit->setToolTip("Back to live");
    h->addWidget(exit);
    for (int i = 0; i < h->count(); ++i) h->itemAt(i)->setAlignment(Qt::AlignVCenter);   // align-items: center
    connect(back, &QAbstractButton::clicked, this, [this] { replayStep(-1); });
    connect(fwd, &QAbstractButton::clicked, this, [this] { replayStep(1); });
    connect(play, &QAbstractButton::clicked, this, [this] { toggleReplay(); });
    connect(pick, &QAbstractButton::clicked, this, [this] { app->setTool("replay-pick"); });
    connect(exit, &QAbstractButton::clicked, this, [this] { stopReplay(); });
    connect(speed, &QComboBox::currentIndexChanged, this, [this, speed] {
        if (!replay) return;
        replay->speed = speed->value().toDouble();
        if (replay->playing) {
            toggleReplay();
            toggleReplay();
        }
    });
    auto *shadow = new QGraphicsDropShadowEffect(bar);
    shadow->setBlurRadius(32);
    shadow->setOffset(0, 8);
    shadow->setColor(theme::shadowColor());
    bar->setGraphicsEffect(shadow);
    replayBar = bar;
    bar->show();
    layoutOverlays();
}

void ChartPane::startReplay(int index)
{
    stopLive();
    Replay r;
    r.index = std::max(1, std::min(index, int(bars.size()) - 1));
    replay = r;
    if (!replayBar) buildReplayBar();
    render(false);
    app->onReplayStart(this);
}

// Follow another chart's replay: show only the bars that had finished by `endTime`.
void ChartPane::replayTo(double endTime)
{
    const double sec = parseTf(cfg.tf).seconds;
    int i = bars.size() - 1;
    while (i > 0 && bars[i].time + sec > endTime) i--;
    if (!replay) {
        stopLive();
        Replay r;
        r.index = i;
        r.follower = true;
        replay = r;
        removeReplayBar();
        auto *tag = box("replay-bar follower", this);
        tag->setProperty("follower", true);
        tag->setObjectName("qtmReplayBar");
        auto *h = new QHBoxLayout(tag);
        h->setContentsMargins(7, 5, 7, 5);
        auto *hint = label("Synced replay", "hint");
        hint->setMinimumHeight(28);
        h->addWidget(hint);
        tag->setAttribute(Qt::WA_TransparentForMouseEvents);
        auto *op = new QGraphicsOpacityEffect(tag);
        op->setOpacity(0.8);
        tag->setGraphicsEffect(op);
        replayBar = tag;
        tag->show();
        layoutOverlays();
    } else {
        replay->index = i;
    }
    render(false);
}

void ChartPane::replayStep(int dir)
{
    if (!replay) return;
    const int next = replay->index + dir;
    if (next < 1 || next >= bars.size()) {
        if (replay->playing) toggleReplay();
        return;
    }
    replay->index = next;
    render(false);
    app->onReplayBar(this, bars[next]);
}

void ChartPane::toggleReplay()
{
    if (!replay) return;
    replay->playing = !replay->playing;
    replayTimer->stop();
    if (replay->playing) replayTimer->start(int(1000 / replay->speed));
    if (replayPlayBtn) replayPlayBtn->setLabel(QString::fromUtf8(replay->playing ? "❚❚" : "▶"));
}

void ChartPane::stopReplay()
{
    replayTimer->stop();
    const bool wasFollower = replay && replay->follower;
    replay.reset();
    removeReplayBar();
    render(true);
    startLive();
    if (!wasFollower) app->onReplayStop(this);
}

// ---------------------------------------------------------------- export

QImage ChartPane::screenshot()
{
    QImage img = chart->takeScreenshot();
    // The label is drawn on the canvas as is, in device pixels (no devicePixelRatio scaling).
    const qreal dpr = img.devicePixelRatio();
    img.setDevicePixelRatio(1);
    {
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    p.setFont(theme::font(14, 600));
    p.setPen(css("--text"));
    p.drawText(QPointF(12, 22), QString::fromUtf8("%1 · %2").arg(theme::ticker(cfg.symbol), cfg.tf));
    }
    img.setDevicePixelRatio(dpr);
    return img;
}

void ChartPane::destroy()
{
    if (destroyed) return;
    destroyed = true;
    ++loadToken;
    stopLive();
    if (tradeFeed) tradeFeed->stop();
    if (depthFeed) depthFeed->stop();
    btTimer->stop();
    replayTimer->stop();
}

// ---------------------------------------------------------------- layout

void ChartPane::setActiveFrame(bool on)
{
    frameOn = on;
    frame->setVisible(on && app->panes.size() > 1);
    frame->raise();
    if (replayBar) replayBar->raise();
}

void ChartPane::resizeEvent(QResizeEvent *)
{
    layoutOverlays();
}

void ChartPane::layoutOverlays()
{
    const QRect r = rect();
    // The chart, and the range slider under it (full width; its track lines up with the time axis).
    const int navH = navigator ? ChartNavigator::HEIGHT : 0;
    chart->setGeometry(r.adjusted(0, 0, 0, -navH));
    if (navigator) navigator->setGeometry(0, r.height() - navH, r.width(), navH);
    if (legend) {
        legend->relayout();
    }
    if (msgEl) msgEl->setGeometry(r);
    if (frame) frame->setGeometry(r);
    if (replayBar) {
        squeezeReplayBar(replayBar, r.width() / 2.0);
        const QSize s = replayBar->sizeHint();
        replayBar->setGeometry((r.width() - s.width()) / 2, r.height() - navH - 36 - s.height(), s.width(), s.height());
        replayBar->raise();
    }
}

} // namespace qtm
