// Port of public/js/ptq.js: PTQ connector UI, "Check with PTQ" under the strategy tester, and the
// PTQ tab. The terminal only reads from PTQ and can only make it do less (stop, disarm).
// Arming a strategy to trade happens in PTQ itself, on the command line.

#include "Ptq.h"

#include "Api.h"
#include "PanelsPrivate.h"
#include "Theme.h"
#include "../core/JsCompat.h"

#include <QAbstractButton>
#include <QClipboard>
#include <QDateTime>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <cmath>

namespace qtm::ptq {

using namespace qtm::kit;
using theme::esc;
using theme::fmtNum;
using theme::fmtPrice;

namespace {

// ---------------------------------------------------------------- brand colours and text

const QColor White("#f4f5f7");
const QColor Body("#c9ccd3");
const QColor Muted("#a4a9b4");
QColor rgba(int r, int g, int b, double a) { return QColor(r, g, b, qRound(a * 255)); }

QString enc(const QString &s)
{
    return QString::fromLatin1(QUrl::toPercentEncoding(s));
}

double fontPx(const QFont &f)
{
    return f.pixelSize() > 0 ? f.pixelSize() : f.pointSizeF();
}

double baselineIn(const QFont &f, double lineH)
{
    const QFontMetricsF fm(f);
    const double a = std::round(fm.ascent()), d = std::round(fm.descent());
    return (lineH - (a + d)) / 2 + a;
}

QFont spaced(QFont f, double em)
{
    f.setLetterSpacing(QFont::AbsoluteSpacing, fontPx(f) * em);
    return f;
}

Text *btext(const QString &html, const QFont &f, const QColor &c, bool wrap = true, double lh = 1.4)
{
    TextStyle st;
    st.font = f;
    st.color = c;
    st.wrap = wrap;
    st.lineHeight = lh;
    st.brand = true;
    auto *t = new Text(html, st);
    QObject::connect(t, &Text::linkActivated, t, [](const QString &href) { openUrl(href); });
    return t;
}

// <p> inside .ptq-brand (13px, #c9ccd3). Also <p class="down">: `.ptq-brand p` outranks `.down`.
Text *bpara(const QString &html, const QColor &c = Body)
{
    return btext(html, brandSans(13), c);
}

// .ptq-brand .hint-text (12px, #a4a9b4)
Text *bhint(const QString &html)
{
    return btext(html, brandSans(12), Muted);
}

// ---------------------------------------------------------------- the PTQ ground (radial glows)

// background: three radial-gradient ellipses (size, position, colour, transparent at 70%) on #0d0e12.
void paintGround(QPainter &p, const QRectF &r)
{
    p.fillRect(r, QColor("#0d0e12"));
    struct Glow { double rx, ry, px, py; QColor c; };
    const Glow glows[] = {
        {520, 320, -0.06, -0.20, rgba(255, 122, 217, 0.26)},
        {620, 380, 1.08, 0.20, rgba(94, 231, 255, 0.2)},
        {520, 300, 0.46, 1.30, rgba(255, 179, 107, 0.2)},
    };
    for (const Glow &g : glows) {
        const double cx = r.left() + r.width() * g.px, cy = r.top() + r.height() * g.py;
        QRadialGradient grad(QPointF(0, 0), 1);
        QColor clear = g.c;
        clear.setAlpha(0);   // "transparent" interpolates in premultiplied space: only alpha fades
        grad.setColorAt(0, g.c);
        grad.setColorAt(0.7, clear);
        grad.setColorAt(1, clear);
        p.save();
        p.translate(cx, cy);
        p.scale(g.rx, g.ry);
        p.fillRect(QRectF((r.left() - cx) / g.rx, (r.top() - cy) / g.ry, r.width() / g.rx, r.height() / g.ry), grad);
        p.restore();
    }
}

// ---------------------------------------------------------------- pill buttons

struct PillStyle {
    QColor bg, bgHover, border, borderHover, fg, fgHover;
    QFont font;
    int padX = 20;
    int height = 28;
    bool inset = false;   // box-shadow: inset 0 1px 0 rgba(255,255,255,.06)
};

PillStyle pillPtq(bool small = false)
{
    PillStyle s;
    s.bg = QColor("#0467cc");
    s.bgHover = QColor("#0a78e6");
    s.border = s.bg;
    s.borderHover = s.bgHover;
    s.fg = s.fgHover = Qt::white;
    s.font = brandSans(small ? 12 : 14, 600);
    s.padX = small ? 14 : 20;
    s.height = small ? 24 : 28;
    return s;
}

PillStyle pillOutline()
{
    PillStyle s;
    s.bg = rgba(58, 161, 255, 0.12);
    s.bgHover = rgba(58, 161, 255, 0.2);
    s.border = s.borderHover = rgba(58, 161, 255, 0.55);
    s.fg = QColor("#9ccfff");
    s.fgHover = Qt::white;
    s.font = brandSans(14, 600);
    s.inset = true;
    return s;
}

PillStyle pillGhostSmall()
{
    PillStyle s;
    s.bg = rgba(255, 255, 255, 0.06);
    s.bgHover = rgba(255, 255, 255, 0.1);
    s.border = s.borderHover = rgba(255, 255, 255, 0.14);
    s.fg = s.fgHover = White;
    s.font = brandSans(12, 500);
    s.padX = 14;
    s.height = 24;
    return s;
}

PillStyle pillDanger()
{
    PillStyle s;
    s.bg = rgba(255, 122, 122, 0.12);
    s.bgHover = rgba(255, 122, 122, 0.2);
    s.border = s.borderHover = rgba(255, 122, 122, 0.4);
    s.fg = s.fgHover = QColor("#ff7a7a");
    s.font = brandSans(13);
    s.padX = 12;
    s.height = 28;
    return s;
}

// .ptq-brand .btn.small (a plain button in the brand look)
PillStyle pillSmall()
{
    PillStyle s;
    s.bg = rgba(23, 24, 29, 0.72);
    s.bgHover = rgba(255, 255, 255, 0.06);
    s.border = s.borderHover = rgba(255, 255, 255, 0.1);
    s.fg = s.fgHover = Body;
    s.font = brandSans(12);
    s.padX = 14;
    s.height = 24;
    return s;
}

class Pill : public QAbstractButton
{
public:
    Pill(const QString &text, const PillStyle &s, QWidget *parent = nullptr) : QAbstractButton(parent), m_s(s)
    {
        setText(text);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::TabFocus);
        setAttribute(Qt::WA_Hover);
    }
    void setLabel(const QString &t)
    {
        setText(t);
        updateGeometry();
        update();
    }
    QSize sizeHint() const override
    {
        const QFontMetricsF fm(m_s.font);
        return {int(std::ceil(fm.horizontalAdvance(text()))) + 2 * m_s.padX + 2, m_s.height};
    }
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const bool hov = underMouse() && isEnabled();
        const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        QPainterPath path;
        path.addRoundedRect(r, r.height() / 2, r.height() / 2);
        p.fillPath(path, hov ? m_s.bgHover : m_s.bg);
        if (m_s.inset) {
            p.save();
            p.setClipPath(path);
            p.fillRect(QRectF(0, 1, width(), 1), rgba(255, 255, 255, 0.06));
            p.restore();
        }
        p.setPen(QPen(hov ? m_s.borderHover : m_s.border, 1));
        p.drawPath(path);
        p.setFont(m_s.font);
        p.setPen(hov ? m_s.fgHover : m_s.fg);
        const QFontMetricsF fm(m_s.font);
        const double tw = fm.horizontalAdvance(text());
        const double base = (height() - (std::round(fm.ascent()) + std::round(fm.descent()))) / 2 + std::round(fm.ascent());
        p.drawText(QPointF((width() - tw) / 2, base), text());
    }

private:
    PillStyle m_s;
};

