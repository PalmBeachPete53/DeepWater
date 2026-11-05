#include <QPointer>
#include <QAbstractButton>
#include "MainWindow.h"
#include "IndicatorData.h"
#include "IndicatorsView.h"
#include "CountriesView.h"

#include "AiPage.h"
#include "AiRead.h"
#include "MlData.h"
#include "AisStream.h"
#include "CalendarData.h"
#include "CalendarPage.h"
#include "CommodityPrices.h"
#include "Chokepoints.h"
#include "DomesticFinance.h"
#include "KeyIndicators.h"
#include "KeyedSources.h"
#include "NationalSources.h"
#include "SurveyHeadlines.h"
#include "ForexPage.h"
#include "FxData.h"
#include "MacroData.h"
#include "GlobeWidget.h"
#include "HeaderBar.h"
#include "LeftPanel.h"
#include "MarketData.h"
#include "PortWatch.h"
#include "SupplyChain.h"
#include <QCoreApplication>
#include <algorithm>
#include <cmath>
#include "QuantData.h"
#include "QuantMarketState.h"
#include "JournalPage.h"
#include "QuantPage.h"
#include "ResearchPage.h"
#include "DetailPanel.h"
#include "DevTools.h"
#include "AiDock.h"
#include "SideFlap.h"
#include "LocalChat.h"
#include "LocalTools.h"
#include "McpServer.h"
#include "MacroIndicators.h"
#include "MacroPanel.h"
#include "OverviewPage.h"
#include "NewsPage.h"
#include "EconomyPage.h"
#include "CountryData.h"
#include "SettingsDialog.h"
#include "HelpWindow.h"
#include "SourcesPage.h"
#include "Theme.h"
#include "VesselStore.h"
#include "quant/server/Server.h"
#include "quant/ui/App.h"
#include "quant/ui/Notifications.h"
#include "quant/ui/Theme.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QShortcut>
#include <QTimer>
#include <QSplitter>
#include <QStackedWidget>
#include <QTranslator>
#include <QVBoxLayout>

namespace {

QVector<FuturesQuotes::Contract> futuresContracts(const MarketData *data)
{
    QVector<FuturesQuotes::Contract> out;
    for (const Commodity &c : data->commodities())
        if (!c.future.symbol.isEmpty())
            out.push_back({c.future.symbol, c.future.roll, c.future.suffix, c.future.dailyOnly});
    return out;
}

QStringList benchmarkCodes(const MarketData *data)
{
    QStringList out;
    for (const Commodity &c : data->commodities())
        if (!c.imf.code.isEmpty())
            out << c.imf.code;
    return out;
}

} // namespace

