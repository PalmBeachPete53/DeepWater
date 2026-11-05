#include "OfficialSources.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <algorithm>

const OfficialSources &OfficialSources::instance()
{
    static const OfficialSources catalogue;
    return catalogue;
}

OfficialSources::OfficialSources()
{
    QFile file(":/data_sources.json");
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonObject doc = QJsonDocument::fromJson(file.readAll()).object();
    m_generated = doc.value("generated").toString();
    m_pages = doc.value("pages").toInt();
    for (const QJsonValue &v : doc.value("sources").toArray()) {
        const QJsonObject o = v.toObject();
        Source s;
        s.name = o.value("name").toString();
        for (const QJsonValue &a : o.value("aliases").toArray())
            s.aliases << a.toString();
        s.url = o.value("url").toString();
        const QString kind = o.value("kind").toString();
        s.kind = kind == "stat"       ? Kind::Statistics
                 : kind == "cb"       ? Kind::CentralBank
                 : kind == "gov"      ? Kind::Government
                 : kind == "intl"     ? Kind::International
                 : kind == "exchange" ? Kind::Exchange
                 : kind == "te"       ? Kind::TradingEconomics
                                      : Kind::Private;
        const QString access = o.value("access").toString();
        s.access = access == "free"       ? Access::Free
                   : access == "headline" ? Access::Headline
                   : access == "paid"     ? Access::Paid
                   : access == "mixed"    ? Access::Mixed
                                          : Access::Proprietary;
        s.api = o.value("api").toString();
        for (const QJsonValue &c : o.value("countries").toArray())
            s.countries << c.toString();
        s.pages = o.value("pages").toInt();
        const QJsonObject test = o.value("test").toObject();
        if (!test.isEmpty()) {
            s.testOk = test.value("ok").toBool();
            s.testStatus = test.value("status").toInt();
            s.testMs = test.value("ms").toInt();
            s.testError = test.value("error").toString();
            s.testDate = test.value("date").toString();
        }
        m_sources.push_back(s);
    }
    const QJsonObject areas = doc.value("areas").toObject();
    for (auto it = areas.begin(); it != areas.end(); ++it)
        m_areas.insert(it.key(), it.value().toString());
    const QJsonObject countries = doc.value("countries").toObject();
    for (auto it = countries.begin(); it != countries.end(); ++it) {
        const QJsonObject c = it.value().toObject();
        m_pageNames.insert(it.key(), c.value("te").toString());
        QHash<QString, int> &indicators = m_indicators[it.key()];
        const QJsonObject ind = c.value("indicators").toObject();
        for (auto i = ind.begin(); i != ind.end(); ++i)
            indicators.insert(i.key(), i.value().toInt());
    }
}

QVector<OfficialSources::CountrySource> OfficialSources::forCountry(const QString &iso3) const
{
    QHash<int, CountrySource> bySource;
    const QHash<QString, int> indicators = m_indicators.value(iso3);
    for (auto it = indicators.begin(); it != indicators.end(); ++it) {
        CountrySource &cs = bySource[it.value()];
        cs.source = it.value();
        ++cs.indicators;
        cs.slugs << it.key();
    }
    QVector<CountrySource> out;
    for (CountrySource &cs : bySource) {
        std::sort(cs.slugs.begin(), cs.slugs.end());
        out.push_back(cs);
    }
    std::sort(out.begin(), out.end(), [this](const CountrySource &a, const CountrySource &b) {
        if (a.indicators != b.indicators)
            return a.indicators > b.indicators;
        return m_sources[a.source].name < m_sources[b.source].name;
    });
    return out;
}

const OfficialSources::Source *OfficialSources::sourceFor(const QString &iso3, const QString &slug) const
{
    const int i = m_indicators.value(iso3).value(slug, -1);
    return i >= 0 && i < m_sources.size() ? &m_sources[i] : nullptr;
}

QString OfficialSources::pageName(const QString &iso3) const
{
    return m_pageNames.value(iso3);
}

