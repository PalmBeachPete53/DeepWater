// Port of public/js/panels.js: initPanels(app) -> qtm::Panels. This file has the tab bars, the
// side panel (watchlist, alerts, calendar; news is in Research.cpp), the alert dialog and the
// hooks the app calls. The bottom tabs live in PanelsTester.cpp, PanelsMarket.cpp,
// PanelsPaper.cpp, PanelsEditor.cpp, Research.cpp and Ptq.cpp.

#include "PanelsPrivate.h"

#include "Api.h"
#include "Notifications.h"
#include "Ptq.h"
#include "Research.h"
#include "Theme.h"
#include "../core/JsCompat.h"

#include <QApplication>
#include <QCheckBox>
#include <QDateTime>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QSet>
#include <QSettings>
#include <QToolButton>
#include <QSvgRenderer>
#include <QToolTip>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>

namespace qtm {

using namespace qtm::kit;
using theme::esc;
using theme::fmtNum;
using theme::fmtPct;
using theme::fmtPrice;

namespace panels {

// ================================================================ tab buttons, frames, body

TabButton::TabButton(const QString &text, const QString &t, QWidget *parent) : QAbstractButton(parent), tab(t)
{
    setText(text);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
    ptq = t == "ptq";
}

QSize TabButton::sizeHint() const
{
    if (icon) return {30, 30};
    const QFontMetricsF fm(uiFont(13, ptq ? 600 : 400));
    return {int(std::ceil(fm.horizontalAdvance(text()) + 20)), 30};
}

void TabButton::enterEvent(QEnterEvent *)
{
    m_hover = true;
    update();
}

void TabButton::leaveEvent(QEvent *)
{
    m_hover = false;
    update();
}

void TabButton::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    if (active || m_hover) {
        QPainterPath path;
        path.addRoundedRect(QRectF(rect()), 4, 4);
        p.fillPath(path, theme::color("--hover"));
    }
    QColor c = (active || m_hover) ? theme::color("--text") : theme::color("--muted");
    if (ptq) c = QColor("#6cb8ff");
    if (icon) {
        // <svg viewBox="0 0 24 24"><path d="M6 15l6-6 6 6"/></svg>, 18px, stroke 1.6
        p.translate(width() / 2.0, height() / 2.0);
        if (rotated) p.rotate(180);
        p.translate(-9, -9);
        p.scale(18.0 / 24, 18.0 / 24);
        QPen pen(c, 1.6);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        p.setPen(pen);
        p.drawPolyline(QPolygonF({QPointF(6, 15), QPointF(12, 9), QPointF(18, 15)}));
        return;
    }
    const QFont f = uiFont(13, ptq ? 600 : 400);
    p.setFont(f);
    p.setPen(c);
    const QFontMetricsF fm(f);
    const double lh = 13 * 1.4;
    const double base = (height() - lh) / 2 + (lh - (std::round(fm.ascent()) + std::round(fm.descent()))) / 2
                        + std::round(fm.ascent());
    p.drawText(QPointF(10, base), text());
}

void Frame::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), theme::color("--bg"));
    if (m_edge == Left) p.fillRect(QRect(0, 0, 1, height()), theme::color("--border"));
    else p.fillRect(QRect(0, 0, width(), 1), theme::color("--border"));
}

Body::Body(QWidget *parent) : QWidget(parent)
{
    m_scroll = new ScrollCol(QMargins(), this);
    m_scroll->hide();
}

void Body::setContent(QWidget *content, bool fill)
{
    if (m_content) {
        m_content->hide();
        m_content->deleteLater();
    }
    m_content = content;
    if (!content) {
        m_scroll->hide();
        return;
    }
    if (fill) {
        m_scroll->hide();
        content->setParent(this);
        content->setGeometry(rect());
        content->show();
    } else {
        auto *col = static_cast<ScrollCol *>(m_scroll.data());
        col->layout()->addWidget(content);
        m_scroll->verticalScrollBar()->setValue(0);
        m_scroll->setGeometry(rect());
        m_scroll->show();
        content->show();
    }
}

void Body::resizeEvent(QResizeEvent *)
{
    m_scroll->setGeometry(rect());
    if (m_content && m_content->parentWidget() == this) m_content->setGeometry(rect());
}

// ================================================================ TesterState

QJsonValue TesterState::setting(const QString &k) const
{
    for (const auto &p : settings)
        if (p.first == k) return p.second;
    return QJsonValue::Undefined;
}

void TesterState::set(const QString &k, const QJsonValue &v)
{
    for (auto &p : settings)
        if (p.first == k) {
            p.second = v;
            return;
        }
    settings.push_back({k, v});
}

// ================================================================ JSON.stringify(v, null, 2)

namespace {
QString jsonString(const QString &s)
{
    QString out = "\"";
    for (const QChar c : s) {
        switch (c.unicode()) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        default:
            if (c.unicode() < 0x20) out += QString("\\u%1").arg(uint(c.unicode()), 4, 16, QChar('0'));
            else out += c;
        }
    }
    return out + "\"";
}
} // namespace

QString stringify(const QJsonValue &v, const QHash<QString, QStringList> &order, const QString &path, int indent)
{
    const QString pad(indent + 2, ' '), end(indent, ' ');
    if (v.isObject()) {
        const QJsonObject o = v.toObject();
        if (o.isEmpty()) return "{}";
        QStringList keys;
        for (const QString &k : order.value(path))
            if (o.contains(k) && !keys.contains(k)) keys << k;
        for (const QString &k : o.keys())
            if (!keys.contains(k)) keys << k;
        QStringList parts;
        for (const QString &k : keys) {
            const QJsonValue x = o.value(k);
            if (x.isUndefined()) continue;
            const QString childPath = path.isEmpty() ? k : path + "." + k;
            parts << pad + jsonString(k) + ": " + stringify(x, order, childPath, indent + 2);
        }
        return "{\n" + parts.join(",\n") + "\n" + end + "}";
    }
    if (v.isArray()) {
        const QJsonArray a = v.toArray();
        if (a.isEmpty()) return "[]";
        QStringList parts;
        for (const auto &x : a) parts << pad + stringify(x, order, path + "[]", indent + 2);
        return "[\n" + parts.join(",\n") + "\n" + end + "]";
    }
    if (v.isDouble()) {
        const double d = v.toDouble();
        return std::isfinite(d) ? js::numberToString(d) : QString("null");
    }
    if (v.isString()) return jsonString(v.toString());
    if (v.isBool()) return v.toBool() ? "true" : "false";
    return "null";
}

// ================================================================ watchlist row

