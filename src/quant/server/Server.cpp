// Port of server.js handle(): every /api route, with the same status codes and JSON.

#include "Server.h"

#include "../core/Backtest.h"
#include "../core/CustomIndicators.h"
#include "../core/Error.h"
#include "../core/Indicators.h"
#include "../core/Quant.h"
#include "../core/Series.h"
#include "Alerts.h"
#include "Async.h"
#include "Feeds.h"
#include "Http.h"
#include "HttpApi.h"
#include "Imports.h"
#include "Json.h"
#include "MarketInfo.h"
#include "Providers.h"
#include "Ptq.h"
#include "Store.h"
#include "DepthStream.h"
#include "KlineStream.h"
#include "TradeStream.h"

#include <QAtomicPointer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QRegularExpression>
#include <QSysInfo>
#include <QTimer>

#include <cmath>

namespace qtm {

static QAtomicPointer<Backend> s_instance;
static const QString VERSION = QStringLiteral("1.0.0");

struct Backend::Private {
    QString dataDir;
    QString customDir;
    Store store;
    Feeds feeds;
    Imports imports;
    Providers data;
    MarketInfo info;
    Alerts alerts;
    Ptq ptq;
    HttpApi *http = nullptr;
    std::unique_ptr<TradeStreams> tradeStreams;   // on the UI thread; made on first use
    std::unique_ptr<DepthStreams> depthStreams;   // the same
    std::unique_ptr<KlineStreams> klineStreams;   // the same

    QMutex customMutex;                 // custom/ file writes and customErrors
    QHash<QString, QString> customErrors;
    QMutex stateMutex;                  // PUT /api/state read-modify-write

    QTimer *alertTimer = nullptr;
    bool alertsStarted = false;
    int alertInterval = 15000;

    static int timeoutMs()
    {
        const int t = qEnvironmentVariableIntValue("QT_TIMEOUT_MS");
        return t > 0 ? t : 10000;
    }
    static ProviderOptions providerOptions()
    {
        ProviderOptions o;
        o.timeoutMs = timeoutMs();
        o.offline = qEnvironmentVariable("QT_OFFLINE") == "1";
        o.fixturesDir = qEnvironmentVariable("QT_FIXTURES_DIR");
        return o;
    }

    Private(const QString &data, const QString &custom)
        : dataDir(data), customDir(custom), store(data), feeds(data, timeoutMs()), imports(data),
          data(feeds, imports, providerOptions()), info(this->data), alerts(store, this->data), ptq(Ptq::fromEnv())
    {
    }

    QJsonArray listCustom();
    void loadCustom(const QString &file);
};

// ---------- helpers ----------
static Error httpError(int status, const QString &message)
{
    return Error(message, status);
}

static Response json(int status, const QJsonValue &body)
{
    Response r;
    r.status = status;
    r.json = body;
    return r;
}

static Response ok(const QJsonValue &body)
{
    return json(200, body);
}

// readBody(req, limit): a JSON object, or {} for an empty body.
static QJsonObject readBody(const QByteArray &body, double limit = 2e6)
{
    if (body.size() > limit) throw httpError(413, "Request too large");
    if (body.isEmpty()) return {};
    bool okParse = false;
    const QJsonValue v = jsv::parse(body, &okParse);
    if (!okParse) throw httpError(400, "The request body isn't valid JSON");
    if (!v.isObject()) throw httpError(400, "The request body must be a JSON object");
    return v.toObject();
}

// A raw text body (CSV imports), up to `limit` bytes.
static QString readText(const QByteArray &body, double limit = 300e6)
{
    if (body.size() > limit)
        throw httpError(413, QStringLiteral("The file is over %1 MB").arg(jsv::numberToString(jsv::round(limit / 1e6))));
    return QString::fromUtf8(body);
}

static QJsonObject candlesJson(const CandleResult &r)
{
    QJsonArray candles;
    for (const Bar &b : r.candles)
        candles.append(QJsonObject { { "time", b.time }, { "open", b.open }, { "high", b.high },
                                     { "low", b.low }, { "close", b.close }, { "volume", b.volume } });
    return { { "symbol", r.symbol }, { "tf", r.tf }, { "provider", r.provider }, { "candles", candles },
             { "notes", QJsonArray::fromStringList(r.notes) } };
}

// String(raw || ""): what the JS passes to resolve() for a symbol parameter.
static QString symbolArg(const QJsonValue &v)
{
    return jsv::truthy(v) ? jsv::toString(v) : QString();
}

// ---------- custom indicators (plain JS files in custom/, and Pine Script files run by PineTS) ----------
static QString slug(const QString &s)
{
    QString out = s.toLower();
    out.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), "-");
    out.replace(QRegularExpression(QStringLiteral("^-|-$")), QString());
    out = out.left(60);
    return out.isEmpty() ? QStringLiteral("indicator") : out;
}

