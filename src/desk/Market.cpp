#include "desk/Market.h"

#include <QHash>
#include <QTimeZone>

#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace desk {

// ------------------------------------------------------------------ currency strength

Strength currencyStrength(const QVector<CurrencySeries> &series)
{
    Strength out;
    if (series.size() < 2)
        return out;
    // Keep only the bars all the pairs have.
    std::map<qint64, int> seen;
    int pairs = 0;
    QVector<QHash<qint64, double>> values(series.size());
    for (int i = 0; i < series.size(); ++i) {
        if (series[i].dollarsPerUnit.isEmpty())
            continue;
        ++pairs;
        for (int k = 0; k < series[i].times.size() && k < series[i].dollarsPerUnit.size(); ++k) {
            const double v = series[i].dollarsPerUnit[k];
            if (v > 0) {
                values[i].insert(series[i].times[k], std::log(v));
                seen[series[i].times[k]]++;
            }
        }
    }
    for (const auto &[t, n] : seen)
        if (n == pairs)
            out.times.push_back(t);
    if (out.times.isEmpty())
        return out;

    const int currencies = series.size();
    for (int c = 0; c < currencies; ++c)
        out.lines.push_back({series[c].currency, QVector<double>(out.times.size())});
    for (int k = 0; k < out.times.size(); ++k) {
        QVector<double> logs(currencies, 0.0);   // a currency without a series is the numeraire: log 1 = 0
        double sum = 0;
        for (int c = 0; c < currencies; ++c) {
            if (!series[c].dollarsPerUnit.isEmpty())
                logs[c] = values[c].value(out.times[k]);
            sum += logs[c];
        }
        for (int c = 0; c < currencies; ++c)
            // The mean of (log c - log other) over the others.
            out.lines[c].index[k] = (logs[c] * currencies - sum) / (currencies - 1);
    }
    for (StrengthLine &line : out.lines) {
        const double base = line.index.first();
        for (double &v : line.index)
            v = 100.0 * std::exp(v - base);
    }
    return out;
}

QVector<std::pair<QString, double>> rankStrength(const Strength &s, int from)
{
    QVector<std::pair<QString, double>> out;
    for (const StrengthLine &l : s.lines) {
        if (l.index.isEmpty())
            continue;
        const int start = std::clamp<int>(from, 0, l.index.size() - 1);
        out.push_back({l.currency, (l.index.last() / l.index[start] - 1) * 100.0});
    }
    std::sort(out.begin(), out.end(), [](const auto &a, const auto &b) { return a.second > b.second; });
    return out;
}

// ------------------------------------------------------------------ technical signals

QVector<double> emaOf(const QVector<double> &values, int period)
{
    QVector<double> out(values.size());
    if (values.isEmpty())
        return out;
    const double k = 2.0 / (period + 1);
    out[0] = values[0];
    for (int i = 1; i < values.size(); ++i)
        out[i] = values[i] * k + out[i - 1] * (1 - k);
    return out;
}

double rsiOf(const QVector<double> &close, int period)
{
    if (close.size() <= period)
        return 50;
    double gain = 0, loss = 0;
    for (int i = 1; i <= period; ++i) {
        const double d = close[i] - close[i - 1];
        (d > 0 ? gain : loss) += std::abs(d);
    }
    gain /= period;
    loss /= period;
    for (int i = period + 1; i < close.size(); ++i) {
        const double d = close[i] - close[i - 1];
        gain = (gain * (period - 1) + std::max(d, 0.0)) / period;
        loss = (loss * (period - 1) + std::max(-d, 0.0)) / period;
    }
    if (loss == 0)
        return gain == 0 ? 50 : 100;
    return 100.0 - 100.0 / (1.0 + gain / loss);
}