MainWindow::MainWindow(MarketData *data, const QString &theme, QWidget *parent)
    : QMainWindow(parent)
    , m_data(data)
    , m_vessels(new VesselStore(this))
    , m_ais(new AisStream(m_vessels, this))
    , m_chokepoints(new ChokepointMonitor(m_vessels, this))
    , m_portWatch(new PortWatch(this))
    , m_fx(new FxData(this))
    , m_policyRates(new PolicyRates(this))
    , m_cot(new CotData(this))
    , m_cotReport(new CotReport(this))
    , m_economy(new WorldEconomy(this))
    , m_worldRates(new WorldPolicyRates(this))
    , m_bop(new BalanceOfPayments(this))
    , m_trade(new TradeFlows(this))
    , m_sectors(new EconomySectors(this))
    , m_propertyCredit(new PropertyCredit(this))
    , m_housePrices(new HousePriceRatios(this))
    , m_globalDebt(new GlobalDebt(this))
    , m_bankCredit(new BankCredit(this))
    , m_oecd(new OecdIndicators(this))
    , m_eurostat(new EurostatIndicators(this))
    , m_imfPrices(new ImfConsumerPrices(this))
    , m_bls(new BlsIndicators(this))
    , m_brazil(new BrazilIndicators(this))
    , m_argentina(new ArgentinaIndicators(this))
    , m_southAfrica(new SouthAfricaIndicators(this))
    , m_singapore(new SingaporeIndicators(this))
    , m_fred(new FredIndicators(this))
    , m_eia(new EiaIndicators(this))
    , m_bea(new BeaIndicators(this))
    , m_banxico(new BanxicoIndicators(this))
    , m_ecos(new EcosIndicators(this))
    , m_evds(new EvdsIndicators(this))
    , m_surveys(new SurveyHeadlines(this))
    , m_comtrade(new ComtradeProducts(this))
    , m_supply(new supply::SupplyChain(this))
    , m_keyIndicators(new KeyIndicators({m_oecd, m_eurostat, m_imfPrices, m_bls, m_brazil, m_argentina, m_southAfrica,
                                         m_singapore, m_fred, m_eia, m_bea, m_banxico, m_ecos, m_evds, m_surveys},
                                        this))
    , m_futures(new FuturesQuotes(futuresContracts(data), this))
    , m_benchmarks(new CommodityBenchmarks(benchmarkCodes(data), this))
    , m_economicCalendar(new EconomicCalendar(this))
    , m_corporateCalendar(new CorporateCalendar(this))
    , m_marketSessions(new MarketSessions(MarketHoursView::sessionSymbols(), this))
    , m_macro(new MacroIndicators(m_economy, m_worldRates, m_bop, m_sectors, this))
{
    // Quant Terminal backend: data and custom indicators live next to deepwater's other files.
    const QString quantRoot = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/quant-terminal";
    const QString customDir = quantRoot + "/custom";
    if (!QDir(customDir).exists()) {
        QDir().mkpath(customDir);
        QFile::copy(":/quant/ema-ribbon-squeeze.js", customDir + "/ema-ribbon-squeeze.js");
        QFile(customDir + "/ema-ribbon-squeeze.js").setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    }
    m_quantBackend = new qtm::Backend(quantRoot + "/data", customDir);
    m_quantBackend->start();
    // Clicking an alert's desktop notification brings the window up on the Charts page.
    connect(qtm::Notifications::instance(), &qtm::Notifications::activated, this, [this] {
        if (isMinimized()) showNormal();
        raise();
        activateWindow();
        showPage("charts");
    });

    m_seaRoutes.load(":/searoutes.json");
    m_ports.load(":/ports.json");
    resize(1600, 980);
    connect(m_vessels, &VesselStore::changed, this, [this] { updateVesselRoute(false); });

    // The data sets parse their cached copies on the thread pool, side by side, while the above runs:
    // wait for all of them before anything reads them.
    for (const RemoteDataSet *d : std::initializer_list<const RemoteDataSet *> {
             m_policyRates, m_cot, m_cotReport, m_economy, m_worldRates, m_bop, m_trade, m_sectors,
             m_propertyCredit, m_housePrices, m_globalDebt, m_bankCredit, m_oecd, m_eurostat, m_imfPrices, m_bls,
             m_brazil, m_argentina, m_southAfrica, m_singapore, m_fred, m_eia, m_bea, m_banxico, m_ecos, m_evds, m_surveys,
             m_futures, m_benchmarks, m_economicCalendar, m_corporateCalendar, m_marketSessions})
        d->waitCached();
    // Sources turned off in the Sources page make no requests (the Charts backend's included).
    SourcesPage::applySaved(sourceTargets());
    m_economy->refreshIfStale();
    m_worldRates->refreshIfStale();
    m_bop->refreshIfStale();
    m_trade->refreshIfStale();
    m_sectors->refreshIfStale();
    m_macro->setFinance({m_propertyCredit, m_housePrices, m_globalDebt, m_bankCredit});
    m_macro->setKeyIndicators(m_keyIndicators);
    countries::setKeyIndicators(m_keyIndicators);
    m_macro->setComtrade(m_comtrade);
    m_macro->setSupplyChain(m_supply);
    for (RemoteDataSet *d : std::initializer_list<RemoteDataSet *> {m_propertyCredit, m_housePrices, m_globalDebt, m_bankCredit,
                                                                    m_oecd, m_eurostat, m_imfPrices, m_bls, m_brazil, m_argentina,
                                                                    m_southAfrica, m_singapore, m_fred, m_eia, m_bea, m_banxico,
                                                                    m_ecos, m_evds, m_surveys})
        d->refreshIfStale();
    connect(m_trade, &RemoteDataSet::updated, this, &MainWindow::updateFlows);
    // Commodity prices: the cached ones at once, then fresh ones every five minutes (Yahoo's quotes are about ten
    // minutes behind the exchange; the IMF publishes once a month).
    auto applyPrices = [this] { m_data->applyPrices(m_futures, m_benchmarks); };
    applyPrices();
    connect(m_futures, &RemoteDataSet::updated, this, applyPrices);
    connect(m_benchmarks, &RemoteDataSet::updated, this, applyPrices);
    m_futures->refreshIfStale();
    m_benchmarks->refreshIfStale();
    auto *quoteTimer = new QTimer(this);
    connect(quoteTimer, &QTimer::timeout, m_futures, &RemoteDataSet::refreshIfStale);
    connect(quoteTimer, &QTimer::timeout, m_benchmarks, &RemoteDataSet::refreshIfStale);
    quoteTimer->start(5 * 60 * 1000);
    connect(m_macro, &MacroIndicators::updated, this, [this] {
        updateHeatmap();
        if (m_detail)
            m_detail->refreshMacro();
    });
    m_fx->refreshIfStale();
    m_policyRates->refreshIfStale();
    m_cot->refreshIfStale();
    m_cotReport->refreshIfStale();
    QStringList portIds;
    for (const Chokepoint &z : m_chokepoints->chokepoints())
        portIds << QString::fromUtf8(z.portwatchId);
    m_portWatch->setChokepoints(portIds);
    m_portWatch->refreshIfStale();
    m_supply->refreshIfStale();
    connect(m_supply, &supply::SupplyChain::updated, this, &MainWindow::updateSupplyPoints);
    auto *portWatchTimer = new QTimer(this);   // PortWatch publishes weekly; check twice a day
    connect(portWatchTimer, &QTimer::timeout, m_supply, &supply::SupplyChain::refreshIfStale);
    connect(portWatchTimer, &QTimer::timeout, m_portWatch, &PortWatch::refreshIfStale);
    connect(portWatchTimer, &QTimer::timeout, m_fx, &FxData::refreshIfStale);
    connect(portWatchTimer, &QTimer::timeout, m_policyRates, &PolicyRates::refreshIfStale);
    connect(portWatchTimer, &QTimer::timeout, m_cot, &CotData::refreshIfStale);
    connect(portWatchTimer, &QTimer::timeout, m_cotReport, &CotReport::refreshIfStale);
    connect(portWatchTimer, &QTimer::timeout, m_economy, &WorldEconomy::refreshIfStale);
    connect(portWatchTimer, &QTimer::timeout, m_worldRates, &WorldPolicyRates::refreshIfStale);
    connect(portWatchTimer, &QTimer::timeout, m_bop, &BalanceOfPayments::refreshIfStale);
    connect(portWatchTimer, &QTimer::timeout, m_trade, &TradeFlows::refreshIfStale);
    connect(portWatchTimer, &QTimer::timeout, m_sectors, &EconomySectors::refreshIfStale);
    for (RemoteDataSet *d : std::initializer_list<RemoteDataSet *> {m_propertyCredit, m_housePrices, m_globalDebt, m_bankCredit,
                                                                    m_oecd, m_eurostat, m_imfPrices, m_bls, m_brazil, m_argentina,
                                                                    m_southAfrica, m_singapore, m_fred, m_eia, m_bea, m_banxico,
                                                                    m_ecos, m_evds, m_surveys})
        connect(portWatchTimer, &QTimer::timeout, d, &RemoteDataSet::refreshIfStale);
    portWatchTimer->start(60 * 60 * 1000);

    QSettings settings;
    applyTheme(theme.isEmpty() ? settings.value("theme", "ember").toString() : theme);
    applyLanguage(settings.value("language", "en").toString());
    // The environment variable wins over the saved key (handy for development).
    const QString envKey = qEnvironmentVariable("AISSTREAM_API_KEY");
    m_aisKeyFromEnv = !envKey.isEmpty();
    m_ais->setApiKey(m_aisKeyFromEnv ? envKey : settings.value("aisApiKey").toString());
    // Deepwater's tools served to opencode over MCP, from launch when turned on (AI › Models › Integration).
    new McpServer(mlSources(), this);
    buildUi();
    // The regimes and volatility forecasts of the Quant › Markov models are re-estimated in the background when
    // they are older than the last trading day.
    quant::StateRefresher::instance()->start();

    auto *esc = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(esc, &QShortcut::activated, this, [this] { clearSelection(); });
    auto *space = new QShortcut(QKeySequence(Qt::Key_Space), this);
    connect(space, &QShortcut::activated, this, [this] { m_globe->toggleAutoSpin(); });
    // The help, on the chapter of the page shown: F1, and the platform's own (Cmd+? on macOS).
    for (const QKeySequence &key : {QKeySequence(Qt::Key_F1), QKeySequence(QKeySequence::HelpContents)}) {
        auto *help = new QShortcut(key, this);
        connect(help, &QShortcut::activated, this, [this] { openHelp(); });
    }
}

MainWindow::~MainWindow()
{
    // The Charts page talks to the backend until it's gone: delete the pages first.
    delete takeCentralWidget();
    delete m_quantPage;   // when it wasn't in the page stack
    delete m_quantBackend;
}