static QString customId(const QString &file)
{
    return "custom:" + customIndicatorBaseName(file);
}

QJsonArray Backend::Private::listCustom()
{
    QDir().mkpath(customDir);
    QJsonArray out;
    for (const QString &f : readDir(customDir)) {
        if (!isCustomIndicatorFile(f)) continue;
        QMutexLocker lock(&customMutex);
        out.append(QJsonObject {
            { "file", f },
            { "id", customId(f) },
            { "mtime", mtimeMs(QDir(customDir).filePath(f)) },
            { "error", customErrors.contains(f) ? QJsonValue(customErrors.value(f)) : QJsonValue(QJsonValue::Null) },
        });
    }
    return out;
}

void Backend::Private::loadCustom(const QString &file)
{
    QString error;
    bool failed = false;
    try {
        loadCustomIndicatorFile(QDir(customDir).filePath(file), customId(file));
    } catch (const Error &e) {
        failed = true;
        error = e.message();
    } catch (const std::exception &e) {
        failed = true;
        error = QString::fromUtf8(e.what());
    }
    if (failed) customErrors.insert(file, error);
    else customErrors.remove(file);
}

static bool writeFile(const QString &path, const QByteArray &data)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    return f.write(data) == data.size();
}

// ---------- Backend ----------
Backend::Backend(const QString &dataDir, const QString &customDir, QObject *parent)
    : QObject(parent), d(std::make_unique<Private>(dataDir, customDir))
{
    s_instance.storeRelease(this);
    http::abortAll(false);
    QPointer<Backend> self(this);
    d->alerts.onFire([self](const QJsonObject &event) {
        if (!self) return;
        QMetaObject::invokeMethod(
            self.data(),
            [self, event] {
                if (!self) return;
                if (self->d->http) self->d->http->broadcast("alert", event);
                emit self->alertFired(event);
            },
            Qt::QueuedConnection);
    });
}

Backend::~Backend()
{
    // Quitting with requests in flight: new calls from the UI find no backend, the network gives
    // up at once, and every thread still working for us (the UI's requests, an abandoned Binance
    // mirror request, an alert check) ends before our members go.
    s_instance.testAndSetRelease(this, nullptr);
    if (d->alertTimer) d->alertTimer->stop();
    d->tradeStreams.reset();   // their sockets close; REST calls on the way finish below, their answers dropped
    d->depthStreams.reset();   // each joins its thread
    d->klineStreams.reset();
    http::abortAll(true);
    async::requestPool()->waitForDone();
    async::pool()->waitForDone();
    delete d->http;
    d->http = nullptr;
    http::abortAll(false);
}

void Backend::setDisabledSources(const QStringList &ids)
{
    const QSet<QString> set(ids.begin(), ids.end());
    d->data.setDisabled(set);
    d->feeds.setDisabled(set);
}

QJsonObject Backend::probeSource(const QString &id)
{
    return d->data.probe(id);
}

QJsonObject Backend::feedConfig()
{
    return d->feeds.feedConfig();
}

bool Backend::setFeedConfig(const QJsonObject &config)
{
    return d->feeds.saveFeedConfig(config);
}

Backend *Backend::instance()
{
    return s_instance.loadAcquire();
}

QString Backend::dataDir() const
{
    return d->dataDir;
}

QString Backend::customDir() const
{
    return d->customDir;
}

int Backend::httpPort() const
{
    return d->http ? d->http->port() : 0;
}

void Backend::loadCustomIndicators()
{
    const QJsonArray files = d->listCustom();
    QMutexLocker lock(&d->customMutex);
    for (const QJsonValue c : files) d->loadCustom(c["file"].toString());
}

