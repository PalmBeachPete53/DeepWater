#pragma once

// Port of quant-terminal/server.js and lib/*.js (providers, feeds, imports, market-info, store,
// alerts, ptq, proxy). The backend lives for the whole app (owned by deepwater's MainWindow), not
// by the Charts page, so alerts keep firing while another page is shown.
//
// Threading: handle() and candles() are blocking and are called on worker threads (ui/Api.cpp
// runs them on a QThreadPool). They do their HTTP requests synchronously (server/Http.h) and must
// be safe to call concurrently. Everything a JS module kept in module-level variables (caches,
// health, the alert list, Yahoo's back-off ...) is shared state behind a mutex.

#include "../core/CandleSet.h"

#include <QByteArray>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>

namespace qtm {

struct Response {
    int status = 200;
    QJsonValue json;           // the JSON body (send(res, status, obj))
    bool isText = false;       // send(res, status, "text"): a plain text body in `text`
    QByteArray text;
};

class Backend : public QObject
{
    Q_OBJECT
public:
    // dataDir: where lib/store.js kept ./data (state.json, alerts.json, feeds.json, imports/ ...).
    // customDir: the custom/ folder of indicator .js files.
    Backend(const QString &dataDir, const QString &customDir, QObject *parent = nullptr);
    ~Backend() override;

    static Backend *instance();

    // Like `node server.js` at startup: load custom indicators, start the alert loop (every 15 s),
    // and serve the HTTP API on 127.0.0.1:3777 (or the next free port) for your own scripts.
    void start();

    // server.js handle() for /api/* routes: same paths, methods, status codes and JSON.
    // `path` is decoded (no query string); `query` holds the decoded query parameters.
    Response handle(const QString &method, const QString &path, const QHash<QString, QString> &query,
                    const QByteArray &body);

    // data.getCandles(symbol, tf, limit) without the JSON round trip. Throws qtm::Error.
    CandleSet candles(const QString &symbol, const QString &tf, int limit);

    // The footprint chart's Binance calls (Providers::binanceTickSize / binanceAggTrades). Blocking: off the UI thread.
    double binanceTickSize(const QString &ticker);
    QJsonArray binanceAggTrades(const QString &ticker, qint64 fromId, int limit, bool *us = nullptr);
    // The liquidity heatmap's order book snapshot (Providers::binanceDepth). Blocking: off the UI thread.
    QJsonObject binanceDepth(const QString &ticker, int limit, bool *us = nullptr);

    QString dataDir() const;
    QString customDir() const;
    int httpPort() const;      // 0 until the HTTP API is listening

    // Deepwater's Sources page (not in quant-terminal). Ids: binance, coinbase, kraken, yahoo, nasdaq,
    // calendar, cryptonews, alpaca, polygon. Turned off, a source isn't asked for anything and the
    // fallbacks behind it take over (Providers::setDisabled, Feeds::setDisabled).
    void setDisabledSources(const QStringList &ids);
    // Blocking (a network request): run it off the UI thread. { ok, ms } or { ok: false, error }.
    QJsonObject probeSource(const QString &id);
    // <data>/feeds.json, your own Alpaca / Polygon keys (Feeds.h has the format).
    QJsonObject feedConfig();
    bool setFeedConfig(const QJsonObject &config);

    // start() without the fixed port: serve the HTTP API on 127.0.0.1:port, or the next free port
    // (up to `tries` more). Returns the port, 0 when none was free. Used by start() and the tests.
    int startHttp(quint16 port, int tries = 20);
    // start()'s alert loop on its own (first check after 2 s, then 15 s after each check).
    void startAlerts(int intervalMs = 15000);
    // Loads every custom/*.js file (server.js does this before it listens).
    void loadCustomIndicators();

signals:
    // server-sent event "alert" (alerts.onFire), delivered on the backend's thread.
    void alertFired(const QJsonObject &event);
    // A custom indicator file was (re)loaded or removed through the API.
    void customIndicatorsChanged();

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace qtm
