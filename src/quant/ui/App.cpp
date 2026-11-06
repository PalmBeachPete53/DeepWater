// Port of quant-terminal/public/js/app.js + index.html: the whole terminal as one widget.
#include "PanelsKit.h"
#include "App.h"

#include "Api.h"
#include "Notifications.h"
#include "ChartPane.h"
#include "Drawings.h"
#include "FootprintLayer.h"
#include "Panels.h"
#include "Theme.h"
#include "Widgets.h"

#include "../chart/Chart.h"
#include "../core/CustomIndicators.h"
#include "../core/Error.h"
#include "../core/JsCompat.h"
#include "../core/Series.h"
#include "../server/Server.h"

#include <utility>
#include <QApplication>
#include <QAudioFormat>
#include <QAudioSink>
#include <QBuffer>
#include <QCheckBox>
#include <QConicalGradient>
#include <QDateTime>
#include <QDateTimeEdit>
#include <QDesktopServices>
#include <QFileInfo>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMediaDevices>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QStyle>
#include <QSettings>
#include <QStandardItemModel>
#include <QTextDocument>
#include <QTextEdit>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <cmath>

namespace qtm {

namespace {

const QStringList TF_QUICK = {"1m", "5m", "15m", "1h", "4h", "1D", "1W"};
const QHash<QString, int> LAYOUTS = {{"1", 1}, {"2v", 2}, {"2h", 2}, {"3", 3}, {"4", 4}, {"6", 6}, {"8", 8}, {"16", 16}};

const char *const IconIndicators = R"(<path d="M3 17l5-6 4 3 6-8 3 3" />)";
const char *const IconCompare = R"(<circle cx="12" cy="12" r="8" /><path d="M12 6v12M6 12h12" />)";
const char *const IconReplay = R"(<path d="M4 12a8 8 0 1 0 3-6.2M4 4v4h4" />)";
const char *const IconAlert = R"(<path d="M6 16V11a6 6 0 1 1 12 0v5l2 2H4zM10 20h4" />)";
const char *const IconSnap = R"(<rect x="3" y="6" width="18" height="14" rx="2" /><circle cx="12" cy="13" r="3.5" /><path d="M8 6l2-3h4l2 3" />)";
const char *const IconCsv = R"(<path d="M12 4v11M7 10l5 5 5-5M5 20h14" />)";
const char *const IconSettings = R"(<circle cx="12" cy="12" r="3"/><path d="M19.4 15a1.7 1.7 0 0 0 .3 1.8l.1.1a2 2 0 1 1-2.8 2.8l-.1-.1a1.7 1.7 0 0 0-1.8-.3 1.7 1.7 0 0 0-1 1.5V21a2 2 0 1 1-4 0v-.1a1.7 1.7 0 0 0-1.1-1.5 1.7 1.7 0 0 0-1.8.3l-.1.1a2 2 0 1 1-2.8-2.8l.1-.1a1.7 1.7 0 0 0 .3-1.8 1.7 1.7 0 0 0-1.5-1H3a2 2 0 1 1 0-4h.1a1.7 1.7 0 0 0 1.5-1.1 1.7 1.7 0 0 0-.3-1.8l-.1-.1a2 2 0 1 1 2.8-2.8l.1.1a1.7 1.7 0 0 0 1.8.3H9a1.7 1.7 0 0 0 1-1.5V3a2 2 0 1 1 4 0v.1a1.7 1.7 0 0 0 1 1.5 1.7 1.7 0 0 0 1.8-.3l.1-.1a2 2 0 1 1 2.8 2.8l-.1.1a1.7 1.7 0 0 0-.3 1.8V9a1.7 1.7 0 0 0 1.5 1H21a2 2 0 1 1 0 4h-.1a1.7 1.7 0 0 0-1.5 1z"/>)";
const char *const IconTheme = R"(<path d="M20 14.5A8 8 0 1 1 9.5 4a6.5 6.5 0 0 0 10.5 10.5z" />)";
const char *const IconClose = R"(<path d="M6 6l12 12M18 6L6 18"/>)";
const char *const IconDrawAlert = R"(<path d="M6 16V11a6 6 0 1 1 12 0v5l2 2H4zM10 20h4"/>)";
const char *const IconDelete = R"(<path d="M4 7h16M10 11v6M14 11v6M6 7l1 13h10l1-13M9 7V4h6v3"/>)";
const char *const IconClear = R"(<path d="M5 5l14 14M19 5L5 19"/>)";

const char *const HelpUrl = "https://part-timequant.com/tradingview/help";

QJsonObject defaultWatchlists()
{
    return QJsonObject{{"Main", QJsonArray{"BINANCE:BTCUSDT", "BINANCE:ETHUSDT", "BINANCE:SOLUSDT", "US:AAPL", "US:NVDA",
                                           "US:SPY", "US:QQQ", "FX:EURUSD"}}};
}

BtnStyle tbStyle()
{
    BtnStyle s;
    s.height = 32;
    s.padL = s.padR = 9;
    s.gap = 6;
    return s;
}

BtnStyle tbIconStyle()
{
    BtnStyle s = tbStyle();
    s.padL = s.padR = 7;
    return s;
}

BtnStyle tfStyle()
{
    BtnStyle s;
    s.height = 32;
    s.padL = s.padR = 6;
    s.minWidth = 34;
    s.activeBg = "--accent-soft";
    s.activeColor = "--accent";
    s.centered = true;
    return s;
}

BtnStyle drawStyle()
{
    BtnStyle s;
    s.height = 34;
    s.fixedWidth = 34;
    s.padL = s.padR = 0;
    s.activeBg = "--accent-soft";
    s.activeColor = "--accent";
    s.centered = true;
    return s;
}

// JS Number(el.value) for an <input type=number>: text the input can't parse leaves its value
// empty, which is 0.
double jsNumber(const QString &text)
{
    const QString t = text.trimmed();
    if (t.isEmpty()) return 0;
    bool ok = false;
    const double v = t.toDouble(&ok);
    return ok && std::isfinite(v) ? v : 0;
}

QString jsText(const QJsonValue &v)
{
    if (v.isDouble()) return js::numberToString(v.toDouble());
    if (v.isString()) return v.toString();
    if (v.isBool()) return v.toBool() ? "true" : "false";
    return QString();
}

// The brand mark: <rect rx=7 fill=accent/> with the white zigzag (--on-accent).
class Brand : public QWidget
{
public:
    // The svg has width/height 22, but the CSS `svg { width: 18px; height: 18px }` wins.
    explicit Brand(QWidget *parent = nullptr) : QWidget(parent)
    {
        setFixedSize(28, 18);
        setToolTip("Quant Terminal");
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.translate(2, 0);
        p.scale(18.0 / 32, 18.0 / 32);
        p.setPen(Qt::NoPen);
        p.setBrush(theme::color("--accent"));
        p.drawRoundedRect(QRectF(0, 0, 32, 32), 7, 7);
        QPen pen(theme::color("--on-accent"), 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        QPainterPath path;
        path.moveTo(7, 21);
        path.lineTo(13, 14);
        path.lineTo(18, 18);
        path.lineTo(25, 9);
        p.drawPath(path);
    }
};

// <div class="sep">: 1x22 line with 6px margins.
class Sep : public QWidget
{
public:
    explicit Sep(QWidget *parent = nullptr) : QWidget(parent) { setFixedSize(13, 22); }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.fillRect(QRect(6, 0, 1, 22), theme::color("--border"));
    }
};

// <hr> in the draw bar: 24px wide top border, 4px margins.
class Hr : public QWidget
{
public:
    explicit Hr(QWidget *parent = nullptr) : QWidget(parent) { setFixedSize(24, 9); }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.fillRect(QRect(0, 4, 24, 1), theme::color("--border"));
    }
};

// <span id="status" class="status">.
class StatusDot : public QWidget
{
public:
    explicit StatusDot(QWidget *parent = nullptr) : QWidget(parent)
    {
        setFixedSize(23, 9);
        setToolTip("Data status");
    }
    QString level;

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        QColor c = theme::color("--muted");
        if (level == "ok") c = theme::color("--up");
        else if (level == "warn") c = QColor("#f7a600");
        else if (level == "bad") c = theme::color("--down");
        p.setBrush(c);
        p.drawEllipse(QRectF(8, 0, 9, 9));
    }
};

// .list .item: a row that highlights on hover and when selected.
class ListItem : public QFrame
{
public:
    ListItem(const QString &leftHtml, const QString &meta, std::function<void()> onClick, QWidget *parent = nullptr)
        : QFrame(parent), m_onClick(std::move(onClick))
    {
        setAttribute(Qt::WA_Hover);
        setAttribute(Qt::WA_StyledBackground);
        theme::setCls(this, "item");
        setCursor(Qt::PointingHandCursor);
        auto *h = new QHBoxLayout(this);
        h->setContentsMargins(10, 8, 10, 8);
        h->setSpacing(10);
        auto *l = label(leftHtml, QString(), true);
        l->setTextInteractionFlags(Qt::NoTextInteraction);
        l->setAttribute(Qt::WA_TransparentForMouseEvents);
        l->setMinimumHeight(18);
        l->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);   // a long name is cut, it doesn't widen the list
        h->addWidget(l, 1);
        if (!meta.isNull()) {
            auto *m = label(meta, "meta");
            m->setAttribute(Qt::WA_TransparentForMouseEvents);
            h->addWidget(m, 0);
        }
    }
    void setSel(bool on) { theme::setCls(this, on ? "item sel" : "item"); }

protected:
    void mouseReleaseEvent(QMouseEvent *e) override
    {
        if (e->button() == Qt::LeftButton && rect().contains(e->position().toPoint()) && m_onClick) {
            auto fn = m_onClick;   // the click may delete this item
            fn();
        }
    }

private:
    std::function<void()> m_onClick;
};

// A short sine beep (WebAudio oscillator at 880 Hz, gain 0.08, 0.25 s).
void playBeep(QObject *parent)
{
    QAudioFormat fmt;
    fmt.setSampleRate(44100);
    fmt.setChannelCount(1);
    fmt.setSampleFormat(QAudioFormat::Int16);
    const QAudioDevice dev = QMediaDevices::defaultAudioOutput();
    if (dev.isNull()) return;
    auto *sink = new QAudioSink(dev, fmt, parent);
    const int n = int(44100 * 0.25);
    QByteArray pcm(n * 2, 0);
    auto *s = reinterpret_cast<qint16 *>(pcm.data());
    for (int i = 0; i < n; ++i)
        s[i] = qint16(std::sin(2 * M_PI * 880 * i / 44100.0) * 0.08 * 32767);
    auto *buf = new QBuffer(sink);
    buf->setData(pcm);
    buf->open(QIODevice::ReadOnly);
    QObject::connect(sink, &QAudioSink::stateChanged, sink, [sink](QAudio::State st) {
        if (st == QAudio::IdleState || st == QAudio::StoppedState) {
            sink->stop();
            sink->deleteLater();
        }
    });
    sink->start(buf);
}

QJsonObject paneToJson(const PaneConfig &c)
{
    QJsonArray inds;
    for (const auto &i : c.indicators) {
        QJsonObject o{{"uid", i.uid}, {"id", i.id}, {"params", i.params}};
        if (!i.colors.isEmpty()) {
            QJsonObject cs;
            for (auto it = i.colors.begin(); it != i.colors.end(); ++it) cs[it.key()] = it.value();
            o["colors"] = cs;
        }
        if (!i.tf.isEmpty()) o["tf"] = i.tf;
        if (i.hidden) o["hidden"] = true;
        inds.append(o);
    }
    QJsonObject o{{"symbol", c.symbol}, {"tf", c.tf}, {"type", c.type}, {"indicators", inds},
                  {"compare", QJsonArray::fromStringList(c.compare)}, {"log", c.log}};
    if (c.volumeHidden) o["volumeHidden"] = true;
    if (c.volumeRemoved) o["volumeRemoved"] = true;
    if (!c.footprint.isEmpty()) o["footprint"] = c.footprint;   // not in the JS: Deepwater's footprint chart
    return o;
}

PaneConfig paneFromJson(const QJsonObject &o)
{
    PaneConfig c;
    if (o.contains("symbol")) c.symbol = o.value("symbol").toString();
    if (o.contains("tf")) c.tf = o.value("tf").toString();
    if (o.contains("type")) c.type = o.value("type").toString();
    for (const auto &v : o.value("indicators").toArray()) {
        const QJsonObject x = v.toObject();
        IndicatorInstance i;
        i.uid = x.value("uid").toString();
        i.id = x.value("id").toString();
        i.params = x.value("params").toObject();
        const QJsonObject cs = x.value("colors").toObject();
        for (auto it = cs.begin(); it != cs.end(); ++it) i.colors.insert(it.key(), it.value().toString());
        i.tf = x.value("tf").toString();
        i.hidden = x.value("hidden").toBool();
        c.indicators.push_back(i);
    }
    for (const auto &v : o.value("compare").toArray()) c.compare << v.toString();
    c.log = o.value("log").toBool();
    c.volumeHidden = o.value("volumeHidden").toBool();
    c.volumeRemoved = o.value("volumeRemoved").toBool();
    c.footprint = o.value("footprint").toObject();
    return c;
}

} // namespace

