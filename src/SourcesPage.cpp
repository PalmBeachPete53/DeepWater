#include "SourcesPage.h"
#include "SourceFlow.h"

#include "KeyIndicators.h"
#include "KeyedSources.h"
#include "NationalSources.h"
#include "SurveyHeadlines.h"
#include "AiModels.h"
#include "ClaudeClient.h"
#include "OfficialSources.h"

#include "AisStream.h"
#include "CalendarData.h"
#include "QuantData.h"
#include "QuantMarketState.h"
#include "Myfxbook.h"
#include "ResearchData.h"
#include "CommodityPrices.h"
#include "DomesticFinance.h"
#include "FxData.h"
#include "MacroData.h"
#include "PortWatch.h"
#include "SupplyChain.h"
#include "Theme.h"
#include "VesselStore.h"
#include "quant/server/Server.h"
#include "quant/ui/Api.h"

#include <QComboBox>
#include <QDesktopServices>
#include <QElapsedTimer>
#include <QFrame>
#include <QJsonArray>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QNetworkReply>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QSet>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QStackedWidget>
#include <QToolButton>
#include <QButtonGroup>
#include <QVBoxLayout>

#include "AdBlock.h"
#include "NewsData.h"

namespace {

const char *DisabledKey = "sources/disabled";
constexpr int ActionsWidth = 236;   // Test, Refresh / Reconnect and the switch, in French too

// The Charts page's sources, handed to its backend.
const QStringList MarketIds {"binance", "coinbase", "kraken", "yahoo", "nasdaq", "calendar", "cryptonews", "alpaca", "polygon"};

// Keys kept in the settings ("keys/<id>") and read by a page itself (the AI tab, the News tab).
bool settingsKeyRow(const QString &id)
{
    return id == "anthropic" || id == "huggingface" || id == "fmp";
}

QStringList disabledIds()
{
    return QSettings().value(DisabledKey).toStringList();
}

QLabel *sectionLabel(const QString &text)
{
    auto *l = new QLabel(text.toUpper());
    l->setObjectName("sectionLabel");
    return l;
}

QLabel *dimLabel(const QString &text = {})
{
    auto *l = new QLabel(text);
    l->setObjectName("dim");
    return l;
}

template <typename T>
std::function<QDateTime()> fetchedAtOf(T *t) { return [t] { return t->fetchedAt(); }; }

} // namespace

// ---------------------------------------------------------------- SourceSwitch

SourceSwitch::SourceSwitch(QWidget *parent) : QAbstractButton(parent)
{
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFixedSize(sizeHint());
}

void SourceSwitch::paintEvent(QPaintEvent *)
{
    const Palette &ui = Theme::current();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF track = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    p.setPen(QPen(isChecked() ? ui.Accent : ui.ControlBorderHover, 1));
    p.setBrush(isChecked() ? ui.Accent : ui.ControlBorder);
    p.drawRoundedRect(track, track.height() / 2, track.height() / 2);
    const double d = height() - 6;
    const double x = isChecked() ? width() - 3 - d : 3;
    p.setPen(Qt::NoPen);
    p.setBrush(isChecked() ? ui.Background : ui.TextDim);
    p.drawEllipse(QRectF(x, 3, d, d));
}

// ---------------------------------------------------------------- SourcesPage

bool SourcesPage::isEnabled(const QString &id)
{
    return !disabledIds().contains(id);
}

void SourcesPage::applySaved(const Targets &t)
{
    const QStringList off = disabledIds();
    auto on = [&](const char *id) { return !off.contains(QLatin1String(id)); };
    t.ais->setEnabled(on("ais"));
    t.portWatch->setEnabled(on("portwatch"));
    if (t.supply) {
        t.supply->ports()->setEnabled(on("portwatch_ports"));
        t.supply->crops()->setEnabled(on("crop_yields"));
        t.supply->road()->setEnabled(on("itf_road"));
        t.supply->oil()->setEnabled(on("jodi"));
        t.supply->eia()->setEnabled(on("eia_stocks"));
        t.supply->pressure()->setEnabled(on("nyfed_gscpi"));
        t.supply->freight()->setEnabled(on("fred_freight"));
    }
    t.fx->setEnabled(on("fx"));
    t.policyRates->setEnabled(on("bis_daily"));
    t.cot->setEnabled(on("cftc"));
    t.cotReport->setEnabled(on("cftc_legacy"));
    quant::StateRefresher::instance()->setEnabled(on("markov_refresh"));
    t.economy->setEnabled(on("imf_weo"));
    t.bop->setEnabled(on("imf_bop"));
    t.trade->setEnabled(on("imf_trade"));
    t.worldRates->setEnabled(on("bis_monthly"));
    t.sectors->setEnabled(on("worldbank"));
    t.propertyCredit->setEnabled(on("bis_credit"));
    t.housePrices->setEnabled(on("oecd_housing"));
    t.globalDebt->setEnabled(on("imf_gdd"));
    t.bankCredit->setEnabled(on("worldbank_credit"));
    t.oecd->setEnabled(on("oecd_kei"));
    t.eurostat->setEnabled(on("eurostat"));
    t.imfPrices->setEnabled(on("imf_cpi"));
    t.bls->setEnabled(on("bls"));
    t.brazil->setEnabled(on("nat_brazil"));
    t.argentina->setEnabled(on("nat_argentina"));
    t.southAfrica->setEnabled(on("nat_south_africa"));
    t.singapore->setEnabled(on("nat_singapore"));
    t.fred->setEnabled(on("fred"));
    t.eia->setEnabled(on("eia"));
    t.bea->setEnabled(on("bea"));
    t.banxico->setEnabled(on("banxico"));
    t.ecos->setEnabled(on("ecos"));
    t.evds->setEnabled(on("evds"));
    t.surveys->setEnabled(on("surveys"));
    t.comtrade->setEnabled(on("comtrade"));
    for (const SurveyHeadlines::Publisher &p : SurveyHeadlines::publishers())
        t.surveys->setPublisherEnabled(p.id, on(QByteArray("survey_") + p.id.toLatin1()));
    t.futures->setEnabled(on("yahoo_futures"));
    t.benchmarks->setEnabled(on("imf_pcps"));
    t.economicCalendar->setEnabled(on("tradingview_calendar"));
    t.corporateCalendar->setEnabled(on("nasdaq_calendars"));
    t.marketSessions->setEnabled(on("yahoo_sessions"));
    for (const char *group : {"yahoo", "nasdaq", "sec", "wikipedia", "wikidata", "treasury", "fred", "oecd", "coinmarketcap",
                              "alternative", "worldbank", "imf", "eurostat", "bis", "ecb", "ilo", "eia", "ons", "boe", "bcb",
                              "statcan", "boc", "boj", "snb", "abs", "bbk", "insee", "ine", "pxweb", "inegi", "bps", "estat",
                              "fed", "cftc", "binance", "investinglive", "ing", "mof", "philly"})
        research::Client::instance()->setEnabled(group, on(QByteArray("research_") + group));
    Myfxbook::instance()->setEnabled(on("myfxbook"));
    news::Wire::instance()->setEnabled("rss", on("news_feeds"));
    news::Wire::instance()->setEnabled("fmp", on("fmp"));
    adblock::Blocker::instance()->setEnabled(on("adblock"));
    QStringList market;
    for (const QString &id : MarketIds)
        if (off.contains(id)) market << id;
    if (t.backend)
        t.backend->setDisabledSources(market);
}

