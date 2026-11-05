#include "LocalTools.h"

#include "CountryData.h"
#include "DomesticFinance.h"
#include "FxData.h"
#include "MacroData.h"
#include "QuantData.h"
#include "ResearchData.h"
#include "quant/ui/App.h"
#include "quant/ui/Panels.h"
#include "fusion/Store.h"

#include <QDate>
#include <QDir>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QPointer>
#include <QRegularExpression>

#include <algorithm>
#include <cmath>

namespace {

QJsonObject schema(const QJsonObject &properties, const QStringList &required = {})
{
    return {{"type", "object"}, {"properties", properties}, {"required", QJsonArray::fromStringList(required)}};
}

QJsonObject prop(const char *type, const QString &description)
{
    return {{"type", type}, {"description", description}};
}

QJsonObject error(const QString &message)
{
    return {{"error", message}};
}

// Six significant digits: enough for any of these figures, and fewer tokens.
double tidy(double v)
{
    return QString::number(v, 'g', 6).toDouble();
}

int lastArg(const QJsonObject &a, int fallback, int max)
{
    const int n = a.value("last").toInt(fallback);
    return std::clamp(n, 1, max);
}

// A fractional year as a period, from the spacing of its series: "2024", "2024-Q2", "2024-05".
QString periodLabel(double t, double step)
{
    const int year = int(std::floor(t + 1e-9));
    const double frac = t - year;
    if (frac < 1e-6 && step > 0.9) return QString::number(year);
    if (step > 0.2 && step < 0.3) return QString("%1-Q%2").arg(year).arg(int(frac * 4) + 1);
    // A weekly or daily series: its date (to the day or so), else several points would read as the same month.
    if (step < 0.05) return QDate(year, 1, 1).addDays(qRound(frac * QDate(year, 1, 1).daysInYear())).toString(Qt::ISODate);
    if (step < 0.15) return QString("%1-%2").arg(year).arg(int(frac * 12) + 1, 2, 10, QChar('0'));
    return QString::number(t, 'f', 2);
}

double medianStep(const QMap<double, double> &s)
{
    QVector<double> d;
    double prev = NAN;
    for (auto it = s.begin(); it != s.end(); ++it) {
        if (!std::isnan(prev)) d << it.key() - prev;
        prev = it.key();
    }
    if (d.isEmpty()) return 1;
    std::nth_element(d.begin(), d.begin() + d.size() / 2, d.end());
    return d[d.size() / 2];
}

// Drops the oldest rows of `key` until the result is short enough, and says so.
QJsonObject fit(QJsonObject result, const QString &key)
{
    QJsonArray rows = result.value(key).toArray();
    while (rows.size() > 1 && QJsonDocument(result).toJson(QJsonDocument::Compact).size() > LocalTools::MaxResultChars) {
        const int drop = std::max<qsizetype>(1, rows.size() / 4);
        for (int i = 0; i < drop; ++i) rows.removeFirst();
        result.insert(key, rows);
        result.insert("truncated", "the oldest rows were left out: ask for fewer with \"last\"");
    }
    return result;
}

} // namespace

static LocalTools::Organizer &organizer()
{
    static LocalTools::Organizer instance;
    return instance;
}

LocalTools::LocalTools(const MlDataExporter::Sources &sources, QObject *parent)
    : QObject(parent)
    , m_s(sources)
{
}

