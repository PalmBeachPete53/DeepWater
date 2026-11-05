#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

// Myfxbook's community outlook: the positions of the traders whose accounts it follows, per symbol.
namespace desk {

struct OutlookRow {
    QString symbol;          // "EURUSD"
    double longPct = 0, shortPct = 0;
    double longVolume = 0, shortVolume = 0;   // lots
    int longPositions = 0, shortPositions = 0;
    double avgLongPrice = 0, avgShortPrice = 0;
};
struct Outlook {
    bool ok = false;
    bool sessionInvalid = false;   // the session has expired or the address changed: log in again
    QString message;               // Myfxbook's own, when it refuses
    QVector<OutlookRow> rows;      // as sent
};
// The field names were read from an older client, so it accepts the variants seen since and says so when none fits.
Outlook parseOutlook(const QByteArray &json);

// The session of a login's answer; empty with `message` set when refused.
QString parseLogin(const QByteArray &json, QString *message);

} // namespace desk