// ---------------------------------------------------------------- the logo, status, title, verdict

// MARK: .ptq-logo with its 26px .ptq-logo-mark (the chart line icon) and "PTQ".
class Logo : public QWidget
{
public:
    Logo()
    {
        setFixedSize(sizeHint());
    }
    QSize sizeHint() const override
    {
        const QFontMetricsF fm(font15());
        return {26 + 8 + int(std::ceil(fm.horizontalAdvance("PTQ"))), 26};
    }

protected:
    static QFont font15() { return spaced(brandSans(15, 600), -0.01); }
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QPainterPath box;
        box.addRoundedRect(QRectF(0.5, 0.5, 25, 25), 8, 8);
        p.fillPath(box, QColor("#1c1d24"));
        p.setPen(QPen(rgba(255, 255, 255, 0.1), 1));
        p.drawPath(box);
        // <svg viewBox="0 0 16 16"> drawn at 18px (the CSS svg size), stroke 1.8
        p.save();
        p.translate(4, 4);
        p.scale(18.0 / 16, 18.0 / 16);
        QPen pen(White, 1.8);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        p.setPen(pen);
        p.drawPolyline(QPolygonF({QPointF(3, 12), QPointF(7, 6), QPointF(10, 9), QPointF(13, 4)}));
        p.restore();
        const QFont f = font15();
        p.setFont(f);
        p.setPen(White);
        p.drawText(QPointF(34, baselineIn(f, 26)), "PTQ");
    }
};

// <i> in .ptq-status: a 7px green dot with a glow.
class Dot : public QWidget
{
public:
    Dot() { setFixedSize(7, 7); }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#4cc38a"));
        p.drawEllipse(QRectF(0, 0, 7, 7));
    }
};

// .ptq-status: dot + Fragment Mono text.
QWidget *status(const QString &html, bool dot)
{
    auto *w = new QWidget;
    auto *l = new QHBoxLayout(w);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(7);
    if (dot) l->addWidget(new Dot, 0, Qt::AlignVCenter);
    // The text wraps when the row is too narrow for it (a flex item shrinks to its longest word),
    // so a long status ("... · no licence on this computer") doesn't widen the tab's left column.
    l->addWidget(btext(html, brandMono(11), Muted, true), 1);
    QSizePolicy sp(QSizePolicy::Preferred, QSizePolicy::Preferred);
    sp.setHeightForWidth(true);
    w->setSizePolicy(sp);
    return w;
}

// .ptq-top: MARK, eyebrow, optional status.
QWidget *ptqHead(const QString &statusHtml = QString())
{
    auto *row = rowFlex(nullptr, 12);
    flow(row)->setAlignItems(Qt::AlignVCenter);
    row->layout()->addWidget(new Logo);
    TextStyle st;
    st.font = spaced(brandMono(11), 0.12);
    st.color = Muted;
    st.upper = true;
    st.wrap = false;
    st.brand = true;
    row->layout()->addWidget(new Text("Part-Time Quant", st));
    if (!statusHtml.isEmpty()) row->layout()->addWidget(status(statusHtml, true));
    return row;
}

// <h3 class="ptq-title">Trade it <em>for real.</em></h3>: two fonts on one line.
class Title : public QWidget
{
public:
    Title()
    {
        QSizePolicy sp(QSizePolicy::Preferred, QSizePolicy::Fixed);
        setSizePolicy(sp);
        setFixedHeight(int(std::round(28 * 1.1)));
    }
    QSize sizeHint() const override
    {
        return {int(std::ceil(QFontMetricsF(sans()).horizontalAdvance("Trade it ")
                              + QFontMetricsF(serif()).horizontalAdvance("for real."))),
                int(std::round(28 * 1.1))};
    }

protected:
    static QFont sans() { return spaced(brandSans(28, 600), -0.03); }
    static QFont serif()
    {
        QFont f = spaced(brandSerif(28), -0.01);
        f.setWeight(QFont::Normal);
        return f;
    }
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::TextAntialiasing);
        const double lh = 28 * 1.1;
        const double base = baselineIn(sans(), lh);
        p.setFont(sans());
        p.setPen(White);
        p.drawText(QPointF(0, base), "Trade it ");
        const double x = QFontMetricsF(sans()).horizontalAdvance("Trade it ");
        p.setFont(serif());
        p.setPen(QColor("#9ccfff"));
        p.drawText(QPointF(x, base), "for real.");
    }
};

