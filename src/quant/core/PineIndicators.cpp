// Pine Script files as custom indicators: see PineIndicators.h.
#include "PineIndicators.h"

#include "Error.h"
#include "IndicatorsInternal.h"
#include "PineScript.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMutex>
#include <QRegularExpression>
#include <QSet>

namespace qtm {

namespace {

// ---------------------------------------------------------------- the file, re-read when it changes

struct CachedSource {
    QDateTime mtime;
    qint64 size = -1;
    QString text;
};
QMutex g_sourcesMutex;
QHash<QString, CachedSource> g_sources;

QString sourceOf(const QString &path)
{
    const QFileInfo fi(path);
    if (!fi.isFile()) throw Error(QStringLiteral("Cannot find module '%1'").arg(path));
    QMutexLocker lock(&g_sourcesMutex);
    CachedSource &c = g_sources[path];
    if (c.size == fi.size() && c.mtime == fi.lastModified() && c.size >= 0) return c.text;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) throw Error(QStringLiteral("Cannot read '%1'").arg(path));
    c.text = QString::fromUtf8(f.readAll());
    c.size = fi.size();
    c.mtime = fi.lastModified();
    return c.text;
}

// ---------------------------------------------------------------- keys

// An output or input key the indicator format accepts: letters, digits and _, starting with a
// letter, unique among `used`.
QString keyFor(const QString &wanted, const QString &fallback, QSet<QString> &used)
{
    static const QRegularExpression bad(QStringLiteral("[^A-Za-z0-9_]+"));
    QString k = wanted.trimmed();
    k.replace(bad, QStringLiteral("_"));
    k.remove(QRegularExpression(QStringLiteral("^_+|_+$")));
    if (k.isEmpty() || !k[0].isLetter()) k = fallback + (k.isEmpty() ? QString() : QStringLiteral("_") + k);
    QString out = k;
    for (int n = 2; used.contains(out); ++n) out = k + QStringLiteral("_") + QString::number(n);
    used.insert(out);
    return out;
}

// ---------------------------------------------------------------- inputs

// One chart input per Pine input the indicator format can show, and how its value goes back.
struct InputMap {
    QString key;       // the chart param key
    QString pineId;    // the PineTS input id ("in_0")
    QString type;      // the Pine type
};

bool numberLike(const QJsonValue &v)
{
    return v.isDouble();
}

QVector<InputMap> mapInputs(const QVector<pine::Input> &pineInputs, QVector<IndicatorInput> &out)
{
    QVector<InputMap> maps;
    QSet<QString> used;
    bool haveSource = false;
    int n = 0;
    for (const pine::Input &in : pineInputs) {
        ++n;
        IndicatorInput x;
        x.min = x.max = QJsonValue(QJsonValue::Undefined);   // "not set" (a null QJsonValue is a value)
        x.label = in.title.isEmpty() ? in.id : in.title;
        const QString fallback = QStringLiteral("input%1").arg(n);
        if (in.type == QLatin1String("int") || in.type == QLatin1String("float") || in.type == QLatin1String("price")) {
            if (!numberLike(in.defval)) continue;
            x.type = in.type == QLatin1String("int") ? QStringLiteral("int") : QStringLiteral("float");
            x.def = in.type == QLatin1String("int") ? QJsonValue(double(qint64(in.defval.toDouble()))) : in.defval;
            if (numberLike(in.minval)) x.min = in.minval;
            if (numberLike(in.maxval)) x.max = in.maxval;
            if (!in.options.isEmpty()) {   // int/float with options: a choice between numbers
                x.type = QStringLiteral("select");
                for (const QJsonValue &o : in.options) x.options << QString::number(o.toDouble());
                x.def = QString::number(in.defval.toDouble());
                if (!x.options.contains(x.def.toString())) continue;
                x.min = x.max = QJsonValue(QJsonValue::Undefined);
            }
        } else if (in.type == QLatin1String("bool")) {
            x.type = QStringLiteral("select");
            x.options = QStringList { QStringLiteral("true"), QStringLiteral("false") };
            x.def = in.defval.toBool() ? QStringLiteral("true") : QStringLiteral("false");
        } else if (in.type == QLatin1String("source")) {
            if (haveSource) continue;   // the format allows one, keyed "source"
            static const QStringList FIELDS { "close", "open", "high", "low", "hl2", "hlc3", "ohlc4", "hlcc4", "volume" };
            const QString d = in.defval.toString();
            if (!FIELDS.contains(d)) continue;
            haveSource = true;
            x.type = QStringLiteral("source");
            x.def = d;
            x.key = QStringLiteral("source");
            used.insert(x.key);
        } else if (!in.options.isEmpty()) {   // string, enum, timeframe, session with options
            x.type = QStringLiteral("select");
            for (const QJsonValue &o : in.options) x.options << (o.isString() ? o.toString() : QString::number(o.toDouble()));
            x.def = in.defval.isString() ? in.defval.toString() : QString::number(in.defval.toDouble());
            if (!x.options.contains(x.def.toString())) continue;
        } else {
            continue;   // color, string without options, time, symbol, ...: the script's default
        }
        if (x.key.isEmpty()) x.key = keyFor(in.varId.isEmpty() ? in.id : in.varId, fallback, used);
        out << x;
        maps << InputMap { x.key, in.id, in.type };
    }
    return maps;
}

// The PineTS overrides for the chart's params.
QJsonObject pineInputs(const QVector<InputMap> &maps, const QJsonObject &params)
{
    QJsonObject out;
    for (const InputMap &m : maps) {
        if (!params.contains(m.key)) continue;
        const QJsonValue v = params.value(m.key);
        if (m.type == QLatin1String("bool")) {
            out.insert(m.pineId, v.isBool() ? v.toBool() : v.toString() == QLatin1String("true"));
        } else if (m.type == QLatin1String("int") || m.type == QLatin1String("float") || m.type == QLatin1String("price")) {
            const double d = detail::jsonNumber(v);
            if (!std::isnan(d)) out.insert(m.pineId, m.type == QLatin1String("int") ? double(qint64(d)) : d);
        } else {
            out.insert(m.pineId, v.isString() ? v.toString() : v.toVariant().toString());
        }
    }
    return out;
}

// ---------------------------------------------------------------- plots -> outputs, levels, markers

bool isSeriesStyle(const QString &style)
{
    static const QStringList LINES { "line", "linebr", "stepline", "stepline_diamond", "area", "areabr", "histogram", "columns", "circles", "cross" };
    return LINES.contains(style);
}

QString positionOf(const QString &location)
{
    if (location.compare(QLatin1String("AboveBar"), Qt::CaseInsensitive) == 0) return QStringLiteral("aboveBar");
    if (location.compare(QLatin1String("BelowBar"), Qt::CaseInsensitive) == 0) return QStringLiteral("belowBar");
    return QStringLiteral("inBar");
}

QString shapeOf(const pine::Plot &p)
{
    const QString s = p.shape.toLower();
    if (!s.isEmpty()) {
        if (s.contains(QLatin1String("up"))) return QStringLiteral("arrowUp");
        if (s.contains(QLatin1String("down"))) return QStringLiteral("arrowDown");
        if (s.contains(QLatin1String("circle"))) return QStringLiteral("circle");
        return QStringLiteral("square");
    }
    static const QHash<QString, QString> CHARS {
        { QStringLiteral("▲"), QStringLiteral("arrowUp") }, { QStringLiteral("△"), QStringLiteral("arrowUp") },
        { QStringLiteral("↑"), QStringLiteral("arrowUp") }, { QStringLiteral("▼"), QStringLiteral("arrowDown") },
        { QStringLiteral("▽"), QStringLiteral("arrowDown") }, { QStringLiteral("↓"), QStringLiteral("arrowDown") },
        { QStringLiteral("■"), QStringLiteral("square") }, { QStringLiteral("□"), QStringLiteral("square") },
        { QStringLiteral("◆"), QStringLiteral("square") },
    };
    return CHARS.value(p.ch, QStringLiteral("circle"));
}

struct PlotMap {
    QString plotKey;
    QString outKey;
};

// The chart definition parts of a run: outputs for series plots, a level per hline. `maps` says
// which output each plot feeds.
void defineOutputs(const pine::Result &r, IndicatorDef &d, QVector<PlotMap> &maps)
{
    QSet<QString> used;
    int n = 0;
    for (const pine::Plot &p : r.plots) {
        ++n;
        if (p.style == QLatin1String("hline")) {
            for (double v : p.values)
                if (std::isfinite(v)) {
                    d.levels << v;
                    break;
                }
            continue;
        }
        if (!isSeriesStyle(p.style)) continue;
        IndicatorOutput o;
        o.key = keyFor(p.key, QStringLiteral("plot%1").arg(n), used);
        o.label = p.title.isEmpty() ? o.key : p.title;
        const bool bars = p.style == QLatin1String("histogram") || p.style == QLatin1String("columns");
        o.kind = bars ? QStringLiteral("histogram") : QStringLiteral("line");
        o.color = p.color;
        o.lineWidth = std::clamp(p.linewidth, 0, 4);
        o.dots = p.style == QLatin1String("circles") || p.style == QLatin1String("cross");
        d.outputs << o;
        maps << PlotMap { p.key, o.key };
    }
}

IndicatorResult resultOf(const pine::Result &r, const Bars &bars, const QVector<PlotMap> &maps)
{
    IndicatorResult out;
    bool colorsSet = false;
    for (const pine::Plot &p : r.plots) {
        if (p.isMarks()) {
            out.hasMarkers = true;
            for (int i = 0; i < p.flags.size() && i < bars.size(); ++i) {
                if (!p.flags[i]) continue;
                IndicatorMarker m;
                m.time = bars[i].time;
                m.position = positionOf(p.location);
                m.shape = shapeOf(p);
                m.color = i < p.colors.size() ? p.colors[i] : p.color;
                m.text = i < p.texts.size() ? p.texts[i] : (!p.text.isEmpty() ? p.text : p.ch);   // a char plot shows its character
                out.markers.push_back(m);
            }
            continue;
        }
        for (const PlotMap &m : maps) {
            if (m.plotKey != p.key) continue;
            out.values.insert(m.outKey, p.values);
            if (!colorsSet && !p.colors.isEmpty()
                && (p.style == QLatin1String("histogram") || p.style == QLatin1String("columns"))) {
                out.colors = p.colors;
                colorsSet = true;
            }
            break;
        }
    }
    std::sort(out.markers.begin(), out.markers.end(), [](const IndicatorMarker &a, const IndicatorMarker &b) { return a.time < b.time; });
    return out;
}

} // namespace