SourcesPage::SourcesPage(const Targets &targets, QWidget *parent)
    : QWidget(parent)
    , m_t(targets)
{
    setObjectName("sidePanel");
    setAttribute(Qt::WA_StyledBackground);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // Sub-header: the list of sources, and the diagram of where their data goes.
    auto *bar = new QWidget;
    bar->setObjectName("calSubBar");
    bar->setAttribute(Qt::WA_StyledBackground);
    bar->setFixedHeight(36);
    auto *barLayout = new QHBoxLayout(bar);
    barLayout->setContentsMargins(12, 0, 12, 0);
    barLayout->setSpacing(4);
    m_tabs = new QButtonGroup(this);
    const QStringList tabNames {tr("List"), tr("Data")};
    for (int i = 0; i < tabNames.size(); ++i) {
        auto *b = new QToolButton;
        b->setObjectName("calSubTab");
        b->setText(tabNames[i].toUpper());
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedHeight(36);
        m_tabs->addButton(b, i);
        barLayout->addWidget(b);
    }
    barLayout->addStretch(1);
    outer->addWidget(bar);
    m_stack = new QStackedWidget;
    outer->addWidget(m_stack, 1);

    auto *scroll = new QScrollArea;
    m_listScroll = scroll;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *content = new QWidget;
    content->setObjectName("panelContent");
    auto *l = new QVBoxLayout(content);
    l->setContentsMargins(32, 26, 32, 32);
    l->setSpacing(6);

    auto *head = new QHBoxLayout;
    auto *titles = new QVBoxLayout;
    titles->setSpacing(6);
    auto *title = new QLabel(tr("Sources"));
    title->setObjectName("title");
    titles->addWidget(title);
    auto *hint = dimLabel(tr("Every place Deepwater gets its data from, what each one feeds and how it is doing, and "
                             "the official bodies behind each country's statistics, with their sites and APIs. A source "
                             "turned off makes no more requests: the map and forex pages keep the last copy saved on this "
                             "computer, and the charts move on to the next provider in line."));
    hint->setWordWrap(true);
    titles->addWidget(hint);
    head->addLayout(titles, 1);
    head->addSpacing(24);
    auto *testAll = new QPushButton(tr("Test all"));
    testAll->setObjectName("segButton");
    testAll->setCursor(Qt::PointingHandCursor);
    connect(testAll, &QPushButton::clicked, this, [this] {
        for (Row &r : m_rows)
            if (r.test && r.test->isEnabled()) runTest(r);
    });
    head->addWidget(testAll, 0, Qt::AlignBottom);
    l->addLayout(head);
    l->addSpacing(8);
    m_summary = new QLabel;
    m_summary->setTextFormat(Qt::RichText);
    l->addWidget(m_summary);
    l->addSpacing(6);

    // Search and filters, over every source of the page.
    auto *filters = new QHBoxLayout;
    filters->setSpacing(8);
    m_search = new QLineEdit;
    m_search->setObjectName("keyEdit");
    m_search->setPlaceholderText(tr("Search a source, a site, an indicator"));
    m_search->setClearButtonEnabled(true);
    filters->addWidget(m_search, 2);
    m_country = new QComboBox;
    m_country->setObjectName("sourceCombo");
    m_country->addItem(tr("Every country"), QString());
    {
        QSet<QString> seen;
        QVector<QPair<QString, QString>> items;
        for (const OfficialSources::Source &s : OfficialSources::instance().sources())
            for (const QString &c : s.countries)
                if (!seen.contains(c)) {
                    seen.insert(c);
                    items.push_back({OfficialSources::countryName(c), c});
                }
        std::sort(items.begin(), items.end(), [](const auto &a, const auto &b) { return a.first.localeAwareCompare(b.first) < 0; });
        for (const auto &it : items)
            m_country->addItem(it.first, it.second);
    }
    filters->addWidget(m_country, 1);
    m_access = new QComboBox;
    m_access->setObjectName("sourceCombo");
    m_access->addItem(tr("Free data"), int(OfficialSources::Access::Free));
    m_access->addItem(tr("Headline figure free, series paid"), int(OfficialSources::Access::Headline));
    m_access->addItem(tr("Paid"), int(OfficialSources::Access::Paid));
    m_access->addItem(tr("Every access"), -1);
    filters->addWidget(m_access, 1);
    l->addLayout(filters);
    m_onlyChip = new QPushButton;
    m_onlyChip->setObjectName("segButton");
    m_onlyChip->setCursor(Qt::PointingHandCursor);
    m_onlyChip->setToolTip(tr("Show every source again"));
    m_onlyChip->hide();
    connect(m_onlyChip, &QPushButton::clicked, this, [this] { showOnly({}, {}); });
    l->addSpacing(4);
    l->addWidget(m_onlyChip, 0, Qt::AlignLeft);
    l->addSpacing(10);

    using D = Dataset;
    auto dataset = [](auto *t, qint64 every, const QUrl &probe) {
        D d;
        d.fetchedAt = [t] { return t->fetchedAt(); };
        d.error = [t] { return t->errorString(); };
        d.fetching = [t] { return t->isFetching(); };
        d.refresh = [t] { t->refreshNow(); };
        d.everySecs = every;
        d.probeUrl = probe;
        return d;
    };
    auto add = [&](QWidget *w) {
        l->addWidget(w);
        if (!m_sections.isEmpty() && !m_rows.isEmpty()) {
            m_sections.last().rowIds << m_rows.last().id;
            m_rows.last().section = m_sections.last().title;
        }
    };
    auto section = [&](const QString &title) {
        QLabel *label = sectionLabel(title);
        l->addWidget(label);
        m_sections.push_back({label, {}, title});
    };

    section(tr("Map · shipping"));
    add(buildRow({"ais", Kind::Stream, tr("Live ship positions (AIS)"), "aisstream.io", "stream.aisstream.io",
                  tr("Map: ships, their routes and the traffic through chokepoints")}));
    add(buildRow({"portwatch", Kind::Dataset, tr("Chokepoint transits"), "IMF PortWatch", "services9.arcgis.com",
                  tr("Map: daily transits through the chokepoints by vessel type, a few days behind")},
                 dataset(m_t.portWatch, 12 * 3600,
                         QUrl("https://services9.arcgis.com/weJ1QsnbMYJlCHdG/ArcGIS/rest/services/Daily_Chokepoints_Data/"
                              "FeatureServer/0/query?where=1%3D0&outFields=year&f=json"))));
    l->addSpacing(12);

    section(tr("Map · supply chains"));
    {
        supply::SupplyChain *sc = m_t.supply;
        auto withProbe = [&](auto *set, qint64 every) {
            D d = dataset(set, every, set->sourceUrl());
            d.probe = [set] { return set->sourceUrl(); };   // carries the key of the moment
            return d;
        };
        add(buildRow({"portwatch_ports", Kind::Dataset, tr("Ports and disruptions"), "IMF PortWatch", "services9.arcgis.com",
                      tr("Map: ~2,000 ports with their port calls and cargo against a year before, and the disruptions "
                         "that hit them (supply chain layer)")},
                     dataset(sc->ports(), 24 * 3600, supply::PortActivity::probeUrl())));
        add(buildRow({"crop_yields", Kind::Dataset, tr("Crop yields"),
                      tr("USDA Foreign Agricultural Service (PSD) · Eurostat"), "apps.fas.usda.gov · ec.europa.eu",
                      tr("Map: wheat, corn, rice and soybean yields by country, with the current marketing year's forecasts")},
                     dataset(sc->crops(), 7 * 24 * 3600, sc->crops()->sourceUrl())));
        add(buildRow({"itf_road", Kind::Dataset, tr("Road freight"), tr("International Transport Forum (OECD)"), "sdmx.oecd.org",
                      tr("Map: goods carried by road in tonne-kilometres, about 45 countries, annual")},
                     dataset(sc->road(), 7 * 24 * 3600, sc->road()->sourceUrl())));
        add(buildRow({"jodi", Kind::Dataset, tr("Crude oil stocks"), tr("Joint Organisations Data Initiative (JODI-Oil)"),
                      "www.jodidata.org", tr("Map: month-end crude stocks by country (a 23 MB download, weekly)")},
                     dataset(sc->oil(), 7 * 24 * 3600, sc->oil()->sourceUrl())));
        add(buildRow({"eia_stocks", Kind::Dataset, tr("US crude stocks"), tr("U.S. Energy Information Administration (the EIA key)"),
                      "api.eia.gov", tr("Map: weekly stocks at Cushing, on the Gulf Coast and in the Strategic Petroleum Reserve")},
                     withProbe(sc->eia(), 12 * 3600)));
        add(buildRow({"nyfed_gscpi", Kind::Dataset, tr("Supply chain pressure"), tr("Federal Reserve Bank of New York (GSCPI)"),
                      "www.newyorkfed.org", tr("Map: the Global Supply Chain Pressure Index, monthly")},
                     dataset(sc->pressure(), 24 * 3600, sc->pressure()->sourceUrl())));
        add(buildRow({"fred_freight", Kind::Dataset, tr("US freight"), tr("FRED (the FRED key): ATA, Cass, BTS, BLS"),
                      "api.stlouisfed.org", tr("Map: truck tonnage, freight shipments, freight services and deep sea freight prices")},
                     withProbe(sc->freight(), 12 * 3600)));
    }
    l->addSpacing(12);

    section(tr("Map · commodity prices"));
    add(buildRow({"yahoo_futures", Kind::Dataset, tr("Commodity futures"), tr("Yahoo Finance (NYMEX, COMEX, CBOT, ICE US)"),
                  "query1.finance.yahoo.com",
                  tr("Map: prices of 20 commodities, about 10 minutes behind the exchange")},
                 dataset(m_t.futures, 5 * 60,
                         QUrl("https://query1.finance.yahoo.com/v7/finance/spark?symbols=CL=F&range=1d&interval=1d"))));
    add(buildRow({"imf_pcps", Kind::Dataset, tr("Monthly commodity prices"), tr("International Monetary Fund (PCPS)"),
                  "api.imf.org",
                  tr("Map: monthly average benchmark prices; the only price of coal, uranium, nickel, zinc, lithium and cobalt")},
                 dataset(m_t.benchmarks, 24 * 3600, m_t.benchmarks->sourceUrl())));
    l->addSpacing(12);

    section(tr("Forex"));
    add(buildRow({"fx", Kind::Dataset, tr("Reference exchange rates"), tr("European Central Bank, via Frankfurter"),
                  "api.frankfurter.dev", tr("Forex: pairs, sparklines, correlations")},
                 dataset(m_t.fx, 6 * 3600, QUrl("https://api.frankfurter.dev/v1/latest?base=USD&symbols=EUR"))));
    add(buildRow({"bis_daily", Kind::Dataset, tr("Central bank policy rates"), tr("Bank for International Settlements"),
                  "stats.bis.org", tr("Forex: policy rates and the carry on each pair")},
                 dataset(m_t.policyRates, 12 * 3600, m_t.policyRates->sourceUrl())));
    add(buildRow({"cftc", Kind::Dataset, tr("Futures positioning (COT)"), "CFTC", "publicreporting.cftc.gov",
                  tr("Forex: capital flows of asset managers and leveraged funds")},
                 dataset(m_t.cot, 12 * 3600, m_t.cot->sourceUrl())));
    l->addSpacing(12);

    section(tr("Calendar"));
    add(buildRow({"tradingview_calendar", Kind::Dataset, tr("Economic calendar"), "TradingView",
                  "economic-calendar.tradingview.com",
                  tr("Calendar: economic releases of 21 economies and the euro area, with actual, forecast and previous")},
                 dataset(m_t.economicCalendar, 10 * 60, m_t.economicCalendar->sourceUrl())));
    add(buildRow({"nasdaq_calendars", Kind::Dataset, tr("Corporate calendars"), "Nasdaq", "api.nasdaq.com",
                  tr("Calendar: earnings, dividends, stock splits and IPOs of US listings")},
                 dataset(m_t.corporateCalendar, 3 * 3600, QUrl("https://api.nasdaq.com/api/calendar/splits"))));
    add(buildRow({"yahoo_sessions", Kind::Dataset, tr("Exchange sessions"), "Yahoo Finance", "query1.finance.yahoo.com",
                  tr("Calendar: whether an exchange opened today (holidays) in Market hours")},
                 dataset(m_t.marketSessions, 15 * 60, m_t.marketSessions->sourceUrl())));
    l->addSpacing(12);

    section(tr("Research"));
    // Downloaded on demand, one company at a time: the state is the last answer of each group of hosts.
    auto research = [](const QString &group, const QUrl &probe) {
        D d;
        d.fetchedAt = [group] { return research::Client::instance()->host(group).lastOk; };
        d.error = [group] {
            const research::HostState h = research::Client::instance()->host(group);
            return h.lastError.isValid() && h.lastError > h.lastOk ? h.error : QString();
        };
        d.fetching = [group] { return research::Client::instance()->host(group).pending > 0; };
        d.refresh = [group] { research::Client::instance()->forget(group); };
        d.everySecs = 24 * 3600;
        d.probeUrl = probe;
        return d;
    };
    add(buildRow({"research_yahoo", Kind::Dataset, tr("Company data and prices"), "Yahoo Finance", "query1.finance.yahoo.com",
                  tr("Research: profile, key statistics, statements, analysts' ratings and targets, prices, peers. Desk and "
                     "Journal: quotes and bars, fed funds futures")},
                 research("yahoo", QUrl("https://query1.finance.yahoo.com/v1/finance/search?q=AAPL&quotesCount=1&newsCount=0"))));
    add(buildRow({"research_nasdaq", Kind::Dataset, tr("Holders, insiders and listings"), "Nasdaq", "api.nasdaq.com",
                  tr("Research: institutional holders (13F), insider trades (Form 4), every US listing with its sector "
                     "and industry, the Nasdaq-100")},
                 research("nasdaq", QUrl("https://api.nasdaq.com/api/quote/list-type/nasdaq100"))));
    add(buildRow({"research_sec", Kind::Dataset, tr("Reported financials"), "SEC EDGAR", "data.sec.gov · efts.sec.gov",
                  tr("Research: the history of the figures companies file in 10-K and 10-Q reports (XBRL)")},
                 research("sec", QUrl("https://data.sec.gov/submissions/CIK0000320193.json"))));
    add(buildRow({"research_wikipedia", Kind::Dataset, tr("Index members"), "Wikipedia", "en.wikipedia.org",
                  tr("Research: the S&P 500 and the Dow, with their GICS sectors")},
                 research("wikipedia", QUrl("https://en.wikipedia.org/w/api.php?action=parse&page=Template:Dow_Jones_Industrial_Average_companies&prop=wikitext&format=json"))));
    add(buildRow({"research_wikidata", Kind::Dataset, tr("Head counts and supply chain"), "Wikidata", "query.wikidata.org",
                  tr("Research: a company's number of employees over the years, and the makers of its products (SPLC)")},
                 research("wikidata", QUrl("https://query.wikidata.org/sparql?query=ASK%7B%7D"))));
    l->addSpacing(12);

    section(tr("Overview"));
    add(buildRow({"research_treasury", Kind::Dataset, tr("Treasury yields"), "U.S. Department of the Treasury",
                  "home.treasury.gov",
                  tr("Overview: the daily par yield curve (US bonds); the other quotes, sectors, movers and index charts "
                     "come from Yahoo Finance (Research, above), the largest companies from Nasdaq's screener")},
                 research("treasury", QUrl(QString("https://home.treasury.gov/resource-center/data-chart-center/interest-rates/"
                                                   "daily-treasury-rates.csv/%1/all?type=daily_treasury_yield_curve"
                                                   "&field_tdr_date_value=%1&page&_format=csv")
                                               .arg(QDate::currentDate().year())))));
    l->addSpacing(12);

    section(tr("Economy"));
    add(buildRow({"research_fred", Kind::Dataset, tr("US economic series"), tr("Federal Reserve Bank of St. Louis (FRED)"),
                  "fred.stlouisfed.org",
                  tr("Economy: about fifty US series (VIX, spreads, GDP, prices, jobs, rates, housing, money, the dollar "
                     "against the yen…) and about 170 more in Countries › United States; through FRED's API with the key "
                     "saved under Map · databases with a free key, else FRED's public CSV")},
                 research("fred", QUrl("https://fred.stlouisfed.org/graph/fredgraph.csv?id=UNRATE&cosd=2026-01-01"))));
    add(buildRow({"research_oecd", Kind::Dataset, tr("Consumer confidence"), "OECD", "sdmx.oecd.org",
                  tr("Economy: the OECD's consumer confidence indicator for the United States (key economic indicators)")},
                 research("oecd", QUrl("https://sdmx.oecd.org/public/rest/data/OECD.SDD.STES,DSD_KEI@DF_KEI,4.0/"
                                       "USA.M.CCICP.....?startPeriod=2026-01&format=csv"))));
    add(buildRow({"research_coinmarketcap", Kind::Dataset, tr("Crypto market"), "CoinMarketCap", "api.coinmarketcap.com",
                  tr("Economy: total crypto market cap, bitcoin dominance and the altcoin season index")},
                 research("coinmarketcap", QUrl("https://api.coinmarketcap.com/data-api/v4/global-metrics/quotes/"
                                                "historical?convertId=2781&range=1y"))));
    add(buildRow({"research_alternative", Kind::Dataset, tr("Crypto Fear & Greed"), "Alternative.me", "api.alternative.me",
                  tr("Economy: the crypto Fear & Greed index; the stock market's is Deepwater's estimate from Yahoo "
                     "Finance prices and FRED spreads")},
                 research("alternative", QUrl("https://api.alternative.me/fng/?limit=1"))));
    l->addSpacing(12);

    // Economy › Countries: one country at a time, on demand (resources/country_indicators.json); FRED, the OECD and
    // Yahoo Finance have their rows above.
    section(tr("Economy · countries"));
    add(buildRow({"research_worldbank", Kind::Dataset, tr("Development indicators and Global Economic Monitor"),
                  tr("World Bank"), "api.worldbank.org",
                  tr("Countries: annual national accounts, labour, trade, public finance, energy and health (WDI) and the "
                     "monthly Global Economic Monitor (core prices, industrial production, merchandise trade, reserves, "
                     "retail sales, stock markets)")},
                 research("worldbank", QUrl("https://api.worldbank.org/v2/country/KEN/indicator/CPTOTNSXN?source=15&format=json&mrv=1&frequency=M"))));
    add(buildRow({"research_imf", Kind::Dataset, tr("Statistics department data sets and WEO"), tr("International Monetary Fund"),
                  "api.imf.org",
                  tr("Countries: consumer and producer prices, production, labour, quarterly national accounts, money, "
                     "central bank balance sheets, credit, interest rates, reserves and gold, exchange rates, trade, balance "
                     "of payments, share prices; the World Economic Outlook's reported years")},
                 research("imf", QUrl("https://api.imf.org/external/sdmx/3.0/data/dataflow/IMF.STA/CPI/+/KEN.CPI._T.IX.M"
                                      "?c%5BTIME_PERIOD%5D=ge:2026-M01&attributes=none&measures=all"))));
    add(buildRow({"research_eurostat", Kind::Dataset, tr("European statistics"), "Eurostat", "ec.europa.eu/eurostat",
                  tr("Countries (EU, EFTA, candidates): GDP, HICP, labour market, minimum wages, labour costs, production, "
                     "prices, confidence, construction, house prices, public finance, tourism, car registrations")},
                 research("eurostat", QUrl("https://ec.europa.eu/eurostat/api/dissemination/statistics/1.0/data/"
                                           "une_rt_m?geo=FR&s_adj=SA&age=TOTAL&unit=PC_ACT&sex=T&lastTimePeriod=1"))));
    add(buildRow({"research_ecb", Kind::Dataset, tr("Money, credit and bank rates"), tr("European Central Bank"),
                  "data-api.ecb.europa.eu",
                  tr("Countries (euro area and members): M1, M2, M3, loans, mortgage, lending and deposit rates, central "
                     "bank balance sheets. Desk › Rates: the euro area yield curve, €STR and meeting dates. Desk › Forecasts: its Survey of "
                     "Professional Forecasters")},
                 research("ecb", QUrl("https://data-api.ecb.europa.eu/service/data/MIR/M.FR.B.A2C.A.R.A.2250.EUR.N"
                                      "?format=csvdata&lastNObservations=1"))));
    add(buildRow({"research_bis", Kind::Dataset, tr("Policy rates, property prices and credit"),
                  tr("Bank for International Settlements"), "stats.bis.org",
                  tr("Countries: policy rates, residential property prices, household and private debt in % of GDP. "
                     "Desk › Notes: central bankers' speeches")},
                 research("bis", QUrl("https://stats.bis.org/api/v2/data/dataflow/BIS/WS_CBPOL/1.0/M.XM?format=csv&detail=dataonly&lastNObservations=1"))));
    add(buildRow({"research_ilo", Kind::Dataset, tr("Labour force surveys"), tr("International Labour Organization (ILOSTAT)"),
                  "rplumber.ilo.org",
                  tr("Countries: unemployment, employment and participation rates, unemployed and employed persons from "
                     "the national labour force surveys, statutory minimum wages")},
                 research("ilo", QUrl("https://rplumber.ilo.org/data/indicator/?id=UNE_DEAP_SEX_AGE_RT_A&ref_area=FRA&format=.csv&timefrom=2024"))));
    add(buildRow({"research_eia", Kind::Dataset, tr("Weekly petroleum and gas stocks"),
                  tr("U.S. Energy Information Administration"), "api.eia.gov",
                  tr("Countries (United States): crude, gasoline, distillate and gas stocks, refinery runs, production, net "
                     "imports; with the EIA key saved under Map · databases with a free key")},
                 research("eia", QUrl("https://www.eia.gov/opendata/"))));
    // National offices and central banks read directly, each first for its own country.
    add(buildRow({"research_ons", Kind::Dataset, tr("United Kingdom statistics"), tr("Office for National Statistics"),
                  "www.ons.gov.uk", tr("Countries › United Kingdom: prices, labour market, GDP, retail, public finance, trade, "
                                       "current account, production, saving, productivity")},
                 research("ons", QUrl("https://www.ons.gov.uk/economy/inflationandpriceindices/timeseries/d7g7/mm23/data"))));
    add(buildRow({"research_boe", Kind::Dataset, tr("Money and mortgages"), tr("Bank of England"), "www.bankofengland.co.uk",
                  tr("Countries › United Kingdom: M4, mortgage approvals, fixed mortgage rates. Desk › Rates: the gilt and OIS curves, meeting dates and the last vote")},
                 research("boe", QUrl("https://www.bankofengland.co.uk/boeapps/database/_iadb-fromshowcolumns.asp?csv.x=yes"
                                      "&Datefrom=01/Jan/2026&Dateto=now&SeriesCodes=IUDBEDR&CSVF=TN&UsingCodes=Y&VPD=Y&VFD=N"))));
    add(buildRow({"research_bcb", Kind::Dataset, tr("Brazilian time series (SGS)"), tr("Central Bank of Brazil"), "api.bcb.gov.br",
                  tr("Countries › Brazil: IPCA, Selic, unemployment, formal jobs, public debt, trade and current account, "
                     "M0 to M4, reserves, credit and its rate, consumer confidence, industrial and vehicle production")},
                 research("bcb", QUrl("https://api.bcb.gov.br/dados/serie/bcdata.sgs.433/dados/ultimos/1?formato=json"))));
    add(buildRow({"research_statcan", Kind::Dataset, tr("Canadian data tables"), tr("Statistics Canada"), "www150.statcan.gc.ca",
                  tr("Countries › Canada: labour force survey, CPI and the core measures, wages, housing starts, new housing "
                     "prices")},
                 research("statcan", QUrl("https://www150.statcan.gc.ca/t1/wds/rest/getDataFromVectorByReferencePeriodRange"
                                          "?vectorIds=2062815&startRefPeriod=2026-01-01&endReferencePeriod=2026-12-31"))));
    add(buildRow({"research_boc", Kind::Dataset, tr("Rates and money"), tr("Bank of Canada"), "www.bankofcanada.ca",
                  tr("Countries › Canada: policy rate, 10-year yield, prime and 5-year mortgage rates, M2, the Bank's assets. Desk › Rates: the yield curve and meeting dates")},
                 research("boc", QUrl("https://www.bankofcanada.ca/valet/observations/V39079/json?recent=1"))));
    add(buildRow({"research_boj", Kind::Dataset, tr("Time-series data"), tr("Bank of Japan"), "www.stat-search.boj.or.jp",
                  tr("Countries › Japan: call rate, monetary base, M2, M3, producer, export and import prices, Tankan, the "
                     "Bank's assets, bank loans and lending rate, balance of payments. Desk › Rates: meeting dates")},
                 research("boj", QUrl("https://www.stat-search.boj.or.jp/api/v1/getDataCode?format=json&lang=en&db=FM01&code=STRDCLUCON&startDate=202610"))));
    add(buildRow({"research_snb", Kind::Dataset, tr("Data portal"), tr("Swiss National Bank"), "data.snb.ch",
                  tr("Countries › Switzerland: M1, M2, M3, consumer prices, SARON. Desk › Rates: the Confederation curve and meeting dates")},
                 research("snb", QUrl("https://data.snb.ch/api/cube/plkopr/data/csv/en"))));
    add(buildRow({"research_abs", Kind::Dataset, tr("Australian statistics"), tr("Australian Bureau of Statistics"),
                  "data.api.abs.gov.au", tr("Countries › Australia: labour force, monthly CPI and trimmed mean, GDP, saving "
                                            "ratio, productivity, household spending, job vacancies")},
                 research("abs", QUrl("https://data.api.abs.gov.au/rest/data/ABS,LF/M13.3.1599.20.AUS.M?startPeriod=2026-01&format=csvfile"))));
    add(buildRow({"research_bbk", Kind::Dataset, tr("Time series database"), "Deutsche Bundesbank", "api.statistiken.bundesbank.de",
                  tr("Countries › Germany: registered unemployment, employment, vacancies, production, factory orders, retail "
                     "sales, consumer and producer prices, exports and imports (many from the Federal Statistical Office)")},
                 research("bbk", QUrl("https://api.statistiken.bundesbank.de/rest/data/BBDP1/M.DE.N.VPI.C.A00000.VGJ.LV"
                                      "?format=sdmx_csv&lang=en&lastNObservations=1"))));
    add(buildRow({"research_insee", Kind::Dataset, tr("Macro-economic database (BDM)"), "INSEE", "bdm.insee.fr",
                  tr("Countries › France: business climate, household confidence, private payroll employment, household "
                     "consumption of goods")},
                 research("insee", QUrl("https://bdm.insee.fr/series/sdmx/data/SERIES_BDM/001565530?lastNObservations=1"))));
    add(buildRow({"research_ine", Kind::Dataset, tr("Tempus database"), tr("National Statistics Institute (Spain)"),
                  "servicios.ine.es", tr("Countries › Spain: international tourist arrivals (FRONTUR)")},
                 research("ine", QUrl("https://servicios.ine.es/wstempus/js/EN/DATOS_SERIE/FREG337?nult=1"))));
    add(buildRow({"research_pxweb", Kind::Dataset, tr("PxWeb tables"), tr("Statistics Norway · Statistics Sweden"),
                  "data.ssb.no · api.scb.se", tr("Countries › Norway: CPI-ATE, Mainland GDP growth; Sweden: CPIF excluding "
                                                 "energy")},
                 research("pxweb", QUrl("https://data.ssb.no/api/pxwebapi/v2-beta/tables/05327/metadata?lang=en"))));
    // Offices that want a user's free key: the key is entered here (Banxico, ECOS and EVDS have theirs under Map ·
    // national offices, read directly; Countries reads the same keys).
    auto keyedResearch = [&](const QString &group, const QUrl &probe, const QString &keyId, const QString &hint) {
        D d = research(group, probe);
        d.keyId = keyId;
        d.keyHint = hint;
        return d;
    };
    add(buildRow({"research_inegi", Kind::Dataset, tr("Mexican indicators (BIE)"), tr("INEGI (free token)"), "www.inegi.org.mx",
                  tr("Countries › Mexico: the national statistics office's indicators, with Banxico's (Map · national offices)")},
                 keyedResearch("inegi", QUrl("https://www.inegi.org.mx/app/api/indicadores/"), "inegi",
                               tr("Free token at inegi.org.mx › Servicio web / API de indicadores"))));
    add(buildRow({"research_bps", Kind::Dataset, tr("Indonesian statistics"), tr("Statistics Indonesia (BPS, free key)"),
                  "webapi.bps.go.id", tr("Countries › Indonesia: monthly inflation, unemployment (February and August), exports, imports, "
                                         "foreign tourist visits; three years per request, so since 2015")},
                 keyedResearch("bps", QUrl("https://webapi.bps.go.id/"), "bps", tr("Free key at webapi.bps.go.id (register, then App ID)"))));
    add(buildRow({"research_estat", Kind::Dataset, tr("Japanese government statistics"), tr("e-Stat (free application ID)"),
                  "api.e-stat.go.jp", tr("Countries › Japan: the Statistics Bureau's consumer price index (all items, less fresh "
                                         "food, less fresh food and energy, services), with the Bank of Japan's series (above)")},
                 keyedResearch("estat", QUrl("https://api.e-stat.go.jp/"), "estat", tr("Free application ID at e-stat.go.jp › API (user registration)"))));
    l->addSpacing(12);

    section(tr("Quant"));
    add(buildRow({"cftc_legacy", Kind::Dataset, tr("Commitments of Traders (legacy, futures only)"), "CFTC", "publicreporting.cftc.gov",
                  tr("Quant: positions of non-commercial, commercial and non-reportable traders in 23 futures markets: "
                     "indices, bitcoin, currencies, metals, energy, grains (COT); prices and key figures come from Yahoo "
                     "Finance (Research, above)")},
                 dataset(m_t.cotReport, 12 * 3600, m_t.cotReport->sourceUrl())));
    {
        // The Markov models' background re-estimation: not a download, but it behaves like one (runs on its own,
        // can be turned off, has a last run and a Refresh).
        auto *rf = quant::StateRefresher::instance();
        Dataset d;
        d.fetchedAt = [rf] { return rf->lastRun(); };
        d.error = [rf] { return rf->lastError(); };
        d.fetching = [rf] { return rf->isBusy(); };
        d.refresh = [rf] { rf->refreshAll(); };
        d.everySecs = quant::StateRefresher::IntervalSecs;
        d.probeUrl = QUrl("https://query1.finance.yahoo.com/v8/finance/chart/SPY?range=5d&interval=1d");
        add(buildRow({"markov_refresh", Kind::Dataset, tr("Regimes and volatility forecasts"), tr("Deepwater (Quant › Markov)"),
                      tr("on this computer, prices from Yahoo Finance"),
                      tr("Quant and Research: the HMM regime and MS-GARCH forecast of every symbol in the shared state (Portfolio, "
                         "strips), re-estimated when older than the last trading day")},
                     d));
    }
    l->addSpacing(12);

    section(tr("Desk"));
    add(buildRow({"research_fed", Kind::Dataset, tr("Meeting calendar and press releases"), tr("Federal Reserve Board"),
                  "www.federalreserve.gov", tr("Desk › Rates: the dates of the FOMC's meetings; Desk › Notes: its monetary press releases")},
                 research("fed", QUrl("https://www.federalreserve.gov/monetarypolicy/fomccalendars.htm"))));
    add(buildRow({"research_cftc", Kind::Dataset, tr("Traders in Financial Futures"), "CFTC", "publicreporting.cftc.gov",
                  tr("Quant › COT: dealers, asset managers and leveraged funds in index, currency and bitcoin futures")},
                 research("cftc", QUrl("https://publicreporting.cftc.gov/resource/gpe5-46if.json?$limit=1"))));
    add(buildRow({"research_binance", Kind::Dataset, tr("Long and short accounts"), "Binance", "fapi.binance.com",
                  tr("Desk › Positioning: the share of accounts long and short on twelve USDT perpetuals")},
                 research("binance", QUrl("https://fapi.binance.com/futures/data/globalLongShortAccountRatio?symbol=BTCUSDT&period=1d&limit=1"))));
    {
        // Not through the Research client: its session travels in the URL (Myfxbook.h). Connected in Desk › Positioning.
        D d;
        Myfxbook *m = Myfxbook::instance();
        d.fetchedAt = [m] { return m->fetchedAt(); };
        d.error = [m] { return m->errorString(); };
        d.fetching = [m] { return m->isFetching(); };
        d.refresh = [m] { m->refreshNow(); };
        d.everySecs = 30 * 60;
        d.probeUrl = QUrl("https://www.myfxbook.com/api/get-community-outlook.json");
        add(buildRow({"myfxbook", Kind::Dataset, tr("Community outlook"), tr("Myfxbook (free account)"), "www.myfxbook.com",
                      tr("Desk › Positioning: the share of lots long and short of its community on forex and CFDs. "
                         "Connect the account there: the password isn't kept")},
                     d));
    }
    add(buildRow({"research_investinglive", Kind::Dataset, tr("Market headlines"), "InvestingLive", "investinglive.com",
                  tr("Desk › Wire and Recap: its public feed of market headlines")},
                 research("investinglive", QUrl("https://investinglive.com/feed/"))));
    add(buildRow({"research_ing", Kind::Dataset, tr("Economic research and forecasts"), "ING Think", "think.ing.com",
                  tr("Desk › Notes: the titles of its public research. Desk › Forecasts: its forecasts by quarter for growth, "
                     "inflation, rates, bonds, FX and commodities")},
                 research("ing", QUrl("https://think.ing.com/rss/"))));
    add(buildRow({"research_philly", Kind::Dataset, tr("Survey of Professional Forecasters"), tr("Federal Reserve Bank of Philadelphia"), "www.philadelphiafed.org",
                  tr("Desk › Forecasts: how far apart forecasters are on US inflation, growth and unemployment")},
                 research("philly", QUrl("https://www.philadelphiafed.org/surveys-and-data/cpi-spf"))));
    add(buildRow({"research_mof", Kind::Dataset, tr("Japanese government bond yields"), tr("Ministry of Finance, Japan"), "www.mof.go.jp",
                  tr("Desk › Rates: the yield curve")},
                 research("mof", QUrl("https://www.mof.go.jp/english/policy/jgbs/reference/interest_rate/jgbcme.csv"))));
    l->addSpacing(12);

    section(tr("Map · macroeconomics"));
    add(buildRow({"imf_weo", Kind::Dataset, tr("World Economic Outlook"), tr("International Monetary Fund"), "api.imf.org",
                  tr("Map: country indicators and heatmaps (growth, inflation, debt…)")},
                 dataset(m_t.economy, 24 * 3600, m_t.economy->sourceUrl())));
    add(buildRow({"imf_bop", Kind::Dataset, tr("Balance of payments"), tr("International Monetary Fund"), "api.imf.org",
                  tr("Map: capital flows and trade balance of each country")},
                 dataset(m_t.bop, 24 * 3600, m_t.bop->sourceUrl())));
    add(buildRow({"imf_trade", Kind::Dataset, tr("Trade in goods by partner"), tr("International Monetary Fund"), "api.imf.org",
                  tr("Map: the trade flow arcs between countries")},
                 dataset(m_t.trade, 7 * 24 * 3600, m_t.trade->sourceUrl())));
    add(buildRow({"bis_monthly", Kind::Dataset, tr("Policy rates of every country"), tr("Bank for International Settlements"),
                  "stats.bis.org", tr("Map: the policy rate heatmap and country details")},
                 dataset(m_t.worldRates, 24 * 3600, m_t.worldRates->sourceUrl())));
    add(buildRow({"worldbank", Kind::Dataset, tr("Economic sectors"), tr("World Bank"), "api.worldbank.org",
                  tr("Map: primary, secondary and tertiary sectors of each country (GDP and jobs)")},
                 dataset(m_t.sectors, 7 * 24 * 3600,
                         QUrl("https://api.worldbank.org/v2/country/FRA/indicator/NV.SRV.TOTL.ZS?format=json&per_page=1"))));
    add(buildRow({"bis_credit", Kind::Dataset, tr("Property prices, credit and debt service"),
                  tr("Bank for International Settlements"), "stats.bis.org",
                  tr("Map: real house prices, household and company debt, debt service (about 50 countries)")},
                 dataset(m_t.propertyCredit, 7 * 24 * 3600,
                         QUrl("https://stats.bis.org/api/v1/data/WS_DSR/Q.FR.H/all?startPeriod=2025&format=csv"))));
    add(buildRow({"oecd_housing", Kind::Dataset, tr("House prices against incomes and rents"), tr("OECD"),
                  "sdmx.oecd.org", tr("Map: whether housing is dear or cheap against its long-term average")},
                 dataset(m_t.housePrices, 7 * 24 * 3600, m_t.housePrices->sourceUrl())));
    add(buildRow({"imf_gdd", Kind::Dataset, tr("Global Debt Database"), tr("International Monetary Fund"), "api.imf.org",
                  tr("Map: debt of households and companies (about 90 countries)")},
                 dataset(m_t.globalDebt, 7 * 24 * 3600, m_t.globalDebt->sourceUrl())));
    add(buildRow({"worldbank_credit", Kind::Dataset, tr("Bank credit and bad loans"), tr("World Bank"), "api.worldbank.org",
                  tr("Map: bank credit to the private sector and non-performing loans (nearly every country)")},
                 dataset(m_t.bankCredit, 7 * 24 * 3600,
                         QUrl("https://api.worldbank.org/v2/country/FRA/indicator/FS.AST.PRVT.GD.ZS?format=json&per_page=1"))));
    l->addSpacing(12);

    section(tr("Map · official statistics"));
    add(buildRow({"oecd_kei", Kind::Dataset, tr("Key economic indicators"), tr("OECD (from the national offices)"), "sdmx.oecd.org",
                  tr("Map: latest inflation, unemployment, output, retail, confidence, rates, GDP and current account of "
                     "about 45 economies (country sheet, two heatmaps)")},
                 dataset(m_t.oecd, 12 * 3600, m_t.oecd->sourceUrl())));
    add(buildRow({"eurostat", Kind::Dataset, tr("European statistics"), tr("Eurostat (from the national offices)"), "ec.europa.eu",
                  tr("Map: harmonised inflation, unemployment, GDP, output, retail, confidence, producer prices, debt and "
                     "deficit of the EU, EFTA and candidate countries")},
                 dataset(m_t.eurostat, 12 * 3600, m_t.eurostat->sourceUrl())));
    add(buildRow({"imf_cpi", Kind::Dataset, tr("Consumer prices of every country"), tr("International Monetary Fund (CPI)"),
                  "api.imf.org", tr("Map: monthly inflation of about 190 countries (heatmap and country sheet)")},
                 dataset(m_t.imfPrices, 12 * 3600, m_t.imfPrices->sourceUrl())));
    add(buildRow({"bls", Kind::Dataset, tr("US labour and prices"), tr("U.S. Bureau of Labor Statistics"), "api.bls.gov",
                  tr("Map: CPI, core CPI, unemployment, payrolls and earnings of the United States, straight from the source")},
                 dataset(m_t.bls, 24 * 3600, QUrl("https://api.bls.gov/publicAPI/v1/timeseries/data/LNS14000000"))));
    l->addSpacing(12);

    section(tr("Map · national offices, read directly"));
    auto keyed = [&](auto *set, qint64 every, const QUrl &probe, const QString &keyId, const QString &hint) {
        D d = dataset(set, every, probe);
        d.probe = [set] { return set->sourceUrl(); };
        d.keyId = keyId;
        d.keyHint = hint;
        return d;
    };
    add(buildRow({"nat_brazil", Kind::Dataset, tr("Brazil"), tr("Banco Central do Brasil (SGS) · IBGE (SIDRA)"), "api.bcb.gov.br · apisidra.ibge.gov.br",
                  tr("Map: IPCA inflation, unemployment, industrial production and retail sales of Brazil, at the source")},
                 dataset(m_t.brazil, 12 * 3600, QUrl("https://api.bcb.gov.br/dados/serie/bcdata.sgs.13522/dados/ultimos/1?formato=json"))));
    add(buildRow({"nat_argentina", Kind::Dataset, tr("Argentina"), tr("INDEC, via datos.gob.ar"), "apis.datos.gob.ar",
                  tr("Map: inflation, monthly activity (EMAE) and unemployment of Argentina")},
                 dataset(m_t.argentina, 12 * 3600, m_t.argentina->sourceUrl())));
    add(buildRow({"nat_south_africa", Kind::Dataset, tr("South Africa"), tr("South African Reserve Bank"), "custom.resbank.co.za",
                  tr("Map: consumer and producer prices and the repo rate of South Africa")},
                 dataset(m_t.southAfrica, 12 * 3600, m_t.southAfrica->sourceUrl())));
    add(buildRow({"nat_singapore", Kind::Dataset, tr("Singapore"), tr("Singapore Department of Statistics"), "tablebuilder.singstat.gov.sg",
                  tr("Map: consumer prices of Singapore")},
                 dataset(m_t.singapore, 12 * 3600, m_t.singapore->sourceUrl())));
    add(buildRow({"banxico", Kind::Dataset, tr("Mexico"), tr("Banco de México (SIE API, free token)"), "www.banxico.org.mx",
                  tr("Map: inflation and the target rate of Mexico")},
                 keyed(m_t.banxico, 12 * 3600, m_t.banxico->sourceUrl(), "banxico", tr("Free token at banxico.org.mx › SIE › API"))));
    add(buildRow({"ecos", Kind::Dataset, tr("South Korea"), tr("Bank of Korea (ECOS API, free key)"), "ecos.bok.or.kr",
                  tr("Map: consumer prices of South Korea")},
                 keyed(m_t.ecos, 12 * 3600, m_t.ecos->sourceUrl(), "ecos", tr("Free key at ecos.bok.or.kr › Open API"))));
    add(buildRow({"evds", Kind::Dataset, tr("Türkiye"), tr("Central Bank of the Republic of Türkiye (EVDS, free key)"), "evds2.tcmb.gov.tr",
                  tr("Map: consumer prices of Türkiye")},
                 keyed(m_t.evds, 12 * 3600, m_t.evds->sourceUrl(), "evds", tr("Free key at evds2.tcmb.gov.tr (account › API key)"))));
    l->addSpacing(12);

    section(tr("Map · databases with a free key"));
    add(buildRow({"fred", Kind::Dataset, tr("FRED"), tr("Federal Reserve Bank of St. Louis (free key)"), "api.stlouisfed.org",
                  tr("Map: PCE inflation, housing starts, jobless claims, trade balance and consumer sentiment of the United States")},
                 keyed(m_t.fred, 12 * 3600, m_t.fred->sourceUrl(), "fred", tr("Free key at fred.stlouisfed.org › My Account › API Keys"))));
    add(buildRow({"eia", Kind::Dataset, tr("EIA"), tr("U.S. Energy Information Administration (free key)"), "api.eia.gov",
                  tr("Map: crude oil stocks and production, gasoline price of the United States (weekly)")},
                 keyed(m_t.eia, 12 * 3600, m_t.eia->sourceUrl(), "eia", tr("Free key at eia.gov/opendata"))));
    add(buildRow({"bea", Kind::Dataset, tr("BEA"), tr("U.S. Bureau of Economic Analysis (free key)"), "apps.bea.gov",
                  tr("Map: real GDP growth of the United States at an annual rate, as the BEA publishes it")},
                 keyed(m_t.bea, 12 * 3600, m_t.bea->sourceUrl(), "bea", tr("Free key at apps.bea.gov/API/signup"))));
    {
        D d;
        d.fetchedAt = [this] { return m_t.comtrade->lastFetch(); };
        d.error = [this] { return m_t.comtrade->lastError(); };
        d.fetching = [] { return false; };
        d.refresh = [] {};
        d.everySecs = 30 * 24 * 3600;
        d.probeUrl = m_t.comtrade->probeUrl();
        d.keyId = ComtradeProducts::keyId();
        d.keyHint = tr("Optional: without a key the public preview endpoint is used (a few requests a day); free key at comtradeplus.un.org");
        add(buildRow({"comtrade", Kind::Dataset, tr("UN Comtrade"), tr("United Nations (free key, optional)"), "comtradeapi.un.org",
                      tr("Map: a country's exports and imports by product, in its economy sheet (downloaded when shown)")}, d));
    }
    l->addSpacing(12);

    section(tr("Map · private surveys (headline figures)"));
    add(buildRow({"surveys", Kind::Dataset, tr("Survey releases"), tr("Publishers' headline figures, via TradingView's economic calendar"),
                  "economic-calendar.tradingview.com",
                  tr("Map: PMIs, business climate, economic and consumer sentiment, private payrolls over the last ten weeks; "
                     "each publisher below can be turned off")},
                 dataset(m_t.surveys, 6 * 3600, m_t.surveys->sourceUrl())));
    for (const SurveyHeadlines::Publisher &p : SurveyHeadlines::publishers()) {
        QWidget *card = buildRow({"survey_" + p.id, Kind::Publisher, p.name, tr("Headline figures free, series paid"), p.host, p.about});
        m_rows.last().publisher = p.id;
        add(card);
    }
    l->addSpacing(12);

    section(tr("News"));
    {
        // The News tab: the free feeds (the state is the last merged answer) and the optional key.
        D d;
        d.fetchedAt = [] { return news::Wire::instance()->state("rss").lastOk; };
        d.error = [] {
            const news::State s = news::Wire::instance()->state("rss");
            return s.lastError.isValid() && s.lastError > s.lastOk ? s.error : QString();
        };
        d.fetching = [] { return news::Wire::instance()->state("rss").pending > 0; };
        d.refresh = [] { news::Wire::instance()->forget(); };
        d.everySecs = 5 * 60;
        d.probeUrl = QUrl(news::Wire::feedUrls(news::Category::Stocks).first());
        add(buildRow({"news_feeds", Kind::Dataset, tr("News wire (free feeds)"),
                      tr("CNBC, MarketWatch, BBC, Nasdaq, Seeking Alpha, Investing.com, Cointelegraph, Decrypt, CoinDesk, FXStreet"),
                      "cnbc.com · nasdaq.com · coindesk.com · fxstreet.com…",
                      tr("News: the headlines of each category when no Financial Modeling Prep key is saved, and the ticker filter "
                         "(Yahoo Finance's and Nasdaq's feeds about one stock)")},
                     d));
    }
    {
        // The reader's ad blocker: Brave's engine and the lists Brave turns on by default.
        D d;
        d.fetchedAt = [] { return adblock::Blocker::instance()->fetchedAt(); };
        d.error = [] {
            if (!adblock::Blocker::available()) return tr("Built without the ad blocker (no Rust toolchain)");
            return adblock::Blocker::instance()->errorString();
        };
        d.fetching = [] { return adblock::Blocker::instance()->isFetching(); };
        d.refresh = [] { adblock::Blocker::instance()->start(true); };
        d.everySecs = 24 * 3600;
        d.probeUrl = QUrl("https://easylist.to/easylist/easylist.txt");
        add(buildRow({"adblock", Kind::Dataset, tr("Ad blocking filter lists"),
                      tr("Brave's default lists: uBlock Origin's, EasyList, EasyPrivacy, Brave's own, cookie notices"),
                      "easylist.to · raw.githubusercontent.com · secure.fanboy.co.nz…",
                      tr("News: the article reader blocks ads, trackers and cookie notices with Brave's engine (adblock-rust)")},
                     d));
    }
    add(buildRow({"fmp", Kind::Key, tr("Financial Modeling Prep"), tr("API key from financialmodelingprep.com (optional)"),
                  "financialmodelingprep.com",
                  tr("News: general, stock, crypto and forex news with their tickers, filtered by the server; the free feeds "
                     "otherwise")}));
    l->addSpacing(12);

    section(tr("AI"));
    add(buildRow({"anthropic", Kind::Key, tr("Anthropic (Claude)"), tr("API key from console.anthropic.com"), "api.anthropic.com",
                  tr("AI: the conversation with Claude (Messages API, streamed)")}));
    add(buildRow({"huggingface", Kind::Key, tr("Hugging Face Hub"), tr("Access token from huggingface.co › Settings › Tokens (optional)"),
                  "huggingface.co", tr("AI: model search and downloads; the token opens gated repositories")}));
    l->addSpacing(12);

    section(tr("Charts · market data"));
    add(buildRow({"binance", Kind::Market, "Binance", tr("Binance, its market data mirror, then Binance.US"), "api.binance.com",
                  tr("Charts: crypto candles and quotes, the crypto screener")}));
    add(buildRow({"coinbase", Kind::Market, "Coinbase", "Coinbase Exchange", "api.exchange.coinbase.com",
                  tr("Charts: crypto when Binance can't answer")}));
    add(buildRow({"kraken", Kind::Market, "Kraken", "Kraken", "api.kraken.com",
                  tr("Charts: crypto and forex as a last resort")}));
    add(buildRow({"yahoo", Kind::Market, "Yahoo Finance", "Yahoo", "query1.finance.yahoo.com",
                  tr("Charts: stocks worldwide, indices, futures, forex, intraday")}));
    add(buildRow({"nasdaq", Kind::Market, "Nasdaq", "Nasdaq", "api.nasdaq.com",
                  tr("Charts: US stocks, screener, heatmap, earnings, dividends, news, financials")}));
    add(buildRow({"calendar", Kind::Market, tr("Economic calendar"), "Forex Factory", "nfs.faireconomy.media",
                  tr("Charts: the Calendar tab")}));
    add(buildRow({"cryptonews", Kind::Market, tr("Crypto news"), "Cointelegraph, Decrypt", "cointelegraph.com · decrypt.co",
                  tr("Charts: the News tab for crypto symbols")}));
    l->addSpacing(12);

    section(tr("Charts · your own keys"));
    add(buildRow({"alpaca", Kind::Key, "Alpaca", tr("Free account at alpaca.markets"), "data.alpaca.markets",
                  tr("Charts: US stocks first, before the free public sources")}));
    add(buildRow({"polygon", Kind::Key, "Polygon (Massive)", tr("Account at massive.com"), "api.massive.com",
                  tr("Charts: US stocks and CME futures first")}));
    l->addSpacing(12);

    section(tr("On this computer"));
    add(buildRow({"bundled", Kind::Local, tr("Commodity sites, sea routes, ports, borders"), tr("Bundled with the app"),
                  "markets.json · searoutes.json · ports.json · countries.json",
                  tr("Map: production sites (a sample), routes and countries")}));
    add(buildRow({"imports", Kind::Local, tr("Your CSV imports"), tr("Files you imported"), "quant-terminal/data/imports",
                  tr("Charts: CSV:… symbols")}));
    l->addSpacing(12);

    buildCatalogueSections(l);
    l->addStretch(1);

    scroll->setWidget(content);
    m_stack->addWidget(scroll);
    m_flow = new SourceFlowView;
    m_stack->addWidget(m_flow);
    connect(m_flow, &SourceFlowView::showRows, this, &SourcesPage::showOnly);
    connect(m_flow, &SourceFlowView::showCatalogue, this, &SourcesPage::showCatalogueKind);
    connect(m_tabs, &QButtonGroup::idClicked, this, &SourcesPage::showTab);

    // Data sets and the AIS feed say when they change; the rest is read on the tick.
    auto changed = [this] { if (isVisible()) refresh(); };
    for (RemoteDataSet *d : std::initializer_list<RemoteDataSet *> {m_t.policyRates, m_t.cot, m_t.cotReport, m_t.economy, m_t.bop,
                                                                     m_t.trade, m_t.worldRates, m_t.sectors,
                                                                     m_t.propertyCredit, m_t.housePrices, m_t.globalDebt,
                                                                     m_t.bankCredit, m_t.futures, m_t.benchmarks,
                                                                     m_t.economicCalendar, m_t.corporateCalendar,
                                                                     m_t.marketSessions, m_t.oecd, m_t.eurostat, m_t.imfPrices,
                                                                     m_t.bls, m_t.brazil, m_t.argentina, m_t.southAfrica,
                                                                     m_t.singapore, m_t.fred, m_t.eia, m_t.bea, m_t.banxico,
                                                                     m_t.ecos, m_t.evds, m_t.surveys})
        connect(d, &RemoteDataSet::updated, this, changed);
    connect(m_t.fx, &FxData::updated, this, changed);
    connect(quant::StateRefresher::instance(), &quant::StateRefresher::changed, this, changed);
    connect(m_t.portWatch, &PortWatch::updated, this, changed);
    if (m_t.supply)
        connect(m_t.supply, &supply::SupplyChain::updated, this, changed);
    connect(m_t.ais, &AisStream::statusChanged, this, changed);
    m_tick.setInterval(1000);
    connect(&m_tick, &QTimer::timeout, this, [this] {
        if (++m_ticks % 5 == 0) refreshHealth();
        refresh();
    });
    loadKeys();
    refresh();
    connect(m_search, &QLineEdit::textChanged, this, &SourcesPage::applyFilter);
    connect(m_country, &QComboBox::currentIndexChanged, this, &SourcesPage::applyFilter);
    connect(m_access, &QComboBox::currentIndexChanged, this, &SourcesPage::applyFilter);
    applyFilter();
    showTab(std::clamp(QSettings().value("sources/tab", 0).toInt(), 0, 1));
}

