#include "Legend.h"

#include "ChartPane.h"
#include "Text.h"
#include "Theme.h"
#include "Widgets.h"

#include <QFontMetricsF>
#include <QMouseEvent>
#include <QPainter>
#include <QRegion>

#include <cmath>

namespace qtm {

namespace {

const char *const IconGear = R"(<circle cx="12" cy="12" r="3"/><path d="M12 2v3M12 19v3M4.2 4.2l2.1 2.1M17.7 17.7l2.1 2.1M2 12h3M19 12h3M4.2 19.8l2.1-2.1M17.7 6.3l2.1-2.1"/>)";
const char *const IconEye = R"(<path d="M2 12s4-7 10-7 10 7 10 7-4 7-10 7S2 12 2 12z"/><circle cx="12" cy="12" r="3"/>)";
const char *const IconX = R"(<path d="M6 6l12 12M18 6L6 18"/>)";

QColor resolve(const QString &v)
{
    return v.startsWith("--") ? theme::chartColor(v) : theme::parseColor(v);
}

QFont f12() { return theme::font(12); }
QFont f13b() { return theme::font(13, 600); }
QFont f11() { return theme::font(11); }

double tw(const QFont &f, const QString &s) { return text::width(f, s); }

// Baseline of a text run centred in a line box of `h` px starting at `top` (CSS half-leading).
double baseline(const QFont &f, double top, double h = 20)
{
    const QFontMetricsF fm(f);
    return top + (h - (fm.ascent() + fm.descent())) / 2 + fm.ascent();
}

// Greedy word wrap (white-space: normal).
QStringList wrap(const QFont &f, const QString &text, double width)
{
    QStringList lines;
    const QStringList words = text.split(' ', Qt::SkipEmptyParts);
    QString cur;
    for (const QString &w : words) {
        const QString next = cur.isEmpty() ? w : cur + ' ' + w;
        if (!cur.isEmpty() && tw(f, next) > width) {
            lines << cur;
            cur = w;
        } else {
            cur = next;
        }
    }
    if (!cur.isEmpty() || lines.isEmpty()) lines << cur;
    return lines;
}

double valsWidth(const QVector<LegendModel::Val> &vals)
{
    double w = 0;
    for (int k = 0; k < vals.size(); ++k) w += tw(f12(), vals[k].text) + (k ? tw(f12(), " ") : 0);
    return w;
}

double ohlcWidth(const LegendModel &m)
{
    const QFont f = f12();
    double w = 0;
    for (const auto &[lab, v] : {std::pair{QString("O"), m.o}, {QString("H"), m.h}, {QString("L"), m.l}, {QString("C"), m.c}})
        w += tw(f, lab) + 2 + tw(f, v) + 6;
    w += tw(f, m.chg) + 6;
    return w;
}

} // namespace

Legend::Legend(ChartPane *pane) : QWidget(pane), m_pane(pane)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setObjectName("qtmLegend");
}

void Legend::setModel(const LegendModel &m)
{
    model = m;
    relayout();
}