int Backend::startHttp(quint16 port, int tries)
{
    if (!d->http) d->http = new HttpApi(this, d->customDir);
    const int p = d->http->listen(port, tries);
    if (p) {
        QString note = QStringLiteral("Quant Terminal %1 API on http://127.0.0.1:%2  Data: %3").arg(VERSION).arg(p).arg(d->dataDir);
        const QString proxy = http::proxyUrl();
        if (!proxy.isEmpty()) note += "  Using your proxy " + QString(proxy).replace(QRegularExpression("//[^@]*@"), "//");
        qInfo("%s", qPrintable(note));
    } else {
        qWarning("Quant Terminal: no free port for the local API from %d", int(port));
    }
    return p;
}

void Backend::startAlerts(int intervalMs)
{
    if (d->alertsStarted) return;
    d->alertsStarted = true;
    d->alertInterval = intervalMs;
    d->alertTimer = new QTimer(this);
    d->alertTimer->setSingleShot(true);
    // Each check runs on a worker thread; the next one is scheduled when it has finished.
    connect(d->alertTimer, &QTimer::timeout, this, [this] {
        QPointer<Backend> self(this);
        async::pool()->start([self] {
            if (!self) return;
            try {
                self->d->alerts.evaluate();
            } catch (...) {
            }
            QMetaObject::invokeMethod(
                self.data(), [self] {
                    if (self && self->d->alertTimer) self->d->alertTimer->start(self->d->alertInterval);
                },
                Qt::QueuedConnection);
        });
    });
    d->alertTimer->start(2000);
}

void Backend::start()
{
    loadCustomIndicators();
    // START_PORT = Number(process.env.PORT) || 3777
    const double envPort = jsv::toNumber(qEnvironmentVariable("PORT"));
    startHttp(envPort >= 1 && envPort <= 65535 && envPort == std::floor(envPort) ? quint16(envPort) : 3777, 20);
    startAlerts();
}

double Backend::binanceTickSize(const QString &ticker)
{
    return d->data.binanceTickSize(ticker);
}

KlineStreams *Backend::klineStreams()
{
    if (!d->klineStreams) d->klineStreams = std::make_unique<KlineStreams>(std::make_shared<BinanceKlineSource>());
    return d->klineStreams.get();
}

DepthStreams *Backend::depthStreams()
{
    if (!d->depthStreams)
        d->depthStreams = std::make_unique<DepthStreams>(std::make_shared<BinanceDepthSource>(this), d->dataDir);
    return d->depthStreams.get();
}

TradeStreams *Backend::tradeStreams()
{
    if (!d->tradeStreams) d->tradeStreams = std::make_unique<TradeStreams>(std::make_shared<BinanceTradeSource>(this));
    return d->tradeStreams.get();
}

QJsonArray Backend::binanceAggTrades(const QString &ticker, qint64 fromId, int limit, bool *us)
{
    return d->data.binanceAggTrades(ticker, fromId, limit, us);
}

QJsonObject Backend::binanceDepth(const QString &ticker, int limit, bool *us)
{
    return d->data.binanceDepth(ticker, limit, us);
}

CandleSet Backend::candles(const QString &symbol, const QString &tf, int limit)
{
    const CandleResult r = d->data.getCandles(symbol, tf, limit);
    CandleSet s;
    s.symbol = r.symbol;
    s.provider = r.provider;
    s.notes = r.notes;
    s.candles = r.candles;
    return s;
}