const QVector<LocalTools::Tool> &LocalTools::tools()
{
    const QString country = "Country: ISO3 code (FRA, USA, DEU), ISO2 code or English name";
    static const QVector<Tool> list {
        {"list_data", "read_data",
         "What Deepwater holds: the country series by source with their series names, the IMF WEO indicators, the FX "
         "pairs, the COT markets. Call it first when unsure of a source or series name.",
         schema({})},
        {"read_page", "read_data",
         "What a page of Deepwater shows, as data: its tables, figures and the values of its charts. Any page and section, open or "
         "not: the user's screen doesn't move, and a section not open may take a few seconds to load. Call it when the user speaks of "
         "\"this table\", \"this chart\" or what a page shows. Without arguments: what is on screen. Sources and AI can't be read. A "
         "long page comes in parts.",
         schema({{"page", prop("string", "map, overview, economy, forex, charts, calendar, news, research, quant, desk or journal; omit for the page on screen")},
                 {"section", prop("string", "A section of that page, as the result's `sections` lists them, e.g. Inflation")},
                 {"country", prop("string", country + ": a country's sheet in Economy")},
                 {"tab", prop("string", "The tab of that sheet, e.g. Energy, Prices")},
                 {"indicator", prop("string", "An indicator's table of countries in Economy, by its English name")},
                 {"part", prop("integer", "Which part of a long page, from 1 (default 1)")}})},
        {"open_page", "read_data",
         "Takes the user to a page, a section or a sheet of Deepwater: it moves their screen. Call it only when they ask to be taken there.",
         schema({{"page", prop("string", "map, overview, economy, forex, charts, calendar, news, research, quant, desk or journal; omit for the page on screen")},
                 {"section", prop("string", "A section of that page, as the result's `sections` lists them, e.g. Inflation")},
                 {"country", prop("string", country + ": a country's sheet in Economy")},
                 {"tab", prop("string", "The tab of that sheet, e.g. Energy, Prices")},
                 {"indicator", prop("string", "An indicator's table of countries in Economy, by its English name")}})},
        {"country_series", "read_data",
         "A statistical series of a country from one source (official statistics, central banks, BIS, IMF, World "
         "Bank). Without a country: the latest value of every country, highest first.",
         schema({{"source", prop("string", "Source id from list_data, e.g. oecd, eurostat, bls, bis_property")},
                 {"series", prop("string", "Series name from list_data, e.g. cpi_yoy, unemployment")},
                 {"country", prop("string", country + "; omit to compare all countries")},
                 {"last", prop("integer", "Number of latest observations (default 12, at most 120)")}},
                {"source", "series"})},
        {"golden_series", "read_data",
         "The richest series Deepwater holds for an indicator of a country: a golden record, where the app fused the "
         "indicator's official sources (national office or central bank first, then Eurostat, OECD, IMF, World "
         "Bank…), each filling the gaps and older history of the more trusted ones once checked to measure the same "
         "thing. Every point says its source. Prefer it to country_series when list_data shows the country has one.",
         schema({{"country", prop("string", country)},
                 {"indicator", prop("string", "Indicator id or English name of Economy › Countries, e.g. inflation, "
                                              "unemployment, gdp_growth, cpi, \"Food Inflation\"")},
                 {"frequency", prop("string", "monthly, quarterly or annual (default: the finest the record has)")},
                 {"last", prop("integer", "Number of latest observations (default 12, at most 120)")}},
                {"country", "indicator"})},
        {"imf_weo", "read_data",
         "IMF World Economic Outlook, yearly, with the IMF's projections for the coming years.",
         schema({{"indicator", prop("string", "One of: " + MlDataExporter::weoIndicators().join(", "))},
                 {"country", prop("string", country)},
                 {"from_year", prop("integer", "First year (default: 10 years ago)")},
                 {"to_year", prop("integer", "Last year (default: 5 years ahead)")}},
                {"indicator", "country"})},
        {"policy_rate", "read_data",
         "The central bank policy rate of a country (BIS), monthly, in percent. Euro area countries share the ECB's.",
         schema({{"country", prop("string", country + ", or EA for the euro area")},
                 {"last", prop("integer", "Number of latest months (default 24, at most 240)")}},
                {"country"})},
        {"fx_rate", "read_data",
         "Daily reference exchange rate (European Central Bank) of a pair from list_data.",
         schema({{"pair", prop("string", "Pair such as EUR/USD or EURUSD")},
                 {"last", prop("integer", "Number of latest days (default 30, at most 260)")}},
                {"pair"})},
        {"cot_positions", "read_data",
         "CFTC Commitments of Traders (legacy), weekly positions in a futures market: open interest, non-commercial "
         "(speculators) and commercial (hedgers) longs, shorts and nets.",
         schema({{"market", prop("string", "Market id or label from list_data, e.g. sp500, gold")},
                 {"last", prop("integer", "Number of latest weeks (default 12, at most 104)")}},
                {"market"})},
        {"prices", "read_data",
         "Prices of a stock, ETF, index, currency or crypto from Yahoo Finance, fetched by the app: the summary and "
         "the latest bars.",
         schema({{"symbol", prop("string", "Yahoo symbol: AAPL, SPY, ^GSPC, ^VIX, EURUSD=X, BTC-USD, GC=F")},
                 {"range", prop("string", "1mo, 3mo, 6mo, 1y, 2y, 5y or 10y (default 1y)")},
                 {"interval", prop("string", "1d, 1wk or 1mo (default 1d)")}},
                {"symbol"})},

        // ---- trade: the Charts tab's tester and live paper account
        {"list_workspaces", "organize",
         "The Desk's workspaces with their panels, the sections a panel can be, and the Journal's accounts. Call it first when unsure of a name.",
         schema({})},
        {"open_workspace", "organize",
         "Shows a Desk workspace: it moves the user's screen. Call it only when they ask.",
         schema({{"name", prop("string", "The workspace's name, as list_workspaces gives it")}}, {"name"})},
        {"create_workspace", "organize",
         "Makes a new Desk workspace and shows it: it moves the user's screen. Call it only when they ask. Nothing existing changes.",
         schema({{"name", prop("string", "Its name, 1 to 40 characters, not one that exists")},
                 {"panels", QJsonObject {{"type", "array"}, {"items", QJsonObject {{"type", "string"}}},
                                         {"description", "1 to 6 section names, as list_workspaces gives them"}}}},
                {"name", "panels"})},
        {"switch_journal_account", "organize",
         "Changes the Journal's current account. Call it only when the user asks.",
         schema({{"name", prop("string", "The account's name, as list_workspaces gives it")}}, {"name"})},
        {"draft_journal_trade", "organize",
         "Opens the Journal's trade form filled in, for the user to check. Nothing is written unless they press Save: the result says "
         "whether they did. Only for a trade the user described: never invent a figure, leave out what they didn't give.",
         schema({{"symbol", prop("string", "As their broker writes it, e.g. EURUSD")},
                 {"side", prop("string", "long or short")},
                 {"volume", prop("number", "Lots")},
                 {"open_time", prop("string", "ISO date and time; omit for now")},
                 {"close_time", prop("string", "ISO date and time; omit while the trade is open")},
                 {"open_price", prop("number", "Entry price")},
                 {"close_price", prop("number", "Exit price")},
                 {"stop_loss", prop("number", "Stop loss")},
                 {"take_profit", prop("number", "Take profit")},
                 {"profit", prop("number", "Gross result in the account's currency")},
                 {"strategy", prop("string", "The strategy's name")},
                 {"notes", prop("string", "The user's notes")}},
                {"symbol", "side", "volume"})},
        {"backtest_help", "trade",
         "How to write a strategy for run_backtest: the JSON format, the operators, the price fields, the indicators "
         "(id, default params, outputs), ready-made examples and the user's saved strategies. Call it before writing "
         "a strategy.",
         schema({})},
        {"run_backtest", "trade",
         "Runs a strategy in the strategy tester of the Charts tab, on the active chart (switched to symbol and "
         "timeframe when given), as the user would; returns the stats and the last trades. The user sees the run.",
         schema({{"strategy", QJsonObject {{"description", "A strategy object (see backtest_help), or the name of an "
                                                           "example or saved strategy"}}},
                 {"symbol", prop("string", "Symbol as the Charts tab writes it (AAPL, SPY, BTCUSDT…); omit for the active chart's")},
                 {"tf", prop("string", "Timeframe: 1m, 5m, 15m, 1h, 4h, 1D, 1W; omit for the active chart's")}},
                {"strategy"})},
        {"backtest_trades_csv", "trade",
         "The trades of the last backtest as the tester's CSV export (saved to a file too); read it to analyse "
         "the trades.",
         schema({})},
        {"paper_account", "trade",
         "The live paper trading account of the Charts tab (simulated money): cash, equity, P&L, open positions, "
         "working orders with their ids, latest fills.",
         schema({})},
        {"paper_order", "trade",
         "Places an order on the live paper account, as the order ticket does: market (filled now at the live price), "
         "limit or stop (working until the price reaches it), with an optional stop loss and take profit in %. "
         "Simulated money; only when the user asks to trade.",
         schema({{"symbol", prop("string", "Symbol as the Charts tab writes it")},
                 {"side", prop("string", "buy, or sell (sells short when no position)")},
                 {"type", prop("string", "market (default), limit or stop")},
                 {"amount", prop("number", "Amount in the account's currency (not a quantity)")},
                 {"price", prop("number", "Price of a limit or stop order")},
                 {"stop_loss_pct", prop("number", "Optional stop loss, % from the entry")},
                 {"take_profit_pct", prop("number", "Optional take profit, % from the entry")}},
                {"symbol", "side", "amount"})},
        {"paper_close", "trade",
         "Closes the open paper position in a symbol at the live price, and cancels its working orders.",
         schema({{"symbol", prop("string", "Symbol of the position")}}, {"symbol"})},
        {"paper_cancel", "trade",
         "Cancels a working paper order by its id (from paper_account).",
         schema({{"order_id", prop("string", "The order's id")}}, {"order_id"})},
    };
    return list;
}