// <div class="ptq-verdict up|down|warn">: a pill, inline-block, margin 8px 0 (the caller's).
class Verdict : public QWidget
{
public:
    Verdict(const QString &label, const QString &cls) : m_label(label), m_cls(cls)
    {
        setFixedSize(sizeHint());
    }
    QSize sizeHint() const override
    {
        const QFontMetricsF fm(brandMono(12));
        return {int(std::ceil(fm.horizontalAdvance(m_label))) + 28 + 2, int(std::ceil(12 * 1.4 + 8 + 2))};
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QColor bg = rgba(255, 255, 255, 0.06), border = rgba(255, 255, 255, 0.12), fg = Body;
        if (m_cls == "up") { fg = QColor("#4cc38a"); bg = rgba(76, 195, 138, 0.14); border = rgba(76, 195, 138, 0.35); }
        if (m_cls == "down") { fg = QColor("#ff7a7a"); bg = rgba(255, 122, 122, 0.12); border = rgba(255, 122, 122, 0.4); }
        if (m_cls == "warn") { fg = QColor("#e8b949"); bg = rgba(232, 185, 73, 0.14); border = rgba(232, 185, 73, 0.4); }
        const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        QPainterPath path;
        path.addRoundedRect(r, r.height() / 2, r.height() / 2);
        p.fillPath(path, bg);
        p.setPen(QPen(border, 1));
        p.drawPath(path);
        const QFont f = brandMono(12);
        p.setFont(f);
        p.setPen(fg);
        p.drawText(QPointF(15, 5 + baselineIn(f, 12 * 1.4)), m_label);
    }

private:
    QString m_label, m_cls;
};

// ---------------------------------------------------------------- boxes

// .ptq-box.ptq-brand: the glowing card, radius 22, padding 18 24 16 (+ the 1px border).
class BrandBox : public Box
{
public:
    BrandBox()
    {
        radius = 22;
        border = rgba(255, 255, 255, 0.1);
        paintBackground = [this](QPainter &p, const QRectF &r) {
            paintGround(p, r);
            // inset 0 1px 0 rgba(255,255,255,.06)
            p.fillRect(QRectF(r.left(), r.top() + 1, r.width(), 1), rgba(255, 255, 255, 0.06));
            Q_UNUSED(this);
        };
    }
};

Box *brandBox()
{
    auto *b = new BrandBox;
    auto *l = new QVBoxLayout(b);
    l->setContentsMargins(25, 19, 25, 17);
    l->setSpacing(0);
    return b;
}

int hfw(QWidget *w, int width)
{
    const int h = w->hasHeightForWidth() ? w->heightForWidth(width) : -1;
    return h >= 0 ? h : w->sizeHint().height();
}

// .ptq-box.ptq-split: one column, or 1.1fr / 1fr with a 28px gap (vertically centred) when the
// box is 760px wide or more (the @container ptqhost rule).
class SplitBox : public BrandBox
{
public:
    SplitBox(QWidget *col, QWidget *steps) : m_col(col), m_steps(steps)
    {
        col->setParent(this);
        steps->setParent(this);
        QSizePolicy sp(QSizePolicy::Preferred, QSizePolicy::Preferred);
        sp.setHeightForWidth(true);
        setSizePolicy(sp);
    }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int w) const override
    {
        const int inner = w - 50;
        int h;
        if (w >= 760) {
            const auto [c1, c2] = widths(inner);
            h = std::max(hfw(m_col, c1), hfw(m_steps, c2));
        } else {
            h = hfw(m_col, inner) + 16 + hfw(m_steps, inner);
        }
        return h + 19 + 17;
    }
    QSize sizeHint() const override
    {
        const int w = width() > 0 ? width() : 800;
        return {w, heightForWidth(w)};
    }
    QSize minimumSizeHint() const override { return {0, heightForWidth(std::max(1, width()))}; }

protected:
    std::pair<int, int> widths(int inner) const
    {
        const int c1 = int(std::round((inner - 28) * 1.1 / 2.1));
        return {c1, inner - 28 - c1};
    }
    void resizeEvent(QResizeEvent *) override
    {
        const int inner = width() - 50;
        if (width() >= 760) {
            const auto [c1, c2] = widths(inner);
            const int h1 = hfw(m_col, c1), h2 = hfw(m_steps, c2);
            const int rowH = std::max(h1, h2);
            m_col->setGeometry(25, 19 + (rowH - h1) / 2, c1, h1);
            m_steps->setGeometry(25 + c1 + 28, 19 + (rowH - h2) / 2, c2, h2);
        } else {
            const int h1 = hfw(m_col, inner), h2 = hfw(m_steps, inner);
            m_col->setGeometry(25, 19, inner, h1);
            m_steps->setGeometry(25, 19 + h1 + 16, inner, h2);
        }
    }

private:
    QWidget *m_col, *m_steps;
};

// .ptq-steps li: number (Fragment Mono, baseline-aligned with the title) and the text.
QWidget *step(const QString &n, const QString &title, const QString &textHtml)
{
    auto *li = new Box;
    li->bg = rgba(23, 24, 29, 0.72);
    li->border = rgba(255, 255, 255, 0.1);
    li->radius = 16;
    auto *l = new QHBoxLayout(li);
    l->setContentsMargins(15, 10, 15, 10);
    l->setSpacing(12);
    const QFont numF = spaced(brandMono(11), 0.1);
    const QFont bF = brandSans(14, 600);
    auto *num = btext(n, numF, QColor("#6cb8ff"), false);
    // align-items: baseline
    const double off = baselineIn(bF, 14 * 1.4) - baselineIn(numF, 11 * 1.4);
    num->setContentsMargins(0, int(std::round(off)), 0, 0);
    l->addWidget(num, 0, Qt::AlignTop);
    auto *col = block();
    auto *b = btext(esc(title), bF, White);
    vbox(col)->addWidget(b);
    vbox(col)->addSpacing(3);
    vbox(col)->addWidget(btext(textHtml, brandSans(12.5), Body, true, 1.45));
    l->addWidget(col, 1);
    return li;
}

