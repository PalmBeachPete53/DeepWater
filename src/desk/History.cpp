#include "History.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

#include <algorithm>

namespace desk {

void addPoint(History &history, const QString &key, const QDate &date, double value)
{
    QVector<DayPoint> &points = history[key];
    auto it = std::lower_bound(points.begin(), points.end(), date, [](const DayPoint &p, const QDate &d) { return p.date < d; });
    if (it != points.end() && it->date == date)
        it->value = value;
    else
        points.insert(it, {date, value});
}

QByteArray historyToJson(const History &history)
{
    QJsonObject root;
    for (auto it = history.begin(); it != history.end(); ++it) {
        QJsonArray points;
        for (const DayPoint &p : it.value())
            points.append(QJsonArray {p.date.toString(Qt::ISODate), p.value});
        root.insert(it.key(), points);
    }
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

History historyFromJson(const QByteArray &json)
{
    History history;
    const QJsonObject root = QJsonDocument::fromJson(json).object();
    for (auto it = root.begin(); it != root.end(); ++it)
        for (const QJsonValue &v : it.value().toArray()) {
            const QJsonArray point = v.toArray();
            const QDate date = QDate::fromString(point.at(0).toString(), Qt::ISODate);
            if (date.isValid() && point.at(1).isDouble())
                addPoint(history, it.key(), date, point.at(1).toDouble());
        }
    return history;
}

QDate lastDate(const History &history, const QString &prefix)
{
    QDate last;
    for (auto it = history.begin(); it != history.end(); ++it)
        if (it.key().startsWith(prefix) && !it.value().isEmpty() && (!last.isValid() || it.value().last().date > last))
            last = it.value().last().date;
    return last;
}

QString meetingKey(const QString &currency, const QDate &meeting)
{
    return QString("rate/%1/%2").arg(currency, meeting.toString(Qt::ISODate));
}

QDate meetingOfKey(const QString &key)
{
    return QDate::fromString(key.section('/', 2), Qt::ISODate);
}

QString nextMeetingKey(const History &history, const QString &currency, const QDate &today)
{
    const QString prefix = QString("rate/%1/").arg(currency);
    QString best;
    QDate bestDate;
    for (auto it = history.begin(); it != history.end(); ++it) {
        if (!it.key().startsWith(prefix))
            continue;
        const QDate meeting = meetingOfKey(it.key());
        if (meeting.isValid() && meeting >= today && (!bestDate.isValid() || meeting < bestDate)) {
            bestDate = meeting;
            best = it.key();
        }
    }
    return best;
}

} // namespace desk
