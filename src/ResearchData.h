#pragma once

#include <QDate>
#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QQueue>
#include <QSet>
#include <QVector>

#include <cmath>
#include <functional>

// The Research page's data: one company at a time (profile, statements, holders, analysts) plus the market
// around it (index members, sectors, industries). Everything is downloaded on demand from free endpoints
// without a key and kept on disk (HttpCache, "research_*") for as long as it stays meaningful:
//
// - Yahoo Finance: quoteSummary (profile, key statistics, analyst ratings and targets, holders, insiders),
//   fundamentals-timeseries (the income statement, balance sheet and cash flow as Yahoo lays them out),
//   chart / spark (prices), search, recommendationsbysymbol (peers). quoteSummary and quote want a session
//   cookie and its "crumb", fetched once per session.
// - Nasdaq (api.nasdaq.com): institutional holders (13F), insider trades (Form 4), the stock screener (every
//   US listing with its sector and industry), the Nasdaq-100 members.
// - SEC EDGAR (data.sec.gov): XBRL company facts, the long history of the reported figures (10-K, 10-Q, 20-F);
//   efts.sec.gov gives a ticker's CIK.
// - Wikipedia: the S&P 500 members and their GICS sectors, the Dow Jones members.
// - Wikidata: a company's head count over the years.

namespace research {

constexpr double NaN = std::numeric_limits<double>::quiet_NaN();
inline bool ok(double v) { return std::isfinite(v); }

// A Yahoo value: {"raw": 1.2, "fmt": "1.20"}, or a plain number; NaN when missing.
double raw(const QJsonObject &o, const char *key);
QString text(const QJsonObject &o, const char *key);

struct Point {
    QDate date;
    double value = NaN;
};
using Series = QVector<Point>;

// One candle of Yahoo's chart (intraday or daily), in UTC.
struct Bar {
    QDateTime time;
    double open = NaN, high = NaN, low = NaN, close = NaN, volume = NaN;
};

// A company's reported figures over time, by metric id (fundamentalMetrics()).
struct Fundamentals {
    QString source;      // "SEC EDGAR" or "Yahoo Finance"
    QString currency;
    QHash<QString, Series> annual, quarterly;   // by period end, oldest first
    bool isEmpty() const { return annual.isEmpty() && quarterly.isEmpty(); }
};
struct Metric {
    QString id;
    QString label;
    enum Kind { Money, PerShare, Percent } kind;
};
const QVector<Metric> &fundamentalMetrics();
const Metric *findMetric(const QString &id);

// Yahoo's statements: the columns (period ends, newest first) and every line's values, by Yahoo's line name
// (without the "annual" / "quarterly" prefix).
struct Statement {
    QVector<QDate> ends;
    QString currency;
    QHash<QString, QVector<double>> lines;   // NaN where a period has no value
};

// A line of a statement as Yahoo lays it out: its name, label, indent and whether it opens a group.
struct StatementLine {
    QString key, label;
    int depth = 0;
    bool group = false;
};
// "income", "balance" or "cash".
const QVector<StatementLine> &statementLayout(const QString &kind);

struct Holder {
    QString name;
    QDate date;
    double shares = NaN, change = NaN, changePct = NaN, value = NaN;
};
struct Holders {
    double institutionalPct = NaN;
    int count = 0;
    QVector<Holder> top;
};

struct InsiderTrade {
    QDate date;
    QString name, relation, type;
    double shares = NaN, price = NaN, held = NaN;
    bool buy = false, sell = false;
    bool openMarket = false;   // a sale or purchase on the market (not a gift, a withholding or an exercise)
};

// The US Treasury's daily par yield curve: one day's yields in percent, by tenor as the Treasury names it
// ("3 Mo", "2 Yr"…).
struct YieldCurveDay {
    QDate date;
    QHash<QString, double> yields;
};

struct SearchHit {
    QString symbol, name, exchange, type;
};

// One listing of the Nasdaq screener (every US stock).
struct Listing {
    QString symbol, name, sector, industry, country;
    double price = NaN, changePct = NaN, marketCap = NaN, volume = NaN;
};

// Members of the S&P 500, the Nasdaq-100 and the Dow, with their GICS sector.
struct IndexMember {
    QString symbol, name, sector;
};
struct Indexes {
    QVector<IndexMember> sp500, nasdaq100, dow;
    QStringList indexesOf(const QString &symbol) const;
};

// Supply chain links recorded on Wikidata, through products: suppliers make the company's products (P176,
// manufacturer), customers sell products the company makes (P1716 brand, P178 developer). No revenue shares.
struct Partner {
    QString item;          // Wikidata id
    QString name;
    QString symbol;        // a Yahoo symbol when the company is listed on an exchange Yahoo carries
    QString listing;       // the listing as Wikidata gives it, "Taiwan Stock Exchange: 2317"
    QStringList products;  // the products linking them
};
struct SupplyChain {
    QString item;          // the company's Wikidata id, empty when no item lists this ticker
    QVector<Partner> suppliers, customers;
};

// The crypto market's total capitalisation (dollars) and bitcoin's share of it (percent), daily over the last
// year and weekly before.
struct CryptoMarket {
    Series cap, btcDominance;
};

// Requests the Sources page shows, per host.
struct HostState {
    QDateTime lastOk;
    QDateTime lastError;
    QString error;
    int pending = 0;
};

class Client : public QObject
{
    Q_OBJECT
public:
    static Client *instance();