// .ptq-action: a button and a Fragment Mono note under it.
QWidget *action(QWidget *button, const QString &note)
{
    auto *w = new QWidget;
    auto *l = new QVBoxLayout(w);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(7);
    l->addWidget(button, 0, Qt::AlignLeft);
    auto *t = btext(note, spaced(brandMono(10.5), 0.04), Muted, false);
    t->setContentsMargins(4, 0, 0, 0);
    l->addWidget(t, 0, Qt::AlignLeft);
    return w;
}

// .ptq-actions: flex, wrap, gap 12px 22px, margin 0 0 12px.
QWidget *actions()
{
    auto *row = new QWidget;
    auto *fl = new FlowLayout(row, 22, 12);
    fl->setAlignItems(Qt::AlignTop);
    QSizePolicy sp(QSizePolicy::Preferred, QSizePolicy::Preferred);
    sp.setHeightForWidth(true);
    row->setSizePolicy(sp);
    return row;
}

// notConnected(st, tried)
QWidget *notConnected(const QJsonObject &st, bool tried, std::function<void()> retry)
{
    auto *col = block();
    vbox(col)->addWidget(ptqHead());
    vbox(col)->addSpacing(12);
    vbox(col)->addWidget(new Title);
    vbox(col)->addSpacing(8);
    Text *lead = btext("The quant checks run on your computer. PTQ takes the same rules further and trades the ones that "
                       "pass through your own exchange or broker account.",
                       brandSans(14), Body, true, 1.55);
    lead->setMaximumWidth(640);
    vbox(col)->addWidget(lead);
    vbox(col)->addSpacing(6);
    auto *acts = actions();
    auto *trade = new Pill(QString("Trade on PTQ ") + QChar(0x2197), pillPtq());
    const QString site = st.value("site").toString();
    QObject::connect(trade, &QAbstractButton::clicked, trade, [site] { openUrl(site); });
    acts->layout()->addWidget(action(trade, "New to PTQ? Start here."));
    auto *retryBtn = new Pill("I already have PTQ", pillOutline());
    QObject::connect(retryBtn, &QAbstractButton::clicked, retryBtn, [retry] { retry(); });
    acts->layout()->addWidget(action(retryBtn, tried ? "Not found. Open PTQ on this computer, then try again."
                                                     : "Connect your PTQ account to this tool."));
    vbox(col)->addWidget(acts);
    vbox(col)->addSpacing(12);

    auto *steps = new QWidget;
    auto *sl = new QVBoxLayout(steps);
    sl->setContentsMargins(0, 0, 0, 0);
    sl->setSpacing(7);
    sl->addWidget(step("01", "Test it here", "Backtest and quant checks in this app."));
    sl->addWidget(step("02", "PTQ re-tests it", "On its own engine and price data, to see if the result was luck."));
    sl->addWidget(step("03", "You arm it", "A strategy that passes trades through your own account, with limits you set."));
    return new SplitBox(col, steps);
}

// ---------------------------------------------------------------- status cache

QJsonObject g_cached;
qint64 g_cachedAt = 0;

QJsonValue coalesce(const QJsonObject &o, const QStringList &keys)
{
    for (const QString &k : keys) {
        const QJsonValue v = o.value(k);
        if (!v.isNull() && !v.isUndefined()) return v;
    }
    return QJsonValue::Undefined;
}

double numOr(const QJsonValue &v)
{
    return v.isDouble() ? v.toDouble() : NaN;
}

QString jsText(const QJsonValue &v)
{
    if (v.isDouble()) return js::numberToString(v.toDouble());
    if (v.isBool()) return v.toBool() ? "true" : "false";
    if (v.isNull()) return "null";
    if (v.isUndefined()) return "undefined";
    return v.toVariant().toString();
}

// esc(v): null and undefined are ""
QString escv(const QJsonValue &v)
{
    return v.isNull() || v.isUndefined() ? QString() : esc(jsText(v));
}

// `${val ?? "—"}`
QString orDash(const QJsonValue &v)
{
    return v.isNull() || v.isUndefined() ? QString::fromUtf8("—") : jsText(v);
}

// when(t): new Date(t).toLocaleString() or "—"
QString when(const QJsonValue &t)
{
    if (t.isNull() || t.isUndefined() || (t.isString() && t.toString().isEmpty()) || (t.isDouble() && t.toDouble() == 0)
        || (t.isBool() && !t.toBool()))
        return QString::fromUtf8("—");
    double ms;
    if (t.isDouble()) ms = t.toDouble();
    else {
        const QDateTime d = QDateTime::fromString(t.toString(), Qt::ISODateWithMs);
        if (!d.isValid()) return "Invalid Date";
        ms = double(d.toMSecsSinceEpoch());
    }
    return localeString(ms);
}