bool LocalTools::isOrder(const QString &tool)
{
    return tool == "paper_order" || tool == "paper_close" || tool == "paper_cancel";
}

QString LocalTools::exportsDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/ai/exports";
}

QVector<LocalTools::Tool> LocalTools::toolsFor(const QStringList &skills)
{
    QVector<Tool> out;
    for (const Tool &t : tools())
        if (skills.contains(t.skill)) out << t;
    return out;
}

QString LocalTools::systemPrompt(const QStringList &skills, const QString &userPrompt, bool local)
{
    QString p = (local ? QStringLiteral("You are a model running locally inside Deepwater, a desktop application for markets and economics. ")
                       : QStringLiteral("You are answering inside Deepwater, a desktop application for markets and economics. Your own "
                                        "tools are off here (no shell, files or web); the tools below, if any, are all you can use. ")) +
                "Today is " + QDate::currentDate().toString(Qt::ISODate) + ". Answer in the language of the question. You only see what is in this conversation, not the "
                "screen: if the user speaks of a table or a chart you have no way to read, say you can't see it and ask what "
                "they want to know, rather than describe one you imagine.";
    const QVector<Tool> list = toolsFor(skills);
    if (list.isEmpty()) {
        p += " You have no access to Deepwater's data: answer from the conversation only, and say so when asked "
             "for figures.";
    } else {
        if (skills.contains("read_data"))
            p += " Use the tools to read Deepwater's data whenever a question needs figures: never invent a figure, "
                 "give the period of each one, and say when the data isn't there. When the user speaks of a table, a chart or what a "
                 "page shows, call read_page to read it first: it reads any page without moving them. When they ask to be taken "
                 "somewhere, and only then, call open_page. Call list_data when unsure of a name, use the names "
                 "a tool lists, and don't retry a name it refused.";
        if (skills.contains("trade"))
            p += " You can run backtests in the Charts tab's strategy tester and trade its paper account (simulated "
                 "money). Call backtest_help before writing a strategy. Place, close or cancel paper orders only when "
                 "the user asks you to trade, and report every order you placed. At most " + QString::number(MaxOrders) +
                 " orders per question.";
        if (skills.contains("organize"))
            p += " You can open and create Desk workspaces and change the Journal's account, only when the user asks. "
                 "draft_journal_trade opens a form the user must save: never say a trade is saved unless the result says so, "
                 "and never invent one of its figures.";
        p += "\n\n# Tools\n\nYou may call one or more "
             "functions to assist with the user query.\n\nYou are provided with function signatures within "
             "<tools></tools> XML tags:\n<tools>\n";
        for (const Tool &t : list) {
            const QJsonObject f {{"type", "function"},
                                 {"function", QJsonObject {{"name", t.name}, {"description", t.description},
                                                           {"parameters", t.parameters}}}};
            p += QString::fromUtf8(QJsonDocument(f).toJson(QJsonDocument::Compact)) + '\n';
        }
        p += "</tools>\n\nFor each function call, return a json object with function name and arguments within "
             "<tool_call></tool_call> XML tags:\n<tool_call>\n{\"name\": <function-name>, \"arguments\": "
             "<args-json-object>}\n</tool_call>";
    }
    if (!userPrompt.trimmed().isEmpty()) p += "\n\n" + userPrompt.trimmed();
    return p;
}