    // `ctx` guards the callback: it isn't called once ctx is gone.
    template <typename T>
    using Done = std::function<void(const T &value, const QString &error)>;

    void summary(const QString &symbol, QObject *ctx, Done<QJsonObject> done);
    void quotes(const QStringList &symbols, QObject *ctx, Done<QHash<QString, QJsonObject>> done);
    // The same, kept for maxAgeSecs (the Overview refreshes every minute).
    void quotes(const QStringList &symbols, qint64 maxAgeSecs, QObject *ctx, Done<QHash<QString, QJsonObject>> done);
    // One of Yahoo's predefined screeners ("day_gainers", "day_losers", "most_actives"): its quotes, in order.
    void screener(const QString &id, int count, QObject *ctx, Done<QVector<QJsonObject>> done);
    // The Treasury's daily par yield curve this year (and last year's in early January), newest first.
    void treasuryYields(QObject *ctx, Done<QVector<YieldCurveDay>> done);
    void statement(const QString &symbol, const QString &kind, bool quarterly, QObject *ctx, Done<Statement> done);
    // Economy: one FRED series from `since` (invalid: all of it), oldest first: FRED's API when a key is saved in
    // Sources, else the public CSV of its graphs (no key). Kept six hours.
    void fredSeries(const QString &id, const QDate &since, QObject *ctx, Done<Series> done);
    // One monthly series of the OECD's key economic indicators (KEI), as a flow key ("USA.M.CCICP....."); a day.
    void oecdSeries(const QString &key, const QDate &since, QObject *ctx, Done<Series> done);
    // Economy › Countries: a cached GET of `url` under `key`, whose prefix before ':' names the Sources group
    // ("worldbank", "imf", "eurostat", "bis", "oecd", "eia", "ilo", "ecb"); the body as downloaded. `json` asks for JSON (IMF).
    void raw(const QString &key, const QUrl &url, qint64 maxAgeSecs, bool json, QObject *ctx, Done<QByteArray> done);
    // The same with extra request headers (an API key sent as a header, kept out of the cache key).
    void raw(const QString &key, const QUrl &url, qint64 maxAgeSecs, bool json,
             const QList<std::pair<QByteArray, QByteArray>> &headers, QObject *ctx, Done<QByteArray> done);
    // The crypto Fear & Greed index (alternative.me), daily since 2018; an hour.
    void cryptoFearGreed(QObject *ctx, Done<Series> done);
    // CoinMarketCap's altcoin season index (0 bitcoin season … 100 altcoin season), daily since March 2024; an hour.
    void altcoinSeason(QObject *ctx, Done<Series> done);
    // CoinMarketCap's total market cap and bitcoin dominance; an hour.
    void cryptoMarket(QObject *ctx, Done<CryptoMarket> done);
    void fundamentals(const QString &symbol, QObject *ctx, Done<Fundamentals> done);
    void prices(const QString &symbol, const QString &range, const QString &interval, QObject *ctx, Done<Series> done);
    // Full candles (open, high, low, close, volume); Yahoo's intervals: 5m, 15m, 60m, 1d…
    void candles(const QString &symbol, const QString &range, const QString &interval, QObject *ctx, Done<QVector<Bar>> done);
    void spark(const QStringList &symbols, const QString &range, QObject *ctx, Done<QHash<QString, Series>> done);
    void holders(const QString &symbol, QObject *ctx, Done<Holders> done);
    void insiders(const QString &symbol, QObject *ctx, Done<QVector<InsiderTrade>> done);
    void peers(const QString &symbol, QObject *ctx, Done<QStringList> done);
    void employees(const QString &symbol, QObject *ctx, Done<Series> done);
    void supplyChain(const QString &symbol, QObject *ctx, Done<SupplyChain> done);
    void search(const QString &query, QObject *ctx, Done<QVector<SearchHit>> done);
    void listings(QObject *ctx, Done<QVector<Listing>> done);
    void indexes(QObject *ctx, Done<Indexes> done);