IndicatorDefPtr loadPineIndicator(const QString &path, const QString &id)
{
    const QString file = QFileInfo(path).absoluteFilePath();
    const QString source = sourceOf(file);
    const pine::Result trial = pine::run(source, detail::trialBars());

    IndicatorDef d;
    d.id = id;
    d.name = trial.declaration.title.trimmed();
    if (d.name.isEmpty()) {
        d.name = QFileInfo(file).completeBaseName();
        d.name.replace(QLatin1Char('-'), QLatin1Char(' '));
    }
    d.name = d.name.left(80);
    d.group = QStringLiteral("Custom");
    d.overlay = trial.declaration.overlay;
    d.file = file;
    const QVector<InputMap> inputMaps = mapInputs(trial.inputs, d.inputs);
    QVector<PlotMap> plotMaps;
    defineOutputs(trial, d, plotMaps);
    if (d.outputs.isEmpty())
        throw Error(QStringLiteral("This script plots nothing the chart can draw: add a plot() of a series (line, histogram, columns, ...)"));

    d.calc = [file, inputMaps, plotMaps](const Bars &bars, const QJsonObject &params) {
        const pine::Result r = pine::run(sourceOf(file), bars, pineInputs(inputMaps, params));
        return resultOf(r, bars, plotMaps);
    };
    return registerIndicator(d);
}

QString pineIndicatorJs(const QString &path)
{
    return pine::transpile(sourceOf(QFileInfo(path).absoluteFilePath()));
}

} // namespace qtm
