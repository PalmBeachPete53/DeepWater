#pragma once

#include <QDate>
#include <QString>
#include <QVector>

// What the market prices for the Fed: 30-day fed funds futures read meeting by meeting, the method behind the usual
// "rate probabilities". Plain arithmetic; the prices and the calendar come from elsewhere.
namespace desk {

// The FOMC's scheduled meetings, last day, from the Fed's calendar page. Notation votes and unscheduled calls are left out.
QVector<QDate> parseFomcCalendar(const QString &html);

// Yahoo's symbol of the 30-day fed funds future of a month: "ZQZ26.CBT".
QString fedFutureSymbol(const QDate &month);

struct FedContract {
    QDate month;       // any day of the delivery month
    double price = 0;  // 100 minus the month's average rate
};
struct MeetingPricing {
    QDate date;
    double rateBefore = 0, rateAfter = 0;   // %
    double changeBp = 0;                    // this meeting, in basis points
    double cumulativeBp = 0;                // since today
    // The move spread on the two nearest quarter-point steps: -50, -25, 0, +25, +50 bp.
    double probability[5] = {0, 0, 0, 0, 0};
    double pCut = 0, pHold = 0, pHike = 0;  // %
};
struct FedPath {
    double currentRate = 0;                 // what the first contract says today
    QVector<MeetingPricing> meetings;
};
// Meetings after `today` that a contract covers. `contracts` in any order; a missing month ends the path.
// Spreads a meeting's changeBp on the two nearest quarter points: the usual simplification.
void splitMove(MeetingPricing &p);
FedPath fedPath(const QVector<FedContract> &contracts, const QVector<QDate> &meetings, const QDate &today);

// --- The CFTC's Traders in Financial Futures report.

struct TffWeek {
    QDate date;
    qint64 openInterest = 0;
    qint64 dealerLong = 0, dealerShort = 0;
    qint64 assetLong = 0, assetShort = 0;
    qint64 levLong = 0, levShort = 0;
    qint64 otherLong = 0, otherShort = 0;
    qint64 nonReptLong = 0, nonReptShort = 0;
};
// Where the last of `nets` sits in its range over the last `window` values: 0 lowest, 100 highest; -1 without data.
double rangePosition(const QVector<double> &nets, int window = 52);
QVector<TffWeek> parseTff(const QByteArray &json);

// CME's three-month €STR future of a quarter's month: "ESRZ26.CME".
QString estrFutureSymbol(const QDate &month);
// The quarterly months whose contract hasn't started yet, the next `count` of them.
QVector<QDate> nextQuarterMonths(const QDate &today, int count);

} // namespace desk
