#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include "BuildInfo.h"
#include "HttpCache.h"
#include "LocalEngine.h"
#include "AiRead.h"
#include "JournalData.h"
#include "Myfxbook.h"
#include "MainWindow.h"
#include "DeskHistory.h"
#include "SettingsDialog.h"
#include "MarketData.h"
#include "NewsWebHost.h"
#include "Theme.h"
#include "quant/core/Error.h"
#include "quant/core/PineIndicators.h"

#include <QAbstractButton>
#include <QApplication>
#include <QIcon>
#include <QMessageBox>
#include <QPainter>
#include <QTextStream>
#include <QTimer>

#include <algorithm>

int main(int argc, char *argv[])
{
    // The update system runs a downloaded copy with --version before installing it: it must load and name its commit.
    for (int i = 1; i < argc; ++i)
        if (qstrcmp(argv[i], "--version") == 0) {
            QTextStream(stdout) << "Deepwater " << DEEPWATER_COMMIT << '\n';
            return 0;
        }

    // The News reader embeds web pages (Qt WebEngine), which wants shared GL contexts set before the application.
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication app(argc, argv);
    app.setOrganizationName("Deepwater");
    app.setApplicationName("Deepwater");

    QFont font = app.font();
    font.setFamilies(Theme::installedFamilies({"IBM Plex Sans", "Inter", "Helvetica Neue", "Segoe UI", "Arial"}));
    app.setFont(font);

    // The bundle's Info.plist gives Finder and the Dock the .icns; this covers a bare binary run.
    app.setWindowIcon(QIcon(":/icon/deepwater-512.png"));

    // Checks of the local engine, for the Windows CI. --llm-check <file.gguf>: it loads in the sandbox and answers.
    // --llm-sandbox-check <file>: from inside the sandbox that file, a write beside it and the network are all refused.
    {
        const QStringList args = app.arguments();
        const int sandboxArg = args.indexOf("--llm-sandbox-check");
        if (sandboxArg > 0 && sandboxArg + 1 < args.size())
            return LocalEngine::checkSandbox(args.at(sandboxArg + 1));
        const int llmArg = args.indexOf("--llm-check");
        if (llmArg > 0 && llmArg + 1 < args.size()) {
            LocalEngine engine;
            QObject::connect(&engine, &LocalEngine::loaded, &app, [&engine](const QJsonObject &info) {
                qWarning().noquote() << "loaded:" << QJsonDocument(info).toJson(QJsonDocument::Compact);
                engine.chat(QJsonArray {QJsonObject {{"role", "user"}, {"content", "Hello"}}}, QJsonObject {{"max_tokens", 8}});
            });
            QObject::connect(&engine, &LocalEngine::token, &app, [](const QString &t) { qWarning().noquote() << "token:" << t; });
            QObject::connect(&engine, &LocalEngine::answered, &app, [](const QJsonObject &) { QCoreApplication::exit(0); });
            QObject::connect(&engine, &LocalEngine::failed, &app, [](const QString &why) {
                qWarning().noquote() << "failed:" << why;
                QCoreApplication::exit(1);
            });
            QTimer::singleShot(120000, &app, [] { QCoreApplication::exit(2); });
            // Once the loop runs: a helper that can't start says so at once, and exit() only counts inside the loop.
            const QString model = args.at(llmArg + 1);
            QTimer::singleShot(0, &app, [&engine, model] { engine.load("check", {model}, 512); });
            return app.exec();
        }
    }

    // `deepwater --pine-js file.pine` prints the JavaScript PineTS runs for a Pine Script and exits.
    {
        const QStringList args = app.arguments();
        const int pineArg = args.indexOf("--pine-js");
        if (pineArg > 0 && pineArg + 1 < args.size()) {
            try {
                QTextStream(stdout) << qtm::pineIndicatorJs(args.at(pineArg + 1)) << '\n';
                return 0;
            } catch (const qtm::Error &e) {
                QTextStream(stderr) << e.message() << '\n';
                return 1;
            }
        }
    }

    MarketData data;
    if (!data.load(":/markets.json")) {
        QMessageBox::critical(nullptr, "Deepwater", "Could not load market data.");
        return 1;
    }

    // Dev helpers: `deepwater [--theme id] [--page charts] [--devtools] [--country FRA] [--indicator n] [--supply [port:id|oil:id|event:id]] [--news-ticker NVDA] [--economy-country USA [--economy-tab 2]] [--economy-indicator "Inflation Rate"] [--sources-tab 1] [--sources-disc [floor:1|ai:0|access:2]] [--news-article 0] [--news-favorite 0] [--news-url https://…] [--settings [0|1]] [--help-page [chapter]] [--scroll-end]
    // [--delay ms] --screenshot out.png` uses a theme for this run only (not saved), opens a page (and the developer panel), a
    // country's economy sheet or an indicator heatmap, renders the window after the delay (2.5 s by default) and exits.
    const QStringList args = app.arguments();
    const int themeArg = args.indexOf("--theme");
    MainWindow w(&data, themeArg > 0 && themeArg + 1 < args.size() ? args.at(themeArg + 1) : QString());
    w.show();

    // --journal-sample <file>: a statement in memory for this run; the journal's file isn't read or written.
    if (const int sampleArg = args.indexOf("--journal-sample"); sampleArg > 0 && sampleArg + 1 < args.size())
        JournalStore::instance()->loadSampleForThisRun(args.at(sampleArg + 1));
    // --history-sample <file>: a Desk history in memory for this run: nothing noted, read or written.
    if (const int sampleArg = args.indexOf("--history-sample"); sampleArg > 0 && sampleArg + 1 < args.size())
        DeskHistory::instance()->loadSampleForThisRun(args.at(sampleArg + 1));
    // --myfxbook-sample <file>: an outlook in memory for this run, without a request or a saved session.
    if (const int sampleArg = args.indexOf("--myfxbook-sample"); sampleArg > 0 && sampleArg + 1 < args.size())
        Myfxbook::instance()->loadSampleForThisRun(args.at(sampleArg + 1));
    const int pageArg = args.indexOf("--page");
    if (pageArg > 0 && pageArg + 1 < args.size())
        w.showPage(args.at(pageArg + 1));
    // --section <name>: a sub-tab of the page, as read_page names them.
    if (const int sectionArg = args.indexOf("--section"); sectionArg > 0 && sectionArg + 1 < args.size())
        if (QAbstractButton *b = aiFindSection(aiSectionButtons(w.currentPage()), args.at(sectionArg + 1)))
            b->click();
    if (args.contains("--devtools"))
        w.setDevToolsOpen(true);
    const int supplyArg = args.indexOf("--supply");
    if (supplyArg > 0)
        w.showSupply(supplyArg + 1 < args.size() && !args.at(supplyArg + 1).startsWith("--") ? args.at(supplyArg + 1) : QString());
    const int countryArg = args.indexOf("--country");
    if (countryArg > 0 && countryArg + 1 < args.size())
        w.showCountry(args.at(countryArg + 1));
    const int sourcesTabArg = args.indexOf("--sources-tab");
    if (sourcesTabArg > 0 && sourcesTabArg + 1 < args.size())
        w.showSourcesTab(std::clamp(args.at(sourcesTabArg + 1).toInt(), 0, 1));
    const int sourcesDiscArg = args.indexOf("--sources-disc");
    if (sourcesDiscArg > 0)
        w.showSourcesDisc(sourcesDiscArg + 1 < args.size() && !args.at(sourcesDiscArg + 1).startsWith("--") ? args.at(sourcesDiscArg + 1) : QString());
    const int searchArg = args.indexOf("--search");
    if (searchArg > 0 && searchArg + 1 < args.size())
        w.searchSources(args.at(searchArg + 1));
    const int tickerArg = args.indexOf("--news-ticker");
    if (tickerArg > 0 && tickerArg + 1 < args.size())
        w.filterNews(args.at(tickerArg + 1));
    const int economyCountryArg = args.indexOf("--economy-country");
    if (economyCountryArg > 0 && economyCountryArg + 1 < args.size()) {
        const int tabArg = args.indexOf("--economy-tab");
        w.openEconomyCountry(args.at(economyCountryArg + 1),
                             tabArg > 0 && tabArg + 1 < args.size() ? args.at(tabArg + 1).toInt() : -1);
    }
    const int economyIndicatorArg = args.indexOf("--economy-indicator");
    if (economyIndicatorArg > 0 && economyIndicatorArg + 1 < args.size())
        w.openEconomyIndicator(args.at(economyIndicatorArg + 1));
    const int articleArg = args.indexOf("--news-article");
    if (articleArg > 0 && articleArg + 1 < args.size())
        w.openNewsArticle(args.at(articleArg + 1).toInt());
    const int favoriteArg = args.indexOf("--news-favorite");
    if (favoriteArg > 0 && favoriteArg + 1 < args.size())
        w.favoriteNewsArticle(args.at(favoriteArg + 1).toInt());
    const int urlArg = args.indexOf("--news-url");
    if (urlArg > 0 && urlArg + 1 < args.size())
        w.openNewsUrl(args.at(urlArg + 1));
    const int modelArg = args.indexOf("--ai-search");
    if (modelArg > 0 && modelArg + 1 < args.size())
        w.searchModels(args.at(modelArg + 1));
    const int downloadArg = args.indexOf("--ai-download");
    if (downloadArg > 0 && downloadArg + 2 < args.size())
        w.downloadModelFile(args.at(downloadArg + 1), args.at(downloadArg + 2));
    const int localArg = args.indexOf("--ai-local");
    if (localArg > 0 && localArg + 1 < args.size())
        w.runLocalPrompt(args.at(localArg + 1));
    if (const int dockArg = args.indexOf("--ai-dock"); dockArg > 0)
        w.askAiDock(dockArg + 1 < args.size() && !args.at(dockArg + 1).startsWith("--") ? args.at(dockArg + 1) : QString());
    if (args.contains("--ml-export"))
        w.exportMlData();
    const int trainArg = args.indexOf("--ml-train");
    if (trainArg > 0 && trainArg + 2 < args.size())
        w.trainMl(args.at(trainArg + 1), args.at(trainArg + 2));
    const int indicatorArg = args.indexOf("--indicator");
    if (indicatorArg > 0 && indicatorArg + 1 < args.size())
        w.showIndicator(args.at(indicatorArg + 1).toInt());
    const int settingsArg = args.indexOf("--settings");
    if (settingsArg > 0) {
        w.openSettings();
        if (settingsArg + 1 < args.size())
            static_cast<SettingsDialog *>(w.settingsWindow())->showPage(args.at(settingsArg + 1).toInt());
    }
    // --update-dialog: the update icon's dialog, checking at once. --update-auto: installs what it finds (tests).
    if (args.contains("--update-dialog") || args.contains("--update-auto"))
        w.openUpdates(true);
    if (args.contains("--update-auto"))
        w.installUpdatesUnattended();
    if (const int helpArg = args.indexOf("--help-page"); helpArg > 0)
        w.openHelp(helpArg + 1 < args.size() && !args.at(helpArg + 1).startsWith("--") ? args.at(helpArg + 1) : QString());
    const int delayArg = args.indexOf("--delay");
    const int delay = delayArg > 0 && delayArg + 1 < args.size() ? args.at(delayArg + 1).toInt() : 2500;
    const int shotArg = args.indexOf("--screenshot");
    if (shotArg > 0 && shotArg + 1 < args.size()) {
        const QString path = args.at(shotArg + 1);
        const int scrollArg = args.indexOf("--scroll");
        const double scroll = args.contains("--scroll-end") ? 1.0
                              : scrollArg > 0 && scrollArg + 1 < args.size() ? args.at(scrollArg + 1).toDouble() : -1;
        // --settings and --help-page shoot their floating window instead of the main one.
        QWidget *floating = args.contains("--settings") ? w.settingsWindow()
                            : args.contains("--help-page") ? w.helpWindow()
                            : args.contains("--update-dialog") ? w.updateDialog() : nullptr;
        QTimer::singleShot(delay, &app, [&w, path, scroll, floating] {
            if (scroll >= 0) {
                w.scrollDetailToEnd(scroll);
                QApplication::processEvents();
            }
            QPixmap shot = floating ? floating->grab() : w.grab();
            // The reader's web view is a native view on macOS: its own snapshot is composited in.
            newsui::WebHost *host = floating ? nullptr : newsui::WebHost::visibleHost();
            if (!host) {
                shot.save(path);
                QApplication::quit();
                return;
            }
            const QRect at(host->mapTo(&w, QPoint(0, 0)), host->size());
            host->snapshot([shot, path, at](const QImage &image) mutable {
                if (!image.isNull()) {
                    QPainter p(&shot);
                    p.drawImage(at, image);
                }
                shot.save(path);
                QApplication::quit();
            });
        });
    }
    const int status = app.exec();
    HttpCache::waitForWrites();
    return status;
}
