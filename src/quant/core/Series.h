#pragma once

// Port of quant-terminal/public/lib/series.js: timeframes, resampling and chart-type transforms.
// Every function behaves exactly like its JS original (same numbers, same error messages).

#include "Bar.h"

#include <QPair>
#include <QStringList>

namespace qtm {

struct Tf {
    int n = 1;
    QChar unit = 'D';    // s m h D W M
    qint64 seconds = 86400;
};

Tf parseTf(const QString &tf);               // throws qtm::Error("Bad timeframe ...")
bool isIntraday(const QString &tf);
qint64 bucketStart(qint64 t, const QString &tf);
// Seconds until the current candle closes, from the last bar's time; -1 when there is nothing to count down: that bar's
// period ended more than half an hour ago (market closed; a delayed feed stays within that), or `knownClose` is false and
// the bar is a day or longer, whose close depends on an exchange's session.
qint64 secondsToBarClose(qint64 barTime, const QString &tf, qint64 now, bool knownClose = true);
// 59:30, 03:59:30, 2d 23:59
QString formatCountdown(qint64 seconds);
Bars resample(const Bars &bars, const QString &tf);
QString nativeBase(const QString &tf, const QStringList &natives);   // empty = none

Bars heikinAshi(const Bars &bars);
Bars renko(const Bars &bars, double boxSize = 0);
Bars lineBreak(const Bars &bars, int lines = 3);
Bars rangeBars(const Bars &bars, double range = 0);
Bars kagi(const Bars &bars, double reversal = 0);
Bars pointFigure(const Bars &bars, double box = 0, int reversal = 3);

// extraCols keeps insertion order (JS object key order).
QString toCsv(const Bars &bars, const QVector<QPair<QString, Values>> &extraCols = {});

} // namespace qtm
