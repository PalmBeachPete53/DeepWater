#include "PanelsKit.h"

#include "Theme.h"
#include "../core/JsCompat.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QFontDatabase>
#include <QFontInfo>
#include <QFontMetricsF>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMimeData>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QLocale>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QScrollBar>
#include <QStandardItemModel>
#include <QStyle>
#include <QTimeZone>
#include <QUrl>
#include <QVBoxLayout>

#include <cmath>

namespace qtm::kit {

// ================================================================ colours and fonts

Pal pal(bool brand)
{
    Pal p;
    p.brand = brand;
    if (brand) {
        // .ptq-brand variables (app.css)
        p.bg = QColor("#0d0e12");
        p.panel = QColor(23, 24, 29, qRound(0.72 * 255));
        p.border = QColor(255, 255, 255, qRound(0.1 * 255));
        p.text = QColor("#c9ccd3");
        p.muted = QColor("#a4a9b4");
        p.accent = QColor("#3aa1ff");
        p.up = QColor("#4cc38a");
        p.down = QColor("#ff7a7a");
        p.hover = QColor(255, 255, 255, qRound(0.06 * 255));
        p.input = QColor("#17181d");
        p.gold = theme::color("--gold");
        p.warn = QColor("#f7a600");
        return p;
    }
    p.bg = theme::color("--bg");
    p.panel = theme::color("--panel");
    p.border = theme::color("--border");
    p.text = theme::color("--text");
    p.muted = theme::color("--muted");
    p.accent = theme::color("--accent");
    p.up = theme::color("--up");
    p.down = theme::color("--down");
    p.hover = theme::color("--hover");
    p.input = theme::color("--input");
    p.gold = theme::color("--gold");
    p.warn = QColor("#f7a600");
    return p;
}

QColor clsColor(const QString &classes, const Pal &p)
{
    QColor c;
    for (const QString &cls : classes.split(' ', Qt::SkipEmptyParts)) {
        if (cls == "up") c = p.up;
        else if (cls == "down" || cls == "impact-High") c = p.down;
        else if (cls == "muted" || cls == "hint-text" || cls == "impact-Low" || cls == "impact-Holiday") c = p.muted;
        else if (cls == "warn" || cls == "impact-Medium") c = p.warn;
        else if (cls == "gold-text") c = p.gold;
        else if (cls == "link") c = p.brand ? QColor("#6cb8ff") : p.accent;
    }
    return c;
}

namespace {

// CSS font matching: a 600 the family doesn't have resolves to the next heavier face (Bold),
// where Qt would pick the nearest one (Medium for Helvetica Neue, the PTQ fallback font).
bool hasSemibold(const QString &family)
{
    static QHash<QString, bool> cache;
    auto it = cache.find(family);
    if (it != cache.end()) return *it;
    bool found = false;
    for (const QString &style : QFontDatabase::styles(family)) {
        const int w = QFontDatabase::weight(family, style);
        if (w >= QFont::DemiBold && w < QFont::Bold) found = true;
    }
    cache.insert(family, found);
    return found;
}

QFont withWeight(QFont f, int weight)
{
    QFont::Weight w = weight >= 700 ? QFont::Bold : weight >= 600 ? QFont::DemiBold : weight >= 500 ? QFont::Medium
                                                                                                   : QFont::Normal;
    if (w == QFont::DemiBold && !f.family().startsWith('.') && !hasSemibold(QFontInfo(f).family()))
        w = QFont::Bold;
    f.setWeight(w);
    return f;
}

bool hasFamily(const QString &family)
{
    static QHash<QString, bool> cache;
    auto it = cache.find(family);
    if (it != cache.end()) return *it;
    const bool ok = QFontDatabase::families().contains(family, Qt::CaseInsensitive);
    cache.insert(family, ok);
    return ok;
}

QFont familyFont(const QStringList &families, double px)
{
    QFont f = QApplication::font();
    for (const QString &fam : families) {
        if (hasFamily(fam)) {
            f.setFamily(fam);
            break;
        }
    }
    f.setPixelSize(qMax(1, qRound(px)));
    if (std::abs(px - qRound(px)) > 0.01)
        f.setPointSizeF(px);   // fractional CSS sizes (10.5px, 12.5px): Qt on macOS has 1pt = 1px
    return f;
}

} // namespace

QFont uiFont(double px, int weight)
{
    QFont f = QApplication::font();
    if (std::abs(px - qRound(px)) > 0.01) f.setPointSizeF(px);
    else f.setPixelSize(qRound(px));
    return withWeight(f, weight);
}

QFont monoFont(double px)
{
    QFont f = familyFont({"SF Mono", "Menlo", "Monaco"}, px);
    f.setStyleHint(QFont::Monospace);
    return f;
}

QFont brandSans(double px, int weight)
{
    return withWeight(familyFont({"Instrument Sans", "Helvetica Neue", "Arial"}, px), weight);
}

QFont brandSerif(double px)
{
    QFont f = familyFont({"Instrument Serif", "Georgia", "Times New Roman"}, px);
    f.setItalic(true);
    return f;
}

QFont brandMono(double px)
{
    QFont f = familyFont({"Fragment Mono", "SF Mono", "Menlo"}, px);
    f.setStyleHint(QFont::Monospace);
    return f;
}

// ================================================================ rich text

Rich parseRich(const QString &html, const QFont &base, const QColor &color, const Pal &p)
{
    Rich r;
    struct Frame { QString tag; QTextCharFormat fmt; int start; QString href; };
    QVector<Frame> stack;
    QTextCharFormat cur;
    cur.setFont(base);
    cur.setForeground(color);
    QString out;
    QVector<QPair<int, QTextCharFormat>> runs;   // format changes at positions
    auto mark = [&] { runs.push_back({int(out.size()), cur}); };
    mark();

    static const QHash<QString, QString> entities = {
        {"amp", "&"}, {"lt", "<"}, {"gt", ">"}, {"quot", "\""}, {"#39", "'"}, {"times", QString(QChar(0xd7))},
        {"nbsp", QString(QChar(0xa0))}, {"#8599", QString(QChar(0x2197))}, {"hellip", QString(QChar(0x2026))},
    };
    int i = 0;
    const int n = html.size();
    while (i < n) {
        const QChar c = html[i];
        if (c == '<') {
            const int end = html.indexOf('>', i);
            if (end < 0) { out += html.mid(i); break; }
            QString tag = html.mid(i + 1, end - i - 1).trimmed();
            i = end + 1;
            const bool closing = tag.startsWith('/');
            if (closing) tag = tag.mid(1);
            const QString name = tag.section(' ', 0, 0).toLower().remove('/');
            if (name == "br") {
                out += '\n';
                continue;
            }
            if (closing) {
                for (int k = stack.size() - 1; k >= 0; --k) {
                    if (stack[k].tag != name) continue;
                    const Frame f = stack[k];
                    if (name == "a" && !f.href.isEmpty())
                        r.links.push_back({f.start, int(out.size()) - f.start, f.href});
                    if (name == "code") {
                        // the right padding of the inline code box
                        cur = f.fmt;
                        QTextCharFormat pad = cur;
                        pad.setFontLetterSpacingType(QFont::AbsoluteSpacing);
                        pad.setFontLetterSpacing(4);
                        stack.resize(k);
                        r.codes.push_back({f.start, int(out.size()) - f.start + 1});
                        cur = pad;
                        mark();
                        out += QChar(0x200b);
                        cur = f.fmt;
                        mark();
                        break;
                    }
                    cur = f.fmt;
                    stack.resize(k);
                    mark();
                    break;
                }
                continue;
            }
            // attributes
            static const QRegularExpression attrRe(R"re((\w[\w-]*)\s*=\s*"([^"]*)")re");
            QHash<QString, QString> attrs;
            auto it = attrRe.globalMatch(tag);
            while (it.hasNext()) {
                const auto m = it.next();
                attrs.insert(m.captured(1), m.captured(2));
            }
            Frame f {name, cur, int(out.size()), attrs.value("href")};
            QTextCharFormat next = cur;
            if (name == "b" || name == "strong") {
                QFont fb = next.font();
                fb.setWeight(QFont::Bold);
                next.setFont(fb);
            } else if (name == "i" || name == "em") {
                QFont fi = next.font();
                fi.setItalic(true);
                next.setFont(fi);
            } else if (name == "code") {
                QFont fm = monoFont(12);
                if (p.brand) fm = brandMono(12);
                next.setFont(fm);
                if (p.brand) next.setForeground(QColor("#f4f5f7"));
                // the left padding of the inline code box
                QTextCharFormat pad = next;
                pad.setFontLetterSpacingType(QFont::AbsoluteSpacing);
                pad.setFontLetterSpacing(4);
                f.start = out.size();
                cur = pad;
                mark();
                out += QChar(0x200b);
            } else if (name == "a") {
                next.setForeground(clsColor("link", p));
            }
            const QString cls = attrs.value("class");
            if (!cls.isEmpty()) {
                const QColor cc = clsColor(cls, p);
                if (cc.isValid()) next.setForeground(cc);
            }
            const QString style = attrs.value("style");
            if (!style.isEmpty()) {
                static const QRegularExpression colorRe(R"((?:^|;)\s*color\s*:\s*([^;]+))");
                const auto m = colorRe.match(style);
                if (m.hasMatch()) next.setForeground(theme::parseColor(m.captured(1).trimmed()));
                if (style.contains("font-weight:600") || style.contains("font-weight: 600")) {
                    QFont fb = next.font();
                    fb.setWeight(QFont::DemiBold);
                    next.setFont(fb);
                }
            }
            stack.push_back(f);
            cur = next;
            mark();
            continue;
        }
        if (c == '&') {
            const int semi = html.indexOf(';', i);
            if (semi > i && semi - i <= 8) {
                const QString ent = html.mid(i + 1, semi - i - 1);
                if (entities.contains(ent)) {
                    out += entities.value(ent);
                    i = semi + 1;
                    continue;
                }
                if (ent.startsWith('#')) {
                    bool ok = false;
                    const uint code = ent.startsWith("#x") ? ent.mid(2).toUInt(&ok, 16) : ent.mid(1).toUInt(&ok);
                    if (ok) {
                        out += QString::fromUcs4(reinterpret_cast<const char32_t *>(&code), 1);
                        i = semi + 1;
                        continue;
                    }
                }
            }
        }
        // collapse white space like HTML (newlines in the JS template literals)
        if (c == '\n' || c == '\t' || c == '\r' || c == ' ') {
            if (!out.isEmpty() && !out.endsWith(' ') && !out.endsWith('\n')) out += ' ';
            else if (out.isEmpty()) { /* leading space dropped */ }
            ++i;
            continue;
        }
        out += c;
        // no line break after "/" inside <code> (docs/indicator-template.js stays whole)
        if (c == '/') {
            for (const Frame &f : stack)
                if (f.tag == "code") { out += QChar(0x2060); break; }
        }
        ++i;
    }
    // trailing space
    while (out.endsWith(' ')) out.chop(1);
    r.text = out;
    for (int k = 0; k < runs.size(); ++k) {
        const int s = runs[k].first;
        const int e = k + 1 < runs.size() ? runs[k + 1].first : int(out.size());
        if (e <= s) continue;
        QTextLayout::FormatRange fr;
        fr.start = s;
        fr.length = e - s;
        fr.format = runs[k].second;
        r.formats.push_back(fr);
    }
    return r;
}

// ================================================================ Text

namespace {

// CSS line box: the glyphs of a line sit centred in lineHeight (half-leading above and below).
double baselineIn(const QFont &f, double lineH)
{
    const QFontMetricsF fm(f);
    const double a = std::round(fm.ascent()), d = std::round(fm.descent());
    return (lineH - (a + d)) / 2 + a;
}

double fontPx(const QFont &f)
{
    return f.pixelSize() > 0 ? f.pixelSize() : f.pointSizeF();
}

} // namespace

Text::Text(const QString &html, const TextStyle &style, QWidget *parent)
    : QWidget(parent), m_style(style)
{
    setMouseTracking(true);
    QSizePolicy sp(QSizePolicy::Preferred, QSizePolicy::Preferred);
    sp.setHeightForWidth(style.wrap);
    setSizePolicy(sp);
    setHtml(html);
}

void Text::setHtml(const QString &html)
{
    m_html = html;
    const Pal p = pal(m_style.brand);
    m_rich = parseRich(html, m_style.font, m_style.color.isValid() ? m_style.color : p.text, p);
    if (m_style.upper) m_rich.text = m_rich.text.toUpper();
    updateGeometry();
    update();
}

void Text::setStyle(const TextStyle &style)
{
    m_style = style;
    QSizePolicy sp = sizePolicy();
    sp.setHeightForWidth(style.wrap);
    setSizePolicy(sp);
    setHtml(m_html);
}

void Text::setColor(const QColor &c)
{
    m_style.color = c;
    setHtml(m_html);
}

void Text::layoutFor(double width, QVector<QTextLayout *> *out, double *height) const
{
    const double lh = m_style.lineHeight * fontPx(m_style.font);
    if (m_rich.text.isEmpty()) {   // an empty block has no line box
        if (height) *height = 0;
        return;
    }
    const QStringList paras = m_rich.text.split('\n');
    int offset = 0;
    double y = 0;
    int lines = 0;
    for (const QString &para : paras) {
        auto *tl = new QTextLayout(para, m_style.font);
        QVector<QTextLayout::FormatRange> fr;
        for (const auto &f : m_rich.formats) {
            const int s = qMax(f.start, offset), e = qMin(f.start + f.length, offset + int(para.size()));
            if (e > s) {
                QTextLayout::FormatRange g = f;
                g.start = s - offset;
                g.length = e - s;
                fr.push_back(g);
            }
        }
        tl->setFormats(fr);
        QTextOption opt;
        opt.setWrapMode(m_style.wrap ? QTextOption::WrapAtWordBoundaryOrAnywhere : QTextOption::NoWrap);
        if (m_style.wrap) opt.setWrapMode(QTextOption::WordWrap);
        tl->setTextOption(opt);
        tl->beginLayout();
        for (;;) {
            QTextLine line = tl->createLine();
            if (!line.isValid()) break;
            line.setLineWidth(m_style.wrap ? qMax(1.0, width) : 1e6);
            // baseline placement for this line
            double asc = 0, base = 0;
            Q_UNUSED(asc);
            base = baselineIn(m_style.font, lh);
            line.setPosition(QPointF(0, y + base - line.ascent()));
            y += lh;
            ++lines;
            if (m_style.clamp > 0 && lines >= m_style.clamp) break;
        }
        tl->endLayout();
        offset += para.size() + 1;
        if (out) out->push_back(tl);
        else delete tl;
        if (m_style.clamp > 0 && lines >= m_style.clamp) break;
    }
    if (height) *height = y;
}

double Text::naturalWidth() const
{
    QVector<QTextLayout *> ls;
    const bool wrap = m_style.wrap;
    const_cast<Text *>(this)->m_style.wrap = false;
    layoutFor(1e6, &ls, nullptr);
    const_cast<Text *>(this)->m_style.wrap = wrap;
    double w = 0;
    for (QTextLayout *l : ls) {
        for (int i = 0; i < l->lineCount(); ++i) w = qMax(w, l->lineAt(i).naturalTextWidth());
        delete l;
    }
    return w;
}

int Text::heightForWidth(int w) const
{
    const QMargins m = contentsMargins();
    double h = 0;
    layoutFor(w - m.left() - m.right(), nullptr, &h);
    return int(std::round(h)) + m.top() + m.bottom();
}

QSize Text::sizeHint() const
{
    const QMargins m = contentsMargins();
    const int w = int(std::ceil(naturalWidth())) + m.left() + m.right();
    double h = 0;
    layoutFor(1e6, nullptr, &h);
    return {w, int(std::round(h)) + m.top() + m.bottom()};
}

QSize Text::minimumSizeHint() const
{
    if (!m_style.wrap && !m_style.ellipsis) return sizeHint();
    // wrapping text: layouts ask heightForWidth() for the real height
    return {0, m_style.wrap ? 0 : sizeHint().height()};
}

void Text::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    const QRect cr = contentsRect();
    QVector<QTextLayout *> ls;
    double h = 0;
    layoutFor(cr.width(), &ls, &h);
    const Pal pl = pal(m_style.brand);
    const QColor codeBg = m_style.codeBg.isValid() ? m_style.codeBg
                          : m_style.brand          ? QColor(255, 255, 255, qRound(0.08 * 255))
                                                   : pl.hover;
    int offset = 0;
    int lineNo = 0;
    int totalLines = 0;
    for (QTextLayout *l : ls) totalLines += l->lineCount();
    for (QTextLayout *l : ls) {
        // inline <code> backgrounds
        for (const auto &code : m_rich.codes) {
            const int s = code.start - offset, e = code.start + code.length - offset;
            if (e <= 0 || s >= l->text().size()) continue;
            for (int i = 0; i < l->lineCount(); ++i) {
                QTextLine line = l->lineAt(i);
                const int ls0 = line.textStart(), le0 = ls0 + line.textLength();
                const int a = qMax(s, ls0), b = qMin(e, le0);
                if (b <= a) continue;
                if (b - a == 1 && l->text().at(a) == QChar(0x200b)) continue;   // only the padding at a line end
                double x1 = line.cursorToX(a), x2 = line.cursorToX(b);
                double dx = 0;
                if (m_style.align & Qt::AlignHCenter) dx = (cr.width() - line.naturalTextWidth()) / 2;
                const QFontMetricsF fm(m_style.codeFont);
                const double baseline = line.position().y() + line.ascent();
                QRectF r(cr.left() + dx + x1, cr.top() + baseline - fm.ascent() - 1, x2 - x1,
                         fm.ascent() + fm.descent() + 2);
                QPainterPath path;
                path.addRoundedRect(r, m_style.brand ? 6 : 3, m_style.brand ? 6 : 3);
                p.fillPath(path, codeBg);
            }
        }
        for (int i = 0; i < l->lineCount(); ++i) {
            QTextLine line = l->lineAt(i);
            double dx = 0;
            if (m_style.align & Qt::AlignHCenter) dx = (cr.width() - line.naturalTextWidth()) / 2;
            else if (m_style.align & Qt::AlignRight) dx = cr.width() - line.naturalTextWidth();
            ++lineNo;
            const bool lastClamped = m_style.clamp > 0 && lineNo == m_style.clamp
                                     && (line.textStart() + line.textLength() < l->text().size() || l != ls.last()
                                         || offset + l->text().size() < m_rich.text.size());
            const bool elideOne = !m_style.wrap && m_style.ellipsis && line.naturalTextWidth() > cr.width();
            if (lastClamped || elideOne) {
                const QString rest = l->text().mid(line.textStart());
                const QFontMetricsF fm(m_style.font);
                const QString el = fm.elidedText(rest, Qt::ElideRight, cr.width());
                QColor col = m_style.color.isValid() ? m_style.color : pl.text;
                for (const auto &f : l->formats())
                    if (f.start <= line.textStart() && f.start + f.length > line.textStart())
                        col = f.format.foreground().color();
                p.setPen(col);
                p.setFont(m_style.font);
                p.drawText(QPointF(cr.left(), cr.top() + line.position().y() + line.ascent()), el);
                continue;
            }
            line.draw(&p, QPointF(cr.left() + dx, cr.top()));
        }
        offset += l->text().size() + 1;
    }
    Q_UNUSED(totalLines);
    qDeleteAll(ls);
}

QString Text::linkAt(const QPoint &pos) const
{
    if (m_rich.links.isEmpty()) return {};
    const QRect cr = contentsRect();
    QVector<QTextLayout *> ls;
    layoutFor(cr.width(), &ls, nullptr);
    QString found;
    int offset = 0;
    for (QTextLayout *l : ls) {
        for (int i = 0; i < l->lineCount() && found.isEmpty(); ++i) {
            const QTextLine line = l->lineAt(i);
            const QRectF r = line.naturalTextRect().translated(cr.topLeft());
            if (!r.contains(pos)) continue;
            const int c = line.xToCursor(pos.x() - cr.left()) + offset;
            for (const auto &k : m_rich.links)
                if (c >= k.start && c < k.start + k.length) found = k.href;
        }
        offset += l->text().size() + 1;
    }
    qDeleteAll(ls);
    return found;
}

void Text::mouseMoveEvent(QMouseEvent *e)
{
    if (m_rich.links.isEmpty()) return QWidget::mouseMoveEvent(e);
    setCursor(linkAt(e->pos()).isEmpty() ? Qt::ArrowCursor : Qt::PointingHandCursor);
}

void Text::mousePressEvent(QMouseEvent *e)
{
    const QString href = linkAt(e->pos());
    if (href.isEmpty()) return QWidget::mousePressEvent(e);
    emit linkActivated(href);
}

Text *text(const QString &html, double px, const QString &color, int weight)
{
    TextStyle st;
    st.font = uiFont(px, weight);
    if (!color.isEmpty()) st.color = color.startsWith("--") ? theme::color(color) : theme::parseColor(color);
    return new Text(html, st);
}

Text *hint(const QString &html, bool brand)
{
    TextStyle st;
    st.font = brand ? brandSans(12) : uiFont(12);
    st.brand = brand;
    st.color = pal(brand).muted;
    auto *t = new Text(html, st);
    QObject::connect(t, &Text::linkActivated, t, [](const QString &href) { openUrl(href); });
    return t;
}

Text *para(const QString &html, double px, bool brand)
{
    TextStyle st;
    st.font = brand ? brandSans(px) : uiFont(px);
    st.brand = brand;
    auto *t = new Text(html, st);
    QObject::connect(t, &Text::linkActivated, t, [](const QString &href) { openUrl(href); });
    return t;
}

Text *h4(const QString &s, bool brand)
{
    TextStyle st;
    st.brand = brand;
    st.upper = true;
    if (brand) {
        st.font = brandMono(11);
        st.font.setLetterSpacing(QFont::AbsoluteSpacing, 11 * 0.12);
        st.color = QColor("#a4a9b4");
    } else {
        st.font = uiFont(12, 600);
        st.font.setLetterSpacing(QFont::AbsoluteSpacing, 12 * 0.04);
        st.color = theme::color("--muted");
    }
    st.wrap = true;
    return new Text(s, st);
}

// ================================================================ FlowLayout

FlowLayout::FlowLayout(QWidget *parent, int gapX, int gapY) : QLayout(parent), m_gapX(gapX), m_gapY(gapY)
{
    setContentsMargins(0, 0, 0, 0);
}

FlowLayout::~FlowLayout()
{
    while (QLayoutItem *item = takeAt(0)) delete item;
}

void FlowLayout::addItem(QLayoutItem *item) { m_items.append(item); }
int FlowLayout::count() const { return m_items.size(); }
QLayoutItem *FlowLayout::itemAt(int index) const { return m_items.value(index); }
QLayoutItem *FlowLayout::takeAt(int index)
{
    return index >= 0 && index < m_items.size() ? m_items.takeAt(index) : nullptr;
}

int FlowLayout::heightForWidth(int width) const
{
    return doLayout(QRect(0, 0, width, 0), true);
}

QSize FlowLayout::minimumSize() const
{
    QSize s;
    for (QLayoutItem *item : m_items) {
        if (item->isEmpty()) continue;
        QWidget *w = item->widget();
        const bool shrinks = w && (w->sizePolicy().hasHeightForWidth());
        s = s.expandedTo(shrinks ? QSize(0, 0) : item->minimumSize());
    }
    const QMargins m = contentsMargins();
    return s + QSize(m.left() + m.right(), m.top() + m.bottom());
}

QSize FlowLayout::sizeHint() const
{
    // one line, everything side by side
    int w = 0, h = 0, n = 0;
    for (QLayoutItem *item : m_items) {
        if (item->isEmpty()) continue;
        const QSize s = item->sizeHint();
        w += s.width() + (n++ ? m_gapX : 0);
        h = qMax(h, s.height());
    }
    const QMargins m = contentsMargins();
    return {w + m.left() + m.right(), h + m.top() + m.bottom()};
}

void FlowLayout::setGeometry(const QRect &rect)
{
    QLayout::setGeometry(rect);
    doLayout(rect, false);
}

int FlowLayout::doLayout(const QRect &rect, bool testOnly) const
{
    const QMargins m = contentsMargins();
    const QRect r = rect.adjusted(m.left(), m.top(), -m.right(), -m.bottom());
    const int W = r.width();
    struct It { QLayoutItem *item; int w; int h; int grow; QString align; bool autoLeft; };
    QVector<QVector<It>> lines;
    QVector<It> line;
    int x = 0;
    auto hintW = [](QLayoutItem *item) {
        int w = item->sizeHint().width();
        w = qMax(w, item->minimumSize().width());
        if (item->maximumSize().width() < w) w = item->maximumSize().width();
        return w;
    };
    for (QLayoutItem *item : m_items) {
        if (item->isEmpty()) continue;
        QWidget *wd = item->widget();
        It it {item, hintW(item), 0, wd ? wd->property("flexGrow").toInt() : 0,
               wd ? wd->property("alignSelf").toString() : QString(), wd && wd->property("marginLeftAuto").toBool()};
        if (it.w > W && W > 0) it.w = W;
        const int need = line.isEmpty() ? it.w : x + m_gapX + it.w;
        if (m_wrap && !line.isEmpty() && need > W) {
            lines.push_back(line);
            line.clear();
            x = 0;
        }
        x = line.isEmpty() ? it.w : x + m_gapX + it.w;
        line.push_back(it);
    }
    if (!line.isEmpty()) lines.push_back(line);

    int y = r.top();
    for (int li = 0; li < lines.size(); ++li) {
        auto &ln = lines[li];
        int used = 0, grow = 0;
        for (int k = 0; k < ln.size(); ++k) {
            used += ln[k].w + (k ? m_gapX : 0);
            grow += ln[k].grow;
        }
        int free = qMax(0, W - used);
        if (grow > 0) {
            int left = free;
            for (auto &it : ln) {
                if (!it.grow) continue;
                const int add = free * it.grow / grow;
                it.w += add;
                left -= add;
            }
            for (auto &it : ln)
                if (it.grow && left > 0) { it.w += left; left = 0; }
            free = 0;
        }
        int lh = 0;
        for (auto &it : ln) {
            it.h = it.item->hasHeightForWidth() ? it.item->heightForWidth(it.w) : it.item->sizeHint().height();
            it.h = qMax(it.h, it.item->minimumSize().height());
            if (it.item->maximumSize().height() < it.h) it.h = it.item->maximumSize().height();
            lh = qMax(lh, it.h);
        }
        if (!testOnly) {
            int cx = r.left();
            for (int k = 0; k < ln.size(); ++k) {
                auto &it = ln[k];
                if (it.autoLeft && free > 0) {
                    cx += free;
                    free = 0;
                }
                int iy = y + (lh - it.h) / 2;
                QString a = it.align;
                if (a.isEmpty()) a = m_alignItems & Qt::AlignTop ? "start" : m_alignItems & Qt::AlignBottom ? "end" : "";
                if (a == "end") iy = y + lh - it.h;
                else if (a == "start") iy = y;
                int ih = it.h;
                if (a == "stretch") { iy = y; ih = lh; }
                it.item->setGeometry(QRect(cx, iy, it.w, ih));
                cx += it.w + m_gapX;
            }
        }
        y += lh + (li + 1 < lines.size() ? m_gapY : 0);
    }
    return y - r.top() + m.top() + m.bottom();
}

QWidget *rowFlex(QWidget *parent, int gap)
{
    auto *w = new QWidget(parent);
    new FlowLayout(w, gap, gap);
    QSizePolicy sp(QSizePolicy::Preferred, QSizePolicy::Preferred);
    sp.setHeightForWidth(true);
    w->setSizePolicy(sp);
    return w;
}

FlowLayout *flow(QWidget *w)
{
    return static_cast<FlowLayout *>(w->layout());
}

QWidget *block(QWidget *parent, const QMargins &padding)
{
    auto *w = new QWidget(parent);
    auto *l = new QVBoxLayout(w);
    l->setContentsMargins(padding);
    l->setSpacing(0);
    return w;
}

QVBoxLayout *vbox(QWidget *w)
{
    return static_cast<QVBoxLayout *>(w->layout());
}

QWidget *spacer()
{
    auto *w = new QWidget;
    w->setProperty("flexGrow", 1);
    w->setFixedHeight(0);
    w->setMinimumWidth(0);
    return w;
}

void add(QWidget *blockWidget, QWidget *w, int marginTop, int marginBottom)
{
    auto *l = static_cast<QBoxLayout *>(blockWidget->layout());
    if (marginTop) l->addSpacing(marginTop);
    l->addWidget(w);
    if (marginBottom) l->addSpacing(marginBottom);
}

void addSpace(QWidget *blockWidget, int px)
{
    static_cast<QBoxLayout *>(blockWidget->layout())->addSpacing(px);
}

void clear(QLayout *layout)
{
    if (!layout) return;
    while (QLayoutItem *item = layout->takeAt(0)) {
        if (QWidget *w = item->widget()) {
            w->hide();
            w->deleteLater();
        }
        if (QLayout *l = item->layout()) clear(l);
        delete item;
    }
}

// ================================================================ controls

QPushButton *btn(const QString &text, const QString &cls, QWidget *parent)
{
    auto *b = new QPushButton(text, parent);
    theme::setCls(b, cls);
    b->setCursor(Qt::PointingHandCursor);
    b->setFocusPolicy(Qt::TabFocus);
    b->setAutoDefault(false);
    b->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    return b;
}

Select::Select(Kind kind, QWidget *parent) : QComboBox(parent), m_kind(kind)
{
    m_px = kind == Field ? 11 : 13;
    setModel(new QStandardItemModel(this));
    setSizeAdjustPolicy(QComboBox::AdjustToContents);
    setFocusPolicy(Qt::TabFocus);
    setCursor(Qt::ArrowCursor);
    setAttribute(Qt::WA_LayoutUsesWidgetRect);
    connect(this, QOverload<int>::of(&QComboBox::activated), this, [this](int) { emit changed(value()); });
}

void Select::addOption(const QString &value, const QString &label)
{
    auto *m = static_cast<QStandardItemModel *>(model());
    auto *item = new QStandardItem(m_inGroup ? QStringLiteral("    ") + label : label);
    item->setData(value, Qt::UserRole);
    item->setData(label, Qt::UserRole + 1);
    item->setData(m_inGroup, Qt::UserRole + 2);
    m->appendRow(item);
    if (currentIndex() < 0 || !(m->item(currentIndex())->flags() & Qt::ItemIsEnabled))
        setCurrentIndex(m->rowCount() - 1);
}

void Select::addGroup(const QString &label)
{
    auto *m = static_cast<QStandardItemModel *>(model());
    auto *item = new QStandardItem(label);
    item->setFlags(Qt::NoItemFlags);
    QFont f = item->font();
    f.setBold(true);
    item->setFont(f);
    m->appendRow(item);
    m_inGroup = true;
}

QString Select::value() const
{
    return currentIndex() < 0 ? QString() : itemData(currentIndex(), Qt::UserRole).toString();
}

void Select::setValue(const QString &v)
{
    for (int i = 0; i < count(); ++i)
        if ((itemData(i, Qt::UserRole + 1).isValid()) && itemData(i, Qt::UserRole).toString() == v) {
            setCurrentIndex(i);
            return;
        }
}

namespace {
// Chrome's menulist (macOS): the text starts 4px inside the padding and the box reserves 12px at
// the end for the arrow. Options inside an <optgroup> are measured with 4 leading spaces.
constexpr double SelectInnerStart = 4;
constexpr double SelectArrowArea = 12;
}

QSize Select::sizeHint() const
{
    const QFontMetricsF fm(uiFont(m_px));
    double w = 0;
    for (int i = 0; i < count(); ++i) {
        const QVariant lab = itemData(i, Qt::UserRole + 1);
        if (!lab.isValid()) continue;
        const QString t = itemData(i, Qt::UserRole + 2).toBool() ? QStringLiteral("    ") + lab.toString() : lab.toString();
        w = qMax(w, fm.horizontalAdvance(t));
    }
    const double pad = m_kind == Rule ? 5 : 7;
    int width = int(std::ceil(w + 2 + pad * 2 + SelectInnerStart + SelectArrowArea));
    if (m_maxW > 0) width = qMin(width, m_maxW);
    const int h = m_kind == Rule ? 26 : 28;
    return {width, h};
}

QSize Select::minimumSizeHint() const
{
    return sizeHint();
}

void Select::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const Pal pl = pal(m_brand);
    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath path;
    path.addRoundedRect(r, 4, 4);
    p.fillPath(path, isEnabled() ? pl.input : pl.input);
    p.setPen(QPen(pl.border, 1));
    p.drawPath(path);
    const double pad = m_kind == Rule ? 5 : 7;
    const QFont f = uiFont(m_px);
    p.setFont(f);
    p.setPen(m_brand ? QColor("#f4f5f7") : pl.text);
    const QString label = currentIndex() >= 0 ? itemData(currentIndex(), Qt::UserRole + 1).toString() : QString();
    const double tx = 1 + pad + SelectInnerStart;
    const double right = width() - 1 - pad - 11;   // where Chrome clips a long option
    p.save();
    p.setClipRect(QRectF(tx, 0, qMax(0.0, right - tx), height()));
    const QFontMetricsF fm(f);
    const double base = (height() - (std::round(fm.ascent()) + std::round(fm.descent()))) / 2 + std::round(fm.ascent());
    p.drawText(QPointF(tx, base), label);
    p.restore();
    // chevron (Chrome's menulist arrow: 9 x 5.5, about 2px inside the right border)
    const double cx = width() - 7, cy = height() / 2.0;
    QPen pen(m_brand ? QColor("#f4f5f7") : pl.text, 1.9);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.drawPolyline(QPolygonF({QPointF(cx - 3.6, cy - 1.9), QPointF(cx, cy + 1.8), QPointF(cx + 3.6, cy - 1.9)}));
}

Input::Input(Kind kind, bool number, QWidget *parent) : QLineEdit(parent), m_number(number), m_kind(kind)
{
    theme::setCls(this, kind == Field ? "pin f11" : kind == Rule ? "rin" : "pin");
    setAttribute(Qt::WA_MacShowFocusRect, false);
    setAttribute(Qt::WA_LayoutUsesWidgetRect);
}

double Input::number() const
{
    const QString s = text().trimmed();
    if (s.isEmpty()) return 0;
    bool ok = false;
    const double v = s.toDouble(&ok);
    if (!ok) return m_number ? 0 : NAN;
    return v;
}

void Input::setWidthPx(int w)
{
    m_w = w;
    setFixedWidth(w);
}

QSize Input::sizeHint() const
{
    if (m_w > 0) return {m_w, m_kind == Rule ? 26 : 28};
    // Chrome's default <input> width (size=20) for the font
    const QFontMetricsF fm(uiFont(m_kind == Field ? 11 : 13));
    const double pad = m_kind == Rule ? 5 : 7;
    const int w = int(std::round(fm.averageCharWidth() * 20 + pad * 2));
    return {w, m_kind == Rule ? 26 : 28};
}

CodeArea::CodeArea(bool tabIndents, QWidget *parent) : QTextEdit(parent), m_tab(tabIndents)
{
    theme::setCls(this, "in code");
    setAcceptRichText(false);
    setLineWrapMode(QTextEdit::WidgetWidth);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setTabChangesFocus(!tabIndents);
    setAttribute(Qt::WA_MacShowFocusRect, false);
    document()->setDocumentMargin(0);
    document()->setDefaultFont(monoFont(12));
    setFont(monoFont(12));
}

void CodeArea::applyLineHeight()
{
    QTextCursor c(document());
    c.select(QTextCursor::Document);
    QTextBlockFormat f;
    f.setLineHeight(18, QTextBlockFormat::FixedHeight);
    c.mergeBlockFormat(f);
}

void CodeArea::setCode(const QString &code)
{
    setPlainText(code);
    applyLineHeight();
    moveCursor(QTextCursor::Start);
}

void CodeArea::keyPressEvent(QKeyEvent *e)
{
    // e.key === "Tab" is Shift+Tab too (Qt calls that Backtab)
    if (m_tab && (e->key() == Qt::Key_Tab || e->key() == Qt::Key_Backtab)
        && !(e->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))) {
        textCursor().insertText("  ");
        return;
    }
    QTextEdit::keyPressEvent(e);
}