// resultHtml(r): the PTQ check's result, as widgets added to `out`.
void result(QVBoxLayout *out, const QJsonObject &r)
{
    const QJsonValue vv = r.value("validation");
    const QJsonObject v = vv.toObject();
    const bool hasV = vv.isObject();
    const QString verdict = v.value("verdict").toString();
    QString cls, label = "No verdict";
    if (verdict == "approve") { cls = "up"; label = "Passed PTQ's checks"; }
    else if (verdict == "reduce") { cls = "warn"; label = "Passed with warnings"; }
    else if (verdict == "block") { cls = "down"; label = "Didn't pass PTQ's checks"; }
    const QJsonObject m = r.value("metrics").toObject();
    auto pct = [](const QJsonValue &x) {
        if (x.isNull() || x.isUndefined()) return QString::fromUtf8("—");
        return QString("%1%2%").arg(x.toDouble() >= 0 ? "+" : "", jsText(x));
    };
    // Vertical margins as the browser lays them out. `out` follows .ptq-actions (margin-bottom 12,
    // already added): a first block's margin collapses with it. The verdict is an inline-block, whose
    // margins never collapse. `pending` is the bottom margin of the previous element, not added yet.
    bool atTop = true, prevCollapses = true;
    int pending = 0;
    auto space = [&](int marginTop, bool collapses) {
        if (atTop) out->addSpacing(collapses ? std::max(0, marginTop - 12) : marginTop);
        else out->addSpacing(collapses && prevCollapses ? std::max(pending, marginTop) : pending + marginTop);
        atTop = false;
        prevCollapses = collapses;
    };
    auto addP = [&](QWidget *w, int margin) {
        space(margin, true);
        out->addWidget(w);
        pending = margin;
    };
    if (r.value("provenance").toString() != "real-engine")
        addP(bpara("PTQ's engine wasn't running, so PTQ returned a simulated preview. This is not a real test: start the "
                   "engine in PTQ and check again."),
             6);
    space(8, false);
    out->addWidget(new Verdict(label, cls), 0, Qt::AlignLeft);
    pending = 8;
    const QJsonArray gates = v.value("gates").toArray();
    if (!gates.isEmpty()) {
        auto *t = new Table;
        t->setSticky(false);
        t->setBrand(true);
        t->setWrap(true);
        QVector<Row> rows;
        for (const auto &gv : gates) {
            const QJsonObject g = gv.toObject();
            Row row;
            row.cells << Cell {escv(g.value("label")), "l"} << Cell {escv(g.value("value")), "l muted"}
                      << Cell {g.value("pass").toBool() ? "Pass" : "Fail", g.value("pass").toBool() ? "up" : "down"};
            rows << row;
        }
        t->setRows(rows);
        space(0, true);
        out->addWidget(t);
        pending = 0;
    } else if (!r.value("validationError").toString().isEmpty()) {
        addP(bpara(esc(r.value("validationError").toString())), 6);
    }
    auto *stats = new Stats;
    stats->setBrand(true);
    stats->setMinColumn(110);
    stats->setItems({{"PTQ net return", pct(m.value("netReturnPct")), ""},
                     {"PTQ Sharpe", orDash(m.value("sharpe")), ""},
                     {"PTQ max drawdown",
                      m.value("maxDrawdownPct").isNull() || m.value("maxDrawdownPct").isUndefined()
                          ? QString::fromUtf8("—")
                          : "-" + jsText(m.value("maxDrawdownPct")) + "%",
                      ""},
                     {"PTQ trades", orDash(m.value("trades")), ""}});
    space(8, true);   // style="margin-top:8px"
    out->addWidget(stats);
    pending = 0;
    addP(bhint(QString("PTQ tested %1 on %2 with its own costs and fills, so its numbers won't match this tester's exactly.")
                   .arg(escv(r.value("instrument")), escv(r.value("timeframe")))),
         6);
    for (const auto &n : r.value("notes").toArray()) addP(bhint(escv(n)), 6);
    const QJsonObject links = r.value("links").toObject();
    addP(bpara(QString("Saved in your PTQ as <b>%1</b>. <a class=\"link\" href=\"%2\">Open it in PTQ</a>")
                   .arg(escv(r.value("strategyName")), esc(links.value("strategy").toString()))),
         6);
    if (verdict == "block") {
        addP(bhint("PTQ wouldn't trade this as it stands. Change the rules and check again."), 6);
    } else if (hasV) {
        const QString cmd = QString("ptq arm-strategy %1 --max-notional 100").arg(jsText(r.value("strategyId")));
        // .ptq-brand .ptq-arm
        auto *arm = new Box;
        arm->bg = rgba(58, 161, 255, 0.08);
        arm->border = rgba(58, 161, 255, 0.45);
        arm->radius = 16;
        auto *al = new QVBoxLayout(arm);
        al->setContentsMargins(17, 15, 17, 15);
        al->setSpacing(0);
        al->addWidget(btext("<b>Let PTQ trade it on your exchange</b>", brandSans(13), Body));
        al->addSpacing(6);
        auto li = [&](int n, QWidget *content) {
            auto *row = new QWidget;
            auto *rl = new QHBoxLayout(row);
            rl->setContentsMargins(0, 0, 0, 0);
            rl->setSpacing(0);
            auto *num = btext(QString("%1.").arg(n), brandSans(13), Body, false);
            num->setFixedWidth(18);
            num->setStyle([&] { TextStyle s = num->textStyle(); s.align = Qt::AlignRight; return s; }());
            num->setContentsMargins(0, 0, 4, 0);
            num->setFixedWidth(18);
            rl->addWidget(num, 0, Qt::AlignTop);
            rl->addWidget(content, 1);
            return row;
        };
        al->addWidget(li(1, bpara(QString("Connect your exchange in PTQ: <a class=\"link\" href=\"%1\">Integrations</a>. "
                                          "Your keys stay in PTQ.")
                                      .arg(esc(links.value("integrations").toString())))));
        al->addSpacing(4);
        {
            auto *c = block();
            auto *row = rowFlex(nullptr, 4);
            // the 24px Copy button sits on the text's baseline, which makes this line 25px tall
            row->setMinimumHeight(25);
            row->layout()->addWidget(bpara("In a terminal, run <code>" + esc(cmd) + "</code>"));
            auto *copy = new Pill("Copy", pillSmall());
            row->layout()->addWidget(copy);
            QPointer<Pill> copyP(copy);
            QObject::connect(copy, &QAbstractButton::clicked, copy, [copyP, cmd] {
                QGuiApplication::clipboard()->setText(cmd);
                if (copyP) copyP->setLabel("Copied");
            });
            vbox(c)->addWidget(row);
            vbox(c)->addWidget(bhint("PTQ shows what you're agreeing to and asks you to add <code>--yes</code>. It asks you to "
                                     "confirm each trade unless you also add <code>--unattended</code>. "
                                     "<code>--max-notional</code> caps each order's size."));
            al->addWidget(li(2, c));
        }
        al->addSpacing(4);
        al->addWidget(li(3, bpara(QString("Watch it in the PTQ tab below, or on PTQ's <a class=\"link\" href=\"%1\">Autopilot</a> page.")
                                      .arg(esc(links.value("autopilot").toString())))));
        al->addSpacing(6);   // ol margin-bottom 6, li 4 and p 6 collapse
        al->addWidget(bhint("Live trading in PTQ is new: start small. Past results don't predict future ones."));
        al->addSpacing(6);
        space(14, true);
        out->addWidget(arm);
        pending = 0;
    }
    out->addSpacing(pending);
}

// errLine(x)
void errLine(QVBoxLayout *l, const QJsonValue &x)
{
    const QString e = x.isObject() ? x.toObject().value("error").toString() : QString();
    if (e.isEmpty()) return;
    l->addSpacing(13);
    l->addWidget(bpara(esc(e)));
    l->addSpacing(13);
}