// <div class="wl-row">: sym + ex | px | chg | x, painted (grid 1fr auto 64px 18px, gap 8, padding 6 10).
class WlRow : public QWidget
{
public:
    WlRow(const QString &sym, bool current, const QString &px, const QString &chg, bool up, QWidget *parent = nullptr)
        : QWidget(parent), m_sym(sym), m_current(current), m_px(px), m_chg(chg), m_up(up)
    {
        setMouseTracking(true);
        setCursor(Qt::PointingHandCursor);
        setFixedHeight(int(std::round(6 + 13 * 1.4 + 6 + 1)));
    }
    std::function<void()> onClick, onRemove;
    const QString &symbol() const { return m_sym; }
    // New quotes for the same symbol: the row is kept (rebuilding the list every 5 s laid it all out again).
    void set(bool current, const QString &px, const QString &chg, bool up)
    {
        if (current == m_current && px == m_px && chg == m_chg && up == m_up) return;
        m_current = current;
        m_px = px;
        m_chg = chg;
        m_up = up;
        update();
    }

protected:
    QRectF xRect() const { return QRectF(width() - 10 - 18, 0, 18, height() - 1); }
    void enterEvent(QEnterEvent *) override { m_hover = true; update(); }
    void leaveEvent(QEvent *) override { m_hover = false; update(); }
    void mousePressEvent(QMouseEvent *e) override
    {
        if (e->button() != Qt::LeftButton) return;
        if (xRect().contains(e->position())) {
            if (onRemove) onRemove();
        } else if (onClick) {
            onClick();
        }
    }
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        if (m_current) p.fillRect(rect(), theme::color("--accent-soft"));
        if (m_hover) p.fillRect(rect(), theme::color("--hover"));
        const double lh = 13 * 1.4, top = 6;
        QFont num = uiFont(13);
        num.setFeature(QFont::Tag("tnum"), 1);
        const QFontMetricsF fmN(num);
        auto baseline = [&](const QFont &f) {
            const QFontMetricsF fm(f);
            return top + (lh - (std::round(fm.ascent()) + std::round(fm.descent()))) / 2 + std::round(fm.ascent());
        };
        // columns from the right: x (18), chg (64), px (auto)
        const double right = width() - 10;
        const double xL = right - 18;
        const double chgR = xL - 8, chgL = chgR - 64;
        const double pxW = fmN.horizontalAdvance(m_px);
        const double pxR = chgL - 8, pxL = pxR - pxW;
        const double symL = 10, symR = pxL - 8;
        p.setFont(num);
        p.setPen(theme::color("--text"));
        p.drawText(QPointF(pxL, baseline(num)), m_px);
        p.setPen(m_up ? theme::color("--up") : theme::color("--down"));
        p.drawText(QPointF(chgR - fmN.horizontalAdvance(m_chg), baseline(num)), m_chg);
        // sym: ticker (600) + " " + exchange (11px muted), with text-overflow: ellipsis
        const QFont fs = uiFont(13, 600), fe = uiFont(11);
        const QString tick = theme::ticker(m_sym), ex = theme::exchange(m_sym);
        QTextLayout tl(tick + " " + ex, fs);
        QTextLayout::FormatRange fr;
        fr.start = tick.size() + 1;
        fr.length = ex.size();
        fr.format.setFont(fe);
        fr.format.setForeground(theme::color("--muted"));
        QTextLayout::FormatRange fr0;
        fr0.start = 0;
        fr0.length = tick.size() + 1;
        fr0.format.setForeground(theme::color("--text"));
        tl.setFormats({fr0, fr});
        tl.beginLayout();
        QTextLine line = tl.createLine();
        line.setLineWidth(1e6);
        line.setPosition(QPointF(0, 0));
        tl.endLayout();
        const double avail = symR - symL;
        const double by = baseline(fs);
        if (line.naturalTextWidth() <= avail) {
            line.draw(&p, QPointF(symL, by - line.ascent()));
        } else {
            const QFontMetricsF fm(fs);
            const double ell = fm.horizontalAdvance(QString(QChar(0x2026)));
            int cut = 0;
            for (int i = 1; i <= tl.text().size(); ++i) {
                if (line.cursorToX(i) + ell > avail) break;
                cut = i;
            }
            const double cutX = line.cursorToX(cut);
            p.save();
            p.setClipRect(QRectF(symL, 0, cutX, height()));
            line.draw(&p, QPointF(symL, by - line.ascent()));
            p.restore();
            p.setFont(fs);
            p.setPen(theme::color("--text"));
            p.drawText(QPointF(symL + cutX, by), QString(QChar(0x2026)));
        }
        if (m_hover) {
            // the x button: <svg viewBox="0 0 24 24"><path d="M6 6l12 12M18 6L6 18"/></svg>, 14px
            const QRectF xr = xRect();
            const double cx = xr.center().x(), cy = top + lh / 2;
            p.save();
            p.translate(cx - 7, cy - 7);
            p.scale(14.0 / 24, 14.0 / 24);
            QPen pen(theme::color("--muted"), 1.6);
            pen.setCapStyle(Qt::RoundCap);
            p.setPen(pen);
            p.drawLine(QPointF(6, 6), QPointF(18, 18));
            p.drawLine(QPointF(18, 6), QPointF(6, 18));
            p.restore();
        }
    }

private:
    QString m_sym;
    bool m_current;
    QString m_px, m_chg;
    bool m_up;
    bool m_hover = false;
};

// A side-panel h4 with class "pad" and style="margin:0;padding-bottom:0".
Text *padH4(const QString &s)
{
    Text *t = h4(s);
    t->setContentsMargins(12, 10, 12, 0);
    return t;
}

// <div class="pad hint-text">...</div>
Text *padHint(const QString &html)
{
    Text *t = hint(html);
    t->setContentsMargins(12, 10, 12, 10);
    return t;
}

// .card .sub
Text *sub(const QString &html, const QString &extraCls = QString())
{
    TextStyle st;
    st.font = uiFont(11);
    st.color = theme::color("--muted");
    if (extraCls.contains("down")) st.color = theme::color("--down");
    auto *t = new Text(html, st);
    t->setContentsMargins(0, 2, 0, 0);
    return t;
}

// A column of side cards (margin 8 each, collapsing between siblings).
QWidget *cardColumn()
{
    auto *w = new QWidget;
    auto *l = new QVBoxLayout(w);
    l->setContentsMargins(8, 0, 8, 0);
    l->setSpacing(8);
    return w;
}

} // namespace panels

using namespace panels;

// ================================================================ Panels