// ---------------------------------------------------------------- AppState

namespace {

// The keys of `obj` in the saved order first, then any others (sorted).
QStringList orderedKeys(const QJsonObject &obj, const QJsonValue &order)
{
    QStringList keys;
    for (const auto &v : order.toArray())
        if (obj.contains(v.toString()) && !keys.contains(v.toString())) keys << v.toString();
    for (auto it = obj.begin(); it != obj.end(); ++it)
        if (!keys.contains(it.key())) keys << it.key();
    return keys;
}

} // namespace

QJsonObject AppState::toJson() const
{
    QJsonObject o = extra;
    o["layout"] = layout;
    o["active"] = active;
    QJsonArray ps;
    for (const auto &p : panes) ps.append(paneToJson(p));
    o["panes"] = ps;
    QJsonObject wl;
    for (const auto &p : watchlists.items) wl[p.first] = QJsonArray::fromStringList(p.second);
    o["watchlists"] = wl;
    // QJsonObject sorts keys; JS keeps insertion order, which the watchlist select shows.
    o["watchlistOrder"] = QJsonArray::fromStringList(watchlists.keys());
    o["activeWatchlist"] = activeWatchlist;
    o["drawings"] = drawings;
    o["settings"] = QJsonObject{{"theme", settings.theme},
                                {"volume", settings.volume},
                                {"countdown", settings.countdown},
                                {"bars", settings.bars},
                                {"utc", settings.utc},
                                {"stayInDrawMode", settings.stayInDrawMode},
                                {"syncCrosshair", settings.syncCrosshair},
                                {"syncSymbol", settings.syncSymbol},
                                {"syncReplay", settings.syncReplay}};
    o["paper"] = paper;
    if (!replayPaper.isNull() && !replayPaper.isUndefined()) o["replayPaper"] = replayPaper;
    QJsonObject st;
    for (const auto &p : strategies.items) st[p.first] = p.second;
    o["strategies"] = st;
    o["strategyOrder"] = QJsonArray::fromStringList(strategies.keys());
    return o;
}

AppState AppState::fromJson(const QJsonObject &o)
{
    AppState s;
    static const QStringList known = {"layout", "active", "panes", "watchlists", "activeWatchlist", "drawings",
                                      "settings", "paper", "replayPaper", "strategies",
                                      "watchlistOrder", "strategyOrder"};
    for (auto it = o.begin(); it != o.end(); ++it)
        if (!known.contains(it.key())) s.extra.insert(it.key(), it.value());
    if (o.contains("layout")) s.layout = jsText(o.value("layout"));
    if (o.value("active").isDouble()) s.active = o.value("active").toInt();
    if (o.value("panes").isArray()) {
        for (const auto &v : o.value("panes").toArray()) s.panes.push_back(paneFromJson(v.toObject()));
    } else {
        PaneConfig p;
        IndicatorInstance ema;
        ema.uid = newUid();
        ema.id = "ema";
        ema.params = QJsonObject{{"length", 21}};
        IndicatorInstance rsi;
        rsi.uid = newUid();
        rsi.id = "rsi";
        p.indicators = {ema, rsi};
        s.panes = {p};
    }
    const QJsonObject wl = o.value("watchlists").isObject() ? o.value("watchlists").toObject() : defaultWatchlists();
    for (const QString &name : orderedKeys(wl, o.value("watchlistOrder"))) {
        QStringList syms;
        for (const auto &v : wl.value(name).toArray()) syms << v.toString();
        s.watchlists[name] = syms;
    }
    if (o.value("activeWatchlist").isString()) s.activeWatchlist = o.value("activeWatchlist").toString();
    if (o.value("drawings").isObject()) s.drawings = o.value("drawings").toObject();
    const QJsonObject st = o.value("settings").toObject();
    if (st.contains("theme")) s.settings.theme = st.value("theme").toString();
    if (st.contains("volume")) s.settings.volume = st.value("volume").toBool();
    if (st.contains("countdown")) s.settings.countdown = st.value("countdown").toBool();
    if (st.contains("bars")) s.settings.bars = st.value("bars").toInt();
    if (st.contains("utc")) s.settings.utc = st.value("utc").toBool();
    if (st.contains("stayInDrawMode")) s.settings.stayInDrawMode = st.value("stayInDrawMode").toBool();
    if (st.contains("syncCrosshair")) s.settings.syncCrosshair = st.value("syncCrosshair").toBool();
    if (st.contains("syncSymbol")) s.settings.syncSymbol = st.value("syncSymbol").toBool();
    if (st.contains("syncReplay")) s.settings.syncReplay = st.value("syncReplay").toBool();
    if (o.contains("paper")) s.paper = o.value("paper");
    if (o.contains("replayPaper")) s.replayPaper = o.value("replayPaper");
    const QJsonObject strat = o.value("strategies").toObject();
    for (const QString &name : orderedKeys(strat, o.value("strategyOrder")))
        s.strategies[name] = strat.value(name).toObject();
    return s;
}

// ---------------------------------------------------------------- the widgets

struct App::Ui {
    QVBoxLayout *root = nullptr;
    QWidget *topbar = nullptr;
    Btn *symbolBtn = nullptr;
    QWidget *tfBox = nullptr;
    QHBoxLayout *tfLayout = nullptr;
    QVector<Btn *> tfBtns;
    QLineEdit *tfCustom = nullptr;
    Select *chartType = nullptr;
    Btn *footprintBtn = nullptr;   // the footprint chart's settings, next to the type when it is the type
    Btn *indicatorsBtn = nullptr, *compareBtn = nullptr, *replayBtn = nullptr, *alertBtn = nullptr;
    Select *layoutSel = nullptr;
    QCheckBox *syncSymbol = nullptr;
    Btn *helpBtn = nullptr, *logBtn = nullptr, *snapBtn = nullptr, *csvBtn = nullptr, *settingsBtn = nullptr,
        *themeBtn = nullptr;
    StatusDot *status = nullptr;
    QWidget *mainArea = nullptr;
    QHBoxLayout *mainLayout = nullptr;
    QWidget *drawbar = nullptr;
    QVBoxLayout *drawLayout = nullptr;
    QHash<QString, Btn *> toolBtns;
    QWidget *center = nullptr;
    QVBoxLayout *centerLayout = nullptr;
    QWidget *grid = nullptr;
    QWidget *bottomPlaceholder = nullptr;
    QWidget *sidePlaceholder = nullptr;
    // modal
    QWidget *backdrop = nullptr;
    ModalCard *card = nullptr;
    QWidget *cardHead = nullptr;
    QScrollArea *cardScroll = nullptr;
    QWidget *cardBody = nullptr;   // the padded wrapper of the caller's body
    QWidget *cardFoot = nullptr;
    bool cardWide = false;
    // toasts
    QWidget *toasts = nullptr;
    // timers
    QTimer *saveTimer = nullptr;
    QTimer *healthTimer = nullptr;
    bool topbarCompact = false;     // the top bar's buttons show their icon only (fitTopbar)
    int topbarWords = 0;            // the width their words take
    bool relayoutPending = false;   // a modal relayout is queued (eventFilter)
    bool ready = false;
    bool syncing = false;
};

namespace {
App *g_current = nullptr;
}

App *App::current()
{
    return g_current;
}

App::App(QWidget *parent) : QWidget(parent), ui(new Ui)
{
    g_current = this;
    setObjectName("qtmRoot");
    setAttribute(Qt::WA_StyledBackground);
    setFocusPolicy(Qt::ClickFocus);
    ui->saveTimer = new QTimer(this);
    ui->saveTimer->setSingleShot(true);
    connect(ui->saveTimer, &QTimer::timeout, this, [this] {
        state.panes.clear();
        for (auto *p : panes) state.panes.push_back(p->cfg);
        state.active = panes.indexOf(active);
        api::put(this, "/api/state", state.toJson(), {}, [this](const QString &message) {
            toast(QString("Couldn't save: %1").arg(message), "err");
        });
    });
    ui->healthTimer = new QTimer(this);
    connect(ui->healthTimer, &QTimer::timeout, this, [this] { checkHealth(); });
    buildShell();
    applyStyle();
    qApp->installEventFilter(this);
    api::get(this, "/api/state",
             [this](const QJsonValue &v) { init(v.toObject()); },
             [this](const QString &) { init(QJsonObject()); });   // first run
}

App::~App()
{
    if (g_current == this) g_current = nullptr;
    qApp->removeEventFilter(this);
    for (auto *p : panes) p->destroy();
    // The panels go first, while the widgets they placed in our layout still exist.
    delete panels;
    panels = nullptr;
    delete ui;
    ui = nullptr;
}

bool App::isReady() const
{
    return ui->ready;
}

void App::init(const QJsonObject &saved)
{
    state = AppState::fromJson(saved);
    theme::setCurrent(state.settings.theme);
    applyStyle();
    loadCustomIndicators([this] {
        buildToolbar();
        buildDrawbar();
        setLayout(state.layout, false);
        panels = new Panels(this);
        if (QWidget *b = panels->bottomWidget()) {
            ui->centerLayout->replaceWidget(ui->bottomPlaceholder, b);
            ui->bottomPlaceholder->deleteLater();
            ui->bottomPlaceholder = nullptr;
        }
        if (QWidget *s = panels->sideWidget()) {
            ui->mainLayout->replaceWidget(ui->sidePlaceholder, s);
            ui->sidePlaceholder->deleteLater();
            ui->sidePlaceholder = nullptr;
        }
        // Control+` shows / hides the Terminal tab, as in VS Code (Qt's META is the Control key on macOS).
#ifdef Q_OS_MACOS
        auto *terminalKey = new QShortcut(QKeySequence(Qt::META | Qt::Key_QuoteLeft), this);
#else
        auto *terminalKey = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_QuoteLeft), this);   // Ctrl+` elsewhere
#endif
        terminalKey->setContext(Qt::WidgetWithChildrenShortcut);
        connect(terminalKey, &QShortcut::activated, this, [this] { if (panels) panels->toggleTerminal(); });
        listenAlerts();
        checkHealth();
        ui->healthTimer->start(60000);
        ui->ready = true;
        if (isVisible() && !window()->focusWidget()) setFocus();
        emit ready();
    });
}

void App::applyStyle()
{
    setStyleSheet(theme::styleSheet());
}

