// Port of public/js/research.js: News (side tab), Seasonals and Financials (bottom tabs) for the
// active chart's symbol.

#include "Research.h"

#include "Api.h"
#include "PanelsPrivate.h"
#include "Theme.h"
#include "../chart/Chart.h"
#include "../core/JsCompat.h"
#include "../core/Seasonals.h"

#include <QDateTime>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHelpEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QToolTip>
#include <QUrl>
#include <QVBoxLayout>

#include <cmath>

namespace qtm::research {

using namespace qtm::kit;
using theme::esc;
using theme::fmtPct;
using theme::fmtPrice;

namespace {

QString enc(const QString &s)
{
    return QString::fromLatin1(QUrl::toPercentEncoding(s));
}

QString activeSymbol(App *app)
{
    return app->active ? app->active->cfg.symbol : QString();
}

// A text block with its own font and colour.
Text *styled(const QString &html, const QFont &f, const QColor &c, bool wrap = true, double lineHeight = 1.4)
{
    TextStyle st;
    st.font = f;
    st.color = c;
    st.wrap = wrap;
    st.lineHeight = lineHeight;
    return new Text(html, st);
}

// <div class="pad hint-text">...</div>
Text *padHint(const QString &html)
{
    Text *t = hint(html);
    t->setContentsMargins(12, 10, 12, 10);
    return t;
}

// research.js ago(t)
QString ago(double t)
{
    if (!js::truthy(t)) return QString();
    const double m = js::round((double(QDateTime::currentMSecsSinceEpoch()) - t) / 60000);
    if (m < 60) return QString("%1m ago").arg(js::numberToString(m));
    if (m < 1440) return QString("%1h ago").arg(js::numberToString(js::round(m / 60)));
    return localeDateString(t);
}

// Calls fn on a left click (the whole card is a link).
class ClickFilter : public QObject
{
public:
    ClickFilter(QObject *parent, std::function<void()> fn) : QObject(parent), m_fn(std::move(fn)) {}

protected:
    bool eventFilter(QObject *, QEvent *e) override
    {
        if (e->type() == QEvent::MouseButtonRelease && static_cast<QMouseEvent *>(e)->button() == Qt::LeftButton)
            m_fn();
        return false;
    }

private:
    std::function<void()> m_fn;
};

} // namespace

// ================================================================ news

void renderNews(panels::Body *el, App *app)
{
    const QString sym = activeSymbol(app);
    auto *root = block();
    auto *top = rowFlex();
    top->layout()->setContentsMargins(12, 10, 12, 10);
    top->layout()->addWidget(styled("<b>" + esc(theme::ticker(sym)) + "</b>", uiFont(13), theme::color("--text"), false));
    top->layout()->addWidget(spacer());
    auto *refresh = btn("Refresh", "btn small");
    top->layout()->addWidget(refresh);
    vbox(root)->addWidget(top);
    Text *note = padHint(QString::fromUtf8("Loading headlines…"));
    vbox(root)->addWidget(note);
    auto *list = new QWidget;
    auto *ll = new QVBoxLayout(list);
    ll->setContentsMargins(0, 0, 0, 0);
    ll->setSpacing(8);
    vbox(root)->addWidget(list);
    vbox(root)->addStretch(1);
    el->setContent(root);

    QPointer<panels::Body> elP(el);
    QObject::connect(refresh, &QPushButton::clicked, root, [elP, app] {
        if (elP) renderNews(elP, app);
    });
    QPointer<Text> noteP(note);
    QPointer<QWidget> listP(list);
    api::get(
        root, "/api/news?symbol=" + enc(sym),
        [app, sym, noteP, listP](const QJsonValue &v) {
            if (activeSymbol(app) != sym || !noteP || !listP) return;
            const QJsonObject r = v.toObject();
            noteP->setHtml(esc(r.value("note").toString()));
            const QJsonArray items = r.value("items").toArray();
            auto *l = static_cast<QVBoxLayout *>(listP->layout());
            if (!items.isEmpty()) l->setContentsMargins(8, 8, 8, 8);
            for (const auto &iv : items) {
                const QJsonObject n = iv.toObject();
                // <a class="card news">
                Box *c = card();
                c->hover = true;
                c->hoverBorder = theme::color("--accent");
                c->setCursor(Qt::PointingHandCursor);
                Text *title = styled(esc(n.value("title").toString()), uiFont(13, 600), theme::color("--text"), true, 1.35);
                c->layout()->addWidget(title);
                const double time = n.value("time").toDouble();
                const QString agoText = n.value("ago").toString();
                QString subText = esc(n.value("source").toString());
                if (js::truthy(time) || !agoText.isEmpty())
                    subText += " · " + esc(!agoText.isEmpty() ? agoText : ago(time));
                Text *sub = styled(subText, uiFont(11), theme::color("--muted"));
                sub->setContentsMargins(0, 2, 0, 0);
                c->layout()->addWidget(sub);
                const QString summary = n.value("summary").toString();
                if (!summary.isEmpty()) {
                    TextStyle st;
                    st.font = uiFont(11);
                    st.color = theme::color("--muted");
                    st.clamp = 2;
                    auto *sum = new Text(esc(summary), st);
                    sum->setContentsMargins(0, 2, 0, 0);   // .card .sub beats .news-sum
                    c->layout()->addWidget(sum);
                }
                for (QWidget *child : c->findChildren<QWidget *>()) child->setAttribute(Qt::WA_TransparentForMouseEvents);
                const QString url = n.value("url").toString();
                c->installEventFilter(new ClickFilter(c, [url] { openUrl(url); }));
                l->addWidget(c);
            }
        },
        [noteP](const QString &msg) {
            if (noteP) noteP->setHtml(esc("Headlines unavailable: " + msg));
        });
}

// ================================================================ seasonals

namespace {

const char *const PALETTE[] = {"#2962ff", "#ff9800", "#e91e63", "#00bcd4", "#8bc34a",
                               "#9c27b0", "#795548", "#607d8b", "#cddc39", "#3f51b5"};
const char *const MONTHS[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

// .seas: grid minmax(0, 1.3fr) minmax(0, 1fr) of the panel's height; the left column has a
// border-right. The chart keeps the height it had while the legend was empty (the browser sizes it
// before the legend is filled and doesn't shrink it after), so the filled legend hangs below the
// row: the left column (and with it the row) grows past the panel and the panel scrolls
// (.tab-body is overflow: auto).
// The grid sits in `page`, the panel's scroller; its row is the scroller's height.
class SeasGrid : public QWidget
{
public:
    explicit SeasGrid(QWidget *viewport) : m_vp(viewport)
    {
        left = new QWidget(this);
        auto *l = new QVBoxLayout(left);
        l->setContentsMargins(0, 0, 0, 0);
        l->setSpacing(0);
        right = new ScrollCol(QMargins(), this);
        legend = new QWidget(this);
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        viewport->installEventFilter(this);
        left->installEventFilter(this);
        legend->installEventFilter(this);
    }
    QWidget *left, *legend;
    QWidget *chartBox = nullptr;
    ScrollCol *right;

    QSize sizeHint() const override { return {0, contentH()}; }
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    int rowH() const { return m_vp ? m_vp->height() : 0; }
    // the grid row grows with the left column, so the right column follows it
    int contentH() const { return std::max(rowH(), legendTop() + legendH()); }
    int leftWidth() const { return int(std::round(width() * 1.3 / 2.3)); }
    int legendTop() const { return chartBox ? chartBox->geometry().bottom() + 1 : rowH(); }
    int legendH() const
    {
        const int h = legend->layout() ? legend->layout()->heightForWidth(std::max(1, leftWidth() - 1)) : 0;
        return h > 0 ? h : 12;
    }
    void place()
    {
        const int lw = leftWidth();
        left->setGeometry(0, 0, lw - 1, rowH());
        right->setGeometry(lw, 0, width() - lw, contentH());
        legend->setGeometry(0, legendTop(), lw - 1, legendH());
    }
    bool eventFilter(QObject *, QEvent *e) override
    {
        if (e->type() == QEvent::Resize || e->type() == QEvent::LayoutRequest) {
            place();
            if (height() != sizeHint().height()) updateGeometry();
        }
        return false;
    }
    void resizeEvent(QResizeEvent *) override { place(); }
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.fillRect(QRect(leftWidth() - 1, 0, 1, height()), theme::color("--border"));
    }

private:
    QPointer<QWidget> m_vp;
};

// A legend item: <span><i style="background:..."></i>text</span>
class LegendItem : public QWidget
{
public:
    LegendItem(const QColor &c, const QString &html) : m_color(c)
    {
        auto *l = new QHBoxLayout(this);
        l->setContentsMargins(15, 0, 0, 0);   // the 10px swatch + margin-right 5
        l->setSpacing(0);
        m_text = styled(html, uiFont(12), theme::color("--text"), false);
        l->addWidget(m_text);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        // vertical-align: middle: the middle of the swatch at the baseline + half the x-height
        const QFont f = uiFont(12);
        const QFontMetricsF fm(f);
        const double lh = 12 * 1.4;
        const double base = (lh - (std::round(fm.ascent()) + std::round(fm.descent()))) / 2 + std::round(fm.ascent());
        const double mid = base - fm.xHeight() / 2;
        p.fillRect(QRectF(0, mid - 1.5, 10, 3), m_color);
    }

private:
    QColor m_color;
    Text *m_text;
};

QString signedFixed1(double v)
{
    return QString("%1%2").arg(v >= 0 ? "+" : "", js::toFixed(v, 1));
}

kit::Cell seasCell(const QJsonValue &v)
{
    kit::Cell c;
    if (!v.isDouble()) {
        c.html = QString::fromUtf8("·");
        c.cls = "muted";
        return c;
    }
    const double x = v.toDouble();
    const double a = std::min(1.0, std::abs(x) / 15);
    const double alpha = 0.12 + a * 0.55;
    c.bg = theme::color(x >= 0 ? "--up" : "--down", alpha);
    c.html = signedFixed1(x);
    return c;
}

} // namespace

void renderSeasonals(panels::Body *el, App *app, int showYears)
{
    const QString sym = activeSymbol(app);
    auto *page = new ScrollCol(QMargins());
    auto *grid = new SeasGrid(page->viewport());
    page->layout()->addWidget(grid);
    auto *ll = vbox(grid->left);
    auto *top = rowFlex();
    top->layout()->setContentsMargins(12, 10, 12, 4);
    top->layout()->addWidget(styled("<b>" + esc(theme::ticker(sym)) + " by calendar year</b>", uiFont(13),
                                    theme::color("--text"), false));
    auto *years = new kit::Select(kit::Select::Field);
    for (int n : {3, 6, 10, 20}) years->addOption(QString::number(n), QString::number(n));
    years->setValue(QString::number(showYears));
    top->layout()->addWidget(fieldRow("Years", years));
    Text *note = hint(QString::fromUtf8("Loading daily history…"));
    top->layout()->addWidget(note);
    ll->addWidget(top);
    auto *chartBox = new QWidget;
    chartBox->setMinimumHeight(160);
    auto *cbl = new QVBoxLayout(chartBox);
    cbl->setContentsMargins(0, 0, 0, 0);
    ll->addWidget(chartBox, 1);
    // the empty legend's padding, in the row (see SeasGrid)
    ll->addSpacing(12);
    grid->chartBox = chartBox;
    QWidget *legend = grid->legend;
    auto *lgl = new FlowLayout(legend, 12, 4);
    lgl->setContentsMargins(12, 6, 12, 6);
    el->setContent(page, true);

    QPointer<panels::Body> elP(el);
    QObject::connect(years, &kit::Select::changed, grid, [elP, app](const QString &v) {
        if (elP) renderSeasonals(elP, app, v.toInt());
    });
    QPointer<Text> noteP(note);
    QPointer<SeasGrid> gridP(grid);
    QPointer<QWidget> chartP(chartBox), legendP(legend);
    api::candles(
        grid, sym, "1D", 8000,
        [=](const CandleSet &cs) {
            if (activeSymbol(app) != sym || !gridP) return;
            const QJsonObject s = seasonals(cs.candles);
            const QJsonArray yearsArr = s.value("years").toArray();
            const double completeYears = s.value("completeYears").toDouble();
            noteP->setHtml(QString("%1 years of daily bars · average of %2 complete years")
                               .arg(yearsArr.size())
                               .arg(js::numberToString(completeYears)));
            const double T0 = 946684800;   // Date.UTC(2000, 0, 1) / 1000: a leap year, so 29 February has a slot
            lw::ChartOptions o;
            o.background = theme::color("--bg");
            o.textColor = theme::color("--text");
            o.fontSize = 10;
            o.vertLinesColor = theme::color("--grid");
            o.horzLinesColor = theme::color("--grid");
            o.rightPriceScaleBorderColor = theme::color("--border");
            o.timeScaleBorderColor = theme::color("--border");
            o.crosshairLineColor = theme::color("--crosshair");
            o.crosshairLabelBackgroundColor = theme::color("--crosshair-label");
            if (theme::hasPalette()) o.labelTextColor = [](const QColor &bg) { return theme::textOn(bg); };
            o.tickMarkFormatter = [](double t) { return monthDayUtc(t); };
            o.timeFormatter = [](double t) { return monthDayUtc(t); };
            o.priceFormatter = [](double p) { return QString("%1%2%").arg(p >= 0 ? "+" : "", js::toFixed(p, 1)); };
            auto *chart = new lw::Chart(o, chartP);
            chartP->layout()->addWidget(chart);
            const int from = std::max(0, int(yearsArr.size()) - showYears);
            const int thisYear = QDateTime::currentDateTimeUtc().date().year();
            int k = 0;
            for (int i = from; i < yearsArr.size(); ++i, ++k) {
                const QJsonObject y = yearsArr[i].toObject();
                const int year = y.value("year").toInt();
                const bool current = year == thisYear;
                const QColor color = current ? theme::color("--text") : QColor(PALETTE[k % 10]);
                lw::SeriesOptions so;
                so.color = color;
                so.lineWidth = current ? 3 : 1;
                so.priceLineVisible = false;
                so.lastValueVisible = false;
                so.crosshairMarkerVisible = false;
                lw::Series *line = chart->addSeries(lw::SeriesType::Line, so);
                QVector<lw::DataItem> data;
                QSet<int> seen;
                for (const auto &pv : y.value("points").toArray()) {
                    const QJsonObject p = pv.toObject();
                    const int doy = p.value("doy").toInt();
                    if (seen.contains(doy)) continue;
                    seen.insert(doy);
                    lw::DataItem d;
                    d.time = T0 + (doy - 1) * 86400.0;
                    d.value = p.value("pct").toDouble();
                    data.push_back(d);
                }
                line->setData(data);
                const QJsonValue total = y.value("total");
                const double tot = total.isDouble() ? total.toDouble() : 0;
                legendP->layout()->addWidget(new LegendItem(
                    color, QString("%1 <b class=\"%2\">%3</b>")
                               .arg(year)
                               .arg(tot >= 0 ? "up" : "down", fmtPct(total.isDouble() ? total.toDouble() : NaN))));
            }
            const QJsonArray average = s.value("average").toArray();
            if (!average.isEmpty()) {
                lw::SeriesOptions so;
                so.color = theme::color("--crosshair");   // #9598a1
                so.lineWidth = 2;
                so.lineStyle = lw::LineStyle::Dashed;
                so.priceLineVisible = false;
                so.lastValueVisible = false;
                lw::Series *avg = chart->addSeries(lw::SeriesType::Line, so);
                QVector<lw::DataItem> data;
                for (const auto &pv : average) {
                    const QJsonObject p = pv.toObject();
                    lw::DataItem d;
                    d.time = T0 + (p.value("doy").toInt() - 1) * 86400.0;
                    d.value = p.value("pct").toDouble();
                    data.push_back(d);
                }
                avg->setData(data);
                legendP->layout()->addWidget(new LegendItem(
                    theme::color("--crosshair"), QString("Average of %1 years").arg(js::numberToString(completeYears))));
            }
            chart->timeScale()->fitContent();

            // the monthly table
            auto *table = new kit::Table;
            table->setVariant(kit::Table::Seas);
            QVector<kit::Cell> head;
            head << kit::Cell {"Year"};
            for (const char *m : MONTHS) head << kit::Cell {m};
            head << kit::Cell {"Year"};
            table->setHeader(head);
            QVector<kit::Row> rows;
            const QJsonArray monthly = s.value("monthly").toArray();
            for (int i = monthly.size() - 1; i >= 0; --i) {
                const QJsonObject r = monthly[i].toObject();
                kit::Row row;
                kit::Cell yc;
                yc.html = QString("<b>%1</b>").arg(r.value("year").toInt());
                yc.cls = "l";
                row.cells << yc;
                for (const auto &mv : r.value("months").toArray()) row.cells << seasCell(mv);
                row.cells << seasCell(r.value("total"));
                rows << row;
            }
            const QJsonArray monthAvg = s.value("monthAvg").toArray();
            kit::Row avgRow, upRow;
            kit::Cell a0;
            a0.html = "Average";
            a0.cls = "l";
            avgRow.cells << a0;
            kit::Cell u0;
            u0.html = "% of years up";
            u0.cls = "l";
            upRow.cells << u0;
            for (const auto &mv : monthAvg) {
                avgRow.cells << seasCell(mv.isObject() ? mv.toObject().value("avg") : QJsonValue(QJsonValue::Null));
                kit::Cell uc;
                uc.html = mv.isObject() ? QString("%1%").arg(js::numberToString(js::round(mv.toObject().value("upPct").toDouble())))
                                        : QString::fromUtf8("·");
                upRow.cells << uc;
            }
            avgRow.cells << kit::Cell {};
            upRow.cells << kit::Cell {};
            for (kit::Row *r : {&avgRow, &upRow})
                for (auto &c : r->cells) {
                    c.bold = true;
                    c.topBorder = true;
                }
            rows << avgRow << upRow;
            table->setRows(rows);
            auto *rl = gridP->right->layout();
            rl->addWidget(table);
            Text *foot = hint("Monthly moves in %. Past seasonality doesn't make next year's move.");
            foot->setContentsMargins(12, 22, 12, 22);
            rl->addWidget(foot);
            rl->addStretch(1);
        },
        [noteP](const QString &msg) {
            if (noteP) noteP->setHtml(esc(msg));
        });
}

// ================================================================ financials

namespace {

// research.js toNum(v): parseFloat of the text without $ , ( ); "(...)" is negative.
std::optional<double> toNum(const QJsonValue &v)
{
    QString str = v.isDouble() ? js::numberToString(v.toDouble())
                  : (v.isNull() || v.isUndefined()) ? QString()
                                                     : v.toVariant().toString();
    str = str.trimmed();
    QString clean = str;
    clean.remove(QRegularExpression("[$,()]"));
    static const QRegularExpression num(R"(^\s*([+-]?(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d+)?))");
    const auto m = num.match(clean);
    if (!m.hasMatch()) return std::nullopt;
    const double n = m.captured(1).toDouble();
    if (!std::isfinite(n)) return std::nullopt;
    return str.startsWith('(') ? -std::abs(n) : n;
}

QString money(const std::optional<double> &v)
{
    return v ? QString("$%1B").arg(js::toFixed(*v / 1e6, 1)) : QString::fromUtf8("—");
}

// .fin-bars: one column per period, revenue (accent) and net income (up / down) bars.
class FinBars : public QWidget
{
public:
    struct Col {
        QString p;
        std::optional<double> r, n;
    };
    explicit FinBars(const QVector<Col> &cols) : m_cols(cols)
    {
        setFixedHeight(150);
        setMouseTracking(true);
        m_max = 1;
        for (const Col &c : cols) m_max = std::max(m_max, std::abs(c.r.value_or(0)));
    }

protected:
    struct Geo {
        QRectF rev, ni;
        double x0, w;
    };
    Geo geoOf(int i) const
    {
        const int n = m_cols.size();
        const double cw = (width() - 10.0 * (n - 1)) / n;
        const double x0 = i * (cw + 10);
        const double stackH = height() - 2 * 10 * 1.4;
        const double bw = cw * 0.38;
        const double total = bw * 2 + 3;
        const double bx = x0 + (cw - total) / 2;
        const Col &c = m_cols[i];
        const double hr = std::max(1.0, std::abs(c.r.value_or(0)) / m_max * stackH);
        const double hn = std::max(1.0, std::abs(c.n.value_or(0)) / m_max * stackH);
        return {QRectF(bx, stackH - hr, bw, hr), QRectF(bx + bw + 3, stackH - hn, bw, hn), x0, cw};
    }
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QFont f = uiFont(10);
        const QFontMetricsF fm(f);
        const double lh = 14;
        const double base = (lh - (std::round(fm.ascent()) + std::round(fm.descent()))) / 2 + std::round(fm.ascent());
        auto bar = [&](const QRectF &r, const QColor &c) {
            QPainterPath path;
            const double rad = std::min(3.0, std::min(r.width(), r.height()) / 2);
            path.moveTo(r.left(), r.bottom());
            path.lineTo(r.left(), r.top() + rad);
            path.quadTo(r.left(), r.top(), r.left() + rad, r.top());
            path.lineTo(r.right() - rad, r.top());
            path.quadTo(r.right(), r.top(), r.right(), r.top() + rad);
            path.lineTo(r.right(), r.bottom());
            path.closeSubpath();
            p.fillPath(path, c);
        };
        for (int i = 0; i < m_cols.size(); ++i) {
            const Geo g = geoOf(i);
            const Col &c = m_cols[i];
            bar(g.rev, theme::color("--accent"));
            bar(g.ni, c.n && *c.n < 0 ? theme::color("--down") : theme::color("--up"));
            p.setFont(f);
            p.setPen(theme::color("--text"));
            const double top = height() - 2 * lh;
            for (int k = 0; k < 2; ++k) {
                const QString s = k == 0 ? c.p : money(c.r) + " · " + money(c.n);
                const QString el = fm.elidedText(s, Qt::ElideRight, g.w);
                const double tw = fm.horizontalAdvance(el);
                p.drawText(QPointF(g.x0 + (g.w - tw) / 2, top + k * lh + base), el);
            }
        }
    }
    bool event(QEvent *e) override
    {
        if (e->type() == QEvent::ToolTip) {
            auto *he = static_cast<QHelpEvent *>(e);
            for (int i = 0; i < m_cols.size(); ++i) {
                const Geo g = geoOf(i);
                if (g.rev.contains(he->pos())) {
                    QToolTip::showText(he->globalPos(), "Revenue " + money(m_cols[i].r), this);
                    return true;
                }
                if (g.ni.contains(he->pos())) {
                    QToolTip::showText(he->globalPos(), "Net income " + money(m_cols[i].n), this);
                    return true;
                }
            }
            QToolTip::hideText();
            e->ignore();
            return true;
        }
        return QWidget::event(e);
    }

private:
    QVector<Col> m_cols;
    double m_max;
};

} // namespace