Panels::Panels(App *app) : QObject(app), d(std::make_unique<Private>())
{
    d->q = this;
    d->app = app;

    // <aside class="side">
    d->side = new Frame(Frame::Left);
    d->side->setFixedWidth(300);
    auto *sl = new QVBoxLayout(d->side);
    sl->setContentsMargins(1, 0, 0, 0);
    sl->setSpacing(0);
    // the tab bar's border-bottom
    class Bar : public QWidget
    {
    public:
        using QWidget::QWidget;

    protected:
        void paintEvent(QPaintEvent *) override
        {
            QPainter p(this);
            p.fillRect(QRect(0, height() - 1, width(), 1), theme::color("--border"));
        }
    };
    d->sideFull = new QWidget;
    auto *fl = new QVBoxLayout(d->sideFull);
    fl->setContentsMargins(0, 0, 0, 0);
    fl->setSpacing(0);
    auto *sideBar = new Bar;
    sideBar->setFixedHeight(38);
    {
        auto *l = new QHBoxLayout(sideBar);
        l->setContentsMargins(6, 0, 6, 1);
        l->setSpacing(2);
        for (const auto &[tab, label] : QVector<QPair<QString, QString>> {
                 {"watchlist", "Watchlist"}, {"alerts", "Alerts"}, {"calendar", "Calendar"}, {"news", "News"}}) {
            auto *b = new TabButton(label, tab);
            b->setFixedHeight(30);
            l->addWidget(b);
            d->sideTabs << b;
            // The open tab again folds the panel, as on TradingView.
            connect(b, &QAbstractButton::clicked, this, [this, tab] {
                if (tab == d->sideTab && !d->sideFolded) d->setSideFolded(true);
                else d->showSide(tab);
            });
        }
        l->addStretch(1);
        auto *fold = new QToolButton;
        fold->setAutoRaise(true);
        fold->setCursor(Qt::PointingHandCursor);
        fold->setToolTip("Hide the panel (click its tab again does the same)");
        fold->setFixedSize(26, 26);
        connect(fold, &QToolButton::clicked, this, [this] { d->setSideFolded(true); });
        l->addWidget(fold);
        l->addSpacing(18);   // clear of the window's side flaps
        d->foldButton = fold;
    }
    fl->addWidget(sideBar);
    d->sideBody = new Body;
    fl->addWidget(d->sideBody, 1);
    sl->addWidget(d->sideFull, 1);

    // The folded panel: one icon per tab, on the left of the rail so that the window's flaps (AI, DEV) don't cover them.
    d->sideRail = new QWidget;
    {
        auto *rl = new QVBoxLayout(d->sideRail);
        rl->setContentsMargins(3, 6, 0, 6);
        rl->setSpacing(4);
        for (const auto &[tab, label] : QVector<QPair<QString, QString>> {
                 {"watchlist", "Watchlist"}, {"alerts", "Alerts"}, {"calendar", "Calendar"}, {"news", "News"}}) {
            auto *b = new QToolButton;
            b->setAutoRaise(true);
            b->setCursor(Qt::PointingHandCursor);
            b->setToolTip(label);
            b->setFixedSize(30, 30);
            b->setProperty("tab", tab);
            connect(b, &QToolButton::clicked, this, [this, tab] {
                d->setSideFolded(false);
                d->showSide(tab);
            });
            rl->addWidget(b, 0, Qt::AlignLeft);
            d->railButtons << b;
        }
        rl->addStretch(1);
    }
    sl->addWidget(d->sideRail, 1);
    d->sideRail->hide();
    d->paintRail();

    // <div id="bottom" class="bottom collapsed">
    d->bottomEl = new Frame(Frame::Top);
    d->bottomEl->setFixedHeight(39);
    // .center has min-width: 0, so a narrow window clips the bottom panel instead of being held wide.
    d->bottomEl->setSizePolicy(QSizePolicy::Ignored, d->bottomEl->sizePolicy().verticalPolicy());
    d->bottomEl->setMinimumWidth(0);
    auto *bl = new QVBoxLayout(d->bottomEl);
    bl->setContentsMargins(0, 1, 0, 0);
    bl->setSpacing(0);
    // The bottom tab bar: tabs keep their width (white-space: nowrap) and the toggle sits at the
    // right edge, or right after the tabs when the panel is narrower than them (then it's clipped,
    // as the browser clips the overflowing flex row).
    class BottomBar : public Bar
    {
    public:
        QWidget *tabs = nullptr;
        QWidget *toggle = nullptr;

    protected:
        void resizeEvent(QResizeEvent *) override
        {
            tabs->setGeometry(6, 0, tabs->sizeHint().width(), 37);
            const int x = std::max(6 + tabs->width() + 2, width() - 6 - toggle->width());
            toggle->move(x, (37 - toggle->height()) / 2);
        }
        QSize minimumSizeHint() const override { return QSize(0, 38); }
    };
    auto *bottomBar = new BottomBar;
    bottomBar->setFixedHeight(38);
    {
        auto *tabsBox = new QWidget(bottomBar);
        auto *l = new QHBoxLayout(tabsBox);
        l->setContentsMargins(0, 0, 0, 0);
        l->setSpacing(2);
        for (const auto &[tab, label] : QVector<QPair<QString, QString>> {
                 {"tester", "Strategy tester"}, {"screener", "Screener"}, {"heatmap", "Heatmap"},
                 {"seasonals", "Seasonals"}, {"financials", "Financials"}, {"paper", "Paper trading"},
                 {"editor", "Indicator editor"}, {"terminal", "Terminal"}, {"ptq", "PTQ"}}) {
            auto *b = new TabButton(label, tab);
            b->setFixedHeight(30);
            b->active = tab == "tester";
            l->addWidget(b, 0, Qt::AlignVCenter);
            d->bottomTabs << b;
            connect(b, &QAbstractButton::clicked, this, [this, tab] { d->showBottom(tab, true); });
        }
        d->toggle = new TabButton(QString(), "toggle");
        d->toggle->setParent(bottomBar);
        d->toggle->icon = true;
        d->toggle->rotated = true;
        d->toggle->setFixedSize(30, 30);
        d->toggle->setToolTip("Show / hide panel");
        connect(d->toggle, &QAbstractButton::clicked, this, [this] {
            d->setCollapsed(!d->collapsed);
            if (!d->collapsed) d->renderBottom();
        });
        bottomBar->tabs = tabsBox;
        bottomBar->toggle = d->toggle;
    }
    bl->addWidget(bottomBar);
    d->bottom = new Body;
    d->bottom->hide();
    bl->addWidget(d->bottom, 1);

    d->initTester();
    d->initScreener();

    // Dragging the panel's top edge resizes it (while it is open).
    class Grip : public QWidget
    {
    public:
        Grip(Panels::Private *d) : QWidget(d->bottomEl), m_d(d)
        {
            setCursor(Qt::SizeVerCursor);
            d->bottomEl->installEventFilter(this);
            raise();
        }

    protected:
        bool eventFilter(QObject *, QEvent *e) override
        {
            if (e->type() == QEvent::Resize || e->type() == QEvent::ChildAdded) {
                setGeometry(0, 0, parentWidget()->width(), 4);
                setVisible(!m_d->collapsed);
                raise();
            }
            return false;
        }
        void mousePressEvent(QMouseEvent *e) override
        {
            m_grabY = e->globalPosition().y();
            m_grabH = parentWidget()->height();
        }
        void mouseMoveEvent(QMouseEvent *e) override
        {
            if (!(e->buttons() & Qt::LeftButton)) return;
            m_d->bottomHeight = qRound(m_grabH - (e->globalPosition().y() - m_grabY));
            m_d->applySizes();
            m_d->bottomHeight = parentWidget()->height();
        }
        void mouseReleaseEvent(QMouseEvent *) override { QSettings().setValue("chartsBottomHeight", m_d->bottomHeight); }

    private:
        Panels::Private *m_d;
        double m_grabY = 0;
        int m_grabH = 0;
    };
    d->bottomHeight = QSettings().value("chartsBottomHeight", 320).toInt();
    if (QSettings().value("chartsSideFolded", false).toBool()) d->setSideFolded(true);
    new Grip(d.get());

    // The media query follows the window (the App) and the bottom's column height.
    class Sizes : public QObject
    {
    public:
        Sizes(Panels::Private *d) : QObject(d->q), m_d(d), m_app(d->app), m_side(d->side), m_bottom(d->bottomEl)
        {
            d->app->installEventFilter(this);
            d->bottomEl->installEventFilter(this);
        }

    protected:
        bool eventFilter(QObject *o, QEvent *e) override
        {
            if (!m_app || !m_side || !m_bottom) return false;   // tearing down
            if (o == m_bottom && e->type() == QEvent::ParentChange && m_bottom->parentWidget()) {
                if (m_parent) m_parent->removeEventFilter(this);
                m_parent = m_bottom->parentWidget();
                m_parent->installEventFilter(this);
                m_d->applySizes();
            }
            if ((o == m_app || (m_parent && o == m_parent)) && e->type() == QEvent::Resize) m_d->applySizes();
            return false;
        }

    private:
        Panels::Private *m_d;
        QPointer<QWidget> m_app, m_side, m_bottom, m_parent;
    };
    new Sizes(d.get());
    d->applySizes();

    QPointer<Panels> self(this);
    app->priceListeners.push_back([self](const QString &sym, double px) {
        if (!self) return;
        Private *d = self->d.get();
        d->checkOrders(sym, px);
        const QJsonObject paper = d->app->state.paper.toObject();
        if (d->bottomTab == "paper" && !d->collapsed && paper.value("positions").toObject().contains(sym))
            d->drawPaperNumbers();
    });
    connect(&d->quoteTimer, &QTimer::timeout, this, [this] { d->refreshQuotes(); });
    if (!app->livePaused())
        d->quoteTimer.start(5000);
    d->refreshQuotes();
    d->showSide("watchlist");
    // The system answers the notification permission asynchronously (and the user can change it in
    // System Settings): keep the "Desktop notifications" button in step.
    connect(Notifications::instance(), &Notifications::permissionChanged, this, [this] {
        if (d->sideTab == "alerts") d->renderAlerts();
    });
}

Panels::~Panels()
{
    if (d->side && !d->side->parent()) delete d->side;
    if (d->bottomEl && !d->bottomEl->parent()) delete d->bottomEl;
}

QWidget *Panels::sideWidget() const { return d->side; }
QWidget *Panels::bottomWidget() const { return d->bottomEl; }
void Panels::showSide(const QString &tab) { d->showSide(tab); }
void Panels::showBottom(const QString &tab, bool toggle) { d->showBottom(tab, toggle); }

