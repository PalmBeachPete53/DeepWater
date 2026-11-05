#pragma once

#include <QVector>

// Indicators in words: where each stands on the last bar, and for how long. Plain readings of the numbers, not signals.
// Values hold one number per bar, NaN where there is none.
namespace desk {

// How long `line` has been on the same side of `other`, in bars, the last included. 0 when the last bar has no value.
int barsOnSameSide(const QVector<double> &line, const QVector<double> &other);

struct TrendReading {
    int side = 0;     // +1 up, -1 down, 0 no data
    int bars = 0;     // since it last changed
    double gap = 0;   // fast minus slow on the last bar
};
// Supertrend's direction: negative while the trend is up, the convention of the indicator it comes from.
TrendReading readSupertrend(const QVector<double> &direction);
// EMA against EMA, MACD against its signal.
TrendReading readCross(const QVector<double> &fast, const QVector<double> &slow);

struct RsiReading {
    bool valid = false;
    double value = 0;
    int zone = 0;         // +1 overbought, -1 oversold, 0 between
    bool rising = false;  // against five bars before
};
RsiReading readRsi(const QVector<double> &rsi, double oversold, double overbought);

struct BandsReading {
    bool valid = false;
    double position = 0;    // 0 at the lower band, 100 at the upper: outside them, beyond
    double widthRank = 0;   // the bands' width among the last 126 bars, 0 the narrowest, 100 the widest
    bool squeeze = false;   // in the lowest fifth
};
BandsReading readBands(const QVector<double> &close, const QVector<double> &upper, const QVector<double> &lower);

struct DmiReading {
    bool valid = false;
    double adx = 0;
    int side = 0;         // +DI above -DI: +1
    bool strong = false;  // ADX at 25 or more, the usual threshold
};
DmiReading readDmi(const QVector<double> &adx, const QVector<double> &plus, const QVector<double> &minus);

} // namespace desk
