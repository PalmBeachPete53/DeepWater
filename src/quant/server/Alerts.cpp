#include "Alerts.h"

#include "../core/Backtest.h"
#include "../core/Error.h"
#include "Http.h"
#include "Json.h"
#include "Providers.h"
#include "Store.h"

#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QUrl>
#include <QUuid>

#include <cmath>

namespace qtm {

static Error bad(const QString &message, int status = 400)
{
    return Error(message, status);
}

static const QStringList ALERT_OPS = { ">", "<", ">=", "<=", "crossesAbove", "crossesBelow", "crosses" };

// A short synthetic series is enough to check that every operand and indicator in a rule exists.
static const Bars &probe()
{
    static const Bars bars = [] {
        Bars b;
        for (int t = 0; t < 300; ++t) {
            const double c = 100 + 10 * std::sin(t / 7.0);
            b.push_back(Bar { double(t * 60), c - 1, c + 2, c - 2, c, double(1000 + t), {} });
        }
        return b;
    }();
    return bars;
}

static QString uuid()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

Alerts::Alerts(Store &store, Providers &data) : m_store(store), m_data(data)
{
    for (const QJsonValue a : store.read("alerts", QJsonArray()).toArray())
        m_alerts.push_back(std::make_shared<QJsonObject>(a.toObject()));
    m_log = store.read("alert-log", QJsonArray()).toArray();
}

QJsonArray Alerts::list()
{
    QMutexLocker lock(&m_mutex);
    QJsonArray out;
    for (const auto &a : m_alerts) out.append(*a);
    return out;
}

QJsonArray Alerts::history()
{
    QMutexLocker lock(&m_mutex);
    return m_log;
}

int Alerts::activeCount()
{
    QMutexLocker lock(&m_mutex);
    int n = 0;
    for (const auto &a : m_alerts)
        if (jsv::truthy(a->value("active"))) ++n;
    return n;
}

void Alerts::onFire(std::function<void(const QJsonObject &)> fn)
{
    QMutexLocker lock(&m_mutex);
    m_listeners.push_back(std::move(fn));
}

void Alerts::save()
{
    QJsonArray arr;
    for (const auto &a : m_alerts) arr.append(*a);
    // Alerts are evaluated every 15 s, mostly without changing anything.
    if (arr == m_saved)
        return;
    m_store.write("alerts", arr);
    m_saved = arr;
}

// Up to five conditions (TradingView's multi-condition alerts); `match` says whether all or any
// must hold. Older alerts with a single left/op/right still work.
QJsonArray Alerts::conditionsOf(const QJsonObject &a)
{
    const QJsonValue c = a.value("conditions");
    if (c.isArray() && !c.toArray().isEmpty()) return c.toArray();
    QJsonObject one;
    for (const char *k : { "left", "op", "right" })
        if (a.contains(k)) one.insert(k, a.value(k));
    return { one };
}

void Alerts::validate(const QJsonObject &a)
{
    const QJsonValue wl = a.value("watchlist");
    if (!wl.isUndefined() && (!wl.isString() || wl.toString().isEmpty()))
        throw bad("watchlist must be the name of one of your watchlists");
    if (!jsv::truthy(wl) && (!jsv::truthy(a.value("symbol")) || !a.value("symbol").isString()))
        throw bad("An alert needs a symbol or a watchlist");
    const QJsonArray conds = conditionsOf(a);
    if (conds.size() > 5) throw bad("An alert can have up to 5 conditions");
    // ["all", "any"].includes(a.match || "all"): only those two strings, not ["all"] or 1.
    const QJsonValue match = jsv::orElse(a.value("match"), "all");
    if (match != QJsonValue("all") && match != QJsonValue("any")) throw bad("match must be \"all\" or \"any\"");
    // JS's \S and an end-only $ (PCRE's $ also matches before a final newline).
    static const QRegularExpression hook(QStringLiteral("^https?:\\/\\/[^" QTM_JS_SPACE "]+\\z"), QRegularExpression::CaseInsensitiveOption);
    if (jsv::truthy(a.value("webhook")) && !hook.match(jsv::toString(a.value("webhook"))).hasMatch())
        throw bad("Webhook must be an http(s) URL");
    for (const QJsonValue cv : conds) {
        const QJsonValue right = cv["right"];
        if (jsv::nullish(right) || right == QJsonValue(""))
            throw bad("An alert needs something to compare against");
        const QJsonValue op = cv["op"];
        if (!op.isString() || !ALERT_OPS.contains(op.toString()))
            throw bad(QStringLiteral("Unknown operator \"%1\". Use one of %2").arg(jsv::toString(op), ALERT_OPS.join(", ")));
        try {
            conditionFn(probe(), jsv::orElse(a.value("indicators"), QJsonObject()).toObject())(cv.toObject())(299);
        } catch (const Error &e) {
            throw bad(e.message());
        } catch (const std::exception &e) {
            throw bad(QString::fromUtf8(e.what()));
        }
    }
}

QJsonObject Alerts::add(const QJsonValue &input)
{
    if (!input.isObject() && !input.isArray()) throw bad("Send the alert as a JSON object");
    const QJsonObject in = input.toObject();
    const QJsonValue conds = in.value("conditions");
    QJsonObject a {
        { "id", uuid() },
        { "tf", jsv::orElse(in.value("tf"), "1m") },
        { "left", jsv::coalesce(jsv::coalesce(in.value("left"), conds[0]["left"]), "close") },
        { "op", jsv::orElse(jsv::orElse(in.value("op"), conds[0]["op"]), "crossesAbove") },
        { "match", jsv::orElse(in.value("match"), "all") },
        { "indicators", jsv::orElse(in.value("indicators"), QJsonObject()) },
        { "trigger", jsv::orElse(in.value("trigger"), "once") },
        { "webhook", jsv::orElse(in.value("webhook"), "") },
        { "message", jsv::orElse(in.value("message"), "") },
        { "active", true },
        { "createdAt", jsv::isoNow() },
        { "lastBar", QJsonValue(QJsonValue::Null) },
        { "fired", 0 },
    };
    const QJsonValue symbol = jsv::truthy(in.value("watchlist")) ? QJsonValue("") : in.value("symbol");
    if (!symbol.isUndefined()) a.insert("symbol", symbol);
    if (jsv::truthy(in.value("watchlist"))) a.insert("watchlist", in.value("watchlist"));
    const QJsonValue right = jsv::coalesce(in.value("right"), conds[0]["right"]);
    if (!right.isUndefined()) a.insert("right", right);
    if (conds.isArray()) a.insert("conditions", conds);
    QMutexLocker lock(&m_mutex);
    validate(a);
    m_alerts.push_back(std::make_shared<QJsonObject>(a));
    save();
    return a;
}

QJsonObject Alerts::update(const QString &id, const QJsonObject &patch)
{
    QMutexLocker lock(&m_mutex);
    AlertPtr a;
    for (const auto &x : m_alerts)
        if (x->value("id") == QJsonValue(id)) {
            a = x;
            break;
        }
    if (!a) throw bad("No such alert", 404);
    QJsonObject merged = *a;
    for (auto it = patch.begin(); it != patch.end(); ++it) merged.insert(it.key(), it.value());
    validate(merged);
    merged.insert("id", id);
    *a = merged;
    save();
    return *a;
}

void Alerts::remove(const QString &id)
{
    QMutexLocker lock(&m_mutex);
    QVector<AlertPtr> keep;
    for (const auto &a : m_alerts)
        if (a->value("id") != QJsonValue(id)) keep.push_back(a);
    m_alerts = keep;
    save();
}

QString Alerts::describe(const QJsonObject &a)
{
    return describe(a, jsv::toString(a.value("symbol")));
}

QString Alerts::describe(const QJsonObject &a, const QString &symbol)
{
    static const QHash<QString, QString> ops = {
        { ">", "is above" }, { "<", "is below" }, { ">=", "is at or above" }, { "<=", "is at or below" },
        { "crossesAbove", "crosses above" }, { "crossesBelow", "crosses below" }, { "crosses", "crosses" },
    };
    auto name = [](const QJsonValue &o) {
        return o.isObject() && jsv::truthy(o["line"]) ? QStringLiteral("your drawn line") : jsv::toString(o);
    };
    QStringList parts;
    for (const QJsonValue c : conditionsOf(a)) {
        const QString op = jsv::toString(c["op"]);
        parts << QStringLiteral("%1 %2 %3").arg(name(c["left"]), ops.value(op, op), name(c["right"]));
    }
    if (jsv::truthy(a.value("message"))) {
        const QString msg = jsv::toString(a.value("message"));
        return jsv::truthy(a.value("watchlist")) ? symbol + ": " + msg : msg;
    }
    return QStringLiteral("%1 %2: %3").arg(symbol, jsv::toString(a.value("tf")),
                                          parts.join(a.value("match") == QJsonValue("any") ? " or " : " and "));
}

static void sendWebhook(const QString &url, const QString &text, const QJsonObject &payload)
{
    const QUrl u(url);
    if (!u.isValid() || u.host().isEmpty()) throw Error("Invalid URL");
    QString host = u.host();
    if (u.port() != -1) host += ':' + QString::number(u.port());
    QJsonObject body;
    if (host.contains("discord")) body = { { "content", text } };
    else if (host.contains("slack.com")) body = { { "text", text } };
    else if (host.contains("telegram.org")) body = { { "text", text } };
    else {
        body = { { "text", text }, { "content", text } };
        for (auto it = payload.begin(); it != payload.end(); ++it) body.insert(it.key(), it.value());
    }
    http::FetchOptions o;
    o.method = "POST";
    o.headers << qMakePair(QByteArray("Content-Type"), QByteArray("application/json"));
    o.body = jsv::stringify(body);
    o.hasBody = true;
    o.timeoutMs = 10000;
    http::FetchResponse res;
    try {
        res = http::fetch(url, o);
    } catch (const http::FetchError &e) {
        throw Error(e.message());
    }
    if (!res.ok()) throw Error(QStringLiteral("Webhook returned HTTP %1").arg(res.status));
}

void Alerts::fire(const AlertPtr &a, double barTime, double price, const QString &symbol)
{
    QJsonObject event;
    QJsonObject alertCopy;
    QString webhook;
    {
        QMutexLocker lock(&m_mutex);
        const QString text = QStringLiteral("Alert: %1 (price %2)").arg(describe(*a, symbol), jsv::numberToString(price));
        event = {
            { "id", uuid() },
            { "alertId", a->value("id") },
            { "symbol", symbol },
            { "text", text },
            { "price", price },
            { "barTime", barTime },
            { "at", jsv::isoNow() },
        };
        if (!a->value("tf").isUndefined()) event.insert("tf", a->value("tf"));
        a->insert("fired", jsv::toNumber(a->value("fired")) + 1);
        a->insert("lastFiredAt", event.value("at"));
        if (a->value("trigger") == QJsonValue("once") && !jsv::truthy(a->value("watchlist"))) a->insert("active", false);
        alertCopy = *a;
        webhook = jsv::truthy(a->value("webhook")) ? jsv::toString(a->value("webhook")) : QString();
    }
    if (!webhook.isEmpty()) {
        try {
            sendWebhook(webhook, event.value("text").toString(), { { "alert", alertCopy }, { "event", event } });
            event.insert("webhook", "sent");
        } catch (const Error &e) {
            event.insert("webhook", "failed: " + e.message());
        }
    }
    std::vector<std::function<void(const QJsonObject &)>> listeners;
    {
        QMutexLocker lock(&m_mutex);
        m_log.prepend(event);
        while (m_log.size() > 500) m_log.removeLast();
        m_store.write("alert-log", m_log);
        listeners = m_listeners;
    }
    for (const auto &fn : listeners) fn(event);
}

void Alerts::evaluate()
{
    std::unique_lock l(m_runMutex);
    if (m_running) {
        const quint64 gen = m_runGeneration;
        m_runCv.wait(l, [&] { return m_runGeneration != gen; });
        return;
    }
    m_running = true;
    l.unlock();
    auto finish = [&] {
        std::lock_guard g(m_runMutex);
        m_running = false;
        ++m_runGeneration;
        m_runCv.notify_all();
    };
    try {
        evaluateOnce();
    } catch (...) {
        finish();
        throw;
    }
    finish();
}

void Alerts::evaluateOnce()
{
    QVector<AlertPtr> active;
    {
        QMutexLocker lock(&m_mutex);
        for (const auto &a : m_alerts)
            if (jsv::truthy(a->value("active"))) active.push_back(a);
    }
    if (active.isEmpty()) {
        QMutexLocker lock(&m_mutex);
        save();
        return;
    }
    const QJsonObject lists = jsv::orElse(m_store.read("state", QJsonObject())["watchlists"], QJsonObject()).toObject();
    QStringList order;
    QHash<QString, QVector<AlertPtr>> groups;
    {
        QMutexLocker lock(&m_mutex);
        for (const auto &a : active) {
            QStringList symbols;
            if (jsv::truthy(a->value("watchlist"))) {
                for (const QJsonValue s : lists.value(jsv::toString(a->value("watchlist"))).toArray()) {
                    const QString sym = jsv::toString(s);
                    if (!symbols.contains(sym)) symbols << sym;
                }
            } else {
                symbols << jsv::toString(a->value("symbol"));
            }
            for (const QString &symbol : symbols) {
                const QString k = symbol + '|' + jsv::toString(a->value("tf"));
                if (!groups.contains(k)) order << k;
                groups[k].push_back(a);
            }
        }
    }
    for (const QString &k : order) {
        const QStringList kp = k.split('|');
        const QString symbol = kp[0];
        const QString tf = kp.value(1);
        Bars bars;
        try {
            bars = m_data.getCandles(symbol, tf, 400).candles;
        } catch (...) {
            continue;
        }
        if (bars.size() < 3) continue;
        for (const AlertPtr &a : groups.value(k)) {
            bool doFire = false;
            double barTime = 0, price = 0;
            {
                QMutexLocker lock(&m_mutex);
                try {
                    const ConditionFactory make = conditionFn(bars, a->value("indicators").toObject());
                    QVector<Condition> tests;
                    for (const auto &c : conditionsOf(*a)) tests.push_back(make(c.toObject()));
                    const bool any = a->value("match") == QJsonValue("any");
                    auto test = [&](int i) {
                        for (const auto &t : tests) {
                            const bool v = t(i);
                            if (any && v) return true;
                            if (!any && !v) return false;
                        }
                        return !any;
                    };
                    const QString trigger = jsv::toString(a->value("trigger"));
                    const int i = trigger == "oncePerBarClose" ? int(bars.size()) - 2 : int(bars.size()) - 1;
                    const Bar &bar = bars[i];
                    if (jsv::truthy(a->value("watchlist"))) {
                        QJsonObject lastBars = jsv::orElse(a->value("lastBars"), QJsonObject()).toObject();
                        a->insert("lastBars", jsv::orElse(a->value("lastBars"), QJsonObject()));
                        const bool seen = trigger == "once" ? lastBars.contains(symbol)
                                                            : lastBars.value(symbol) == QJsonValue(bar.time);
                        if (seen || !test(i)) continue;
                        lastBars.insert(symbol, bar.time);
                        a->insert("lastBars", lastBars);
                    } else {
                        if (!jsv::truthy(a->value("active"))) continue;
                        if (trigger != "once" && a->value("lastBar") == QJsonValue(bar.time)) continue;
                        if (!test(i)) continue;
                        a->insert("lastBar", bar.time);
                    }
                    doFire = true;
                    barTime = bar.time;
                    price = bar.close;
                } catch (const Error &e) {
                    a->insert("error", e.message());
                } catch (const std::exception &e) {
                    a->insert("error", QString::fromUtf8(e.what()));
                }
            }
            if (doFire) {
                try {
                    fire(a, barTime, price, symbol);
                } catch (const Error &e) {
                    QMutexLocker lock(&m_mutex);
                    a->insert("error", e.message());
                }
            }
        }
    }
    QMutexLocker lock(&m_mutex);
    save();
}

} // namespace qtm