void App::buildShell()
{
    ui->root = new QVBoxLayout(this);
    ui->root->setContentsMargins(0, 0, 0, 0);
    ui->root->setSpacing(0);

    // ---- top bar
    auto *tb = new QWidget;
    tb->setObjectName("qtmTopbar");
    tb->setAttribute(Qt::WA_StyledBackground);
    tb->setFixedHeight(44);
    // Everything stays in sight: a window narrower than the bar gets a second line of it.
    auto *h = new qtm::kit::WrapBarLayout(tb, 2, 8);
    h->setContentsMargins(8, 6, 30, 6);   // clear of the side flaps
    ui->topbar = tb;
    h->addWidget(new Brand);
    BtnStyle sym = tbStyle();
    sym.fontPx = 14;
    sym.weight = 600;
    ui->symbolBtn = new Btn(sym, QString(), "BTCUSDT");
    ui->symbolBtn->setToolTip("Change symbol (or just start typing)");
    ui->symbolBtn->setObjectName("symbolBtn");
    h->addWidget(ui->symbolBtn);
    h->addWidget(new Sep);
    ui->tfBox = new QWidget;
    ui->tfLayout = new QHBoxLayout(ui->tfBox);
    ui->tfLayout->setContentsMargins(0, 0, 0, 0);
    ui->tfLayout->setSpacing(1);
    h->addWidget(ui->tfBox);
    h->addSpacing(4);
    ui->tfCustom = new QLineEdit;
    ui->tfCustom->setObjectName("tfCustom");
    ui->tfCustom->setPlaceholderText("7m");
    ui->tfCustom->setToolTip("Any timeframe: 1s, 7m, 2h, 3D, 2W, 3M");
    ui->tfCustom->setAlignment(Qt::AlignCenter);
    ui->tfCustom->setFixedSize(46, 28);
    h->addWidget(ui->tfCustom);
    h->addWidget(new Sep);
    ui->chartType = new Select;
    theme::setCls(ui->chartType, "tb");
    ui->chartType->setObjectName("chartType");
    ui->chartType->setToolTip("Chart type");
    ui->chartType->setFixedHeight(32);
    h->addWidget(ui->chartType);
    ui->footprintBtn = new Btn(tbIconStyle(), IconSettings, QString());
    ui->footprintBtn->setToolTip("Footprint settings");
    ui->footprintBtn->setVisible(false);
    h->addWidget(ui->footprintBtn);
    ui->indicatorsBtn = new Btn(tbStyle(), IconIndicators, "Indicators");
    ui->indicatorsBtn->setToolTip("Indicators");
    h->addWidget(ui->indicatorsBtn);
    ui->compareBtn = new Btn(tbStyle(), IconCompare, "Compare");
    ui->compareBtn->setToolTip("Compare symbols");
    h->addWidget(ui->compareBtn);
    h->addWidget(new Sep);
    ui->layoutSel = new Select;
    theme::setCls(ui->layoutSel, "tb");
    ui->layoutSel->setObjectName("layoutSel");
    ui->layoutSel->setToolTip("Chart layout");
    ui->layoutSel->setFixedHeight(32);
    for (const auto &[v, l] : QVector<QPair<QString, QString>>{{"1", "1 chart"}, {"2v", "2 side by side"}, {"2h", "2 stacked"},
                                                             {"3", "3 charts"}, {"4", "4 charts"}, {"6", "6 charts"},
                                                             {"8", "8 charts"}, {"16", "16 charts"}})
        ui->layoutSel->addOption(l, v);
    h->addWidget(ui->layoutSel);
    ui->syncSymbol = new QCheckBox("Sync");
    ui->syncSymbol->setObjectName("syncSymbol");
    theme::setCls(ui->syncSymbol, "tb check");
    ui->syncSymbol->setToolTip("Change the symbol on every chart at once");
    ui->syncSymbol->setFixedHeight(32);
    ui->syncSymbol->setFocusPolicy(Qt::ClickFocus);   // an <input>: once clicked, typing no longer opens the symbol search
    ui->syncSymbol->setCursor(Qt::PointingHandCursor);
    ui->syncSymbol->setAttribute(Qt::WA_Hover);
    h->addWidget(ui->syncSymbol);
    ui->replayBtn = new Btn(tbStyle(), IconReplay, "Replay");
    ui->replayBtn->setToolTip("Bar replay");
    h->addWidget(ui->replayBtn);
    ui->alertBtn = new Btn(tbStyle(), IconAlert, "Alert");
    ui->alertBtn->setToolTip("Create alert");
    h->addWidget(ui->alertBtn);
    ui->helpBtn = new Btn(tbStyle(), QString(), "Help");
    ui->helpBtn->setToolTip("What's new, how to update, and common questions");
    connect(ui->helpBtn, &QAbstractButton::clicked, this, [] { QDesktopServices::openUrl(QUrl(HelpUrl)); });
    ui->helpBtn->setProperty("marginLeftAuto", true);   // this and the rest on the right
    h->addWidget(ui->helpBtn);
    ui->logBtn = new Btn(tbStyle(), QString(), "Log");
    ui->logBtn->setToolTip("Logarithmic price scale");
    h->addWidget(ui->logBtn);
    ui->snapBtn = new Btn(tbIconStyle(), IconSnap, QString());
    ui->snapBtn->setToolTip("Download chart image");
    h->addWidget(ui->snapBtn);
    ui->csvBtn = new Btn(tbIconStyle(), IconCsv, QString());
    ui->csvBtn->setToolTip("Export chart data (CSV)");
    h->addWidget(ui->csvBtn);
    ui->settingsBtn = new Btn(tbIconStyle(), IconSettings, QString());
    ui->settingsBtn->setToolTip("Settings");
    h->addWidget(ui->settingsBtn);
    ui->themeBtn = new Btn(tbIconStyle(), IconTheme, QString());
    ui->themeBtn->setToolTip("Light / dark");
    h->addWidget(ui->themeBtn);
    ui->themeBtn->setVisible(!theme::hasPalette());   // a host palette decides the theme
    ui->status = new StatusDot;
    h->addWidget(ui->status);
    // Its items never shrink (flex: none).
    for (int i = 0; i < h->count(); ++i)
        if (QWidget *w = h->itemAt(i)->widget())
            w->setSizePolicy(QSizePolicy::Fixed, w->sizePolicy().verticalPolicy());
    ui->root->addWidget(tb);

    // ---- main: draw bar, centre (grid + bottom panel), side panel
    auto *mainArea = new QWidget;
    ui->mainArea = mainArea;
    ui->mainLayout = new QHBoxLayout(mainArea);
    ui->mainLayout->setContentsMargins(0, 0, 0, 0);
    ui->mainLayout->setSpacing(0);
    ui->drawbar = new QWidget;
    ui->drawbar->setObjectName("qtmDrawbar");
    ui->drawbar->setAttribute(Qt::WA_StyledBackground);
    ui->drawbar->setFixedWidth(44);
    ui->drawLayout = new QVBoxLayout(ui->drawbar);
    ui->drawLayout->setContentsMargins(0, 6, 1, 0);
    ui->drawLayout->setSpacing(2);
    ui->drawLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    // A window too short for all the tools scrolls them with the wheel rather than holding the window up.
    auto *drawScroll = new QScrollArea;
    drawScroll->setObjectName("qtmDrawbarScroll");
    drawScroll->setFrameShape(QFrame::NoFrame);
    drawScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    drawScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    drawScroll->setWidgetResizable(true);
    drawScroll->setFixedWidth(44);
    drawScroll->setWidget(ui->drawbar);
    ui->mainLayout->addWidget(drawScroll);
    ui->center = new QWidget;
    ui->centerLayout = new QVBoxLayout(ui->center);
    ui->centerLayout->setContentsMargins(0, 0, 0, 0);
    ui->centerLayout->setSpacing(0);
    ui->grid = new QWidget;
    ui->grid->setObjectName("qtmGrid");
    ui->grid->setAttribute(Qt::WA_StyledBackground);
    ui->grid->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    ui->centerLayout->addWidget(ui->grid, 1);
    ui->bottomPlaceholder = new QWidget;
    ui->bottomPlaceholder->setFixedHeight(39);
    ui->centerLayout->addWidget(ui->bottomPlaceholder);
    ui->mainLayout->addWidget(ui->center, 1);
    ui->sidePlaceholder = new QWidget;
    ui->sidePlaceholder->setFixedWidth(300);
    ui->mainLayout->addWidget(ui->sidePlaceholder);
    ui->root->addWidget(mainArea, 1);

    // ---- modal backdrop and toasts (on top of everything)
    ui->backdrop = new QWidget(this);
    ui->backdrop->setObjectName("qtmModalBackdrop");
    ui->backdrop->setAttribute(Qt::WA_StyledBackground);
    ui->backdrop->hide();
    ui->backdrop->installEventFilter(this);
    ui->toasts = new QWidget(this);
    ui->toasts->setObjectName("qtmToasts");
    ui->toasts->hide();
}

// ---------------------------------------------------------------- persistence

void App::persist()
{
    state.panes.clear();
    for (auto *p : panes) state.panes.push_back(p->cfg);
    state.active = panes.indexOf(active);
    ui->saveTimer->start(400);
}

QJsonArray App::getDrawings(const QString &symbol) const
{
    return state.drawings.value(symbol).toArray();
}

void App::saveDrawings(const QString &symbol, const QJsonArray &items)
{
    QJsonArray clean;
    for (const auto &v : items) {
        const QJsonObject d = v.toObject();
        QJsonObject o;
        for (const char *k : {"id", "type", "points", "text", "color", "rr"})
            if (d.contains(k) && !d.value(k).isUndefined()) o[k] = d.value(k);
        clean.append(o);
    }
    state.drawings[symbol] = clean;
    for (auto *p : panes)
        if (p->cfg.symbol == symbol && p != drawingSource) p->drawings->setItems(drawingsFromJson(getDrawings(symbol)));
    persist();
}

void App::saveDrawingsFrom(ChartPane *source, const QString &symbol, const QJsonArray &items)
{
    drawingSource = source;
    saveDrawings(symbol, items);
    drawingSource = nullptr;
}

// ---------------------------------------------------------------- custom indicators

void App::loadCustomIndicators(std::function<void()> done)
{
    api::get(this, "/api/custom",
        [this, done](const QJsonValue &v) {
            for (const auto &c : v.toArray()) importCustom(c.toObject().value("file").toString());
            done();
        },
        [done](const QString &) { done(); });   // none
}

IndicatorDefPtr App::importCustom(const QString &file)
{
    Backend *b = Backend::instance();
    const QString dir = b ? b->customDir() : QString();
    try {
        return loadCustomIndicatorFile(dir + "/" + file, "custom:" + customIndicatorBaseName(file));
    } catch (const Error &e) {
        toast(QString("Custom indicator %1 failed to load: %2").arg(file, e.message()), "err");
        return nullptr;
    } catch (const std::exception &e) {
        toast(QString("Custom indicator %1 failed to load: %2").arg(file, QString::fromUtf8(e.what())), "err");
        return nullptr;
    }
}

// ---------------------------------------------------------------- layout

void App::setLayout(const QString &layout, bool save)
{
    const int count = LAYOUTS.value(layout, 1);
    state.layout = layout;
    QVector<PaneConfig> cfgs;
    if (!panes.isEmpty())
        for (auto *p : panes) cfgs.push_back(p->cfg);
    else
        cfgs = state.panes;
    while (panes.size() > count) {
        ChartPane *p = panes.takeLast();
        if (active == p) active = nullptr;
        p->destroy();
        p->hide();
        p->deleteLater();
    }
    static const QStringList tfs = {"1D", "4h", "1h", "15m", "1W", "5m", "30m", "2h"};
    QVector<ChartPane *> created;
    for (int i = 0; i < count; ++i) {
        if (i < panes.size()) continue;
        PaneConfig base;
        if (i < cfgs.size()) {
            base = cfgs[i];
        } else {
            base = cfgs.isEmpty() ? PaneConfig() : cfgs[0];
            base.tf = tfs[i % tfs.size()];
            base.indicators.clear();
        }
        auto *pane = new ChartPane(this, base, ui->grid);
        panes.push_back(pane);
        created.push_back(pane);
    }
    // CSS grid: columns / rows of the layout, the first pane spanning two rows in "3".
    delete ui->grid->layout();
    auto *g = new QGridLayout(ui->grid);
    g->setContentsMargins(0, 0, 0, 0);
    g->setSpacing(1);
    int cols = 1, rows = 1;
    switch (count) {
    case 2: cols = layout == "2h" ? 1 : 2; rows = layout == "2h" ? 2 : 1; break;
    case 3: cols = 2; rows = 2; break;
    case 4: cols = 2; rows = 2; break;
    case 6: cols = 3; rows = 2; break;
    case 8: cols = 4; rows = 2; break;
    case 16: cols = 4; rows = 4; break;
    default: break;
    }
    if (count == 3) {
        g->addWidget(panes[0], 0, 0, 2, 1);
        g->addWidget(panes[1], 0, 1);
        g->addWidget(panes[2], 1, 1);
        g->setColumnStretch(0, 2);
        g->setColumnStretch(1, 1);
    } else {
        for (int i = 0; i < panes.size(); ++i) g->addWidget(panes[i], i / cols, i % cols);
        for (int c = 0; c < cols; ++c) g->setColumnStretch(c, 1);
    }
    for (int r = 0; r < rows; ++r) g->setRowStretch(r, 1);
    for (auto *p : panes) p->show();
    for (auto *p : created) p->load();
    ChartPane *target = panes.value(std::min(std::max(state.active, 0), count - 1));
    if (state.active < 0) target = nullptr;
    setActive(target ? target : panes.value(0));
    for (auto *p : panes) p->setActiveFrame(p == active);
    {
        QSignalBlocker b(ui->layoutSel);
        ui->layoutSel->setValue(state.layout);
    }
    if (save) persist();
}

void App::setActive(ChartPane *pane)
{
    if (!pane || active == pane) return;
    if (active) active->setActiveFrame(false);
    active = pane;
    pane->setActiveFrame(true);
    syncToolbar();
    if (panels) panels->onActiveChange();
}

void App::paneLoadFinished(ChartPane *pane, const QString &error)
{
    const auto listeners = std::exchange(paneLoadedListeners, {});
    for (const auto &listener : listeners)
        if (!listener(pane, error)) paneLoadedListeners << listener;
}

void App::onPaneLoaded(ChartPane *pane)
{
    paneLoadFinished(pane, QString());
    if (pane == active) {
        syncToolbar();
        if (panels) panels->onActiveChange();
    }
}

// ---------------------------------------------------------------- toolbar

