#pragma once

// Port of quant-terminal/public/js/app.js + index.html: the whole terminal as one widget (the
// deepwater "Charts" page). Top bar, drawing bar, chart grid, bottom panel, side panel, modal,
// and toasts, with the same behaviour as the web app (the Academy ribbon is left out).

#include "../core/Indicators.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QPair>
#include <QPointer>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QKeyEvent;

#include <functional>
#include <optional>

namespace qtm {

class ChartPane;
class Panels;

// Keeps JS object key order (watchlists, strategies show in insertion order).
template <typename T>
struct OrderedMap {
    QVector<QPair<QString, T>> items;
    bool contains(const QString &k) const { return indexOf(k) >= 0; }
    int indexOf(const QString &k) const
    {
        for (int i = 0; i < items.size(); ++i)
            if (items[i].first == k) return i;
        return -1;
    }
    T value(const QString &k, const T &fallback = T()) const
    {
        const int i = indexOf(k);
        return i < 0 ? fallback : items[i].second;
    }
    T &operator[](const QString &k)   // inserts at the end when missing
    {
        const int i = indexOf(k);
        if (i >= 0) return items[i].second;
        items.push_back({k, T()});
        return items.back().second;
    }
    void remove(const QString &k) { const int i = indexOf(k); if (i >= 0) items.remove(i); }
    QStringList keys() const { QStringList out; for (const auto &p : items) out << p.first; return out; }
    bool isEmpty() const { return items.isEmpty(); }
};

// One indicator on a chart: { uid, id, params, colors, tf, hidden }.
struct IndicatorInstance {
    QString uid;
    QString id;
    QJsonObject params;
    QHash<QString, QString> colors;   // output key -> "#rrggbb"
    QString tf;                       // empty = same as chart
    bool hidden = false;
};

// A chart pane's cfg: { symbol, tf, type, indicators, compare, log }.
struct PaneConfig {
    QString symbol = "BINANCE:BTCUSDT";
    QString tf = "1D";
    QString type = "candles";
    QVector<IndicatorInstance> indicators;
    QStringList compare;
    bool log = false;
    QJsonObject footprint;   // Deepwater addition: the footprint chart's settings (FootprintLayer::Settings JSON)
};

struct AppSettings {
    QString theme = "dark";
    bool volume = true;
    int bars = 3000;
    bool utc = false;
    bool stayInDrawMode = false;
    bool syncCrosshair = false;
    bool syncSymbol = false;
    bool syncReplay = false;
};

// app.state, saved with PUT /api/state.
struct AppState {
    QString layout = "1";
    int active = 0;
    QVector<PaneConfig> panes;
    OrderedMap<QStringList> watchlists;
    QString activeWatchlist = "Main";
    QJsonObject drawings;                  // symbol -> [ { id, type, points, text, color, rr } ]
    AppSettings settings;
    QJsonValue paper = QJsonValue::Null;   // the paper account object, as in JS
    QJsonValue replayPaper = QJsonValue::Null;
    OrderedMap<QJsonObject> strategies;    // saved tester strategies

    QJsonObject extra;                     // any other key of the saved state, kept as is

    QJsonObject toJson() const;
    static AppState fromJson(const QJsonObject &o);
};

// The modal card returned by App::modal (JS returns the card element; callers find their inputs
// in the body/foot widgets they passed in).
class ModalCard : public QWidget
{
    Q_OBJECT
public:
    using QWidget::QWidget;
signals:
    void closed();
};

class App : public QWidget
{
    Q_OBJECT
public:
    // The AI side panel's context line: the active chart's symbol and timeframe (and bar replay).
    Q_INVOKABLE QString aiContext() const;
    // The backend (Backend::instance()) must exist before the App is created. The state is loaded
    // asynchronously (GET /api/state), like app.js init(); ready() is emitted once the panes exist.
    explicit App(QWidget *parent = nullptr);
    ~App() override;
    bool isReady() const;
    // The Charts page of Deepwater (there is one), for the local models' Trade skill (Panels::agent*); null before
    // it exists or after it is gone.
    static App *current();

    // ---- state (app.state / app.settings) ----
    AppState state;
    AppSettings &settings() { return state.settings; }
    void persist();                                   // debounced PUT /api/state (400 ms)
    QJsonArray getDrawings(const QString &symbol) const;
    void saveDrawings(const QString &symbol, const QJsonArray &items);
    // saveDrawings from a pane's own drawing layer (JS skips the pane whose items array it is).
    void saveDrawingsFrom(ChartPane *source, const QString &symbol, const QJsonArray &items);

