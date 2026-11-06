#include "broker/MetaCopierClient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

namespace broker {

namespace {

constexpr int ReadTimeout = 15000;
constexpr int OrderTimeout = 20000;

QString messageOf(QNetworkReply *reply, const QByteArray &body)
{
    const QJsonObject o = QJsonDocument::fromJson(body).object();
    for (const char *field : {"message", "error", "detail", "title"})
        if (o.value(field).isString() && !o.value(field).toString().isEmpty())
            return o.value(field).toString();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status > 0)
        return QStringLiteral("HTTP %1").arg(status);
    return reply->errorString();
}

} // namespace

MetaCopierClient::MetaCopierClient(QObject *parent)
    : QObject(parent)
    , m_net(new QNetworkAccessManager(this))
{
}

QString MetaCopierClient::testServer()
{
    const QString url = qEnvironmentVariable("DEEPWATER_METACOPIER_URL");
    return url.startsWith("http://127.0.0.1:") ? url : QString();
}

QString MetaCopierClient::tradingHost(const QString &regionName)
{
    const QString region = regionName.toLower().remove(' ');
    for (const char *known : {"newyork", "london", "berlin", "singapore"})
        if (region == QLatin1String(known))
            return QStringLiteral("api-") + region;
    return QStringLiteral("api");
}

QString MetaCopierClient::base(const QString &host) const
{
    const QString test = testServer();
    return !test.isEmpty() ? test : QStringLiteral("https://%1.metacopier.io").arg(host);
}

QNetworkReply *MetaCopierClient::get(const QString &url)
{
    return send("GET", url, {});
}

QNetworkReply *MetaCopierClient::send(const QByteArray &verb, const QString &url, const QByteArray &body)
{
    QNetworkRequest request {QUrl(url)};
    request.setRawHeader("X-API-KEY", m_key.toUtf8());
    request.setRawHeader("Accept", "application/json");
    request.setTransferTimeout(verb == "GET" ? ReadTimeout : OrderTimeout);
    if (!body.isEmpty())
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    return m_net->sendCustomRequest(request, verb, body);
}

void MetaCopierClient::getJson(const QString &url, std::function<void(const QJsonDocument &, const QString &)> done)
{
    QNetworkReply *reply = get(url);
    connect(reply, &QNetworkReply::finished, this, [reply, done] {
        reply->deleteLater();
        const QByteArray body = reply->readAll();
        if (reply->error() != QNetworkReply::NoError) {
            done({}, messageOf(reply, body));
            return;
        }
        QJsonParseError parse;
        const QJsonDocument doc = QJsonDocument::fromJson(body, &parse);
        if (parse.error != QJsonParseError::NoError)
            done({}, parse.errorString());
        else
            done(doc, {});
    });
}

void MetaCopierClient::accounts(Done<QVector<Account>> done)
{
    getJson(base("api") + "/rest/api/v1/accounts?limit=200", [done](const QJsonDocument &doc, const QString &error) {
        QVector<Account> out;
        for (const QJsonValue &v : doc.array())
            out << accountFromJson(v.toObject());
        done(out, error);
    });
}

void MetaCopierClient::information(const Account &account, Done<QJsonObject> done)
{
    getJson(base(tradingHost(account.region)) + "/rest/api/v1/accounts/" + account.id + "/information",
            [done](const QJsonDocument &doc, const QString &error) { done(doc.object(), error); });
}

void MetaCopierClient::symbols(const Account &account, Done<QVector<SymbolSpec>> done)
{
    getJson(base(tradingHost(account.region)) + "/rest/api/v1/accounts/" + account.id + "/symbols",
            [done](const QJsonDocument &doc, const QString &error) {
                QVector<SymbolSpec> out;
                for (const QJsonValue &v : doc.array())
                    out << symbolFromJson(v.toObject());
                done(out, error);
            });
}

void MetaCopierClient::quote(const Account &account, const QString &symbol, Done<Quote> done)
{
    getJson(base(tradingHost(account.region)) + "/rest/api/v1/accounts/" + account.id + "/quote/" + QString::fromLatin1(QUrl::toPercentEncoding(symbol)),
            [done](const QJsonDocument &doc, const QString &error) { done(quoteFromJson(doc.object()), error); });
}

void MetaCopierClient::positions(const Account &account, Done<QVector<Position>> done)
{
    getJson(base(tradingHost(account.region)) + "/rest/api/v1/accounts/" + account.id + "/positions",
            [done](const QJsonDocument &doc, const QString &error) {
                QVector<Position> out;
                for (const QJsonValue &v : doc.array())
                    out << positionFromJson(v.toObject());
                done(out, error);
            });
}

void MetaCopierClient::getOutcome(QNetworkReply *reply, std::function<void(SendOutcome, const QString &)> done)
{
    connect(reply, &QNetworkReply::finished, this, [reply, done] {
        reply->deleteLater();
        const QByteArray body = reply->readAll();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() == QNetworkReply::NoError && status >= 200 && status < 300)
            done(SendOutcome::Accepted, {});
        else if (status >= 400 && status < 500)
            done(SendOutcome::Rejected, messageOf(reply, body));
        else
            done(SendOutcome::Unknown, messageOf(reply, body));
    });
}

void MetaCopierClient::openPosition(const Account &account, const OrderRequest &order, std::function<void(SendOutcome, const QString &)> done)
{
    const QJsonObject body {
        {"symbol", order.symbol},
        {"orderType", order.buy ? "Buy" : "Sell"},
        {"volume", order.lots},
        {"openPrice", 0},
        {"stopLoss", order.stop},
        {"takeProfit", order.target},
        {"requestId", order.requestId},
        {"comment", "Deepwater"},
    };
    getOutcome(send("POST", base(tradingHost(account.region)) + "/rest/api/v1/accounts/" + account.id + "/positions",
                    QJsonDocument(body).toJson(QJsonDocument::Compact)),
               std::move(done));
}

void MetaCopierClient::closePosition(const Account &account, const QString &positionId, std::function<void(SendOutcome, const QString &)> done)
{
    getOutcome(send("DELETE", base(tradingHost(account.region)) + "/rest/api/v1/accounts/" + account.id + "/positions/" + positionId, {}),
               std::move(done));
}

void MetaCopierClient::closeAll(const Account &account, std::function<void(SendOutcome, const QString &)> done)
{
    getOutcome(send("POST", base(tradingHost(account.region)) + "/rest/api/v1/accounts/" + account.id + "/actions/closeAllPositions", {}),
               std::move(done));
}

} // namespace broker
