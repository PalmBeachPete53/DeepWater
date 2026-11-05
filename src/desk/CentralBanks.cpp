#include "CentralBanks.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QRegularExpression>
#include <QXmlStreamReader>

#include <algorithm>
#include <cmath>

namespace desk {

namespace {

constexpr auto Dot = QRegularExpression::DotMatchesEverythingOption;
constexpr auto NoCase = QRegularExpression::CaseInsensitiveOption;

int monthOf(const QString &name)
{
    static const QStringList months {"jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec"};
    return int(months.indexOf(name.left(3).toLower())) + 1;
}

QString plainText(QString html)
{
    static const QRegularExpression tags("<[^>]+>");
    html.replace(tags, " ");
    html.replace("&nbsp;", " ").replace("&#160;", " ").replace("&ndash;", "–").replace("&amp;", "&");
    return html.simplified();
}

QVector<QDate> sorted(QVector<QDate> dates)
{
    std::sort(dates.begin(), dates.end());
    dates.erase(std::unique(dates.begin(), dates.end()), dates.end());
    return dates;
}

} // namespace

QVector<QDate> parseEcbMeetings(const QString &html)
{
    // "16/12/2026 … monetary policy meeting in Frankfurt (Day 1)", then Day 2 with the press conference;
    // "non-monetary policy meeting" has no decision.
    static const QRegularExpression re(R"(<dt>\s*(\d{2})/(\d{2})/(\d{4})\s*</dt>\s*<dd>\s*([^<]*))");
    QVector<QDate> out;
    for (auto it = re.globalMatch(html); it.hasNext();) {
        const auto m = it.next();
        const QString what = m.captured(4);
        if (what.contains("monetary policy meeting", Qt::CaseInsensitive) && !what.contains("non-monetary", Qt::CaseInsensitive)
            && !what.contains("Day 1"))
            out << QDate(m.captured(3).toInt(), m.captured(2).toInt(), m.captured(1).toInt());
    }
    return sorted(out);
}

QVector<QDate> parseBoeMeetings(const QString &html)
{
    // "<h2>2026 confirmed dates</h2><table>…<td>Thursday 5 February</td>…"; the non-breaking space varies.
    static const QRegularExpression head(R"(<h2[^>]*>\s*(\d{4})[^<]*dates[^<]*</h2>(.*?)</table>)", Dot | NoCase);
    static const QRegularExpression day(R"(<td[^>]*>\s*[A-Za-z]+day(?:\s|&nbsp;)+(\d{1,2})(?:\s|&nbsp;)+([A-Za-z]+)(?:\s|&nbsp;)*</td>)");
    QVector<QDate> out;
    for (auto it = head.globalMatch(html); it.hasNext();) {
        const auto block = it.next();
        const int year = block.captured(1).toInt();
        for (auto d = day.globalMatch(block.captured(2)); d.hasNext();) {
            const auto m = d.next();
            if (const int month = monthOf(m.captured(2)))
                out << QDate(year, month, m.captured(1).toInt());
        }
    }
    return sorted(out);
}

QVector<QDate> parseBojMeetings(const QString &html)
{
    // <caption>Table : 2026</caption>, then rows whose first cell reads "Jan. 22 (Thurs.), 23 (Fri.)" or
    // "Apr. 30 (Thurs.), May 1 (Fri.)".
    static const QRegularExpression table(R"(Table\s*:\s*(\d{4})\s*</caption>(.*?)</table>)", Dot);
    static const QRegularExpression row(R"(<tr>\s*<td[^>]*>(.*?)</td>)", Dot);
    static const QRegularExpression part(R"(([A-Z][a-z]{2})?\.?\s*(\d{1,2})\s*\()");
    QVector<QDate> out;
    for (auto t = table.globalMatch(html); t.hasNext();) {
        const auto tm = t.next();
        const int year = tm.captured(1).toInt();
        for (auto r = row.globalMatch(tm.captured(2)); r.hasNext();) {
            const QString cell = plainText(r.next().captured(1));
            int month = 0, day = 0;
            for (auto p = part.globalMatch(cell); p.hasNext();) {
                const auto m = p.next();
                if (!m.captured(1).isEmpty())
                    month = monthOf(m.captured(1));
                day = m.captured(2).toInt();
            }
            if (month && day)
                out << QDate(year, month, day);
        }
    }
    return sorted(out);
}

QVector<QDate> parseSnbMeetings(const QString &html)
{
    static const QRegularExpression re(R"(Monetary policy assessment of (\d{1,2}) ([A-Za-z]+) (\d{4}))");
    QVector<QDate> out;
    for (auto it = re.globalMatch(html); it.hasNext();) {
        const auto m = it.next();
        if (const int month = monthOf(m.captured(2)))
            out << QDate(m.captured(3).toInt(), month, m.captured(1).toInt());
    }
    return sorted(out);
}

QVector<QDate> parseBocMeetings(const QString &html)
{
    // "Schedule for 2026 Dates Publications January 28 Interest rate announcement and … March 18 Interest rate…"
    const QString text = plainText(html);
    static const QRegularExpression schedule(R"(Schedule for (\d{4}))");
    static const QRegularExpression entry(R"((January|February|March|April|May|June|July|August|September|October|November|December) (\d{1,2}) Interest rate announcement)");
    QVector<QDate> out;
    QVector<QRegularExpressionMatch> heads;
    for (auto it = schedule.globalMatch(text); it.hasNext();)
        heads << it.next();
    for (int i = 0; i < heads.size(); ++i) {
        const qsizetype from = heads[i].capturedEnd();
        const qsizetype to = i + 1 < heads.size() ? heads[i + 1].capturedStart() : text.size();
        const int year = heads[i].captured(1).toInt();
        for (auto it = entry.globalMatch(text.mid(from, to - from)); it.hasNext();) {
            const auto m = it.next();
            out << QDate(year, monthOf(m.captured(1)), m.captured(2).toInt());
        }
    }
    return sorted(out);
}

Vote parseFedVote(const QString &statementHtml)
{
    Vote v;
    const QString text = plainText(statementHtml);
    if (!text.contains("FOMC statement", Qt::CaseInsensitive) && !text.contains("Federal Open Market Committee", Qt::CaseInsensitive))
        return v;
    static const QRegularExpression against(R"(Voting against (?:the|this) (?:monetary policy )?action (?:were|was):? (.*?)(?:, who |(?<![A-Z])\.(?: [A-Z]|$)))");
    const auto m = against.match(text);
    if (m.hasMatch()) {
        static const QRegularExpression separators(R"(\s*,\s*(?:and\s+)?|\s+and\s+)");
        v.dissenters = m.captured(1).split(separators, Qt::SkipEmptyParts);
        v.known = !v.dissenters.isEmpty();
    } else if (text.contains("Voting for", Qt::CaseInsensitive)) {
        v.known = true;
        v.unanimous = true;
    }
    return v;
}

Vote parseBoeVote(const QString &summaryHtml)
{
    Vote v;
    const QString text = plainText(summaryHtml);
    static const QRegularExpression majority(R"(voted by a majority of (\d+)\s*[–-]\s*(\d+))");
    const auto m = majority.match(text);
    if (m.hasMatch()) {
        v.known = true;
        v.majority = m.captured(1).toInt();
        v.minority = m.captured(2).toInt();
    } else if (text.contains("voted unanimously")) {
        v.known = true;
        v.unanimous = true;
    }
    return v;
}

QString latestFomcStatementUrl(const QString &feedXml)
{
    static const QRegularExpression item(R"(<item>\s*<title>([^<]*)</title>\s*<link>(?:<!\[CDATA\[)?([^<\]]+))");
    for (auto it = item.globalMatch(feedXml); it.hasNext();) {
        const auto m = it.next();
        if (m.captured(1).contains("FOMC statement", Qt::CaseInsensitive))
            return m.captured(2).trimmed();
    }
    return {};
}

QString boeSummaryUrl(const QDate &meeting)
{
    static const QStringList months {"january", "february", "march", "april", "may", "june", "july", "august", "september", "october", "november", "december"};
    return QString("https://www.bankofengland.co.uk/monetary-policy-summary-and-minutes/%1/%2-%1").arg(meeting.year()).arg(months.value(meeting.month() - 1));
}

QHash<QString, double> parseMofYields(const QString &csv)
{
    // A title line, "Date,1Y,2Y,…", then a line per day, and stray commas at the end.
    QHash<QString, double> out;
    QStringList head, last;
    for (const QString &line : csv.split('\n')) {
        const QStringList cells = line.trimmed().split(',');
        if (cells.value(0) == "Date")
            head = cells;
        else if (!head.isEmpty() && cells.value(0).contains('/') && cells.size() > 1 && !cells[1].isEmpty())
            last = cells;
    }
    for (int i = 1; i < head.size() && i < last.size(); ++i) {
        bool good = false;
        const double v = last[i].toDouble(&good);
        if (good)
            out.insert(head[i], v);
    }
    return out;
}

QHash<QString, double> parseBoeYields(const QString &csv)
{
    QHash<QString, double> out;
    const QStringList lines = csv.split('\n', Qt::SkipEmptyParts);
    if (lines.size() < 2)
        return out;
    const QStringList head = lines.first().trimmed().split(',');
    const QStringList last = lines.last().trimmed().split(',');
    const QHash<QString, QString> tenors {{"IUDSNZC", "5Y"}, {"IUDMNZC", "10Y"}, {"IUDLNZC", "20Y"}};
    for (int i = 1; i < head.size() && i < last.size(); ++i) {
        bool good = false;
        const double v = last[i].toDouble(&good);
        if (good && tenors.contains(head[i]))
            out.insert(tenors.value(head[i]), v);
    }
    return out;
}

QHash<QString, double> parseValetYields(const QByteArray &json)
{
    QHash<QString, double> out;
    const QJsonObject root = QJsonDocument::fromJson(json).object();
    const QJsonValue observations = root.value("observations");
    if (!observations.isArray() || observations.toArray().isEmpty())
        return out;
    const QJsonObject last = observations.toArray().last().toObject();
    const QHash<QString, QString> tenors {{"BD.CDN.2YR.DQ.YLD", "2Y"}, {"BD.CDN.5YR.DQ.YLD", "5Y"}, {"BD.CDN.10YR.DQ.YLD", "10Y"}, {"BD.CDN.LONG.DQ.YLD", "30Y"}};
    for (auto it = tenors.begin(); it != tenors.end(); ++it) {
        bool good = false;
        const double v = last.value(it.key()).toObject().value("v").toString().toDouble(&good);
        if (good)
            out.insert(it.value(), v);
    }
    return out;
}

QVector<ForecastTab> parseIngForecasts(const QString &html)
{
    static const QRegularExpression pane(R"re(<div class="tab-pane[^"]*" id="([^"]+)"[^>]*>\s*<h2[^>]*>(.*?)</h2>\s*<p>(.*?)</p>(.*?)(?=<div class="tab-pane|$))re", Dot);
    static const QRegularExpression row(R"(<tr[^>]*>(.*?)</tr>)", Dot);
    static const QRegularExpression cell(R"(<(t[hd])[^>]*>(.*?)</t[hd]>)", Dot);
    QVector<ForecastTab> out;
    for (auto p = pane.globalMatch(html); p.hasNext();) {
        const auto pm = p.next();
        ForecastTab tab;
        tab.id = pm.captured(1);
        tab.title = plainText(pm.captured(2));
        tab.updated = plainText(pm.captured(3)).remove("Last updated:").trimmed();
        for (auto r = row.globalMatch(pm.captured(4)); r.hasNext();) {
            QStringList cells;
            bool header = false;
            for (auto c = cell.globalMatch(r.next().captured(1)); c.hasNext();) {
                const auto cm = c.next();
                header = header || cm.captured(1) == "th";
                cells << plainText(cm.captured(2));
            }
            if (std::all_of(cells.begin(), cells.end(), [](const QString &s) { return s.isEmpty(); }))
                continue;
            if (header) {
                ForecastTable table;
                table.group = cells.value(0);
                // Between the group's name and the periods, a blank header over a second label column.
                for (int i = 1; i < cells.size(); ++i)
                    if (!cells[i].isEmpty())
                        table.columns << cells[i];
                tab.tables << table;
            } else if (!tab.tables.isEmpty()) {
                ForecastTable &table = tab.tables.last();
                const int values = int(table.columns.size());
                if (cells.size() <= values)
                    continue;
                ForecastTable::Row out;
                out.labels = cells.mid(0, cells.size() - values);
                out.values = cells.mid(cells.size() - values);
                table.rows << out;
            }
        }
        if (!tab.tables.isEmpty())
            out << tab;
    }
    return out;
}

