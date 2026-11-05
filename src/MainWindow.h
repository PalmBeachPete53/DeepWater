#pragma once

#include "AiRead.h"

#include <QHash>
#include <QMainWindow>
#include <QPointer>
#include <QVector3D>

#include <functional>

#include "MlData.h"
#include "SeaRoutes.h"
#include "SourcesPage.h"

namespace qtm { class App; class Backend; }

class AisStream;
class ChokepointMonitor;
class CotData;
class CotReport;
class DetailPanel;
class DevToolsDock;
class AiDock;
class IndicatorPanel;
class MacroIndicators;
class WorldEconomy;
class WorldPolicyRates;
class BalanceOfPayments;
class TradeFlows;
class EconomySectors;
class BankCredit;
class BlsIndicators;
class CommodityBenchmarks;
class EurostatIndicators;
class ImfConsumerPrices;
class KeyIndicators;
class OecdIndicators;
class BrazilIndicators;
class ArgentinaIndicators;
class SouthAfricaIndicators;
class SingaporeIndicators;
class FredIndicators;
class EiaIndicators;
class BeaIndicators;
class BanxicoIndicators;
class EcosIndicators;
class EvdsIndicators;
class SurveyHeadlines;
class ComtradeProducts;
namespace supply { class SupplyChain; }
class CorporateCalendar;
class EconomicCalendar;
class MarketSessions;
class FuturesQuotes;
class GlobalDebt;
class HousePriceRatios;
class PropertyCredit;
class FxData;
class PolicyRates;
class PortWatch;
class MarketData;
class VesselStore;
class GlobeWidget;
class HeaderBar;
class LeftPanel;
class QStackedWidget;
class QTranslator;
class SettingsDialog;
class HelpWindow;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    // `theme`: a theme id for this run only, not saved (the --theme dev flag); empty = the saved one.
    explicit MainWindow(MarketData *data, const QString &theme = QString(), QWidget *parent = nullptr);
    ~MainWindow() override;

    // Registers a top-level page: adds a tab in the header and a page in the stack.
    void addPage(const QString &id, const QString &label, QWidget *page);
    // Same, but the page is only built the first time it is shown (and again after a UI rebuild).
    void addPage(const QString &id, const QString &label, std::function<QWidget *()> factory);
    void showPage(const QString &id);
    void openSettings();
    QWidget *settingsWindow() const;   // the settings window once opened (for --settings screenshots)
    // The documentation window (header ? icon, F1), on a chapter: a page id, or empty for the page shown.
    void openHelp(const QString &chapter = QString());
    QWidget *helpWindow() const;       // once opened (for --help-page screenshots)
    // Shows the developer flap (header </> icon) and, when open, the side panel.
    void setDevToolsEnabled(bool on);
    void setDevToolsOpen(bool open);
    // Dev flags (--country ISO3, --indicator n): open a country's economy sheet, turn an indicator heatmap on.
    void showCountry(const QString &iso3) { selectCountry(iso3, false); }
    void showIndicator(int id) { selectIndicator(id); }
    // --supply [key]: the supply chain layer's overview, or one of its points ("port:port1114", "oil:cushing").
    void showSupply(const QString &key);
    void scrollDetailToEnd(double fraction = 1.0);
    void searchSources(const QString &text);
    void showSourcesTab(int index);
    void showSourcesDisc(const QString &pin);
    void searchModels(const QString &text);
    void filterNews(const QString &ticker);   // --news-ticker: open the News page on one ticker
    void openEconomyCountry(const QString &iso3, int tab);   // --economy-country [--economy-tab n]
    void openEconomyIndicator(const QString &name);          // --economy-indicator "Inflation Rate"
    void openNewsArticle(int index);          // --news-article: open the reader on the index-th article
    void favoriteNewsArticle(int index);      // --news-favorite: star the index-th article
    void openNewsUrl(const QString &url);     // --news-url: open the reader on a URL
    void downloadModelFile(const QString &modelId, const QString &file);
    void exportMlData();
    void trainMl(const QString &file, const QString &target);
    void runLocalPrompt(const QString &prompt);
    void askAiDock(const QString &prompt);   // --ai-dock [prompt]: the AI side panel open (and asked)

    // Apply, persist and rebuild the UI so every widget picks up the change.
    void setLanguage(const QString &code);
    void setTheme(const QString &id);
    void setAisKey(const QString &key);