void MainWindow::buildUi()
{
    m_selectedVessel = 0;
    m_selectedCountry.clear();
    setWindowTitle(tr("Deepwater — Commodity Markets"));

    // Left: home panel (overview, layers, watchlist) with selection details
    // shown over it. Right: macro indicators.
    m_left = new LeftPanel(m_data, m_vessels);
    m_detail = new DetailPanel(m_data, m_vessels, m_chokepoints, m_portWatch, m_macro);
    m_leftStack = new QStackedWidget;
    m_leftStack->addWidget(m_left);
    m_leftStack->addWidget(m_detail);
    m_leftStack->setMinimumWidth(300);
    m_globe = new GlobeWidget(m_data, m_vessels, m_chokepoints);
    m_indicators = new IndicatorPanel(m_macro);
    m_indicators->setMinimumWidth(260);

    auto *splitter = new QSplitter(Qt::Horizontal);
    splitter->setHandleWidth(1);
    splitter->setChildrenCollapsible(false);
    splitter->addWidget(m_leftStack);
    splitter->addWidget(m_globe);
    splitter->addWidget(m_indicators);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({340, 960, 300});

    m_header = new HeaderBar;
    m_pages = new QStackedWidget;
    m_pageById.clear();
    m_pageFactories.clear();
    auto *central = new QWidget;
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_header);
    layout->addWidget(m_pages, 1);
    setCentralWidget(central);
    connect(m_header, &HeaderBar::navigationRequested, this, &MainWindow::showPage);
    connect(m_header, &HeaderBar::settingsRequested, this, &MainWindow::openSettings);
    connect(m_header, &HeaderBar::helpRequested, this, [this] { openHelp(); });

    // The map is built at once, the other pages the first time they are shown. The Charts page is built at
    // once too: it is cheap, and it is what plays an alert's sound and desktop notification.
    addPage("map", tr("Map"), splitter);
    addPage("overview", tr("Overview"), [this] {
        auto *overview = new OverviewPage;
        connect(overview, &OverviewPage::openResearch, this, [this](const QString &symbol) {
            showPage("research");
            if (auto *research = qobject_cast<ResearchPage *>(m_pageById.value("research")))
                research->setSymbol(symbol);
        });
        return overview;
    });
    addPage("economy", tr("Economy"), [] { return new EconomyPage; });
    addPage("forex", tr("Forex"), [this] { return new ForexPage(m_fx, m_policyRates, m_cot); });
    if (!m_quantPage)
        m_quantPage = new qtm::App;
    addPage("charts", tr("Charts"), m_quantPage);
    addPage("calendar", tr("Calendar"),
            [this] { return new CalendarPage(m_economicCalendar, m_corporateCalendar, m_marketSessions); });
    addPage("news", tr("News"), [] { return new NewsPage; });
    addPage("research", tr("Research"), [] { return new ResearchPage; });
    addPage("quant", tr("Quant"), [this] { return new QuantPage(m_cotReport); });
    addPage("journal", tr("Journal"), [] { return new JournalPage; });
    addPage("ai", tr("AI"), [this] { return new AiPage(mlSources()); });
    addPage("sources", tr("Sources"), [this] {
        auto *sourcesPage = new SourcesPage(sourceTargets());
        connect(sourcesPage, &SourcesPage::aisKeyChanged, this, &MainWindow::setAisKey);
        return sourcesPage;
    });
    m_header->setNavOrder(QSettings().value("navOrder").toStringList());
    connect(m_header, &HeaderBar::navOrderChanged, this,
            [](const QStringList &ids) { QSettings().setValue("navOrder", ids); });
    m_header->setNavHidden(QSettings().value("navHidden").toStringList());
    connect(m_header, &HeaderBar::navHiddenChanged, this,
            [](const QStringList &ids) { QSettings().setValue("navHidden", ids); });

    // Developer tools: the flap hangs on the right edge of the page area, over the pages.
    m_devTools = new DevToolsDock(central, m_header->height());
    const bool devEnabled = QSettings().value("devtools/enabled", false).toBool();
    m_devTools->setVisible(devEnabled);
    m_devTools->setOpen(devEnabled && QSettings().value("devtools/open", false).toBool(), false);
    m_header->setDevToolsChecked(devEnabled);
    connect(m_header, &HeaderBar::devToolsToggled, this, &MainWindow::setDevToolsEnabled);
    connect(m_devTools, &DevToolsDock::openChanged, this,
            [](bool open) { QSettings().setValue("devtools/open", open); });

    // The AI side panel, under the developer tools' flap: AI › Local's conversation, from any page.
    m_aiDock = new AiDock(
        central, m_header->height(),
        [this]() -> LocalChat * {
            auto *ai = qobject_cast<AiPage *>(ensurePage("ai"));
            return ai ? ai->chat() : nullptr;
        },
        [this] { return pageContext(); });
    // The read_page and open_page tools of the read_data skill.
    LocalTools::setPageAccess(
        [this](const AiPageRequest &request, std::function<void(const AiPageRead &)> done) { aiReadPage(request, std::move(done)); },
        [this](const AiPageRequest &request, QString *error) { return aiOpenPage(request, error); });
    m_aiDock->setRightInset(devEnabled ? SideFlap::Width : 0);
    m_aiDock->setOpen(QSettings().value("ai/dockOpen", false).toBool(), false);
    connect(m_aiDock, &AiDock::openChanged, this, [](bool open) { QSettings().setValue("ai/dockOpen", open); });

    connect(m_globe, &GlobeWidget::selectionChanged, this, [this](const QVector<int> &sites) {
        if (sites.isEmpty()) {
            clearSelection(true);
            return;
        }
        m_selectedVessel = 0;
        m_globe->setRoute({}, {});
        setSelectedCountry({});
        m_left->setActiveCommodity({});
        m_detail->showSites(sites);
        showDetail();
    });
    connect(m_globe, &GlobeWidget::vesselSelected, this, [this](qint64 mmsi) { selectVessel(mmsi, false); });
    connect(m_globe, &GlobeWidget::chokepointSelected, this, [this](int zone) { selectChokepoint(zone, false); });
    connect(m_globe, &GlobeWidget::countrySelected, this, [this](const QString &iso) { selectCountry(iso, false); });
    connect(m_globe, &GlobeWidget::pointSelected, this, [this](const QString &key) { selectSupplyPoint(key, false); });
    connect(m_globe, &GlobeWidget::pointsModeChanged, this, [this](bool on) {
        if (on) {
            clearSelection(true);
            m_detail->showSupplyOverview();
            showDetail();
        } else if (m_leftStack->currentWidget() == m_detail && m_detail->showingSupply()) {
            clearSelection(true);
        }
    });
    connect(m_detail, &DetailPanel::supplyPointActivated, this, [this](const QString &key) { selectSupplyPoint(key, true); });
    connect(m_detail, &DetailPanel::vesselActivated, this, [this](qint64 mmsi) { selectVessel(mmsi, true); });
    connect(m_left, &LeftPanel::commoditySelected, this, &MainWindow::selectCommodity);
    connect(m_detail, &DetailPanel::commodityActivated, this, &MainWindow::selectCommodity);
    connect(m_detail, &DetailPanel::siteActivated, this, [this](int i) { selectSite(i, true); });
    connect(m_detail, &DetailPanel::countryActivated, this, [this](const QString &iso) { selectCountry(iso, true); });
    connect(m_detail, &DetailPanel::closeRequested, this, [this] { clearSelection(); });
    connect(m_detail, &DetailPanel::yearChanged, this, [this](int year) {
        m_indicatorYear = year;
        updateHeatmap();
    });
    connect(m_indicators, &IndicatorPanel::indicatorClicked, this, &MainWindow::selectIndicator);

    // Keep the active indicator across rebuilds (language or theme change).
    if (m_activeIndicator >= 0) {
        m_indicators->setActive(m_activeIndicator);
        m_detail->setActiveIndicator(m_activeIndicator);
        m_detail->showIndicator(m_activeIndicator, m_indicatorYear);
        showDetail();
        updateHeatmap();
    }
    updateFlows();
    updateSupplyPoints();
}

void MainWindow::searchSources(const QString &text)
{
    if (auto *research = qobject_cast<ResearchPage *>(m_pages->currentWidget())) {
        research->searchIcs(text);
        return;
    }
    showPage("sources");
    if (auto *page = qobject_cast<SourcesPage *>(m_pages->currentWidget()))
        page->setSearch(text);
}

void MainWindow::showSourcesTab(int index)
{
    showPage("sources");
    if (auto *page = qobject_cast<SourcesPage *>(m_pages->currentWidget()))
        page->showTab(index);
}