QDate forecastPeriodEnd(const QString &label)
{
    static const QRegularExpression quarter(R"(^([1-4])Q(\d{2})F?$)"), year(R"(^(\d{4})F?$)");
    if (const auto m = quarter.match(label.trimmed()); m.hasMatch()) {
        const int month = m.captured(1).toInt() * 3;
        return QDate(2000 + m.captured(2).toInt(), month, 1).addMonths(1).addDays(-1);
    }
    if (const auto m = year.match(label.trimmed()); m.hasMatch())
        return QDate(m.captured(1).toInt(), 12, 31);
    return {};
}

double OisCurve::at(double months) const
{
    if (points.isEmpty() || months < points.first().first || months > points.last().first)
        return std::nan("");
    for (int i = 1; i < points.size(); ++i)
        if (months <= points[i].first) {
            const auto &a = points[i - 1], &b = points[i];
            return a.second + (b.second - a.second) * (months - a.first) / (b.first - a.first);
        }
    return points.last().second;
}

OisCurve parseBoeOisSheet(const QByteArray &sheetXml)
{
    // Rows of cells named "B5": a header whose first numbers are 1, 2, 3 (months), then one row per day, column A an
    // Excel date. Text and error cells are left out; a day can have blanks.
    const auto columnOf = [](const QString &ref) {
        int c = 0;
        for (const QChar ch : ref) {
            if (!ch.isLetter())
                break;
            c = c * 26 + (ch.unicode() - 'A' + 1);
        }
        return c;
    };
    QHash<int, double> months;
    OisCurve out;
    QHash<int, double> row;
    QXmlStreamReader xml(sheetXml);
    int column = 0;
    bool numeric = false;
    const auto endRow = [&] {
        if (months.isEmpty()) {
            // Fractions of a year come first in this file's second header: months start at about 1 and step by 1.
            if (std::abs(row.value(2, 0) - 1) < 0.01 && std::abs(row.value(3, 0) - 2) < 0.01)
                months = row;
        } else if (row.value(1, 0) > 30000) {
            OisCurve day;
            day.date = QDate(1899, 12, 30).addDays(qint64(row.value(1)));
            QVector<int> columns = months.keys();
            std::sort(columns.begin(), columns.end());
            for (const int c : std::as_const(columns))
                if (c > 1 && row.contains(c))
                    day.points.push_back({months.value(c), row.value(c)});
            if (day.points.size() >= 3)
                out = day;
        }
        row.clear();
    };
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == u"c") {
            column = columnOf(xml.attributes().value("r").toString());
            const auto type = xml.attributes().value("t");
            numeric = type.isEmpty() || type == u"n";
        } else if (xml.isStartElement() && xml.name() == u"v" && numeric) {
            bool good = false;
            const double v = xml.readElementText().toDouble(&good);
            if (good)
                row.insert(column, v);
        } else if (xml.isEndElement() && xml.name() == u"row") {
            endRow();
        }
    }
    return out;
}

} // namespace desk
