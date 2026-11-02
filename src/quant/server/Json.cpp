#include "Json.h"

#include "../core/Error.h"

#include <QDate>
#include <QDateTime>
#include <QHash>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QTime>
#include <QTimeZone>
#include <QUrl>

#include <algorithm>
#include <charconv>
#include <cmath>

namespace qtm::jsv {

// ---------------------------------------------------------------- values

static bool isJsSpace(QChar c)
{
    const char16_t u = c.unicode();
    return u == 0x09 || u == 0x0A || u == 0x0B || u == 0x0C || u == 0x0D || u == 0x20 || u == 0xA0
           || u == 0x1680 || (u >= 0x2000 && u <= 0x200A) || u == 0x2028 || u == 0x2029
           || u == 0x202F || u == 0x205F || u == 0x3000 || u == 0xFEFF;
}

QString trim(const QString &s)
{
    qsizetype a = 0, b = s.size();
    while (a < b && isJsSpace(s[a])) ++a;
    while (b > a && isJsSpace(s[b - 1])) --b;
    return s.mid(a, b - a);
}

double toNumber(const QString &raw)
{
    const QString s = trim(raw);
    if (s.isEmpty())
        return 0;
    static const QRegularExpression dec(
        QStringLiteral("^[+-]?(?:\\d+\\.?\\d*(?:[eE][+-]?\\d+)?|\\.\\d+(?:[eE][+-]?\\d+)?)$"));
    if (dec.match(s).hasMatch()) {
        bool ok = false;
        const double v = s.toLatin1().toDouble(&ok);
        if (ok)
            return v;
        // Out of double range: JS gives +-Infinity (or 0 for tiny values).
        const bool neg = s.startsWith('-');
        const QString mant = s.section(QRegularExpression("[eE]"), 0, 0);
        bool allZero = true;
        for (QChar c : mant)
            if (c.isDigit() && c != '0') allZero = false;
        if (allZero) return neg ? -0.0 : 0.0;
        const int e = s.contains(QRegularExpression("[eE]-")) ? -1 : 1;
        if (e < 0) return neg ? -0.0 : 0.0;
        return neg ? -INFINITY : INFINITY;
    }
    if (s == QLatin1String("Infinity") || s == QLatin1String("+Infinity"))
        return INFINITY;
    if (s == QLatin1String("-Infinity"))
        return -INFINITY;
    static const QRegularExpression radix(QStringLiteral("^0([xXoObB])([0-9a-fA-F]+)$"));
    const auto m = radix.match(s);
    if (m.hasMatch()) {
        const QChar k = m.captured(1).toLower()[0];
        const int base = k == 'x' ? 16 : k == 'o' ? 8 : 2;
        double v = 0;
        for (QChar c : m.captured(2)) {
            const int d = c.isDigit() ? c.unicode() - '0' : c.toLower().unicode() - 'a' + 10;
            if (d >= base)
                return NAN;
            v = v * base + d;
        }
        return v;
    }
    return NAN;
}

double toNumber(const QJsonValue &v)
{
    switch (v.type()) {
    case QJsonValue::Double:
        return v.toDouble();
    case QJsonValue::String:
        return toNumber(v.toString());
    case QJsonValue::Bool:
        return v.toBool() ? 1 : 0;
    case QJsonValue::Null:
        return 0;
    case QJsonValue::Array: {
        // [] -> 0, [x] -> Number(String(x)), more -> NaN (via String(array)).
        return toNumber(toString(v));
    }
    case QJsonValue::Object:
    case QJsonValue::Undefined:
    default:
        return NAN;
    }
}

double parseFloat(const QString &raw)
{
    QString s = raw;
    qsizetype a = 0;
    while (a < s.size() && isJsSpace(s[a])) ++a;
    s = s.mid(a);
    static const QRegularExpression re(
        QStringLiteral("^[+-]?(?:Infinity|(?:\\d+\\.?\\d*|\\.\\d+)(?:[eE][+-]?\\d+)?)"));
    const auto m = re.match(s);
    if (!m.hasMatch())
        return NAN;
    QString t = m.captured(0);
    if (t.endsWith(QLatin1String("Infinity")))
        return t.startsWith('-') ? -INFINITY : INFINITY;
    return toNumber(t);
}

QString numberToString(double d)
{
    if (std::isnan(d))
        return QStringLiteral("NaN");
    if (std::isinf(d))
        return d > 0 ? QStringLiteral("Infinity") : QStringLiteral("-Infinity");
    if (d == 0)
        return QStringLiteral("0");
    char buf[64];
    auto res = std::to_chars(buf, buf + sizeof buf, std::fabs(d), std::chars_format::scientific);
    std::string sci(buf, res.ptr);
    // "d.ddddde+XX"
    const auto ePos = sci.find('e');
    std::string mant = sci.substr(0, ePos);
    const int exp = std::stoi(sci.substr(ePos + 1));
    std::string digits;
    for (char c : mant)
        if (c != '.') digits += c;
    while (digits.size() > 1 && digits.back() == '0') digits.pop_back();
    const int k = int(digits.size());
    const int n = exp + 1;
    std::string out;
    if (k <= n && n <= 21) {
        out = digits + std::string(n - k, '0');
    } else if (0 < n && n <= 21) {
        out = digits.substr(0, n) + "." + digits.substr(n);
    } else if (-6 < n && n <= 0) {
        out = "0." + std::string(-n, '0') + digits;
    } else {
        out = digits.substr(0, 1);
        if (k > 1) out += "." + digits.substr(1);
        out += "e";
        out += (n - 1 >= 0) ? "+" : "-";
        out += std::to_string(std::abs(n - 1));
    }
    if (d < 0)
        out = "-" + out;
    return QString::fromLatin1(out.c_str());
}

QString toString(const QJsonValue &v)
{
    switch (v.type()) {
    case QJsonValue::Double:
        return numberToString(v.toDouble());
    case QJsonValue::String:
        return v.toString();
    case QJsonValue::Bool:
        return v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    case QJsonValue::Null:
        return QStringLiteral("null");
    case QJsonValue::Array: {
        QStringList parts;
        for (const QJsonValue x : v.toArray())
            parts << (nullish(x) ? QString() : toString(x));
        return parts.join(',');
    }
    case QJsonValue::Object:
        return QStringLiteral("[object Object]");
    case QJsonValue::Undefined:
    default:
        return QStringLiteral("undefined");
    }
}

bool truthy(const QJsonValue &v)
{
    switch (v.type()) {
    case QJsonValue::Double: {
        const double d = v.toDouble();
        return d != 0 && !std::isnan(d);
    }
    case QJsonValue::String:
        return !v.toString().isEmpty();
    case QJsonValue::Bool:
        return v.toBool();
    case QJsonValue::Array:
    case QJsonValue::Object:
        return true;
    default:
        return false;
    }
}

bool nullish(const QJsonValue &v)
{
    return v.isNull() || v.isUndefined();
}

QJsonValue num(double d)
{
    return QJsonValue(d);
}

// ---------------------------------------------------------------- JSON.stringify

QString quote(const QString &s)
{
    QString out;
    out.reserve(s.size() + 2);
    out += '"';
    const qsizetype n = s.size();
    for (qsizetype i = 0; i < n; ++i) {
        const char16_t c = s[i].unicode();
        switch (c) {
        case '"': out += QLatin1String("\\\""); break;
        case '\\': out += QLatin1String("\\\\"); break;
        case '\b': out += QLatin1String("\\b"); break;
        case '\f': out += QLatin1String("\\f"); break;
        case '\n': out += QLatin1String("\\n"); break;
        case '\r': out += QLatin1String("\\r"); break;
        case '\t': out += QLatin1String("\\t"); break;
        default:
            if (c < 0x20) {
                out += QStringLiteral("\\u%1").arg(int(c), 4, 16, QChar('0'));
            } else if (c >= 0xD800 && c <= 0xDBFF) {
                if (i + 1 < n && s[i + 1].unicode() >= 0xDC00 && s[i + 1].unicode() <= 0xDFFF) {
                    out += s[i];
                    out += s[i + 1];
                    ++i;
                } else {
                    out += QStringLiteral("\\u%1").arg(int(c), 4, 16, QChar('0'));
                }
            } else if (c >= 0xDC00 && c <= 0xDFFF) {
                out += QStringLiteral("\\u%1").arg(int(c), 4, 16, QChar('0'));
            } else {
                out += s[i];
            }
        }
    }
    out += '"';
    return out;
}

// QJsonObject keeps its keys sorted. Node prints them in the order the JS built them, so the keys
// of the common response shapes go first, in their JS order; any other key follows, sorted.
static int keyRank(const QString &k)
{
    static const QHash<QString, int> rank = [] {
        const char *order[] = {
            "ok", "app", "root", "version", "node", "platform", "providers", "feeds", "yahoo",
            "id", "alertId", "ticker", "symbol", "watchlist", "name", "file", "type", "exchange",
            "url", "site", "licensed", "leaseExpires", "connected", "reason", "engine",
            "tf", "provider", "bars", "from", "to", "skipped", "rows", "changes", "pass", "passes",
            "left", "op", "right", "conditions", "match", "indicators", "trigger", "webhook",
            "message", "active", "createdAt", "lastBar", "fired", "lastFiredAt", "lastBars",
            "text", "content", "price", "changePct", "bid", "ask", "spreadPct", "source",
            "time", "open", "high", "low", "close", "volume", "marketCap", "sector", "industry",
            "country", "live", "status", "barTime", "at", "mtime", "error", "candles", "notes",
            "stats", "trades", "equity", "quantChecks", "strategy", "alerts",
            "title", "date", "impact", "forecast", "previous", "when", "epsForecast", "quarter",
            "lastYearEps", "exDate", "payDate", "recordDate", "amount", "annual", "items", "note",
            "ago", "summary", "frequency", "nextEarnings", "income", "balance", "cashflow",
            "ratios", "periods", "label", "values", "value", "inputs", "outputs", "default",
            "options", "passed", "total", "gates", "walkForward", "monteCarlo", "ms",
        };
        QHash<QString, int> h;
        int i = 0;
        for (const char *k : order)
            h.insert(QString::fromLatin1(k), i++);
        return h;
    }();
    return rank.value(k, 1 << 20);
}

static void write(QString &out, const QJsonValue &v, int indent, int depth)
{
    auto nl = [&](int d) {
        if (indent > 0) {
            out += '\n';
            out += QString(d * indent, ' ');
        }
    };
    switch (v.type()) {
    case QJsonValue::Double: {
        const double d = v.toDouble();
        out += std::isfinite(d) ? numberToString(d) : QStringLiteral("null");
        break;
    }
    case QJsonValue::String:
        out += quote(v.toString());
        break;
    case QJsonValue::Bool:
        out += v.toBool() ? QLatin1String("true") : QLatin1String("false");
        break;
    case QJsonValue::Array: {
        const QJsonArray a = v.toArray();
        if (a.isEmpty()) {
            out += QLatin1String("[]");
            break;
        }
        out += '[';
        bool first = true;
        for (const auto &x : a) {
            if (!first) out += ',';
            first = false;
            nl(depth + 1);
            if (x.isUndefined()) out += QLatin1String("null");
            else write(out, x, indent, depth + 1);
        }
        nl(depth);
        out += ']';
        break;
    }
    case QJsonValue::Object: {
        const QJsonObject o = v.toObject();
        QStringList keys;
        for (auto it = o.begin(); it != o.end(); ++it)
            if (!it.value().isUndefined()) keys << it.key();
        if (keys.isEmpty()) {
            out += QLatin1String("{}");
            break;
        }
        std::stable_sort(keys.begin(), keys.end(), [](const QString &a, const QString &b) {
            return keyRank(a) < keyRank(b);
        });
        out += '{';
        bool first = true;
        for (const auto &k : keys) {
            if (!first) out += ',';
            first = false;
            nl(depth + 1);
            out += quote(k);
            out += indent > 0 ? QLatin1String(": ") : QLatin1String(":");
            write(out, o.value(k), indent, depth + 1);
        }
        nl(depth);
        out += '}';
        break;
    }
    case QJsonValue::Null:
    default:
        out += QLatin1String("null");
    }
}

QByteArray stringify(const QJsonValue &v, int indent)
{
    QString out;
    write(out, v, indent, 0);
    return out.toUtf8();
}

QString numbersJson(const QVector<double> &values)
{
    QString out = QStringLiteral("[");
    for (qsizetype i = 0; i < values.size(); ++i) {
        if (i) out += ',';
        out += std::isfinite(values[i]) ? numberToString(values[i]) : QStringLiteral("null");
    }
    out += ']';
    return out;
}

QJsonValue parse(const QByteArray &text, bool *ok)
{
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(text, &err);
    if (err.error == QJsonParseError::NoError) {
        if (ok) *ok = true;
        if (doc.isArray()) return doc.array();
        if (doc.isObject()) return doc.object();
        return QJsonValue();
    }
    // Scalars ("x", 5, null): parse them inside an array.
    QJsonParseError err2;
    const QJsonDocument wrapped = QJsonDocument::fromJson("[" + text + "]", &err2);
    if (err2.error == QJsonParseError::NoError && wrapped.isArray() && wrapped.array().size() == 1
        && !text.trimmed().isEmpty()) {
        if (ok) *ok = true;
        return wrapped.array().at(0);
    }
    if (ok) *ok = false;
    return QJsonValue(QJsonValue::Undefined);
}

// ---------------------------------------------------------------- key order scanner

namespace {
struct Scanner {
    const QByteArray &s;
    qsizetype i = 0;
    bool bad = false;