void SourcesPage::showTab(int index)
{
    m_tabs->button(index)->setChecked(true);
    m_stack->setCurrentIndex(index);
    QSettings().setValue("sources/tab", index);
    if (index == 1) updateFlow();
}

void SourcesPage::showDisc(const QString &pin)
{
    showTab(1);
    m_flow->showDisc(pin);
}

void SourcesPage::updateFlow()
{
    // The list's own sources (not the catalogue of official bodies, nor the survey publishers' switches).
    QVector<sourceflow::Row> rows;
    for (const Row &r : std::as_const(m_rows)) {
        if (r.kind == Kind::Catalogue || r.kind == Kind::Publisher) continue;
        sourceflow::Row f;
        f.id = r.id;
        f.name = r.name;
        f.provider = r.provider;
        f.section = r.section;
        f.host = r.host;
        f.state = r.state == Row::Good ? sourceflow::Row::Good : r.state == Row::Bad ? sourceflow::Row::Bad
                  : r.state == Row::Off ? sourceflow::Row::Off : sourceflow::Row::Neutral;
        rows << f;
    }
    m_flow->setRows(rows);
}

void SourcesPage::showOnly(const QString &title, const QStringList &ids)
{
    m_only = QSet<QString>(ids.begin(), ids.end());
    m_onlyChip->setText(tr("Only %1 · %n data set(s)  ✕", nullptr, int(ids.size())).arg(title));
    m_onlyChip->setVisible(!m_only.isEmpty());
    if (!m_only.isEmpty()) {
        m_search->clear();
        showTab(0);
        m_listScroll->verticalScrollBar()->setValue(0);
    }
    applyFilter();
}

