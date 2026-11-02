#include "NewsData.h"

#include "KeyedSources.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QSaveFile>
#include <QStandardPaths>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSet>
#include <QTextDocumentFragment>
#include <QTimeZone>
#include <QUrl>
#include <QUrlQuery>

#include <algorithm>
#include <memory>

namespace news {

namespace {

constexpr qint64 KeepSecs = 5 * 60;   // an answer is reused this long
constexpr int MaxItems = 60;

// The free feeds, per category. Nasdaq's feeds carry the tickers of each article; Seeking Alpha's its symbol.
const char *const GeneralFeeds[] {
    "https://search.cnbc.com/rs/search/combinedcms/view.xml?partnerId=wrss01&id=100003114",   // CNBC top news
    "https://feeds.content.dowjones.io/public/rss/mw_topstories",                             // MarketWatch
    "https://feeds.bbci.co.uk/news/business/rss.xml",                                         // BBC business
    "https://search.cnbc.com/rs/search/combinedcms/view.xml?partnerId=wrss01&id=10000664",    // CNBC finance
};
const char *const StockFeeds[] {
    "https://www.nasdaq.com/feed/rssoutbound?category=Stocks",
    "https://feeds.content.dowjones.io/public/rss/mw_marketpulse",
    "https://seekingalpha.com/market_currents.xml",
    "https://www.investing.com/rss/news_25.rss",
    "https://www.nasdaq.com/feed/rssoutbound?category=Markets",
};
const char *const CryptoFeeds[] {
    "https://cointelegraph.com/rss",
    "https://decrypt.co/feed",
    "https://www.coindesk.com/arc/outboundfeeds/rss/",
    "https://www.investing.com/rss/news_301.rss",
};
const char *const ForexFeeds[] {
    "https://www.fxstreet.com/rss/news",
    "https://www.investing.com/rss/news_1.rss",
};

QString fmpPath(Category category)
{
    switch (category) {
    case Category::General: return "general";
    case Category::Stocks: return "stock";
    case Category::Crypto: return "crypto";
    case Category::Forex: return "forex";
    }
    return "general";
}

// Entities decoded, tags removed, white space collapsed. A feed's body is often HTML escaped once more
// ("&lt;p&gt;S&amp;amp;P&lt;/p&gt;"), so it is read as HTML until nothing is left to decode (twice at most).
QString plainText(QString s)
{
    static const QRegularExpression cdata("^\\s*<!\\[CDATA\\[([\\s\\S]*?)\\]\\]>\\s*$");
    if (const auto m = cdata.match(s); m.hasMatch())
        s = m.captured(1);
    static const QRegularExpression entityOrTag("&[#a-zA-Z0-9]+;|<[a-zA-Z/!]");
    for (int pass = 0; pass < 2 && entityOrTag.match(s).hasMatch(); ++pass)
        s = QTextDocumentFragment::fromHtml(s).toPlainText();
    static const QRegularExpression spaces("[\\s\\p{Z}]+", QRegularExpression::UseUnicodePropertiesOption);
    s.replace(QChar(0x2028), ' ').replace(QChar::ParagraphSeparator, ' ').replace(QChar::Nbsp, ' ').replace(spaces, " ");
    s = s.trimmed();
    // The Motley Fool's articles (Nasdaq's feed) open on a "Key Points" heading.
    if (s.startsWith("Key Points") && s.size() > 10 && s.at(10).isUpper())
        s = s.mid(10).trimmed();
    return s;
}

// The text of the first <tag>…</tag> (or <tag …/>'s `attr`) of a block.
QString pick(const QString &block, const QString &tag, const QString &attr = QString())
{
    const QRegularExpression re(QStringLiteral("<%1(?=[\\s>])([^>]*)>([\\s\\S]*?)<\\/%1\\s*>").arg(QRegularExpression::escape(tag)));
    const auto m = re.match(block);
    if (!attr.isEmpty()) {
        const QRegularExpression self(QStringLiteral("<%1(?=[\\s>/])([^>]*?)/?>").arg(QRegularExpression::escape(tag)));
        auto all = self.globalMatch(block);
        const QRegularExpression value(QStringLiteral("\\b%1\\s*=\\s*\"([^\"]*)\"").arg(QRegularExpression::escape(attr)));
        // Atom: prefer the alternate link, else the first.
        QString first;
        while (all.hasNext()) {
            const QString attrs = all.next().captured(1);
            const auto v = value.match(attrs);
            if (!v.hasMatch()) continue;
            if (first.isEmpty()) first = v.captured(1);
            if (!attrs.contains("rel=\"") || attrs.contains("rel=\"alternate\""))
                return plainText(v.captured(1));
        }
        return plainText(first);
    }
    return m.hasMatch() ? plainText(m.captured(2)) : QString();
}

QDateTime parseDate(const QString &s)
{
    if (s.isEmpty())
        return {};
    // Qt's RFC 2822 reader wants a numeric offset: "GMT", "UT", "Z" and the US zones become one.
    static const QRegularExpression zone("\\s+(GMT|UTC?|Z|EST|EDT|CST|CDT|MST|MDT|PST|PDT)\\s*$");
    static const QHash<QString, QString> offsets {{"GMT", "+0000"}, {"UT", "+0000"}, {"UTC", "+0000"}, {"Z", "+0000"},
                                                 {"EST", "-0500"}, {"EDT", "-0400"}, {"CST", "-0600"}, {"CDT", "-0500"},
                                                 {"MST", "-0700"}, {"MDT", "-0600"}, {"PST", "-0800"}, {"PDT", "-0700"}};
    QString rfc = s;
    if (const auto m = zone.match(rfc); m.hasMatch())
        rfc = rfc.left(m.capturedStart()) + " " + offsets.value(m.captured(1), "+0000");
    QDateTime t = QDateTime::fromString(rfc, Qt::RFC2822Date);
    if (!t.isValid())
        t = QDateTime::fromString(s, Qt::ISODate);
    if (!t.isValid()) {
        // Investing.com: "2026-10-30 20:44:36", GMT.
        t = QDateTime::fromString(s, "yyyy-MM-dd HH:mm:ss");
        if (t.isValid()) t.setTimeZone(QTimeZone::utc());
    }
    return t.isValid() ? t.toUTC() : QDateTime();
}

QString hostOf(const QString &url)
{
    QString h = QUrl(url).host();
    if (h.startsWith("www.")) h.remove(0, 4);
    return h;
}

struct Coin {
    const char *symbol;
    const char *pattern;   // case-insensitive, whole words
};
// The large coins: the words that name them. Order matters only for ties (the first match in the text wins).
const Coin Coins[] {
    {"BTC", "bitcoin|\\bbtc\\b"},        {"ETH", "ethereum|\\beth\\b|\\bether\\b"}, {"XRP", "\\bxrp\\b|ripple"},
    {"SOL", "solana|\\bsol\\b"},         {"BNB", "\\bbnb\\b|binance coin"},      {"DOGE", "dogecoin|\\bdoge\\b"},
    {"ADA", "cardano|\\bada\\b"},        {"TRX", "\\btron\\b|\\btrx\\b"},        {"LINK", "chainlink|\\blink\\b"},
    {"USDT", "tether|\\busdt\\b"},       {"USDC", "\\busdc\\b"},                 {"HYPE", "hyperliquid|\\bhype\\b"},
    {"SHIB", "shiba inu|\\bshib\\b"},    {"AVAX", "avalanche|\\bavax\\b"},       {"DOT", "polkadot|\\bdot\\b"},
    {"XLM", "stellar|\\bxlm\\b"},        {"SUI", "\\bsui\\b"},                   {"ZEC", "zcash|\\bzec\\b"},
    {"LTC", "litecoin|\\bltc\\b"},       {"XMR", "monero|\\bxmr\\b"},            {"UNI", "uniswap|\\buni\\b"},
    {"AAVE", "\\baave\\b"},              {"PEPE", "\\bpepe\\b"},                 {"TON", "toncoin|\\bton\\b"},
    {"NEAR", "\\bnear protocol\\b"},     {"LUNC", "terra luna classic|\\blunc\\b"}, {"MARA", "\\bmara\\b"},
};

struct Currency {
    const char *code;
    const char *pattern;
};
const Currency Currencies[] {
    {"EUR", "\\beuro\\b|\\beur\\b"},         {"GBP", "\\bpound\\b|sterling|\\bcable\\b|\\bgbp\\b"},
    {"JPY", "\\byen\\b|\\bjpy\\b"},          {"CHF", "\\bfranc\\b|\\bswissie\\b|\\bchf\\b"},
    {"AUD", "\\baussie\\b|\\baud\\b"},       {"NZD", "\\bkiwi\\b|\\bnzd\\b"},
    {"CAD", "\\bloonie\\b|\\bcad\\b"},       {"CNY", "\\byuan\\b|renminbi|\\bcny\\b|\\bcnh\\b"},
    {"MXN", "\\bpeso\\b|\\bmxn\\b"},         {"TRY", "\\blira\\b|\\btry\\b"},
    {"INR", "\\brupee\\b|\\binr\\b"},        {"ZAR", "\\brand\\b|\\bzar\\b"},
    {"XAU", "\\bgold\\b|\\bxau\\b"},         {"XAG", "\\bsilver\\b|\\bxag\\b"},
};
// Pairs quoted as units of dollar per currency (EUR/USD); the others as currency per dollar (USD/JPY).
bool quotedAgainstDollar(const QString &code)
{
    return code == "EUR" || code == "GBP" || code == "AUD" || code == "NZD" || code == "XAU" || code == "XAG";
}

QString stockTicker(const QString &text)
{
    // "(NASDAQ: NFLX)", "NYSE:T", "Nasdaq: AAPL", "(TSX: SHOP)".
    static const QRegularExpression listed("\\b(?:NASDAQ|NYSE(?: American)?|AMEX|TSX|TSXV|LSE|OTCQB|OTCQX|OTC)\\s*:\\s*([A-Z]{1,5}(?:\\.[A-Z])?)\\b",
                                           QRegularExpression::CaseInsensitiveOption);
    if (const auto m = listed.match(text); m.hasMatch())
        return m.captured(1).toUpper();
    // "Netflix (NFLX)", "$NVDA".
    static const QRegularExpression bracket("\\(([A-Z]{2,5})\\)|\\$([A-Z]{2,5})\\b");
    if (const auto m = bracket.match(text); m.hasMatch()) {
        const QString t = m.captured(1).isEmpty() ? m.captured(2) : m.captured(1);
        static const QSet<QString> notTickers {"CEO", "CFO", "USD", "GDP", "ETF", "IPO", "AI", "US", "UK", "EU", "FED", "SEC", "PPI", "CPI", "NYSE"};
        if (!notTickers.contains(t)) return t;
    }
    return {};
}

} // namespace

QString Wire::symbolFromFmp(const QString &symbol)
{
    return symbol.trimmed().toUpper();
}

QString Wire::guessSymbol(const QString &text, Category category)
{
    switch (category) {
    case Category::Stocks:
        return stockTicker(text);
    case Category::Crypto: {
        int best = -1;
        QString symbol;
        for (const Coin &c : Coins) {
            const QRegularExpression re(QString::fromLatin1(c.pattern), QRegularExpression::CaseInsensitiveOption);
            const auto m = re.match(text);
            if (m.hasMatch() && (best < 0 || m.capturedStart() < best)) {
                best = m.capturedStart();
                symbol = QString::fromLatin1(c.symbol);
            }
        }
        if (symbol == "MARA") return "MARA";
        return symbol.isEmpty() ? QString() : symbol + "USD";
    }
    case Category::Forex: {
        static const QRegularExpression pair("\\b([A-Z]{3})\\s*/\\s*([A-Z]{3})\\b");
        if (const auto m = pair.match(text); m.hasMatch())
            return m.captured(1) + m.captured(2);
        int best = -1;
        QString code;
        for (const Currency &c : Currencies) {
            const QRegularExpression re(QString::fromLatin1(c.pattern), QRegularExpression::CaseInsensitiveOption);
            const auto m = re.match(text);
            if (m.hasMatch() && (best < 0 || m.capturedStart() < best)) {
                best = m.capturedStart();
                code = QString::fromLatin1(c.code);
            }
        }
        if (code.isEmpty()) {
            static const QRegularExpression dollar("\\bdollar\\b|\\bgreenback\\b|\\bdxy\\b", QRegularExpression::CaseInsensitiveOption);
            return dollar.match(text).hasMatch() ? QStringLiteral("DXY") : QString();
        }
        return quotedAgainstDollar(code) ? code + "USD" : "USD" + code;
    }
    case Category::General:
        return stockTicker(text);
    }
    return {};
}

QVector<Item> Wire::parseFeed(const QByteArray &xml, Category category, const QString &prefer)
{
    QVector<Item> out;
    const QString doc = QString::fromUtf8(xml);
    static const QRegularExpression entry("<(item|entry)(?=[\\s>])[^>]*>([\\s\\S]*?)<\\/\\1\\s*>");
    auto it = entry.globalMatch(doc);
    while (it.hasNext()) {
        const auto m = it.next();
        const QString block = m.captured(2);
        Item item;
        item.title = pick(block, "title");
        if (item.title.isEmpty()) continue;
        item.url = pick(block, "link");
        if (item.url.isEmpty() || !item.url.startsWith("http")) item.url = pick(block, "link", "href");
        if (item.url.isEmpty()) {
            const QString guid = pick(block, "guid");
            if (guid.startsWith("http")) item.url = guid;
        }
        item.summary = pick(block, "description");
        if (item.summary.isEmpty()) item.summary = pick(block, "summary");
        if (item.summary.isEmpty()) item.summary = pick(block, "content:encoded");
        if (item.summary.isEmpty()) item.summary = pick(block, "content");
        if (item.summary.compare(item.title, Qt::CaseInsensitive) == 0) item.summary.clear();
        if (item.summary.size() > 400) item.summary = item.summary.left(397).trimmed() + "…";
        QString date = pick(block, "pubDate");
        if (date.isEmpty()) date = pick(block, "published");
        if (date.isEmpty()) date = pick(block, "updated");
        if (date.isEmpty()) date = pick(block, "dc:date");
        item.published = parseDate(date);
        item.publisher = pick(block, "dc:creator");
        if (item.publisher.isEmpty()) item.publisher = pick(block, "author");
        item.site = hostOf(item.url);
        // The ticker: Nasdaq lists them, Seeking Alpha names the symbol's page, else the text.
        const QString tickers = pick(block, "nasdaq:tickers");
        if (!tickers.isEmpty()) {
            const QStringList list = tickers.toUpper().split(',', Qt::SkipEmptyParts);
            item.symbol = !prefer.isEmpty() && list.contains(prefer) ? prefer : list.first().trimmed();
            if (category == Category::Crypto && !item.symbol.endsWith("USD") && item.symbol.size() <= 5) item.symbol += "USD";
        } else {
            static const QRegularExpression saSymbol("<category[^>]*domain=\"https?://seekingalpha\\.com/symbol/([A-Za-z.\\-]+)\"");
            if (const auto s = saSymbol.match(block); s.hasMatch())
                item.symbol = s.captured(1).toUpper();
        }
        if (item.symbol.isEmpty())
            item.symbol = guessSymbol(item.title + " " + item.summary, category);
        if (item.symbol.isEmpty())
            item.symbol = prefer;   // a feed about one ticker
        out.push_back(item);
    }
    return out;
}

QStringList Wire::feedUrls(Category category)
{
    QStringList out;
    auto add = [&](const auto &list) { for (const char *u : list) out << QString::fromLatin1(u); };
    switch (category) {
    case Category::General: add(GeneralFeeds); break;
    case Category::Stocks: add(StockFeeds); break;
    case Category::Crypto: add(CryptoFeeds); break;
    case Category::Forex: add(ForexFeeds); break;
    }
    return out;
}

Wire *Wire::instance()
{
    static Wire *w = new Wire;
    return w;
}

Wire::Wire(QObject *parent)
    : QObject(parent)
{
    m_net.setTransferTimeout(15000);
}

bool Wire::usesFmp() const
{
    return isEnabled("fmp") && !sourceKey("fmp").isEmpty();
}

void Wire::setEnabled(const QString &provider, bool on)
{
    const bool was = isEnabled(provider);
    m_disabled.removeAll(provider);
    if (!on) m_disabled << provider;
    if (was != on) m_cache.clear();
}

void Wire::forget()
{
    m_cache.clear();
}

void Wire::fetch(Category category, const QString &ticker, bool force, QObject *ctx, Done done)
{
    const QString symbol = ticker.trimmed().toUpper();
    const bool fmp = usesFmp();
    const QString key = QString("%1|%2|%3").arg(fmp ? "fmp" : "rss").arg(int(category)).arg(symbol);
    const auto cached = m_cache.constFind(key);
    if (!force && cached != m_cache.constEnd() && cached->at.secsTo(QDateTime::currentDateTimeUtc()) < KeepSecs) {
        done(cached->items, cached->provider, cached->error);
        return;
    }
    Waiter w {ctx, std::move(done)};
    const bool inFlight = m_waiting.contains(key);
    m_waiting[key].push_back(std::move(w));
    if (inFlight)
        return;
    auto deliverHere = [this, key](const QVector<Item> &items, const QString &provider, const QString &error) {
        deliver(key, items, provider, error);
    };
    if (fmp)
        fetchFmp(category, symbol, sourceKey("fmp"), deliverHere);
    else if (!isEnabled("rss"))
        deliver(key, {}, "rss", tr("The news feeds are turned off in Sources"));
    else
        fetchRss(category, symbol, QString(), deliverHere);
}

void Wire::deliver(const QString &key, const QVector<Item> &items, const QString &provider, const QString &error)
{
    m_cache.insert(key, {items, provider, error, QDateTime::currentDateTimeUtc()});
    const QVector<Waiter> waiters = m_waiting.take(key);
    for (const Waiter &w : waiters)
        if (w.ctx) w.done(items, provider, error);
}

void Wire::fetchFmp(Category category, const QString &ticker, const QString &apiKey, Done done)
{
    QUrl url;
    QUrlQuery q;
    if (!ticker.isEmpty() && category != Category::General) {
        url = QUrl("https://financialmodelingprep.com/stable/news/" + fmpPath(category));
        q.addQueryItem("symbols", ticker);
    } else {
        url = QUrl("https://financialmodelingprep.com/stable/news/" + fmpPath(category) + "-latest");
        q.addQueryItem("page", "0");
    }
    q.addQueryItem("limit", QString::number(MaxItems));
    q.addQueryItem("apikey", apiKey);
    url.setQuery(q);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Deepwater/0.1 (desktop app; news page)"));
    m_states["fmp"].pending++;
    QNetworkReply *reply = m_net.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, category, ticker, done] {
        reply->deleteLater();
        State &s = m_states["fmp"];
        s.pending--;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QString error;
        if (reply->error() != QNetworkReply::NoError) {
            error = status == 401 || status == 403 ? tr("Financial Modeling Prep refused the key (HTTP %1)").arg(status)
                    : status == 402              ? tr("This endpoint isn't in your Financial Modeling Prep plan (HTTP 402)")
                    : status                     ? tr("Financial Modeling Prep: HTTP %1").arg(status)
                                                 : reply->errorString();
        }
        QVector<Item> items;
        if (error.isEmpty()) {
            const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            if (!doc.isArray()) {
                const QString msg = doc.object().value("Error Message").toString();
                error = msg.isEmpty() ? tr("No usable data in Financial Modeling Prep's answer") : msg;
            }
            for (const QJsonValue &v : doc.array()) {
                const QJsonObject o = v.toObject();
                Item item;
                item.title = o.value("title").toString().trimmed();
                if (item.title.isEmpty()) continue;
                item.summary = plainText(o.value("text").toString());
                item.url = o.value("url").toString();
                item.site = o.value("site").toString();
                if (item.site.isEmpty()) item.site = hostOf(item.url);
                item.publisher = o.value("publisher").toString();
                item.symbol = symbolFromFmp(o.value("symbol").toString());
                // "2026-02-27 14:45:00": UTC.
                QDateTime t = QDateTime::fromString(o.value("publishedDate").toString(), "yyyy-MM-dd HH:mm:ss");
                if (t.isValid()) t.setTimeZone(QTimeZone::utc());
                else t = parseDate(o.value("publishedDate").toString());
                item.published = t;
                items.push_back(item);
            }
            if (!ticker.isEmpty() && category == Category::General) {
                QVector<Item> kept;
                for (const Item &i : items)
                    if (i.symbol == ticker || i.title.contains(ticker, Qt::CaseInsensitive) || i.summary.contains(ticker, Qt::CaseInsensitive))
                        kept.push_back(i);
                items = kept;
            }
        }
        const QDateTime now = QDateTime::currentDateTimeUtc();
        if (error.isEmpty()) {
            s.lastOk = now;
            done(items, "fmp", QString());
            return;
        }
        s.lastError = now;
        s.error = error;
        // The free feeds take over, with the reason shown.
        if (isEnabled("rss"))
            fetchRss(category, ticker, error, done);
        else
            done({}, "fmp", error);
    });
}