void renderFinancials(panels::Body *el, App *app, const QString &freq, const QString &tab)
{
    const QString sym = activeSymbol(app);
    if (theme::exchange(sym) != "US") {
        el->setContent(padHint("Company financials are for US-listed stocks. Open one (AAPL, NVDA, JPM…) to see its "
                               "income statement, balance sheet, cash flow and key stats."));
        return;
    }
    Text *loading = padHint(QString::fromUtf8("Loading financials…"));
    el->setContent(loading);
    QPointer<panels::Body> elP(el);
    api::get(
        loading, "/api/financials?symbol=" + enc(sym) + "&frequency=" + freq,
        [elP, app, sym, freq, tab](const QJsonValue &v) {
            if (activeSymbol(app) != sym || !elP) return;
            const QJsonObject f = v.toObject();
            const QVector<QPair<QString, QPair<QString, QJsonValue>>> tables = {
                {"income", {"Income statement", f.value("income")}},
                {"balance", {"Balance sheet", f.value("balance")}},
                {"cashflow", {"Cash flow", f.value("cashflow")}},
                {"ratios", {"Ratios", f.value("ratios")}},
            };
            QJsonValue tv;
            for (const auto &t : tables)
                if (t.first == tab) tv = t.second.second;
            const QJsonObject t = tv.toObject();
            const bool hasT = tv.isObject();
            // revenue and net income bars, oldest first
            const QJsonObject income = f.value("income").toObject();
            const QJsonArray incRows = income.value("rows").toArray();
            auto find = [&](const QRegularExpression &re) -> std::optional<QJsonObject> {
                for (const auto &r : incRows)
                    if (re.match(r.toObject().value("label").toString()).hasMatch()) return r.toObject();
                return std::nullopt;
            };
            const auto rev = find(QRegularExpression("^total revenue", QRegularExpression::CaseInsensitiveOption));
            auto ni = find(QRegularExpression("^net income$", QRegularExpression::CaseInsensitiveOption));
            if (!ni) ni = find(QRegularExpression("net income", QRegularExpression::CaseInsensitiveOption));

            auto *split = new Split;
            // ---- left
            auto *L = split->left->layout();
            L->addWidget(h4(esc(theme::ticker(sym)) + " key stats"));
            L->addSpacing(4);
            auto *stats = new QWidget;
            auto *sg = new QGridLayout(stats);
            sg->setContentsMargins(0, 0, 0, 0);
            sg->setHorizontalSpacing(12);
            sg->setVerticalSpacing(6);
            sg->setColumnStretch(0, 1);
            sg->setColumnStretch(1, 1);
            int i = 0;
            for (const auto &sv : f.value("stats").toArray()) {
                const QJsonObject s = sv.toObject();
                auto *cell = new QWidget;
                auto *cl = new QVBoxLayout(cell);
                cl->setContentsMargins(0, 0, 0, 0);
                cl->setSpacing(0);
                cl->addWidget(styled(esc(s.value("label").toString()), uiFont(12), theme::color("--muted")));
                cl->addWidget(styled("<b>" + esc(s.value("value").toVariant().toString()) + "</b>", uiFont(12),
                                     theme::color("--text")));
                sg->addWidget(cell, i / 2, i % 2, Qt::AlignTop);
                ++i;
            }
            L->addWidget(stats);
            const QString nextEarnings = f.value("nextEarnings").toString();
            if (!nextEarnings.isEmpty()) {
                L->addSpacing(12);
                L->addWidget(hint(esc(nextEarnings)));
            }
            if (rev && !income.isEmpty()) {
                QVector<FinBars::Col> cols;
                const QJsonArray periods = income.value("periods").toArray();
                const QJsonArray rv = rev->value("values").toArray();
                const QJsonArray nv = ni ? ni->value("values").toArray() : QJsonArray();
                for (int k = 0; k < periods.size(); ++k)
                    cols.push_back({periods[k].toVariant().toString(), toNum(rv.at(k)),
                                    ni ? toNum(nv.at(k)) : std::nullopt});
                std::reverse(cols.begin(), cols.end());
                L->addSpacing(12);
                if (!cols.isEmpty()) L->addWidget(new FinBars(cols));
                L->addSpacing(4);
                L->addWidget(hint("Blue: revenue. Green: net income."));
            }
            L->addStretch(1);

            // ---- right (flex column, gap 10)
            auto *R = split->right->layout();
            auto *row = rowFlex();
            for (const auto &tb : tables) {
                auto *b = btn(tb.second.first, tb.first == tab ? "btn small primary" : "btn small");
                b->setEnabled(tb.second.second.isObject());
                row->layout()->addWidget(b);
                const QString key = tb.first;
                QObject::connect(b, &QPushButton::clicked, b, [elP, app, freq, key] {
                    if (elP) QMetaObject::invokeMethod(elP, [elP, app, freq, key] {
                        if (elP) renderFinancials(elP, app, freq, key);
                    }, Qt::QueuedConnection);
                });
            }
            row->layout()->addWidget(spacer());
            auto *fs = new kit::Select(kit::Select::In);
            fs->addOption("annual", "Annual");
            fs->addOption("quarterly", "Quarterly");
            fs->setValue(freq);
            row->layout()->addWidget(fs);
            QObject::connect(fs, &kit::Select::changed, fs, [elP, app, tab](const QString &value) {
                if (elP) QMetaObject::invokeMethod(elP, [elP, app, tab, value] {
                    if (elP) renderFinancials(elP, app, value, tab);
                }, Qt::QueuedConnection);
            });
            R->addWidget(row);
            R->addSpacing(10);
            if (hasT) {
                auto *table = new kit::Table;
                table->setWrap(true);
                QVector<kit::Cell> head;
                kit::Cell h0;
                h0.cls = "l";
                head << h0;
                for (const auto &p : t.value("periods").toArray()) head << kit::Cell {esc(p.toVariant().toString())};
                table->setHeader(head);
                QVector<kit::Row> rows;
                for (const auto &rv : t.value("rows").toArray()) {
                    const QJsonObject r = rv.toObject();
                    kit::Row row2;
                    kit::Cell c0;
                    c0.html = esc(r.value("label").toString());
                    c0.cls = "l";
                    row2.cells << c0;
                    for (const auto &x : r.value("values").toArray())
                        row2.cells << kit::Cell {esc(x.isDouble() ? js::numberToString(x.toDouble()) : x.toVariant().toString())};
                    rows << row2;
                }
                table->setRows(rows);
                R->addWidget(table);
                R->addSpacing(10 + 12);
            } else {
                R->addSpacing(12);
                R->addWidget(hint("Not available for this company."));
                R->addSpacing(12 + 10 + 12);
            }
            R->addWidget(hint(esc(f.value("note").toString()) + " Source: Nasdaq."));
            R->addSpacing(12);
            R->addStretch(1);
            elP->setContent(split, true);
        },
        [elP](const QString &msg) {
            if (elP) elP->setContent(padHint(esc(msg)));
        });
}

} // namespace qtm::research
