#include "Text.h"

#include <QFontDatabase>
#include <QFontMetricsF>
#include <QGlyphRun>
#include <QHash>
#include <QPainter>
#include <QRawFont>

#ifdef Q_OS_MACOS
#include <CoreText/CoreText.h>
#endif

#include <memory>
#include <vector>

namespace qtm::text {

namespace {

struct Shaped {
    double width = 0;
    QVector<quint32> glyphs;
    QVector<QPointF> positions;
};

#ifdef Q_OS_MACOS
// CSS font-weight to the CoreText weight trait (NSFontWeight values).
double ctWeight(int w)
{
    if (w <= 100) return -0.8;
    if (w <= 200) return -0.6;
    if (w <= 300) return -0.4;
    if (w <= 400) return 0.0;
    if (w <= 500) return 0.23;
    if (w <= 600) return 0.3;
    if (w <= 700) return 0.4;
    if (w <= 800) return 0.56;
    return 0.62;
}

struct CtFont {
    CTFontRef font = nullptr;
    QHash<QString, Shaped> shapes;
    ~CtFont()
    {
        if (font) CFRelease(font);
    }
};

CtFont *ctFontFor(const QFont &f)
{
    static thread_local QHash<QString, std::shared_ptr<CtFont>> cache;
    const QString key = f.key();
    auto it = cache.find(key);
    if (it != cache.end()) return it->get();
    auto ct = std::make_shared<CtFont>();
    const bool system = f.family().startsWith('.') || f.family() == QFontDatabase::systemFont(QFontDatabase::GeneralFont).family();
    const double size = f.pixelSize() > 0 ? f.pixelSize() : f.pointSizeF();
    if (system && f.letterSpacing() == 0 && f.capitalization() == QFont::MixedCase) {
        CTFontRef base = CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, size, nullptr);
        if (base && f.weight() != QFont::Normal) {
            CTFontDescriptorRef d = CTFontCopyFontDescriptor(base);
            const double w = ctWeight(f.weight());
            CFNumberRef num = CFNumberCreate(nullptr, kCFNumberDoubleType, &w);
            const void *tk[] = {kCTFontWeightTrait};
            const void *tv[] = {num};
            CFDictionaryRef traits = CFDictionaryCreate(nullptr, tk, tv, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
            const void *ak[] = {kCTFontTraitsAttribute};
            const void *av[] = {traits};
            CFDictionaryRef attrs = CFDictionaryCreate(nullptr, ak, av, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
            CTFontDescriptorRef d2 = CTFontDescriptorCreateCopyWithAttributes(d, attrs);
            CTFontRef weighted = CTFontCreateWithFontDescriptor(d2, size, nullptr);
            if (weighted) {
                CFRelease(base);
                base = weighted;
            }
            CFRelease(d2);
            CFRelease(attrs);
            CFRelease(traits);
            CFRelease(num);
            CFRelease(d);
        }
        ct->font = base;
    }
    cache.insert(key, ct);
    return ct.get();
}

const Shaped *shape(const QFont &f, const QString &text)
{
    CtFont *ct = ctFontFor(f);
    if (!ct->font) return nullptr;
    auto it = ct->shapes.constFind(text);
    if (it != ct->shapes.constEnd()) return &*it;
    if (ct->shapes.size() > 4000) ct->shapes.clear();
    Shaped sh;
    CFStringRef str = text.toCFString();
    const void *keys[] = {kCTFontAttributeName};
    const void *vals[] = {ct->font};
    CFDictionaryRef attrs = CFDictionaryCreate(nullptr, keys, vals, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFAttributedStringRef as = CFAttributedStringCreate(nullptr, str, attrs);
    CTLineRef line = CTLineCreateWithAttributedString(as);
    sh.width = CTLineGetTypographicBounds(line, nullptr, nullptr, nullptr);
    CFArrayRef runs = CTLineGetGlyphRuns(line);
    bool otherFont = false;
    for (CFIndex r = 0; r < CFArrayGetCount(runs); ++r) {
        CTRunRef run = (CTRunRef)CFArrayGetValueAtIndex(runs, r);
        CFDictionaryRef ra = CTRunGetAttributes(run);
        CTFontRef rf = (CTFontRef)CFDictionaryGetValue(ra, kCTFontAttributeName);
        if (rf && !CFEqual(rf, ct->font)) otherFont = true;   // a fallback font: let Qt draw it
        const CFIndex n = CTRunGetGlyphCount(run);
        std::vector<CGGlyph> glyphs(n);
        std::vector<CGPoint> pos(n);
        CTRunGetGlyphs(run, CFRangeMake(0, 0), glyphs.data());
        CTRunGetPositions(run, CFRangeMake(0, 0), pos.data());
        for (CFIndex i = 0; i < n; ++i) {
            sh.glyphs.push_back(glyphs[i]);
            sh.positions.push_back(QPointF(pos[i].x, -pos[i].y));
        }
    }
    if (otherFont) sh.glyphs.clear();
    CFRelease(line);
    CFRelease(as);
    CFRelease(attrs);
    CFRelease(str);
    return &*ct->shapes.insert(text, sh);
}
#endif

} // namespace

double width(const QFont &font, const QString &s)
{
#ifdef Q_OS_MACOS
    if (const Shaped *sh = shape(font, s)) return sh->width;
#endif
    return QFontMetricsF(font).horizontalAdvance(s);
}

void draw(QPainter &p, const QPointF &origin, const QFont &font, const QString &s, const QColor &color)
{
    if (s.isEmpty()) return;
#ifdef Q_OS_MACOS
    if (const Shaped *sh = shape(font, s); sh && !sh->glyphs.isEmpty()) {
        static thread_local QHash<QString, QRawFont> rawFonts;
        const QString key = font.key();
        auto it = rawFonts.find(key);
        if (it == rawFonts.end()) it = rawFonts.insert(key, QRawFont::fromFont(font));
        if (it->isValid()) {
            QGlyphRun run;
            run.setRawFont(*it);
            run.setGlyphIndexes(sh->glyphs);
            run.setPositions(sh->positions);
            p.setPen(color);
            p.drawGlyphRun(origin, run);
            return;
        }
    }
#endif
    p.setFont(font);
    p.setPen(color);
    p.drawText(origin, s);
}

double ascent(const QFont &font)
{
    return QFontMetricsF(font).ascent();
}

double descent(const QFont &font)
{
    return QFontMetricsF(font).descent();
}

} // namespace qtm::text
