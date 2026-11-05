#include "IndicatorRead.h"

#include <algorithm>
#include <cmath>

namespace desk {

namespace {

bool lastNumber(const QVector<double> &v)
{
    return !v.isEmpty() && !std::isnan(v.last());
}

} // namespace

int barsOnSameSide(const QVector<double> &line, const QVector<double> &other)
{
    if (line.size() != other.size() || !lastNumber(line) || !lastNumber(other))
        return 0;
    const bool above = line.last() > other.last();
    int bars = 0;
    for (qsizetype i = line.size() - 1; i >= 0; --i) {
        if (std::isnan(line[i]) || std::isnan(other[i]) || (line[i] > other[i]) != above)
            break;
        ++bars;
    }
    return bars;
}

TrendReading readSupertrend(const QVector<double> &direction)
{
    TrendReading r;
    if (!lastNumber(direction) || direction.last() == 0)
        return r;
    const bool up = direction.last() < 0;
    r.side = up ? 1 : -1;
    for (qsizetype i = direction.size() - 1; i >= 0 && !std::isnan(direction[i]) && (direction[i] < 0) == up && direction[i] != 0; --i)
        ++r.bars;
    return r;
}

TrendReading readCross(const QVector<double> &fast, const QVector<double> &slow)
{
    TrendReading r;
    if (fast.size() != slow.size() || !lastNumber(fast) || !lastNumber(slow))
        return r;
    r.side = fast.last() > slow.last() ? 1 : -1;
    r.bars = barsOnSameSide(fast, slow);
    r.gap = fast.last() - slow.last();
    return r;
}

RsiReading readRsi(const QVector<double> &rsi, double oversold, double overbought)
{
    RsiReading r;
    if (!lastNumber(rsi))
        return r;
    r.valid = true;
    r.value = rsi.last();
    r.zone = r.value >= overbought ? 1 : r.value <= oversold ? -1 : 0;
    const qsizetype before = rsi.size() - 6;
    r.rising = before >= 0 && !std::isnan(rsi[before]) && r.value > rsi[before];
    return r;
}

BandsReading readBands(const QVector<double> &close, const QVector<double> &upper, const QVector<double> &lower)
{
    BandsReading r;
    if (close.size() != upper.size() || close.size() != lower.size() || !lastNumber(close) || !lastNumber(upper) || !lastNumber(lower) || upper.last() <= lower.last())
        return r;
    r.valid = true;
    r.position = (close.last() - lower.last()) / (upper.last() - lower.last()) * 100.0;
    QVector<double> widths;
    for (qsizetype i = std::max<qsizetype>(0, close.size() - 126); i < close.size(); ++i)
        if (!std::isnan(upper[i]) && !std::isnan(lower[i]))
            widths << upper[i] - lower[i];
    const double width = upper.last() - lower.last();
    const auto below = std::count_if(widths.begin(), widths.end(), [&](double w) { return w < width; });
    r.widthRank = widths.size() > 1 ? 100.0 * below / (widths.size() - 1) : 50;
    r.squeeze = widths.size() >= 20 && r.widthRank <= 20;
    return r;
}

DmiReading readDmi(const QVector<double> &adx, const QVector<double> &plus, const QVector<double> &minus)
{
    DmiReading r;
    if (!lastNumber(adx) || !lastNumber(plus) || !lastNumber(minus))
        return r;
    r.valid = true;
    r.adx = adx.last();
    r.side = plus.last() > minus.last() ? 1 : -1;
    r.strong = r.adx >= 25;
    return r;
}

} // namespace desk