void Wire::fetchRss(Category category, const QString &ticker, const QString &note, Done done)
{
    QStringList urls = feedUrls(category);
    if (!ticker.isEmpty() && category == Category::Stocks) {
        // The feeds about one stock.
        urls = {QStringLiteral("https://feeds.finance.yahoo.com/rss/2.0/headline?s=%1&region=US&lang=en-US").arg(ticker),
                QStringLiteral("https://www.nasdaq.com/feed/rssoutbound?symbol=%1").arg(ticker.toLower())};
    }
    struct Pending {
        int left = 0;
        QVector<Item> items;
        QStringList errors;
    };
    auto state = std::make_shared<Pending>();
    state->left = urls.size();
    m_states["rss"].pending++;
    for (const QString &u : urls) {
        QNetworkRequest req{QUrl(u)};
        req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Mozilla/5.0 (Macintosh) Deepwater/0.1"));
        req.setRawHeader("Accept", "application/rss+xml, application/atom+xml, application/xml, text/xml, */*");
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        QNetworkReply *reply = m_net.get(req);
        connect(reply, &QNetworkReply::finished, this, [this, reply, state, category, ticker, note, done, u] {
            reply->deleteLater();
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (reply->error() != QNetworkReply::NoError) {
                state->errors << QStringLiteral("%1: %2").arg(hostOf(u), status ? QStringLiteral("HTTP %1").arg(status) : reply->errorString());
            } else {
                const QVector<Item> items = parseFeed(reply->readAll(), category, category == Category::Stocks ? ticker : QString());
                if (items.isEmpty()) state->errors << tr("%1: no article in the feed").arg(hostOf(u));
                state->items += items;
            }
            if (--state->left > 0)
                return;
            State &s = m_states["rss"];
            s.pending--;
            const QDateTime now = QDateTime::currentDateTimeUtc();
            // Merge: one copy per article, newest first; a ticker keeps the articles about it.
            QVector<Item> items;
            QSet<QString> seen;
            for (const Item &i : state->items) {
                QUrl url(i.url);
                url.setQuery(QString());
                const QString id = i.url.isEmpty() ? i.title.toLower() : url.toString();
                if (seen.contains(id)) continue;
                seen.insert(id);
                if (!ticker.isEmpty() && category != Category::Stocks) {
                    const QRegularExpression word("(^|[^A-Za-z])" + QRegularExpression::escape(ticker) + "([^A-Za-z]|$)",
                                                  QRegularExpression::CaseInsensitiveOption);
                    const QString base = ticker.endsWith("USD") && ticker.size() > 3 ? ticker.chopped(3) : ticker;
                    const QRegularExpression baseWord("(^|[^A-Za-z])" + QRegularExpression::escape(base) + "([^A-Za-z]|$)",
                                                      QRegularExpression::CaseInsensitiveOption);
                    const QString text = i.title + " " + i.summary;
                    if (i.symbol != ticker && !word.match(text).hasMatch() && !baseWord.match(text).hasMatch()
                        && !(category == Category::Crypto && guessSymbol(text, category) == ticker))
                        continue;
                }
                items.push_back(i);
            }
            std::stable_sort(items.begin(), items.end(), [](const Item &a, const Item &b) {
                if (a.published.isValid() != b.published.isValid()) return a.published.isValid();
                return a.published > b.published;
            });
            if (items.size() > MaxItems) items.resize(MaxItems);
            QString error = note;
            if (items.isEmpty()) {
                s.lastError = now;
                s.error = state->errors.isEmpty() ? tr("No article") : state->errors.join(" · ");
                error = error.isEmpty() ? s.error : error + " · " + s.error;
            } else {
                s.lastOk = now;
                if (!state->errors.isEmpty()) {
                    s.lastError = now;
                    s.error = state->errors.join(" · ");
                }
            }
            done(items, "rss", error);
        });
    }
}

