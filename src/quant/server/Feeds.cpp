#include "Feeds.h"

#include "../core/Error.h"
#include "../core/Series.h"
#include "Http.h"
#include "Json.h"
#include "Store.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QJsonArray>
#include <QRegularExpression>
#include <QTimeZone>
#include <QUrl>

#include <algorithm>
#include <cmath>

namespace qtm {

static const QString ALPACA = QStringLiteral("https://data.alpaca.markets");
static const QString POLYGON = QStringLiteral("https://api.massive.com");

// application/x-www-form-urlencoded, as URLSearchParams writes it.
static QString formEncode(const QString &s)
{
    QString out;
    for (const uchar c : s.toUtf8()) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '*'
            || c == '-' || c == '.' || c == '_')
            out += QChar(c);
        else if (c == ' ')
            out += '+';
        else
            out += QStringLiteral("%%1").arg(int(c), 2, 16, QChar('0')).toUpper();
    }
    return out;
}

static QString query(const QVector<QPair<QString, QString>> &params)
{
    QStringList parts;
    for (const auto &p : params)
        parts << formEncode(p.first) + '=' + formEncode(p.second);
    return parts.join('&');
}

Feeds::Feeds(const QString &dataDir, int timeoutMs)
    : m_file(QDir(dataDir).filePath("feeds.json")), m_timeoutMs(timeoutMs)
{
}

// Re-read when the file changes, so a key you add works without a restart.
QJsonObject Feeds::feedConfig()
{
    const double mtime = mtimeMs(m_file);
    if (mtime < 0)
        return {};
    QMutexLocker lock(&m_mutex);
    if (mtime != m_memoMtime) {
        QJsonObject value;
        QFile f(m_file);
        bool ok = false;
        QJsonValue v;
        if (f.open(QIODevice::ReadOnly)) v = jsv::parse(f.readAll(), &ok);
        if (!ok)
            qWarning("data/feeds.json isn't valid JSON, so your own keys are off");
        else
            value = v.toObject();
        m_memoMtime = mtime;
        m_memo = value;
    }
    return m_memo;
}

void Feeds::setDisabled(const QSet<QString> &ids)
{
    QMutexLocker lock(&m_mutex);
    m_disabled = ids;
    m_resting.clear();   // a key turned back on is tried at once
}

bool Feeds::saveFeedConfig(const QJsonObject &config)
{
    QDir().mkpath(QFileInfo(m_file).absolutePath());
    QSaveFile f(m_file);
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(QJsonDocument(config).toJson(QJsonDocument::Indented));
    if (!f.commit()) return false;
    QFile::setPermissions(m_file, QFile::ReadOwner | QFile::WriteOwner);
    QMutexLocker lock(&m_mutex);
    m_memoMtime = -1;   // re-read even if the file time didn't move
    m_resting.clear();
    return true;
}

QJsonObject Feeds::alpacaCfg()
{
    {
        QMutexLocker lock(&m_mutex);
        if (m_disabled.contains("alpaca")) return {};
    }
    const QJsonValue a = feedConfig().value("alpaca");
    return jsv::truthy(a["keyId"]) && jsv::truthy(a["secretKey"]) ? a.toObject() : QJsonObject();
}

QJsonObject Feeds::polygonCfg()
{
    {
        QMutexLocker lock(&m_mutex);
        if (m_disabled.contains("polygon")) return {};
    }
    const QJsonObject c = feedConfig();
    const QJsonValue p = jsv::orElse(c.value("polygon"), c.value("massive"));
    return jsv::truthy(p["apiKey"]) ? p.toObject() : QJsonObject();
}

QStringList Feeds::configured()
{
    QStringList out;
    if (!alpacaCfg().isEmpty()) out << "alpaca";
    if (!polygonCfg().isEmpty()) out << "polygon";
    return out;
}

QString Feeds::alpacaName()
{
    return QStringLiteral("Alpaca (%1)").arg(jsv::toString(jsv::orElse(alpacaCfg().value("feed"), "iex")).toUpper());
}

bool Feeds::isKeyProvider(const QString &name)
{
    static const QRegularExpression re(QStringLiteral("^(Alpaca|Polygon)\\b"));
    return re.match(name).hasMatch();
}

