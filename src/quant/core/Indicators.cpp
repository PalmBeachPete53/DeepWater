// The indicator registry (built-ins, then custom indicators in registration order), compute(),
// and the custom indicator format checks. Port of the end of quant-terminal/public/lib/indicators.js.
#include "Indicators.h"

#include "Error.h"
#include "IndicatorsInternal.h"

#include <QReadWriteLock>
#include <QRegularExpression>

#if defined(__clang__)
#pragma clang fp contract(off)
#endif

namespace qtm {

namespace {

// Read from the UI thread and from backend workers; custom indicators can be added at any time.
struct Registry {
    QReadWriteLock lock;
    QVector<IndicatorDefPtr> list;
    QHash<QString, IndicatorDefPtr> byId;

    Registry()
    {
        for (IndicatorDef &d : detail::builtinIndicators()) {
            auto p = std::make_shared<const IndicatorDef>(std::move(d));
            list.push_back(p);
            byId.insert(p->id, p);
        }
    }
};

Registry &registry()
{
    static Registry r;
    return r;
}

const QStringList INPUT_TYPES { "int", "float", "select", "source", "time" };
const QStringList PRICE_FIELDS { "close", "open", "high", "low", "hl2", "hlc3", "ohlc4", "hlcc4", "volume" };
const QStringList MARKER_POSITIONS { "aboveBar", "belowBar", "inBar" };
const QStringList MARKER_SHAPES { "arrowUp", "arrowDown", "circle", "square" };

bool isFiniteNumber(const QJsonValue &v)
{
    return v.isDouble() && std::isfinite(v.toDouble());
}

} // namespace

namespace detail {

IndicatorDefPtr insertIndicator(IndicatorDef def)
{
    auto p = std::make_shared<const IndicatorDef>(std::move(def));
    Registry &r = registry();
    QWriteLocker lock(&r.lock);
    bool replaced = false;
    for (auto &d : r.list)
        if (d->id == p->id) {
            d = p;
            replaced = true;
            break;
        }
    if (!replaced) r.list.push_back(p);
    r.byId.insert(p->id, p);
    return p;
}

} // namespace detail

QVector<IndicatorDefPtr> indicators()
{
    Registry &r = registry();
    QReadLocker lock(&r.lock);
    return r.list;
}

IndicatorDefPtr indicatorById(const QString &id)
{
    Registry &r = registry();
    QReadLocker lock(&r.lock);
    return r.byId.value(id);
}

QJsonObject indicatorDefaults(const QString &id)
{
    QJsonObject out;
    if (const IndicatorDefPtr def = indicatorById(id))
        for (const IndicatorInput &i : def->inputs) out.insert(i.key, i.def);
    return out;
}

// Compute an indicator. Unknown params fall back to defaults.
IndicatorResult compute(const QString &id, const Bars &bars, const QJsonObject &params)
{
    const IndicatorDefPtr def = indicatorById(id);
    if (!def) throw Error(QStringLiteral("Unknown indicator: %1").arg(id));
    QJsonObject p;
    for (const IndicatorInput &i : def->inputs) p.insert(i.key, i.def);
    for (auto it = params.begin(); it != params.end(); ++it) p.insert(it.key(), it.value());
    return def->calc(bars, p);
}

// The custom indicator format (docs/custom-indicators.md), checked before an indicator loads, so a
// malformed file is refused with every problem named instead of breaking a chart later. This is
// the typed version (C++ definitions); custom JS files are checked on the JS object itself
// (CustomIndicators.cpp), where the types can still be wrong.
QStringList indicatorProblems(const IndicatorDef &def, bool run)
{
    static const QRegularExpression KEY_RE(QStringLiteral("^[A-Za-z][A-Za-z0-9_]*$"));
    QStringList problems;
    if (def.name.trimmed().isEmpty())
        problems << QStringLiteral("it needs a `name` (text)");
    else if (def.name.size() > 80)
        problems << QStringLiteral("its `name` is longer than 80 characters");
    if (!def.calc) problems << QStringLiteral("it needs a `calc(bars, p, ta)` function");

    QSet<QString> seen;
    for (const IndicatorInput &inp : def.inputs) {
        const QString at = QStringLiteral("input \"%1\"").arg(inp.key);
        if (!KEY_RE.match(inp.key).hasMatch())
            problems << at + QStringLiteral(": `key` must be letters, digits or _ and start with a letter");
        else if (seen.contains(inp.key))
            problems << at + QStringLiteral(": the key is used twice");
        seen.insert(inp.key);
        if (!INPUT_TYPES.contains(inp.type)) {
            problems << at + QStringLiteral(": `type` must be one of ") + INPUT_TYPES.join(QStringLiteral(", "));
            continue;
        }
        const QJsonValue &d = inp.def;
        if (inp.type == QLatin1String("int") && !(isFiniteNumber(d) && std::trunc(d.toDouble()) == d.toDouble()))
            problems << at + QStringLiteral(": an int input needs a whole-number default");
        if (inp.type == QLatin1String("float") && !isFiniteNumber(d))
            problems << at + QStringLiteral(": a float input needs a number default");
        if (inp.type == QLatin1String("time") && !(isFiniteNumber(d) && d.toDouble() >= 0))
            problems << at + QStringLiteral(": a time input's default is a bar time in seconds, or 0");
        if (inp.type == QLatin1String("select")) {
            if (inp.options.isEmpty())
                problems << at + QStringLiteral(": a select input needs `options`, a list of text choices");
            else if (!d.isString() || !inp.options.contains(d.toString()))
                problems << at + QStringLiteral(": its default must be one of its options");
        }
        if (inp.type == QLatin1String("source")) {
            if (inp.key != QLatin1String("source")) problems << at + QStringLiteral(": a source input's key must be \"source\"");
            if (!d.isString() || !PRICE_FIELDS.contains(d.toString()))
                problems << at + QStringLiteral(": a source input's default must be one of ") + PRICE_FIELDS.join(QStringLiteral(", "));
        }
        if (!inp.min.isUndefined() && !isFiniteNumber(inp.min)) problems << at + QStringLiteral(": `min` must be a number");
        if (!inp.max.isUndefined() && !isFiniteNumber(inp.max)) problems << at + QStringLiteral(": `max` must be a number");
    }

    QSet<QString> outKeys;
    if (def.outputs.isEmpty())
        problems << QStringLiteral("`outputs` must be a list with at least one output");
    for (const IndicatorOutput &o : def.outputs) {
        const QString at = QStringLiteral("output \"%1\"").arg(o.key);
        if (!KEY_RE.match(o.key).hasMatch())
            problems << at + QStringLiteral(": `key` must be letters, digits or _ and start with a letter");
        else if (outKeys.contains(o.key))
            problems << at + QStringLiteral(": the key is used twice");
        outKeys.insert(o.key);
        if (!o.kind.isEmpty() && o.kind != QLatin1String("line") && o.kind != QLatin1String("histogram"))
            problems << at + QStringLiteral(": `kind` must be \"line\" or \"histogram\"");
        if (!o.color.isEmpty() && o.color.trimmed().isEmpty())
            problems << at + QStringLiteral(": `color` must be a colour like #2962ff");
        if (o.lineWidth != 0 && !(o.lineWidth >= 1 && o.lineWidth <= 4))
            problems << at + QStringLiteral(": `lineWidth` must be 1, 2, 3 or 4");
    }
    for (double l : def.levels)
        if (!std::isfinite(l)) {
            problems << QStringLiteral("`levels` must be a list of numbers");
            break;
        }

    // A trial run on test bars: calc must return one value per bar for every output.
    if (run && problems.isEmpty()) {
        const Bars bars = detail::trialBars();
        QJsonObject p;
        for (const IndicatorInput &i : def.inputs) p.insert(i.key, i.def);
        IndicatorResult res;
        try {
            res = def.calc(bars, p);
        } catch (const Error &e) {
            problems << QStringLiteral("calc() stopped with an error on test bars: ") + e.message();
        } catch (const std::exception &e) {
            problems << QStringLiteral("calc() stopped with an error on test bars: ") + QString::fromUtf8(e.what());
        }
        if (problems.isEmpty()) {
            for (const IndicatorOutput &o : def.outputs) {
                if (!res.values.contains(o.key))
                    problems << QStringLiteral("output \"%1\": calc() returned no list for it. Return { %1: [...] }").arg(o.key);
                else if (res.values.value(o.key).size() != bars.size())
                    problems << QStringLiteral("output \"%1\": calc() returned %2 values, but there are %3 bars. Return one "
                                               "value per bar (NaN where there isn't one)")
                                    .arg(o.key)
                                    .arg(res.values.value(o.key).size())
                                    .arg(bars.size());
            }
            if (res.hasMarkers) {
                bool ok = true;
                for (const IndicatorMarker &m : res.markers)
                    if (!std::isfinite(m.time) || !MARKER_POSITIONS.contains(m.position) || !MARKER_SHAPES.contains(m.shape))
                        ok = false;
                if (!ok)
                    problems << QStringLiteral("markers: each needs a time in seconds, a position (%1) and a shape (%2)")
                                    .arg(MARKER_POSITIONS.join(QStringLiteral(", ")), MARKER_SHAPES.join(QStringLiteral(", ")));
            }
        }
    }
    return problems;
}

void validateIndicator(const IndicatorDef &def)
{
    const QStringList problems = indicatorProblems(def);
    if (!problems.isEmpty())
        throw Error(QStringLiteral("This indicator doesn't follow the format (docs/custom-indicators.md):\n- ")
                    + problems.join(QStringLiteral("\n- ")));
}

// Custom indicators register here. Each one is checked against the format first, so a malformed
// definition is refused with its problems listed.
IndicatorDefPtr registerIndicator(IndicatorDef def)
{
    if (def.id.isEmpty()) throw Error(QStringLiteral("A custom indicator needs an id"));
    if (def.group.isEmpty()) def.group = QStringLiteral("Custom");
    if (def.outputs.isEmpty()) {
        IndicatorOutput o;
        o.key = QStringLiteral("value");
        o.label = def.name.isEmpty() ? def.id : def.name;
        o.kind = QStringLiteral("line");
        o.color = QStringLiteral("#ff9800");
        def.outputs << o;
    }
    validateIndicator(def);
    return detail::insertIndicator(std::move(def));
}

} // namespace qtm