QVector<LocalTools::Call> LocalTools::parseCalls(const QString &answer)
{
    QVector<Call> out;
    static const QRegularExpression re("<tool_call>(.*?)(</tool_call>|$)", QRegularExpression::DotMatchesEverythingOption);
    for (auto it = re.globalMatch(answer); it.hasNext();) {
        const QRegularExpressionMatch m = it.next();
        const QString body = m.captured(1).trimmed();
        if (body.isEmpty()) continue;
        Call c;
        QJsonParseError pe;
        const QJsonDocument doc = QJsonDocument::fromJson(body.toUtf8(), &pe);
        if (!doc.isObject()) {
            c.error = "the tool call isn't a JSON object: " + pe.errorString();
        } else {
            const QJsonObject o = doc.object();
            c.name = o.value("name").toString();
            const QJsonValue args = o.value("arguments");
            // Some models write the arguments as a JSON string.
            c.arguments = args.isString() ? QJsonDocument::fromJson(args.toString().toUtf8()).object() : args.toObject();
            if (c.name.isEmpty()) c.error = "the tool call has no \"name\"";
        }
        out << c;
    }
    return out;
}

QString LocalTools::withoutCalls(const QString &answer)
{
    static const QRegularExpression re("<tool_call>.*?(</tool_call>|$)", QRegularExpression::DotMatchesEverythingOption);
    return QString(answer).remove(re).trimmed();
}

QString LocalTools::responses(const QVector<QJsonObject> &results)
{
    QStringList parts;
    for (const QJsonObject &r : results)
        parts << "<tool_response>\n" + QString::fromUtf8(QJsonDocument(r).toJson(QJsonDocument::Compact)) + "\n</tool_response>";
    return parts.join('\n');
}

void LocalTools::call(const Call &c, const QStringList &skills, const Done &done)
{
    if (!c.error.isEmpty()) {
        done(error(c.error));
        return;
    }
    const auto tool = std::find_if(tools().begin(), tools().end(), [&](const Tool &t) { return t.name == c.name; });
    if (tool == tools().end()) {
        done(error("no tool named \"" + c.name + "\""));
        return;
    }
    if (!skills.contains(tool->skill)) {
        done(error("not allowed: the slot holding this model doesn't grant the skill \"" + tool->skill + "\""));
        return;
    }
    const QJsonObject &a = c.arguments;
    if (c.name == "list_data") done(listData());
    else if (c.name == "read_page") readPage(a, done);
    else if (c.name == "open_page") done(openPage(a));
    else if (c.name == "country_series") done(countrySeries(a));
    else if (c.name == "imf_weo") done(weo(a));
    else if (c.name == "policy_rate") done(policyRate(a));
    else if (c.name == "fx_rate") done(fxRate(a));
    else if (c.name == "cot_positions") done(cot(a));
    else if (c.name == "golden_series") done(golden(a));
    else if (c.name == "prices") prices(a, done);
    else if (tool->skill == "trade") trade(c, done);
    else if (tool->skill == "organize") {
        if (!organizer())
            return done(error("the app isn't ready"));
        organizer()(c.name, a, c.question, done);
    }
}

void LocalTools::trade(const Call &c, const Done &done)
{
    qtm::App *app = qtm::App::current();
    if (!app || !app->panels) return done(error("the Charts tab isn't available"));
    qtm::Panels *panels = app->panels;
    const QJsonObject &a = c.arguments;
    if (c.name == "backtest_help") {
        return done(panels->agentStrategyHelp());
    }
    if (c.name == "run_backtest")
        return panels->agentBacktest(a.value("symbol").toString(), a.value("tf").toString(), a.value("strategy"), done);
    if (c.name == "backtest_trades_csv") {
        QJsonObject r = panels->agentTradesCsv(QString());
        if (r.contains("error")) return done(r);
        QString name = QString("%1-%2-trades.csv").arg(r.value("symbol").toString(), r.value("tf").toString());
        name.replace(QRegularExpression("[^A-Za-z0-9._-]"), "_");
        r = panels->agentTradesCsv(exportsDir() + '/' + name);
        // The model reads the header and as many of the latest trades as fit.
        QStringList lines = r.value("csv").toString().split('\n');
        const QString header = lines.isEmpty() ? QString() : lines.takeFirst();
        QStringList kept;
        int size = int(header.size());
        for (qsizetype i = lines.size() - 1; i >= 0 && size + lines[i].size() + 1 < MaxResultChars - 400; --i) {
            kept.prepend(lines[i]);
            size += int(lines[i].size()) + 1;
        }
        r.insert("csv", header + '\n' + kept.join('\n'));
        if (kept.size() < lines.size())
            r.insert("truncated", QString("the CSV below has only the latest %1 of the %2 trades (the file has them all): "
                                          "use \"summary\" for figures over every trade").arg(kept.size()).arg(lines.size()));
        return done(r);
    }
    if (c.name == "paper_account") return done(panels->agentPaperAccount());
    if (c.name == "paper_order") return panels->agentPaperOrder(a, done);
    if (c.name == "paper_close") return done(panels->agentPaperClose(a.value("symbol").toString()));
    if (c.name == "paper_cancel") return done(panels->agentPaperCancel(a.value("order_id").toString()));
    done(error("unknown trade tool"));
}