void CodeArea::insertFromMimeData(const QMimeData *source)
{
    QTextEdit::insertFromMimeData(source);
    applyLineHeight();
}

QWidget *field(const QString &label, QWidget *control, bool brand)
{
    auto *w = new QWidget;
    auto *l = new QVBoxLayout(w);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(3);
    TextStyle st;
    st.font = uiFont(11);
    st.color = pal(brand).muted;
    st.wrap = false;
    st.brand = brand;
    auto *t = new Text(label, st);
    l->addWidget(t);
    l->addWidget(control);
    if (control->minimumWidth() < 70 && !qobject_cast<QPushButton *>(control)) control->setMinimumWidth(70);
    return w;
}

QWidget *fieldRow(const QString &label, QWidget *control)
{
    auto *w = new QWidget;
    auto *l = new QHBoxLayout(w);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(6);
    TextStyle st;
    st.font = uiFont(11);
    st.color = theme::color("--muted");
    st.wrap = false;
    l->addWidget(new Text(label, st), 0, Qt::AlignVCenter);
    l->addWidget(control, 0, Qt::AlignVCenter);
    if (control->minimumWidth() < 70) control->setMinimumWidth(70);
    return w;
}

// ================================================================ Table

struct Table::Geo {
    int width = -1;
    QVector<double> colX, colW;
    double headH = 0;
    QVector<double> rowY, rowH;
    double total = 0;
    // laid out cells: [row + 1][col] (row -1 = header)
    QVector<QVector<std::shared_ptr<QTextLayout>>> cells;
};

