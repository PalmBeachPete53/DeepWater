#include "fresh/Fresh.h"

#include <QSet>

#include <algorithm>
#include <limits>

namespace fresh {

qint64 checkEverySecs(qint64 staleSecs, bool conditionalWorks, qint64 lastBytes)
{
    if (conditionalWorks)
        return std::min(staleSecs, LightCheckSecs);
    if (lastBytes >= 0 && lastBytes <= SmallAnswerBytes)
        return std::min(staleSecs, SmallCheckSecs);
    return staleSecs;
}

Conditional judgeConditional(const Validators &sent, int status, const Validators &received)
{
    if (!sent.any())
        return Conditional::Unknown;
    if (status == 304)
        return Conditional::Works;
    // The same validators with the whole body: it was asked "unless this is still it" and sent it anyway. New ones
    // say the data changed, which proves nothing.
    if (status == 200 && received == sent)
        return Conditional::Ignored;
    return Conditional::Unknown;
}

bool futuresMarketOpen(const QDateTime &utc)
{
    const QDateTime t = utc.toUTC();
    const int day = t.date().dayOfWeek();   // 1 Monday ... 7 Sunday
    const int hour = t.time().hour();
    if (day == 6)
        return false;
    if (day == 5)
        return hour < 22;
    if (day == 7)
        return hour >= 22;
    return true;
}

qint64 futuresEverySecs(const QDateTime &utc)
{
    return futuresMarketOpen(utc) ? 60 : 15 * 60;
}

QStringList takeDue(QVector<Job> &jobs, const QDateTime &now, int max, qint64 minGapSecs)
{
    struct Late {
        int index;
        qint64 late;
    };
    QVector<Late> due;
    for (int i = 0; i < jobs.size(); ++i) {
        const Job &j = jobs[i];
        if (j.lastAttempt.isValid() && j.lastAttempt.secsTo(now) < std::min(minGapSecs, j.everySecs))
            continue;
        // A job that reports no answers of its own runs on its attempts.
        const QDateTime from = j.last.isValid() ? j.last : j.lastAttempt;
        const qint64 late = from.isValid() ? from.secsTo(now) - j.everySecs : std::numeric_limits<qint64>::max() / 2;
        if (late >= 0)
            due.push_back({i, late});
    }
    std::stable_sort(due.begin(), due.end(), [](const Late &a, const Late &b) { return a.late > b.late; });
    QStringList out;
    for (int i = 0; i < due.size() && i < max; ++i) {
        jobs[due[i].index].lastAttempt = now;
        out << jobs[due[i].index].id;
    }
    return out;
}

WatchPlan planWatch(const QVector<Release> &releases, const QDateTime &now)
{
    WatchPlan plan;
    QSet<QString> countries;
    for (const Release &r : releases) {
        if (r.hasActual || !r.time.isValid())
            continue;
        const qint64 sinceDue = r.time.secsTo(now);
        if (sinceDue < -60 || sinceDue > WatchMinutes * 60)
            continue;
        countries.insert(r.country);
        const int every = r.impact >= 2 ? 10 : 30;
        plan.pollSecs = plan.pollSecs == 0 ? every : std::min(plan.pollSecs, every);
    }
    plan.countries = countries.values();
    plan.countries.sort();
    return plan;
}

namespace {

QString keyOf(const Release &r)
{
    return QString::number(r.time.toSecsSinceEpoch()) + '|' + r.country + '|' + r.title;
}

} // namespace

QVector<Release> newActuals(const QVector<Release> &before, const QVector<Release> &after)
{
    QSet<QString> had;
    for (const Release &r : before)
        if (r.hasActual)
            had.insert(keyOf(r));
    QVector<Release> out;
    for (const Release &r : after)
        if (r.hasActual && !had.contains(keyOf(r)))
            out.push_back(r);
    return out;
}

const QVector<qint64> &followUpDelays()
{
    static const QVector<qint64> delays {0, 2 * 60, 10 * 60, 60 * 60, 3 * 3600};
    return delays;
}

void FollowUps::add(const Release &release, const QDateTime &seen)
{
    if (release.impact < 1)
        return;
    for (const qint64 delay : followUpDelays())
        m_list.push_back({release.country, release.title, seen, seen.addSecs(delay)});
}

QVector<FollowUp> FollowUps::takeDue(const QDateTime &now)
{
    QVector<FollowUp> due;
    QSet<QString> seen;
    for (int i = 0; i < m_list.size();) {
        if (m_list[i].at > now) {
            ++i;
            continue;
        }
        const QString key = m_list[i].country + '|' + m_list[i].title;
        if (!seen.contains(key)) {
            seen.insert(key);
            due.push_back(m_list[i]);
        }
        m_list.removeAt(i);
    }
    return due;
}

Targets targetsFor(const QString &country, const QString &title)
{
    static const QHash<QString, QStringList> byCountry {
        {"US", {"fred", "bls", "bea"}},
        {"EU", {"eurostat", "ecb"}},
        {"DE", {"bbk", "eurostat"}},
        {"FR", {"insee", "eurostat"}},
        {"IT", {"eurostat"}},
        {"ES", {"ine", "eurostat"}},
        {"GB", {"ons", "boe"}},
        {"JP", {"estat", "boj"}},
        {"CA", {"statcan", "boc"}},
        {"AU", {"abs"}},
        {"CH", {"snb"}},
        {"KR", {"ecos"}},
        {"MX", {"banxico", "inegi"}},
        {"BR", {"bcb"}},
        {"TR", {"evds"}},
        {"ID", {"bps"}},
        {"SE", {"pxweb"}},
        {"NO", {"pxweb"}},
        {"FI", {"pxweb"}},
        {"DK", {"pxweb"}},
    };
    Targets t;
    t.groups = byCountry.value(country);
    const QString lower = title.toLower();
    t.policyRates = lower.contains("rate decision") || lower.contains("interest rate") || lower.contains("policy rate")
                    || lower.contains("fed funds") || lower.contains("cash rate");
    if (t.policyRates && !t.groups.contains("bis"))
        t.groups << "bis";
    if (country == "US" && (lower.contains("crude") || lower.contains("gasoline") || lower.contains("distillate") || lower.contains("natural gas")))
        t.groups << "eia";
    return t;
}

} // namespace fresh
