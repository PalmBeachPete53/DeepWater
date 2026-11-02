#pragma once

// Port of quant-terminal/public/js/panels.js (with research.js, quantui.js and ptq.js): the side
// panel (Watchlist, Alerts, Calendar, News) and the bottom panel (Strategy tester, Screener,
// Heatmap, Seasonals, Financials, Paper trading, Indicator editor, Terminal, PTQ; Terminal is a
// deepwater addition, see Terminal.h). JS initPanels(app)
// becomes the Panels constructor; App places sideTabs/sideBody and bottom into its layout.

#include "../core/Bar.h"

#include <QJsonObject>
#include <QJsonValue>
#include <QObject>

#include <memory>

class QWidget;

namespace qtm {

class App;
class ChartPane;

class Panels : public QObject
{
    Q_OBJECT
public:
    explicit Panels(App *app);
    ~Panels() override;

    // The <aside class="side"> (tabs + body) and the <div class="bottom"> (tabs + body), built
    // here and inserted into the App layout.
    QWidget *sideWidget() const;
    QWidget *bottomWidget() const;

    void showSide(const QString &tab);
    void showBottom(const QString &tab, bool toggle = false);
    // Control+`, as in VS Code: opens the Terminal tab and focuses it; hides the panel when the
    // terminal already has the focus.
    void toggleTerminal();
    // JS openAlertDialog(preset): preset may hold { rules: [...], rule, symbol }.
    void openAlertDialog(const QJsonObject &preset = {});

    // Hooks app.js calls (panels?.onActiveChange?.() ...).
    void onActiveChange();
    void onReplayChange();
    void onReplayBar(ChartPane *pane, const Bar &bar);
    void onAlertFired(const QJsonObject &event);
    void onTheme();
    // Watchlist quotes are polled every 5 s, except while paused (App::livePaused); resuming
    // refreshes them at once.
    void setQuotesPaused(bool paused);

    struct Private;

private:
    std::unique_ptr<Private> d;
};

} // namespace qtm