void App::buildToolbar()
{
    for (const QString &t : TF_QUICK) {
        auto *b = new Btn(tfStyle(), QString(), t);
        connect(b, &QAbstractButton::clicked, this, [this, t] { setTf(t); });
        ui->tfLayout->addWidget(b);
        ui->tfBtns.push_back(b);
    }
    connect(ui->tfCustom, &QLineEdit::returnPressed, this, [this] {
        QString v = ui->tfCustom->text().trimmed();
        static const QRegularExpression d(R"(^(\d+)d$)", QRegularExpression::CaseInsensitiveOption);
        static const QRegularExpression w(R"(^(\d+)w$)", QRegularExpression::CaseInsensitiveOption);
        v.replace(d, "\\1D");
        v.replace(w, "\\1W");
        try {
            parseTf(v);
            setTf(v);
            ui->tfCustom->clear();
            ui->tfCustom->clearFocus();
            setFocus();
        } catch (const Error &e) {
            toast(e.message(), "err");
        }
    });
    for (const auto &[v, l] : CHART_TYPES) ui->chartType->addOption(l, v);
    connect(ui->chartType, &QComboBox::activated, this, [this](int) {
        // <select> onchange: only when the value really changes
        if (!active || active->cfg.type == ui->chartType->value()) return;
        active->cfg.type = ui->chartType->value();
        active->render(true);
        syncToolbar();
        persist();
    });
    connect(ui->footprintBtn, &QAbstractButton::clicked, this, [this] {
        if (active) openFootprintSettings(active);
    });
    connect(ui->symbolBtn, &QAbstractButton::clicked, this, [this] { openSymbolSearch(); });
    connect(ui->indicatorsBtn, &QAbstractButton::clicked, this, [this] { openIndicatorPicker(); });
    connect(ui->compareBtn, &QAbstractButton::clicked, this, [this] {
        openSymbolSearch(QString(), [this](const QString &sym) {
            active->cfg.compare.push_back(sym);
            persist();
            active->load(true);
        }, QString::fromUtf8("Compare with…"));
    });
    connect(ui->layoutSel, &QComboBox::activated, this, [this](int) {
        if (ui->layoutSel->value() != state.layout) setLayout(ui->layoutSel->value());   // onchange
    });
    ui->syncSymbol->setChecked(state.settings.syncSymbol);
    connect(ui->syncSymbol, &QCheckBox::toggled, this, [this](bool on) {
        state.settings.syncSymbol = on;
        persist();
    });
    connect(ui->replayBtn, &QAbstractButton::clicked, this, [this] {
        if (active->replay) {
            active->stopReplay();
            return;
        }
        setTool("replay-pick");
        toast("Click the bar you want replay to start from.");
    });
    connect(ui->alertBtn, &QAbstractButton::clicked, this, [this] { if (panels) panels->openAlertDialog(); });
    connect(ui->logBtn, &QAbstractButton::clicked, this, [this] {
        active->cfg.log = !active->cfg.log;
        active->render(false);
        syncToolbar();
        persist();
    });
    connect(ui->snapBtn, &QAbstractButton::clicked, this, [this] {
        const QImage img = active->screenshot();
        QByteArray png;
        QBuffer buf(&png);
        buf.open(QIODevice::WriteOnly);
        img.save(&buf, "PNG");
        download(this, QString("%1-%2.png").arg(theme::ticker(active->cfg.symbol), active->cfg.tf), png);
    });
    connect(ui->csvBtn, &QAbstractButton::clicked, this, [this] { exportCsv(); });
    connect(ui->themeBtn, &QAbstractButton::clicked, this, [this] { toggleTheme(); });
    connect(ui->settingsBtn, &QAbstractButton::clicked, this, [this] { openSettings(); });
}

void App::toggleTheme()
{
    state.settings.theme = state.settings.theme == "dark" ? "light" : "dark";
    theme::setCurrent(state.settings.theme);
    refreshTheme();
    persist();
}

void App::refreshTheme()
{
    applyStyle();
    if (ui->themeBtn) ui->themeBtn->setVisible(!theme::hasPalette());
    for (auto *p : panes) p->applyTheme();
    if (panels) panels->onTheme();
    for (auto *e : findChildren<QGraphicsDropShadowEffect *>()) e->setColor(theme::shadowColor());
    for (auto *w : findChildren<QWidget *>()) w->update();
    emit themeChanged();
}

void App::syncToolbar()
{
    ChartPane *p = active;
    if (!p) return;
    ui->symbolBtn->setLabel(theme::ticker(p->cfg.symbol));
    ui->symbolBtn->setToolTip(QString::fromUtf8("%1 — change symbol (or just start typing)").arg(p->cfg.symbol));
    for (auto *b : ui->tfBtns) b->setActive(b->label() == p->cfg.tf);
    ui->tfCustom->setPlaceholderText(TF_QUICK.contains(p->cfg.tf) ? "7m" : p->cfg.tf);
    theme::toggleCls(ui->tfCustom, "on", !TF_QUICK.contains(p->cfg.tf));
    {
        QSignalBlocker b(ui->chartType);
        ui->chartType->setValue(p->cfg.type);
    }
    ui->footprintBtn->setVisible(p->cfg.type == "footprint");
    ui->logBtn->setOn(p->cfg.log);
    ui->replayBtn->setOn(p->replay.has_value());
    ui->topbar->layout()->invalidate();
}

void App::setTf(const QString &tf)
{
    active->cfg.tf = tf;
    active->load();
    syncToolbar();
    persist();
}

void App::setSymbol(const QString &sym, ChartPane *pane)
{
    if (!pane) pane = active;
    QVector<ChartPane *> targets = state.settings.syncSymbol ? panes : QVector<ChartPane *>{pane};
    for (auto *p : targets) {
        p->cfg.symbol = sym;
        p->cfg.compare.removeAll(sym);
        p->load();
    }
    syncToolbar();
    persist();
}

void App::buildDrawbar()
{
    const auto &tools = drawingTools();
    for (int i = 0; i < tools.size(); ++i) {
        if (i == 1) ui->drawLayout->addWidget(new Hr, 0, Qt::AlignHCenter);
        auto *b = new Btn(drawStyle(), tools[i].icon, QString());
        b->setToolTip(tools[i].label);
        const QString id = tools[i].id;
        connect(b, &QAbstractButton::clicked, this, [this, id] { setTool(tool == id ? "cursor" : id); });
        ui->drawLayout->addWidget(b, 0, Qt::AlignHCenter);
        ui->toolBtns.insert(id, b);
    }
    ui->drawLayout->addWidget(new Hr, 0, Qt::AlignHCenter);
    auto *alert = new Btn(drawStyle(), IconDrawAlert, QString());
    alert->setToolTip("Alert when price crosses the selected line");
    connect(alert, &QAbstractButton::clicked, this, [this] { alertOnSelectedDrawing(); });
    ui->drawLayout->addWidget(alert, 0, Qt::AlignHCenter);
    auto *del = new Btn(drawStyle(), IconDelete, QString());
    del->setToolTip("Delete selected drawing (Delete key)");
    connect(del, &QAbstractButton::clicked, this, [this] {
        if (!active->deleteSelectedDrawing()) toast("Click a drawing first to select it.");
    });
    ui->drawLayout->addWidget(del, 0, Qt::AlignHCenter);
    auto *clear = new Btn(drawStyle(), IconClear, QString());
    clear->setToolTip("Remove all drawings on this symbol");
    connect(clear, &QAbstractButton::clicked, this, [this] {
        const auto r = QMessageBox::question(window(), QString(),
                                             QString("Remove every drawing on %1?").arg(theme::ticker(active->cfg.symbol)),
                                             QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Ok);
        if (r == QMessageBox::Ok) active->clearDrawings();
    });
    ui->drawLayout->addWidget(clear, 0, Qt::AlignHCenter);
    setTool("cursor");
}

void App::setTool(const QString &t)
{
    tool = t;
    for (auto it = ui->toolBtns.begin(); it != ui->toolBtns.end(); ++it) it.value()->setActive(it.key() == t);
    for (auto *p : panes) p->setInteractive(t == "cursor");
    if (t == "cursor") unsetCursor();
    else setCursor(Qt::CrossCursor);
}

// ---------------------------------------------------------------- modals

ModalCard *App::modal(const QString &title, QWidget *body, QWidget *foot, bool wide)
{
    if (ui->card) {
        ModalCard *old = ui->card;
        if (old->isAncestorOf(window()->focusWidget())) setFocus(Qt::OtherFocusReason);
        ui->card = nullptr;
        emit old->closed();
        old->hide();
        old->deleteLater();
    }
    auto *card = new ModalCard(ui->backdrop);
    card->setObjectName("qtmModalCard");
    card->setAttribute(Qt::WA_StyledBackground);
    auto *v = new QVBoxLayout(card);
    v->setContentsMargins(1, 1, 1, 1);
    v->setSpacing(0);
    auto *head = new QWidget;
    head->setAttribute(Qt::WA_StyledBackground);
    theme::setCls(head, "modal-head");
    auto *hh = new QHBoxLayout(head);
    hh->setContentsMargins(14, 0, 14, 1);
    hh->setSpacing(0);
    head->setFixedHeight(50);   // padding 12 + the close button's 24px line + 12, border 1
    auto *t = label(title, "modal-title");
    t->setFixedHeight(21);
    hh->addWidget(t, 1);
    BtnStyle cs;
    cs.height = 20;
    cs.padL = cs.padR = 6;
    cs.color = "--muted";
    cs.hoverBg = QString();
    auto *close = new Btn(cs, IconClose, QString());
    close->setToolTip("Close");
    connect(close, &QAbstractButton::clicked, this, [this] { closeModal(); });
    hh->addWidget(close);
    v->addWidget(head);
    auto *scroll = new QScrollArea;
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidgetResizable(true);
    auto *wrap = new QWidget;
    wrap->setObjectName("qtmModalBody");
    auto *wv = new QVBoxLayout(wrap);
    wv->setContentsMargins(14, 12, 14, 12);
    wv->setSpacing(0);
    if (body) wv->addWidget(body);
    wv->addStretch(0);
    scroll->setWidget(wrap);
    v->addWidget(scroll, 1);
    QWidget *footWrap = nullptr;
    if (foot) {
        footWrap = new QWidget;
        footWrap->setAttribute(Qt::WA_StyledBackground);
        theme::setCls(footWrap, "modal-foot");
        auto *fh = new QHBoxLayout(footWrap);
        fh->setContentsMargins(14, 11, 14, 10);
        fh->setSpacing(0);
        if (auto *bl = qobject_cast<QHBoxLayout *>(foot->layout())) {
            bool hasStretch = false;
            for (int i = 0; i < bl->count(); ++i)
                if (bl->itemAt(i)->spacerItem()) hasStretch = true;
            if (!hasStretch) bl->insertStretch(0, 1);
            if (bl->spacing() < 0) bl->setSpacing(8);
            bl->setContentsMargins(0, 0, 0, 0);
        }
        fh->addWidget(foot, 1);
        v->addWidget(footWrap);
    }
    auto *shadow = new QGraphicsDropShadowEffect(card);
    shadow->setBlurRadius(32);
    shadow->setOffset(0, 8);
    shadow->setColor(theme::shadowColor());
    card->setGraphicsEffect(shadow);
    ui->card = card;
    ui->cardHead = head;
    ui->cardScroll = scroll;
    ui->cardBody = wrap;
    ui->cardFoot = footWrap;
    ui->cardWide = wide;
    wrap->installEventFilter(this);
    if (body) body->installEventFilter(this);
    ui->backdrop->show();
    ui->backdrop->raise();
    ui->toasts->raise();
    card->show();
    relayoutModal();
    QTimer::singleShot(0, this, [this] { relayoutModal(); });
    return card;
}

void App::relayoutModal()
{
    ui->backdrop->setGeometry(rect());
    if (!ui->card) return;
    const int W = width(), H = height();
    const int cw = ui->cardWide ? std::min(780, int(W * 0.96)) : std::min(560, int(W * 0.94));
    const int maxH = int(H * 0.78);
    const int headH = std::max(ui->cardHead->sizeHint().height(), ui->cardHead->minimumHeight());
    const int footH = ui->cardFoot ? ui->cardFoot->sizeHint().height() : 0;
    QLayout *bl = ui->cardBody->layout();
    bl->activate();
    const int innerW = cw - 2;
    const int contentH = bl->hasHeightForWidth() ? bl->totalHeightForWidth(innerW) : bl->totalSizeHint().height();
    const int bodyH = std::max(0, std::min(contentH, maxH - headH - footH - 2));
    ui->card->setGeometry((W - cw) / 2, int(std::round(H * 0.09)), cw, headH + bodyH + footH + 2);
}

void App::closeModal()
{
    // Focus goes back to the page (document.body), not to the next input of the window.
    if (ui->card && ui->card->isAncestorOf(window()->focusWidget())) setFocus(Qt::OtherFocusReason);
    ui->backdrop->hide();
    if (ui->card) {
        ModalCard *c = ui->card;
        ui->card = nullptr;
        emit c->closed();
        c->hide();
        c->deleteLater();
    }
}

bool App::modalOpen() const
{
    return ui->card != nullptr;
}

ModalCard *App::currentModal() const
{
    return ui->card;
}