    void ws()
    {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) ++i;
    }
    QString string()
    {
        // s[i] == '"'
        const qsizetype start = ++i;
        bool escaped = false;
        while (i < s.size()) {
            const char c = s[i];
            if (c == '\\') { escaped = true; i += 2; continue; }
            if (c == '"') break;
            ++i;
        }
        const QByteArray raw = s.mid(start, i - start);
        ++i;
        if (!escaped)
            return QString::fromUtf8(raw);
        bool ok = false;
        const QJsonValue v = parse("\"" + raw + "\"", &ok);
        return v.toString();
    }
    void skip()
    {
        ws();
        if (i >= s.size()) { bad = true; return; }
        const char c = s[i];
        if (c == '"') { string(); return; }
        if (c == '{' || c == '[') {
            const char close = c == '{' ? '}' : ']';
            ++i;
            ws();
            if (i < s.size() && s[i] == close) { ++i; return; }
            while (i < s.size() && !bad) {
                ws();
                if (c == '{') {
                    if (i >= s.size() || s[i] != '"') { bad = true; return; }
                    string();
                    ws();
                    if (i >= s.size() || s[i] != ':') { bad = true; return; }
                    ++i;
                }
                skip();
                ws();
                if (i < s.size() && s[i] == ',') { ++i; continue; }
                if (i < s.size() && s[i] == close) { ++i; return; }
                bad = true;
            }
            return;
        }
        while (i < s.size() && s[i] != ',' && s[i] != '}' && s[i] != ']' && s[i] != ' '
               && s[i] != '\n' && s[i] != '\r' && s[i] != '\t')
            ++i;
    }
    // Walks down `path`; at its end returns the keys of the object found there.
    QStringList keysAt(const QStringList &path, int depth)
    {
        ws();
        if (i >= s.size()) return {};
        if (depth == path.size()) {
            if (s[i] != '{') return {};
            QStringList keys;
            ++i;
            ws();
            if (i < s.size() && s[i] == '}') return keys;
            while (i < s.size() && !bad) {
                ws();
                if (i >= s.size() || s[i] != '"') return keys;
                keys << string();
                ws();
                if (i >= s.size() || s[i] != ':') return keys;
                ++i;
                skip();
                ws();
                if (i < s.size() && s[i] == ',') { ++i; continue; }
                break;
            }
            return keys;
        }
        const QString want = path[depth];
        if (s[i] == '{') {
            ++i;
            QStringList found;
            bool have = false;
            while (i < s.size() && !bad) {
                ws();
                if (i < s.size() && s[i] == '}') break;
                if (i >= s.size() || s[i] != '"') return {};
                const QString k = string();
                ws();
                if (i >= s.size() || s[i] != ':') return {};
                ++i;
                if (k == want) {
                    // The last duplicate wins in JSON.parse, so keep scanning.
                    found = keysAt(path, depth + 1);
                    have = true;
                    // keysAt stops inside the value; we can't resume reliably, so return now.
                    return found;
                }
                skip();
                ws();
                if (i < s.size() && s[i] == ',') { ++i; continue; }
                break;
            }
            return have ? found : QStringList();
        }
        if (s[i] == '[') {
            bool okIdx = false;
            const int idx = want.toInt(&okIdx);
            if (!okIdx) return {};
            ++i;
            for (int k = 0; i < s.size() && !bad; ++k) {
                ws();
                if (i < s.size() && s[i] == ']') return {};
                if (k == idx) return keysAt(path, depth + 1);
                skip();
                ws();
                if (i < s.size() && s[i] == ',') { ++i; continue; }
                return {};
            }
        }
        return {};
    }
};
} // namespace