void Panels::toggleTerminal()
{
    if (!d->collapsed && d->bottomTab == "terminal" && d->terminal && d->terminal->isAncestorOf(QApplication::focusWidget())) {
        d->setCollapsed(true);
        d->app->setFocus(Qt::ShortcutFocusReason);
        return;
    }
    if (d->collapsed || d->bottomTab != "terminal") d->showBottom("terminal", false);
    if (d->terminal) d->terminal->focusCurrent();
}
void Panels::openAlertDialog(const QJsonObject &preset) { d->openAlertDialog(preset); }

void Panels::onActiveChange()
{
    if (d->sideTab == "watchlist") d->drawRows();
    const QString sym = d->app->active ? d->app->active->cfg.symbol : QString();
    if (!d->lastSymbolSet || sym != d->lastSymbol) {
        d->lastSymbol = sym;
        d->lastSymbolSet = true;
        if (d->sideTab == "news") research::renderNews(d->sideBody, d->app);
        if (!d->collapsed && d->bottomTab == "seasonals") research::renderSeasonals(d->bottom, d->app);
        if (!d->collapsed && d->bottomTab == "financials") research::renderFinancials(d->bottom, d->app);
    }
    if (d->bottomTab == "paper" && !d->collapsed) d->renderPaper();
}

void Panels::onReplayChange()
{
    if (d->bottomTab == "paper" && !d->collapsed) d->renderPaper();
}

void Panels::onReplayBar(ChartPane *pane, const Bar &bar)
{
    if (pane != d->app->active) return;
    d->checkReplayOrders(pane, bar);
    if (d->bottomTab == "paper" && !d->collapsed
        && (!d->paperRenderAt.isValid() || d->paperRenderAt.elapsed() > 250)) {
        d->paperRenderAt.start();
        d->renderPaper();
    }
}

void Panels::onAlertFired(const QJsonObject &)
{
    if (d->sideTab == "alerts") d->renderAlerts();
}

void Panels::setQuotesPaused(bool paused)
{
    if (paused) {
        d->quoteTimer.stop();
    } else if (!d->quoteTimer.isActive()) {
        d->refreshQuotes();
        d->quoteTimer.start(5000);
    }
}

void Panels::onTheme()
{
    // CSS variables update the side panel by themselves in the browser; here it is redrawn.
    d->side->update();
    d->paintRail();
    d->bottomEl->update();
    d->renderSide();
    d->renderBottom();
    if (d->terminal) d->terminal->refreshTheme();
}

// ---------------------------------------------------------------- tabs

void Panels::Private::showSide(const QString &t)
{
    sideTab = t;
    for (TabButton *b : sideTabs) {
        b->active = b->tab == t;
        b->update();
    }
    renderSide();
}

void Panels::Private::setSideFolded(bool on)
{
    sideFolded = on;
    side->setFixedWidth(on ? 56 : 300);   // the window's flaps cover its right 22 px
    sideFull->setVisible(!on);
    sideRail->setVisible(on);
    for (QAbstractButton *b : railButtons) {
        // The tab the panel would open on stands out.
        const bool current = b->property("tab").toString() == sideTab;
        b->setStyleSheet(current ? QString("QToolButton { background: %1; border-radius: 4px; }").arg(theme::css("--hover")) : QString());
    }
    QSettings().setValue("chartsSideFolded", on);
    if (!on) renderSide();
}

void Panels::Private::paintRail()
{
    static const QHash<QString, QString> icons {
        {"watchlist", R"(<path d="M8 6h13M8 12h13M8 18h13M3 6h.01M3 12h.01M3 18h.01"/>)"},
        {"alerts", R"(<path d="M6 8a6 6 0 0 1 12 0c0 7 3 9 3 9H3s3-2 3-9"/><path d="M10.3 21a1.94 1.94 0 0 0 3.4 0"/>)"},
        {"calendar", R"(<rect x="3" y="4" width="18" height="18" rx="2"/><path d="M16 2v4M8 2v4M3 10h18"/>)"},
        {"news", R"(<path d="M4 22h16a2 2 0 0 0 2-2V4a2 2 0 0 0-2-2H8a2 2 0 0 0-2 2v16a2 2 0 0 1-2 2Zm0 0a2 2 0 0 1-2-2v-9c0-1.1.9-2 2-2h2"/><path d="M18 14h-8M15 18h-5M10 6h8v4h-8V6Z"/>)"},
    };
    const QColor c = theme::color("--muted");
    for (QAbstractButton *b : railButtons) {
        b->setIcon(theme::svgIcon(icons.value(b->property("tab").toString()), c, 18));
        b->setIconSize(QSize(18, 18));
    }
    if (foldButton) {
        foldButton->setIcon(theme::svgIcon(R"(<path d="m6 17 5-5-5-5M13 17l5-5-5-5"/>)", c, 16));
        foldButton->setIconSize(QSize(16, 16));
    }
}

void Panels::Private::setCollapsed(bool on)
{
    collapsed = on;
    applySizes();
    // The Terminal tab shows its own widget, kept alive with its shells, instead of the tab body.
    if (!on && bottomTab == "terminal" && !terminal) {
        terminal = new TerminalPanel;
        static_cast<QVBoxLayout *>(bottomEl->layout())->addWidget(terminal, 1);
    }
    bottom->setVisible(!on && bottomTab != "terminal");
    if (terminal) terminal->setVisible(!on && bottomTab == "terminal");
    toggle->rotated = on;
    toggle->update();
}

// .bottom { height: 320px } / .bottom.collapsed { height: 39px }, and @media (max-width: 900px):
// .side { display: none } .bottom { height: 45% } (of the column it sits in).
void Panels::Private::applySizes()
{
    const bool narrow = app->width() <= 900;
    side->setVisible(!narrow);
    int h = bottomHeight;
    if (bottomEl->parentWidget()) h = std::clamp(h, 120, std::max(120, bottomEl->parentWidget()->height() - 160));
    if (narrow && bottomEl->parentWidget()) h = qRound(bottomEl->parentWidget()->height() * 0.45);
    if (collapsed) h = 39;
    if (bottomEl->height() != h || bottomEl->minimumHeight() != h) bottomEl->setFixedHeight(h);
}

void Panels::Private::showBottom(const QString &t, bool toggleIt)
{
    if (toggleIt && t == bottomTab && !collapsed) {
        setCollapsed(true);
        return;
    }
    bottomTab = t;
    setCollapsed(false);
    for (TabButton *b : bottomTabs) {
        b->active = b->tab == t;
        b->update();
    }
    renderBottom();
}

void Panels::Private::renderSide()
{
    if (sideTab == "watchlist") renderWatchlist();
    else if (sideTab == "alerts") renderAlerts();
    else if (sideTab == "calendar") renderCalendar();
    else if (sideTab == "news") research::renderNews(sideBody, app);
}

void Panels::Private::renderBottom()
{
    if (collapsed) return;
    if (bottomTab == "tester") renderTester();
    else if (bottomTab == "screener") renderScreener();
    else if (bottomTab == "heatmap") renderHeatmap();
    else if (bottomTab == "seasonals") research::renderSeasonals(bottom, app);
    else if (bottomTab == "financials") research::renderFinancials(bottom, app);
    else if (bottomTab == "paper") renderPaper();
    else if (bottomTab == "editor") renderEditor();
    else if (bottomTab == "terminal" && terminal) terminal->focusCurrent();
    else if (bottomTab == "ptq") ptq::renderPtqTab(bottom, app);
}

// ================================================================ WATCHLIST

QStringList Panels::Private::wlSymbols() const
{
    return app->state.watchlists.value(app->state.activeWatchlist);
}

