#pragma once

#include "fresh/Fresh.h"

#include <QDateTime>
#include <QObject>
#include <QTimer>

class EconomicCalendar;

// The calendar knows when a figure is due, so that's when the app looks: every 10 to 30 seconds from a minute before
// the time announced until the figure is there (fresh::planWatch), asking only about the countries concerned. Once it
// is there, askAgain says which sources to read again, at once and then a few more times, since the publisher's own
// series can follow the calendar by hours (fresh::followUpDelays). At launch, a figure published while the app was
// closed makes older copies stale in the same way.
//
// data/release-timing.log, in the app's data folder, says how long each figure took to reach the app.
class ReleaseWatcher : public QObject
{
    Q_OBJECT
public:
    explicit ReleaseWatcher(EconomicCalendar *calendar, QObject *parent = nullptr);
    ~ReleaseWatcher() override;
    static ReleaseWatcher *instance();   // null before the main window makes it

signals:
    // Read again what was fetched before `olderThan` for a figure of that country.
    void askAgain(const QString &country, const QString &title, const QDateTime &olderThan);
    // After askAgain's owner has marked the sources stale: pages showing them read them again.
    void sourcesAsked(const QString &country, const QStringList &groups);

private:
    void tick();
    void calendarChanged();
    void note(const fresh::Release &release, const QDateTime &seen);

    EconomicCalendar *m_calendar;
    QTimer m_timer;
    QVector<fresh::Release> m_known;
    fresh::FollowUps m_followUps;
    QDateTime m_lastPoll;
    bool m_polling = false;
    bool m_started = false;
};