void SourcesPage::showCatalogueKind(int kind)
{
    m_only.clear();
    m_onlyChip->hide();
    m_search->clear();
    m_country->setCurrentIndex(0);
    m_access->setCurrentIndex(m_access->findData(-1));   // the diagram counts every access
    applyFilter();
    showTab(0);
    for (CatalogueSection &s : m_catalogue)
        if (s.kind == kind) {
            if (!s.header->isChecked()) s.header->setChecked(true);
            QPointer<QPushButton> header = s.header;
            QTimer::singleShot(0, this, [this, header] {
                if (header) m_listScroll->verticalScrollBar()->setValue(header->mapTo(m_listScroll->widget(), QPoint(0, 0)).y() - 12);
            });
        }
}

void SourcesPage::setSearch(const QString &text)
{
    if (m_search) m_search->setText(text);
}

QWidget *SourcesPage::buildRow(const Info &info)
{
    return buildRow(info, Dataset());
}

QWidget *SourcesPage::buildCatalogueRow(int source)
{
    const OfficialSources &catalogue = OfficialSources::instance();
    const OfficialSources::Source &s = catalogue.sources()[source];
    QUrl url(s.url);
    if (!s.url.isEmpty() && url.scheme().isEmpty())
        url = QUrl("https://" + s.url);
    const QString where = s.countries.size() == 1 ? OfficialSources::countryName(s.countries.first())
                                                  : tr("%n countries", nullptr, s.countries.size());
    QStringList titles;
    const QStringList slugs = catalogue.indicatorsOf(source);
    for (int i = 0; i < slugs.size() && i < 8; ++i)
        titles << OfficialSources::slugTitle(slugs[i]);
    if (slugs.size() > 8)
        titles << tr("and %1 more").arg(slugs.size() - 8);
    QString access = OfficialSources::accessName(s.access);
    if (s.api != "web" && !s.api.isEmpty())
        access += " · " + s.api;
    Info info;
    info.id = QString("cat:%1").arg(source);
    info.kind = Kind::Catalogue;
    info.name = s.name;
    info.provider = OfficialSources::kindName(s.kind) + " · " + where;
    info.host = url.host().isEmpty() ? tr("no site known") : url.host();
    info.feeds = tr("%n indicator(s): %1", nullptr, slugs.size()).arg(titles.join(", ")) + "\n" + access;
    Dataset d;
    d.probeUrl = url;
    QWidget *card = buildRow(info, d);
    m_rows.last().catalogue = source;
    m_rows.last().search += ' ' + slugs.join(' ').replace('-', ' ').toLower() + ' ' + s.countries.join(' ').toLower();
    return card;
}