// ---------- requests ----------
// A provider that rate limits or refuses the key is skipped for a minute, so every chart on screen
// doesn't hit it again; the public sources carry the charts meanwhile.
QJsonValue Feeds::getFeed(const QString &name, const QString &url, const QList<QPair<QByteArray, QByteArray>> &headers)
{
    {
        QMutexLocker lock(&m_mutex);
        const auto it = m_resting.find(name);
        if (it != m_resting.end() && it->until > jsv::nowMs())
            throw Error(it->why, it->status, true);
    }
    http::FetchOptions o;
    o.headers << qMakePair(QByteArray("Accept"), QByteArray("application/json"));
    o.headers += headers;
    o.timeoutMs = m_timeoutMs;
    http::FetchResponse res;
    try {
        res = http::fetch(url, o);
    } catch (const http::FetchError &) {
        throw Error(QStringLiteral("%1 couldn't be reached").arg(name), 0, true);
    }
    if (!res.ok()) {
        // Plain words for the chart; the provider's own message stays out of it.
        const QString why = res.status == 401 ? QStringLiteral("%1 refused your key").arg(name)
                            : res.status == 403 ? QStringLiteral("your %1 plan doesn't include this").arg(name)
                            : res.status == 429 ? QStringLiteral("%1 is rate limiting your key").arg(name)
                                                : QStringLiteral("%1 returned an error").arg(name);
        if (res.status == 401 || res.status == 429) {
            QMutexLocker lock(&m_mutex);
            m_resting.insert(name, Rest { jsv::nowMs() + 60e3, why, res.status });
        }
        throw Error(why, res.status, true);
    }
    return res.json();
}

// ---------- shared ----------
// New York's UTC offset for each day. QTimeZone does the work Intl.DateTimeFormat did.
static int nyOffsetHours(double sec)
{
    static QMutex mutex;
    static QHash<qint64, int> cache;
    const qint64 day = qint64(std::floor(sec / 86400));
    QMutexLocker lock(&mutex);
    auto it = cache.find(day);
    if (it == cache.end()) {
        static const QTimeZone ny("America/New_York");
        const QDateTime at = QDateTime::fromSecsSinceEpoch(day * 86400 + 12 * 3600, ny);
        it = cache.insert(day, 12 - at.time().hour());
    }
    return *it;
}

// 09:30 to 16:00 New York, the same session the free sources chart.
bool Feeds::inUsSession(double sec)
{
    const double local = std::fmod(std::fmod(sec - nyOffsetHours(sec) * 3600, 86400) + 86400, 86400);
    return local >= 34200 && local < 57600;
}

// How far back to ask for `want` bars of `base`, with room for weekends, holidays and a stock's
// 6.5-hour day.
static QString startFor(const QString &base, double want, double earliest)
{
    const double sec = double(parseTf(base).seconds);
    const double tradingDays = isIntraday(base) ? want / (23400 / sec) : want;
    const double days = std::ceil(tradingDays * 1.5) + 5;
    const double t = std::max(jsv::nowMs() - days * 86400e3, earliest);
    return jsv::isoString(t).left(10);
}

// Daily bars carry the trading date as UTC midnight, like every other source in the app.
static double dayOf(double sec)
{
    return std::floor(sec / 86400) * 86400;
}

static Bars finish(Bars bars, const QString &base, const QString &tf, int limit)
{
    std::stable_sort(bars.begin(), bars.end(), [](const Bar &a, const Bar &b) { return a.time < b.time; });
    Bars out;
    for (const Bar &b : bars)
        if (out.isEmpty() || b.time > out.last().time) out.push_back(b);
    if (base != tf) out = resample(out, tf);
    return out.mid(std::max<qsizetype>(0, out.size() - limit));
}

static double numOr0(const QJsonValue &v)
{
    // b.v || 0
    return jsv::truthy(v) ? jsv::toNumber(v) : 0;
}

// ---------- Alpaca (US stocks) ----------
static const QHash<QString, QString> &alpacaTf()
{
    static const QHash<QString, QString> m = {
        { "1m", "1Min" }, { "5m", "5Min" }, { "15m", "15Min" }, { "30m", "30Min" }, { "1D", "1Day" },
    };
    return m;
}