    // Sources page, per group of hosts: "yahoo", "nasdaq", "sec", "wikipedia", "wikidata", "treasury", and the
    // Economy's "fred", "oecd", "coinmarketcap", "alternative". A group turned off
    // makes no requests (cached copies stay in use); forget() makes its cached answers stale.
    HostState host(const QString &group) const { return m_hosts.value(group); }
    void setEnabled(const QString &group, bool on);
    void forget(const QString &group);

private:
    explicit Client(QObject *parent = nullptr);

    enum Flag { YahooCrumb = 1, Nasdaq = 2, Browser = 4, Sec = 8, Wiki = 16, Json = 32 };
    using Raw = std::function<void(const QByteArray &body, const QString &error)>;
    // A cached GET: from memory or disk while younger than maxAge, else downloaded. A failed download
    // falls back to a stale copy (with the error). `transform` turns a download into what is cached.
    void get(const QString &key, const QUrl &url, qint64 maxAgeSecs, int flags, QObject *ctx, Raw done,
             std::function<QByteArray(const QByteArray &)> transform = {});
    struct Job {
        QString key;
        QUrl url;
        int flags = 0;
        std::function<QByteArray(const QByteArray &)> transform;
        int attempt = 0;
        QList<std::pair<QByteArray, QByteArray>> headers;   // extra request headers (an API key)
    };
    QHash<QString, QList<std::pair<QByteArray, QByteArray>>> m_pendingHeaders;   // by key, for the next enqueue
    void enqueue(const Job &job);
    void pump();
    void send(Job job);
    void finish(const QString &key, const QByteArray &body, const QString &error);
    void ensureCrumb();
    void secCik(const QString &symbol, QObject *ctx, Done<QString> done);

    struct Waiter {
        QPointer<QObject> ctx;
        Raw done;
    };
    struct Entry {
        QByteArray body;
        QDateTime at;
    };
    // What a cached body parses to, kept with that body (the same shared bytes until the copy is refreshed):
    // the views asking for a symbol's summary, the listings or the indexes parse them once, not each.
    static bool sameBody(const QByteArray &a, const QByteArray &b)
    {
        return !a.isEmpty() && a.constData() == b.constData() && a.size() == b.size();
    }
    struct ParsedListings {
        QByteArray body;
        QVector<Listing> value;
    } m_listingsParsed;
    struct ParsedSummary {
        QByteArray body;
        QJsonObject value;
    };
    QHash<QString, ParsedSummary> m_summaryParsed;   // by symbol, the last few
    struct ParsedIndexes {
        QByteArray sp, dow, ndx;
        Indexes value;
    } m_indexesParsed;

    QNetworkAccessManager m_net;
    QHash<QString, Entry> m_memory;
    QHash<QString, QVector<Waiter>> m_waiting;
    QQueue<Job> m_queue;
    QHash<QString, int> m_inFlight;   // per host
    QHash<QString, qint64> m_lastStart;   // per host, ms since epoch: hosts with a rate limit are paced
    bool m_pumpScheduled = false;
    QHash<QString, HostState> m_hosts;
    QString m_crumb;
    bool m_crumbPending = false;
    QVector<Job> m_needCrumb;
    QSet<QString> m_disabled;
};

// --- Derived figures, shared by the views.

// Intrinsic value per share from a two-stage discounted free cash flow (see the .cpp for the method).
struct Valuation {
    double intrinsic = NaN;      // per share
    double price = NaN;
    double upside = NaN;         // intrinsic / price - 1
    double discountRate = NaN, growth = NaN;
    QString method;              // a sentence, for the tooltip
};
Valuation intrinsicValue(const QJsonObject &summary);
// The rate from the statements' currency to the quote's (an ADR: TWD to USD), from Yahoo's currency pair;
// 1 when they're the same currency, NaN when unknown.
double financialToQuote(const QJsonObject &summary);

// Letter grades (A+ … F) of three factors, and an overall rating from 1 (strong sell) to 5 (strong buy).
struct Grades {
    QString valuation, profitability, health;
    double valuationScore = NaN, profitabilityScore = NaN, healthScore = NaN;   // 0..1
    double score = NaN;          // 1..5
    QString rating;              // "Strong Buy" … "Strong Sell"
};
Grades factorGrades(const QJsonObject &summary, const Valuation &valuation);
QString ratingName(double score);   // 1..5, 5 = strong buy

// Analyst firms' grades sorted into five buckets (0 strong sell … 4 strong buy), -1 when unknown.
int gradeBucket(const QString &grade);

// Formatting.
QString money(double v, const QString &currency = "USD", int decimals = 2);   // $3.98T, $435.62B, $7.49
QString compact(double v, int decimals = 2);                                   // 14.70B, 164.00K
QString percent(double v, int decimals = 2, bool sign = true);                 // v as a fraction: 0.12 -> +12.00%
QString plain(double v, int decimals = 2);
QString full(double v, const QString &currency = "USD");                       // $416,161,000,000.00
QString currencySymbol(const QString &currency);

} // namespace research