namespace {

double stdDev(const QVector<double> &v, int from, int to)
{
    const int n = to - from;
    if (n < 2)
        return 0;
    double mean = 0, sum = 0;
    for (int i = from; i < to; ++i)
        mean += v[i];
    mean /= n;
    for (int i = from; i < to; ++i)
        sum += (v[i] - mean) * (v[i] - mean);
    return std::sqrt(sum / (n - 1));
}

double trueRange(const QVector<Candle> &b, int i)
{
    if (i == 0)
        return b[0].high - b[0].low;
    return std::max({b[i].high - b[i].low, std::abs(b[i].high - b[i - 1].close), std::abs(b[i].low - b[i - 1].close)});
}

double atrOf(const QVector<Candle> &b, int period = 14)
{
    if (b.isEmpty())
        return 0;
    const int from = std::max<int>(0, b.size() - period);
    double sum = 0;
    for (int i = from; i < b.size(); ++i)
        sum += trueRange(b, i);
    return sum / (b.size() - from);
}

// The swing structure of the last pivots: higher highs and higher lows, or the reverse.
Structure structureOf(const QVector<Candle> &b, int pivot = 3)
{
    QVector<double> highs, lows;
    for (int i = pivot; i < b.size() - pivot; ++i) {
        bool hi = true, lo = true;
        for (int k = 1; k <= pivot; ++k) {
            hi = hi && b[i].high >= b[i - k].high && b[i].high > b[i + k].high;
            lo = lo && b[i].low <= b[i - k].low && b[i].low < b[i + k].low;
        }
        if (hi) highs.push_back(b[i].high);
        if (lo) lows.push_back(b[i].low);
    }
    if (highs.size() < 2 || lows.size() < 2)
        return Structure::Ranging;
    const bool hh = highs.last() > highs[highs.size() - 2], hl = lows.last() > lows[lows.size() - 2];
    if (hh && hl)
        return Structure::Bullish;
    if (!hh && !hl)
        return Structure::Bearish;
    return Structure::Ranging;
}

} // namespace

Signals computeSignals(const QVector<Candle> &daily, const QVector<Candle> &volumeProxy)
{
    Signals s;
    if (daily.size() < 60)
        return s;
    QVector<double> close, ret;
    close.reserve(daily.size());
    for (const Candle &c : daily)
        close.push_back(c.close);
    for (int i = 1; i < close.size(); ++i)
        if (close[i - 1] > 0 && close[i] > 0)
            ret.push_back(std::log(close[i] / close[i - 1]) * 100.0);
    if (ret.size() < 30)
        return s;

    s.valid = true;
    s.last = close.last();
    s.changePct = close[close.size() - 2] > 0 ? (close.last() / close[close.size() - 2] - 1) * 100.0 : 0;

    const double fast = emaOf(close, 20).last(), slow = emaOf(close, 50).last();
    s.trendGapPct = slow != 0 ? (fast / slow - 1) * 100.0 : 0;
    const double vol20 = stdDev(ret, ret.size() - 20, ret.size());
    const double volAll = stdDev(ret, 0, ret.size());
    s.volAnnual = vol20 * std::sqrt(252.0);
    s.volBaseline = volAll * std::sqrt(252.0);
    // A trend is a gap larger than a fifth of a day-and-a-half's move, so a quiet pair and a wild coin read alike.
    const double neutralBand = std::max(0.05, vol20 * 0.3);
    s.trend = s.trendGapPct > neutralBand ? Trend::Bullish : s.trendGapPct < -neutralBand ? Trend::Bearish : Trend::Neutral;

    s.rsi = rsiOf(close);
    s.momentum = s.rsi >= 70 ? Momentum::Overbought : s.rsi <= 30 ? Momentum::Oversold : Momentum::Neutral;

    const double ratio = s.volBaseline > 0 ? s.volAnnual / s.volBaseline : 1;
    s.volatility = ratio > 1.25 ? Volatility::High : ratio < 0.8 ? Volatility::Low : Volatility::Normal;

    const QVector<Candle> &v = daily.last().volume > 0 ? daily : volumeProxy;
    if (v.size() >= 65 && v.last().volume > 0) {
        double recent = 0, before = 0;
        for (int i = v.size() - 5; i < v.size(); ++i)
            recent += v[i].volume;
        for (int i = v.size() - 65; i < v.size() - 5; ++i)
            before += v[i].volume;
        if (before > 0) {
            s.volumeRatio = (recent / 5) / (before / 60);
            s.volume = s.volumeRatio > 1.2 ? VolumeState::Above : s.volumeRatio < 0.8 ? VolumeState::Below : VolumeState::Average;
        }
    }

    s.structure = structureOf(daily);

    const Liquidity liq = findLiquidity(daily);
    const qint64 lastTime = daily.last().time;
    for (const Sweep &sw : liq.sweeps)
        if (lastTime - sw.time <= 3 * 86400) {
            s.liquidity = LiquidityState::Swept;
            s.liquidityDistancePct = (sw.price / s.last - 1) * 100.0;
            break;
        }
    if (s.liquidity == LiquidityState::None)
        for (const Pool &p : liq.pools)
            if (!p.swept) {
                const double atrs = liq.atr > 0 ? std::abs(p.price - s.last) / liq.atr : 99;
                s.liquidity = atrs <= 1 ? LiquidityState::Near : p.buySide ? LiquidityState::PoolAbove : LiquidityState::PoolBelow;
                s.liquidityDistancePct = p.distancePct;
                break;
            }

    const auto clamp = [](double v) { return std::clamp(v, 0.0, 100.0); };
    s.radar[0] = clamp(50 + s.trendGapPct / std::max(0.2, vol20) * 25);
    s.radar[1] = clamp(s.rsi);
    s.radar[2] = clamp(ratio * 50);
    s.radar[3] = s.volume == VolumeState::NoData ? 50 : clamp(s.volumeRatio * 50);
    s.radar[4] = s.structure == Structure::Bullish ? 85 : s.structure == Structure::Bearish ? 15 : 50;
    s.radar[5] = s.liquidity == LiquidityState::Swept ? 90 : s.liquidity == LiquidityState::Near ? 70 : s.liquidity == LiquidityState::None ? 20 : 40;
    return s;
}

