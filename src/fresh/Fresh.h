#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

// How often to ask a source for something new, and when a published figure calls for asking at once. Official
// statistics have no push: an app can only ask, so it asks little where a check costs nothing or where nothing can
// have changed, and fast around the minutes a figure is due. No network here: RefreshScheduler, ReleaseWatcher,
// RemoteDataSet and research::Client act on these answers. Tested by tests/quant/fresh.
namespace fresh {

// Between two checks of a source that answers "nothing new" without sending its data again (HTTP 304).
constexpr qint64 LightCheckSecs = 15 * 60;

// What a server gave to recognise its answer later.
struct Validators {
    QByteArray etag;
    QByteArray lastModified;
    bool any() const { return !etag.isEmpty() || !lastModified.isEmpty(); }
    bool operator==(const Validators &o) const { return etag == o.etag && lastModified == o.lastModified; }
};

// Between two checks of a source whose answer is small (most servers give no way to check for free): it costs little,
// and an answer already known is neither parsed nor drawn again.
constexpr qint64 SmallCheckSecs = 3600;
constexpr qint64 SmallAnswerBytes = 512 * 1024;

// A data set's period between two checks: its own, unless its server has shown that a check is free, or its last answer
// was small. `lastBytes` < 0: not known yet.
qint64 checkEverySecs(qint64 staleSecs, bool conditionalWorks, qint64 lastBytes = -1);

// Whether a server took a conditional request into account. `sent` empty: nothing to learn.
enum class Conditional { Unknown, Works, Ignored };
Conditional judgeConditional(const Validators &sent, int status, const Validators &received);

// CME's week: from Sunday 22:00 UTC to Friday 22:00 UTC, the hour after Friday's close counted as closed. The daily
// pause isn't worth a rule.
bool futuresMarketOpen(const QDateTime &utc);
// A futures quote's period: a minute while they trade, a quarter of an hour at the weekend (nothing moves).
qint64 futuresEverySecs(const QDateTime &utc);

// ---------------------------------------------------------------- one timer for every source

struct Job {
    QString id;
    qint64 everySecs = 3600;
    QDateTime last;          // the last time this source answered; invalid: never, or not told
    QDateTime lastAttempt;   // the last time it was asked to check
};

// The jobs due now, the longest overdue first, at most `max` so that a pass never asks everything at once. Marks
// them attempted. A job that doesn't answer waits `minGapSecs` before it is asked again.
QStringList takeDue(QVector<Job> &jobs, const QDateTime &now, int max, qint64 minGapSecs = 30);

// ---------------------------------------------------------------- figures being published

struct Release {
    QDateTime time;     // UTC
    QString country;    // ISO alpha-2, "EU" for the euro area
    QString title;
    int impact = 0;     // 0 low, 1 medium, 2 high
    bool hasActual = false;
};

struct WatchPlan {
    int pollSecs = 0;       // 0: nothing is due, the calendar's own period applies
    QStringList countries;  // the countries to ask about, sorted
};

// From a minute before a figure is due until it comes or WatchMinutes have passed without it. High impact every
// 10 s, others every 30 s.
constexpr int WatchMinutes = 20;
WatchPlan planWatch(const QVector<Release> &releases, const QDateTime &now);

// The figures that have an actual in `after` and didn't in `before`; the same figure is the same time, country and
// title.
QVector<Release> newActuals(const QVector<Release> &before, const QVector<Release> &after);

// ---------------------------------------------------------------- what a published figure makes stale

// The official series follows the calendar by seconds or hours, depending on the publisher: asked again at these
// delays after the figure, then left to its usual period.
const QVector<qint64> &followUpDelays();

struct FollowUp {
    QString country;
    QString title;
    QDateTime release;     // when the figure was seen
    QDateTime at;          // when to ask again
};

class FollowUps
{
public:
    // Medium and high impact only: a low impact figure isn't worth the requests.
    void add(const Release &release, const QDateTime &seen);
    // The follow-ups due, each country and title once, removed from the list.
    QVector<FollowUp> takeDue(const QDateTime &now);
    int size() const { return int(m_list.size()); }

private:
    QVector<FollowUp> m_list;
};

// What to ask again for a figure of that country: research::Client's groups, the first part of a request's key. The
// OECD and the IMF aren't in it: they republish days or weeks later.
struct Targets {
    QStringList groups;
    bool policyRates = false;   // a central bank's decision
};
Targets targetsFor(const QString &country, const QString &title);

} // namespace fresh