Bars Feeds::alpacaCandles(const QString &ticker, const QString &tf, int limit)
{
    const QJsonObject k = alpacaCfg();
    if (k.isEmpty()) throw Error("No Alpaca key in data/feeds.json");
    const QString base = nativeBase(tf, { "1m", "5m", "15m", "30m", "1D" });
    if (base.isEmpty()) throw Error(QStringLiteral("Alpaca can't build a %1 chart").arg(tf));
    const bool intraday = isIntraday(base);
    const double ratio = std::max(1.0, jsv::round(double(parseTf(tf).seconds) / double(parseTf(base).seconds)));
    const double want = std::min(limit * ratio + ratio, 60000.0);
    const QList<QPair<QByteArray, QByteArray>> headers = {
        { "APCA-API-KEY-ID", jsv::toString(k.value("keyId")).toUtf8() },
        { "APCA-API-SECRET-KEY", jsv::toString(k.value("secretKey")).toUtf8() },
    };
    Bars out;
    QString token;
    for (int page = 0; page < 12 && out.size() < want; ++page) {
        QVector<QPair<QString, QString>> q = {
            { "symbols", ticker },
            { "timeframe", alpacaTf().value(base) },
            { "start", startFor(base, want, jsv::dateUtc(2016, 0, 1)) },
            { "limit", "10000" },
            { "feed", jsv::toString(jsv::orElse(k.value("feed"), "iex")) },
            { "adjustment", "split" },
            { "sort", "desc" },
        };
        if (!token.isEmpty()) q.push_back({ "page_token", token });
        const QJsonValue j = getFeed("Alpaca", ALPACA + "/v2/stocks/bars?" + query(q), headers);
        for (const QJsonValue &b : j["bars"][ticker].toArray()) {
            const double t = std::floor(jsv::dateParse(jsv::toString(b["t"])) / 1000);
            if (intraday && !inUsSession(t)) continue;
            Bar bar;
            bar.time = intraday ? t : dayOf(t);
            bar.open = jsv::toNumber(b["o"]);
            bar.high = jsv::toNumber(b["h"]);
            bar.low = jsv::toNumber(b["l"]);
            bar.close = jsv::toNumber(b["c"]);
            bar.volume = numOr0(b["v"]);
            out.push_back(bar);
        }
        const QJsonValue next = j["next_page_token"];
        token = jsv::truthy(next) ? jsv::toString(next) : QString();
        if (token.isEmpty()) break;
    }
    return finish(out, base, tf, limit);
}

// ---------- Polygon / Massive (US stocks) ----------
static QString urlHost(const QUrl &u)
{
    const int def = u.scheme() == "https" ? 443 : u.scheme() == "http" ? 80 : -1;
    QString h = u.host().toLower();
    if (u.port() != -1 && u.port() != def) h += ':' + QString::number(u.port());
    return h;
}

void Feeds::polygonPages(const QString &firstUrl, int want, const std::function<bool(const QJsonObject &)> &eachRow)
{
    const QList<QPair<QByteArray, QByteArray>> headers = {
        { "Authorization", ("Bearer " + jsv::toString(polygonCfg().value("apiKey"))).toUtf8() },
    };
    QString url = firstUrl;
    int n = 0;
    for (int page = 0; !url.isEmpty() && page < 6 && n < want; ++page) {
        const QJsonValue j = getFeed("Polygon", url, headers);
        for (const QJsonValue &r : j["results"].toArray())
            if (eachRow(r.toObject())) ++n;
        // next_url is Polygon's own link to the next page; the key goes in the header, never the URL.
        // new URL(next_url).host: host and a non-default port, never the user info. A link that
        // doesn't parse throws, like new URL() does.
        const QString next = jsv::truthy(j["next_url"]) ? jsv::toString(j["next_url"]) : QString();
        url.clear();
        if (!next.isEmpty()) {
            const QUrl u(next, QUrl::StrictMode);
            if (!u.isValid() || u.scheme().isEmpty() || (u.host().isEmpty() && (u.scheme() == "http" || u.scheme() == "https")))
                throw Error("Invalid URL");
            if (urlHost(u) == urlHost(QUrl(POLYGON))) url = next;
        }
    }
}

