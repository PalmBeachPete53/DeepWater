#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

// A journal of real trades: what they are, how they are read from the statements of MetaTrader 4 and 5 and cTrader
// (CSV or HTML files, no broker connection), and the statistics computed from them. No widgets, no network.
namespace desk {

struct Trade {
    QString id;            // the ticket, or a generated id for a trade logged by hand
    QString symbol;
    bool isLong = true;
    double volume = 0;     // lots
    QDateTime openTime, closeTime;   // closeTime invalid: still open
    double openPrice = 0, closePrice = 0;
    double stopLoss = 0, takeProfit = 0;
    double commission = 0, swap = 0, profit = 0;   // as the statement gives them, in the account's currency
    QString strategy, notes;

    bool isClosed() const { return closeTime.isValid(); }
    double net() const { return profit + commission + swap; }
    double holdHours() const;
    QJsonObject toJson() const;
    static Trade fromJson(const QJsonObject &o);
};

// --- Reading a statement.

enum class TradeField { Ticket, Symbol, Side, Volume, OpenTime, CloseTime, OpenPrice, ClosePrice, StopLoss, TakeProfit, Commission, Swap, Profit, Comment };

struct ImportResult {
    QString format;              // "MetaTrader 4 HTML", "MetaTrader 5 HTML", "cTrader CSV", "CSV", or empty
    QVector<Trade> trades;
    QStringList headers;         // the columns found in a CSV
    QHash<int, int> mapping;     // TradeField -> column, for a CSV
    int skipped = 0;             // rows that aren't closed trades (balance lines, orders, broken rows)
    QString error;               // untranslated key when nothing could be read: "empty", "format", "columns"
};

// Reads the bytes of a statement, whichever it is: the encoding (MT5 writes UTF-16) and the format are detected.
ImportResult readStatement(const QByteArray &bytes);
// A CSV read with a mapping the user corrected in the preview.
ImportResult readCsv(const QString &text, const QHash<int, int> &mapping);
// "1 234,56", "1,234.56", "-12.5", "(12.50)", "$1,234": NaN when it isn't a number.
double parseAmount(const QString &text);
// "2026.01.15 10:30:00", "2026-01-15 10:30", "15/01/2026 10:30:00.123": UTC, invalid when unreadable.
QDateTime parseTradeTime(const QString &text);

// --- Merging: a trade with a known ticket is replaced, so importing a statement twice adds nothing.
QVector<Trade> mergeTrades(const QVector<Trade> &existing, const QVector<Trade> &incoming, int *added = nullptr, int *updated = nullptr);

// --- A trade described to a model. Models invent figures even when told not to, so what reaches the form is checked.

// The numbers written in a text, each as it can be read: "1,1225" and "1.1225" both give 1.1225; "0,5" gives 0.5.
QVector<double> numbersInText(const QString &text);
// Keeps only what `question` states: a figure or a text it doesn't hold is emptied, and named in the result by its
// field ("take_profit"). An open trade has no exit, so no close price or profit; a lone price given as its exit is its
// entry. Without a question there is nothing to compare with, and nothing changes.
QStringList keepStated(Trade &trade, const QString &question);

// --- Statistics of closed trades.

struct Filter {
    QString symbol, strategy;   // empty: all
    int side = 0;               // 0 all, 1 long, -1 short
    int year = 0;               // 0 all
};
QVector<Trade> applyFilter(const QVector<Trade> &trades, const Filter &f);

struct Bucket {
    QString label;
    double net = 0;
    int count = 0;
};
struct EquityPoint {
    QDateTime time;
    double cumulative = 0, drawdown = 0, tradeNet = 0;
};

struct JournalStats {
    int closed = 0, wins = 0, losses = 0, flat = 0;
    double net = 0, grossWin = 0, grossLoss = 0;
    double winRate = 0;           // %
    double profitFactor = 0;      // 0 when there is no loss to divide by
    double avgWin = 0, avgLoss = 0, winLossRatio = 0;
    double expectancy = 0;        // net per trade
    double maxDrawdown = 0, maxDrawdownPct = 0;   // positive amounts; the % is of the balance at the peak
    double recoveryFactor = 0;
    double sharpeLike = 0, sortinoLike = 0;       // per trade, not annualised
    int longestWinStreak = 0, longestLossStreak = 0, currentStreak = 0;
    double avgHoldHours = 0;
    double score = 0;             // 0-100, the mean of `radar`: this app's own, not a standard
    QVector<double> radar;        // win %, profit factor, avg win/loss, recovery, drawdown, consistency: 0-100
    QVector<EquityPoint> equity;
    QVector<Bucket> byMonth, byQuarter, byWeekday, bySymbol;
    QVector<Trade> best, worst;   // five each, best first / worst first
};
JournalStats computeStats(const QVector<Trade> &trades, double startingBalance);

} // namespace desk