// The widest and narrowest each column can be (max-content, min-content), whatever the width.
struct Table::Measure {
    int ncol = 0;
    QVector<double> maxW, minW;
};

Table::Table(QWidget *parent) : QWidget(parent)
{
    setMouseTracking(true);
    QSizePolicy sp(QSizePolicy::Preferred, QSizePolicy::Preferred);
    sp.setHeightForWidth(true);
    setSizePolicy(sp);
}

void Table::setHeader(const QVector<Cell> &cells)
{
    m_header = cells;
    relayout();
}

void Table::setRows(const QVector<Row> &rows)
{
    m_rows = rows;
    m_hoverRow = -1;
    relayout();
}

void Table::relayout()
{
    m_geos.clear();
    m_measure.reset();
    updateGeometry();
    update();
}

QFont Table::cellFont(const Cell &c, bool header) const
{
    if (header) {
        if (m_brand) {
            QFont f = brandMono(10.5);
            f.setLetterSpacing(QFont::AbsoluteSpacing, 10.5 * 0.08);
            return f;
        }
        return uiFont(m_variant == Seas ? 11 : 12, 500);
    }
    QFont f = m_brand ? brandSans(12) : uiFont(m_variant == Seas ? 11 : 12);
    if (c.bold || (m_variant == Heat && c.cls.contains("base"))) f.setWeight(QFont::Bold);
    if (c.bold && m_variant == Seas) f.setWeight(QFont::DemiBold);
    return f;
}