    // ---- panes ----
    QVector<ChartPane *> panes;
    ChartPane *active = nullptr;
    void setLayout(const QString &layout, bool save = true);
    void setActive(ChartPane *pane);
    void onPaneLoaded(ChartPane *pane);
    // Called each time a pane has loaded its bars or failed to (`error`), until the listener returns true: a caller
    // waiting for a symbol it asked for.
    QVector<std::function<bool(ChartPane *, const QString &error)>> paneLoadedListeners;
    void paneLoadFinished(ChartPane *pane, const QString &error);
    void syncToolbar();
    void setTf(const QString &tf);
    void setSymbol(const QString &sym, ChartPane *pane = nullptr);   // null = active
    QString tool = "cursor";
    void setTool(const QString &tool);

    // Anchored indicators: waiting for a click on the chart.
    struct PendingAnchor { QPointer<ChartPane> pane; QString uid; QString key; };
    std::optional<PendingAnchor> pendingAnchor;
    void pickAnchor(ChartPane *pane, const QString &uid, const QString &key);

    // ---- custom indicators ----
    // JS importCustom(file): (re)load custom/<file> and register it; toast on failure.
    IndicatorDefPtr importCustom(const QString &file);

    // ---- modals and toasts ----
    // JS modal(title, bodyHtml, { foot, wide }). `body` and `foot` are owned by the card. The card
    // is closed (deleted) by closeModal(), the ✕ button, a click on the backdrop or Escape.
    // `body` goes in the scrolling .modal-body (padding 12px 14px). `foot` goes in .modal-foot
    // (padding 10px 14px, right-aligned): when its layout is a QHBoxLayout without a stretch, a
    // stretch is put in front so its items sit at the right (justify-content: flex-end), and its
    // spacing defaults to 8 (gap: 8px). The card resizes itself when the body's layout changes.
    ModalCard *modal(const QString &title, QWidget *body, QWidget *foot = nullptr, bool wide = false);
    void closeModal();
    bool modalOpen() const;
    // With `inList`, picking doesn't close the search: several symbols can be picked in a row, and
    // rows for which inList(symbol) is true are marked (adding to a watchlist; a deepwater addition).
    void openSymbolSearch(const QString &initial = QString(),
                          std::function<void(const QString &)> onPick = {},
                          const QString &title = "Symbol search",
                          std::function<bool(const QString &)> inList = {});
    void openIndicatorPicker();
    void openIndicatorSettings(ChartPane *pane, IndicatorInstance *ind);
    // The footprint chart's settings (a Deepwater addition): display, rows, imbalances, colours, history.
    void openFootprintSettings(ChartPane *pane);
    void openSettings();
    // toast(html, kind = "" | "err" | "alert", ms = 3500). `html` is rich text, like innerHTML.
    void toast(const QString &html, const QString &kind = QString(), int ms = 3500);

    // ---- crosshair / replay sync ----
    void syncCrosshair(ChartPane *src, double logical);
    void syncReplayFrom(ChartPane *pane, const Bar &bar);
    void onReplayStart(ChartPane *pane);
    void onReplayBar(ChartPane *pane, const Bar &bar);
    void onReplayStop(ChartPane *pane);

    // ---- prices ----
    QHash<QString, double> prices;
    QVector<std::function<void(const QString &, double)>> priceListeners;
    void onPrice(const QString &symbol, double price);
    // Live prices (chart feeds, watchlist quotes) stop while the page is hidden behind another
    // tab of the host app, unless paper orders are pending: they fill on those prices.
    bool livePaused() const;

    // ---- status dot ----
    void setStatus(const QString &level, const QString &title = QString());   // ok | warn | bad
    void beep();

    Panels *panels = nullptr;

    // ---- the rest of app.js ----
    void exportCsv();
    void alertOnSelectedDrawing();
    QJsonObject lineOperandFor(const QJsonObject &drawing) const;   // {} when not a line
    void checkHealth();
    void toggleTheme();
    // Re-applies the current theme colours everywhere (after toggleTheme or theme::setPalette):
    // stylesheet, charts, panels, shadows. Charts keep their data and view.
    void refreshTheme();
    void relayoutModal();
    // The demo harness / screenshots: the modal card currently shown (null when none).
    ModalCard *currentModal() const;

signals:
    void themeChanged();
    void ready();

protected:
    bool eventFilter(QObject *o, QEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;
    void showEvent(QShowEvent *e) override;
    void hideEvent(QHideEvent *e) override;

private:
    struct Ui;
    Ui *ui = nullptr;
    void init(const QJsonObject &saved);
    void buildShell();
    void buildToolbar();
    void buildDrawbar();
    void listenAlerts();
    void loadCustomIndicators(std::function<void()> done);
    void applyStyle();
    void layoutToasts();
    ChartPane *drawingSource = nullptr;
};

} // namespace qtm
