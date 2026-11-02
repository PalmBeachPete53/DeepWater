// Port of public/js/quantui.js: the gold "Bonus: quant checks" box under a strategy tester result.

#include "QuantUi.h"

#include "Api.h"
#include "PanelsKit.h"
#include "PanelsPrivate.h"
#include "Theme.h"
#include "../core/Error.h"
#include "../core/JsCompat.h"
#include "../core/Quant.h"

#include <QHBoxLayout>
#include <QJsonArray>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace qtm::quantui {

using namespace qtm::kit;
using theme::esc;
using theme::fmtPct;

namespace {

QString show(const QJsonValue &v)
{
    if (v.isDouble()) return js::numberToString(v.toDouble());
    if (v.isString()) return v.toString();
    if (v.isNull()) return "null";
    if (v.isUndefined()) return "undefined";
    return v.toVariant().toString();
}

// <div class="ptq-verdict cls">: inline-block pill, margin 8px 0, padding 4px 10px, 600.
class Verdict : public QWidget
{
public:
    Verdict(const QString &text, const QString &cls) : m_text(text), m_cls(cls)
    {
        const QFontMetricsF fm(uiFont(13, 600));
        setFixedSize(int(std::ceil(fm.horizontalAdvance(text) + 20)), int(std::round(4 + 13 * 1.4 + 4)));
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const Pal pl = pal();
        QPainterPath path;
        path.addRoundedRect(QRectF(rect()), 4, 4);
        p.fillPath(path, pl.hover);
        QColor c = clsColor(m_cls, pl);
        p.setPen(c.isValid() ? c : pl.text);
        const QFont f = uiFont(13, 600);
        p.setFont(f);
        const QFontMetricsF fm(f);
        const double lh = 13 * 1.4;
        p.drawText(QPointF(10, 4 + (lh - (std::round(fm.ascent()) + std::round(fm.descent()))) / 2 + std::round(fm.ascent())),
                   m_text);
    }

private:
    QString m_text, m_cls;
};

// The gold badge: background --gold, #1a1405 700 11px, padding 1px 7px, radius 3.
class Badge : public QWidget
{
public:
    explicit Badge(const QString &text) : m_text(text)
    {
        QFont f = uiFont(11, 700);
        f.setLetterSpacing(QFont::AbsoluteSpacing, 11 * 0.03);
        m_font = f;
        const QFontMetricsF fm(f);
        setFixedSize(int(std::ceil(fm.horizontalAdvance(text) + 14)), int(std::round(1 + 11 * 1.4 + 1)));
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QPainterPath path;
        path.addRoundedRect(QRectF(rect()), 3, 3);
        p.fillPath(path, theme::color("--gold"));
        p.setPen(QColor("#1a1405"));
        p.setFont(m_font);
        const QFontMetricsF fm(m_font);
        const double lh = 11 * 1.4;
        p.drawText(QPointF(7, 1 + (lh - (std::round(fm.ascent()) + std::round(fm.descent()))) / 2 + std::round(fm.ascent())),
                   m_text);
    }

private:
    QString m_text;
    QFont m_font;
};

Cell cell(const QString &html, const QString &cls = QString())
{
    Cell c;
    c.html = html;
    c.cls = cls;
    return c;
}

// render(q) from quantui.js, as widgets appended to `out`.
void render(QVBoxLayout *out, const QJsonObject &q)
{
    const QJsonArray gates = q.value("gates").toArray();
    const int total = gates.size();
    const double passed = q.value("passed").toDouble();
    const QString cls = passed == total ? "up" : passed >= total / 2.0 ? "warn" : "down";
    const QString verdict =
        passed == total ? "It holds up on every check here. Paper trade it next before any real money."
        : passed == 0   ? "It failed every check. The backtest result is most likely luck or tuned to the past."
                        : "Some checks failed. Part of this result may be luck or tuned to the past.";
    // the pill sits in a line box with its 8px margins, then <p> (margin 13px)
    auto *vl = new QWidget;
    auto *vh = new QHBoxLayout(vl);
    vh->setContentsMargins(0, 8, 0, 8);
    vh->addWidget(new Verdict(QString("Passed %1 of %2 checks").arg(js::numberToString(passed)).arg(total), cls));
    vh->addStretch(1);
    out->addWidget(vl);
    out->addSpacing(13);
    out->addWidget(para(esc(verdict)));
    int pendingBottom = 13;   // margin-bottom of the last <p>

    auto *gt = new Table;
    gt->setWrap(true);
    gt->setSticky(false);
    QVector<Row> rows;
    for (const auto &gv : gates) {
        const QJsonObject g = gv.toObject();
        const bool pass = g.value("pass").toBool();
        rows << Row {{cell("<b>" + esc(show(g.value("label"))) + "</b>", "l"), cell(esc(show(g.value("value"))), "l muted"),
                      cell(pass ? "Pass" : "Fail", pass ? "up" : "down")},
                     QString()};
    }
    gt->setRows(rows);
    out->addSpacing(pendingBottom);
    out->addWidget(gt);
    pendingBottom = 0;

    auto addH4 = [&](const QString &t) {
        out->addSpacing(qMax(pendingBottom, 10));
        out->addWidget(h4(t));
        pendingBottom = 4;
    };
    auto addP = [&](QWidget *w, int margin) {
        out->addSpacing(qMax(pendingBottom, margin));
        out->addWidget(w);
        pendingBottom = margin;
    };
    auto addTable = [&](Table *t) {
        out->addSpacing(pendingBottom);
        out->addWidget(t);
        pendingBottom = 0;
    };

    const QJsonValue wfv = q.value("walkForward");
    if (wfv.isObject()) {
        const QJsonObject wf = wfv.toObject();
        const QJsonArray folds = wf.value("folds").toArray();
        auto day = [](const QJsonValue &t) { return js::truthy(t.toDouble()) ? monthYear(t.toDouble() * 1000) : QString(); };
        addH4("Walk-forward");
        addP(hint(QString("For each stretch, the settings that had done best up to then, then how they did on the stretch "
                          "itself. %1 of %2 stretches made money.")
                      .arg(show(wf.value("positiveFolds")))
                      .arg(folds.size())),
             12);
        auto *t = new Table;
        t->setWrap(true);
        t->setSticky(false);
        t->setHeader({cell("Stretch", "l"), cell("Settings picked"), cell("Return"), cell("Sharpe")});
        QVector<Row> fr;
        for (const auto &fv : folds) {
            const QJsonObject f = fv.toObject();
            QStringList chosen;
            for (const auto &c : f.value("chosen").toArray()) chosen << show(c);
            const double ret = f.value("testReturnPct").toDouble();
            fr << Row {{cell(QString("%1 to %2").arg(day(f.value("from")), day(f.value("to"))), "l"),
                        cell(chosen.isEmpty() ? QString("as set") : esc(chosen.join(" / "))),
                        cell(fmtPct(ret), ret >= 0 ? "up" : "down"),
                        cell(js::toFixed(f.value("testSharpe").toDouble(), 2))},
                       QString()};
        }
        t->setRows(fr);
        addTable(t);
    }

    const QJsonArray tunables = q.value("tunables").toArray();
    const QJsonArray variants = q.value("variants").toArray();
    if (tunables.size() == 2) {
        const QJsonObject a = tunables[0].toObject(), b = tunables[1].toObject();
        auto find = [&](const QJsonValue &va, const QJsonValue &vb) -> QJsonObject {
            for (const auto &x : variants) {
                const QJsonArray vals = x.toObject().value("values").toArray();
                if (vals.size() >= 2 && vals[0] == va && vals[1] == vb) return x.toObject();
            }
            return {};
        };
        addH4("Nearby settings");
        addP(hint("Net return for each pair of settings. A real edge is a broad patch of green, not one lucky square. "
                  "Outlined: yours."),
             12);
        auto *t = new Table;
        t->setWrap(true);
        t->setSticky(false);
        t->setVariant(Table::Heat);
        QVector<Cell> head {cell(esc(show(a.value("label"))) + " \\ " + esc(show(b.value("label"))), "l")};
        for (const auto &vb : b.value("values").toArray()) head << cell(show(vb));
        t->setHeader(head);
        QVector<Row> hr;
        for (const auto &va : a.value("values").toArray()) {
            Row row;
            row.cells << cell("<b>" + show(va) + "</b>", "l");
            for (const auto &vb : b.value("values").toArray()) {
                const QJsonObject v = find(va, vb);
                const double n = v.value("netPct").isDouble() ? v.value("netPct").toDouble() : 0;
                const double k = qMin(1.0, std::abs(n) / 100);
                Cell c = cell(fmtPct(n), v.value("isBase").toBool() ? "base" : "");
                c.bg = theme::color(n >= 0 ? "--up" : "--down", 0.12 + 0.55 * k);
                c.outline = v.value("isBase").toBool();
                row.cells << c;
            }
            hr << row;
        }
        t->setRows(hr);
        addTable(t);
    } else if (tunables.size() == 1) {
        const QJsonObject a = tunables[0].toObject();
        addH4("Nearby settings");
        auto *t = new Table;
        t->setWrap(true);
        t->setSticky(false);
        QVector<Cell> head {cell(esc(show(a.value("label"))), "l")};
        Row row;
        row.cells << cell("Net return", "l");
        for (const auto &xv : variants) {
            const QJsonObject v = xv.toObject();
            head << cell(show(v.value("values").toArray().at(0)) + (v.value("isBase").toBool() ? " (yours)" : ""));
            const double n = v.value("netPct").toDouble();
            row.cells << cell(fmtPct(n), n >= 0 ? "up" : "down");
        }
        t->setHeader(head);
        t->setRows({row});
        addTable(t);
    }

    const QJsonValue mcv = q.value("monteCarlo");
    if (mcv.isObject()) {
        const QJsonObject mc = mcv.toObject();
        addH4("Monte Carlo");
        addP(para(QString("Reshuffling these %1 trades %2 times: the worst 5% of runs ended at %3, the middle at %4, the "
                          "best 5% at %5. The usual worst drop was %6%, and 1 run in 20 fell %7% or more. %8% of runs "
                          "ended down.")
                      .arg(show(q.value("base").toObject().value("trades")), intLocale(mc.value("runs").toDouble()),
                           fmtPct(mc.value("p5").toDouble()), fmtPct(mc.value("p50").toDouble()),
                           fmtPct(mc.value("p95").toDouble()), js::toFixed(mc.value("ddP50").toDouble(), 1),
                           js::toFixed(mc.value("ddP95").toDouble(), 1), js::toFixed(mc.value("lossProb").toDouble(), 0))),
             13);
    }
    if (tunables.isEmpty())
        addP(hint("Walk-forward, overfitting and nearby settings need a whole-number setting to vary, like an "
                  "indicator's length, so they don't run on fixed rules or an imported signal."),
             12);
    const QJsonValue psr = q.value("psr");
    addP(hint(QString("Probabilistic Sharpe %1; the best Sharpe %2 random versions would be expected to show is %3. These "
                      "checks use past prices only: passing them doesn't mean a strategy will make money.")
                  .arg(psr.isNull() || psr.isUndefined() ? QString("n/a") : js::toFixed(psr.toDouble(), 3),
                       show(q.value("trials")), js::toFixed(q.value("sr0").toDouble(), 2))),
         12);
    out->addSpacing(pendingBottom);
}

} // namespace