void MainWindow::showSourcesDisc(const QString &pin)
{
    showPage("sources");
    if (auto *page = qobject_cast<SourcesPage *>(m_pages->currentWidget()))
        page->showDisc(pin);
}

void MainWindow::searchModels(const QString &text)
{
    showPage("ai");
    if (auto *page = qobject_cast<AiPage *>(m_pages->currentWidget()))
        page->searchFor(text);
}

void MainWindow::openEconomyCountry(const QString &iso3, int tab)
{
    showPage("economy");
    if (auto *page = qobject_cast<EconomyPage *>(m_pages->currentWidget()))
        page->openCountry(iso3, tab);
}

void MainWindow::openEconomyIndicator(const QString &name)
{
    showPage("economy");
    if (auto *page = qobject_cast<EconomyPage *>(m_pages->currentWidget()))
        page->openIndicator(name);
}

void MainWindow::filterNews(const QString &ticker)
{
    showPage("news");
    if (auto *page = qobject_cast<NewsPage *>(m_pages->currentWidget()))
        page->setTicker(ticker);
}

void MainWindow::openNewsArticle(int index)
{
    showPage("news");
    if (auto *page = qobject_cast<NewsPage *>(m_pages->currentWidget()))
        page->openArticle(index);
}

void MainWindow::favoriteNewsArticle(int index)
{
    showPage("news");
    if (auto *page = qobject_cast<NewsPage *>(m_pages->currentWidget()))
        page->toggleFavorite(index);
}

void MainWindow::openNewsUrl(const QString &url)
{
    showPage("news");
    if (auto *page = qobject_cast<NewsPage *>(m_pages->currentWidget()))
        page->openUrl(url);
}

void MainWindow::exportMlData()
{
    showPage("ai");
    if (auto *page = qobject_cast<AiPage *>(m_pages->currentWidget()))
        page->exportMlData();
}

void MainWindow::trainMl(const QString &file, const QString &target)
{
    showPage("ai");
    if (auto *page = qobject_cast<AiPage *>(m_pages->currentWidget()))
        page->trainMl(file, target);
}

void MainWindow::runLocalPrompt(const QString &prompt)
{
    showPage("ai");
    if (auto *page = qobject_cast<AiPage *>(m_pages->currentWidget()))
        page->runLocalPrompt(prompt);
}

void MainWindow::downloadModelFile(const QString &modelId, const QString &file)
{
    showPage("ai");
    if (auto *page = qobject_cast<AiPage *>(m_pages->currentWidget()))
        page->downloadFile(modelId, file);
}

void MainWindow::scrollDetailToEnd(double fraction)
{
    if (m_detail)
        m_detail->scrollTo(fraction);
    // And the shown page's own scroll areas (the Sources page's list).
    if (QWidget *page = m_pages ? m_pages->currentWidget() : nullptr)
        for (QScrollArea *area : page->findChildren<QScrollArea *>())
            if (area->isVisible() && area->verticalScrollBar())
                area->verticalScrollBar()->setValue(int(area->verticalScrollBar()->maximum() * fraction));
}

MlDataExporter::Sources MainWindow::mlSources() const
{
    MlDataExporter::Sources ml;
    ml.key = m_keyIndicators;
    ml.economy = m_economy;
    ml.rates = m_worldRates;
    ml.fx = m_fx;
    ml.cot = m_cotReport;
    ml.property = m_propertyCredit;
    ml.housePrices = m_housePrices;
    ml.globalDebt = m_globalDebt;
    ml.bankCredit = m_bankCredit;
    ml.sectors = m_sectors;
    return ml;
}

SourcesPage::Targets MainWindow::sourceTargets() const
{
    SourcesPage::Targets t;
    t.ais = m_ais;
    t.vessels = m_vessels;
    t.portWatch = m_portWatch;
    t.fx = m_fx;
    t.policyRates = m_policyRates;
    t.cot = m_cot;
    t.cotReport = m_cotReport;
    t.economy = m_economy;
    t.worldRates = m_worldRates;
    t.bop = m_bop;
    t.trade = m_trade;
    t.sectors = m_sectors;
    t.propertyCredit = m_propertyCredit;
    t.housePrices = m_housePrices;
    t.globalDebt = m_globalDebt;
    t.bankCredit = m_bankCredit;
    t.oecd = m_oecd;
    t.eurostat = m_eurostat;
    t.imfPrices = m_imfPrices;
    t.bls = m_bls;
    t.brazil = m_brazil;
    t.argentina = m_argentina;
    t.southAfrica = m_southAfrica;
    t.singapore = m_singapore;
    t.fred = m_fred;
    t.eia = m_eia;
    t.bea = m_bea;
    t.banxico = m_banxico;
    t.ecos = m_ecos;
    t.evds = m_evds;
    t.surveys = m_surveys;
    t.comtrade = m_comtrade;
    t.supply = m_supply;
    t.futures = m_futures;
    t.benchmarks = m_benchmarks;
    t.economicCalendar = m_economicCalendar;
    t.corporateCalendar = m_corporateCalendar;
    t.marketSessions = m_marketSessions;
    t.backend = m_quantBackend;
    t.aisKeyFromEnv = m_aisKeyFromEnv;
    return t;
}

void MainWindow::rebuildUi()
{
    const GlobeWidget::ViewState view = m_globe->viewState();
    // The page shown comes back after the rebuild (a theme or language change), whichever it was.
    const QString shown = m_pageById.key(m_pages->currentWidget());
    if (m_quantPage)
        m_quantPage->setParent(nullptr);   // survives the rebuild
    QWidget *old = takeCentralWidget();
    old->hide();
    old->deleteLater();
    buildUi();
    m_globe->setViewState(view);
    if (!shown.isEmpty())
        showPage(shown);
}

void MainWindow::setLanguage(const QString &code)
{
    if (code == m_language)
        return;
    QSettings().setValue("language", code);
    applyLanguage(code);
    rebuildUi();
    if (m_settings)
        m_settings->setCurrent(m_language, Theme::current().id);
    if (m_help)
        m_help->setLanguage(m_language);
}

void MainWindow::setTheme(const QString &id)
{
    if (id == Theme::current().id)
        return;
    QSettings().setValue("theme", id);
    applyTheme(id);
    rebuildUi();
    if (m_settings)
        m_settings->setCurrent(m_language, Theme::current().id);
    if (m_help)
        m_help->retheme();
}

void MainWindow::setAisKey(const QString &key)
{
    if (m_aisKeyFromEnv || key == m_ais->apiKey())
        return;
    QSettings().setValue("aisApiKey", key);
    m_vessels->clear();
    m_ais->setApiKey(key);
}

void MainWindow::applyLanguage(const QString &code)
{
    for (QTranslator *t : std::as_const(m_translators)) {
        qApp->removeTranslator(t);
        delete t;
    }
    m_translators.clear();

    m_language = code == "fr" ? "fr" : "en";
    if (m_language == "fr") {
        // UI strings, and market data strings (categories, commodities, countries...).
        for (const QString &file : {":/i18n/deepwater_fr.qm", ":/i18n/deepwater_data_fr.qm"}) {
            auto *t = new QTranslator(this);
            if (t->load(file)) {
                qApp->installTranslator(t);
                m_translators.push_back(t);
            } else {
                delete t;
            }
        }
        QLocale::setDefault(QLocale(QLocale::French, QLocale::France));
    } else {
        QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
    }
    m_data->retranslate();
}