// ---------------------------------------------------------------- Favorites

namespace {

QString favoritesPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/news-favorites.json";
}

QJsonObject toJson(const Item &i)
{
    return {{"title", i.title},   {"summary", i.summary},     {"url", i.url},
            {"site", i.site},     {"symbol", i.symbol},       {"publisher", i.publisher},
            {"published", i.published.isValid() ? i.published.toUTC().toString(Qt::ISODate) : QString()}};
}

Item fromJson(const QJsonObject &o)
{
    Item i;
    i.title = o.value("title").toString();
    i.summary = o.value("summary").toString();
    i.url = o.value("url").toString();
    i.site = o.value("site").toString();
    i.symbol = o.value("symbol").toString();
    i.publisher = o.value("publisher").toString();
    const QString t = o.value("published").toString();
    if (!t.isEmpty()) i.published = QDateTime::fromString(t, Qt::ISODate).toUTC();
    return i;
}

} // namespace

Favorites *Favorites::instance()
{
    static Favorites *f = new Favorites;
    return f;
}

Favorites::Favorites(QObject *parent)
    : QObject(parent)
{
    load();
}

void Favorites::load()
{
    QFile file(favoritesPath());
    if (!file.open(QIODevice::ReadOnly))
        return;
    m_entries.clear();
    for (const QJsonValue &v : QJsonDocument::fromJson(file.readAll()).array()) {
        const QJsonObject o = v.toObject();
        Entry e {fromJson(o), QDateTime::fromString(o.value("savedAt").toString(), Qt::ISODate).toUTC()};
        if (!e.item.url.isEmpty()) m_entries.push_back(e);
    }
    std::stable_sort(m_entries.begin(), m_entries.end(), [](const Entry &a, const Entry &b) { return a.savedAt > b.savedAt; });
}

