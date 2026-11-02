#pragma once

// Port of lib/market-info.js: earnings and dividend calendars, news, financial statements and key
// stats. Nasdaq's public API for US stocks; Cointelegraph's and Decrypt's RSS feeds for crypto
// headlines.

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

namespace qtm {

class Providers;

class MarketInfo
{
public:
    explicit MarketInfo(Providers &data);

    QJsonArray earnings(double days = 5);
    QJsonArray dividends(double days = 5);
    QJsonObject news(const QString &symbol);
    // frequency: JS default "annual" when the parameter is missing (hasFrequency=false).
    QJsonValue financials(const QString &symbol, const QString &frequency, bool hasFrequency);

    static QJsonArray parseRss(const QString &xml, const QString &source);

private:
    QJsonArray cryptoNews();
    Providers &m_data;
};

} // namespace qtm
