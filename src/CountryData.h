#pragma once

#include "ResearchData.h"

#include <QDate>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>

class KeyIndicators;

// Economy › Countries: a country's indicators in tables (Overview, GDP, Labour, Prices, Money, Trade, Government,
// Business, Consumer, Housing, Energy, Health), each row the last value, the one before, the highest and lowest of
// the series' history, its unit and period. The rows and where each comes from are in
// resources/country_indicators.json: for each indicator, a list of sources tried in order, the first that covers
// the country and answers winning. Providers ("p"):
//   fred      FRED series `id` (United States; FRED's API with the key saved in Sources)
//   wb        World Bank indicator `id`: WDI, annual (every country; one request per country for all of them), or
//             with "source": "15" the Global Economic Monitor, monthly
//   ilo       ILOSTAT indicator `id` (national labour force surveys), the total unless `classif` says otherwise
//   ecb       ECB Data Portal data set `flow` ("BSI"), series `key` with {ECB} (U2 for the euro area)
//   weo       IMF World Economic Outlook indicator `id`, annual, the years before the current one only
//   imf       IMF data set `flow` (IMF.STA), series `key` without the country ("U.PT.M"); one request per data set
//   oecd      OECD data flow `flow` ("agency,id,version"), series `key` with {ISO3}; one request per flow
//   eurostat  Eurostat data set `ds` filtered by `q`, the country as geo
//   bis       BIS data flow `flow` ("WS_CBPOL/1.0"), series `key` with {ISO2} or {BIS} (the euro area for members)
//   yahoo     `kind`: "currency" (the dollar index or the country's currency against the dollar) or "stock" (the
//             main index), daily closes
//   eia       EIA series `id` (United States; the key saved in Sources)
//   ons       Office for National Statistics series `id`, its path ("economy/…/timeseries/d7g7/mm23") (United Kingdom)
//   boe       Bank of England statistical database series `id` (United Kingdom)
//   National offices and central banks, each for its own country: bcb (Brazil, SGS series `id`, "kind": "daily"
//   for a daily one), statcan (Canada, vector `id`), boc (Canada, Valet series `id`), boj (Japan, data base `flow`
//   and series `id`), snb (Switzerland, cube `flow`, dimensions `key`), abs (Australia, SDMX `flow` and `key`),
//   bbk (Germany, Bundesbank `flow` and `key`), insee (France, BDM idbank `id`), ine (Spain, Tempus series `id`),
//   pxweb (Norway "ssb" or Sweden "scb" as `flow`, table `id`, other dimensions' values in `q`); with the user's
//   free key (QSettings keys/<provider>): ecos (South Korea, "STAT/CYCLE/ITEM…" `id`), banxico and inegi (Mexico,
//   series / indicator `id`), evds (Türkiye, series `id`), bps (Indonesia, variable `id`, "vervar/turvar" `key`,
//   2015 on), estat (Japan, statsDataId `id`, filters in `q`)
//   survey    a private survey's headline (KeyIndicators id: ManufacturingPmi…): the last value and the previous
//             only, from the economic calendar
// Optional: `t` the transformation ("yoy" % over a year, "pop" % on the previous period, "diff" change on the
// previous period), `scale` (applied to the source's values), `minus` / `plus` / `div` a second series of the same
// request (the first less, plus, or divided by the second), `unit` and `note` shown instead of the indicator's; a
// unit's {CUR} is the country's currency; `since` ("1994-07") drops earlier periods; `only` restricts a source to some countries ("EMU" for an aggregate).
namespace countries {

using research::Point;
using research::Series;

struct SourceSpec {
    QString provider;
    QString id, flow, key, minus, plus, div, ds, query, kind, transform, unit, note;
    QString source;    // wb: the World Bank database (2 WDI, the default; 15 Global Economic Monitor, monthly)
    QString classif;   // ilo: the breakdown wanted (default the total aged 15+; "-" for none)
    QStringList only;  // the countries this source is for (all those its provider covers when empty)
    QDate since;       // the first period kept (a currency change, a break in the series)
    double scale = 1;
};

struct IndicatorDef {
    QString id;
    QString name;   // English; translated in the "CountryIndicators" context
    QString unit;   // likewise
    QVector<SourceSpec> sources;
};

struct TabDef {
    QString id;
    QString name;
    QVector<QStringList> tables;   // indicator ids, a table each
};

struct Group {
    QString name;
    QStringList countries;   // ISO3 ("EMU" for the euro area)
};

struct Catalogue {
    QVector<Group> groups;
    QVector<TabDef> tabs;
    QHash<QString, IndicatorDef> indicators;
    QSet<QString> euroMembers, oecdAreas, eurostatGeos;
    QHash<QString, QString> currencies, stockIndexes, extraNames;
    // Every id or key a bulk request needs, so one request per country serves every tab.
    QHash<QString, QStringList> wbIds;   // World Bank database -> indicator ids
    QStringList weoIds;
    QHash<QString, QStringList> imfKeys;    // flow -> series keys
    QHash<QString, QStringList> oecdKeys;   // flow -> series keys with {ISO3}

