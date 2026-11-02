#include "JsCompat.h"

#include "Error.h"

#include <charconv>
#include <cstdio>
#include <string>

namespace qtm::js {

QString numberToString(double x)
{
    if (std::isnan(x)) return QStringLiteral("NaN");
    if (x == 0) return QStringLiteral("0");
    if (std::isinf(x)) return x > 0 ? QStringLiteral("Infinity") : QStringLiteral("-Infinity");
    if (x < 0) return QLatin1Char('-') + numberToString(-x);

    // Shortest digits that round-trip, as "d.ddde+XX" (the same digits V8 picks).
    char buf[64];
    auto res = std::to_chars(buf, buf + sizeof buf, x, std::chars_format::scientific);
    std::string s(buf, res.ptr);
    const auto e = s.find('e');
    std::string mant = s.substr(0, e);
    const int exp10 = std::stoi(s.substr(e + 1));
    std::string digits;
    for (char c : mant)
        if (c != '.') digits += c;
    const int k = int(digits.size());
    const int n = exp10 + 1;   // value = 0.digits * 10^n

    std::string out;
    if (k <= n && n <= 21) {
        out = digits + std::string(n - k, '0');
    } else if (0 < n && n <= 21) {
        out = digits.substr(0, n) + '.' + digits.substr(n);
    } else if (-6 < n && n <= 0) {
        out = "0." + std::string(-n, '0') + digits;
    } else {
        const int ex = n - 1;
        const std::string es = (ex >= 0 ? "+" : "-") + std::to_string(ex >= 0 ? ex : -ex);
        if (k == 1)
            out = digits + "e" + es;
        else
            out = digits.substr(0, 1) + "." + digits.substr(1) + "e" + es;
    }
    return QString::fromLatin1(out.data(), qsizetype(out.size()));
}

// Days from 1970-01-01 to y-m-d (proleptic Gregorian), and back (H. Hinnant's algorithms).
static qint64 daysFromCivil(qint64 y, qint64 m, qint64 d)
{
    y -= m <= 2;
    const qint64 era = (y >= 0 ? y : y - 399) / 400;
    const qint64 yoe = y - era * 400;
    const qint64 doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const qint64 doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}
static void civilFromDays(qint64 z, qint64 &y, qint64 &m, qint64 &d)
{
    z += 719468;
    const qint64 era = (z >= 0 ? z : z - 146096) / 146097;
    const qint64 doe = z - era * 146097;
    const qint64 yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    y = yoe + era * 400;
    const qint64 doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const qint64 mp = (5 * doy + 2) / 153;
    d = doy - (153 * mp + 2) / 5 + 1;
    m = mp < 10 ? mp + 3 : mp - 9;
    y += m <= 2;
}

// TimeClip: NaN beyond +-8.64e15 ms, otherwise truncated toward zero.
static double timeClip(double ms)
{
    if (!std::isfinite(ms) || std::fabs(ms) > 8.64e15) return std::numeric_limits<double>::quiet_NaN();
    return std::trunc(ms) + 0.0;   // + 0.0 turns -0 into +0
}

UtcParts utcParts(double ms)
{
    UtcParts p;
    const double t = timeClip(ms);
    if (std::isnan(t)) {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        p = { nan, nan, nan, nan, nan, nan, nan, nan, false };
        return p;
    }
    const qint64 v = qint64(t);
    qint64 day = v / 86400000, rem = v % 86400000;
    if (rem < 0) {
        rem += 86400000;
        day -= 1;
    }
    qint64 y, m, d;
    civilFromDays(day, y, m, d);
    p.year = double(y);
    p.month = double(m - 1);
    p.day = double(d);
    p.hours = double(rem / 3600000);
    p.minutes = double(rem / 60000 % 60);
    p.seconds = double(rem / 1000 % 60);
    p.ms = double(rem % 1000);
    qint64 wd = (day + 4) % 7;   // 1970-01-01 was a Thursday
    if (wd < 0) wd += 7;
    p.weekDay = double(wd);
    return p;
}

double dateUtc(double year, double month, double day, double h, double m, double s, double ms)
{
    // MakeDay / MakeTime / MakeDate with ToIntegerOrInfinity on each part.
    for (double v : { year, month, day, h, m, s, ms })
        if (!std::isfinite(v)) return std::numeric_limits<double>::quiet_NaN();
    double y = std::trunc(year);
    if (y >= 0 && y <= 99) y += 1900;
    const double mo = std::trunc(month);
    const double ym = y + std::floor(mo / 12);
    const double mn = std::fmod(mo, 12) < 0 ? std::fmod(mo, 12) + 12 : std::fmod(mo, 12);
    if (std::fabs(ym) > 400000) return std::numeric_limits<double>::quiet_NaN();
    const double days = double(daysFromCivil(qint64(ym), qint64(mn) + 1, 1)) + std::trunc(day) - 1;
    const double time = std::trunc(h) * 3600000.0 + std::trunc(m) * 60000.0 + std::trunc(s) * 1000.0 + std::trunc(ms);
    return timeClip(days * 86400000.0 + time);
}

QString toIsoString(double ms)
{
    const UtcParts p = utcParts(ms);
    if (!p.valid) throw Error(QStringLiteral("Invalid time value"));
    QString year;
    if (p.year >= 0 && p.year <= 9999)
        year = QStringLiteral("%1").arg(qint64(p.year), 4, 10, QLatin1Char('0'));
    else
        year = (p.year < 0 ? QStringLiteral("-") : QStringLiteral("+"))
               + QStringLiteral("%1").arg(qint64(std::fabs(p.year)), 6, 10, QLatin1Char('0'));
    auto two = [](double v) { return QStringLiteral("%1").arg(qint64(v), 2, 10, QLatin1Char('0')); };
    return QStringLiteral("%1-%2-%3T%4:%5:%6.%7Z")
        .arg(year, two(p.month + 1), two(p.day), two(p.hours), two(p.minutes), two(p.seconds))
        .arg(qint64(p.ms), 3, 10, QLatin1Char('0'));
}

QString toFixed(double x, int digits)
{
    if (std::isnan(x)) return QStringLiteral("NaN");
    if (std::fabs(x) >= 1e21) return numberToString(x);
    QString sign;
    if (x < 0) {
        sign = QStringLiteral("-");
        x = -x;
    }
    // The exact decimal expansion (a double has at most 1074 fractional digits), then round half up.
    static thread_local char buf[1500];
    std::snprintf(buf, sizeof buf, "%.1100f", x);
    std::string s(buf);
    const auto dot = s.find('.');
    std::string intPart = s.substr(0, dot), frac = s.substr(dot + 1);
    std::string keep = intPart + frac.substr(0, digits);
    bool up = frac[digits] >= '5';
    if (up) {
        int i = int(keep.size()) - 1;
        for (; i >= 0; --i) {
            if (keep[i] == '9') keep[i] = '0';
            else {
                keep[i]++;
                break;
            }
        }
        if (i < 0) keep.insert(keep.begin(), '1');
    }
    const int intLen = int(keep.size()) - digits;
    std::string out = keep.substr(0, intLen);
    if (digits > 0) out += "." + keep.substr(intLen);
    return sign + QString::fromLatin1(out.data(), qsizetype(out.size()));
}

} // namespace qtm::js