namespace {
constexpr double BtnH = 24;
double btnWidth(const QString &text, bool brand)
{
    const QFontMetricsF fm(brand ? brandSans(12) : uiFont(12));
    return std::ceil(fm.horizontalAdvance(text)) + (brand ? 28 : 16) + 2;
}
} // namespace

const Table::Measure &Table::measure() const
{
    if (m_measure) return *m_measure;
    auto m = std::make_shared<Measure>();
    const Pal pl = pal(m_brand);
    int ncol = m_header.size();
    for (const Row &r : m_rows) {
        int n = 0;
        for (const Cell &c : r.cells) n += c.colspan;
        ncol = qMax(ncol, n);
    }
    const double padX = 8;
    QVector<double> maxW(ncol, 0), minW(ncol, 0);
    auto measureCell = [&](const Cell &c, bool header, int col) {
        if (c.colspan != 1) return;
        double mx = 0, mn = 0;
        if (!c.button.isEmpty()) {
            mx = mn = btnWidth(c.button, m_brand);
        } else {
            const QFont f = cellFont(c, header);
            const Rich rich = parseRich(c.html, f, pl.text, pl);
            QString txt = rich.text;
            if (header && m_brand) txt = txt.toUpper();
            QTextLayout tl(txt, f);
            tl.setFormats(rich.formats);
            tl.beginLayout();
            QTextLine line = tl.createLine();
            if (line.isValid()) line.setLineWidth(1e6);
            tl.endLayout();
            mx = line.isValid() ? line.naturalTextWidth() : 0;
            mn = mx;
            if (m_wrap) {
                // min-content: the longest word
                const QFontMetricsF fm(f);
                mn = 0;
                for (const QString &w : txt.split(' ', Qt::SkipEmptyParts)) mn = qMax(mn, fm.horizontalAdvance(w));
            }
        }
        maxW[col] = qMax(maxW[col], std::ceil(mx) + padX * 2);
        minW[col] = qMax(minW[col], std::ceil(mn) + padX * 2);
    };
    for (int c = 0; c < m_header.size(); ++c) measureCell(m_header[c], true, c);
    for (const Row &r : m_rows) {
        int col = 0;
        for (const Cell &c : r.cells) {
            if (col < ncol) measureCell(c, false, col);
            col += c.colspan;
        }
    }
    m->ncol = ncol;
    m->maxW = maxW;
    m->minW = minW;
    m_measure = m;
    return *m_measure;
}