    bool covers(const SourceSpec &s, const QString &iso3) const;
};
const Catalogue &catalogue();

// Translated names: a country (ISO3, "EMU" for the euro area), an indicator's name or unit, a tab or group.
QString countryName(const QString &iso3);
QString text(const QString &english);

// The KeyIndicators façade (survey headlines); set by the main window.
void setKeyIndicators(KeyIndicators *indicators);

enum class Freq { Unknown, Daily, Weekly, Monthly, Quarterly, Annual };

struct Row {
    enum State { Pending, Found, Missing };
    State state = Pending;
    double last = research::NaN, previous = research::NaN, highest = research::NaN, lowest = research::NaN;
    QDate date;                 // first day of the latest period
    Freq freq = Freq::Unknown;
    QString unit;               // English, as in the catalogue ({CUR}: the currency)
    QString currency;           // the country's currency code, for {CUR}
    QString unitText() const;   // translated, the currency filled in
    QString source;             // "FRED", "World Bank", "IMF"…, translated
    QString code;               // the series, for the tooltip
    QString note;               // English
    QString error;              // why it is missing (a key needed, an error)
    bool needsKey = false;      // missing because a free key isn't saved (FRED, EIA)
    bool latestOnly = false;    // only the last and previous values are known (survey headlines)
};

// "Sep 2026", "Q3 2026", "2025", "24 Oct 2026".
QString periodLabel(const QDate &date, Freq freq);
// 30770, 3.4, -0.68: no more than two decimals, none from 100 on (unless `decimals` says), trailing zeros dropped.
QString formatValue(double value, int decimals = -1);

// Loads one country's rows tab by tab, as they are shown; `rowChanged` as each one settles.
class CountryLoader : public QObject
{
    Q_OBJECT
public:
    explicit CountryLoader(const QString &iso3, QObject *parent = nullptr);
    QString iso3() const { return m_iso3; }
    void loadTab(int tab);
    // Downloads again (cached answers become stale) and reloads the tabs loaded so far.
    void refresh();
    Row row(const QString &id) const { return m_rows.value(id); }
    int pending() const { return m_pending; }

signals:
    void rowChanged(const QString &id);
    void busyChanged();

private:
    using SeriesDone = std::function<void(const Series &series, const QString &error)>;
    void resolve(const QString &id, int from);
    void fetch(const SourceSpec &s, SeriesDone done);
    // A request whose answer holds several series (World Bank, WEO, IMF, OECD): parsed once, the series by key.
    using Bulk = QHash<QString, Series>;
    void bulk(const QString &key, const QUrl &url, qint64 maxAge, bool json,
              std::function<Bulk(const QByteArray &)> parse, std::function<void(const Bulk &, const QString &)> done);
    QString placeholders(QString key) const;
    void settle(const QString &id, const Row &row);

    QString m_iso3;
    QHash<QString, Row> m_rows;
    QHash<QString, Row> m_stale;   // a series found that stopped long ago, while later sources are tried
    QSet<int> m_loadedTabs;
    QSet<QString> m_waitedSurveys;   // survey rows that waited for the calendar once
    QHash<QString, Bulk> m_bulk;
    QHash<QString, QString> m_bulkErrors;
    QHash<QString, QVector<std::function<void(const Bulk &, const QString &)>>> m_bulkWaiting;
    int m_pending = 0;
    int m_generation = 0;
};

} // namespace countries
