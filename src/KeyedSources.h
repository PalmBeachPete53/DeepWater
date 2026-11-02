#pragma once

#include "DomesticFinance.h"

#include <QNetworkAccessManager>

// Sources that need a free key per user, entered in the Sources page and kept in the settings ("keys/<id>").
// Without a key a data set stays off ("No key" in Sources); with one it behaves like the others. FRED, the EIA
// and the BEA feed the United States' latest figures; Banxico, the Bank of Korea and the Turkish central bank
// their country's; UN Comtrade the trade by product of the country shown.

// The key of a keyed source, from the settings; empty when none.
QString sourceKey(const QString &id);
void setSourceKey(const QString &id, const QString &key);

class KeyedSeriesSet : public CountrySeriesSet
{
    Q_OBJECT
public:
    QString keyId() const { return m_keyId; }
    bool hasKey() const { return !sourceKey(m_keyId).isEmpty(); }

protected:
    KeyedSeriesSet(const QString &cacheName, const QString &keyId, qint64 staleSecs, int seriesCount, QObject *parent);
    bool ready(QString *why) const override;
    QString key() const { return sourceKey(m_keyId); }

private:
    QString m_keyId;
};

// FRED (Federal Reserve Bank of St. Louis): US series the other compilations lack. One request per series.
class FredIndicators : public KeyedSeriesSet
{
    Q_OBJECT
public:
    enum Series {
        PceInflationYoY,     // PCE price index, % over a year (from the index)
        HousingStarts,       // thousands of units, annual rate
        InitialClaims,       // weekly, thousands
        TradeBalance,        // goods and services, billions of dollars, monthly
        ConsumerSentiment,   // University of Michigan index
        SeriesCount
    };
    explicit FredIndicators(QObject *parent = nullptr);

protected:
    QList<QUrl> urls() const override;
    QByteArray joinParts(const QList<QByteArray> &parts) const override;
};

// US Energy Information Administration (API v2): weekly petroleum figures.
class EiaIndicators : public KeyedSeriesSet
{
    Q_OBJECT
public:
    enum Series {
        CrudeStocks,       // commercial crude oil stocks, million barrels
        CrudeProduction,   // thousand barrels a day
        GasolinePrice,     // regular retail, dollars a gallon
        SeriesCount
    };
    explicit EiaIndicators(QObject *parent = nullptr);

protected:
    QList<QUrl> urls() const override;
    QByteArray joinParts(const QList<QByteArray> &parts) const override;
};

// US Bureau of Economic Analysis: real GDP, % change at an annual rate (NIPA table 1.1.1).
class BeaIndicators : public KeyedSeriesSet
{
    Q_OBJECT
public:
    enum Series { GdpAnnualized, SeriesCount };
    explicit BeaIndicators(QObject *parent = nullptr);

protected:
    QList<QUrl> urls() const override;
    QByteArray joinParts(const QList<QByteArray> &parts) const override;
};

// Banco de México (SIE API): annual inflation and the target rate.
class BanxicoIndicators : public KeyedSeriesSet
{
    Q_OBJECT
public:
    enum Series { CpiYoY, PolicyRate, SeriesCount };
    explicit BanxicoIndicators(QObject *parent = nullptr);

protected:
    QList<QUrl> urls() const override;
    QByteArray joinParts(const QList<QByteArray> &parts) const override;
};

// Bank of Korea (ECOS API): consumer prices (index, % over a year computed here).
class EcosIndicators : public KeyedSeriesSet
{
    Q_OBJECT
public:
    enum Series { CpiYoY, SeriesCount };
    explicit EcosIndicators(QObject *parent = nullptr);

protected:
    QList<QUrl> urls() const override;
    QByteArray joinParts(const QList<QByteArray> &parts) const override;
};

// Central Bank of the Republic of Türkiye (EVDS): consumer prices (index, % over a year computed here).
class EvdsIndicators : public KeyedSeriesSet
{
    Q_OBJECT
public:
    enum Series { CpiYoY, SeriesCount };
    explicit EvdsIndicators(QObject *parent = nullptr);

protected:
    QList<QUrl> urls() const override;
    void prepare(QNetworkRequest &request) const override;
    QByteArray joinParts(const QList<QByteArray> &parts) const override;
};

// UN Comtrade: a country's exports and imports by product (HS chapters) for the latest full year, downloaded
// when its sheet asks for it. With a subscription key the data API is used; without one the public preview
// endpoint (500 records at most, rate limited), which is enough for the 97 chapters of one country.
class ComtradeProducts : public QObject
{
    Q_OBJECT
public:
    struct Product {
        QString code;   // HS chapter, "27"
        QString name;
        double value = 0;   // US dollars
    };
    struct Trade {
        int year = 0;
        QVector<Product> exports, imports;   // largest first
        double exportTotal = 0, importTotal = 0;
        QString error;
        bool valid() const { return year > 0 && error.isEmpty(); }
    };

    explicit ComtradeProducts(QObject *parent = nullptr);

    static QString keyId() { return "comtrade"; }
    bool hasKey() const { return !sourceKey(keyId()).isEmpty(); }
    // The cached trade of a country (invalid when not downloaded yet); request() downloads it when missing or
    // older than a month and emits ready().
    Trade trade(const QString &iso3) const;
    void request(const QString &iso3);
    bool isFetching(const QString &iso3) const { return m_fetching.contains(iso3); }
    QDateTime fetchedAt(const QString &iso3) const;
    // When a country's trade last came in, this session (for the Sources page).
    QDateTime lastFetch() const { return m_lastFetch; }
    QString lastError() const { return m_lastError; }
    QUrl probeUrl() const;

    void setEnabled(bool on) { m_enabled = on; }
    bool isEnabled() const { return m_enabled; }

signals:
    void ready(const QString &iso3);

private:
    void fetchReporters(const std::function<void()> &then);
    void fetchTrade(const QString &iso3);
    static Trade parse(const QByteArray &body, QString *error);

    QNetworkAccessManager m_net;
    QHash<QString, int> m_reporters;   // ISO3 -> Comtrade reporter code
    QHash<QString, Trade> m_trades;
    QHash<QString, QDateTime> m_fetchedAt;
    QDateTime m_lastFetch;
    QSet<QString> m_fetching;
    QString m_lastError;
    bool m_enabled = true;
    bool m_reportersLoading = false;
    QVector<std::function<void()>> m_afterReporters;
};