void App::openSymbolSearch(const QString &initial, std::function<void(const QString &)> onPick, const QString &title,
                           std::function<bool(const QString &)> inList)
{
    // Market tabs: "All" searches every source by name (the JS search); the others list a whole
    // market to browse and filter, for when you don't know the ticker (a deepwater addition).
    static const QVector<QPair<QString, QString>> MARKETS = {
        {"all", "All"}, {"crypto", "Crypto"}, {"stocks", "Stocks"}, {"etf", "ETFs"},
        {"forex", "Forex"}, {"futures", "Futures"}, {"indices", "Indices"},
    };
    static const QHash<QString, QString> ALL_GROUPS = {
        {"crypto", "All quotes"}, {"stocks", "All sectors"}, {"forex", "All pairs"},
        {"futures", "All kinds"}, {"indices", "All regions"},
    };
    static QString lastMarket = "all";   // the tab you used last, for the next search
    const int MAX_ROWS = 100;
    auto *body = new QWidget;
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);
    auto *q = new QLineEdit(initial);
    theme::setCls(q, "search-in");
    q->setObjectName("symQ");
    q->setPlaceholderText(QString::fromUtf8("BTC, AAPL, EURUSD, ^FTSE, VOD.L…"));
    q->setFixedHeight(38);
    v->addWidget(q);
    auto *bar = new QWidget;
    auto *bh = new QHBoxLayout(bar);
    bh->setContentsMargins(0, 8, 0, 0);
    bh->setSpacing(2);
    QVector<QPair<QString, QPushButton *>> tabs;
    for (const auto &[id, name] : MARKETS) {
        auto *b = new QPushButton(name);
        b->setObjectName("symMarket-" + id);
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        bh->addWidget(b);
        tabs.push_back({id, b});
    }
    bh->addStretch(1);
    v->addWidget(bar);
    // On its own row: sector names are too long to sit next to the tabs.
    auto *group = new Select;
    theme::setCls(group, "in");
    group->setObjectName("symGroup");
    group->padLeft = 7;
    group->setFocusPolicy(Qt::NoFocus);
    group->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    auto *groupRow = new QWidget;
    auto *gh = new QHBoxLayout(groupRow);
    gh->setContentsMargins(0, 8, 0, 0);
    gh->setSpacing(0);
    gh->addWidget(group);
    gh->addStretch(1);
    v->addWidget(groupRow);
    auto *list = new QWidget;
    list->setObjectName("symList");
    auto *lv = new QVBoxLayout(list);
    lv->setContentsMargins(0, 8, 0, 0);
    lv->setSpacing(0);
    v->addWidget(list);
    auto *hint = label("Crypto: Binance (falls back to Coinbase and Kraken). US stocks and ETFs: Nasdaq. Forex: Kraken. Global indices, futures and non-US stocks use Yahoo symbols (YAHOO:^FTSE, YAHOO:GC=F) when Yahoo is reachable.", "hint-text");
    hint->setWordWrap(true);
    hint->setMinimumWidth(1);
    hint->setContentsMargins(0, 8, 0, 0);
    v->addWidget(hint);
    ModalCard *card = modal(title, body);
    struct S {
        int sel = 0;
        QJsonArray results;   // the rows shown, in order (arrows and Enter go through them)
        QTimer timer;
        QVector<ListItem *> items;
        QVector<QWidget *> extra;   // status lines under the rows
        QString market = "all";
        QHash<QString, QJsonArray> rows;     // every symbol of a market, once loaded
        QHash<QString, QString> errors;
        int more = 0;                         // rows matching but not shown
    };
    auto st = std::make_shared<S>();
    st->market = initial.isEmpty() ? lastMarket : QString("all");
    st->timer.setSingleShot(true);
    QPointer<QLineEdit> qp(q);
    auto draw = std::make_shared<std::function<void()>>();
    std::weak_ptr<std::function<void()>> redraw = draw;
    auto pick = [this, onPick, inList, redraw](const QString &sym) {
        if (inList && onPick) {
            // Several picks in a row: the caller adds (or removes) it, the rows show the new marks.
            onPick(sym);
            if (auto d = redraw.lock()) (*d)();
            return;
        }
        closeModal();
        if (onPick) onPick(sym);
        else setSymbol(sym);
    };
    *draw = [st, q, lv, pick, list, inList]() {
        for (auto *it : st->items) it->deleteLater();
        st->items.clear();
        for (auto *w : st->extra) w->deleteLater();
        st->extra.clear();
        while (QLayoutItem *li = lv->takeAt(0)) delete li;
        for (int i = 0; i < st->results.size(); ++i) {
            const QJsonObject r = st->results[i].toObject();
            const QString sym = r.value("symbol").toString();
            QString name = r.value("name").toString();
            if (name.size() > 52) name = name.left(51).trimmed() + QString::fromUtf8("…");
            const bool marked = inList && inList(sym);
            auto *it = new ListItem(
                QString("<b>%1</b> <span style=\"color:%2\">%3</span>").arg(theme::esc(theme::ticker(sym)), theme::css("--muted"), theme::esc(name)),
                marked ? QString::fromUtf8("✓ in list")
                       : QString::fromUtf8("%1 · %2").arg(r.value("type").toString(), r.value("exchange").toString()),
                [pick, sym] { pick(sym); }, list);
            it->setSel(i == st->sel);
            lv->addWidget(it);
            st->items.push_back(it);
        }
        auto status = [&](const QString &text) {
            auto *l = label(text, "hint-text", false, list);
            l->setWordWrap(true);
            l->setMinimumWidth(1);
            l->setContentsMargins(10, 8, 10, 4);
            lv->addWidget(l);
            st->extra.push_back(l);
        };
        const bool browsing = st->market != "all";
        if (browsing && !st->rows.contains(st->market)) {
            status(st->errors.contains(st->market) ? QString("Couldn't load the list: %1").arg(st->errors.value(st->market))
                                                   : QString::fromUtf8("Loading…"));
            return;
        }
        if (browsing && st->more > 0)
            status(QString::fromUtf8("%1 more. Type a name or ticker, or pick a filter, to narrow it down.")
                       .arg(QLocale(QLocale::English, QLocale::UnitedStates).toString(st->more)));
        if (st->results.isEmpty() && browsing && q->text().trimmed().isEmpty()) status("Nothing in this list.");
        if (st->results.isEmpty() && !q->text().trimmed().isEmpty()) {
            auto *it = new ListItem(QString("%1 <b>%2</b>").arg(inList ? "Add" : "Open", theme::esc(q->text().trimmed().toUpper())), "press Enter",
                                    [pick, q] { pick(q->text().trimmed()); }, list);
            lv->addWidget(it);
            st->items.push_back(it);
        }
    };
    // Browsing: the market's rows matching the text and the group, exact tickers first.
    auto filter = [st, qp, group, MAX_ROWS]() {
        const QJsonArray all = st->rows.value(st->market);
        const QString term = qp ? qp->text().trimmed().toUpper() : QString();
        const QString g = group->value();
        QVector<QJsonObject> exact, starts, rest;
        for (const QJsonValue x : all) {
            const QJsonObject r = x.toObject();
            if (!g.isEmpty() && r.value("group").toString() != g) continue;
            if (term.isEmpty()) {
                rest << r;
                continue;
            }
            const QString t = theme::ticker(r.value("symbol").toString()).toUpper();
            if (t == term) exact << r;
            else if (t.startsWith(term)) starts << r;
            else if (t.contains(term) || r.value("name").toString().toUpper().contains(term)) rest << r;
        }
        QJsonArray out;
        int n = 0;
        for (const auto *part : {&exact, &starts, &rest})
            for (const auto &r : *part) {
                if (out.size() < MAX_ROWS) out.append(r);
                ++n;
            }
        st->results = out;
        st->more = n - int(out.size());
        st->sel = 0;
    };
    auto run = [this, st, qp, draw, card, filter]() {
        if (!qp) return;
        if (st->market != "all") {
            filter();
            (*draw)();
            return;
        }
        const QString v = qp->text().trimmed();
        if (v.isEmpty()) {
            st->results = QJsonArray();
            (*draw)();
            return;
        }
        auto done = [st, qp, draw, v](const QJsonArray &res) {
            if (st->market != "all") return;   // switched to a list meanwhile
            st->results = res;
            if (qp && qp->text().trimmed() == v) {
                st->sel = 0;
                (*draw)();
            }
        };
        api::search(card, v, [done](const QJsonValue &r) { done(r.toArray()); }, [done](const QString &) { done(QJsonArray()); });
    };
    auto setMarket = std::make_shared<std::function<void(const QString &)>>();
    std::weak_ptr<std::function<void(const QString &)>> again = setMarket;   // no cycle through its own capture
    *setMarket = [st, tabs, group, groupRow, draw, run, card, qp, again](const QString &m) {
        st->market = m;
        lastMarket = m;
        for (const auto &[id, b] : tabs) theme::setCls(b, id == m ? "tab active" : "tab");
        // The group filter: the groups of this market, in the list's order (sectors A to Z).
        group->blockSignals(true);
        group->clear();
        QStringList groups;
        for (const QJsonValue x : st->rows.value(m)) {
            const QString g = x.toObject().value("group").toString();
            if (!g.isEmpty() && !groups.contains(g)) groups << g;
        }
        if (m == "stocks") groups.sort(Qt::CaseInsensitive);
        group->addOption(ALL_GROUPS.value(m, "All"), QString(""));   // "": no filter (a null value means the label)
        for (const QString &g : groups) group->addOption(g);
        group->setCurrentIndex(0);
        groupRow->setVisible(!groups.isEmpty());
        group->blockSignals(false);
        st->results = QJsonArray();
        st->more = 0;
        if (m != "all" && !st->rows.contains(m)) {
            st->errors.remove(m);
            (*draw)();
            auto loaded = [st, m, draw, again](const QJsonValue &r, const QString &err) {
                if (err.isEmpty()) st->rows.insert(m, r.toArray());
                else st->errors.insert(m, err);
                if (st->market != m) return;   // another tab meanwhile
                if (err.isEmpty()) {
                    if (auto f = again.lock()) (*f)(m);
                } else {
                    (*draw)();
                }
            };
            api::get(card, "/api/symbols?market=" + m, [loaded](const QJsonValue &r) { loaded(r, QString()); },
                     [loaded](const QString &e) { loaded(QJsonValue(), e); });
            if (qp) qp->setFocus();
            return;
        }
        run();
        if (qp) qp->setFocus();
    };
    for (const auto &[id, b] : tabs) {
        const QString m = id;
        connect(b, &QPushButton::clicked, card, [setMarket, m] { (*setMarket)(m); });
    }
    connect(group, &QComboBox::currentIndexChanged, card, [run] { run(); });
    connect(&st->timer, &QTimer::timeout, card, run);
    connect(q, &QLineEdit::textEdited, card, [st] { st->timer.start(st->market == "all" ? 180 : 60); });
    // Arrows and Enter (q.onkeydown).
    class KeyFilter : public QObject
    {
    public:
        std::function<bool(QKeyEvent *)> fn;
        using QObject::QObject;
        bool eventFilter(QObject *, QEvent *e) override
        {
            if (e->type() == QEvent::KeyPress) return fn(static_cast<QKeyEvent *>(e));
            return false;
        }
    };
    // Moving the selection only restyles two rows and scrolls to the new one.
    auto select = [this, st](int i) {
        if (st->sel >= 0 && st->sel < st->items.size()) st->items[st->sel]->setSel(false);
        st->sel = i;
        if (i >= 0 && i < st->items.size()) {
            st->items[i]->setSel(true);
            if (ui->cardScroll) ui->cardScroll->ensureWidgetVisible(st->items[i], 0, 40);
        }
    };
    auto *kf = new KeyFilter(q);
    kf->fn = [st, q, select, pick](QKeyEvent *e) {
        if (e->key() == Qt::Key_Down) {
            select(std::max(0, std::min(st->sel + 1, int(st->results.size()) - 1)));
            return true;
        }
        if (e->key() == Qt::Key_Up) {
            select(std::max(st->sel - 1, 0));
            return true;
        }
        if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
            const QString sym = st->sel >= 0 && st->sel < st->results.size()
                ? st->results[st->sel].toObject().value("symbol").toString()
                : QString();
            pick(!sym.isEmpty() ? sym : q->text().trimmed());
            return true;
        }
        return false;
    };
    q->installEventFilter(kf);
    q->setFocus();
    q->setCursorPosition(q->text().size());
    (*setMarket)(st->market);   // runs the search for `initial`
}

