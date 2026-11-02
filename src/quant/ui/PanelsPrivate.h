#pragma once

// Internal state of qtm::Panels (the closure variables of initPanels in panels.js), shared by the
// Panels*.cpp files: Panels.cpp (tabs, watchlist, alerts, calendar, hooks), PanelsTester.cpp,
// PanelsMarket.cpp (screener, heatmap), PanelsPaper.cpp, PanelsEditor.cpp.

#include "Panels.h"

#include "App.h"
#include "ChartPane.h"
#include "PanelsKit.h"
#include "RuleBuilder.h"
#include "Terminal.h"

#include <QAbstractButton>
#include <QElapsedTimer>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPointer>
#include <QTimer>

#include <optional>

namespace lw { class Chart; }

namespace qtm {

namespace panels {

// A .tabs button (side and bottom tab bars), painted like the CSS.
class TabButton : public QAbstractButton
{
    Q_OBJECT
public:
    TabButton(const QString &text, const QString &tab, QWidget *parent = nullptr);
    QString tab;
    bool active = false;
    bool ptq = false;
    bool icon = false;      // #bottomToggle: the chevron icon
    bool rotated = false;   // .bottom.collapsed #bottomToggle svg
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *) override;
    void enterEvent(QEnterEvent *) override;
    void leaveEvent(QEvent *) override;

private:
    bool m_hover = false;
};

// .tab-body: holds what the JS writes with innerHTML. `fill` content takes the body's size
// (.tester, .seas, heatmap: height 100%); other content scrolls in the body (overflow: auto).
class Body : public QWidget
{
    Q_OBJECT
public:
    explicit Body(QWidget *parent = nullptr);
    void setContent(QWidget *content, bool fill = false);
    QWidget *content() const { return m_content; }
    QScrollArea *scroll() const { return m_scroll; }

protected:
    void resizeEvent(QResizeEvent *) override;

private:
    QPointer<QWidget> m_content;
    QPointer<QScrollArea> m_scroll;
};

// The <aside class="side"> and <div class="bottom"> frames (background and the 1px border).
class Frame : public QWidget
{
    Q_OBJECT
public:
    enum Edge { Left, Top };
    Frame(Edge edge, QWidget *parent = nullptr) : QWidget(parent), m_edge(edge) {}

protected:
    void paintEvent(QPaintEvent *) override;

private:
    Edge m_edge;
};

// The tester's working strategy (tState).
struct TesterState {
    QVector<QPair<QString, QVector<rules::Rule>>> groups;   // entryLong, exitLong, entryShort, exitShort
    // settings in JS key order
    QVector<QPair<QString, QJsonValue>> settings;
    QString name;
    QJsonValue setting(const QString &k) const;
    void set(const QString &k, const QJsonValue &v);
};

// lastResult: backtest result + what it ran on.
struct TesterResult {
    QJsonObject result;     // { stats, trades, equity }
    QString symbol, tf;
    Bars bars;
    QJsonObject strat;
    QStringList indicatorOrder;
    std::optional<Bars> subBars;
};

} // namespace panels

struct Panels::Private
{
    Panels *q;
    App *app;

    // ---- frames and tabs ----
    panels::Frame *side = nullptr;
    panels::Frame *bottomEl = nullptr;
    QVector<panels::TabButton *> sideTabs, bottomTabs;
    panels::TabButton *toggle = nullptr;
    panels::Body *sideBody = nullptr;
    panels::Body *bottom = nullptr;
    QPointer<TerminalPanel> terminal;   // made the first time the Terminal tab opens; kept after
    bool collapsed = true;
    QString sideTab = "watchlist", bottomTab = "tester";
    void showSide(const QString &t);
    void showBottom(const QString &t, bool toggleIt);
    void setCollapsed(bool on);
    void applySizes();   // .bottom heights and the max-width: 900px media query
    // The open panel's height: 320 px as in app.css until you drag its top edge (a deepwater
    // addition, for the Terminal tab mostly); kept in the settings.
    int bottomHeight = 320;
    void renderSide();
    void renderBottom();