void SourcesPage::buildCatalogueSections(QVBoxLayout *l)
{
    // One collapsed section per kind of body, opened on click; the summary in each header follows the filters.
    const OfficialSources::Kind kinds[] = {OfficialSources::Kind::Statistics, OfficialSources::Kind::CentralBank,
                                           OfficialSources::Kind::Government, OfficialSources::Kind::International,
                                           OfficialSources::Kind::Private};
    m_catalogueIntro << sectionLabel(tr("Official statistics · every source"));
    l->addWidget(m_catalogueIntro.last());
    auto *hint = dimLabel(tr("The bodies behind each country's figures, from the statistical offices and central banks to "
                             "the private institutes: what each one publishes, whether its data is free and how a program "
                             "reads it, and whether its site answered the last check. Open a group, or search above. The "
                             "compilations above (OECD, Eurostat, IMF, BLS, World Bank, BIS) read these bodies for the map."));
    hint->setWordWrap(true);
    l->addWidget(hint);
    m_catalogueIntro << hint;
    for (OfficialSources::Kind kind : kinds) {
        CatalogueSection section;
        section.kind = int(kind);
        section.header = new QPushButton;
        section.header->setObjectName("catalogueHeader");
        section.header->setCursor(Qt::PointingHandCursor);
        section.header->setCheckable(true);
        section.header->setFlat(true);
        l->addWidget(section.header);
        section.body = new QWidget;
        section.layout = new QVBoxLayout(section.body);
        section.layout->setContentsMargins(0, 0, 0, 0);
        section.layout->setSpacing(6);
        section.more = new QPushButton(tr("Show more"));
        section.more->setObjectName("segButton");
        section.more->setCursor(Qt::PointingHandCursor);
        section.layout->addWidget(section.more, 0, Qt::AlignLeft);
        section.body->setVisible(false);
        l->addWidget(section.body);
        const int index = m_catalogue.size();
        connect(section.header, &QPushButton::toggled, this, [this, index](bool on) {
            CatalogueSection &s = m_catalogue[index];
            s.expanded = on;
            if (on) fillCatalogueSection(s, 100);
            s.body->setVisible(on && !s.matches.isEmpty());
            s.header->setText(s.header->text().replace(0, 1, on ? QString::fromUtf8("▾") : QString::fromUtf8("▸")));
        });
        connect(section.more, &QPushButton::clicked, this, [this, index] {
            CatalogueSection &s = m_catalogue[index];
            fillCatalogueSection(s, s.shown + 100);
        });
        m_catalogue.push_back(section);
    }
}