const Table::Geo &Table::geo(int width) const
{
    constexpr int Kept = 4;
    for (int i = 0; i < m_geos.size(); ++i) {
        if (m_geos[i]->width != width) continue;
        if (i > 0) m_geos.move(i, 0);
        return *m_geos.first();
    }
    auto g = std::make_shared<Geo>();
    g->width = width;
    const Pal pl = pal(m_brand);
    const Measure &m = measure();
    const int ncol = m.ncol;
    const QVector<double> &maxW = m.maxW, &minW = m.minW;
    const double padX = 8;
    const double lineTd = (m_variant == Seas ? 11 : 12) * 1.4;
    double sumMax = 0, sumMin = 0;
    for (int c = 0; c < ncol; ++c) {
        sumMax += maxW[c];
        sumMin += minW[c];
    }
    g->colW.resize(ncol);
    const double W = width;
    for (int c = 0; c < ncol; ++c) {
        if (sumMax <= W) g->colW[c] = maxW[c] + (sumMax > 0 ? (W - sumMax) * maxW[c] / sumMax : (W / ncol));
        else if (sumMin >= W) g->colW[c] = minW[c];
        else g->colW[c] = minW[c] + (maxW[c] - minW[c]) * (W - sumMin) / qMax(1.0, sumMax - sumMin);
    }
    g->colX.resize(ncol + 1);
    double x = 0;
    for (int c = 0; c < ncol; ++c) {
        g->colX[c] = x;
        x += g->colW[c];
    }
    g->colX[ncol] = x;

    auto layCell = [&](const Cell &c, bool header, int col) -> std::pair<std::shared_ptr<QTextLayout>, double> {
        if (!c.button.isEmpty()) return {nullptr, BtnH};
        const QFont f = cellFont(c, header);
        Pal cp = pl;
        QColor color = header ? (m_brand ? QColor("#8d929e") : pl.muted) : pl.text;
        const QColor cc = clsColor(c.cls, pl);
        if (cc.isValid()) color = cc;
        const Rich rich = parseRich(c.html, f, color, pl);
        QString txt = rich.text;
        if (header && m_brand) txt = txt.toUpper();
        auto tl = std::make_shared<QTextLayout>(txt, f);
        tl->setFormats(rich.formats);
        QTextOption opt;
        opt.setWrapMode(m_wrap ? QTextOption::WordWrap : QTextOption::NoWrap);
        tl->setTextOption(opt);
        double cw = 0;
        for (int k = col; k < qMin(ncol, col + c.colspan); ++k) cw += g->colW[k];
        cw -= padX * 2;
        const double lh = (header ? (m_brand ? 10.5 : (m_variant == Seas ? 11 : 12)) : (m_variant == Seas ? 11 : 12)) * 1.4;
        const double base = baselineIn(f, lh);
        tl->beginLayout();
        double y = 0;
        for (;;) {
            QTextLine line = tl->createLine();
            if (!line.isValid()) break;
            line.setLineWidth(m_wrap ? qMax(1.0, cw) : 1e6);
            line.setPosition(QPointF(0, y + base - line.ascent()));
            y += lh;
        }
        tl->endLayout();
        return {tl, qMax(y, lh)};
    };
    // header
    const int rowsN = m_rows.size();
    g->cells.resize(rowsN + 1);
    double y = 0;
    if (!m_header.isEmpty()) {
        double h = 0;
        g->cells[0].resize(m_header.size());
        for (int c = 0; c < m_header.size(); ++c) {
            auto [tl, ch] = layCell(m_header[c], true, c);
            g->cells[0][c] = tl;
            h = qMax(h, ch);
        }
        g->headH = h + 12 + 1;
        y = g->headH;
    }
    g->rowY.resize(rowsN);
    g->rowH.resize(rowsN);
    for (int r = 0; r < rowsN; ++r) {
        double h = lineTd;
        g->cells[r + 1].resize(m_rows[r].cells.size());
        int col = 0;
        for (int c = 0; c < m_rows[r].cells.size(); ++c) {
            auto [tl, ch] = layCell(m_rows[r].cells[c], false, col);
            g->cells[r + 1][c] = tl;
            h = qMax(h, ch);
            col += m_rows[r].cells[c].colspan;
        }
        g->rowY[r] = y;
        g->rowH[r] = h + 10 + 1;
        y += g->rowH[r];
    }
    g->total = y;
    m_geos.prepend(g);
    if (m_geos.size() > Kept) m_geos.removeLast();
    return *m_geos.first();
}

