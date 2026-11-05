#include "SourceFlow.h"
#include "CountryData.h"
#include "LocalTools.h"
#include "McpServer.h"
#include "ModelSlots.h"
#include "OfficialSources.h"
#include "Theme.h"

#include <QCoreApplication>
#include <QEvent>
#include <QDateTime>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLocale>
#include <QMap>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPathStroker>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QVariantAnimation>
#include <QSet>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <functional>

namespace sourceflow {

namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("SourceFlowView", text);
}

// The groups of sources, in the order they are drawn.
const char *const Groups[] = {
    QT_TRANSLATE_NOOP("SourceFlowView", "International bodies"),
    QT_TRANSLATE_NOOP("SourceFlowView", "United States"),
    QT_TRANSLATE_NOOP("SourceFlowView", "National offices and central banks"),
    QT_TRANSLATE_NOOP("SourceFlowView", "Markets and data companies"),
    QT_TRANSLATE_NOOP("SourceFlowView", "Web, news and AI"),
    QT_TRANSLATE_NOOP("SourceFlowView", "On this computer"),
    QT_TRANSLATE_NOOP("SourceFlowView", "Other sources"),
};
constexpr int OtherGroup = 6;

struct SourceDef {
    const char *id;
    const char *name;
    int group;
};
const SourceDef Sources[] = {
    {"imf", QT_TRANSLATE_NOOP("SourceFlowView", "International Monetary Fund"), 0},
    {"worldbank", QT_TRANSLATE_NOOP("SourceFlowView", "World Bank"), 0},
    {"oecd", QT_TRANSLATE_NOOP("SourceFlowView", "OECD"), 0},
    {"bis", QT_TRANSLATE_NOOP("SourceFlowView", "Bank for International Settlements"), 0},
    {"ecb", QT_TRANSLATE_NOOP("SourceFlowView", "European Central Bank"), 0},
    {"eurostat", QT_TRANSLATE_NOOP("SourceFlowView", "Eurostat"), 0},
    {"ilo", QT_TRANSLATE_NOOP("SourceFlowView", "International Labour Organization"), 0},
    {"un", QT_TRANSLATE_NOOP("SourceFlowView", "United Nations (Comtrade)"), 0},
    {"jodi", QT_TRANSLATE_NOOP("SourceFlowView", "JODI-Oil"), 0},
    {"fred", QT_TRANSLATE_NOOP("SourceFlowView", "Federal Reserve Bank of St. Louis (FRED)"), 1},
    {"nyfed", QT_TRANSLATE_NOOP("SourceFlowView", "Federal Reserve Bank of New York"), 1},
    {"fedboard", QT_TRANSLATE_NOOP("SourceFlowView", "Federal Reserve Board"), 1},
    {"mofjapan", QT_TRANSLATE_NOOP("SourceFlowView", "Ministry of Finance, Japan"), 2},
    {"eia", QT_TRANSLATE_NOOP("SourceFlowView", "Energy Information Administration"), 1},
    {"bls", QT_TRANSLATE_NOOP("SourceFlowView", "Bureau of Labor Statistics"), 1},
    {"bea", QT_TRANSLATE_NOOP("SourceFlowView", "Bureau of Economic Analysis"), 1},
    {"treasury", QT_TRANSLATE_NOOP("SourceFlowView", "Department of the Treasury"), 1},
    {"usda", QT_TRANSLATE_NOOP("SourceFlowView", "Department of Agriculture"), 1},
    {"cftc", QT_TRANSLATE_NOOP("SourceFlowView", "CFTC"), 1},
    {"sec", QT_TRANSLATE_NOOP("SourceFlowView", "SEC EDGAR"), 1},
    {"ons", QT_TRANSLATE_NOOP("SourceFlowView", "Office for National Statistics (UK)"), 2},
    {"boe", QT_TRANSLATE_NOOP("SourceFlowView", "Bank of England"), 2},
    {"bbk", QT_TRANSLATE_NOOP("SourceFlowView", "Deutsche Bundesbank"), 2},
    {"insee", QT_TRANSLATE_NOOP("SourceFlowView", "INSEE (France)"), 2},
    {"ine", QT_TRANSLATE_NOOP("SourceFlowView", "INE (Spain)"), 2},
    {"snb", QT_TRANSLATE_NOOP("SourceFlowView", "Swiss National Bank"), 2},
    {"ssb", QT_TRANSLATE_NOOP("SourceFlowView", "Statistics Norway"), 2},
    {"scb", QT_TRANSLATE_NOOP("SourceFlowView", "Statistics Sweden"), 2},
    {"cbrt", QT_TRANSLATE_NOOP("SourceFlowView", "Central Bank of Türkiye"), 2},
    {"statcan", QT_TRANSLATE_NOOP("SourceFlowView", "Statistics Canada"), 2},
    {"boc", QT_TRANSLATE_NOOP("SourceFlowView", "Bank of Canada"), 2},
    {"banxico", QT_TRANSLATE_NOOP("SourceFlowView", "Banco de México"), 2},
    {"inegi", QT_TRANSLATE_NOOP("SourceFlowView", "INEGI (Mexico)"), 2},
    {"bcb", QT_TRANSLATE_NOOP("SourceFlowView", "Central Bank of Brazil"), 2},
    {"ibge", QT_TRANSLATE_NOOP("SourceFlowView", "IBGE (Brazil)"), 2},
    {"indec", QT_TRANSLATE_NOOP("SourceFlowView", "INDEC (Argentina)"), 2},
    {"sarb", QT_TRANSLATE_NOOP("SourceFlowView", "South African Reserve Bank"), 2},
    {"boj", QT_TRANSLATE_NOOP("SourceFlowView", "Bank of Japan"), 2},
    {"estat", QT_TRANSLATE_NOOP("SourceFlowView", "e-Stat (Japan)"), 2},
    {"bok", QT_TRANSLATE_NOOP("SourceFlowView", "Bank of Korea"), 2},
    {"bps", QT_TRANSLATE_NOOP("SourceFlowView", "Statistics Indonesia (BPS)"), 2},
    {"singstat", QT_TRANSLATE_NOOP("SourceFlowView", "Singapore Department of Statistics"), 2},
    {"abs", QT_TRANSLATE_NOOP("SourceFlowView", "Australian Bureau of Statistics"), 2},
    {"aisstream", QT_TRANSLATE_NOOP("SourceFlowView", "aisstream.io"), 3},
    {"yahoo", QT_TRANSLATE_NOOP("SourceFlowView", "Yahoo Finance"), 3},
    {"nasdaq", QT_TRANSLATE_NOOP("SourceFlowView", "Nasdaq"), 3},
    {"tradingview", QT_TRANSLATE_NOOP("SourceFlowView", "TradingView"), 3},
    {"forexfactory", QT_TRANSLATE_NOOP("SourceFlowView", "Forex Factory"), 3},
    {"coinmarketcap", QT_TRANSLATE_NOOP("SourceFlowView", "CoinMarketCap"), 3},
    {"alternative", QT_TRANSLATE_NOOP("SourceFlowView", "Alternative.me"), 3},
    {"binance", QT_TRANSLATE_NOOP("SourceFlowView", "Binance"), 3},
    {"coinbase", QT_TRANSLATE_NOOP("SourceFlowView", "Coinbase"), 3},
    {"kraken", QT_TRANSLATE_NOOP("SourceFlowView", "Kraken"), 3},
    {"alpaca", QT_TRANSLATE_NOOP("SourceFlowView", "Alpaca"), 3},
    {"polygon", QT_TRANSLATE_NOOP("SourceFlowView", "Polygon (Massive)"), 3},
    {"wikipedia", QT_TRANSLATE_NOOP("SourceFlowView", "Wikipedia"), 4},
    {"wikidata", QT_TRANSLATE_NOOP("SourceFlowView", "Wikidata"), 4},
    {"newsfeeds", QT_TRANSLATE_NOOP("SourceFlowView", "News sites (RSS feeds)"), 4},
    {"investinglive", QT_TRANSLATE_NOOP("SourceFlowView", "InvestingLive"), 4},
    {"myfxbook", QT_TRANSLATE_NOOP("SourceFlowView", "Myfxbook"), 3},
    {"ing", QT_TRANSLATE_NOOP("SourceFlowView", "ING Think"), 4},
    {"cryptonews", QT_TRANSLATE_NOOP("SourceFlowView", "Cointelegraph · Decrypt"), 4},
    {"fmp", QT_TRANSLATE_NOOP("SourceFlowView", "Financial Modeling Prep"), 4},
    {"adblock", QT_TRANSLATE_NOOP("SourceFlowView", "Ad blocking lists"), 4},
    {"anthropic", QT_TRANSLATE_NOOP("SourceFlowView", "Anthropic (Claude)"), 4},
    {"huggingface", QT_TRANSLATE_NOOP("SourceFlowView", "Hugging Face"), 4},
    {"deepwater", QT_TRANSLATE_NOOP("SourceFlowView", "Deepwater's own models"), 5},
    {"bundled", QT_TRANSLATE_NOOP("SourceFlowView", "Bundled with the app"), 5},
    {"imports", QT_TRANSLATE_NOOP("SourceFlowView", "Your CSV imports"), 5},
};

// The parts of the app, in the order they are drawn (that of the navigation bar); a part without a name is a tab
// drawn on its own.
struct PartDef {
    const char *id;
    const char *tab;
    const char *name;
};
const PartDef Parts[] = {
    {"map_ships", QT_TRANSLATE_NOOP("SourceFlowView", "Map"), QT_TRANSLATE_NOOP("SourceFlowView", "Ships and chokepoints")},
    {"map_supply", QT_TRANSLATE_NOOP("SourceFlowView", "Map"), QT_TRANSLATE_NOOP("SourceFlowView", "Supply chains")},
    {"map_commodities", QT_TRANSLATE_NOOP("SourceFlowView", "Map"), QT_TRANSLATE_NOOP("SourceFlowView", "Commodities")},
    {"map_macro", QT_TRANSLATE_NOOP("SourceFlowView", "Map"), QT_TRANSLATE_NOOP("SourceFlowView", "Heatmaps and country sheets")},
    {"map_trade", QT_TRANSLATE_NOOP("SourceFlowView", "Map"), QT_TRANSLATE_NOOP("SourceFlowView", "Trade flows")},
    {"overview", QT_TRANSLATE_NOOP("SourceFlowView", "Overview"), ""},
    {"forex", QT_TRANSLATE_NOOP("SourceFlowView", "Forex"), ""},
    {"cal_economic", QT_TRANSLATE_NOOP("SourceFlowView", "Calendar"), QT_TRANSLATE_NOOP("SourceFlowView", "Economic")},
    {"cal_corporate", QT_TRANSLATE_NOOP("SourceFlowView", "Calendar"), QT_TRANSLATE_NOOP("SourceFlowView", "Corporate")},
    {"cal_hours", QT_TRANSLATE_NOOP("SourceFlowView", "Calendar"), QT_TRANSLATE_NOOP("SourceFlowView", "Market hours")},
    {"eco_board", QT_TRANSLATE_NOOP("SourceFlowView", "Economy"), QT_TRANSLATE_NOOP("SourceFlowView", "Dashboards")},
    {"eco_countries", QT_TRANSLATE_NOOP("SourceFlowView", "Economy"), QT_TRANSLATE_NOOP("SourceFlowView", "Countries")},
    {"eco_indicators", QT_TRANSLATE_NOOP("SourceFlowView", "Economy"), QT_TRANSLATE_NOOP("SourceFlowView", "Indicators")},
    {"research", QT_TRANSLATE_NOOP("SourceFlowView", "Research"), ""},
    {"quant", QT_TRANSLATE_NOOP("SourceFlowView", "Quant"), ""},
    {"desk", QT_TRANSLATE_NOOP("SourceFlowView", "Desk"), ""},
    {"journal", QT_TRANSLATE_NOOP("SourceFlowView", "Journal"), ""},
    {"charts", QT_TRANSLATE_NOOP("SourceFlowView", "Charts"), ""},
    {"news", QT_TRANSLATE_NOOP("SourceFlowView", "News"), ""},
    {"ai", QT_TRANSLATE_NOOP("SourceFlowView", "AI"), ""},
};

// Each row of the Sources list: the bodies behind it and the parts of the app it feeds (from its "feeds" line in
// SourcesPage.cpp). The country catalogue's sources feed both Economy › Countries and Economy › Indicators.
struct RowDef {
    const char *row;
    const char *sources;
    const char *parts;
};
const RowDef RowDefs[] = {
    {"ais", "aisstream", "map_ships"},
    {"portwatch", "imf", "map_ships"},
    {"portwatch_ports", "imf", "map_supply"},
    {"crop_yields", "usda eurostat", "map_supply"},
    {"itf_road", "oecd", "map_supply"},
    {"jodi", "jodi", "map_supply"},
    {"eia_stocks", "eia", "map_supply"},
    {"nyfed_gscpi", "nyfed", "map_supply"},
    {"fred_freight", "fred", "map_supply"},
    {"yahoo_futures", "yahoo", "map_commodities"},
    {"imf_pcps", "imf", "map_commodities"},
    {"fx", "ecb", "forex"},
    {"bis_daily", "bis", "forex"},
    {"cftc", "cftc", "forex"},
    {"tradingview_calendar", "tradingview", "cal_economic eco_indicators desk"},
    {"nasdaq_calendars", "nasdaq", "cal_corporate"},
    {"yahoo_sessions", "yahoo", "cal_hours"},
    {"research_yahoo", "yahoo", "overview eco_board eco_countries eco_indicators research quant desk journal"},
    {"research_nasdaq", "nasdaq", "overview research"},
    {"research_sec", "sec", "research"},
    {"research_wikipedia", "wikipedia", "research"},
    {"research_wikidata", "wikidata", "research"},
    {"research_treasury", "treasury", "overview desk"},
    {"research_fred", "fred", "eco_board eco_countries eco_indicators desk"},
    {"research_oecd", "oecd", "eco_board eco_countries eco_indicators desk"},
    {"research_coinmarketcap", "coinmarketcap", "eco_board"},
    {"research_alternative", "alternative", "eco_board"},
    {"research_worldbank", "worldbank", "eco_countries eco_indicators"},
    {"research_imf", "imf", "eco_countries eco_indicators"},
    {"research_eurostat", "eurostat", "eco_countries eco_indicators"},
    {"research_ecb", "ecb", "eco_countries eco_indicators desk"},
    {"research_bis", "bis", "eco_countries eco_indicators desk"},
    {"research_ilo", "ilo", "eco_countries eco_indicators"},
    {"research_eia", "eia", "eco_countries eco_indicators"},
    {"research_ons", "ons", "eco_countries eco_indicators"},
    {"research_boe", "boe", "eco_countries eco_indicators desk"},
    {"research_bcb", "bcb", "eco_countries eco_indicators"},
    {"research_statcan", "statcan", "eco_countries eco_indicators"},
    {"research_boc", "boc", "eco_countries eco_indicators desk"},
    {"research_boj", "boj", "eco_countries eco_indicators desk"},
    {"research_snb", "snb", "eco_countries eco_indicators desk"},
    {"research_abs", "abs", "eco_countries eco_indicators"},
    {"research_bbk", "bbk", "eco_countries eco_indicators"},
    {"research_insee", "insee", "eco_countries eco_indicators"},
    {"research_ine", "ine", "eco_countries eco_indicators"},
    {"research_pxweb", "ssb scb", "eco_countries eco_indicators"},
    {"research_inegi", "inegi", "eco_countries eco_indicators"},
    {"research_bps", "bps", "eco_countries eco_indicators"},
    {"research_estat", "estat", "eco_countries eco_indicators"},
    {"cftc_legacy", "cftc", "quant"},
    {"research_fed", "fedboard", "desk"},
    {"research_cftc", "cftc", "quant"},
    {"research_binance", "binance", "desk"},
    {"research_investinglive", "investinglive", "desk"},
    {"research_ing", "ing", "desk"},
    {"research_mof", "mofjapan", "desk"},
    {"myfxbook", "myfxbook", "desk"},
    {"markov_refresh", "deepwater yahoo", "quant research"},
    {"imf_weo", "imf", "map_macro"},
    {"imf_bop", "imf", "map_macro"},
    {"imf_trade", "imf", "map_trade"},
    {"bis_monthly", "bis", "map_macro"},
    {"worldbank", "worldbank", "map_macro"},
    {"bis_credit", "bis", "map_macro"},
    {"oecd_housing", "oecd", "map_macro"},
    {"imf_gdd", "imf", "map_macro"},
    {"worldbank_credit", "worldbank", "map_macro"},
    {"oecd_kei", "oecd", "map_macro"},
    {"eurostat", "eurostat", "map_macro"},
    {"imf_cpi", "imf", "map_macro"},
    {"bls", "bls", "map_macro"},
    {"nat_brazil", "bcb ibge", "map_macro"},
    {"nat_argentina", "indec", "map_macro"},
    {"nat_south_africa", "sarb", "map_macro"},
    {"nat_singapore", "singstat", "map_macro"},
    {"banxico", "banxico", "map_macro"},
    {"ecos", "bok", "map_macro eco_countries eco_indicators"},
    {"evds", "cbrt", "map_macro"},
    {"fred", "fred", "map_macro"},
    {"eia", "eia", "map_macro"},
    {"bea", "bea", "map_macro"},
    {"comtrade", "un", "map_macro"},
    {"surveys", "tradingview", "map_macro eco_countries eco_indicators"},
    {"news_feeds", "newsfeeds yahoo nasdaq", "news"},
    {"adblock", "adblock", "news"},
    {"fmp", "fmp", "news"},
    {"anthropic", "anthropic", "ai"},
    {"huggingface", "huggingface", "ai"},
    {"binance", "binance", "charts"},
    {"coinbase", "coinbase", "charts"},
    {"kraken", "kraken", "charts"},
    {"yahoo", "yahoo", "charts"},
    {"nasdaq", "nasdaq", "charts"},
    {"calendar", "forexfactory", "charts"},
    {"cryptonews", "cryptonews", "charts"},
    {"alpaca", "alpaca", "charts"},
    {"polygon", "polygon", "charts"},
    {"bundled", "bundled", "map_ships map_commodities"},
    {"imports", "imports", "charts"},
};

// What the AIs can reach of the data, in the order it is drawn: the tools of LocalTools (their skill says who may call
// them) and what those tools read. A tool missing here is still drawn, on a row of its own, without shafts: a NEW TOOL
// OF LocalTools SHOULD ALSO GET AN ENTRY. `exported`: AI › Machine learning's CSV files hold it too (MlData.cpp).
struct AccessDef {
    const char *id;
    const char *name;
    const char *what;
    const char *tools;
    const char *rows;       // rows of the Sources list it reads
    const char *part;       // or a part of the app it acts on
    bool stage;             // or the golden records
    bool exported;
};
const AccessDef AccessDefs[] = {
    {"series", QT_TRANSLATE_NOOP("SourceFlowView", "Country series"),
     QT_TRANSLATE_NOOP("SourceFlowView", "The statistical series of the countries, source by source: official statistics, central banks, BIS, IMF, World Bank."),
     "list_data country_series",
     "oecd_kei eurostat imf_cpi bls nat_brazil nat_argentina nat_south_africa nat_singapore fred eia bea banxico ecos evds "
     "surveys bis_credit oecd_housing imf_gdd worldbank_credit", "", false, true},
    {"golden", QT_TRANSLATE_NOOP("SourceFlowView", "Golden records"),
     QT_TRANSLATE_NOOP("SourceFlowView", "The richest series of a country's indicator, fused from its official sources, every point saying its source."),
     "golden_series", "", "", true, true},
    {"weo", QT_TRANSLATE_NOOP("SourceFlowView", "IMF WEO"),
     QT_TRANSLATE_NOOP("SourceFlowView", "The IMF's World Economic Outlook, yearly, with its projections."),
     "imf_weo", "imf_weo", "", false, true},
    {"rates", QT_TRANSLATE_NOOP("SourceFlowView", "Policy rates"),
     QT_TRANSLATE_NOOP("SourceFlowView", "The central banks' policy rates of every country, monthly."),
     "policy_rate", "bis_monthly", "", false, true},
    {"fx", QT_TRANSLATE_NOOP("SourceFlowView", "Exchange rates"),
     QT_TRANSLATE_NOOP("SourceFlowView", "The European Central Bank's daily reference rates."),
     "fx_rate", "fx", "", false, true},
    {"cot", QT_TRANSLATE_NOOP("SourceFlowView", "COT positions"),
     QT_TRANSLATE_NOOP("SourceFlowView", "The CFTC's weekly Commitments of Traders in the futures markets."),
     "cot_positions", "cftc_legacy", "", false, true},
    {"prices", QT_TRANSLATE_NOOP("SourceFlowView", "Prices"),
     QT_TRANSLATE_NOOP("SourceFlowView", "The bars of any stock, ETF, index, currency or crypto on Yahoo Finance, fetched by the app."),
     "prices", "research_yahoo", "", false, true},
    {"tester", QT_TRANSLATE_NOOP("SourceFlowView", "Strategy tester"),
     QT_TRANSLATE_NOOP("SourceFlowView", "Runs strategies in the Charts tab's tester, which can switch the active chart, and reads their trades."),
     "backtest_help run_backtest backtest_trades_csv", "", "charts", false, false},
    {"paper", QT_TRANSLATE_NOOP("SourceFlowView", "Paper account"),
     QT_TRANSLATE_NOOP("SourceFlowView", "Reads the Charts tab's live paper account and places, closes and cancels its orders: simulated money."),
     "paper_account paper_order paper_close paper_cancel", "", "charts", false, false},
    {"table", QT_TRANSLATE_NOOP("SourceFlowView", "Tables and charts"),
     QT_TRANSLATE_NOOP("SourceFlowView", "What a page shows, as data: its tables, figures and the values of its charts, open or not, through the read_page tool. Every page but Sources and AI. The open_page tool takes you to a page when you ask."),
     "read_page open_page", "", "", false, false},
    {"context", QT_TRANSLATE_NOOP("SourceFlowView", "The page in view"),
     QT_TRANSLATE_NOOP("SourceFlowView", "A question asked from the AI side panel goes with a line naming the page and what it shows (a country, a symbol). No skill needed, no figures in it: a table is read through its tool. Models in a slot and opencode."),
     "", "", "", false, false},
};