void App::openIndicatorPicker()
{
    const auto all = indicators();
    auto *body = new QWidget;
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);
    auto *q = new QLineEdit;
    theme::setCls(q, "search-in");
    q->setObjectName("indQ");
    q->setPlaceholderText(QString::fromUtf8("Search %1 indicators…").arg(all.size()));
    q->setFixedHeight(38);
    v->addWidget(q);
    auto *list = new QWidget;
    list->setObjectName("indList");
    auto *lv = new QVBoxLayout(list);
    lv->setContentsMargins(0, 8, 0, 0);
    lv->setSpacing(0);
    v->addWidget(list);
    auto *foot = new QWidget;
    auto *fh = new QHBoxLayout(foot);
    fh->setSpacing(8);
    auto *fhint = label("No limit per chart. Write your own in the Indicator editor tab.", "hint-text");
    fh->addWidget(fhint);
    fh->addStretch(1);
    auto *newBtn = new QPushButton("New custom indicator");
    theme::setCls(newBtn, "btn");
    newBtn->setCursor(Qt::PointingHandCursor);
    fh->addWidget(newBtn);
    ModalCard *card = modal("Indicators", body, foot);
    auto onPick = [this](const QString &id) {
        const QString uid = active->addIndicator(id);
        const IndicatorDefPtr def = indicatorById(id);
        QString anchorKey;
        for (const auto &in : def->inputs)
            if (in.type == "time") { anchorKey = in.key; break; }
        if (!anchorKey.isEmpty()) {
            closeModal();
            pickAnchor(active, uid, anchorKey);
        } else {
            toast(QString("Added %1").arg(def->name));
        }
    };
    auto draw = [q, lv, list, all, onPick]() {
        while (QLayoutItem *li = lv->takeAt(0)) {
            if (li->widget()) li->widget()->deleteLater();
            delete li;
        }
        const QString term = q->text().trimmed().toLower();
        QVector<QPair<QString, QVector<IndicatorDefPtr>>> groups;
        for (const auto &d : all) {
            if (!term.isEmpty() && !QString("%1 %2 %3").arg(d->name, d->id, d->group).toLower().contains(term)) continue;
            const QString g = d->group.isEmpty() ? QString("Other") : d->group;
            int k = -1;
            for (int i = 0; i < groups.size(); ++i)
                if (groups[i].first == g) k = i;
            if (k < 0) {
                groups.push_back({g, {}});
                k = groups.size() - 1;
            }
            groups[k].second.push_back(d);
        }
        std::stable_sort(groups.begin(), groups.end(), [](const auto &a, const auto &b) {
            return (a.first == "Quant (bonus)") > (b.first == "Quant (bonus)");
        });
        for (const auto &[g, ds] : groups) {
            auto *gl = label(g, g == "Quant (bonus)" ? "group gold-text" : "group", false, list);
            QFont f = theme::font(11);
            f.setCapitalization(QFont::AllUppercase);
            f.setLetterSpacing(QFont::AbsoluteSpacing, 0.55);
            gl->setFont(f);
            gl->setContentsMargins(10, 10, 10, 4);
            gl->setFixedHeight(10 + 15 + 4);
            lv->addWidget(gl);
            for (const auto &d : ds) {
                const QString id = d->id;
                auto *it = new ListItem(theme::esc(d->name), d->overlay ? "on price" : "own pane", [onPick, id] { onPick(id); }, list);
                lv->addWidget(it);
            }
        }
    };
    connect(q, &QLineEdit::textEdited, card, draw);
    connect(newBtn, &QPushButton::clicked, card, [this] {
        closeModal();
        if (panels) panels->showBottom("editor");
    });
    draw();
    q->setFocus();
}

/** Wait for a click on the chart and anchor indicator `uid` there (Anchored VWAP). Escape keeps the current anchor. */
void App::pickAnchor(ChartPane *pane, const QString &uid, const QString &key)
{
    pendingAnchor = PendingAnchor{pane, uid, key};
    setTool("anchor-pick");
    toast("Click the bar to anchor it to.");
}

void App::openFootprintSettings(ChartPane *pane)
{
    using Settings = FootprintLayer::Settings;
    using Mode = FootprintLayer::Mode;
    const Settings st = Settings::fromJson(pane->cfg.footprint);
    QPointer<ChartPane> pp(pane);
    auto *body = new QWidget;
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);
    auto grid2 = [] {
        auto *w = new QWidget;
        auto *g = new QGridLayout(w);
        g->setContentsMargins(0, 0, 0, 0);
        g->setSpacing(10);
        g->setColumnStretch(0, 1);
        g->setColumnStretch(1, 1);
        return std::pair{w, g};
    };
    auto select = [] {
        auto *s = new Select;
        theme::setCls(s, "in");
        s->padLeft = 7;
        return s;
    };
    auto edit = [](const QString &text, const QString &placeholder = QString()) {
        auto *e = new QLineEdit(text);
        theme::setCls(e, "in");
        e->setPlaceholderText(placeholder);
        return e;
    };
    auto heading = [](const QString &text) {
        auto *h4 = label(text, "h4");
        QFont f = theme::font(12, 600);
        f.setCapitalization(QFont::AllUppercase);
        f.setLetterSpacing(QFont::AbsoluteSpacing, 0.48);
        h4->setFont(f);
        h4->setContentsMargins(0, 14, 0, 4);
        return h4;
    };
    auto hint = [](const QString &text) {
        auto *h = label(text, "hint-text p");
        h->setWordWrap(true);
        h->setMinimumWidth(1);
        h->setContentsMargins(0, 8, 0, 0);
        return h;
    };

    auto [gw, g] = grid2();
    auto *mode = select();
    mode->addOption("Bid x Ask", "bidask");
    mode->addOption("Delta", "delta");
    mode->addOption("Volume profile", "profile");
    mode->setValue(st.mode == Mode::Delta ? "delta" : st.mode == Mode::Profile ? "profile" : "bidask");
    g->addWidget(field("Display", mode), 0, 0, Qt::AlignTop);
    // Rows: Auto, or a number of ticks, with the price that makes when Binance has given the tick size.
    auto *rows = select();
    rows->addOption("Auto (fits the zoom)", "0");
    const double tick = pane->footprintTickSize();
    for (int n : {1, 2, 4, 5, 10, 20, 25, 50, 100, 200, 250, 500, 1000, 2500, 5000, 10000}) {
        QString text = n == 1 ? QString("1 tick") : QString("%1 ticks").arg(n);
        if (tick > 0) text += QString(" (%1)").arg(theme::fmtPrice(n * tick));
        rows->addOption(text, QString::number(n));
    }
    if (st.rowTicks > 0 && rows->findData(QString::number(st.rowTicks)) < 0)
        rows->addOption(QString("%1 ticks").arg(st.rowTicks), QString::number(st.rowTicks));
    rows->setValue(QString::number(st.rowTicks));
    g->addWidget(field("Row size", rows), 0, 1, Qt::AlignTop);
    v->addWidget(gw);

    v->addWidget(heading("Imbalances"));
    auto [iw, ig] = grid2();
    auto *ratio = edit(QString::number(st.params.ratio));
    ig->addWidget(field("Ratio (diagonal)", ratio), 0, 0, Qt::AlignTop);
    auto *minVol = edit(st.params.autoMinVolume ? QString() : QString::number(st.params.minVolume), "Auto");
    ig->addWidget(field("Minimum volume", minVol), 0, 1, Qt::AlignTop);
    auto *stack = edit(QString::number(st.params.stackCount));
    ig->addWidget(field("Stacked: rows in a row", stack), 1, 0, Qt::AlignTop);
    ig->addWidget(new QWidget, 1, 1);
    v->addWidget(iw);
    v->addWidget(hint("A buy imbalance is an ask volume at least ratio × the bid volume one row lower; a sell imbalance a "
                      "bid volume at least ratio × the ask volume one row higher. Rows whose stronger side is under the "
                      "minimum don't count; empty = Auto, the candle's average volume per row."));

    v->addWidget(heading("Colours"));
    auto [cw, cg] = grid2();
    auto *buy = new ColorInput((st.buy.isValid() ? st.buy : theme::color("--up")).name());
    auto *sell = new ColorInput((st.sell.isValid() ? st.sell : theme::color("--down")).name());
    auto *poc = new ColorInput((st.poc.isValid() ? st.poc : QColor("#ff9800")).name());
    cg->addWidget(field("Buyers", buy), 0, 0);
    cg->addWidget(field("Sellers", sell), 0, 1);
    cg->addWidget(field("POC", poc), 1, 0);
    cg->addWidget(new QWidget, 1, 1);
    v->addWidget(cw);

    v->addWidget(heading("History"));
    auto [hw, hg] = grid2();
    auto *budget = edit(QString::number(st.budget));
    hg->addWidget(field("Trades loaded at start", budget), 0, 0, Qt::AlignTop);
    hg->addWidget(new QWidget, 0, 1);
    v->addWidget(hw);
    v->addWidget(hint("Binance sends 1,000 trades a request; 300,000 take about half a minute. Candles before the first "
                      "trade loaded show as plain candles."));

    auto *foot = new QWidget;
    auto *fh = new QHBoxLayout(foot);
    auto *reset = new QPushButton("Defaults");
    theme::setCls(reset, "btn");
    reset->setCursor(Qt::PointingHandCursor);
    auto *ok = new QPushButton("Apply");
    theme::setCls(ok, "btn primary");
    ok->setCursor(Qt::PointingHandCursor);
    fh->addWidget(reset);
    fh->addWidget(ok);
    ModalCard *card = modal("Footprint", body, foot);
    auto apply = [this, pp](const QJsonObject &o) {
        closeModal();
        if (!pp) return;
        pp->cfg.footprint = o;
        pp->render(false);
        persist();
    };
    connect(ok, &QPushButton::clicked, card, [=] {
        Settings s;
        s.mode = mode->value() == "delta" ? Mode::Delta : mode->value() == "profile" ? Mode::Profile : Mode::BidAsk;
        s.rowTicks = rows->value().toInt();
        if (const double r = jsNumber(ratio->text()); r >= 1) s.params.ratio = r;
        const QString mv = minVol->text().trimmed();
        if (!mv.isEmpty() && mv.compare("auto", Qt::CaseInsensitive) != 0 && jsNumber(mv) >= 0) {
            s.params.minVolume = jsNumber(mv);
            s.params.autoMinVolume = false;
        }
        if (const double n = jsNumber(stack->text()); n >= 1) s.params.stackCount = int(n);
        if (const double b = jsNumber(budget->text()); b > 0) s.budget = int(std::clamp(b, 10000.0, 2000000.0));
        // A colour left at the theme's stays unset, so it follows the theme.
        if (QColor(buy->value()) != theme::color("--up")) s.buy = QColor(buy->value());
        if (QColor(sell->value()) != theme::color("--down")) s.sell = QColor(sell->value());
        if (QColor(poc->value()) != QColor("#ff9800")) s.poc = QColor(poc->value());
        apply(s.toJson());
    });
    connect(reset, &QPushButton::clicked, card, [apply] { apply(QJsonObject()); });
}

