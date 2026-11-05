#pragma once

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

// The catalogue of the data sources named on tradingeconomics.com (resources/data_sources.json): for every
// country, the body behind each of its indicators (statistical office, central bank, ministry, international
// organisation, private institute), with its site, whether its data is free and how a program can read it.
// Built from a crawl of the site's public pages; the sources are the ones Trading Economics credits.
class OfficialSources
{
public:
    enum class Kind { Statistics, CentralBank, Government, International, Private, Exchange, TradingEconomics };
    enum class Access { Free, Headline, Paid, Mixed, Proprietary };
    struct Source {
        QString name;
        QStringList aliases;     // the other names it is credited under, found by the search
        QString url;
        Kind kind;
        Access access;
        QString api;             // how a program reads it ("web" when the site only publishes tables)
        QStringList countries;   // ISO3 codes (and areas: EA, EU, WLD…)
        int pages = 0;           // indicator pages it is the source of
        // The last availability test of the site made when the catalogue was built (the inventory of the audit):
        // whether it answered, the HTTP status, the time to the headers, the date. testDate empty: never tested.
        bool testOk = false;
        int testStatus = 0;
        int testMs = 0;
        QString testError;
        QString testDate;
    };

    static const OfficialSources &instance();

    const QVector<Source> &sources() const { return m_sources; }
    QString generated() const { return m_generated; }
    int pageCount() const { return m_pages; }
    bool isEmpty() const { return m_sources.isEmpty(); }

    // The sources of a country's indicators, the one behind most of them first, with that count.
    struct CountrySource {
        int source;
        int indicators;
        QStringList slugs;   // the indicators, as named on tradingeconomics.com
    };
    QVector<CountrySource> forCountry(const QString &iso3) const;
    // The source of one indicator of a country (its tradingeconomics.com name), or null.
    const Source *sourceFor(const QString &iso3, const QString &slug) const;
    // "united-states": the country's page name on tradingeconomics.com, empty when it has none.
    QString pageName(const QString &iso3) const;
    // The indicators a source publishes, over every country, most countries first.
    QStringList indicatorsOf(int source) const;
    // How many of the free sources' sites answered the last availability test.
    int reachableFreeCount() const;

    // The (translated) name of a country of the catalogue, or of an area (EA, EU, WLD…).
    static QString countryName(const QString &iso3);
    static QString kindName(Kind kind);
    static QString accessName(Access access);
    // "gdp-growth-annual" -> "GDP growth annual".
    static QString slugTitle(const QString &slug);

private:
    OfficialSources();

    QVector<Source> m_sources;
    QHash<QString, QHash<QString, int>> m_indicators;   // iso3 -> slug -> source index
    QHash<QString, QString> m_pageNames;
    QHash<QString, QString> m_areas;   // EA -> "Euro area"
    mutable QHash<int, QStringList> m_indicatorsOf;
    QString m_generated;
    int m_pages = 0;
};