void SourcesPage::fillCatalogueSection(CatalogueSection &section, int limit)
{
    // Lays out the cards of the first `limit` matches, building the ones that don't exist yet, in catalogue order.
    for (int i = section.layout->count() - 1; i >= 0; --i)
        if (QWidget *w = section.layout->itemAt(i)->widget(); w && w != section.more) {
            section.layout->removeWidget(w);
            w->hide();
        }
    const int n = std::min(limit, int(section.matches.size()));
    for (int i = 0; i < n; ++i) {
        const int source = section.matches[i];
        const QString id = QString("cat:%1").arg(source);
        QWidget *card = nullptr;
        if (const auto it = m_index.constFind(id); it != m_index.constEnd()) {
            card = m_rows[it.value()].card;
        } else {
            card = buildCatalogueRow(source);
            updateRow(m_rows.last());
        }
        section.layout->insertWidget(i, card);
        card->show();
    }
    section.shown = n;
    section.more->setVisible(n < section.matches.size());
    section.more->setText(tr("Show %1 more").arg(std::min(100, int(section.matches.size()) - n)));
}

void SourcesPage::applyFilter()
{
    const QString text = m_search ? m_search->text().trimmed().toLower() : QString();
    const QString country = m_country ? m_country->currentData().toString() : QString();
    const int access = m_access ? m_access->currentData().toInt() : -1;
    const QStringList words = text.split(' ', Qt::SkipEmptyParts);
    auto matches = [&](const QString &haystack) {
        for (const QString &w : words)
            if (!haystack.contains(w)) return false;
        return true;
    };
    // The app's own sources: hidden by the search only (they aren't about one country).
    for (Section &s : m_sections) {
        bool any = false;
        for (const QString &id : s.rowIds) {
            Row &r = m_rows[m_index.value(id)];
            const bool shown = matches(r.search) && (m_only.isEmpty() || m_only.contains(r.id));
            if (r.card) r.card->setVisible(shown);
            any = any || shown;
        }
        s.label->setVisible(any);
    }
    // The catalogue: its sections' matches, counts and cards.
    const OfficialSources &catalogue = OfficialSources::instance();
    const QVector<OfficialSources::Source> &sources = catalogue.sources();
    const bool filtering = !words.isEmpty() || !country.isEmpty();
    for (QWidget *w : std::as_const(m_catalogueIntro)) w->setVisible(m_only.isEmpty());
    for (CatalogueSection &s : m_catalogue) {
        if (!m_only.isEmpty()) {   // the diagram's data sets only
            s.header->hide();
            s.body->hide();
            continue;
        }
        s.matches.clear();
        int reachable = 0;
        for (int i = 0; i < sources.size(); ++i) {
            const OfficialSources::Source &src = sources[i];
            if (int(src.kind) != s.kind) continue;
            if (access >= 0 && int(src.access) != access) continue;
            if (!country.isEmpty() && !src.countries.contains(country)) continue;
            if (!words.isEmpty()) {
                const auto it = m_index.constFind(QString("cat:%1").arg(i));
                const QString haystack = it != m_index.constEnd()
                                             ? m_rows[it.value()].search
                                             : (src.name + ' ' + src.url + ' ' + src.api + ' ' + src.countries.join(' ') + ' '
                                                + catalogue.indicatorsOf(i).join(' ').replace('-', ' ')).toLower();
                if (!matches(haystack)) continue;
            }
            s.matches.push_back(i);
            reachable += src.testOk;
        }
        // A search or a country opens the groups it leaves small enough to read.
        if (filtering && !s.expanded && s.matches.size() <= 40 && !s.matches.isEmpty()) {
            s.expanded = true;
            s.header->blockSignals(true);
            s.header->setChecked(true);
            s.header->blockSignals(false);
        }
        const QString arrow = s.expanded ? QString::fromUtf8("▾") : QString::fromUtf8("▸");
        s.header->setText(QString("%1  %2  ·  %3").arg(arrow, OfficialSources::kindName(OfficialSources::Kind(s.kind)).toUpper(),
                                                       tr("%n source(s)", nullptr, s.matches.size()))
                          + (s.matches.isEmpty() ? QString() : "  ·  " + tr("%1 with a reachable site").arg(reachable)));
        s.header->setVisible(!s.matches.isEmpty() || !filtering);
        if (s.expanded)
            fillCatalogueSection(s, std::max(100, s.shown));
        s.body->setVisible(s.expanded && !s.matches.isEmpty());
    }
}

