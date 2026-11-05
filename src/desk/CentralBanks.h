#pragma once

#include "Rates.h"

#include <QByteArray>
#include <QDate>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

#include <utility>

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
// "4Q26F" or "2027F": the last day of that period; invalid otherwise.
QDate forecastPeriodEnd(const QString &label);

// The Bank of England's OIS forward curve, from the sheet "fwds, short end" of its daily file: the last day's rate by
// months ahead, oldest maturity first.
struct OisCurve {
    QDate date;
    QVector<std::pair<double, double>> points;   // months ahead, rate %
    double at(double months) const;              // interpolated; NaN outside the curve
};
OisCurve parseBoeOisSheet(const QByteArray &sheetXml);
// What a curve prices at each meeting ahead, from `currentRate`, moves split as for the Fed's.
FedPath oisMeetingPath(const OisCurve &curve, double currentRate, const QVector<QDate> &meetings, const QDate &today);

// Meetings of the banks whose sites refuse programs (the RBA, the RBNZ): their published dates, kept here and updated
// with the app. Empty for the others.
QVector<QDate> bundledMeetings(const QString &currency);

// The SNB's spot rates on Confederation bonds (cube rendeiduebd), last day published.
QHash<QString, double> parseSnbYields(const QString &csv);

// How far apart professional forecasters are: one survey's individual answers for a year.
struct ForecastRange {
    QString topic;     // "CPI", "GDP", "UNEMP"
    int year = 0;
    QString survey;    // "2026-Q4"
    int count = 0;
    double low = 0, median = 0, high = 0;
};
// The ECB's Survey of Professional Forecasters, individual point forecasts, latest survey.
QVector<ForecastRange> parseEcbSpf(const QString &csv);
// The Philadelphia Fed's file of individual forecasts for "CPI", "UNEMP" or "RGDP", latest survey: an xlsx's first sheet
// and shared strings, unzipped.
QVector<ForecastRange> parsePhillyIndividual(const QByteArray &sheetXml, const QByteArray &sharedStringsXml, const QString &variable);

} // namespace desk