QString LocalTools::country(const QString &text) const
{
    const QString t = text.trimmed();
    if (t.isEmpty() || !m_s.economy) return t.toUpper();
    for (const WorldEconomy::Country &c : m_s.economy->countries())
        if (c.iso3.compare(t, Qt::CaseInsensitive) == 0 || c.iso2.compare(t, Qt::CaseInsensitive) == 0
            || c.name.compare(t, Qt::CaseInsensitive) == 0)
            return c.iso3;
    return t.toUpper();   // an aggregate a set keeps under its own code (EA21, EU27…)
}

namespace {
LocalTools::PageReader &pageReader()
{
    static LocalTools::PageReader reader;
    return reader;
}
LocalTools::PageOpener &pageOpener()
{
    static LocalTools::PageOpener opener;
    return opener;
}
} // namespace

void LocalTools::setOrganizer(Organizer o)
{
    organizer() = std::move(o);
}

void LocalTools::setPageAccess(PageReader reader, PageOpener opener)
{
    pageReader() = std::move(reader);
    pageOpener() = std::move(opener);
}

AiPageRequest LocalTools::pageRequest(const QJsonObject &a) const
{
    AiPageRequest r;
    r.page = a.value("page").toString().trimmed().toLower();
    r.section = a.value("section").toString().trimmed();
    if (const QString c = a.value("country").toString().trimmed(); !c.isEmpty()) r.country = country(c);
    r.tab = a.value("tab").toString().trimmed();
    r.indicator = a.value("indicator").toString().trimmed();
    return r;
}

void LocalTools::readPage(const QJsonObject &a, const Done &done) const
{
    if (!pageReader()) {
        done(error("no page to read in this context"));
        return;
    }
    const int wantedPart = a.value("part").toInt(1);
    pageReader()(pageRequest(a), [done, wantedPart](const AiPageRead &read) {
        constexpr int PartChars = 5000;
        if (!read.error.isEmpty()) {
            QJsonObject out {{"error", read.error}};
            if (!read.sections.isEmpty()) out.insert("sections", QJsonArray::fromStringList(read.sections));
            done(out);
            return;
        }
        if (read.text.isEmpty()) {
            done({{"error", "nothing to read on \"" + read.page + "\" yet: it may still be loading"}});
            return;
        }
        const int parts = int(read.text.size() + PartChars - 1) / PartChars;
        const int part = std::clamp(wantedPart, 1, parts);
        QJsonObject out {{"page", read.page}, {"on_screen", read.onScreen}, {"part", part}, {"parts", parts},
                         {"content", read.text.mid((part - 1) * PartChars, PartChars)},
                         {"how_to_read", "As the page shows it, in the app's language; a table's cells are apart by \" | \". With columns such as "
                                         "Last, Previous, Highest, Lowest, the first number is the indicator's latest value in the row's unit: a level, "
                                         "unless the row's name says it is a change. Answer in the language of the user's question."}};
        if (!read.section.isEmpty()) out.insert("section", read.section);
        if (!read.sections.isEmpty()) out.insert("sections", QJsonArray::fromStringList(read.sections));
        if (read.stillLoading)
            out.insert("still_loading", "Some values hadn't arrived after a few seconds: an empty value (a dash) doesn't mean there is no data. Say so.");
        done(out);
    });
}

QJsonObject LocalTools::openPage(const QJsonObject &a) const
{
    if (!pageOpener()) return error("no page to open in this context");
    QString failed;
    const QString where = pageOpener()(pageRequest(a), &failed);
    if (!failed.isEmpty()) return error(failed);
    // A small model describes what it has just opened, from nothing.
    return {{"opened", where},
            {"note", "The user now sees this page. You have read nothing of it: say in one sentence that it's open, and give no figure "
                     "or description of it unless you call read_page first."}};
}

QJsonObject LocalTools::listData() const
{
    QJsonArray sets;
    for (const MlDataExporter::CountrySet &s : MlDataExporter::countrySets(m_s)) {
        if (!s.set->hasData()) continue;
        sets.append(QJsonObject {{"source", s.source}, {"series", QJsonArray::fromStringList(s.series)},
                                 {"countries", int(s.set->countries().size())}});
    }
    QJsonObject out {{"country_series", sets}, {"country_codes", "ISO3: FRA, USA, DEU, GBR, JPN, CHN…"}};
    if (m_s.economy && m_s.economy->hasData())
        out.insert("imf_weo_indicators", QJsonArray::fromStringList(MlDataExporter::weoIndicators()));
    if (m_s.fx) {
        QStringList pairs;
        for (const FxPair &p : m_s.fx->pairs()) pairs << p.label();
        out.insert("fx_pairs", QJsonArray::fromStringList(pairs));
    }
    if (m_s.cot) {
        QJsonArray markets;
        for (const CotReport::Market &m : m_s.cot->markets())
            markets.append(QJsonObject {{"id", m.id}, {"label", m.label}});
        out.insert("cot_markets", markets);
    }
    out.insert("prices", "any Yahoo Finance symbol, through the prices tool");
    // Golden records (golden_series): how many each country has; the indicators come with golden_series' answers.
    QJsonObject golden;
    for (const fusion::Summary &f : fusion::loadIndex(countries::fusionDir()))
        if (f.records > 0)
            golden.insert(f.country, golden.value(f.country).toInt() + 1);
    if (!golden.isEmpty())
        out.insert("golden_records_by_country", golden);
    return out;
}

