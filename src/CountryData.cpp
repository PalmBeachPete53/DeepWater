#include "CountryData.h"

#include "KeyIndicators.h"
#include "KeyedSources.h"
#include "SurveyHeadlines.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QPointer>
#include <QRegularExpression>
#include <QTimeZone>
#include <QTimer>
#include <QUrlQuery>

#include <algorithm>
#include <cmath>
#include <memory>

namespace countries {

namespace {

constexpr qint64 Hour = 3600;

QPointer<KeyIndicators> g_keyIndicators;

QStringList strings(const QJsonValue &v)
{
    QStringList out;
    for (const QJsonValue &x : v.toArray())
        out << x.toString();
    return out;
}

QHash<QString, QString> stringMap(const QJsonValue &v)
{
    QHash<QString, QString> out;
    const QJsonObject o = v.toObject();
    for (auto it = o.begin(); it != o.end(); ++it)
        out.insert(it.key(), it.value().toString());
    return out;
}

// ISO3 -> [ISO2, English name, continent], from resources/country_names.json.
const QJsonObject &countryNames()
{
    static const QJsonObject names = [] {
        QFile f(":/country_names.json");
        return f.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(f.readAll()).object() : QJsonObject();
    }();
    return names;
}

QString iso2Of(const QString &iso3)
{
    return countryNames().value(iso3).toArray().at(0).toString();
}

// Adds each part of a dotted series key to the per-position alternatives: "A.B" and "A.C" -> {A}, {B, C}.
void addKeyParts(QVector<QStringList> &parts, const QString &key)
{
    const QStringList p = key.split('.');
    if (parts.isEmpty())
        parts.resize(p.size());
    for (int i = 0; i < p.size() && i < parts.size(); ++i)
        if (!parts[i].contains(p[i]))
            parts[i] << p[i];
}

QString joinKeyParts(const QVector<QStringList> &parts)
{
    QStringList out;
    for (const QStringList &alternatives : parts)
        out << alternatives.join('+');
    return out.join('.');
}

// "2026-09", "2026-M09", "2026-Q3", "2026", "2026-09-01" -> the first day of the period.
QDate periodStart(const QString &raw)
{
    const QString s = raw.trimmed();
    if (s.size() == 4)
        return QDate(s.toInt(), 1, 1);
    if (s.size() >= 10 && s[4] == '-' && s[7] == '-')
        return QDate::fromString(s.left(10), Qt::ISODate);
    const int year = s.left(4).toInt();
    if (year <= 0)
        return {};
    // "2026-M07", "2026M07" (World Bank), "2026-Q3", "2026Q3", "2026-W42" (ECB), "2026-S2" (half-year).
    const QString rest = s.at(4) == '-' ? s.mid(5) : s.mid(4);
    if (rest.startsWith('W')) {
        // The Monday of ISO week n: the week holding January 4th is week 1.
        const QDate jan4(year, 1, 4);
        return jan4.addDays(1 - jan4.dayOfWeek()).addDays(7 * (rest.mid(1).toInt() - 1));
    }
    if (rest.startsWith('Q') || rest.startsWith('K'))   // K: kvartal (Statistics Norway)
        return QDate(year, (rest.mid(1).toInt() - 1) * 3 + 1, 1);
    if (rest.startsWith('M'))
        return QDate(year, rest.mid(1).toInt(), 1);
    if (rest.startsWith('S'))   // half-year
        return QDate(year, rest.mid(1).toInt() == 2 ? 7 : 1, 1);
    return QDate(year, rest.toInt(), 1);
}

void sortSeries(Series &s)
{
    std::sort(s.begin(), s.end(), [](const Point &a, const Point &b) { return a.date < b.date; });
    Series out;
    for (const Point &p : std::as_const(s)) {
        if (!std::isfinite(p.value) || !p.date.isValid())
            continue;
        if (!out.isEmpty() && out.back().date == p.date)
            out.back() = p;
        else
            out.push_back(p);
    }
    s = out;
}

// One CSV line, with quoted fields.
QList<QByteArray> splitCsv(const QByteArray &line)
{
    QList<QByteArray> out;
    QByteArray cur;
    bool quoted = false;
    for (int i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (quoted) {
            if (c == '"' && i + 1 < line.size() && line[i + 1] == '"') {
                cur += '"';
                ++i;
            } else if (c == '"') {
                quoted = false;
            } else {
                cur += c;
            }
        } else if (c == '"') {
            quoted = true;
        } else if (c == ',') {
            out << cur;
            cur.clear();
        } else if (c != '\r') {
            cur += c;
        }
    }
    out << cur;
    return out;
}

// An SDMX CSV (OECD, BIS): the series keyed by the dimensions between the first column and TIME_PERIOD (OECD's
// "csvfile" starts with DATAFLOW, BIS's "dataonly" with the first dimension).
QHash<QString, Series> parseSdmxCsv(const QByteArray &body)
{
    QHash<QString, Series> out;
    const QList<QByteArray> lines = body.split('\n');
    if (lines.isEmpty())
        return out;
    const QList<QByteArray> head = splitCsv(lines.first().trimmed());
    const int time = head.indexOf("TIME_PERIOD"), value = head.indexOf("OBS_VALUE");
    if (time < 0 || value < 0)
        return out;
    const int first = head.value(0) == "DATAFLOW" ? 1 : 0;
    for (int i = 1; i < lines.size(); ++i) {
        const QList<QByteArray> cells = splitCsv(lines[i].trimmed());
        if (cells.size() <= std::max(time, value))
            continue;
        bool ok = false;
        const double v = cells[value].toDouble(&ok);
        const QDate d = periodStart(QString::fromLatin1(cells[time]));
        if (!ok || !d.isValid())
            continue;
        QStringList key;
        for (int c = first; c < time; ++c)
            key << QString::fromLatin1(cells[c]);
        out[key.join('.')].push_back({d, v});
    }
    for (Series &s : out)
        sortSeries(s);
    return out;
}

// An SDMX-JSON message (IMF): the series keyed by their dimensions after the first (the country).
QHash<QString, Series> parseSdmxJson(const QByteArray &body)
{
    QHash<QString, Series> out;
    const QJsonObject data = QJsonDocument::fromJson(body).object().value("data").toObject();
    const QJsonObject dims = data.value("structures").toArray().at(0).toObject().value("dimensions").toObject();
    const QJsonArray seriesDims = dims.value("series").toArray();
    auto ids = [](const QJsonValue &dim) {
        QStringList list;
        for (const QJsonValue &v : dim.toObject().value("values").toArray())
            list << v.toObject().value("id").toString(v.toObject().value("value").toString());
        return list;
    };
    QVector<QStringList> values;
    for (const QJsonValue &d : seriesDims)
        values << ids(d);
    const QStringList periods = ids(dims.value("observation").toArray().at(0));
    const QJsonObject series = data.value("dataSets").toArray().at(0).toObject().value("series").toObject();
    for (auto it = series.begin(); it != series.end(); ++it) {
        const QStringList idx = it.key().split(':');
        QStringList key;
        for (int i = 1; i < idx.size() && i < values.size(); ++i)
            key << values[i].value(idx[i].toInt());
        Series s;
        const QJsonObject obs = it.value().toObject().value("observations").toObject();
        for (auto o = obs.begin(); o != obs.end(); ++o) {
            bool ok = false;
            const double v = o.value().toArray().at(0).toString().toDouble(&ok);
            const QDate d = periodStart(periods.value(o.key().toInt()));
            if (ok && d.isValid())
                s.push_back({d, v});
        }
        sortSeries(s);
        out.insert(key.join('.'), s);
    }
    return out;
}

// A JSON-stat data set (Eurostat) filtered down to one country and one series: its cells by time (the first
// category of any other dimension left with several).
Series parseJsonStat(const QByteArray &body)
{
    Series s;
    const QJsonObject doc = QJsonDocument::fromJson(body).object();
    const QJsonArray ids = doc.value("id").toArray();
    const QJsonArray sizes = doc.value("size").toArray();
    // The time dimension: "time" (Eurostat), the one JSON-stat 2's role names, or "Tid" (PxWeb).
    const QString timeId = doc.value("role").toObject().value("time").toArray().at(0).toString();
    int timeDim = -1;
    for (int i = 0; i < ids.size(); ++i)
        if (ids[i].toString() == "time" || ids[i].toString() == timeId || ids[i].toString() == "Tid")
            timeDim = i;
    if (timeDim < 0 || sizes.size() != ids.size())
        return s;
    QStringList times(sizes[timeDim].toInt());
    const QJsonValue index = doc.value("dimension").toObject().value(ids[timeDim].toString()).toObject().value("category")
                                 .toObject().value("index");
    if (index.isArray()) {
        const QJsonArray a = index.toArray();
        for (int i = 0; i < a.size() && i < times.size(); ++i)
            times[i] = a[i].toString();
    } else {
        const QJsonObject o = index.toObject();
        for (auto it = o.begin(); it != o.end(); ++it)
            if (const int pos = it.value().toInt(); pos >= 0 && pos < times.size())
                times[pos] = it.key();
    }
    QVector<qint64> stride(ids.size(), 1);
    for (int i = ids.size() - 2; i >= 0; --i)
        stride[i] = stride[i + 1] * sizes[i + 1].toInt();
    // Values: an object by cell number (Eurostat) or an array of every cell, nulls included (JSON-stat 2).
    QVector<std::pair<qint64, double>> cells;
    const QJsonValue raw = doc.value("value");
    if (raw.isArray()) {
        const QJsonArray a = raw.toArray();
        for (int i = 0; i < a.size(); ++i)
            if (a[i].isDouble())
                cells.push_back({i, a[i].toDouble()});
    } else {
        const QJsonObject o = raw.toObject();
        for (auto it = o.begin(); it != o.end(); ++it) {
            bool ok = false;
            const qint64 n = it.key().toLongLong(&ok);
            if (ok && it.value().isDouble())
                cells.push_back({n, it.value().toDouble()});
        }
    }
    for (const auto &[n, value] : cells) {
        bool firstElsewhere = true;
        for (int d = 0; d < ids.size(); ++d)
            if (d != timeDim && (n / stride[d]) % sizes[d].toInt() != 0)
                firstElsewhere = false;
        if (!firstElsewhere)
            continue;
        const int t = int((n / stride[timeDim]) % sizes[timeDim].toInt());
        if (const QDate d = periodStart(times.value(t)); d.isValid())
            s.push_back({d, value});
    }
    sortSeries(s);
    return s;
}

Freq detectFreq(const Series &s)
{
    if (s.size() < 2)
        return Freq::Annual;
    QVector<qint64> gaps;
    for (int i = std::max(1, int(s.size()) - 24); i < s.size(); ++i)
        gaps << s[i - 1].date.daysTo(s[i].date);
    std::sort(gaps.begin(), gaps.end());
    const qint64 g = gaps[gaps.size() / 2];
    if (g <= 4) return Freq::Daily;
    if (g <= 10) return Freq::Weekly;
    if (g <= 45) return Freq::Monthly;
    if (g <= 120) return Freq::Quarterly;
    return Freq::Annual;
}

// The value of the point a year before `i` (exact period for monthly and slower series, within four days for
// weekly and daily ones); NaN when there is none.
double yearBefore(const Series &s, int i, Freq f)
{
    const QDate target = s[i].date.addYears(-1);
    auto it = std::lower_bound(s.begin(), s.begin() + i, target, [](const Point &p, const QDate &d) { return p.date < d; });
    const qint64 tolerance = (f == Freq::Weekly || f == Freq::Daily) ? 4 : 0;
    double best = research::NaN;
    qint64 bestGap = tolerance + 1;
    for (auto c : {it - 1, it}) {
        if (c < s.begin() || c >= s.begin() + i)
            continue;
        const qint64 gap = std::abs(c->date.daysTo(target));
        if (gap < bestGap) {
            bestGap = gap;
            best = c->value;
        }
    }
    return best;
}

// The last day of the period that starts on `date`.
QDate periodEnd(const QDate &date, Freq f)
{
    switch (f) {
    case Freq::Annual: return QDate(date.year(), 12, 31);
    case Freq::Quarterly: return date.addMonths(3).addDays(-1);
    case Freq::Monthly: return date.addMonths(1).addDays(-1);
    default: return date;
    }
}

Series transformed(const Series &s, const QString &t, Freq f)
{
    if (t.isEmpty())
        return s;
    Series out;
    for (int i = 1; i < s.size(); ++i) {
        double v = research::NaN;
        if (t == "pop" && s[i - 1].value != 0)
            v = 100 * (s[i].value / s[i - 1].value - 1);
        else if (t == "diff")
            v = s[i].value - s[i - 1].value;
        else if (t == "yoy") {
            const double before = yearBefore(s, i, f);
            if (std::isfinite(before) && before != 0)
                v = 100 * (s[i].value / before - 1);
        }
        if (std::isfinite(v))
            out.push_back({s[i].date, v});
    }
    return out;
}

// a op b on the dates both have.
enum class Op { Minus, Divide, Plus };
Series combine(const Series &a, const Series &b, Op op)
{
    const bool divide = op == Op::Divide;
    QHash<QDate, double> other;
    for (const Point &p : b)
        other.insert(p.date, p.value);
    Series out;
    for (const Point &p : a) {
        const auto it = other.constFind(p.date);
        if (it == other.constEnd())
            continue;
        if (divide && *it == 0)
            continue;
        out.push_back({p.date, divide ? p.value / *it : op == Op::Plus ? p.value + *it : p.value - *it});
    }
    return out;
}

Series scaled(Series s, double k)
{
    if (k != 1)
        for (Point &p : s)
            p.value *= k;
    return s;
}

QString imfCountry(const QString &iso3)
{
    return iso3 == "XKX" ? QStringLiteral("KOS") : iso3;
}

QString eurostatGeo(const QString &iso3)
{
    if (iso3 == "GRC") return "EL";
    if (iso3 == "XKX") return "XK";
    if (iso3 == "EMU") return "EA21";   // with Bulgaria since January 2026
    if (iso3 == "GBR") return "UK";
    return iso2Of(iso3);
}

} // namespace

// ---------------------------------------------------------------- catalogue

bool Catalogue::covers(const SourceSpec &s, const QString &iso3) const
{
    if (!s.only.isEmpty() && !s.only.contains(iso3))
        return false;
    const QString &p = s.provider;
    if (p == "fred" || p == "eia")
        return iso3 == "USA";
    if (p == "ons" || p == "boe")
        return iso3 == "GBR";
    if (p == "bcb")
        return iso3 == "BRA";
    if (p == "statcan" || p == "boc")
        return iso3 == "CAN";
    if (p == "boj")
        return iso3 == "JPN";
    if (p == "snb")
        return iso3 == "CHE";
    if (p == "abs")
        return iso3 == "AUS";
    if (p == "bbk")
        return iso3 == "DEU";
    if (p == "ecos")
        return iso3 == "KOR";
    if (p == "banxico" || p == "inegi")
        return iso3 == "MEX";
    if (p == "evds")
        return iso3 == "TUR";
    if (p == "bps")
        return iso3 == "IDN";
    if (p == "estat")
        return iso3 == "JPN";
    if (p == "insee")
        return iso3 == "FRA";
    if (p == "ine")
        return iso3 == "ESP";
    if (p == "pxweb")
        return iso3 == (s.flow == "scb" ? QStringLiteral("SWE") : QStringLiteral("NOR"));
    if (p == "eurostat")
        return eurostatGeos.contains(iso3);
    if (p == "oecd")
        return s.flow.contains("DF_KEI") ? oecdAreas.contains(iso3) : iso3 != "EMU";
    if (p == "imf" || p == "weo")
        return iso3 != "EMU" && iso3 != "TWN";
    if (p == "yahoo")
        return s.kind == "stock" ? stockIndexes.contains(iso3) : currencies.contains(iso3);
    if (p == "bis")
        return iso3 == "EMU" || !iso2Of(iso3).isEmpty();
    if (p == "ecb")
        return iso3 == "EMU" || euroMembers.contains(iso3);
    if (p == "ilo")
        return iso3 != "EMU";
    return true;
}

const Catalogue &catalogue()
{
    static const Catalogue cat = [] {
        Catalogue c;
        QFile f(":/country_indicators.json");
        if (!f.open(QIODevice::ReadOnly))
            return c;
        const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
        for (const QJsonValue &g : root.value("groups").toArray())
            c.groups.push_back({g.toObject().value("name").toString(), strings(g.toObject().value("countries"))});
        for (const QString &s : strings(root.value("euroMembers"))) c.euroMembers.insert(s);
        for (const QString &s : strings(root.value("oecdAreas"))) c.oecdAreas.insert(s);
        for (const QString &s : strings(root.value("eurostatGeos"))) c.eurostatGeos.insert(s);
        c.currencies = stringMap(root.value("currencies"));
        c.stockIndexes = stringMap(root.value("stockIndexes"));
        c.extraNames = stringMap(root.value("extraNames"));
        for (const QJsonValue &t : root.value("tabs").toArray()) {
            TabDef tab;
            tab.id = t.toObject().value("id").toString();
            tab.name = t.toObject().value("name").toString();
            for (const QJsonValue &table : t.toObject().value("tables").toArray())
                tab.tables << strings(table);
            c.tabs << tab;
        }
        const QJsonObject inds = root.value("indicators").toObject();
        for (auto it = inds.begin(); it != inds.end(); ++it) {
            const QJsonObject o = it.value().toObject();
            IndicatorDef def;
            def.id = it.key();
            def.name = o.value("name").toString();
            def.unit = o.value("unit").toString();
            for (const QJsonValue &sv : o.value("src").toArray()) {
                const QJsonObject so = sv.toObject();
                SourceSpec s;
                s.provider = so.value("p").toString();
                s.id = so.value("id").toString();
                s.flow = so.value("flow").toString();
                s.key = so.value("key").toString();
                s.minus = so.value("minus").toString();
                s.plus = so.value("plus").toString();
                s.source = so.value("source").toString("2");
                s.classif = so.value("classif").toString();
                s.only = strings(so.value("only"));
                s.since = periodStart(so.value("since").toString());
                s.div = so.value("div").toString();
                s.ds = so.value("ds").toString();
                s.query = so.value("q").toString();
                s.kind = so.value("kind").toString();
                s.transform = so.value("t").toString();
                s.unit = so.value("unit").toString();
                s.note = so.value("note").toString();
                s.scale = so.value("scale").toDouble(1);
                def.sources << s;
                auto addTo = [](QStringList &list, const QString &v) { if (!v.isEmpty() && !list.contains(v)) list << v; };
                if (s.provider == "wb") addTo(c.wbIds[s.source], s.id);
                if (s.provider == "weo") addTo(c.weoIds, s.id);
                if (s.provider == "imf") { addTo(c.imfKeys[s.flow], s.key); addTo(c.imfKeys[s.flow], s.minus); addTo(c.imfKeys[s.flow], s.plus); }
                if (s.provider == "oecd") { addTo(c.oecdKeys[s.flow], s.key); addTo(c.oecdKeys[s.flow], s.minus); }
            }
            c.indicators.insert(def.id, def);
        }
        return c;
    }();
    return cat;
}

QString text(const QString &english)
{
    return english.isEmpty() ? english : QCoreApplication::translate("CountryIndicators", english.toUtf8().constData());
}

QString Row::unitText() const
{
    return text(unit).replace("{CUR}", currency);
}

QString countryName(const QString &iso3)
{
    const QString english = countryNames().value(iso3).toArray().at(1).toString();
    if (!english.isEmpty())
        return QCoreApplication::translate("Country", english.toUtf8().constData());
    const QString extra = catalogue().extraNames.value(iso3);
    return extra.isEmpty() ? iso3 : text(extra);
}

void setKeyIndicators(KeyIndicators *indicators)
{
    g_keyIndicators = indicators;
}

QString periodLabel(const QDate &date, Freq freq)
{
    if (!date.isValid())
        return {};
    const QLocale loc;
    switch (freq) {
    case Freq::Annual: return QString::number(date.year());
    case Freq::Quarterly: return QStringLiteral("Q%1 %2").arg((date.month() - 1) / 3 + 1).arg(date.year());
    case Freq::Monthly: return loc.toString(date, "MMM yyyy");
    default: return loc.toString(date, "d MMM yyyy");
    }
}

QString formatValue(double v, int decimals)
{
    if (!std::isfinite(v))
        return {};
    const QLocale loc;
    const double a = std::abs(v);
    QString s = loc.toString(v, 'f', decimals >= 0 ? decimals : (a >= 100 ? 0 : 2));
    // Trailing zeros of the decimals go: 3.40 -> 3.4, 2.00 -> 2.
    const QChar dot = loc.decimalPoint().at(0);
    if (s.contains(dot)) {
        while (s.endsWith('0'))
            s.chop(1);
        if (s.endsWith(dot))
            s.chop(1);
    }
    return s == "-0" ? QStringLiteral("0") : s;
}

// ---------------------------------------------------------------- loader

CountryLoader::CountryLoader(const QString &iso3, QObject *parent)
    : QObject(parent)
    , m_iso3(iso3)
{
}

void CountryLoader::loadTab(int tab)
{
    const Catalogue &cat = catalogue();
    if (tab < 0 || tab >= cat.tabs.size() || m_loadedTabs.contains(tab))
        return;
    m_loadedTabs.insert(tab);
    for (const QStringList &table : cat.tabs[tab].tables)
        for (const QString &id : table)
            if (!m_rows.contains(id)) {
                m_rows.insert(id, Row());
                ++m_pending;
                resolve(id, 0);
            }
    emit busyChanged();
}

void CountryLoader::refresh()
{
    for (const char *group : {"fred", "worldbank", "imf", "oecd", "eurostat", "bis", "eia", "yahoo", "ilo", "ecb", "ons", "boe", "bcb", "statcan", "boc", "boj", "snb", "abs", "bbk", "insee", "ine", "pxweb", "ecos", "banxico", "inegi", "evds", "bps", "estat"})
        research::Client::instance()->forget(group);
    ++m_generation;
    m_bulk.clear();
    m_bulkErrors.clear();
    m_bulkWaiting.clear();
    m_stale.clear();
    m_waitedSurveys.clear();
    const QSet<int> tabs = m_loadedTabs;
    const QStringList ids = m_rows.keys();
    m_rows.clear();
    m_loadedTabs.clear();
    m_pending = 0;
    for (const QString &id : ids)
        emit rowChanged(id);
    for (int t : tabs)
        loadTab(t);
}

void CountryLoader::settle(const QString &id, const Row &row)
{
    m_rows[id] = row;
    m_pending = std::max(0, m_pending - 1);
    emit rowChanged(id);
    emit busyChanged();
}

void CountryLoader::resolve(const QString &id, int from)
{
    const Catalogue &cat = catalogue();
    const IndicatorDef def = cat.indicators.value(id);
    int i = from;
    while (i < def.sources.size() && !cat.covers(def.sources[i], m_iso3))
        ++i;
    if (i >= def.sources.size()) {
        // Nothing fresher came up: the stale series kept on the way, if any.
        if (m_stale.contains(id)) {
            settle(id, m_stale.take(id));
            return;
        }
        Row r = m_rows.value(id);
        r.state = Row::Missing;
        settle(id, r);
        return;
    }
    const SourceSpec s = def.sources[i];

    if (s.provider == "survey") {
        static const QHash<QString, KeyIndicators::Id> ids {
            {"ManufacturingPmi", KeyIndicators::ManufacturingPmi}, {"ServicesPmi", KeyIndicators::ServicesPmi},
            {"CompositePmi", KeyIndicators::CompositePmi}};
        KeyIndicators::Figure fig;
        if (g_keyIndicators && ids.contains(s.id))
            fig = g_keyIndicators->figure(m_iso3, ids.value(s.id));
        // The calendar may still be downloading when the tab opens: wait for it (half a minute at most).
        SurveyHeadlines *surveys = g_keyIndicators ? g_keyIndicators->sets().surveys : nullptr;
        if (!fig.valid() && surveys && !surveys->hasData() && !m_waitedSurveys.contains(id)) {
            m_waitedSurveys.insert(id);
            auto done = std::make_shared<bool>(false);
            const int generation = m_generation;
            QPointer<CountryLoader> self(this);
            auto retry = [self, done, generation, id, i] {
                if (*done || !self || self->m_generation != generation)
                    return;
                *done = true;
                self->resolve(id, i);
            };
            connect(surveys, &RemoteDataSet::updated, this, retry, Qt::SingleShotConnection);
            QTimer::singleShot(30000, this, retry);
            return;
        }
        if (!fig.valid()) {
            resolve(id, i + 1);
            return;
        }
        Row r;
        r.state = Row::Found;
        r.last = fig.value;
        r.previous = fig.previous;
        const int year = int(std::floor(fig.time));
        const int month = std::clamp(int((fig.time - year) * 12) + 1, 1, 12);
        r.freq = fig.quarterly ? Freq::Quarterly : Freq::Monthly;
        r.date = QDate(year, fig.quarterly ? ((month - 1) / 3) * 3 + 1 : month, 1);
        r.unit = s.unit.isEmpty() ? def.unit : s.unit;
        r.source = fig.via;
        r.latestOnly = true;
        r.note = s.note;
        settle(id, r);
        return;
    }

    const int generation = m_generation;
    QPointer<CountryLoader> self(this);
    fetch(s, [self, generation, id, i, s, def](const Series &raw, const QString &error) {
        if (!self || self->m_generation != generation)
            return;
        Series series = scaled(raw, s.scale);
        if (s.since.isValid())
            series.erase(std::remove_if(series.begin(), series.end(), [&](const Point &p) { return p.date < s.since; }),
                         series.end());
        const Freq f = detectFreq(series);
        series = transformed(series, s.transform, f);
        if (series.isEmpty()) {
            // Keep the reason of the first failure worth telling (a missing key), then try the next source.
            Row &r = self->m_rows[id];
            static const QHash<QString, QString> keyNames {
                {"fred", "FRED"}, {"eia", "EIA"}, {"ecos", "ECOS"}, {"banxico", "Banxico"}, {"inegi", "INEGI"}, {"evds", "EVDS"},
                {"bps", "BPS"}, {"estat", "e-Stat"}};
            const bool keyless = keyNames.contains(s.provider) && sourceKey(s.provider).isEmpty();
            if (keyless) {
                r.needsKey = true;
                r.error = tr("A free %1 key is needed (Sources)").arg(keyNames.value(s.provider));
            } else if (r.error.isEmpty() && !error.isEmpty()) {
                r.error = error;
            }
            self->resolve(id, i + 1);
            return;
        }
        Row r;
        r.state = Row::Found;
        r.last = series.back().value;
        r.previous = series.size() > 1 ? series[series.size() - 2].value : research::NaN;
        r.highest = r.lowest = series.front().value;
        for (const Point &p : std::as_const(series)) {
            r.highest = std::max(r.highest, p.value);
            r.lowest = std::min(r.lowest, p.value);
        }
        // Growth rates computed here keep one decimal, as published ones do.
        if (s.transform == "yoy" || s.transform == "pop")
            for (double *v : {&r.last, &r.previous, &r.highest, &r.lowest})
                if (std::isfinite(*v))
                    *v = std::round(*v * 10) / 10;
        r.date = series.back().date;
        r.freq = detectFreq(series);
        r.unit = s.unit.isEmpty() ? def.unit : s.unit;
        r.currency = catalogue().currencies.value(self->m_iso3);
        if (r.currency == "DXY" || r.currency.isEmpty())
            r.currency = r.currency.isEmpty() ? QStringLiteral("LCU") : QStringLiteral("USD");
        r.note = s.note;
        static const QHash<QString, const char *> labels {
            {"fred", "FRED"}, {"wb", QT_TRANSLATE_NOOP("countries::CountryLoader", "World Bank")},
            {"weo", QT_TRANSLATE_NOOP("countries::CountryLoader", "IMF WEO")},
            {"imf", QT_TRANSLATE_NOOP("countries::CountryLoader", "IMF")}, {"oecd", QT_TRANSLATE_NOOP("countries::CountryLoader", "OECD")},
            {"eurostat", "Eurostat"}, {"bis", QT_TRANSLATE_NOOP("countries::CountryLoader", "BIS")},
            {"yahoo", "Yahoo Finance"}, {"eia", "EIA"}, {"ilo", QT_TRANSLATE_NOOP("countries::CountryLoader", "ILO")},
            {"ecb", QT_TRANSLATE_NOOP("countries::CountryLoader", "ECB")}, {"ons", "ONS"}, {"boe", QT_TRANSLATE_NOOP("countries::CountryLoader", "Bank of England")},
            {"bcb", QT_TRANSLATE_NOOP("countries::CountryLoader", "Central Bank of Brazil")}, {"statcan", "Statistics Canada"},
            {"boc", QT_TRANSLATE_NOOP("countries::CountryLoader", "Bank of Canada")},
            {"boj", QT_TRANSLATE_NOOP("countries::CountryLoader", "Bank of Japan")},
            {"snb", QT_TRANSLATE_NOOP("countries::CountryLoader", "Swiss National Bank")}, {"abs", "ABS"},
            {"bbk", "Bundesbank"}, {"insee", "INSEE"}, {"ine", "INE"}, {"pxweb", ""},
            {"ecos", QT_TRANSLATE_NOOP("countries::CountryLoader", "Bank of Korea")}, {"banxico", "Banxico"}, {"inegi", "INEGI"},
            {"evds", QT_TRANSLATE_NOOP("countries::CountryLoader", "Central Bank of Türkiye")}, {"bps", "BPS"}, {"estat", "e-Stat"}};
        r.source = s.provider == "pxweb" ? (s.flow == "scb" ? QStringLiteral("Statistics Sweden") : QStringLiteral("Statistics Norway"))
                                         : tr(labels.value(s.provider, ""));
        if (r.source.isEmpty())
            r.source = s.provider;
        r.code = !s.id.isEmpty() ? s.id : !s.key.isEmpty() ? self->placeholders(s.key) : s.ds;
        if (s.provider == "yahoo") {
            const QString cur = catalogue().currencies.value(self->m_iso3);
            if (s.kind == "currency" && cur != "DXY")
                r.unit = (cur == "EUR" || cur == "GBP" || cur == "AUD" || cur == "NZD") ? cur + "/USD" : "USD/" + cur;
            if (s.kind == "currency" && cur == "DXY")
                r.unit = "DXY";
        }
        // A series that stopped more than a year ago gives way to a later source that goes on (a compilation
        // that lags, an office that stopped reporting to it): kept aside while the next sources are tried.
        if (periodEnd(r.date, r.freq) < QDate::currentDate().addDays(-400)) {
            const auto stale = self->m_stale.constFind(id);
            if (stale == self->m_stale.constEnd() || periodEnd(stale->date, stale->freq) < periodEnd(r.date, r.freq))
                self->m_stale.insert(id, r);
            self->resolve(id, i + 1);
            return;
        }
        self->m_stale.remove(id);
        self->settle(id, r);
    });
}

QString CountryLoader::placeholders(QString key) const
{
    const bool euro = m_iso3 == "EMU" || catalogue().euroMembers.contains(m_iso3);
    const QString iso2 = m_iso3 == "EMU" ? QStringLiteral("XM") : iso2Of(m_iso3);
    key.replace("{ISO3}", m_iso3 == "EMU" ? QStringLiteral("EA20") : m_iso3);
    key.replace("{ISO2}", iso2);
    key.replace("{BIS}", euro ? QStringLiteral("XM") : iso2);
    key.replace("{ECB}", m_iso3 == "EMU" ? QStringLiteral("U2") : iso2);
    return key;
}

void CountryLoader::bulk(const QString &key, const QUrl &url, qint64 maxAge, bool json,
                         std::function<Bulk(const QByteArray &)> parse, std::function<void(const Bulk &, const QString &)> done)
{
    if (m_bulk.contains(key) || m_bulkErrors.contains(key)) {
        done(m_bulk.value(key), m_bulkErrors.value(key));
        return;
    }
    const bool already = m_bulkWaiting.contains(key);
    m_bulkWaiting[key].push_back(done);
    if (already)
        return;
    const int generation = m_generation;
    QPointer<CountryLoader> self(this);
    research::Client::instance()->raw(key, url, maxAge, json, this,
        [self, generation, key, parse](const QByteArray &body, const QString &err) {
            if (!self || self->m_generation != generation)
                return;
            const Bulk parsed = body.isEmpty() ? Bulk() : parse(body);
            const QString error = parsed.isEmpty() ? (err.isEmpty() ? tr("No data") : err) : QString();
            if (parsed.isEmpty())
                self->m_bulkErrors.insert(key, error);
            else
                self->m_bulk.insert(key, parsed);
            const auto waiting = self->m_bulkWaiting.take(key);
            for (const auto &w : waiting)
                w(parsed, error);
        });
}

void CountryLoader::fetch(const SourceSpec &s, SeriesDone done)
{
    const Catalogue &cat = catalogue();
    research::Client *client = research::Client::instance();
    const QString year = QString::number(QDate::currentDate().year());

    // Picks `key` (and `minus` / `div`) out of a bulk answer.
    auto pick = [s, done](const QString &key, const QString &minus, const QString &div) {
        const QString plus = s.plus;
        return [key, minus, plus, div, done](const Bulk &b, const QString &err) {
            Series a = b.value(key);
            if (!a.isEmpty() && !minus.isEmpty())
                a = combine(a, b.value(minus), Op::Minus);
            if (!a.isEmpty() && !plus.isEmpty())
                a = combine(a, b.value(plus), Op::Plus);
            if (!a.isEmpty() && !div.isEmpty())
                a = combine(a, b.value(div), Op::Divide);
            done(a, a.isEmpty() ? (err.isEmpty() ? tr("Not published") : err) : QString());
        };
    };

    if (s.provider == "fred") {
        if (s.div.isEmpty()) {
            client->fredSeries(s.id, QDate(), this, done);
            return;
        }
        QPointer<CountryLoader> self(this);
        client->fredSeries(s.id, QDate(), this, [self, s, done](const Series &a, const QString &err) {
            if (!self || a.isEmpty()) {
                done(a, err);
                return;
            }
            research::Client::instance()->fredSeries(s.div, QDate(), self, [a, done](const Series &b, const QString &err2) {
                done(combine(a, b, Op::Divide), err2);
            });
        });
        return;
    }

    if (s.provider == "wb") {
        // One request per database: WDI (annual) or the Global Economic Monitor (monthly).
        const bool monthly = s.source == "15";
        QUrl url(QString("https://api.worldbank.org/v2/country/%1/indicator/%2").arg(m_iso3, cat.wbIds.value(s.source).join(';')));
        url.setQuery(monthly ? QString("source=15&format=json&per_page=20000&frequency=M&date=1960M01:%1M12").arg(year)
                             : QString("source=%1&format=json&per_page=20000&date=1960:%2").arg(s.source, year));
        bulk("worldbank:" + s.source + ":" + m_iso3, url, (monthly ? 12 : 24) * Hour, false, [](const QByteArray &body) {
            Bulk out;
            for (const QJsonValue &v : QJsonDocument::fromJson(body).array().at(1).toArray()) {
                const QJsonObject o = v.toObject();
                if (!o.value("value").isDouble())
                    continue;
                out[o.value("indicator").toObject().value("id").toString()].push_back(
                    {periodStart(o.value("date").toString()), o.value("value").toDouble()});
            }
            for (Series &x : out)
                sortSeries(x);
            return out;
        }, pick(s.id, {}, {}));
        return;
    }

    if (s.provider == "weo") {
        const QUrl url(QString("https://api.imf.org/external/sdmx/3.0/data/dataflow/IMF.RES/WEO/+/%1.%2.A?attributes=none&measures=all")
                           .arg(imfCountry(m_iso3), cat.weoIds.join('+')));
        const int thisYear = QDate::currentDate().year();
        bulk("imf:weo:" + m_iso3, url, 24 * Hour, true, [thisYear](const QByteArray &body) {
            // The current year is an estimate and later years projections: reported years only, keyed by the
            // indicator ("GGXWDG_NGDP.A" -> "GGXWDG_NGDP").
            const Bulk all = parseSdmxJson(body);
            Bulk out;
            for (auto it = all.begin(); it != all.end(); ++it) {
                Series kept;
                for (const Point &p : it.value())
                    if (p.date.year() < thisYear)
                        kept.push_back(p);
                if (!kept.isEmpty())
                    out.insert(it.key().section('.', 0, 0), kept);
            }
            return out;
        }, pick(s.id, {}, {}));
        return;
    }

    if (s.provider == "imf") {
        QVector<QStringList> parts;
        for (const QString &k : cat.imfKeys.value(s.flow))
            addKeyParts(parts, k);
        const QUrl url(QString("https://api.imf.org/external/sdmx/3.0/data/dataflow/IMF.STA/%1/+/%2.%3?attributes=none&measures=all")
                           .arg(s.flow, imfCountry(m_iso3), joinKeyParts(parts)));
        bulk("imf:" + s.flow + ":" + m_iso3, url, 12 * Hour, true, parseSdmxJson, pick(s.key, s.minus, s.div));
        return;
    }

    if (s.provider == "oecd") {
        QVector<QStringList> parts;
        for (const QString &k : cat.oecdKeys.value(s.flow))
            addKeyParts(parts, placeholders(k));
        QUrl url("https://sdmx.oecd.org/public/rest/data/" + s.flow + "/" + joinKeyParts(parts));
        url.setQuery("dimensionAtObservation=AllDimensions&format=csvfile");
        bulk("oecd:" + s.flow + ":" + m_iso3, url, 12 * Hour, false, parseSdmxCsv,
             pick(placeholders(s.key), s.minus.isEmpty() ? QString() : placeholders(s.minus), {}));
        return;
    }

    if (s.provider == "eurostat") {
        const QString geo = eurostatGeo(m_iso3);
        QUrl url("https://ec.europa.eu/eurostat/api/dissemination/statistics/1.0/data/" + s.ds);
        url.setQuery(s.query + "&geo=" + geo + "&format=JSON&lang=EN");
        client->raw("eurostat:" + s.ds + "?" + s.query + "&geo=" + geo, url, 12 * Hour, false, this,
                    [done](const QByteArray &body, const QString &err) {
                        const Series x = parseJsonStat(body);
                        done(x, x.isEmpty() ? (err.isEmpty() ? tr("Not published") : err) : QString());
                    });
        return;
    }

    if (s.provider == "bis") {
        const QString key = placeholders(s.key);
        QUrl url("https://stats.bis.org/api/v2/data/dataflow/BIS/" + s.flow + "/" + key);
        url.setQuery("format=csv&detail=dataonly");
        client->raw("bis:" + s.flow + ":" + key, url, 12 * Hour, false, this, [done](const QByteArray &body, const QString &err) {
            const QHash<QString, Series> all = parseSdmxCsv(body);
            const Series x = all.isEmpty() ? Series() : all.begin().value();
            done(x, x.isEmpty() ? (err.isEmpty() ? tr("Not published") : err) : QString());
        });
        return;
    }

    if (s.provider == "ecb") {
        const QString key = placeholders(s.key);
        QUrl url("https://data-api.ecb.europa.eu/service/data/" + s.flow + "/" + key);
        url.setQuery("format=csvdata&detail=dataonly");
        client->raw("ecb:" + s.flow + ":" + key, url, 12 * Hour, false, this, [done](const QByteArray &body, const QString &err) {
            const QHash<QString, Series> all = parseSdmxCsv(body);
            const Series x = all.isEmpty() ? Series() : all.begin().value();
            done(x, x.isEmpty() ? (err.isEmpty() ? tr("Not published") : err) : QString());
        });
        return;
    }

    // Sources with a user's free key (Sources › Economy · countries): none is asked for without it.
    if (s.provider == "ecos" || s.provider == "banxico" || s.provider == "inegi" || s.provider == "evds" || s.provider == "bps" || s.provider == "estat") {
        const QString key = sourceKey(s.provider);
        if (key.isEmpty()) {
            done({}, tr("No key"));
            return;
        }
    }

    if (s.provider == "ecos") {
        // The Bank of Korea's ECOS: `id` "STAT/CYCLE/ITEM1[/ITEM2]"; periods 202609, 2026Q3, 2026, 20261030 (daily
        // series ten years back). {"StatisticSearch": {"row": [{"TIME": …, "DATA_VALUE": …}]}}.
        const QStringList parts = s.id.split('/');
        const QString cycle = parts.value(1);
        const QDate today = QDate::currentDate();
        QString start, end;
        if (cycle == "D") { start = today.addYears(-10).toString("yyyyMMdd"); end = today.toString("yyyyMMdd"); }
        else if (cycle == "Q") { start = "1960Q1"; end = QString("%1Q4").arg(today.year()); }
        else if (cycle == "A") { start = "1960"; end = QString::number(today.year()); }
        else { start = "196001"; end = today.toString("yyyy") + "12"; }
        QStringList path {parts.value(0), cycle, start, end};
        path += parts.mid(2);
        const QUrl url("https://ecos.bok.or.kr/api/StatisticSearch/" + sourceKey("ecos") + "/json/en/1/100000/" + path.join('/'));
        client->raw("ecos:" + s.id, url, 12 * Hour, true, this, [done](const QByteArray &body, const QString &err) {
            Series x;
            for (const QJsonValue &v : QJsonDocument::fromJson(body).object().value("StatisticSearch").toObject().value("row").toArray()) {
                const QJsonObject o = v.toObject();
                bool ok = false;
                const double value = o.value("DATA_VALUE").toString().toDouble(&ok);
                const QString t = o.value("TIME").toString();
                const QDate d = t.size() == 8 ? QDate::fromString(t, "yyyyMMdd") : periodStart(t);
                if (ok && d.isValid())
                    x.push_back({d, value});
            }
            sortSeries(x);
            done(x, x.isEmpty() ? (err.isEmpty() ? tr("Not published") : err) : QString());
        });
        return;
    }

    if (s.provider == "banxico") {
        // Banco de México's SIE: every observation of a series, {"bmx": {"series": [{"datos": [{"fecha": "30/09/2026",
        // "dato": "1,234.5"}]}]}}; "N/E" where there is none.
        QUrl url("https://www.banxico.org.mx/SieAPIRest/service/v1/series/" + s.id + "/datos");
        url.setQuery("token=" + sourceKey("banxico"));
        client->raw("banxico:" + s.id, url, 12 * Hour, true, this, [done](const QByteArray &body, const QString &err) {
            Series x;
            const QJsonArray series = QJsonDocument::fromJson(body).object().value("bmx").toObject().value("series").toArray();
            for (const QJsonValue &v : series.at(0).toObject().value("datos").toArray()) {
                const QJsonObject o = v.toObject();
                bool ok = false;
                const double value = o.value("dato").toString().remove(',').toDouble(&ok);
                const QDate d = QDate::fromString(o.value("fecha").toString(), "dd/MM/yyyy");
                if (ok && d.isValid())
                    x.push_back({d, value});
            }
            sortSeries(x);
            done(x, x.isEmpty() ? (err.isEmpty() ? tr("Not published") : err) : QString());
        });
        return;
    }

    if (s.provider == "inegi") {
        // INEGI's indicators API (BIE): {"Series": [{"OBSERVATIONS": [{"TIME_PERIOD": "2026/09", "OBS_VALUE": "…"}]}]};
        // "2026/03" is a month for a monthly series, a quarter for a quarterly one ("kind": "quarterly").
        const QUrl url(QString("https://www.inegi.org.mx/app/api/indicadores/desarrolladores/jsonxml/INDICATOR/%1/en/0700/false/BIE/2.0/%2?type=json")
                           .arg(s.id, sourceKey("inegi")));
        const bool quarterly = s.kind == "quarterly";
        client->raw("inegi:" + s.id, url, 12 * Hour, true, this, [done, quarterly](const QByteArray &body, const QString &err) {
            Series x;
            const QJsonArray series = QJsonDocument::fromJson(body).object().value("Series").toArray();
            for (const QJsonValue &v : series.at(0).toObject().value("OBSERVATIONS").toArray()) {
                const QJsonObject o = v.toObject();
                bool ok = false;
                const double value = o.value("OBS_VALUE").toString().toDouble(&ok);
                const QStringList t = o.value("TIME_PERIOD").toString().split('/');
                const int year = t.value(0).toInt(), sub = t.value(1).toInt();
                const QDate d = t.size() < 2 ? QDate(year, 1, 1) : QDate(year, quarterly ? (sub - 1) * 3 + 1 : sub, 1);
                if (ok && d.isValid())
                    x.push_back({d, value});
            }
            sortSeries(x);
            done(x, x.isEmpty() ? (err.isEmpty() ? tr("Not published") : err) : QString());
        });
        return;
    }

    if (s.provider == "evds") {
        // The Turkish central bank's EVDS: the key goes in a "key" header; {"items": [{"Tarih": "2026-9" (or
        // "30-10-2026", "2026-Q3"), "TP_FG_J0": "…"}]}, the series code with '_' for '.'.
        QUrl url(QString("https://evds2.tcmb.gov.tr/service/evds/series=%1&startDate=01-01-1950&endDate=%2&type=json")
                     .arg(s.id, QDate::currentDate().toString("dd-MM-yyyy")));
        const QString field = QString(s.id).replace('.', '_');
        client->raw("evds:" + s.id, url, 12 * Hour, true, {{"key", sourceKey("evds").toUtf8()}}, this,
                    [done, field](const QByteArray &body, const QString &err) {
            Series x;
            for (const QJsonValue &v : QJsonDocument::fromJson(body).object().value("items").toArray()) {
                const QJsonObject o = v.toObject();
                const QJsonValue raw = o.value(field);
                bool ok = raw.isDouble();
                const double value = ok ? raw.toDouble() : raw.toString().toDouble(&ok);
                const QString t = o.value("Tarih").toString();
                QDate d;
                if (t.count('-') == 2) d = QDate::fromString(t, "d-M-yyyy");
                else if (t.contains("-Q")) d = periodStart(t);
                else if (const QStringList ym = t.split('-'); ym.size() == 2) d = QDate(ym[0].toInt(), ym[1].toInt(), 1);
                else d = periodStart(t);
                if (ok && d.isValid())
                    x.push_back({d, value});
            }
            sortSeries(x);
            done(x, x.isEmpty() ? (err.isEmpty() ? tr("Not published") : err) : QString());
        });
        return;
    }

    if (s.provider == "estat") {
        // e-Stat (Japan's government statistics): a table (`id`, statsDataId) narrowed by `q` ("cdTab=3&cdCat01=0161&
        // cdArea=00000"); {"GET_STATS_DATA": {"STATISTICAL_DATA": {"DATA_INF": {"VALUE": [{"@time": "2026001010",
        // "$": "2.9"}]}}}}. Times are YYYY00MMMM for months, YYYY000000 for years (left out of a monthly series).
        QUrl url("https://api.e-stat.go.jp/rest/3.0/app/json/getStatsData");
        url.setQuery("appId=" + sourceKey("estat") + "&statsDataId=" + s.id + "&" + s.query + "&metaGetFlg=N&cntGetFlg=N&limit=100000");
        client->raw("estat:" + s.id + ":" + s.query, url, 12 * Hour, true, this, [done](const QByteArray &body, const QString &err) {
            Series monthly, annual;
            const QJsonValue values = QJsonDocument::fromJson(body).object().value("GET_STATS_DATA").toObject()
                                          .value("STATISTICAL_DATA").toObject().value("DATA_INF").toObject().value("VALUE");
            const QJsonArray list = values.isArray() ? values.toArray() : QJsonArray {values};
            for (const QJsonValue &v : list) {
                const QJsonObject o = v.toObject();
                bool ok = false;
                const double value = o.value("$").toString().toDouble(&ok);
                const QString t = o.value("@time").toString();
                if (!ok || t.size() < 10)
                    continue;
                const int year = t.left(4).toInt(), month = t.mid(8, 2).toInt();
                if (month >= 1 && month <= 12)
                    monthly.push_back({QDate(year, month, 1), value});
                else
                    annual.push_back({QDate(year, 1, 1), value});
            }
            Series x = monthly.isEmpty() ? annual : monthly;
            sortSeries(x);
            done(x, x.isEmpty() ? (err.isEmpty() ? tr("Not published") : err) : QString());
        });
        return;
    }

    if (s.provider == "bps") {
        // Statistics Indonesia's web API: three years at most per request, so four requests cover 2015 on. `id` is
        // the variable, `key` "vervar/turvar" (9999/0: Indonesia, no sub-variable). A cell's key is
        // vervar + var + turvar + year id (year - 1900) + period id; the period's label names its month ("Annual"
        // and the like are skipped).
        const int thisYear = QDate::currentDate().year();
        const QString vervar = s.key.section('/', 0, 0), turvar = s.key.section('/', 1, 1);
        struct Gather {
            int left = 0;
            Series all;
            QString error;
        };
        auto gather = std::make_shared<Gather>();
        static const QStringList months {"january", "february", "march", "april", "may", "june", "july", "august",
                                         "september", "october", "november", "december"};
        const QString var = s.id;
        for (int from = thisYear - 11; from <= thisYear; from += 3) {
            const int a = from - 1900, b = std::min(from + 2, thisYear) - 1900;
            const QUrl url(QString("https://webapi.bps.go.id/v1/api/list/model/data/lang/eng/domain/0000/var/%1/th/%2:%3/key/%4/")
                               .arg(var).arg(a).arg(b).arg(sourceKey("bps")));
            ++gather->left;
            client->raw(QString("bps:%1:%2:%3:%4").arg(var, s.key).arg(a).arg(b), url, 12 * Hour, true, this,
                        [done, gather, vervar, turvar, var](const QByteArray &body, const QString &err) {
                const QJsonObject o = QJsonDocument::fromJson(body).object();
                const QJsonObject cells = o.value("datacontent").toObject();
                for (const QJsonValue &y : o.value("tahun").toArray()) {
                    const QString yearId = QString::number(y.toObject().value("val").toInt());
                    const int year = y.toObject().value("label").toString().toInt();
                    for (const QJsonValue &t : o.value("turtahun").toArray()) {
                        const QString label = t.toObject().value("label").toString().toLower();
                        const int m = months.indexOf(label.section(' ', 0, 0));
                        if (m < 0)
                            continue;
                        const QString cell = vervar + var + turvar + yearId + QString::number(t.toObject().value("val").toInt());
                        const QJsonValue v = cells.value(cell);
                        if (v.isDouble())
                            gather->all.push_back({QDate(year, m + 1, 1), v.toDouble()});
                    }
                }
                if (!err.isEmpty())
                    gather->error = err;
                if (--gather->left == 0) {
                    sortSeries(gather->all);
                    done(gather->all, gather->all.isEmpty() ? (gather->error.isEmpty() ? tr("Not published") : gather->error) : QString());
                }
            });
        }
        return;
    }

    if (s.provider == "bbk") {
        // The Bundesbank's SDMX API (it mirrors much of the Federal Statistical Office's figures), one series.
        QUrl url("https://api.statistiken.bundesbank.de/rest/data/" + s.flow + "/" + s.key);
        url.setQuery("format=sdmx_csv&lang=en");
        client->raw("bbk:" + s.flow + ":" + s.key, url, 12 * Hour, false, this, [done](const QByteArray &body, const QString &err) {
            const QHash<QString, Series> all = parseSdmxCsv(body);
            const Series x = all.isEmpty() ? Series() : all.begin().value();
            done(x, x.isEmpty() ? (err.isEmpty() ? tr("Not published") : err) : QString());
        });
        return;
    }

    if (s.provider == "insee") {
        // INSEE's BDM, SDMX-ML: <Obs TIME_PERIOD="2026-10" OBS_VALUE="97.5"/> for an idbank.
        const QUrl url("https://bdm.insee.fr/series/sdmx/data/SERIES_BDM/" + s.id);
        client->raw("insee:" + s.id, url, 12 * Hour, false, this, [done](const QByteArray &body, const QString &err) {
            Series x;
            static const QRegularExpression obs(R"x(<Obs\b([^>]*)/?>)x");
            static const QRegularExpression period(R"x(TIME_PERIOD="([^"]+)")x"), value(R"x(OBS_VALUE="([^"]+)")x");
            for (auto it = obs.globalMatch(QString::fromUtf8(body)); it.hasNext();) {
                const QString attrs = it.next().captured(1);
                bool ok = false;
                const double v = value.match(attrs).captured(1).toDouble(&ok);
                const QDate d = periodStart(period.match(attrs).captured(1));
                if (ok && d.isValid())
                    x.push_back({d, v});
            }
            sortSeries(x);
            done(x, x.isEmpty() ? (err.isEmpty() ? tr("Not published") : err) : QString());
        });
        return;
    }

    if (s.provider == "ine") {
        // Spain's INE (Tempus 3): {"Data": [{"Fecha": ms since 1970, "Valor": 12256087.0}]} for a series code.
        QUrl url("https://servicios.ine.es/wstempus/js/EN/DATOS_SERIE/" + s.id);
        url.setQuery("nult=2000");
        client->raw("ine:" + s.id, url, 12 * Hour, true, this, [done](const QByteArray &body, const QString &err) {
            Series x;
            for (const QJsonValue &v : QJsonDocument::fromJson(body).object().value("Data").toArray()) {
                const QJsonObject o = v.toObject();
                if (!o.value("Valor").isDouble())
                    continue;
                const QDate d = QDateTime::fromMSecsSinceEpoch(qint64(o.value("Fecha").toDouble()), QTimeZone::UTC)
                                    .addSecs(12 * 3600).date();
                if (d.isValid())
                    x.push_back({QDate(d.year(), d.month(), 1), o.value("Valor").toDouble()});
            }
            sortSeries(x);
            done(x, x.isEmpty() ? (err.isEmpty() ? tr("Not published") : err) : QString());
        });
        return;
    }

    if (s.provider == "pxweb") {
        // PxWeb API 2 (Statistics Norway "ssb", Statistics Sweden "scb"): a table (`id`), the values of its other
        // dimensions (`q`: "Konsumgrp=JAE_TOTAL&ContentsCode=Tolvmanedersendring") and every period, as JSON-stat 2.
        const QString base = s.flow == "scb" ? QStringLiteral("https://api.scb.se/OV0104/v2beta/api/v2/tables/")
                                             : QStringLiteral("https://data.ssb.no/api/pxwebapi/v2-beta/tables/");
        QUrlQuery q;
        q.addQueryItem("lang", "en");
        q.addQueryItem("outputFormat", "json-stat2");
        for (const QString &pair : s.query.split('&', Qt::SkipEmptyParts))
            q.addQueryItem("valuecodes[" + pair.section('=', 0, 0) + "]", pair.section('=', 1));
        q.addQueryItem("valuecodes[Tid]", "*");
        QUrl url(base + s.id + "/data");
        url.setQuery(q);
        client->raw("pxweb:" + s.flow + ":" + s.id + ":" + s.query, url, 12 * Hour, true, this,
                    [done](const QByteArray &body, const QString &err) {
                        const Series x = parseJsonStat(body);
                        done(x, x.isEmpty() ? (err.isEmpty() ? tr("Not published") : err) : QString());
                    });
        return;
    }

    if (s.provider == "abs") {
        // The Australian Bureau of Statistics' SDMX API, as the OECD's: flow ("LF", "CPI,2.0.0") and series key.
        QUrl url("https://data.api.abs.gov.au/rest/data/ABS," + s.flow + "/" + s.key);
        url.setQuery("format=csvfile");
        client->raw("abs:" + s.flow + ":" + s.key, url, 12 * Hour, false, this, [done](const QByteArray &body, const QString &err) {
            const QHash<QString, Series> all = parseSdmxCsv(body);
            const Series x = all.isEmpty() ? Series() : all.begin().value();
            done(x, x.isEmpty() ? (err.isEmpty() ? tr("Not published") : err) : QString());
        });
        return;
    }

    if (s.provider == "bcb") {
        // Banco Central do Brasil's SGS: [{"data": "01/09/2026", "valor": "0.82"}]. Daily series ("kind": "daily")
        // are served ten years at most per request.
        QUrl url("https://api.bcb.gov.br/dados/serie/bcdata.sgs." + s.id + "/dados");
        const QDate today = QDate::currentDate();
        url.setQuery(s.kind == "daily" ? QString("formato=json&dataInicial=%1&dataFinal=%2")
                                             .arg(today.addYears(-10).addDays(1).toString("dd/MM/yyyy"), today.toString("dd/MM/yyyy"))
                                       : QStringLiteral("formato=json"));
        client->raw("bcb:" + s.id, url, 12 * Hour, true, this, [done](const QByteArray &body, const QString &err) {
            Series x;
            for (const QJsonValue &v : QJsonDocument::fromJson(body).array()) {
                bool ok = false;
                const double value = v.toObject().value("valor").toString().toDouble(&ok);
                const QDate d = QDate::fromString(v.toObject().value("data").toString(), "dd/MM/yyyy");
                if (ok && d.isValid())
                    x.push_back({d, value});
            }
            sortSeries(x);
            done(x, x.isEmpty() ? (err.isEmpty() ? tr("Not published") : err) : QString());
        });
        return;
    }

    if (s.provider == "statcan") {
        // Statistics Canada's Web Data Service, one vector over its whole history.
        QUrl url("https://www150.statcan.gc.ca/t1/wds/rest/getDataFromVectorByReferencePeriodRange");
        url.setQuery(QString("vectorIds=%1&startRefPeriod=1900-01-01&endReferencePeriod=%2")
                         .arg(s.id, QDate::currentDate().addYears(1).toString(Qt::ISODate)));
        client->raw("statcan:" + s.id, url, 12 * Hour, true, this, [done](const QByteArray &body, const QString &err) {
            Series x;
            const QJsonArray points = QJsonDocument::fromJson(body).array().at(0).toObject().value("object").toObject()
                                          .value("vectorDataPoint").toArray();
            for (const QJsonValue &v : points) {
                const QJsonObject o = v.toObject();
                const QDate d = QDate::fromString(o.value("refPer").toString(), Qt::ISODate);
                if (o.value("value").isDouble() && d.isValid())
                    x.push_back({d, o.value("value").toDouble()});
            }
            sortSeries(x);
            done(x, x.isEmpty() ? (err.isEmpty() ? tr("Not published") : err) : QString());
        });
        return;
    }

    if (s.provider == "boc") {
        // The Bank of Canada's Valet API: {"observations": [{"d": "2026-10-30", "V39079": {"v": "2.50"}}]}.
        const QUrl url("https://www.bankofcanada.ca/valet/observations/" + s.id + "/json");
        const QString id = s.id;
        client->raw("boc:" + s.id, url, 12 * Hour, true, this, [done, id](const QByteArray &body, const QString &err) {
            Series x;
            for (const QJsonValue &v : QJsonDocument::fromJson(body).object().value("observations").toArray()) {
                const QJsonObject o = v.toObject();
                bool ok = false;
                const double value = o.value(id).toObject().value("v").toString().toDouble(&ok);
                const QDate d = QDate::fromString(o.value("d").toString(), Qt::ISODate);
                if (ok && d.isValid())
                    x.push_back({d, value});
            }
            sortSeries(x);
            done(x, x.isEmpty() ? (err.isEmpty() ? tr("Not published") : err) : QString());
        });
        return;
    }

    if (s.provider == "boj") {
        // The Bank of Japan's time-series API: a data base ("db": `flow`) and a series code; dates 202609 (monthly),
        // 202603 (the third quarter), 20261029 (daily), 2026 (annual), in parallel arrays.
        QUrl url("https://www.stat-search.boj.or.jp/api/v1/getDataCode");
        url.setQuery(QString("format=json&lang=en&db=%1&code=%2").arg(s.flow, s.id));
        client->raw("boj:" + s.flow + ":" + s.id, url, 12 * Hour, true, this, [done](const QByteArray &body, const QString &err) {
            Series x;
            const QJsonObject r = QJsonDocument::fromJson(body).object().value("RESULTSET").toArray().at(0).toObject();
            const QString freq = r.value("FREQUENCY").toString();
            const QJsonArray dates = r.value("VALUES").toObject().value("SURVEY_DATES").toArray();
            const QJsonArray values = r.value("VALUES").toObject().value("VALUES").toArray();
            for (int i = 0; i < dates.size() && i < values.size(); ++i) {
                if (!values[i].isDouble())
                    continue;
                const QString t = QString::number(dates[i].toInteger());
                QDate d;
                if (freq.startsWith("DAILY") || t.size() == 8) d = QDate::fromString(t, "yyyyMMdd");
                else if (freq.startsWith("QUARTERLY")) d = QDate(t.left(4).toInt(), (t.mid(4).toInt() - 1) * 3 + 1, 1);
                else if (freq.startsWith("SEMIANNUAL")) d = QDate(t.left(4).toInt(), t.mid(4).toInt() == 2 ? 7 : 1, 1);
                else if (t.size() == 4) d = QDate(t.toInt(), 1, 1);
                else d = QDate(t.left(4).toInt(), t.mid(4).toInt(), 1);
                if (d.isValid())
                    x.push_back({d, values[i].toDouble()});
            }
            sortSeries(x);
            done(x, x.isEmpty() ? (err.isEmpty() ? tr("Not published") : err) : QString());
        });
        return;
    }

    if (s.provider == "snb") {
        // The Swiss National Bank's data portal: a cube (`flow`) as CSV, ";"-separated, a few metadata lines, then
        // "Date";"D0"[;"D1"…];"Value"; the series is the dimension values joined by '.' (`key`).
        const QUrl url("https://data.snb.ch/api/cube/" + s.flow + "/data/csv/en");
        const QString key = s.key;
        client->raw("snb:" + s.flow, url, 12 * Hour, false, this, [done, key](const QByteArray &raw, const QString &err) {
            QByteArray body = raw;
            if (body.startsWith("\xEF\xBB\xBF"))
                body.remove(0, 3);
            Series x;
            bool data = false;
            for (const QByteArray &line : body.split('\n')) {
                QList<QByteArray> cells = line.trimmed().split(';');
                for (QByteArray &c : cells)
                    c = c.trimmed().replace("\"", "");
                if (cells.value(0) == "Date") {
                    data = true;
                    continue;
                }
                if (!data || cells.size() < 3)
                    continue;
                QStringList dims;
                for (int i = 1; i < cells.size() - 1; ++i)
                    dims << QString::fromLatin1(cells[i]);
                if (dims.join('.') != key)
                    continue;
                bool ok = false;
                const double v = cells.last().toDouble(&ok);
                const QDate d = periodStart(QString::fromLatin1(cells[0]));
                if (ok && d.isValid())
                    x.push_back({d, v});
            }
            sortSeries(x);
            done(x, x.isEmpty() ? (err.isEmpty() ? tr("Not published") : err) : QString());
        });
        return;
    }

    if (s.provider == "boe") {
        // The Bank of England's statistical database, one series as CSV ("DATE,CODE", dates "30 Sep 2026"); it redirects
        // to an error page for a start before 1963.
        QUrl url("https://www.bankofengland.co.uk/boeapps/database/_iadb-fromshowcolumns.asp");
        url.setQuery("csv.x=yes&Datefrom=01/Jan/1963&Dateto=now&SeriesCodes=" + s.id + "&CSVF=TN&UsingCodes=Y&VPD=Y&VFD=N");
        client->raw("boe:csv:" + s.id, url, 12 * Hour, false, this, [done](const QByteArray &body, const QString &err) {
            if (body.trimmed().startsWith('<')) {   // its error page, served with a 200
                done({}, tr("Not published"));
                return;
            }
            Series x;
            const QLocale c(QLocale::C);
            for (const QByteArray &line : body.split('\n')) {
                const QList<QByteArray> cells = line.trimmed().split(',');
                if (cells.size() < 2 || cells[0] == "DATE")
                    continue;
                bool ok = false;
                const double v = cells[1].toDouble(&ok);
                const QDate d = c.toDate(QString::fromLatin1(cells[0]), "d MMM yyyy");
                if (ok && d.isValid())
                    x.push_back({d, v});
            }
            sortSeries(x);
            // Monthly figures are dated the last day of the month: the first, as elsewhere.
            if (detectFreq(x) >= Freq::Monthly)
                for (Point &p : x)
                    p.date = QDate(p.date.year(), p.date.month(), 1);
            done(x, x.isEmpty() ? (err.isEmpty() ? tr("Not published") : err) : QString());
        });
        return;
    }

    if (s.provider == "ons") {
        // The Office for National Statistics' time series JSON: the monthly, quarterly or yearly observations
        // (the finest the series has), dates "2026 SEP", "2026 Q2", "2026".
        const QUrl url("https://www.ons.gov.uk/" + s.id + "/data");
        client->raw("ons:" + s.id, url, 12 * Hour, true, this, [done](const QByteArray &body, const QString &err) {
            const QJsonObject o = QJsonDocument::fromJson(body).object();
            Series x;
            for (const char *freq : {"months", "quarters", "years"}) {
                const QJsonArray obs = o.value(freq).toArray();
                if (obs.isEmpty())
                    continue;
                static const QStringList months {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
                for (const QJsonValue &v : obs) {
                    const QString date = v.toObject().value("date").toString();
                    bool ok = false;
                    const double value = v.toObject().value("value").toString().toDouble(&ok);
                    const int year = date.left(4).toInt();
                    const QString rest = date.mid(5).trimmed();
                    QDate d;
                    if (rest.isEmpty()) d = QDate(year, 1, 1);
                    else if (rest.startsWith('Q')) d = QDate(year, (rest.mid(1).toInt() - 1) * 3 + 1, 1);
                    else if (const int m = months.indexOf(rest.left(3).toUpper()); m >= 0) d = QDate(year, m + 1, 1);
                    if (ok && d.isValid())
                        x.push_back({d, value});
                }
                break;
            }
            sortSeries(x);
            done(x, x.isEmpty() ? (err.isEmpty() ? tr("Not published") : err) : QString());
        });
        return;
    }

    if (s.provider == "ilo") {
        // ILOSTAT's bulk service (the one its R package reads): a CSV of every breakdown of the indicator for
        // the country; the total of both sexes, aged 15 and over unless `classif` says otherwise.
        QUrl url("https://rplumber.ilo.org/data/indicator/");
        url.setQuery(QString("id=%1&ref_area=%2&format=.csv&type=code").arg(s.id, m_iso3 == "XKX" ? QStringLiteral("KOS") : m_iso3));
        const QString classif = s.classif.isEmpty() ? QStringLiteral("AGE_YTHADULT_YGE15") : s.classif;
        client->raw("ilo:" + s.id + ":" + m_iso3, url, 24 * Hour, false, this, [done, classif](const QByteArray &raw, const QString &err) {
            QByteArray body = raw;
            if (body.startsWith("\xEF\xBB\xBF"))   // a UTF-8 byte order mark
                body.remove(0, 3);
            Series x;
            const QList<QByteArray> lines = body.split('\n');
            const QList<QByteArray> head = splitCsv(lines.value(0).trimmed());
            const int sex = head.indexOf("sex"), cls = head.indexOf("classif1"), time = head.indexOf("time"),
                      value = head.indexOf("obs_value");
            for (int i = 1; i < lines.size() && time >= 0 && value >= 0; ++i) {
                const QList<QByteArray> c = splitCsv(lines[i].trimmed());
                if (c.size() <= std::max(time, value))
                    continue;
                if (sex >= 0 && !c.value(sex).isEmpty() && c.value(sex) != "SEX_T")
                    continue;
                if (classif != "-" && cls >= 0 && !c.value(cls).isEmpty() && c.value(cls) != classif.toLatin1())
                    continue;
                bool ok = false;
                const double v = c[value].toDouble(&ok);
                const QDate d = periodStart(QString::fromLatin1(c[time]));
                if (ok && d.isValid())
                    x.push_back({d, v});
            }
            sortSeries(x);
            done(x, x.isEmpty() ? (err.isEmpty() ? tr("Not published") : err) : QString());
        });
        return;
    }

    if (s.provider == "yahoo") {
        QString symbol;
        if (s.kind == "stock") {
            symbol = cat.stockIndexes.value(m_iso3);
        } else {
            const QString cur = cat.currencies.value(m_iso3);
            if (cur == "DXY") symbol = "DX-Y.NYB";
            else if (cur == "EUR" || cur == "GBP" || cur == "AUD" || cur == "NZD") symbol = cur + "USD=X";
            else symbol = cur + "=X";
        }
        // Yahoo thins "max" out to a point a quarter: the monthly history for the highest and lowest, then the
        // daily closes of the last month for the last and previous values.
        QPointer<CountryLoader> self(this);
        client->prices(symbol, "max", "1mo", this, [self, symbol, done](const Series &history, const QString &err) {
            if (!self) return;
            research::Client::instance()->prices(symbol, "1mo", "1d", self, [history, err, done](const Series &recent, const QString &err2) {
                Series all;
                const QDate from = recent.isEmpty() ? QDate() : recent.front().date;
                for (const Point &p : history)
                    if (!from.isValid() || p.date < from.addDays(-31))
                        all.push_back(p);
                all += recent;
                sortSeries(all);
                // Yahoo's long histories carry the odd bad tick (USD/KRW at 0.11): points more than fifty times off
                // the median go.
                if (all.size() > 10) {
                    QVector<double> v;
                    for (const Point &p : std::as_const(all))
                        v << p.value;
                    std::nth_element(v.begin(), v.begin() + v.size() / 2, v.end());
                    const double median = v[v.size() / 2];
                    if (median > 0)
                        all.erase(std::remove_if(all.begin(), all.end(), [median](const Point &p) {
                                      return p.value <= 0 || p.value < median / 50 || p.value > median * 50;
                                  }), all.end());
                }
                done(all, all.isEmpty() ? (err2.isEmpty() ? err : err2) : QString());
            });
        });
        return;
    }

    if (s.provider == "eia") {
        const QString key = sourceKey("eia");
        if (key.isEmpty()) {
            done({}, tr("A free EIA key is needed (Sources)"));
            return;
        }
        QUrl url("https://api.eia.gov/v2/seriesid/" + s.id);
        url.setQuery("api_key=" + key);
        client->raw("eia:" + s.id, url, 6 * Hour, false, this, [done](const QByteArray &body, const QString &err) {
            Series x;
            for (const QJsonValue &v : QJsonDocument::fromJson(body).object().value("response").toObject().value("data").toArray()) {
                const QJsonObject o = v.toObject();
                const QJsonValue val = o.value("value");
                bool ok = val.isDouble();
                const double d = ok ? val.toDouble() : val.toString().toDouble(&ok);
                const QDate date = periodStart(o.value("period").toString());
                if (ok && date.isValid())
                    x.push_back({date, d});
            }
            sortSeries(x);
            done(x, x.isEmpty() ? (err.isEmpty() ? tr("Not published") : err) : QString());
        });
        return;
    }

    done({}, tr("Unknown source"));
}

} // namespace countries