QPointer<QTimer> g_tabTimer;

} // namespace

// ================================================================ status

void ptqStatus(QObject *context, bool force, std::function<void(const QJsonObject &)> done)
{
    if (!force && !g_cached.isEmpty() && QDateTime::currentMSecsSinceEpoch() - g_cachedAt < 15000) {
        const QJsonObject c = g_cached;
        QMetaObject::invokeMethod(context, [done, c] { done(c); }, Qt::QueuedConnection);
        return;
    }
    api::get(
        context, "/api/ptq/status",
        [done](const QJsonValue &v) {
            g_cached = v.toObject();
            g_cachedAt = QDateTime::currentMSecsSinceEpoch();
            done(g_cached);
        },
        [done](const QString &msg) {
            g_cached = QJsonObject {{"connected", false}, {"reason", msg}};
            g_cachedAt = QDateTime::currentMSecsSinceEpoch();
            done(g_cached);
        });
}

// ================================================================ Check with PTQ

void mountPtqCheck(QWidget *el, App *app, const QJsonObject &ctx, bool tried)
{
    QPointer<QWidget> elP(el);
    ptqStatus(el, false, [elP, app, ctx, tried](const QJsonObject &st) {
        if (!elP) return;
        auto *l = static_cast<QVBoxLayout *>(elP->layout());
        clear(l);
        if (!st.value("connected").toBool()) {
            l->addWidget(notConnected(st, tried, [elP, app, ctx] {
                if (!elP) return;
                ptqStatus(elP, true, [elP, app, ctx](const QJsonObject &) {
                    if (elP) mountPtqCheck(elP, app, ctx, true);
                });
            }));
            return;
        }
        Box *box = brandBox();
        auto *bl = static_cast<QVBoxLayout *>(box->layout());
        bl->addWidget(ptqHead(QString("PTQ %1 connected%2")
                                  .arg(esc(st.value("version").toString()),
                                       st.value("licensed").toBool() ? "" : " · no licence found on this computer")));
        bl->addSpacing(12);
        bl->addWidget(new Title);
        bl->addSpacing(8);
        Text *lead = btext(QString("Send these rules to your PTQ: it re-tests them on %1 %2 with its own engine, and a "
                                   "strategy that passes can trade through your own account once you arm it.")
                               .arg(esc(theme::ticker(ctx.value("symbol").toString())), esc(ctx.value("tf").toString())),
                           brandSans(14), Body, true, 1.55);
        lead->setMaximumWidth(640);
        bl->addWidget(lead);
        bl->addSpacing(6);
        auto *acts = actions();
        auto *check = new Pill("Check with PTQ", pillPtq());
        acts->layout()->addWidget(check);
        bl->addWidget(acts);
        bl->addSpacing(12);
        auto *out = new QWidget;
        auto *ol = new QVBoxLayout(out);
        ol->setContentsMargins(0, 0, 0, 0);
        ol->setSpacing(0);
        bl->addWidget(out);
        l->addWidget(box);
        QPointer<Pill> checkP(check);
        QPointer<QWidget> outP(out);
        QObject::connect(check, &QAbstractButton::clicked, check, [checkP, outP, ctx] {
            if (!checkP || !outP) return;
            checkP->setEnabled(false);
            checkP->setLabel(QString::fromUtf8("PTQ is testing…"));
            auto *ol = static_cast<QVBoxLayout *>(outP->layout());
            clear(ol);
            // the <p>'s top margin (6) collapses with .ptq-actions' margin-bottom (12)
            ol->addWidget(bhint("This can take up to a minute the first time PTQ loads the price history."));
            ol->addSpacing(6);
            api::post(
                outP, "/api/ptq/check", ctx,
                [checkP, outP](const QJsonValue &v) {
                    if (!outP) return;
                    auto *ol = static_cast<QVBoxLayout *>(outP->layout());
                    clear(ol);
                    result(ol, v.toObject());
                    if (checkP) {
                        checkP->setEnabled(true);
                        checkP->setLabel("Check again");
                    }
                },
                [checkP, outP](const QString &msg) {
                    if (!outP) return;
                    auto *ol = static_cast<QVBoxLayout *>(outP->layout());
                    clear(ol);
                    ol->addWidget(bpara(esc(msg)));
                    ol->addSpacing(6);
                    if (checkP) {
                        checkP->setEnabled(true);
                        checkP->setLabel("Check again");
                    }
                });
        });
    });
}

// ================================================================ the PTQ tab

