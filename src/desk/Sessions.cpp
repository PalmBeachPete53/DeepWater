#include "desk/Sessions.h"

#include <QTimeZone>

#include <algorithm>

namespace desk {

const QVector<FxSession> &fxSessions()
{
    static const QVector<FxSession> list {
        {"sydney", "Sydney", "Australia/Sydney", 7, 16},
        {"tokyo", "Tokyo", "Asia/Tokyo", 9, 18},
        {"london", "London", "Europe/London", 8, 17},
        {"newyork", "New York", "America/New_York", 8, 17},
    };
    return list;
}

void sessionWindow(const FxSession &s, const QDate &date, QDateTime *open, QDateTime *close)
{
    *open = *close = QDateTime();
    if (date.dayOfWeek() > 5)
        return;
    const QTimeZone zone(s.zone.toUtf8());
    *open = QDateTime(date, QTime(s.openHour, 0), zone).toUTC();
    *close = QDateTime(date, QTime(s.closeHour, 0), zone).toUTC();
}

SessionState sessionState(const FxSession &s, const QDateTime &now)
{
    SessionState st;
    const QTimeZone zone(s.zone.toUtf8());
    const QDate local = now.toTimeZone(zone).date();
    for (int offset = 0; offset < 8; ++offset) {
        QDateTime open, close;
        sessionWindow(s, local.addDays(offset), &open, &close);
        if (!open.isValid() || now >= close)
            continue;
        st.todayOpen = open;
        st.todayClose = close;
        st.nextClose = close;
        st.open = now >= open;
        if (st.open) {
            st.progress = double(open.secsTo(now)) / std::max<qint64>(1, open.secsTo(close));
            for (int after = offset + 1; after < offset + 8; ++after) {
                QDateTime o, c;
                sessionWindow(s, local.addDays(after), &o, &c);
                if (o.isValid()) {
                    st.nextOpen = o;
                    break;
                }
            }
        } else {
            st.nextOpen = open;
        }
        break;
    }
    return st;
}

QStringList openSessions(const QDateTime &now)
{
    QStringList out;
    for (const FxSession &s : fxSessions())
        if (sessionState(s, now).open)
            out << s.id;
    return out;
}

QVector<RecapWindow> recapWindows(const QDateTime &now, int days)
{
    QVector<RecapWindow> out;
    const struct { const char *id, *name, *zone; QTime from, to; } defs[] = {
        {"tokyo", "Tokyo", "Asia/Tokyo", QTime(9, 0), QTime(15, 0)},
        {"london", "London", "Europe/London", QTime(8, 0), QTime(16, 30)},
        {"newyork", "New York", "America/New_York", QTime(8, 30), QTime(16, 0)},
    };
    const QDate today = now.toUTC().date();
    for (int d = -1; d <= days; ++d) {
        const QDate date = today.addDays(-d);
        for (const auto &def : defs) {
            if (date.dayOfWeek() > 5)
                continue;
            const QTimeZone zone(def.zone);
            RecapWindow w {def.id, def.name, QDateTime(date, def.from, zone).toUTC(), QDateTime(date, def.to, zone).toUTC(), false};
            if (w.start > now)
                continue;
            w.running = now < w.end;
            out.push_back(w);
        }
    }
    std::sort(out.begin(), out.end(), [](const RecapWindow &a, const RecapWindow &b) { return a.start > b.start; });
    return out;
}

} // namespace desk