void Legend::relayout()
{
    rows.clear();
    const QFont f = f12();
    const double space = 8;
    // Natural widths first (the legend shrinks to its widest row, capped at the pane width - 90).
    QVector<double> natural;
    Row main;
    main.kind = Row::Main;
    const double titleW = tw(f13b(), model.name);
    main.title = QRectF(0, 0, titleW, 20);
    main.width = titleW + (model.hasBar ? space + ohlcWidth(model) : 0);
    rows << main;
    for (int k = 0; k < model.compare.size(); ++k) {
        Row r;
        r.kind = Row::Compare;
        r.key = "c:" + model.compare[k].sym;
        r.index = k;
        r.width = 2 + tw(f, model.compare[k].label) + space + 18 + 4;
        rows << r;
    }
    for (int k = 0; k < model.inds.size(); ++k) {
        Row r;
        r.kind = Row::Ind;
        r.key = "i:" + model.inds[k].uid;
        r.index = k;
        r.width = 2 + tw(f, model.inds[k].name.trimmed()) + space + valsWidth(model.inds[k].vals)
            + space + (model.inds[k].volume ? 38 : 58) + 4;
        rows << r;
    }
    for (int k = 0; k < model.notes.size(); ++k) {
        Row r;
        r.kind = Row::Note;
        r.index = k;
        r.width = std::min(tw(f11(), model.notes[k]), 420.0);
        rows << r;
    }
    double w = 0;
    for (const Row &r : rows) w = std::max(w, r.width);
    const double maxW = std::max(0.0, m_pane->width() - 90.0);
    legendWidth = std::ceil(std::min(w, maxW));
    double y = 0;
    for (Row &r : rows) {
        r.y = y;
        if (r.kind == Row::Note) {
            r.width = std::min(legendWidth, 420.0);
            r.lines = wrap(f11(), model.notes[r.index], r.width + 0.5);
            r.h = 20 * r.lines.size();
        } else {
            r.h = 20;
            if (r.kind != Row::Main) r.width = legendWidth;
        }
        // Always there: a value too long for the pane goes under them rather than pushing them out of reach.
        if (r.kind == Row::Compare) {
            const double x = std::min(2 + tw(f, model.compare[r.index].label) + space, legendWidth - 20);
            r.acts = {QRectF(x, y + 1, 18, 18)};
            r.actNames = {"rmcompare"};
        } else if (r.kind == Row::Ind) {
            const bool volume = model.inds[r.index].volume;
            const double x = std::min(2 + tw(f, model.inds[r.index].name.trimmed()) + space + valsWidth(model.inds[r.index].vals) + space,
                                      legendWidth - (volume ? 40 : 60));
            if (volume) {
                r.acts = {QRectF(x, y + 1, 18, 18), QRectF(x + 20, y + 1, 18, 18)};
                r.actNames = {"hide", "remove"};
            } else {
                r.acts = {QRectF(x, y + 1, 18, 18), QRectF(x + 20, y + 1, 18, 18), QRectF(x + 40, y + 1, 18, 18)};
                r.actNames = {"settings", "hide", "remove"};
            }
        }
        y += r.h;
    }
    setGeometry(8, 6, int(legendWidth) + 1, int(std::ceil(y)));
    raise();
    update();
    if (m_input) m_input->sync();
}

void Legend::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    const QFont f = f12();
    const double space = 8;
    const QColor text = theme::chartColor("--text"), muted = theme::chartColor("--muted");
    for (const Row &r : rows) {
        p.save();
        p.setClipRect(QRectF(0, r.y, legendWidth, r.h));
        if (r.kind == Row::Main) {
            text::draw(p, QPointF(0, baseline(f13b(), r.y)), f13b(), model.name, text);
            if (model.hasBar) {
                const QColor c = model.cls == "up" ? theme::chartColor("--up") : model.cls == "down" ? theme::chartColor("--down") : text;
                double x = r.title.width() + space;
                const double by = baseline(f, r.y);
                p.setFont(f);
                for (const auto &[lab, v] : {std::pair{QString("O"), model.o}, {QString("H"), model.h}, {QString("L"), model.l}, {QString("C"), model.c}}) {
                    text::draw(p, QPointF(x, by), f, lab, muted);
                    x += tw(f, lab) + 2;
                    text::draw(p, QPointF(x, by), f, v, c);
                    x += tw(f, v) + 6;
                }
                text::draw(p, QPointF(x, by), f, model.chg, c);
            }
        } else if (r.kind == Row::Note) {
            for (int k = 0; k < r.lines.size(); ++k)
                text::draw(p, QPointF(0, baseline(f11(), r.y + 20 * k)), f11(), r.lines[k], QColor("#f7a600"));
        } else {
            const bool hidden = r.kind == Row::Ind && model.inds[r.index].hidden;
            if (hidden) p.setOpacity(0.45);
            const bool hover = hoverKey == r.key;
            if (hover) {
                p.setPen(Qt::NoPen);
                p.setBrush(theme::chartColor("--panel"));
                p.drawRoundedRect(QRectF(0, r.y, legendWidth, 20), 3, 3);
            }
            const double by = baseline(f, r.y);
            double x = 2;
            const QString name = r.kind == Row::Compare ? model.compare[r.index].label : model.inds[r.index].name.trimmed();
            text::draw(p, QPointF(x, by), f, name, muted);
            x += tw(f, name) + space;
            if (r.kind == Row::Ind) {
                const auto &vals = model.inds[r.index].vals;
                for (int k = 0; k < vals.size(); ++k) {
                    if (k) x += tw(f, " ");
                    text::draw(p, QPointF(x, by), f, vals[k].text, resolve(vals[k].color));
                    x += tw(f, vals[k].text);
                }
            }
            if (!r.acts.isEmpty() && x > r.acts.first().x()) {
                p.setPen(Qt::NoPen);
                p.setBrush(theme::chartColor("--panel"));
                p.drawRect(QRectF(r.acts.first().x() - 2, r.y, legendWidth - r.acts.first().x() + 2, r.h));
            }
            for (int k = 0; k < r.acts.size(); ++k) {
                const QString act = r.actNames[k];
                const QString svg = act == "settings" ? IconGear : act == "hide" ? IconEye : IconX;
                const QColor c = hoverAct == r.key + "|" + act ? text : muted;
                const QPixmap pm = theme::svgPixmap(svg, c, 14, devicePixelRatioF());
                p.drawPixmap(QPointF(r.acts[k].x() + 2, r.y + 3), pm);
            }
        }
        p.restore();
    }
}

