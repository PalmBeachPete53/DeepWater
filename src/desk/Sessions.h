#pragma once

#include <QDateTime>
#include <QString>
#include <QVector>

// The four FX sessions: the conventional business hours of each centre, Monday to Friday, with daylight saving.
namespace desk {

struct FxSession {
    QString id, name, zone;   // "london", "London", "Europe/London"
    int openHour = 8, closeHour = 17;   // local to the centre
};
const QVector<FxSession> &fxSessions();

struct SessionState {
    bool open = false;
    QDateTime nextOpen, nextClose;   // UTC; nextClose is the running session's when open
    QDateTime todayOpen, todayClose; // this session's window around `now`: the running one, or the next
    double progress = 0;             // 0-1 through the window when open
};
SessionState sessionState(const FxSession &s, const QDateTime &now);
// The ids of the sessions open at `now`.
QStringList openSessions(const QDateTime &now);
// One day's window of a session on `date` in the centre's calendar; weekends give invalid times.
void sessionWindow(const FxSession &s, const QDate &date, QDateTime *open, QDateTime *close);

// A session of the recap: London and New York in their cash hours, Tokyo in its own.
struct RecapWindow {
    QString id, name;
    QDateTime start, end;   // UTC
    bool running = false;
};
// The windows that started before `now`, newest first, over `days` calendar days.
QVector<RecapWindow> recapWindows(const QDateTime &now, int days);

} // namespace desk