QWidget *SourcesPage::buildRow(const Info &info, const Dataset &dataset)
{
    Row row;
    row.id = info.id;
    row.kind = info.kind;
    row.dataset = dataset;
    row.search = (info.name + ' ' + info.provider + ' ' + info.host + ' ' + info.feeds).toLower();
    row.name = info.name;
    row.provider = info.provider;
    row.host = info.host;

    auto *card = new QFrame;
    card->setObjectName("sourceCard");
    row.card = card;
    auto *v = new QVBoxLayout(card);
    v->setContentsMargins(16, 12, 16, 12);
    v->setSpacing(10);
    auto *h = new QHBoxLayout;
    h->setSpacing(14);
    v->addLayout(h);

    row.dot = new QLabel(QString::fromUtf8("●"));
    row.dot->setFixedWidth(12);
    h->addWidget(row.dot, 0, Qt::AlignTop);

    auto *about = new QVBoxLayout;
    about->setSpacing(3);
    auto *name = new QLabel(info.name);
    name->setObjectName("sourceName");
    about->addWidget(name);
    auto *meta = dimLabel(QString("%1  ·  %2").arg(info.provider, info.host));
    meta->setObjectName("sourceMeta");
    meta->setTextInteractionFlags(Qt::TextSelectableByMouse);
    meta->setWordWrap(true);   // the news feeds' and the ad block lists' lines would widen the whole page
    about->addWidget(meta);
    auto *feeds = new QLabel(QString::fromUtf8("→  ") + info.feeds);
    feeds->setObjectName("sourceFeeds");
    feeds->setWordWrap(true);
    about->addWidget(feeds);
    h->addLayout(about, 1);

    auto *state = new QVBoxLayout;
    state->setSpacing(3);
    row.status = new QLabel;
    row.status->setObjectName("sourceStatus");
    row.status->setWordWrap(true);
    row.detail = dimLabel();
    row.detail->setObjectName("sourceMeta");
    row.detail->setWordWrap(true);
    state->addWidget(row.status);
    state->addWidget(row.detail);
    state->addStretch(1);
    auto *stateBox = new QWidget;
    stateBox->setFixedWidth(330);
    stateBox->setLayout(state);
    state->setContentsMargins(0, 0, 0, 0);
    h->addWidget(stateBox);

    auto *actions = new QWidget;
    actions->setFixedWidth(ActionsWidth);
    auto *a = new QHBoxLayout(actions);
    a->setContentsMargins(0, 0, 0, 0);
    a->setSpacing(6);
    a->addStretch(1);
    const QString id = info.id;
    if (info.kind == Kind::Catalogue) {
        auto *open = new QPushButton(tr("Open the site"));
        open->setObjectName("segButton");
        open->setCursor(Qt::PointingHandCursor);
        const QUrl url = dataset.probeUrl;
        open->setEnabled(url.isValid() && !url.host().isEmpty());
        connect(open, &QPushButton::clicked, this, [url] { QDesktopServices::openUrl(url); });
        a->addWidget(open);
    }
    if (info.kind == Kind::Dataset || info.kind == Kind::Market || info.kind == Kind::Key || info.kind == Kind::Catalogue) {
        row.test = new QPushButton(tr("Test"));
        row.test->setObjectName("segButton");
        row.test->setCursor(Qt::PointingHandCursor);
        row.test->setToolTip(tr("Ask the source for a little data now and time the answer"));
        connect(row.test, &QPushButton::clicked, this, [this, id] { runTest(m_rows[m_index.value(id)]); });
        a->addWidget(row.test);
    }
    if (info.kind == Kind::Dataset) {
        row.refresh = new QPushButton(tr("Refresh"));
        row.refresh->setObjectName("segButton");
        row.refresh->setCursor(Qt::PointingHandCursor);
        row.refresh->setToolTip(tr("Download the data again now"));
        connect(row.refresh, &QPushButton::clicked, this, [this, id] {
            Row &r = m_rows[m_index.value(id)];
            r.dataset.refresh();
            updateRow(r);
        });
        a->addWidget(row.refresh);
    }
    if (info.kind == Kind::Stream) {
        row.refresh = new QPushButton(tr("Reconnect"));
        row.refresh->setObjectName("segButton");
        row.refresh->setCursor(Qt::PointingHandCursor);
        connect(row.refresh, &QPushButton::clicked, this, [this] { m_t.ais->setApiKey(m_t.ais->apiKey()); });
        a->addWidget(row.refresh);
    }
    if (info.kind != Kind::Local && info.kind != Kind::Catalogue) {
        row.toggle = new SourceSwitch;
        row.toggle->setChecked(isEnabled(id));
        row.toggle->setToolTip(tr("Turn this source on or off"));
        connect(row.toggle, &QAbstractButton::toggled, this, [this, id](bool on) { setEnabled(id, on); });
        a->addSpacing(6);
        a->addWidget(row.toggle);
    }
    h->addWidget(actions, 0, Qt::AlignTop);

    // API keys
    if (info.kind == Kind::Stream || info.kind == Kind::Key || (info.kind == Kind::Dataset && !dataset.keyId.isEmpty())) {
        row.keyBox = new QWidget;
        auto *k = new QHBoxLayout(row.keyBox);
        k->setContentsMargins(26, 0, 0, 0);
        k->setSpacing(8);
        auto edit = [](const QString &placeholder) {
            auto *e = new QLineEdit;
            e->setObjectName("keyEdit");
            e->setEchoMode(QLineEdit::Password);
            e->setClearButtonEnabled(true);
            e->setPlaceholderText(placeholder);
            return e;
        };
        if (info.id == "ais") {
            row.key = edit(tr("aisstream.io API key"));
            row.key->setText(m_t.ais->apiKey());
            row.key->setEnabled(!m_t.aisKeyFromEnv);
            if (m_t.aisKeyFromEnv) row.key->setToolTip(tr("The key is set by the AISSTREAM_API_KEY environment variable."));
        } else if (info.id == "alpaca") {
            row.key = edit(tr("Key ID"));
            row.secret = edit(tr("Secret key"));
            row.feed = new QComboBox;
            row.feed->setObjectName("sourceCombo");
            row.feed->addItem(tr("IEX feed (free)"), "iex");
            row.feed->addItem(tr("SIP feed (paid)"), "sip");
        } else if (!dataset.keyId.isEmpty()) {
            row.key = edit(dataset.keyHint.isEmpty() ? tr("API key") : dataset.keyHint);
            row.key->setText(sourceKey(dataset.keyId));
        } else if (settingsKeyRow(info.id)) {
            row.key = edit(info.id == "anthropic" ? tr("sk-ant-…") : info.id == "huggingface" ? tr("hf_…") : tr("API key"));
            row.key->setText(sourceKey(info.id));
        } else {
            row.key = edit(tr("API key"));
        }
        k->addWidget(row.key, 1);
        if (row.secret) k->addWidget(row.secret, 1);
        if (row.feed) k->addWidget(row.feed);
        row.save = new QPushButton(info.id == "ais" ? tr("Connect") : tr("Save"));
        row.save->setObjectName("primaryButton");
        row.save->setCursor(Qt::PointingHandCursor);
        row.save->setEnabled(!(info.id == "ais" && m_t.aisKeyFromEnv));
        connect(row.save, &QPushButton::clicked, this, [this, id] { saveKeys(m_rows[m_index.value(id)]); });
        connect(row.key, &QLineEdit::returnPressed, row.save, &QPushButton::click);
        if (row.secret) connect(row.secret, &QLineEdit::returnPressed, row.save, &QPushButton::click);
        k->addWidget(row.save);
        k->addSpacing(ActionsWidth + 14 - 8);
        v->addWidget(row.keyBox);
    }

    m_index.insert(info.id, int(m_rows.size()));
    m_rows.push_back(row);
    return card;
}

void SourcesPage::setEnabled(const QString &id, bool on)
{
    QStringList off = disabledIds();
    off.removeAll(id);
    if (!on) off << id;
    off.sort();
    QSettings().setValue(DisabledKey, off);
    applySaved(m_t);
    if (m_index.contains(id)) m_rows[m_index.value(id)].test_ = {};
    refresh();
}

void SourcesPage::loadKeys()
{
    if (!m_t.backend) return;
    m_feedConfig = m_t.backend->feedConfig();
    for (const QString id : {"alpaca", "polygon"}) {
        if (!m_index.contains(id)) continue;
        Row &r = m_rows[m_index.value(id)];
        if (id == "alpaca") {
            const QJsonObject a = m_feedConfig.value("alpaca").toObject();
            r.key->setText(a.value("keyId").toString());
            r.secret->setText(a.value("secretKey").toString());
            r.feed->setCurrentIndex(a.value("feed").toString() == "sip" ? 1 : 0);
        } else {
            const QJsonObject p = m_feedConfig.value("polygon").toObject().isEmpty() ? m_feedConfig.value("massive").toObject()
                                                                                       : m_feedConfig.value("polygon").toObject();
            r.key->setText(p.value("apiKey").toString());
        }
    }
}

void SourcesPage::saveKeys(Row &row)
{
    if (row.id == "ais") {
        emit aisKeyChanged(row.key->text().trimmed());
        refresh();
        return;
    }
    if (row.kind == Kind::Key && settingsKeyRow(row.id)) {
        setSourceKey(row.id, row.key->text());
        if (row.id == "fmp") news::Wire::instance()->forget();
        row.test_ = {};
        if (!row.key->text().trimmed().isEmpty() && isEnabled(row.id)) runTest(row);
        refresh();
        return;
    }
    if (row.kind == Kind::Dataset && !row.dataset.keyId.isEmpty()) {
        setSourceKey(row.dataset.keyId, row.key->text());
        row.test_ = {};
        if (!row.key->text().trimmed().isEmpty() && isEnabled(row.id)) {
            row.dataset.refresh();
            runTest(row);
        }
        refresh();
        return;
    }
    if (!m_t.backend) return;
    QJsonObject cfg = m_t.backend->feedConfig();   // keeps whatever else the file holds
    if (row.id == "alpaca") {
        const QString key = row.key->text().trimmed(), secret = row.secret->text().trimmed();
        if (key.isEmpty() && secret.isEmpty()) {
            cfg.remove("alpaca");
        } else {
            QJsonObject a = cfg.value("alpaca").toObject();
            a.insert("keyId", key);
            a.insert("secretKey", secret);
            a.insert("feed", row.feed->currentData().toString());
            cfg.insert("alpaca", a);
        }
    } else {
        const QString key = row.key->text().trimmed();
        cfg.remove("massive");
        if (key.isEmpty()) {
            cfg.remove("polygon");
        } else {
            QJsonObject p = cfg.value("polygon").toObject();
            p.insert("apiKey", key);
            cfg.insert("polygon", p);
        }
    }
    row.test_ = {};
    if (!m_t.backend->setFeedConfig(cfg)) {
        row.test_.done = true;
        row.test_.error = tr("The keys couldn't be saved in %1").arg("feeds.json");
    }
    loadKeys();
    const bool hasKey = row.id == "alpaca" ? !m_feedConfig.value("alpaca").toObject().isEmpty()
                                           : !m_feedConfig.value("polygon").toObject().isEmpty();
    if (hasKey && isEnabled(row.id)) runTest(row);
    refresh();
}

void SourcesPage::runTest(Row &row)
{
    if (row.test_.running) return;
    row.test_ = {};
    row.test_.running = true;
    updateRow(row);
    const QString id = row.id;
    QPointer<SourcesPage> self(this);
    auto finish = [self, id](bool ok, double ms, const QString &error) {
        if (!self || !self->m_index.contains(id)) return;
        Row &r = self->m_rows[self->m_index.value(id)];
        r.test_.running = false;
        r.test_.done = true;
        r.test_.ok = ok;
        r.test_.ms = ms;
        r.test_.error = error;
        r.test_.at = QDateTime::currentDateTimeUtc();
        self->updateRow(r);
        self->updateSummary();
    };
    if (row.kind == Kind::Key && settingsKeyRow(row.id)) {
        const QString key = sourceKey(row.id);
        if (key.isEmpty()) {
            finish(false, 0, tr("No key"));
            return;
        }
        QNetworkRequest req(row.id == "anthropic"   ? QUrl("https://api.anthropic.com/v1/models?limit=1")
                            : row.id == "huggingface" ? QUrl("https://huggingface.co/api/whoami-v2")
                                                      : QUrl("https://financialmodelingprep.com/stable/news/general-latest?page=0&limit=1&apikey="
                                                             + QString::fromUtf8(QUrl::toPercentEncoding(key))));
        req.setTransferTimeout(20000);
        req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Deepwater/0.1"));
        if (row.id == "anthropic") {
            req.setRawHeader("x-api-key", key.toUtf8());
            req.setRawHeader("anthropic-version", "2026-10-21");
        } else if (row.id == "huggingface") {
            req.setRawHeader("Authorization", "Bearer " + key.toUtf8());
        }
        auto *timer = new QElapsedTimer;
        timer->start();
        QNetworkReply *reply = m_net.get(req);
        connect(reply, &QNetworkReply::finished, this, [reply, timer, finish] {
            reply->deleteLater();
            const double ms = timer->nsecsElapsed() / 1e6;
            delete timer;
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (reply->error() == QNetworkReply::NoError) finish(true, ms, QString());
            else finish(false, ms, status == 401 || status == 403 ? tr("The key was refused (HTTP %1)").arg(status)
                                                                   : status ? tr("HTTP %1").arg(status) : reply->errorString());
        });
        return;
    }
    if (row.kind == Kind::Dataset || row.kind == Kind::Catalogue) {
        if (row.kind == Kind::Dataset && !isEnabled(id)) {
            finish(false, 0, tr("Turned off"));
            return;
        }
        if (row.kind == Kind::Catalogue && row.dataset.probeUrl.host().isEmpty()) {
            finish(false, 0, tr("No site known"));
            return;
        }
        // The data set's own request, dropped as soon as the server answers: the time to the first
        // byte, without downloading megabytes.
        QNetworkRequest req(row.dataset.probe ? row.dataset.probe() : row.dataset.probeUrl);
        req.setTransferTimeout(20000);
        req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Deepwater/0.1"));
        req.setRawHeader("Accept-Language", "en");   // the OECD server fails now and then on Qt's default
        if (req.url().host().endsWith("finance.yahoo.com"))   // Yahoo answers 429 to most user agents
            req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Mozilla/5.0"));
        if (req.url().host() == "economic-calendar.tradingview.com")   // it wants a TradingView origin
            req.setRawHeader("Origin", "https://www.tradingview.com");
        if (req.url().host() == "api.nasdaq.com") {   // and Nasdaq a browser and its own site
            req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Mozilla/5.0"));
            req.setRawHeader("Origin", "https://www.nasdaq.com");
            req.setRawHeader("Referer", "https://www.nasdaq.com/");
        }
        auto *timer = new QElapsedTimer;
        timer->start();
        QNetworkReply *reply = m_net.get(req);
        auto done = std::make_shared<bool>(false);
        auto onHeaders = [reply, timer, finish, done] {
            if (*done) return;
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (!status) return;
            *done = true;
            const double ms = double(timer->elapsed());
            if (status >= 200 && status < 300) finish(true, ms, {});
            else finish(false, ms, tr("HTTP %1").arg(status));
            reply->abort();
        };
        connect(reply, &QNetworkReply::metaDataChanged, this, onHeaders);
        connect(reply, &QNetworkReply::finished, this, [reply, timer, finish, done] {
            reply->deleteLater();
            if (!*done) {
                *done = true;
                finish(false, 0, reply->errorString());
            }
            delete timer;
        });
        return;
    }
    // Charts sources: the backend's own check, through its fallbacks' code (off the UI thread).
    qtm::api::run(
        this, [id]() -> QJsonObject {
            qtm::Backend *b = qtm::Backend::instance();
            return b ? b->probeSource(id) : QJsonObject {{"ok", false}, {"error", "The Charts backend isn't running"}};
        },
        [finish](const QJsonObject &r) { finish(r.value("ok").toBool(), r.value("ms").toDouble(), r.value("error").toString()); },
        [finish](const QString &e) { finish(false, 0, e); });
}