// The catalogue's kinds, in the order they are drawn, with their names in the plural.
struct ListedDef {
    OfficialSources::Kind kind;
    const char *name;
};
const ListedDef ListedKinds[] = {
    {OfficialSources::Kind::Statistics, QT_TRANSLATE_NOOP("SourceFlowView", "Statistical offices")},
    {OfficialSources::Kind::CentralBank, QT_TRANSLATE_NOOP("SourceFlowView", "Central banks")},
    {OfficialSources::Kind::Government, QT_TRANSLATE_NOOP("SourceFlowView", "Ministries and agencies")},
    {OfficialSources::Kind::International, QT_TRANSLATE_NOOP("SourceFlowView", "International organisations")},
    {OfficialSources::Kind::Private, QT_TRANSLATE_NOOP("SourceFlowView", "Private institutes")},
    {OfficialSources::Kind::Exchange, QT_TRANSLATE_NOOP("SourceFlowView", "Exchanges")},
    {OfficialSources::Kind::TradingEconomics, QT_TRANSLATE_NOOP("SourceFlowView", "Trading Economics")},
};

// A site's domain as an organisation owns it: "api.bcb.gov.br" -> "bcb.gov.br", "data-api.ecb.europa.eu" ->
// "ecb.europa.eu", "fred.stlouisfed.org" -> "stlouisfed.org".
QString siteDomain(QString host)
{
    static const QSet<QString> generic {"gov", "gob", "go", "co", "com", "org", "ac", "gouv", "govt", "or", "net", "edu", "gv", "gc", "nic"};
    QStringList labels = host.trimmed().toLower().split('.', Qt::SkipEmptyParts);
    if (labels.size() < 2) return {};
    if (labels.first() == "www") labels.removeFirst();
    const int n = int(labels.size());
    if (n >= 3 && (generic.contains(labels[n - 2]) || (labels[n - 2] == "europa" && labels[n - 1] == "eu")))
        return labels.mid(n - 3).join('.');
    return labels.mid(n - 2).join('.');
}

// The catalogue less the bodies whose site the rows read; recomputed only when the rows' hosts change.
void addListed(Graph &g)
{
    static QString cachedKey;
    static QVector<Graph::Listed> cached;
    static int cachedRead = 0;
    QSet<QString> domains;
    QStringList hosts;
    for (const Row &r : std::as_const(g.rows)) hosts << r.host;
    const QString key = hosts.join('|');
    if (key != cachedKey || cached.isEmpty()) {
        for (const QString &h : std::as_const(hosts))
            for (const QString &one : h.split(QRegularExpression("[\\s·]+"), Qt::SkipEmptyParts))
                if (one.contains('.') && !one.endsWith(".json") && !one.contains('/'))
                    domains.insert(siteDomain(one));
        domains.remove(QString());
        cached.clear();
        cachedRead = 0;
        const QVector<OfficialSources::Source> &all = OfficialSources::instance().sources();
        for (const ListedDef &def : ListedKinds) {
            Graph::Listed l;
            l.kind = int(def.kind);
            l.name = tr(def.name);
            cached.push_back(l);
        }
        for (int i = 0; i < all.size(); ++i) {
            const OfficialSources::Source &s = all[i];
            const QUrl url(s.url.contains("://") ? s.url : "https://" + s.url);
            if (domains.contains(siteDomain(url.host()))) {
                ++cachedRead;
                continue;
            }
            for (Graph::Listed &l : cached)
                if (l.kind == int(s.kind)) {
                    l.sources << i;
                    l.free += s.access == OfficialSources::Access::Free;
                    l.headline += s.access == OfficialSources::Access::Headline;
                    l.paid += s.access != OfficialSources::Access::Free && s.access != OfficialSources::Access::Headline;
                    l.reachable += s.testOk;
                }
        }
        cached.erase(std::remove_if(cached.begin(), cached.end(), [](const Graph::Listed &l) { return l.sources.isEmpty(); }),
                     cached.end());
        cachedKey = key;
    }
    g.listed = cached;
    g.listedRead = cachedRead;
}

// The worst state of a set of rows: failing, else working, else off when they all are.
Row::State combine(const QVector<Row> &rows, const QVector<int> &indexes)
{
    bool bad = false, good = false, allOff = !indexes.isEmpty();
    for (int i : indexes) {
        bad = bad || rows[i].state == Row::Bad;
        good = good || rows[i].state == Row::Good;
        allOff = allOff && rows[i].state == Row::Off;
    }
    return bad ? Row::Bad : allOff ? Row::Off : good ? Row::Good : Row::Neutral;
}

QColor stateColor(Row::State s)
{
    const Palette &ui = Theme::current();
    switch (s) {
    case Row::Good: return ui.Up;
    case Row::Bad: return ui.Down;
    case Row::Off: return ui.TextFaint;
    default: return ui.TextDim;
    }
}

} // namespace

QVector<Graph::Ai> readAis()
{
    QVector<Graph::Ai> out;
    // A model has skills only in a slot, bound to its files: the slot's check (ModelSlotsView), by sizes and dates.
    const QVector<modelslots::Slot> held = modelslots::load();
    for (int i = 0; i < held.size(); ++i) {
        const modelslots::Slot &s = held[i];
        if (s.model.isEmpty() || modelslots::isIntegration(s.model)) continue;
        Graph::Ai ai;
        ai.id = "slot:" + QString::number(i);
        ai.model = s.model;
        ai.name = QCoreApplication::translate("SourceFlowView", "Slot %1 · %2").arg(i + 1).arg(s.model.section('/', 1));
        ai.skills = s.skills;
        const QHash<QString, LocalModels::Fingerprint> now = LocalModels::stat(s.model);
        if (!s.files.isEmpty()) {
            ai.on = now.size() == s.files.size();
            for (auto f = now.begin(); f != now.end() && ai.on; ++f) {
                const auto was = s.files.constFind(f.key());
                ai.on = was != s.files.constEnd() && was->size == f->size && was->modified == f->modified;
            }
        }
        out << ai;
    }
    if (out.isEmpty()) {
        Graph::Ai ai;
        ai.id = "slots";
        ai.name = QCoreApplication::translate("SourceFlowView", "Local models");
        ai.on = false;
        out << ai;
    }
    // An integration's row says which slot holds it; its skills come from there.
    auto named = [&held](const char *id, const QString &name) {
        const int slot = modelslots::slotHolding(held, QString::fromLatin1(id));
        return slot < 0 ? name : QCoreApplication::translate("SourceFlowView", "Slot %1 · %2").arg(slot + 1).arg(name);
    };
    const McpServer *mcp = McpServer::instance();
    Graph::Ai opencode;
    opencode.id = "opencode";
    opencode.name = named(modelslots::OpencodeId, QCoreApplication::translate("SourceFlowView", "opencode (MCP)"));
    opencode.kind = Graph::Ai::Agent;
    opencode.skills = mcp ? mcp->skills() : QStringList();
    // In a slot it reaches them through the conversation (side panel, AI › Local), serving or not.
    const int opencodeSlot = modelslots::slotHolding(held, QString::fromLatin1(modelslots::OpencodeId));
    if (opencodeSlot >= 0) opencode.model = QString::fromLatin1(modelslots::OpencodeId);
    opencode.on = opencodeSlot >= 0 || (mcp && mcp->listening());
    out << opencode;
    Graph::Ai claude;
    claude.id = "claude";
    claude.name = named(modelslots::ClaudeId, QCoreApplication::translate("SourceFlowView", "Claude (Anthropic API)"));
    claude.kind = Graph::Ai::Cloud;
    // Nothing in the app calls it yet: what its slot grants is granted, not in force.
    if (const int slot = modelslots::slotHolding(held, QString::fromLatin1(modelslots::ClaudeId)); slot >= 0) claude.skills = held[slot].skills;
    claude.on = false;
    out << claude;
    Graph::Ai training;
    training.id = "ml";
    training.name = QCoreApplication::translate("SourceFlowView", "Machine learning");
    training.kind = Graph::Ai::Training;
    out << training;
    return out;
}

namespace {

// The underground floor's model: what can be reached, from LocalTools and AccessDefs, and who reaches it.
void addUnderground(Graph &g, const QVector<Graph::Ai> &ais)
{
    auto skillName = [](const QString &id) {
        for (const modelslots::Skill &k : modelslots::skills())
            if (id == QLatin1String(k.id)) return QCoreApplication::translate("ModelSlotsView", k.name);
        return id;
    };
    QHash<QString, int> rowIndex;
    for (int r = 0; r < g.rows.size(); ++r) rowIndex.insert(g.rows[r].id, r);
    QVector<bool> exported;
    QVector<QString> skills;   // by access: the id of the skill that grants it
    QSet<QString> drawn;
    for (const AccessDef &d : AccessDefs) {
        Graph::Access a;
        a.id = d.id;
        a.name = tr(d.name);
        a.what = tr(d.what);
        QString skill;
        for (const QString &tool : QString(d.tools).split(' ', Qt::SkipEmptyParts))
            for (const LocalTools::Tool &t : LocalTools::tools())
                if (t.name == tool) {
                    a.tools << tool;
                    skill = t.skill;
                    drawn << tool;
                }
        if (a.tools.isEmpty() && *d.tools) continue;   // its tools are gone
        a.skill = skill.isEmpty() ? QString() : skillName(skill);
        for (const QString &id : QString(d.rows).split(' ', Qt::SkipEmptyParts))
            if (const auto r = rowIndex.constFind(id); r != rowIndex.constEnd()) a.rows << r.value();
        for (int s = 0; s < g.sources.size(); ++s)
            for (int r : std::as_const(a.rows))
                if (g.sources[s].rows.contains(r)) {
                    a.sources << s;
                    break;
                }
        if (*d.part)
            for (int p = 0; p < g.parts.size(); ++p)
                if (g.parts[p].id == QLatin1String(d.part)) {
                    a.parts << p;
                    a.rows = g.parts[p].rows;
                }
        if (d.stage) {
            a.stage = g.stage.present;
            for (const Graph::Link &l : std::as_const(g.links))
                if (l.staged)
                    for (int r : l.rows)
                        if (!a.rows.contains(r)) a.rows << r;
        }
        a.state = combine(g.rows, a.rows);
        g.accesses << a;
        skills << skill;
        exported << d.exported;
    }
    for (const LocalTools::Tool &t : LocalTools::tools()) {
        if (drawn.contains(t.name)) continue;
        Graph::Access a;
        a.id = t.name;
        a.name = t.name;
        a.what = t.description;
        a.skill = skillName(t.skill);
        a.tools << t.name;
        g.accesses << a;
        skills << t.skill;
        exported << false;
    }
    g.ais = ais;
    for (int i = 0; i < g.ais.size(); ++i) {
        Graph::Ai &ai = g.ais[i];
        for (int a = 0; a < g.accesses.size(); ++a) {
            const bool reach = ai.kind == Graph::Ai::Training ? exported[a]
                               : skills[a].isEmpty()          ? ai.kind != Graph::Ai::Cloud && !ai.model.isEmpty()
                                                              : ai.skills.contains(skills[a]);
            if (!reach) continue;
            ai.accesses << a;
            g.aiLinks.push_back({i, a, ai.on});
        }
    }
}

} // namespace

Graph build(const QVector<Row> &rows, const QVector<fusion::Summary> &fusion, const QVector<Graph::Ai> &ais)
{
    static const QHash<QString, int> sourceDefs = [] {
        QHash<QString, int> h;
        for (int i = 0; i < int(std::size(Sources)); ++i) h.insert(Sources[i].id, i);
        return h;
    }();
    static const QHash<QString, int> partDefs = [] {
        QHash<QString, int> h;
        for (int i = 0; i < int(std::size(Parts)); ++i) h.insert(Parts[i].id, i);
        return h;
    }();
    static const QHash<QString, int> rowDefs = [] {
        QHash<QString, int> h;
        for (int i = 0; i < int(std::size(RowDefs)); ++i) h.insert(RowDefs[i].row, i);
        return h;
    }();

    Graph g;
    g.rows = rows;
    QHash<QString, int> sourceIndex, partIndex, linkIndex;
    auto source = [&](const QString &id, const QString &fallbackName) {
        if (const auto it = sourceIndex.constFind(id); it != sourceIndex.constEnd()) return it.value();
        Graph::Source s;
        s.id = id;
        if (const auto def = sourceDefs.constFind(id); def != sourceDefs.constEnd()) {
            s.name = tr(Sources[def.value()].name);
            s.groupOrder = Sources[def.value()].group;
        } else {
            s.name = fallbackName;
            s.groupOrder = OtherGroup;
        }
        s.group = tr(Groups[s.groupOrder]);
        g.sources.push_back(s);
        sourceIndex.insert(id, int(g.sources.size()) - 1);
        return int(g.sources.size()) - 1;
    };
    auto part = [&](const QString &id, const QString &fallbackTab) {
        if (const auto it = partIndex.constFind(id); it != partIndex.constEnd()) return it.value();
        Graph::Part p;
        p.id = id;
        if (const auto def = partDefs.constFind(id); def != partDefs.constEnd()) {
            p.tab = tr(Parts[def.value()].tab);
            p.name = *Parts[def.value()].name ? tr(Parts[def.value()].name) : QString();
            p.order = def.value();
        } else {
            p.tab = fallbackTab;
            p.order = 1000;
        }
        g.parts.push_back(p);
        partIndex.insert(id, int(g.parts.size()) - 1);
        return int(g.parts.size()) - 1;
    };

    for (int r = 0; r < rows.size(); ++r) {
        const Row &row = rows[r];
        QVector<int> ss, ps;
        if (const auto def = rowDefs.constFind(row.id); def != rowDefs.constEnd()) {
            for (const QString &id : QString(RowDefs[def.value()].sources).split(' ', Qt::SkipEmptyParts)) ss << source(id, id);
            for (const QString &id : QString(RowDefs[def.value()].parts).split(' ', Qt::SkipEmptyParts)) ps << part(id, id);
        } else {
            // A row added to the list since this table was written: its provider and its section of the list.
            ss << source("other:" + row.provider, row.provider);
            ps << part("section:" + row.section, row.section);
        }
        for (int s : ss) {
            g.sources[s].rows << r;
            for (int p : ps) {
                const QString key = QString::number(s) + ':' + QString::number(p);
                int l = linkIndex.value(key, -1);
                if (l < 0) {
                    g.links.push_back({s, p, {}, Row::Neutral});
                    l = int(g.links.size()) - 1;
                    linkIndex.insert(key, l);
                    ++g.parts[p].sources;
                }
                g.links[l].rows << r;
            }
        }
        for (int p : ps)
            if (!g.parts[p].rows.contains(r)) g.parts[p].rows << r;
    }
    for (Graph::Source &s : g.sources) s.state = combine(rows, s.rows);
    for (Graph::Link &l : g.links) l.state = combine(rows, l.rows);
    addListed(g);

    // Data fusion: the diagram's id of each provider that gave points to a golden record.
    QSet<QString> fused, countries, indicators;
    for (const fusion::Summary &f : fusion) {
        if (f.records <= 0)
            continue;
        Graph::Stage &st = g.stage;
        st.records += f.records;
        st.points += f.points;
        st.added += f.added;
        st.spliced += f.spliced;
        st.unverified += f.unverified;
        st.conflicts += f.conflicts;
        countries << f.country;
        indicators << f.indicator;
        if (!st.latest.isValid() || f.fused > st.latest) st.latest = f.fused;
        for (int i = 0; i < f.providers.size(); ++i) {
            static const QHash<QString, QString> ids {{"wb", "worldbank"}, {"weo", "imf"}, {"ecos", "bok"}, {"evds", "cbrt"}};
            const QString &provider = f.providers[i];
            fused << (provider == "pxweb" ? (f.labels.value(i).contains("Sweden") ? QStringLiteral("scb") : QStringLiteral("ssb"))
                                          : ids.value(provider, provider));
        }
    }
    g.stage.countries = int(countries.size());
    g.stage.indicators = int(indicators.size());
    for (Graph::Link &l : g.links) {
        const QString part = g.parts[l.part].id;
        if ((part == "eco_countries" || part == "eco_indicators") && fused.contains(g.sources[l.source].id)) {
            l.staged = true;
            g.stage.present = true;
            if (!g.stage.parts.contains(l.part)) g.stage.parts << l.part;
            if (!g.stage.sources.contains(l.source)) g.stage.sources << l.source;
        }
    }
    addUnderground(g, ais);
    return g;
}

// ---------------------------------------------------------------- Canvas

// The diagram itself, in a scroll area: the sources down the left, the parts of the app down the right, a curve for
// each link. Its height follows the number of sources; its width the view's.
class Canvas : public QWidget
{
public:
    enum Kind { None, Source, Part, Link, Listed, Stage, Ai, Access, AiLink };
    struct Item {
        Kind kind = None;
        int index = -1;
        bool operator==(const Item &o) const { return kind == o.kind && index == o.index; }
    };

    explicit Canvas(QWidget *parent) : QWidget(parent)
    {
        setMouseTracking(true);
        setAttribute(Qt::WA_OpaquePaintEvent, false);
        setFocusPolicy(Qt::ClickFocus);   // the arrows change floor; the disc view gives it the focus
        m_focusAnim = new QVariantAnimation(this);
        m_focusAnim->setDuration(520);
        m_focusAnim->setEasingCurve(QEasingCurve::InOutCubic);
        connect(m_focusAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
            m_focus = v.toDouble();
            placeShafts();
            update();
        });
        connect(m_focusAnim, &QVariantAnimation::finished, this, [this] { update(); });
        m_inspectAnim = new QVariantAnimation(this);
        m_inspectAnim->setDuration(480);
        m_inspectAnim->setStartValue(0.0);
        m_inspectAnim->setEndValue(1.0);
        m_inspectAnim->setEasingCurve(QEasingCurve::InOutCubic);
        connect(m_inspectAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
            m_inspectT = v.toDouble();
            update();
        });
        connect(m_inspectAnim, &QVariantAnimation::finished, this, [this] {
            if (m_inspectClosing) closeInspectNow();
            update();
        });
        m_anim = new QVariantAnimation(this);
        m_anim->setDuration(700);
        m_anim->setEasingCurve(QEasingCurve::InOutCubic);
        connect(m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
            m_tilt = v.toDouble();
            place(false);
            update();
        });
        connect(m_anim, &QVariantAnimation::finished, this, [this] {
            place(true);
            if (m_overview && m_tilt <= 0) {
                if (m_switching) {
                    // From one overview to the other: back to the columns, then out again.
                    m_switching = false;
                    m_mode = m_nextMode;
                    computeFloors();
                    computePlates();
                    place(true);
                    warmLabels();
                    m_anim->setStartValue(0.0);
                    m_anim->setEndValue(1.0);
                    m_anim->start();
                    return;
                }
                // Back to the flat diagram: its full height again, scrolled where it was.
                m_overview = false;
                setMinimumHeight(int(std::ceil(m_naturalHeight)));
                if (scrollTo) scrollTo(m_scroll);
                update();
            }
        });
    }

    Graph graph;
    Item hover, pinned;
    std::function<void()> changed;
    std::function<void(int)> scrollTo;   // the scroll area's position, given back when the overview closes

    // The overviews: the whole diagram zoomed out to fit `viewport` and seen in isometric view, from the flat diagram
    // scrolled at `scroll` (and back to it when `on` is false). Disc: laid out on a flat disc. Plates: thin plates
    // standing one behind the other (the sources, the fusion, the parts of the app), the rows laid on them, the links
    // going through. From one to the other, the diagram goes back to its columns first.
    enum Mode { Disc, Plates };
    // The disc view's floors. The camera goes to the one in focus, which has the view to itself, stands out with a
    // coloured rim and answers the mouse; the other shows past its edge, faded unless an item in view reaches it.
    enum Floor { Ground, Underground };
    bool discShown() const { return m_overview && m_mode == Disc; }
    // An item of the disc looked at closely: it rises, the camera comes close, what it descends from on its left and what it
    // goes to on its right. The arrows go to the next or the previous one; Escape or a click beside goes back to the disc.
    void inspect(const Item &item);
    bool inspecting() const { return m_inspect.kind != None; }
    Floor floor() const { return m_floor; }
    void setFloor(Floor floor)
    {
        if (floor == m_floor) return;
        m_floor = floor;
        hover = pinned = {};
        setCursor(Qt::ArrowCursor);
        m_focusAnim->stop();
        m_focusAnim->setStartValue(m_focus);
        m_focusAnim->setEndValue(floor == Underground ? 1.0 : 0.0);
        m_focusAnim->start();
        if (changed) changed();
    }
    void setOverview(bool on, Mode mode, int scroll, const QSize &viewport)
    {
        if (on && mode == Disc) setFocus();
        // The underground floor's items exist on the disc only.
        if (!on || mode != Disc) {
            const auto under = [](const Item &i) { return i.kind == Ai || i.kind == Access || i.kind == AiLink; };
            if (under(hover)) hover = {};
            if (under(pinned)) pinned = {};
            closeInspectNow();
            // The camera comes back to the disc on the way out (quicker than the view closes).
            m_floor = Ground;
            m_focusAnim->stop();
            if (m_focus > 0) {
                m_focusAnim->setStartValue(m_focus);
                m_focusAnim->setEndValue(0.0);
                m_focusAnim->start();
            }
            if (changed) changed();
        }
        if (on && m_overview && mode != m_mode) {
            m_switching = true;
            m_nextMode = mode;
            m_anim->stop();
            m_anim->setStartValue(m_tilt);
            m_anim->setEndValue(0.0);
            m_anim->start();
            return;
        }
        m_switching = false;
        m_anim->stop();
        if (on && !m_overview) m_scroll = scroll;
        if (on) {
            m_overview = true;
            m_mode = mode;
            m_viewport = viewport;
            computeFloors();
            computePlates();
            place(true);
        }
        // The labels' images made before the first frame moves, not during it.
        warmLabels();
        m_anim->setStartValue(m_tilt);
        m_anim->setEndValue(on ? 1.0 : 0.0);
        m_anim->start();
        if (on) setMinimumHeight(viewport.height());   // the scroll area fits the canvas to its view: no scrolling
    }
    bool overview() const { return m_overview; }
    // The scroll area's view resized during the overview: the diagram fits it again.
    void setViewport(const QSize &viewport)
    {
        if (!m_overview || viewport == m_viewport) return;
        m_viewport = viewport;
        setMinimumHeight(viewport.height());
        computeFloors();
        computePlates();
        place(true);
        m_discImage = QImage();
        if (inspecting() && !m_inspectClosing) {
            m_camToAt = inspectAnchor(m_inspect);
            if (m_inspectAnim->state() != QAbstractAnimation::Running) m_camFromAt = m_camToAt;
            buildInspect();
        }
        update();
    }

    Item shown() const { return hover.kind != None ? hover : pinned; }

    void setGraph(const Graph &g, bool structure)
    {
        if (!structure) {
            bool same = true;
            for (int i = 0; i < g.rows.size() && same; ++i) same = g.rows[i].state == graph.rows[i].state;
            if (same) return;
            graph = g;
            m_discImage = QImage();
            if (inspecting() && !m_inspectClosing) buildInspect();
            update();
            if (changed) changed();
            return;
        }
        graph = g;
        closeInspectNow();
        hover = pinned = {};
        relayout();
        update();
        if (changed) changed();
    }

