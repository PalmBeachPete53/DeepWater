// Number, date and colour formatting of Lightweight Charts, plus canvas-like text metrics.

#include "ChartPrivate.h"

#include <QFontDatabase>
#include <QGlyphRun>
#include <QPainter>
#include <QPainterPath>
#include <QRawFont>
#include <QRegularExpression>

#ifdef Q_OS_MACOS
#include <CoreText/CoreText.h>
#endif

#include <charconv>
#include <cstdint>

namespace lw {

QColor cssColor(const QString &css)
{
    const QString s = css.trimmed();
    if (s.isEmpty())
        return {};
    if (s == QLatin1String("transparent"))
        return QColor(0, 0, 0, 0);
    static const QRegularExpression rgbRe(
        QStringLiteral(R"(^rgba?\s*\(\s*([\d.]+)\s*,\s*([\d.]+)\s*,\s*([\d.]+)\s*(?:,\s*([\d.]+)\s*)?\)$)"));
    const auto m = rgbRe.match(s);
    if (m.hasMatch()) {
        auto ch = [](const QString &v) {
            return std::clamp(int(std::lround(v.toDouble())), 0, 255);
        };
        QColor c(ch(m.captured(1)), ch(m.captured(2)), ch(m.captured(3)));
        if (!m.captured(4).isEmpty())
            c.setAlphaF(float(std::clamp(m.captured(4).toDouble(), 0.0, 1.0)));
        return c;
    }
    if (s.startsWith('#') && s.size() == 9) {   // CSS #rrggbbaa (Qt reads #aarrggbb)
        bool ok = false;
        const uint v = s.mid(1).toUInt(&ok, 16);
        if (ok)
            return QColor((v >> 24) & 255, (v >> 16) & 255, (v >> 8) & 255, v & 255);
    }
    return QColor::fromString(s);
}

bool DataItem::whitespace() const
{
    return std::isnan(value) && std::isnan(open) && std::isnan(close);
}

} // namespace lw

