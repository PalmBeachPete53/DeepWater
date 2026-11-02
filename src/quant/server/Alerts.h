#pragma once

// Port of lib/alerts.js. Alerts run in the backend, so they keep firing while the Charts page is
// hidden (as long as the app is running). An alert is a condition in the same format the strategy
// tester uses:
//
// { symbol: 'BINANCE:BTCUSDT', tf: '1h', left: 'close', op: 'crossesAbove', right: 100000,
//   indicators: {}, trigger: 'once' | 'oncePerBar' | 'oncePerBarClose', webhook: 'https://...',
//   message: 'BTC broke 100k' }
//
// Give `watchlist: 'Main'` instead of a symbol and the alert checks every symbol on that list
// (read fresh each time, so edits to the list count). "Only once" then means once per symbol.

#include <QJsonArray>
#include <QJsonObject>
#include <QMutex>
#include <QString>

#include <condition_variable>
#include <functional>
#include <optional>
#include <memory>
#include <mutex>

namespace qtm {

class Providers;
class Store;

class Alerts
{
public:
    Alerts(Store &store, Providers &data);

    QJsonArray list();
    QJsonArray history();
    void onFire(std::function<void(const QJsonObject &event)> fn);

    QJsonObject add(const QJsonValue &input);
    QJsonObject update(const QString &id, const QJsonObject &patch);
    void remove(const QString &id);
    int activeCount();

    // One evaluation at a time: the timer and a manual "check now" must not both fire a "once"
    // alert. A call made while one runs waits for that one.
    void evaluate();

    static QJsonArray conditionsOf(const QJsonObject &a);
    static QString describe(const QJsonObject &a, const QString &symbol);
    static QString describe(const QJsonObject &a);

private:
    using AlertPtr = std::shared_ptr<QJsonObject>;
    void validate(const QJsonObject &a);
    void save();                 // with m_mutex held
    void evaluateOnce();
    void fire(const AlertPtr &a, double barTime, double price, const QString &symbol);

    Store &m_store;
    Providers &m_data;
    QMutex m_mutex;              // the alert list, every alert's fields and the log
    QVector<AlertPtr> m_alerts;
    std::optional<QJsonArray> m_saved;   // what save() last wrote
    QJsonArray m_log;
    std::vector<std::function<void(const QJsonObject &)>> m_listeners;

    std::mutex m_runMutex;
    std::condition_variable m_runCv;
    bool m_running = false;
    quint64 m_runGeneration = 0;
};

} // namespace qtm