protected:
    // Escape is a shortcut of the main window: while an item is looked at, it's this view's.
    bool event(QEvent *e) override
    {
        if (e->type() == QEvent::ShortcutOverride && inspecting() && static_cast<QKeyEvent *>(e)->key() == Qt::Key_Escape) {
            e->accept();
            return true;
        }
        return QWidget::event(e);
    }
    void resizeEvent(QResizeEvent *) override
    {
        m_discImage = QImage();
        relayout();
    }
    void showEvent(QShowEvent *) override
    {
        if (discShown()) setFocus();
    }
    void keyPressEvent(QKeyEvent *e) override
    {
        if (inspecting()) {
            if (e->key() == Qt::Key_Escape) closeInspect();
            else if (e->key() == Qt::Key_Left || e->key() == Qt::Key_Right) inspectStep(e->key() == Qt::Key_Right ? 1 : -1);
            else if (e->key() != Qt::Key_Up && e->key() != Qt::Key_Down) QWidget::keyPressEvent(e);
            return;   // the floors wait until the disc is back
        }
        if (discShown() && (e->key() == Qt::Key_Up || e->key() == Qt::Key_Down) && !(e->modifiers() & ~Qt::KeypadModifier)) {
            setFloor(e->key() == Qt::Key_Up ? Ground : Underground);
            return;
        }
        QWidget::keyPressEvent(e);   // the columns: the scroll area's
    }
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *e) override
    {
        if (inspecting()) {
            int at = -1;
            for (int i = 0; i < m_inspectRows.size(); ++i)
                if (m_inspectRows[i].item.kind != None && m_inspectRows[i].box.contains(e->position())) at = i;
            setCursor(at >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
            if (at != m_inspectHover) {
                m_inspectHover = at;
                update();
            }
            return;
        }
        const Item at = itemAt(e->position());
        setCursor(at.kind == None ? Qt::ArrowCursor : Qt::PointingHandCursor);
        if (at == hover) return;
        hover = at;
        update();
        if (changed) changed();
    }
    void leaveEvent(QEvent *) override
    {
        if (hover.kind == None) return;
        hover = {};
        update();
        if (changed) changed();
    }
    void mousePressEvent(QMouseEvent *e) override
    {
        if (e->button() != Qt::LeftButton) return;
        if (inspecting()) {
            for (const InspectRow &r : std::as_const(m_inspectRows))
                if (r.item.kind != None && r.box.contains(e->position())) {
                    inspect(r.item);
                    return;
                }
            if (!m_inspectCard.contains(e->position())) closeInspect();
            return;
        }
        const Item at = itemAt(e->position());
        // On the disc at rest, an item opens up; a link is kept, as elsewhere.
        if (discShown() && m_tilt >= 1 && m_focusAnim->state() != QAbstractAnimation::Running && at.kind != None && at.kind != Link
            && at.kind != AiLink) {
            inspect(at);
            return;
        }
        pinned = at == pinned ? Item() : at;
        update();
        if (changed) changed();
    }

private:
    static constexpr double Margin = 16, SourceRow = 22, GroupHead = 30, PartHead = 24, TabGap = 12;

    QFont nodeFont() const
    {
        QFont f = font();
        f.setPixelSize(12);
        return f;
    }
    QFont headFont() const
    {
        QFont f = font();
        f.setPixelSize(10);
        f.setBold(true);
        f.setLetterSpacing(QFont::AbsoluteSpacing, 1);
        return f;
    }
    QString partLabel(int p) const { return graph.parts[p].name.isEmpty() ? graph.parts[p].tab : graph.parts[p].name; }

    void relayout();
    // The links (and, with `hits`, the bands the mouse finds them by) for the overview's progress: each element moves
    // from its place in the columns to its place on the disc by m_tilt of the way.
    void place(bool hits);
    void ensureHits() const;   // the links' mouse bands, made when the mouse first needs them after a change
    QPointF moved(const QPointF &delta) const { return delta * m_tilt; }
    // Where a row is drawn: from its place in the columns (around its anchor `a`) to its place on the disc, moved by
    // `move` and turned by `turn` (radians: outward along the radius) by m_tilt of the way.
    QTransform rowTransform(const QPointF &a, const QPointF &move, double turn) const
    {
        return QTransform::fromTranslate(-a.x(), -a.y()) * QTransform().rotateRadians(turn * m_tilt)
               * QTransform::fromTranslate(a.x() + move.x() * m_tilt, a.y() + move.y() * m_tilt);
    }
    Item itemAt(const QPointF &pos) const;
    // The overview's projection of the diagram (its own coordinates) onto the canvas, `t` from 0 (flat, as scrolled)
    // to 1 (zoomed out to fit, isometric); identity outside the overview.
    QTransform projection(double t) const;
    void paintContent(QPainter &p);
    // Text, or while the overview moves the same text from a cache of small images: under a turn, Qt draws text
    // glyph by glyph as paths, too slow for 60 frames a second.
    void drawLabel(QPainter &p, const QRectF &box, int flags, const QString &text);
    void drawLabel(QPainter &p, const QPointF &baseline, const QString &text);
    void warmLabels()
    {
        QImage scratch(size().boundedTo(QSize(64, 64)), QImage::Format_ARGB32_Premultiplied);
        QPainter sp(&scratch);
        m_fast = true;
        paintContent(sp);
        m_fast = false;
    }
    QImage m_frame;             // the overview's frames while it moves
    bool m_fast = false;        // drawing such a frame: labels from images, links as thin lines
    QHash<QString, QImage> m_labels;
    // Where each row is drawn, as one transform from the diagram's coordinates to the canvas: the columns (scrolled),
    // the disc (moved, turned, projected) or a plate, by m_tilt of the way.
    enum RowKind { SourceRowKind, ListedRowKind, PartRowKind, LeftHeadKind, RightHeadKind };
    QTransform rowMatrix(RowKind kind, int i) const;
    QTransform stageMatrix() const;
    bool platesShown() const { return m_overview && m_mode == Plates; }
    // The plates: where each row sits on its plate and the plates themselves, fitted to the view.
    void computePlates();
    // The underground floor: both floors' sizes and places in the view, the shafts between them.
    QFont underFont() const
    {
        QFont f = font();
        f.setPixelSize(14);
        return f;
    }
    void computeFloors();
    void placeShafts();
    double underOpacity() const { return std::clamp((m_tilt - 0.55) / 0.45, 0.0, 1.0); }
    // Where the camera puts the floor below (computeFloors places it for the camera on it); it rises as the view opens.
    QPointF underShift() const { return {0, m_floorDistance * (1 - m_focus) + (1 - m_tilt) * 60}; }
    QString accessDetail(int access) const;
    QString aiDetail(int ai) const;
    bool underAt(const QPointF &pos, Item *found) const;
    struct Lit {
        Item shown;
        bool active = false;
        QSet<int> links, sources, parts;
        QSet<int> ais, accesses, aiLinks, shafts;
        bool stage = false;
    };
    Lit lighting() const;
    void drawLinks(QPainter &p, const Lit &lit, const QVector<QPainterPath> &paths);
    void drawStage(QPainter &p, const Lit &lit);
    void drawLeftRows(QPainter &p, const Lit &lit);
    void drawRightRows(QPainter &p, const Lit &lit);
    void drawPlate(QPainter &p, int plate);
    void drawUnderground(QPainter &p, const Lit &lit);
    void drawFloorChip(QPainter &p);
    // The item looked at: the camera, the card that rises, and on each side the rows it comes from and goes to.
    struct InspectRow {
        Item item;               // None: a data set, nothing to go to
        QString name, detail;
        QColor dot;
        int side = 0;            // -1 left, 1 right
        QRectF box;
    };
    void closeInspect();
    void closeInspectNow();
    void inspectStep(int step);
    QVector<Item> inspectOrder(Item from) const;
    QPointF inspectAnchor(const Item &item) const;
    void buildInspect();
    void describe(const Item &item, QString *name, QString *detail, QColor *dot) const;
    void paintInspect(QPainter &p);
    QTransform inspectCamera() const;
    // How strongly a floor's rows are drawn: in full when in focus, or while an item is followed across the floors.
    double floorInk(Floor floor, const Lit &lit) const
    {
        if (lit.active) return 1;
        const double focus = floor == Underground ? m_focus : 1 - m_focus;
        return 0.3 + 0.7 * focus;
    }
    QColor floorColor(double focusBelow) const
    {
        const Palette &ui = Theme::current();
        const QColor a = ui.Accent, b = ui.Up;
        return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * focusBelow, a.greenF() + (b.greenF() - a.greenF()) * focusBelow,
                                a.blueF() + (b.blueF() - a.blueF()) * focusBelow);
    }
    // A floor's rim: the panel's line, its colour with a soft glow as it comes into focus.
    void drawRim(QPainter &p, const std::function<void()> &shape, double focus, const QColor &colour);
    void drawShafts(QPainter &p, const Lit &lit, bool on);

    QVariantAnimation *m_inspectAnim;
    double m_inspectT = 0;
    Item m_inspect;
    bool m_inspectClosing = false;
    QPointF m_camFromAt, m_camToAt;
    double m_camFromZoom = 1, m_camToZoom = 1, m_dimFrom = 0, m_dimTo = 0, m_liftFrom = 0;
    QImage m_discImage;          // the disc at rest, drawn once: the camera moves over it
    bool m_paintingDisc = false;
    QVector<InspectRow> m_inspectRows;
    QString m_inspectName, m_inspectDetail, m_inspectLeftTitle, m_inspectRightTitle, m_inspectNote;
    QColor m_inspectDot;
    QRectF m_inspectCard;
    int m_inspectHover = -1;
    QVariantAnimation *m_focusAnim;
    Floor m_floor = Ground;
    double m_focus = 0;         // the camera: 0 on the ground floor .. 1 on the underground
    QVariantAnimation *m_anim;
    double m_tilt = 0;          // the overview's progress, 0 flat .. 1 isometric
    bool m_overview = false;
    Mode m_mode = Disc;
    bool m_switching = false;   // going back to the columns on the way to the other overview
    Mode m_nextMode = Disc;
    int m_scroll = 0;
    QSize m_viewport;
    double m_naturalHeight = 0;  // the diagram's height, laid out flat

    QVector<int> m_sourceOrder, m_partOrder;
    QVector<double> m_sourceY, m_partY;            // by source / part index
    QVector<double> m_listedY;                     // by index in graph.listed
    QVector<QPair<double, QString>> m_leftHeads, m_rightHeads;
    QVector<QPainterPath> m_paths;                 // by link index
    mutable QVector<QPainterPath> m_hits;          // likewise, empty until ensureHits()
    double m_leftAnchor = 0, m_rightAnchor = 0, m_partRow = 26;
    QRectF m_stageRect;                            // the fusion block (empty without one)
    // The overview's disc: the sources along its left arc, the parts along its right one, every row turned outward
    // as in the columns. Each element's move from the columns to the disc (its anchor's), by index.
    QPointF m_discCentre;
    double m_discRadius = 0;                       // the anchors' circle
    double m_plateRadius = 0;                      // the disc the diagram lies on, its labels included
    QVector<QPointF> m_sourceMove, m_listedMove, m_partMove, m_leftHeadMove, m_rightHeadMove;
    QVector<double> m_sourceTurn, m_listedTurn, m_partTurn, m_leftHeadTurn, m_rightHeadTurn;
    QPointF m_stageMove;
    // The underground floor, on the canvas with the camera on it (its rows aren't turned): the floor's centre and half
    // height (half its width), the anchors, the links and the shafts.
    struct Shaft {
        int access = 0, source = -1, part = -1;
        bool stage = false;
        QPainterPath path;
    };
    QPointF m_underCentre;
    double m_underB = 0, m_underLabel = 0, m_underRow = 16, m_floorFit = 1, m_floorDistance = 0;
    QVector<QPointF> m_aiAt, m_accessAt;
    QVector<QPainterPath> m_underLinks;
    mutable QVector<QPainterPath> m_underHits;
    QVector<Shaft> m_shafts;
    // The plates (front to back: sources, fusion when there is one, parts), fitted to the view: each row's
    // transform onto its plate, the plates' outlines and titles, and the depth direction the links leave by.
    QVector<QTransform> m_plateSource, m_plateListed, m_platePart, m_plateLeftHead, m_plateRightHead;
    QTransform m_plateStage;
    QVector<QPolygonF> m_plateOutline;
    QVector<QTransform> m_plateFrame;              // plate coordinates (from its top left) to the canvas
    QStringList m_plateTitle;
    QPointF m_plateDepth;                          // a quarter of the gap between two plates, on the canvas
    QVector<QPainterPath> m_frontPaths, m_backPaths;   // the plates' links: before and after the middle plate
};

void Canvas::relayout()
{
    const int ns = int(graph.sources.size()), np = int(graph.parts.size());
    // Parts in the navigation bar's order; sources by group, then near the parts they feed (fewer crossings).
    m_partOrder.resize(np);
    std::iota(m_partOrder.begin(), m_partOrder.end(), 0);
    std::stable_sort(m_partOrder.begin(), m_partOrder.end(), [&](int a, int b) { return graph.parts[a].order < graph.parts[b].order; });
    QVector<int> rank(np);
    for (int i = 0; i < np; ++i) rank[m_partOrder[i]] = i;
    QVector<double> centre(ns, 0), weight(ns, 0);
    for (const Graph::Link &l : std::as_const(graph.links)) {
        centre[l.source] += rank[l.part] * l.rows.size();
        weight[l.source] += l.rows.size();
    }
    for (int i = 0; i < ns; ++i) centre[i] = weight[i] > 0 ? centre[i] / weight[i] : 0;
    m_sourceOrder.resize(ns);
    std::iota(m_sourceOrder.begin(), m_sourceOrder.end(), 0);
    std::stable_sort(m_sourceOrder.begin(), m_sourceOrder.end(), [&](int a, int b) {
        const Graph::Source &x = graph.sources[a], &y = graph.sources[b];
        if (x.groupOrder != y.groupOrder) return x.groupOrder < y.groupOrder;
        if (centre[a] != centre[b]) return centre[a] < centre[b];
        return x.name.localeAwareCompare(y.name) < 0;
    });

    // Left column: a heading per group, a row per source.
    m_sourceY.assign(ns, 0);
    m_leftHeads.clear();
    double y = Margin;
    int group = -1;
    for (int s : std::as_const(m_sourceOrder)) {
        if (graph.sources[s].groupOrder != group) {
            group = graph.sources[s].groupOrder;
            if (!m_leftHeads.isEmpty()) y += 8;
            m_leftHeads.push_back({y + GroupHead / 2 + 3, graph.sources[s].group.toUpper()});
            y += GroupHead;
        }
        m_sourceY[s] = y + SourceRow / 2;
        y += SourceRow;
    }
    // Then the catalogue's bodies the app doesn't read: a row per kind, without links.
    m_listedY.assign(graph.listed.size(), 0);
    if (!graph.listed.isEmpty()) {
        y += 8;
        m_leftHeads.push_back({y + GroupHead / 2 + 3, QCoreApplication::translate("SourceFlowView", "Listed, not read by the app").toUpper()});
        y += GroupHead;
        for (int i = 0; i < graph.listed.size(); ++i) {
            m_listedY[i] = y + SourceRow / 2;
            y += SourceRow;
        }
    }
    const double leftHeight = y + Margin;

    // Right column: a heading per tab of several parts, spread over the left column's height.
    QVector<QPair<int, int>> tabs;   // [begin, end) in m_partOrder
    int heads = 0;
    for (int i = 0; i < np;) {
        int j = i;
        while (j < np && graph.parts[m_partOrder[j]].tab == graph.parts[m_partOrder[i]].tab) ++j;
        tabs.push_back({i, j});
        heads += !(j - i == 1 && graph.parts[m_partOrder[i]].name.isEmpty());
        i = j;
    }
    const double gaps = std::max(0, int(tabs.size()) - 1) * TabGap;
    const double room = leftHeight - 2 * Margin - heads * PartHead - gaps;
    m_partRow = np > 0 ? std::clamp(room / np, 26.0, 120.0) : 26;
    const double rightHeight = heads * PartHead + gaps + np * m_partRow;
    y = Margin + std::max(0.0, (leftHeight - 2 * Margin - rightHeight) / 2);
    m_partY.assign(np, 0);
    m_rightHeads.clear();
    for (int t = 0; t < tabs.size(); ++t) {
        const auto [begin, end] = tabs[t];
        if (t > 0) y += TabGap;
        if (!(end - begin == 1 && graph.parts[m_partOrder[begin]].name.isEmpty())) {
            m_rightHeads.push_back({y + PartHead / 2 + 3, graph.parts[m_partOrder[begin]].tab.toUpper()});
            y += PartHead;
        }
        for (int k = begin; k < end; ++k) {
            m_partY[m_partOrder[k]] = y + m_partRow / 2;
            y += m_partRow;
        }
    }
    const int height = int(std::ceil(std::max(leftHeight, y + Margin)));
    m_naturalHeight = height;
    if (!m_overview && minimumHeight() != height) setMinimumHeight(height);

    // Anchors: right of the longest source name, left of the longest part name.
    QFont boldFont = nodeFont();
    boldFont.setWeight(QFont::DemiBold);   // the name in view is drawn bold
    const QFontMetricsF node(boldFont), head(headFont());
    double leftWidth = 0, rightWidth = 0;
    for (const Graph::Source &s : std::as_const(graph.sources)) leftWidth = std::max(leftWidth, node.horizontalAdvance(s.name));
    for (const Graph::Listed &l : std::as_const(graph.listed))
        leftWidth = std::max(leftWidth, node.horizontalAdvance(l.name) + 12 + node.horizontalAdvance(QString::number(l.sources.size())));
    for (int p = 0; p < np; ++p) rightWidth = std::max(rightWidth, node.horizontalAdvance(partLabel(p)));
    for (const auto &h : std::as_const(m_rightHeads)) rightWidth = std::max(rightWidth, head.horizontalAdvance(h.second));
    m_leftAnchor = Margin + 16 + std::min(leftWidth, 280.0) + 14;
    m_rightAnchor = std::max(m_leftAnchor + 160, width() - Margin - std::min(rightWidth, 240.0) - 16);

    // The fusion block: two thirds of the way across, level with the parts it feeds.
    m_stageRect = QRectF();
    if (graph.stage.present) {
        double sy = 0;
        for (int p : graph.stage.parts) sy += m_partY[p];
        sy /= graph.stage.parts.size();
        const double cx = m_leftAnchor + (m_rightAnchor - m_leftAnchor) * 0.66;
        m_stageRect = QRectF(cx - 58, sy - 23, 116, 46);
    }

    // The disc: the columns' heights become arcs of up to 77 degrees each side of the middle (a little tighter than
    // the columns: the overview is small), the rows keep their order and spacing along them.
    double top = 1e9, bottom = -1e9;
    for (double v : std::as_const(m_sourceY)) { top = std::min(top, v); bottom = std::max(bottom, v); }
    for (double v : std::as_const(m_listedY)) { top = std::min(top, v); bottom = std::max(bottom, v); }
    for (const auto &h : std::as_const(m_leftHeads)) { top = std::min(top, h.first); bottom = std::max(bottom, h.first); }
    const double span = std::max(1.0, bottom - top), middle = (top + bottom) / 2, squeeze = 0.8, maxAngle = 1.35;
    m_discRadius = squeeze * span / (2 * maxAngle);
    m_discCentre = QPointF((m_leftAnchor + m_rightAnchor) / 2, middle);
    const double rowOut = std::max(m_leftAnchor + 4 - Margin + 8, std::min(rightWidth, 240.0) + 40);
    m_plateRadius = m_discRadius + rowOut;
    auto angleOf = [&](double y) { return std::clamp(squeeze * (y - middle) / m_discRadius, -maxAngle, maxAngle); };
    auto onArc = [&](double y, bool left) {
        const double angle = angleOf(y);
        return m_discCentre + QPointF((left ? -1 : 1) * m_discRadius * std::cos(angle), m_discRadius * std::sin(angle));
    };
    // The turn that points a row outward: a left row runs leftward from its anchor, a right row rightward.
    auto leftTurn = [&](double y) { return -angleOf(y); };
    m_sourceMove.resize(ns);
    m_sourceTurn.resize(ns);
    for (int i = 0; i < ns; ++i) {
        m_sourceMove[i] = onArc(m_sourceY[i], true) - QPointF(m_leftAnchor + 4, m_sourceY[i]);
        m_sourceTurn[i] = leftTurn(m_sourceY[i]);
    }
    m_listedMove.resize(graph.listed.size());
    m_listedTurn.resize(graph.listed.size());
    for (int i = 0; i < graph.listed.size(); ++i) {
        m_listedMove[i] = onArc(m_listedY[i], true) - QPointF(m_leftAnchor + 4, m_listedY[i]);
        m_listedTurn[i] = leftTurn(m_listedY[i]);
    }
    m_leftHeadMove.resize(m_leftHeads.size());
    m_leftHeadTurn.resize(m_leftHeads.size());
    for (int i = 0; i < m_leftHeads.size(); ++i) {
        m_leftHeadMove[i] = onArc(m_leftHeads[i].first, true) - QPointF(m_leftAnchor + 4, m_leftHeads[i].first);
        m_leftHeadTurn[i] = leftTurn(m_leftHeads[i].first);
    }
    // The parts spread over the same arc on the right (their column is spread over the sources' height).
    double ptop = 1e9, pbottom = -1e9;
    for (double v : std::as_const(m_partY)) { ptop = std::min(ptop, v); pbottom = std::max(pbottom, v); }
    for (const auto &h : std::as_const(m_rightHeads)) { ptop = std::min(ptop, h.first); pbottom = std::max(pbottom, h.first); }
    auto rightY = [&](double y) {
        const double u = pbottom > ptop ? (y - ptop) / (pbottom - ptop) : 0.5;
        return top + u * span;
    };
    m_partMove.resize(np);
    m_partTurn.resize(np);
    for (int i = 0; i < np; ++i) {
        m_partMove[i] = onArc(rightY(m_partY[i]), false) - QPointF(m_rightAnchor - 4, m_partY[i]);
        m_partTurn[i] = angleOf(rightY(m_partY[i]));
    }
    m_rightHeadMove.resize(m_rightHeads.size());
    m_rightHeadTurn.resize(m_rightHeads.size());
    for (int i = 0; i < m_rightHeads.size(); ++i) {
        m_rightHeadMove[i] = onArc(rightY(m_rightHeads[i].first), false) - QPointF(m_rightAnchor - 4, m_rightHeads[i].first);
        m_rightHeadTurn[i] = angleOf(rightY(m_rightHeads[i].first));
    }
    // The fusion block: on the way from the centre to the parts it feeds.
    m_stageMove = QPointF();
    if (!m_stageRect.isNull()) {
        QPointF towards;
        for (int p : graph.stage.parts) towards += QPointF(m_rightAnchor - 4, m_partY[p]) + m_partMove[p];
        towards /= graph.stage.parts.size();
        m_stageMove = m_discCentre + (towards - m_discCentre) * 0.45 - m_stageRect.center();
    }
    computeFloors();
    computePlates();
    place(true);
}