void MainWindow::applyTheme(const QString &id)
{
    Theme::setCurrent(id);
    m_data->setCategoryColors(Theme::current().categoryColors);
    QFile qss(":/style.qss");
    if (qss.open(QIODevice::ReadOnly))
        qApp->setStyleSheet(Theme::styleSheet(QString::fromUtf8(qss.readAll())));
    // The Charts page takes the same palette (its own light / dark toggle is then hidden). It
    // survives rebuildUi(), so it re-applies the colours itself and keeps its charts.
    const Palette &p = Theme::current();
    qtm::theme::setPalette(Theme::chartVars(p), Theme::isLight(p));
    if (m_quantPage)
        m_quantPage->refreshTheme();
}

void MainWindow::openSettings()
{
    if (!m_settings) {
        m_settings = new SettingsDialog(this);
        connect(m_settings, &SettingsDialog::languageChosen, this, &MainWindow::setLanguage);
        connect(m_settings, &SettingsDialog::themeChosen, this, &MainWindow::setTheme);
        // Float it near the gear icon, top-right of the window.
        m_settings->move(mapToGlobal(QPoint(width() - m_settings->width() - 24, 64)));
    }
    m_settings->setCurrent(m_language, Theme::current().id);
    m_settings->show();
    m_settings->raise();
    m_settings->activateWindow();
}

QWidget *MainWindow::settingsWindow() const
{
    return m_settings;
}

void MainWindow::openHelp(const QString &chapter)
{
    if (!m_help) {
        m_help = new HelpWindow(m_language, this);
        // Float it under the ? icon, top-right of the window, inside the screen on a narrow window.
        m_help->move(mapToGlobal(QPoint(qMax(0, width() - m_help->width() - 24), 64)));
    }
    m_help->showChapter(chapter.isEmpty() ? m_pageById.key(m_pages->currentWidget()) : chapter);
}

QWidget *MainWindow::helpWindow() const
{
    return m_help;
}

void MainWindow::setDevToolsEnabled(bool on)
{
    QSettings().setValue("devtools/enabled", on);
    m_header->setDevToolsChecked(on);
    m_devTools->setVisible(on);
    if (!on)
        m_devTools->setOpen(false, false);
    if (m_aiDock)
        m_aiDock->setRightInset(on ? SideFlap::Width : 0);
}

void MainWindow::setDevToolsOpen(bool open)
{
    if (open)
        setDevToolsEnabled(true);
    m_devTools->setOpen(open, false);
}

void MainWindow::addPage(const QString &id, const QString &label, QWidget *page)
{
    m_pageLabels.insert(id, label);
    m_pages->addWidget(page);
    m_pageById.insert(id, page);
    m_header->addNavItem(id, label);
}

void MainWindow::addPage(const QString &id, const QString &label, std::function<QWidget *()> factory)
{
    m_pageLabels.insert(id, label);
    m_pageFactories.insert(id, std::move(factory));
    m_header->addNavItem(id, label);
}

QWidget *MainWindow::currentPage() const
{
    return m_pages->currentWidget();
}

QWidget *MainWindow::ensurePage(const QString &id)
{
    if (const auto factory = m_pageFactories.take(id)) {
        QWidget *page = factory();
        m_pages->addWidget(page);
        m_pageById.insert(id, page);
    }
    return m_pageById.value(id);
}

void MainWindow::showPage(const QString &id)
{
    if (QWidget *page = ensurePage(id)) {
        m_pages->setCurrentWidget(page);
        m_header->setCurrent(id);
    }
    if (m_aiDock)
        m_aiDock->refreshContext();
}

namespace {

constexpr char AiPages[] = "map, overview, economy, forex, charts, calendar, news, research or quant";
constexpr int AiReadTick = 1500;
constexpr int AiReadTicks = 8;

// Still waiting for its data: cells that are placeholders.
bool looksLoading(const QString &text)
{
    return text.contains(QString::fromUtf8("…\n")) || text.endsWith(QString::fromUtf8("…")) || text.count(QString::fromUtf8("—")) > 3
           || text.isEmpty();
}

int countryTabIndex(const QString &name)
{
    const QVector<countries::TabDef> &tabs = countries::catalogue().tabs;
    for (int i = 0; i < tabs.size(); ++i)
        if (name.compare(QString::fromUtf8(tabs[i].name.toUtf8()), Qt::CaseInsensitive) == 0
            || name.compare(countries::text(tabs[i].name), Qt::CaseInsensitive) == 0)
            return i;
    return -1;
}

} // namespace

// Reads again until the data has arrived or a few seconds have gone; `read` must leave everything as it found it.
void MainWindow::aiPoll(std::function<AiPageRead()> read, std::function<void()> finished, std::function<void(const AiPageRead &)> done,
                        int tick, qsizetype previous)
{
    AiPageRead result = read();
    if (!result.error.isEmpty()) {
        finished();
        done(result);
        return;
    }
    const qsizetype size = result.text.size();
    const bool stable = tick > 0 && std::abs(size - previous) <= size / 50;
    if ((stable && !looksLoading(result.text)) || tick >= AiReadTicks) {
        result.stillLoading = looksLoading(result.text);
        finished();
        done(result);
        return;
    }
    QTimer::singleShot(AiReadTick, this, [this, read, finished, done, tick, size] { aiPoll(read, finished, done, tick + 1, size); });
}

