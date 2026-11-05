#include "desk/Journal.h"

#include <QJsonArray>
#include <QRegularExpression>
#include <QStringDecoder>
#include <QTimeZone>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>

namespace desk {

namespace {

const double NaN = std::numeric_limits<double>::quiet_NaN();

QString normalized(const QString &header)
{
    QString h = header.toLower().trimmed();
    h.remove(QRegularExpression("[^a-z0-9/ ]"));
    return h.simplified();
}

// Which field a column name means. Time and price columns come first: "open price" must not read as "open time".
int fieldOf(const QString &header)
{
    const QString h = normalized(header);
    const auto is = [&](std::initializer_list<const char *> names) {
        for (const char *n : names)
            if (h == n)
                return true;
        return false;
    };
    if (is({"ticket", "position", "position id", "id", "order", "deal", "trade id", "order id"})) return int(TradeField::Ticket);
    if (is({"symbol", "instrument", "item", "market", "pair", "asset"})) return int(TradeField::Symbol);
    if (is({"type", "side", "direction", "action", "opening direction", "position type", "buy/sell"})) return int(TradeField::Side);
    if (is({"volume", "lots", "lot", "size", "quantity", "closing quantity", "opening quantity", "qty", "amount lots"})) return int(TradeField::Volume);
    if (is({"open time", "opening time", "entry time", "open date", "opening date", "time open", "entry date", "open"})) return int(TradeField::OpenTime);
    if (is({"close time", "closing time", "exit time", "close date", "closing date", "time close", "exit date", "close"})) return int(TradeField::CloseTime);
    if (is({"open price", "entry price", "opening price", "price open", "entry"})) return int(TradeField::OpenPrice);
    if (is({"close price", "exit price", "closing price", "price close", "exit"})) return int(TradeField::ClosePrice);
    if (is({"s/l", "sl", "stop loss", "stoploss", "stop"})) return int(TradeField::StopLoss);
    if (is({"t/p", "tp", "take profit", "takeprofit", "target"})) return int(TradeField::TakeProfit);
    if (is({"commission", "commissions", "fees", "fee"})) return int(TradeField::Commission);
    if (is({"swap", "swaps", "rollover", "financing"})) return int(TradeField::Swap);
    if (h == "profit" || h == "p/l" || h == "pl" || h == "pnl" || h == "net" || h.startsWith("net ") || h == "net profit" || h == "gross profit"
        || h.startsWith("gross ") || h == "realized pnl")
        return int(TradeField::Profit);
    if (is({"comment", "comments", "label", "note", "notes", "strategy", "tag"})) return int(TradeField::Comment);
    return -1;
}

QChar detectDelimiter(const QString &line)
{
    int best = 0;
    QChar out = ',';
    for (QChar c : {QChar(','), QChar(';'), QChar('\t'), QChar('|')}) {
        const int n = line.count(c);
        if (n > best) {
            best = n;
            out = c;
        }
    }
    return out;
}

QStringList splitCsvLine(const QString &line, QChar delimiter)
{
    QStringList cells;
    QString cur;
    bool quoted = false;
    for (int i = 0; i < line.size(); ++i) {
        const QChar c = line[i];
        if (c == '"') {
            if (quoted && i + 1 < line.size() && line[i + 1] == '"') {
                cur += '"';
                ++i;
            } else {
                quoted = !quoted;
            }
        } else if (c == delimiter && !quoted) {
            cells << cur.trimmed();
            cur.clear();
        } else {
            cur += c;
        }
    }
    cells << cur.trimmed();
    return cells;
}

QStringList nonEmptyLines(const QString &text)
{
    QStringList out;
    for (const QString &l : text.split(QRegularExpression("\r\n|\r|\n")))
        if (!l.trimmed().isEmpty())
            out << l;
    return out;
}

// A side from what a statement writes; 0 when the row isn't a buy or a sell (balance, credit).
int sideOf(const QString &text)
{
    const QString t = text.trimmed().toLower();
    if (t == "buy" || t == "long" || t.startsWith("buy ") || t == "b")
        return 1;
    if (t == "sell" || t == "short" || t.startsWith("sell ") || t == "s")
        return -1;
    return 0;
}


QString cellText(QString raw)
{
    raw.remove(QRegularExpression("<[^>]*>"));
    raw.replace("&nbsp;", " ").replace("&amp;", "&").replace("&#160;", " ").replace(QChar(0xA0), ' ');
    return raw.simplified();
}

// Every row of the document's tables, cells as text. colspan cells count for as many columns: MT reports line up on them.
QVector<QStringList> htmlRows(const QString &html)
{
    QVector<QStringList> rows;
    static const QRegularExpression tr("<tr[^>]*>(.*?)(?=<tr[ >]|</table|\\z)", QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression td("<t[dh]([^>]*)>(.*?)(?=</t[dh]>|<t[dh][ >]|\\z)", QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression span("colspan\\s*=\\s*\"?(\\d+)", QRegularExpression::CaseInsensitiveOption);
    auto rowIt = tr.globalMatch(html);
    while (rowIt.hasNext()) {
        const QString body = rowIt.next().captured(1);
        QStringList cells;
        auto cellIt = td.globalMatch(body);
        while (cellIt.hasNext()) {
            const auto m = cellIt.next();
            cells << cellText(m.captured(2));
            const auto sp = span.match(m.captured(1));
            for (int i = 1; sp.hasMatch() && i < sp.captured(1).toInt() && i < 30; ++i)
                cells << QString();
        }
        if (!cells.isEmpty())
            rows.push_back(cells);
    }
    return rows;
}

double amountOr0(const QString &s)
{
    const double v = parseAmount(s);
    return std::isnan(v) ? 0 : v;
}

// MetaTrader 4's "Closed Transactions": Ticket, Open Time, Type, Size, Item, Price, S/L, T/P, Close Time, Price,
// Commission, Taxes, Swap, Profit. A row with a buy or sell type and a close time is a closed trade.
ImportResult readMt4(const QVector<QStringList> &rows)
{
    ImportResult r;
    r.format = "MetaTrader 4 HTML";
    for (const QStringList &c : rows) {
        if (c.size() < 14) {
            ++r.skipped;
            continue;
        }
        const int side = sideOf(c[2]);
        const QDateTime open = parseTradeTime(c[1]), close = parseTradeTime(c[8]);
        if (!side || !open.isValid() || c[4].isEmpty()) {
            ++r.skipped;
            continue;
        }
        Trade t;
        t.id = c[0];
        t.openTime = open;
        t.isLong = side > 0;
        t.volume = amountOr0(c[3]);
        t.symbol = c[4].toUpper();
        t.openPrice = amountOr0(c[5]);
        t.stopLoss = amountOr0(c[6]);
        t.takeProfit = amountOr0(c[7]);
        t.closeTime = close;
        t.closePrice = amountOr0(c[9]);
        t.commission = amountOr0(c[10]) + amountOr0(c[11]);
        t.swap = amountOr0(c[12]);
        t.profit = amountOr0(c[13]);
        r.trades.push_back(t);
    }
    return r;
}

// MetaTrader 5's report, section "Positions": Time, Position, Symbol, Type, Volume, Price, S/L, T/P, Time, Price,
// Commission, Swap, Profit. The Orders and Deals sections that follow have other shapes and are skipped.
ImportResult readMt5(const QVector<QStringList> &rows)
{
    ImportResult r;
    r.format = "MetaTrader 5 HTML";
    bool inPositions = false;
    for (const QStringList &raw : rows) {
        // A title has one filled cell, whatever its colspan.
        QStringList filled;
        for (const QString &cell : raw)
            if (!cell.isEmpty())
                filled << cell;
        if (filled.size() == 1) {
            const QString title = filled[0].toLower();
            if (title == "positions" || title == "closed positions")
                inPositions = true;
            else if (title == "orders" || title == "deals" || title == "open positions" || title == "working orders" || title == "results")
                inPositions = false;
            continue;
        }
        if (!inPositions)
            continue;
        // Some reports put hidden cells before the row: it starts at its first time. Empty S/L and T/P keep their place.
        int start = -1;
        for (int i = 0; i < raw.size() && start < 0; ++i)
            if (parseTradeTime(raw[i]).isValid())
                start = i;
        if (start < 0) {
            if (filled.isEmpty() || normalized(filled[0]) != "time")
                ++r.skipped;
            continue;
        }
        const QStringList c = raw.mid(start);
        if (c.size() < 13) {
            ++r.skipped;
            continue;
        }
        const int side = sideOf(c[3]);
        const QDateTime open = parseTradeTime(c[0]), close = parseTradeTime(c[8]);
        if (!side || !open.isValid()) {
            ++r.skipped;
            continue;
        }
        Trade t;
        t.openTime = open;
        t.id = c[1];
        t.symbol = c[2].toUpper();
        t.isLong = side > 0;
        t.volume = amountOr0(c[4]);
        t.openPrice = amountOr0(c[5]);
        t.stopLoss = amountOr0(c[6]);
        t.takeProfit = amountOr0(c[7]);
        t.closeTime = close;
        t.closePrice = amountOr0(c[9]);
        t.commission = amountOr0(c[10]);
        t.swap = amountOr0(c[11]);
        t.profit = amountOr0(c[12]);
        r.trades.push_back(t);
    }
    return r;
}

} // namespace

double Trade::holdHours() const
{
    return isClosed() && openTime.isValid() ? openTime.secsTo(closeTime) / 3600.0 : 0;
}

QJsonObject Trade::toJson() const
{
    QJsonObject o {{"id", id}, {"symbol", symbol}, {"long", isLong}, {"volume", volume}, {"open", openTime.toString(Qt::ISODate)},
                   {"openPrice", openPrice}, {"closePrice", closePrice}, {"sl", stopLoss}, {"tp", takeProfit},
                   {"commission", commission}, {"swap", swap}, {"profit", profit}};
    if (closeTime.isValid())
        o.insert("close", closeTime.toString(Qt::ISODate));
    if (!strategy.isEmpty())
        o.insert("strategy", strategy);
    if (!notes.isEmpty())
        o.insert("notes", notes);
    return o;
}

Trade Trade::fromJson(const QJsonObject &o)
{
    Trade t;
    t.id = o.value("id").toString();
    t.symbol = o.value("symbol").toString();
    t.isLong = o.value("long").toBool(true);
    t.volume = o.value("volume").toDouble();
    t.openTime = QDateTime::fromString(o.value("open").toString(), Qt::ISODate);
    t.closeTime = QDateTime::fromString(o.value("close").toString(), Qt::ISODate);
    t.openPrice = o.value("openPrice").toDouble();
    t.closePrice = o.value("closePrice").toDouble();
    t.stopLoss = o.value("sl").toDouble();
    t.takeProfit = o.value("tp").toDouble();
    t.commission = o.value("commission").toDouble();
    t.swap = o.value("swap").toDouble();
    t.profit = o.value("profit").toDouble();
    t.strategy = o.value("strategy").toString();
    t.notes = o.value("notes").toString();
    return t;
}

double parseAmount(const QString &text)
{
    QString s = text.trimmed();
    if (s.isEmpty())
        return NaN;
    bool negative = false;
    if (s.startsWith('(') && s.endsWith(')')) {
        negative = true;
        s = s.mid(1, s.size() - 2);
    }
    s.remove(QRegularExpression("[\\s\\x{00A0}\\x{202F}$€£¥']"));
    if (s.startsWith(QChar(0x2212)))   // the minus sign
        s[0] = '-';
    const int comma = s.lastIndexOf(','), dot = s.lastIndexOf('.');
    if (comma >= 0 && dot >= 0) {
        // The last separator is the decimal one.
        if (comma > dot) {
            s.remove('.');
            s.replace(',', '.');
        } else {
            s.remove(',');
        }
    } else if (comma >= 0) {
        // Several commas, or three digits after one with a real integer part ("1,234"), are thousands; "12,5" and
        // "0,125" are decimals.
        const QString after = s.mid(comma + 1);
        if (s.count(',') > 1 || (after.size() == 3 && s.left(comma).remove('-') != "0"))
            s.remove(',');
        else
            s.replace(',', '.');
    }
    bool ok = false;
    const double v = s.toDouble(&ok);
    if (!ok)
        return NaN;
    return negative ? -v : v;
}

QVector<double> numbersInText(const QString &text)
{
    QVector<double> numbers;
    static const QRegularExpression pattern("\\d+(?:[.,\\s\\x{00A0}\\x{202F}]\\d+)*");
    auto it = pattern.globalMatch(text);
    while (it.hasNext()) {
        QString token = it.next().captured();
        token.remove(QRegularExpression("[\\s\\x{00A0}\\x{202F}]"));
        const int comma = token.lastIndexOf(','), dot = token.lastIndexOf('.');
        // The last separator as the decimal one, and also none: "1,125" may be either.
        const int last = std::max(comma, dot);
        QString digits = token;
        digits.remove(',').remove('.');
        bool ok = false;
        const double whole = digits.toDouble(&ok);
        if (ok)
            numbers << whole;
        if (last >= 0) {
            const double decimal = (token.left(last).remove(',').remove('.') + "." + token.mid(last + 1)).toDouble(&ok);
            if (ok)
                numbers << decimal;
        }
    }
    return numbers;
}

QStringList keepStated(Trade &trade, const QString &question)
{
    QStringList emptied;
    // An open trade: its one price is its entry, whatever field the model chose.
    if (!trade.closeTime.isValid()) {
        if (trade.openPrice == 0 && trade.closePrice != 0)
            trade.openPrice = trade.closePrice;
        if (trade.closePrice != 0) {
            trade.closePrice = 0;
            emptied << "close_price";
        }
        if (trade.profit != 0) {
            trade.profit = 0;
            emptied << "profit";
        }
    }
    if (question.trimmed().isEmpty())
        return emptied;
    const QVector<double> numbers = numbersInText(question);
    const auto stated = [&](double v) {
        return std::any_of(numbers.begin(), numbers.end(), [&](double n) { return std::abs(n - std::abs(v)) < 1e-9; });
    };
    const auto number = [&](double &field, const char *name) {
        if (field != 0 && !stated(field)) {
            field = 0;
            emptied << name;
        }
    };
    number(trade.volume, "volume");
    number(trade.openPrice, "open_price");
    number(trade.closePrice, "close_price");
    number(trade.stopLoss, "stop_loss");
    number(trade.takeProfit, "take_profit");
    number(trade.profit, "profit");
    const auto words = [&](QString &field, const char *name) {
        if (!field.isEmpty() && !question.contains(field, Qt::CaseInsensitive)) {
            field.clear();
            emptied << name;
        }
    };
    words(trade.strategy, "strategy");
    words(trade.notes, "notes");
    return emptied;
}

QDateTime parseTradeTime(const QString &text)
{
    const QString s = text.trimmed();
    if (s.isEmpty())
        return {};
    static const char *formats[] = {
        "yyyy.MM.dd HH:mm:ss", "yyyy.MM.dd HH:mm", "yyyy-MM-dd HH:mm:ss", "yyyy-MM-dd HH:mm", "yyyy-MM-ddTHH:mm:ss", "yyyy/MM/dd HH:mm:ss", "yyyy/MM/dd HH:mm",
        "dd.MM.yyyy HH:mm:ss", "dd.MM.yyyy HH:mm", "dd/MM/yyyy HH:mm:ss", "dd/MM/yyyy HH:mm", "dd-MM-yyyy HH:mm:ss", "dd-MM-yyyy HH:mm",
        "yyyy.MM.dd", "yyyy-MM-dd", "dd/MM/yyyy", "dd.MM.yyyy",
    };
    QString clean = s;
    clean.remove(QRegularExpression("\\.\\d{1,6}$"));   // fractions of a second
    clean.remove(QRegularExpression("Z$"));
    for (const char *f : formats) {
        QDateTime d = QDateTime::fromString(clean, QString::fromLatin1(f));
        if (d.isValid()) {
            d.setTimeZone(QTimeZone::UTC);
            return d;
        }
    }
    return {};
}

ImportResult readCsv(const QString &text, const QHash<int, int> &mapping)
{
    ImportResult r;
    r.format = "CSV";
    r.mapping = mapping;
    const QStringList lines = nonEmptyLines(text);
    if (lines.isEmpty()) {
        r.error = "empty";
        return r;
    }
    const QChar delimiter = detectDelimiter(lines.first());
    r.headers = splitCsvLine(lines.first(), delimiter);
    const auto col = [&](TradeField f) { return mapping.value(int(f), -1); };
    if (col(TradeField::Symbol) < 0 || col(TradeField::Profit) < 0 || (col(TradeField::OpenTime) < 0 && col(TradeField::CloseTime) < 0)) {
        r.error = "columns";
        return r;
    }
    int serial = 0;
    for (int i = 1; i < lines.size(); ++i) {
        const QStringList c = splitCsvLine(lines[i], delimiter);
        const auto at = [&](TradeField f) { const int k = col(f); return k >= 0 && k < c.size() ? c[k] : QString(); };
        const QString symbol = at(TradeField::Symbol).toUpper();
        const double profit = parseAmount(at(TradeField::Profit));
        QDateTime open = parseTradeTime(at(TradeField::OpenTime)), close = parseTradeTime(at(TradeField::CloseTime));
        if (!open.isValid())
            open = close;
        if (symbol.isEmpty() || !open.isValid() || std::isnan(profit)) {
            ++r.skipped;
            continue;
        }
        Trade t;
        t.id = at(TradeField::Ticket);
        if (t.id.isEmpty())
            t.id = QString("csv-%1-%2-%3").arg(symbol, open.toString(Qt::ISODate)).arg(++serial);
        t.symbol = symbol;
        const int side = sideOf(at(TradeField::Side));
        t.isLong = side >= 0;
        t.volume = amountOr0(at(TradeField::Volume));
        t.openTime = open;
        t.closeTime = close.isValid() ? close : open;
        t.openPrice = amountOr0(at(TradeField::OpenPrice));
        t.closePrice = amountOr0(at(TradeField::ClosePrice));
        t.stopLoss = amountOr0(at(TradeField::StopLoss));
        t.takeProfit = amountOr0(at(TradeField::TakeProfit));
        t.commission = amountOr0(at(TradeField::Commission));
        t.swap = amountOr0(at(TradeField::Swap));
        t.profit = profit;
        t.notes = at(TradeField::Comment);
        r.trades.push_back(t);
    }
    if (r.trades.isEmpty())
        r.error = "format";
    return r;
}

ImportResult readStatement(const QByteArray &bytes)
{
    if (bytes.trimmed().isEmpty()) {
        ImportResult r;
        r.error = "empty";
        return r;
    }
    // MT5 writes UTF-16 with a BOM; the others UTF-8 or Latin-1.
    QString text;
    if (bytes.startsWith("\xFF\xFE") || bytes.startsWith("\xFE\xFF")) {
        auto toUtf16 = QStringDecoder(bytes.startsWith("\xFF\xFE") ? QStringDecoder::Utf16LE : QStringDecoder::Utf16BE);
        text = toUtf16(bytes.mid(2));
    } else {
        auto toUtf8 = QStringDecoder(QStringDecoder::Utf8);
        text = toUtf8(bytes.startsWith("\xEF\xBB\xBF") ? bytes.mid(3) : bytes);
        if (toUtf8.hasError())
            text = QString::fromLatin1(bytes);
    }

    if (text.contains(QRegularExpression("<\\s*(table|html)", QRegularExpression::CaseInsensitiveOption))) {
        const QVector<QStringList> rows = htmlRows(text);
        const QString lower = text.toLower();
        ImportResult r;
        if (lower.contains(">positions<") || lower.contains("trade history report") || lower.contains("metatrader 5"))
            r = readMt5(rows);
        if (r.trades.isEmpty())
            r = readMt4(rows);
        if (r.trades.isEmpty())
            r.error = "format";
        return r;
    }

    const QStringList lines = nonEmptyLines(text);
    const QChar delimiter = detectDelimiter(lines.first());
    const QStringList headers = splitCsvLine(lines.first(), delimiter);
    QHash<int, int> mapping;
    for (int i = 0; i < headers.size(); ++i) {
        const int f = fieldOf(headers[i]);
        if (f >= 0 && !mapping.contains(f))
            mapping.insert(f, i);
    }
    ImportResult r = readCsv(text, mapping);
    bool cTrader = false;
    for (const QString &h : headers)
        cTrader = cTrader || normalized(h) == "opening direction" || normalized(h).startsWith("closing quantity");
    if (cTrader)
        r.format = "cTrader CSV";
    r.headers = headers;
    r.mapping = mapping;
    return r;
}

QVector<Trade> mergeTrades(const QVector<Trade> &existing, const QVector<Trade> &incoming, int *added, int *updated)
{
    QVector<Trade> out = existing;
    QHash<QString, int> index;
    for (int i = 0; i < out.size(); ++i)
        index.insert(out[i].id, i);
    int a = 0, u = 0;
    for (const Trade &t : incoming) {
        const auto it = index.find(t.id);
        if (it != index.end()) {
            Trade merged = t;
            // What only the journal knows survives a new import.
            if (merged.strategy.isEmpty())
                merged.strategy = out[it.value()].strategy;
            if (merged.notes.isEmpty())
                merged.notes = out[it.value()].notes;
            out[it.value()] = merged;
            ++u;
        } else {
            index.insert(t.id, int(out.size()));
            out.push_back(t);
            ++a;
        }
    }
    if (added)
        *added = a;
    if (updated)
        *updated = u;
    return out;
}

QVector<Trade> applyFilter(const QVector<Trade> &trades, const Filter &f)
{
    QVector<Trade> out;
    for (const Trade &t : trades) {
        if (!f.symbol.isEmpty() && t.symbol != f.symbol)
            continue;
        if (!f.strategy.isEmpty() && t.strategy != f.strategy)
            continue;
        if ((f.side > 0 && !t.isLong) || (f.side < 0 && t.isLong))
            continue;
        if (f.year && (t.isClosed() ? t.closeTime : t.openTime).date().year() != f.year)
            continue;
        out.push_back(t);
    }
    return out;
}

JournalStats computeStats(const QVector<Trade> &trades, double startingBalance)
{
    JournalStats s;
    QVector<Trade> closed;
    for (const Trade &t : trades)
        if (t.isClosed())
            closed.push_back(t);
    std::sort(closed.begin(), closed.end(), [](const Trade &a, const Trade &b) { return a.closeTime < b.closeTime; });
    s.closed = closed.size();
    if (closed.isEmpty())
        return s;

    double cumulative = 0, peak = 0, peakBalance = startingBalance, hold = 0, sumSq = 0, downSq = 0;
    int streak = 0;
    std::map<QString, Bucket> months, quarters, symbols;
    QVector<Bucket> weekdays(7);
    const char *dayNames[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
    for (int i = 0; i < 7; ++i)
        weekdays[i].label = dayNames[i];

    for (const Trade &t : closed) {
        const double net = t.net();
        s.net += net;
        if (net > 0) {
            ++s.wins;
            s.grossWin += net;
            streak = streak > 0 ? streak + 1 : 1;
        } else if (net < 0) {
            ++s.losses;
            s.grossLoss += -net;
            streak = streak < 0 ? streak - 1 : -1;
        } else {
            ++s.flat;
            streak = 0;
        }
        s.longestWinStreak = std::max(s.longestWinStreak, streak);
        s.longestLossStreak = std::max(s.longestLossStreak, -streak);
        cumulative += net;
        if (cumulative > peak) {
            peak = cumulative;
            peakBalance = startingBalance + cumulative;
        }
        const double dd = peak - cumulative;
        if (dd > s.maxDrawdown) {
            s.maxDrawdown = dd;
            s.maxDrawdownPct = peakBalance > 0 ? dd / peakBalance * 100.0 : 0;
        }
        s.equity.push_back({t.closeTime, cumulative, -dd, net});
        hold += t.holdHours();
        sumSq += net * net;
        if (net < 0)
            downSq += net * net;

        const QDate d = t.closeTime.date();
        const QString month = d.toString("yyyy-MM");
        months[month].label = month;
        months[month].net += net;
        months[month].count++;
        const QString quarter = QString("%1 Q%2").arg(d.year()).arg((d.month() - 1) / 3 + 1);
        quarters[quarter].label = quarter;
        quarters[quarter].net += net;
        quarters[quarter].count++;
        symbols[t.symbol].label = t.symbol;
        symbols[t.symbol].net += net;
        symbols[t.symbol].count++;
        weekdays[d.dayOfWeek() - 1].net += net;
        weekdays[d.dayOfWeek() - 1].count++;
    }
    s.currentStreak = streak;
    const int n = s.closed;
    s.winRate = 100.0 * s.wins / n;
    s.profitFactor = s.grossLoss > 0 ? s.grossWin / s.grossLoss : 0;
    s.avgWin = s.wins ? s.grossWin / s.wins : 0;
    s.avgLoss = s.losses ? s.grossLoss / s.losses : 0;
    s.winLossRatio = s.avgLoss > 0 ? s.avgWin / s.avgLoss : 0;
    s.expectancy = s.net / n;
    s.recoveryFactor = s.maxDrawdown > 0 ? s.net / s.maxDrawdown : 0;
    s.avgHoldHours = hold / n;
    const double variance = sumSq / n - s.expectancy * s.expectancy;
    s.sharpeLike = variance > 0 ? s.expectancy / std::sqrt(variance) : 0;
    s.sortinoLike = downSq > 0 ? s.expectancy / std::sqrt(downSq / n) : 0;

    for (const auto &[k, b] : months) s.byMonth.push_back(b);
    for (const auto &[k, b] : quarters) s.byQuarter.push_back(b);
    s.byWeekday = weekdays;
    for (const auto &[k, b] : symbols) s.bySymbol.push_back(b);
    std::sort(s.bySymbol.begin(), s.bySymbol.end(), [](const Bucket &a, const Bucket &b) { return a.net > b.net; });

    QVector<Trade> sorted = closed;
    std::sort(sorted.begin(), sorted.end(), [](const Trade &a, const Trade &b) { return a.net() > b.net(); });
    for (int i = 0; i < std::min<qsizetype>(5, sorted.size()); ++i) {
        if (sorted[i].net() > 0)
            s.best.push_back(sorted[i]);
        const Trade &w = sorted[sorted.size() - 1 - i];
        if (w.net() < 0)
            s.worst.push_back(w);
    }

    // The score: each measure on 0-100 by a plain scale, then their mean. Stated in the page as this app's own.
    const auto scale = [](double v, double lo, double hi) { return std::clamp((v - lo) / (hi - lo) * 100.0, 0.0, 100.0); };
    int profitableMonths = 0;
    for (const Bucket &b : s.byMonth)
        profitableMonths += b.net > 0;
    s.radar = {scale(s.winRate, 30, 70),
               scale(s.profitFactor, 1, 3),
               scale(s.winLossRatio, 0.5, 2.5),
               scale(s.recoveryFactor, 0, 6),
               startingBalance > 0 ? scale(-s.maxDrawdownPct, -30, 0) : scale(-s.maxDrawdown, -std::max(1.0, s.grossWin), 0),
               s.byMonth.isEmpty() ? 0 : 100.0 * profitableMonths / s.byMonth.size()};
    double sum = 0;
    for (double v : s.radar)
        sum += v;
    s.score = sum / s.radar.size();
    return s;
}

} // namespace desk
