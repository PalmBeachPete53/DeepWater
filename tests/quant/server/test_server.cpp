// qtm_test_server: quant-terminal's server tests (test/providers, feeds, imports, ptq, server
// .test.js) ported to C++, and the server-side JS functions compared with their Node outputs
// (reference/reference.json, written by make_reference.mjs).
//
// The network is replaced by a fake, like the JS tests replace globalThis.fetch; the tests' own
// calls to the local HTTP API go through for real. Data lives in temporary folders.

#include "../../../src/quant/core/CustomIndicators.h"
#include "../../../src/quant/core/Error.h"
#include "../../../src/quant/core/Indicators.h"
#include "../../../src/quant/core/Series.h"
#include "../../../src/quant/server/Alerts.h"
#include "../../../src/quant/server/Async.h"
#include "../../../src/quant/server/CandleStore.h"
#include "../../../src/quant/server/Feeds.h"
#include "../../../src/quant/server/Http.h"
#include "../../../src/quant/server/HttpApi.h"
#include "../../../src/quant/server/Imports.h"
#include "../../../src/quant/server/Json.h"
#include "../../../src/quant/server/MarketInfo.h"
#include "../../../src/quant/server/Providers.h"
#include "../../../src/quant/server/Ptq.h"
#include "../../../src/quant/server/Server.h"
#include "../../../src/quant/server/Store.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QHttpHeaders>
#include <QHttpServer>
#include <QHttpServerRequest>
#include <QHttpServerResponder>
#include <QJsonDocument>
#include <QMutex>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

#include <atomic>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <functional>
#ifdef _WIN32
#include <QDateTime>
#else
#include <sys/time.h>
#endif

using namespace qtm;

// ---------------------------------------------------------------- tiny harness

static int g_checks = 0, g_failures = 0, g_skipped = 0;
static QString g_test;

static void fail(const QString &what, const char *file, int line)
{
    ++g_failures;
    std::fprintf(stderr, "FAIL [%s] %s:%d: %s\n", qPrintable(g_test), file, line, qPrintable(what));
}