void App::openIndicatorSettings(ChartPane *pane, IndicatorInstance *indPtr)
{
    const IndicatorInstance ind = *indPtr;   // the instance is looked up again on Apply
    const IndicatorDefPtr def = indicatorById(ind.id);
    if (!def) return;
    QPointer<ChartPane> pp(pane);
    auto *body = new QWidget;
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);
    auto *gridW = new QWidget;
    auto *grid = new QGridLayout(gridW);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(10);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    int cell = 0;
    auto addField = [&](QWidget *f) {
        grid->addWidget(f, cell / 2, cell % 2, Qt::AlignTop);   // a grid cell stretches; the field's content sits at its top
        ++cell;
    };
    struct Input { QString key, type; QWidget *w; };
    auto inputs = std::make_shared<QVector<Input>>();
    for (const auto &i : def->inputs) {
        QJsonValue val = ind.params.value(i.key);
        if (val.isUndefined() || val.isNull()) val = i.def;
        if (i.type == "source") {
            // Indicators on indicators: any output of another indicator on this chart can be the source.
            auto *s = new Select;
            theme::setCls(s, "in");
            s->padLeft = 7;
            s->addGroup("Price");
            for (const char *p : {"close", "open", "high", "low", "hl2", "hlc3", "ohlc4", "hlcc4"}) s->addOption(p);
            QVector<QPair<QString, QString>> others;
            for (const auto &x : pane->cfg.indicators) {
                const IndicatorDefPtr xd = indicatorById(x.id);
                if (x.uid == ind.uid || jsText(x.params.value("source")).contains(ind.uid) || !xd || xd->outputs.isEmpty()) continue;
                for (const auto &o : xd->outputs)
                    others.push_back({QString("ind:%1:%2").arg(x.uid, o.key),
                                      QString("%1 %2: %3").arg(shortIndicatorName(xd->name), pane->indicatorParamsLabel(x), o.label)});
            }
            if (!others.isEmpty()) {
                s->addGroup("Another indicator");
                for (const auto &[val2, lab] : others) s->addOption(lab, val2);
            }
            s->setValue(jsText(val));
            if (s->currentIndex() >= 0 && s->itemData(s->currentIndex(), Qt::UserRole + 1).toBool()) s->setCurrentIndex(1);
            addField(field(i.label, s));
            inputs->push_back({i.key, QString(), s});
        } else if (i.type == "select") {
            auto *s = new Select;
            theme::setCls(s, "in");
            s->padLeft = 7;
            for (const QString &o : i.options) s->addOption(o);
            s->setValue(jsText(val));
            addField(field(i.label, s));
            inputs->push_back({i.key, QString(), s});
        } else if (i.type == "time") {
            auto *e = new QDateTimeEdit;
            theme::setCls(e, "in");
            e->setDisplayFormat("MM/dd/yyyy, hh:mm AP");
            e->setCalendarPopup(true);
            const QDateTime min(QDate(1970, 1, 1), QTime(0, 0));
            e->setMinimumDateTime(min);
            e->setSpecialValueText("mm/dd/yyyy, --:-- --");
            const double t = val.toDouble();
            if (js::truthy(t)) e->setDateTime(QDateTime::fromSecsSinceEpoch(qint64(t)));
            else e->setDateTime(min);
            auto *pick = new QPushButton("Pick on chart");
            theme::setCls(pick, "btn small fieldbtn");
            pick->setCursor(Qt::PointingHandCursor);
            const QString key = i.key, uid = ind.uid;
            connect(pick, &QPushButton::clicked, this, [this, pp, uid, key] {
                closeModal();
                if (pp) pickAnchor(pp, uid, key);
            });
            addField(field(i.label, e, pick));
            inputs->push_back({i.key, "time", e});
        } else {
            auto *e = new QLineEdit(jsText(val));
            theme::setCls(e, "in");
            addField(field(i.label, e));
            inputs->push_back({i.key, i.type, e});
        }
    }
    if (def->inputs.isEmpty()) {
        auto *none = label("No inputs.", "hint-text");
        grid->addWidget(none, cell / 2, cell % 2, Qt::AlignTop);
        ++cell;
    }
    auto *tfSel = new Select;
    theme::setCls(tfSel, "in");
    tfSel->padLeft = 7;
    for (const char *t : {"", "5m", "15m", "1h", "4h", "1D", "1W", "1M"})
        tfSel->addOption(*t ? QString(t) : QString("Same as chart (%1)").arg(pane->cfg.tf), t);
    tfSel->setValue(ind.tf);
    // Big Trades and the liquidity heatmap (Deepwater additions) come from trades and the order book, not bars: no
    // timeframe of their own.
    const bool hasTf = ind.id != "bigtrades" && ind.id != "heatmap";
    if (hasTf) addField(field("Timeframe", tfSel));
    v->addWidget(gridW);
    auto *hint = label(hasTf ? QString("A higher timeframe shows its value once each of its bars has closed, so history never peeks ahead.")
                       : ind.id == "heatmap"
                           ? QString::fromUtf8("Binance's order book by price level, updated every 100 ms; it has no history of the book, so the heatmap fills in from when the chart opens.")
                           : QString("Each bubble is judged against the threshold of its moment, so history never peeks ahead."),
                       "hint-text p");
    hint->setWordWrap(true);
    hint->setMinimumWidth(1);
    hint->setContentsMargins(0, 12, 0, 12);
    v->addWidget(hint);
    auto colors = std::make_shared<QVector<QPair<QString, ColorInput *>>>();
    if (!def->outputs.isEmpty()) {
        auto *h4 = label("Colours", "h4");
        QFont f = theme::font(12, 600);
        f.setCapitalization(QFont::AllUppercase);
        f.setLetterSpacing(QFont::AbsoluteSpacing, 0.48);
        h4->setFont(f);
        h4->setContentsMargins(0, 0, 0, 4);
        v->addWidget(h4);
        auto *cw = new QWidget;
        auto *cg = new QGridLayout(cw);
        cg->setContentsMargins(0, 0, 0, 0);
        cg->setSpacing(10);
        cg->setColumnStretch(0, 1);
        cg->setColumnStretch(1, 1);
        int k = 0;
        for (const auto &o : def->outputs) {
            QString c = (ind.colors.value(o.key).isEmpty() ? o.color : ind.colors.value(o.key)).left(7);
            if (!QRegularExpression("^#[0-9a-fA-F]{6}$").match(c).hasMatch()) c = "#000000";
            auto *ci = new ColorInput(c.toLower());
            cg->addWidget(field(o.label, ci), k / 2, k % 2);
            colors->push_back({o.key, ci});
            ++k;
        }
        if (k == 1) cg->addWidget(new QWidget, 0, 1);   // keep the second grid track (1fr 1fr)
        v->addWidget(cw);
    }
    auto *foot = new QWidget;
    auto *fh = new QHBoxLayout(foot);
    auto *reset = new QPushButton("Defaults");
    theme::setCls(reset, "btn");
    reset->setCursor(Qt::PointingHandCursor);
    auto *ok = new QPushButton("Apply");
    theme::setCls(ok, "btn primary");
    ok->setCursor(Qt::PointingHandCursor);
    fh->addWidget(reset);
    fh->addWidget(ok);
    ModalCard *card = modal(def->name, body, foot);
    const QString uid = ind.uid;
    auto find = [pp, uid]() -> IndicatorInstance * {
        if (!pp) return nullptr;
        for (auto &x : pp->cfg.indicators)
            if (x.uid == uid) return &x;
        return nullptr;
    };
    connect(ok, &QPushButton::clicked, card, [this, pp, find, inputs, colors, tfSel] {
        IndicatorInstance *x = find();
        if (!x) {
            closeModal();
            return;
        }
        x->params = QJsonObject();
        for (const auto &in : *inputs) {
            if (in.type == "time") {
                auto *e = static_cast<QDateTimeEdit *>(in.w);
                x->params[in.key] = e->dateTime() == e->minimumDateTime() ? 0.0 : double(e->dateTime().toSecsSinceEpoch() / 60 * 60);
            } else if (!in.type.isEmpty()) {
                x->params[in.key] = jsNumber(static_cast<QLineEdit *>(in.w)->text());
            } else {
                x->params[in.key] = static_cast<Select *>(in.w)->value();
            }
        }
        x->colors.clear();
        for (const auto &[k, ci] : *colors) x->colors.insert(k, ci->value());
        x->tf = tfSel->value();
        closeModal();
        pp->render(false);
        persist();
    });
    connect(reset, &QPushButton::clicked, card, [this, pp, find] {
        IndicatorInstance *x = find();
        if (!x) {
            closeModal();
            return;
        }
        x->params = QJsonObject();
        x->colors.clear();
        closeModal();
        pp->render(false);
        persist();
    });
}

void App::openSettings()
{
    AppSettings &s = state.settings;
    auto *body = new QWidget;
    auto *v = new QVBoxLayout(body);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);
    auto *gw = new QWidget;
    auto *g = new QGridLayout(gw);
    g->setContentsMargins(0, 0, 0, 0);
    g->setSpacing(10);
    g->setColumnStretch(0, 1);
    g->setColumnStretch(1, 1);
    auto *bars = new QLineEdit(QString::number(s.bars));
    theme::setCls(bars, "in");
    bars->setObjectName("sBars");
    g->addWidget(field("Bars to load per chart", bars), 0, 0);
    auto mk = [](const QString &name, const QVector<QPair<QString, QString>> &opts, const QString &val) {
        auto *sel = new Select;
        theme::setCls(sel, "in");
        sel->setObjectName(name);
        sel->padLeft = 7;
        for (const auto &[v2, l] : opts) sel->addOption(l, v2);
        sel->setValue(val);
        return sel;
    };
    auto *utc = mk("sUtc", {{"0", "My local time"}, {"1", "UTC"}}, s.utc ? "1" : "0");
    g->addWidget(field("Time axis", utc), 0, 1);
    const bool volumeShown = s.volume && !(active && active->cfg.volumeRemoved);
    auto *vol = mk("sVol", {{"1", "Show"}, {"0", "Hide"}}, volumeShown ? "1" : "0");
    g->addWidget(field("Volume on the price chart", vol), 1, 0);
    auto *stay = mk("sStay", {{"0", "Back to cursor"}, {"1", "Keep the tool"}}, s.stayInDrawMode ? "1" : "0");
    g->addWidget(field("After placing a drawing", stay), 1, 1);
    auto *sync = mk("sSync", {{"0", "Independent"}, {"1", "Synced"}}, s.syncCrosshair ? "1" : "0");
    g->addWidget(field("Crosshair across charts", sync), 2, 0);
    auto *countdown = mk("sCountdown", {{"1", "Show"}, {"0", "Hide"}}, s.countdown ? "1" : "0");
    g->addWidget(field("Countdown to the candle's close", countdown), 2, 1);
    v->addWidget(gw);
    auto *hint = label(QString("Everything you save lives in the <code style=\"font-family:'%1'\">data</code> folder next to the app, on your machine.")
                           .arg(theme::monoFont(12).family()),
                       "hint-text p", true);
    hint->setWordWrap(true);
    hint->setMinimumWidth(1);
    hint->setContentsMargins(0, 12, 0, 0);
    v->addWidget(hint);
    auto *foot = new QWidget;
    auto *fh = new QHBoxLayout(foot);
    auto *times = new QPushButton("Load times");
    theme::setCls(times, "btn");
    times->setCursor(Qt::PointingHandCursor);
    times->setToolTip("How long each chart took to load, in a text file to send with a report of slowness");
    connect(times, &QPushButton::clicked, this, [] {
        if (const Backend *b = Backend::instance())
            QDesktopServices::openUrl(QUrl::fromLocalFile(b->dataDir() + "/chart-timing.log"));
    });
    fh->addWidget(times);
    fh->addStretch(1);
    auto *ok = new QPushButton("Save");
    theme::setCls(ok, "btn primary");
    ok->setCursor(Qt::PointingHandCursor);
    fh->addWidget(ok);
    ModalCard *card = modal("Settings", body, foot);
    connect(ok, &QPushButton::clicked, card, [this, bars, utc, vol, volumeShown, stay, sync, countdown] {
        AppSettings &st = state.settings;
        const double n = jsNumber(bars->text());
        st.bars = int(std::max(200.0, std::min(20000.0, js::truthy(n) ? n : 3000.0)));
        st.utc = utc->value() == "1";
        const bool showVolume = vol->value() == "1";
        if (showVolume != volumeShown) {
            st.volume = showVolume;
            if (showVolume)   // back on every chart, whatever its legend had done
                for (auto *p : panes) p->cfg.volumeHidden = p->cfg.volumeRemoved = false;
        }
        st.countdown = countdown->value() == "1";
        st.stayInDrawMode = stay->value() == "1";
        st.syncCrosshair = sync->value() == "1";
        closeModal();
        persist();
        for (auto *p : panes) p->load(true);
    });
}

void App::syncCrosshair(ChartPane *src, double logical)
{
    if (ui->syncing) return;
    ui->syncing = true;
    const double utc = src->timeOfLogical(logical);
    for (auto *pane : panes) {
        if (pane == src || pane->shown.isEmpty()) continue;
        const double l = pane->logicalOf(utc);
        const int i = std::max(0, std::min(int(pane->shown.size()) - 1, int(js::round(l))));
        if (i >= 0 && i < pane->shown.size())
            pane->chart->setCrosshairPosition(pane->shown[i].close, pane->times[i], pane->main);
    }
    ui->syncing = false;
}

void App::exportCsv()
{
    ChartPane *p = active;
    QVector<QPair<QString, Values>> extra;
    for (const auto &ind : p->cfg.indicators) {
        const IndicatorDefPtr def = indicatorById(ind.id);
        const IndicatorResult res = p->indResults.value(ind.uid);
        if (!def) continue;
        for (const auto &o : def->outputs) {
            if (!res.values.contains(o.key)) continue;
            const QString key = QString("%1_%2").arg(ind.id, o.key);
            bool found = false;
            for (auto &e : extra)
                if (e.first == key) {
                    e.second = res.values[o.key];
                    found = true;
                }
            if (!found) extra.push_back({key, res.values[o.key]});
        }
    }
    download(this, QString("%1-%2.csv").arg(theme::ticker(p->cfg.symbol), p->cfg.tf), toCsv(p->shown, extra).toUtf8());
}

// ---------------------------------------------------------------- keys

void App::keyPressEvent(QKeyEvent *e)
{
    if (!ui->ready) return QWidget::keyPressEvent(e);
    QWidget *fw = window()->focusWidget();
    const bool typing = qobject_cast<QLineEdit *>(fw) || qobject_cast<QTextEdit *>(fw) || qobject_cast<QPlainTextEdit *>(fw)
        || qobject_cast<QComboBox *>(fw) || qobject_cast<QAbstractSpinBox *>(fw)
        || qobject_cast<QCheckBox *>(fw);   // <input type=checkbox> is an INPUT too
    if (e->key() == Qt::Key_Escape) {
        e->accept();
        if (modalOpen()) return closeModal();
        if (tool != "cursor") return setTool("cursor");
        if (active && active->drawings->selectedItem()) {
            active->drawings->selected.clear();
            active->drawings->update();
        }
        return;
    }
    if (typing || (e->modifiers() & (Qt::ControlModifier | Qt::MetaModifier | Qt::AltModifier))) return QWidget::keyPressEvent(e);
    if ((e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace) && active && active->deleteSelectedDrawing()) {
        e->accept();
        return;
    }
    if (modalOpen()) return QWidget::keyPressEvent(e);
    static const QRegularExpression re("^[a-z0-9^]$", QRegularExpression::CaseInsensitiveOption);
    const QString t = e->text();
    if (re.match(t).hasMatch()) {
        e->accept();
        openSymbolSearch(t.toUpper());
        return;
    }
    QWidget::keyPressEvent(e);
}

