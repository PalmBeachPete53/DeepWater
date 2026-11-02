#include "Providers.h"

#include "../core/Error.h"
#include "../core/Series.h"
#include "Async.h"
#include "Http.h"
#include "Json.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QLocale>
#include <QRegularExpression>
#include <QTimeZone>
#include <QUrl>

#include <algorithm>
#include <cmath>
#include <optional>

namespace qtm {

using Headers = QList<QPair<QByteArray, QByteArray>>;

static const QByteArray UA =
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126.0 Safari/537.36";
// Yahoo answers 429 to any full browser User-Agent (seen 15 Oct 2026) but 200 to a bare one.
static const Headers YAHOO = { { "User-Agent", "Mozilla/5.0" } };
static const QStringList FIAT = { "USD", "EUR", "GBP", "JPY", "CHF", "CAD", "AUD", "NZD" };

// Spot metals, quoted like forex (XAUUSD). Yahoo's futures first; gold falls back to PAXG, a token
// backed one-to-one by gold, so the chart still loads when Yahoo is rate limiting.
struct Metal {
    QString name, word, yahoo, token;
};
static const QVector<QPair<QString, Metal>> &metals()
{
    static const QVector<QPair<QString, Metal>> m = {
        { "XAUUSD", { "Gold / USD", "GOLD", "GC=F", "PAXG" } },
        { "XAGUSD", { "Silver / USD", "SILVER", "SI=F", "" } },
    };
    return m;
}
static const Metal *metal(const QString &t)
{
    for (const auto &p : metals())
        if (p.first == t) return &p.second;
    return nullptr;
}

// CME equity index futures as continuous front-month charts, TradingView-style (NQ1!). The public
// chart is Yahoo's, about 10 minutes behind; your own Polygon key charts the live contract.
struct Future {
    QString name, yahoo;
};
static const QVector<QPair<QString, Future>> &futuresList()
{
    static const QVector<QPair<QString, Future>> f = {
        { "NQ", { "E-mini Nasdaq-100 futures", "NQ=F" } },
        { "MNQ", { "Micro E-mini Nasdaq-100 futures", "MNQ=F" } },
        { "ES", { "E-mini S&P 500 futures", "ES=F" } },
        { "MES", { "Micro E-mini S&P 500 futures", "MES=F" } },
    };
    return f;
}
static const Future *future(const QString &root)
{
    for (const auto &p : futuresList())
        if (p.first == root) return &p.second;
    return nullptr;
}

QJsonObject Providers::futures()
{
    QJsonObject o;
    for (const auto &p : futuresList())
        o.insert(p.first, QJsonObject { { "name", p.second.name }, { "yahoo", p.second.yahoo } });
    return o;
}

static const QStringList MAJOR_CRYPTO = {
    "BTC", "ETH", "SOL", "XRP", "BNB", "DOGE", "ADA", "AVAX", "LINK", "DOT", "LTC", "BCH", "TRX", "XLM", "ATOM", "UNI",
    "NEAR", "APT", "ARB", "OP", "SUI", "PEPE", "SHIB", "AAVE", "ETC", "FIL", "HBAR", "ICP", "INJ", "TON", "XMR", "ZEC", "HYPE",
};

static Error badInput(const QString &message)
{
    return Error(message, 400);
}

static Error upstreamError(const QString &message, int status = 0)
{
    return Error(message, status, true);
}

static QString jsNum(double d)
{
    return jsv::numberToString(d);
}

static QRegularExpression rx(const QString &p)
{
    return QRegularExpression(p);
}

static Bars tail(const Bars &bars, int limit)
{
    if (limit <= 0) return bars.mid(bars.size());   // slice(-0) is the whole array, but limit is >= 10
    return bars.mid(std::max<qsizetype>(0, bars.size() - limit));
}

static Bars dedupe(const Bars &bars)
{
    Bars out;
    for (const Bar &b : bars) {
        if (!out.isEmpty() && b.time <= out.last().time) {
            if (b.time == out.last().time) out.last() = b;
            continue;
        }
        out.push_back(b);
    }
    return out;
}

static Bars barsFromJson(const QJsonArray &a)
{
    Bars out;
    out.reserve(a.size());
    for (const QJsonValue v : a) {
        Bar b;
        b.time = jsv::toNumber(v["time"]);
        b.open = jsv::toNumber(v["open"]);
        b.high = jsv::toNumber(v["high"]);
        b.low = jsv::toNumber(v["low"]);
        b.close = jsv::toNumber(v["close"]);
        b.volume = v["volume"].isUndefined() ? 0 : jsv::toNumber(v["volume"]);
        out.push_back(b);
    }
    return out;
}

// ---------- plumbing ----------
Providers::Providers(Feeds &feeds, Imports &imports, ProviderOptions options)
    : m_feeds(feeds), m_imports(imports), m_opt(std::move(options))
{
}

QJsonObject Providers::health()
{
    QMutexLocker lock(&m_mutex);
    QJsonObject out = m_health;
    for (const QString &id : std::as_const(m_disabled)) out.remove(id);
    return out;
}

void Providers::setDisabled(const QSet<QString> &ids)
{
    QMutexLocker lock(&m_mutex);
    m_disabled = ids;
}

bool Providers::isDisabled(const QString &id)
{
    QMutexLocker lock(&m_mutex);
    return m_disabled.contains(id);
}

// Which Sources entry a host belongs to.
static QString sourceOfHost(const QString &host)
{
    static const QHash<QString, QString> map = {
        { "api.binance.com", "binance" }, { "data-api.binance.vision", "binance" }, { "api.binance.us", "binance" },
        { "api.exchange.coinbase.com", "coinbase" }, { "api.kraken.com", "kraken" },
        { "query1.finance.yahoo.com", "yahoo" }, { "query2.finance.yahoo.com", "yahoo" },
        { "api.nasdaq.com", "nasdaq" }, { "nfs.faireconomy.media", "calendar" },
    };
    return map.value(host);
}

static QString sourceName(const QString &id)
{
    static const QHash<QString, QString> names = {
        { "binance", "Binance" }, { "coinbase", "Coinbase" }, { "kraken", "Kraken" }, { "yahoo", "Yahoo Finance" },
        { "nasdaq", "Nasdaq" }, { "calendar", "The economic calendar" }, { "cryptonews", "Crypto news" },
    };
    return names.value(id, id);
}

void Providers::mark(const QString &name, bool ok, const QString &err)
{
    QJsonObject o { { "ok", ok }, { "at", jsv::isoNow() } };
    if (!err.isNull()) o.insert("error", err.left(200));
    QMutexLocker lock(&m_mutex);
    m_health.insert(name, o);
}

QList<QPair<QByteArray, QByteArray>> Providers::NQ()
{
    return { { "Origin", "https://www.nasdaq.com" }, { "Referer", "https://www.nasdaq.com/" } };
}

QJsonValue Providers::fixture(const QString &name) const
{
    QFile f(QDir(m_opt.fixturesDir).filePath(name));
    if (!f.open(QIODevice::ReadOnly))
        throw Error(QStringLiteral("ENOENT: no such file or directory, open '%1'").arg(f.fileName()));
    return jsv::parse(f.readAll());
}

static QString hostOf(const QString &url)
{
    const QUrl u(url);
    QString h = u.host();
    if (u.port() != -1
        && !((u.scheme() == "https" && u.port() == 443) || (u.scheme() == "http" && u.port() == 80)))
        h += ':' + QString::number(u.port());
    return h;
}

// A host that timed out or couldn't be reached is skipped for a minute, so a firewall that
// silently drops one provider costs one timeout, not one per chart, before the fallback runs.
QJsonValue Providers::getJson(const QString &url, const Headers &headers, QByteArray *raw)
{
    const QString host = hostOf(url);
    {
        QMutexLocker lock(&m_mutex);
        const QString source = sourceOfHost(host);
        if (!source.isEmpty() && m_disabled.contains(source))
            throw upstreamError(QStringLiteral("%1 is turned off in Sources").arg(sourceName(source)));
        if (m_hostDownUntil.value(host, 0) > jsv::nowMs())
            throw upstreamError(QStringLiteral("%1 isn't reachable from here (trying again in a minute)").arg(host));
    }
    http::FetchOptions o;
    Headers h = {
        { "User-Agent", UA },
        { "Accept", "application/json, text/plain, */*" },
        { "Accept-Language", "en-US,en;q=0.9" },
    };
    for (const auto &x : headers) {
        bool replaced = false;
        for (auto &y : h)
            if (y.first.toLower() == x.first.toLower()) {
                y.second = x.second;
                replaced = true;
            }
        if (!replaced) h << x;
    }
    o.headers = h;
    o.timeoutMs = m_opt.timeoutMs;
    http::FetchResponse res;
    try {
        res = http::fetch(url, o);
    } catch (const http::FetchError &e) {
        {
            QMutexLocker lock(&m_mutex);
            m_hostDownUntil.insert(host, jsv::nowMs() + 60e3);
        }
        const QString why = e.timeout() ? QStringLiteral("timed out") : !e.code().isEmpty() ? e.code() : e.message();
        throw upstreamError(QStringLiteral("%1 couldn't be reached (%2)").arg(host, why));
    }
    {
        QMutexLocker lock(&m_mutex);
        m_hostDownUntil.remove(host);
    }
    if (!res.ok())
        throw upstreamError(QStringLiteral("%1 returned HTTP %2").arg(host).arg(res.status), res.status);
    if (raw) *raw = res.body;
    return res.json();
}

static double ttlFor(const QString &tf)
{
    const double s = double(parseTf(tf).seconds);
    return s < 60 ? 1500 : s < 3600 ? 4000 : s < 86400 ? 15000 : 60000;
}

// ---------- symbol resolution ----------
SymbolMapPtr Providers::binanceSymbolMap()
{
    if (m_opt.offline) {
        auto m = std::make_shared<SymbolMap>();
        m->order = { "BTCUSDT", "ETHUSDT" };
        m->map.insert("BTCUSDT", { "BTC", "USDT" });
        m->map.insert("ETHUSDT", { "ETH", "USDT" });
        return m;
    }
    try {
        return cached<SymbolMapPtr>("binance:symbols", 6 * 3600e3, [this]() -> SymbolMapPtr {
            const QJsonValue info = binanceGet("/api/v3/exchangeInfo");
            auto m = std::make_shared<SymbolMap>();
            for (const QJsonValue s : info["symbols"].toArray()) {
                if (s["status"].toString() != "TRADING") continue;
                const QString sym = jsv::toString(s["symbol"]);
                if (!m->map.contains(sym)) m->order << sym;
                m->map.insert(sym, { jsv::toString(s["baseAsset"]), jsv::toString(s["quoteAsset"]) });
            }
            QMutexLocker lock(&m_mutex);
            m_binanceSymbols = m;
            return m;
        });
    } catch (...) {
        QMutexLocker lock(&m_mutex);
        return m_binanceSymbols ? m_binanceSymbols : std::make_shared<SymbolMap>();
    }
}

static const QStringList BINANCE_HOSTS = {
    "https://api.binance.com",
    "https://data-api.binance.vision",
    "https://api.binance.us",
};

double Providers::binanceTickSize(const QString &ticker)
{
    return cached<double>("binance:tick:" + ticker, 6 * 3600e3, [this, ticker]() -> double {
        const QJsonValue info = binanceGet("/api/v3/exchangeInfo?symbol=" + jsv::encodeURIComponent(ticker));
        for (const QJsonValue s : info["symbols"].toArray())
            for (const QJsonValue f : s["filters"].toArray())
                if (f["filterType"].toString() == "PRICE_FILTER") {
                    const double tick = jsv::toString(f["tickSize"]).toDouble();
                    if (tick > 0) return tick;
                }
        throw Error(QStringLiteral("No tick size for %1 on Binance").arg(ticker), 502);
    });
}

QJsonArray Providers::binanceAggTrades(const QString &ticker, qint64 fromId, int limit, bool *us)
{
    QString p = "/api/v3/aggTrades?symbol=" + jsv::encodeURIComponent(ticker)
                + "&limit=" + QString::number(std::clamp(limit, 1, 1000));
    if (fromId >= 0) p += "&fromId=" + QString::number(fromId);
    const QJsonArray out = binanceGet(p).toArray();
    // binanceGet() leaves the index on the host that answered.
    if (us) *us = BINANCE_HOSTS.value(m_binanceHostIdx.load()).contains("binance.us");
    return out;
}

QJsonObject Providers::binanceDepth(const QString &ticker, int limit, bool *us)
{
    const QString p = "/api/v3/depth?symbol=" + jsv::encodeURIComponent(ticker)
                      + "&limit=" + QString::number(std::clamp(limit, 1, 5000));
    const QJsonObject out = binanceGet(p).toObject();
    if (us) *us = BINANCE_HOSTS.value(m_binanceHostIdx.load()).contains("binance.us");
    return out;
}

QSet<QString> Providers::usTickerSet()
{
    {
        QMutexLocker lock(&m_mutex);
        if (m_usTickers) return *m_usTickers;
    }
    auto set = std::make_shared<QSet<QString>>();
    try {
        for (const QJsonValue r : nasdaqScreener("stocks"))
            set->insert(jsv::toString(r["ticker"]));
    } catch (...) {
        set->clear();
    }
    QMutexLocker lock(&m_mutex);
    m_usTickers = set;
    return *set;
}

// Spreads and ratios, like TradingView's BTCUSDT/ETHUSDT or AAPL - MSFT: one operator between two
// symbols (or a symbol and a number). Canonical id: "SPREAD:<id> <op> <id>", spaces around op.
static bool isNumStr(const QString &x)
{
    static const QRegularExpression re(QStringLiteral("^-?\\d+(\\.\\d+)?$"));
    return re.match(jsv::trim(x)).hasMatch();
}

std::optional<Spread> Providers::parseSpread(const QString &raw)
{
    const QString s = jsv::trim(raw).toUpper();
    if (s.startsWith("SPREAD:")) {
        static const QRegularExpression re(QStringLiteral("^SPREAD:(.+?) ([/*+-]) (.+)$"));
        const auto m = re.match(s);
        if (!m.hasMatch()) return std::nullopt;
        return Spread { m.captured(1), m.captured(2), m.captured(3) };
    }
    static const QRegularExpression fx(QStringLiteral("^([A-Z]{3})\\/([A-Z]{3})$"));
    const auto f = fx.match(s);
    if (f.hasMatch() && FIAT.contains(f.captured(1)) && FIAT.contains(f.captured(2)))
        return std::nullopt;   // EUR/USD is a pair, not a ratio
    // SOL/USDT, BTC/USD: a quote currency on the right means a trading pair, not a ratio.
    static const QRegularExpression pair(QStringLiteral("^[A-Z0-9]{2,12}\\/(USDT|USDC|FDUSD|BUSD|USD)$"));
    if (pair.match(s).hasMatch()) return std::nullopt;
    static const QRegularExpression OP_SPLIT[] = {
        QRegularExpression(QStringLiteral("^(.+?)[" QTM_JS_SPACE "]*([/*])[" QTM_JS_SPACE "]*(.+)$")),
        QRegularExpression(QStringLiteral("^(.+?)[" QTM_JS_SPACE "]+([+-])[" QTM_JS_SPACE "]+(.+)$")),
    };
    for (const auto &re : OP_SPLIT) {
        const auto m = re.match(s);
        if (m.hasMatch() && !jsv::trim(m.captured(1)).isEmpty() && !jsv::trim(m.captured(3)).isEmpty())
            return Spread { jsv::trim(m.captured(1)), m.captured(2), jsv::trim(m.captured(3)) };
    }
    return std::nullopt;
}

Resolved Providers::resolve(const QString &raw)
{
    auto sp = parseSpread(raw);
    // ETH/BTC is Binance's ETHBTC pair when Binance lists it; otherwise it's a ratio.
    if (sp && sp->op == "/" && rx("^[A-Z0-9]{2,12}$").match(sp->a).hasMatch()
        && rx("^[A-Z0-9]{2,6}$").match(sp->b).hasMatch() && binanceSymbolMap()->has(sp->a + sp->b))
        sp.reset();
    if (sp) {
        if (parseSpread(sp->a) || parseSpread(sp->b))
            throw badInput("Use one operator per spread, like BTCUSDT/ETHUSDT");
        if (isNumStr(sp->a) && isNumStr(sp->b))
            throw badInput("A spread needs at least one symbol");
        const Spread s = *sp;
        const auto sides = async::all(2, [this, s](int i) {
            const QString x = i == 0 ? s.a : s.b;
            return isNumStr(x) ? jsNum(jsv::toNumber(x)) : resolve(x).id;
        });
        const QString ticker = QStringLiteral("%1 %2 %3").arg(sides[0], s.op, sides[1]);
        return { "SPREAD:" + ticker, "SPREAD", ticker };
    }
    QString input = jsv::trim(raw).toUpper();
    input.replace(rx("[" QTM_JS_SPACE "]+"), QString());
    if (input.isEmpty()) throw badInput("Empty symbol");
    // NQ1!, CME_MINI:NQ1!, MNQ: futures. A bare root that is also a US stock ticker (ES) stays the stock.
    static const QRegularExpression futRe(QStringLiteral("^(CME(?:_MINI)?:)?([A-Z]{2,3})(1!)?$"));
    const auto fut = futRe.match(input);
    if (fut.hasMatch() && future(fut.captured(2))
        && (!fut.captured(1).isEmpty() || !fut.captured(3).isEmpty() || !usTickerSet().contains(fut.captured(2))))
        return { "CME:" + fut.captured(2) + "1!", "CME", fut.captured(2) + "1!" };
    // Tickers go into upstream URLs, so only the characters real tickers use are allowed.
    QString probe = input;
    {
        static const QRegularExpression caret(QStringLiteral("(^|:)\\^"));
        const auto m = caret.match(probe);
        if (m.hasMatch()) probe.replace(m.capturedStart(), m.capturedLength(), m.captured(1) + "A");
    }
    static const QRegularExpression valid(QStringLiteral("^([A-Z0-9_]{1,20}:)?[A-Z0-9][A-Z0-9.\\-_^=/]{0,29}$"));
    if (!valid.match(probe).hasMatch())
        throw badInput(QStringLiteral("\"%1\" isn't a symbol").arg(raw.left(40)));
    if (input.contains(':')) {
        const QStringList parts = input.split(':');
        return { parts[0] + ':' + parts[1], parts[0], parts[1] };
    }
    QString fx = input;
    {
        const qsizetype k = fx.indexOf('/');
        if (k >= 0) fx.remove(k, 1);
    }
    if (rx("^[A-Z]{6}$").match(fx).hasMatch() && FIAT.contains(fx.left(3)) && FIAT.contains(fx.mid(3)))
        return { "FX:" + fx, "FX", fx };
    if (metal(fx)) return { "FX:" + fx, "FX", fx };
    if (rx("[=^.]").match(input).hasMatch()) return { "YAHOO:" + input, "YAHOO", input };
    const SymbolMapPtr bin = binanceSymbolMap();
    if (!bin->size()) {
        // Binance unreachable (blocked country, firewall): recognise crypto without its symbol list,
        // so the Coinbase and Kraken fallbacks still get the request.
        const auto pair = rx("^([A-Z0-9]{2,12})[-/]?(USDT|USDC|FDUSD)$").match(input);
        if (pair.hasMatch()) {
            const QString t = pair.captured(1) + pair.captured(2);
            return { "BINANCE:" + t, "BINANCE", t };
        }
        QString base = input;
        base.replace(rx("[-/]?USD$"), QString());
        if (MAJOR_CRYPTO.contains(base)) return { "BINANCE:" + base + "USDT", "BINANCE", base + "USDT" };
    }
    const auto slashPair = rx("^([A-Z0-9]{2,12})\\/([A-Z0-9]{2,6})$").match(input);
    if (slashPair.hasMatch() && bin->has(slashPair.captured(1) + slashPair.captured(2))) {
        const QString t = slashPair.captured(1) + slashPair.captured(2);
        return { "BINANCE:" + t, "BINANCE", t };
    }
    if (bin->has(input)) return { "BINANCE:" + input, "BINANCE", input };
    QString crypto = input;
    crypto.replace(rx("[-/]"), QString());
    if (bin->has(crypto + "USDT") && !usTickerSet().contains(input))
        return { "BINANCE:" + crypto + "USDT", "BINANCE", crypto + "USDT" };
    if (rx("^[A-Z]{2,10}-USD$").match(input).hasMatch()) return { "COINBASE:" + input, "COINBASE", input };
    return { "US:" + input, "US", input };
}

QString Providers::kindOf(const QString &id)
{
    const QString ex = id.section(':', 0, 0);
    return QStringList { "BINANCE", "COINBASE", "KRAKEN" }.contains(ex) ? "crypto"
           : ex == "FX"                                                ? "fx"
           : ex == "US"                                                ? "stock"
                                                                       : "global";
}

// ---------- Binance ----------

QJsonValue Providers::binanceGet(const QString &p)
{
    const int startIdx = m_binanceHostIdx.load();
    QVector<int> order;
    for (int k = 0; k < BINANCE_HOSTS.size(); ++k) order << (startIdx + k) % BINANCE_HOSTS.size();
    // Until a host has answered, ask all of them at once but still prefer them in order
    // (binance.us is a separate exchange with its own prices, so it is the last resort).
    // A firewall that silently drops Binance then costs one timeout, not three.
    QVector<async::Task<QJsonValue>> early;
    if (!m_binanceKnownGood.load())
        for (int idx : order) {
            const QString url = BINANCE_HOSTS[idx] + p;
            early.push_back(async::spawn([this, url] { return getJson(url); }));
        }
    std::optional<Error> lastErr;
    for (int k = 0; k < order.size(); ++k) {
        const int idx = order[k];
        try {
            const QJsonValue out = early.isEmpty() ? getJson(BINANCE_HOSTS[idx] + p) : early[k].get();
            m_binanceHostIdx = idx;
            m_binanceKnownGood = true;
            mark("binance", true);
            return out;
        } catch (const Error &e) {
            lastErr = e;
            // 451/403 = blocked in your country; 400 = bad symbol (don't hop hosts for that).
            if (e.status() == 400) throw;
        }
    }
    m_binanceKnownGood = false;
    mark("binance", false, lastErr->message());
    throw *lastErr;
}

static const QStringList BINANCE_NATIVE = { "1s", "1m", "3m", "5m", "15m", "30m", "1h", "2h", "4h", "6h", "8h", "12h", "1D", "3D", "1W", "1M" };

static QString binanceInterval(const QString &tf)
{
    if (tf == "1D") return "1d";
    if (tf == "3D") return "3d";
    if (tf == "1W") return "1w";
    return tf;
}

Bars Providers::binanceCandles(const QString &ticker, const QString &tf, int limit)
{
    const QString base = nativeBase(tf, BINANCE_NATIVE);
    if (base.isEmpty()) throw Error(QStringLiteral("Binance can't build a %1 chart").arg(tf));
    const double ratio = std::max(1.0, jsv::round(double(parseTf(tf).seconds) / double(parseTf(base).seconds)));
    const double want = std::min(limit * ratio + ratio, 20000.0);
    Bars out;
    double endTime = 0;
    bool haveEnd = false;
    while (out.size() < want) {
        const double n = std::min(1000.0, want - out.size());
        const QString q = QStringLiteral("/api/v3/klines?symbol=%1&interval=%2&limit=%3%4")
                              .arg(jsv::encodeURIComponent(ticker), binanceInterval(base), jsNum(n),
                                   haveEnd ? "&endTime=" + jsNum(endTime) : QString());
        const QJsonValue rowsV = binanceGet(q);
        const QJsonArray rows = rowsV.toArray();
        if (!rowsV.isArray() || rows.isEmpty()) break;
        Bars page;
        page.reserve(rows.size());
        for (const auto &k : rows) {
            Bar b;
            b.time = jsv::toNumber(k[0]) / 1000;
            b.open = jsv::toNumber(k[1]);
            b.high = jsv::toNumber(k[2]);
            b.low = jsv::toNumber(k[3]);
            b.close = jsv::toNumber(k[4]);
            b.volume = jsv::toNumber(k[5]);
            page.push_back(b);
        }
        out = page + out;
        endTime = jsv::toNumber(rows[0][0]) - 1;
        haveEnd = true;
        if (rows.size() < n) break;
    }
    return base == tf ? tail(out, limit) : tail(resample(out, tf), limit);
}

// ---------- Coinbase ----------
Bars Providers::coinbaseCandles(const QString &product, const QString &tf, int limit)
{
    static const QVector<QPair<QString, int>> NATIVE = {
        { "1m", 60 }, { "5m", 300 }, { "15m", 900 }, { "1h", 3600 }, { "6h", 21600 }, { "1D", 86400 },
    };
    QStringList keys;
    for (const auto &p : NATIVE) keys << p.first;
    const QString base = nativeBase(tf, keys);
    if (base.isEmpty()) throw Error(QStringLiteral("Coinbase can't build a %1 chart").arg(tf));
    int g = 0;
    for (const auto &p : NATIVE)
        if (p.first == base) g = p.second;
    const double ratio = std::max(1.0, jsv::round(double(parseTf(tf).seconds) / g));
    const double want = std::min(limit * ratio, 1500.0);
    double end = std::floor(jsv::nowMs() / 1000);
    Bars out;
    try {
        while (out.size() < want) {
            const double start = end - 300.0 * g;
            const QJsonValue rowsV = getJson(
                QStringLiteral("https://api.exchange.coinbase.com/products/%1/candles?granularity=%2&start=%3&end=%4")
                    .arg(jsv::encodeURIComponent(product)).arg(g)
                    .arg(jsv::isoString(start * 1000), jsv::isoString(end * 1000)));
            const QJsonArray rows = rowsV.toArray();
            if (rows.isEmpty()) break;
            Bars page;
            for (qsizetype i = rows.size() - 1; i >= 0; --i) {
                const QJsonValue r = rows[i];
                Bar b;
                b.time = jsv::toNumber(r[0]);
                b.low = jsv::toNumber(r[1]);
                b.high = jsv::toNumber(r[2]);
                b.open = jsv::toNumber(r[3]);
                b.close = jsv::toNumber(r[4]);
                b.volume = jsv::toNumber(r[5]);
                page.push_back(b);
            }
            out = page + out;
            end = start;
        }
        mark("coinbase", true);
    } catch (const Error &e) {
        mark("coinbase", false, e.message());
        throw;
    }
    out = dedupe(out);
    return base == tf ? tail(out, limit) : tail(resample(out, tf), limit);
}

// ---------- Kraken ----------
static QString krakenAsset(const QString &a)
{
    return a == "BTC" ? "XBT" : a;
}

// Product lists for search. An exchange that doesn't answer just drops out of the results.
std::shared_ptr<const QSet<QString>> Providers::coinbaseProducts()
{
    using P = std::shared_ptr<const QSet<QString>>;
    try {
        return cached<P>("coinbase:products", 6 * 3600e3, [this]() -> P {
            auto s = std::make_shared<QSet<QString>>();
            for (const QJsonValue p : getJson("https://api.exchange.coinbase.com/products").toArray())
                if (p["status"].toString() == "online") s->insert(jsv::toString(p["id"]));
            return s;
        });
    } catch (...) {
        return std::make_shared<QSet<QString>>();
    }
}

QHash<QString, QString> Providers::krakenPairMap(const QJsonObject &result)
{
    QHash<QString, QString> m;
    for (const QJsonValue p : result) {
        if (!jsv::truthy(p["wsname"]) || p["status"].toString() == "delisted") continue;
        const QStringList parts = jsv::toString(p["wsname"]).split('/');
        QString b = parts.value(0);
        const QString q = parts.size() > 1 ? parts[1] : QStringLiteral("undefined");
        if (b == "XBT") b = "BTC";
        else if (b == "XDG") b = "DOGE";
        m.insert(b + q, jsv::toString(p["altname"]));
    }
    return m;
}

std::shared_ptr<const QHash<QString, QString>> Providers::krakenPairs()
{
    using P = std::shared_ptr<const QHash<QString, QString>>;
    try {
        return cached<P>("kraken:pairs", 6 * 3600e3, [this]() -> P {
            const QJsonValue j = getJson("https://api.kraken.com/0/public/AssetPairs");
            const QJsonArray err = j["error"].toArray();
            if (!err.isEmpty()) throw Error(jsv::toString(err).replace(",", ", "));
            return std::make_shared<QHash<QString, QString>>(krakenPairMap(j["result"].toObject()));
        });
    } catch (...) {
        return std::make_shared<QHash<QString, QString>>();
    }
}

static QString joinErrors(const QJsonArray &a)
{
    QStringList parts;
    for (const auto &x : a) parts << (jsv::nullish(x) ? QString() : jsv::toString(x));
    return parts.join(", ");
}

Bars Providers::krakenCandles(const QString &pair, const QString &tf, int limit)
{
    static const QVector<QPair<QString, int>> NATIVE = {
        { "1m", 1 }, { "5m", 5 }, { "15m", 15 }, { "30m", 30 }, { "1h", 60 }, { "4h", 240 }, { "1D", 1440 },
    };
    QStringList keys;
    for (const auto &p : NATIVE) keys << p.first;
    const QString base = nativeBase(tf, keys);
    if (base.isEmpty()) throw Error(QStringLiteral("Kraken can't build a %1 chart").arg(tf));
    int interval = 0;
    for (const auto &p : NATIVE)
        if (p.first == base) interval = p.second;
    try {
        const QJsonValue j = getJson(QStringLiteral("https://api.kraken.com/0/public/OHLC?pair=%1&interval=%2")
                                         .arg(jsv::encodeURIComponent(pair)).arg(interval));
        if (!j["error"].toArray().isEmpty()) throw Error("Kraken: " + joinErrors(j["error"].toArray()));
        if (!j["result"].isObject()) throw Error("Cannot convert undefined or null to object");
        const QJsonObject result = j["result"].toObject();
        QString key;
        for (auto it = result.begin(); it != result.end(); ++it)
            if (it.key() != "last") {
                key = it.key();
                break;
            }
        if (!result.value(key).isArray()) throw Error("Cannot read properties of undefined (reading 'map')");
        Bars out;
        for (const QJsonValue r : result.value(key).toArray()) {
            Bar b;
            b.time = jsv::toNumber(r[0]);
            b.open = jsv::toNumber(r[1]);
            b.high = jsv::toNumber(r[2]);
            b.low = jsv::toNumber(r[3]);
            b.close = jsv::toNumber(r[4]);
            b.volume = jsv::toNumber(r[6]);
            out.push_back(b);
        }
        mark("kraken", true);
        return base == tf ? tail(out, limit) : tail(resample(out, tf), limit);
    } catch (const Error &e) {
        mark("kraken", false, e.message());
        throw;
    }
}

// ---------- Yahoo ----------
bool Providers::yahooAvailable() const
{
    return jsv::nowMs() > m_yahooDownUntil.load();
}

void Providers::setYahooDown()
{
    m_yahooDownUntil = jsv::nowMs() + 10 * 60e3;
}

Bars Providers::yahooCandles(const QString &symbol, const QString &tf, int limit)
{
    if (!yahooAvailable()) throw Error("Yahoo is rate limiting this connection (retrying in a few minutes)");
    static const QVector<QPair<QString, QPair<QString, QString>>> NATIVE = {
        { "1m", { "1m", "7d" } }, { "2m", { "2m", "60d" } }, { "5m", { "5m", "60d" } }, { "15m", { "15m", "60d" } },
        { "30m", { "30m", "60d" } }, { "1h", { "60m", "730d" } }, { "1D", { "1d", "20y" } }, { "1W", { "1wk", "max" } },
        { "1M", { "1mo", "max" } },
    };
    QStringList keys;
    for (const auto &p : NATIVE) keys << p.first;
    const QString base = nativeBase(tf, keys);
    if (base.isEmpty()) throw Error(QStringLiteral("Yahoo can't build a %1 chart").arg(tf));
    QString interval, range;
    for (const auto &p : NATIVE)
        if (p.first == base) {
            interval = p.second.first;
            range = p.second.second;
        }
    // Only the days the bars asked for need (a live poll asks for 5): the full range is years of bars,
    // downloaded and parsed again at every poll. Six calendar days per day of bars covers a stock's 6.5-hour
    // sessions on weekdays (24 / 6.5 × 7 / 5 = 5.2), plus five days for holidays and a first bucket cut short; a
    // full chart load (1500 bars and more) asks more than the native range and keeps it.
    const double maxDays = range.endsWith('d') ? range.chopped(1).toDouble() : range.endsWith('y') ? range.chopped(1).toDouble() * 365
                                                                                                    : 20 * 365;   // "max": kept beyond 20 years
    const double neededDays = std::ceil(double(limit) * double(parseTf(tf).seconds) / 86400 * 6) + 5;
    if (limit > 0 && neededDays < maxDays)
        range = QStringLiteral("%1d").arg(qint64(neededDays));
    QJsonValue j;
    try {
        j = getJson(QStringLiteral("https://query1.finance.yahoo.com/v8/finance/chart/%1?interval=%2&range=%3&includePrePost=false")
                        .arg(jsv::encodeURIComponent(symbol), interval, range),
                    YAHOO);
    } catch (const Error &e) {
        if (e.status() == 429) setYahooDown();
        mark("yahoo", false, e.message());
        throw;
    }
    const Bars out = parseYahoo(j, base);
    mark("yahoo", true);
    return base == tf ? tail(out, limit) : tail(resample(out, tf), limit);
}

Bars Providers::parseYahoo(const QJsonValue &j, const QString &tf)
{
    const QJsonValue r = j["chart"]["result"][0];
    if (!jsv::truthy(r)) {
        const QJsonValue d = j["chart"]["error"]["description"];
        throw Error(jsv::truthy(d) ? jsv::toString(d) : QStringLiteral("Yahoo returned no data for that symbol"));
    }
    const QJsonValue q = jsv::orElse(r["indicators"]["quote"][0], QJsonObject());
    const double off = jsv::truthy(r["meta"]["gmtoffset"]) ? jsv::toNumber(r["meta"]["gmtoffset"]) : 0;
    const bool daily = !isIntraday(tf);
    Bars out;
    const QJsonArray ts = r["timestamp"].toArray();
    const QJsonArray open = q["open"].toArray(), high = q["high"].toArray(), low = q["low"].toArray(),
                     close = q["close"].toArray(), volume = q["volume"].toArray();
    auto val = [](const QJsonArray &a, qsizetype i) { return i < a.size() ? a[i] : QJsonValue(QJsonValue::Undefined); };
    auto numOrNaN = [](const QJsonValue &v) { return jsv::nullish(v) ? NAN : jsv::toNumber(v); };
    for (qsizetype i = 0; i < ts.size(); ++i) {
        if (jsv::nullish(val(close, i)) || jsv::nullish(val(open, i))) continue;
        const double t = jsv::toNumber(ts[i]);
        Bar b;
        b.time = daily ? std::floor((t + off) / 86400) * 86400 : t;
        b.open = numOrNaN(val(open, i));
        b.high = numOrNaN(val(high, i));
        b.low = numOrNaN(val(low, i));
        b.close = numOrNaN(val(close, i));
        const QJsonValue v = val(volume, i);
        b.volume = jsv::truthy(v) ? jsv::toNumber(v) : 0;
        out.push_back(b);
    }
    return dedupe(out);
}

// ---------- Nasdaq (US stocks, ETFs, NDX/COMP) ----------
static double nqNum(const QJsonValue &s)
{
    if (jsv::nullish(s)) return 0;
    QString t = jsv::toString(s);
    static const QRegularExpression re(QStringLiteral("[$,%" QTM_JS_SPACE "]"));
    t.replace(re, QString());
    const double v = jsv::parseFloat(t);
    return std::isfinite(v) ? v : 0;
}

Providers::NasdaqResult Providers::nasdaqQuery(const QString &ticker, const std::function<QString(const QString &)> &pathFn)
{
    QStringList classes;
    {
        QMutexLocker lock(&m_mutex);
        const QString known = m_nqClass.value(ticker);
        if (!known.isEmpty()) classes << known;
    }
    for (const char *c : { "stocks", "etf", "index" })
        if (!classes.contains(c)) classes << c;
    std::optional<Error> lastErr;
    for (const QString &cls : classes) {
        try {
            const QJsonValue j = getJson(pathFn(cls), NQ());
            if (jsv::truthy(j["data"])) {
                {
                    QMutexLocker lock(&m_mutex);
                    m_nqClass.insert(ticker, cls);
                }
                mark("nasdaq", true);
                return { j, cls };
            }
            const QJsonValue msg = j["status"]["bCodeMessage"][0]["errorMessage"];
            lastErr = Error(jsv::truthy(msg) ? jsv::toString(msg) : QStringLiteral("Nasdaq has no data for %1").arg(ticker));
        } catch (const Error &e) {
            lastErr = e;
        }
    }
    if (lastErr->status()) mark("nasdaq", false, lastErr->message());
    else mark("nasdaq", true);
    throw *lastErr;
}

static int nyHour(double ms)
{
    static const QTimeZone ny("America/New_York");
    return QDateTime::fromMSecsSinceEpoch(qint64(ms), ny).time().hour();
}

double Providers::nyWallToUtc(double ms)
{
    const int wallHour = jsv::utcHours(ms);
    for (int h : { 4, 5 }) {
        const double utc = ms + h * 3600e3;
        if (nyHour(utc) == wallHour) return utc;
    }
    return ms + 5 * 3600e3;
}

Providers::Today Providers::nasdaqToday(const QString &ticker)
{
    const auto r = nasdaqQuery(ticker, [&](const QString &cls) {
        return QStringLiteral("https://api.nasdaq.com/api/quote/%1/chart?assetclass=%2").arg(jsv::encodeURIComponent(ticker), cls);
    });
    Today t;
    for (const QJsonValue p : r.j["data"]["chart"].toArray()) {
        const double x = jsv::toNumber(p["x"]);
        const int mins = jsv::utcHours(x) * 60 + jsv::utcMinutes(x);
        if (mins >= 570 && mins < 960) t.pts.append(p);   // 09:30-16:00 New York
    }
    t.volume = nqNum(r.j["data"]["volume"]);
    return t;
}

Bars Providers::nasdaqCandles(const QString &ticker, const QString &tf, int limit)
{
    if (isIntraday(tf)) {
        const Today t = nasdaqToday(ticker);
        Bars minute;
        for (const QJsonValue p : t.pts) {
            const double y = jsv::toNumber(p["y"]);
            Bar b;
            b.time = std::floor(nyWallToUtc(jsv::toNumber(p["x"])) / 1000 / 60) * 60;
            b.open = b.high = b.low = b.close = y;
            b.volume = 0;
            minute.push_back(b);
        }
        return tail(resample(dedupe(minute), tf), limit);
    }
    const QString to = jsv::isoNow().left(10);
    const QString from = jsv::isoString(jsv::nowMs() - 3653 * 86400e3).left(10);   // Nasdaq serves up to ~10 years
    const auto r = nasdaqQuery(ticker, [&](const QString &cls) {
        return QStringLiteral("https://api.nasdaq.com/api/quote/%1/historical?assetclass=%2&fromdate=%3&limit=9999&todate=%4")
            .arg(jsv::encodeURIComponent(ticker), cls, from, to);
    });
    const QJsonArray rows = r.j["data"]["tradesTable"]["rows"].toArray();
    Bars daily;
    for (qsizetype i = rows.size() - 1; i >= 0; --i) {
        const QJsonValue row = rows[i];
        const QStringList mdy = jsv::toString(row["date"]).split('/');
        Bar b;
        b.time = jsv::dateUtc(jsv::toNumber(mdy.value(2, "x")), jsv::toNumber(mdy.value(0, "x")) - 1,
                             jsv::toNumber(mdy.value(1, "x"))) / 1000;
        b.open = nqNum(row["open"]);
        b.high = nqNum(row["high"]);
        b.low = nqNum(row["low"]);
        b.close = nqNum(row["close"]);
        b.volume = nqNum(row["volume"]);
        if (!(b.close > 0)) continue;
        if (!b.open) {
            b.open = b.close;
            b.high = b.high ? b.high : b.close;
            b.low = b.low ? b.low : b.close;
        }
        daily.push_back(b);
    }
    // Today's session isn't in the history table until the close, so build it from the intraday feed.
    try {
        const Today t = nasdaqToday(ticker);
        if (!t.pts.isEmpty()) {
            const double day = std::floor(jsv::toNumber(QJsonValue(t.pts[0])["x"]) / 86400e3) * 86400;
            if (daily.isEmpty() || daily.last().time < day) {
                QVector<double> ys;
                for (const QJsonValue p : t.pts) ys << jsv::toNumber(p["y"]);
                Bar b;
                b.time = day;
                b.open = ys.first();
                b.high = *std::max_element(ys.begin(), ys.end());
                b.low = *std::min_element(ys.begin(), ys.end());
                b.close = ys.last();
                b.volume = t.volume;
                daily.push_back(b);
            }
        }
    } catch (...) {
        // history alone is fine
    }
    return tf == "1D" ? tail(daily, limit) : tail(resample(daily, tf), limit);
}

QJsonArray Providers::nasdaqScreener(const QString &kind)
{
    if (m_opt.offline) return fixture("screener-us.json").toArray();
    return cached<QJsonArray>("nq:screener:" + kind, 60e3, [this, kind]() -> QJsonArray {
        // The full list (with sectors) is last-close data, so overlay live quotes on the 200 biggest.
        const QJsonValue full = cached<QJsonValue>("nq:screener-full:" + kind, 6 * 3600e3, [this, kind] {
            return getJson(QStringLiteral("https://api.nasdaq.com/api/screener/%1?tableonly=true&limit=25&offset=0&download=true").arg(kind), NQ());
        });
        const QJsonValue raw = jsv::orElse(jsv::orElse(full["data"]["rows"], full["data"]["data"]["rows"]), QJsonArray());
        mark("nasdaq", true);
        QVector<QJsonObject> rows;
        static const QRegularExpression badTicker(QStringLiteral("[\\^/]"));
        for (const QJsonValue r : raw.toArray()) {
            const QString ticker = r["symbol"].isString() ? jsv::trim(r["symbol"].toString()) : QString();
            const bool hasTicker = r["symbol"].isString();
            QJsonObject row {
                { "symbol", "US:" + (hasTicker ? ticker : QStringLiteral("undefined")) },
                { "name", jsv::orElse(jsv::orElse(r["name"], r["companyName"]), "") },
                { "price", nqNum(jsv::coalesce(r["lastsale"], r["lastSalePrice"])) },
                { "changePct", nqNum(jsv::coalesce(r["pctchange"], r["percentageChange"])) },
                { "volume", nqNum(r["volume"]) },
                { "marketCap", nqNum(r["marketCap"]) },
                { "sector", jsv::orElse(r["sector"], kind == "etf" ? "ETF" : "") },
                { "industry", jsv::orElse(r["industry"], "") },
                { "country", jsv::orElse(r["country"], "") },
                { "live", false },
            };
            if (hasTicker) row.insert("ticker", ticker);
            if (ticker.isEmpty() || badTicker.match(ticker).hasMatch()) continue;
            rows.push_back(row);
        }
        QVector<int> top(rows.size());
        for (int i = 0; i < rows.size(); ++i) top[i] = i;
        std::stable_sort(top.begin(), top.end(), [&](int x, int y) {
            return rows[y]["marketCap"].toDouble() < rows[x]["marketCap"].toDouble();
        });
        if (top.size() > 200) top.resize(200);
        try {
            QStringList tickers;
            for (int i : top) tickers << rows[i]["ticker"].toString();
            const auto live = nasdaqLiveQuotes(tickers, kind == "etf" ? "etf" : "stocks");
            for (int i : top) {
                const QJsonObject l = live.value(rows[i]["ticker"].toString());
                if (jsv::truthy(l.value("price"))) {
                    rows[i].insert("price", l.value("price"));
                    rows[i].insert("changePct", l.value("changePct"));
                    rows[i].insert("volume", jsv::orElse(l.value("volume"), rows[i]["volume"]));
                    rows[i].insert("live", true);
                }
            }
        } catch (...) {
            // last-close data is still useful
        }
        QJsonArray out;
        for (const auto &r : rows) out.append(r);
        return out;
    });
}

// Live quotes for US symbols, 20 per request.
QHash<QString, QJsonObject> Providers::nasdaqLiveQuotes(const QStringList &tickers, const QString &defaultClass)
{
    QSet<QString> etfs;
    if (defaultClass.isEmpty()) {
        try {
            for (const QJsonValue r : nasdaqScreener("etf")) etfs.insert(jsv::toString(r["ticker"]));
        } catch (...) {
        }
    }
    QVector<QStringList> chunks;
    for (qsizetype i = 0; i < tickers.size(); i += 20) chunks << tickers.mid(i, 20);
    QHash<QString, QString> nqClass;
    {
        QMutexLocker lock(&m_mutex);
        nqClass = m_nqClass;
    }
    const auto results = async::all(int(chunks.size()), [&](int c) {
        QStringList parts;
        for (const QString &t : chunks[c]) {
            const QString cls = !nqClass.value(t).isEmpty() ? nqClass.value(t)
                                : !defaultClass.isEmpty()   ? defaultClass
                                : etfs.contains(t)          ? QStringLiteral("etf")
                                : QStringList { "NDX", "COMP", "RUT", "DJI" }.contains(t) ? QStringLiteral("index")
                                                                                           : QStringLiteral("stocks");
            parts << "symbol=" + jsv::encodeURIComponent(t.toLower() + '|' + cls);
        }
        const QString qs = parts.join('&');
        const QJsonValue j = cached<QJsonValue>("nq:wl:" + qs, 5000, [this, qs] {
            return getJson("https://api.nasdaq.com/api/quote/watchlist?" + qs, NQ());
        });
        QVector<QPair<QString, QJsonObject>> out;
        for (const QJsonValue r : jsv::orElse(j["data"], QJsonArray()).toArray()) {
            QJsonObject q { { "price", nqNum(r["lastSalePrice"]) }, { "changePct", nqNum(r["percentageChange"]) },
                            { "volume", nqNum(r["volume"]) } };
            if (!r["marketStatus"].isUndefined()) q.insert("status", r["marketStatus"]);
            out.push_back({ jsv::toString(r["symbol"]), q });
        }
        return out;
    });
    QHash<QString, QJsonObject> out;
    for (const auto &list : results)
        for (const auto &p : list) out.insert(p.first, p.second);
    return out;
}

// ---------- public API ----------
Providers::ChainResult Providers::tryChain(const Chain &chain)
{
    QStringList errors;
    for (const auto &[provider, fn] : chain) {
        try {
            const Bars candles = fn();
            if (!candles.isEmpty()) return { candles, provider, errors };
            errors << provider + ": no data";
        } catch (const Error &e) {
            errors << provider + ": " + e.message();
        } catch (const std::exception &e) {
            errors << provider + ": " + QString::fromUtf8(e.what());
        }
    }
    throw Error("No source could load this chart. " + errors.join(" | "), 502);
}

struct CryptoParts {
    QString base, quote;
};
static CryptoParts cryptoParts(const QString &ticker)
{
    static const QRegularExpression re(QStringLiteral("^(.+?)(USDT|USDC|FDUSD|USD|BTC|ETH|EUR|BNB)$"));
    const auto m = re.match(ticker);
    return m.hasMatch() ? CryptoParts { m.captured(1), m.captured(2) } : CryptoParts { ticker, "USDT" };
}

CandleResult Providers::spreadCandles(const Resolved &sym, const QString &tf, int limit)
{
    const Spread sp = *parseSpread(sym.id);
    struct Side {
        bool constant = false;
        double value = 0;
        CandleResult r;
    };
    const auto sides = async::all(2, [&](int i) {
        const QString x = i == 0 ? sp.a : sp.b;
        Side s;
        if (isNumStr(x)) {
            s.constant = true;
            s.value = jsv::toNumber(x);
        } else {
            s.r = getCandles(x, tf, limit);
        }
        return s;
    });
    const Side &ra = sides[0], &rb = sides[1];
    const QString op = sp.op;
    auto f = [&](double x, double y) {
        return op == "/" ? x / y : op == "*" ? x * y : op == "+" ? x + y : x - y;
    };
    auto combine = [&](const Bar &ba, bool baConst, const Bar &bb) {
        const double o = f(ba.open, bb.open), c = f(ba.close, bb.close);
        QVector<double> pts;
        for (double v : { o, c, f(ba.high, bb.high), f(ba.low, bb.low) })
            if (std::isfinite(v)) pts << v;
        Bar out;
        out.time = baConst ? bb.time : ba.time;
        out.open = o;
        out.high = pts.isEmpty() ? -INFINITY : *std::max_element(pts.begin(), pts.end());
        out.low = pts.isEmpty() ? INFINITY : *std::min_element(pts.begin(), pts.end());
        out.close = c;
        out.volume = 0;
        return out;
    };
    auto k = [](double v) {
        Bar b;
        b.open = b.high = b.low = b.close = v;
        return b;
    };
    Bars candles;
    if (ra.constant) {
        for (const Bar &x : rb.r.candles) candles << combine(k(ra.value), true, x);
    } else if (rb.constant) {
        for (const Bar &x : ra.r.candles) candles << combine(x, false, k(rb.value));
    } else {
        QHash<double, Bar> byTime;
        for (const Bar &x : rb.r.candles) byTime.insert(x.time, x);
        for (const Bar &x : ra.r.candles)
            if (byTime.contains(x.time)) candles << combine(x, false, byTime.value(x.time));
    }
    Bars finite;
    for (const Bar &b : candles)
        if (std::isfinite(b.open) && std::isfinite(b.close)) finite << b;
    if (finite.isEmpty()) throw Error("The two symbols have no bars at the same times", 422);
    QStringList from;
    if (!ra.constant && !ra.r.provider.isEmpty()) from << ra.r.provider;
    if (!rb.constant && !rb.r.provider.isEmpty()) from << rb.r.provider;
    CandleResult r;
    r.symbol = sym.id;
    r.tf = tf;
    r.provider = from.isEmpty() ? QStringLiteral("constant") : from.join(" + ");
    r.candles = tail(finite, limit);
    r.notes << "Spread built from both symbols' bars at matching times.";
    return r;
}

CandleResult Providers::getCandles(const QString &rawSymbol, const QString &tf, double limitIn)
{
    try {
        parseTf(tf);
    } catch (const Error &e) {
        throw badInput(e.message());
    }
    const double l = std::max(10.0, std::min((std::isnan(limitIn) || limitIn == 0) ? 1500.0 : limitIn, 20000.0));
    const int limit = int(std::trunc(l));
    const Resolved sym = resolve(rawSymbol);
    if (sym.exchange == "SPREAD") return spreadCandles(sym, tf, limit);
    // Your own imported bars (data/imports/<NAME>.csv).
    if (sym.exchange == "CSV") return m_imports.csvCandles(sym.ticker, tf, limit);
    if (m_opt.offline) {
        const Bars bars = barsFromJson(fixture("btc-1d.json").toArray());
        CandleResult r;
        r.symbol = sym.id;
        r.tf = tf;
        r.provider = "fixture";
        r.candles = tf == "1D" ? tail(bars, limit) : tail(resample(bars, tf), limit);
        return r;
    }
    return cached<CandleResult>(QStringLiteral("candles:%1:%2:%3").arg(sym.id, tf, jsNum(l)), ttlFor(tf), [=, this]() -> CandleResult {
        const QString t = sym.ticker;
        QStringList notes;
        Chain chain;
        if (sym.exchange == "BINANCE") {
            const auto [base, quote] = cryptoParts(t);
            const bool usdQuote = QStringList { "USDT", "USDC", "FDUSD", "USD" }.contains(quote);
            chain << ChainStep { "Binance", [=, this] { return binanceCandles(t, tf, limit); } };
            if (usdQuote) {
                chain << ChainStep { "Coinbase", [=, this] { return coinbaseCandles(base + "-USD", tf, limit); } };
                chain << ChainStep { "Kraken", [=, this] { return krakenCandles(krakenAsset(base) + "USD", tf, limit); } };
            }
        } else if (sym.exchange == "COINBASE") {
            chain << ChainStep { "Coinbase", [=, this] { return coinbaseCandles(t, tf, limit); } };
        } else if (sym.exchange == "KRAKEN") {
            chain << ChainStep { "Kraken", [=, this] { return krakenCandles(t, tf, limit); } };
        } else if (sym.exchange == "FX" && metal(t)) {
            const Metal m = *metal(t);
            chain << ChainStep { "Yahoo", [=, this] { return yahooCandles(m.yahoo, tf, limit); } };
            if (!m.token.isEmpty()) {
                chain << ChainStep { "Binance", [=, this] { return binanceCandles(m.token + "USDT", tf, limit); } };
                chain << ChainStep { "Coinbase", [=, this] { return coinbaseCandles(m.token + "-USD", tf, limit); } };
            }
        } else if (sym.exchange == "FX") {
            chain << ChainStep { "Yahoo", [=, this] { return yahooCandles(t + "=X", tf, limit); } };
            chain << ChainStep { "Kraken", [=, this] { return krakenCandles(t, tf, limit); } };
        } else if (sym.exchange == "CME" && future(QString(t).replace(rx("1!$"), QString()))) {
            const QString root = QString(t).replace(rx("1!$"), QString());
            const QString yahoo = future(root)->yahoo;
            chain = m_feeds.futuresChain(root, tf, limit);
            chain << ChainStep { "Yahoo (delayed)", [=, this] { return yahooCandles(yahoo, tf, limit); } };
        } else if (sym.exchange == "US") {
            // Your own keys (data/feeds.json) go first; the public sources stay behind them.
            chain = m_feeds.stockChain(t, tf, limit);
            if (isIntraday(tf)) {
                chain << ChainStep { "Yahoo", [=, this] { return yahooCandles(t, tf, limit); } };
                chain << ChainStep { "Nasdaq", [=, this] { return nasdaqCandles(t, tf, limit); } };
            } else {
                chain << ChainStep { "Nasdaq", [=, this] { return nasdaqCandles(t, tf, limit); } };
                chain << ChainStep { "Yahoo", [=, this] { return yahooCandles(t, tf, limit); } };
            }
        } else {
            chain << ChainStep { "Yahoo", [=, this] { return yahooCandles(t, tf, limit); } };
        }
        const ChainResult res = tryChain(chain);
        // Plain words on the chart; the technical reason stays in /api/health.
        for (const QString &f : res.fallbacks) {
            if (!Feeds::isKeyProvider(f)) continue;
            QString why = f;
            why.replace(rx("^[^:]*:\\s*"), QString());
            const QString first = why.isEmpty() ? QStringLiteral("UNDEFINED") : why.left(1).toUpper();
            notes << QStringLiteral("%1%2, so this chart uses %3.").arg(first, why.mid(1), res.provider);
        }
        QStringList busy, off;   // off: turned off in deepwater's Sources page
        for (const QString &f : res.fallbacks) {
            if (Feeds::isKeyProvider(f)) continue;
            const QString name = jsv::trim(f.section(':', 0, 0));
            QStringList &list = f.endsWith(QLatin1String("is turned off in Sources")) ? off : busy;
            if (!list.contains(name)) list << name;
        }
        if (!off.isEmpty())
            notes << QStringLiteral("%1 %2 turned off in Sources, so this chart uses %3.")
                         .arg(off.join(" and "), off.size() > 1 ? "are" : "is", res.provider);
        if (!busy.isEmpty())
            notes << QStringLiteral("%1 isn't answering right now, so this chart uses %2.").arg(busy.join(" and "), res.provider);
        if (res.provider == "Yahoo (delayed)")
            notes << "Delayed about 10 minutes, because CME charges for live futures data. Your own Polygon futures key in data/feeds.json makes it live.";
        if (sym.exchange == "CME" && res.provider.startsWith("Polygon "))
            notes << QStringLiteral("Front contract %1 from your Polygon key, not joined to older contracts. Whether it's live depends on your Polygon futures plan.")
                         .arg(res.provider.mid(8));
        if (res.provider == "Alpaca (IEX)")
            notes << "Live from your Alpaca key on the free IEX feed: prices are live, volume counts IEX trades only.";
        if (metal(t) && res.provider != "Yahoo")
            notes << QStringLiteral("This chart follows %1, a token backed by real gold, until Yahoo's gold feed is back.")
                         .arg(metal(t)->token.isEmpty() ? QStringLiteral("undefined") : metal(t)->token);
        if (sym.exchange == "US" && res.provider == "Nasdaq" && isIntraday(tf))
            notes << "Free intraday stock data covers today only and has no volume per bar. Older bars and volume appear when Yahoo is back.";
        if (res.provider == "Kraken") notes << "Kraken's free feed shows the last 720 bars.";
        CandleResult r;
        r.symbol = sym.id;
        r.tf = tf;
        r.provider = res.provider;
        r.candles = res.candles;
        r.notes = notes;
        return r;
    });
}

// Latest price and day change for a list of symbols (watchlists, paper trading, heatmaps).
QJsonObject Providers::getQuotes(const QStringList &symbols)
{
    QJsonObject out;
    const auto resolved = async::all(int(symbols.size()), [&](int i) -> std::optional<Resolved> {
        try {
            return resolve(symbols[i]);
        } catch (...) {
            return std::nullopt;
        }
    });
    QStringList binance;
    for (const auto &r : resolved)
        if (r && r->exchange == "BINANCE") binance << r->ticker;
    if (!binance.isEmpty() && !m_opt.offline) {
        try {
            std::sort(binance.begin(), binance.end());
            QJsonArray list;
            for (const auto &b : binance) list.append(b);
            const QString p = "/api/v3/ticker/24hr?symbols=" + jsv::encodeURIComponent(QString::fromUtf8(jsv::stringify(list)));
            const QJsonValue rows = cached<QJsonValue>("binance:24h:" + binance.join(','), 3000, [this, p] { return binanceGet(p); });
            if (!rows.isArray()) throw Error("rows is not iterable");
            for (const QJsonValue r : rows.toArray())
                out.insert("BINANCE:" + jsv::toString(r["symbol"]),
                           QJsonObject { { "price", jsv::toNumber(r["lastPrice"]) },
                                         { "changePct", jsv::toNumber(r["priceChangePercent"]) },
                                         { "volume", jsv::toNumber(r["quoteVolume"]) } });
        } catch (...) {
            // fall through to candles
        }
    }
    QVector<Resolved> us;
    for (const auto &r : resolved)
        if (r && r->exchange == "US") us << *r;
    if (!us.isEmpty() && !m_opt.offline) {
        try {
            QStringList tickers;
            for (const auto &r : us) tickers << r.ticker;
            const auto live = nasdaqLiveQuotes(tickers);
            for (const auto &r : us)
                if (live.contains(r.ticker)) out.insert(r.id, live.value(r.ticker));
        } catch (...) {
            // fall through to candles
        }
    }
    const QJsonObject have = out;
    const auto filled = async::all(int(resolved.size()), [&](int i) -> std::optional<QPair<QString, QJsonObject>> {
        const auto &r = resolved[i];
        if (!r || have.contains(r->id)) return std::nullopt;
        try {
            const Bars candles = getCandles(r->id, "1D", 3).candles;
            if (candles.isEmpty()) throw Error("Cannot read properties of undefined (reading 'close')");
            const Bar &last = candles.last();
            const bool hasPrev = candles.size() >= 2;
            const double prev = hasPrev ? candles[candles.size() - 2].close : 0;
            return QPair<QString, QJsonObject> { r->id, QJsonObject {
                { "price", last.close },
                { "changePct", hasPrev ? ((last.close - prev) / prev) * 100 : 0 },
                { "volume", last.volume },
            } };
        } catch (const Error &e) {
            return QPair<QString, QJsonObject> { r->id, QJsonObject { { "error", e.message() } } };
        }
    });
    for (const auto &f : filled)
        if (f) out.insert(f->first, f->second);
    return out;
}

// Live bid and ask for the tester's spread setting. Only the crypto exchanges publish one for free.
QJsonObject Providers::getSpread(const QString &rawSymbol)
{
    const Resolved sym = resolve(rawSymbol);
    double bid, ask;
    QString source;
    if (sym.exchange == "BINANCE") {
        const QJsonValue r = binanceGet("/api/v3/ticker/bookTicker?symbol=" + jsv::encodeURIComponent(sym.ticker));
        bid = jsv::toNumber(r["bidPrice"]);
        ask = jsv::toNumber(r["askPrice"]);
        source = "Binance";
    } else if (sym.exchange == "COINBASE") {
        const QJsonValue r = getJson(QStringLiteral("https://api.exchange.coinbase.com/products/%1/ticker").arg(jsv::encodeURIComponent(sym.ticker)));
        bid = jsv::toNumber(r["bid"]);
        ask = jsv::toNumber(r["ask"]);
        source = "Coinbase";
    } else if (sym.exchange == "KRAKEN") {
        const QJsonValue j = getJson("https://api.kraken.com/0/public/Ticker?pair=" + jsv::encodeURIComponent(sym.ticker));
        if (!j["error"].toArray().isEmpty()) throw Error("Kraken: " + joinErrors(j["error"].toArray()));
        const QJsonObject result = j["result"].toObject();
        if (result.isEmpty()) throw Error("Cannot read properties of undefined (reading 'b')");
        const QJsonValue r = *result.begin();
        bid = jsv::toNumber(r["b"][0]);
        ask = jsv::toNumber(r["a"][0]);
        source = "Kraken";
    } else {
        throw badInput("A live spread is only published for crypto (Binance, Coinbase and Kraken). Enter one by hand for this market.");
    }
    if (!(bid > 0 && ask >= bid)) throw Error(source + " sent no usable bid and ask");
    return { { "symbol", sym.id }, { "bid", bid }, { "ask", ask }, { "spreadPct", (200 * (ask - bid)) / (ask + bid) }, { "source", source } };
}

static QJsonObject hit(const QString &symbol, const QJsonValue &name, const QJsonValue &type, const QJsonValue &exchange)
{
    QJsonObject o { { "symbol", symbol } };
    if (!name.isUndefined()) o.insert("name", name);
    if (!type.isUndefined()) o.insert("type", type);
    if (!exchange.isUndefined()) o.insert("exchange", exchange);
    return o;
}

QJsonArray Providers::search(const QString &q)
{
    const QString query = jsv::trim(q).toUpper();
    if (query.isEmpty()) return {};
    QVector<QJsonObject> results;
    static const QLocale en(QLocale::English, QLocale::UnitedStates);
    for (const auto &fv : m_imports.listImports()) {
        const QJsonObject f = fv.toObject();
        if (f.contains("error")) continue;
        QString needle = query;
        needle.replace(rx("^CSV:"), QString());
        if (f.value("name").toString().contains(needle) || query == "CSV")
            results << hit(f.value("symbol").toString(),
                           QStringLiteral("Your file, %1 %2 bars").arg(en.toString(qlonglong(f.value("bars").toDouble())), f.value("tf").toString()),
                           "your data", "CSV");
    }
    QString root = query;
    root.replace(rx("^CME(_MINI)?:"), QString());
    root.replace(rx("1!?$"), QString());
    for (const auto &[t, f] : futuresList())
        if ((root.size() >= 2 && t.startsWith(root)) || (query.size() >= 4 && f.name.toUpper().contains(query)))
            results << hit("CME:" + t + "1!", f.name, "futures", "CME");
    QString noSlash = query;
    if (noSlash.indexOf('/') >= 0) noSlash.remove(noSlash.indexOf('/'), 1);
    for (const auto &[t, m] : metals())
        if (query.size() >= 2 && (t.startsWith(noSlash) || m.word.startsWith(query)))
            results << hit("FX:" + t, m.name, "commodity", "FX");
    const SymbolMapPtr bin = binanceSymbolMap();
    QStringList hits;
    for (const QString &t : bin->order)
        if (t.startsWith(query) || bin->map.value(t).base == query) hits << t;
    std::stable_sort(hits.begin(), hits.end(), [&](const QString &a, const QString &b) {
        const int ka = bin->map.value(a).quote == "USDT" ? -1 : 0, kb = bin->map.value(b).quote == "USDT" ? -1 : 0;
        if (ka != kb) return ka < kb;
        return a.size() < b.size();
    });
    hits = hits.mid(0, 8);
    QVector<QJsonObject> cryptoHits;
    for (const QString &t : hits) {
        const BinSym s = bin->map.value(t);
        cryptoHits << hit("BINANCE:" + t, s.base + " / " + s.quote, "crypto", "Binance");
    }
    // The same coin on Coinbase's and Kraken's own feeds, the way TradingView lists one row per
    // exchange, straight after the main Binance pair.
    QVector<QJsonObject> feeds;
    QStringList bases;
    for (const auto &h : cryptoHits) {
        const QString b = h.value("name").toString().section(" / ", 0, 0);
        if (!bases.contains(b)) bases << b;
    }
    bases = bases.mid(0, 3);
    if (!bases.isEmpty() && !m_opt.offline) {
        auto cbTask = async::spawn([this] { return coinbaseProducts(); });
        auto krTask = async::spawn([this] { return krakenPairs(); });
        const auto cb = cbTask.get();
        const auto kr = krTask.get();
        for (const QString &b : bases) {
            if (cb->contains(b + "-USD")) feeds << hit("COINBASE:" + b + "-USD", b + " / USD", "crypto", "Coinbase");
            if (kr->contains(b + "USD")) feeds << hit("KRAKEN:" + kr->value(b + "USD"), b + " / USD", "crypto", "Kraken");
        }
    }
    if (!cryptoHits.isEmpty()) results << cryptoHits.first();
    results << feeds;
    results << cryptoHits.mid(1);
    for (const QString &a : FIAT)
        for (const QString &b : FIAT)
            if (a != b && (a + b).startsWith(noSlash)) results << hit("FX:" + a + b, a + " / " + b, "forex", "FX");
    if (!m_opt.offline) {
        auto nasdaq = async::spawn([this, q]() -> QVector<QJsonObject> {
            try {
                const QJsonValue j = getJson("https://api.nasdaq.com/api/autocomplete/slookup/10?search=" + jsv::encodeURIComponent(q), NQ());
                QVector<QJsonObject> out;
                for (const QJsonValue r : jsv::orElse(j["data"], QJsonArray()).toArray())
                    out << hit("US:" + jsv::toString(r["symbol"]), r["name"],
                               jsv::toString(jsv::orElse(jsv::orElse(r["asset"], r["subCategory"]), "stock")).toLower(),
                               jsv::orElse(r["exchange"], "US"));
                return out;
            } catch (...) {
                return {};
            }
        });
        const bool yahooOk = yahooAvailable();
        auto yahoo = async::spawn([this, q, yahooOk]() -> QVector<QJsonObject> {
            if (!yahooOk) return {};
            try {
                const QJsonValue j = getJson(QStringLiteral("https://query1.finance.yahoo.com/v1/finance/search?q=%1&quotesCount=8&newsCount=0")
                                                 .arg(jsv::encodeURIComponent(q)),
                                             YAHOO);
                QVector<QJsonObject> out;
                static const QRegularExpression plain(QStringLiteral("^[A-Z]+$"));
                for (const QJsonValue r : jsv::orElse(j["quotes"], QJsonArray()).toArray()) {
                    if (!jsv::truthy(r["symbol"]) || plain.match(jsv::toString(r["symbol"])).hasMatch()
                        || r["quoteType"].toString() == "CRYPTOCURRENCY")
                        continue;
                    out << hit("YAHOO:" + jsv::toString(r["symbol"]),
                               jsv::orElse(jsv::orElse(r["shortname"], r["longname"]), r["symbol"]),
                               jsv::toString(jsv::orElse(r["quoteType"], "")).toLower(), jsv::orElse(r["exchDisp"], "Yahoo"));
                }
                return out;
            } catch (const Error &e) {
                if (e.status() == 429) setYahooDown();
                return {};
            }
        });
        results << nasdaq.get();
        results << yahoo.get();
    }
    QSet<QString> seen;
    QJsonArray out;
    for (const auto &r : results) {
        const QString s = r.value("symbol").toString();
        if (seen.contains(s)) continue;
        seen.insert(s);
        if (out.size() < 30) out.append(r);
    }
    return out;
}

// Browsing lists for the markets search() can't enumerate: Yahoo symbols (charted through Yahoo only).
struct Listed {
    QString symbol, name, group;
};
static const QVector<Listed> &yahooFutures()
{
    static const QVector<Listed> l = {
        { "YAHOO:CL=F", "Crude Oil WTI", "Energy" },
        { "YAHOO:BZ=F", "Brent Crude Oil", "Energy" },
        { "YAHOO:NG=F", "Natural Gas", "Energy" },
        { "YAHOO:RB=F", "RBOB Gasoline", "Energy" },
        { "YAHOO:HO=F", "Heating Oil", "Energy" },
        { "YAHOO:GC=F", "Gold", "Metals" },
        { "YAHOO:SI=F", "Silver", "Metals" },
        { "YAHOO:HG=F", "Copper", "Metals" },
        { "YAHOO:PL=F", "Platinum", "Metals" },
        { "YAHOO:PA=F", "Palladium", "Metals" },
        { "YAHOO:ZC=F", "Corn", "Agriculture" },
        { "YAHOO:ZW=F", "Wheat", "Agriculture" },
        { "YAHOO:ZS=F", "Soybeans", "Agriculture" },
        { "YAHOO:LE=F", "Live Cattle", "Agriculture" },
        { "YAHOO:HE=F", "Lean Hogs", "Agriculture" },
        { "YAHOO:KC=F", "Coffee", "Softs" },
        { "YAHOO:CC=F", "Cocoa", "Softs" },
        { "YAHOO:SB=F", "Sugar", "Softs" },
        { "YAHOO:CT=F", "Cotton", "Softs" },
        { "YAHOO:ZN=F", "10-Year T-Note", "Rates" },
        { "YAHOO:ZB=F", "30-Year T-Bond", "Rates" },
        { "YAHOO:6E=F", "Euro FX", "Currencies" },
        { "YAHOO:6J=F", "Japanese Yen", "Currencies" },
        { "YAHOO:DX-Y.NYB", "US Dollar Index", "Currencies" },
    };
    return l;
}
static const QVector<Listed> &yahooIndices()
{
    static const QVector<Listed> l = {
        { "YAHOO:^GSPC", "S&P 500", "Americas" },
        { "YAHOO:^DJI", "Dow Jones Industrial Average", "Americas" },
        { "YAHOO:^IXIC", "Nasdaq Composite", "Americas" },
        { "YAHOO:^NDX", "Nasdaq 100", "Americas" },
        { "YAHOO:^RUT", "Russell 2000", "Americas" },
        { "YAHOO:^VIX", "CBOE Volatility Index", "Americas" },
        { "YAHOO:^GSPTSE", "S&P/TSX Composite", "Americas" },
        { "YAHOO:^BVSP", "Bovespa", "Americas" },
        { "YAHOO:^MXX", "IPC Mexico", "Americas" },
        { "YAHOO:^STOXX50E", "Euro Stoxx 50", "Europe" },
        { "YAHOO:^FTSE", "FTSE 100", "Europe" },
        { "YAHOO:^GDAXI", "DAX", "Europe" },
        { "YAHOO:^FCHI", "CAC 40", "Europe" },
        { "YAHOO:^IBEX", "IBEX 35", "Europe" },
        { "YAHOO:FTSEMIB.MI", "FTSE MIB", "Europe" },
        { "YAHOO:^AEX", "AEX", "Europe" },
        { "YAHOO:^SSMI", "Swiss Market Index", "Europe" },
        { "YAHOO:^N225", "Nikkei 225", "Asia-Pacific" },
        { "YAHOO:^HSI", "Hang Seng", "Asia-Pacific" },
        { "YAHOO:000001.SS", "SSE Composite", "Asia-Pacific" },
        { "YAHOO:^KS11", "KOSPI", "Asia-Pacific" },
        { "YAHOO:^TWII", "Taiwan Weighted", "Asia-Pacific" },
        { "YAHOO:^BSESN", "BSE Sensex", "Asia-Pacific" },
        { "YAHOO:^NSEI", "Nifty 50", "Asia-Pacific" },
        { "YAHOO:^AXJO", "S&P/ASX 200", "Asia-Pacific" },
        { "YAHOO:^STI", "Straits Times Index", "Asia-Pacific" },
    };
    return l;
}

// Rows like search() hits plus "group" (quote currency, sector, kind or region), the list's filter.
QJsonArray Providers::browse(const QString &market)
{
    QJsonArray out;
    auto add = [&](const QString &symbol, const QString &name, const QString &type, const QString &exchange, const QString &group) {
        QJsonObject o = hit(symbol, name, type, exchange);
        o.insert("group", group);
        out.append(o);
    };
    if (market == "crypto") {
        const SymbolMapPtr bin = binanceSymbolMap();
        if (bin->size() == 0) {
            // Binance blocked: the screener's fallback (Kraken's USD pairs)
            for (const QJsonValue r : cryptoScreener())
                add(r["symbol"].toString(), r["name"].toString() + " / USD", "crypto", "Kraken", "USD");
            return out;
        }
        QHash<QString, double> vol;
        try {
            vol = cached<QHash<QString, double>>("browse:crypto-volume", 60e3, [this] {
                QHash<QString, double> v;
                for (const QJsonValue r : binanceGet("/api/v3/ticker/24hr").toArray())
                    v.insert(jsv::toString(r["symbol"]), jsv::toNumber(r["quoteVolume"]));
                return v;
            });
        } catch (...) {
            // no volumes: listed in Binance's order
        }
        // The usual quotes first; volumes are in quote units, so they only compare within one quote.
        static const QStringList QUOTES = { "USDT", "USDC", "FDUSD", "BTC", "ETH", "BNB", "EUR", "TRY", "BRL", "JPY" };
        static const QRegularExpression lev(QStringLiteral("(UP|DOWN|BULL|BEAR)(USDT|BUSD)$"));
        QStringList tickers;
        for (const QString &t : bin->order)
            if (!lev.match(t).hasMatch()) tickers << t;
        auto rank = [&](const QString &quote) {
            const int i = int(QUOTES.indexOf(quote));
            return i < 0 ? int(QUOTES.size()) : i;
        };
        std::stable_sort(tickers.begin(), tickers.end(), [&](const QString &a, const QString &b) {
            const BinSym sa = bin->map.value(a), sb = bin->map.value(b);
            if (rank(sa.quote) != rank(sb.quote)) return rank(sa.quote) < rank(sb.quote);
            if (sa.quote != sb.quote) return sa.quote < sb.quote;
            return vol.value(a) > vol.value(b);
        });
        for (const QString &t : tickers) {
            const BinSym s = bin->map.value(t);
            add("BINANCE:" + t, s.base + " / " + s.quote, "crypto", "Binance", s.quote);
        }
        return out;
    }
    if (market == "stocks" || market == "etf") {
        QVector<QJsonObject> rows;
        for (const QJsonValue r : nasdaqScreener(market)) rows << r.toObject();
        std::stable_sort(rows.begin(), rows.end(), [](const QJsonObject &a, const QJsonObject &b) {
            return a.value("marketCap").toDouble() > b.value("marketCap").toDouble();
        });
        // "Apple Inc. Common Stock" -> "Apple Inc.": every row says it, it only hides the name.
        static const QRegularExpression share(QStringLiteral("\\s+(Common Stock|Common Shares|Ordinary Shares)$"));
        for (const auto &r : rows) {
            const QString sector = r.value("sector").toString();
            const QString name = r.value("name").toString().replace(share, QString());
            if (market == "etf") add(r.value("symbol").toString(), name, "etf", "US", QString());
            else add(r.value("symbol").toString(), name, "stock", "US", sector.isEmpty() ? QStringLiteral("Other") : sector);
        }
        return out;
    }
    if (market == "forex") {
        for (const QString &a : FIAT)
            for (const QString &b : FIAT)
                if (a != b) add("FX:" + a + b, a + " / " + b, "forex", "FX", a == "USD" || b == "USD" ? "Majors" : "Crosses");
        for (const auto &[t, m] : metals()) add("FX:" + t, m.name, "commodity", "FX", "Metals");
        return out;
    }
    if (market == "futures") {
        for (const auto &[t, f] : futuresList()) add("CME:" + t + "1!", f.name, "futures", "CME", "Equity index");
        for (const Listed &l : yahooFutures()) add(l.symbol, l.name, "futures", "Yahoo", l.group);
        return out;
    }
    if (market == "indices") {
        for (const Listed &l : yahooIndices()) add(l.symbol, l.name, "index", "Yahoo", l.group);
        return out;
    }
    throw badInput(QStringLiteral("Unknown market: %1").arg(market));
}

QJsonArray Providers::cryptoScreener()
{
    if (m_opt.offline) return fixture("screener-crypto.json").toArray();
    return cached<QJsonArray>("screener:crypto", 20e3, [this]() -> QJsonArray {
        try {
            const SymbolMapPtr bin = binanceSymbolMap();
            const QJsonValue rows = binanceGet("/api/v3/ticker/24hr");
            if (!rows.isArray()) throw Error("rows.filter is not a function");
            static const QRegularExpression lev(QStringLiteral("(UP|DOWN|BULL|BEAR)USDT$"));
            QJsonArray out;
            for (const QJsonValue r : rows.toArray()) {
                const QString s = jsv::toString(r["symbol"]);
                if (!(bin->has(s) && bin->map.value(s).quote == "USDT") || lev.match(s).hasMatch()
                    || !(jsv::toNumber(r["quoteVolume"]) > 0))
                    continue;
                out.append(QJsonObject {
                    { "ticker", s },
                    { "symbol", "BINANCE:" + s },
                    { "name", bin->map.value(s).base },
                    { "price", jsv::toNumber(r["lastPrice"]) },
                    { "changePct", jsv::toNumber(r["priceChangePercent"]) },
                    { "volume", jsv::toNumber(r["quoteVolume"]) },
                    { "high", jsv::toNumber(r["highPrice"]) },
                    { "low", jsv::toNumber(r["lowPrice"]) },
                    { "sector", "Crypto" },
                });
            }
            return out;
        } catch (...) {
            // Binance blocked: Kraken's ticker (change measured from today's UTC open)
            QByteArray raw;
            const QJsonValue j = getJson("https://api.kraken.com/0/public/Ticker", {}, &raw);
            const QJsonObject result = jsv::orElse(j["result"], QJsonObject()).toObject();
            static const QRegularExpression usd(QStringLiteral("USD$")), skip(QStringLiteral("USDT?USD$|^Z|EURUSD|GBPUSD")),
                xPrefix(QStringLiteral("^X(?=[A-Z]{3}$)"));
            QJsonArray out;
            for (const QString &k : jsv::keyOrder(raw, { "result" })) {
                if (!usd.match(k).hasMatch() || skip.match(k).hasMatch()) continue;
                const QJsonValue v = result.value(k);
                QString name = k;
                name.replace(usd, QString());
                name.replace(xPrefix, QString());
                const double c = jsv::toNumber(v["c"][0]), o = jsv::toNumber(v["o"]);
                out.append(QJsonObject {
                    { "ticker", k },
                    { "symbol", "KRAKEN:" + k },
                    { "name", name },
                    { "price", c },
                    { "changePct", ((c - o) / o) * 100 },
                    { "volume", jsv::toNumber(v["v"][1]) * jsv::toNumber(v["p"][1]) },
                    { "sector", "Crypto" },
                });
            }
            return out;
        }
    });
}

QJsonArray Providers::economicCalendar()
{
    if (m_opt.offline) return {};
    return cached<QJsonArray>("calendar", 30 * 60e3, [this]() -> QJsonArray {
        const QJsonValue rows = getJson("https://nfs.faireconomy.media/ff_calendar_thisweek.json");
        mark("calendar", true);
        if (!rows.isArray()) throw Error("rows.map is not a function");
        QJsonArray out;
        for (const QJsonValue r : rows.toArray()) {
            QJsonObject o;
            for (const char *k : { "title", "country", "date", "impact", "forecast", "previous" })
                if (!r[k].isUndefined()) o.insert(k, r[k]);
            out.append(o);
        }
        return out;
    });
}

QJsonObject Providers::probe(const QString &id)
{
    std::function<void()> check;
    if (id == "binance") check = [this] { binanceGet("/api/v3/ping"); };
    else if (id == "coinbase") check = [this] { getJson("https://api.exchange.coinbase.com/products/BTC-USD/ticker"); };
    else if (id == "kraken") check = [this] { getJson("https://api.kraken.com/0/public/Time"); };
    else if (id == "nasdaq") check = [this] { getJson("https://api.nasdaq.com/api/quote/AAPL/info?assetclass=stocks", NQ()); };
    else if (id == "yahoo") check = [this] { getJson("https://query1.finance.yahoo.com/v8/finance/chart/SPY?interval=1d&range=5d", YAHOO); };
    else if (id == "calendar") check = [this] { getJson("https://nfs.faireconomy.media/ff_calendar_thisweek.json"); };
    else if (id == "cryptonews") check = [] {
        http::FetchOptions o;
        o.headers << qMakePair(QByteArray("User-Agent"), QByteArray("Mozilla/5.0 QuantTerminal"));
        o.timeoutMs = 10000;
        for (const char *url : { "https://cointelegraph.com/rss", "https://decrypt.co/feed" }) {
            const auto res = http::fetch(QString::fromLatin1(url), o);
            if (!res.ok()) throw Error(QStringLiteral("%1 returned HTTP %2").arg(QUrl(QString::fromLatin1(url)).host()).arg(res.status));
        }
    };
    else {
        const QString wanted = id == "alpaca" ? "Alpaca" : id == "polygon" ? "Polygon" : QString();
        for (const auto &p : m_feeds.probes())
            if (!wanted.isEmpty() && p.first.startsWith(wanted)) check = p.second;
        if (!check) return { { "ok", false }, { "error", wanted.isEmpty() ? "Unknown source" : "No key saved" } };
    }
    if (isDisabled(id)) return { { "ok", false }, { "error", QStringLiteral("%1 is turned off in Sources").arg(sourceName(id)) } };
    const double t = jsv::nowMs();
    try {
        check();
        return { { "ok", true }, { "ms", jsv::nowMs() - t } };
    } catch (const Error &e) {
        return { { "ok", false }, { "error", e.message() } };
    } catch (const std::exception &e) {
        return { { "ok", false }, { "error", QString::fromUtf8(e.what()) } };
    }
}

QJsonObject Providers::probeAll()
{
    QVector<QPair<QString, std::function<void()>>> checks = {
        { "Binance (crypto)", [this] { binanceGet("/api/v3/ping"); } },
        { "Coinbase (crypto fallback)", [this] { getJson("https://api.exchange.coinbase.com/products/BTC-USD/ticker"); } },
        { "Kraken (crypto + forex)", [this] { getJson("https://api.kraken.com/0/public/Time"); } },
        { "Nasdaq (US stocks)", [this] { getJson("https://api.nasdaq.com/api/quote/AAPL/info?assetclass=stocks", NQ()); } },
        { "Yahoo (global, intraday stocks)", [this] { getJson("https://query1.finance.yahoo.com/v8/finance/chart/SPY?interval=1d&range=5d", YAHOO); } },
        { "Economic calendar", [this] { getJson("https://nfs.faireconomy.media/ff_calendar_thisweek.json"); } },
    };
    checks += m_feeds.probes();
    const auto results = async::all(int(checks.size()), [&](int i) {
        const double t = jsv::nowMs();
        try {
            checks[i].second();
            return QJsonObject { { "ok", true }, { "ms", jsv::nowMs() - t } };
        } catch (const Error &e) {
            return QJsonObject { { "ok", false }, { "error", e.message() } };
        } catch (const std::exception &e) {
            return QJsonObject { { "ok", false }, { "error", QString::fromUtf8(e.what()) } };
        }
    });
    QJsonObject out;
    for (int i = 0; i < checks.size(); ++i) out.insert(checks[i].first, results[i]);
    return out;
}

} // namespace qtm
