// Seasonals: how a symbol moved through each calendar year, from daily bars (UTC dates).
// Each year's path is measured from the previous year's last close, so 1 January's move counts.
// Port of quant-terminal/public/lib/seasonals.js.
#include "Seasonals.h"

#include "JsCompat.h"

#include <QDateTime>
#include <QJsonArray>

#include <algorithm>
#include <map>
#include <optional>

#if defined(__clang__)
#pragma clang fp contract(off)
#endif

namespace qtm {

namespace {

double dayOfYear(double t)
{
    const js::UtcParts d = js::utcParts(t * 1000);
    return std::floor((js::dateUtc(d.year, d.month, d.day) - js::dateUtc(d.year, 0, 1)) / 86400000) + 1;
}

QJsonValue num(std::optional<double> v)
{
    return v ? QJsonValue(*v) : QJsonValue(QJsonValue::Null);
}

} // namespace

QJsonObject seasonals(const Bars &bars)
{
    // Map keyed by year; the JS sorts the keys numerically afterwards, which is the map's order.
    std::map<double, Bars> byYear;
    for (const Bar &b : bars) byYear[js::utcParts(b.time * 1000).year].push_back(b);
    QVector<double> yearsList;
    for (const auto &kv : byYear) yearsList.push_back(kv.first);
    const double thisYear = double(QDateTime::currentDateTimeUtc().date().year());

    struct Point {
        double doy, pct;
    };
    struct Year {
        double year;
        QVector<Point> points;
        bool complete;
    };
    QVector<Year> years;
    QJsonArray yearsJson, monthlyJson;
    QVector<std::array<std::optional<double>, 12>> monthlyMonths;
    for (int k = 0; k < yearsList.size(); ++k) {
        const double y = yearsList[k];
        const Bars &rows = byYear[y];
        std::optional<double> prev;
        if (k > 0 && yearsList[k - 1] == y - 1) prev = byYear[y - 1].last().close;
        const double base = prev ? *prev : rows[0].open;
        QVector<Point> points;
        for (const Bar &b : rows) points.push_back({ dayOfYear(b.time), (b.close / base - 1) * 100 });
        // complete = we hold the whole year (starts within the first week, and it's not the current year)
        const bool complete = y != thisYear && prev.has_value() && dayOfYear(rows.last().time) > 350;
        years.push_back({ y, points, complete });
        QJsonArray pts;
        for (const Point &p : points) pts.append(QJsonObject { { "doy", p.doy }, { "pct", p.pct } });
        const QJsonValue total = points.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(points.last().pct);
        yearsJson.append(QJsonObject { { "year", y }, { "points", pts }, { "total", total }, { "complete", complete } });

        std::array<std::optional<double>, 12> months;
        double last = base;
        for (int m = 0; m < 12; ++m) {
            const Bar *lastIn = nullptr;
            for (const Bar &b : rows)
                if (js::utcParts(b.time * 1000).month == m) lastIn = &b;
            if (!lastIn) continue;
            const double close = lastIn->close;
            months[m] = (close / last - 1) * 100;
            last = close;
        }
        monthlyMonths.push_back(months);
        QJsonArray ms;
        for (const auto &v : months) ms.append(num(v));
        monthlyJson.append(QJsonObject { { "year", y }, { "months", ms }, { "total", total } });
    }

    // Average path over complete years: each year sampled per day of year, carrying the last value
    // over weekends and holidays.
    QVector<const Year *> done;
    for (const Year &y : years)
        if (y.complete) done.push_back(&y);
    QJsonArray average;
    if (!done.isEmpty()) {
        QVector<QVector<double>> sampled;
        for (const Year *y : done) {
            QVector<double> out(367, 0);
            int i = 0;
            double cur = 0;
            for (int d = 1; d <= 366; ++d) {
                while (i < y->points.size() && y->points[i].doy <= d) cur = y->points[i++].pct;
                out[d] = cur;
            }
            sampled.push_back(out);
        }
        for (int d = 1; d <= 366; ++d) {
            double s = 0;
            for (const auto &x : sampled) s += x[d];
            average.append(QJsonObject { { "doy", d }, { "pct", s / sampled.size() } });
        }
    }
    QJsonArray monthAvg;
    for (int m = 0; m < 12; ++m) {
        QVector<double> vals;
        for (const auto &r : monthlyMonths)
            if (r[m]) vals.push_back(*r[m]);
        if (vals.isEmpty()) {
            monthAvg.append(QJsonValue(QJsonValue::Null));
            continue;
        }
        double s = 0;
        int up = 0;
        for (double v : vals) {
            s += v;
            if (v > 0) up++;
        }
        monthAvg.append(QJsonObject { { "avg", s / vals.size() },
                                      { "upPct", (double(up) / vals.size()) * 100 },
                                      { "n", vals.size() } });
    }
    return QJsonObject { { "years", yearsJson }, { "monthly", monthlyJson }, { "average", average },
                         { "monthAvg", monthAvg }, { "completeYears", done.size() } };
}

} // namespace qtm