// The two floors, one above the other, and the camera that goes from one to the other: at each floor, that floor has the
// view to itself, the other showing only past its edge. The disc takes the scale it has alone; the floor below fills
// the view too, its rows spread over it with their details (its labels stay level). All at the camera on that floor;
// underShift() and projection() move them by the camera's place.
void Canvas::computeFloors()
{
    const QSizeF view = m_viewport.isValid() ? QSizeF(m_viewport) : QSizeF(size());
    m_floorFit = std::min((view.width() - 60) / (2 * m_plateRadius), (view.height() - 70) / m_plateRadius);
    m_underB = std::max(120.0, std::min((view.height() - 70) / 2, (view.width() - 60) / 4));
    m_underCentre = {view.width() / 2, view.height() / 2};
    // From one floor's centre to the other's: the floor below starts a quarter of its height up under the disc.
    m_floorDistance = m_plateRadius * m_floorFit / 2 + m_underB * 0.75;

    const QFontMetricsF fm(underFont());
    QFont detailFont = underFont();
    detailFont.setPixelSize(11);
    const QFontMetricsF detail(detailFont);
    double leftWidth = 0, rightWidth = 0;
    int leftGaps = 0, rightGaps = 0;
    for (int i = 0; i < graph.accesses.size(); ++i) {
        leftWidth = std::max({leftWidth, fm.horizontalAdvance(graph.accesses[i].name), detail.horizontalAdvance(accessDetail(i))});
        leftGaps += i > 0 && graph.accesses[i].skill != graph.accesses[i - 1].skill;
    }
    for (int i = 0; i < graph.ais.size(); ++i) {
        rightWidth = std::max({rightWidth, fm.horizontalAdvance(graph.ais[i].name), detail.horizontalAdvance(aiDetail(i))});
        rightGaps += i > 0 && graph.ais[i].kind != graph.ais[i - 1].kind;
    }
    const double labelMax = m_underB * 0.8;
    m_underLabel = std::min(std::max(leftWidth, rightWidth) + 4, labelMax);
    const int steps = std::max(1, int(std::max(graph.accesses.size(), graph.ais.size())) - 1);
    const double gap = 10;
    m_underRow = std::clamp((1.35 * m_underB - std::max(leftGaps, rightGaps) * gap) / steps, 16.0, 40.0);
    const double leftHeight = (graph.accesses.size() - 1) * m_underRow + leftGaps * gap;
    const double rightHeight = (graph.ais.size() - 1) * m_underRow + rightGaps * gap;

    // Each side's dots follow the rim, its longest label's width inside it.
    auto spread = [&](QVector<QPointF> &at, int n, double height, bool left, auto gapBefore) {
        const double label = std::min(left ? leftWidth : rightWidth, labelMax) + 30;
        at.resize(n);
        double y = m_underCentre.y() - height / 2 - 8;
        for (int i = 0; i < n; ++i) {
            if (i > 0) y += m_underRow + (gapBefore(i) ? gap : 0);
            // The rim at the row's second line where it's the nearer.
            const double v = std::max(std::abs(y - m_underCentre.y()), std::abs(y + 22 - m_underCentre.y())) / m_underB;
            const double rim = 2 * m_underB * std::sqrt(std::max(0.0, 1 - v * v));
            const double x = std::max(rim - label, 0.3 * rim);
            at[i] = {m_underCentre.x() + (left ? -x : x), y};
        }
    };
    spread(m_accessAt, int(graph.accesses.size()), leftHeight, true,
           [&](int i) { return graph.accesses[i].skill != graph.accesses[i - 1].skill; });
    spread(m_aiAt, int(graph.ais.size()), rightHeight, false,
           [&](int i) { return graph.ais[i].kind != graph.ais[i - 1].kind; });
    m_underLinks.resize(graph.aiLinks.size());
    for (int i = 0; i < graph.aiLinks.size(); ++i) {
        const QPointF a = m_accessAt[graph.aiLinks[i].access], b = m_aiAt[graph.aiLinks[i].ai];
        QPainterPath path(a);
        path.cubicTo(a + (m_underCentre - a) * 0.6, b + (m_underCentre - b) * 0.6, b);
        m_underLinks[i] = path;
    }
    m_underHits.clear();
}

// The shafts, from the rows of the disc as they are now down to the floor below.
void Canvas::placeShafts()
{
    m_shafts.clear();
    if (!m_overview || m_mode != Disc || underOpacity() <= 0) return;
    const QTransform m = projection(m_tilt);
    const QPointF shift = underShift();
    // Down to the floor inside its first dot, where they run side by side, then out to their dots from the inside:
    // the names are on the outside, and no shaft crosses one.
    double inner = -1e9;
    for (const QPointF &at : std::as_const(m_accessAt)) inner = std::max(inner, at.x());
    const double top = m_accessAt.isEmpty() ? 0 : m_accessAt.first().y() - 34;
    auto shaft = [&](int access, int source, int part, bool stage, const QPointF &from) {
        const QPointF to = m_accessAt[access] + shift;
        const QPointF riser = QPointF(inner + 22 + access * 6, top) + shift;
        const double bend = std::max(30.0, (riser.y() - from.y()) * 0.55);
        QPainterPath path(from);
        path.cubicTo(from + QPointF(0, bend), riser - QPointF(0, bend), riser);
        path.cubicTo(riser + QPointF(0, (to.y() - riser.y()) * 0.6), to + QPointF((riser.x() - to.x()) * 0.5, 0), to);
        m_shafts.push_back({access, source, part, stage, path});
    };
    for (int a = 0; a < graph.accesses.size(); ++a) {
        const Graph::Access &x = graph.accesses[a];
        for (int s : x.sources) shaft(a, s, -1, false, m.map(QPointF(m_leftAnchor + 4, m_sourceY[s]) + moved(m_sourceMove[s])));
        for (int p : x.parts) shaft(a, -1, p, false, m.map(QPointF(m_rightAnchor - 4, m_partY[p]) + moved(m_partMove[p])));
        if (x.stage && !m_stageRect.isNull()) shaft(a, -1, -1, true, stageMatrix().map(m_stageRect.center()));
    }
}

QTransform Canvas::rowMatrix(RowKind kind, int i) const
{
    QPointF anchor, move;
    double turn = 0;
    const QVector<QTransform> *plate = nullptr;
    switch (kind) {
    case SourceRowKind: anchor = {m_leftAnchor + 4, m_sourceY[i]}; move = m_sourceMove[i]; turn = m_sourceTurn[i]; plate = &m_plateSource; break;
    case ListedRowKind: anchor = {m_leftAnchor + 4, m_listedY[i]}; move = m_listedMove[i]; turn = m_listedTurn[i]; plate = &m_plateListed; break;
    case PartRowKind: anchor = {m_rightAnchor - 4, m_partY[i]}; move = m_partMove[i]; turn = m_partTurn[i]; plate = &m_platePart; break;
    case LeftHeadKind: anchor = {m_leftAnchor + 4, m_leftHeads[i].first}; move = m_leftHeadMove[i]; turn = m_leftHeadTurn[i]; plate = &m_plateLeftHead; break;
    case RightHeadKind: anchor = {m_rightAnchor - 4, m_rightHeads[i].first}; move = m_rightHeadMove[i]; turn = m_rightHeadTurn[i]; plate = &m_plateRightHead; break;
    }
    if (platesShown() && i < plate->size()) {
        // From the columns as scrolled to the plate, coefficient by coefficient.
        const QTransform from = QTransform::fromTranslate(0, -m_scroll), &to = (*plate)[i];
        const double t = m_tilt;
        auto mix = [t](double a, double b) { return a + (b - a) * t; };
        return QTransform(mix(from.m11(), to.m11()), mix(from.m12(), to.m12()), mix(from.m21(), to.m21()),
                          mix(from.m22(), to.m22()), mix(from.dx(), to.dx()), mix(from.dy(), to.dy()));
    }
    return rowTransform(anchor, move, turn) * projection(m_tilt);
}

QTransform Canvas::stageMatrix() const
{
    if (platesShown()) {
        const QTransform from = QTransform::fromTranslate(0, -m_scroll), &to = m_plateStage;
        const double t = m_tilt;
        auto mix = [t](double a, double b) { return a + (b - a) * t; };
        return QTransform(mix(from.m11(), to.m11()), mix(from.m12(), to.m12()), mix(from.m21(), to.m21()),
                          mix(from.m22(), to.m22()), mix(from.dx(), to.dx()), mix(from.dy(), to.dy()));
    }
    return QTransform::fromTranslate(moved(m_stageMove).x(), moved(m_stageMove).y()) * projection(m_tilt);
}

void Canvas::computePlates()
{
    const QSizeF view = m_viewport.isValid() ? QSizeF(m_viewport) : QSizeF(size());
    // Isometric axes on the screen: a plate's width runs down to the right, its height straight down, and the plates
    // stand one behind the other up to the right.
    const QPointF across(std::cos(M_PI / 6), std::sin(M_PI / 6)), down(0, 1), depth(std::cos(M_PI / 6), -std::sin(M_PI / 6));
    const double cell = SourceRow, title = 40, pad = 18, gap = 520;

    // The rows of each side in the columns' order, headings included, filled into columns on their plate.
    struct Entry { RowKind kind; int index; double y; };
    QVector<Entry> left, right;
    for (int i = 0; i < m_leftHeads.size(); ++i) left.push_back({LeftHeadKind, i, m_leftHeads[i].first - 3});
    for (int i = 0; i < m_sourceY.size(); ++i) left.push_back({SourceRowKind, i, m_sourceY[i]});
    for (int i = 0; i < m_listedY.size(); ++i) left.push_back({ListedRowKind, i, m_listedY[i]});
    for (int i = 0; i < m_rightHeads.size(); ++i) right.push_back({RightHeadKind, i, m_rightHeads[i].first - 3});
    for (int i = 0; i < m_partY.size(); ++i) right.push_back({PartRowKind, i, m_partY[i]});
    auto byY = [](const Entry &a, const Entry &b) { return a.y < b.y; };
    std::stable_sort(left.begin(), left.end(), byY);
    std::stable_sort(right.begin(), right.end(), byY);
    const double leftSpan = m_leftAnchor + 8 - (Margin - 8);                    // a source row, anchor at its right end
    const double rightSpan = std::min(width() - Margin + 6 - (m_rightAnchor - 6), 260.0);   // a part row, anchor at its left end
    const int leftCols = std::max(1, int(std::ceil(left.size() / 24.0)));
    const int leftRows = std::max(1, int(std::ceil(double(left.size()) / leftCols)));
    const int rightCols = std::max(1, int(std::ceil(right.size() / 14.0)));
    const int rightRows = std::max(1, int(std::ceil(double(right.size()) / rightCols)));
    const QSizeF leftSize(leftCols * (leftSpan + 30) + 2 * pad - 30, title + leftRows * cell + pad);
    const QSizeF rightSize(rightCols * (rightSpan + 30) + 2 * pad - 30, title + rightRows * cell + pad);
    const bool fusion = !m_stageRect.isNull();
    // The middle plate covers where the links go through it: halfway between their ends, so the mean of the other two.
    const QSizeF middleSize((leftSize.width() + rightSize.width()) / 2 + 40, (leftSize.height() + rightSize.height()) / 2 + 40);

    QVector<QSizeF> sizes {leftSize};
    m_plateTitle = {QCoreApplication::translate("SourceFlowView", "Sources")};
    if (fusion) {
        sizes << middleSize;
        m_plateTitle << QCoreApplication::translate("SourceFlowView", "Fusion");
    }
    sizes << rightSize;
    m_plateTitle << QCoreApplication::translate("SourceFlowView", "The app");
    // Each plate's frame before fitting: its top left corner where its centre is `gap` deeper than the one before.
    QVector<QTransform> frames;
    QVector<QPolygonF> outlines;
    QRectF bounds;
    for (int k = 0; k < sizes.size(); ++k) {
        const QPointF centre = depth * (gap * k);
        const QPointF origin = centre - across * (sizes[k].width() / 2) - down * (sizes[k].height() / 2);
        const QTransform frame(across.x(), across.y(), down.x(), down.y(), origin.x(), origin.y());
        frames << frame;
        const QPolygonF outline = frame.map(QPolygonF(QRectF(QPointF(0, 0), sizes[k])));
        outlines << outline;
        bounds = bounds.united(outline.boundingRect());
    }
    const double scale = std::min((view.width() - 60) / bounds.width(), (view.height() - 60) / bounds.height());
    const QTransform fit = QTransform::fromTranslate(-bounds.center().x(), -bounds.center().y()) * QTransform::fromScale(scale, scale)
                           * QTransform::fromTranslate(view.width() / 2, view.height() / 2);
    m_plateFrame.clear();
    m_plateOutline.clear();
    for (int k = 0; k < frames.size(); ++k) {
        m_plateFrame << frames[k] * fit;
        m_plateOutline << fit.map(outlines[k]);
    }
    m_plateDepth = QPointF(fit.map(depth * (gap / 4)) - fit.map(QPointF(0, 0)));

    // A row onto its plate: its anchor at its cell's place, the rest of the row around it as in the columns.
    auto onPlate = [&](int plate, const QPointF &anchor, const QPointF &at) {
        return QTransform::fromTranslate(at.x() - anchor.x(), at.y() - anchor.y()) * m_plateFrame[plate];
    };
    m_plateSource.resize(m_sourceY.size());
    m_plateListed.resize(m_listedY.size());
    m_plateLeftHead.resize(m_leftHeads.size());
    for (int k = 0; k < left.size(); ++k) {
        const int col = k / leftRows, row = k % leftRows;
        const QPointF at(pad + col * (leftSpan + 30) + (m_leftAnchor + 4 - (Margin - 8)), title + row * cell + cell / 2);
        const Entry &e = left[k];
        if (e.kind == LeftHeadKind)
            m_plateLeftHead[e.index] = onPlate(0, {m_leftAnchor + 4, m_leftHeads[e.index].first}, at + QPointF(0, 4));
        else if (e.kind == SourceRowKind)
            m_plateSource[e.index] = onPlate(0, {m_leftAnchor + 4, m_sourceY[e.index]}, at);
        else
            m_plateListed[e.index] = onPlate(0, {m_leftAnchor + 4, m_listedY[e.index]}, at);
    }
    const int back = int(sizes.size()) - 1;
    m_platePart.resize(m_partY.size());
    m_plateRightHead.resize(m_rightHeads.size());
    for (int k = 0; k < right.size(); ++k) {
        const int col = k / rightRows, row = k % rightRows;
        const QPointF at(pad + col * (rightSpan + 30) + 2, title + row * cell + cell / 2);
        const Entry &e = right[k];
        if (e.kind == RightHeadKind)
            m_plateRightHead[e.index] = onPlate(back, {m_rightAnchor - 4, m_rightHeads[e.index].first}, at + QPointF(0, 4));
        else
            m_platePart[e.index] = onPlate(back, {m_rightAnchor - 4, m_partY[e.index]}, at);
    }
    if (fusion)
        m_plateStage = onPlate(1, m_stageRect.center(), QPointF(middleSize.width() / 2, middleSize.height() / 2 + title / 4));
}