namespace lw::detail {

// ---------------------------------------------------------------- JS numbers

QString jsNumber(double v)
{
    if (std::isnan(v))
        return QStringLiteral("NaN");
    if (std::isinf(v))
        return v < 0 ? QStringLiteral("-Infinity") : QStringLiteral("Infinity");
    if (v == 0)
        return QStringLiteral("0");
    char buf[512];
    const double a = std::abs(v);
    std::to_chars_result r;
    if (a >= 1e-6 && a < 1e21)
        r = std::to_chars(buf, buf + sizeof buf, v, std::chars_format::fixed);
    else
        r = std::to_chars(buf, buf + sizeof buf, v, std::chars_format::scientific);
    QString s = QString::fromLatin1(buf, int(r.ptr - buf));
    // JS writes exponents as e+21 / e-7
    const int e = s.indexOf('e');
    if (e >= 0) {
        QString mant = s.left(e), ex = s.mid(e + 1);
        const bool neg = ex.startsWith('-');
        if (neg || ex.startsWith('+'))
            ex = ex.mid(1);
        while (ex.size() > 1 && ex.startsWith('0'))
            ex = ex.mid(1);
        s = mant + 'e' + (neg ? '-' : '+') + ex;
    }
    return s;
}

QString jsToFixed(double v, int digits)
{
    if (!std::isfinite(v))
        return jsNumber(v);
    if (std::abs(v) >= 1e21)
        return jsNumber(v);
    char buf[512];
    auto r = std::to_chars(buf, buf + sizeof buf, v, std::chars_format::fixed, digits);
    QString s = QString::fromLatin1(buf, int(r.ptr - buf));
    // JS rounds exact ties away from zero ("pick the larger n"); to_chars rounds them to even.
    // Detect an exact tie by printing more digits.
    char buf2[512];
    auto r2 = std::to_chars(buf2, buf2 + sizeof buf2, v, std::chars_format::fixed, digits + 25);
    const QByteArray more(buf2, int(r2.ptr - buf2));
    const int dot = more.indexOf('.');
    if (dot >= 0) {
        const QByteArray tail = more.mid(dot + 1 + digits);
        if (tail.size() > 0 && tail[0] == '5' && tail.mid(1).count('0') == tail.size() - 1) {
            const double scale = std::pow(10.0, digits);
            const double up = (std::floor(std::abs(v) * scale) + 1) / scale;
            auto r3 = std::to_chars(buf, buf + sizeof buf, v < 0 ? -up : up,
                                    std::chars_format::fixed, digits);
            s = QString::fromLatin1(buf, int(r3.ptr - buf));
        }
    }
    if (s.startsWith('-')) {   // JS prints (-0.001).toFixed(2) as "-0.00" too; keep
    }
    return s;
}

// JS numberToStringWithLeadingZero(value, length)
static QString leadingZero(double value, int length)
{
    if (!std::isfinite(value))
        return QStringLiteral("n/a");
    if (length == 0)
        return jsNumber(value);
    const QString s = QStringLiteral("0000000000000000") + jsNumber(value);
    return s.right(length);
}

// ---------------------------------------------------------------- price formatters

NumberFormatter NumberFormatter::price(double priceScale, double minMove)
{
    NumberFormatter f;
    f.kind = Price;
    if (!(minMove != 0 && std::isfinite(minMove)))
        minMove = 1;
    if (!std::isfinite(priceScale) || std::floor(priceScale) != priceScale)
        priceScale = 100;
    f.priceScale = priceScale;
    f.minMove = minMove;
    f.fractional = 0;
    if (priceScale > 0 && minMove > 0) {
        double t = priceScale;
        while (t > 1) {
            t /= 10;
            ++f.fractional;
        }
    }
    return f;
}

NumberFormatter NumberFormatter::percent(double priceScale)
{
    NumberFormatter f = price(priceScale, 1);
    f.kind = Percent;
    return f;
}

NumberFormatter NumberFormatter::volume(int precision)
{
    NumberFormatter f;
    f.kind = Volume;
    f.volumePrecision = precision;
    return f;
}

NumberFormatter NumberFormatter::fromFormat(const PriceFormat &pf)
{
    switch (pf.type) {
    case PriceFormatType::Volume:
        return volume(pf.precision);
    case PriceFormatType::Percent:
        return percent(pf.precision);   // JS: new PercentageFormatter(precision)
    default: {
        const double t = std::pow(10.0, pf.precision);
        return price(t, pf.minMove * t);
    }
    }
}

QString NumberFormatter::format(double v) const
{
    switch (kind) {
    case Volume:
        return formatVolume(v);
    case Percent:
        return formatPrice(v) + '%';
    default:
        return formatPrice(v);
    }
}

QString NumberFormatter::formatPrice(double t) const
{
    const QString sign = t < 0 ? QString(QChar(0x2212)) : QString();
    t = std::abs(t);
    const double i = priceScale / minMove;
    double n = std::floor(t);
    QString s;
    const int e = fractional;
    if (i > 1) {
        double r = jsToFixed(jsRound(t * i) - n * i, e).toDouble();
        if (r >= i) {
            r -= i;
            n += 1;
        }
        s = '.' + leadingZero(jsToFixed(r, e).toDouble() * minMove, e);
    } else {
        n = jsRound(n * i) / i;
        if (e > 0)
            s = '.' + leadingZero(0, e);
    }
    return sign + jsToFixed(n, 0) + s;
}

QString NumberFormatter::formatVolume(double t) const
{
    auto fmt = [this](double v) {
        const double n = std::pow(10.0, volumePrecision);
        v = jsRound(v * n) / n;
        QString s;
        if (v >= 1e-15 && v < 1) {
            s = jsToFixed(v, volumePrecision);
            static const QRegularExpression trail(QStringLiteral(R"(\.?0+$)"));
            s.remove(trail);
        } else {
            s = jsNumber(v);
        }
        static const QRegularExpression tail(QStringLiteral(R"((\.[1-9]*)0+$)"));
        s.replace(tail, QStringLiteral("\\1"));
        return s;
    };
    QString sign;
    if (t < 0) {
        sign = QStringLiteral("-");
        t = -t;
    }
    if (t < 995)
        return sign + fmt(t);
    if (t < 999995)
        return sign + fmt(t / 1e3) + 'K';
    if (t < 999999995) {
        t = 1e3 * jsRound(t / 1e3);
        return sign + fmt(t / 1e6) + 'M';
    }
    t = 1e6 * jsRound(t / 1e6);
    return sign + fmt(t / 1e9) + 'B';
}

// ---------------------------------------------------------------- dates

static void civilFromDays(std::int64_t z, int &y, int &m, int &d)
{
    z += 719468;
    const std::int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = unsigned(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const std::int64_t yy = std::int64_t(yoe) + era * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    d = int(doy - (153 * mp + 2) / 5 + 1);
    m = int(mp < 10 ? mp + 3 : mp - 9);
    y = int(yy + (m <= 2));
}

UtcParts utcParts(double timeSec)
{
    UtcParts p;
    const double ms = std::trunc(timeSec * 1000);
    const double secs = std::floor(ms / 1000);
    const std::int64_t days = std::int64_t(std::floor(secs / 86400));
    const std::int64_t rem = std::int64_t(secs) - days * 86400;
    civilFromDays(days, p.year, p.month, p.day);
    p.hour = int(rem / 3600);
    p.minute = int((rem % 3600) / 60);
    p.second = int(rem % 60);
    return p;
}

int timeWeight(double cur, double prev)
{
    const UtcParts a = utcParts(cur), b = utcParts(prev);
    if (a.year != b.year)
        return 70;
    if (a.month != b.month)
        return 60;
    if (a.day != b.day)
        return 50;
    static const struct { double div; int weight; } divisors[] = {
        {1000, 10},           {60000, 20},          {5 * 60000, 21},      {30 * 60000, 22},
        {3600000, 30},        {3 * 3600000, 31},    {6 * 3600000, 32},    {12 * 3600000, 33},
    };
    const double ca = std::trunc(cur * 1000), pb = std::trunc(prev * 1000);
    for (int n = 7; n >= 0; --n)
        if (std::floor(pb / divisors[n].div) != std::floor(ca / divisors[n].div))
            return divisors[n].weight;
    return 0;
}

TickMarkType tickMarkType(int weight, bool timeVisible, bool secondsVisible)
{
    switch (weight) {
    case 0:
    case 10:
        return timeVisible ? (secondsVisible ? TickMarkType::TimeWithSeconds : TickMarkType::Time)
                           : TickMarkType::DayOfMonth;
    case 20:
    case 21:
    case 22:
    case 30:
    case 31:
    case 32:
    case 33:
        return timeVisible ? TickMarkType::Time : TickMarkType::DayOfMonth;
    case 50:
        return TickMarkType::DayOfMonth;
    case 60:
        return TickMarkType::Month;
    case 70:
        return TickMarkType::Year;
    }
    return TickMarkType::DayOfMonth;
}

static const char *const kMonths[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

static QString two(int v) { return QString::number(v).rightJustified(2, '0'); }

QString defaultTickMarkLabel(double time, TickMarkType type)
{
    const UtcParts p = utcParts(time);
    switch (type) {
    case TickMarkType::Year:
        return QString::number(p.year);
    case TickMarkType::Month:
        return QString::fromLatin1(kMonths[p.month - 1]);
    case TickMarkType::DayOfMonth:
        return QString::number(p.day);
    case TickMarkType::Time:
        return two(p.hour) + ':' + two(p.minute);
    case TickMarkType::TimeWithSeconds:
        return two(p.hour) + ':' + two(p.minute) + ':' + two(p.second);
    }
    return {};
}

QString defaultTimeLabel(double time, bool timeVisible, bool secondsVisible)
{
    // dateFormat "dd MMM 'yy", then "   %h:%m(:%s)" when the time is visible
    const UtcParts p = utcParts(time);
    QString s = two(p.day) + ' ' + QString::fromLatin1(kMonths[p.month - 1]) + QStringLiteral(" '")
                + two(((p.year % 100) + 100) % 100);
    if (timeVisible) {
        s += QStringLiteral("   ") + two(p.hour) + ':' + two(p.minute);
        if (secondsVisible)
            s += ':' + two(p.second);
    }
    return s;
}

// ---------------------------------------------------------------- text metrics

static QString normalizeDigits(const QString &text, bool allDigits)
{
    QString s = text;
    for (QChar &c : s) {
        const ushort u = c.unicode();
        if (u >= (allDigits ? '1' : '2') && u <= '9')
            c = QLatin1Char('0');
    }
    return s;
}

TextMetrics::~TextMetrics()
{
#ifdef Q_OS_MACOS
    if (ctFont)
        CFRelease(ctFont);
#endif
}

void TextMetrics::setFont(const QFont &f)
{
    font = f;
    fm = QFontMetricsF(f);
    shapes.clear();
    // Canvas textBaseline "middle": halfway between the OS/2 typo ascent and descent normalised
    // to the em size (Chromium's NormalizedTypoAscentAndDescent), in 1/64 px.
    middleOffset = (fm.ascent() - fm.descent()) / 2;
#ifdef Q_OS_MACOS
    if (ctFont)
        CFRelease(ctFont);
    ctFont = nullptr;
    const double size = f.pixelSize() > 0 ? f.pixelSize() : f.pointSizeF();
    CTFontRef base = nullptr;
    if (f.family().startsWith('.') || f.family() == QFontDatabase::systemFont(QFontDatabase::GeneralFont).family()) {
        base = CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, size, nullptr);
    } else {
        CFStringRef name = f.family().toCFString();
        base = CTFontCreateWithName(name, size, nullptr);
        CFRelease(name);
    }
    if (base && f.bold()) {
        CTFontRef b = CTFontCreateCopyWithSymbolicTraits(base, 0, nullptr, kCTFontBoldTrait, kCTFontBoldTrait);
        if (b) {
            CFRelease(base);
            base = b;
        }
    }
    ctFont = (void *)base;
    if (base) {
        if (CFDataRef os2 = CTFontCopyTable(base, kCTFontTableOS2, kCTFontTableOptionNoOptions)) {
            if (CFDataGetLength(os2) >= 72) {
                const UInt8 *p = CFDataGetBytePtr(os2);
                const double asc = qint16((p[68] << 8) | p[69]);
                const double desc = -double(qint16((p[70] << 8) | p[71]));
                if (asc > 0 && desc >= 0 && asc + desc > 0) {
                    const double a = std::round(size * asc / (asc + desc) * 64) / 64;
                    const double d = std::round(size * desc / (asc + desc) * 64) / 64;
                    middleOffset = (a - d) / 2;
                }
            }
            CFRelease(os2);
        }
    }
#endif
}

const TextMetrics::Shaped &TextMetrics::shape(const QString &text)
{
    auto it = shapes.constFind(text);
    if (it != shapes.constEnd())
        return *it;
    if (shapes.size() > 2000)
        shapes.clear();
    Shaped sh;
#ifdef Q_OS_MACOS
    if (ctFont && !text.isEmpty()) {
        CFStringRef str = text.toCFString();
        const void *keys[] = {kCTFontAttributeName};
        const void *vals[] = {ctFont};
        CFDictionaryRef attrs = CFDictionaryCreate(nullptr, keys, vals, 1, &kCFTypeDictionaryKeyCallBacks,
                                                   &kCFTypeDictionaryValueCallBacks);
        CFAttributedStringRef as = CFAttributedStringCreate(nullptr, str, attrs);
        CTLineRef line = CTLineCreateWithAttributedString(as);
        sh.width = CTLineGetTypographicBounds(line, nullptr, nullptr, nullptr);
        CFArrayRef runs = CTLineGetGlyphRuns(line);
        double top = 0, bottom = 0;
        bool any = false;
        for (CFIndex r = 0; r < CFArrayGetCount(runs); ++r) {
            CTRunRef run = (CTRunRef)CFArrayGetValueAtIndex(runs, r);
            const CFIndex n = CTRunGetGlyphCount(run);
            std::vector<CGGlyph> glyphs(n);
            std::vector<CGPoint> pos(n);
            std::vector<CGRect> rects(n);
            CTRunGetGlyphs(run, CFRangeMake(0, 0), glyphs.data());
            CTRunGetPositions(run, CFRangeMake(0, 0), pos.data());
            CFDictionaryRef ra = CTRunGetAttributes(run);
            CTFontRef rf = (CTFontRef)CFDictionaryGetValue(ra, kCTFontAttributeName);
            CTFontGetBoundingRectsForGlyphs(rf ? rf : (CTFontRef)ctFont, kCTFontOrientationHorizontal,
                                            glyphs.data(), rects.data(), n);
            for (CFIndex i = 0; i < n; ++i) {
                sh.glyphs.push_back(glyphs[i]);
                sh.positions.push_back(QPointF(pos[i].x, -pos[i].y));
                const CGRect &b = rects[i];
                if (b.size.width <= 0 && b.size.height <= 0)
                    continue;
                const double t = b.origin.y + b.size.height + pos[i].y;
                const double bt = -(b.origin.y + pos[i].y);
                top = any ? std::max(top, t) : t;
                bottom = any ? std::max(bottom, bt) : bt;
                any = true;
            }
        }
        sh.top = top;
        sh.bottom = bottom;
        CFRelease(line);
        CFRelease(as);
        CFRelease(attrs);
        CFRelease(str);
        return *shapes.insert(text, sh);
    }
#endif
    sh.width = fm.horizontalAdvance(text);
    QPainterPath path;
    path.addText(0, 0, font, text);
    const QRectF r = path.boundingRect();
    sh.top = r.isEmpty() ? fm.ascent() : -r.top();
    sh.bottom = r.isEmpty() ? 0 : r.bottom();
    return *shapes.insert(text, sh);
}

double TextMetrics::width(const QString &text, bool allDigits)
{
    return shape(normalizeDigits(text, allDigits)).width;
}

double TextMetrics::rawWidth(const QString &text)
{
    return shape(text).width;
}

double TextMetrics::centerOffset(const QString &text, bool allDigits)
{
    // canvas actualBoundingBoxAscent / Descent: the glyphs end up centred on y
    const Shaped &s = shape(normalizeDigits(text, allDigits));
    return (s.top - s.bottom) / 2;
}

std::function<void(const QString &, const QPointF &, bool, const QColor &)> textTrace;

void TextMetrics::draw(QPainter &p, const QPointF &origin, const QString &text, const QColor &color)
{
    if (text.isEmpty() || color.alpha() == 0)
        return;
    if (textTrace)
        textTrace(text, p.transform().map(origin), font.bold(), color);
#ifdef Q_OS_MACOS
    if (ctFont) {
        const Shaped &s = shape(text);
        static thread_local QHash<QString, QRawFont> rawFonts;
        const QString key = font.key();
        auto it = rawFonts.find(key);
        if (it == rawFonts.end())
            it = rawFonts.insert(key, QRawFont::fromFont(font));
        if (it->isValid()) {
            QGlyphRun run;
            run.setRawFont(*it);
            run.setGlyphIndexes(s.glyphs);
            run.setPositions(s.positions);
            p.setPen(color);
            p.drawGlyphRun(origin, run);
            return;
        }
    }
#endif
    p.setFont(font);
    p.setPen(color);
    p.drawText(origin, text);
}

} // namespace lw::detail