    // ---- watchlist ----
    QHash<QString, QJsonObject> quotes;
    QPointer<QWidget> wlRows;
    QTimer quoteTimer;
    QStringList wlSymbols() const;
    void renderWatchlist();
    void drawRows();
    void refreshQuotes();

    // ---- alerts ----
    void renderAlerts();
    void openAlertDialog(const QJsonObject &preset);

    // ---- calendar ----
    QString calFilter = "High", calView = "economic";
    bool calMine = false;
    void renderCalendar();

    // ---- strategy tester (PanelsTester.cpp) ----
    panels::TesterState tState;
    std::optional<panels::TesterResult> lastResult;
    int testerRun = 0;   // the latest run: an earlier one still computing is dropped when it ends
    QPointer<QWidget> testerRoot;
    QHash<QString, QPointer<rules::RulesBox>> testerGroups;
    QHash<QString, QPointer<QWidget>> testerInputs;   // data-s inputs
    QPointer<QWidget> tOut;
    QPointer<lw::Chart> equityChart;
    void initTester();
    void strategyToUi(const QJsonObject &s);
    QJsonObject readTester(QStringList *order = nullptr);
    void renderTester();
    void runTester();
    void showResult(const panels::TesterResult &r);
    void applyStrategy(const QJsonObject &s, const QString &name);

    // ---- screener / heatmap (PanelsMarket.cpp) ----
    struct Screener {
        QString universe = "crypto";
        QJsonArray rows;
        QString sort = "volume";
        int dir = -1;
        QHash<QString, QJsonValue> filters;
        std::optional<QHash<QString, QJsonObject>> scan;
        rules::Rule rule;
        QString tf = "1D";
        QPointer<QWidget> table;
        QPointer<QPushButton> scanBtn;
    } sc;
    struct Heatmap {
        QString universe = "us", size = "marketCap";
        std::optional<QJsonArray> rows;
        QPointer<QWidget> canvas;
    } hm;
    void initScreener();
    void loadScreener();
    QVector<QJsonObject> filtered() const;
    void renderScreener();
    void drawScreener();
    void renderHeatmap();
    void drawHeatmap();

    // ---- paper trading (PanelsPaper.cpp) ----
    QPointer<kit::Stats> ppEq;
    QPointer<kit::Table> ppPos;
    QElapsedTimer paperRenderAt;
    ChartPane *replayPane() const;
    QJsonObject paper();                       // the account in use (a copy)
    void savePaper(const QJsonObject &acct);   // writes it back (replay or live)
    QJsonObject livePaper();
    std::optional<double> price(const QString &s) const;
    QStringList paperSymbols() const;
    void fill(const QString &sym, const QString &side, double qty, double px, const QString &note = QString(),
              bool liveAccount = false);
    void checkOrders(const QString &sym, double px);
    double equity();
    void drawPaperNumbers();
    void renderPaper();
    void checkReplayOrders(ChartPane *pane, const Bar &bar);

    // ---- indicator editor (PanelsEditor.cpp) ----
    QString edFile;
    QPointer<QWidget> edList;
    QPointer<QTextEdit> edCode;   // #edCode (a kit::CodeArea)
    QPointer<QLineEdit> edName;        // #edName
    void renderEditor();
    void renderEditorList();
    QWidget *edListWidget(const QJsonArray &list);

    QString lastSymbol;
    bool lastSymbolSet = false;
};

namespace panels {
// Ordered JSON text like JSON.stringify(value, null, 2), with object keys in the given order
// where known (`order` maps an object's path, e.g. "" or "indicators", to its key order).
QString stringify(const QJsonValue &v, const QHash<QString, QStringList> &order, const QString &path = QString(),
                  int indent = 0);
}

} // namespace qtm