void Canvas::place(bool)
{
    m_paths.resize(graph.links.size());
    m_hits.clear();   // made again when the mouse needs them (stroking 92 curves takes tens of milliseconds)
    const double t = m_tilt;
    if (platesShown()) {
        // On the canvas: each link leaves its plate square to it (along the depth) and is cut where it goes through
        // the middle plate, so the plates can be drawn between its halves.
        m_frontPaths.resize(graph.links.size());
        m_backPaths.resize(graph.links.size());
        const QTransform from = QTransform::fromTranslate(0, -m_scroll), stage = stageMatrix();
        auto halves = [&](const QPointF &a, const QPointF &b, const QPointF &flatA, const QPointF &flatB,
                          QPainterPath &front, QPainterPath &back, bool split) {
            const double dx = (flatB.x() - flatA.x()) * 0.5;
            const QPointF flat1 = from.map(flatA + QPointF(dx, 0)), flat2 = from.map(flatB - QPointF(dx, 0));
            const QPointF c1 = flat1 + (a + m_plateDepth - flat1) * t, c2 = flat2 + (b - m_plateDepth - flat2) * t;
            if (!split) {
                front.moveTo(a);
                front.cubicTo(c1, c2, b);
                return;
            }
            const QPointF p01 = (a + c1) / 2, p12 = (c1 + c2) / 2, p23 = (c2 + b) / 2, p012 = (p01 + p12) / 2,
                          p123 = (p12 + p23) / 2, mid = (p012 + p123) / 2;
            front.moveTo(a);
            front.cubicTo(p01, p012, mid);
            back.moveTo(mid);
            back.cubicTo(p123, p23, b);
        };
        const bool middle = !m_stageRect.isNull();
        for (int i = 0; i < graph.links.size(); ++i) {
            const Graph::Link &l = graph.links[i];
            const QPointF flatA(m_leftAnchor + 4, m_sourceY[l.source]), flatB(m_rightAnchor - 4, m_partY[l.part]);
            const QPointF a = rowMatrix(SourceRowKind, l.source).map(flatA), b = rowMatrix(PartRowKind, l.part).map(flatB);
            QPainterPath front, back;
            if (l.staged && middle) {
                const QPointF inL(m_stageRect.left(), m_stageRect.center().y()), outR(m_stageRect.right(), m_stageRect.center().y());
                QPainterPath unused;
                halves(a, stage.map(inL), flatA, inL, front, unused, false);
                halves(stage.map(outR), b, outR, flatB, back, unused, false);
            } else {
                halves(a, b, flatA, flatB, front, back, middle);
            }
            m_frontPaths[i] = front;
            m_backPaths[i] = back;
            QPainterPath all = front;
            all.addPath(back);
            m_paths[i] = all;
        }
        return;
    }
    m_frontPaths.clear();
    m_backPaths.clear();
    // A curve from a to b: level at both ends in the columns, bent through the disc's middle on it.
    auto curve = [&](QPainterPath &path, const QPointF &a, const QPointF &b) {
        const double dx = (b.x() - a.x()) * 0.5;
        const QPointF flat1 = a + QPointF(dx, 0), flat2 = b - QPointF(dx, 0);
        const QPointF disc1 = a + (m_discCentre - a) * 0.55, disc2 = b + (m_discCentre - b) * 0.55;
        path.cubicTo(flat1 + (disc1 - flat1) * t, flat2 + (disc2 - flat2) * t, b);
    };
    const QRectF stage = m_stageRect.translated(moved(m_stageMove));
    for (int i = 0; i < graph.links.size(); ++i) {
        const Graph::Link &l = graph.links[i];
        const QPointF a = QPointF(m_leftAnchor + 4, m_sourceY[l.source]) + moved(m_sourceMove[l.source]);
        const QPointF b = QPointF(m_rightAnchor - 4, m_partY[l.part]) + moved(m_partMove[l.part]);
        QPainterPath path(a);
        if (l.staged && !m_stageRect.isNull()) {
            // Into the block on its left, out of it on its right.
            const double y = stage.center().y();
            curve(path, a, QPointF(stage.left(), y));
            path.moveTo(stage.right(), y);
            curve(path, QPointF(stage.right(), y), b);
        } else {
            curve(path, a, b);
        }
        m_paths[i] = path;
    }
    placeShafts();
}

void Canvas::ensureHits() const
{
    if (m_hits.size() == m_paths.size()) return;
    QPainterPathStroker stroker;
    stroker.setWidth(7);
    m_hits.resize(m_paths.size());
    for (int i = 0; i < m_paths.size(); ++i) m_hits[i] = stroker.createStroke(m_paths[i]);
}

Canvas::Item Canvas::itemAt(const QPointF &pos) const
{
    // On the disc view, only the floor in focus answers the mouse.
    if (discShown() && m_floor == Underground) {
        Item below;
        return underAt(pos, &below) ? below : Item();
    }
    // Each row where it is now (on the disc or a plate in an overview), as wide as in the columns.
    const double partRow = std::min(m_partRow, 26.0 + (m_partRow - 26.0) * (1 - m_tilt));
    for (int s = 0; s < graph.sources.size(); ++s) {
        const QPointF q = rowMatrix(SourceRowKind, s).inverted().map(pos);
        if (q.x() > Margin - 8 && q.x() < m_leftAnchor + 8 && std::abs(q.y() - m_sourceY[s]) < SourceRow / 2) return {Source, s};
    }
    for (int l = 0; l < graph.listed.size(); ++l) {
        const QPointF q = rowMatrix(ListedRowKind, l).inverted().map(pos);
        if (q.x() > Margin - 8 && q.x() < m_leftAnchor + 8 && std::abs(q.y() - m_listedY[l]) < SourceRow / 2) return {Listed, l};
    }
    for (int p = 0; p < graph.parts.size(); ++p) {
        const QPointF q = rowMatrix(PartRowKind, p).inverted().map(pos);
        if (q.x() > m_rightAnchor - 8 && q.x() < width() && std::abs(q.y() - m_partY[p]) < partRow / 2) return {Part, p};
    }
    if (!m_stageRect.isNull() && m_stageRect.adjusted(-4, -4, 4, 4).contains(stageMatrix().inverted().map(pos))) return {Stage, 0};
    // The links: on the canvas for the plates, in the diagram's coordinates otherwise.
    const QPointF q = platesShown() ? pos : projection(m_tilt).inverted().map(pos);
    if (m_tilt > 0 || (q.x() > m_leftAnchor && q.x() < m_rightAnchor)) {
        // The links in view first (drawn on top), then the others.
        ensureHits();
        const Item s = shown();
        for (int pass = 0; pass < 2; ++pass)
            for (int i = int(m_hits.size()) - 1; i >= 0; --i) {
                const Graph::Link &l = graph.links[i];
                const bool lit = (s.kind == Source && l.source == s.index) || (s.kind == Part && l.part == s.index)
                                 || (s.kind == Link && i == s.index) || (s.kind == Stage && l.staged);
                if ((pass == 0) == lit && m_hits[i].contains(q)) return {Link, i};
            }
    }
    return {};
}

// The underground floor's rows and links at a point of the canvas; not through the disc above, which hides them.
bool Canvas::underAt(const QPointF &pos, Item *found) const
{
    if (!m_overview || m_mode != Disc || m_tilt < 1) return false;
    const QPointF at = pos - underShift();
    const QFontMetricsF fm(underFont());
    for (int i = 0; i < m_accessAt.size(); ++i) {
        const double w = std::min(fm.horizontalAdvance(graph.accesses[i].name), m_underLabel) + 18;
        if (QRectF(m_accessAt[i].x() - w, m_accessAt[i].y() - 11, w + 8, m_underRow).contains(at)) {
            *found = {Access, i};
            return true;
        }
    }
    for (int i = 0; i < m_aiAt.size(); ++i) {
        const double w = std::min(fm.horizontalAdvance(graph.ais[i].name), m_underLabel) + 18;
        if (QRectF(m_aiAt[i].x() - 8, m_aiAt[i].y() - 11, w + 8, m_underRow).contains(at)) {
            *found = {Ai, i};
            return true;
        }
    }
    const QPointF plan = projection(1).inverted().map(pos);
    if (QLineF(plan, m_discCentre).length() <= m_plateRadius) return false;
    if (m_underHits.size() != m_underLinks.size()) {
        QPainterPathStroker stroker;
        stroker.setWidth(7);
        m_underHits.resize(m_underLinks.size());
        for (int i = 0; i < m_underLinks.size(); ++i) m_underHits[i] = stroker.createStroke(m_underLinks[i]);
    }
    for (int i = int(m_underHits.size()) - 1; i >= 0; --i)
        if (m_underHits[i].contains(at)) {
            *found = {AiLink, i};
            return true;
        }
    return false;
}

QTransform Canvas::projection(double t) const
{
    if (!m_overview || m_mode != Disc) return {};
    const QPointF c = m_discCentre;
    // Isometric (2:1): turned 45 degrees, half as high; the disc becomes an ellipse twice as wide as high. The
    // scale that fits it in the view.
    const QSizeF view = m_viewport.isValid() ? QSizeF(m_viewport) : QSizeF(size());
    const double scale = 1 + (m_floorFit - 1) * t;
    // From the centre as it was scrolled to the middle of the view, less the camera's way down.
    const QPointF from(c.x(), c.y() - m_scroll), to(view.width() / 2, view.height() / 2 - m_floorDistance * m_focus);
    const QPointF centre = from + (to - from) * t;
    return QTransform::fromTranslate(-c.x(), -c.y()) * QTransform().rotate(45 * t) * QTransform::fromScale(1, 1 - 0.5 * t)
           * QTransform::fromScale(scale, scale) * QTransform::fromTranslate(centre.x(), centre.y());
}

void Canvas::paintEvent(QPaintEvent *)
{
    if (inspecting()) {
        QPainter p(this);
        paintInspect(p);
        return;
    }
    // While the overview moves, each frame is drawn at one pixel per point, a quarter of a Retina screen's pixels,
    // and shown scaled up: the 92 links and the turned labels are too slow to draw at full resolution 60 times a
    // second. At rest (the columns, the disc) it is drawn sharp.
    if (m_anim->state() == QAbstractAnimation::Running || m_focusAnim->state() == QAbstractAnimation::Running
        || (m_overview && m_tilt > 0 && m_tilt < 1)) {
        if (m_frame.size() != size()) m_frame = QImage(size(), QImage::Format_ARGB32_Premultiplied);
        m_frame.fill(Qt::transparent);
        {
            QPainter fp(&m_frame);
            fp.setRenderHint(QPainter::SmoothPixmapTransform);
            m_fast = true;
            paintContent(fp);
            m_fast = false;
        }
        QPainter p(this);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        p.drawImage(rect(), m_frame);
        return;
    }
    m_frame = QImage();
    QPainter p(this);
    paintContent(p);
}

void Canvas::drawLabel(QPainter &p, const QRectF &box, int flags, const QString &text)
{
    if (!m_fast) {
        p.drawText(box, flags, text);
        return;
    }
    const QFont f = p.font();
    const QColor c = p.pen().color();
    const QFontMetricsF fm(f);
    const QSizeF size(std::ceil(fm.horizontalAdvance(text)) + 2, std::ceil(fm.height()) + 2);
    if (m_labels.size() > 4000) m_labels.clear();
    QImage &image = m_labels[text + QChar(1) + f.key() + QChar(1) + QString::number(c.rgba())];
    if (image.isNull()) {
        image = QImage(size.toSize(), QImage::Format_ARGB32_Premultiplied);   // one pixel per point, as the frame
        image.fill(Qt::transparent);
        QPainter ip(&image);
        ip.setFont(f);
        ip.setPen(c);
        ip.drawText(QRectF(QPointF(0, 0), size), Qt::AlignLeft | Qt::AlignVCenter, text);
    }
    const double x = (flags & Qt::AlignRight) ? box.right() - size.width()
                     : (flags & Qt::AlignHCenter) ? box.center().x() - size.width() / 2 : box.left();
    p.drawImage(QRectF(QPointF(x, box.center().y() - size.height() / 2), size), image);
}

void Canvas::drawLabel(QPainter &p, const QPointF &baseline, const QString &text)
{
    if (!m_fast) {
        p.drawText(baseline, text);
        return;
    }
    const QFontMetricsF fm(p.font());
    drawLabel(p, QRectF(baseline.x(), baseline.y() - fm.ascent() - 1, 1e6, fm.height() + 2), Qt::AlignLeft | Qt::AlignVCenter, text);
}

Canvas::Lit Canvas::lighting() const
{
    Lit lit;
    lit.shown = shown();
    lit.active = lit.shown.kind != None;
    const Item &s = lit.shown;
    for (int i = 0; i < graph.links.size(); ++i) {
        const Graph::Link &l = graph.links[i];
        if ((s.kind == Source && l.source == s.index) || (s.kind == Part && l.part == s.index) || (s.kind == Link && i == s.index)
            || (s.kind == Stage && l.staged)) {
            lit.links.insert(i);
            lit.sources.insert(l.source);
            lit.parts.insert(l.part);
        }
    }
    // The underground floor: from an item above, what reaches it; from an AI, an access or a link below, what it reaches.
    const bool fromBelow = s.kind == Ai || s.kind == Access || s.kind == AiLink;
    for (int a = 0; a < graph.accesses.size(); ++a) {
        const Graph::Access &x = graph.accesses[a];
        if ((s.kind == Source && x.sources.contains(s.index)) || (s.kind == Part && x.parts.contains(s.index))
            || (s.kind == Stage && x.stage) || (s.kind == Access && s.index == a))
            lit.accesses.insert(a);
    }
    for (int i = 0; i < graph.aiLinks.size(); ++i) {
        const Graph::AiLink &l = graph.aiLinks[i];
        if ((s.kind == Ai && l.ai == s.index) || (s.kind == AiLink && s.index == i)) lit.accesses.insert(l.access);
    }
    for (int i = 0; i < graph.aiLinks.size(); ++i) {
        const Graph::AiLink &l = graph.aiLinks[i];
        if ((s.kind == Ai && l.ai == s.index) || (s.kind == AiLink && s.index == i)
            || (s.kind != Ai && s.kind != AiLink && lit.accesses.contains(l.access))) {
            lit.aiLinks.insert(i);
            lit.ais.insert(l.ai);
        }
    }
    if (s.kind == Ai) lit.ais.insert(s.index);
    if (fromBelow)
        for (int a : std::as_const(lit.accesses)) {
            const Graph::Access &x = graph.accesses[a];
            for (int v : x.sources) lit.sources.insert(v);
            for (int v : x.parts) lit.parts.insert(v);
            if (x.stage) {
                lit.stage = true;
                for (int v : graph.stage.sources) lit.sources.insert(v);
            }
        }
    for (int i = 0; i < m_shafts.size(); ++i) {
        const Shaft &h = m_shafts[i];
        if (!lit.accesses.contains(h.access)) continue;
        if (fromBelow || (s.kind == Source && h.source == s.index) || (s.kind == Part && h.part == s.index) || (s.kind == Stage && h.stage))
            lit.shafts.insert(i);
    }
    return lit;
}

void Canvas::drawLinks(QPainter &p, const Lit &lit, const QVector<QPainterPath> &paths)
{
    const Palette &ui = Theme::current();
    auto linkPen = [&](int i, bool on) {
        const Graph::Link &l = graph.links[i];
        QColor c;
        if (on) {
            c = l.state == Row::Bad ? ui.Down : l.state == Row::Off ? ui.TextDim : ui.Accent;
            c.setAlphaF(0.9);
        } else {
            c = l.state == Row::Bad ? ui.Down : ui.TextDim;
            c.setAlphaF(lit.active ? 0.09 : (l.state == Row::Bad ? 0.55 : 0.32));
        }
        QPen pen(c, std::min(1.2 + 0.7 * (l.rows.size() - 1), 4.5));
        pen.setCapStyle(Qt::RoundCap);
        if (m_fast) {
            // A one-pixel line takes Qt's fast cosmetic stroker; the thickness comes back at rest.
            pen.setCosmetic(true);
            pen.setWidthF(1.0);
        }
        if (l.state == Row::Off) pen.setDashPattern({3, 3});
        return pen;
    };
    p.setBrush(Qt::NoBrush);
    for (int i = 0; i < paths.size(); ++i)
        if (!lit.links.contains(i)) {
            p.setPen(linkPen(i, false));
            p.drawPath(paths[i]);
        }
    for (int i : std::as_const(lit.links))
        if (i < paths.size()) {
            p.setPen(linkPen(i, true));
            p.drawPath(paths[i]);
        }
}

void Canvas::drawStage(QPainter &p, const Lit &lit)
{
    if (m_stageRect.isNull()) return;
    const Palette &ui = Theme::current();
    p.save();
    p.setTransform(stageMatrix());
    bool on = lit.shown.kind == Stage;
    for (int i : std::as_const(lit.links)) on = on || graph.links[i].staged;
    on = on || lit.stage;
    const bool dim = lit.active && !on;
    p.setBrush(lit.shown.kind == Stage || (pinned.kind == Stage) ? ui.Hover : ui.Panel);
    p.setPen(QPen(on ? ui.Accent : dim ? ui.PanelLine : ui.TextDim, on ? 1.5 : 1.1));
    p.drawRoundedRect(m_stageRect, 6, 6);
    p.setFont(headFont());
    p.setPen(on ? ui.Accent : dim ? ui.TextFaint : ui.TextSection);
    drawLabel(p, m_stageRect.adjusted(0, 7, 0, -m_stageRect.height() / 2), Qt::AlignHCenter | Qt::AlignVCenter,
              QCoreApplication::translate("SourceFlowView", "Fusion").toUpper());
    QFont small = nodeFont();
    small.setPixelSize(11);
    p.setFont(small);
    p.setPen(dim ? ui.TextFaint : ui.TextDim);
    drawLabel(p, m_stageRect.adjusted(4, m_stageRect.height() / 2, -4, -6), Qt::AlignHCenter | Qt::AlignVCenter,
              QCoreApplication::translate("SourceFlowView", "%n golden record(s)", nullptr, graph.stage.records));
    p.restore();
}

// The headings, sources and listed kinds of the left side, each where rowMatrix() puts it.
void Canvas::drawLeftRows(QPainter &p, const Lit &lit)
{
    const Palette &ui = Theme::current();
    const Item &s = lit.shown;
    p.setFont(headFont());
    p.setPen(ui.TextSection);
    for (int i = 0; i < m_leftHeads.size(); ++i) {
        p.setTransform(rowMatrix(LeftHeadKind, i));
        drawLabel(p, QPointF(Margin, m_leftHeads[i].first), m_leftHeads[i].second);
    }
    const QFont font = nodeFont();
    QFont bold = font;
    bold.setWeight(QFont::DemiBold);
    const QFontMetricsF fm(font);
    const bool againstAnchor = m_overview && m_mode == Disc && m_tilt > 0.5;   // on the disc, names end on the circle
    for (int i = 0; i < graph.sources.size(); ++i) {
        const Graph::Source &src = graph.sources[i];
        const double y = m_sourceY[i];
        p.setTransform(rowMatrix(SourceRowKind, i));
        const bool self = s.kind == Source && s.index == i;
        const bool dim = lit.active && !lit.sources.contains(i);
        if (self || (pinned.kind == Source && pinned.index == i))
            p.fillRect(QRectF(Margin - 6, y - SourceRow / 2 + 1, m_leftAnchor - Margin + 6, SourceRow - 2), ui.Hover);
        p.setPen(Qt::NoPen);
        QColor dot = stateColor(src.state);
        if (dim) dot.setAlphaF(0.4);
        p.setBrush(dot);
        p.drawEllipse(QPointF(Margin + 4, y), 3.5, 3.5);
        p.setFont(self ? bold : font);
        p.setPen(self ? ui.Accent : dim ? ui.TextFaint : ui.Text);
        const QString name = fm.elidedText(src.name, Qt::ElideRight, m_leftAnchor - Margin - 30);
        drawLabel(p, QRectF(Margin + 16, y - SourceRow / 2, m_leftAnchor - Margin - 30, SourceRow),
                  Qt::AlignVCenter | (againstAnchor ? Qt::AlignRight : Qt::AlignLeft), name);
        p.setPen(Qt::NoPen);
        p.setBrush(dim ? ui.PanelLine : lit.sources.contains(i) ? ui.Accent : ui.TextDim);
        p.drawEllipse(QPointF(m_leftAnchor + 4, y), 2.5, 2.5);
    }
    // The catalogue's kinds: a hollow dot, the name, how many bodies; no anchor, they feed nothing.
    for (int i = 0; i < graph.listed.size(); ++i) {
        const Graph::Listed &l = graph.listed[i];
        const double y = m_listedY[i];
        p.setTransform(rowMatrix(ListedRowKind, i));
        const bool self = s.kind == Listed && s.index == i;
        if (self || (pinned.kind == Listed && pinned.index == i))
            p.fillRect(QRectF(Margin - 6, y - SourceRow / 2 + 1, m_leftAnchor - Margin + 6, SourceRow - 2), ui.Hover);
        p.setPen(QPen(ui.TextFaint, 1.2));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPointF(Margin + 4, y), 3.2, 3.2);
        p.setFont(self ? bold : font);
        p.setPen(self ? ui.Accent : lit.active ? ui.TextFaint : ui.TextDim);
        const QRectF box(Margin + 16, y - SourceRow / 2, m_leftAnchor - Margin - 16, SourceRow);
        drawLabel(p, box, Qt::AlignVCenter | Qt::AlignLeft, l.name);
        p.setFont(font);
        p.setPen(ui.TextFaint);
        drawLabel(p, box, Qt::AlignVCenter | Qt::AlignRight, QLocale().toString(int(l.sources.size())));
    }
    p.resetTransform();
}

// The headings and parts of the app on the right side.
void Canvas::drawRightRows(QPainter &p, const Lit &lit)
{
    const Palette &ui = Theme::current();
    const Item &s = lit.shown;
    p.setFont(headFont());
    p.setPen(ui.TextSection);
    for (int i = 0; i < m_rightHeads.size(); ++i) {
        p.setTransform(rowMatrix(RightHeadKind, i));
        drawLabel(p, QPointF(m_rightAnchor + 12, m_rightHeads[i].first), m_rightHeads[i].second);
    }
    const QFont font = nodeFont();
    QFont bold = font;
    bold.setWeight(QFont::DemiBold);
    const QFontMetricsF fm(font);
    for (int i = 0; i < graph.parts.size(); ++i) {
        const double y = m_partY[i];
        p.setTransform(rowMatrix(PartRowKind, i));
        const bool self = s.kind == Part && s.index == i;
        const bool dim = lit.active && !lit.parts.contains(i);
        if (self || (pinned.kind == Part && pinned.index == i))
            p.fillRect(QRectF(m_rightAnchor - 6, y - 11, width() - m_rightAnchor - Margin + 12, 22), ui.Hover);
        p.setPen(Qt::NoPen);
        p.setBrush(dim ? ui.PanelLine : lit.parts.contains(i) ? ui.Accent : ui.TextDim);
        p.drawEllipse(QPointF(m_rightAnchor - 4, y), 3.5, 3.5);
        p.setFont(self ? bold : font);
        p.setPen(self ? ui.Accent : dim ? ui.TextFaint : ui.Text);
        const QString name = fm.elidedText(partLabel(i), Qt::ElideRight, width() - m_rightAnchor - Margin - 12);
        drawLabel(p, QRectF(m_rightAnchor + 12, y - 11, width() - m_rightAnchor - Margin - 12, 22), Qt::AlignVCenter | Qt::AlignLeft, name);
    }
    p.resetTransform();
}

