#pragma once

// Port of lib/imports.js. Your own data files, kept in <data>/imports/:
//   - Bars: an OHLCV CSV. It charts as CSV:<NAME> on its own timeframe and any longer one (built
//     from its bars), and backtests like any other symbol.
//   - Signals: a CSV of time and position (+1 long, 0 flat, -1 short). It becomes an indicator
//     (custom/signal-<name>.js), so the strategy tester and the quant checks run on your strategy
//     exactly as your own code decided it.

#include "../core/Bar.h"
#include "../core/CandleSet.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QMutex>
#include <QString>

namespace qtm {

struct ParsedBars {
    Bars bars;
    double baseSeconds = 0;
    int skipped = 0;
};

struct Signals {
    int rows = 0;
    QVector<QPair<double, double>> changes;    // [time, position]
    double from = 0;
    double to = 0;
};

// What getCandles returns: { symbol, tf, provider, candles, notes }.
struct CandleResult {
    QString symbol;
    QString tf;
    QString provider;
    Bars candles;
    QStringList notes;
};

namespace imports {
QString cleanName(const QString &raw);         // throws 400 like the JS
double parseTime(const QString &cell, const QString *timeOfDay = nullptr);   // NaN when unreadable
QString tfLabel(double sec);
ParsedBars parseBars(const QString &text);
Signals parseSignals(const QString &text);
QString signalIndicatorSource(const QString &name, const Signals &s, const QString &fileName);
QJsonObject signalStrategy(const QString &id);
} // namespace imports

class Imports
{
public:
    explicit Imports(const QString &dataDir);

    QString importsDir() const { return m_dir; }
    QJsonObject importBars(const QString &rawName, const QString &text);
    QJsonArray listImports();
    CandleResult csvCandles(const QString &name, const QString &tf, int limit);

private:
    struct Parsed {
        double mtime = 0;
        ParsedBars p;
        QHash<QString, Bars> byTf;
    };
    std::shared_ptr<Parsed> load(const QString &name);

    QString m_dir;
    QMutex m_mutex;
    QHash<QString, std::shared_ptr<Parsed>> m_parsed;
};

} // namespace qtm