int Table::heightForWidth(int w) const
{
    return int(std::ceil(geo(w).total));
}

QSize Table::sizeHint() const
{
    const int w = width() > 0 ? width() : 400;
    return {w, heightForWidth(w)};
}

QSize Table::minimumSizeHint() const
{
    return {0, 0};
}

QRectF Table::buttonRect(int row, int col, const Geo &g) const
{
    const Row &r = m_rows[row];
    int cx = 0;
    for (int c = 0; c < col; ++c) cx += r.cells[c].colspan;
    const Cell &cell = r.cells[col];
    const double bw = btnWidth(cell.button, m_brand);
    const double x0 = g.colX[cx], x1 = g.colX[qMin(g.colX.size() - 1, cx + cell.colspan)];
    double bx = x1 - 8 - bw;   // right aligned (td default)
    if (cell.cls.contains('l') || cx == 0) bx = x0 + 8;
    const double by = g.rowY[row] + (g.rowH[row] - 1 - BtnH) / 2;
    return QRectF(bx, by, bw, BtnH);
}

double Table::stickyOffset() const
{
    if (!m_sticky || !m_scroll || m_header.isEmpty()) return 0;
    // position: sticky; top: 0 sticks to the scroll container's padding edge, not its border edge.
    int pad = 0;
    if (QWidget *content = m_scroll->widget(); content && content->layout())
        pad = content->layout()->contentsMargins().top();
    const int top = mapTo(m_scroll->viewport(), QPoint(0, 0)).y() - pad;
    if (top >= 0) return 0;
    const Geo &g = geo(width());
    return qMin(double(-top), qMax(0.0, g.total - g.headH));
}

void Table::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::TextAntialiasing);
    const Geo &g = geo(width());
    const Pal pl = pal(m_brand);
    const QColor line = m_brand ? QColor(255, 255, 255, qRound(0.08 * 255)) : pl.border;
    const double padX = 8;
    auto drawCellText = [&](const std::shared_ptr<QTextLayout> &tl, const Cell &c, double x0, double x1, double top,
                            double h, bool header, int colIndex) {
        if (!tl) return;
        double content = 0;
        for (int i = 0; i < tl->lineCount(); ++i) content = qMax(content, tl->lineAt(i).naturalTextWidth());
        const int lines = tl->lineCount();
        const double lh = (header ? (m_brand ? 10.5 : (m_variant == Seas ? 11 : 12)) : (m_variant == Seas ? 11 : 12)) * 1.4;
        const double textH = qMax(1, lines) * lh;
        const double ty = top + (h - textH) / 2;   // vertical-align: middle
        bool left = colIndex == 0 || c.cls.split(' ').contains("l");
        bool center = false;
        if (m_variant == Heat && !header) { center = true; left = false; }
        for (int i = 0; i < tl->lineCount(); ++i) {
            QTextLine ln = tl->lineAt(i);
            double x;
            if (left) x = x0 + padX;
            else if (center) x = (x0 + x1) / 2 - ln.naturalTextWidth() / 2;
            else x = x1 - padX - ln.naturalTextWidth();
            ln.draw(&p, QPointF(x, ty));
        }
    };
    // rows
    for (int r = 0; r < m_rows.size(); ++r) {
        const double top = g.rowY[r], h = g.rowH[r];
        const QRectF rowRect(0, top, width(), h);
        if (r == m_hoverRow) p.fillRect(rowRect.adjusted(0, 0, 0, -1), pl.hover);
        int col = 0;
        for (int c = 0; c < m_rows[r].cells.size(); ++c) {
            const Cell &cell = m_rows[r].cells[c];
            const double x0 = g.colX.value(col), x1 = g.colX.value(qMin(int(g.colX.size()) - 1, col + cell.colspan));
            if (cell.bg.isValid()) p.fillRect(QRectF(x0, top, x1 - x0, h - 1), cell.bg);
            if (cell.topBorder) p.fillRect(QRectF(x0, top - 1, x1 - x0, 1), line);
            if (!cell.button.isEmpty()) {
                const QRectF br = buttonRect(r, c, g);
                const bool hov = br.contains(m_mouse);
                QPainterPath path;
                const double rad = m_brand ? BtnH / 2 : 4;
                path.addRoundedRect(br.adjusted(0.5, 0.5, -0.5, -0.5), rad, rad);
                p.setRenderHint(QPainter::Antialiasing);
                const bool danger = cell.buttonCls.contains("danger");
                QColor bgc = hov ? pl.hover : pl.panel, bc = pl.border, fg = danger ? pl.down : pl.text;
                if (m_brand) {
                    if (danger) {
                        bgc = QColor(255, 122, 122, qRound((hov ? 0.2 : 0.12) * 255));
                        bc = QColor(255, 122, 122, qRound(0.4 * 255));
                        fg = QColor("#ff7a7a");
                    } else {
                        bgc = hov ? QColor(255, 255, 255, 15) : pl.panel;
                        fg = QColor("#f4f5f7");
                    }
                }
                p.fillPath(path, bgc);
                p.setPen(QPen(bc, 1));
                p.drawPath(path);
                p.setRenderHint(QPainter::Antialiasing, false);
                p.setPen(fg);
                p.setFont(m_brand ? brandSans(12) : uiFont(12));
                p.drawText(br, Qt::AlignCenter, cell.button);
            } else {
                drawCellText(g.cells[r + 1][c], cell, x0, x1, top + 5, h - 11, false, col);
            }
            if (cell.outline) {
                p.setPen(QPen(pl.gold, 2));
                p.setBrush(Qt::NoBrush);
                p.drawRect(QRectF(x0 + 1, top + 1, x1 - x0 - 2, h - 1 - 2));
            }
            col += cell.colspan;
        }
        p.fillRect(QRectF(0, top + h - 1, width(), 1), line);
    }
    // header (sticky)
    if (!m_header.isEmpty()) {
        const double off = stickyOffset();
        const QRectF hr(0, off, width(), g.headH);
        // .ptq-brand table.t th { background: transparent }: stuck, the rows show through it.
        if (!m_brand) p.fillRect(hr, pl.bg);
        for (int c = 0; c < m_header.size(); ++c)
            drawCellText(g.cells[0][c], m_header[c], g.colX[c], g.colX[c + 1], off + 6, g.headH - 13, true, c);
        p.fillRect(QRectF(0, off + g.headH - 1, width(), 1), m_brand ? QColor(255, 255, 255, qRound(0.1 * 255)) : pl.border);
    }
}

