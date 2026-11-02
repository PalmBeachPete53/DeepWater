#pragma once

// Port of lib/providers.js. Market data from free, keyless public sources, each with fallbacks.
// Keys you add yourself (<data>/feeds.json, see Feeds.h) go in front of them.
//
//   Crypto  BINANCE:BTCUSDT   Binance -> Binance market-data mirror -> Binance.US -> Coinbase -> Kraken
//   Stocks  US:AAPL           Your Alpaca / Polygon key -> Yahoo (when it isn't rate limiting you) -> Nasdaq
//   Forex   FX:EURUSD         Yahoo -> Kraken
//   Futures CME:NQ1!          Your Polygon key -> Yahoo, about 10 minutes delayed
//   Global  YAHOO:^FTSE       Yahoo only (indices, futures, non-US stocks)
//
// Symbols are "EXCHANGE:TICKER" strings, like TradingView's. resolve() turns what a person types
// ("btc", "aapl", "eurusd", "VOD.L") into one.
//
// Every function is blocking and safe to call from several threads at once; the module state of
// the JS (health, caches, Yahoo's back-off, the symbol maps ...) is shared behind mutexes.

#include "Cache.h"
#include "Feeds.h"
#include "Imports.h"

#include <QAtomicInteger>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QMutex>
#include <QSet>
#include <QString>
#include <QStringList>

#include <atomic>
#include <memory>

namespace qtm {

struct Resolved {
    QString id;
    QString exchange;
    QString ticker;
    QJsonObject toJson() const { return { { "id", id }, { "exchange", exchange }, { "ticker", ticker } }; }
};

struct Spread {
    QString a, op, b;
};

struct BinSym {
    QString base, quote;
};
// A JS Map ticker -> { base, quote }, in insertion order.
struct SymbolMap {
    QStringList order;
    QHash<QString, BinSym> map;
    bool has(const QString &k) const { return map.contains(k); }
    qsizetype size() const { return order.size(); }
};
using SymbolMapPtr = std::shared_ptr<const SymbolMap>;

struct ProviderOptions {
    int timeoutMs = 10000;     // QT_TIMEOUT_MS
    bool offline = false;      // QT_OFFLINE=1: fixtures instead of the network (tests)
    QString fixturesDir;       // test/fixtures, for offline mode
};

class Providers
{
public:
    Providers(Feeds &feeds, Imports &imports, ProviderOptions options);

    static QJsonObject futures();          // FUTURES
    QJsonObject health();                  // export const health
    bool yahooAvailable() const;

    static std::optional<Spread> parseSpread(const QString &raw);
    Resolved resolve(const QString &raw);
    static QString kindOf(const QString &id);

    CandleResult getCandles(const QString &rawSymbol, const QString &tf = "1D", double limit = 1500);
    QJsonObject getQuotes(const QStringList &symbols);
    QJsonObject getSpread(const QString &rawSymbol);
    QJsonArray search(const QString &q);
    // Every symbol of one market (crypto, stocks, etf, forex, futures, indices), biggest first, for
    // browsing when you don't know the ticker. Not in the JS: a deepwater addition.
    QJsonArray browse(const QString &market);
    QJsonArray cryptoScreener();
    QJsonArray nasdaqScreener(const QString &kind = "stocks");
    QJsonArray economicCalendar();
    QJsonObject probeAll();

    // Deepwater's Sources page (not in the JS). Sources turned off there: binance, coinbase, kraken,
    // yahoo, nasdaq, calendar, cryptonews. Their requests fail at once with "<name> is turned off in
    // Sources", so the fallbacks behind them carry the charts, and health() leaves them out.
    void setDisabled(const QSet<QString> &ids);
    bool isDisabled(const QString &id);
    // One source's check from probeAll() by id (the ids above, alpaca, polygon): { ok, ms } or
    // { ok: false, error }.
    QJsonObject probe(const QString &id);