void renderPtqTab(panels::Body *el, App *app, bool tried)
{
    if (g_tabTimer) g_tabTimer->stop();
    QPointer<panels::Body> elP(el);
    // the status is fetched before anything is drawn (as the JS awaits it)
    QWidget *ctxObj = el->content() ? el->content() : static_cast<QWidget *>(el);
    ptqStatus(ctxObj, true, [elP, app, tried](const QJsonObject &st) {
        if (!elP) return;
        if (!st.value("connected").toBool()) {
            // <div class="pad ptq-tab">: #0d0e12, padding 12 18, the box centred (max-width 1040)
            class Ground : public QWidget
            {
            protected:
                void paintEvent(QPaintEvent *) override
                {
                    QPainter p(this);
                    p.fillRect(rect(), QColor("#0d0e12"));
                }
            };
            auto *ground = new Ground;
            auto *gl = new QVBoxLayout(ground);
            gl->setContentsMargins(0, 0, 0, 0);
            auto *sc = new ScrollCol(QMargins(18, 12, 18, 12));
            gl->addWidget(sc);
            auto *row = new QHBoxLayout;
            row->setContentsMargins(0, 0, 0, 0);
            QWidget *box = notConnected(st, tried, [elP, app] {
                if (elP) renderPtqTab(elP, app, true);
            });
            box->setMaximumWidth(1040);
            row->addStretch(1);   // justify-content: center
            row->addWidget(box, 1000);
            row->addStretch(1);
            sc->layout()->addLayout(row);
            sc->layout()->addStretch(1);
            elP->setContent(ground, true);
            return;
        }
        api::get(
            elP->content() ? elP->content() : static_cast<QWidget *>(elP.data()), "/api/ptq/overview",
            [elP, app, st](const QJsonValue &ov) {
                if (!elP) return;
                const QJsonObject o = ov.toObject();
                const QJsonObject ks = o.value("killSwitch").toObject(), pf = o.value("portfolio").toObject(),
                                  autoP = o.value("autopilot").toObject();
                const QJsonArray arms = o.value("arms").isArray() ? o.value("arms").toArray() : QJsonArray();
                const QJsonArray journal = o.value("journal").isArray() ? o.value("journal").toArray() : QJsonArray();
                const QJsonObject links = o.value("links").toObject();

                auto *split = new Split(nullptr, true);
                split->paintBackground = [](QPainter &p, const QRectF &r) { paintGround(p, r); };
                // ---- left
                auto *L = split->left->layout();
                auto *top = rowFlex();
                top->layout()->addWidget(new Logo);
                top->layout()->addWidget(status(QString("%1 connected · %2%3")
                                                    .arg(esc(st.value("version").toString()), esc(st.value("url").toString()),
                                                         st.value("licensed").toBool() ? "" : " · no licence on this computer"),
                                                true));
                top->layout()->addWidget(spacer());
                auto *refresh = new Pill("Refresh", pillGhostSmall());
                top->layout()->addWidget(refresh);
                auto *open = new Pill("Open PTQ", pillPtq(true));
                const QString appUrl = links.value("app").toString();
                QObject::connect(open, &QAbstractButton::clicked, open, [appUrl] { openUrl(appUrl); });
                top->layout()->addWidget(open);
                L->addWidget(top);
                L->addSpacing(16);
                L->addWidget(h4("Trading", true));
                QPointer<QWidget> killBtn;
                if (ks.value("engaged").toBool()) {
                    L->addSpacing(13);
                    L->addWidget(bpara("<b>Stopped.</b> PTQ's stop is on, so it places no new orders. Release it in PTQ "
                                       "when you're ready."));
                    L->addSpacing(13);
                } else {
                    L->addSpacing(13);
                    QString s = autoP.value("running").toBool() ? "PTQ's autopilot is running" : "PTQ's autopilot is paused";
                    if (autoP.value("unattendedLiveEnabled").isBool() && !autoP.value("unattendedLiveEnabled").toBool())
                        s += " (unattended live is switched off)";
                    L->addWidget(bpara(s + "."));
                    L->addSpacing(13);
                    auto *kill = new Pill("Stop all PTQ trading", pillDanger());
                    L->addWidget(kill, 0, Qt::AlignLeft);
                    killBtn = kill;
                    L->addSpacing(12);
                    L->addWidget(bhint("Stops new orders straight away. Open positions stay as they are; close them in "
                                       "PTQ or on your exchange."));
                    L->addSpacing(12);
                }
                errLine(L, o.value("killSwitch"));
                L->addSpacing(4);   // h4 margin-top 16 collapses with the 12-13px above
                L->addWidget(h4("Account", true));
                L->addSpacing(8);
                if (pf.value("connected").toBool()) {
                    auto *stats = new Stats;
                    stats->setBrand(true);
                    const QJsonValue gross = pf.value("grossExposurePct"), day = pf.value("dayPnlUsd");
                    stats->setItems({{"Equity", fmtPrice(numOr(pf.value("equity"))), ""},
                                     {"Cash", fmtPrice(numOr(pf.value("cash"))), ""},
                                     {"Today", day.isNull() || day.isUndefined() ? QString::fromUtf8("—") : fmtPrice(numOr(day)), ""},
                                     {"Exposure",
                                      gross.isNull() || gross.isUndefined() ? QString::fromUtf8("—") : jsText(gross) + "%", ""}});
                    L->addWidget(stats);
                    L->addSpacing(12);
                    const QString src = pf.value("source").toString();
                    L->addWidget(bhint(QString("From %1.").arg(esc(src.isEmpty() ? QString("your broker") : src))));
                    L->addSpacing(12);
                } else {
                    L->addSpacing(4);
                    const QString msg = pf.value("message").toString();
                    L->addWidget(bhint(QString("%1 <a class=\"link\" href=\"%2\">Connect one in PTQ</a>")
                                           .arg(esc(msg.isEmpty() ? QString("No exchange connected in PTQ yet.") : msg),
                                                esc(links.value("integrations").toString()))));
                    L->addSpacing(12);
                }
                errLine(L, o.value("portfolio"));
                L->addStretch(1);

                // ---- right (flex column, gap 10)
                auto *R = split->right->layout();
                auto section = [&](const QString &title, bool first) {
                    if (!first) R->addSpacing(10 + 16);
                    R->addWidget(h4(title, true));
                    R->addSpacing(8 + 10);
                };
                auto table = [](const QStringList &head) {
                    auto *t = new Table;
                    t->setBrand(true);
                    t->setWrap(true);
                    QVector<Cell> h;
                    for (const QString &s : head) h << Cell {s};
                    t->setHeader(h);
                    return t;
                };
                section("Positions", true);
                {
                    auto *t = table({"Market", "Qty", "Entry", "Value", "P&amp;L"});
                    QVector<Row> rows;
                    for (const auto &pv : pf.value("positions").toArray()) {
                        const QJsonObject p = pv.toObject();
                        const QJsonValue inst = coalesce(p, {"instrument", "symbol"});
                        const QString name = p.value("instrument").toString().isEmpty()
                                                 ? (p.value("symbol").toString())
                                                 : p.value("instrument").toString();
                        Q_UNUSED(inst);
                        const QJsonValue pnl = coalesce(p, {"unrealizedPnl", "pnl"});
                        const double pnlV = pnl.isUndefined() ? 0 : pnl.toDouble();
                        Row row;
                        row.cells << Cell {"<b>" + esc(name) + "</b>"} << Cell {fmtNum(numOr(p.value("qty")), 4)}
                                  << Cell {fmtPrice(numOr(coalesce(p, {"avgPrice", "entry", "avgEntryPrice"})))}
                                  << Cell {fmtPrice(numOr(coalesce(p, {"marketValue", "value"})))}
                                  << Cell {fmtPrice(numOr(pnl)), pnlV >= 0 ? "up" : "down"};
                        rows << row;
                    }
                    if (rows.isEmpty()) {
                        Cell c {"No open positions.", "l muted"};
                        c.colspan = 5;
                        rows << Row {{c}, {}};
                    }
                    t->setRows(rows);
                    R->addWidget(t);
                }
                section("Armed strategies", false);
                {
                    auto *t = table({"Strategy", "Markets", "Max per order", "Confirm trades", "Expires", ""});
                    QVector<Row> rows;
                    for (const auto &av : arms) {
                        const QJsonObject a = av.toObject();
                        const QJsonObject scope = a.value("scope").toObject();
                        QString name = a.value("strategyName").toString();
                        if (name.isEmpty() && !a.value("strategyId").isNull() && !a.value("strategyId").isUndefined())
                            name = jsText(a.value("strategyId"));
                        QString html = "<b>" + esc(name) + "</b>";
                        if (a.value("halted").toBool()) {
                            const QString reason = a.value("haltedReason").toString();
                            html += " <span class=\"down\">halted" + (reason.isEmpty() ? QString() : ": " + esc(reason)) + "</span>";
                        }
                        QStringList syms;
                        for (const auto &s : scope.value("symbols").toArray()) syms << (s.isNull() ? QString() : jsText(s));   // join() writes null as ""
                        Row row;
                        row.cells << Cell {html} << Cell {esc(syms.join(", "))}
                                  << Cell {fmtNum(numOr(scope.value("maxNotional")), 2)}
                                  << Cell {a.value("requirePerTradeConfirm").toBool() ? "Yes" : "No (unattended)"}
                                  << Cell {when(scope.value("expiresAt"))};
                        if (a.value("armed").toBool()) {
                            Cell b;
                            b.button = "Disarm";
                            b.buttonCls = "danger";
                            b.data = jsText(a.value("strategyId"));
                            row.cells << b;
                        } else {
                            row.cells << Cell {"<span class=\"muted\">disarmed</span>"};
                        }
                        rows << row;
                    }
                    if (rows.isEmpty()) {
                        Cell c {"Nothing armed. Run \"Check with PTQ\" in the Strategy tester tab to send a strategy over.",
                                "l muted"};
                        c.colspan = 6;
                        rows << Row {{c}, {}};
                    }
                    t->setRows(rows);
                    R->addWidget(t);
                    QObject::connect(t, &Table::buttonClicked, t, [elP, app](int, int, const QString &id) {
                        if (!kit::confirm(app, "Disarm this strategy? PTQ stops trading it.")) return;
                        auto after = [elP, app] {
                            if (elP) renderPtqTab(elP, app);
                        };
                        api::post(
                            app, "/api/ptq/arms/" + enc(id) + "/disarm", QJsonObject(),
                            [app, after](const QJsonValue &) {
                                app->toast("Disarmed.");
                                after();
                            },
                            [app, after](const QString &msg) {
                                app->toast(msg, "err");
                                after();
                            });
                    });
                }
                if (o.value("arms").isObject() && !o.value("arms").toObject().value("error").toString().isEmpty()) {
                    R->addSpacing(10 + 13);
                    R->addWidget(bpara(esc(o.value("arms").toObject().value("error").toString())));
                    R->addSpacing(13 - 10);
                }
                section("Recent PTQ orders", false);
                {
                    auto *t = table({"Time", "Market", "Side", "Qty", "Price", "Status"});
                    QVector<Row> rows;
                    int n = 0;
                    for (const auto &jv : journal) {
                        if (n++ >= 25) break;
                        const QJsonObject j = jv.toObject();
                        QString stText = esc(j.value("status").toString());
                        if (j.value("simulated").toBool()) stText += " (simulated)";
                        else if (j.value("live").toBool()) stText += " (live)";
                        Row row;
                        row.cells << Cell {when(j.value("ts"))} << Cell {esc(j.value("instrument").toString())}
                                  << Cell {esc(j.value("direction").toString())}
                                  << Cell {fmtNum(numOr(coalesce(j, {"filledQty", "qty"})), 4)}
                                  << Cell {fmtPrice(numOr(coalesce(j, {"fillPrice", "limitPrice"})))}
                                  << Cell {stText, j.value("status").toString() == "rejected" ? "down" : ""};
                        rows << row;
                    }
                    if (rows.isEmpty()) {
                        Cell c {"No orders yet.", "l muted"};
                        c.colspan = 6;
                        rows << Row {{c}, {}};
                    }
                    t->setRows(rows);
                    R->addWidget(t);
                }
                R->addStretch(1);
                elP->setContent(split, true);

                QObject::connect(refresh, &QAbstractButton::clicked, refresh, [elP, app] {
                    if (elP) renderPtqTab(elP, app);
                });
                if (killBtn) {
                    QObject::connect(static_cast<Pill *>(killBtn.data()), &QAbstractButton::clicked, killBtn, [elP, app] {
                        if (!kit::confirm(app, "Stop all PTQ trading now? PTQ places no new orders until you release "
                                               "the stop in PTQ."))
                            return;
                        auto after = [elP, app] {
                            if (elP) renderPtqTab(elP, app);
                        };
                        api::post(
                            app, "/api/ptq/kill-switch", QJsonObject(),
                            [app, after](const QJsonValue &) {
                                app->toast("PTQ stopped. No new orders.");
                                after();
                            },
                            [app, after](const QString &msg) {
                                app->toast(msg, "err");
                                after();
                            });
                    });
                }
                // refresh every 15 s while this tab's content is shown
                auto *timer = new QTimer(split);
                timer->setSingleShot(true);
                QPointer<Split> splitP(split);
                QObject::connect(timer, &QTimer::timeout, split, [elP, app, splitP] {
                    if (elP && splitP && elP->content() == splitP) renderPtqTab(elP, app);
                });
                timer->start(15000);
                g_tabTimer = timer;
            },
            [elP](const QString &msg) {
                if (!elP) return;
                auto *t = btext(esc(msg), uiFont(13), theme::color("--down"));
                t->setContentsMargins(12, 10, 12, 10);
                elP->setContent(t);
            });
    });
}

} // namespace qtm::ptq
