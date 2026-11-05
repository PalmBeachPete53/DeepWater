#include "desk/Rates.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <algorithm>
#include <cmath>

namespace desk {

QVector<QDate> parseFomcCalendar(const QString &html)
{
    static const QHash<QString, int> months {{"january", 1}, {"february", 2}, {"march", 3}, {"april", 4}, {"may", 5}, {"june", 6}, {"july", 7},
                                             {"august", 8}, {"september", 9}, {"october", 10}, {"november", 11}, {"december", 12},
                                             {"jan", 1}, {"feb", 2}, {"mar", 3}, {"apr", 4}, {"jun", 6}, {"jul", 7}, {"aug", 8},
                                             {"sep", 9}, {"sept", 9}, {"oct", 10}, {"nov", 11}, {"dec", 12}};
    QVector<QDate> out;
    static const QRegularExpression year("(\\d{4}) FOMC Meetings");
    static const QRegularExpression row("fomc-meeting__month[^>]*>\\s*<strong>\\s*([A-Za-z]+)(?:/([A-Za-z]+))?\\s*</strong>.*?"
                                        "fomc-meeting__date[^>]*>\\s*(\\d+)-(\\d+)",
                                        QRegularExpression::DotMatchesEverythingOption);
    QVector<std::pair<int, int>> sections;   // position, year
    auto it = year.globalMatch(html);
    while (it.hasNext()) {
        const auto m = it.next();
        sections.push_back({int(m.capturedStart()), m.captured(1).toInt()});
    }
    for (int s = 0; s < sections.size(); ++s) {
        const int from = sections[s].first, to = s + 1 < sections.size() ? sections[s + 1].first : int(html.size());
        // A single match per row: an unscheduled call has one day and no dash, and is never paired with the next row's date.
        auto rows = row.globalMatchView(QStringView(html).mid(from, to - from));
        while (rows.hasNext()) {
            const auto m = rows.next();
            const QString month = (m.captured(2).isEmpty() ? m.captured(1) : m.captured(2)).toLower();
            const QDate d(sections[s].second, months.value(month), m.captured(4).toInt());
            if (d.isValid())
                out.push_back(d);
        }
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

QString fedFutureSymbol(const QDate &month)
{
    static const char codes[] = "FGHJKMNQUVXZ";
    return QString("ZQ%1%2.CBT").arg(QChar(codes[month.month() - 1])).arg(month.year() % 100, 2, 10, QChar('0'));
}

void splitMove(MeetingPricing &p)
{
    const double steps = std::clamp(p.changeBp / 25.0, -2.0, 2.0);
    const double lower = std::floor(steps);
    const double weight = steps - lower;
    p.probability[int(lower) + 2] += (1 - weight) * 100.0;
    if (lower < 2)
        p.probability[int(lower) + 3] += weight * 100.0;
    p.pCut = p.probability[0] + p.probability[1];
    p.pHold = p.probability[2];
    p.pHike = p.probability[3] + p.probability[4];
}

FedPath fedPath(const QVector<FedContract> &contracts, const QVector<QDate> &meetings, const QDate &today)
{
    FedPath out;
    QHash<int, double> byMonth;
    const auto key = [](const QDate &d) { return d.year() * 12 + d.month(); };
    for (const FedContract &c : contracts)
        if (c.price > 0)
            byMonth.insert(key(c.month), 100.0 - c.price);
    if (byMonth.isEmpty())
        return out;

    QVector<QDate> future;
    for (const QDate &m : meetings)
        if (m >= today)
            future.push_back(m);

    // The rate today: the first month with a contract and no meeting left in it, else the average of this one.
    const int thisMonth = key(today);
    double pre = byMonth.value(thisMonth, -1);
    if (pre < 0) {
        QList<int> keys = byMonth.keys();
        std::sort(keys.begin(), keys.end());
        pre = byMonth.value(keys.first());
    }
    if (!future.isEmpty() && key(future.first()) == thisMonth) {
        // A meeting this month: what the month's average says about the rate before it is unknowable from it alone, so
        // the rate before it is read from the month before when there is one.
        pre = byMonth.value(thisMonth - 1, pre);
    }
    out.currentRate = pre;

    double cumulative = 0;
    for (const QDate &m : future) {
        const int k = key(m);
        if (!byMonth.contains(k))
            break;
        const int days = m.daysInMonth(), day = m.day();
        double post;
        const bool nextClear = byMonth.contains(k + 1)
            && std::none_of(future.begin(), future.end(), [&](const QDate &o) { return key(o) == k + 1; });
        // Late in a month the month's average says little about the rate after: the next month's contract does better.
        if (nextClear && days - day < 10)
            post = byMonth.value(k + 1);
        else if (days - day <= 0)
            post = byMonth.value(k + 1, byMonth.value(k));
        else
            post = (byMonth.value(k) * days - pre * day) / (days - day);

        MeetingPricing p;
        p.date = m;
        p.rateBefore = pre;
        p.rateAfter = post;
        p.changeBp = (post - pre) * 100.0;
        cumulative += p.changeBp;
        p.cumulativeBp = cumulative;

        splitMove(p);
        out.meetings.push_back(p);
        pre = post;
    }
    return out;
}

double rangePosition(const QVector<double> &nets, int window)
{
    if (nets.isEmpty())
        return -1;
    const int from = std::max<int>(0, nets.size() - window);
    const auto [lo, hi] = std::minmax_element(nets.begin() + from, nets.end());
    return *hi > *lo ? (nets.last() - *lo) / (*hi - *lo) * 100.0 : 50;
}

QVector<TffWeek> parseTff(const QByteArray &json)
{
    QVector<TffWeek> out;
    const auto num = [](const QJsonObject &o, const char *k) { return o.value(k).toString().toLongLong(); };
    for (const QJsonValue &v : QJsonDocument::fromJson(json).array()) {
        const QJsonObject o = v.toObject();
        TffWeek w;
        w.date = QDate::fromString(o.value("report_date_as_yyyy_mm_dd").toString().left(10), Qt::ISODate);
        if (!w.date.isValid())
            continue;
        w.openInterest = num(o, "open_interest_all");
        w.dealerLong = num(o, "dealer_positions_long_all");
        w.dealerShort = num(o, "dealer_positions_short_all");
        w.assetLong = num(o, "asset_mgr_positions_long");
        w.assetShort = num(o, "asset_mgr_positions_short");
        w.levLong = num(o, "lev_money_positions_long");
        w.levShort = num(o, "lev_money_positions_short");
        w.otherLong = num(o, "other_rept_positions_long");
        w.otherShort = num(o, "other_rept_positions_short");
        w.nonReptLong = num(o, "nonrept_positions_long_all");
        w.nonReptShort = num(o, "nonrept_positions_short_all");
        out.push_back(w);
    }
    std::sort(out.begin(), out.end(), [](const TffWeek &a, const TffWeek &b) { return a.date < b.date; });
    return out;
}

QString estrFutureSymbol(const QDate &month)
{
    static const QHash<int, QChar> codes {{3, 'H'}, {6, 'M'}, {9, 'U'}, {12, 'Z'}};
    return QString("ESR%1%2.CME").arg(codes.value(month.month())).arg(month.year() % 100, 2, 10, QLatin1Char('0'));
}

QVector<QDate> nextQuarterMonths(const QDate &today, int count)
{
    QVector<QDate> out;
    // A quarter's contract covers about three months from the third Wednesday of its month.
    for (QDate m(today.year(), today.month(), 1); out.size() < count; m = m.addMonths(1))
        if (m.month() % 3 == 0 && (m > today || today.day() < 15))
            out << m;
    return out;
}

} // namespace desk