static bool isArrayIndex(const QString &k, quint64 *out)
{
    if (k.isEmpty() || k.size() > 10) return false;
    if (k.size() > 1 && k[0] == '0') return false;
    quint64 v = 0;
    for (QChar c : k) {
        if (!c.isDigit() || c.unicode() > '9') return false;
        v = v * 10 + (c.unicode() - '0');
    }
    if (v >= 4294967295ULL) return false;
    *out = v;
    return true;
}

QStringList keyOrder(const QByteArray &json, const QStringList &path)
{
    Scanner sc { json };
    const QStringList raw = sc.keysAt(path, 0);
    QStringList unique;
    for (const auto &k : raw)
        if (!unique.contains(k)) unique << k;
    QVector<QPair<quint64, QString>> idx;
    QStringList rest;
    for (const auto &k : unique) {
        quint64 v;
        if (isArrayIndex(k, &v)) idx.push_back({ v, k });
        else rest << k;
    }
    std::sort(idx.begin(), idx.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
    QStringList out;
    for (const auto &p : idx) out << p.second;
    out << rest;
    return out;
}

// ---------------------------------------------------------------- URLs

QString encodeURIComponent(const QString &s)
{
    return QString::fromLatin1(QUrl::toPercentEncoding(s, "!*'()~"));
}

static int hexVal(char16_t c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

QString decodeURIComponent(const QString &s)
{
    const auto malformed = [] { return Error(QStringLiteral("URI malformed")); };
    QString out;
    const qsizetype n = s.size();
    auto byteAt = [&](qsizetype k) -> int {
        if (k + 2 >= n || s[k] != '%') return -1;
        const int h = hexVal(s[k + 1].unicode()), l = hexVal(s[k + 2].unicode());
        if (h < 0 || l < 0) return -1;
        return h * 16 + l;
    };
    for (qsizetype k = 0; k < n;) {
        if (s[k] != '%') {
            out += s[k++];
            continue;
        }
        if (k + 2 >= n) throw malformed();
        const int b = byteAt(k);
        if (b < 0) throw malformed();
        k += 3;
        if (b < 0x80) {
            out += QChar(char16_t(b));
            continue;
        }
        int extra;
        uint cp;
        if ((b & 0xE0) == 0xC0) { extra = 1; cp = b & 0x1F; }
        else if ((b & 0xF0) == 0xE0) { extra = 2; cp = b & 0x0F; }
        else if ((b & 0xF8) == 0xF0) { extra = 3; cp = b & 0x07; }
        else throw malformed();
        for (int e = 0; e < extra; ++e) {
            if (k + 2 >= n) throw malformed();
            const int c = byteAt(k);
            if (c < 0 || (c & 0xC0) != 0x80) throw malformed();
            cp = (cp << 6) | (c & 0x3F);
            k += 3;
        }
        const uint mins[] = { 0, 0x80, 0x800, 0x10000 };
        if (cp < mins[extra] || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
            throw malformed();
        char32_t c32 = cp;
        out += QString::fromUcs4(&c32, 1);
    }
    return out;
}

QString decodeFormComponent(const QString &s)
{
    QByteArray in = s.toUtf8();
    in.replace('+', ' ');
    QByteArray bytes;
    bytes.reserve(in.size());
    for (qsizetype k = 0; k < in.size(); ++k) {
        if (in[k] == '%' && k + 2 < in.size()) {
            const int h = hexVal(char16_t(uchar(in[k + 1]))), l = hexVal(char16_t(uchar(in[k + 2])));
            if (h >= 0 && l >= 0) {
                bytes += char(h * 16 + l);
                k += 2;
                continue;
            }
        }
        bytes += in[k];
    }
    return QString::fromUtf8(bytes);
}

// ---------------------------------------------------------------- dates

double nowMs()
{
    return double(QDateTime::currentMSecsSinceEpoch());
}

static qint64 daysFromCivil(qint64 y, unsigned m, unsigned d)
{
    y -= m <= 2;
    const qint64 era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = unsigned(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + qint64(doe) - 719468;
}

static void civilFromDays(qint64 z, qint64 &y, unsigned &m, unsigned &d)
{
    z += 719468;
    const qint64 era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = unsigned(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    y = qint64(yoe) + era * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    d = doy - (153 * mp + 2) / 5 + 1;
    m = mp < 10 ? mp + 3 : mp - 9;
    y += m <= 2;
}

static double makeDay(double y, double m, double d)
{
    if (!std::isfinite(y) || !std::isfinite(m) || !std::isfinite(d)) return NAN;
    y = std::trunc(y);
    m = std::trunc(m);
    d = std::trunc(d);
    const double ym = y + std::floor(m / 12);
    double mn = std::fmod(m, 12);
    if (mn < 0) mn += 12;
    return double(daysFromCivil(qint64(ym), unsigned(mn) + 1, 1)) + d - 1;
}

double dateUtc(double y, double m, double d, double h, double mi, double s, double ms)
{
    if (std::isfinite(y)) {
        const double yi = std::trunc(y);
        if (yi >= 0 && yi <= 99) y = 1900 + yi;
    }
    const double day = makeDay(y, m, d);
    if (!std::isfinite(h) || !std::isfinite(mi) || !std::isfinite(s) || !std::isfinite(ms))
        return NAN;
    const double t = day * 86400000.0 + std::trunc(h) * 3600000.0 + std::trunc(mi) * 60000.0
                     + std::trunc(s) * 1000.0 + std::trunc(ms);
    if (!std::isfinite(t) || std::fabs(t) > 8.64e15) return NAN;
    return t;
}

static qint64 dayOf(double ms)
{
    return qint64(std::floor(ms / 86400000.0));
}

int utcDay(double ms)
{
    qint64 w = (dayOf(ms) + 4) % 7;
    if (w < 0) w += 7;
    return int(w);
}
int utcHours(double ms)
{
    double r = std::fmod(ms, 86400000.0);
    if (r < 0) r += 86400000.0;
    return int(r / 3600000.0);
}
int utcMinutes(double ms)
{
    double r = std::fmod(ms, 3600000.0);
    if (r < 0) r += 3600000.0;
    return int(r / 60000.0);
}
int utcFullYear(double ms)
{
    qint64 y;
    unsigned m, d;
    civilFromDays(dayOf(ms), y, m, d);
    return int(y);
}

QString isoString(double ms)
{
    if (!std::isfinite(ms)) return QStringLiteral("Invalid Date");
    const qint64 days = dayOf(ms);
    qint64 y;
    unsigned mo, d;
    civilFromDays(days, y, mo, d);
    double r = ms - double(days) * 86400000.0;
    const qint64 rem = qint64(r);
    const int hh = int(rem / 3600000), mi = int(rem / 60000 % 60), ss = int(rem / 1000 % 60),
              mss = int(rem % 1000);
    QString year = (y >= 0 && y <= 9999) ? QStringLiteral("%1").arg(y, 4, 10, QChar('0'))
                                         : QStringLiteral("%1%2").arg(y < 0 ? "-" : "+").arg(std::abs(y), 6, 10, QChar('0'));
    return QStringLiteral("%1-%2-%3T%4:%5:%6.%7Z")
        .arg(year)
        .arg(mo, 2, 10, QChar('0'))
        .arg(d, 2, 10, QChar('0'))
        .arg(hh, 2, 10, QChar('0'))
        .arg(mi, 2, 10, QChar('0'))
        .arg(ss, 2, 10, QChar('0'))
        .arg(mss, 3, 10, QChar('0'));
}

static double localToUtc(int y, int mo, int d, int h, int mi, int s, int ms)
{
    // JS local time: build from the date fields in the system zone.
    const QDate date(y, mo, d);
    if (!date.isValid()) {
        // Out-of-range days roll over like JS (Feb 31 -> Mar 3).
        const double utc = dateUtc(y, mo - 1, d, h, mi, s, ms);
        if (std::isnan(utc)) return NAN;
        const QDateTime asUtc = QDateTime::fromMSecsSinceEpoch(qint64(utc), QTimeZone::UTC);
        const QDateTime local(asUtc.date(), asUtc.time(), QTimeZone::LocalTime);
        return double(local.toMSecsSinceEpoch());
    }
    const QDateTime local(date, QTime(0, 0), QTimeZone::LocalTime);
    return double(local.toMSecsSinceEpoch()) + h * 3600000.0 + mi * 60000.0 + s * 1000.0 + ms;
}

double dateParse(const QString &raw)
{
    const QString s = raw.trimmed();
    if (s.isEmpty()) return NAN;
    // ES date-time string format.
    static const QRegularExpression iso(QStringLiteral(
        "^([+-]\\d{6}|\\d{4})(?:-(\\d{2})(?:-(\\d{2}))?)?"
        "(?:T(\\d{2}):(\\d{2})(?::(\\d{2})(?:\\.(\\d+))?)?(Z|[+-]\\d{2}:\\d{2})?)?$"));
    const auto m = iso.match(s);
    if (m.hasMatch()) {
        const int y = m.captured(1).toInt();
        const int mo = m.captured(2).isEmpty() ? 1 : m.captured(2).toInt();
        const int d = m.captured(3).isEmpty() ? 1 : m.captured(3).toInt();
        const int h = m.captured(4).toInt(), mi = m.captured(5).toInt(), sec = m.captured(6).toInt();
        const int msv = m.captured(7).isEmpty() ? 0 : (m.captured(7) + "00").left(3).toInt();
        if (mo < 1 || mo > 12 || d < 1 || d > 31 || h > 24 || mi > 59 || sec > 59) return NAN;
        const bool hasTime = !m.captured(4).isEmpty();
        const QString zone = m.captured(8);
        if (!hasTime || !zone.isEmpty()) {
            // dateUtc maps years 0-99 to the 1900s; the ISO form doesn't.
            double t = dateUtc(y < 100 && y >= 0 ? y + 400 : y, mo - 1, d, h, mi, sec, msv);
            if (y < 100 && y >= 0) t -= double(daysFromCivil(y + 400, 1, 1) - daysFromCivil(y, 1, 1)) * 86400000.0;
            if (zone.size() > 1) {
                const int sign = zone[0] == '-' ? -1 : 1;
                t -= sign * (zone.mid(1, 2).toInt() * 3600000.0 + zone.mid(4, 2).toInt() * 60000.0);
            }
            return t;
        }
        return localToUtc(y, mo, d, h, mi, sec, msv);
    }
    // Legacy formats (RFC 2822 and friends): "Wed, 29 Oct 2026 10:00:00 +0000", "Oct 28, 2026".
    static const QHash<QString, int> months = {
        { "jan", 1 }, { "feb", 2 }, { "mar", 3 }, { "apr", 4 }, { "may", 5 }, { "jun", 6 },
        { "jul", 7 }, { "aug", 8 }, { "sep", 9 }, { "oct", 10 }, { "nov", 11 }, { "dec", 12 },
    };
    static const QHash<QString, int> zones = {
        { "ut", 0 }, { "utc", 0 }, { "gmt", 0 }, { "z", 0 }, { "est", -5 }, { "edt", -4 },
        { "cst", -6 }, { "cdt", -5 }, { "mst", -7 }, { "mdt", -6 }, { "pst", -8 }, { "pdt", -7 },
    };
    int month = 0, h = 0, mi = 0, sec = 0, msv = 0;
    QVector<int> nums;
    bool haveTime = false, pm = false, am = false, haveZone = false;
    int tzMinutes = 0;
    static const QRegularExpression tok(QStringLiteral(
        "(\\d{1,2}):(\\d{2})(?::(\\d{2})(?:\\.(\\d+))?)?|([+-])(\\d{2}):?(\\d{2})|(\\d+)|([A-Za-z]+)|(\\([^)]*\\))|([^\\sA-Za-z0-9]+)"));
    auto it = tok.globalMatch(s);
    while (it.hasNext()) {
        const auto t = it.next();
        // V8 refuses a word glued to a number ("4:05PM").
        if (!t.captured(9).isEmpty() && t.capturedStart() > 0 && s[t.capturedStart() - 1].isDigit()) return NAN;
        if (!t.captured(1).isEmpty()) {
            haveTime = true;
            h = t.captured(1).toInt();
            mi = t.captured(2).toInt();
            sec = t.captured(3).toInt();
            msv = t.captured(4).isEmpty() ? 0 : (t.captured(4) + "00").left(3).toInt();
        } else if (!t.captured(5).isEmpty() && (haveTime || nums.size() >= 3)) {
            haveZone = true;
            const int sign = t.captured(5) == "-" ? -1 : 1;
            tzMinutes += sign * (t.captured(6).toInt() * 60 + t.captured(7).toInt());
        } else if (!t.captured(8).isEmpty()) {
            nums << t.captured(8).toInt();
        } else if (!t.captured(9).isEmpty()) {
            const QString w = t.captured(9).toLower();
            if (w == "am") am = true;
            else if (w == "pm") pm = true;
            else if (zones.contains(w)) { haveZone = true; tzMinutes += zones.value(w) * 60; }
            else if (w.size() >= 3 && months.contains(w.left(3))) month = months.value(w.left(3));
        } else if (!t.captured(11).isEmpty()) {
            const QString p = t.captured(11);
            if (p.startsWith('-') || p.startsWith('+')) {
                // a sign glued to a number: handled by the zone branch when it fits, else ignored
            }
        }
    }
    int y = -1, d = -1;
    if (month) {
        if (nums.size() < 2) return NAN;
        // "Oct 28 2026" or "28 Oct 2026": the year is the number above 31 (or the last one).
        if (nums[0] > 31) { y = nums[0]; d = nums[1]; }
        else { d = nums[0]; y = nums[1]; }
    } else {
        if (nums.size() < 3) return NAN;
        if (nums[0] > 31) { y = nums[0]; month = nums[1]; d = nums[2]; }
        else { month = nums[0]; d = nums[1]; y = nums[2]; }
    }
    if (y >= 0 && y < 50) y += 2000;
    else if (y >= 50 && y < 100) y += 1900;
    if (month < 1 || month > 12 || d < 1 || d > 31) return NAN;
    if (pm && h < 12) h += 12;
    if (am && h == 12) h = 0;
    if (h > 24 || mi > 59 || sec > 59) return NAN;
    if (haveZone)
        return dateUtc(y, month - 1, d, h, mi, sec, msv) - tzMinutes * 60000.0;
    return localToUtc(y, month, d, h, mi, sec, msv);
}

double round(double x)
{
    if (!std::isfinite(x)) return x;
    double r = std::floor(x);
    if (x - r >= 0.5) r += 1;
    return r;
}

} // namespace qtm::jsv
