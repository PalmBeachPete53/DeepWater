#pragma once

// Port of lib/feeds.js: your own market data keys, optional. The free public sources stay the
// default; a key you add takes over the charts it covers, and the public sources stay behind it
// as the fallback. Keys live in <data>/feeds.json:
//
//   {
//     "alpaca":  { "keyId": "PK...", "secretKey": "...", "feed": "iex" },
//     "polygon": { "apiKey": "..." }
//   }
//
// Each key is only ever sent to its own provider, in a request header.

#include "../core/Bar.h"

#include <QHash>
#include <QJsonObject>
#include <QMutex>
#include <QPair>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>

namespace qtm {

// A provider chain: [name, () => bars] tried in order (providers.js tryChain).
using ChainStep = QPair<QString, std::function<Bars()>>;
using Chain = QVector<ChainStep>;

class Feeds
{
public:
    Feeds(const QString &dataDir, int timeoutMs);

    QString feedsFile() const { return m_file; }
    QJsonObject feedConfig();                 // re-read when the file changes
    QStringList configured();                 // names only: ["alpaca", "polygon"]
    QString alpacaName();                     // "Alpaca (IEX)"
    static bool isKeyProvider(const QString &name);

    static bool inUsSession(double sec);
    static QString frontContract(const QString &root, double nowMs);   // "" when none (JS null)
    QString frontContract(const QString &root);

    Bars alpacaCandles(const QString &ticker, const QString &tf, int limit);
    Bars polygonCandles(const QString &ticker, const QString &tf, int limit);
    Bars polygonFuturesCandles(const QString &contract, const QString &tf, int limit);

    Chain stockChain(const QString &ticker, const QString &tf, int limit);
    Chain futuresChain(const QString &root, const QString &tf, int limit);

    // Deepwater's Sources page: "alpaca" / "polygon" turned off there count as not configured.
    void setDisabled(const QSet<QString> &ids);
    // feeds.json written back (the Sources page edits the keys), 0600 like a key file should be.
    bool saveFeedConfig(const QJsonObject &config);

    // For the probes in providers.probeAll(): name -> check.
    QVector<QPair<QString, std::function<void()>>> probes();

private:
    QJsonObject alpacaCfg();
    QJsonObject polygonCfg();
    QJsonValue getFeed(const QString &name, const QString &url, const QList<QPair<QByteArray, QByteArray>> &headers);
    void polygonPages(const QString &firstUrl, int want, const std::function<bool(const QJsonObject &)> &eachRow);

    QString m_file;
    int m_timeoutMs;
    QMutex m_mutex;
    double m_memoMtime = -1;
    QJsonObject m_memo;
    struct Rest { double until; QString why; int status; };
    QHash<QString, Rest> m_resting;
    QSet<QString> m_disabled;
};

} // namespace qtm
