#include "MarketInfo.h"

#include "../core/Error.h"
#include "Async.h"
#include "Http.h"
#include "Json.h"
#include "Providers.h"

#include <QRegularExpression>

#include <algorithm>
#include <cmath>

namespace qtm {

MarketInfo::MarketInfo(Providers &data) : m_data(data) {}

static QJsonValue num(const QJsonValue &s)
{
    QString t = jsv::nullish(s) ? QString() : jsv::toString(s);
    static const QRegularExpression re(QStringLiteral("[$,%" QTM_JS_SPACE "]"));
    t.replace(re, QString());
    const double v = jsv::parseFloat(t);
    return std::isfinite(v) ? QJsonValue(v) : QJsonValue(QJsonValue::Null);
}

static QString ymd(double ms)
{
    return jsv::isoString(ms).left(10);
}

// The next n weekdays starting today (UTC).
static QStringList weekdays(double n)
{
    QStringList out;
    double d = jsv::nowMs();
    while (out.size() < n) {
        const int wd = jsv::utcDay(d);
        if (wd != 0 && wd != 6) out << ymd(d);
        d += 86400e3;
    }
    return out;
}

static void put(QJsonObject &o, const char *key, const QJsonValue &v)
{
    if (!v.isUndefined()) o.insert(key, v);
}

QJsonArray MarketInfo::earnings(double days)
{
    if (m_data.offline()) return {};
    const QStringList dates = weekdays(std::min(10.0, std::max(1.0, days)));
    const auto perDay = async::all(int(dates.size()), [&](int i) -> QJsonArray {
        const QString date = dates[i];
        try {
            return m_data.cached<QJsonArray>("earnings:" + date, 30 * 60e3, [this, date]() -> QJsonArray {
                const QJsonValue j = m_data.getJson("https://api.nasdaq.com/api/calendar/earnings?date=" + date, Providers::NQ());
                QJsonArray out;
                for (const QJsonValue r : jsv::orElse(j["data"]["rows"], QJsonArray()).toArray()) {
                    const QString time = jsv::toString(r["time"]);
                    QJsonObject o {
                        { "date", date },
                        { "symbol", "US:" + jsv::toString(r["symbol"]) },
                        { "when", time == "time-pre-market"     ? "Before the open"
                                  : time == "time-after-hours" ? "After the close"
                                                               : "Time not given" },
                        { "marketCap", num(r["marketCap"]) },
                        { "epsForecast", jsv::orElse(r["epsForecast"], "") },
                        { "quarter", jsv::orElse(r["fiscalQuarterEnding"], "") },
                        { "lastYearEps", jsv::orElse(r["lastYearEPS"], "") },
                    };
                    put(o, "ticker", r["symbol"]);
                    put(o, "name", r["name"]);
                    out.append(o);
                }
                return out;
            });
        } catch (...) {
            return {};
        }
    });
    QJsonArray out;
    for (const auto &day : perDay)
        for (const QJsonValue r : day) out.append(r);
    return out;
}

QJsonArray MarketInfo::dividends(double days)
{
    if (m_data.offline()) return {};
    const QStringList dates = weekdays(std::min(10.0, std::max(1.0, days)));
    const auto perDay = async::all(int(dates.size()), [&](int i) -> QJsonArray {
        const QString date = dates[i];
        try {
            return m_data.cached<QJsonArray>("dividends:" + date, 30 * 60e3, [this, date]() -> QJsonArray {
                const QJsonValue j = m_data.getJson("https://api.nasdaq.com/api/calendar/dividends?date=" + date, Providers::NQ());
                QJsonArray out;
                for (const QJsonValue r : jsv::orElse(j["data"]["calendar"]["rows"], QJsonArray()).toArray()) {
                    QJsonObject o { { "date", date }, { "symbol", "US:" + jsv::toString(r["symbol"]) } };
                    put(o, "ticker", r["symbol"]);
                    put(o, "name", r["companyName"]);
                    put(o, "exDate", r["dividend_Ex_Date"]);
                    put(o, "payDate", r["payment_Date"]);
                    put(o, "recordDate", r["record_Date"]);
                    put(o, "amount", r["dividend_Rate"]);
                    put(o, "annual", r["indicated_Annual_Dividend"]);
                    out.append(o);
                }
                return out;
            });
        } catch (...) {
            return {};
        }
    });
    QJsonArray out;
    for (const auto &day : perDay)
        for (const auto &r : day) out.append(r);
    return out;
}

// ---------- news ----------
static QString decode(QString s)
{
    static const QRegularExpression cdata(QStringLiteral("<!\\[CDATA\\[|\\]\\]>")), apos(QStringLiteral("&#0?39;|&apos;")),
        tags(QStringLiteral("<[^>]+>"));
    s.replace(cdata, QString());
    s.replace("&amp;", "&");
    s.replace("&lt;", "<");
    s.replace("&gt;", ">");
    s.replace("&quot;", "\"");
    s.replace(apos, "'");
    s.replace(tags, QString());
    return jsv::trim(s);
}

QJsonArray MarketInfo::parseRss(const QString &xml, const QString &source)
{
    QJsonArray items;
    static const QRegularExpression item(QStringLiteral("<item>([\\s\\S]*?)<\\/item>"));
    auto it = item.globalMatch(xml);
    while (it.hasNext()) {
        const QString block = it.next().captured(1);
        auto pick = [&](const QString &tag) {
            const QRegularExpression re(QStringLiteral("<%1[^>]*>([\\s\\S]*?)<\\/%1>").arg(tag));
            const auto m = re.match(block);
            return decode(m.hasMatch() ? m.captured(1) : QString());
        };
        const QString title = pick("title");
        if (title.isEmpty()) continue;
        const double t = jsv::dateParse(pick("pubDate"));
        items.append(QJsonObject {
            { "title", title },
            { "url", pick("link") },
            { "source", source },
            { "time", (t != 0 && !std::isnan(t)) ? QJsonValue(t) : QJsonValue(QJsonValue::Null) },
            { "summary", pick("description").left(280) },
        });
    }
    return items;
}

QJsonArray MarketInfo::cryptoNews()
{
    if (m_data.isDisabled("cryptonews")) return {};   // turned off in deepwater's Sources page
    return m_data.cached<QJsonArray>("news:crypto", 10 * 60e3, []() -> QJsonArray {
        const QVector<QPair<QString, QString>> feeds = {
            { "https://cointelegraph.com/rss", "Cointelegraph" },
            { "https://decrypt.co/feed", "Decrypt" },
        };
        const auto lists = async::all(int(feeds.size()), [&](int i) -> QJsonArray {
            try {
                http::FetchOptions o;
                o.headers << qMakePair(QByteArray("User-Agent"), QByteArray("Mozilla/5.0 QuantTerminal"));
                o.timeoutMs = 10000;
                const auto res = http::fetch(feeds[i].first, o);
                return res.ok() ? parseRss(res.text(), feeds[i].second) : QJsonArray();
            } catch (...) {
                return {};
            }
        });
        QVector<QJsonValue> all;
        for (const auto &l : lists)
            for (const auto &x : l) all << x;
        auto timeOf = [](const QJsonValue &v) {
            const double t = v["time"].toDouble();
            return std::isnan(t) ? 0.0 : t;
        };
        std::stable_sort(all.begin(), all.end(), [&](const QJsonValue &a, const QJsonValue &b) { return timeOf(b) < timeOf(a); });
        QJsonArray out;
        for (const auto &x : all) out.append(x);
        return out;
    });
}

static QJsonArray first(const QJsonArray &a, int n)
{
    QJsonArray out;
    for (int i = 0; i < a.size() && i < n; ++i) out.append(a[i]);
    return out;
}

QJsonObject MarketInfo::news(const QString &symbol)
{
    if (m_data.offline()) return { { "items", QJsonArray() }, { "note", "" } };
    const Resolved sym = m_data.resolve(symbol);
    if (QStringList { "BINANCE", "COINBASE", "KRAKEN" }.contains(sym.exchange)) {
        QString base = sym.ticker;
        base.remove('-');
        base.replace(QRegularExpression(QStringLiteral("(USDT|USDC|FDUSD|USD|BTC|ETH|EUR)$")), QString());
        base.replace(QRegularExpression(QStringLiteral("^XBT$")), "BTC");
        if (base.isEmpty()) base = "BTC";
        const QJsonArray all = cryptoNews();
        static const QHash<QString, QStringList> COIN_NAMES = {
            { "BTC", { "bitcoin", "btc" } }, { "ETH", { "ethereum", "ether", "eth" } }, { "SOL", { "solana", "sol" } },
            { "XRP", { "xrp", "ripple" } }, { "BNB", { "bnb", "binance coin" } }, { "DOGE", { "dogecoin", "doge" } },
            { "ADA", { "cardano", "ada" } }, { "AVAX", { "avalanche", "avax" } }, { "LINK", { "chainlink", "link" } },
            { "DOT", { "polkadot" } }, { "LTC", { "litecoin", "ltc" } }, { "TRX", { "tron", "trx" } },
            { "TON", { "toncoin", "ton" } }, { "SHIB", { "shiba inu", "shib" } }, { "PEPE", { "pepe" } },
            { "SUI", { "sui" } }, { "NEAR", { "near protocol", "near" } }, { "ZEC", { "zcash", "zec" } },
        };
        const QStringList words = COIN_NAMES.value(base, { base.toLower() });
        QStringList escaped;
        for (const auto &w : words) escaped << QRegularExpression::escape(w);
        const QRegularExpression re(QStringLiteral("\\b(%1)\\b").arg(escaped.join('|')), QRegularExpression::CaseInsensitiveOption);
        QJsonArray hits;
        for (const QJsonValue n : all)
            if (re.match(jsv::toString(n["title"]) + ' ' + jsv::toString(n["summary"])).hasMatch()) hits.append(n);
        if (hits.size() >= 3) return { { "items", first(hits, 30) }, { "note", "Headlines mentioning " + base } };
        return { { "items", first(all, 30) },
                 { "note", QStringLiteral("Few headlines mention %1 right now, so these are the latest crypto headlines").arg(base) } };
    }
    if (sym.exchange == "US") {
        const QString ticker = sym.ticker;
        const QJsonArray items = m_data.cached<QJsonArray>("news:" + ticker, 10 * 60e3, [this, ticker]() -> QJsonArray {
            for (const char *cls : { "stocks", "etf", "index" }) {
                QJsonValue j;
                try {
                    j = m_data.getJson(QStringLiteral("https://api.nasdaq.com/api/news/topic/articlebysymbol?q=%1&offset=0&limit=30&fallback=true")
                                           .arg(jsv::encodeURIComponent(ticker.toLower() + '|' + cls)),
                                       Providers::NQ());
                } catch (...) {
                    j = QJsonValue();
                }
                const QJsonArray rows = jsv::orElse(j["data"]["rows"], QJsonArray()).toArray();
                if (rows.isEmpty()) continue;
                QJsonArray out;
                for (const QJsonValue r : rows) {
                    const QJsonValue u = r["url"];
                    const QString url = u.isString() && u.toString().startsWith("http") ? u.toString()
                                                                                        : "https://www.nasdaq.com" + jsv::toString(u);
                    const double t = jsv::nullish(r["created"]) ? NAN : jsv::dateParse(jsv::toString(r["created"]));
                    QJsonObject o {
                        { "url", url },
                        { "time", (t != 0 && !std::isnan(t)) ? QJsonValue(t) : QJsonValue(QJsonValue::Null) },
                        { "summary", jsv::orElse(r["description"], "") },
                    };
                    put(o, "title", r["title"]);
                    put(o, "source", r["publisher"]);
                    put(o, "ago", r["ago"]);
                    out.append(o);
                }
                return out;
            }
            return {};
        });
        return { { "items", items },
                 { "note", items.isEmpty() ? "No headlines found for " + ticker : QStringLiteral("Headlines for %1 from Nasdaq").arg(ticker) } };
    }
    return { { "items", QJsonArray() }, { "note", "Headlines are available for crypto and US stocks." } };
}

// ---------- fundamentals ----------
static QJsonValue table(const QJsonValue &t, const QByteArray &raw, const QString &name)
{
    if (!jsv::truthy(t["headers"]) || !jsv::truthy(t["rows"])) return QJsonValue(QJsonValue::Null);
    QStringList keys = jsv::keyOrder(raw, { "data", name, "headers" });
    if (keys.isEmpty()) keys = t["headers"].toObject().keys();
    keys.removeAll("value1");
    QJsonArray periods;
    for (const auto &k : keys) periods.append(t["headers"][k]);
    QJsonArray rows;
    for (const QJsonValue r : t["rows"].toArray()) {
        QJsonArray values;
        for (const auto &k : keys) values.append(jsv::coalesce(r[k], ""));
        QJsonObject o { { "values", values } };
        put(o, "label", r["value1"]);
        rows.append(o);
    }
    return QJsonObject { { "periods", periods }, { "rows", rows } };
}

QJsonValue MarketInfo::financials(const QString &symbol, const QString &frequencyIn, bool hasFrequency)
{
    if (m_data.offline()) return QJsonValue(QJsonValue::Null);
    const QString frequency = hasFrequency ? frequencyIn : QStringLiteral("annual");
    const Resolved sym = m_data.resolve(symbol);
    if (sym.exchange != "US") throw Error("Financial statements are for US-listed stocks.", 422);
    const int f = frequency == "quarterly" ? 2 : 1;
    return m_data.cached<QJsonValue>(QStringLiteral("fin:%1:%2").arg(sym.ticker).arg(f), 6 * 3600e3, [this, sym, f, frequency]() -> QJsonValue {
        QByteArray finRaw, sumRaw;
        const auto got = async::all(3, [&](int i) -> QJsonValue {
            try {
                if (i == 0)
                    return m_data.getJson(QStringLiteral("https://api.nasdaq.com/api/company/%1/financials?frequency=%2").arg(sym.ticker).arg(f),
                                          Providers::NQ(), &finRaw);
                if (i == 1)
                    return m_data.getJson(QStringLiteral("https://api.nasdaq.com/api/quote/%1/summary?assetclass=stocks").arg(sym.ticker),
                                          Providers::NQ(), &sumRaw);
                return m_data.getJson(QStringLiteral("https://api.nasdaq.com/api/analyst/%1/earnings-date").arg(sym.ticker), Providers::NQ());
            } catch (...) {
                return QJsonValue(QJsonValue::Null);
            }
        });
        const QJsonValue fin = got[0], sum = got[1], ed = got[2];
        const QJsonValue d = jsv::orElse(fin["data"], QJsonObject());
        QJsonArray stats;
        const QJsonObject summary = sum["data"]["summaryData"].toObject();
        QStringList order = jsv::keyOrder(sumRaw, { "data", "summaryData" });
        if (order.isEmpty()) order = summary.keys();
        for (const auto &k : order) {
            const QJsonValue x = summary.value(k);
            if (!jsv::truthy(x["value"]) || x["value"] == QJsonValue("N/A")) continue;
            QJsonObject o;
            put(o, "label", x["label"]);
            put(o, "value", x["value"]);
            stats.append(o);
        }
        QString next = jsv::toString(jsv::orElse(ed["data"]["reportText"], ""));
        next.replace(QRegularExpression(QStringLiteral("[" QTM_JS_SPACE "]+")), " ");
        const QJsonValue income = table(d["incomeStatementTable"], finRaw, "incomeStatementTable");
        QJsonObject out {
            { "symbol", sym.id },
            { "frequency", frequency },
            { "stats", stats },
            { "nextEarnings", jsv::trim(next) },
            { "income", income },
            { "balance", table(d["balanceSheetTable"], finRaw, "balanceSheetTable") },
            { "cashflow", table(d["cashFlowTable"], finRaw, "cashFlowTable") },
            { "ratios", table(d["financialRatiosTable"], finRaw, "financialRatiosTable") },
            { "note", "Statement figures are in thousands of US dollars, as Nasdaq reports them." },
        };
        if (income.isNull() && stats.isEmpty())
            throw Error(QStringLiteral("Nasdaq has no financials for %1 (ETFs and funds don't file them).").arg(sym.ticker), 404);
        return out;
    });
}

} // namespace qtm