// A plate, faded in with the overview: a thin sheet, a little see-through so the links show through it, its title
// at its top.
void Canvas::drawPlate(QPainter &p, int plate)
{
    if (plate >= m_plateOutline.size()) return;
    const Palette &ui = Theme::current();
    p.resetTransform();
    QColor fill = ui.Panel;
    fill.setAlphaF(0.82 * m_tilt);
    QColor edge = ui.PanelLine;
    edge.setAlphaF(m_tilt);
    QColor shadow = Qt::black;
    shadow.setAlphaF(0.10 * m_tilt);
    p.setPen(Qt::NoPen);
    p.setBrush(shadow);
    p.drawPolygon(m_plateOutline[plate].translated(6 * m_tilt, 8 * m_tilt));
    p.setBrush(fill);
    p.setPen(QPen(edge, 1));
    p.drawPolygon(m_plateOutline[plate]);
    p.setTransform(m_plateFrame[plate]);
    p.setFont(headFont());
    QColor ink = ui.TextSection;
    ink.setAlphaF(m_tilt);
    p.setPen(ink);
    drawLabel(p, QPointF(18, 24), m_plateTitle.value(plate).toUpper());
    p.resetTransform();
}

// The shafts between the floors: those at rest go under the disc (only their way down shows), those followed over it.
void Canvas::drawShafts(QPainter &p, const Lit &lit, bool on)
{
    const Palette &ui = Theme::current();
    p.setBrush(Qt::NoBrush);
    QColor c = on ? ui.Accent : ui.TextDim;
    c.setAlphaF(on ? 0.85 : lit.active ? 0.08 : 0.26);
    QPen pen(c, on ? 1.4 : 1);
    pen.setCapStyle(Qt::RoundCap);
    if (m_fast) pen.setCosmetic(true);
    p.setPen(pen);
    for (int i = 0; i < m_shafts.size(); ++i)
        if (lit.shafts.contains(i) == on) p.drawPath(m_shafts[i].path);
}

void Canvas::drawUnderground(QPainter &p, const Lit &lit)
{
    const double opacity = underOpacity();
    if (opacity <= 0) return;
    const Palette &ui = Theme::current();
    const Item &s = lit.shown;
    p.save();
    p.resetTransform();
    p.setOpacity(opacity);
    p.translate(underShift());
    // The floor, a little thicker than the disc above: it holds the building up.
    const QRectF floor(m_underCentre - QPointF(2 * m_underB, m_underB), QSizeF(4 * m_underB, 2 * m_underB));
    QColor shadow = Qt::black;
    shadow.setAlphaF(0.12);
    p.setPen(Qt::NoPen);
    p.setBrush(shadow);
    p.drawEllipse(floor.translated(0, 16));
    p.setBrush(ui.PanelLine);
    p.drawEllipse(floor.translated(0, 6));
    p.setBrush(ui.Panel);
    p.setPen(Qt::NoPen);
    p.drawEllipse(floor);
    p.setBrush(Qt::NoBrush);
    drawRim(p, [&] { p.drawEllipse(floor); }, m_focus, ui.Up);

    const double ink = floorInk(Underground, lit);
    // The shafts at rest under the rows.
    p.save();
    p.translate(-underShift());
    p.setOpacity(opacity * ink);
    drawShafts(p, lit, false);
    p.restore();
    p.setOpacity(opacity * ink);
    p.setFont(headFont());
    p.setPen(ui.TextSection);
    drawLabel(p, QRectF(floor.left(), floor.bottom() - 48, floor.width(), 18), Qt::AlignHCenter | Qt::AlignVCenter,
              QCoreApplication::translate("SourceFlowView", "Underground: the AIs and what they reach").toUpper());

    // The links.
    p.setBrush(Qt::NoBrush);
    for (int pass = 0; pass < 2; ++pass)
        for (int i = 0; i < m_underLinks.size(); ++i) {
            const bool on = lit.aiLinks.contains(i);
            if (on != (pass == 1)) continue;
            QColor c = on ? ui.Accent : ui.TextDim;
            c.setAlphaF(on ? 0.9 : lit.active ? 0.08 : 0.38);
            QPen pen(c, on ? 1.8 : 1.2);
            pen.setCapStyle(Qt::RoundCap);
            if (m_fast) {
                pen.setCosmetic(true);
                pen.setWidthF(1);
            }
            if (!graph.aiLinks[i].on) pen.setDashPattern({3, 3});
            p.setPen(pen);
            p.drawPath(m_underLinks[i]);
        }

    const QFont font = underFont();
    QFont bold = font;
    bold.setWeight(QFont::DemiBold);
    QFont small = font;
    small.setPixelSize(11);
    const QFontMetricsF fm(font);
    const bool details = m_underRow >= 30;   // room for a second line
    // What can be reached on the left, the AIs on the right, names running outward from their dots, what's to know
    // under them.
    auto hilite = [&](const QRectF &box) { p.fillRect(box, ui.Hover); };
    for (int i = 0; i < m_accessAt.size(); ++i) {
        const Graph::Access &a = graph.accesses[i];
        const QPointF at = m_accessAt[i];
        const bool self = s.kind == Access && s.index == i;
        const bool dim = lit.active && !lit.accesses.contains(i);
        const QRectF box(at.x() - 10 - m_underLabel, at.y() - 11, m_underLabel, 22);
        if (self || (pinned.kind == Access && pinned.index == i)) {
            const double w = std::min(fm.horizontalAdvance(a.name), m_underLabel) + 18;
            hilite(QRectF(at.x() - w, at.y() - 11, w + 8, m_underRow));
        }
        p.setPen(Qt::NoPen);
        QColor dot = stateColor(a.state);
        if (dim) dot.setAlphaF(0.4);
        p.setBrush(lit.accesses.contains(i) ? ui.Accent : dot);
        p.drawEllipse(at, 3.6, 3.6);
        p.setFont(self ? bold : font);
        p.setPen(self ? ui.Accent : dim ? ui.TextFaint : ui.Text);
        drawLabel(p, box, Qt::AlignVCenter | Qt::AlignRight, fm.elidedText(a.name, Qt::ElideRight, m_underLabel));
        if (details) {
            p.setFont(small);
            p.setPen(dim ? ui.TextFaint : ui.TextDim);
            drawLabel(p, box.translated(0, 15), Qt::AlignVCenter | Qt::AlignRight, accessDetail(i));
        }
    }
    for (int i = 0; i < m_aiAt.size(); ++i) {
        const Graph::Ai &ai = graph.ais[i];
        const QPointF at = m_aiAt[i];
        const bool self = s.kind == Ai && s.index == i;
        const bool dim = lit.active && !lit.ais.contains(i);
        const QRectF box(at.x() + 10, at.y() - 11, m_underLabel, 22);
        if (self || (pinned.kind == Ai && pinned.index == i)) {
            const double w = std::min(fm.horizontalAdvance(ai.name), m_underLabel) + 18;
            hilite(QRectF(at.x() - 8, at.y() - 11, w + 8, m_underRow));
        }
        // Full when it reaches data and that's in force, hollow when it reaches nothing or isn't.
        const bool live = ai.on && !ai.accesses.isEmpty();
        QColor dot = lit.ais.contains(i) ? ui.Accent : live ? stateColor(Row::Good) : ui.TextFaint;
        if (dim) dot.setAlphaF(0.4);
        p.setPen(live ? Qt::NoPen : QPen(dot, 1.2));
        p.setBrush(live ? QBrush(dot) : Qt::NoBrush);
        p.drawEllipse(at, 3.6, 3.6);
        p.setFont(self ? bold : font);
        p.setPen(self ? ui.Accent : dim || ai.accesses.isEmpty() ? ui.TextFaint : ui.Text);
        drawLabel(p, box, Qt::AlignVCenter | Qt::AlignLeft, fm.elidedText(ai.name, Qt::ElideRight, m_underLabel));
        if (details) {
            p.setFont(small);
            p.setPen(dim ? ui.TextFaint : ui.TextDim);
            drawLabel(p, box.translated(0, 15), Qt::AlignVCenter | Qt::AlignLeft, aiDetail(i));
        }
    }
    p.restore();
}

// A row's second line on the floor below.
QString Canvas::accessDetail(int access) const
{
    const Graph::Access &a = graph.accesses[access];
    auto text = [](const char *source, int n = -1) { return QCoreApplication::translate("SourceFlowView", source, nullptr, n); };
    if (a.stage) return text("%n golden record(s)", graph.stage.records);
    if (!a.parts.isEmpty()) return graph.parts[a.parts.first()].tab;
    if (!a.sources.isEmpty()) return text("%n source(s)", int(a.sources.size()));
    return a.skill.isEmpty() ? text("No skill needed") : a.skill;
}

QString Canvas::aiDetail(int ai) const
{
    const Graph::Ai &a = graph.ais[ai];
    auto text = [](const char *source, int n = -1) { return QCoreApplication::translate("SourceFlowView", source, nullptr, n); };
    const char *kind = a.kind == Graph::Ai::Local      ? "Local model"
                       : a.kind == Graph::Ai::Agent    ? "Agent outside the app"
                       : a.kind == Graph::Ai::Cloud    ? "Remote model"
                                                       : "Models trained on this computer";
    return text(kind) + QString::fromUtf8(" · ") + text("%n access(es)", int(a.accesses.size()));
}

void Canvas::drawRim(QPainter &p, const std::function<void()> &shape, double focus, const QColor &colour)
{
    const Palette &ui = Theme::current();
    if (focus > 0) {
        QColor glow = colour;
        glow.setAlphaF(0.22 * focus);
        p.setPen(QPen(glow, 5));
        shape();
    }
    const QColor line = ui.PanelLine;
    p.setPen(QPen(QColor::fromRgbF(line.redF() + (colour.redF() - line.redF()) * focus, line.greenF() + (colour.greenF() - line.greenF()) * focus,
                                   line.blueF() + (colour.blueF() - line.blueF()) * focus),
                  1 + 0.6 * focus));
    shape();
}

// The floor in view, in a small rectangle of its colour at the bottom of the view; the arrow that leaves it beside it.
void Canvas::drawFloorChip(QPainter &p)
{
    const double opacity = underOpacity();
    if (opacity <= 0) return;
    const Palette &ui = Theme::current();
    const QSizeF view = m_viewport.isValid() ? QSizeF(m_viewport) : QSizeF(size());
    p.save();
    p.resetTransform();
    p.setOpacity(opacity);
    QFont f = font();
    f.setPixelSize(11);
    f.setBold(true);
    p.setFont(f);
    const QString text = m_floor == Underground ? QCoreApplication::translate("SourceFlowView", "Underground · AIs")
                                                : QCoreApplication::translate("SourceFlowView", "Ground floor · Data");
    const QFontMetricsF fm(f);
    const QRectF chip(4, view.height() - 30, fm.horizontalAdvance(text) + 20, 22);
    p.setPen(Qt::NoPen);
    p.setBrush(floorColor(m_focus));
    p.drawRoundedRect(chip, 4, 4);
    p.setPen(ui.Background);
    drawLabel(p, chip, Qt::AlignCenter, text);
    p.setPen(ui.TextDim);
    drawLabel(p, QRectF(chip.right() + 8, chip.top(), 30, chip.height()), Qt::AlignVCenter | Qt::AlignLeft,
              m_floor == Underground ? QString::fromUtf8("↑") : QString::fromUtf8("↓"));
    p.restore();
}

void Canvas::paintContent(QPainter &p)
{
    const Palette &ui = Theme::current();
    p.setRenderHint(QPainter::Antialiasing);
    const Lit lit = lighting();
    if (platesShown()) {
        // Back to front: the parts' plate, the links' far halves, the fusion's plate and block, the near halves, the
        // sources' plate. The plates are a little see-through: the links show going through them.
        const int back = int(m_plateOutline.size()) - 1;
        drawPlate(p, back);
        drawLinks(p, lit, m_backPaths);
        drawRightRows(p, lit);
        if (back == 2) drawPlate(p, 1);
        drawStage(p, lit);
        drawLinks(p, lit, m_frontPaths);
        drawPlate(p, 0);
        drawLeftRows(p, lit);
        return;
    }
    if (m_overview) {
        drawUnderground(p, lit);
        // A flat disc, every element at the same level on it; the columns' rectangle rounds into it on the way.
        const QTransform m = projection(m_tilt);
        const QRectF board = QRectF(0, 0, width(), m_naturalHeight).adjusted(-12, -12, 12, 12);
        const QRectF disc(m_discCentre - QPointF(m_plateRadius, m_plateRadius), QSizeF(2 * m_plateRadius, 2 * m_plateRadius));
        auto mix = [&](double a, double b) { return a + (b - a) * m_tilt; };
        const QRectF shape(QPointF(mix(board.left(), disc.left()), mix(board.top(), disc.top())),
                           QPointF(mix(board.right(), disc.right()), mix(board.bottom(), disc.bottom())));
        const double round = m_tilt * std::min(shape.width(), shape.height()) / 2;
        QPainterPath outline;
        outline.addRoundedRect(shape, round, round);
        const QPainterPath plane = m.map(outline);
        QColor shadow = Qt::black;
        shadow.setAlphaF(0.12 * m_tilt);
        p.setPen(Qt::NoPen);
        p.setBrush(shadow);
        p.drawPath(plane.translated(0, 10 * m_tilt));
        p.setBrush(ui.Background);
        p.setPen(Qt::NoPen);
        p.drawPath(plane);
        p.setBrush(Qt::NoBrush);
        drawRim(p, [&] { p.drawPath(plane); }, (1 - m_focus) * underOpacity(), ui.Accent);
        p.setOpacity(floorInk(Ground, lit));
    }
    p.setTransform(projection(m_tilt));
    drawLinks(p, lit, m_paths);
    drawStage(p, lit);
    drawLeftRows(p, lit);
    drawRightRows(p, lit);
    p.setOpacity(1);
    if (m_overview) {
        if (!lit.shafts.isEmpty()) {
            p.resetTransform();
            drawShafts(p, lit, true);
        }
        if (!m_paintingDisc) drawFloorChip(p);
    }
}

QVector<Canvas::Item> Canvas::inspectOrder(Item from) const
{
    QVector<Item> out;
    if (from.kind == Ai || from.kind == Access) {
        for (int i = 0; i < graph.accesses.size(); ++i) out << Item {Access, i};
        for (int i = 0; i < graph.ais.size(); ++i) out << Item {Ai, i};
        return out;
    }
    // In the order they are drawn around the disc.
    for (int s : m_sourceOrder) out << Item {Source, s};
    for (int l = 0; l < graph.listed.size(); ++l) out << Item {Listed, l};
    if (!m_stageRect.isNull()) out << Item {Stage, 0};
    for (int p : m_partOrder) out << Item {Part, p};
    return out;
}

QPointF Canvas::inspectAnchor(const Item &item) const
{
    switch (item.kind) {
    case Source: return rowMatrix(SourceRowKind, item.index).map(QPointF(m_leftAnchor + 4, m_sourceY[item.index]));
    case Listed: return rowMatrix(ListedRowKind, item.index).map(QPointF(m_leftAnchor + 4, m_listedY[item.index]));
    case Part: return rowMatrix(PartRowKind, item.index).map(QPointF(m_rightAnchor - 4, m_partY[item.index]));
    case Stage: return stageMatrix().map(m_stageRect.center());
    case Access: return m_accessAt.value(item.index) + underShift();
    case Ai: return m_aiAt.value(item.index) + underShift();
    default: return {};
    }
}

void Canvas::describe(const Item &item, QString *name, QString *detail, QColor *dot) const
{
    const Palette &ui = Theme::current();
    auto text = [](const char *source, int n = -1) { return QCoreApplication::translate("SourceFlowView", source, nullptr, n); };
    *dot = ui.Accent;
    detail->clear();
    switch (item.kind) {
    case Source: {
        const Graph::Source &s = graph.sources[item.index];
        *name = s.name;
        *detail = s.group + QString::fromUtf8(" · ") + text("%n data set(s)", int(s.rows.size()));
        *dot = stateColor(s.state);
        break;
    }
    case Listed: {
        const Graph::Listed &l = graph.listed[item.index];
        *name = l.name;
        *detail = QCoreApplication::translate("SourceFlowView", "%1 listed").arg(QLocale().toString(int(l.sources.size())));
        *dot = ui.TextFaint;
        break;
    }
    case Part: {
        const Graph::Part &p = graph.parts[item.index];
        *name = partLabel(item.index);
        *detail = (p.name.isEmpty() ? QString() : p.tab + QString::fromUtf8(" · ")) + text("%n data set(s)", int(p.rows.size()));
        break;
    }
    case Stage:
        *name = text("Data fusion");
        *detail = text("%n golden record(s)", graph.stage.records);
        break;
    case Access:
        *name = graph.accesses[item.index].name;
        *detail = accessDetail(item.index);
        *dot = stateColor(graph.accesses[item.index].state);
        break;
    case Ai:
        *name = graph.ais[item.index].name;
        *detail = aiDetail(item.index);
        *dot = graph.ais[item.index].on && !graph.ais[item.index].accesses.isEmpty() ? stateColor(Row::Good) : ui.TextFaint;
        break;
    default:
        break;
    }
}

// What the item comes from and goes to, laid out on each side of its card.
void Canvas::buildInspect()
{
    const Item it = m_inspect;
    describe(it, &m_inspectName, &m_inspectDetail, &m_inspectDot);
    m_inspectRows.clear();
    m_inspectHover = -1;
    m_inspectNote.clear();
    auto text = [](const char *source, int n = -1) { return QCoreApplication::translate("SourceFlowView", source, nullptr, n); };
    const QString comesFrom = text("Where it comes from"), feeds = QCoreApplication::translate("SourceFlowView", "What it feeds"), reached = text("Reached by the AIs");
    QString leftTitle, rightTitle;
    auto add = [this](const Item &item, int side, const QString &extra = {}) {
        InspectRow r;
        r.item = item;
        r.side = side;
        describe(item, &r.name, &r.detail, &r.dot);
        if (!extra.isEmpty()) r.detail = extra;
        for (const InspectRow &o : std::as_const(m_inspectRows))
            if (o.item == item && o.side == side) return;
        m_inspectRows << r;
    };
    auto dataSets = [&](int n) { return text("%n data set(s)", n); };
    switch (it.kind) {
    case Source:
        leftTitle = QCoreApplication::translate("SourceFlowView", "Its data sets");
        for (int r : graph.sources[it.index].rows) {
            InspectRow row;
            row.name = graph.rows[r].name;
            row.dot = stateColor(graph.rows[r].state);
            row.side = -1;
            m_inspectRows << row;
        }
        rightTitle = feeds;
        for (const Graph::Link &l : std::as_const(graph.links))
            if (l.source == it.index) {
                if (l.staged) add({Stage, 0}, 1);
                add({Part, l.part}, 1, dataSets(int(l.rows.size())));
            }
        for (int a = 0; a < graph.accesses.size(); ++a)
            if (graph.accesses[a].sources.contains(it.index)) add({Access, a}, 1);
        break;
    case Part:
        leftTitle = comesFrom;
        for (const Graph::Link &l : std::as_const(graph.links))
            if (l.part == it.index) {
                if (l.staged) add({Stage, 0}, -1);
                add({Source, l.source}, -1, dataSets(int(l.rows.size())));
            }
        rightTitle = reached;
        for (int a = 0; a < graph.accesses.size(); ++a)
            if (graph.accesses[a].parts.contains(it.index)) add({Access, a}, 1);
        break;
    case Stage:
        leftTitle = comesFrom;
        for (int s : graph.stage.sources) add({Source, s}, -1);
        rightTitle = feeds;
        for (int p : graph.stage.parts) add({Part, p}, 1);
        for (int a = 0; a < graph.accesses.size(); ++a)
            if (graph.accesses[a].stage) add({Access, a}, 1);
        break;
    case Access: {
        const Graph::Access &x = graph.accesses[it.index];
        leftTitle = comesFrom;
        if (x.stage) add({Stage, 0}, -1);
        for (int s : x.sources) add({Source, s}, -1);
        for (int p : x.parts) add({Part, p}, -1);
        rightTitle = reached;
        for (const Graph::AiLink &l : std::as_const(graph.aiLinks))
            if (l.access == it.index) add({Ai, l.ai}, 1);
        break;
    }
    case Ai:
        leftTitle = QCoreApplication::translate("SourceFlowView", "What it reaches");
        for (int a : graph.ais[it.index].accesses) add({Access, a}, -1);
        break;
    case Listed:
        m_inspectNote = QCoreApplication::translate("SourceFlowView", "Listed in the catalogue, not read by the app: nothing is linked to it.");
        break;
    default:
        break;
    }
    if (m_inspectRows.isEmpty() && m_inspectNote.isEmpty()) m_inspectNote = QCoreApplication::translate("SourceFlowView", "Nothing is linked to it.");
    m_inspectLeftTitle = leftTitle;
    m_inspectRightTitle = rightTitle;

    // On each side, as many rows as fit, the rest told by a last one.
    const QSizeF view = m_viewport.isValid() ? QSizeF(m_viewport) : QSizeF(size());
    const double rowW = std::min(250.0, view.width() * 0.27), rowH = 38, top = 44, bottom = view.height() - 52;
    const int fit = std::max(1, int((bottom - top) / rowH));
    QVector<InspectRow> kept;
    for (int side : {-1, 1}) {
        QVector<InspectRow> rows;
        for (const InspectRow &r : std::as_const(m_inspectRows))
            if (r.side == side) rows << r;
        if (rows.size() > fit) {
            const int more = int(rows.size()) - (fit - 1);
            rows.resize(fit - 1);
            InspectRow r;
            r.side = side;
            r.name = QCoreApplication::translate("SourceFlowView", "and %1 more").arg(more);
            r.dot = Qt::transparent;
            rows << r;
        }
        const double height = rows.size() * rowH;
        const double y0 = std::max(top, view.height() * 0.5 - height / 2);
        for (int i = 0; i < rows.size(); ++i) {
            rows[i].box = QRectF(side < 0 ? 18 : view.width() - rowW - 18, y0 + i * rowH, rowW, rowH - 6);
            kept << rows[i];
        }
    }
    m_inspectRows = kept;
}