bool App::eventFilter(QObject *o, QEvent *e)
{
    if (!ui) return false;
    if (o == ui->backdrop && e->type() == QEvent::MouseButtonPress) {
        // Only a press on the backdrop itself (e.target === backdrop). A press in the card that no
        // widget accepted (a list item, which acts on release) propagates up to here too.
        if (!ui->backdrop->childAt(static_cast<QMouseEvent *>(e)->position().toPoint())) closeModal();
        return true;
    }
    if (ui->cardBody && (o == ui->cardBody || o->parent() == ui->cardBody) && e->type() == QEvent::LayoutRequest
        && !ui->relayoutPending) {   // one relayout for all the requests of a pass
        ui->relayoutPending = true;
        QTimer::singleShot(0, this, [this] {
            ui->relayoutPending = false;
            relayoutModal();
        });
    }
    // Scrollbars show their thumb only while the mouse is over the scrolling element (app.css).
    if ((e->type() == QEvent::Enter || e->type() == QEvent::Leave) && o->isWidgetType()) {
        QAbstractScrollArea *area = qobject_cast<QAbstractScrollArea *>(o);
        if (!area && o->parent()) area = qobject_cast<QAbstractScrollArea *>(o->parent());
        if (area && (o == area || o == area->viewport())) {
            const bool on = e->type() == QEvent::Enter || area->underMouse();
            for (QScrollBar *sb : {area->verticalScrollBar(), area->horizontalScrollBar()}) {
                if (sb->property("hover").toBool() == on) continue;
                sb->setProperty("hover", on);
                sb->style()->unpolish(sb);
                sb->style()->polish(sb);
                sb->update();
            }
        }
    }
    if (e->type() == QEvent::MouseButtonPress && o->isWidgetType()) {
        auto *w = static_cast<QWidget *>(o);
        if (w == this || isAncestorOf(w)) {
            // A pane becomes active on any mouse down inside it (capture phase in JS).
            for (QWidget *x = w; x && x != this; x = x->parentWidget())
                if (auto *pane = qobject_cast<ChartPane *>(x)) {
                    setActive(pane);
                    break;
                }
            // Clicking something that can't take focus blurs the focused input (focus goes to body).
            if (!(w->focusPolicy() & Qt::ClickFocus) && !qobject_cast<QLineEdit *>(w)) {
                QWidget *fw = window()->focusWidget();
                if (fw && fw != this && isAncestorOf(fw) && fw->window() == window()) setFocus(Qt::MouseFocusReason);
                else if (!fw) setFocus(Qt::MouseFocusReason);
            }
        }
    }
    return QWidget::eventFilter(o, e);
}

// A drawn line as an alert condition: trend lines and rays are checked at the line's price on
// each bar, on the server, so they fire with this tab closed.
QJsonObject App::lineOperandFor(const QJsonObject &d) const
{
    const QJsonArray pts = d.value("points").toArray();
    const QJsonObject a = pts.at(0).toObject(), b = pts.at(1).toObject();
    auto line = [&](const QString &extend, const QString &label) {
        return QJsonObject{{"kind", "line"}, {"label", label},
                           {"line", QJsonObject{{"t1", a.value("time")}, {"p1", a.value("price")}, {"t2", b.value("time")},
                                                {"p2", b.value("price")}, {"extend", extend}}}};
    };
    const QString type = d.value("type").toString();
    if (type == "hline") {
        const double p = a.value("price").toDouble();
        return QJsonObject{{"kind", "value"}, {"v", QString::number(p, 'g', 8).toDouble()}};
    }
    if (type == "hray")
        return QJsonObject{{"kind", "line"}, {"label", "Horizontal ray"},
                           {"line", QJsonObject{{"t1", a.value("time")}, {"p1", a.value("price")},
                                                {"t2", a.value("time").toDouble() + 1}, {"p2", a.value("price")}, {"extend", "right"}}}};
    if (type == "trend") return line("none", "Trend line (between its two points)");
    if (type == "ray") return line("right", "Ray");
    if (type == "extended") return line("both", "Extended line");
    if (type == "channel") return line("none", "Channel's first line");
    return QJsonObject();
}

void App::alertOnSelectedDrawing()
{
    Drawing *d = active ? active->drawings->selectedItem() : nullptr;
    if (!d) return toast("Click a line on the chart first to select it.");
    const QJsonObject right = lineOperandFor(d->toJson());
    if (right.isEmpty()) return toast("Alerts work on trend lines, rays, extended and horizontal lines.");
    if (panels)
        panels->openAlertDialog(QJsonObject{{"rules", QJsonArray{QJsonObject{
            {"left", QJsonObject{{"kind", "price"}, {"field", "close"}}}, {"op", "crosses"}, {"right", right}}}}});
}

// ---------------------------------------------------------------- replay across charts

void App::syncReplayFrom(ChartPane *pane, const Bar &bar)
{
    const double end = bar.time + parseTf(pane->cfg.tf).seconds;
    for (auto *p : panes)
        if (p != pane && !p->bars.isEmpty() && !(p->replay && !p->replay->follower)) p->replayTo(end);
}

void App::onReplayStart(ChartPane *pane)
{
    if (state.settings.syncReplay && !(pane->replay && pane->replay->follower) && pane->replay
        && pane->replay->index < pane->bars.size())
        syncReplayFrom(pane, pane->bars[pane->replay->index]);
    syncToolbar();
    if (panels) panels->onReplayChange();
}

void App::onReplayBar(ChartPane *pane, const Bar &bar)
{
    if (state.settings.syncReplay && !(pane->replay && pane->replay->follower)) syncReplayFrom(pane, bar);
    if (panels) panels->onReplayBar(pane, bar);
}

void App::onReplayStop(ChartPane *pane)
{
    for (auto *p : QVector<ChartPane *>(panes))
        if (p != pane && p->replay && p->replay->follower) p->stopReplay();
    syncToolbar();
    if (panels) panels->onReplayChange();
}

// ---------------------------------------------------------------- prices, alerts, status

void App::onPrice(const QString &symbol, double price)
{
    prices[symbol] = price;
    for (const auto &fn : priceListeners) fn(symbol, price);
}

void App::listenAlerts()
{
    Backend *b = Backend::instance();
    if (!b) return;
    connect(b, &Backend::alertFired, this, [this](const QJsonObject &e) {
        toast(QString("<b>Alert</b><br>%1").arg(theme::esc(e.value("text").toString())), "alert", 12000);
        beep();
        Notifications *n = Notifications::instance();
        if (n->supported() && n->permission() == "granted")
            n->notify("Quant Terminal alert", e.value("text").toString());
        if (panels) panels->onAlertFired(e);
    }, Qt::QueuedConnection);
}

void App::beep()
{
    playBeep(this);
}

void App::checkHealth()
{
    api::get(this, "/api/health",
        [this](const QJsonValue &v) {
            QStringList failing;
            const QJsonObject prov = v.toObject().value("providers").toObject();
            for (auto it = prov.begin(); it != prov.end(); ++it)
                if (!it.value().toObject().value("ok").toBool()) failing << it.key();
            setStatus(!failing.isEmpty() ? "warn" : "ok",
                      !failing.isEmpty() ? QString("Some sources are failing (%1); fallbacks are in use.").arg(failing.join(", "))
                                         : QString("All data sources responding"));
        },
        [this](const QString &) { setStatus("bad", "The local server is not responding. Is it still running?"); });
}

void App::setStatus(const QString &level, const QString &title)
{
    ui->status->level = level;
    if (!title.isEmpty()) ui->status->setToolTip(title);
    ui->status->update();
}

void App::toast(const QString &html, const QString &kind, int ms)
{
    auto *el = new QFrame(ui->toasts);
    el->setAttribute(Qt::WA_StyledBackground);
    theme::setCls(el, kind.isEmpty() ? "toast" : "toast " + kind);
    auto *h = new QHBoxLayout(el);
    h->setContentsMargins(12, 10, 12, 10);
    // line-height 1.4 of 13px
    const QString body = QString("<div style=\"-qt-line-height-type: fixed; line-height: 18px\">%1</div>").arg(html);
    auto *l = label(body, "toast-text", true);
    l->setWordWrap(true);
    l->setFont(theme::font(13));
    h->addWidget(l);
    auto *shadow = new QGraphicsDropShadowEffect(el);
    shadow->setBlurRadius(32);
    shadow->setOffset(0, 8);
    shadow->setColor(theme::shadowColor());
    el->setGraphicsEffect(shadow);
    el->setProperty("toastLabel", QVariant::fromValue<QObject *>(l));
    el->setProperty("toastHtml", body);
    el->show();
    ui->toasts->show();
    layoutToasts();
    QTimer::singleShot(ms, el, [this, el] {
        el->hide();
        el->deleteLater();
        QTimer::singleShot(0, this, [this] { layoutToasts(); });
    });
}

// .toasts: a column at the bottom right (gap 8); every toast as wide as the widest (max 360px).
void App::layoutToasts()
{
    if (!ui) return;
    QVector<QFrame *> list;
    for (QObject *o : ui->toasts->children())
        if (auto *f = qobject_cast<QFrame *>(o); f && !f->isHidden() && f->property("toastLabel").isValid()) list << f;
    if (list.isEmpty()) {
        ui->toasts->hide();
        return;
    }
    const int maxText = 360 - 24 - 4;
    int textW = 0;
    for (QFrame *f : list) {
        QTextDocument doc;
        doc.setDefaultFont(theme::font(13));
        doc.setDocumentMargin(0);
        doc.setHtml(f->property("toastHtml").toString());
        textW = std::max(textW, std::min(int(std::ceil(doc.idealWidth())) + 1, maxText));
    }
    const int w = textW + 24 + 4;
    int y = 0;
    QVector<int> hs;
    for (QFrame *f : list) {
        QTextDocument doc;
        doc.setDefaultFont(theme::font(13));
        doc.setDocumentMargin(0);
        doc.setHtml(f->property("toastHtml").toString());
        doc.setTextWidth(textW);
        const int th = int(std::round(doc.size().height()));
        auto *l = qobject_cast<QLabel *>(f->property("toastLabel").value<QObject *>());
        if (l) l->setFixedSize(textW, th);
        hs << th + 20 + 2;
    }
    int total = 0;
    for (int hh : hs) total += hh;
    total += 8 * (hs.size() - 1);
    for (int i = 0; i < list.size(); ++i) {
        list[i]->setGeometry(0, y, w, hs[i]);
        y += hs[i] + 8;
    }
    ui->toasts->setGeometry(width() - 14 - w, height() - 14 - total, w, total);
    ui->toasts->raise();
}

// Under the width of the whole top bar, the buttons with an icon lose their word (the tooltip stays) before it takes a second line.
void App::fitTopbar()
{
    const QVector<QPair<Btn *, QString>> buttons = {{ui->indicatorsBtn, "Indicators"}, {ui->compareBtn, "Compare"}, {ui->replayBtn, "Replay"}, {ui->alertBtn, "Alert"}};
    if (ui->topbarWords == 0) {   // measured once, with the words on
        for (const auto &[button, word] : buttons) {
            ui->topbarWords += button->sizeHint().width();
            button->setLabel(QString());
            ui->topbarWords -= button->sizeHint().width();
            button->setLabel(word);
        }
    }
    const int bar = ui->topbar->layout()->sizeHint().width();
    const bool compact = width() < (ui->topbarCompact ? bar + ui->topbarWords : bar);
    if (compact == ui->topbarCompact)
        return;
    ui->topbarCompact = compact;
    for (const auto &[button, word] : buttons)
        button->setLabel(compact ? QString() : word);
}

void App::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    fitTopbar();
    relayoutModal();
    layoutToasts();
}

bool App::livePaused() const
{
    return !isVisible() && state.paper.toObject().value("orders").toArray().isEmpty();
}

void App::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);
    // Spontaneous: the window was restored, while the feeds kept going.
    if (!e->spontaneous()) {
        for (auto *p : panes) p->resumeLive();
        if (panels) panels->setQuotesPaused(false);
    }
    if (!window()->focusWidget()) setFocus();
}

void App::hideEvent(QHideEvent *e)
{
    QWidget::hideEvent(e);
    if (e->spontaneous() || !livePaused()) return;
    for (auto *p : panes) p->pauseLive();
    if (panels) panels->setQuotesPaused(true);
}

} // namespace qtm

QString qtm::App::aiContext() const
{
    if (!active) return {};
    QString s = active->cfg.symbol + ", " + active->cfg.tf;
    if (active->replay) s += ", " + tr("bar replay");
    return s;
}