    SymbolMapPtr binanceSymbolMap();
    // Deepwater's footprint chart (ui/FootprintFeed), not in the JS. Binance's price step for a pair (exchangeInfo
    // PRICE_FILTER tickSize, kept 6 h), and its aggregated trades: `limit` (≤ 1000) from trade id `fromId` on, or
    // the latest ones when fromId < 0. `us` says whether they came from binance.us, a separate exchange whose trade
    // ids don't match binance.com's (the fallback when binance.com is blocked). Blocking; throws qtm::Error.
    double binanceTickSize(const QString &ticker);
    QJsonArray binanceAggTrades(const QString &ticker, qint64 fromId, int limit, bool *us = nullptr);
    // The liquidity heatmap's order book snapshot: /api/v3/depth (lastUpdateId, bids, asks), `limit` levels a side
    // (5000 at most; weight 250 at 5000, 5 up to 100). `us` as for binanceAggTrades.
    QJsonObject binanceDepth(const QString &ticker, int limit, bool *us = nullptr);
    // "BTCUSD" -> "XBTUSD": Kraken's own name for the pair, keyed by the names everyone else uses.
    static QHash<QString, QString> krakenPairMap(const QJsonObject &result);
    static Bars parseYahoo(const QJsonValue &j, const QString &tf);
    // Nasdaq stamps intraday points with New York wall-clock time written as if it were UTC.
    static double nyWallToUtc(double ms);

    // Shared with market-info.js (it imports getJson, cached, NQ from providers.js).
    QJsonValue getJson(const QString &url, const QList<QPair<QByteArray, QByteArray>> &headers = {},
                       QByteArray *raw = nullptr);
    template <typename T>
    T cached(const QString &key, double ttlMs, const std::function<T()> &fn)
    {
        return m_cache.get<T>(key, ttlMs, fn);
    }
    static QList<QPair<QByteArray, QByteArray>> NQ();
    bool offline() const { return m_opt.offline; }
    QJsonValue fixture(const QString &name) const;

private:
    void mark(const QString &name, bool ok, const QString &err = QString());
    QJsonValue binanceGet(const QString &p);
    Bars binanceCandles(const QString &ticker, const QString &tf, int limit);
    Bars coinbaseCandles(const QString &product, const QString &tf, int limit);
    Bars krakenCandles(const QString &pair, const QString &tf, int limit);
    Bars yahooCandles(const QString &symbol, const QString &tf, int limit);
    std::shared_ptr<const QSet<QString>> coinbaseProducts();
    std::shared_ptr<const QHash<QString, QString>> krakenPairs();
    QSet<QString> usTickerSet();
    struct NasdaqResult {
        QJsonValue j;
        QString cls;
    };
    NasdaqResult nasdaqQuery(const QString &ticker, const std::function<QString(const QString &)> &pathFn);
    struct Today {
        QJsonArray pts;
        double volume = 0;
    };
    Today nasdaqToday(const QString &ticker);
    Bars nasdaqCandles(const QString &ticker, const QString &tf, int limit);
    QHash<QString, QJsonObject> nasdaqLiveQuotes(const QStringList &tickers, const QString &defaultClass = QString());
    CandleResult spreadCandles(const Resolved &sym, const QString &tf, int limit);
    struct ChainResult {
        Bars candles;
        QString provider;
        QStringList fallbacks;
    };
    ChainResult tryChain(const Chain &chain);
    void setYahooDown();

    Feeds &m_feeds;
    Imports &m_imports;
    ProviderOptions m_opt;
    Cache m_cache;

    QMutex m_mutex;                        // guards everything below
    QJsonObject m_health;
    QSet<QString> m_disabled;
    QHash<QString, double> m_hostDownUntil;
    QHash<QString, QString> m_nqClass;
    SymbolMapPtr m_binanceSymbols;
    std::shared_ptr<QSet<QString>> m_usTickers;
    std::atomic<int> m_binanceHostIdx { 0 };
    std::atomic<bool> m_binanceKnownGood { false };
    std::atomic<double> m_yahooDownUntil { 0 };
};

} // namespace qtm