// ------------------------------------------------------------------ stop pools

QVector<Sweep> sweepsSince(const QVector<Sweep> &sweeps, qint64 lastSeen)
{
    QVector<Sweep> out;
    for (const Sweep &s : sweeps)
        if (s.time > lastSeen)
            out.push_back(s);
    return out;
}

Liquidity findLiquidity(const QVector<Candle> &bars, int pivot, double tolerance)
{
    Liquidity out;
    if (bars.size() < pivot * 2 + 2)
        return out;
    out.atr = atrOf(bars);
    const double band = std::max(out.atr * tolerance, bars.last().close * 1e-5);
    const double last = bars.last().close;

    // Days, newest first: the previous day's extremes are levels everyone sees.
    std::map<QDate, SessionExtreme> days;
    for (const Candle &c : bars) {
        const QDate d = QDateTime::fromSecsSinceEpoch(c.time, QTimeZone::UTC).date();
        auto it = days.find(d);
        if (it == days.end())
            days.insert({d, {d, c.high, c.low}});
        else {
            it->second.high = std::max(it->second.high, c.high);
            it->second.low = std::min(it->second.low, c.low);
        }
    }
    for (auto it = days.rbegin(); it != days.rend(); ++it)
        out.days.push_back(it->second);
    if (out.days.size() >= 2) {
        out.previousDayHigh = out.days[1].high;
        out.previousDayLow = out.days[1].low;
    }

    // Swings; a swing near an existing pool of its side joins it.
    QVector<Pool> pools;
    for (int i = pivot; i < bars.size() - pivot; ++i) {
        bool hi = true, lo = true;
        for (int k = 1; k <= pivot; ++k) {
            hi = hi && bars[i].high >= bars[i - k].high && bars[i].high > bars[i + k].high;
            lo = lo && bars[i].low <= bars[i - k].low && bars[i].low < bars[i + k].low;
        }
        const auto addSwing = [&](bool buySide, double price) {
            for (Pool &p : pools)
                if (p.buySide == buySide && !p.swept && std::abs(p.price - price) <= band) {
                    p.price = (p.price * p.touches + price) / (p.touches + 1);
                    p.touches++;
                    p.lastTime = bars[i].time;
                    return;
                }
            Pool p;
            p.buySide = buySide;
            p.price = price;
            p.touches = 1;
            p.firstTime = p.lastTime = bars[i].time;
            pools.push_back(p);
        };
        if (hi) addSwing(true, bars[i].high);
        if (lo) addSwing(false, bars[i].low);
        // A later bar trading through an untested pool sweeps it: a wick beyond, or a close beyond.
        for (Pool &p : pools) {
            if (p.swept || bars[i].time <= p.lastTime)
                continue;
            if (p.buySide ? bars[i].high > p.price : bars[i].low < p.price) {
                p.swept = true;
                p.sweptTime = bars[i].time;
            }
        }
    }
    // The last `pivot` bars can't form swings but can still sweep.
    for (int i = std::max(0, int(bars.size()) - pivot); i < bars.size(); ++i)
        for (Pool &p : pools)
            if (!p.swept && bars[i].time > p.lastTime && (p.buySide ? bars[i].high > p.price : bars[i].low < p.price)) {
                p.swept = true;
                p.sweptTime = bars[i].time;
            }

    for (Pool &p : pools) {
        p.distancePct = last > 0 ? (p.price / last - 1) * 100.0 : 0;
        if (p.swept)
            out.sweeps.push_back({p.sweptTime, p.buySide, p.price, p.touches});
        else
            ++out.untested;
    }
    std::sort(out.sweeps.begin(), out.sweeps.end(), [](const Sweep &a, const Sweep &b) { return a.time > b.time; });
    std::sort(pools.begin(), pools.end(), [&](const Pool &a, const Pool &b) {
        if (a.swept != b.swept)
            return !a.swept;
        return std::abs(a.price - last) < std::abs(b.price - last);
    });
    out.pools = pools;

    double above = -1, below = -1;
    for (const Pool &p : pools) {
        if (p.swept)
            continue;
        if (p.buySide && p.price > last && above < 0)
            above = p.price - last;
        if (!p.buySide && p.price < last && below < 0)
            below = last - p.price;
    }
    const double nearest = above < 0 ? below : below < 0 ? above : std::min(above, below);
    out.stopRunBuySideCloser = above >= 0 && (below < 0 || above <= below);
    if (nearest >= 0 && out.atr > 0)
        out.stopRun = nearest <= 0.5 * out.atr ? Liquidity::High : nearest <= 1.5 * out.atr ? Liquidity::Moderate : Liquidity::Low;
    return out;
}