namespace {

QString goldenPeriod(const QDate &d, fusion::Freq f)
{
    switch (f) {
    case fusion::Freq::Annual: return QString::number(d.year());
    case fusion::Freq::Quarterly: return QStringLiteral("%1-Q%2").arg(d.year()).arg((d.month() - 1) / 3 + 1);
    case fusion::Freq::Monthly: return d.toString("yyyy-MM");
    default: return d.toString(Qt::ISODate);
    }
}

} // namespace

QJsonObject LocalTools::golden(const QJsonObject &a) const
{
    const QString iso3 = country(a.value("country").toString());
    const QString dir = countries::fusionDir();
    QStringList have;   // the indicators this country has records for
    for (const fusion::Summary &f : fusion::loadIndex(dir))
        if (f.country == iso3 && f.records > 0) have << f.indicator;
    have.sort();
    const QString hint = have.isEmpty()
        ? QString("no golden record for %1 yet: the app fuses an indicator's sources when the user opens the country in "
                  "Economy › Countries (each tab fuses its rows)").arg(iso3)
        : QString("golden records for %1: %2").arg(iso3, have.join(", "));
    // Most series aren't golden records: where to go instead of trying other names.
    const QString elsewhere = " Anything else (energy, housing, surveys…) is under country_series: list_data gives its sources "
                              "and series, e.g. source eia for US energy.";

    // The indicator: a catalogue id, or a name (exact, then contained).
    const QString want = a.value("indicator").toString().trimmed();
    const countries::Catalogue &cat = countries::catalogue();
    QString id;
    if (cat.indicators.contains(want.toLower()))
        id = want.toLower();
    for (int pass = 0; pass < 2 && id.isEmpty(); ++pass)
        for (auto it = cat.indicators.cbegin(); it != cat.indicators.cend() && id.isEmpty(); ++it) {
            const QString key = QString(it.key()).replace('_', ' ');
            if (pass == 0 ? (it->name.compare(want, Qt::CaseInsensitive) == 0 || key.compare(want, Qt::CaseInsensitive) == 0)
                          : (have.contains(it.key()) && (it->name.contains(want, Qt::CaseInsensitive) || key.contains(want, Qt::CaseInsensitive))))
                id = it.key();
        }
    if (id.isEmpty()) return error("unknown indicator \"" + want + "\"; " + hint + "." + elsewhere);
    const std::optional<fusion::Record> record = fusion::load(dir, iso3, id);
    if (!record) return error("no golden record of " + id + " for " + iso3 + "; " + hint + "." + elsewhere);

    // The frequency asked for, else the finest record of two sources or more, else the finest.
    const QString freq = a.value("frequency").toString().trimmed().toLower();
    const fusion::Golden *g = nullptr;
    QStringList others;
    for (const fusion::Golden &x : record->golden) {
        if (!freq.isEmpty() ? fusion::freqName(x.freq) == freq : (!g || (g->sources.size() < 2 && x.sources.size() >= 2)))
            g = &x;
    }
    for (const fusion::Golden &x : record->golden)
        if (&x != g) others << fusion::freqName(x.freq);
    if (!g || g->points.isEmpty())
        return error("no " + (freq.isEmpty() ? QString("data") : freq + " record") + " of " + id + " for " + iso3
                     + (others.isEmpty() ? QString() : "; it has: " + others.join(", ")));

    auto label = [&](int source) { return source >= 0 && source < g->sources.size() ? g->sources[source].label : QString(); };
    QJsonArray sources;
    for (const fusion::SourceUse &u : g->sources) {
        QJsonObject o {{"source", u.label}, {"relation", fusion::relationName(u.comparison.relation)}, {"periods_used", u.used}};
        if (u.comparison.relation != fusion::Relation::Anchor) o.insert("why", u.comparison.why);
        sources.append(o);
    }
    const fusion::GoldenPoint &end = g->points.back();
    QJsonObject latest {{"period", goldenPeriod(end.date, g->freq)}, {"value", tidy(end.value)}, {"source", label(end.source)}};
    const int last = lastArg(a, 12, 120);
    QJsonArray points;
    for (qsizetype i = std::max<qsizetype>(0, g->points.size() - last); i < g->points.size(); ++i) {
        const fusion::GoldenPoint &p = g->points[i];
        QJsonObject o {{"period", goldenPeriod(p.date, g->freq)}, {"value", tidy(p.value)}, {"source", label(p.source)}};
        if (p.spliced) o.insert("rebased", true);
        if (p.unverified) o.insert("unchecked", true);
        points.append(o);
    }
    QJsonObject out {{"country", iso3}, {"indicator", id}, {"name", cat.indicators.value(id).name},
                     {"frequency", fusion::freqName(g->freq)}, {"unit", g->unit},
                     {"span", goldenPeriod(g->points.front().date, g->freq) + " to " + goldenPeriod(end.date, g->freq)},
                     {"periods", int(g->points.size())}, {"fused", record->fused.toString(Qt::ISODate)},
                     {"latest", latest}, {"sources", sources}, {"disagreements", int(g->conflicts.size())}};
    if (!others.isEmpty()) out.insert("other_frequencies", QJsonArray::fromStringList(others));
    out.insert("points", points);
    return fit(out, "points");
}