void Panels::Private::renderWatchlist()
{
    const QStringList names = app->state.watchlists.keys();
    auto *root = block();
    // .wl-head
    class Head : public QWidget
    {
    public:
        using QWidget::QWidget;

    protected:
        void paintEvent(QPaintEvent *) override
        {
            QPainter p(this);
            p.fillRect(QRect(0, height() - 1, width(), 1), theme::color("--border"));
        }
    };
    auto *head = new Head;
    auto *hl = new QHBoxLayout(head);
    hl->setContentsMargins(8, 8, 8, 9);
    hl->setSpacing(6);
    auto *sel = new kit::Select(kit::Select::In);
    for (const QString &n : names) sel->addOption(n, n);
    sel->setValue(app->state.activeWatchlist);
    sel->setMinimumWidth(0);
    sel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    auto *bNew = btn("+ List", "btn small");
    bNew->setToolTip("New list");
    auto *bDel = btn("Delete", "btn small");
    bDel->setToolTip("Delete this list");
    hl->addWidget(sel, 1);
    hl->addWidget(bNew);
    hl->addWidget(bDel);
    vbox(root)->addWidget(head);
    auto *addBox = block(nullptr, QMargins(8, 8, 8, 8));
    auto *add = new Input(Input::In);
    add->setPlaceholderText("Add symbol (Enter)");
    // Browse: the symbol search's market lists, to add symbols whose ticker you don't know (deepwater).
    auto *bBrowse = btn("Browse", "btn small");
    bBrowse->setToolTip("Find symbols to add by market, sector or region");
    auto *addRow = new QWidget;
    auto *al = new QHBoxLayout(addRow);
    al->setContentsMargins(0, 0, 0, 0);
    al->setSpacing(6);
    al->addWidget(add, 1);
    al->addWidget(bBrowse);
    vbox(addBox)->addWidget(addRow);
    vbox(root)->addWidget(addBox);
    auto *rows = new QWidget;
    auto *rl = new QVBoxLayout(rows);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(0);
    vbox(root)->addWidget(rows);
    vbox(root)->addStretch(1);
    wlRows = rows;
    sideBody->setContent(root);

    QObject::connect(sel, &kit::Select::changed, root, [this](const QString &v) {
        app->state.activeWatchlist = v;
        app->persist();
        renderWatchlist();
        refreshQuotes();
    });
    QObject::connect(bNew, &QPushButton::clicked, root, [this] {
        bool ok = false;
        const QString n = kit::prompt(app, "Name for the new list:", &ok);
        if (n.isEmpty() || app->state.watchlists.contains(n)) return;
        app->state.watchlists[n] = QStringList();
        app->state.activeWatchlist = n;
        app->persist();
        renderWatchlist();
    });
    QObject::connect(bDel, &QPushButton::clicked, root, [this, names] {
        if (names.size() < 2) return app->toast("Keep at least one list.");
        if (!kit::confirm(app, QString("Delete the list \"%1\"?").arg(app->state.activeWatchlist))) return;
        app->state.watchlists.remove(app->state.activeWatchlist);
        app->state.activeWatchlist = app->state.watchlists.keys().value(0);
        app->persist();
        renderWatchlist();
    });
    QPointer<Input> addP(add);
    QObject::connect(add, &QLineEdit::returnPressed, root, [this, addP] {
        if (!addP || addP->text().trimmed().isEmpty()) return;
        api::get(
            addP, "/api/resolve?symbol=" + QString::fromLatin1(QUrl::toPercentEncoding(addP->text().trimmed())),
            [this, addP](const QJsonValue &r) {
                const QString id = r.toObject().value("id").toString();
                QStringList list = wlSymbols();
                if (!list.contains(id)) list << id;
                app->state.watchlists[app->state.activeWatchlist] = list;
                if (addP) addP->clear();
                app->persist();
                drawRows();
                refreshQuotes();
            },
            [this](const QString &msg) { app->toast(msg, "err"); });
    });
    QObject::connect(bBrowse, &QPushButton::clicked, root, [this] {
        const QString name = app->state.activeWatchlist;
        // Rows of the search are symbols already ("US:AAPL"); what you type and Enter is resolved first.
        auto toggle = [this, name](const QString &id) {
            if (!app->state.watchlists.contains(name)) return;
            QStringList &list = app->state.watchlists[name];
            if (list.contains(id)) {
                list.removeAll(id);
                app->toast(QString("Removed %1 from %2").arg(theme::esc(theme::ticker(id)), theme::esc(name)));
            } else {
                list << id;
                app->toast(QString("Added %1 to %2").arg(theme::esc(theme::ticker(id)), theme::esc(name)));
            }
            app->persist();
            if (sideTab == "watchlist") {
                drawRows();
                refreshQuotes();
            }
        };
        app->openSymbolSearch(
            QString(),
            [this, toggle](const QString &sym) {
                if (sym.contains(':')) return toggle(sym);
                api::get(
                    q, "/api/resolve?symbol=" + QString::fromLatin1(QUrl::toPercentEncoding(sym)),
                    [toggle](const QJsonValue &r) { toggle(r.toObject().value("id").toString()); },
                    [this](const QString &msg) { app->toast(msg, "err"); });
            },
            QString("Add to %1").arg(name),
            [this, name](const QString &sym) { return app->state.watchlists.value(name).contains(sym); });
    });
    drawRows();
}

void Panels::Private::drawRows()
{
    QWidget *box = wlRows;
    if (!box) return;
    const QString cur = app->active ? app->active->cfg.symbol : QString();
    const QStringList syms = wlSymbols();
    auto shown = [&](const QString &s) {
        const QJsonObject qt = quotes.value(s);
        const double live = app->prices.contains(s) ? app->prices.value(s)
                            : qt.value("price").isDouble() ? qt.value("price").toDouble()
                                                           : NaN;
        const QJsonValue chg = qt.value("changePct");
        const double c = chg.isDouble() ? chg.toDouble() : NaN;
        return std::tuple(fmtPrice(live), fmtPct(c), chg.isDouble() && c >= 0);
    };
    // The same symbols in the same order: the rows take the new quotes in place.
    QLayout *l = box->layout();
    bool same = !syms.isEmpty() && l->count() == syms.size();
    for (int i = 0; same && i < syms.size(); ++i) {
        auto *row = dynamic_cast<WlRow *>(l->itemAt(i)->widget());
        same = row && row->symbol() == syms[i];
    }
    if (same) {
        for (int i = 0; i < syms.size(); ++i) {
            const auto [px, chg, up] = shown(syms[i]);
            static_cast<WlRow *>(l->itemAt(i)->widget())->set(syms[i] == cur, px, chg, up);
        }
        return;
    }
    clear(l);
    for (const QString &s : syms) {
        const auto [px, chgText, up] = shown(s);
        auto *row = new WlRow(s, s == cur, px, chgText, up);
        row->onClick = [this, s] { app->setSymbol(s); };
        row->onRemove = [this, s] {
            QStringList list = wlSymbols();
            list.removeAll(s);
            app->state.watchlists[app->state.activeWatchlist] = list;
            app->persist();
            QMetaObject::invokeMethod(q, [this] { drawRows(); }, Qt::QueuedConnection);
        };
        box->layout()->addWidget(row);
    }
    if (syms.isEmpty()) box->layout()->addWidget(padHint("Empty list. Add a symbol above."));
}

void Panels::Private::refreshQuotes()
{
    QStringList syms;
    for (const QString &s : wlSymbols() + paperSymbols())
        if (!syms.contains(s)) syms << s;
    if (syms.isEmpty()) return;
    api::quotes(q, syms, [this](const QJsonValue &v) {
        const QJsonObject o = v.toObject();
        for (auto it = o.begin(); it != o.end(); ++it) quotes.insert(it.key(), it->toObject());
        for (auto it = o.begin(); it != o.end(); ++it) {
            const double px = it->toObject().value("price").toDouble();
            if (js::truthy(px)) app->onPrice(it.key(), px);
        }
        if (sideTab == "watchlist") drawRows();
    }, [](const QString &) { /* offline */ });
}