void mountQuantChecks(QWidget *el, const panels::TesterResult &result)
{
    auto *l = static_cast<QVBoxLayout *>(el->layout());
    clear(l);
    auto *box = new Box;
    box->border = theme::color("--gold");
    box->borderWidth = 1.5;
    box->radius = 8;
    box->bg = theme::color("--panel");
    auto *bl = new QVBoxLayout(box);
    // padding 10px 12px + border 1.5
    bl->setContentsMargins(12 + 2, 10 + 2, 12 + 2, 10 + 2);
    bl->setSpacing(0);
    auto *head = new QWidget;
    auto *hl = new QHBoxLayout(head);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(8);
    hl->addWidget(new Badge("Bonus"));
    hl->addWidget(text("<b>Quant checks</b>"));
    hl->addStretch(1);
    bl->addWidget(head);
    bl->addSpacing(12);   // gold-head margin-bottom 4 collapses into the <p> margin 12
    bl->addWidget(hint("The checks quant desks run before they trust a backtest: walk-forward, overfitting (PBO), "
                       "deflated Sharpe, cost stress, nearby settings, and a Monte Carlo of these trades. It re-runs "
                       "this strategy about 25 times with nearby settings, here on your computer."));
    bl->addSpacing(12);
    auto *row = rowFlex();
    auto *run = btn("Run quant checks", "btn gold");
    row->layout()->addWidget(run);
    bl->addWidget(row);
    auto *out = new QWidget;
    auto *ol = new QVBoxLayout(out);
    ol->setContentsMargins(0, 0, 0, 0);
    ol->setSpacing(0);
    bl->addWidget(out);
    l->addWidget(box);
    // box-shadow: 0 0 0 3px var(--gold-soft) takes no space: it is painted by the column around
    // the box, over the column's padding.
    class Ring : public QObject
    {
    public:
        Ring(QWidget *host, QWidget *box) : QObject(box), m_host(host), m_box(box) { host->installEventFilter(this); }

    protected:
        bool eventFilter(QObject *o, QEvent *e) override
        {
            if (o == m_host && e->type() == QEvent::Paint && m_box && m_box->isVisible()) {
                QPainter p(m_host);
                p.setRenderHint(QPainter::Antialiasing);
                const QRectF r(m_box->mapTo(m_host, QPoint(0, 0)), m_box->size());
                QPainterPath outer, inner;
                outer.addRoundedRect(r.adjusted(-3, -3, 3, 3), 11, 11);
                inner.addRoundedRect(r, 8, 8);
                p.fillPath(outer.subtracted(inner), theme::color("--gold-soft"));
            }
            return false;
        }

    private:
        QPointer<QWidget> m_host, m_box;
    };
    // the column's padded block (ScrollCol: outer block > content block > #tQuant)
    QWidget *host = el;
    for (int i = 0; i < 2 && host->parentWidget(); ++i) host = host->parentWidget();
    new Ring(host, box);

    const panels::TesterResult r = result;
    QPointer<QPushButton> runP(run);
    QPointer<QWidget> outP(out);
    QObject::connect(run, &QPushButton::clicked, box, [runP, outP, r] {
        if (!runP || !outP) return;
        runP->setEnabled(false);
        runP->setText(QString::fromUtf8("Checking…"));
        // The previous result stays up while the checks run on a worker thread (a grid of
        // backtests, Monte Carlo, walk-forward), then gives way to the new one or the error.
        auto shown = [runP, outP](const std::function<void(QVBoxLayout *)> &fill) {
            auto *ol = static_cast<QVBoxLayout *>(outP->layout());
            clear(ol);
            fill(ol);
            if (runP) {
                runP->setEnabled(true);
                runP->setText("Run again");
            }
        };
        auto error = [](QVBoxLayout *ol, const QString &message) {
            auto *p = para(esc(message));
            p->setColor(theme::color("--down"));
            ol->addSpacing(13);
            ol->addWidget(p);
            ol->addSpacing(13);
        };
        api::run(outP,
            [r] {
                QuantOptions o;
                o.subBars = r.subBars ? &*r.subBars : nullptr;
                o.indicatorOrder = r.indicatorOrder;
                return quantChecks(r.bars, r.strat, o);
            },
            [shown, error](const QJsonObject &checks) {
                shown([&](QVBoxLayout *ol) {
                    try {
                        render(ol, checks);
                    } catch (const Error &e) {
                        error(ol, e.message());
                    } catch (const std::exception &e) {
                        error(ol, QString::fromUtf8(e.what()));
                    }
                });
            },
            [shown, error](const QString &message) {
                shown([&](QVBoxLayout *ol) { error(ol, message); });
            });
    });
}

} // namespace qtm::quantui
