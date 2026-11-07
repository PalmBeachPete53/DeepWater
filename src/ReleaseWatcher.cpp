#include "ReleaseWatcher.h"

#include "CalendarData.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QStandardPaths>
#include <QTextStream>

#include <cmath>

namespace {

ReleaseWatcher *s_instance = nullptr;

constexpr int StartupLookBackHours = 12;   // at launch, the figures of that long ago can have made a saved copy stale
constexpr qint64 LogLimit = 256 * 1024;

QVector<fresh::Release> releasesOf(const QVector<EconomicCalendar::Event> &events)
{
    QVector<fresh::Release> out;
    out.reserve(events.size());
    for (const EconomicCalendar::Event &e : events)
        out.push_back({e.time, e.country, e.title, e.impact, !std::isnan(e.actual)});
    return out;
}

} // namespace

ReleaseWatcher::ReleaseWatcher(EconomicCalendar *calendar, QObject *parent)
    : QObject(parent)
    , m_calendar(calendar)
{
    s_instance = this;
    connect(calendar, &RemoteDataSet::updated, this, &ReleaseWatcher::calendarChanged);
    connect(&m_timer, &QTimer::timeout, this, &ReleaseWatcher::tick);
    m_timer.start(2000);
    // Once whoever made this has connected: the first pass says what came out while the app was closed.
    QTimer::singleShot(0, this, &ReleaseWatcher::calendarChanged);
}

ReleaseWatcher::~ReleaseWatcher()
{
    if (s_instance == this)
        s_instance = nullptr;
}

ReleaseWatcher *ReleaseWatcher::instance()
{
    return s_instance;
}

void ReleaseWatcher::calendarChanged()
{
    if (!m_calendar->hasData())
        return;
    const QVector<fresh::Release> now = releasesOf(m_calendar->events());
    const QDateTime seen = QDateTime::currentDateTimeUtc();
    if (!m_started) {
        m_started = true;
        // Whatever came out while the app was closed: only what was saved before it is stale, so nothing is asked
        // again on a launch that follows another by a few minutes.
        QSet<QString> asked;
        for (const fresh::Release &r : now) {
            if (!r.hasActual || r.impact < 1)
                continue;
            const qint64 age = r.time.secsTo(seen);
            if (age < 0 || age > StartupLookBackHours * 3600)
                continue;
            const QString key = r.country + '|' + r.title;
            if (asked.contains(key))
                continue;
            asked.insert(key);
            emit askAgain(r.country, r.title, r.time);
        }
    } else {
        for (const fresh::Release &r : fresh::newActuals(m_known, now)) {
            note(r, seen);
            m_followUps.add(r, seen);
        }
    }
    m_known = now;
}

void ReleaseWatcher::tick()
{
    const QDateTime now = QDateTime::currentDateTimeUtc();
    for (const fresh::FollowUp &f : m_followUps.takeDue(now))
        emit askAgain(f.country, f.title, now);
    if (m_polling || !m_started || !m_calendar->isEnabled())
        return;
    const fresh::WatchPlan plan = fresh::planWatch(m_known, now);
    if (plan.pollSecs == 0 || (m_lastPoll.isValid() && m_lastPoll.secsTo(now) < plan.pollSecs))
        return;
    m_lastPoll = now;
    m_polling = true;
    m_calendar->fetchLive(plan.countries, this, [this] { m_polling = false; });
}

void ReleaseWatcher::note(const fresh::Release &release, const QDateTime &seen)
{
    // Only what the watch saw come in tells how late it was: not the catch-up after a night without the app.
    const qint64 late = release.time.secsTo(seen);
    if (late > 2 * 3600)
        return;
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/data";
    QDir().mkpath(dir);
    const QString path = dir + "/release-timing.log";
    if (QFileInfo(path).size() > LogLimit) {
        QFile::remove(path + ".old");
        QFile::rename(path, path + ".old");
    }
    QFile file(path);
    if (!file.open(QIODevice::Append | QIODevice::Text))
        return;
    QTextStream(&file) << seen.toString(Qt::ISODate) << ' ' << release.country << " impact " << release.impact << " due "
                       << release.time.toString(Qt::ISODate) << " seen after " << late << " s: " << release.title << '\n';
}
