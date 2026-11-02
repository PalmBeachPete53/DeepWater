#pragma once

#include <QColor>
#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QVector>

#include <cmath>

class CommodityBenchmarks;
class FuturesQuotes;

// Display strings (name, type, country) are translated copies of the raw
// strings from markets.json; see MarketData::retranslate().
struct Category {
    QString id;
    QString rawName;
    QString name;
    QColor baseColor;   // from markets.json
    QColor color;       // baseColor, unless the active theme overrides it
};

struct Commodity {
    QString id;
    QString rawName;
    QString name;
    QString categoryId;

    // Its futures contract on a US exchange, quoted by Yahoo (symbol empty when none is free and current).
    struct Future {
        QString symbol;
        QString roll;      // a root rolled by hand: its contract months (see FuturesQuotes::Contract)
        QString suffix;
        QString market;    // NYMEX, COMEX, CBOT, ICE US
        QString unit;
        QString rawWhat;   // what the contract is, when its name doesn't say
        QString what;
        bool dailyOnly = false;
    } future;
    // Its benchmark in the IMF's monthly prices.
    struct Benchmark {
        QString code;      // POILWTI
        QString unit;
        QString rawWhat;
        QString what;
    } imf;

    // The price shown: the futures quote when there's a current one, else the IMF's latest monthly average.
    enum class Source { None, Futures, Monthly };
    Source source = Source::None;
    QString unit;                       // of the price shown
    double price = qQNaN();
    QDateTime time;                     // of the price: a trade, a day's close (settlement), a month (average)
    bool settlement = false;
    double previousClose = qQNaN();     // the day or month before
    QDate previousDate;
    QString contract;                   // the symbol quoted, "CL=F"
    QString contractName;               // Yahoo's: "Crude Oil Dec 26"
    // The chart: 30-minute closes over five days, daily closes over three months, or monthly averages over two
    // years. Oldest first.
    enum class Span { FiveDays, ThreeMonths, TwoYears };
    Span span = Span::FiveDays;
    QVector<double> history;
    // The IMF's latest monthly average, shown next to a futures price as a reference.
    double benchmark = qQNaN();
    QDate benchmarkMonth;

    bool hasPrice() const { return !std::isnan(price); }
    // NaN when there's nothing to compare with.
    double changePct() const { return (price - previousClose) / previousClose * 100.0; }
};

struct Site {
    int index = -1;
    QString name;
    QString rawCountry;
    QString country;
    double lat = 0;
    double lon = 0;
    QString commodityId;
    QString categoryId;   // commodity category, or "hub" for exchanges/trading hubs
    QString rawType;
    QString type;
};

// Holds categories, commodities and sites, and the real prices of the commodities (applyPrices()).
class MarketData : public QObject
{
    Q_OBJECT
public:
    explicit MarketData(QObject *parent = nullptr);

    bool load(const QString &path);
    // Re-applies the installed translators to category/commodity/site strings.
    void retranslate();
    // Category id -> color overrides from the active theme (empty = defaults).
    void setCategoryColors(const QHash<QString, QColor> &overrides);

    const QVector<Category> &categories() const { return m_categories; }
    const QVector<Commodity> &commodities() const { return m_commodities; }
    const QVector<Site> &sites() const { return m_sites; }

    const Category *category(const QString &id) const;
    const Commodity *commodity(const QString &id) const;
    QColor categoryColor(const QString &id) const;

    int siteCount(const QString &categoryId) const;
    QVector<const Site *> sitesForCommodity(const QString &commodityId) const;

    bool isCategoryVisible(const QString &id) const { return !m_hidden.contains(id); }
    void setCategoryVisible(const QString &id, bool visible);

    // Takes the latest prices from the futures quotes and the IMF's monthly averages (either may be null).
    void applyPrices(const FuturesQuotes *futures, const CommodityBenchmarks *benchmarks);

signals:
    void pricesUpdated();
    void filterChanged();

private:
    QVector<Category> m_categories;
    QVector<Commodity> m_commodities;
    QVector<Site> m_sites;
    QHash<QString, int> m_categoryIndex;
    QHash<QString, int> m_commodityIndex;
    QSet<QString> m_hidden;
};
