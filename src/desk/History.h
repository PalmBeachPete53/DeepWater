#pragma once

#include <QByteArray>
#include <QDate>
#include <QHash>
#include <QString>
#include <QVector>

// What no source keeps: a value a day, per key, noted on this computer from the day the app first saw it.
namespace desk {

struct DayPoint {
    QDate date;
    double value = 0;
};
using History = QHash<QString, QVector<DayPoint>>;

// In date order; a second value on the same day replaces the first.
void addPoint(History &history, const QString &key, const QDate &date, double value);
QByteArray historyToJson(const History &history);
History historyFromJson(const QByteArray &json);   // what can't be read is left out
// The newest date among the keys that start with `prefix`; invalid when there are none.
QDate lastDate(const History &history, const QString &prefix);

// A central bank's meeting priced day after day: the key of a meeting, and the next one on or after `today`.
QString meetingKey(const QString &currency, const QDate &meeting);
QString nextMeetingKey(const History &history, const QString &currency, const QDate &today);
QDate meetingOfKey(const QString &key);
inline QString crowdKey(const QString &symbol) { return "crowd/" + symbol; }

} // namespace desk