// ================================================================ ALERTS

void Panels::Private::renderAlerts()
{
    auto *root = block();
    auto *top = rowFlex();
    top->layout()->setContentsMargins(12, 10, 12, 10);
    auto *bNew = btn("New alert", "btn primary");
    top->layout()->addWidget(bNew);
    Notifications *notes = Notifications::instance();
    if (notes->supported() && notes->permission() != "granted") {
        auto *perm = btn("Desktop notifications", "btn");
        top->layout()->addWidget(perm);
        QObject::connect(perm, &QPushButton::clicked, root, [this, notes] {
            // .then(() => renderAlerts()); a changed permission re-renders through permissionChanged.
            QPointer<Panels> guard(q);
            notes->requestPermission([this, guard, before = notes->permission()](const QString &p) {
                if (guard && p == before && sideTab == "alerts") renderAlerts();
            });
        });
    }
    vbox(root)->addWidget(top);
    auto *list = new QWidget;
    auto *ll = new QVBoxLayout(list);
    ll->setContentsMargins(0, 0, 0, 0);
    ll->setSpacing(0);
    vbox(root)->addWidget(list);
    vbox(root)->addWidget(padH4("Recent"));
    auto *log = new QWidget;
    auto *lgl = new QVBoxLayout(log);
    lgl->setContentsMargins(0, 0, 0, 0);
    lgl->setSpacing(0);
    vbox(root)->addWidget(log);
    vbox(root)->addStretch(1);
    sideBody->setContent(root);
    QObject::connect(bNew, &QPushButton::clicked, root, [this] { openAlertDialog({}); });

    // Promise.all of the two lists
    struct Pending { QJsonArray list, log; int left = 2; };
    auto pending = std::make_shared<Pending>();
    QPointer<QWidget> listP(list), logP(log);
    auto done = [this, pending, listP, logP] {
        if (--pending->left > 0) return;
        if (!listP || !logP) return;
        const QJsonArray alerts = pending->list;
        auto *col = cardColumn();
        col->layout()->setContentsMargins(8, 8, 8, 8);
        for (const auto &av : alerts) {
            const QJsonObject a = av.toObject();
            Box *c = card();
            auto *topRow = new QWidget;
            auto *tl = new QHBoxLayout(topRow);
            tl->setContentsMargins(0, 0, 0, 0);
            tl->setSpacing(6);
            auto *info = block();
            const QString title = !a.value("watchlist").toString().isEmpty()
                                      ? "Watchlist: " + esc(a.value("watchlist").toString())
                                      : esc(theme::ticker(a.value("symbol").toString()));
            vbox(info)->addWidget(text(QString("<b>%1</b> <span class=\"muted\">%2</span>")
                                           .arg(title, esc(a.value("tf").toString()))));
            vbox(info)->addWidget(text(esc(rules::describeAlert(a))));
            static const QHash<QString, QString> trig = {
                {"once", "only once"}, {"oncePerBar", "once per bar"}, {"oncePerBarClose", "once per bar close"}};
            const QString t = a.value("trigger").toString();
            const double fired = a.value("fired").toDouble();
            vbox(info)->addWidget(sub(QString("%1 · %2%3%4")
                                          .arg(a.value("active").toBool() ? "Active" : "Stopped",
                                               trig.value(t, t),
                                               !a.value("webhook").toString().isEmpty() ? " · webhook" : "",
                                               js::truthy(fired) ? QString(" · fired %1x").arg(js::numberToString(fired)) : "")));
            tl->addWidget(info, 1, Qt::AlignTop);
            auto *acts = new QWidget;
            auto *al = new QHBoxLayout(acts);
            al->setContentsMargins(0, 0, 0, 0);
            al->setSpacing(8);
            auto *bT = btn(a.value("active").toBool() ? "Stop" : "Restart", "btn small");
            auto *bD = btn("Delete", "btn small danger");
            al->addWidget(bT);
            al->addWidget(bD);
            tl->addWidget(acts, 0, Qt::AlignTop);
            c->layout()->addWidget(topRow);
            col->layout()->addWidget(c);
            const QString id = a.value("id").toString();
            const bool active = a.value("active").toBool();
            QObject::connect(bD, &QPushButton::clicked, q, [this, id] {
                api::del(q, "/api/alerts/" + id, [this](const QJsonValue &) { renderAlerts(); },
                         [this](const QString &) { renderAlerts(); });
            });
            QObject::connect(bT, &QPushButton::clicked, q, [this, id, active] {
                api::patch(q, "/api/alerts/" + id,
                           QJsonObject {{"active", !active}, {"lastBar", QJsonValue::Null}, {"lastBars", QJsonObject()}},
                           [this](const QJsonValue &) { renderAlerts(); }, [this](const QString &) { renderAlerts(); });
            });
        }
        if (alerts.isEmpty())
            listP->layout()->addWidget(padHint("No alerts yet. They run inside the app, so they keep firing with this "
                                               "tab closed as long as the app is running."));
        else
            listP->layout()->addWidget(col);
        auto *lcol = cardColumn();
        lcol->layout()->setContentsMargins(8, 8, 8, 8);
        int n = 0;
        for (const auto &ev : pending->log) {
            if (n++ >= 30) break;
            const QJsonObject e = ev.toObject();
            Box *c = card();
            c->layout()->addWidget(text(esc(e.value("text").toString())));
            const QJsonValue at = e.value("at");
            double ms = at.isDouble() ? at.toDouble() : QDateTime::fromString(at.toString(), Qt::ISODateWithMs).toMSecsSinceEpoch();
            c->layout()->addWidget(sub(localeString(ms)
                                       + (!e.value("webhook").toString().isEmpty() ? " · webhook " + esc(e.value("webhook").toString()) : "")));
            lcol->layout()->addWidget(c);
        }
        if (pending->log.isEmpty()) logP->layout()->addWidget(padHint("Nothing has fired yet."));
        else logP->layout()->addWidget(lcol);
    };
    api::get(list, "/api/alerts", [pending, done](const QJsonValue &v) { pending->list = v.toArray(); done(); },
             [done](const QString &) { done(); });
    api::get(list, "/api/alerts/log", [pending, done](const QJsonValue &v) { pending->log = v.toArray(); done(); },
             [done](const QString &) { done(); });
}