void Canvas::inspect(const Item &item)
{
    const bool opening = !inspecting() || m_inspectClosing;
    // Across the floors: the camera is on the item's own at once, the fade hides the jump.
    const Floor floor = item.kind == Ai || item.kind == Access ? Underground : Ground;
    if (floor != m_floor) {
        m_floor = floor;
        m_focusAnim->stop();
        m_focus = floor == Underground ? 1 : 0;
        placeShafts();
    }
    const double t = m_inspectT;
    const auto mix = [t](double a, double b) { return a + (b - a) * t; };
    const QPointF at = inspectAnchor(item);
    if (opening && !m_inspectClosing) {
        // From the view as it is: the camera at rest looks at the item where it is.
        m_camFromAt = at;
        m_camFromZoom = 1;
        m_dimFrom = 0;
        m_liftFrom = 0;
    } else {
        m_camFromAt = m_camFromAt + (m_camToAt - m_camFromAt) * t;
        m_camFromZoom = mix(m_camFromZoom, m_camToZoom);
        m_dimFrom = mix(m_dimFrom, m_dimTo);
        m_liftFrom = 0;
    }
    m_camToAt = at;
    m_camToZoom = 2.6;
    m_dimTo = 1;
    m_inspectClosing = false;
    m_inspect = item;
    hover = {};
    pinned = item;
    m_discImage = QImage();
    setCursor(Qt::ArrowCursor);
    buildInspect();
    m_inspectAnim->stop();
    m_inspectT = 0;
    m_inspectAnim->start();
    setFocus();
    update();
    if (changed) changed();
}

void Canvas::closeInspect()
{
    if (!inspecting() || m_inspectClosing) return;
    const double t = m_inspectT;
    const auto mix = [t](double a, double b) { return a + (b - a) * t; };
    m_camFromAt = m_camFromAt + (m_camToAt - m_camFromAt) * t;
    m_camFromZoom = mix(m_camFromZoom, m_camToZoom);
    m_dimFrom = mix(m_dimFrom, m_dimTo);
    m_liftFrom = 1;
    m_camToZoom = 1;
    m_dimTo = 0;
    m_inspectClosing = true;
    m_inspectAnim->stop();
    m_inspectT = 0;
    m_inspectAnim->start();
}

void Canvas::closeInspectNow()
{
    if (!inspecting()) return;
    m_inspectAnim->stop();
    m_inspect = {};
    m_inspectClosing = false;
    m_inspectRows.clear();
    m_discImage = QImage();
    pinned = {};
    setCursor(Qt::ArrowCursor);
    update();
    if (changed) changed();
}

void Canvas::inspectStep(int step)
{
    const QVector<Item> order = inspectOrder(m_inspect);
    if (order.isEmpty()) return;
    const int i = int(order.indexOf(m_inspect));
    inspect(order[(std::max(i, 0) + step + order.size()) % order.size()]);
}

QTransform Canvas::inspectCamera() const
{
    // The item's place comes to the middle of the view, a little under the card that rises from it.
    const QSizeF view = m_viewport.isValid() ? QSizeF(m_viewport) : QSizeF(size());
    const double t = m_inspectT;
    const double zoom = m_camFromZoom + (m_camToZoom - m_camFromZoom) * t;
    const QPointF at = m_camFromAt + (m_camToAt - m_camFromAt) * t;
    // At rest the item stays where it is, at full zoom it is at the target: in between, by how far the zoom has gone.
    const double way = std::clamp((zoom - 1) / 1.6, 0.0, 1.0);
    const QPointF target(view.width() / 2, view.height() * 0.62);
    const QPointF on = at + (target - at) * way;
    return QTransform::fromTranslate(-at.x(), -at.y()) * QTransform::fromScale(zoom, zoom) * QTransform::fromTranslate(on.x(), on.y());
}

void Canvas::paintInspect(QPainter &p)
{
    const Palette &ui = Theme::current();
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    const qreal ratio = devicePixelRatioF();
    if (m_discImage.isNull()) {
        // The disc with the item followed, at full sharpness, once.
        m_discImage = QImage(size() * ratio, QImage::Format_ARGB32_Premultiplied);
        m_discImage.setDevicePixelRatio(ratio);
        m_discImage.fill(Qt::transparent);
        QPainter dp(&m_discImage);
        m_paintingDisc = true;
        paintContent(dp);
        m_paintingDisc = false;
    }
    const QSizeF view = m_viewport.isValid() ? QSizeF(m_viewport) : QSizeF(size());
    const double t = m_inspectT;
    const double dim = m_dimFrom + (m_dimTo - m_dimFrom) * t;
    // The rows come in with the camera, and go first on the way back.
    const double ink = m_inspectClosing ? std::max(0.0, 1 - t * 1.6) : t;
    const double lift = m_inspectClosing ? 1 - t : (m_liftFrom + (1 - m_liftFrom) * t);

    p.save();
    p.setTransform(inspectCamera());
    p.drawImage(QRectF(QPointF(0, 0), QSizeF(size())), m_discImage);
    p.restore();
    QColor veil = ui.Background;
    veil.setAlphaF(0.72 * dim);
    p.fillRect(rect(), veil);

    // The card, risen from its place.
    const double zoom = m_camFromZoom + (m_camToZoom - m_camFromZoom) * t;
    const QPointF place = inspectCamera().map(m_camFromAt + (m_camToAt - m_camFromAt) * t);
    Q_UNUSED(zoom);
    QFont nameFont = font();
    nameFont.setPixelSize(16);
    nameFont.setBold(true);
    QFont detailFont = font();
    detailFont.setPixelSize(12);
    const QFontMetricsF nameMetrics(nameFont), detailMetrics(detailFont);
    const double maxCard = std::max(180.0, view.width() - 2 * (std::min(250.0, view.width() * 0.27) + 18) - 110);
    const double cardW = std::clamp(std::max(nameMetrics.horizontalAdvance(m_inspectName), detailMetrics.horizontalAdvance(m_inspectDetail)) + 48, 170.0, maxCard);
    const double cardH = 66;
    const double rise = 96 * lift;
    const QPointF centre = place - QPointF(0, rise);
    const QRectF card(centre.x() - cardW / 2, centre.y() - cardH / 2, cardW, cardH);
    m_inspectCard = card;

    // Its shadow on the disc and the stem it rose on.
    if (lift > 0) {
        QColor shadow = Qt::black;
        shadow.setAlphaF(0.32 * lift);
        p.setPen(Qt::NoPen);
        p.setBrush(shadow);
        p.drawEllipse(place, cardW * 0.42 * (0.6 + 0.4 * lift), 11 * lift + 2);
        QColor stem = m_inspectDot;
        stem.setAlphaF(0.6 * lift);
        p.setPen(QPen(stem, 1.2, Qt::DashLine));
        p.drawLine(place, QPointF(centre.x(), card.bottom()));
        p.setPen(Qt::NoPen);
        p.setBrush(m_inspectDot);
        p.drawEllipse(place, 3.5, 3.5);
    }

    // The rows of each side and their links to the card.
    QFont rowFont = font();
    rowFont.setPixelSize(13);
    QFont smallFont = font();
    smallFont.setPixelSize(11);
    QFont titleFont = font();
    titleFont.setPixelSize(10);
    titleFont.setBold(true);
    titleFont.setLetterSpacing(QFont::AbsoluteSpacing, 1);
    p.save();
    p.setOpacity(std::clamp(ink, 0.0, 1.0));
    const QFontMetricsF rowMetrics(rowFont), smallMetrics(smallFont);
    double leftTop = view.height(), rightTop = view.height();
    for (const InspectRow &r : std::as_const(m_inspectRows)) (r.side < 0 ? leftTop : rightTop) = std::min(r.side < 0 ? leftTop : rightTop, r.box.top());
    for (int i = 0; i < m_inspectRows.size(); ++i) {
        const InspectRow &r = m_inspectRows[i];
        const bool lit = i == m_inspectHover;
        const QPointF from = r.side < 0 ? QPointF(r.box.right(), r.box.center().y()) : QPointF(r.box.left(), r.box.center().y());
        const QPointF to = r.side < 0 ? QPointF(card.left(), centre.y()) : QPointF(card.right(), centre.y());
        QPainterPath link(from);
        const double bend = (to.x() - from.x()) * 0.5;
        link.cubicTo(from + QPointF(bend, 0), to - QPointF(bend, 0), to);
        QColor line = r.dot.alpha() ? r.dot : ui.TextFaint;
        line.setAlphaF(lit ? 0.95 : 0.5);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(line, lit ? 2 : 1.2));
        p.drawPath(link);
    }
    for (int i = 0; i < m_inspectRows.size(); ++i) {
        const InspectRow &r = m_inspectRows[i];
        const bool lit = i == m_inspectHover;
        p.setPen(QPen(lit ? ui.Accent : ui.PanelLine, 1));
        p.setBrush(lit ? ui.Hover : ui.Panel);
        p.drawRoundedRect(r.box, 6, 6);
        const bool twoLines = !r.detail.isEmpty();
        const double textLeft = r.box.left() + 22, textWidth = r.box.width() - 30;
        if (r.dot.alpha()) {
            p.setPen(Qt::NoPen);
            p.setBrush(r.dot);
            p.drawEllipse(QPointF(r.box.left() + 11, r.box.center().y()), 3.5, 3.5);
        }
        p.setFont(rowFont);
        p.setPen(r.item.kind == None && !r.dot.alpha() ? ui.TextDim : ui.Text);
        p.drawText(QRectF(textLeft, twoLines ? r.box.top() + 2 : r.box.top(), textWidth, twoLines ? 17 : r.box.height()), Qt::AlignVCenter | Qt::AlignLeft,
                   rowMetrics.elidedText(r.name, Qt::ElideRight, textWidth));
        if (twoLines) {
            p.setFont(smallFont);
            p.setPen(ui.TextDim);
            p.drawText(QRectF(textLeft, r.box.top() + 17, textWidth, 13), Qt::AlignVCenter | Qt::AlignLeft,
                       smallMetrics.elidedText(r.detail, Qt::ElideRight, textWidth));
        }
    }
    p.setFont(titleFont);
    p.setPen(ui.TextDim);
    const double rowW = std::min(250.0, view.width() * 0.27);
    if (leftTop < view.height())
        p.drawText(QRectF(18, leftTop - 22, rowW, 18), Qt::AlignVCenter | Qt::AlignLeft, m_inspectLeftTitle.toUpper());
    if (rightTop < view.height())
        p.drawText(QRectF(view.width() - rowW - 18, rightTop - 22, rowW, 18), Qt::AlignVCenter | Qt::AlignLeft, m_inspectRightTitle.toUpper());
    p.restore();

    // The card over the links.
    QColor cardShadow = Qt::black;
    cardShadow.setAlphaF(0.28);
    p.setPen(Qt::NoPen);
    p.setBrush(cardShadow);
    p.drawRoundedRect(card.translated(0, 6), 10, 10);
    p.setBrush(ui.Panel);
    p.setPen(QPen(m_inspectDot, 1.6));
    p.drawRoundedRect(card, 10, 10);
    p.setPen(Qt::NoPen);
    p.setBrush(m_inspectDot);
    p.drawEllipse(QPointF(card.left() + 15, card.top() + 23), 4.5, 4.5);
    p.setFont(nameFont);
    p.setPen(ui.Text);
    p.drawText(QRectF(card.left() + 27, card.top() + 11, card.width() - 37, 24), Qt::AlignVCenter | Qt::AlignLeft,
               nameMetrics.elidedText(m_inspectName, Qt::ElideRight, card.width() - 37));
    p.setFont(detailFont);
    p.setPen(ui.TextDim);
    p.drawText(QRectF(card.left() + 27, card.top() + 36, card.width() - 37, 18), Qt::AlignVCenter | Qt::AlignLeft,
               detailMetrics.elidedText(m_inspectDetail, Qt::ElideRight, card.width() - 37));
    if (!m_inspectNote.isEmpty() && ink > 0) {
        p.setOpacity(std::clamp(ink, 0.0, 1.0));
        p.setPen(ui.TextDim);
        p.drawText(QRectF(card.left() - 60, card.top() - 52, card.width() + 120, 40), Qt::AlignHCenter | Qt::AlignBottom | Qt::TextWordWrap, m_inspectNote);
        p.setOpacity(1);
    }

    // How to leave and go on.
    p.setOpacity(std::clamp(ink, 0.0, 1.0));
    p.setFont(smallFont);
    p.setPen(ui.TextDim);
    p.drawText(QRectF(18, view.height() - 30, view.width() - 36, 22), Qt::AlignVCenter | Qt::AlignLeft,
               QCoreApplication::translate("SourceFlowView", "← → previous or next · a row goes to it · Esc or a click beside: back to the disc"));
    p.setOpacity(1);
}

} // namespace sourceflow

// ---------------------------------------------------------------- SourceFlowView

using namespace sourceflow;

SourceFlowView::SourceFlowView(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("panelContent");
    setAttribute(Qt::WA_StyledBackground);
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(32, 26, 32, 20);
    v->setSpacing(6);

    auto *title = new QLabel(tr("Data"));
    title->setObjectName("title");
    v->addWidget(title);
    auto *hint = new QLabel(tr("Where Deepwater's data comes from and which parts of the app it feeds: a link for each "
                               "data set, as thick as the data sets it carries, coloured by their state in the list. "
                               "Hover a source, a part of the app or a link to follow its data; click to keep it."));
    hint->setObjectName("dim");
    hint->setWordWrap(true);
    v->addWidget(hint);
    v->addSpacing(6);
    m_summary = new QLabel;
    m_summary->setTextFormat(Qt::RichText);
    v->addWidget(m_summary);
    v->addSpacing(6);
    // Under the sources' states: the overview.
    auto *zoomRow = new QHBoxLayout;
    zoomRow->setSpacing(6);
    m_zoom = new QPushButton(tr("Zoom out: disc"));
    m_zoom->setObjectName("segButton");
    m_zoom->setCursor(Qt::PointingHandCursor);
    m_zoom->setCheckable(true);
    m_zoom->setToolTip(tr("The whole diagram at once, tilted on a plane; under it, the underground floor: the AIs and what they "
                          "can reach of the data. The up and down arrows change floor; a click on an item looks at it closely."));
    zoomRow->addWidget(m_zoom);
    m_layers = new QPushButton(tr("Zoom out: plates"));
    m_layers->setObjectName("segButton");
    m_layers->setCursor(Qt::PointingHandCursor);
    m_layers->setCheckable(true);
    m_layers->setToolTip(tr("The whole diagram at once on plates standing one behind the other: the sources, the fusion, "
                            "the parts of the app, the links going through them"));
    zoomRow->addWidget(m_layers);
    zoomRow->addStretch(1);
    v->addLayout(zoomRow);
    v->addSpacing(8);

    auto *h = new QHBoxLayout;
    h->setSpacing(16);
    m_scroll = new QScrollArea;
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_canvas = new Canvas(m_scroll);
    m_canvas->setObjectName("panelContent");
    m_canvas->setAttribute(Qt::WA_StyledBackground);
    m_canvas->setMinimumWidth(640);
    m_scroll->setWidget(m_canvas);
    m_scroll->setFocusProxy(m_canvas);   // the arrows keep changing floor after the wheel or a click beside
    h->addWidget(m_scroll, 1);

    auto *card = new QFrame;
    card->setObjectName("sourceCard");
    card->setFixedWidth(320);
    auto *cv = new QVBoxLayout(card);
    cv->setContentsMargins(16, 14, 16, 14);
    cv->setSpacing(10);
    auto *detailScroll = new QScrollArea;
    detailScroll->setWidgetResizable(true);
    detailScroll->setFrameShape(QFrame::NoFrame);
    detailScroll->setStyleSheet("QScrollArea, QScrollArea > QWidget > QWidget { background: transparent; }");
    m_detail = new QLabel;
    m_detail->setTextFormat(Qt::RichText);
    m_detail->setWordWrap(true);
    m_detail->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    detailScroll->setWidget(m_detail);
    detailScroll->setFocusProxy(m_canvas);
    cv->addWidget(detailScroll, 1);
    m_show = new QPushButton(tr("Show in the list"));
    m_show->setObjectName("segButton");
    m_show->setCursor(Qt::PointingHandCursor);
    cv->addWidget(m_show, 0, Qt::AlignLeft);
    h->addWidget(card);
    v->addLayout(h, 1);

    m_canvas->changed = [this] { updateDetail(); };
    m_canvas->scrollTo = [this](int y) { m_scroll->verticalScrollBar()->setValue(y); };
    m_scroll->viewport()->installEventFilter(this);
    // The two overviews, one at a time: switching goes back to the columns and out again.
    auto overview = [this](QPushButton *button, QPushButton *other, Canvas::Mode mode) {
        connect(button, &QPushButton::toggled, this, [this, button, other, mode](bool on) {
            if (on && other->isChecked()) {
                const QSignalBlocker block(other);
                other->setChecked(false);
            }
            m_zoom->setText(m_zoom->isChecked() ? tr("Zoom in") : tr("Zoom out: disc"));
            m_layers->setText(m_layers->isChecked() ? tr("Zoom in") : tr("Zoom out: plates"));
            m_canvas->setOverview(on, mode, m_scroll->verticalScrollBar()->value(), m_scroll->viewport()->size());
        });
    };
    overview(m_zoom, m_layers, Canvas::Disc);
    overview(m_layers, m_zoom, Canvas::Plates);
    connect(m_show, &QPushButton::clicked, this, [this] {
        const Canvas::Item item = m_canvas->pinned;
        const Graph &g = m_canvas->graph;
        QVector<int> rows;
        QString title;
        if (item.kind == Canvas::Listed) {
            emit showCatalogue(g.listed[item.index].kind);
            return;
        }
        auto addAccess = [&](int a) {
            for (int r : g.accesses[a].rows)
                if (!rows.contains(r)) rows << r;
        };
        if (item.kind == Canvas::Ai) {
            for (int a : g.ais[item.index].accesses) addAccess(a);
            title = g.ais[item.index].name;
        } else if (item.kind == Canvas::Access) {
            addAccess(item.index);
            title = g.accesses[item.index].name;
        } else if (item.kind == Canvas::AiLink) {
            const Graph::AiLink &l = g.aiLinks[item.index];
            addAccess(l.access);
            title = g.ais[l.ai].name + QString::fromUtf8(" → ") + g.accesses[l.access].name;
        }
        if (item.kind == Canvas::Source) {
            rows = g.sources[item.index].rows;
            title = g.sources[item.index].name;
        } else if (item.kind == Canvas::Part) {
            rows = g.parts[item.index].rows;
            const Graph::Part &p = g.parts[item.index];
            title = p.name.isEmpty() ? p.tab : p.tab + " · " + p.name;
        } else if (item.kind == Canvas::Stage) {
            for (const Graph::Link &l : g.links)
                if (l.staged)
                    for (int r : l.rows)
                        if (!rows.contains(r)) rows << r;
            title = tr("Data fusion");
        } else if (item.kind == Canvas::Link) {
            const Graph::Link &l = g.links[item.index];
            rows = l.rows;
            const Graph::Part &p = g.parts[l.part];
            title = g.sources[l.source].name + QString::fromUtf8(" → ") + (p.name.isEmpty() ? p.tab : p.tab + " · " + p.name);
        }
        QStringList ids;
        for (int r : std::as_const(rows)) ids << g.rows[r].id;
        if (!ids.isEmpty()) emit showRows(title, ids);
    });
    updateDetail();
}

bool SourceFlowView::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_scroll->viewport() && event->type() == QEvent::Resize)
        m_canvas->setViewport(m_scroll->viewport()->size());
    return QWidget::eventFilter(watched, event);
}