void Favorites::save() const
{
    QJsonArray array;
    for (const Entry &e : m_entries) {
        QJsonObject o = toJson(e.item);
        o.insert("savedAt", e.savedAt.toString(Qt::ISODate));
        array.append(o);
    }
    QDir().mkpath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    QSaveFile file(favoritesPath());
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(QJsonDocument(array).toJson(QJsonDocument::Indented));
    file.commit();
}

bool Favorites::contains(const QString &url) const
{
    for (const Entry &e : m_entries)
        if (e.item.url == url) return true;
    return false;
}

void Favorites::toggle(const Item &item)
{
    if (item.url.isEmpty())
        return;
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].item.url == item.url) {
            m_entries.remove(i);
            save();
            emit changed();
            return;
        }
    }
    m_entries.prepend({item, QDateTime::currentDateTimeUtc()});
    save();
    emit changed();
}

QVector<Item> Favorites::items() const
{
    QVector<Item> out;
    out.reserve(m_entries.size());
    for (const Entry &e : m_entries) out.push_back(e.item);
    return out;
}

QString ago(const QDateTime &t, const QDateTime &now)
{
    if (!t.isValid())
        return {};
    const qint64 secs = std::max<qint64>(0, t.secsTo(now));
    if (secs < 60) return Wire::tr("now");
    if (secs < 3600) return Wire::tr("%1m ago").arg(secs / 60);
    if (secs < 86400) return Wire::tr("%1h ago").arg(secs / 3600);
    return Wire::tr("%1d ago").arg(secs / 86400);
}

} // namespace news
