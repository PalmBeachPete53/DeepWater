#pragma once

// Pine Script, run by PineTS (third_party/pinets, LuxAlgo's Pine Script runtime for JavaScript,
// AGPL-3.0) in a Node.js child process: one per thread, started on first use and kept for the
// thread's life, speaking line-delimited JSON over its pipes (third_party/pinets/deepwater-pinets.mjs
// is the other end). The bundle and the bridge script are compiled into the app as resources and
// copied to runtimeDir() before the first start.
//
// Node.js is found through the `pinets/node` setting, the DEEPWATER_NODE environment variable, the
// PATH, then the usual places (Homebrew, /usr/local, nvm, volta, fnm). Nothing here needs the
// network: PineTS runs on the bars it is given.

#include "Bar.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>
#include <QVector>

namespace qtm::pine {

// An `input.*()` of the script, as PineTS scans it (title-keyed overrides go to run()).
struct Input {
    QString id;        // "in_0", "in_1", ...: the key run() accepts for an override
    QString varId;     // the Pine variable the input is assigned to ("len"), empty when none
    QString title;
    QString type;      // int | float | bool | string | source | color | enum | price | time | session | symbol | timeframe | text_area
    QJsonValue defval;
    QJsonArray options;
    QJsonValue minval, maxval, step;   // undefined when not set
    QString group, tooltip;
};

// The `indicator()` / `strategy()` declaration.
struct Declaration {
    QString type;      // "indicator", "strategy", or empty when the script has neither call
    int version = 0;   // the `//@version=` of the source, 0 when absent
    QString title, shorttitle;
    bool overlay = false;
    QJsonObject props; // every declaration argument PineTS resolved (precision, format, ...)
};

// One plot*(), hline() or bgcolor()/fill() of the script.
struct Plot {
    QString key;       // PineTS's plot key: the title, or "plot#0"-style when untitled
    QString title;
    QString style;     // line | linebr | stepline | area | histogram | columns | circles | cross | char | shape | hline | background | fill | candle | bar
    QString color;     // CSS colour (#rrggbb or #rrggbbaa), empty when none
    int linewidth = 0; // 0 = not set
    bool overlay = true;
    QString location;  // char and shape plots: AboveBar | BelowBar | Top | Bottom | Absolute
    QString shape;     // shape plots: shape_triangle_up, shape_circle, ...
    QString ch;        // char plots: the character
    QString text;      // char and shape plots: the text argument
    Values values;     // one per bar, NaN where na (empty for char and shape plots)
    QVector<bool> flags;   // char and shape plots: one per bar, true where the mark shows
    QStringList colors;    // per-bar colours when they vary (else empty: use `color`)
    QStringList texts;     // char and shape plots: per-bar texts when they vary

    bool isMarks() const { return style == QLatin1String("char") || style == QLatin1String("shape"); }
};

struct Result {
    Declaration declaration;
    QVector<Input> inputs;
    QVector<Plot> plots;
    QStringList warnings;   // PineTS runtime warnings (out-of-bounds reads, ...): the script went on
};

struct Options {
    QString tickerId;   // syminfo.tickerid (default CHART)
    QString timeframe;  // TradingView style ("1", "60", "D", "W"); empty = from the bar spacing
    QString timezone;   // chart timezone for log timestamps (IANA name); empty = PineTS's default
    int timeoutMs = 60000;
};

// Whether Node.js and the bridge answer on this thread (starting them when needed). `reason` gets
// the problem when they don't.
bool available(QString *reason = nullptr);
QString nodePath();                 // the Node.js binary in use, empty when none was found
void setNodePath(const QString &path);   // saves the `pinets/node` setting (empty = detect again)
QString nodeVersion();              // "v22.0.0" once a bridge answered on this thread, else empty
QString pinetsVersion();            // the bundled PineTS version
QString runtimeDir();               // where the bundle and bridge are copied (AppData/pinets)
void setRuntimeDir(const QString &dir);   // tests: use another folder (before the first call)

// The JavaScript PineTS runs for a Pine Script source: an `async $ => { ... }` function over
// `$.data` (the series) and `$.pine` (ta, input, plot, ...). Throws qtm::Error with PineTS's
// message when the source doesn't transpile, or when Node.js can't be run.
QString transpile(const QString &source);

// Runs the script on the bars. `inputs` overrides inputs by id ("in_0"), variable ("len") or
// title ("Length"). Throws qtm::Error with PineTS's message (transpile or runtime error), or when
// Node.js can't be run.
Result run(const QString &source, const Bars &bars, const QJsonObject &inputs = {}, const Options &options = {});

} // namespace qtm::pine
