#include "fusion/Store.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>

namespace fusion {

namespace {

constexpr int Version = 1;
enum Flags { Spliced = 1, Unverified = 2 };

QString day(const QDate &d) { return d.toString(Qt::ISODate); }
QDate day(const QJsonValue &v) { return QDate::fromString(v.toString(), Qt::ISODate); }

Relation relationFrom(const QString &name)
{
    for (Relation r : {Relation::Anchor, Relation::Same, Relation::Rebased, Relation::Distinct, Relation::Unverified,
                       Relation::Incompatible})
        if (relationName(r) == name)
            return r;
    return Relation::Unverified;
}

Freq freqFrom(const QString &name)
{
    for (Freq f : {Freq::Daily, Freq::Weekly, Freq::Monthly, Freq::Quarterly, Freq::Annual})
        if (freqName(f) == name)
            return f;
    return Freq::Unknown;
}

// NaN isn't JSON: null.
QJsonValue num(double v) { return std::isfinite(v) ? QJsonValue(v) : QJsonValue(); }
double num(const QJsonValue &v) { return v.isDouble() ? v.toDouble() : NaN; }

bool write(const QString &path, const QJsonObject &o, QString *error)
{
    QDir().mkpath(QFileInfo(path).path());
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        if (error) *error = f.errorString();
        return false;
    }
    f.write(QJsonDocument(o).toJson(QJsonDocument::Compact));
    if (!f.commit()) {
        if (error) *error = f.errorString();
        return false;
    }
    return true;
}

} // namespace

QString recordPath(const QString &dir, const QString &country, const QString &indicator)
{
    return dir + '/' + country + '/' + indicator + ".json";
}

QString rawPath(const QString &dir, const QString &country, const QString &indicator)
{
    return dir + '/' + country + '/' + indicator + ".raw.json";
}

QJsonObject toJson(const Golden &g)
{
    QJsonArray points;
    for (const GoldenPoint &p : g.points)
        points.append(QJsonArray {day(p.date), num(p.value), p.source, (p.spliced ? Spliced : 0) | (p.unverified ? Unverified : 0)});
    QJsonArray sources;
    for (const SourceUse &s : g.sources) {
        const Comparison &c = s.comparison;
        sources.append(QJsonObject {
            {"id", s.id}, {"label", s.label}, {"priority", s.priority}, {"freq", freqName(s.freq)},
            {"relation", relationName(c.relation)}, {"why", c.why}, {"unit_factor", c.unitFactor}, {"factor", c.factor},
            {"overlap", c.overlap}, {"median_gap", num(c.medianGap)}, {"correlation", num(c.correlation)}, {"agreement", num(c.agreement)},
            {"points", s.points}, {"used", s.used}, {"conflicts", s.conflicts}, {"first", day(s.first)}, {"last", day(s.last)}});
    }
    QJsonArray conflicts;
    for (const Conflict &c : g.conflicts)
        conflicts.append(QJsonArray {day(c.date), c.kept, c.other, num(c.keptValue), num(c.otherValue)});
    return {{"freq", freqName(g.freq)}, {"unit", g.unit}, {"spliced", g.spliced}, {"unverified", g.unverified},
            {"points", points}, {"sources", sources}, {"conflicts", conflicts}};
}

Golden goldenFromJson(const QJsonObject &o)
{
    Golden g;
    g.freq = freqFrom(o.value("freq").toString());
    g.unit = o.value("unit").toString();
    g.spliced = o.value("spliced").toInt();
    g.unverified = o.value("unverified").toInt();
    for (const QJsonValue &v : o.value("points").toArray()) {
        const QJsonArray a = v.toArray();
        GoldenPoint p;
        p.date = day(a.at(0));
        p.value = num(a.at(1));
        p.source = a.at(2).toInt(-1);
        const int flags = a.at(3).toInt();
        p.spliced = flags & Spliced;
        p.unverified = flags & Unverified;
        if (p.date.isValid())
            g.points.push_back(p);
    }
    for (const QJsonValue &v : o.value("sources").toArray()) {
        const QJsonObject s = v.toObject();
        SourceUse u;
        u.id = s.value("id").toString();
        u.label = s.value("label").toString();
        u.priority = s.value("priority").toInt();
        u.freq = freqFrom(s.value("freq").toString());
        u.comparison.relation = relationFrom(s.value("relation").toString());
        u.comparison.why = s.value("why").toString();
        u.comparison.unitFactor = s.value("unit_factor").toDouble(1);
        u.comparison.factor = s.value("factor").toDouble(1);
        u.comparison.overlap = s.value("overlap").toInt();
        u.comparison.medianGap = num(s.value("median_gap"));
        u.comparison.correlation = num(s.value("correlation"));
        u.comparison.agreement = num(s.value("agreement"));
        u.points = s.value("points").toInt();
        u.used = s.value("used").toInt();
        u.conflicts = s.value("conflicts").toInt();
        u.first = day(s.value("first"));
        u.last = day(s.value("last"));
        g.sources.push_back(u);
    }
    for (const QJsonValue &v : o.value("conflicts").toArray()) {
        const QJsonArray a = v.toArray();
        g.conflicts.push_back({day(a.at(0)), a.at(1).toInt(-1), a.at(2).toInt(-1), num(a.at(3)), num(a.at(4))});
    }
    return g;
}