void SourcesPage::showEvent(QShowEvent *e)
{
    loadKeys();
    refreshHealth();
    refresh();
    m_tick.start();
    QWidget::showEvent(e);
}

void SourcesPage::hideEvent(QHideEvent *e)
{
    m_tick.stop();
    QWidget::hideEvent(e);
}

void SourcesPage::refreshHealth()
{
    QPointer<SourcesPage> self(this);
    qtm::api::get(this, "/api/health", [self](const QJsonValue &v) {
        if (!self) return;
        self->m_health = v.toObject().value("providers").toObject();
        self->m_healthLoaded = true;
        self->refresh();
    });
    qtm::api::get(this, "/api/imports", [self](const QJsonValue &v) {
        if (!self) return;
        self->m_imports = int(v.toArray().size());
    });
}

QString SourcesPage::age(const QDateTime &t) const
{
    const qint64 s = t.secsTo(QDateTime::currentDateTimeUtc());
    if (s < 60) return tr("just now");
    if (s < 3600) return tr("%1 min ago").arg(s / 60);
    if (s < 48 * 3600) return tr("%1 h ago").arg(s / 3600);
    return tr("%1 days ago").arg(s / 86400);
}

QString SourcesPage::every(qint64 secs) const
{
    return secs >= 48 * 3600 ? tr("checked every %1 days").arg(secs / 86400) : tr("checked every %1 h").arg(secs / 3600);
}

void SourcesPage::refresh()
{
    const QStringList disabled = disabledIds();
    for (Row &r : m_rows)
        if (r.kind != Kind::Catalogue)   // those only change when tested, and there can be a thousand of them
            updateRow(r, disabled);
    updateSummary();
    if (m_flow && m_stack->currentIndex() == 1) updateFlow();
}

void SourcesPage::updateRow(Row &r)
{
    updateRow(r, disabledIds());
}

void SourcesPage::updateRow(Row &r, const QStringList &disabled)
{
    const Palette &ui = Theme::current();
    const bool on = !disabled.contains(r.id);
    QString status, detail;
    QColor color = ui.TextDim;
    bool testable = on;

    switch (r.kind) {
    case Kind::Dataset: {
        const QDateTime at = r.dataset.fetchedAt();
        const QString error = r.dataset.error();
        const bool needsKey = !r.dataset.keyId.isEmpty() && sourceKey(r.dataset.keyId).isEmpty() && r.id != "comtrade";
        if (needsKey) {
            status = tr("No key");
            color = ui.TextFaint;
            detail = tr("Enter a free key below to turn this source on");
            testable = false;
        } else if (!on) {
            status = tr("Off");
            color = ui.TextFaint;
            detail = at.isValid() ? tr("Using the copy saved %1").arg(age(at)) : tr("No copy saved on this computer");
        } else if (r.dataset.fetching()) {
            status = tr("Downloading…");
            detail = at.isValid() ? tr("Last copy %1").arg(age(at)) : QString();
        } else if (!error.isEmpty()) {
            status = tr("Error: %1").arg(error);
            color = ui.Down;
            detail = at.isValid() ? tr("Using the copy saved %1 · retrying in a few minutes").arg(age(at))
                                  : tr("No data yet · retrying in a few minutes");
        } else if (at.isValid()) {
            const bool fresh = at.secsTo(QDateTime::currentDateTimeUtc()) <= 2 * r.dataset.everySecs;
            status = fresh ? tr("Up to date") : tr("Out of date");
            color = fresh ? ui.Up : ui.TextDim;
            detail = tr("Updated %1 · %2").arg(age(at), every(r.dataset.everySecs));
        } else {
            status = tr("Waiting for the first download");
        }
        r.refresh->setEnabled(on && !r.dataset.fetching());
        break;
    }
    case Kind::Stream: {
        const int ships = int(m_t.vessels->vessels().size());
        if (!on) {
            status = tr("Off");
            color = ui.TextFaint;
            detail = tr("No ships on the map while the feed is off");
        } else if (m_t.ais->apiKey().isEmpty()) {
            status = tr("No API key");
            detail = tr("Create a free account on aisstream.io and paste its key below");
        } else {
            switch (m_t.ais->status()) {
            case AisStream::Status::Live:
                status = tr("Live");
                color = ui.Up;
                detail = tr("%1 ships tracked").arg(QLocale().toString(ships));
                break;
            case AisStream::Status::Connecting:
                status = tr("Connecting…");
                break;
            case AisStream::Status::Error:
                status = tr("Error: %1").arg(m_t.ais->errorString());
                color = ui.Down;
                detail = tr("Reconnecting on its own");
                break;
            case AisStream::Status::Off:
                status = tr("Not connected");
                break;
            }
        }
        if (m_t.aisKeyFromEnv) detail += (detail.isEmpty() ? "" : " · ") + tr("key from AISSTREAM_API_KEY");
        r.refresh->setEnabled(on && !m_t.ais->apiKey().isEmpty());
        break;
    }
    case Kind::Market: {
        const QJsonObject h = m_health.value(r.id).toObject();
        if (!on) {
            status = tr("Off");
            color = ui.TextFaint;
            detail = tr("The charts use the next source in line");
        } else if (!h.isEmpty() && h.value("ok").toBool()) {
            status = tr("Working");
            color = ui.Up;
            detail = tr("Last answer %1").arg(age(QDateTime::fromString(h.value("at").toString(), Qt::ISODateWithMs)));
        } else if (!h.isEmpty()) {
            status = tr("Failing: %1").arg(h.value("error").toString());
            color = ui.Down;
            detail = tr("The next source in line takes over");
        } else {
            status = m_healthLoaded ? tr("Not used yet in this session") : QString();
        }
        break;
    }
    case Kind::Key: {
        const bool settingsKey = settingsKeyRow(r.id);
        const bool hasKey = settingsKey ? !sourceKey(r.id).isEmpty()
                            : r.id == "alpaca" ? !m_feedConfig.value("alpaca").toObject().value("keyId").toString().isEmpty()
                                               : !m_feedConfig.value("polygon").toObject().value("apiKey").toString().isEmpty()
                                                     || !m_feedConfig.value("massive").toObject().value("apiKey").toString().isEmpty();
        if (!hasKey) {
            status = tr("No key");
            detail = r.id == "anthropic"     ? tr("Needed to talk to Claude in the AI tab")
                     : r.id == "huggingface" ? tr("Optional: public repositories download without it")
                     : r.id == "fmp"         ? tr("Optional: the News tab reads the free feeds without it")
                                             : tr("Optional: the free public sources are used without it");
        } else if (!on) {
            status = tr("Off");
            color = ui.TextFaint;
            detail = tr("Key kept, not used");
        } else {
            status = tr("Key saved");
            color = ui.Accent;
            detail = r.id == "fmp" ? tr("Used by the News tab") : settingsKey ? tr("Used by the AI tab") : tr("Charts ask it first");
        }
        testable = hasKey && on;
        break;
    }
    case Kind::Publisher: {
        const int n = m_t.surveys ? m_t.surveys->figureCount(r.publisher) : 0;
        if (!on) {
            status = tr("Off");
            color = ui.TextFaint;
            detail = tr("Its figures are left out of the country sheets");
        } else if (!m_t.surveys || !m_t.surveys->hasData()) {
            status = tr("Waiting for the survey releases");
        } else if (n > 0) {
            status = tr("%n figure(s) in the last ten weeks", nullptr, n);
            color = ui.Up;
        } else {
            status = tr("No release in the last ten weeks");
        }
        break;
    }
    case Kind::Catalogue: {
        const OfficialSources::Source &s = OfficialSources::instance().sources()[r.catalogue];
        testable = !r.dataset.probeUrl.host().isEmpty();
        if (!testable) {
            status = tr("No site known");
            color = ui.TextFaint;
        } else if (s.testDate.isEmpty()) {
            status = tr("Not checked yet");
        } else if (s.testOk) {
            status = tr("Site reachable");
            color = ui.Up;
            detail = tr("Answered in %1 ms (HTTP %2) · checked %3").arg(s.testMs).arg(s.testStatus).arg(s.testDate);
        } else {
            status = tr("Site unreachable");
            color = ui.Down;
            const QString why = s.testStatus      ? tr("HTTP %1").arg(s.testStatus)
                                : s.testError == "dns"     ? tr("unknown host")
                                : s.testError == "tls"     ? tr("TLS error")
                                : s.testError == "timeout" ? tr("no answer")
                                : s.testError == "refused" ? tr("connection refused")
                                                           : tr("network error");
            detail = tr("%1 · checked %2").arg(why, s.testDate);
            if (s.testStatus == 401 || s.testStatus == 403)
                detail += " · " + tr("the site refuses a client that isn't a browser");
        }
        break;
    }
    case Kind::Local:
        if (r.id == "imports") {
            status = m_imports > 0 ? tr("Available") : m_imports == 0 ? tr("None yet") : QString();
            color = m_imports > 0 ? ui.Up : ui.TextDim;
            detail = m_imports > 0 ? tr("%1 files").arg(m_imports) : QString();
        } else {
            status = tr("Available");
            color = ui.Up;
            detail = tr("Part of the app, no network needed");
        }
        break;
    }

    // A test result, while it is recent, says more than the passive state.
    if (r.test_.running) {
        detail = tr("Testing…");
    } else if (r.test_.done && on) {
        if (r.test_.ok) {
            detail = tr("Test: answered in %1 ms · %2").arg(qRound(r.test_.ms)).arg(age(r.test_.at));
            if (r.kind == Kind::Market && m_health.value(r.id).toObject().isEmpty()) {
                status = tr("Working");
                color = ui.Up;
            }
            if (r.kind == Kind::Catalogue) {
                status = tr("Site reachable");
                color = ui.Up;
            }
        } else {
            detail = tr("Test failed: %1").arg(r.test_.error);
            color = ui.Down;
            if (r.kind == Kind::Market || r.kind == Kind::Key) status = tr("Failing");
            if (r.kind == Kind::Catalogue) status = tr("Site unreachable");
        }
    }
    if (r.test) r.test->setEnabled(testable && !r.test_.running);
    r.state = !on && r.kind != Kind::Local ? Row::Off : color == ui.Down ? Row::Bad
              : color == ui.Up || color == ui.Accent ? Row::Good : Row::Neutral;

    r.status->setText(status);
    if (color != r.color) {   // the page refreshes every second; the colour rarely changes
        r.color = color;
        r.status->setStyleSheet(QString("color: %1;").arg(color.name()));
        r.dot->setStyleSheet(QString("color: %1;").arg(color.name()));
    }
    r.detail->setText(detail);
    r.detail->setVisible(!detail.isEmpty());
    r.status->setToolTip(status.size() > 60 ? status : QString());
}

void SourcesPage::updateSummary()
{
    const Palette &ui = Theme::current();
    int total = 0, ok = 0, failing = 0, off = 0;
    for (const Row &r : std::as_const(m_rows)) {
        if (r.kind == Kind::Local || r.kind == Kind::Catalogue || r.kind == Kind::Publisher) continue;
        ++total;
        ok += r.state == Row::Good;
        failing += r.state == Row::Bad;
        off += r.state == Row::Off;
    }
    auto chip = [](const QColor &c, const QString &text) {
        return QString("<span style='color:%1'>●</span>&nbsp;%2").arg(c.name(), text.toHtmlEscaped());
    };
    QStringList parts;
    parts << tr("%1 online sources").arg(total);
    parts << chip(ui.Up, tr("%1 working").arg(ok));
    parts << chip(ui.Down, tr("%1 failing").arg(failing));
    parts << chip(ui.TextFaint, tr("%1 off").arg(off));
    {
        int freeSources = 0;
        for (const OfficialSources::Source &s : OfficialSources::instance().sources())
            freeSources += s.access == OfficialSources::Access::Free;
        parts << tr("%1 official sources listed, %2 free with a reachable site")
                     .arg(freeSources).arg(OfficialSources::instance().reachableFreeCount());
    }
    m_summary->setText(parts.join("&nbsp;&nbsp;&nbsp;&nbsp;"));
}