void SourceFlowView::setRows(const QVector<Row> &rows)
{
    QStringList ids;
    for (const Row &r : rows) ids << r.id;
    const bool fusionChanged = reloadFusion();
    const QVector<Graph::Ai> ais = readAis();
    QStringList aiIds;
    for (const Graph::Ai &a : ais) aiIds << a.id + '|' + a.name + '|' + a.skills.join(',') + '|' + QString::number(a.on);
    const bool structure = ids != m_rowIds || fusionChanged || aiIds != m_aiIds;
    m_rowIds = ids;
    m_aiIds = aiIds;
    m_canvas->setGraph(build(rows, m_fusion, ais), structure);
    const Graph &g = m_canvas->graph;
    const Palette &ui = Theme::current();
    int counts[4] = {};
    for (const Row &r : rows) ++counts[r.state];
    auto chip = [](const QColor &c, const QString &text) {
        return QString("<span style='color:%1'>●</span>&nbsp;%2").arg(c.name(), text.toHtmlEscaped());
    };
    QStringList parts;
    parts << tr("%1 sources feed %2 parts of the app through %3 data sets")
                 .arg(g.sources.size()).arg(g.parts.size()).arg(rows.size()).toHtmlEscaped();
    parts << chip(ui.Up, tr("%1 working").arg(counts[Row::Good]));
    parts << chip(ui.Down, tr("%1 failing").arg(counts[Row::Bad]));
    parts << chip(ui.TextFaint, tr("%1 off").arg(counts[Row::Off]));
    parts << chip(ui.TextDim, tr("%1 not checked yet").arg(counts[Row::Neutral]));
    int listed = 0;
    for (const Graph::Listed &l : g.listed) listed += int(l.sources.size());
    if (listed > 0)
        parts << QString("<span style='color:%1'>○</span>&nbsp;%2")
                     .arg(ui.TextFaint.name(), tr("%1 more listed in the catalogue, not read by the app").arg(QLocale().toString(listed)).toHtmlEscaped());
    if (g.stage.present)
        parts << QString("<span style='color:%1'>▭</span>&nbsp;%2")
                     .arg(ui.Accent.name(), tr("%n golden record(s) from data fusion", nullptr, g.stage.records).toHtmlEscaped());
    const QString summary = parts.join("&nbsp;&nbsp;&nbsp;&nbsp;");
    if (m_summary->text() != summary) m_summary->setText(summary);
}

void SourceFlowView::showDisc(const QString &pin)
{
    QTimer::singleShot(400, this, [this, pin] {
        m_zoom->setChecked(true);
        const int index = pin.section(':', 1).toInt();
        const Graph &g = m_canvas->graph;
        if (pin.startsWith("floor:") || pin.startsWith("ai:") || pin.startsWith("access:"))
            m_canvas->setFloor(pin == "floor:0" ? Canvas::Ground : Canvas::Underground);
        if (pin.startsWith("ai:") && index < g.ais.size()) m_canvas->pinned = {Canvas::Ai, index};
        if (pin.startsWith("access:") && index < g.accesses.size()) m_canvas->pinned = {Canvas::Access, index};
        updateDetail();
        // "open:source:3": that item looked at closely, once the disc is there.
        if (pin.startsWith("open:")) {
            const QString what = pin.section(':', 1, 1);
            const int i = pin.section(':', 2).toInt();
            QTimer::singleShot(1100, this, [this, what, i] {
                const Canvas::Kind kind = what == "source" ? Canvas::Source : what == "part" ? Canvas::Part : what == "stage" ? Canvas::Stage
                                          : what == "listed" ? Canvas::Listed : what == "access" ? Canvas::Access : what == "ai" ? Canvas::Ai : Canvas::None;
                if (kind != Canvas::None) m_canvas->inspect({kind, i});
            });
        }
    });
}

bool SourceFlowView::reloadFusion()
{
    const QString dir = countries::fusionDir();
    const QDateTime stamp = QFileInfo(fusion::indexPath(dir)).lastModified();
    if (m_fusionRead && stamp == m_fusionStamp)
        return false;
    m_fusionRead = true;
    m_fusion = fusion::loadIndex(dir);   // writes the index from the records when it is missing
    m_fusionStamp = QFileInfo(fusion::indexPath(dir)).lastModified();
    return true;
}

void SourceFlowView::updateDetail()
{
    const Palette &ui = Theme::current();
    const Graph &g = m_canvas->graph;
    const Canvas::Item item = m_canvas->shown();
    // Nothing to show in the list for an AI that reaches nothing, or for the page in view.
    const Canvas::Item &pin = m_canvas->pinned;
    m_show->setVisible(pin.kind != Canvas::None && !(pin.kind == Canvas::Ai && g.ais[pin.index].accesses.isEmpty())
                       && !(pin.kind == Canvas::Access && g.accesses[pin.index].rows.isEmpty())
                       && !(pin.kind == Canvas::AiLink && g.accesses[g.aiLinks[pin.index].access].rows.isEmpty()));

    auto partName = [&](int p) {
        const Graph::Part &part = g.parts[p];
        return part.name.isEmpty() ? part.tab : part.tab + " · " + part.name;
    };
    auto heading = [&](const QString &title, const QString &sub) {
        return QString("<div style='font-size:15px; font-weight:600; color:%1'>%2</div>"
                       "<div style='color:%3; margin-top:3px'>%4</div>")
            .arg(ui.Text.name(), title.toHtmlEscaped(), ui.TextDim.name(), sub.toHtmlEscaped());
    };
    auto rowsHtml = [&](const QVector<int> &rows) {
        QStringList lines;
        for (int r : rows) {
            const Row &row = g.rows[r];
            QString state;
            if (row.state == Row::Bad) state = tr("failing");
            else if (row.state == Row::Off) state = tr("off");
            QString line = QString("<span style='color:%1'>●</span>&nbsp;%2").arg(stateColor(row.state).name(), row.name.toHtmlEscaped());
            if (!state.isEmpty()) line += QString(" <span style='color:%1'>· %2</span>").arg(ui.TextFaint.name(), state);
            lines << line;
        }
        return lines.join("<br>");
    };
    auto group = [&](const QString &title, const QVector<int> &rows) {
        return QString("<div style='margin-top:12px; color:%1; font-size:10px; font-weight:700; letter-spacing:1px'>%2</div>"
                       "<div style='margin-top:2px; line-height:140%'>%3</div>")
            .arg(ui.TextSection.name(), title.toUpper().toHtmlEscaped(), rowsHtml(rows));
    };

    auto textGroup = [&](const QString &title, const QStringList &lines) {
        return QString("<div style='margin-top:12px; color:%1; font-size:10px; font-weight:700; letter-spacing:1px'>%2</div>"
                       "<div style='margin-top:2px; line-height:140%'>%3</div>")
            .arg(ui.TextSection.name(), title.toUpper().toHtmlEscaped(), lines.join("<br>"));
    };
    auto paragraph = [&](const QString &text) {
        return QString("<p style='color:%1; line-height:140%'>%2</p>").arg(ui.TextDim.name(), text.toHtmlEscaped());
    };
    // The AIs that reach an access, as a list of names.
    auto reachedBy = [&](int access) {
        QStringList names;
        for (const Graph::AiLink &l : g.aiLinks)
            if (l.access == access) {
                QString name = g.ais[l.ai].name.toHtmlEscaped();
                if (!l.on) name += QString(" <span style='color:%1'>· %2</span>").arg(ui.TextFaint.name(), tr("off"));
                names << name;
            }
        return names;
    };
    // Under a source, a part or the fusion: which AIs reach its data, and through what.
    auto aiReach = [&](const std::function<bool(const Graph::Access &)> &reads) {
        QStringList lines;
        for (int a = 0; a < g.accesses.size(); ++a) {
            if (!reads(g.accesses[a])) continue;
            const QStringList names = reachedBy(a);
            if (!names.isEmpty())
                lines << QString("%1 <span style='color:%2'>← %3</span>")
                             .arg(g.accesses[a].name.toHtmlEscaped(), ui.TextDim.name(), names.join(", "));
        }
        return lines.isEmpty() ? QString() : textGroup(tr("Reached by the AIs"), lines);
    };
    auto accessCard = [&](int a) {
        const Graph::Access &x = g.accesses[a];
        QString out = paragraph(x.what);
        if (!x.tools.isEmpty()) out += textGroup(tr("Tools"), QStringList(x.tools.join(", ").toHtmlEscaped()));
        const QStringList names = reachedBy(a);
        out += textGroup(tr("Reached by"), names.isEmpty() ? QStringList(tr("no AI today")) : names);
        if (!x.rows.isEmpty()) out += group(tr("Data sets"), x.rows);
        return out;
    };

    QString html;
    if (item.kind == Canvas::Ai) {
        const Graph::Ai &ai = g.ais[item.index];
        QString sub;
        QString how;
        switch (ai.kind) {
        case Graph::Ai::Local:
            sub = tr("Local model");
            how = ai.model.isEmpty()
                      ? tr("No slot holds a model: a model has no access outside a slot (AI › Models).")
                      : tr("Runs on this computer in a sandbox with no network and no files: it reaches only the tools "
                           "of the skills its slot grants, through the app, checked again at each call and written to the journal.");
            if (!ai.model.isEmpty() && !ai.on)
                how += ' ' + tr("Suspended: the model's files changed since the slot took it.");
            break;
        case Graph::Ai::Agent:
            sub = tr("Agent outside the app");
            how = tr("Reaches the same tools over MCP, on this computer only (127.0.0.1) and with a token, according to the "
                     "skills ticked in AI › Models › Integration; every call is written to the journal.");
            if (!ai.on) how += ' ' + tr("Serving is off: it reaches nothing until it's turned on.");
            break;
        case Graph::Ai::Cloud:
            sub = tr("Remote model");
            how = tr("Nothing of Deepwater's data is sent to it: the app only checks its key and lists its models.");
            break;
        case Graph::Ai::Training:
            sub = tr("Models trained on this computer");
            how = tr("No tools: Python reads the CSV files AI › Machine learning writes when you export the data, and "
                     "prices only for the symbols you enter.");
            break;
        }
        html = heading(ai.name, sub + " · " + tr("%n access(es)", nullptr, int(ai.accesses.size())));
        if (!ai.model.isEmpty())
            html += QString("<div style='margin-top:6px; color:%1'>%2</div>").arg(ui.TextDim.name(), ai.model.toHtmlEscaped());
        html += paragraph(how);
        // By skill, in the floor's order.
        QStringList bySkill;
        for (int a : ai.accesses) {
            const QString skill = g.accesses[a].skill.isEmpty() ? tr("No skill needed") : g.accesses[a].skill;
            if (!bySkill.contains(skill)) bySkill << skill;
        }
        for (const QString &skill : std::as_const(bySkill)) {
            QStringList names;
            for (int a : ai.accesses)
                if ((g.accesses[a].skill.isEmpty() ? tr("No skill needed") : g.accesses[a].skill) == skill)
                    names << g.accesses[a].name.toHtmlEscaped();
            html += textGroup(ai.kind == Graph::Ai::Training ? tr("In the exported files") : skill, names);
        }
        // Where that comes from: up the shafts, on the floor out of view.
        QStringList origins;
        for (int a : ai.accesses) {
            const Graph::Access &x = g.accesses[a];
            for (int s : x.sources) origins << g.sources[s].name.toHtmlEscaped();
            for (int p : x.parts) origins << partName(p).toHtmlEscaped();
            if (x.stage) origins << tr("Data fusion").toHtmlEscaped();
        }
        origins.removeDuplicates();
        std::sort(origins.begin(), origins.end(), [](const QString &a, const QString &b) { return a.localeAwareCompare(b) < 0; });
        if (!origins.isEmpty()) html += textGroup(tr("Where it comes from"), origins);
    } else if (item.kind == Canvas::Access) {
        const Graph::Access &x = g.accesses[item.index];
        html = heading(x.name, x.skill.isEmpty() ? tr("No skill needed") : tr("Skill: %1").arg(x.skill));
        html += accessCard(item.index);
    } else if (item.kind == Canvas::AiLink) {
        const Graph::AiLink &l = g.aiLinks[item.index];
        html = heading(g.ais[l.ai].name + QString::fromUtf8(" → ") + g.accesses[l.access].name,
                       g.accesses[l.access].skill.isEmpty() ? tr("No skill needed") : tr("Skill: %1").arg(g.accesses[l.access].skill));
        html += accessCard(l.access);
    } else if (item.kind == Canvas::Source) {
        const Graph::Source &s = g.sources[item.index];
        QVector<int> links;
        for (int i = 0; i < g.links.size(); ++i)
            if (g.links[i].source == item.index) links << i;
        std::sort(links.begin(), links.end(), [&](int a, int b) { return g.parts[g.links[a].part].order < g.parts[g.links[b].part].order; });
        html = heading(s.name, s.group + " · " + tr("%n data set(s)", nullptr, int(s.rows.size())) + " · "
                                   + tr("%n part(s) of the app", nullptr, int(links.size())));
        for (int l : std::as_const(links)) html += group(partName(g.links[l].part), g.links[l].rows);
        html += aiReach([&](const Graph::Access &a) { return a.sources.contains(item.index); });
    } else if (item.kind == Canvas::Part) {
        const Graph::Part &p = g.parts[item.index];
        QVector<int> links;
        for (int i = 0; i < g.links.size(); ++i)
            if (g.links[i].part == item.index) links << i;
        std::sort(links.begin(), links.end(), [&](int a, int b) {
            const Graph::Source &x = g.sources[g.links[a].source], &y = g.sources[g.links[b].source];
            return x.groupOrder != y.groupOrder ? x.groupOrder < y.groupOrder : x.name.localeAwareCompare(y.name) < 0;
        });
        html = heading(partName(item.index), tr("%n source(s)", nullptr, p.sources) + " · "
                                                 + tr("%n data set(s)", nullptr, int(p.rows.size())));
        for (int l : std::as_const(links)) html += group(g.sources[g.links[l].source].name, g.links[l].rows);
        html += aiReach([&](const Graph::Access &a) { return a.parts.contains(item.index); });
    } else if (item.kind == Canvas::Listed) {
        // The bodies of one kind the app doesn't read, by country (those of more than ten countries together).
        const Graph::Listed &l = g.listed[item.index];
        const QVector<OfficialSources::Source> &all = OfficialSources::instance().sources();
        html = heading(l.name, tr("%n source(s) listed, not read by the app", nullptr, int(l.sources.size())));
        html += QString("<p style='color:%1; line-height:140%'>%2</p>")
                    .arg(ui.TextDim.name(),
                         (tr("Bodies the official catalogue of the list names behind the countries' figures (from the "
                             "sources Trading Economics credits), whose site the app doesn't read.")
                          + ' ' + tr("%1 free, %2 with the headline figure free, %3 paid; %4 answered the last check of their site.")
                                      .arg(l.free).arg(l.headline).arg(l.paid).arg(l.reachable)
                          + ' ' + tr("The list's section also shows the bodies the app reads, drawn above."))
                             .toHtmlEscaped());
        QMap<QString, QStringList> byCountry;
        const QString several = tr("Several countries");
        for (int i : l.sources) {
            const OfficialSources::Source &s = all[i];
            if (s.countries.isEmpty())
                byCountry[tr("No country")] << s.name;
            else if (s.countries.size() > 10)
                byCountry[several] << tr("%1 (%2 countries)").arg(s.name).arg(s.countries.size());
            else
                for (const QString &c : s.countries) byCountry[OfficialSources::countryName(c)] << s.name;
        }
        QStringList countries = byCountry.keys();
        std::sort(countries.begin(), countries.end(), [&](const QString &a, const QString &b) {
            if ((a == several) != (b == several)) return a == several;
            return a.localeAwareCompare(b) < 0;
        });
        for (const QString &c : std::as_const(countries)) {
            QStringList names = byCountry.value(c);
            names.removeDuplicates();
            std::sort(names.begin(), names.end(), [](const QString &a, const QString &b) { return a.localeAwareCompare(b) < 0; });
            html += QString("<div style='margin-top:10px; color:%1; font-size:10px; font-weight:700; letter-spacing:1px'>%2</div>"
                            "<div style='margin-top:2px; color:%3; line-height:140%'>%4</div>")
                        .arg(ui.TextSection.name(), c.toUpper().toHtmlEscaped(), ui.TextDim.name(),
                             names.join(QString::fromUtf8(" · ")).toHtmlEscaped());
        }
    } else if (item.kind == Canvas::Stage) {
        const Graph::Stage &st = g.stage;
        const QLocale loc;
        html = heading(tr("Data fusion"), tr("%n golden record(s)", nullptr, st.records) + " · "
                                              + tr("%n country(ies)", nullptr, st.countries) + " · "
                                              + tr("%n indicator(s)", nullptr, st.indicators));
        html += QString("<p style='color:%1; line-height:140%'>%2</p>")
                    .arg(ui.TextDim.name(),
                         tr("When a country's indicator has several sources, the app reads them all and makes one "
                            "series of them, a golden record: the most trusted source first, the others filling its "
                            "gaps and its older history once checked to measure the same thing (on another base if "
                            "need be); the sources that measure something else are left out. The tables of Economy › "
                            "Countries and Indicators show these records; their tooltips say where each part comes from.")
                             .toHtmlEscaped());
        QStringList facts;
        facts << tr("%1 periods, of which %2 from a source after the most trusted one")
                     .arg(loc.toString(st.points), loc.toString(st.added));
        if (st.spliced) facts << tr("%n period(s) rebased onto the most trusted source's base", nullptr, st.spliced);
        if (st.unverified) facts << tr("%n period(s) from a source no common period could check", nullptr, st.unverified);
        if (st.conflicts) facts << tr("%n disagreement(s) between sources, the more trusted value kept", nullptr, st.conflicts);
        if (st.latest.isValid()) facts << tr("Last fusion: %1").arg(loc.toString(st.latest.toLocalTime(), QLocale::ShortFormat));
        html += QString("<div style='line-height:150%'>%1</div>").arg(facts.join("<br>").toHtmlEscaped().replace("&lt;br&gt;", "<br>"));
        QStringList names;
        for (int src : st.sources) names << g.sources[src].name;
        std::sort(names.begin(), names.end(), [](const QString &a, const QString &b) { return a.localeAwareCompare(b) < 0; });
        html += QString("<div style='margin-top:12px; color:%1; font-size:10px; font-weight:700; letter-spacing:1px'>%2</div>"
                        "<div style='margin-top:2px; line-height:140%'>%3</div>")
                    .arg(ui.TextSection.name(), tr("Sources combined").toUpper().toHtmlEscaped(),
                         names.join("<br>").toHtmlEscaped().replace("&lt;br&gt;", "<br>"));
        QStringList partNames;
        for (int part : st.parts) partNames << partName(part);
        html += QString("<div style='margin-top:12px; color:%1; font-size:10px; font-weight:700; letter-spacing:1px'>%2</div>"
                        "<div style='margin-top:2px; line-height:140%'>%3</div>")
                    .arg(ui.TextSection.name(), tr("Feeds").toUpper().toHtmlEscaped(), partNames.join("<br>").toHtmlEscaped().replace("&lt;br&gt;", "<br>"));
        html += aiReach([](const Graph::Access &a) { return a.stage; });
    } else if (item.kind == Canvas::Link) {
        const Graph::Link &l = g.links[item.index];
        html = heading(g.sources[l.source].name + QString::fromUtf8(" → ") + partName(l.part),
                       tr("%n data set(s)", nullptr, int(l.rows.size()))
                           + (l.staged ? QString::fromUtf8(" · ") + tr("through data fusion") : QString()));
        html += QString("<div style='margin-top:10px; line-height:150%'>%1</div>").arg(rowsHtml(l.rows));
    } else if (m_canvas->discShown() && m_canvas->floor() == Canvas::Underground) {
        html = heading(tr("The AIs and what they reach"), tr("Hover or click an item of this floor"));
        html += paragraph(tr("On the right, the AIs of the app; on the left, what they can reach of the data, with a shaft up to "
                             "where it comes from. A dashed link is granted but not in force. The up arrow goes back to the "
                             "ground floor."));
        QStringList lines;
        for (const Graph::Ai &ai : g.ais)
            lines << QString("%1 <span style='color:%2'>· %3</span>")
                         .arg(ai.name.toHtmlEscaped(), ui.TextFaint.name(), tr("%n access(es)", nullptr, int(ai.accesses.size())));
        html += textGroup(tr("AIs"), lines);
    } else {
        html = heading(tr("Sources and the app"), tr("Hover or click an item of the diagram"));
        html += QString("<p style='color:%1; line-height:140%'>%2</p>")
                    .arg(ui.TextDim.name(),
                         tr("On the left, the bodies and companies Deepwater reads, by kind; on the right, the pages of "
                            "the app and their sections. The dots and links take the states of the Sources list: green "
                            "working, red failing, grey not checked yet; a dashed link is turned off. The disc view has "
                            "an underground floor, the down arrow goes there: the AIs and what they can reach of this data.")
                             .toHtmlEscaped());
        // The parts fed by the most sources.
        QVector<int> order(g.parts.size());
        std::iota(order.begin(), order.end(), 0);
        std::sort(order.begin(), order.end(), [&](int a, int b) { return g.parts[a].sources > g.parts[b].sources; });
        html += QString("<div style='margin-top:12px; color:%1; font-size:10px; font-weight:700; letter-spacing:1px'>%2</div><div style='margin-top:4px; line-height:150%'>")
                    .arg(ui.TextSection.name(), tr("Parts with the most sources").toUpper().toHtmlEscaped());
        for (int i = 0; i < order.size() && i < 6; ++i)
            html += QString("%1 <span style='color:%2'>· %3</span><br>")
                        .arg(partName(order[i]).toHtmlEscaped(), ui.TextFaint.name(), tr("%n source(s)", nullptr, g.parts[order[i]].sources));
        html += "</div>";
    }
    if (m_canvas->pinned.kind == Canvas::None && item.kind != Canvas::None)
        html += QString("<p style='color:%1'>%2</p>").arg(ui.TextFaint.name(), tr("Click to keep it here.").toHtmlEscaped());
    m_detail->setText(html);
}