void Table::mouseMoveEvent(QMouseEvent *e)
{
    m_mouse = e->position().toPoint();
    const Geo &g = geo(width());
    int hover = -1;
    const double off = stickyOffset();
    if (!(m_mouse.y() >= off && m_mouse.y() < off + g.headH && !m_header.isEmpty()))
        for (int r = 0; r < m_rows.size(); ++r)
            if (m_mouse.y() >= g.rowY[r] && m_mouse.y() < g.rowY[r] + g.rowH[r]) hover = r;
    if (hover != m_hoverRow) m_hoverRow = hover;
    update();
}

void Table::leaveEvent(QEvent *)
{
    m_hoverRow = -1;
    m_mouse = QPoint(-1, -1);
    update();
}

void Table::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton) return;
    const QPointF pos = e->position();
    const Geo &g = geo(width());
    const double off = stickyOffset();
    if (!m_header.isEmpty() && pos.y() >= off && pos.y() < off + g.headH) {
        for (int c = 0; c < m_header.size(); ++c)
            if (pos.x() >= g.colX[c] && pos.x() < g.colX[c + 1]) emit headerClicked(c);
        return;
    }
    for (int r = 0; r < m_rows.size(); ++r) {
        if (!(pos.y() >= g.rowY[r] && pos.y() < g.rowY[r] + g.rowH[r])) continue;
        for (int c = 0; c < m_rows[r].cells.size(); ++c) {
            if (m_rows[r].cells[c].button.isEmpty()) continue;
            if (buttonRect(r, c, g).contains(pos)) {
                emit buttonClicked(r, c, m_rows[r].cells[c].data);
                return;
            }
        }
        emit rowClicked(r);
        return;
    }
}

void Table::resizeEvent(QResizeEvent *)
{
    update();
}

void Table::showEvent(QShowEvent *)
{
    if (m_scroll) return;
    for (QWidget *w = parentWidget(); w; w = w->parentWidget()) {
        if (auto *sa = qobject_cast<QScrollArea *>(w)) {
            m_scroll = sa;
            connect(sa->verticalScrollBar(), &QScrollBar::valueChanged, this, [this] { update(); });
            break;
        }
    }
}

// ================================================================ Stats

Stats::Stats(QWidget *parent) : QWidget(parent)
{
    QSizePolicy sp(QSizePolicy::Preferred, QSizePolicy::Fixed);
    sp.setHeightForWidth(true);
    setSizePolicy(sp);
}

void Stats::setItems(const QVector<Stat> &items)
{
    m_items = items;
    updateGeometry();
    update();
}

int Stats::columns(int w) const
{
    return qMax(1, (w + 8) / (m_min + 8));
}

namespace {
// One line of value: the box height is this plus 16 * 1.4 per extra line.
constexpr double StatH = 1 + 8 + 11 * 1.4 + 2 + 16 * 1.4 + 8 + 1;

// .stat .v: 16px semibold, font-variant-numeric: tabular-nums.
QFont statValueFont(bool brand)
{
    QFont f = brand ? brandSans(16, 600) : uiFont(16, 600);
    f.setFeature(QFont::Tag("tnum"), 1);
    return f;
}

// white-space: normal inside the box (padding 10px, border 1px): long values such as
// "-26.7676 (-0.03%)" wrap at their spaces like the browser does.
QStringList statLines(const QString &v, const QFont &f, double boxW)
{
    const QFontMetricsF fm(f);
    const double avail = boxW - 2 - 20;
    QStringList lines;
    QString cur;
    for (const QString &word : v.split(' ')) {
        const QString next = cur.isEmpty() ? word : cur + ' ' + word;
        if (!cur.isEmpty() && fm.horizontalAdvance(next) > avail + 0.01) {
            lines << cur;
            cur = word;
        } else cur = next;
    }
    lines << cur;
    return lines;
}

// .stat .k: 11px; the label wraps too (e.g. "PTQ max drawdown" in a narrow box).
QFont statKeyFont(bool brand)
{
    return brand ? brandSans(11) : uiFont(11);
}

// Grid rows stretch every box of a row to its tallest one.
QVector<double> statRowHeights(const QVector<Stat> &items, int n, double cw, const QFont &fk, const QFont &fv)
{
    QVector<double> rows((items.size() + n - 1) / n, StatH);
    for (int i = 0; i < items.size(); ++i)
        rows[i / n] = std::max(rows[i / n], StatH + (statLines(items[i].k, fk, cw).size() - 1) * 11 * 1.4
                                                + (statLines(items[i].v, fv, cw).size() - 1) * 16 * 1.4);
    return rows;
}
} // namespace

int Stats::heightForWidth(int w) const
{
    if (m_items.isEmpty()) return 0;
    const int n = columns(w);
    const QVector<double> rows = statRowHeights(m_items, n, (w - 8.0 * (n - 1)) / n, statKeyFont(m_brand), statValueFont(m_brand));
    double h = 8.0 * (rows.size() - 1);
    for (double r : rows) h += r;
    return int(std::ceil(h));
}

QSize Stats::sizeHint() const
{
    const int w = width() > 0 ? width() : 600;
    return {w, heightForWidth(w)};
}

void Stats::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const Pal pl = pal(m_brand);
    const int n = columns(width());
    const double cw = (width() - 8.0 * (n - 1)) / n;
    const QFont fk = statKeyFont(m_brand);
    const QFont fv = statValueFont(m_brand);
    const QVector<double> rowH = statRowHeights(m_items, n, cw, fk, fv);
    QVector<double> rowTop(rowH.size(), 0);
    for (int r = 1; r < rowH.size(); ++r) rowTop[r] = rowTop[r - 1] + rowH[r - 1] + 8;
    for (int i = 0; i < m_items.size(); ++i) {
        const int r = i / n, c = i % n;
        const QRectF box(c * (cw + 8), rowTop[r], cw, rowH[r]);
        QPainterPath path;
        const double rad = m_brand ? 14 : 6;
        path.addRoundedRect(box.adjusted(0.5, 0.5, -0.5, -0.5), rad, rad);
        p.fillPath(path, pl.panel);
        p.setPen(QPen(pl.border, 1));
        p.drawPath(path);
        p.setFont(fk);
        p.setPen(pl.muted);
        const double kTop = box.top() + 1 + 8;
        const QStringList kLines = statLines(m_items[i].k, fk, cw);
        for (int l = 0; l < kLines.size(); ++l)
            p.drawText(QPointF(box.left() + 11, kTop + l * 11 * 1.4 + baselineIn(fk, 11 * 1.4)), kLines[l]);
        p.setFont(fv);
        QColor vc = clsColor(m_items[i].cls, pl);
        p.setPen(vc.isValid() ? vc : (m_brand ? QColor("#c9ccd3") : pl.text));
        const double vTop = kTop + kLines.size() * 11 * 1.4 + 2;
        p.save();
        p.setClipRect(box.adjusted(1, 1, -1, -1));
        const QStringList lines = statLines(m_items[i].v, fv, cw);
        for (int l = 0; l < lines.size(); ++l)
            p.drawText(QPointF(box.left() + 11, vTop + l * 16 * 1.4 + baselineIn(fv, 16 * 1.4)), lines[l]);
        p.restore();
    }
}

// ================================================================ containers

Box::Box(QWidget *parent) : QWidget(parent)
{
    const Pal p = pal();
    bg = p.panel;
    border = p.border;
}

