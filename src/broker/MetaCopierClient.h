#pragma once

// MetaCopier's REST API (docs.metacopier.io), the service holding the connections to the MetaTrader accounts. Nothing here
// retries an order: a request whose answer got lost may have been executed.

#include "broker/Broker.h"

#include <QHash>
#include <QObject>

#include <functional>

class QNetworkAccessManager;
class QNetworkReply;

namespace broker {

enum class SendOutcome {
    Accepted,   // the service took it (2xx)
    Rejected,   // it said no (4xx): nothing was opened
    Unknown,    // no clear answer (network, 5xx): it may have been executed
};

struct OrderRequest {
    QString symbol;
    bool buy = true;
    double lots = 0, stop = 0, target = 0;
    int requestId = 0;
};

class MetaCopierClient : public QObject
{
    Q_OBJECT
public:
    explicit MetaCopierClient(QObject *parent = nullptr);

    void setApiKey(const QString &key) { m_key = key; }
    bool hasKey() const { return !m_key.isEmpty(); }

    template <typename T> using Done = std::function<void(const T &, const QString &error)>;

    void accounts(Done<QVector<Account>> done);
    void information(const Account &account, Done<QJsonObject> done);
    void symbols(const Account &account, Done<QVector<SymbolSpec>> done);
    void quote(const Account &account, const QString &symbol, Done<Quote> done);
    void positions(const Account &account, Done<QVector<Position>> done);
    void openPosition(const Account &account, const OrderRequest &order, std::function<void(SendOutcome, const QString &message)> done);
    void closePosition(const Account &account, const QString &positionId, std::function<void(SendOutcome, const QString &message)> done);
    void closeAll(const Account &account, std::function<void(SendOutcome, const QString &message)> done);

    // Regional hosts answer the trading calls fastest; everything else goes to the global one.
    static QString tradingHost(const QString &regionName);
    // A test's fake server, from DEEPWATER_METACOPIER_URL: the loopback address only, so that a setting can't send the key away.
    static QString testServer();

private:
    QString base(const QString &host) const;
    QNetworkReply *get(const QString &url);
    QNetworkReply *send(const QByteArray &verb, const QString &url, const QByteArray &body);
    void getJson(const QString &url, std::function<void(const QJsonDocument &, const QString &error)> done);
    void getOutcome(QNetworkReply *reply, std::function<void(SendOutcome, const QString &message)> done);

    QNetworkAccessManager *m_net;
    QString m_key;
};

} // namespace broker
