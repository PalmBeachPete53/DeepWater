#pragma once

#include "CountryData.h"

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QVector>

namespace countries {

// Economy › Indicators: the list of indicators by section (resources/indicator_list.json), Trading Economics'
// names. Each item is read from rows of the Countries catalogue, the first a country has winning; an item without
// rows has no free source and is shown greyed with the reason.
struct ListItem {
    QString name;          // English (the "CountryIndicators" context translates it)
    QStringList ids;       // catalogue indicators, in order; those missing from the catalogue are left out
    QStringList calendar;  // other names of its releases in the economic calendar, lower case
    QString why;           // English: why there is no source
};

struct ListSection {
    QString name;
    int column = 0;
    bool star = false;
    QVector<ListItem> items;
};

const QVector<ListSection> &indicatorList();
// The item of that name (English, any case), or null.
const ListItem *findListItem(const QString &name);

// The next releases of the indicators, from TradingView's economic calendar (the Calendar page's source): every
// country it covers (~130) from this week's Monday over ten weeks, one request a week (it serves 2000 events at most
// per request), each kept twelve hours. An event's "indicator" field carries Trading Economics' name.
class ReleaseCalendar : public QObject
{
    Q_OBJECT
public:
    static ReleaseCalendar *instance();
    // Downloads the weeks not loaded (or loaded more than twelve hours ago).
    void load();
    bool loading() const { return m_left > 0; }
    struct Release {
        QDateTime time;   // UTC
        QString period;   // the period the figure covers: "Oct", "Q3"
    };
    // The country's next release of the item after now; an invalid time when none is planned in the weeks loaded.
    Release next(const QString &iso3, const ListItem &item) const;
    // Whether the calendar has releases of the item (for any country).
    bool covers(const ListItem &item) const;

signals:
    void updated();

private:
    explicit ReleaseCalendar(QObject *parent = nullptr);
    QStringList names(const ListItem &item) const;

    QHash<QString, QVector<Release>> m_releases;   // "FR|inflation rate" -> by time
    QSet<QString> m_names;                          // indicator names (lower case) with releases
    QHash<QDate, QDateTime> m_weekLoaded;           // a week's Monday -> when its answer came
    int m_left = 0;
};

// One list item for every country of the catalogue's groups: a CountryLoader per country, sharing one CrossCountry
// (so World Bank, IMF, OECD, Eurostat… are asked once for all of them); a country whose first row is missing tries
// the item's next one.
class IndicatorLoader : public QObject
{
    Q_OBJECT
public:
    explicit IndicatorLoader(const ListItem &item, QObject *parent = nullptr);
    const ListItem &item() const { return m_item; }
    const QStringList &countries() const { return m_countries; }
    Row row(const QString &iso3) const;
    int pending() const;
    // Downloads again (cached answers become stale).
    void refresh();

signals:
    void rowChanged(const QString &iso3);
    void busyChanged();

private:
    void start();
    void changed(const QString &iso3, const QString &id);

    ListItem m_item;
    QStringList m_countries;
    CrossCountry *m_shared = nullptr;
    QHash<QString, CountryLoader *> m_loaders;
    QHash<QString, int> m_which;   // the item's id each country is on
};

} // namespace countries