QJsonObject LocalTools::countrySeries(const QJsonObject &a) const
{
    const QString source = a.value("source").toString().trimmed().toLower();
    const QString series = a.value("series").toString().trimmed().toLower();
    const QVector<MlDataExporter::CountrySet> sets = MlDataExporter::countrySets(m_s);
    const auto set = std::find_if(sets.begin(), sets.end(), [&](const MlDataExporter::CountrySet &s) { return s.source == source; });
    if (set == sets.end()) return error("unknown source \"" + source + "\": see list_data");
    const int k = int(set->series.indexOf(series));
    if (k < 0) return error("source " + source + " has no series \"" + series + "\"; it has: " + set->series.join(", "));
    if (!set->set->hasData()) return error("source " + source + " isn't loaded in Deepwater yet");

    const QString text = a.value("country").toString();
    if (text.trimmed().isEmpty()) {
        struct Row { QString iso3; double value; double time; };
        QVector<Row> rows;
        for (const QString &iso3 : set->set->countries()) {
            double time = 0;
            const double v = set->set->latest(iso3, k, &time);
            if (std::isfinite(v)) rows << Row {iso3, v, time};
        }
        std::sort(rows.begin(), rows.end(), [](const Row &x, const Row &y) { return x.value > y.value; });
        QJsonArray out;
        for (const Row &r : rows.mid(0, 60)) {
            const WorldEconomy::Country *c = m_s.economy ? m_s.economy->country(r.iso3) : nullptr;
            const double step = medianStep(set->set->series(r.iso3, k));
            out.append(QJsonObject {{"country", r.iso3}, {"name", c ? c->name : r.iso3},
                                    {"period", periodLabel(r.time, step)}, {"value", tidy(r.value)}});
        }
        return fit({{"source", source}, {"series", series}, {"latest_by_country", out}}, "latest_by_country");
    }
    const QString iso3 = country(text);
    const QMap<double, double> s = set->set->series(iso3, k);
    if (s.isEmpty()) return error(source + " has no " + series + " for " + iso3);
    const double step = medianStep(s);
    const QJsonObject latest {{"period", periodLabel(s.lastKey(), step)}, {"value", tidy(s.last())}};
    const int last = lastArg(a, 12, 120);
    QJsonArray points;
    int skip = int(s.size()) - last;
    for (auto it = s.begin(); it != s.end(); ++it)
        if (skip-- <= 0) points.append(QJsonObject {{"period", periodLabel(it.key(), step)}, {"value", tidy(it.value())}});
    return fit({{"source", source}, {"series", series}, {"country", iso3}, {"latest", latest}, {"points", points}}, "points");
}

QJsonObject LocalTools::weo(const QJsonObject &a) const
{
    if (!m_s.economy || !m_s.economy->hasData()) return error("the IMF WEO isn't loaded in Deepwater yet");
    const QString name = a.value("indicator").toString().trimmed().toLower();
    const int k = int(MlDataExporter::weoIndicators().indexOf(name));
    if (k < 0) return error("unknown indicator; one of: " + MlDataExporter::weoIndicators().join(", "));
    const QString iso3 = country(a.value("country").toString());
    const int year = QDate::currentDate().year();
    const int from = a.value("from_year").toInt(year - 10), to = a.value("to_year").toInt(year + 5);
    QJsonArray points;
    const QMap<int, double> s = m_s.economy->series(iso3, WorldEconomy::Indicator(k));
    for (auto it = s.begin(); it != s.end(); ++it)
        if (it.key() >= from && it.key() <= to && std::isfinite(it.value()))
            points.append(QJsonObject {{"year", it.key()}, {"value", tidy(it.value())}});
    if (points.isEmpty()) return error("no " + name + " for " + iso3 + " in the IMF WEO");
    return fit({{"indicator", name}, {"country", iso3},
                {"note", "years after the last reported one are IMF projections"}, {"points", points}}, "points");
}

QJsonObject LocalTools::policyRate(const QJsonObject &a) const
{
    if (!m_s.rates) return error("the policy rates aren't loaded in Deepwater");
    const QString text = a.value("country").toString().trimmed();
    QString iso2;
    if (QStringList {"EA", "EZ", "XM", "EURO AREA", "EUROZONE", "ECB"}.contains(text.toUpper())) {
        iso2 = "XM";
    } else {
        const WorldEconomy::Country *c = m_s.economy ? m_s.economy->country(country(text)) : nullptr;
        if (!c) return error("unknown country \"" + text + "\"");
        iso2 = c->iso2;
    }
    const QVector<WorldPolicyRates::Point> h = m_s.rates->history(iso2);
    if (h.isEmpty()) return error("no policy rate for " + text);
    QJsonArray points;
    for (const WorldPolicyRates::Point &p : h.mid(std::max<qsizetype>(0, h.size() - lastArg(a, 24, 240))))
        points.append(QJsonObject {{"month", p.month.toString("yyyy-MM")}, {"rate", tidy(p.rate)}});
    return fit({{"country", text}, {"unit", "percent"},
                {"latest", QJsonObject {{"month", h.last().month.toString("yyyy-MM")}, {"rate", tidy(h.last().rate)}}},
                {"points", points}}, "points");
}

