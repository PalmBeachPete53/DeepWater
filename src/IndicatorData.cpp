#include "IndicatorData.h"

#include "ResearchData.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QTimeZone>
#include <QUrlQuery>

#include <algorithm>

namespace countries {

// ---------------------------------------------------------------- list

const QVector<ListSection> &indicatorList()
{
    static const QVector<ListSection> list = [] {
        QVector<ListSection> out;
        QFile f(":/indicator_list.json");
        if (!f.open(QIODevice::ReadOnly))
            return out;
        const Catalogue &cat = catalogue();
        for (const QJsonValue &sv : QJsonDocument::fromJson(f.readAll()).object().value("sections").toArray()) {
            const QJsonObject so = sv.toObject();
            ListSection section;
            section.name = so.value("name").toString();
            section.column = so.value("column").toInt();
            section.star = so.value("star").toBool();
            for (const QJsonValue &iv : so.value("items").toArray()) {
                const QJsonObject io = iv.toObject();
                ListItem item;
                item.name = io.value("name").toString();
                for (const QJsonValue &id : io.value("ids").toArray())
                    if (cat.indicators.contains(id.toString()))
                        item.ids << id.toString();
                for (const QJsonValue &c : io.value("calendar").toArray())
                    item.calendar << c.toString().toLower();
                item.why = io.value("why").toString();
                if (item.ids.isEmpty() && item.why.isEmpty())
                    item.why = "No free source publishes it country by country";
                section.items << item;
            }
            out << section;
        }
        return out;
    }();
    return list;
}

const ListItem *findListItem(const QString &name)
{
    for (const ListSection &s : indicatorList())
        for (const ListItem &item : s.items)
            if (item.name.compare(name, Qt::CaseInsensitive) == 0)
                return &item;
    return nullptr;
}

// ---------------------------------------------------------------- calendar

ReleaseCalendar *ReleaseCalendar::instance()
{
    static ReleaseCalendar *calendar = new ReleaseCalendar(qApp);
    return calendar;
}

ReleaseCalendar::ReleaseCalendar(QObject *parent)
    : QObject(parent)
{
}

void ReleaseCalendar::load()
{
    const QDate today = QDate::currentDate();
    const QDate monday = today.addDays(1 - today.dayOfWeek());
    const QDateTime now = QDateTime::currentDateTimeUtc();
    for (int w = 0; w < 10; ++w) {
        const QDate from = monday.addDays(7 * w);
        const QDateTime loaded = m_weekLoaded.value(from);
        if (loaded.isValid() && loaded.secsTo(now) < 12 * 3600)
            continue;
        if (m_weekLoaded.contains(from) && !loaded.isValid())
            continue;   // under way
        m_weekLoaded.insert(from, QDateTime());
        auto iso = [](const QDate &d) { return d.startOfDay(QTimeZone::UTC).toString("yyyy-MM-ddTHH:mm:ss.000Z"); };
        QUrl url("https://economic-calendar.tradingview.com/events");
        QUrlQuery q;
        q.addQueryItem("from", iso(from));
        q.addQueryItem("to", iso(from.addDays(7)));
        url.setQuery(q);
        ++m_left;
        QPointer<ReleaseCalendar> self(this);
        research::Client::instance()->raw(
            "tradingview:releases:" + from.toString(Qt::ISODate), url, 12 * 3600, true,
            {{"Origin", "https://www.tradingview.com"}}, this, [self, from](const QByteArray &body, const QString &) {
                if (!self)
                    return;
                --self->m_left;
                self->m_weekLoaded.insert(from, QDateTime::currentDateTimeUtc());
                const QJsonObject root = QJsonDocument::fromJson(body).object();
                // A week loaded again replaces what it had.
                const QDateTime start = from.startOfDay(QTimeZone::UTC), end = from.addDays(7).startOfDay(QTimeZone::UTC);
                for (auto it = self->m_releases.begin(); it != self->m_releases.end(); ++it)
                    it->erase(std::remove_if(it->begin(), it->end(), [&](const Release &r) { return r.time >= start && r.time < end; }),
                              it->end());
                for (const QJsonValue &v : root.value("result").toArray()) {
                    const QJsonObject o = v.toObject();
                    const QString name = o.value("indicator").toString().toLower();
                    const QDateTime time = QDateTime::fromString(o.value("date").toString(), Qt::ISODateWithMs).toUTC();
                    if (name.isEmpty() || !time.isValid())
                        continue;
                    self->m_names.insert(name);
                    auto &list = self->m_releases[o.value("country").toString() + "|" + name];
                    list.push_back({time, o.value("period").toString()});
                    std::sort(list.begin(), list.end(), [](const Release &a, const Release &b) { return a.time < b.time; });
                }
                emit self->updated();
            });
    }
}

QStringList ReleaseCalendar::names(const ListItem &item) const
{
    QStringList out {item.name.toLower()};
    out += item.calendar;
    return out;
}

ReleaseCalendar::Release ReleaseCalendar::next(const QString &iso3, const ListItem &item) const
{
    const QString country = iso2(iso3);
    if (country.isEmpty())
        return {};
    const QDateTime now = QDateTime::currentDateTimeUtc();
    Release best;
    for (const QString &name : names(item))
        for (const Release &r : m_releases.value(country + "|" + name))
            if (r.time > now && (!best.time.isValid() || r.time < best.time)) {
                best = r;
                break;
            }
    return best;
}

bool ReleaseCalendar::covers(const ListItem &item) const
{
    for (const QString &name : names(item))
        if (m_names.contains(name))
            return true;
    return false;
}

// ---------------------------------------------------------------- loader

IndicatorLoader::IndicatorLoader(const ListItem &item, QObject *parent)
    : QObject(parent)
    , m_item(item)
    , m_countries(allCountries())
{
    start();
}

void IndicatorLoader::start()
{
    m_shared = new CrossCountry(this);
    for (const QString &iso3 : std::as_const(m_countries)) {
        auto *loader = new CountryLoader(iso3, this);
        loader->setShared(m_shared);
        m_loaders.insert(iso3, loader);
        m_which.insert(iso3, 0);
        connect(loader, &CountryLoader::rowChanged, this, [this, iso3](const QString &id) { changed(iso3, id); });
        connect(loader, &CountryLoader::busyChanged, this, &IndicatorLoader::busyChanged);
    }
    if (m_item.ids.isEmpty())
        return;
    for (const QString &iso3 : std::as_const(m_countries))
        m_loaders.value(iso3)->loadIndicator(m_item.ids.first());
}

void IndicatorLoader::changed(const QString &iso3, const QString &id)
{
    const int which = m_which.value(iso3);
    if (m_item.ids.value(which) != id)
        return;
    CountryLoader *loader = m_loaders.value(iso3);
    if (loader->row(id).state == Row::Missing && which + 1 < m_item.ids.size()) {
        m_which.insert(iso3, which + 1);
        loader->loadIndicator(m_item.ids[which + 1]);
        return;
    }
    emit rowChanged(iso3);
}

Row IndicatorLoader::row(const QString &iso3) const
{
    const CountryLoader *loader = m_loaders.value(iso3);
    if (!loader || m_item.ids.isEmpty()) {
        Row r;
        r.state = Row::Missing;
        return r;
    }
    return loader->row(m_item.ids.value(m_which.value(iso3)));
}

int IndicatorLoader::pending() const
{
    int n = 0;
    for (const QString &iso3 : m_countries)
        if (row(iso3).state == Row::Pending)
            ++n;
    return n;
}

void IndicatorLoader::refresh()
{
    CountryLoader::forgetCached();
    qDeleteAll(m_loaders);
    m_loaders.clear();
    m_which.clear();
    delete m_shared;
    start();
    for (const QString &iso3 : std::as_const(m_countries))
        emit rowChanged(iso3);
    emit busyChanged();
}

} // namespace countries