void Panels::Private::openAlertDialog(const QJsonObject &preset)
{
    ChartPane *p = app->active;
    const double price = p && !p->shown.isEmpty() ? p->shown.last().close : 0;
    QVector<rules::Rule> rs;
    if (preset.value("rules").isArray()) {
        for (const auto &r : preset.value("rules").toArray()) rs << rules::Rule::fromJson(r.toObject());
    } else if (preset.value("rule").isObject()) {
        rs << rules::Rule::fromJson(preset.value("rule").toObject());
    } else {
        rules::Rule r;
        r.left.kind = "price";
        r.left.field = "close";
        r.op = "crossesAbove";
        r.right.kind = "value";
        r.right.v = QString::number(price, 'g', 6).toDouble();
        rs << r;
    }
    auto *body = block();
    // form-grid: Symbol, Check, Timeframe
    auto grid = [] {
        auto *g = new QWidget;
        auto *gl = new QGridLayout(g);
        gl->setContentsMargins(0, 0, 0, 0);
        gl->setHorizontalSpacing(10);
        gl->setVerticalSpacing(10);
        gl->setColumnStretch(0, 1);
        gl->setColumnStretch(1, 1);
        return g;
    };
    auto *g1 = grid();
    auto *gl1 = static_cast<QGridLayout *>(g1->layout());
    auto *aSym = new Input(Input::Field);
    aSym->setText(!preset.value("symbol").toString().isEmpty() ? preset.value("symbol").toString()
                  : p                                          ? p->cfg.symbol
                                                               : QString());
    auto *aScope = new kit::Select(kit::Select::Field);
    aScope->addOption("", "Just this symbol");
    for (const QString &n : app->state.watchlists.keys()) aScope->addOption(n, "Every symbol on " + n);
    auto *aTf = new Input(Input::Field);
    aTf->setText(p ? p->cfg.tf : QString("1h"));
    aScope->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    gl1->addWidget(field("Symbol", aSym), 0, 0);
    gl1->addWidget(field("Check", aScope), 0, 1);
    gl1->addWidget(field("Timeframe", aTf), 1, 0);
    vbox(body)->addWidget(g1);
    add(body, h4("Conditions"), 10, 4);
    auto *rulesBox = new rules::RulesBox;
    for (const auto &r : rs) rulesBox->addRule(r);
    add(body, rulesBox, 0, 10);
    auto *row1 = rowFlex();
    auto *aAdd = btn("+ Condition", "btn small");
    auto *aMatch = new kit::Select(kit::Select::Field);
    aMatch->addOption("all", "all are true");
    aMatch->addOption("any", "any is true");
    row1->layout()->addWidget(aAdd);
    row1->layout()->addWidget(fieldRow("Fire when", aMatch));
    row1->layout()->addWidget(hint("Up to 5."));
    vbox(body)->addWidget(row1);
    auto *g2 = grid();
    auto *gl2 = static_cast<QGridLayout *>(g2->layout());
    auto *aTrig = new kit::Select(kit::Select::Field);
    aTrig->addOption("once", "Only once");
    aTrig->addOption("oncePerBar", "Once per bar");
    aTrig->addOption("oncePerBarClose", "Once per bar close");
    aTrig->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    auto *aMsg = new Input(Input::Field);
    aMsg->setPlaceholderText("BTC broke out");
    gl2->addWidget(field("Trigger", aTrig), 0, 0);
    gl2->addWidget(field("Message (optional)", aMsg), 0, 1);
    vbox(body)->addWidget(g2);
    add(body, h4("Also send to (optional)"), 10, 4);
    auto *row2 = rowFlex();
    auto *aHook = new Input(Input::In);
    aHook->setPlaceholderText("Discord, Slack or Telegram webhook URL");
    aHook->setProperty("flexGrow", 1);
    aHook->setMinimumWidth(0);
    auto *aTest = btn("Test", "btn");
    row2->layout()->addWidget(aHook);
    row2->layout()->addWidget(aTest);
    vbox(body)->addWidget(row2);
    add(body, hint("Telegram: https://api.telegram.org/bot&lt;token&gt;/sendMessage?chat_id=&lt;your id&gt;. Alerts are "
                   "checked every 15 seconds while the app runs."),
        12, 12);

    auto *foot = new QWidget;
    auto *fl = new QHBoxLayout(foot);
    fl->setContentsMargins(0, 0, 0, 0);
    fl->addStretch(1);
    auto *aOk = btn("Create alert", "btn primary");
    fl->addWidget(aOk);

    app->modal("Create alert", body, foot, true);

    QPointer<rules::RulesBox> rb(rulesBox);
    QObject::connect(aAdd, &QPushButton::clicked, body, [this, rb, price] {
        if (!rb) return;
        if (rb->count() >= 5) return app->toast("An alert can have up to 5 conditions.");
        rules::Rule r;
        r.left.kind = "price";
        r.left.field = "close";
        r.op = ">";
        r.right.kind = "value";
        r.right.v = QString::number(price, 'g', 6).toDouble();
        rb->addRule(r);
    });
    QPointer<Input> hookP(aHook);
    QObject::connect(aTest, &QPushButton::clicked, body, [this, hookP] {
        if (!hookP) return;
        api::post(q, "/api/webhook-test", QJsonObject {{"url", hookP->text().trimmed()}},
                  [this](const QJsonValue &v) {
                      const QJsonObject r = v.toObject();
                      const bool ok = r.value("ok").toBool();
                      app->toast(ok ? QString("Test message sent.")
                                    : QString("Webhook replied HTTP %1").arg(js::numberToString(r.value("status").toDouble())),
                                 ok ? QString() : QString("err"));
                  },
                  [this](const QString &m) { app->toast(m, "err"); });
    });
    QObject::connect(aOk, &QPushButton::clicked, body, [=, this] {
        if (!rb) return;
        const QVector<rules::Rule> read = rb->read();
        if (read.isEmpty()) return app->toast("Add a condition.", "err");
        const rules::Compiled c = rules::compileRules({{"rules", read}});
        const QJsonArray cr = c.group("rules");
        const QJsonObject r0 = cr.at(0).toObject();
        QJsonObject body {
            {"symbol", aSym->text().trimmed()},
            {"tf", aTf->text().trimmed()},
            {"left", r0.value("left")},
            {"op", r0.value("op")},
            {"right", r0.value("right")},
            {"conditions", cr},
            {"match", aMatch->value()},
            {"indicators", c.indicators},
            {"trigger", aTrig->value()},
            {"message", aMsg->text().trimmed()},
            {"webhook", aHook->text().trimmed()},
        };
        const QString wl = aScope->value();
        if (!wl.isEmpty()) body.insert("watchlist", wl);
        api::post(q, "/api/alerts", body,
                  [this](const QJsonValue &) {
                      app->closeModal();
                      app->toast("Alert created.");
                      Notifications *notes = Notifications::instance();
                      if (notes->supported() && notes->permission() == "default") notes->requestPermission();
                      showSide("alerts");
                  },
                  [this](const QString &m) { app->toast(m, "err"); });
    });
}

// ================================================================ CALENDAR

