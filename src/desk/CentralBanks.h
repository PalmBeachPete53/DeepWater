#pragma once

#include <QByteArray>
#include <QDate>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

// Central banks' own pages: the dates of their decisions, how the last one was voted, their governments' yield
// curves. Pages change: each parser returns nothing rather than a guess, and the Desk then shows a dash.
namespace desk {

// The day of the decision, sorted. A two-day meeting counts by its last day.
QVector<QDate> parseEcbMeetings(const QString &html);
QVector<QDate> parseBoeMeetings(const QString &html);
QVector<QDate> parseBojMeetings(const QString &html);
QVector<QDate> parseSnbMeetings(const QString &html);
QVector<QDate> parseBocMeetings(const QString &html);

struct Vote {
    bool known = false;
    int majority = 0, minority = 0;   // the BoE's count, 0 when a text gives none
    QStringList dissenters;           // the Fed's names
    bool unanimous = false;
};
Vote parseFedVote(const QString &statementHtml);
Vote parseBoeVote(const QString &summaryHtml);
QString latestFomcStatementUrl(const QString &feedXml);
// The English month and year in the address of the BoE's summary of a meeting.
QString boeSummaryUrl(const QDate &meeting);

// Yields by tenor ("2Y"), from the last line published.
QHash<QString, double> parseMofYields(const QString &csv);
QHash<QString, double> parseBoeYields(const QString &csv);   // columns IUDSNZC, IUDMNZC, IUDLNZC
QHash<QString, double> parseValetYields(const QByteArray &json);

// ING Think's forecast page: tabs of tables whose header row is made of <th>.
struct ForecastTable {
    QString group;              // "G10 (eop)"
    QStringList columns;        // the period columns
    struct Row { QStringList labels; QStringList values; };
    QVector<Row> rows;
};
struct ForecastTab {
    QString id, title, updated;
    QVector<ForecastTable> tables;
};
QVector<ForecastTab> parseIngForecasts(const QString &html);

} // namespace desk