// ------------------------------------------------------------------ seasonality

Seasonal computeSeasonal(const QVector<Candle> &daily, const SeasonalOptions &o, int currentYear)
{
    Seasonal s;
    if (daily.size() < 200)
        return s;
    struct Day {
        QDate date;
        double ret;
    };
    QVector<Day> days;
    for (int i = 1; i < daily.size(); ++i) {
        if (daily[i - 1].close <= 0 || daily[i].close <= 0)
            continue;
        const QDate d = QDateTime::fromSecsSinceEpoch(daily[i].time, QTimeZone::UTC).date();
        if (d.year() < currentYear - o.years || d.year() >= currentYear)
            continue;
        days.push_back({d, (daily[i].close / daily[i - 1].close - 1) * 100.0});
    }
    if (days.size() < 100)
        return s;
    const auto inPeriod = [&](const QDate &d) { return d.month() >= o.firstMonth && d.month() <= o.lastMonth; };

    // The return of the period, year by year: its days compounded.
    std::map<int, double> compounded;
    for (const Day &d : days)
        if (inPeriod(d.date)) {
            auto it = compounded.find(d.date.year());
            if (it == compounded.end())
                compounded.insert({d.date.year(), 1 + d.ret / 100.0});
            else
                it->second *= 1 + d.ret / 100.0;
        }
    for (const auto &[year, c] : compounded)
        s.years.push_back({year, (c - 1) * 100.0, false});
    if (s.years.size() < 2)
        return s;

    double mean = 0, sq = 0;
    for (const SeasonalYear &y : s.years)
        mean += y.periodReturn;
    mean /= s.years.size();
    for (const SeasonalYear &y : s.years)
        sq += (y.periodReturn - mean) * (y.periodReturn - mean);
    const double sd = std::sqrt(sq / (s.years.size() - 1));
    std::set<int> excluded;
    if (o.excludeOutliers && sd > 0)
        for (SeasonalYear &y : s.years)
            if (std::abs(y.periodReturn - mean) > 1.5 * sd) {
                y.excluded = true;
                excluded.insert(y.year);
            }

    double sum = 0;
    int used = 0;
    for (const SeasonalYear &y : s.years) {
        if (y.excluded)
            continue;
        ++used;
        sum += y.periodReturn;
        (y.periodReturn > 0 ? s.positive : s.negative)++;
    }
    if (!used)
        return s;
    s.meanReturn = sum / used;
    s.winRate = 100.0 * s.positive / used;

    // Per year and month: compounded, then the mean per month. Per weekday: the mean of the days.
    std::map<std::pair<int, int>, double> months;
    double weekdaySum[5] = {};
    int weekdayCount[5] = {};
    QVector<double> dayReturn(366, 0.0);
    QVector<int> dayCount(366, 0);
    QVector<double> periodReturns, allReturns;
    for (const Day &d : days) {
        allReturns.push_back(d.ret);
        if (excluded.count(d.date.year()))
            continue;
        auto key = std::pair(d.date.year(), d.date.month());
        auto it = months.find(key);
        if (it == months.end())
            months.insert({key, 1 + d.ret / 100.0});
        else
            it->second *= 1 + d.ret / 100.0;
        if (!inPeriod(d.date))
            continue;
        periodReturns.push_back(d.ret);
        const int wd = d.date.dayOfWeek() - 1;
        if (wd < 5) {
            weekdaySum[wd] += d.ret;
            weekdayCount[wd]++;
        }
    }
    for (const Day &d : days) {
        if (excluded.count(d.date.year()))
            continue;
        const int doy = std::min(d.date.dayOfYear(), 366) - 1;
        dayReturn[doy] += d.ret;
        dayCount[doy]++;
    }
    for (int i = 0; i < 5; ++i)
        s.byWeekday[i] = weekdayCount[i] ? weekdaySum[i] / weekdayCount[i] : 0;
    double monthSum[12] = {};
    int monthCount[12] = {};
    for (const auto &[key, c] : months) {
        monthSum[key.second - 1] += (c - 1) * 100.0;
        monthCount[key.second - 1]++;
    }
    bool first = true;
    for (int i = 0; i < 12; ++i) {
        s.byMonth[i] = monthCount[i] ? monthSum[i] / monthCount[i] : 0;
        if (i + 1 < o.firstMonth || i + 1 > o.lastMonth)
            continue;
        if (first) {
            s.bestMonth = s.worstMonth = i;
            first = false;
        }
        if (s.byMonth[i] > s.byMonth[s.bestMonth]) s.bestMonth = i;
        if (s.byMonth[i] < s.byMonth[s.worstMonth]) s.worstMonth = i;
    }

    // The average path: the mean return of each calendar day, summed, then a light 3-day smoothing for reading.
    QVector<double> raw(366);
    double running = 0;
    for (int i = 0; i < 366; ++i) {
        running += dayCount[i] ? dayReturn[i] / used : 0;
        raw[i] = running;
    }
    s.path = raw;
    for (int i = 1; i < 365; ++i)
        s.path[i] = (raw[i - 1] + raw[i] + raw[i + 1]) / 3.0;

    s.volatilityPeriod = stdDev(periodReturns, 0, periodReturns.size());
    s.volatilityAll = stdDev(allReturns, 0, allReturns.size());
    s.volatilityRatio = s.volatilityAll > 0 ? s.volatilityPeriod / s.volatilityAll : 1;
    s.valid = true;
    return s;
}