void MainWindow::aiReadPage(const AiPageRequest &request, std::function<void(const AiPageRead &)> done)
{
    // A country's or an indicator's sheet: one of its own, shown nowhere, so that the one the user has open stays.
    if (!request.country.isEmpty() || !request.indicator.isEmpty()) {
        const QVariant savedTab = QSettings().value("economy/countryTab");
        QWidget *sheet = nullptr;
        QString name;
        if (!request.country.isEmpty()) {
            const int tab = request.tab.isEmpty() ? 0 : countryTabIndex(request.tab);
            if (tab < 0) {
                AiPageRead read;
                read.error = "unknown tab \"" + request.tab + "\"";
                for (const countries::TabDef &t : countries::catalogue().tabs) read.sections << t.name;
                done(read);
                return;
            }
            auto *page = new CountryPage(request.country);
            page->showTab(tab);
            name = page->aiContext();
            sheet = page;
        } else {
            const countries::ListItem *item = countries::findListItem(request.indicator);
            if (!item || item->ids.isEmpty()) {
                AiPageRead read;
                read.error = "unknown indicator \"" + request.indicator + "\": see list_data, or the English name shown in Economy › Indicators";
                done(read);
                return;
            }
            auto *page = new IndicatorPage(*item);
            name = page->name();
            sheet = page;
        }
        sheet->setAttribute(Qt::WA_DontShowOnScreen);
        sheet->resize(m_pages->size());
        sheet->show();
        const QPointer<QWidget> guard(sheet);
        aiPoll([guard, name] {
                   AiPageRead read;
                   read.page = name;
                   read.onScreen = false;
                   if (guard) read.text = ::aiReadPage(guard);
                   return read;
               },
               [guard, savedTab] {
                   delete guard.data();
                   if (savedTab.isValid()) QSettings().setValue("economy/countryTab", savedTab);
                   else QSettings().remove("economy/countryTab");
               },
               std::move(done), 0, 0);
        return;
    }

    const QString id = request.page.isEmpty() ? m_pageById.key(m_pages->currentWidget()) : request.page;
    AiPageRead refused;
    refused.page = m_pageLabels.value(id);
    if (id == "sources" || id == "ai") {
        refused.error = "not allowed: the " + id + " page can't be read";
        done(refused);
        return;
    }
    if (!m_pageLabels.contains(id) || !ensurePage(id)) {
        refused.error = "unknown page \"" + request.page + "\": " + AiPages;
        done(refused);
        return;
    }
    const QString section = request.section;
    auto once = [this, id, section] {
        AiPageRead read;
        QWidget *page = ensurePage(id);
        read.onScreen = page == m_pages->currentWidget();
        read.page = read.onScreen ? pageContext() : m_pageLabels.value(id);
        // A page not on screen is really shown, past the edge of its parent, for the time of the read: its views load and
        // draw as usual, and nothing reaches the screen. The map and the charts are always loaded, and the globe is heavy.
        const bool place = !read.onScreen && id != "map" && id != "charts";
        if (place) {
            page->setGeometry(QRect(QPoint(m_pages->width() + 100, 0), m_pages->size()));
            page->show();
        } else if (!read.onScreen) {
            page->resize(m_pages->size());
        }
        if (page->layout()) page->layout()->activate();
        const QVector<QAbstractButton *> buttons = aiSectionButtons(page);
        QAbstractButton *open = nullptr;
        for (QAbstractButton *b : buttons) {
            read.sections << aiSectionName(b);
            if (b->isChecked() && !open) open = b;
        }
        QAbstractButton *wanted = section.isEmpty() ? open : aiFindSection(buttons, section);
        if (!section.isEmpty() && !wanted) {
            read.error = "no section \"" + section + "\" on " + id;
        } else {
            if (wanted && wanted != open) wanted->click();
            if (wanted) read.section = aiSectionName(wanted);
            if (page->layout()) page->layout()->activate();
            read.text = ::aiReadPage(page);
            if (wanted && open && wanted != open) open->click();   // in the same tick: nothing was painted
        }
        if (place) {
            page->hide();
            page->setGeometry(m_pages->rect());
        }
        return read;
    };
    // What is on screen is already there.
    if (id == m_pageById.key(m_pages->currentWidget()) && section.isEmpty()) {
        done(once());
        return;
    }
    aiPoll(once, [] {}, std::move(done), 0, 0);
}

QString MainWindow::aiOpenPage(const AiPageRequest &request, QString *error)
{
    if (!request.country.isEmpty()) {
        const int tab = request.tab.isEmpty() ? -1 : countryTabIndex(request.tab);
        if (!request.tab.isEmpty() && tab < 0) {
            *error = "unknown tab \"" + request.tab + "\"";
            return {};
        }
        openEconomyCountry(request.country, tab);
        return pageContext();
    }
    if (!request.indicator.isEmpty()) {
        if (!countries::findListItem(request.indicator)) {
            *error = "unknown indicator \"" + request.indicator + "\"";
            return {};
        }
        openEconomyIndicator(request.indicator);
        return pageContext();
    }
    const QString id = request.page.isEmpty() ? m_pageById.key(m_pages->currentWidget()) : request.page;
    if (!m_pageLabels.contains(id) || !ensurePage(id)) {
        *error = "unknown page \"" + request.page + "\": " + QString(AiPages) + ", sources or ai";
        return {};
    }
    // Checked first: a section that doesn't exist doesn't move the user.
    QAbstractButton *wanted = nullptr;
    if (!request.section.isEmpty()) {
        const QVector<QAbstractButton *> buttons = aiSectionButtons(ensurePage(id));
        wanted = aiFindSection(buttons, request.section);
        if (!wanted) {
            QStringList names;
            for (const QAbstractButton *b : buttons) names << aiSectionName(b);
            *error = "no section \"" + request.section + "\" on " + id + ": " + names.join(", ");
            return {};
        }
    }
    showPage(id);
    if (wanted) wanted->click();
    return pageContext();
}

QString MainWindow::pageContext() const
{
    QWidget *page = m_pages->currentWidget();
    const QString id = m_pageById.key(page);
    QString detail;
    if (id == "map" && !m_selectedCountry.isEmpty())
        detail = QStringLiteral("%1 (%2)").arg(countries::countryName(m_selectedCountry), m_selectedCountry);
    else if (page && page->metaObject()->indexOfMethod("aiContext()") >= 0)
        QMetaObject::invokeMethod(page, "aiContext", Qt::DirectConnection, Q_RETURN_ARG(QString, detail));
    const QString label = m_pageLabels.value(id);
    return detail.isEmpty() ? label : label + QString::fromUtf8(" › ") + detail;
}

void MainWindow::askAiDock(const QString &prompt)
{
    if (!m_aiDock) return;
    if (prompt.isEmpty()) m_aiDock->setOpen(true, false);
    else m_aiDock->ask(prompt);
}

void MainWindow::selectVessel(qint64 mmsi, bool fly)
{
    const Vessel *v = m_vessels->vessel(mmsi);
    if (!v)
        return;
    m_selectedVessel = mmsi;
    m_globe->setSelection({});
    m_globe->setSelectedChokepoint(-1);
    m_globe->setSelectedPoint({});
    m_globe->setSelectedVessel(mmsi);
    if (fly)
        m_globe->flyTo(v->lat, v->lon, 3.0);
    m_left->setActiveCommodity({});
    setSelectedCountry({});
    m_detail->showVessel(mmsi);
    showDetail();
    updateVesselRoute(true);
}

void MainWindow::selectChokepoint(int zone, bool fly)
{
    m_selectedVessel = 0;
    m_globe->setRoute({}, {});
    m_globe->setSelection({});
    m_globe->setSelectedVessel(0);
    m_globe->setSelectedChokepoint(zone);
    if (fly) {
        const Chokepoint &z = m_chokepoints->chokepoints()[zone];
        m_globe->flyTo(z.lat, z.lon, 3.5);
    }
    m_left->setActiveCommodity({});
    setSelectedCountry({});
    m_detail->showChokepoint(zone);
    showDetail();
}

void MainWindow::updateVesselRoute(bool force)
{
    const Vessel *v = m_selectedVessel ? m_vessels->vessel(m_selectedVessel) : nullptr;
    if (!v) {
        if (m_selectedVessel)
            m_globe->setRoute({}, {});
        return;
    }
    // Routing is cheap but not free: only redo it when the ship moved ~2 km or
    // its destination changed.
    const bool moved = QVector3D::dotProduct(m_routeFrom, v->pos) < std::cos(2.0 / 6371.0);
    if (!force && !moved && v->destination == m_routeDestination)
        return;
    m_routeFrom = v->pos;
    m_routeDestination = v->destination;

    const Port *port = v->destination.isEmpty() ? nullptr : m_ports.resolve(v->destination);
    if (!port) {
        m_globe->setRoute({}, {});
        m_detail->setVesselRoute({}, 0);
        return;
    }
    const SeaRoutes::Route route = m_seaRoutes.route(v->lat, v->lon, port->lat, port->lon);
    const QString label = QString("%1 (%2)").arg(port->name, port->locode);
    m_globe->setRoute(route.points, label);
    m_detail->setVesselRoute(label, route.lengthKm);
}