Bars Feeds::polygonCandles(const QString &ticker, const QString &tf, int limit)
{
    if (polygonCfg().isEmpty()) throw Error("No Polygon key in data/feeds.json");
    static const QHash<QString, QPair<int, QString>> POLYGON_TF = {
        { "1m", { 1, "minute" } }, { "5m", { 5, "minute" } }, { "15m", { 15, "minute" } },
        { "30m", { 30, "minute" } }, { "1D", { 1, "day" } },
    };
    const QString base = nativeBase(tf, { "1m", "5m", "15m", "30m", "1D" });
    if (base.isEmpty()) throw Error(QStringLiteral("Polygon can't build a %1 chart").arg(tf));
    const bool intraday = isIntraday(base);
    const double ratio = std::max(1.0, jsv::round(double(parseTf(tf).seconds) / double(parseTf(base).seconds)));
    const double want = std::min(limit * ratio + ratio, 60000.0);
    const auto [mult, span] = POLYGON_TF.value(base);
    const QString from = startFor(base, want, jsv::dateUtc(2003, 0, 1));
    const QString to = jsv::isoString(jsv::nowMs() + 86400e3).left(10);
    Bars out;
    polygonPages(QStringLiteral("%1/v2/aggs/ticker/%2/range/%3/%4/%5/%6?adjusted=true&sort=desc&limit=50000")
                     .arg(POLYGON, jsv::encodeURIComponent(ticker)).arg(mult).arg(span, from, to),
                 int(want), [&](const QJsonObject &r) {
                     const double t = std::floor(jsv::toNumber(r.value("t")) / 1000);
                     if (intraday && !inUsSession(t)) return false;
                     Bar b;
                     b.time = intraday ? t : dayOf(t);
                     b.open = jsv::toNumber(r.value("o"));
                     b.high = jsv::toNumber(r.value("h"));
                     b.low = jsv::toNumber(r.value("l"));
                     b.close = jsv::toNumber(r.value("c"));
                     b.volume = numOr0(r.value("v"));
                     out.push_back(b);
                     return true;
                 });
    return finish(out, base, tf, limit);
}

// Sources to try before the public ones for a US stock, in this order.
Chain Feeds::stockChain(const QString &ticker, const QString &tf, int limit)
{
    Chain chain;
    if (!alpacaCfg().isEmpty())
        chain.push_back({ alpacaName(), [=, this] { return alpacaCandles(ticker, tf, limit); } });
    if (!polygonCfg().isEmpty())
        chain.push_back({ "Polygon", [=, this] { return polygonCandles(ticker, tf, limit); } });
    return chain;
}

// ---------- Polygon / Massive (CME futures) ----------
// Equity index futures expire on the third Friday of March, June, September and December. Trading
// moves to the next contract 8 days before expiry (CME's roll date), so that's when the front contract changes.
static double thirdFriday(int y, int m)
{
    const int first = jsv::utcDay(jsv::dateUtc(y, m, 1));
    return jsv::dateUtc(y, m, 1 + ((12 - first) % 7) + 14, 13, 30);
}

QString Feeds::frontContract(const QString &root, double now)
{
    static const QString MONTH_CODES = QStringLiteral("FGHJKMNQUVXZ");
    int y = jsv::utcFullYear(now);
    for (int k = 0; k < 12; ++k) {
        const int m = 2 + 3 * (k % 4);
        if (k && k % 4 == 0) ++y;
        if (now < thirdFriday(y, m) - 8 * 86400e3)
            return QStringLiteral("%1%2%3").arg(root).arg(MONTH_CODES[m]).arg(((y % 10) + 10) % 10);
    }
    return {};
}

QString Feeds::frontContract(const QString &root)
{
    return frontContract(root, jsv::nowMs());
}