QStringList OfficialSources::indicatorsOf(int source) const
{
    if (const auto it = m_indicatorsOf.constFind(source); it != m_indicatorsOf.constEnd())
        return it.value();
    QHash<QString, int> counts;
    for (const auto &indicators : m_indicators)
        for (auto it = indicators.begin(); it != indicators.end(); ++it)
            if (it.value() == source)
                ++counts[it.key()];
    QStringList slugs = counts.keys();
    std::sort(slugs.begin(), slugs.end(), [&](const QString &a, const QString &b) {
        if (counts[a] != counts[b]) return counts[a] > counts[b];
        return a < b;
    });
    m_indicatorsOf.insert(source, slugs);
    return slugs;
}

int OfficialSources::reachableFreeCount() const
{
    int n = 0;
    for (const Source &s : m_sources)
        n += s.access == Access::Free && s.testOk;
    return n;
}

QString OfficialSources::countryName(const QString &iso3)
{
    // English names as in country_names.json, translated through the "Country" context (as the calendar does).
    static const QHash<QString, QString> names = [] {
        QHash<QString, QString> out;
        QFile file(":/country_names.json");
        if (file.open(QIODevice::ReadOnly)) {
            const QJsonObject o = QJsonDocument::fromJson(file.readAll()).object();
            for (auto it = o.begin(); it != o.end(); ++it)
                out.insert(it.key(), it.value().toArray().at(1).toString());
        }
        return out;
    }();
    if (const auto it = names.constFind(iso3); it != names.constEnd())
        return QCoreApplication::translate("Country", it.value().toUtf8().constData());
    const OfficialSources &c = instance();
    if (const auto it = c.m_areas.constFind(iso3); it != c.m_areas.constEnd())
        return QCoreApplication::translate("OfficialSources", it.value().toUtf8().constData());
    // Countries outside the IMF's list (Cuba, North Korea, Monaco, territories): the page name on
    // tradingeconomics.com, made readable.
    const QString page = c.m_pageNames.value(iso3);
    return page.isEmpty() ? iso3 : slugTitle(page);
}

QString OfficialSources::kindName(Kind kind)
{
    switch (kind) {
    case Kind::Statistics: return QCoreApplication::translate("OfficialSources", "Statistical office");
    case Kind::CentralBank: return QCoreApplication::translate("OfficialSources", "Central bank");
    case Kind::Government: return QCoreApplication::translate("OfficialSources", "Ministry or agency");
    case Kind::International: return QCoreApplication::translate("OfficialSources", "International organisation");
    case Kind::Private: return QCoreApplication::translate("OfficialSources", "Private institute");
    case Kind::Exchange: return QCoreApplication::translate("OfficialSources", "Exchange");
    case Kind::TradingEconomics: return QCoreApplication::translate("OfficialSources", "Trading Economics");
    }
    return {};
}

QString OfficialSources::accessName(Access access)
{
    switch (access) {
    case Access::Free: return QCoreApplication::translate("OfficialSources", "free");
    case Access::Headline: return QCoreApplication::translate("OfficialSources", "headline figure free, series paid");
    case Access::Paid: return QCoreApplication::translate("OfficialSources", "paid");
    case Access::Mixed: return QCoreApplication::translate("OfficialSources", "delayed quotes free, data paid");
    case Access::Proprietary: return QCoreApplication::translate("OfficialSources", "proprietary");
    }
    return {};
}

QString OfficialSources::slugTitle(const QString &slug)
{
    QString s = slug;
    s.replace('-', ' ');
    if (!s.isEmpty())
        s[0] = s[0].toUpper();
    // The abbreviations Trading Economics writes in capitals.
    for (const char *word : {"gdp", "cpi", "ppi", "pmi", "fdi", "gni", "ppp", "yoy", "mom", "qoq", "ism", "nfib", "api", "ipo", "cftc"}) {
        const QString w = QString::fromLatin1(word);
        s.replace(QRegularExpression("\\b" + w + "\\b", QRegularExpression::CaseInsensitiveOption), w.toUpper());
    }
    return s;
}