void MainWindow::selectSite(int index, bool fly)
{
    m_selectedVessel = 0;
    m_globe->setRoute({}, {});
    m_globe->setSelectedChokepoint(-1);
    m_globe->setSelectedPoint({});
    const Site &s = m_data->sites()[index];
    m_globe->setSelectedVessel(0);
    m_globe->setSelection({index});
    if (fly)
        m_globe->flyTo(s.lat, s.lon);
    m_left->setActiveCommodity({});
    setSelectedCountry({});
    m_detail->showSites({index});
    showDetail();
}

void MainWindow::selectCommodity(const QString &id)
{
    m_selectedVessel = 0;
    m_globe->setRoute({}, {});
    m_globe->setSelectedChokepoint(-1);
    m_globe->setSelectedPoint({});
    QVector<int> sites;
    double x = 0, y = 0, z = 0;
    for (const Site *s : m_data->sitesForCommodity(id)) {
        sites.push_back(s->index);
        const QVector3D v = Geo::toVector(s->lat, s->lon);
        x += v.x(); y += v.y(); z += v.z();
    }
    m_globe->setSelectedVessel(0);
    m_globe->setSelection(sites);
    m_left->setActiveCommodity(id);
    setSelectedCountry({});
    m_detail->showCommodity(id);
    showDetail();

    // Turn the globe toward the centroid of the commodity's sites.
    const double len = std::sqrt(x * x + y * y + z * z);
    if (len > 1e-6) {
        const double lat = qRadiansToDegrees(std::asin(z / len));
        const double lon = qRadiansToDegrees(std::atan2(y, x));
        m_globe->flyTo(lat, lon, 1.0);
    }
}

void MainWindow::showDetail()
{
    m_leftStack->setCurrentWidget(m_detail);
}

void MainWindow::clearSelection(bool keepIndicator)
{
    const bool indicatorShown = m_leftStack->currentWidget() == m_detail && m_detail->showingIndicator();
    m_selectedVessel = 0;
    m_globe->setRoute({}, {});
    m_globe->setSelection({});
    m_globe->setSelectedVessel(0);
    m_globe->setSelectedChokepoint(-1);
    m_globe->setSelectedPoint({});
    setSelectedCountry({});
    m_left->setActiveCommodity({});

    if (m_activeIndicator >= 0 && indicatorShown && !keepIndicator) {
        m_activeIndicator = -1;
        m_indicators->setActive(-1);
        m_detail->setActiveIndicator(-1);
        updateHeatmap();
    }
    if (m_activeIndicator >= 0) {
        if (!indicatorShown)
            m_detail->showIndicator(m_activeIndicator, m_indicatorYear);
        showDetail();
    } else {
        m_leftStack->setCurrentWidget(m_left);
    }
}

void MainWindow::selectIndicator(int id)
{
    if (id == m_activeIndicator) {
        // Clicking the active indicator again turns the heatmap off.
        m_activeIndicator = -1;
        m_indicators->setActive(-1);
        m_detail->setActiveIndicator(-1);
        updateHeatmap();
        clearSelection(true);
        return;
    }
    m_activeIndicator = id;
    m_indicatorYear = m_macro->defaultYear(MacroIndicators::Id(id));
    m_indicators->setActive(id);
    m_detail->setActiveIndicator(id);
    clearSelection(true);
    // clearSelection keeps an indicator view that is already shown, which would be the previous
    // indicator's: show the new one.
    m_detail->showIndicator(id, m_indicatorYear);
    showDetail();
    updateHeatmap();
}

void MainWindow::selectCountry(const QString &iso3, bool fly)
{
    m_selectedVessel = 0;
    m_globe->setRoute({}, {});
    m_globe->setSelection({});
    m_globe->setSelectedVessel(0);
    m_globe->setSelectedChokepoint(-1);
    m_globe->setSelectedPoint({});
    setSelectedCountry(iso3);
    if (fly)
        m_globe->flyToCountry(iso3);
    m_left->setActiveCommodity({});
    m_detail->showCountry(iso3);
    showDetail();
}

void MainWindow::setSelectedCountry(const QString &iso3)
{
    m_globe->setSelectedCountry(iso3);
    if (iso3 == m_selectedCountry)
        return;
    m_selectedCountry = iso3;
    updateFlows();
}

void MainWindow::updateFlows()
{
    if (!m_globe)
        return;
    const Palette &pal = Theme::current();
    GlobeWidget::FlowMap map;
    if (!m_trade->hasData()) {
        map.title = tr("Trade in goods");
        map.keys.push_back({pal.TextFaint, m_trade->errorString().isEmpty()
                                               ? tr("Loading IMF data…")
                                               : tr("IMF data unavailable: %1").arg(m_trade->errorString())});
        m_globe->setFlowMap(map);
        return;
    }
    const int year = m_trade->year();
    // Totals per country, for the shares in the tooltips.
    QHash<QString, double> exportsOf, importsOf;
    for (const TradeFlows::Flow &f : m_trade->flows()) {
        exportsOf[f.from] += f.value;
        importsOf[f.to] += f.value;
    }
    const QString dim = pal.TextDim.name();
    auto addArc = [&](const TradeFlows::Flow &f, const QColor &color) {
        const QString from = m_economy->displayName(f.from).toHtmlEscaped();
        const QString to = m_economy->displayName(f.to).toHtmlEscaped();
        const QString label =
            QString("<b>%1 → %2</b><br>%3 <span style='color:%4'>%5</span><br>"
                    "<span style='color:%4'>%6<br>%7</span>")
                .arg(from, to, m_macro->format(MacroIndicators::GdpSize, f.value), dim,
                     tr("of goods exports in %1").arg(year),
                     tr("%1: %2% of its exports").arg(from).arg(QLocale().toString(100 * f.value / exportsOf.value(f.from), 'f', 1)),
                     tr("%1: %2% of its imports").arg(to).arg(QLocale().toString(100 * f.value / importsOf.value(f.to), 'f', 1)));
        map.arcs.push_back({f.from, f.to, f.value, color, label});
    };
    // Only countries with a name (the IMF also lists territories the rest of the app doesn't know).
    auto known = [this](const TradeFlows::Flow &f) { return m_economy->country(f.from) && m_economy->country(f.to); };

    if (m_selectedCountry.isEmpty()) {
        constexpr int Count = 50;
        for (const TradeFlows::Flow &f : m_trade->flows()) {
            if (map.arcs.size() == Count)
                break;
            if (known(f))
                addArc(f, pal.Accent);
        }
        map.title = tr("Trade in goods · %1").arg(year);
        map.keys.push_back({pal.Accent, tr("The %1 largest flows, exporter → importer").arg(map.arcs.size())});
    } else {
        // The selected country's top partners each way; flows are sorted largest first.
        constexpr int Partners = 10;
        int exports = 0, imports = 0;
        for (const TradeFlows::Flow &f : m_trade->flows()) {
            if (!known(f))
                continue;
            if (f.from == m_selectedCountry && exports < Partners) {
                addArc(f, pal.Up);
                ++exports;
            } else if (f.to == m_selectedCountry && imports < Partners) {
                addArc(f, pal.Down);
                ++imports;
            }
        }
        std::sort(map.arcs.begin(), map.arcs.end(), [](const auto &a, const auto &b) { return a.value > b.value; });
        map.title = tr("%1 · trade in goods %2").arg(m_economy->displayName(m_selectedCountry)).arg(year);
        map.keys.push_back({pal.Up, tr("Exports to its %1 main customers").arg(exports)});
        map.keys.push_back({pal.Down, tr("Imports from its %1 main suppliers").arg(imports)});
    }
    m_globe->setFlowMap(map);
}