QJsonObject LocalTools::fxRate(const QJsonObject &a) const
{
    if (!m_s.fx || m_s.fx->dates().isEmpty()) return error("the exchange rates aren't loaded in Deepwater yet");
    const QString pair = a.value("pair").toString().remove('/').remove(' ').toUpper();
    const QVector<FxPair> &pairs = m_s.fx->pairs();
    int index = -1;
    bool inverted = false;
    for (int i = 0; i < pairs.size() && index < 0; ++i) {
        if (pairs[i].base + pairs[i].quote == pair) index = i;
        else if (pairs[i].quote + pairs[i].base == pair) { index = i; inverted = true; }
    }
    if (index < 0) return error("no pair " + pair + "; see list_data");
    const QVector<double> s = m_s.fx->series(index);
    const QVector<QDate> &dates = m_s.fx->dates();
    QJsonArray points;
    const int n = std::min<int>(int(dates.size()), int(s.size()));
    int wanted = lastArg(a, 30, 260);
    QVector<QJsonObject> rows;
    for (int d = n - 1; d >= 0 && wanted > 0; --d)
        if (std::isfinite(s[d]) && s[d] != 0) {
            rows.prepend({{"date", dates[d].toString(Qt::ISODate)}, {"rate", tidy(inverted ? 1.0 / s[d] : s[d])}});
            --wanted;
        }
    for (const QJsonObject &r : rows) points.append(r);
    QJsonObject result {{"pair", pair.left(3) + '/' + pair.mid(3)}};
    if (!rows.isEmpty()) result.insert("latest", rows.last());
    result.insert("points", points);
    return fit(result, "points");
}

QJsonObject LocalTools::cot(const QJsonObject &a) const
{
    if (!m_s.cot || !m_s.cot->fetchedAt().isValid()) return error("the COT report isn't loaded in Deepwater yet");
    const QString want = a.value("market").toString().trimmed();
    const CotReport::Market *market = nullptr;
    for (const CotReport::Market &m : m_s.cot->markets())
        if (m.id.compare(want, Qt::CaseInsensitive) == 0 || m.label.compare(want, Qt::CaseInsensitive) == 0) market = &m;
    if (!market) return error("unknown market \"" + want + "\"; see list_data");
    const CotReport::Series &s = m_s.cot->series(market->id);
    QJsonArray weeks;
    for (const CotReport::Week &w : s.weeks.mid(std::max<qsizetype>(0, s.weeks.size() - lastArg(a, 12, 104))))
        weeks.append(QJsonObject {{"date", w.date.toString(Qt::ISODate)}, {"open_interest", double(w.openInterest)},
                                  {"noncommercial_long", double(w.ncLong)}, {"noncommercial_short", double(w.ncShort)},
                                  {"noncommercial_net", double(w.ncNet())}, {"commercial_net", double(w.cNet())},
                                  {"nonreportable_net", double(w.nrNet())}});
    return fit({{"market", market->label}, {"contract", s.name}, {"units", s.units}, {"weeks", weeks}}, "weeks");
}

void LocalTools::prices(const QJsonObject &a, const Done &done)
{
    static const QRegularExpression symbolRe("^[A-Za-z0-9.^=\\-]{1,20}$");
    const QString symbol = a.value("symbol").toString().trimmed();
    const QString range = a.value("range").toString("1y");
    const QString interval = a.value("interval").toString("1d");
    if (!symbolRe.match(symbol).hasMatch()) return done(error("not a symbol: \"" + symbol + "\""));
    if (!QStringList {"1mo", "3mo", "6mo", "1y", "2y", "5y", "10y"}.contains(range)) return done(error("range: 1mo, 3mo, 6mo, 1y, 2y, 5y or 10y"));
    if (!QStringList {"1d", "1wk", "1mo"}.contains(interval)) return done(error("interval: 1d, 1wk or 1mo"));
    research::Client::instance()->candles(symbol, range, interval, this,
                                         [symbol, range, interval, done](const QVector<research::Bar> &bars, const QString &err) {
        QVector<research::Bar> ok;
        for (const research::Bar &b : bars)
            if (std::isfinite(b.close)) ok << b;
        if (!err.isEmpty() || ok.isEmpty()) {
            done(error("no prices for " + symbol + (err.isEmpty() ? QString() : ": " + err)));
            return;
        }
        double low = ok.first().close, high = low;
        for (const research::Bar &b : ok) {
            low = std::min(low, std::isfinite(b.low) ? b.low : b.close);
            high = std::max(high, std::isfinite(b.high) ? b.high : b.close);
        }
        const double first = ok.first().close, last = ok.last().close;
        QJsonArray rows;
        for (const research::Bar &b : ok.mid(std::max<qsizetype>(0, ok.size() - 40))) {
            QJsonObject r {{"date", b.time.toLocalTime().date().toString(Qt::ISODate)}, {"close", tidy(b.close)}};
            if (std::isfinite(b.volume) && b.volume > 0) r.insert("volume", tidy(b.volume));
            rows.append(r);
        }
        done(fit({{"symbol", symbol}, {"range", range}, {"interval", interval},
                  {"summary", QJsonObject {{"from", ok.first().time.toLocalTime().date().toString(Qt::ISODate)},
                                           {"to", ok.last().time.toLocalTime().date().toString(Qt::ISODate)},
                                           {"first_close", tidy(first)}, {"last_close", tidy(last)},
                                           {"change_pct", tidy((last / first - 1) * 100)},
                                           {"low", tidy(low)}, {"high", tidy(high)}, {"bars", int(ok.size())}}},
                  {"latest_bars", rows}}, "latest_bars"));
    });
}