Response Backend::handle(const QString &method, const QString &p, const QHash<QString, QString> &qs, const QByteArray &body)
{
    // q.x is undefined when the parameter is missing.
    auto q = [&](const char *k) -> QJsonValue {
        const QString key = QString::fromLatin1(k);
        return qs.contains(key) ? QJsonValue(qs.value(key)) : QJsonValue(QJsonValue::Undefined);
    };
    auto qstr = [&](const char *k) { return qs.value(QString::fromLatin1(k)); };
    Private &D = *d;
    try {
        if (p == "/api/health") {
            QString platform = QSysInfo::productType();
            platform = platform == "macos" || platform == "osx" ? "darwin" : platform == "windows" ? "win32" : "linux";
            return ok(QJsonObject {
                { "ok", true },
                { "app", "quant-terminal" },
                { "root", QDir::cleanPath(QDir(D.dataDir).absoluteFilePath("..")) },
                { "version", VERSION },
                { "node", QString::fromLatin1(qVersion()) },
                { "platform", platform },
                { "providers", D.data.health() },
                { "feeds", QJsonArray::fromStringList(D.feeds.configured()) },
                { "yahoo", D.data.yahooAvailable() ? "available" : "rate limited" },
                { "alerts", D.alerts.activeCount() },
            });
        }
        if (p == "/api/probe") return ok(D.data.probeAll());
        if (p == "/api/resolve") return ok(D.data.resolve(symbolArg(q("symbol"))).toJson());
        if (p == "/api/search") return ok(D.data.search(qstr("q")));
        if (p == "/api/symbols") return ok(D.data.browse(qstr("market")));
        if (p == "/api/spread") return ok(D.data.getSpread(symbolArg(q("symbol"))));
        if (p == "/api/candles")
            return ok(candlesJson(D.data.getCandles(symbolArg(q("symbol")), jsv::truthy(q("tf")) ? qstr("tf") : QStringLiteral("1D"),
                                                    q("limit").isUndefined() ? NAN : jsv::toNumber(q("limit")))));
        if (p == "/api/quotes") {
            QStringList symbols;
            for (const QString &s : qstr("symbols").split(','))
                if (!s.isEmpty() && symbols.size() < 200) symbols << s;
            return ok(D.data.getQuotes(symbols));
        }
        if (p == "/api/calendar") return ok(D.data.economicCalendar());
        auto days = [&] {
            const double n = jsv::toNumber(q("days"));
            return (n != 0 && !std::isnan(n)) ? n : 5.0;
        };
        if (p == "/api/earnings") return ok(D.info.earnings(days()));
        if (p == "/api/dividends") return ok(D.info.dividends(days()));
        if (p == "/api/news") return ok(D.info.news(symbolArg(q("symbol"))));
        if (p == "/api/financials") return ok(D.info.financials(symbolArg(q("symbol")), qstr("frequency"), qs.contains("frequency")));
        if (p == "/api/screener") {
            const QString u = jsv::truthy(q("universe")) ? qstr("universe") : QStringLiteral("crypto");
            return ok(u == "crypto" ? D.data.cryptoScreener() : D.data.nasdaqScreener(u == "etf" ? "etf" : "stocks"));
        }
        if (p == "/api/scan" && method == "POST") {
            const QJsonObject b = readBody(body);
            if (!b.value("symbols").isArray() || !jsv::coalesce(b.value("rules"), QJsonArray()).isArray())
                throw httpError(400, "Send { symbols: [...], rules: [...] }");
            QJsonArray symbols;
            for (const QJsonValue s : b.value("symbols").toArray())
                if (symbols.size() < 150) symbols.append(s);
            const QJsonArray rules = jsv::orElse(b.value("rules"), QJsonArray()).toArray();
            const QString tf = jsv::truthy(b.value("tf")) ? jsv::toString(b.value("tf")) : QStringLiteral("1D");
            const QJsonObject indicators = jsv::orElse(b.value("indicators"), QJsonObject()).toObject();
            const auto results = async::mapLimit(int(symbols.size()), 4, [&](int k) -> QJsonObject {
                const QJsonValue symbol = symbols[k];
                try {
                    const Bars candles = D.data.getCandles(symbolArg(symbol), tf, 400).candles;
                    const ConditionFactory make = conditionFn(candles, indicators);
                    const int i = int(candles.size()) - 1;
                    if (i < 0) throw Error("Cannot read properties of undefined (reading 'close')");
                    QJsonArray passes;
                    bool all = true;
                    for (const auto &r : rules) {
                        const bool v = make(r.toObject())(i);
                        passes.append(v);
                        all = all && v;
                    }
                    return { { "symbol", symbol }, { "pass", all }, { "passes", passes }, { "close", candles[i].close } };
                } catch (const Error &e) {
                    return { { "symbol", symbol }, { "error", e.message() } };
                } catch (const std::exception &e) {
                    return { { "symbol", symbol }, { "error", QString::fromUtf8(e.what()) } };
                }
            });
            QJsonArray out;
            for (const auto &r : results) out.append(r);
            return ok(out);
        }
        // The strategy tester on the server, for your own code (docs/api.md). Same rule format as the tester.
        if (p == "/api/backtest" && method == "POST") {
            const QJsonObject b = readBody(body);
            const QJsonValue strategyV = b.value("strategy");
            if (!jsv::truthy(b.value("symbol")) || !strategyV.isObject())
                throw httpError(400, "Send { symbol, tf, strategy: { indicators, entryLong, exitLong, ... } }");
            const QJsonObject strategy = strategyV.toObject();
            const QString tf = jsv::truthy(b.value("tf")) ? jsv::toString(b.value("tf")) : QStringLiteral("1D");
            const CandleResult cr = D.data.getCandles(symbolArg(b.value("symbol")), tf, jsv::toNumber(jsv::orElse(b.value("limit"), 1500)));
            const Bars &candles = cr.candles;
            if (candles.size() < 2) throw httpError(422, "Not enough bars to test on");
            Bars subBars;
            bool haveSub = false;
            if (strategy.value("intrabar") == QJsonValue("magnifier")) {
                const double sec = double(parseTf(tf).seconds);
                const double span = candles.last().time - candles.first().time + sec;
                const MagnifierTf m = magnifierTf(qint64(sec), span);
                if (!m.tf.isEmpty()) {
                    const CandleResult r = D.data.getCandles(cr.symbol, m.tf, std::ceil(span / double(m.seconds)) + 10);
                    for (const Bar &c : r.candles)
                        if (c.time >= candles.first().time && c.time < candles.last().time + sec) subBars.push_back(c);
                    haveSub = true;
                }
            }
            QJsonObject r, qc;
            bool haveQ = false;
            try {
                r = backtest(candles, strategy, haveSub ? &subBars : nullptr);
                if (jsv::truthy(b.value("quantChecks"))) {
                    qc = quantChecks(candles, strategy, 2000, haveSub ? &subBars : nullptr);
                    haveQ = true;
                }
            } catch (const Error &e) {
                throw httpError(400, e.message());
            } catch (const std::exception &e) {
                throw httpError(400, QString::fromUtf8(e.what()));
            }
            QJsonObject out {
                { "symbol", cr.symbol },
                { "tf", tf },
                { "provider", cr.provider },
                { "bars", int(candles.size()) },
                { "from", candles.first().time },
                { "to", candles.last().time },
                { "stats", r.value("stats") },
                { "trades", r.value("trades") },
            };
            if (jsv::truthy(b.value("equity"))) out.insert("equity", r.value("equity"));
            if (haveQ) {
                QJsonArray gates;
                for (const QJsonValue g : qc.value("gates").toArray()) {
                    QJsonObject o;
                    for (const char *k : { "label", "value", "pass" })
                        if (!g[k].isUndefined()) o.insert(k, g[k]);
                    gates.append(o);
                }
                QJsonObject qo { { "total", qc.value("gates").toArray().size() }, { "gates", gates } };
                for (const char *k : { "passed", "walkForward", "monteCarlo" })
                    if (!qc.value(k).isUndefined()) qo.insert(k, qc.value(k));
                out.insert("quantChecks", qo);
            }
            return ok(out);
        }
        // Your own data files (Imports.h): bars chart as CSV:<NAME>; a signal becomes an indicator.
        if (p == "/api/imports") return ok(D.imports.listImports());
        if (p == "/api/import/bars" && method == "POST") return ok(D.imports.importBars(qstr("name"), readText(body)));
        if (p == "/api/import/signal" && method == "POST") {
            const QString name = imports::cleanName(qstr("name"));
            const Signals sig = imports::parseSignals(readText(body, 100e6));
            const QString file = "signal-" + name.toLower() + ".js";
            const QString full = QDir(D.customDir).filePath(file);
            QMutexLocker lock(&D.customMutex);
            QDir().mkpath(D.customDir);
            QFile before(full);
            const bool existed = before.exists();
            const QByteArray old = existed && before.open(QIODevice::ReadOnly) ? before.readAll() : QByteArray();
            if (!writeFile(full, imports::signalIndicatorSource(name, sig, qstr("name")).toUtf8()))
                throw Error("Couldn't write " + full);
            D.loadCustom(file);
            if (D.customErrors.contains(file)) {
                const QString error = D.customErrors.value(file);
                if (!existed) QFile::remove(full);
                else writeFile(full, old);
                D.customErrors.remove(file);
                throw httpError(400, error);
            }
            lock.unlock();
            QMetaObject::invokeMethod(this, [this] { emit customIndicatorsChanged(); }, Qt::QueuedConnection);
            const QString id = customId(file);
            return ok(QJsonObject {
                { "ok", true }, { "name", name }, { "id", id }, { "file", file }, { "rows", sig.rows },
                { "changes", int(sig.changes.size()) }, { "from", sig.from }, { "to", sig.to },
                { "strategy", imports::signalStrategy(id) },
            });
        }
        if (p == "/api/indicators") {
            QJsonArray out;
            for (const auto &def : indicators()) {
                QJsonArray inputs;
                for (const auto &in : def->inputs) {
                    QJsonObject o { { "key", in.key }, { "type", in.type } };
                    if (!in.def.isUndefined()) o.insert("default", in.def);
                    if (!in.options.isEmpty()) o.insert("options", QJsonArray::fromStringList(in.options));
                    inputs.append(o);
                }
                QJsonArray outputs;
                for (const auto &o : def->outputs) outputs.append(o.key);
                out.append(QJsonObject { { "id", def->id }, { "name", def->name }, { "inputs", inputs }, { "outputs", outputs } });
            }
            return ok(out);
        }
        if (p == "/api/state") {
            if (method == "GET") return ok(D.store.read("state", QJsonObject()));
            if (method == "PUT") {
                const QJsonObject patch = readBody(body, 20e6);
                QMutexLocker lock(&D.stateMutex);
                QJsonObject next = D.store.read("state", QJsonObject()).toObject();
                for (auto it = patch.begin(); it != patch.end(); ++it) next.insert(it.key(), it.value());
                D.store.write("state", next);
                return ok(QJsonObject { { "ok", true } });
            }
        }
        if (p == "/api/events") {
            // In-process callers get alerts from Backend::alertFired; over HTTP this is a stream.
            Response r;
            r.isText = true;
            r.text = "retry: 3000\n\n";
            return r;
        }
        if (p == "/api/alerts") {
            if (method == "GET") return ok(D.alerts.list());
            if (method == "POST") {
                const QJsonObject b = readBody(body);
                return ok(D.alerts.add(b));
            }
        }
        if (p == "/api/alerts/log") return ok(D.alerts.history());
        if (p == "/api/alerts/check" && method == "POST") {
            D.alerts.evaluate();
            return ok(QJsonObject { { "ok", true } });
        }
        if (p.startsWith("/api/alerts/")) {
            const QString id = p.split('/').value(3);
            if (method == "DELETE") {
                D.alerts.remove(id);
                return ok(QJsonObject { { "ok", true } });
            }
            if (method == "PATCH") return ok(D.alerts.update(id, readBody(body)));
        }
        if (p == "/api/webhook-test" && method == "POST") {
            const QJsonValue hook = readBody(body).value("url");
            // JS's \s and an end-only $ (PCRE's $ also matches before a final newline).
            static const QRegularExpression re(QStringLiteral("^https?:\\/\\/[^" QTM_JS_SPACE "]+\\z"), QRegularExpression::CaseInsensitiveOption);
            const QString url = jsv::truthy(hook) ? jsv::toString(hook) : QString();
            if (!re.match(url).hasMatch()) throw httpError(400, "Webhook must be an http(s) URL");
            http::FetchOptions o;
            o.method = "POST";
            o.headers << qMakePair(QByteArray("Content-Type"), QByteArray("application/json"));
            o.body = jsv::stringify(QJsonObject { { "text", "Quant Terminal test alert" }, { "content", "Quant Terminal test alert" } });
            o.hasBody = true;
            o.timeoutMs = 10000;
            http::FetchResponse r;
            try {
                r = http::fetch(url, o);
            } catch (const http::FetchError &e) {
                throw Error(e.message());
            }
            return ok(QJsonObject { { "ok", r.ok() }, { "status", r.status } });
        }
        if (p == "/api/custom") {
            if (method == "GET") return ok(D.listCustom());
            if (method == "POST") {
                const QJsonObject b = readBody(body);
                const QJsonValue code = b.value("code");
                static const QRegularExpression exportDefault(QStringLiteral("export[" QTM_JS_SPACE "]+default"));
                // A Pine Script source (`//@version=N`, or an indicator()/strategy() declaration and no
                // export) is saved as .pine and run by PineTS.
                static const QRegularExpression pineHead(QStringLiteral("^[ \t]*//[ \t]*@version[ \t]*=[ \t]*\\d"),
                                                         QRegularExpression::MultilineOption);
                static const QRegularExpression pineDecl(QStringLiteral("^[ \t]*(indicator|strategy)[ \t]*\\("),
                                                         QRegularExpression::MultilineOption);
                const QString text = jsv::toString(code);
                const bool isJs = exportDefault.match(text).hasMatch();
                const bool isPine = !isJs && jsv::truthy(code) && (pineHead.match(text).hasMatch() || pineDecl.match(text).hasMatch());
                if (!jsv::truthy(code) || (!isJs && !isPine))
                    throw httpError(400, "The file must `export default { name, calc(bars, params, ta) { ... } }`, or be a "
                                         "Pine Script (`//@version=6` and `indicator(...)`)");
                const QString file = slug(jsv::toString(b.value("name"))) + (isPine ? ".pine" : ".js");
                const QString full = QDir(D.customDir).filePath(file);
                QMutexLocker lock(&D.customMutex);
                QDir().mkpath(D.customDir);
                QFile before(full);
                const bool existed = before.exists();
                const QByteArray old = existed && before.open(QIODevice::ReadOnly) ? before.readAll() : QByteArray();
                before.close();
                if (!writeFile(full, jsv::toString(code).toUtf8())) throw Error("Couldn't write " + full);
                D.loadCustom(file);
                if (D.customErrors.contains(file)) {
                    // Refused: put back what was there (or nothing), so a broken save never replaces a working file.
                    // The refused code never registered, so the working version is still the one loaded.
                    const QString error = D.customErrors.value(file);
                    if (!existed) QFile::remove(full);
                    else writeFile(full, old);
                    D.customErrors.remove(file);
                    return json(400, QJsonObject { { "error", error }, { "file", file } });
                }
                lock.unlock();
                QMetaObject::invokeMethod(this, [this] { emit customIndicatorsChanged(); }, Qt::QueuedConnection);
                return ok(QJsonObject { { "ok", true }, { "file", file }, { "id", customId(file) } });
            }
        }
        if (p.startsWith("/api/custom/") && method == "DELETE") {
            // path.basename(): trailing slashes don't count ("x.js/" is x.js).
            QString rel = p.mid(12);
            while (rel.size() > 1 && rel.endsWith('/')) rel.chop(1);
            const QString file = rel == "/" ? QString() : rel.section('/', -1);
            const QString full = QDir::cleanPath(D.customDir + '/' + file);   // path.join normalizes
            if (QFileInfo(full).isDir())
                throw Error(QStringLiteral("Path is a directory: rm returned EISDIR (is a directory) %1").arg(full));
            QFile::remove(full);
            QMetaObject::invokeMethod(this, [this] { emit customIndicatorsChanged(); }, Qt::QueuedConnection);
            return ok(QJsonObject { { "ok", true } });
        }
        if (p == "/api/shutdown" && method == "POST") {
            // The installer used this to stop a running copy. Deepwater owns the backend, so it stays up.
            return ok(QJsonObject { { "ok", true } });
        }
        // PTQ connector: reads and fail-safe controls only. There is no route here that arms a
        // strategy or places an order; that stays inside PTQ (`ptq arm-strategy`).
        if (p == "/api/ptq/status") return ok(D.ptq.status());
        if (p == "/api/ptq/overview") return ok(D.ptq.overview());
        if (p == "/api/ptq/check" && method == "POST") {
            try {
                return ok(D.ptq.check(readBody(body)));
            } catch (const PtqError &e) {
                return json(e.status() ? e.status() : 502,
                            QJsonObject { { "error", e.message() }, { "code", e.code() },
                                          { "problems", QJsonArray::fromStringList(e.problems()) },
                                          { "notes", QJsonArray::fromStringList(e.notes()) } });
            } catch (const Error &e) {
                return json(e.status() ? e.status() : 502, QJsonObject { { "error", e.message() }, { "problems", QJsonArray() }, { "notes", QJsonArray() } });
            }
        }
        if (p == "/api/ptq/kill-switch" && method == "POST") return ok(D.ptq.engageKillSwitch());
        static const QRegularExpression disarmRe(QStringLiteral("^\\/api\\/ptq\\/arms\\/([^/]+)\\/disarm\\z"));
        const auto dm = disarmRe.match(p);
        if (dm.hasMatch() && method == "POST") return ok(D.ptq.disarm(jsv::decodeURIComponent(dm.captured(1))));
        return json(404, QJsonObject { { "error", "Unknown endpoint" } });
    } catch (const Error &e) {
        // Upstream failures are a bad gateway, whatever status the provider used.
        const int code = e.upstream() ? 502 : e.status() >= 400 && e.status() < 600 ? e.status() : 500;
        return json(code, QJsonObject { { "error", e.message() } });
    } catch (const std::exception &e) {
        return json(500, QJsonObject { { "error", QString::fromUtf8(e.what()) } });
    }
}

} // namespace qtm