void Panels::Private::renderCalendar()
{
    auto *root = block();
    auto *top = rowFlex();
    top->layout()->setContentsMargins(12, 10, 12, 10);
    auto *calV = new kit::Select(kit::Select::In);
    calV->addOption("economic", "Economic");
    calV->addOption("earnings", "Earnings");
    calV->addOption("dividends", "Dividends");
    calV->setValue(calView);
    top->layout()->addWidget(calV);
    top->layout()->addWidget(spacer());
    if (calView == "economic") {
        auto *calF = new kit::Select(kit::Select::In);
        calF->addOption("all", "All");
        calF->addOption("Medium", "Medium + high");
        calF->addOption("High", "High impact");
        calF->setValue(calFilter);
        top->layout()->addWidget(calF);
        QObject::connect(calF, &kit::Select::changed, root, [this](const QString &v) {
            calFilter = v;
            renderCalendar();
        });
    } else {
        auto *cb = new QCheckBox("My watchlists");
        cb->setChecked(calMine);
        cb->setStyleSheet(QString("QCheckBox { color: %1; font-size: 12px; }").arg(theme::css("--muted")));
        top->layout()->addWidget(cb);
        QObject::connect(cb, &QCheckBox::toggled, root, [this](bool on) {
            calMine = on;
            renderCalendar();
        });
    }
    vbox(root)->addWidget(top);
    auto *list = new QWidget;
    auto *ll = new QVBoxLayout(list);
    ll->setContentsMargins(0, 0, 0, 0);
    ll->setSpacing(0);
    auto *loading = padHint(QString::fromUtf8("Loading…"));
    ll->addWidget(loading);
    vbox(root)->addWidget(list);
    vbox(root)->addStretch(1);
    sideBody->setContent(root);
    QObject::connect(calV, &kit::Select::changed, root, [this](const QString &v) {
        calView = v;
        renderCalendar();
    });

    QSet<QString> mine;
    for (const auto &wl : app->state.watchlists.items)
        for (const QString &s : wl.second) mine.insert(s);
    QPointer<QWidget> listP(list);
    auto fail = [listP](const QString &m) {
        if (!listP) return;
        clear(listP->layout());
        listP->layout()->addWidget(padHint(esc("Calendar unavailable: " + m)));
    };
    // cards follow each other with 8px margins; a day heading has margin 0 and padding-top 10
    auto addCard = [](QVBoxLayout *l, QWidget *c, bool afterHead) {
        if (afterHead || l->count() == 0) l->addSpacing(8);
        else l->addSpacing(8);
        auto *wrap = new QWidget;
        auto *wl = new QHBoxLayout(wrap);
        wl->setContentsMargins(8, 0, 8, 0);
        wl->addWidget(c);
        l->addWidget(wrap);
    };
    if (calView == "economic") {
        api::get(list, "/api/calendar", [this, listP, addCard](const QJsonValue &v) {
            if (!listP) return;
            auto *l = static_cast<QVBoxLayout *>(listP->layout());
            clear(l);
            QString day;
            int n = 0;
            bool lastWasCard = false;
            for (const auto &rv : v.toArray()) {
                const QJsonObject r = rv.toObject();
                const QString imp = r.value("impact").toString();
                const bool keep = calFilter == "all" || imp == "High" || (calFilter == "Medium" && imp == "Medium");
                if (!keep) continue;
                const double ms = QDateTime::fromString(r.value("date").toString(), Qt::ISODateWithMs).toMSecsSinceEpoch();
                const QString label = weekdayDayMonth(ms);
                bool head = false;
                if (label != day) {
                    day = label;
                    if (lastWasCard) l->addSpacing(8);
                    l->addWidget(padH4(label));
                    head = true;
                }
                Box *c = card();
                auto *top = new QWidget;
                auto *tl = new QHBoxLayout(top);
                tl->setContentsMargins(0, 0, 0, 0);
                tl->setSpacing(6);
                tl->addWidget(text(QString("<b>%1</b> %2").arg(esc(r.value("country").toString()),
                                                               esc(r.value("title").toString()))),
                              1, Qt::AlignTop);
                Text *impT = text(QString("<span class=\"impact-%1\">%2</span>").arg(esc(imp), esc(imp)));
                impT->setStyle([&] { TextStyle s = impT->textStyle(); s.wrap = false; return s; }());
                tl->addWidget(impT, 0, Qt::AlignTop);
                c->layout()->addWidget(top);
                QString subText = localeTimeHM(ms);
                if (!r.value("forecast").toString().isEmpty()) subText += " · forecast " + esc(r.value("forecast").toString());
                if (!r.value("previous").toString().isEmpty()) subText += " · previous " + esc(r.value("previous").toString());
                c->layout()->addWidget(sub(subText));
                addCard(l, c, head);
                lastWasCard = true;
                ++n;
            }
            if (lastWasCard) l->addSpacing(8);
            if (!n) l->addWidget(padHint("Nothing at this impact level this week."));
        }, fail);
        return;
    }
    const QString view = calView;
    api::get(list, QString("/api/%1?days=5").arg(view), [this, listP, mine, view, addCard](const QJsonValue &v) {
        if (!listP) return;
        QVector<QJsonObject> rows;
        for (const auto &rv : v.toArray()) {
            const QJsonObject r = rv.toObject();
            if (!calMine || mine.contains(r.value("symbol").toString())) rows << r;
        }
        std::stable_sort(rows.begin(), rows.end(), [&](const QJsonObject &x, const QJsonObject &y) {
            const int dc = QString::localeAwareCompare(x.value("date").toString(), y.value("date").toString());
            if (dc) return dc < 0;
            const int mx = mine.contains(x.value("symbol").toString()), my = mine.contains(y.value("symbol").toString());
            if (my - mx) return my - mx < 0;
            return (y.value("marketCap").toDouble() - x.value("marketCap").toDouble()) < 0;
        });
        auto *l = static_cast<QVBoxLayout *>(listP->layout());
        clear(l);
        QString day;
        bool lastWasCard = false;
        int n = 0;
        for (const QJsonObject &r : rows) {
            if (n >= 250) break;
            const QString date = r.value("date").toString();
            bool head = false;
            if (date != day) {
                day = date;
                const double ms = QDateTime::fromString(date + "T12:00:00Z", Qt::ISODate).toMSecsSinceEpoch();
                if (lastWasCard) l->addSpacing(8);
                l->addWidget(padH4(weekdayDayMonth(ms)));
                head = true;
            }
            const bool star = mine.contains(r.value("symbol").toString());
            QString subText;
            if (view == "earnings") {
                subText = esc(r.value("when").toString());
                if (!r.value("epsForecast").toString().isEmpty()) subText += " · EPS forecast " + esc(r.value("epsForecast").toString());
                if (!r.value("lastYearEps").toString().isEmpty()) subText += " · last year " + esc(r.value("lastYearEps").toString());
                if (js::truthy(r.value("marketCap").toDouble())) subText += " · cap " + fmtNum(r.value("marketCap").toDouble());
            } else {
                QStringList parts;
                QString s;
                if (r.value("amount").isDouble()) s = fmtPrice(r.value("amount").toDouble()) + " a share";
                if (!r.value("payDate").toString().isEmpty()) s += " · paid " + esc(r.value("payDate").toString());
                if (js::truthy(r.value("annual").toDouble())) s += " · " + fmtPrice(r.value("annual").toDouble()) + " a year";
                subText = s;
            }
            Box *c = card();
            c->setCursor(Qt::PointingHandCursor);
            const QString tick = r.value("ticker").toString();
            Text *title = text(QString("<b>%1</b>%2 <span class=\"muted\">%3</span>")
                                   .arg(esc(tick),
                                        star ? QString(" <span class=\"up\">%1</span>").arg(QChar(0x25cf)) : QString(),
                                        esc(r.value("name").toString().left(34))));
            c->layout()->addWidget(title);
            c->layout()->addWidget(sub(subText));
            const QString sym = r.value("symbol").toString();
            // The row opens its chart; the watchlist dot has title="On your watchlist".
            class Click : public QObject
            {
            public:
                Click(QWidget *card, Text *title, const QString &tick, bool star, std::function<void()> fn)
                    : QObject(card), m_card(card), m_title(title), m_tick(tick), m_star(star), m_fn(std::move(fn)) {}

            protected:
                bool eventFilter(QObject *, QEvent *e) override
                {
                    if (e->type() == QEvent::MouseButtonRelease) m_fn();
                    if (e->type() == QEvent::ToolTip) {
                        auto *he = static_cast<QHelpEvent *>(e);
                        if (m_star && m_title) {
                            // the dot follows "<b>TICKER</b> " on the first line
                            const QFont base = m_title->textStyle().font;
                            QFont bold = base;
                            bold.setWeight(QFont::Bold);
                            const double x0 = QFontMetricsF(bold).horizontalAdvance(m_tick)
                                              + QFontMetricsF(base).horizontalAdvance(" ");
                            const double w = QFontMetricsF(base).horizontalAdvance(QChar(0x25cf));
                            const QPointF p = m_title->mapFrom(m_card, QPointF(he->pos()));
                            const double lh = base.pixelSize() * 1.4;
                            if (p.x() >= x0 && p.x() <= x0 + w && p.y() >= 0 && p.y() <= lh) {
                                QToolTip::showText(he->globalPos(), "On your watchlist", m_card);
                                return true;
                            }
                        }
                        QToolTip::hideText();
                        e->ignore();
                        return true;
                    }
                    return false;
                }

            private:
                QWidget *m_card;
                QPointer<Text> m_title;
                QString m_tick;
                bool m_star;
                std::function<void()> m_fn;
            };
            c->installEventFilter(new Click(c, title, tick, star, [this, sym] { app->setSymbol(sym); }));
            for (QWidget *child : c->findChildren<QWidget *>()) child->setAttribute(Qt::WA_TransparentForMouseEvents);
            addCard(l, c, head);
            lastWasCard = true;
            ++n;
        }
        if (lastWasCard) l->addSpacing(8);
        if (!n)
            l->addWidget(padHint(calMine ? "None of your watchlist symbols in the next five trading days."
                                         : "Nothing listed for the next five trading days."));
    }, fail);
}

} // namespace qtm