void Box::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QRectF r = rect();
    if (ringWidth > 0) {
        r.adjust(ringWidth, ringWidth, -ringWidth, -ringWidth);
        QPainterPath ringPath;
        ringPath.addRoundedRect(r.adjusted(-ringWidth, -ringWidth, ringWidth, ringWidth), radius + ringWidth,
                                radius + ringWidth);
        QPainterPath inner;
        inner.addRoundedRect(r, radius, radius);
        p.fillPath(ringPath.subtracted(inner), ring);
    }
    if (borderLeftOnly) {
        if (bg.isValid() && bg.alpha()) p.fillRect(r, bg);
        p.fillRect(QRectF(r.left(), r.top(), borderWidth, r.height()), border);
        return;
    }
    QPainterPath path;
    path.addRoundedRect(r.adjusted(borderWidth / 2, borderWidth / 2, -borderWidth / 2, -borderWidth / 2), radius, radius);
    if (paintBackground) {
        p.save();
        p.setClipPath(path);
        paintBackground(p, r);
        p.restore();
    } else if (bg.isValid() && bg.alpha()) {
        p.fillPath(path, bg);
    }
    if (borderWidth > 0) {
        p.setPen(QPen(hover && m_in && hoverBorder.isValid() ? hoverBorder : border, borderWidth));
        p.drawPath(path);
    }
}

void Box::enterEvent(QEnterEvent *)
{
    m_in = true;
    if (hover) update();
}

void Box::leaveEvent(QEvent *)
{
    m_in = false;
    if (hover) update();
}

Box *card(QWidget *parent)
{
    auto *b = new Box(parent);
    auto *l = new QVBoxLayout(b);
    l->setContentsMargins(11, 9, 11, 9);
    l->setSpacing(0);
    return b;
}

ScrollCol::ScrollCol(const QMargins &padding, QWidget *parent) : QScrollArea(parent)
{
    setFrameShape(QFrame::NoFrame);
    setWidgetResizable(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    viewport()->setAutoFillBackground(false);
    setAutoFillBackground(false);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_body = new QWidget;
    m_body->setAutoFillBackground(false);
    auto *outer = new QVBoxLayout(m_body);
    outer->setContentsMargins(padding);
    outer->setSpacing(0);
    m_inner = new QWidget;
    auto *l = new QVBoxLayout(m_inner);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(0);
    outer->addWidget(m_inner);
    outer->addStretch(1);
    setWidget(m_body);
    m_bar = new QScrollBar(Qt::Vertical, this);
    m_bar->hide();
    QScrollBar *real = verticalScrollBar();
    connect(real, &QScrollBar::rangeChanged, m_bar, [this](int min, int max) {
        m_bar->setRange(min, max);
        m_bar->setPageStep(verticalScrollBar()->pageStep());
        if (max <= min) m_bar->hide();
    });
    connect(real, &QScrollBar::valueChanged, m_bar, &QScrollBar::setValue);
    connect(m_bar, &QScrollBar::valueChanged, real, &QScrollBar::setValue);
}

void ScrollCol::placeBar()
{
    m_bar->setGeometry(width() - 8, 0, 8, height());
    m_bar->raise();
}

void ScrollCol::resizeEvent(QResizeEvent *e)
{
    QScrollArea::resizeEvent(e);
    placeBar();
}

void ScrollCol::enterEvent(QEnterEvent *e)
{
    QScrollArea::enterEvent(e);
    if (verticalScrollBar()->maximum() > 0) {
        placeBar();
        m_bar->show();
    }
}

void ScrollCol::leaveEvent(QEvent *e)
{
    QScrollArea::leaveEvent(e);
    m_bar->hide();
}

QVBoxLayout *ScrollCol::layout() const
{
    return static_cast<QVBoxLayout *>(m_inner->layout());
}

void ScrollCol::setFill(bool on)
{
    auto *outer = static_cast<QVBoxLayout *>(m_body->layout());
    outer->setStretchFactor(m_inner, on ? 1 : 0);
    if (QLayoutItem *last = outer->itemAt(outer->count() - 1); last && last->spacerItem())
        last->spacerItem()->changeSize(0, 0, QSizePolicy::Minimum, on ? QSizePolicy::Fixed : QSizePolicy::Expanding);
    outer->invalidate();
}

Split::Split(QWidget *parent, bool brand) : QWidget(parent)
{
    left = new ScrollCol(QMargins(12, 10, 12, 10), this);
    right = new ScrollCol(QMargins(12, 10, 12, 10), this);
    divider = brand ? QColor(255, 255, 255, qRound(0.1 * 255)) : theme::color("--border");
}

void Split::resizeEvent(QResizeEvent *)
{
    const int W = width();
    int lw;
    if (leftRatio > 0) lw = int(std::round(W * leftRatio / (leftRatio + 1)));
    else lw = qBound(qMin(330, W), W, 420);
    left->setGeometry(0, 0, lw - 1, height());
    right->setGeometry(lw, 0, W - lw, height());
}

void Split::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    if (paintBackground) paintBackground(p, rect());
    const int lw = left->width();
    p.fillRect(QRect(lw, 0, 1, height()), divider);
}

// ================================================================ misc

namespace {
const char *const MonthsShort[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
const char *const Weekdays[] = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};
// Chromium formats en-US times with a plain space before AM/PM (not ICU's U+202F), so the line
// can break there, as it does in the web app.
const QChar AmPmSpace(' ');

QDateTime local(double ms)
{
    return QDateTime::fromMSecsSinceEpoch(qint64(std::floor(ms)));
}
} // namespace

QString localeString(double ms)
{
    const QDateTime d = local(ms);
    const QTime t = d.time();
    const int h12 = t.hour() % 12 == 0 ? 12 : t.hour() % 12;
    return QString("%1/%2/%3, %4:%5:%6%7%8")
        .arg(d.date().month())
        .arg(d.date().day())
        .arg(d.date().year())
        .arg(h12)
        .arg(t.minute(), 2, 10, QChar('0'))
        .arg(t.second(), 2, 10, QChar('0'))
        .arg(AmPmSpace)
        .arg(t.hour() < 12 ? "AM" : "PM");
}

QString localeDateString(double ms)
{
    const QDate d = local(ms).date();
    return QString("%1/%2/%3").arg(d.month()).arg(d.day()).arg(d.year());
}

QString localeTimeHM(double ms)
{
    const QTime t = local(ms).time();
    const int h12 = t.hour() % 12 == 0 ? 12 : t.hour() % 12;
    return QString("%1:%2%3%4")
        .arg(h12, 2, 10, QChar('0'))
        .arg(t.minute(), 2, 10, QChar('0'))
        .arg(AmPmSpace)
        .arg(t.hour() < 12 ? "AM" : "PM");
}

QString weekdayDayMonth(double ms, bool utc)
{
    QDateTime d = local(ms);
    if (utc) d = d.toTimeZone(QTimeZone::UTC);
    const QDate date = d.date();
    return QString("%1, %2 %3").arg(Weekdays[date.dayOfWeek() - 1]).arg(MonthsShort[date.month() - 1]).arg(date.day());
}

QString monthYear(double ms)
{
    const QDate d = local(ms).date();
    return QString("%1 %2").arg(MonthsShort[d.month() - 1]).arg(d.year());
}

QString monthDayUtc(double sec)
{
    const QDate d = QDateTime::fromSecsSinceEpoch(qint64(std::floor(sec)), QTimeZone::UTC).date();
    return QString("%1 %2").arg(MonthsShort[d.month() - 1]).arg(d.day());
}

QString intLocale(double v)
{
    if (!std::isfinite(v)) return js::numberToString(v);
    QLocale en(QLocale::English, QLocale::UnitedStates);
    if (v == std::floor(v)) return en.toString(qint64(v));
    // up to 3 fraction digits, like toLocaleString()
    QString s = en.toString(v, 'f', 3);
    while (s.endsWith('0')) s.chop(1);
    if (s.endsWith('.')) s.chop(1);
    return s;
}

QString num(double v)
{
    return js::numberToString(v);
}

QString numPrecision(double v, int p)
{
    if (!std::isfinite(v)) return js::numberToString(v);
    return js::numberToString(QString::number(v, 'g', p).toDouble());
}

bool confirm(QWidget *parent, const QString &text)
{
    QMessageBox box(QMessageBox::NoIcon, QString(), text, QMessageBox::Ok | QMessageBox::Cancel, parent);
    box.setDefaultButton(QMessageBox::Ok);
    return box.exec() == QMessageBox::Ok;
}

QString prompt(QWidget *parent, const QString &text, bool *ok)
{
    bool accepted = false;
    const QString v = QInputDialog::getText(parent, QString(), text, QLineEdit::Normal, QString(), &accepted);
    if (ok) *ok = accepted;
    return accepted ? v : QString();
}

void openUrl(const QString &url)
{
    if (!url.isEmpty()) QDesktopServices::openUrl(QUrl(url));
}

} // namespace qtm::kit