void MainWindow::updateHeatmap()
{
    if (!m_globe)
        return;
    GlobeWidget::Heatmap heatmap;
    if (m_activeIndicator >= 0) {
        const auto id = MacroIndicators::Id(m_activeIndicator);
        const MacroIndicators::Info info = MacroIndicators::info(id);
        const QHash<QString, double> values = m_macro->values(id, m_indicatorYear);
        const MacroIndicators::Range range = m_macro->range(id, values);
        const QString dim = Theme::current().TextDim.name();
        for (auto it = values.begin(); it != values.end(); ++it) {
            heatmap.colors.insert(it.key(), m_macro->color(id, it.value(), range));
            heatmap.heights.insert(it.key(), m_macro->level(id, it.value(), range));
            const QString detail = m_macro->detail(id, it.key(), m_indicatorYear);
            heatmap.labels.insert(it.key(), QString("<b>%1</b><br>%2 <span style='color:%3'>%4</span>%5")
                                                .arg(m_economy->displayName(it.key()).toHtmlEscaped(),
                                                     m_macro->format(id, it.value()), dim, info.name,
                                                     detail.isEmpty() ? QString()
                                                                      : QString("<br><span style='color:%1'>%2</span>")
                                                                            .arg(dim, detail.toHtmlEscaped())));
        }
        heatmap.title = info.yearly ? QString("%1 · %2").arg(info.name).arg(m_indicatorYear) : info.name;
        heatmap.ramp = m_macro->ramp(id);
        const bool log = info.scale == MacroIndicators::Scale::Log;
        heatmap.lowLabel = m_macro->format(id, log ? std::pow(10.0, range.lo) : range.lo);
        heatmap.highLabel = m_macro->format(id, log ? std::pow(10.0, range.hi) : range.hi);
    }
    m_globe->setHeatmap(heatmap);
}

void MainWindow::updateSupplyPoints()
{
    if (!m_globe)
        return;
    const QLocale loc;
    const QString dim = Theme::current().TextDim.name();
    QVector<GlobeWidget::MapPoint> points;

    // Ports, the busiest drawn last (on top) and shown from afar; the others as the view closes in.
    QVector<const supply::PortActivity::Port *> ports;
    int maxVessels = 1;
    for (const auto &p : m_supply->ports()->ports()) {
        ports.push_back(&p);
        maxVessels = std::max(maxVessels, p.vessels);
    }
    std::sort(ports.begin(), ports.end(), [](auto *a, auto *b) { return a->vessels > b->vessels; });
    for (int rank = 0; rank < ports.size(); ++rank) {
        const auto &p = *ports[rank];
        GlobeWidget::MapPoint pt;
        pt.key = "port:" + p.id;
        pt.lat = p.lat;
        pt.lon = p.lon;
        pt.shape = GlobeWidget::MapPoint::Dot;
        pt.color = supply::SupplyChain::portColor(p);
        pt.size = 2.2 + 3.6 * std::sqrt(double(p.vessels) / maxVessels);
        pt.minZoom = rank < 120 ? 0 : rank < 500 ? 1.8 : rank < 1000 ? 3.0 : 5.0;
        const double c = supply::SupplyChain::changePct(p.calls, p.callsYearAgo);
        pt.tip = QString("<b>%1</b> &nbsp;<span style='color:%2'>%3</span><br>%4")
                     .arg(p.name.toHtmlEscaped(), dim,
                          (m_economy->country(p.iso3) ? m_economy->displayName(p.iso3) : p.country).toHtmlEscaped(),
                          p.calls < 0 ? tr("No port calls reported over the last 7 days")
                          : std::isfinite(c)
                              ? tr("%1 port calls over 7 days, %2 on a year")
                                    .arg(p.calls)
                                    .arg(QCoreApplication::translate("MacroIndicators", "%1%")
                                             .arg((c > 0 ? "+" : c < 0 ? QString::fromUtf8("−") : QString())
                                                  + loc.toString(std::abs(c), 'f', 0)))
                              : tr("%1 port calls over 7 days").arg(p.calls));
        points.push_back(pt);
    }
    std::reverse(points.begin(), points.end());

    for (const supply::OilSite &s : m_supply->oilSites()) {
        GlobeWidget::MapPoint pt;
        pt.key = "oil:" + s.id;
        pt.lat = s.lat;
        pt.lon = s.lon;
        pt.shape = GlobeWidget::MapPoint::Diamond;
        pt.color = m_data->categoryColor("energy");
        pt.size = 4;
        pt.tip = QString("<b>%1</b><br><span style='color:%2'>%3</span>")
                     .arg(QCoreApplication::translate("OilSites", s.name.toUtf8().constData()).toHtmlEscaped(), dim,
                          supply::SupplyChain::siteKindName(s.kind));
        points.push_back(pt);
    }

    for (const auto &d : m_supply->ports()->recentDisruptions(30)) {
        if (d.lat == 0 && d.lon == 0)
            continue;
        GlobeWidget::MapPoint pt;
        pt.key = "event:" + QString::number(d.id);
        pt.lat = d.lat;
        pt.lon = d.lon;
        pt.shape = GlobeWidget::MapPoint::Ring;
        pt.color = supply::SupplyChain::alertColor(d.alert);
        pt.size = 9;
        pt.tip = QString("<b>%1</b><br><span style='color:%2'>%3 · %4</span>")
                     .arg(d.name.toHtmlEscaped(), dim, supply::SupplyChain::disruptionTypeName(d.type),
                          tr("%n port(s) affected", nullptr, int(d.ports.size())));
        points.push_back(pt);
    }
    m_globe->setPoints(points);
}

void MainWindow::selectSupplyPoint(const QString &key, bool fly)
{
    m_selectedVessel = 0;
    m_globe->setRoute({}, {});
    m_globe->setSelection({});
    m_globe->setSelectedVessel(0);
    m_globe->setSelectedChokepoint(-1);
    setSelectedCountry({});
    m_left->setActiveCommodity({});
    if (!m_globe->pointsMode())
        m_globe->setPointsMode(true);
    m_globe->setSelectedPoint(key);
    if (fly) {
        const int colon = key.indexOf(':');
        const QString kind = key.left(colon), id = key.mid(colon + 1);
        if (kind == "port") {
            if (const auto *p = m_supply->ports()->port(id))
                m_globe->flyTo(p->lat, p->lon, 3.5);
        } else if (kind == "oil") {
            if (const auto *s = m_supply->oilSite(id))
                m_globe->flyTo(s->lat, s->lon, 3.5);
        } else if (kind == "event") {
            for (const auto &d : m_supply->ports()->disruptions())
                if (d.id == id.toInt() && (d.lat != 0 || d.lon != 0))
                    m_globe->flyTo(d.lat, d.lon, 2.5);
        }
    }
    m_detail->showSupplyPoint(key);
    showDetail();
}

void MainWindow::showSupply(const QString &key)
{
    if (!key.isEmpty() && key.contains(':')) {
        selectSupplyPoint(key, true);
        return;
    }
    if (!m_globe->pointsMode()) {
        m_globe->setPointsMode(true);   // shows the overview
        return;
    }
    clearSelection(true);
    m_detail->showSupplyOverview();
    showDetail();
}