private:
    void buildUi();
    void rebuildUi();
    void applyLanguage(const QString &code);
    void applyTheme(const QString &id);
    SourcesPage::Targets sourceTargets() const;
    // The data the AI tools read (machine learning exports, local models, opencode over MCP).
    MlDataExporter::Sources mlSources() const;

    void selectSite(int index, bool fly);
    void selectVessel(qint64 mmsi, bool fly);
    void selectChokepoint(int zone, bool fly);
    void updateVesselRoute(bool force);
    void selectCommodity(const QString &id);
    void selectCountry(const QString &iso3, bool fly);
    void selectIndicator(int id);
    void updateHeatmap();
    void updateFlows();   // the globe's flow map: largest flows, or the selected country's
    // The globe's supply chain layer (ports, oil storage sites, disruptions) and its sheets.
    void updateSupplyPoints();
    void selectSupplyPoint(const QString &key, bool fly);
    void setSelectedCountry(const QString &iso3);
    void showDetail();
    // Clears the selected site, vessel, chokepoint or country. Unless
    // keepIndicator, a second call also turns off the active indicator.
    void clearSelection(bool keepIndicator = false);

    MarketData *m_data;
    HeaderBar *m_header = nullptr;
    DevToolsDock *m_devTools = nullptr;
    AiDock *m_aiDock = nullptr;
    QHash<QString, QString> m_pageLabels;   // the navigation's labels, by page id
    // A page, built if it wasn't yet (without showing it).
    QWidget *ensurePage(const QString &id);
    // Where the user is, for the AI side panel: the page and what it shows ("Charts › BINANCE:BTCUSDT, 1h").
    QString pageContext() const;
    // The read_page and open_page tools (LocalTools).
    void aiReadPage(const AiPageRequest &request, std::function<void(const AiPageRead &)> done);
    QString aiOpenPage(const AiPageRequest &request, QString *error);
    void aiPoll(std::function<AiPageRead()> read, std::function<void()> finished, std::function<void(const AiPageRead &)> done, int tick, qsizetype previous);
    QStackedWidget *m_pages = nullptr;
    QHash<QString, QWidget *> m_pageById;
    QHash<QString, std::function<QWidget *()>> m_pageFactories;   // pages not built yet
    GlobeWidget *m_globe = nullptr;
    LeftPanel *m_left = nullptr;
    QStackedWidget *m_leftStack = nullptr;
    DetailPanel *m_detail = nullptr;
    IndicatorPanel *m_indicators = nullptr;

    VesselStore *m_vessels;
    AisStream *m_ais;
    ChokepointMonitor *m_chokepoints;
    PortWatch *m_portWatch;
    FxData *m_fx;
    PolicyRates *m_policyRates;
    CotData *m_cot;
    CotReport *m_cotReport;
    WorldEconomy *m_economy;
    WorldPolicyRates *m_worldRates;
    BalanceOfPayments *m_bop;
    TradeFlows *m_trade;
    EconomySectors *m_sectors;
    PropertyCredit *m_propertyCredit;
    HousePriceRatios *m_housePrices;
    GlobalDebt *m_globalDebt;
    BankCredit *m_bankCredit;
    OecdIndicators *m_oecd;                // the latest official figures (Eurostat, OECD, IMF, BLS)
    EurostatIndicators *m_eurostat;
    ImfConsumerPrices *m_imfPrices;
    BlsIndicators *m_bls;
    BrazilIndicators *m_brazil;            // national offices read directly
    ArgentinaIndicators *m_argentina;
    SouthAfricaIndicators *m_southAfrica;
    SingaporeIndicators *m_singapore;
    FredIndicators *m_fred;                // keyed sources
    EiaIndicators *m_eia;
    BeaIndicators *m_bea;
    BanxicoIndicators *m_banxico;
    EcosIndicators *m_ecos;
    EvdsIndicators *m_evds;
    SurveyHeadlines *m_surveys;            // private surveys' headlines
    ComtradeProducts *m_comtrade;          // trade by product, on demand
    supply::SupplyChain *m_supply;         // crop yields, road freight, oil stocks, ports, freight
    KeyIndicators *m_keyIndicators;
    FuturesQuotes *m_futures;              // commodity prices: futures, delayed
    CommodityBenchmarks *m_benchmarks;     // and the IMF's monthly averages
    EconomicCalendar *m_economicCalendar;  // the Calendar page
    CorporateCalendar *m_corporateCalendar;
    MarketSessions *m_marketSessions;
    MacroIndicators *m_macro;
    int m_activeIndicator = -1;
    int m_indicatorYear = 0;
    SeaRoutes m_seaRoutes;
    PortIndex m_ports;
    qint64 m_selectedVessel = 0;
    QString m_selectedCountry;
    QVector3D m_routeFrom;        // vessel position the current route was computed from
    QString m_routeDestination;
    bool m_aisKeyFromEnv = false;

    QString m_language;
    QList<QTranslator *> m_translators;
    QPointer<SettingsDialog> m_settings;
    QPointer<HelpWindow> m_help;
    // The Charts page (Quant Terminal port). Its backend runs for the whole session so alerts
    // keep firing; the page itself is kept across UI rebuilds so charts don't reload.
    qtm::Backend *m_quantBackend = nullptr;
    QPointer<qtm::App> m_quantPage;
};