#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        ++g_checks;                                                                                                    \
        if (!(cond)) fail(QStringLiteral(#cond), __FILE__, __LINE__);                                                  \
    } while (0)
#define CHECK_MSG(cond, msg)                                                                                           \
    do {                                                                                                               \
        ++g_checks;                                                                                                    \
        if (!(cond)) fail(QStringLiteral(#cond) + "  -- " + (msg), __FILE__, __LINE__);                                \
    } while (0)
#define CHECK_EQ(a, b)                                                                                                 \
    do {                                                                                                               \
        ++g_checks;                                                                                                    \
        const auto va_ = (a);                                                                                          \
        const auto vb_ = (b);                                                                                          \
        if (!(va_ == vb_)) fail(QStringLiteral(#a " == " #b "  -- got: ") + show(va_) + "  want: " + show(vb_), __FILE__, __LINE__); \
    } while (0)

static QString show(const QString &s) { return '"' + s + '"'; }
static QString show(const char *s) { return show(QString::fromUtf8(s)); }
static QString show(int v) { return QString::number(v); }
static QString show(qsizetype v) { return QString::number(v); }
static QString show(double v) { return jsv::numberToString(v); }
static QString show(bool v) { return v ? "true" : "false"; }
static QString show(const QJsonValue &v) { return QString::fromUtf8(jsv::stringify(v)).left(600); }
static QString show(const QJsonObject &v) { return show(QJsonValue(v)); }
static QString show(const QJsonArray &v) { return show(QJsonValue(v)); }
static QString show(const QStringList &v) { return '[' + v.join(" | ") + ']'; }
static QString show(const Values &v) { QStringList p; for (double x : v) p << jsv::numberToString(x); return '[' + p.join(",") + ']'; }

static void test(const QString &name)
{
    g_test = name;
    std::printf("- %s\n", qPrintable(name));
    std::fflush(stdout);
}

static void skip(const QString &why)
{
    ++g_skipped;
    std::printf("    skipped: %s\n", qPrintable(why));
}

// JSON values compare as numbers when both are numbers (1 == 1.0), and recursively otherwise.
static bool sameJson(const QJsonValue &a, const QJsonValue &b)
{
    if (a.isDouble() && b.isDouble()) {
        const double x = a.toDouble(), y = b.toDouble();
        return x == y || (std::isnan(x) && std::isnan(y));
    }
    if (a.isArray() && b.isArray()) {
        const QJsonArray x = a.toArray(), y = b.toArray();
        if (x.size() != y.size()) return false;
        for (int i = 0; i < x.size(); ++i)
            if (!sameJson(x[i], y[i])) return false;
        return true;
    }
    if (a.isObject() && b.isObject()) {
        const QJsonObject x = a.toObject(), y = b.toObject();
        if (x.keys() != y.keys()) return false;
        for (const auto &k : x.keys())
            if (!sameJson(x.value(k), y.value(k))) return false;
        return true;
    }
    return a == b;
}
#define CHECK_JSON(a, b)                                                                                               \
    do {                                                                                                               \
        ++g_checks;                                                                                                    \
        const QJsonValue va_ = (a);                                                                                    \
        const QJsonValue vb_ = (b);                                                                                    \
        if (!sameJson(va_, vb_)) fail(QStringLiteral(#a "  -- got: ") + show(va_) + "\n      want: " + show(vb_), __FILE__, __LINE__); \
    } while (0)

static QJsonValue barsJson(const Bars &bars)
{
    QJsonArray a;
    for (const Bar &b : bars)
        a.append(QJsonObject { { "time", b.time }, { "open", b.open }, { "high", b.high }, { "low", b.low },
                               { "close", b.close }, { "volume", b.volume } });
    // NaN -> null, like JSON.stringify
    return jsv::parse(jsv::stringify(a));
}

static QJsonValue readJson(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        std::fprintf(stderr, "can't read %s\n", qPrintable(path));
        std::exit(2);
    }
    return jsv::parse(f.readAll());
}

static QByteArray readFile(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

static void writeFile(const QString &path, const QByteArray &data)
{
    QFile f(path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) f.write(data);
}

static void setMtime(const QString &path, double sec)
{
#ifdef _WIN32
    QFile f(path);
    if (f.open(QIODevice::ReadWrite))
        f.setFileTime(QDateTime::fromSecsSinceEpoch(qint64(sec)), QFileDevice::FileModificationTime);
#else
    struct timeval tv[2];
    tv[0].tv_sec = tv[1].tv_sec = time_t(sec);
    tv[0].tv_usec = tv[1].tv_usec = 0;
    ::utimes(QFile::encodeName(path).constData(), tv);
#endif
}

// ---------------------------------------------------------------- fake network

struct Call {
    QString host;
    QString url;
    QList<QPair<QByteArray, QByteArray>> headers;
    QByteArray body;
    QByteArray header(const QByteArray &name) const
    {
        for (const auto &h : headers)
            if (h.first.toLower() == name.toLower()) return h.second;
        return {};
    }
};
static QMutex g_callsMutex;
static QVector<Call> g_calls;

static void record(const QString &url, const http::FetchOptions &o)
{
    QMutexLocker lock(&g_callsMutex);
    QUrl u(url);
    QString host = u.host();
    if (u.port() != -1) host += ':' + QString::number(u.port());
    g_calls.push_back({ host, url, o.headers, o.body });
}
static QVector<Call> calls()
{
    QMutexLocker lock(&g_callsMutex);
    return g_calls;
}
static void clearCalls()
{
    QMutexLocker lock(&g_callsMutex);
    g_calls.clear();
}

static http::FetchResponse jsonResponse(const QJsonValue &v, int status = 200)
{
    http::FetchResponse r;
    r.status = status;
    r.body = jsv::stringify(v);
    return r;
}

static bool isLocal(const QString &url)
{
    const QString h = QUrl(url).host();
    return h == "127.0.0.1" || h == "localhost";
}

// ---------------------------------------------------------------- HTTP client for the local API

struct Res {
    int status = 0;
    QByteArray body;
    QJsonValue json() const { return jsv::parse(body); }
};

static int g_port = 0;

static Res request(const QByteArray &method, const QString &path, const QByteArray &body = {},
                   QList<QPair<QByteArray, QByteArray>> headers = {}, int port = 0)
{
    http::FetchOptions o;
    o.method = method;
    o.headers = headers;
    o.body = body;
    o.hasBody = !body.isEmpty() || method == "POST" || method == "PUT" || method == "PATCH";
    o.timeoutMs = 60000;
    Res r;
    try {
        const auto f = http::fetch(QStringLiteral("http://127.0.0.1:%1%2").arg(port ? port : g_port).arg(path), o);
        r.status = f.status;
        r.body = f.body;
    } catch (const http::FetchError &e) {
        r.status = -1;
        r.body = e.message().toUtf8();
    }
    return r;
}
static const QList<QPair<QByteArray, QByteArray>> H = { { "x-qt", "1" }, { "Content-Type", "application/json" } };
static Res get(const QString &path) { return request("GET", path); }
static Res post(const QString &path, const QByteArray &body = {}) { return request("POST", path, body, H); }
static QByteArray J(const QJsonValue &v) { return jsv::stringify(v); }
static QJsonObject obj(const char *json) { return jsv::parse(QByteArray(json)).toObject(); }

// ---------------------------------------------------------------- reference: JS helpers

static QJsonObject REF;

// A save over an existing file: C's rename refuses it on Windows, and every save after the first failed there.
static void testStoreOverwrites()
{
    test("Store overwrites");
    QTemporaryDir dir;
    Store store(dir.path());
    store.write("state", QJsonObject {{"n", 1}});
    bool threw = false;
    try {
        store.write("state", QJsonObject {{"n", 2}});
    } catch (const std::exception &) {
        threw = true;
    }
    CHECK(!threw);
    CHECK(store.read("state", QJsonValue()).toObject().value("n").toInt() == 2);
    CHECK(QDir(dir.path()).entryList({"*.tmp"}, QDir::Files).isEmpty());
}

static void testCandleStore()
{
    test("CandleStore");
    QTemporaryDir dir;
    CandleStore store(dir.path() + "/candles");
    CHECK(!store.read("BINANCE:BTCUSDT", "1h").has_value());
    StoredCandles s;
    s.set.symbol = "BINANCE:BTCUSDT";
    s.set.provider = "Binance";
    s.limit = 3000;
    s.fullAt = 1800000000;
    for (int i = 0; i < 10; ++i) {
        Bar b;
        b.time = 1800000000 - 3600 * (9 - i);
        b.open = 100 + i;
        b.high = 101 + i;
        b.low = 99 + i;
        b.close = 100.5 + i;
        b.volume = 7 * i;
        s.set.candles.push_back(b);
    }
    CHECK(store.write("BINANCE:BTCUSDT", "1h", s));
    const auto back = store.read("BINANCE:BTCUSDT", "1h");
    CHECK(back.has_value());
    CHECK(back->set.symbol == s.set.symbol && back->set.provider == "Binance" && back->limit == 3000 && back->fullAt == s.fullAt);
    CHECK(back->set.candles.size() == 10);
    CHECK(back->set.candles.last().time == s.set.candles.last().time && back->set.candles[3].volume == 21 && back->set.candles[9].close == 109.5);
    // 1m and 1M are one file name on Windows and macOS when written as they are.
    CHECK(store.write("A", "1m", s));
    StoredCandles other = s;
    other.limit = 5;
    CHECK(store.write("A", "1M", other));
    CHECK(store.read("A", "1m")->limit == 3000 && store.read("A", "1M")->limit == 5);
    // A file cut short is as if it weren't there.
    QDir files(dir.path() + "/candles");
    const QStringList all = files.entryList({"*.candles"}, QDir::Files);
    CHECK(all.size() == 3);
    for (const QString &name : all) {
        QFile f(files.filePath(name));
        CHECK(f.open(QIODevice::ReadWrite));
        CHECK(f.resize(f.size() - 20));
    }
    CHECK(!store.read("BINANCE:BTCUSDT", "1h").has_value());

    test("planCandleRefresh");
    const qint64 last = qint64(s.set.candles.last().time);
    s.fullAt = last;
    CandleRefresh plan = planCandleRefresh(s, "1h", 3000, last + 7200);
    CHECK(!plan.full && plan.recent == 5);
    plan = planCandleRefresh(s, "1h", 3000, last + 3600 * 20);
    CHECK(!plan.full && plan.recent == 22);
    CHECK(planCandleRefresh(s, "1h", 3000, last + CandleStore::FullAfter + 1).full);   // too old to trust
    CHECK(planCandleRefresh(s, "1h", 4000, last + 60).full);                           // asked for more bars
    CHECK(planCandleRefresh(StoredCandles{}, "1h", 3000, last).full);
    CHECK(planCandleRefresh(s, "nonsense", 3000, last + 60).full);
    CHECK(planCandleRefresh(s, "1m", 500, last + 3600 * 12).full);                     // more missing than asked for

    test("secondsToBarClose");
    const qint64 h4 = 1800000000 - 1800000000 % 14400;
    CHECK(secondsToBarClose(h4, "4h", h4 + 100) == 14300);
    CHECK(secondsToBarClose(h4, "1h", h4 + 3000) == 600);
    CHECK(secondsToBarClose(h4, "1h", h4 + 3600 + 60) == 3540);          // a delayed feed: on to the next bar
    CHECK(secondsToBarClose(h4, "1h", h4 + 3600 + 1801) == -1);          // market closed
    CHECK(secondsToBarClose(h4, "1D", h4 + 60, false) == -1);            // a session's day
    CHECK(secondsToBarClose(h4, "1D", h4 + 60, true) == 86340);
    CHECK(secondsToBarClose(h4, "nonsense", h4) == -1);
    // A month's length follows the calendar: 1 February 2027 to 1 March 2027 is 28 days.
    CHECK(secondsToBarClose(1801440000, "1M", 1801440000) == 28 * 86400);
    CHECK(formatCountdown(-1).isEmpty());
    CHECK(formatCountdown(59) == "00:59");
    CHECK(formatCountdown(3599) == "59:59");
    CHECK(formatCountdown(3600) == "01:00:00");
    CHECK(formatCountdown(14300) == "03:58:20");
    CHECK(formatCountdown(2 * 86400 + 3 * 3600 + 4 * 60 + 5) == "2d 03:04");
}

static void testJsHelpers()
{
    test("JS numbers print like Number#toString and JSON.stringify");
    for (const QJsonValue x : REF.value("numbers").toArray()) {
        const double n = x["n"].toDouble();
        CHECK_EQ(jsv::numberToString(n), x["s"].toString());
        CHECK_EQ(QString::fromUtf8(jsv::stringify(QJsonValue(n))), x["j"].toString());
    }
    CHECK_EQ(QString::fromUtf8(jsv::stringify(QJsonObject { { "a", QJsonArray { 1, "x\n\"", QJsonValue() } }, { "b", QJsonObject() } }, 2)),
             QString("{\n  \"a\": [\n    1,\n    \"x\\n\\\"\",\n    null\n  ],\n  \"b\": {}\n}"));

    test("Date.parse reads ISO and RFC 2822 dates like V8");
    for (const QJsonValue x : REF.value("dates").toArray()) {
        const double t = jsv::dateParse(x["s"].toString());
        CHECK_JSON(std::isnan(t) ? QJsonValue() : QJsonValue(t), x["t"]);
    }

    test("decodeURIComponent refuses malformed escapes; URLSearchParams decoding");
    CHECK_EQ(jsv::decodeURIComponent("/api/x%20y%C3%A9"), QString::fromUtf8("/api/x yé"));
    bool threw = false;
    try {
        jsv::decodeURIComponent("/%E0%A4%A");
    } catch (const Error &e) {
        threw = e.message() == "URI malformed";
    }
    CHECK(threw);
    const auto q = HttpApi::parseQuery("symbol=BTC%2FUSDT&q=a+b&x=%ZZ&x=2&flag");
    CHECK_EQ(q.value("symbol"), QString("BTC/USDT"));
    CHECK_EQ(q.value("q"), QString("a b"));
    CHECK_EQ(q.value("x"), QString("2"));
    CHECK(q.contains("flag") && q.value("flag").isEmpty());
    CHECK_EQ(HttpApi::normalizePath("/a/./b/../c/%2e%2E/d"), QString("/a/d"));
    CHECK_EQ(jsv::encodeURIComponent("a b/c?d=e&f|g^h!*'()~"), QString("a%20b%2Fc%3Fd%3De%26f%7Cg%5Eh!*'()~"));

    test("allowed(): this computer only; writes need x-qt and a same-host Origin");
    CHECK(HttpApi::allowed("GET", "127.0.0.1:3777", "", "", false));
    CHECK(HttpApi::allowed("GET", "localhost", "", "", false));
    CHECK(HttpApi::allowed("GET", "[::1]:3777", "", "", false));
    CHECK(!HttpApi::allowed("GET", "evil.com", "", "", false));
    CHECK(!HttpApi::allowed("GET", "", "", "", false));
    CHECK(!HttpApi::allowed("GET", "192.168.1.5:3777", "", "", false));
    CHECK(!HttpApi::allowed("POST", "127.0.0.1:3777", "", "", false));
    CHECK(HttpApi::allowed("POST", "127.0.0.1:3777", "1", "", false));
    CHECK(HttpApi::allowed("POST", "127.0.0.1:3777", "1", "http://127.0.0.1:3777", true));
    CHECK(!HttpApi::allowed("POST", "127.0.0.1:3777", "1", "http://127.0.0.1:3778", true));
    CHECK(!HttpApi::allowed("POST", "127.0.0.1:3777", "1", "https://evil.com", true));
    CHECK(!HttpApi::allowed("DELETE", "127.0.0.1:3777", "1", "null", true));
    CHECK(HttpApi::allowed("PUT", "localhost", "1", "http://localhost:80", true));
    CHECK(HttpApi::allowed("HEAD", "localhost:1", "", "", false));
}

// ---------------------------------------------------------------- reference: providers & co

static void testReferenceProviders(const QString &dataDir)
{
    Feeds feeds(dataDir, 10000);
    Imports imports(dataDir);
    Providers P(feeds, imports, { 10000, true, QTM_FIXTURES });

    test("parseSpread matches the JS");
    for (const QJsonValue x : REF.value("parseSpread").toArray()) {
        const auto sp = Providers::parseSpread(x["s"].toString());
        const QJsonValue got = sp ? QJsonValue(QJsonObject { { "a", sp->a }, { "op", sp->op }, { "b", sp->b } }) : QJsonValue();
        CHECK_JSON(got, x["r"]);
    }

    test("resolve matches the JS (offline symbol lists)");
    for (const QJsonValue x : REF.value("resolve").toArray()) {
        QJsonObject got;
        try {
            got = { { "ok", P.resolve(x["s"].toString()).toJson() } };
        } catch (const Error &e) {
            got = { { "error", e.message() }, { "status", e.status() ? QJsonValue(e.status()) : QJsonValue() } };
        }
        CHECK_MSG(sameJson(got, x["r"]), x["s"].toString() + " got " + show(got) + " want " + show(x["r"]));
    }

    test("krakenPairMap matches the JS");
    {
        const auto m = Providers::krakenPairMap(obj(R"({
            "XXBTZUSD": { "altname": "XBTUSD", "wsname": "XBT/USD", "status": "online" },
            "XDGUSD": { "altname": "XDGUSD", "wsname": "XDG/USD", "status": "online" },
            "SOLUSD": { "altname": "SOLUSD", "wsname": "SOL/USD", "status": "online" },
            "OLDUSD": { "altname": "OLDUSD", "wsname": "OLD/USD", "status": "delisted" },
            "NOWS": { "altname": "NOWS" },
            "ETHEUR": { "altname": "ETHEUR", "wsname": "ETH/EUR", "status": "reduce_only" } })"));
        QJsonObject got;
        for (auto it = m.begin(); it != m.end(); ++it) got.insert(it.key(), it.value());
        CHECK_JSON(got, REF.value("krakenPairMap"));
    }

    test("parseYahoo matches the JS");
    {
        const QJsonObject ref = REF.value("parseYahoo").toObject();
        // Rebuild the same input the reference script used.
        QJsonArray ts, open, high, low, close, volume;
        for (int i = 0; i < 30; ++i) {
            ts.append(1760450400.0 + i * 3600 * 7);
            open.append(i == 3 ? QJsonValue() : QJsonValue(100 + i));
            high.append(101 + i);
            low.append(99 + i);
            close.append(i == 5 ? QJsonValue() : QJsonValue(100.5 + i));
            volume.append(i == 7 ? QJsonValue() : QJsonValue(1000 + i));
        }
        ts.append(ts.last());
        open.append(200);
        high.append(201);
        low.append(199);
        close.append(200.5);
        volume.append(5);
        const QJsonObject y { { "chart", QJsonObject { { "result", QJsonArray { QJsonObject {
            { "meta", QJsonObject { { "gmtoffset", -14400 } } },
            { "timestamp", ts },
            { "indicators", QJsonObject { { "quote", QJsonArray { QJsonObject { { "open", open }, { "high", high }, { "low", low },
                                                                                  { "close", close }, { "volume", volume } } } } } },
        } } } } } };
        CHECK_JSON(barsJson(Providers::parseYahoo(y, "1D")), ref.value("daily"));
        CHECK_JSON(barsJson(Providers::parseYahoo(y, "5m")), ref.value("intraday"));
        auto err = [](const QJsonValue &j) {
            try {
                Providers::parseYahoo(j, "1D");
            } catch (const Error &e) {
                return e.message();
            }
            return QString();
        };
        CHECK_EQ(err(obj(R"({"chart":{"result":null,"error":{"description":"No data found, symbol may be delisted"}}})")),
                 ref.value("noData").toString());
        CHECK_EQ(err(QJsonObject()), ref.value("empty").toString());
    }

    test("nyWallToUtc, frontContract and inUsSession match the JS");
    for (const QJsonValue x : REF.value("nyWallToUtc").toArray())
        CHECK_EQ(Providers::nyWallToUtc(x["ms"].toDouble()), x["r"].toDouble());
    for (const QJsonValue x : REF.value("frontContract").toArray())
        CHECK_EQ(Feeds::frontContract(x["root"].toString(), x["ms"].toDouble()), x["r"].toString());
    for (const QJsonValue x : REF.value("inUsSession").toArray())
        CHECK_EQ(Feeds::inUsSession(x[0].toDouble()), x[1].toBool());

    test("search matches the JS (offline)");
    const QJsonObject search = REF.value("search").toObject();
    for (const QString &q : search.keys()) CHECK_JSON(P.search(q), search.value(q));

    test("parseRss matches the JS");
    {
        const QString rss = QString::fromUtf8(R"(<?xml version="1.0"?><rss><channel><title>Feed</title>
<item><title><![CDATA[Bitcoin &amp; Ether rally]]></title><link>https://x.test/a</link><pubDate>Wed, 29 Oct 2026 10:00:00 +0000</pubDate><description><![CDATA[<p>BTC rose &quot;fast&quot; &#039;today&#39; &lt;b&gt;</p>]]></description></item>
<item><title>No date</title><link>https://x.test/b</link><description>)") + QString("word ").repeated(80) + QString::fromUtf8(R"(</description></item>
<item><title></title><link>https://x.test/c</link></item>
<item>
  <title type="html">Solana &apos;news&apos;</title>
  <link>https://x.test/d</link>
  <pubDate>Tue, 28 Oct 2026 18:05:12 GMT</pubDate>
  <description>Short</description>
</item>
</channel></rss>)");
        CHECK_JSON(MarketInfo::parseRss(rss, "Test"), REF.value("parseRss"));
    }

    test("alerts describe() matches the JS");
    for (const QJsonValue x : REF.value("describe").toArray()) {
        const QJsonObject a = x["a"].toObject();
        CHECK_EQ(Alerts::describe(a, a.contains("watchlist") ? "BINANCE:ETHUSDT" : a.value("symbol").toString()), x["r"].toString());
    }
}

static QString csvFixture(const QString &kind);

static void testReferenceImports()
{
    test("parseTime, tfLabel and cleanName match the JS");
    for (const QJsonValue x : REF.value("parseTime").toArray()) {
        const double t = imports::parseTime(x["s"].toString());
        CHECK_MSG(sameJson(std::isnan(t) ? QJsonValue() : QJsonValue(t), x["t"]), x["s"].toString());
    }
    for (const QJsonValue x : REF.value("parseTimeTod").toArray()) {
        const QString tod = x["t"].toString();
        const double t = imports::parseTime(x["d"].toString(), &tod);
        CHECK_JSON(std::isnan(t) ? QJsonValue() : QJsonValue(t), x["r"]);
    }
    for (const QJsonValue x : REF.value("tfLabel").toArray()) CHECK_EQ(imports::tfLabel(x[0].toDouble()), x[1].toString());
    for (const QJsonValue x : REF.value("cleanName").toArray()) {
        QJsonObject got;
        try {
            got = { { "ok", imports::cleanName(x["n"].toString()) } };
        } catch (const Error &e) {
            got = { { "error", e.message() }, { "status", e.status() } };
        }
        CHECK_JSON(got, x["r"]);
    }

    test("parseBars matches the JS on every sample file");
    const QJsonObject bars = REF.value("parseBars").toObject();
    for (const QString &k : bars.keys()) {
        QJsonObject got;
        try {
            const ParsedBars p = imports::parseBars(csvFixture(k));
            got = { { "ok", QJsonObject { { "bars", barsJson(p.bars) }, { "baseSeconds", p.baseSeconds }, { "skipped", p.skipped } } } };
        } catch (const Error &e) {
            got = { { "error", e.message() }, { "status", e.status() } };
        }
        CHECK_MSG(sameJson(got, bars.value(k)), k + ": " + show(got).left(300) + "\n want " + show(bars.value(k)).left(300));
    }

    test("parseSignals, the signal indicator source and its strategy match the JS");
    const QJsonObject sigs = REF.value("parseSignals").toObject();
    const qint64 T0 = qint64(jsv::dateUtc(2026, 9, 14, 13, 30) / 1000);
    const QHash<QString, QString> inputs = {
        { "basic", "time,position\n2026-10-14 13:30,0\n2026-10-14 13:31,1\n2026-10-14 13:32,1\n2026-10-14 13:33,short\n2026-10-14 13:34,flat\n" },
        { "bad", "time,position\n2026-10-14 13:30,2\n" },
        { "headerless", QStringLiteral("%1,1\n%2,-1\n%3,buy\n%4,").arg(T0).arg(T0 + 60).arg(T0 + 120).arg(T0 + 180) },
        { "half", "time,signal\n2026-10-14 13:30,0.5\n2026-10-14 13:29,-0.5\n" },
        { "noRows", "time,position\n" },
    };
    for (const QString &k : sigs.keys()) {
        QJsonObject got;
        try {
            const Signals s = imports::parseSignals(inputs.value(k));
            QJsonArray changes;
            for (const auto &c : s.changes) changes.append(QJsonArray { c.first, c.second });
            got = { { "ok", QJsonObject { { "rows", s.rows }, { "changes", changes }, { "from", s.from }, { "to", s.to } } } };
        } catch (const Error &e) {
            got = { { "error", e.message() }, { "status", e.status() } };
        }
        CHECK_MSG(sameJson(got, sigs.value(k)), k + ": " + show(got));
    }
    QString src = imports::signalIndicatorSource("TEST", imports::parseSignals(inputs.value("basic")), "test\nfile.csv");
    QString want = REF.value("signalSource").toString();
    static const QRegularExpression date(QStringLiteral(" on \\d{4}-\\d{2}-\\d{2}\\."));
    src.replace(date, " on DATE.");
    want.replace(date, " on DATE.");
    CHECK_EQ(src, want);
    CHECK_JSON(imports::signalStrategy("custom:signal-test"), REF.value("signalStrategy"));
}

static QJsonObject exampleStrategy(const QString &name);

static void testReferencePtq()
{
    test("PTQ translate and toTerm match the JS");
    const QJsonArray extra = jsv::parse(R"([
      { "indicators": {}, "entryLong": [{ "left": "close", "op": "crosses", "right": 10 }] },
      { "indicators": { "fast": { "id": "ema", "params": { "length": 9 } }, "slow": { "id": "ema", "params": { "length": 21 } } },
        "entryLong": [{ "left": "fast", "op": "crossesAbove", "right": "slow" }],
        "exitLong": [{ "left": "fast", "op": "crossesBelow", "right": "slow" }, { "left": "close", "op": "<", "right": "slow" }], "exitMatch": "any", "maxBarsInTrade": 10 },
      { "indicators": { "m": { "id": "macross", "params": { "type": "SMA" } }, "v": { "id": "volume" }, "d": { "id": "donchian" } },
        "entryShort": [{ "left": "m.slow", "op": ">", "right": "m.fast" }, { "left": "v.ma", "op": "<", "right": "v" }, { "left": "close", "op": ">", "right": "d.upper" }],
        "exitShort": [{ "left": "close", "op": "<", "right": "d.lower" }, { "left": "close", "op": "<", "right": "d.basis" }], "spreadPct": 0.1, "takeProfitPct": 5, "trailingStopPct": 3 },
      { "indicators": { "e": { "id": "ema", "params": { "length": 900, "source": "hl2" } }, "r": { "id": "rsi", "params": { "length": "14" } } },
        "entryLong": [{ "left": "e", "op": ">", "right": "hl2" }, { "left": "r", "op": "<", "right": "30" }, { "left": "nope", "op": ">", "right": { "line": {} } }], "maxBarsInTrade": 7 }
    ])").toArray();
    for (const QJsonValue x : REF.value("translate").toArray()) {
        const QString name = x["name"].toString();
        QJsonObject strategy;
        QJsonValue nameArg = name;
        if (name.startsWith("extra")) {
            const int k = name.mid(5).toInt();
            strategy = extra[k].toObject();
            nameArg = k ? QString("x").repeated(90) : QString();
        } else {
            strategy = exampleStrategy(name);
        }
        const QJsonValue tf = x["tf"].isNull() ? QJsonValue(QJsonValue::Undefined) : x["tf"];
        PtqTranslation t = Ptq::translate(strategy, tf, nameArg);
        t.manifest.insert("id", "ID");
        const QJsonObject got { { "manifest", t.manifest }, { "problems", QJsonArray::fromStringList(t.problems) },
                                { "notes", QJsonArray::fromStringList(t.notes) } };
        CHECK_MSG(sameJson(got, x["r"]), name + " " + x["tf"].toString() + ": " + show(got) + "\n want " + show(x["r"]));
    }
    for (const QJsonValue x : REF.value("toTerm").toArray()) {
        const PtqTerm t = Ptq::toTerm(x["o"], x["inds"].toObject());
        QJsonObject got;
        if (!t.error.isEmpty()) got.insert("error", t.error);
        else {
            got.insert("term", t.term);
            if (x["r"].toObject().contains("notes")) got.insert("notes", QJsonArray::fromStringList(t.notes));
        }
        CHECK_MSG(sameJson(got, x["r"]), show(x["o"]) + ": " + show(got) + " want " + show(x["r"]));
    }
}

// ---------------------------------------------------------------- CSV samples (same as the reference script)

struct MinuteBar {
    double time, open, high, low, close, volume;
};
static QVector<MinuteBar> minuteBars(int n)
{
    const double T0 = jsv::dateUtc(2026, 9, 14, 13, 30) / 1000;
    QVector<MinuteBar> out;
    for (int i = 0; i < n; ++i) {
        const double c = 60000 + 50 * std::sin(i / 20.0) + i;
        out.push_back({ T0 + i * 60, c - 3, c + 10, c - 10, c, double(1 + (i % 7)) });
    }
    return out;
}
static QString N(double v) { return jsv::numberToString(v); }

static QString alpacaCsv(int n)
{
    QStringList rows { "symbol,timestamp,open,high,low,close,volume,trade_count,vwap" };
    for (const auto &b : minuteBars(n)) {
        QString ts = jsv::isoString(b.time * 1000).replace("T", " ").replace(".000Z", "+00:00");
        rows << QStringLiteral("BTC/USD,%1,%2,%3,%4,%5,%6,12,%5").arg(ts, N(b.open), N(b.high), N(b.low), N(b.close), N(b.volume));
    }
    return rows.join('\n');
}

static QString csvFixture(const QString &kind)
{
    const auto minute = minuteBars(40);
    if (kind == "alpaca") return alpacaCsv(40);
    if (kind == "tv") {
        QStringList rows;
        for (int i = 0; i < 20; ++i) {
            const auto &b = minute[i];
            rows << QStringList { N(b.time), N(b.open), N(b.high), N(b.low), N(b.close), N(b.volume) }.join(',');
        }
        return rows.join('\n');
    }
    if (kind == "mt") {
        QStringList rows { "Date;Time;Open;High;Low;Close;Volume" };
        for (int i = 0; i < 20; ++i) {
            const auto &b = minute[i];
            const QString d = jsv::isoString(b.time * 1000);
            rows << QStringLiteral("%1;%2;%3;%4;%5;%6;%7").arg(d.left(10).replace("-", "."), d.mid(11, 5), N(b.open), N(b.high), N(b.low), N(b.close), N(b.volume));
        }
        return rows.join('\n');
    }
    if (kind == "tabs") {
        QStringList rows;
        for (int i = 0; i < 12; ++i) rows << N(minute[i].time * 1000) + '\t' + N(minute[i].close);
        return QString(QChar(0xFEFF)) + "time\tclose\n" + rows.join("\r\n");
    }
    if (kind == "quoted") {
        QStringList rows { "\"Date\",\"Open\",\"High\",\"Low\",\"Close\",\"Vol\"" };
        for (int i = 0; i < 15; ++i) {
            const auto &b = minute[i];
            rows << QStringLiteral("\"%1\",\"%2\",\"%3\",\"%4\",\"%5\",\"\"").arg(jsv::isoString(b.time * 1000), N(b.open), N(b.high), N(b.low), N(b.close));
        }
        return rows.join('\n');
    }
    if (kind == "skips") {
        QStringList rows { "time,open,high,low,close", "# a comment" };
        for (int i = 0; i < 14; ++i) {
            const auto &b = minute[i];
            rows << QStringLiteral("%1,%2,%3,%4,%5").arg(N(b.time), i == 3 ? "x" : N(b.open), N(b.high), N(b.low), i == 5 ? "" : N(b.close));
        }
        return rows.join('\n');
    }
    if (kind == "dupes") {
        QStringList rows { "time,close" };
        for (int i = 0; i < 12; ++i) rows << N(minute[i].time) + ',' + N(minute[i].close);
        rows << N(minute[4].time) + ',' + N(minute[4].close);
        return rows.join('\n');
    }
    if (kind == "daily") {
        QStringList rows { "date,close" };
        for (int i = 0; i < 12; ++i) rows << QStringLiteral("2026-01-%1,%2").arg(i + 1, 2, 10, QChar('0')).arg(100 + i);
        return rows.join('\n');
    }
    if (kind == "weekly") {
        QStringList rows { "date,close" };
        for (int i = 0; i < 12; ++i) rows << jsv::isoString(jsv::dateUtc(2026, 0, 5 + 7 * i)).left(10) + ',' + QString::number(100 + i);
        return rows.join('\n');
    }
    if (kind == "zone") {
        QStringList rows { "time,close" };
        for (int i = 0; i < 12; ++i) rows << QStringLiteral("2026-10-14T10:%1:00-04:00,%2").arg(i, 2, 10, QChar('0')).arg(i);
        return rows.join('\n');
    }
    if (kind == "badTime") return "time,open,high,low,close\n14/10/2026,1,2,0,1\n";
    if (kind == "twoSymbols") {
        QString s = alpacaCsv(40);
        const qsizetype k = s.indexOf("\nBTC/USD,");
        return s.left(k) + "\nETH/USD," + s.mid(k + 9);
    }
    if (kind == "few") return alpacaCsv(40).split('\n').mid(0, 5).join('\n');
    if (kind == "noCols") return "a,b\n1,2";
    if (kind == "empty") return "\n\n# only a comment\n";
    return {};
}

static QJsonObject exampleStrategy(const QString &name)
{
    static const QJsonObject all = jsv::parse(R"J({
      "EMA cross (9/21)": {
        "indicators": { "fast": { "id": "ema", "params": { "length": 9 } }, "slow": { "id": "ema", "params": { "length": 21 } } },
        "entryLong": [{ "left": "fast", "op": "crossesAbove", "right": "slow" }],
        "exitLong": [{ "left": "fast", "op": "crossesBelow", "right": "slow" }] },
      "RSI mean reversion (30/55)": {
        "indicators": { "rsi": { "id": "rsi", "params": { "length": 14 } } },
        "entryLong": [{ "left": "rsi", "op": "crossesAbove", "right": 30 }],
        "exitLong": [{ "left": "rsi", "op": "crossesAbove", "right": 55 }], "stopLossPct": 8 },
      "Supertrend follow": {
        "indicators": { "st": { "id": "supertrend", "params": { "atrLength": 10, "factor": 3 }, "output": "direction" } },
        "entryLong": [{ "left": "st", "op": "<", "right": 0 }], "exitLong": [{ "left": "st", "op": ">", "right": 0 }] },
      "MACD + 200 EMA trend filter": {
        "indicators": { "macd": { "id": "macd", "params": {} }, "trend": { "id": "ema", "params": { "length": 200 } } },
        "entryLong": [{ "left": "macd.macd", "op": "crossesAbove", "right": "macd.signal" }, { "left": "close", "op": ">", "right": "trend" }],
        "exitLong": [{ "left": "macd.macd", "op": "crossesBelow", "right": "macd.signal" }] },
      "Bollinger breakout": {
        "indicators": { "bb": { "id": "bb", "params": { "length": 20, "mult": 2 } } },
        "entryLong": [{ "left": "close", "op": "crossesAbove", "right": "bb.upper" }],
        "exitLong": [{ "left": "close", "op": "crossesBelow", "right": "bb.basis" }], "trailingStopPct": 10 }
    })J").toObject();
    return all.value(name).toObject();
}

// ---------------------------------------------------------------- providers.test.js

static Bars g_btc;

static void testProviders(const QString &dataDir)
{
    // mode: host -> "drop" | "dns" | status code
    auto mode = std::make_shared<QHash<QString, QString>>();
    auto modeMutex = std::make_shared<QMutex>();
    auto setMode = [=](QHash<QString, QString> m) {
        QMutexLocker lock(modeMutex.get());
        *mode = m;
    };
    http::setFetchMock([=](const QString &url, const http::FetchOptions &o) -> std::optional<http::FetchResponse> {
        if (isLocal(url)) return std::nullopt;
        record(url, o);
        const QUrl u(url);
        QString m;
        {
            QMutexLocker lock(modeMutex.get());
            m = mode->value(u.host(), mode->value("*"));
        }
        if (m == "drop") {
            // A dropped connection: nothing comes back until the timeout aborts the request.
            QThread::msleep(o.timeoutMs);
            throw http::FetchError(true, {}, "The operation was aborted due to timeout");
        }
        if (m == "dns") throw http::FetchError(false, "ENOTFOUND", "fetch failed");
        if (!m.isEmpty()) {
            http::FetchResponse r;
            r.status = m.toInt();
            r.body = "blocked";
            return r;
        }
        if (u.host() == "api.exchange.coinbase.com") {
            const double end = jsv::dateParse(QUrlQuery(u).queryItemValue("end", QUrl::FullyDecoded)) / 1000;
            QVector<Bar> before;
            for (const Bar &b : g_btc)
                if (b.time < end) before << b;
            before = before.mid(std::max<qsizetype>(0, before.size() - 300));
            QJsonArray rows;
            for (qsizetype i = before.size() - 1; i >= 0; --i) {
                const Bar &b = before[i];
                rows.append(QJsonArray { b.time, b.low, b.high, b.open, b.close, b.volume });
            }
            return jsonResponse(rows);
        }
        http::FetchResponse r;
        r.status = 404;
        r.body = "not mocked";
        return r;
    });

    Feeds feeds(dataDir, 300);
    Imports imports(dataDir);
    Providers P(feeds, imports, { 300, false, QTM_FIXTURES });

    test("with Binance blocked, crypto still resolves as crypto and loads from Coinbase");
    setMode({ { "api.binance.com", "451" }, { "data-api.binance.vision", "451" }, { "api.binance.us", "451" } });
    for (const QString s : { "BTCUSDT", "btc", "ETH-USD", "SOL/USDT" }) CHECK_EQ(P.resolve(s).exchange, QString("BINANCE"));
    CHECK_EQ(P.resolve("AAPL").exchange, QString("US"));
    {
        const CandleResult r = P.getCandles("BTCUSDT", "1D", 100);
        CHECK_EQ(r.provider, QString("Coinbase"));
        CHECK_EQ(r.candles.size(), qsizetype(100));
    }

    test("a silently dropped host costs one timeout, then is skipped");
    setMode({ { "api.binance.com", "drop" }, { "data-api.binance.vision", "drop" }, { "api.binance.us", "drop" } });
    {
        QElapsedTimer t;
        t.start();
        const CandleResult a = P.getCandles("BINANCE:ETHUSDT", "1D", 50);
        CHECK_EQ(a.provider, QString("Coinbase"));
        CHECK_MSG(t.elapsed() < 1500, QStringLiteral("first fallback took %1 ms (three hosts should time out together)").arg(t.elapsed()));
        t.restart();
        clearCalls();
        const CandleResult b = P.getCandles("BINANCE:SOLUSDT", "1D", 50);
        CHECK_EQ(b.provider, QString("Coinbase"));
        CHECK_MSG(t.elapsed() < 200, "second chart skips the dead hosts");
        bool binance = false;
        for (const auto &c : calls()) binance = binance || c.host.contains("binance");
        CHECK_MSG(!binance, "no new Binance calls while it is marked down");
    }

    test("network failures are upstream errors (the server answers 502)");
    setMode({ { "*", "dns" } });
    {
        bool upstream = false;
        try {
            P.economicCalendar();
        } catch (const Error &e) {
            upstream = e.upstream();
        }
        CHECK(upstream);
    }

    test("the chart note for a fallback is plain words: no hostnames or HTTP codes");
    setMode({ { "api.binance.com", "451" }, { "data-api.binance.vision", "451" }, { "api.binance.us", "451" } });
    {
        const CandleResult r = P.getCandles("BTCUSDT", "1D", 100);
        const QString text = r.notes.join(" ");
        CHECK_MSG(text.contains("Binance isn't answering right now, so this chart uses Coinbase."), text);
        CHECK(!QRegularExpression("HTTP|\\.com|returned").match(text).hasMatch());
    }

    test("Kraken pairs are keyed by the names everyone else uses (BTC, DOGE), valued by Kraken's own");
    {
        const auto m = Providers::krakenPairMap(obj(R"({"XXBTZUSD":{"altname":"XBTUSD","wsname":"XBT/USD","status":"online"},
            "XDGUSD":{"altname":"XDGUSD","wsname":"XDG/USD","status":"online"},"SOLUSD":{"altname":"SOLUSD","wsname":"SOL/USD","status":"online"},
            "OLDUSD":{"altname":"OLDUSD","wsname":"OLD/USD","status":"delisted"}})"));
        CHECK_EQ(m.value("BTCUSD"), QString("XBTUSD"));
        CHECK_EQ(m.value("DOGEUSD"), QString("XDGUSD"));
        CHECK_EQ(m.value("SOLUSD"), QString("SOLUSD"));
        CHECK(!m.contains("OLDUSD"));
    }

    test("XAUUSD charts gold, and falls back to PAXG when Yahoo is down");
    setMode({ { "query1.finance.yahoo.com", "429" }, { "query2.finance.yahoo.com", "429" } });
    for (const QString s : { "XAUUSD", "xau/usd", "FX:XAUUSD" }) CHECK_EQ(P.resolve(s).id, QString("FX:XAUUSD"));
    CHECK_EQ(P.resolve("XAGUSD").id, QString("FX:XAGUSD"));
    CHECK_EQ(P.resolve("GOLD").exchange, QString("US"));
    for (const QString s : { "^FTSE", "YAHOO:^FTSE" }) CHECK_EQ(P.resolve(s).id, QString("YAHOO:^FTSE"));
    clearCalls();
    {
        bool ok = true;
        CandleResult r;
        try {
            r = P.getCandles("XAUUSD", "1D", 50);
        } catch (const Error &) {
            ok = false;
        }
        bool tried = false;
        for (const auto &c : calls()) tried = tried || c.host.contains("binance") || c.host.contains("coinbase");
        CHECK_MSG(tried, "gold tries the PAXG token feeds after Yahoo");
        if (ok) {
            bool paxg = false;
            for (const auto &n : r.notes) paxg = paxg || n.contains("PAXG");
            CHECK(paxg);
        }
    }
    http::setFetchMock({});
}

// ---------------------------------------------------------------- feeds.test.js

static void testFeeds(const QString &dataDir)
{
    const QString FILE = QDir(dataDir).filePath("feeds.json");
    double stamp = jsv::nowMs() / 1000;
    auto writeFeeds = [&](const QJsonValue &v) {
        if (v.isNull()) {
            QFile::remove(FILE);
            return;
        }
        writeFile(FILE, jsv::stringify(v));
        stamp += 5;   // a clearly newer mtime, whatever the file system's clock resolution
        setMtime(FILE, stamp);
    };
    const QString keyId = "PKTESTKEYID", secretKey = "TESTSECRET", apiKey = "POLYTESTKEY";
    // 5-minute bars for one day, 08:00 to 19:55 New York (EDT, UTC-4): pre-market, the session, after hours.
    const double DAY = jsv::dateUtc(2026, 9, 14) / 1000;
    QVector<double> fiveMin;
    for (int i = 0; i < 144; ++i) fiveMin << DAY + 12 * 3600 + i * 300;
    auto bar = [](int k) {
        return QJsonObject { { "o", 100 + k }, { "h", 101 + k }, { "l", 99 + k }, { "c", 100.5 + k }, { "v", 1000 + k } };
    };
    auto alpacaStatus = std::make_shared<std::atomic<int>>(200);
    auto futuresStatus = std::make_shared<std::atomic<int>>(200);
    auto sortOk = std::make_shared<std::atomic<bool>>(true);

    http::setFetchMock([=](const QString &url, const http::FetchOptions &o) -> std::optional<http::FetchResponse> {
        if (isLocal(url)) return std::nullopt;
        record(url, o);
        const QUrl u(url);
        const QUrlQuery q(u);
        if (u.host() == "data.alpaca.markets") {
            if (*alpacaStatus != 200) return jsonResponse(QJsonObject { { "message", "forbidden." } }, *alpacaStatus);
            const QString sym = q.queryItemValue("symbols", QUrl::FullyDecoded);
            QJsonArray rows;
            if (q.queryItemValue("timeframe") == "1Day") {
                for (int i = 0; i < 30; ++i) {
                    QJsonObject r = bar(i);
                    r.insert("t", jsv::isoString((DAY - i * 86400 + 4 * 3600) * 1000));
                    rows.append(r);
                }
            } else {
                for (int i = int(fiveMin.size()) - 1; i >= 0; --i) {
                    QJsonObject r = bar(i);
                    r.insert("t", jsv::isoString(fiveMin[i] * 1000));
                    rows.append(r);
                }
            }
            if (q.queryItemValue("sort") != "desc") *sortOk = false;
            const bool page2 = q.queryItemValue("page_token") == "p2";
            const int half = int(std::ceil(rows.size() / 2.0));
            QJsonArray part;
            for (int i = page2 ? half : 0; i < (page2 ? rows.size() : half); ++i) part.append(rows[i]);
            return jsonResponse(QJsonObject { { "bars", QJsonObject { { sym, part } } },
                                              { "next_page_token", page2 ? QJsonValue() : QJsonValue("p2") } });
        }
        if (u.host() == "api.massive.com" && u.path().startsWith("/futures/")) {
            if (*futuresStatus != 200) return jsonResponse(QJsonObject { { "status", "NOT_AUTHORIZED" } }, *futuresStatus);
            if (q.queryItemValue("sort") != "window_start.desc") *sortOk = false;
            QJsonArray rows;
            for (int i = int(fiveMin.size()) - 1; i >= 0; --i)
                rows.append(QJsonObject { { "window_start", fiveMin[i] * 1e9 }, { "open", 20000 + i }, { "high", 20001 + i },
                                          { "low", 19999 + i }, { "close", 20000.5 + i }, { "volume", 50 } });
            return jsonResponse(QJsonObject { { "results", rows }, { "status", "OK" } });
        }
        if (u.host() == "query1.finance.yahoo.com") {
            QJsonArray ts, open, high, low, close, volume;
            for (double t : fiveMin) {
                ts.append(t);
                open.append(25000);
                high.append(25010);
                low.append(24990);
                close.append(25005);
                volume.append(7);
            }
            return jsonResponse(QJsonObject { { "chart", QJsonObject { { "result", QJsonArray { QJsonObject {
                { "meta", QJsonObject { { "gmtoffset", -14400 } } }, { "timestamp", ts },
                { "indicators", QJsonObject { { "quote", QJsonArray { QJsonObject { { "open", open }, { "high", high }, { "low", low },
                                                                                      { "close", close }, { "volume", volume } } } } } } } } } } } });
        }
        if (u.host() == "api.massive.com") {
            QJsonArray rows;
            for (int i = int(fiveMin.size()) - 1; i >= 0; --i) {
                QJsonObject r = bar(i);
                r.insert("t", fiveMin[i] * 1000);
                rows.append(r);
            }
            return jsonResponse(QJsonObject { { "results", rows }, { "status", "OK" } });
        }
        http::FetchResponse r;
        r.status = 404;
        r.body = "not mocked";
        return r;
    });

    Feeds F(dataDir, 300);
    Imports imports(dataDir);
    Providers P(F, imports, { 300, false, QTM_FIXTURES });
    auto leaks = [&] {
        int n = 0;
        for (const auto &c : calls()) {
            QString carries = c.url;
            for (const auto &h : c.headers) carries += QString::fromUtf8(h.first + h.second);
            QStringList own;
            if (c.host == "data.alpaca.markets") own = { keyId, secretKey };
            if (c.host == "api.massive.com") own = { apiKey };
            for (const QString &k : { keyId, secretKey, apiKey })
                if (carries.contains(k) && !own.contains(k)) ++n;
        }
        return n;
    };

    test("with no feeds.json, stock charts use only the public sources");
    writeFeeds(QJsonValue());
    CHECK(F.configured().isEmpty());
    CHECK_EQ(F.stockChain("AAPL", "1D", 100).size(), qsizetype(0));

    test("an Alpaca key takes over US stock charts: session bars only, keys in headers only");
    writeFeeds(QJsonObject { { "alpaca", QJsonObject { { "keyId", keyId }, { "secretKey", secretKey } } } });
    clearCalls();
    {
        const CandleResult r = P.getCandles("US:AAPL", "5m", 50);
        CHECK_EQ(r.provider, QString("Alpaca (IEX)"));
        CHECK_EQ(r.candles.size(), qsizetype(50));
        bool session = true, sorted = true;
        for (int i = 0; i < r.candles.size(); ++i) {
            session = session && Feeds::inUsSession(r.candles[i].time);
            if (i) sorted = sorted && r.candles[i].time > r.candles[i - 1].time;
        }
        CHECK_MSG(session, "pre-market and after-hours bars are left out");
        CHECK(sorted);
        CHECK_EQ(r.candles.last().time, DAY + 19 * 3600 + 55 * 60);
        CHECK_MSG(r.notes.join(" ").contains("IEX"), "the chart says it's the IEX feed");
        QVector<Call> alpaca;
        for (const auto &c : calls())
            if (c.host == "data.alpaca.markets") alpaca << c;
        CHECK_EQ(alpaca.size(), qsizetype(2));
        if (!alpaca.isEmpty()) CHECK_EQ(QString::fromUtf8(alpaca[0].header("APCA-API-KEY-ID")), keyId);
        bool inUrl = false;
        for (const auto &c : alpaca) inUrl = inUrl || c.url.contains(secretKey) || c.url.contains(keyId);
        CHECK_MSG(!inUrl, "keys never go in the URL");
        CHECK_EQ(leaks(), 0);
        CHECK_EQ(QString::fromUtf8(jsv::stringify(QJsonArray::fromStringList(F.configured()))), QString("[\"alpaca\"]"));
        CHECK(*sortOk);
    }

    test("daily bars from a key land on the trading date at UTC midnight, like the public sources");
    {
        const CandleResult r = P.getCandles("US:MSFT", "1D", 20);
        CHECK_EQ(r.provider, QString("Alpaca (IEX)"));
        bool midnight = true;
        for (const Bar &b : r.candles) midnight = midnight && std::fmod(b.time, 86400) == 0;
        CHECK(midnight);
        CHECK_EQ(r.candles.last().time, DAY);
    }

    test("weekly charts are built from the key's daily bars");
    {
        const CandleResult r = P.getCandles("US:NVDA", "1W", 3);
        CHECK_EQ(r.provider, QString("Alpaca (IEX)"));
        CHECK(r.candles.size() >= 3);
    }

    test("a refused Alpaca key falls back to Polygon, and the chart says why in plain words");
    writeFeeds(QJsonObject { { "alpaca", QJsonObject { { "keyId", keyId }, { "secretKey", secretKey } } },
                             { "polygon", QJsonObject { { "apiKey", apiKey } } } });
    *alpacaStatus = 401;
    clearCalls();
    {
        const CandleResult r = P.getCandles("US:TSLA", "15m", 10);
        CHECK_EQ(r.provider, QString("Polygon"));
        CHECK(!r.candles.isEmpty());
        bool session = true;
        for (const Bar &b : r.candles) session = session && Feeds::inUsSession(b.time);
        CHECK(session);
        CHECK_MSG(r.notes.contains("Alpaca refused your key, so this chart uses Polygon."), r.notes.join(" | "));
        CHECK(!QRegularExpression("HTTP|401|\\.com").match(r.notes.join(" ")).hasMatch());
        Call poly;
        for (const auto &c : calls())
            if (c.host == "api.massive.com") {
                poly = c;
                break;
            }
        CHECK_EQ(QString::fromUtf8(poly.header("Authorization")), "Bearer " + apiKey);
        CHECK_MSG(!poly.url.contains(apiKey), "the Polygon key goes in a header, not the URL");
        CHECK_EQ(leaks(), 0);
        // A refused key rests for a minute instead of being asked again on every chart.
        clearCalls();
        P.getCandles("US:AMD", "15m", 10);
        int n = 0;
        for (const auto &c : calls()) n += c.host == "data.alpaca.markets";
        CHECK_EQ(n, 0);
    }

    test("a broken feeds.json leaves the public sources working");
    writeFile(FILE, "{ not json");
    stamp += 5;
    setMtime(FILE, stamp);
    CHECK(F.configured().isEmpty());

    test("the front contract rolls 8 days before the third Friday of each quarter month");
    auto at = [](const char *d) { return Feeds::frontContract("NQ", jsv::dateParse(d)); };
    CHECK_EQ(at("2026-10-15T12:00:00Z"), QString("NQZ6"));
    CHECK_EQ(at("2026-12-09T12:00:00Z"), QString("NQZ6"));
    CHECK_EQ(at("2026-12-10T14:00:00Z"), QString("NQH7"));
    CHECK_EQ(at("2027-03-12T12:00:00Z"), QString("NQM7"));
    CHECK_EQ(Feeds::frontContract("MNQ", jsv::dateParse("2026-01-02T12:00:00Z")), QString("MNQH6"));

    test("NQ, MNQ, ES and MES resolve and search as CME futures");
    for (const auto &[typed, id] : QVector<QPair<QString, QString>> { { "NQ", "CME:NQ1!" }, { "nq1!", "CME:NQ1!" }, { "MNQ", "CME:MNQ1!" },
                                                                       { "CME_MINI:NQ1!", "CME:NQ1!" }, { "CME:ES1!", "CME:ES1!" }, { "MES1!", "CME:MES1!" } })
        CHECK_EQ(P.resolve(typed).id, id);
    CHECK_EQ(QJsonValue(P.search("MNQ"))[0]["symbol"].toString(), QString("CME:MNQ1!"));
    {
        const QJsonArray es = P.search("ES");
        CHECK_EQ(es[0]["symbol"].toString(), QString("CME:ES1!"));
        CHECK_EQ(es[0]["type"].toString(), QString("futures"));
    }

    test("without a Polygon key, futures chart Yahoo's delayed feed and say so");
    writeFeeds(QJsonValue());
    {
        const CandleResult r = P.getCandles("NQ1!", "5m", 30);
        CHECK_EQ(r.provider, QString("Yahoo (delayed)"));
        CHECK_EQ(r.candles.size(), qsizetype(30));
        bool note = false;
        for (const auto &n : r.notes) note = note || n.startsWith("Delayed about 10 minutes");
        CHECK_MSG(note, r.notes.join(" | "));
    }

    test("with a Polygon key, futures chart the live front contract, key in a header");
    writeFeeds(QJsonObject { { "polygon", QJsonObject { { "apiKey", apiKey } } } });
    clearCalls();
    {
        const CandleResult r = P.getCandles("CME:MNQ1!", "15m", 20);
        const QString contract = F.frontContract("MNQ");
        CHECK_EQ(r.provider, "Polygon " + contract);
        CHECK(!r.candles.isEmpty() && r.candles.size() <= 20);
        bool note = false;
        for (const auto &n : r.notes) note = note || n.contains("Front contract " + contract);
        CHECK_MSG(note, r.notes.join(" | "));
        Call c;
        for (const auto &x : calls())
            if (x.url.contains("/futures/v1/aggs/")) {
                c = x;
                break;
            }
        CHECK_MSG(c.url.contains("/futures/v1/aggs/" + contract + "?"), c.url);
        CHECK_EQ(QString::fromUtf8(c.header("Authorization")), "Bearer " + apiKey);
        CHECK_EQ(leaks(), 0);
    }

    test("a Polygon plan without futures falls back to the delayed chart, in plain words");
    *futuresStatus = 403;
    {
        const CandleResult r = P.getCandles("CME:ES1!", "1h", 20);
        CHECK_EQ(r.provider, QString("Yahoo (delayed)"));
        CHECK_MSG(r.notes.contains("Your Polygon plan doesn't include this, so this chart uses Yahoo (delayed)."), r.notes.join(" | "));
    }
    *futuresStatus = 200;
    CHECK(*sortOk);
    http::setFetchMock({});
}

// ---------------------------------------------------------------- server.test.js / imports.test.js (over HTTP)

// Runs fn on n worker threads at once while this thread keeps serving the HTTP API.
static void concurrently(int n, const std::function<void()> &fn)
{
    std::atomic<int> left { n };
    for (int i = 0; i < n; ++i)
        async::pool()->start([&] {
            fn();
            --left;
        });
    while (left > 0) QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

static void testServer(Backend &backend, const QString &customDir)
{
    const bool stubBacktest = false, stubQuant = false;   // the stand-ins are gone; kept for the branches below

    test("health answers, and anything outside /api is a plain 404");
    {
        const QJsonValue h = get("/api/health").json();
        CHECK_EQ(h["ok"].toBool(), true);
        CHECK_EQ(h["app"].toString(), QString("quant-terminal"));
        CHECK_EQ(h["version"].toString(), QString("1.0.0"));
        CHECK_EQ(h["platform"].toString(), QString("darwin"));
        CHECK(h["providers"].isObject() && h["feeds"].isArray() && h["yahoo"].toString() == "available" && h["alerts"].isDouble());
        CHECK_EQ(get("/").status, 404);
        CHECK_EQ(get("/index.html").status, 404);
    }

    test("candles come back sorted with OHLCV");
    {
        const QJsonValue r = get("/api/candles?symbol=BTCUSDT&tf=1W&limit=50").json();
        const QJsonArray c = r["candles"].toArray();
        CHECK_EQ(c.size(), qsizetype(50));
        bool sorted = true;
        for (int i = 1; i < c.size(); ++i) sorted = sorted && c[i]["time"].toDouble() > c[i - 1]["time"].toDouble();
        CHECK(sorted);
        for (const char *k : { "open", "high", "low", "close", "volume" }) CHECK(c[0].toObject().contains(k));
        CHECK_EQ(r["symbol"].toString(), QString("BINANCE:BTCUSDT"));
        CHECK_EQ(r["provider"].toString(), QString("fixture"));
        // Same bytes as Node for the bars: key order time, open, high, low, close, volume.
        CHECK(get("/api/candles?symbol=BTCUSDT&tf=1D&limit=10").body.contains("{\"time\":"));
    }

    test("state round-trips");
    request("PUT", "/api/state", J(obj(R"({"watchlists":{"Main":["BINANCE:BTCUSDT"]}})")), H);
    CHECK_JSON(get("/api/state").json()["watchlists"], obj(R"({"Main":["BINANCE:BTCUSDT"]})"));

    test("alerts can be created, listed and deleted");
    {
        const QJsonValue a = post("/api/alerts", J(obj(R"({"symbol":"BINANCE:BTCUSDT","tf":"1D","left":"close","op":">","right":1})"))).json();
        CHECK(!a["id"].toString().isEmpty());
        post("/api/alerts/check");
        bool fired = false;
        for (const QJsonValue e : get("/api/alerts/log").json().toArray()) fired = fired || e["alertId"] == a["id"];
        CHECK_MSG(fired, "the alert fired");
        request("DELETE", "/api/alerts/" + a["id"].toString(), {}, H);
        CHECK_EQ(get("/api/alerts").json().toArray().size(), qsizetype(0));
    }

    test("a malformed custom indicator is refused on save and never replaces a working one");
    {
        auto save = [](const QString &code) {
            return post("/api/custom", J(QJsonObject { { "name", "Test format check" }, { "code", code } }));
        };
        const QString broken = "export default { name: 'Broken', calc: (bars) => ({ value: bars.slice(5).map((b) => b.close) }) };";
        const Res r1 = save(broken);
        CHECK_EQ(r1.status, 400);
        CHECK_MSG(r1.json()["error"].toString().contains(QRegularExpression("calc\\(\\) returned 295 values, but there are 300 bars")),
                  QString::fromUtf8(r1.body));
        bool listed = false;
        for (const QJsonValue c : get("/api/custom").json().toArray()) listed = listed || c["file"] == QJsonValue("test-format-check.js");
        CHECK_MSG(!listed, "a refused new file is not kept");
        const QString good = "export default { name: 'Fine', calc: (bars, p, ta) => ({ value: ta.source(bars) }) };";
        CHECK_EQ(save(good).status, 200);
        CHECK_EQ(save(broken).status, 400);
        CHECK_EQ(QString::fromUtf8(get("/custom/test-format-check.js").body), good);
        QJsonValue entry;
        for (const QJsonValue c : get("/api/custom").json().toArray())
            if (c["file"] == QJsonValue("test-format-check.js")) entry = c;
        CHECK(entry.isObject() && entry["error"].isNull());
        request("DELETE", "/api/custom/test-format-check.js", {}, H);
        CHECK(!QFile::exists(QDir(customDir).filePath("test-format-check.js")));
    }

    test("POST /api/backtest runs the tester on the server, with quant checks on request");
    {
        const QJsonObject strategy = exampleStrategy("EMA cross (9/21)");
        if (stubBacktest) {
            skip("core/Backtest.cpp isn't there yet");
        } else {
            const QJsonValue r = post("/api/backtest", J(QJsonObject { { "symbol", "BTCUSDT" }, { "tf", "1D" }, { "limit", 500 },
                                                                       { "strategy", strategy }, { "quantChecks", !stubQuant } })).json();
            CHECK_EQ(r["symbol"].toString(), QString("BINANCE:BTCUSDT"));
            CHECK_EQ(r["bars"].toInt(), 500);
            CHECK(r["stats"]["totalTrades"].toDouble() > 0 && std::isfinite(r["stats"]["netProfitPct"].toDouble()));
            CHECK_EQ(r["trades"].toArray().size(), qsizetype(r["stats"]["totalTrades"].toInt()));
            bool bools = true;
            for (const QJsonValue g : r["quantChecks"]["gates"].toArray()) bools = bools && g["pass"].isBool();
            if (!stubQuant) CHECK(r["quantChecks"]["total"].toInt() >= 3 && bools);
            CHECK_MSG(r["equity"].isUndefined(), "equity only when asked for");
            const Res bad = post("/api/backtest", J(obj(R"({"symbol":"BTCUSDT","strategy":{"entryLong":[{"left":"nope","op":">","right":1}]}})")));
            CHECK_EQ(bad.status, 400);
            CHECK(bad.json()["error"].toString().contains("Unknown operand \"nope\""));
        }
        CHECK_EQ(request("POST", "/api/backtest", "{}").status, 403);
        bool ema = false;
        for (const QJsonValue d : get("/api/indicators").json().toArray()) {
            bool length = false;
            for (const QJsonValue i : d["inputs"].toArray()) length = length || i["key"] == QJsonValue("length");
            ema = ema || (d["id"] == QJsonValue("ema") && length && d["outputs"].toArray().contains(QJsonValue("value")));
        }
        CHECK(ema);
    }

    test("writes without the app header are refused");
    CHECK_EQ(request("PUT", "/api/state", "{}", { { "Content-Type", "application/json" } }).status, 403);

    test("a custom indicator file can be saved and loaded");
    {
        const QString code = "export default { name: 'Double close', calc: (bars, p, ta) => ({ value: ta.source(bars).map((v) => v * 2) }) };";
        const QJsonValue r = post("/api/custom", J(QJsonObject { { "name", "Test double close" }, { "code", code } })).json();
        CHECK_EQ(r["ok"].toBool(), true);
        CHECK_EQ(r["id"].toString(), QString("custom:test-double-close"));
        bool listed = false;
        for (const QJsonValue c : get("/api/custom").json().toArray()) listed = listed || c["file"] == QJsonValue("test-double-close.js");
        CHECK(listed);
        CHECK(indicatorById("custom:test-double-close") != nullptr);
        request("DELETE", "/api/custom/test-double-close.js", {}, H);
    }

    test("a Pine Script is recognised and saved as .pine");
    {
        // Saved as .pine whether or not Node.js is there to run it (without, the answer is a 400 naming the file).
        // Both ways of spotting Pine: the //@version line, and an indicator() declaration alone.
        for (const auto &[name, code] : { std::pair<QString, QString> { "Test pine head", "//@version=6\nindicator(\"T\")\nplot(close)\n" },
                                          std::pair<QString, QString> { "Test pine decl", "indicator(\"T\", overlay=true)\nplot(close)\n" } }) {
            const QJsonValue r = post("/api/custom", J(QJsonObject { { "name", name }, { "code", code } })).json();
            const QString file = QString(name).toLower().replace(' ', '-') + ".pine";
            CHECK_EQ(r["file"].toString(), file);
            request("DELETE", "/api/custom/" + file, {}, H);
        }
    }

    test("screener and scan work");
    {
        CHECK(get("/api/screener?universe=crypto").json().toArray().size() > 0);
        CHECK(get("/api/screener?universe=stocks").json().toArray().size() > 0);
        const QJsonValue scan = post("/api/scan", J(obj(R"({"symbols":["BTCUSDT"],"tf":"1D","indicators":{"rsi":{"id":"rsi","params":{"length":14}}},"rules":[{"left":"rsi","op":">","right":0}]})"))).json();
        CHECK_MSG(scan[0]["pass"] == QJsonValue(true), show(scan));
        const QJsonValue bad = post("/api/scan", J(obj(R"({"symbols":["BTCUSDT", 5, "A B?C"],"rules":[{"left":"nope","op":">","right":0}]})"))).json();
        CHECK(bad[0]["error"].toString().startsWith("Unknown operand \"nope\""));
        CHECK_EQ(bad[1]["symbol"].toDouble(), 5.0);
        CHECK(bad[2]["error"].toString().contains("isn't a symbol"));
    }

    test("client mistakes get a 400, not a 500");
    CHECK_EQ(get("/api/candles?symbol=&tf=1D").status, 400);
    CHECK_EQ(get("/api/candles?symbol=BTCUSDT&tf=banana").status, 400);
    CHECK_EQ(get("/api/resolve?symbol=").status, 400);
    CHECK_EQ(get("/%E0%A4%A").status, 400);
    CHECK_EQ(get("/api/%E0%A4%A").status, 400);
    CHECK_EQ(request("PUT", "/api/state", "not json", H).status, 400);
    CHECK_EQ(request("PUT", "/api/state", "[1,2]", H).status, 400);
    CHECK_EQ(request("PUT", "/api/state", "\"x\"", H).json()["error"].toString(), QString("The request body must be a JSON object"));
    CHECK_EQ(post("/api/scan", "{\"symbols\":5,\"rules\":\"x\"}").status, 400);
    CHECK_EQ(post("/api/webhook-test", "{\"url\":\"notaurl\"}").status, 400);

    test("symbols that could reshape an upstream URL are refused");
    for (const QString s : { "BINANCE:BTC&interval=1m", "US:../../admin", ":::", "A B?C", "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx" })
        CHECK_MSG(get("/api/candles?symbol=" + jsv::encodeURIComponent(s) + "&tf=1D").status == 400, s);
    for (const QString s : { "BTCUSDT", "EUR/USD", "^FTSE", "YAHOO:GC=F", "VOD.L", "BRK-B" })
        CHECK_MSG(get("/api/resolve?symbol=" + jsv::encodeURIComponent(s)).status == 200, s);

    test("an opaque Origin is refused with 403, not a crash");
    CHECK_EQ(request("PUT", "/api/state", "{}", { { "x-qt", "1" }, { "Content-Type", "application/json" }, { "Origin", "null" } }).status, 403);
    CHECK_EQ(request("PUT", "/api/state", "{}", { { "x-qt", "1" }, { "Origin", QStringLiteral("http://127.0.0.1:%1").arg(g_port).toUtf8() } }).status, 200);
    CHECK_EQ(request("GET", "/api/health", {}, { { "Host", "evil.example:80" } }).status, 403);

    test("alerts with a bad operator, operand or body are refused up front");
    {
        auto postAlert = [](const QJsonValue &b) { return post("/api/alerts", J(b)).status; };
        CHECK_EQ(postAlert("x"), 400);
        CHECK_EQ(postAlert(obj(R"({"symbol":"BTCUSDT","right":1,"op":"bogus"})")), 400);
        CHECK_EQ(postAlert(obj(R"({"symbol":"BTCUSDT","right":1,"op":">","left":"nope"})")), 400);
        CHECK_EQ(postAlert(obj(R"({"symbol":"BTCUSDT","op":">","left":"rsi","right":70,"indicators":{"rsi":{"id":"rsi","params":{"length":14}}}})")), 200);
        CHECK_EQ(request("PATCH", "/api/alerts/doesnotexist", "{}", H).status, 404);
        CHECK_EQ(post("/api/alerts", J(obj(R"({"symbol":"BTCUSDT","right":1,"op":"bogus"})"))).json()["error"].toString(),
                 QString("Unknown operator \"bogus\". Use one of >, <, >=, <=, crossesAbove, crossesBelow, crosses"));
        for (const QJsonValue a : get("/api/alerts").json().toArray()) request("DELETE", "/api/alerts/" + a["id"].toString(), {}, H);
    }

    test("a \"once\" alert fires once even when checks overlap");
    {
        const QJsonValue a = post("/api/alerts", J(obj(R"({"symbol":"BINANCE:BTCUSDT","tf":"1D","left":"close","op":">","right":1})"))).json();
        concurrently(5, [] { post("/api/alerts/check"); });
        int n = 0;
        for (const QJsonValue e : get("/api/alerts/log").json().toArray()) n += e["alertId"] == a["id"];
        CHECK_EQ(n, 1);
        request("DELETE", "/api/alerts/" + a["id"].toString(), {}, H);
    }

    test("static files cannot escape their folder");
    for (const QString u : { "/..%2F..%2Fserver.js", "/custom/..%2Fserver.js", "/custom/..%2F..%2F..%2Fetc%2Fpasswd", "/..%5cserver.js", "/custom/../../x" }) {
        const int s = get(u).status;
        CHECK_MSG(s == 403 || s == 404, u + " -> " + QString::number(s));
    }

    test("POST /api/shutdown answers ok and the backend keeps running (deepwater owns it)");
    CHECK_EQ(request("POST", "/api/shutdown").status, 403);
    CHECK_EQ(request("POST", "/api/shutdown", {}, { { "x-qt", "1" } }).status, 200);
    CHECK_EQ(get("/api/health").status, 200);

    test("slash notation: pairs stay pairs, two symbols make a ratio");
    {
        auto r = [](const QString &s) { return get("/api/resolve?symbol=" + jsv::encodeURIComponent(s)).json()["id"].toString(); };
        CHECK_EQ(r("BTC/USDT"), QString("BINANCE:BTCUSDT"));
        CHECK_EQ(r("EUR/USD"), QString("FX:EURUSD"));
        CHECK_EQ(r("BTCUSDT/ETHUSDT"), QString("SPREAD:BINANCE:BTCUSDT / BINANCE:ETHUSDT"));
        const QJsonValue sp = get("/api/candles?symbol=" + jsv::encodeURIComponent("BTCUSDT/ETHUSDT") + "&tf=1D&limit=20").json();
        CHECK_EQ(sp["candles"].toArray().size(), qsizetype(20));
        CHECK_MSG(std::fabs(sp["candles"].toArray().last()["close"].toDouble() - 1) < 1e-9, "offline fixture serves the same bars for both sides");
        CHECK_EQ(sp["provider"].toString(), QString("fixture + fixture"));
        const QJsonValue k = get("/api/candles?symbol=" + jsv::encodeURIComponent("2 * BTCUSDT") + "&tf=1D&limit=10").json();
        CHECK_EQ(k["provider"].toString(), QString("fixture"));
        CHECK_EQ(k["candles"].toArray().size(), qsizetype(10));
    }

    test("a watchlist alert checks every symbol on the list, once each");
    {
        request("PUT", "/api/state", J(obj(R"({"watchlists":{"Crypto":["BINANCE:BTCUSDT","BINANCE:ETHUSDT"]}})")), H);
        const QJsonValue a = post("/api/alerts", J(obj(R"({"watchlist":"Crypto","tf":"1D","left":"close","op":">","right":0})"))).json();
        CHECK_EQ(a["watchlist"].toString(), QString("Crypto"));
        post("/api/alerts/check");
        post("/api/alerts/check");
        QStringList fired;
        bool named = true;
        for (const QJsonValue e : get("/api/alerts/log").json().toArray())
            if (e["alertId"] == a["id"]) {
                fired << e["symbol"].toString();
                named = named && e["text"].toString().contains(e["symbol"].toString());
            }
        fired.sort();
        CHECK_EQ(fired, (QStringList { "BINANCE:BTCUSDT", "BINANCE:ETHUSDT" }));
        CHECK_MSG(named, "each message names its symbol");
        CHECK_EQ(post("/api/alerts", J(obj(R"({"tf":"1D","left":"close","op":">","right":0})"))).status, 400);
        request("DELETE", "/api/alerts/" + a["id"].toString(), {}, H);
    }

    test("multi-condition alerts: all must hold, or any, and five at most");
    {
        auto mk = [](const char *body) {
            QJsonObject b = obj(body);
            b.insert("symbol", "BTCUSDT");
            b.insert("tf", "1D");
            b.insert("trigger", "once");
            return post("/api/alerts", J(b));
        };
        const QJsonValue all = mk(R"({"conditions":[{"left":"close","op":">","right":1},{"left":"close","op":"<","right":1}],"match":"all"})").json();
        const QJsonValue any = mk(R"({"conditions":[{"left":"close","op":">","right":1},{"left":"close","op":"<","right":1}],"match":"any"})").json();
        const QJsonValue line = mk(R"({"conditions":[{"left":"close","op":">","right":{"line":{"t1":0,"p1":1,"t2":1,"p2":1,"extend":"right"}}}]})").json();
        post("/api/alerts/check");
        bool fAll = false, fAny = false, fLine = false;
        QString lineText;
        for (const QJsonValue e : get("/api/alerts/log").json().toArray()) {
            fAll = fAll || e["alertId"] == all["id"];
            fAny = fAny || e["alertId"] == any["id"];
            if (e["alertId"] == line["id"]) {
                fLine = true;
                lineText = e["text"].toString();
            }
        }
        CHECK_MSG(!fAll, "all: one false condition blocks it");
        CHECK_MSG(fAny, "any: one true condition fires it");
        CHECK_MSG(fLine, "a drawn line works as a condition");
        CHECK(lineText.startsWith("Alert: BTCUSDT 1D: close is above your drawn line (price "));
        QJsonArray six;
        for (int i = 0; i < 6; ++i) six.append(obj(R"({"left":"close","op":">","right":1})"));
        QJsonObject b { { "symbol", "BTCUSDT" }, { "tf", "1D" }, { "conditions", six } };
        CHECK_EQ(post("/api/alerts", J(b)).status, 400);
        // PATCH keeps the id and validates the merged alert.
        const QJsonValue patched = request("PATCH", "/api/alerts/" + all["id"].toString(), J(obj(R"({"active":false,"id":"other"})")), H).json();
        CHECK_EQ(patched["id"], all["id"]);
        CHECK_EQ(patched["active"].toBool(), false);
        CHECK_EQ(request("PATCH", "/api/alerts/" + all["id"].toString(), J(obj(R"({"match":"some"})")), H).status, 400);
        for (const QJsonValue a : { all, any, line }) request("DELETE", "/api/alerts/" + a["id"].toString(), {}, H);
    }

    test("the alert event stream: retry, then an `alert` event (HTTP/1.1 and HTTP/1.0 clients)");
    for (const QByteArray version : { QByteArray("HTTP/1.1"), QByteArray("HTTP/1.0") }) {
        QTcpSocket sock;
        sock.connectToHost(QHostAddress::LocalHost, quint16(g_port));
        CHECK(sock.waitForConnected(3000));
        sock.write("GET /api/events " + version + "\r\nHost: 127.0.0.1:" + QByteArray::number(g_port) + "\r\n\r\n");
        QByteArray got;
        QElapsedTimer t;
        t.start();
        while (!got.contains("retry: 3000") && t.elapsed() < 3000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            got += sock.readAll();
        }
        CHECK_MSG(got.contains("retry: 3000"), QString::fromUtf8(got));
        CHECK(got.contains("text/event-stream"));
        std::atomic<int> signalled { 0 };
        auto conn = QObject::connect(&backend, &Backend::alertFired, [&](const QJsonObject &) { ++signalled; });
        const QJsonValue a = post("/api/alerts", J(obj(R"({"symbol":"BINANCE:BTCUSDT","tf":"1D","left":"close","op":">","right":1,"message":"stream test"})"))).json();
        post("/api/alerts/check");
        t.restart();
        while (!got.contains("event: alert") && t.elapsed() < 3000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            got += sock.readAll();
        }
        CHECK_MSG(got.contains("event: alert\ndata: {"), QString::fromUtf8(got));
        CHECK(got.contains("\"text\":\"Alert: stream test (price "));
        CHECK(signalled > 0);
        QObject::disconnect(conn);
        request("DELETE", "/api/alerts/" + a["id"].toString(), {}, H);
        sock.close();
    }

    test("webhooks: Discord, Slack and Telegram get their own format, any other URL { text, content, alert, event }");
    {
        http::setFetchMock([](const QString &url, const http::FetchOptions &o) -> std::optional<http::FetchResponse> {
            if (isLocal(url)) return std::nullopt;
            record(url, o);
            http::FetchResponse r;
            r.status = url.contains("fail.example") ? 500 : url.contains("discord") ? 204 : 200;
            return r;
        });
        clearCalls();
        const QStringList hooks = { "https://discord.com/api/webhooks/1/x", "https://hooks.slack.com/services/T/B/X",
                                    "https://api.telegram.org/botX/sendMessage?chat_id=1", "https://my.example/hook", "https://fail.example/hook" };
        QStringList ids;
        for (const QString &h : hooks)
            ids << post("/api/alerts", J(QJsonObject { { "symbol", "BINANCE:BTCUSDT" }, { "tf", "1D" }, { "left", "close" }, { "op", ">" },
                                                        { "right", 1 }, { "webhook", h } })).json()["id"].toString();
        post("/api/alerts/check");
        QHash<QString, QJsonValue> sent;
        for (const auto &c : calls()) sent.insert(c.host, jsv::parse(c.body));
        QHash<QString, QJsonValue> events;
        for (const QJsonValue e : get("/api/alerts/log").json().toArray()) events.insert(e["alertId"].toString(), e);
        const QString text = events.value(ids[0])["text"].toString();
        CHECK(text.startsWith("Alert: BINANCE:BTCUSDT 1D: close is above 1 (price "));
        CHECK_JSON(sent.value("discord.com"), (QJsonObject { { "content", text } }));
        CHECK_JSON(sent.value("hooks.slack.com"), (QJsonObject { { "text", text } }));
        CHECK_JSON(sent.value("api.telegram.org"), (QJsonObject { { "text", text } }));
        const QJsonValue generic = sent.value("my.example");
        CHECK_EQ(generic.toObject().keys(), (QStringList { "alert", "content", "event", "text" }));
        CHECK_EQ(generic["event"]["alertId"].toString(), ids[3]);
        CHECK(generic["event"]["webhook"].isUndefined());
        CHECK_EQ(generic["alert"]["fired"].toInt(), 1);
        CHECK_EQ(generic["alert"]["active"].toBool(), false);
        CHECK_EQ(events.value(ids[0])["webhook"].toString(), QString("sent"));
        CHECK_EQ(events.value(ids[4])["webhook"].toString(), QString("failed: Webhook returned HTTP 500"));
        const QJsonValue t = post("/api/webhook-test", J(QJsonObject { { "url", "https://fail.example/x" } })).json();
        CHECK_JSON(t, (QJsonObject { { "ok", false }, { "status", 500 } }));
        CHECK_JSON(jsv::parse(calls().last().body), (QJsonObject { { "text", "Quant Terminal test alert" }, { "content", "Quant Terminal test alert" } }));
        for (const QString &id : ids) request("DELETE", "/api/alerts/" + id, {}, H);
        http::setFetchMock({});
    }

    test("unknown endpoints and methods are a JSON 404");
    {
        const Res r = get("/api/nope");
        CHECK_EQ(r.status, 404);
        CHECK_EQ(QString::fromUtf8(r.body), QString("{\"error\":\"Unknown endpoint\"}"));
        CHECK_EQ(request("POST", "/api/state", "{}", H).status, 404);
        CHECK_EQ(get("/api/alerts/check").status, 404);
        CHECK_EQ(get("/api/ptq/arms/x/disarm").status, 404);
    }

    test("in-process handle() is the same router (what the Charts page uses)");
    {
        const Response r = backend.handle("GET", "/api/resolve", { { "symbol", "eurusd" } }, {});
        CHECK_EQ(r.status, 200);
        CHECK_EQ(r.json["id"].toString(), QString("FX:EURUSD"));
        const Response e = backend.handle("GET", "/api/candles", { { "symbol", "BTCUSDT" }, { "tf", "banana" } }, {});
        CHECK_EQ(e.status, 400);
        CHECK(e.json["error"].toString().startsWith("Bad timeframe \"banana\""));
        const CandleSet c = backend.candles("BTCUSDT", "1D", 30);
        CHECK_EQ(c.candles.size(), qsizetype(30));
        CHECK_EQ(c.provider, QString("fixture"));
    }
}

static void testImportsHttp(const QString &customDir)
{
    const bool stubBacktest = false, stubQuant = false;   // the stand-ins are gone; kept for the branches below
    const auto minute = minuteBars(600);
    const QString csv = alpacaCsv(600);

    test("times: ISO with or without a zone, Unix seconds or milliseconds, dotted dates with a separate time");
    {
        const double t = jsv::dateUtc(2026, 9, 14, 14, 30) / 1000;
        for (const QString s : { "2026-10-14 14:30:00", "2026-10-14T14:30:00Z", "2026-10-14T14:30:00.000Z", "2026-10-14 14:30:00+00:00", "2026-10-14T10:30:00-04:00" })
            CHECK_MSG(imports::parseTime(s) == t, s);
        CHECK_EQ(imports::parseTime(N(t)), t);
        CHECK_EQ(imports::parseTime(N(t * 1000)), t);
        const QString tod = "14:30";
        CHECK_EQ(imports::parseTime("2026.10.14", &tod), t);
        CHECK_EQ(imports::parseTime("2026-10-14"), jsv::dateUtc(2026, 9, 14) / 1000);
        CHECK(std::isnan(imports::parseTime("14/10/2026")));
    }

    test("an Alpaca 1-minute export parses, and its timeframe is found from the bars");
    {
        const ParsedBars p = imports::parseBars(csv);
        CHECK_EQ(p.bars.size(), qsizetype(600));
        CHECK_EQ(p.baseSeconds, 60.0);
        CHECK_EQ(p.bars[0].time, minute[0].time);
        CHECK_EQ(p.bars[0].close, minute[0].close);
        CHECK_EQ(p.bars[0].volume, minute[0].volume);
    }

    test("imported bars chart as CSV:NAME on their own timeframe and longer, never shorter");
    {
        const QJsonValue r = request("POST", "/api/import/bars?name=" + jsv::encodeURIComponent("btc 1m (alpaca).csv"), csv.toUtf8(), { { "x-qt", "1" } }).json();
        CHECK_EQ(r["symbol"].toString(), QString("CSV:BTC_1M_ALPACA"));
        CHECK_EQ(r["tf"].toString(), QString("1m"));
        CHECK_EQ(r["bars"].toInt(), 600);
        const QJsonValue m1 = get("/api/candles?symbol=CSV:BTC_1M_ALPACA&tf=1m&limit=1000").json();
        CHECK_EQ(m1["candles"].toArray().size(), qsizetype(600));
        CHECK(m1["provider"].toString().contains("Your file BTC_1M_ALPACA.csv"));
        const QJsonValue m5 = get("/api/candles?symbol=CSV:BTC_1M_ALPACA&tf=5m&limit=1000").json();
        CHECK_EQ(m5["candles"].toArray().size(), qsizetype(120));
        double hi = -INFINITY, lo = INFINITY, vol = 0;
        for (int i = 0; i < 5; ++i) {
            hi = std::max(hi, minute[i].high);
            lo = std::min(lo, minute[i].low);
            vol += minute[i].volume;
        }
        CHECK_JSON(m5["candles"][0], (QJsonObject { { "time", minute[0].time }, { "open", minute[0].open }, { "high", hi }, { "low", lo },
                                                     { "close", minute[4].close }, { "volume", vol } }));
        CHECK(m5["notes"][0].toString().contains("built from its 1m bars"));
        const Res s30 = get("/api/candles?symbol=CSV:BTC_1M_ALPACA&tf=30s");
        CHECK_EQ(s30.status, 400);
        CHECK(s30.json()["error"].toString().contains("BTC_1M_ALPACA has 1m bars, so it charts at 1m and longer"));
        bool listed = false;
        for (const QJsonValue x : get("/api/imports").json().toArray())
            listed = listed || (x["symbol"] == QJsonValue("CSV:BTC_1M_ALPACA") && x["bars"].toInt() == 600);
        CHECK(listed);
        bool found = false;
        for (const QJsonValue x : get("/api/search?q=btc_1m").json().toArray()) found = found || x["symbol"] == QJsonValue("CSV:BTC_1M_ALPACA");
        CHECK(found);
        CHECK_EQ(get("/api/search?q=btc_1m").json()[0]["name"].toString(), QString("Your file, 600 1m bars"));
        CHECK_EQ(request("POST", "/api/import/bars?name=x", "a,b\n1,2", { { "x-qt", "1" } }).status, 400);
        CHECK_EQ(request("POST", "/api/import/bars", csv.toUtf8(), { { "x-qt", "1" } }).json()["error"].toString(),
                 QString("Give the file a name made of letters or digits"));
    }

    test("a signal becomes a valid indicator: each bar takes the position set by its close");
    {
        QStringList rows { "time,position" };
        for (const auto &[k, p] : QVector<QPair<int, int>> { { 0, 0 }, { 10, 1 }, { 17, -1 }, { 22, 0 } })
            rows << N(minute[k].time) + ',' + QString::number(p);
        const Signals s = imports::parseSignals(rows.join('\n'));
        QTemporaryDir dir;
        const QString file = dir.filePath("signal-probe.js");
        writeFile(file, imports::signalIndicatorSource("TEST", s, "test.csv").toUtf8());
        bool loaded = true;
        try {
            loadCustomIndicator(file, "custom:signal-probe");
        } catch (const Error &e) {
            loaded = false;
            fail("loadCustomIndicator: " + e.message(), __FILE__, __LINE__);
        }
        if (loaded) {
            Bars bars;
            for (int i = 0; i < 30; ++i) bars << Bar { minute[i].time, minute[i].open, minute[i].high, minute[i].low, minute[i].close, minute[i].volume, {} };
            const Values pos = compute("custom:signal-probe", bars).values.value("position");
            CHECK(pos.size() == 30 && pos[9] == 0 && pos[10] == 1 && pos[16] == 1 && pos[17] == -1 && pos[22] == 0);
            const Values five = compute("custom:signal-probe", resample(bars, "5m")).values.value("position");
            CHECK_EQ(five, (Values { 0, 0, 1, -1, 0, 0 }));
        }
    }

    test("an imported signal backtests exactly as written, and the quant checks run on it");
    {
        QStringList rows { "time,position" };
        for (const auto &[k, p] : QVector<QPair<int, int>> { { 0, 0 }, { 100, 1 }, { 300, -1 }, { 450, 0 } })
            rows << jsv::isoString(minute[k].time * 1000) + ',' + QString::number(p);
        const QJsonValue r = request("POST", "/api/import/signal?name=test%20v10", rows.join('\n').toUtf8(), { { "x-qt", "1" } }).json();
        CHECK_EQ(r["id"].toString(), QString("custom:signal-test_v10"));
        CHECK_EQ(r["changes"].toInt(), 4);
        CHECK_EQ(r["file"].toString(), QString("signal-test_v10.js"));
        CHECK(QFile::exists(QDir(customDir).filePath("signal-test_v10.js")));
        if (stubBacktest) {
            skip("core/Backtest.cpp isn't there yet");
        } else {
            QJsonObject strategy = r["strategy"].toObject();
            strategy.insert("feePct", 0);
            strategy.insert("slippagePct", 0);
            const QJsonValue bt = post("/api/backtest", J(QJsonObject { { "symbol", "CSV:BTC_1M_ALPACA" }, { "tf", "1m" }, { "limit", 1000 },
                                                                        { "strategy", strategy }, { "quantChecks", !stubQuant } })).json();
            const QJsonArray trades = bt["trades"].toArray();
            CHECK_EQ(trades.size(), qsizetype(2));
            if (trades.size() == 2) {
                CHECK_EQ(trades[0]["side"].toString(), QString("long"));
                CHECK_EQ(trades[0]["entryTime"].toDouble(), minute[101].time);
                CHECK_EQ(trades[0]["exitTime"].toDouble(), minute[301].time);
                CHECK_EQ(trades[1]["side"].toString(), QString("short"));
                CHECK_EQ(trades[1]["exitTime"].toDouble(), minute[451].time);
                CHECK_EQ(trades[0]["entryPrice"].toDouble(), minute[101].open);
            }
            bool cost = false;
            for (const QJsonValue g : bt["quantChecks"]["gates"].toArray()) cost = cost || g["label"] == QJsonValue("Cost stress");
            if (!stubQuant) CHECK(cost);
        }
        const Res bad = request("POST", "/api/import/signal?name=broken", "time,position\n2026-10-14,9\n", { { "x-qt", "1" } });
        CHECK_EQ(bad.status, 400);
        CHECK(!QFile::exists(QDir(customDir).filePath("signal-broken.js")));
    }
}

// ---------------------------------------------------------------- ptq.test.js

class FakePtq : public QObject
{
public:
    struct Seen {
        QString method, url, auth;
        QJsonValue body;
    };
    QMutex mutex;
    QVector<Seen> seen;
    bool killSwitch = false;
    QHttpServer *server = nullptr;
    QTcpServer *tcp = nullptr;
    int port = 0;

    void startHere()
    {
        server = new QHttpServer(this);
        server->setMissingHandler(this, [this](const QHttpServerRequest &req, QHttpServerResponder &r) { onRequest(req, r); });
        tcp = new QTcpServer(this);
        tcp->listen(QHostAddress::LocalHost, 0);
        server->bind(tcp);
        port = tcp->serverPort();
    }
    QVector<Seen> all()
    {
        QMutexLocker l(&mutex);
        return seen;
    }

private:
    void onRequest(const QHttpServerRequest &req, QHttpServerResponder &r)
    {
        const QString method = req.method() == QHttpServerRequest::Method::Post ? "POST" : "GET";
        const QJsonValue body = req.body().isEmpty() ? QJsonValue(QJsonValue::Undefined) : jsv::parse(req.body());
        const QString url = req.url().path() + (req.url().hasQuery() ? "?" + req.url().query(QUrl::FullyEncoded) : QString());
        {
            QMutexLocker l(&mutex);
            seen.push_back({ method, url, QString::fromUtf8(req.headers().value(QHttpHeaders::WellKnownHeader::Authorization).toByteArray()), body });
        }
        auto send = [&](int code, const QJsonValue &o) {
            QHttpHeaders h;
            h.append(QHttpHeaders::WellKnownHeader::ContentType, "application/json");
            r.write(jsv::stringify(o), h, QHttpServerResponder::StatusCode(code));
        };
        const QString p = req.url().path();
        if (p == "/api/health") return send(200, obj(R"({"ok":true,"version":"9.9.9","engine":"real"})"));
        if (p == "/api/markets")
            return send(200, obj(R"({"items":[{"id":"BITGET:SPOT:BTCUSDT","history":{"status":"available"}},{"id":"YAHOO:STOCKS:AAPL","history":{"status":"available"}}]})"));
        if (p == "/api/strategies" && method == "POST") return send(200, body);
        if (p == "/api/backtest/run") return send(200, obj(R"({"id":"bt_1","provenance":"real-engine","metrics":{"netReturnPct":4.2,"sharpe":0.6,"trades":30,"maxDrawdownPct":5}})"));
        if (p == "/api/backtest/bt_1/validate")
            return send(200, obj(R"J({"verdict":"block","pboPct":71,"deflatedSharpe":0.5,"gates":[{"key":"pbo","label":"Overfitting (PBO)","value":"71%","pass":false}]})J"));
        if (p == "/api/portfolio") return send(200, obj(R"({"connected":false,"positions":[],"message":"No broker connected"})"));
        if (p == "/api/execution/strategy-arms") return send(200, QJsonArray());
        if (p == "/api/autopilot") return send(200, obj(R"({"running":true})"));
        if (p == "/api/journal") return send(200, QJsonArray());
        if (p == "/api/execution/kill-switch" && method == "POST") {
            killSwitch = body["engaged"].toBool();
            return send(200, QJsonObject { { "engaged", killSwitch } });
        }
        if (p == "/api/execution/kill-switch") return send(200, QJsonObject { { "engaged", killSwitch } });
        if (QRegularExpression("/api/execution/strategy-arms/.+/disarm$").match(p).hasMatch()) return send(200, obj(R"({"ok":true})"));
        return send(404, obj(R"({"error":"not in fake"})"));
    }
};

static void testPtq(const QString &dataDir, const QString &customDir)
{
    QThread thread;
    thread.start();
    FakePtq fake;
    fake.moveToThread(&thread);
    QMetaObject::invokeMethod(&fake, [&] { fake.startHere(); }, Qt::BlockingQueuedConnection);
    QTemporaryDir home;
    writeFile(home.filePath("license.json"), J(QJsonObject { { "key", "TOS-x" }, { "lease", "lease-token" }, { "exp", jsv::nowMs() + 3600e3 } }));
    qputenv("PTQ_URL", QStringLiteral("http://127.0.0.1:%1").arg(fake.port).toUtf8());
    qputenv("PTQ_HOME", home.path().toUtf8());
    int port = 0;
    {
        Backend backend(dataDir, customDir);
        port = backend.startHttp(quint16(41000 + QRandomGenerator::global()->bounded(1000)), 20);
        auto preq = [&](const QByteArray &m, const QString &path, const QByteArray &body = {}, QList<QPair<QByteArray, QByteArray>> h = H) {
            return request(m, path, body, h, port);
        };

        test("the EMA cross example becomes a PTQ composer manifest");
        {
            const auto t = Ptq::translate(exampleStrategy("EMA cross (9/21)"), "1D", "EMA");
            CHECK(t.problems.isEmpty());
            CHECK_EQ(t.manifest.value("entryLogic").toString(), QString("composed"));
            CHECK_EQ(t.manifest.value("signalTimeframe").toString(), QString("1d"));
            CHECK_JSON(t.manifest["composed"]["entry"]["conditions"][0],
                       obj(R"({"kind":"comparison","left":{"indicator":"ema","params":{"length":9}},"op":"crosses_above","right":{"indicator":"ema","params":{"length":21}}})"));
            CHECK_EQ(t.manifest["composed"]["exit"]["conditions"][0]["op"].toString(), QString("crosses_below"));
            CHECK(QRegularExpression("^strat_qt_[0-9a-f]{10}$").match(t.manifest.value("id").toString()).hasMatch());
        }

        test("anything PTQ cannot evaluate is refused with a reason, never approximated");
        {
            bool st = false;
            for (const auto &p : Ptq::translate(exampleStrategy("Supertrend follow"), "1D", {}).problems)
                st = st || p.contains("Supertrend isn't in PTQ's strategy builder");
            CHECK(st);
            bool tf = false;
            for (const auto &p : Ptq::translate(exampleStrategy("EMA cross (9/21)"), "7m", {}).problems) tf = tf || p.contains("PTQ tests on");
            CHECK(tf);
            CHECK(!Ptq::toTerm("hl2").error.isEmpty());
            CHECK(!Ptq::toTerm("fast", obj(R"({"fast":{"id":"ema","params":{"length":900}}})")).error.isEmpty());
        }

        test("status reports the connection and the lease is forwarded");
        {
            const QJsonValue s = preq("GET", "/api/ptq/status").json();
            CHECK_EQ(s["connected"].toBool(), true);
            CHECK_EQ(s["licensed"].toBool(), true);
            CHECK_EQ(s["version"].toString(), QString("9.9.9"));
            QString auth;
            for (const auto &x : fake.all())
                if (x.url == "/api/health") auth = x.auth;
            CHECK_EQ(auth, QString("Bearer lease-token"));
        }

        test("check creates the strategy, backtests it and validates it in PTQ");
        {
            const QJsonValue r = preq("POST", "/api/ptq/check", J(QJsonObject { { "strategy", exampleStrategy("EMA cross (9/21)") }, { "symbol", "BINANCE:BTCUSDT" },
                                                                                { "tf", "1D" }, { "name", "EMA" } })).json();
            CHECK_EQ(r["validation"]["verdict"].toString(), QString("block"));
            CHECK_EQ(r["instrument"].toString(), QString("BITGET:SPOT:BTCUSDT"));
            FakePtq::Seen created;
            for (const auto &x : fake.all())
                if (x.url == "/api/strategies" && x.method == "POST") created = x;
            CHECK_EQ(created.body["instrument"].toString(), QString("BITGET:SPOT:BTCUSDT"));
            CHECK(r["links"]["strategy"].toString().contains(jsv::encodeURIComponent(created.body["id"].toString())));
            CHECK_EQ(r["metrics"]["trades"].toInt(), 30);
            CHECK(r["validationError"].isNull());
        }

        test("an untranslatable strategy answers 422 with the reasons");
        {
            const Res res = preq("POST", "/api/ptq/check", J(QJsonObject { { "strategy", exampleStrategy("Supertrend follow") }, { "symbol", "BINANCE:BTCUSDT" }, { "tf", "1D" } }));
            CHECK_EQ(res.status, 422);
            CHECK(res.json()["problems"].toArray().size() > 0);
            CHECK_EQ(res.json()["code"].toString(), QString("translate"));
            const Res inst = preq("POST", "/api/ptq/check", J(QJsonObject { { "strategy", exampleStrategy("EMA cross (9/21)") }, { "symbol", "YAHOO:^FTSE" }, { "tf", "1D" } }));
            CHECK_EQ(inst.status, 422);
            CHECK_EQ(inst.json()["code"].toString(), QString("instrument"));
        }

        test("the terminal can only engage PTQ's stop, never release it");
        preq("POST", "/api/ptq/kill-switch", J(obj(R"({"engaged":false})")));
        CHECK_EQ(fake.killSwitch, true);
        CHECK_EQ(preq("POST", "/api/ptq/kill-switch", {}, {}).status, 403);

        test("there is no route to arm a strategy or place an order");
        for (const QString p : { "/api/ptq/arm", "/api/ptq/strategies/x/arm", "/api/ptq/execution/submit", "/api/ptq/order" })
            CHECK_MSG(preq("POST", p, "{}").status == 404, p);
        bool armed = false;
        for (const auto &x : fake.all())
            armed = armed || (QRegularExpression("arm$|submit").match(x.url).hasMatch() && !x.url.contains("disarm"));
        CHECK(!armed);
        CHECK_EQ(preq("POST", "/api/ptq/arms/" + jsv::encodeURIComponent("strat a") + "/disarm", "{}").json()["ok"].toBool(), true);
        CHECK_EQ(preq("POST", "/api/ptq/arms/" + jsv::encodeURIComponent("strat a/b") + "/disarm", "{}").status, 404);

        test("overview gathers portfolio, arms, autopilot, stop and journal");
        {
            const QJsonValue o = preq("GET", "/api/ptq/overview").json();
            CHECK_EQ(o["autopilot"]["running"].toBool(), true);
            CHECK(o["arms"].isArray());
            CHECK_EQ(o["portfolio"]["connected"].toBool(), false);
            CHECK(o["killSwitch"]["engaged"].toBool());
        }
    }

    test("with PTQ not running, status says so plainly");
    {
        const QJsonObject s = Ptq("http://127.0.0.1:9", home.path()).status();
        CHECK_EQ(s.value("connected").toBool(), false);
        CHECK(s.value("reason").toString().contains("ptq serve"));
    }

    test("either/or exits go to PTQ as an OR group, and the time exit as whole hours");
    {
        QJsonObject s = exampleStrategy("EMA cross (9/21)");
        s.insert("exitLong", jsv::parse(R"([{"left":"fast","op":"crossesBelow","right":"slow"},{"left":"close","op":"<","right":"slow"}])"));
        s.insert("exitMatch", "any");
        s.insert("maxBarsInTrade", 10);
        const auto h = Ptq::translate(s, "1h", {});
        CHECK_EQ(h.manifest["composed"]["exit"]["op"].toString(), QString("OR"));
        CHECK_EQ(h.manifest["maxHoldingHours"].toDouble(), 10.0);
        const auto q = Ptq::translate(s, "15m", {});
        CHECK_EQ(q.manifest["maxHoldingHours"].toDouble(), 3.0);
        bool note = false;
        for (const auto &n : q.notes) note = note || n.contains("3 hours");
        CHECK(note);
        CHECK_EQ(QJsonValue(Ptq::translate(exampleStrategy("EMA cross (9/21)"), "1h", {}).manifest)["composed"]["exit"]["op"].toString(), QString("AND"));
        CHECK_EQ(QJsonValue(Ptq::translate(exampleStrategy("EMA cross (9/21)"), "1h", {}).manifest)["maxHoldingHours"].toDouble(), 0.0);
    }

    QMetaObject::invokeMethod(&fake, [&] {
        delete fake.server;
        fake.server = nullptr;
        fake.moveToThread(QCoreApplication::instance()->thread());
    }, Qt::BlockingQueuedConnection);
    thread.quit();
    thread.wait();
    qunsetenv("PTQ_URL");
    qunsetenv("PTQ_HOME");
}

// ---------------------------------------------------------------- main

// ---------------------------------------------------------------- audit regressions (server audit, Oct 2026)

// A raw GET, for the response headers the fetch client doesn't return.
static QByteArray rawGet(const QString &path)
{
    QTcpSocket sock;
    sock.connectToHost(QHostAddress::LocalHost, quint16(g_port));
    if (!sock.waitForConnected(3000)) return {};
    sock.write("GET " + path.toUtf8() + " HTTP/1.0\r\nHost: 127.0.0.1:" + QByteArray::number(g_port) + "\r\n\r\n");
    QByteArray got;
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < 3000 && sock.state() == QAbstractSocket::ConnectedState) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        got += sock.readAll();
        if (got.contains("\r\n\r\n") && got.endsWith("}")) break;
    }
    return got + sock.readAll();
}

static void testAuditJson()
{
    test("res.json() fails with V8's SyntaxError text");
    // Expected texts written by Node 26 (JSON.parse on the same input).
    const QVector<QPair<QByteArray, QString>> cases = {
        { "ok", "Unexpected token 'o', \"ok\" is not valid JSON" },
        { "<!DOCTYPE html><html>", "Unexpected token '<', \"<!DOCTYPE \"... is not valid JSON" },
        { "Too Many Requests", "Unexpected token 'T', \"Too Many Requests\" is not valid JSON" },
        { "            <html><body>hello world</body></html>", "Unexpected token '<', ...\"          <html><bod\"... is not valid JSON" },
        { "               x123456789012345", "Unexpected token 'x', ...\"          x123456789\"... is not valid JSON" },
        { QByteArray(25, ' ') + "x", "Unexpected token 'x', ...\"          x\" is not valid JSON" },
        { "nope", "Unexpected token 'o', \"nope\" is not valid JSON" },
        { "nulx", "Unexpected token 'x', \"nulx\" is not valid JSON" },
        { "nul", "Unexpected end of JSON input" },
        { "true1", "Unexpected non-whitespace character after JSON at position 4 (line 1 column 5)" },
        { "   true  false", "Unexpected non-whitespace character after JSON at position 9 (line 1 column 10)" },
        { "", "Unexpected end of JSON input" },
        { " \n ", "Unexpected end of JSON input" },
    };
    for (const auto &[body, want] : cases) {
        http::FetchResponse r;
        r.status = 200;
        r.body = body;
        QString got;
        try {
            r.json();
            got = "(parsed)";
        } catch (const Error &e) {
            got = e.message();
        }
        CHECK_EQ(got, want);
    }
}

static void testAuditHttp(const QString &customDir)
{
    test("webhook URLs: JS's \\S and an end-only $ (a trailing newline or a no-break space is refused)");
    for (const char *hook : { "https://x\n", "https://x y", "ftp://x" }) {
        const Res r = post("/api/alerts", J(QJsonObject { { "symbol", "BINANCE:BTCUSDT" }, { "right", 1 }, { "webhook", hook } }));
        CHECK_EQ(r.status, 400);
        CHECK_EQ(r.json()["error"].toString(), QStringLiteral("Webhook must be an http(s) URL"));
        CHECK_EQ(post("/api/webhook-test", J(QJsonObject { { "url", hook } })).status, 400);
    }
    const Res okHook = post("/api/alerts", J(QJsonObject { { "symbol", "BINANCE:BTCUSDT" }, { "right", 1 }, { "webhook", "HTTPS://ok.example/x" } }));
    CHECK_EQ(okHook.status, 200);
    request("DELETE", "/api/alerts/" + okHook.json()["id"].toString(), {}, H);

    test("alert match is the string \"all\" or \"any\", nothing that merely prints as one");
    for (const QJsonValue m : { QJsonValue(QJsonArray { "all" }), QJsonValue(1), QJsonValue("some") }) {
        const Res r = post("/api/alerts", J(QJsonObject { { "symbol", "BINANCE:BTCUSDT" }, { "right", 1 }, { "match", m } }));
        CHECK_EQ(r.status, 400);
        CHECK_EQ(r.json()["error"].toString(), QStringLiteral("match must be \"all\" or \"any\""));
    }

    test("the webhook test posts { text, content } in that order");
    {
        auto body = std::make_shared<QByteArray>();
        http::setFetchMock([body](const QString &url, const http::FetchOptions &o) -> std::optional<http::FetchResponse> {
            if (isLocal(url)) return std::nullopt;
            *body = o.body;
            return jsonResponse(QJsonObject {}, 204);
        });
        const Res r = post("/api/webhook-test", J(QJsonObject { { "url", "https://hook.test/x" } }));
        http::setFetchMock({});
        CHECK_EQ(r.status, 200);
        CHECK_EQ(QString::fromUtf8(*body), QStringLiteral("{\"text\":\"Quant Terminal test alert\",\"content\":\"Quant Terminal test alert\"}"));
    }

    test("DELETE /api/custom/<file>/ deletes the file (path.basename drops trailing slashes)");
    writeFile(QDir(customDir).filePath("gone.js"), "export default {}");
    CHECK_EQ(request("DELETE", "/api/custom/gone.js/", {}, H).status, 200);
    CHECK(!QFile::exists(QDir(customDir).filePath("gone.js")));
    CHECK_EQ(request("DELETE", "/api/custom/", {}, H).status, 500);

    test("custom/ files are served with server.js's MIME types");
    writeFile(QDir(customDir).filePath("notes.json"), "{}");
    writeFile(QDir(customDir).filePath("notes.txt"), "}");
    CHECK(rawGet("/custom/notes.json").toLower().contains("content-type: application/json"));
    CHECK(rawGet("/custom/notes.txt").toLower().contains("content-type: text/plain; charset=utf-8"));
    QFile::remove(QDir(customDir).filePath("notes.json"));
    QFile::remove(QDir(customDir).filePath("notes.txt"));

    test("PTQ disarm: a path that only ends in /disarm before a newline is not the route");
    CHECK_EQ(request("POST", "/api/ptq/arms/x/disarm%0A", {}, H).status, 404);
}

static void testAuditParsing()
{
    test("JS's whitespace: no-break spaces in a symbol are dropped like \\s, spreads split on them");
    {
        QTemporaryDir dir;
        Feeds feeds(dir.path(), 1000);
        Imports imports(dir.path());
        ProviderOptions o;
        o.offline = true;
        o.fixturesDir = QTM_FIXTURES;
        Providers p(feeds, imports, o);
        CHECK_EQ(p.resolve(QStringLiteral("BTC USDT")).id, QStringLiteral("BINANCE:BTCUSDT"));
        const auto sp = Providers::parseSpread(QStringLiteral("AAPL - MSFT"));
        CHECK(sp.has_value());
        if (sp) CHECK_EQ(sp->op, QStringLiteral("-"));
    }
    test("cleanName: .csv only at the very end (a trailing newline keeps it in the name)");
    CHECK_EQ(imports::cleanName("a.csv\n"), QStringLiteral("A_CSV"));
    CHECK_EQ(imports::cleanName("b.CSV"), QStringLiteral("B"));
}

static void testAuditPolygonNextUrl()
{
    test("Polygon next_url: followed on Polygon's own host (user info ignored), never elsewhere; a bad link throws");
    QTemporaryDir dir;
    writeFile(QDir(dir.path()).filePath("feeds.json"), R"({"polygon":{"apiKey":"POLYKEY"}})");
    Feeds feeds(dir.path(), 1000);
    auto next = std::make_shared<QString>();
    auto hosts = std::make_shared<QStringList>();
    auto mutex = std::make_shared<QMutex>();
    http::setFetchMock([=](const QString &url, const http::FetchOptions &o) -> std::optional<http::FetchResponse> {
        if (isLocal(url)) return std::nullopt;
        QMutexLocker lock(mutex.get());
        QString auth;
        for (const auto &h : o.headers)
            if (h.first.toLower() == "authorization") auth = QString::fromUtf8(h.second);
        hosts->append(QUrl(url).host() + (auth.isEmpty() ? "" : " +key"));
        const bool first = !url.contains("/next");
        QJsonArray rows;
        const double day = jsv::dateUtc(2026, 9, first ? 14 : 13);
        rows.append(QJsonObject { { "t", day }, { "o", 1 }, { "h", 2 }, { "l", 0.5 }, { "c", 1.5 }, { "v", 10 } });
        QJsonObject out { { "results", rows } };
        if (first && !next->isEmpty()) out.insert("next_url", *next);
        return jsonResponse(out);
    });
    *next = "https://someone@api.massive.com/next";
    Bars bars = feeds.polygonCandles("AAPL", "1D", 10);
    CHECK_EQ(int(bars.size()), 2);
    CHECK_EQ(*hosts, (QStringList { "api.massive.com +key", "api.massive.com +key" }));
    hosts->clear();
    *next = "https://evil.example/next";
    bars = feeds.polygonCandles("MSFT", "1D", 10);
    CHECK_EQ(int(bars.size()), 1);
    CHECK_EQ(*hosts, (QStringList { "api.massive.com +key" }));
    *next = "http://[bad";
    QString err;
    try {
        feeds.polygonCandles("NVDA", "1D", 10);
    } catch (const Error &e) {
        err = e.message();
    }
    CHECK_EQ(err, QStringLiteral("Invalid URL"));
    http::setFetchMock({});
}

// A local HTTP/1.1 server on its own thread that answers every request after `delayMs`, keeps the
// connection open, and counts connections.
struct SlowServer {
    QThread thread;
    std::atomic<int> port { 0 }, accepted { 0 }, open { 0 }, maxOpen { 0 };
    int delayMs = 300;
    void start()
    {
        QObject::connect(&thread, &QThread::started, [this] {
            auto *server = new QTcpServer;
            QObject::connect(&thread, &QThread::finished, server, [server] { delete server; }, Qt::DirectConnection);
            QObject::connect(server, &QTcpServer::newConnection, server, [this, server] {
                while (QTcpSocket *s = server->nextPendingConnection()) {
                    ++accepted;
                    const int now = ++open;
                    for (int m = maxOpen; now > m && !maxOpen.compare_exchange_weak(m, now);) {}
                    auto buf = std::make_shared<QByteArray>();
                    QObject::connect(s, &QTcpSocket::readyRead, s, [this, s, buf] {
                        *buf += s->readAll();
                        if (!buf->contains("\r\n\r\n")) return;
                        buf->clear();
                        QTimer::singleShot(delayMs, s, [s] {
                            s->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: 2\r\nConnection: keep-alive\r\n\r\n{}");
                        });
                    });
                    QObject::connect(s, &QTcpSocket::disconnected, s, [this, s] {
                        --open;
                        s->deleteLater();
                    });
                }
            });
            server->listen(QHostAddress::LocalHost, 0);
            port = server->serverPort();
        });
        thread.start();
        while (!port) QThread::msleep(5);
    }
    ~SlowServer()
    {
        thread.quit();
        thread.wait();
    }
};

static void testAuditConnections()
{
    test("one connection pool for every thread, like Node's fetch: keep-alive reused across threads");
    {
        SlowServer srv;
        srv.delayMs = 10;
        srv.start();
        const QString url = QStringLiteral("http://127.0.0.1:%1/x").arg(int(srv.port));
        for (int i = 0; i < 3; ++i) {
            QThread *t = QThread::create([url] { http::fetch(url, {}); });
            t->start();
            t->wait();
            delete t;
        }
        CHECK_EQ(int(srv.accepted), 1);
    }
    test("no 6-per-host queue: twelve requests to one host run at once");
    {
        SlowServer srv;
        srv.delayMs = 400;
        srv.start();
        const QString url = QStringLiteral("http://127.0.0.1:%1/y").arg(int(srv.port));
        QElapsedTimer t;
        t.start();
        async::all(12, [&](int) {
            http::fetch(url, {});
            return true;
        });
        CHECK_MSG(int(srv.maxOpen) >= 12, QString::number(int(srv.maxOpen)));
        CHECK_MSG(t.elapsed() < 750, QString::number(t.elapsed()));
    }
}

int main(int argc, char **argv)
{
    // Local-time dates (Date.parse without a zone) as in the reference, which Node wrote in UTC.
    qputenv("TZ", "UTC");
    tzset();
    QCoreApplication app(argc, argv);
    REF = readJson(QTM_SERVER_REF).toObject();
    g_btc = [] {
        Bars out;
        for (const QJsonValue b : readJson(QString(QTM_FIXTURES) + "/btc-1d.json").toArray())
            out << Bar { b["time"].toDouble(), b["open"].toDouble(), b["high"].toDouble(), b["low"].toDouble(), b["close"].toDouble(), b["volume"].toDouble(), {} };
        return out;
    }();

    QTemporaryDir refData, provData, feedData;
    testStoreOverwrites();
    testCandleStore();
    testJsHelpers();
    testReferenceProviders(refData.path());
    testReferenceImports();
    testReferencePtq();
    testProviders(provData.path());
    testFeeds(feedData.path());
    testAuditJson();
    testAuditParsing();
    testAuditPolygonNextUrl();
    testAuditConnections();

    {
        // server.test.js and imports.test.js: an offline server on a random port, temp data.
        QTemporaryDir data, custom;
        qputenv("QT_OFFLINE", "1");
        qputenv("QT_FIXTURES_DIR", QTM_FIXTURES);
        {
            Backend backend(data.path(), custom.path());
            backend.loadCustomIndicators();
            g_port = backend.startHttp(quint16(39000 + QRandomGenerator::global()->bounded(1000)), 20);
            CHECK(g_port > 0);
            testServer(backend, custom.path());
            testImportsHttp(custom.path());
            testAuditHttp(custom.path());
        }
        QTemporaryDir ptqData, ptqCustom;
        testPtq(ptqData.path(), ptqCustom.path());
        qunsetenv("QT_OFFLINE");
    }

    std::printf("\n%d checks, %d failed, %d skipped\n", g_checks, g_failures, g_skipped);
    return g_failures ? 1 : 0;
}