// ---------------------------------------------------------------- input

LegendInput::LegendInput(ChartPane *pane, Legend *legend) : QWidget(pane), m_pane(pane), m_legend(legend)
{
    setMouseTracking(true);
    setObjectName("qtmLegendInput");
    legend->setInput(this);
}

void LegendInput::sync()
{
    setGeometry(m_legend->geometry());
    QRegion region;
    for (const auto &r : m_legend->rows) {
        if (r.kind == Legend::Row::Main) region += r.title.toAlignedRect();
        else region += QRectF(0, r.y, r.width, r.h).toAlignedRect();
    }
    if (region.isEmpty()) region = QRegion(0, 0, 1, 1);
    setMask(region);
    raise();
}

QString LegendInput::actAt(const QPointF &p, QString *key) const
{
    for (const auto &r : m_legend->rows) {
        if (p.y() < r.y || p.y() >= r.y + r.h) continue;
        if (key) *key = r.key;
        if (r.kind == Legend::Row::Main) return r.title.contains(p) ? "symbol" : QString();
        for (int k = 0; k < r.acts.size(); ++k)
            if (r.acts[k].contains(p)) return r.actNames[k];
        return QString();
    }
    return QString();
}

void LegendInput::hoverAt(const QPointF &p)
{
    QString key;
    QString act = actAt(p, &key);
    QString hk;
    for (const auto &r : m_legend->rows)
        if ((r.kind == Legend::Row::Ind || r.kind == Legend::Row::Compare) && p.y() >= r.y && p.y() < r.y + r.h
            && p.x() >= 0 && p.x() < r.width)
            hk = r.key;
    bool changed = false;
    if (hk != m_legend->hoverKey) {
        m_legend->hoverKey = hk;
        m_legend->relayout();
        changed = true;
    }
    const QString ha = act.isEmpty() || act == "symbol" ? QString() : key + "|" + act;
    if (ha != m_legend->hoverAct) {
        m_legend->hoverAct = ha;
        changed = true;
    }
    if (changed) m_legend->update();
    setCursor(act.isEmpty() ? Qt::ArrowCursor : Qt::PointingHandCursor);
}

void LegendInput::mouseMoveEvent(QMouseEvent *e)
{
    hoverAt(e->position());
}

void LegendInput::mousePressEvent(QMouseEvent *e)
{
    m_pressKey.clear();
    m_pressAct = e->button() == Qt::LeftButton ? actAt(e->position(), &m_pressKey) : QString();
}

void LegendInput::mouseReleaseEvent(QMouseEvent *e)
{
    QString key;
    const QString act = actAt(e->position(), &key);
    const QString pressed = m_pressAct;
    const QString pressedKey = m_pressKey;
    m_pressAct.clear();
    if (act.isEmpty() || act != pressed || key != pressedKey) return;
    ChartPane *pane = m_pane;
    if (act == "symbol") {
        pane->app->openSymbolSearch();
        return;
    }
    if (act == "rmcompare") {
        const QString sym = key.mid(2);
        pane->cfg.compare.removeAll(sym);
        pane->app->persist();
        pane->load(true);
        return;
    }
    const QString uid = key.mid(2);
    if (uid == VolumeRow) {
        if (act == "hide") pane->cfg.volumeHidden = !pane->cfg.volumeHidden;
        if (act == "remove") pane->cfg.volumeRemoved = true;
        pane->app->persist();
        pane->render(false);
        return;
    }
    IndicatorInstance *ind = nullptr;
    for (auto &x : pane->cfg.indicators)
        if (x.uid == uid) ind = &x;
    if (!ind) return;
    if (act == "remove") pane->removeIndicator(uid);
    if (act == "hide") {
        ind->hidden = !ind->hidden;
        pane->render(false);
        pane->app->persist();
    }
    if (act == "settings") pane->app->openIndicatorSettings(pane, ind);
}

void LegendInput::leaveEvent(QEvent *)
{
    if (!m_legend->hoverKey.isEmpty() || !m_legend->hoverAct.isEmpty()) {
        m_legend->hoverKey.clear();
        m_legend->hoverAct.clear();
        m_legend->relayout();
    }
}

void LegendInput::wheelEvent(QWheelEvent *e)
{
    e->accept();
}

} // namespace qtm