Bars Feeds::polygonFuturesCandles(const QString &contract, const QString &tf, int limit)
{
    if (polygonCfg().isEmpty()) throw Error("No Polygon key in data/feeds.json");
    static const QHash<QString, QString> FUT_RES = {
        { "1m", "1min" }, { "5m", "5min" }, { "15m", "15min" }, { "30m", "30min" }, { "1h", "1hour" }, { "1D", "1session" },
    };
    const QString base = nativeBase(tf, { "1m", "5m", "15m", "30m", "1h", "1D" });
    if (base.isEmpty()) throw Error(QStringLiteral("Polygon can't build a %1 futures chart").arg(tf));
    const bool intraday = isIntraday(base);
    const double sec = double(parseTf(base).seconds);
    const double ratio = std::max(1.0, jsv::round(double(parseTf(tf).seconds) / sec));
    const double want = std::min(limit * ratio + ratio, 60000.0);
    // Futures trade about 23 hours a day, 5 days a week.
    const double days = intraday ? std::ceil(((want * sec) / 82800) * 1.5) + 4 : std::ceil(want * 1.5) + 5;
    const QString since = jsv::isoString(jsv::nowMs() - days * 86400e3).left(10);
    Bars out;
    polygonPages(QStringLiteral("%1/futures/v1/aggs/%2?resolution=%3&window_start.gte=%4&sort=window_start.desc&limit=50000")
                     .arg(POLYGON, jsv::encodeURIComponent(contract), FUT_RES.value(base), since),
                 int(want), [&](const QJsonObject &r) {
                     const double t = intraday
                         ? std::floor(jsv::toNumber(r.value("window_start")) / 1e9)
                         : jsv::dateParse(jsv::toString(r.value("session_end_date")) + "T00:00:00Z") / 1000;
                     if (!std::isfinite(t) || jsv::nullish(r.value("close"))) return false;
                     Bar b;
                     b.time = t;
                     b.open = jsv::toNumber(r.value("open"));
                     b.high = jsv::toNumber(r.value("high"));
                     b.low = jsv::toNumber(r.value("low"));
                     b.close = jsv::toNumber(r.value("close"));
                     b.volume = numOr0(r.value("volume"));
                     out.push_back(b);
                     return true;
                 });
    return finish(out, base, tf, limit);
}

// Sources to try before Yahoo's delayed chart for a CME future. The provider name carries the
// contract, so the chart's legend shows which one it is.
Chain Feeds::futuresChain(const QString &root, const QString &tf, int limit)
{
    if (polygonCfg().isEmpty()) return {};
    const QString contract = frontContract(root);
    return { { "Polygon " + (contract.isEmpty() ? QStringLiteral("null") : contract),
               [=, this] { return polygonFuturesCandles(contract, tf, limit); } } };
}

// For the probe: does each key you added work?
QVector<QPair<QString, std::function<void()>>> Feeds::probes()
{
    QVector<QPair<QString, std::function<void()>>> out;
    const QString since = jsv::isoString(jsv::nowMs() - 10 * 86400e3).left(10);
    const QJsonObject k = alpacaCfg();
    if (!k.isEmpty()) {
        const QString url = QStringLiteral("%1/v2/stocks/bars?symbols=SPY&timeframe=1Day&limit=1&start=%2&feed=%3")
                                .arg(ALPACA, since, jsv::encodeURIComponent(jsv::toString(jsv::orElse(k.value("feed"), "iex"))));
        const QList<QPair<QByteArray, QByteArray>> headers = {
            { "APCA-API-KEY-ID", jsv::toString(k.value("keyId")).toUtf8() },
            { "APCA-API-SECRET-KEY", jsv::toString(k.value("secretKey")).toUtf8() },
        };
        out.push_back({ "Alpaca (your key)", [this, url, headers] { getFeed("Alpaca", url, headers); } });
    }
    if (!polygonCfg().isEmpty()) {
        const QString url = QStringLiteral("%1/v2/aggs/ticker/SPY/range/1/day/%2/%3?limit=1")
                                .arg(POLYGON, since, jsv::isoNow().left(10));
        const QList<QPair<QByteArray, QByteArray>> headers = {
            { "Authorization", ("Bearer " + jsv::toString(polygonCfg().value("apiKey"))).toUtf8() },
        };
        out.push_back({ "Polygon (your key)", [this, url, headers] { getFeed("Polygon", url, headers); } });
    }
    return out;
}

} // namespace qtm