// ------------------------------------------------------------------ risk appetite

AppetiteZone zoneOf(double v)
{
    if (v < 20) return AppetiteZone::ExtremeOff;
    if (v < 40) return AppetiteZone::Off;
    if (v <= 60) return AppetiteZone::Neutral;
    if (v <= 80) return AppetiteZone::On;
    return AppetiteZone::ExtremeOn;
}

double percentileOf(const QVector<double> &values, int index, int window)
{
    const int from = std::max(0, index - window);
    int below = 0, count = 0;
    for (int i = from; i < index; ++i) {
        if (std::isnan(values[i]))
            continue;
        ++count;
        if (values[i] < values[index])
            ++below;
    }
    return count ? 100.0 * below / count : 50;
}

Appetite computeAppetite(const QVector<QDate> &dates, QVector<AppetiteComponent> components)
{
    Appetite a;
    if (dates.isEmpty() || components.isEmpty())
        return a;
    const int n = dates.size();
    // Only components with a real measure take part: one missing doesn't drag the composite to its neutral value.
    QVector<AppetiteComponent> usable;
    for (AppetiteComponent &c : components)
        if (c.raw.size() == n)
            usable.push_back(c);
    if (usable.isEmpty())
        return a;
    const int warmup = std::min(252, std::max(20, n / 2));
    for (int i = warmup; i < n; ++i) {
        double sum = 0;
        int count = 0;
        for (const AppetiteComponent &c : usable) {
            if (std::isnan(c.raw[i]))
                continue;
            const double p = percentileOf(c.raw, i);
            sum += c.invert ? 100.0 - p : p;
            ++count;
        }
        if (count) {
            a.dates.push_back(dates[i]);
            a.history.push_back(sum / count);
        }
    }
    if (a.history.isEmpty())
        return a;
    for (AppetiteComponent &c : usable) {
        int last = n - 1;
        while (last > 0 && std::isnan(c.raw[last]))
            --last;
        c.value = c.raw[last];
        const double p = percentileOf(c.raw, last);
        c.percentile = c.invert ? 100.0 - p : p;
    }
    a.components = usable;
    a.now = a.history.last();
    a.weekAgo = a.history[std::max<qsizetype>(0, a.history.size() - 6)];
    a.monthAgo = a.history[std::max<qsizetype>(0, a.history.size() - 22)];
    a.valid = true;
    return a;
}

} // namespace desk