Summary summarize(const Record &r)
{
    Summary sum;
    sum.country = r.country;
    sum.indicator = r.indicator;
    sum.fused = r.fused;
    for (const Golden &g : r.golden) {
        if (g.sources.size() < 2)
            continue;
        ++sum.records;
        sum.points += int(g.points.size());
        sum.spliced += g.spliced;
        sum.unverified += g.unverified;
        sum.conflicts += int(g.conflicts.size());
        for (int k = 0; k < g.sources.size(); ++k) {
            const SourceUse &u = g.sources[k];
            if (u.used <= 0)
                continue;
            if (k > 0)
                sum.added += u.used;
            const QString provider = u.id.section(':', 0, 0);
            if (!sum.providers.contains(provider) || !sum.labels.contains(u.label)) {
                sum.providers << provider;
                sum.labels << u.label;
            }
        }
    }
    return sum;
}

QString indexPath(const QString &dir)
{
    return dir + "/index.json";
}

namespace {

QJsonObject toJson(const Summary &s)
{
    return {{"fused", s.fused.toUTC().toString(Qt::ISODate)}, {"records", s.records}, {"points", s.points},
            {"added", s.added}, {"spliced", s.spliced}, {"unverified", s.unverified}, {"conflicts", s.conflicts},
            {"providers", QJsonArray::fromStringList(s.providers)}, {"labels", QJsonArray::fromStringList(s.labels)}};
}

QJsonObject readIndex(const QString &dir)
{
    QFile f(indexPath(dir));
    if (!f.open(QIODevice::ReadOnly))
        return {};
    const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
    return o.value("version").toInt() == Version ? o.value("records").toObject() : QJsonObject();
}

} // namespace

QVector<Summary> loadIndex(const QString &dir)
{
    QVector<Summary> out;
    QJsonObject records = readIndex(dir);
    if (records.isEmpty() && !QFile::exists(indexPath(dir))) {
        const QStringList countries = QDir(dir).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &country : countries)
            for (const QString &file : QDir(dir + '/' + country).entryList({"*.json"}, QDir::Files)) {
                if (file.endsWith(".raw.json"))
                    continue;
                if (const std::optional<Record> r = load(dir, country, file.chopped(5)))
                    records.insert(country + '/' + r->indicator, toJson(summarize(*r)));
            }
        if (!records.isEmpty())
            write(indexPath(dir), QJsonObject {{"version", Version}, {"records", records}}, nullptr);
    }
    for (auto it = records.constBegin(); it != records.constEnd(); ++it) {
        const QJsonObject o = it.value().toObject();
        Summary s;
        s.country = it.key().section('/', 0, 0);
        s.indicator = it.key().section('/', 1);
        s.fused = QDateTime::fromString(o.value("fused").toString(), Qt::ISODate);
        s.records = o.value("records").toInt();
        s.points = o.value("points").toInt();
        s.added = o.value("added").toInt();
        s.spliced = o.value("spliced").toInt();
        s.unverified = o.value("unverified").toInt();
        s.conflicts = o.value("conflicts").toInt();
        for (const QJsonValue &v : o.value("providers").toArray()) s.providers << v.toString();
        for (const QJsonValue &v : o.value("labels").toArray()) s.labels << v.toString();
        out.push_back(s);
    }
    return out;
}

bool save(const QString &dir, const Record &r, QString *error)
{
    QJsonArray golden;
    for (const Golden &g : r.golden)
        golden.append(toJson(g));
    const QJsonObject head {{"version", Version}, {"country", r.country}, {"indicator", r.indicator},
                            {"fused", r.fused.toUTC().toString(Qt::ISODate)}};
    QJsonObject o = head;
    o.insert("golden", golden);
    if (!write(recordPath(dir, r.country, r.indicator), o, error))
        return false;
    QJsonObject records = readIndex(dir);
    records.insert(r.country + '/' + r.indicator, toJson(summarize(r)));
    if (!write(indexPath(dir), QJsonObject {{"version", Version}, {"records", records}}, error))
        return false;
    if (r.inputs.isEmpty())
        return true;
    QJsonArray inputs;
    for (const Input &in : r.inputs) {
        QJsonArray points;
        for (const Point &p : in.points)
            points.append(QJsonArray {day(p.date), num(p.value)});
        inputs.append(QJsonObject {{"id", in.id}, {"label", in.label}, {"priority", in.priority}, {"unit", in.unit},
                                   {"fetched", in.fetched.toUTC().toString(Qt::ISODate)}, {"points", points}});
    }
    QJsonObject raw = head;
    raw.insert("inputs", inputs);
    return write(rawPath(dir, r.country, r.indicator), raw, error);
}

std::optional<Record> load(const QString &dir, const QString &country, const QString &indicator)
{
    QFile f(recordPath(dir, country, indicator));
    if (!f.open(QIODevice::ReadOnly))
        return std::nullopt;
    const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
    if (o.value("version").toInt() != Version)
        return std::nullopt;
    Record r;
    r.country = o.value("country").toString();
    r.indicator = o.value("indicator").toString();
    r.fused = QDateTime::fromString(o.value("fused").toString(), Qt::ISODate);
    for (const QJsonValue &v : o.value("golden").toArray())
        r.golden.push_back(goldenFromJson(v.toObject()));
    return r;
}

} // namespace fusion
