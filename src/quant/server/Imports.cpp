#include "Imports.h"

#include "../core/Error.h"
#include "../core/Series.h"
#include "Json.h"
#include "Store.h"

#include <QDir>
#include <QFile>
#include <QMap>
#include <QRegularExpression>

#include <algorithm>
#include <cmath>
#include <optional>

namespace qtm {

static Error bad(const QString &message, int status = 400)
{
    return Error(message, status);
}

namespace imports {

// "btc 1m (Alpaca).csv" -> "BTC_1M_ALPACA": safe as a file name and as a ticker.
QString cleanName(const QString &raw)
{
    static const QRegularExpression ext(QStringLiteral("\\.(csv|txt)\\z"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression other(QStringLiteral("[^A-Z0-9]+"));
    static const QRegularExpression edges(QStringLiteral("^_+|_+$"));
    QString n = raw;
    n.replace(ext, QString());
    n = n.toUpper();
    n.replace(other, "_");
    n.replace(edges, QString());
    n = n.left(30);
    if (n.isEmpty()) throw bad("Give the file a name made of letters or digits");
    return n;
}

// ---------- CSV ----------
using Row = QStringList;

static QVector<Row> rowsOf(const QString &text)
{
    QString t = text;
    if (t.startsWith(QChar(0xFEFF))) t = t.mid(1);
    static const QRegularExpression nl(QStringLiteral("\\r?\\n"));
    QStringList lines;
    for (const QString &l : t.split(nl))
        if (!jsv::trim(l).isEmpty() && !l.startsWith('#')) lines << l;
    if (lines.isEmpty()) throw bad("The file is empty");
    const QString first = lines[0];
    const QString delims[] = { ",", ";", "\t" };
    QString delim = delims[0];
    qsizetype best = first.split(delims[0]).size();
    for (const QString &d : delims) {
        const qsizetype c = first.split(d).size();
        if (c > best) { best = c; delim = d; }
    }
    static const QRegularExpression quoted(QStringLiteral("^\"(.*)\"$"));
    QVector<Row> rows;
    for (const QString &l : lines) {
        Row r;
        for (const QString &c : l.split(delim)) {
            QString cell = jsv::trim(c);
            cell.replace(quoted, "\\1");
            r << cell;
        }
        rows.push_back(r);
    }
    return rows;
}

static QString norm(const QString &h)
{
    static const QRegularExpression re(QStringLiteral("[^a-z0-9]"));
    QString s = h.toLower();
    s.replace(re, QString());
    return s;
}

using Columns = QHash<QString, int>;

static Columns columns(const Row &header)
{
    static const QVector<QPair<QString, QStringList>> ALIASES = {
        { "time", { "time", "timestamp", "datetime", "date", "t", "opentime", "datetimeutc", "dateutc", "timeutc", "ts", "unix", "barstart" } },
        { "open", { "open", "o", "openprice" } },
        { "high", { "high", "h", "highprice" } },
        { "low", { "low", "l", "lowprice" } },
        { "close", { "close", "c", "closeprice", "last", "price" } },
        { "volume", { "volume", "v", "vol", "basevolume" } },
        { "position", { "position", "pos", "signal", "side", "target", "direction", "value" } },
        { "symbol", { "symbol", "ticker" } },
    };
    QStringList h;
    for (const auto &x : header) h << norm(x);
    Columns col;
    for (const auto &[k, names] : ALIASES) {
        for (const auto &n : names) {
            const int i = int(h.indexOf(n));
            if (i >= 0) {
                col.insert(k, i);
                break;
            }
        }
    }
    // MetaTrader-style exports: a date column and a separate time-of-day column.
    if (h.contains("date") && h.contains("time")) {
        col.insert("time", int(h.indexOf("date")));
        col.insert("timeOfDay", int(h.indexOf("time")));
    }
    return col;
}

static std::optional<QString> cellAt(const Row &row, const Columns &col, const QString &key)
{
    if (!col.contains(key)) return std::nullopt;
    const int i = col.value(key);
    if (i < 0 || i >= row.size()) return std::nullopt;
    return row[i];
}
// String(row[i]): a missing cell is "undefined".
static QString cellStr(const Row &row, const Columns &col, const QString &key)
{
    return cellAt(row, col, key).value_or(QStringLiteral("undefined"));
}
// Number(row[i]): a missing cell is NaN.
static double cellNum(const Row &row, const Columns &col, const QString &key)
{
    const auto c = cellAt(row, col, key);
    return c ? jsv::toNumber(*c) : NAN;
}

// Seconds since 1970 (UTC). Unix seconds, milliseconds, microseconds or nanoseconds; ISO 8601 with
// or without a zone (no zone means UTC); dates with -, / or . between year, month and day.
double parseTime(const QString &cell, const QString *timeOfDay)
{
    const QString s = jsv::trim(cell);
    const bool tod = timeOfDay && !timeOfDay->isEmpty();
    static const QRegularExpression numeric(QStringLiteral("^\\d+(\\.\\d+)?$"));
    if (numeric.match(s).hasMatch() && !tod) {
        const double n = jsv::toNumber(s);
        if (n > 1e17) return std::floor(n / 1e9);
        if (n > 1e14) return std::floor(n / 1e6);
        if (n > 1e11) return std::floor(n / 1e3);
        if (n > 1e8) return std::floor(n);
    }
    static const QRegularExpression re(
        QStringLiteral("^(\\d{4})[-/.](\\d{1,2})[-/.](\\d{1,2})(?:[T ]+(\\d{1,2}):(\\d{2})(?::(\\d{2})(?:\\.\\d+)?)?)?[" QTM_JS_SPACE "]*(Z|[+-]\\d{2}:?\\d{2}|UTC)?$"),
        QRegularExpression::CaseInsensitiveOption);
    const auto m = re.match(tod ? s + ' ' + jsv::trim(*timeOfDay) : s);
    if (!m.hasMatch()) return NAN;
    auto part = [&](int i, const char *def) {
        const QString v = m.captured(i);
        return jsv::toNumber(v.isEmpty() ? QString::fromLatin1(def) : v);
    };
    double t = jsv::dateUtc(part(1, "0"), part(2, "0") - 1, part(3, "0"), part(4, "0"), part(5, "0"), part(6, "0")) / 1000;
    const QString zone = m.captured(7);
    static const QRegularExpression utc(QStringLiteral("^(Z|UTC)$"), QRegularExpression::CaseInsensitiveOption);
    if (!zone.isEmpty() && !utc.match(zone).hasMatch()) {
        static const QRegularExpression z(QStringLiteral("([+-])(\\d{2}):?(\\d{2})"));
        const auto zm = z.match(zone);
        t -= (zm.captured(1) == "-" ? -1 : 1) * (zm.captured(2).toInt() * 3600 + zm.captured(3).toInt() * 60);
    }
    return t;
}

static const QString TIME_HELP = QStringLiteral(
    "Use a time like 2026-10-14 14:30:00 (UTC), an ISO time like 2026-10-14T14:30:00Z, or Unix seconds.");

QString tfLabel(double sec)
{
    if (std::fmod(sec, 2592000) == 0) return jsv::numberToString(sec / 2592000) + "M";
    if (std::fmod(sec, 604800) == 0) return jsv::numberToString(sec / 604800) + "W";
    if (std::fmod(sec, 86400) == 0) return jsv::numberToString(sec / 86400) + "D";
    if (std::fmod(sec, 3600) == 0) return jsv::numberToString(sec / 3600) + "h";
    if (std::fmod(sec, 60) == 0) return jsv::numberToString(sec / 60) + "m";
    return jsv::numberToString(sec) + "s";
}

static double parseRowTime(const Row &row, const Columns &col)
{
    const auto tod = cellAt(row, col, "timeOfDay");
    return parseTime(cellStr(row, col, "time"), tod ? &*tod : nullptr);
}

// Bars from CSV text. Throws with the row number and what to change when a row can't be read.
ParsedBars parseBars(const QString &text)
{
    const QVector<Row> rows = rowsOf(text);
    Columns col = columns(rows[0]);
    int start = 1;
    if (!col.contains("time") || !col.contains("close")) {
        if (std::isfinite(parseTime(rows[0][0])) && rows[0].size() >= 5) {
            col = { { "time", 0 }, { "open", 1 }, { "high", 2 }, { "low", 3 }, { "close", 4 } };
            if (rows[0].size() > 5) col.insert("volume", 5);
            start = 0;
        } else {
            throw bad("Couldn't find the time and close columns. Name them time, open, high, low, close and volume in the first row.");
        }
    }
    QStringList symbols;
    Bars bars;
    int skipped = 0;
    for (int r = start; r < rows.size(); ++r) {
        const Row &row = rows[r];
        const double time = parseRowTime(row, col);
        if (!std::isfinite(time))
            throw bad(QStringLiteral("Row %1: couldn't read the time \"%2\". %3")
                          .arg(r + 1).arg(cellStr(row, col, "time").left(40), TIME_HELP));
        const double close = cellNum(row, col, "close");
        const double open = col.contains("open") ? cellNum(row, col, "open") : close;
        const double high = col.contains("high") ? cellNum(row, col, "high") : std::max(open, close);
        const double low = col.contains("low") ? cellNum(row, col, "low") : std::min(open, close);
        const auto closeCell = cellAt(row, col, "close");
        if (!std::isfinite(open) || !std::isfinite(high) || !std::isfinite(low) || !std::isfinite(close)
            || (closeCell && closeCell->isEmpty())) {
            ++skipped;
            continue;
        }
        const auto sym = cellAt(row, col, "symbol");
        if (sym && !sym->isEmpty() && !symbols.contains(*sym)) symbols << *sym;
        Bar b;
        b.time = time;
        b.open = open;
        // JS Math.max/min: NaN can't get here (checked above).
        b.high = std::max({ high, open, close });
        b.low = std::min({ low, open, close });
        b.close = close;
        if (col.contains("volume")) {
            const double v = cellNum(row, col, "volume");
            b.volume = (v != 0 && !std::isnan(v)) ? v : 0;
        }
        bars.push_back(b);
    }
    if (symbols.size() > 1)
        throw bad(QStringLiteral("The file has %1 symbols (%2...). Import one symbol per file.")
                      .arg(symbols.size()).arg(symbols.mid(0, 3).join(", ")));
    std::stable_sort(bars.begin(), bars.end(), [](const Bar &a, const Bar &b) { return a.time < b.time; });
    Bars out;
    for (const Bar &b : bars) {
        if (!out.isEmpty() && b.time == out.last().time) out.last() = b;
        else out.push_back(b);
    }
    if (out.size() < 10)
        throw bad(QStringLiteral("Only %1 usable bars in the file. It needs at least 10.").arg(out.size()));
    // The file's own timeframe: the most common gap between bars.
    QMap<double, int> gaps;
    for (qsizetype i = 1; i < std::min<qsizetype>(out.size(), 5000); ++i)
        gaps[out[i].time - out[i - 1].time] += 1;
    double base = 0;
    int bestCount = -1;
    for (auto it = gaps.begin(); it != gaps.end(); ++it)   // ascending gap: the first max wins ties
        if (it.value() > bestCount) { bestCount = it.value(); base = it.key(); }
    return { out, base, skipped };
}

// ---------- signals ----------
// Time and position per row; only the changes are kept (the position holds until the next one).
Signals parseSignals(const QString &text)
{
    const QVector<Row> rows = rowsOf(text);
    Columns col = columns(rows[0]);
    int start = 1;
    if (!col.contains("time") || !col.contains("position")) {
        if (std::isfinite(parseTime(rows[0][0])) && rows[0].size() >= 2) {
            col = { { "time", 0 }, { "position", int(rows[0].size() - 1) } };
            start = 0;
        } else {
            throw bad("Couldn't find the time and position columns. Name them time and position in the first row.");
        }
    }
    static const QHash<QString, double> POSITION_WORDS = {
        { "long", 1 }, { "buy", 1 }, { "short", -1 }, { "sell", -1 },
        { "flat", 0 }, { "none", 0 }, { "exit", 0 }, { "close", 0 },
    };
    QVector<QPair<double, double>> pts;
    for (int r = start; r < rows.size(); ++r) {
        const Row &row = rows[r];
        const double time = parseRowTime(row, col);
        if (!std::isfinite(time))
            throw bad(QStringLiteral("Row %1: couldn't read the time \"%2\". %3")
                          .arg(r + 1).arg(cellStr(row, col, "time").left(40), TIME_HELP));
        const QString cell = jsv::trim(cellAt(row, col, "position").value_or(QString())).toLower();
        const double pos = POSITION_WORDS.contains(cell) ? POSITION_WORDS.value(cell) : jsv::toNumber(cell);
        if (!std::isfinite(pos) || pos < -1 || pos > 1)
            throw bad(QStringLiteral("Row %1: the position \"%2\" isn't one of 1 (long), 0 (flat) or -1 (short).")
                          .arg(r + 1).arg(cellStr(row, col, "position").left(20)));
        pts.push_back({ time, pos });
    }
    std::stable_sort(pts.begin(), pts.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
    Signals s;
    for (const auto &p : pts)
        if (s.changes.isEmpty() || s.changes.last().second != p.second) s.changes.push_back(p);
    if (pts.isEmpty()) throw bad("The file has no rows");
    s.rows = int(pts.size());
    s.from = pts.first().first;
    s.to = pts.last().first;
    return s;
}

// The indicator file a signal becomes. Each bar takes the last position set before the next bar
// starts, so on a longer timeframe it's the position at the bar's close.
QString signalIndicatorSource(const QString &name, const Signals &s, const QString &fileName)
{
    QString from = fileName.isEmpty() ? name : fileName;
    from.replace(QRegularExpression(QStringLiteral("[\\r\\n]")), " ");
    QVector<double> T, P;
    for (const auto &c : s.changes) {
        T << c.first;
        P << c.second;
    }
    return QStringLiteral(
               "// Position signal imported from %1 on %2.\n"
               "// +1 long, 0 flat, -1 short, each holding until the next change. Import the CSV again to replace it.\n"
               "const T = %3;\n"
               "const P = %4;\n"
               "\n"
               "export default {\n"
               "  name: %5,\n"
               "  overlay: false,\n"
               "  inputs: [],\n"
               "  outputs: [{ key: \"position\", label: \"Position\", kind: \"histogram\", color: \"#2962ff\" }],\n"
               "  levels: [0],\n"
               "  calc(bars) {\n"
               "    const position = new Array(bars.length).fill(NaN);\n"
               "    const colors = new Array(bars.length).fill(\"#787b86\");\n"
               "    let k = -1;\n"
               "    for (let i = 0; i < bars.length; i++) {\n"
               "      const end = i + 1 < bars.length ? bars[i + 1].time : bars[i].time + (i ? bars[i].time - bars[i - 1].time : 86400);\n"
               "      while (k + 1 < T.length && T[k + 1] < end) k++;\n"
               "      if (k < 0) continue;\n"
               "      position[i] = P[k];\n"
               "      colors[i] = P[k] > 0 ? \"#089981\" : P[k] < 0 ? \"#f23645\" : \"#787b86\";\n"
               "    }\n"
               "    return { position, colors };\n"
               "  },\n"
               "};\n")
        .arg(from, jsv::isoNow().left(10), jsv::numbersJson(T), jsv::numbersJson(P),
             jsv::quote(QStringLiteral("Signal: ") + name));
}

// The tester rules that trade a signal exactly: long while it's above 0, short while it's below.
QJsonObject signalStrategy(const QString &id)
{
    auto rule = [](const char *op) {
        return QJsonArray { QJsonObject { { "left", "signal" }, { "op", op }, { "right", 0 } } };
    };
    return QJsonObject {
        { "indicators", QJsonObject { { "signal", QJsonObject { { "id", id } } } } },
        { "entryLong", rule(">") },
        { "exitLong", rule("<=") },
        { "entryShort", rule("<") },
        { "exitShort", rule(">=") },
    };
}

} // namespace imports

// ---------- bars: store, list, chart ----------
Imports::Imports(const QString &dataDir) : m_dir(QDir(dataDir).filePath("imports")) {}

std::shared_ptr<Imports::Parsed> Imports::load(const QString &name)
{
    const QString file = QDir(m_dir).filePath(name + ".csv");
    const double mtime = mtimeMs(file);
    if (mtime < 0)
        throw bad(QStringLiteral("There's no imported file called %1. Import it from the strategy tester's \"Your own data\" row.").arg(name), 404);
    {
        QMutexLocker lock(&m_mutex);
        const auto hit = m_parsed.value(name);
        if (hit && hit->mtime == mtime) return hit;
    }
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly))
        throw bad(QStringLiteral("There's no imported file called %1. Import it from the strategy tester's \"Your own data\" row.").arg(name), 404);
    auto p = std::make_shared<Parsed>();
    p->mtime = mtime;
    p->p = imports::parseBars(QString::fromUtf8(f.readAll()));
    QMutexLocker lock(&m_mutex);
    m_parsed.insert(name, p);
    return p;
}

static QJsonObject summary(const QString &name, const ParsedBars &p)
{
    return {
        { "name", name },
        { "symbol", "CSV:" + name },
        { "bars", int(p.bars.size()) },
        { "tf", imports::tfLabel(p.baseSeconds) },
        { "from", p.bars.first().time },
        { "to", p.bars.last().time },
        { "skipped", p.skipped },
    };
}

QJsonObject Imports::importBars(const QString &rawName, const QString &text)
{
    const QString name = imports::cleanName(rawName);
    const ParsedBars p = imports::parseBars(text);
    QDir().mkpath(m_dir);
    QFile f(QDir(m_dir).filePath(name + ".csv"));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        throw Error(QStringLiteral("Couldn't save %1: %2").arg(f.fileName(), f.errorString()));
    f.write(text.toUtf8());
    f.close();
    {
        QMutexLocker lock(&m_mutex);
        m_parsed.remove(name);
    }
    return summary(name, p);
}

QJsonArray Imports::listImports()
{
    bool exists = false;
    const QStringList names = readDir(m_dir, &exists);
    QJsonArray out;
    for (const QString &f : names) {
        if (!f.endsWith(".csv")) continue;
        const QString name = f.left(f.size() - 4);
        try {
            out.append(summary(name, load(name)->p));
        } catch (const Error &e) {
            out.append(QJsonObject { { "name", name }, { "symbol", "CSV:" + name }, { "error", e.message() } });
        }
    }
    return out;
}

CandleResult Imports::csvCandles(const QString &name, const QString &tf, int limit)
{
    const auto p = load(name);
    const double want = double(parseTf(tf).seconds);
    if (want < p->p.baseSeconds)
        throw bad(QStringLiteral("%1 has %2 bars, so it charts at %2 and longer.").arg(name, imports::tfLabel(p->p.baseSeconds)));
    Bars built;
    {
        QMutexLocker lock(&m_mutex);
        if (!p->byTf.contains(tf)) p->byTf.insert(tf, resample(p->p.bars, tf));
        built = p->byTf.value(tf);
    }
    CandleResult r;
    r.symbol = "CSV:" + name;
    r.tf = tf;
    r.provider = QStringLiteral("Your file %1.csv").arg(name);
    r.candles = built.mid(std::max<qsizetype>(0, built.size() - limit));
    r.notes << (want > p->p.baseSeconds
                    ? QStringLiteral("Your own bars from %1.csv, built from its %2 bars.").arg(name, imports::tfLabel(p->p.baseSeconds))
                    : QStringLiteral("Your own bars from %1.csv.").arg(name));
    return r;
}

} // namespace qtm
