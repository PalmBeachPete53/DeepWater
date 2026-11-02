// Port of panels.js SCREENER and HEATMAP (lines 868-1236).

#include "PanelsPrivate.h"

#include "Api.h"
#include "Theme.h"
#include "../core/JsCompat.h"

#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QSet>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <limits>

namespace qtm {

using namespace qtm::kit;
using theme::esc;
using theme::fmtNum;
using theme::fmtPct;
using theme::fmtPrice;

namespace {

// r[k] as a JS number (NaN for undefined / non-numbers).
double numOf(const QJsonObject &r, const QString &k)
{
    const QJsonValue v = r.value(k);
    return v.isDouble() ? v.toDouble() : NaN;
}

// String(x) of a JSON field for text matching ("undefined" when missing, like the template literal).
QString strOf(const QJsonObject &r, const QString &k)
{
    const QJsonValue v = r.value(k);
    if (v.isString()) return v.toString();
    if (v.isDouble()) return js::numberToString(v.toDouble());
    if (v.isNull()) return "null";
    if (v.isBool()) return v.toBool() ? "true" : "false";
    return "undefined";
}

} // namespace

// ================================================================ SCREENER

void Panels::Private::initScreener()
{
    sc.rule.left.kind = "ind";
    sc.rule.left.id = "rsi";
    sc.rule.left.params = QJsonObject {{"length", 14}};
    sc.rule.left.paramOrder = {"length"};
    sc.rule.left.output = "value";
    sc.rule.op = "<";
    sc.rule.right.kind = "value";
    sc.rule.right.v = 30;
}

void Panels::Private::loadScreener()
{
    if (sc.table) {
        clear(sc.table->layout());
        sc.table->layout()->addWidget([] {
            Text *t = hint(QString::fromUtf8("Loading…"));
            t->setContentsMargins(12, 10, 12, 10);
            return t;
        }());
    }
    api::get(q, "/api/screener?universe=" + sc.universe,
             [this](const QJsonValue &v) {
                 sc.rows = v.toArray();
                 sc.scan.reset();
                 drawScreener();
             },
             [this](const QString &m) {
                 sc.rows = QJsonArray();
                 app->toast(m, "err");
                 drawScreener();
             });
}

QVector<QJsonObject> Panels::Private::filtered() const
{
    const auto &f = sc.filters;
    auto fv = [&](const QString &k) -> std::optional<double> {
        const QJsonValue v = f.value(k);
        if (v.isDouble()) return v.toDouble();
        return std::nullopt;
    };
    const QString term = f.value("q").toString().toLower();
    const auto minPrice = fv("minPrice"), maxPrice = fv("maxPrice"), minChg = fv("minChg"), maxChg = fv("maxChg"),
               minVol = fv("minVol"), minCap = fv("minCap");
    const QString sector = f.value("sector").toString();
    QVector<QJsonObject> out;
    for (const auto &rv : sc.rows) {
        const QJsonObject r = rv.toObject();
        if (!term.isEmpty() && !(strOf(r, "ticker") + " " + strOf(r, "name")).toLower().contains(term)) continue;
        if (minPrice && !(numOf(r, "price") >= *minPrice)) continue;
        if (maxPrice && !(numOf(r, "price") <= *maxPrice)) continue;
        if (minChg && !(numOf(r, "changePct") >= *minChg)) continue;
        if (maxChg && !(numOf(r, "changePct") <= *maxChg)) continue;
        if (minVol && !(numOf(r, "volume") >= *minVol)) continue;
        if (minCap && !(js::orZero(numOf(r, "marketCap")) >= *minCap)) continue;
        if (!sector.isEmpty() && strOf(r, "sector") != sector) continue;
        if (sc.scan) {
            const auto it = sc.scan->find(r.value("symbol").toString());
            if (it == sc.scan->end() || !it->value("pass").toBool()) continue;
        }
        out << r;
    }
    const QString key = sc.sort;
    const int dir = sc.dir;
    std::stable_sort(out.begin(), out.end(), [&](const QJsonObject &a, const QJsonObject &b) {
        const QJsonValue av = a.value(key);
        double c;
        if (av.isString()) c = QString::localeAwareCompare(av.toString(), strOf(b, key));
        else c = js::orZero(numOf(a, key)) - js::orZero(numOf(b, key));
        return dir * c < 0;
    });
    return out;
}

void Panels::Private::renderScreener()
{
    QStringList sectors;
    for (const auto &rv : sc.rows) {
        const QString s = rv.toObject().value("sector").toString();
        if (!s.isEmpty() && !sectors.contains(s)) sectors << s;
    }
    std::sort(sectors.begin(), sectors.end());

    auto *root = block();
    auto *pad = block(nullptr, QMargins(12, 10, 12, 10));
    vbox(root)->addWidget(pad);

    // first row: market, search, number filters, sector, refresh
    auto *row1 = rowFlex();
    auto *scU = new kit::Select(kit::Select::Field);
    scU->addOption("crypto", "Crypto (USDT pairs)");
    scU->addOption("us", "US stocks");
    scU->addOption("etf", "US ETFs");
    scU->setValue(sc.universe);
    row1->layout()->addWidget(field("Market", scU));
    QVector<QPair<QString, Input *>> inputs;
    auto *search = new Input(Input::Field);
    search->setText(sc.filters.value("q").toString());
    search->setWidthPx(120);
    row1->layout()->addWidget(field("Search", search));
    inputs.push_back({"q", search});
    auto n = [&](const QString &k, const QString &label, int w = 80) {
        auto *in = new Input(Input::Field, true);
        const QJsonValue v = sc.filters.value(k);
        if (v.isDouble()) in->setText(js::numberToString(v.toDouble()));
        in->setWidthPx(w);
        row1->layout()->addWidget(field(label, in));
        inputs.push_back({k, in});
    };
    n("minPrice", "Price from");
    n("maxPrice", "Price to");
    n("minChg", "Change % from", 70);
    n("maxChg", "Change % to", 70);
    n("minVol", "Min volume", 100);
    if (sc.universe != "crypto") n("minCap", "Min market cap", 110);
    kit::Select *sectorSel = nullptr;
    if (sectors.size() > 1) {
        sectorSel = new kit::Select(kit::Select::Field);
        sectorSel->addOption("", "All");
        for (const QString &s : sectors) sectorSel->addOption(s, s);
        sectorSel->setValue(sc.filters.value("sector").toString());
        row1->layout()->addWidget(field("Sector", sectorSel));
    }
    auto *refresh = btn("Refresh", "btn");
    refresh->setProperty("alignSelf", "end");
    row1->layout()->addWidget(refresh);
    vbox(pad)->addWidget(row1);

    // second row: technical filter
    auto *row2 = rowFlex();
    row2->layout()->addWidget(hint("Technical filter:"));
    auto *rulesBox = new rules::RulesBox;
    rules::RuleWidget *rw = rulesBox->addRule(sc.rule);
    rw->removeX();
    row2->layout()->addWidget(rulesBox);
    auto *scTf = new kit::Select(kit::Select::Field);
    for (const QString &t : {QString("1h"), QString("4h"), QString("1D"), QString("1W")}) scTf->addOption(t, t);
    scTf->setValue(sc.tf);
    row2->layout()->addWidget(field("On", scTf));
    auto *scan = btn("Scan top 100", "btn primary");
    scan->setProperty("alignSelf", "end");
    row2->layout()->addWidget(scan);
    QPushButton *clearBtn = nullptr;
    if (sc.scan) {
        clearBtn = btn("Clear scan", "btn");
        clearBtn->setProperty("alignSelf", "end");
        row2->layout()->addWidget(clearBtn);
    }
    add(pad, row2, 8, 0);

    auto *table = new QWidget;
    auto *tl = new QVBoxLayout(table);
    tl->setContentsMargins(0, 0, 0, 0);
    tl->setSpacing(0);
    vbox(root)->addWidget(table);
    vbox(root)->addStretch(1);
    sc.table = table;
    sc.scanBtn = scan;
    bottom->setContent(root, false);

    QObject::connect(scU, &kit::Select::changed, root, [this](const QString &v) {
        sc.universe = v;
        sc.filters.clear();
        renderScreener();
        loadScreener();
    });
    for (const auto &[k, in] : inputs) {
        const QString key = k;
        Input *input = in;
        QObject::connect(input, &QLineEdit::textEdited, root, [this, key, input] {
            const QString v = input->text().trimmed();
            if (v.isEmpty()) {
                sc.filters.insert(key, QJsonValue::Null);
            } else if (input->isNumber()) {
                bool ok = false;
                const double d = v.toDouble(&ok);
                // an <input type="number"> holding no number has value "" (null filter)
                sc.filters.insert(key, ok ? QJsonValue(d) : QJsonValue(QJsonValue::Null));
            } else {
                sc.filters.insert(key, v);
            }
            drawScreener();
        });
    }
    if (sectorSel)
        QObject::connect(sectorSel, &kit::Select::changed, root, [this](const QString &v) {
            sc.filters.insert("sector", v.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(v));
            drawScreener();
        });
    QObject::connect(refresh, &QPushButton::clicked, root, [this] { loadScreener(); });
    if (clearBtn)
        QObject::connect(clearBtn, &QPushButton::clicked, root, [this] {
            sc.scan.reset();
            renderScreener();
        });
    QPointer<rules::RulesBox> rb(rulesBox);
    QPointer<kit::Select> tfP(scTf);
    QPointer<QWidget> rootP(root);
    QObject::connect(scan, &QPushButton::clicked, root, [this, rb, tfP, rootP, scan] {
        if (!rb || !tfP) return;
        const QVector<rules::Rule> read = rb->read();
        if (read.isEmpty()) return;
        const rules::Rule rule = read.first();
        sc.rule = rule;
        sc.tf = tfP->value();
        const rules::Compiled c = rules::compileRules({{"rules", {rule}}});
        auto prev = sc.scan;
        sc.scan.reset();
        QVector<QJsonObject> f = filtered();
        std::stable_sort(f.begin(), f.end(), [](const QJsonObject &a, const QJsonObject &b) {
            return numOf(b, "volume") - numOf(a, "volume") < 0;
        });
        QJsonArray targets;
        for (int i = 0; i < f.size() && i < 100; ++i) targets.append(f[i].value("symbol"));
        sc.scan = prev;
        scan->setText(QString::fromUtf8("Scanning %1…").arg(targets.size()));
        const int n = targets.size();
        api::post(q, "/api/scan",
                  QJsonObject {{"symbols", targets}, {"tf", sc.tf}, {"indicators", c.indicators}, {"rules", c.group("rules")}},
                  [this, n, rootP](const QJsonValue &v) {
                      QHash<QString, QJsonObject> map;
                      int hits = 0;
                      for (const auto &xv : v.toArray()) {
                          const QJsonObject x = xv.toObject();
                          map.insert(x.value("symbol").toString(), x);
                          if (x.value("pass").toBool()) ++hits;
                      }
                      sc.scan = map;
                      app->toast(QString("%1 of %2 match.").arg(hits).arg(n));
                      if (rootP) renderScreener();
                  },
                  [this, rootP](const QString &m) {
                      app->toast(m, "err");
                      if (rootP) renderScreener();
                  });
    });

    if (sc.rows.isEmpty()) loadScreener();
    else drawScreener();
}

void Panels::Private::drawScreener()
{
    QWidget *box = sc.table;
    if (!box) return;
    clear(box->layout());
    const QVector<QJsonObject> rows = filtered();
    const bool crypto = sc.universe == "crypto";
    QVector<QPair<QString, QString>> cols = {
        {"ticker", "Symbol"}, {"name", "Name"}, {"price", "Price"}, {"changePct", "Change %"},
        {"volume", crypto ? "Volume (USDT)" : "Volume"},
    };
    if (!crypto) {
        cols.push_back({"marketCap", "Market cap"});
        cols.push_back({"sector", "Sector"});
    }
    auto *t = new Table;
    QVector<Cell> head;
    for (const auto &[k, l] : cols) {
        Cell c;
        c.html = esc(l) + (sc.sort == k ? (sc.dir < 0 ? QString::fromUtf8(" ↓") : QString::fromUtf8(" ↑")) : QString());
        c.cls = k == "name" || k == "sector" ? "l" : "";
        head << c;
    }
    t->setHeader(head);
    QVector<Row> trs;
    for (int i = 0; i < rows.size() && i < 500; ++i) {
        const QJsonObject &r = rows[i];
        Row row;
        row.key = r.value("symbol").toString();
        const double chg = numOf(r, "changePct");
        row.cells << Cell {"<b>" + esc(r.value("ticker").toString()) + "</b>"};
        Cell name;
        name.html = esc(r.value("name").toString().left(40));
        name.cls = "l muted";
        row.cells << name;
        row.cells << Cell {fmtPrice(numOf(r, "price"))};
        Cell ch;
        ch.html = fmtPct(chg);
        ch.cls = chg >= 0 ? "up" : "down";
        row.cells << ch;
        row.cells << Cell {fmtNum(numOf(r, "volume"), 0)};
        if (!crypto) {
            row.cells << Cell {fmtNum(numOf(r, "marketCap"), 0)};
            Cell sec;
            sec.html = esc(r.value("sector").toString());
            sec.cls = "l muted";
            row.cells << sec;
        }
        trs << row;
    }
    t->setRows(trs);
    box->layout()->addWidget(t);
    const int n = rows.size();
    Text *note = hint(QString("%1 match%2%3. Click a row to open its chart.%4")
                          .arg(n)
                          .arg(n == 1 ? "" : "es")
                          .arg(n > 500 ? " (showing 500)" : "")
                          .arg(crypto ? "" : " Live prices for the 200 largest companies; the rest show the last close."));
    note->setContentsMargins(12, 10, 12, 10);
    box->layout()->addWidget(note);
    QStringList keys;
    for (const auto &c : cols) keys << c.first;
    QObject::connect(t, &Table::headerClicked, box, [this, keys](int col) {
        const QString k = keys.value(col);
        if (k.isEmpty()) return;
        sc.dir = sc.sort == k ? -sc.dir : -1;
        sc.sort = k;
        QMetaObject::invokeMethod(q, [this] { drawScreener(); }, Qt::QueuedConnection);
    });
    QObject::connect(t, &Table::rowClicked, box, [this, t](int r) {
        const QString s = t->rows().value(r).key;
        if (!s.isEmpty()) app->setSymbol(s);
    });
}

// ================================================================ HEATMAP

namespace {

struct Tile {
    QJsonObject r;       // the screener row
    double value = 0;
    double x = 0, y = 0, w = 0, h = 0;
    // sector groups
    QString name;
    QVector<QJsonObject> rs;
};

QVector<Tile> squarify(const QVector<Tile> &items, double x, double y, double w, double h)
{
    QVector<Tile> out;
    QVector<Tile> rest;
    for (const Tile &i : items)
        if (i.value > 0) rest << i;
    double total = 0;
    for (const Tile &i : rest) total += i.value;
    if (!js::truthy(total)) return out;
    const double scale = (w * h) / total;
    double rx = x, ry = y, rw = w, rh = h;
    while (!rest.isEmpty()) {
        const double shortSide = js::min(rw, rh);
        QVector<Tile> row;
        double best = js::Inf;
        for (const Tile &it : rest) {
            double s = 0, mx = -js::Inf, mn = js::Inf;
            auto acc = [&](const Tile &t) {
                const double v = t.value * scale;
                s += v;
                mx = js::max(mx, v);
                mn = js::min(mn, v);
            };
            for (const Tile &t : row) acc(t);
            acc(it);
            const double worst = js::max((shortSide * shortSide * mx) / (s * s), (s * s) / (shortSide * shortSide * mn));
            if (worst > best) break;
            best = worst;
            row << it;
        }
        double area = 0;
        for (const Tile &t : row) area += t.value * scale;
        const double thick = area / shortSide;
        double off = 0;
        for (const Tile &it : row) {
            const double len = (it.value * scale) / thick;
            Tile t = it;
            if (rw >= rh) {
                t.x = rx;
                t.y = ry + off;
                t.w = thick;
                t.h = len;
            } else {
                t.x = rx + off;
                t.y = ry;
                t.w = len;
                t.h = thick;
            }
            out << t;
            off += len;
        }
        if (rw >= rh) {
            rx += thick;
            rw -= thick;
        } else {
            ry += thick;
            rh -= thick;
        }
        rest = rest.mid(row.size());
    }
    return out;
}

// The <canvas id="hmC"> and its .tip tooltip.
class HeatCanvas : public QWidget
{
public:
    HeatCanvas(Panels::Private *d, QWidget *parent = nullptr) : QWidget(parent), m_d(d)
    {
        setMouseTracking(true);
    }
    ~HeatCanvas() override
    {
        if (m_tip) m_tip->deleteLater();
    }
    // drawHeatmap() has drawn on this canvas: a new canvas stays blank until its data arrives
    // (and after a failed load), like a fresh <canvas>.
    bool drawn = false;

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void leaveEvent(QEvent *) override { removeTip(); }
    void mousePressEvent(QMouseEvent *e) override
    {
        if (e->button() != Qt::LeftButton) return;
        const Tile *t = tileAt(e->position());
        if (t) m_d->app->setSymbol(t->r.value("symbol").toString());
    }

private:
    const Tile *tileAt(const QPointF &p) const
    {
        for (const Tile &q : m_tiles)
            if (p.x() >= q.x && p.x() <= q.x + q.w && p.y() >= q.y && p.y() <= q.y + q.h) return &q;
        return nullptr;
    }
    void removeTip()
    {
        if (m_tip) {
            m_tip->hide();
            m_tip->deleteLater();
            m_tip = nullptr;
        }
    }
    Panels::Private *m_d;
    QVector<Tile> m_tiles;
    QPointer<Box> m_tip;
    QPointer<Text> m_tipText;
};

void HeatCanvas::paintEvent(QPaintEvent *)
{
    auto &hm = m_d->hm;
    m_tiles.clear();
    if (!hm.rows || !drawn) return;
    QPainter ctx(this);
    ctx.setRenderHint(QPainter::Antialiasing);
    ctx.setRenderHint(QPainter::TextAntialiasing);
    const double W = width(), H = height();
    const bool us = hm.universe == "us";
    const double cap = us ? 3 : 8;
    // From a neutral tile (#424550, or the theme's --panel-2) toward --up / --down.
    const QColor b0 = theme::hasPalette() ? theme::color("--panel-2") : QColor(66, 69, 80);
    const QColor u0 = theme::color("--up"), d0 = theme::color("--down");
    auto color = [&](double ch) {
        const double t = js::max(-1, js::min(1, ch / cap));
        const int base[3] = {b0.red(), b0.green(), b0.blue()}, up[3] = {u0.red(), u0.green(), u0.blue()},
                  dn[3] = {d0.red(), d0.green(), d0.blue()};
        const int *tgt = t >= 0 ? up : dn;
        const double k = std::abs(t);
        int c[3];
        for (int i = 0; i < 3; ++i) c[i] = int(js::round(base[i] + (tgt[i] - base[i]) * k));
        if (std::isnan(t)) return QColor(base[0], base[1], base[2]);
        return QColor(c[0], c[1], c[2]);
    };
    auto val = [&](const QJsonObject &r) {
        const double v = hm.size == "volume" ? numOf(r, "volume") * (us ? numOf(r, "price") : 1)
                                             : (js::truthy(numOf(r, "marketCap")) ? numOf(r, "marketCap") : numOf(r, "volume"));
        return js::max(1, v);
    };
    auto rowTiles = [&](const QVector<QJsonObject> &rs) {
        QVector<Tile> items;
        for (const QJsonObject &r : rs) {
            Tile t;
            t.r = r;
            t.value = val(r);
            items << t;
        }
        std::stable_sort(items.begin(), items.end(), [](const Tile &a, const Tile &b) { return b.value - a.value < 0; });
        return items;
    };
    QVector<Tile> tiles;
    if (us) {
        QVector<QPair<QString, QVector<QJsonObject>>> bySector;
        for (const auto &rv : *hm.rows) {
            const QJsonObject r = rv.toObject();
            QString s = r.value("sector").toString();
            if (s.isEmpty()) s = "Other";
            int i = 0;
            for (; i < bySector.size(); ++i)
                if (bySector[i].first == s) break;
            if (i == bySector.size()) bySector.push_back({s, {}});
            bySector[i].second << r;
        }
        QVector<Tile> groups;
        for (const auto &[name, rs] : bySector) {
            Tile g;
            g.name = name;
            g.rs = rs;
            for (const QJsonObject &r : rs) g.value += val(r);
            groups << g;
        }
        std::stable_sort(groups.begin(), groups.end(), [](const Tile &a, const Tile &b) { return b.value - a.value < 0; });
        const QFont gf = uiFont(10, 600);
        const QFontMetricsF gfm(gf);
        for (const Tile &g : squarify(groups, 0, 0, W, H)) {
            tiles << squarify(rowTiles(g.rs), g.x + 1, g.y + 14, g.w - 2, g.h - 15);
            ctx.fillRect(QRectF(g.x, g.y, g.w, g.h), theme::color("--panel"));
            if (g.w > 50) {
                ctx.setFont(gf);
                ctx.setPen(theme::color("--muted"));
                ctx.drawText(QPointF(g.x + 4, g.y + 2 + 0.72 * 10), g.name.toUpper().left(int(std::floor(g.w / 7))));
            }
        }
    } else {
        QVector<QJsonObject> rs;
        for (const auto &rv : *hm.rows) rs << rv.toObject();
        tiles = squarify(rowTiles(rs), 0, 0, W, H);
    }
    for (const Tile &t : tiles) {
        ctx.fillRect(QRectF(t.x + 0.5, t.y + 0.5, t.w - 1, t.h - 1), color(numOf(t.r, "changePct")));
        if (t.w > 34 && t.h > 22) {
            const double fs = js::max(10, js::min(22, std::sqrt(t.w * t.h) / 5));
            ctx.setPen(theme::textOn(color(numOf(t.r, "changePct"))));
            QFont f = uiFont(fs, 600);
            ctx.setFont(f);
            QFontMetricsF fm(f);
            QString tick = t.r.value("ticker").toString();
            if (tick.endsWith("USDT")) tick.chop(4);
            double fontPx = fs;
            // textAlign center, textBaseline middle
            auto drawMid = [&](const QString &s, double cx, double cy) {
                // canvas "middle": half way between the em box top and bottom (typo ascent 0.72 em)
                ctx.drawText(QPointF(cx - fm.horizontalAdvance(s) / 2, cy + (0.72 - 0.28) / 2 * fontPx), s);
            };
            drawMid(tick, t.x + t.w / 2, t.y + t.h / 2 - (t.h > 40 ? fs * 0.45 : 0));
            if (t.h > 40) {
                f = uiFont(fs * 0.75);
                fontPx = fs * 0.75;
                ctx.setFont(f);
                fm = QFontMetricsF(f);
                drawMid(fmtPct(numOf(t.r, "changePct")), t.x + t.w / 2, t.y + t.h / 2 + fs * 0.6);
            }
        }
    }
    m_tiles = tiles;
}

void HeatCanvas::mouseMoveEvent(QMouseEvent *e)
{
    const Tile *t = tileAt(e->position());
    if (!t) {
        removeTip();
        return;
    }
    QWidget *top = window();
    if (!m_tip) {
        m_tip = new Box(top);
        m_tip->radius = 6;
        auto *l = new QVBoxLayout(m_tip);
        l->setContentsMargins(10, 7, 10, 7);
        TextStyle st;
        st.font = uiFont(12);
        st.wrap = false;
        m_tipText = new Text(QString(), st);
        l->addWidget(m_tipText);
        m_tip->setAttribute(Qt::WA_TransparentForMouseEvents);
    }
    const QJsonObject &r = t->r;
    const double chg = numOf(r, "changePct");
    QString html = QString("<b>%1</b> %2<br>%3 <span class=\"%4\">%5</span>")
                       .arg(esc(r.value("ticker").toString()), esc(r.value("name").toString()),
                            fmtPrice(numOf(r, "price")), chg >= 0 ? "up" : "down", fmtPct(chg));
    if (js::truthy(numOf(r, "marketCap"))) html += "<br>Market cap " + fmtNum(numOf(r, "marketCap"));
    m_tipText->setHtml(html);
    m_tip->adjustSize();
    m_tip->move(mapTo(top, e->position().toPoint()) + QPoint(14, 14));
    m_tip->raise();
    m_tip->show();
}

// The heatmap tab: the .pad row-flex header and the canvas below (height: calc(100% - 64px)).
class HeatRoot : public QWidget
{
public:
    QWidget *head = nullptr;
    QWidget *canvas = nullptr;

protected:
    void resizeEvent(QResizeEvent *) override
    {
        const int hh = head->heightForWidth(width());
        head->setGeometry(0, 0, width(), hh);
        canvas->setGeometry(0, hh, width(), qMax(0, height() - 64));
    }
};

} // namespace

void Panels::Private::renderHeatmap()
{
    auto *root = new HeatRoot;
    auto *head = rowFlex(root);
    head->layout()->setContentsMargins(12, 10, 12, 10);
    auto *hmU = new kit::Select(kit::Select::Field);
    hmU->addOption("us", "US stocks (200 largest)");
    hmU->addOption("crypto", "Crypto (100 most traded)");
    hmU->setValue(hm.universe);
    auto *hmS = new kit::Select(kit::Select::Field);
    hmS->addOption("marketCap", "Market cap");
    hmS->addOption("volume", "Volume");
    hmS->setValue(hm.size);
    head->layout()->addWidget(field("Market", hmU));
    head->layout()->addWidget(field("Size by", hmS));
    Text *note = hint("Colour = today's change. Click a tile to open its chart.");
    note->setProperty("alignSelf", "end");
    head->layout()->addWidget(note);
    auto *canvas = new HeatCanvas(this, root);
    root->head = head;
    root->canvas = canvas;
    hm.canvas = canvas;
    bottom->setContent(root, true);

    QObject::connect(hmU, &kit::Select::changed, root, [this](const QString &v) {
        hm.universe = v;
        renderHeatmap();
    });
    QObject::connect(hmS, &kit::Select::changed, root, [this](const QString &v) {
        hm.size = v;
        drawHeatmap();
    });
    const QString universe = hm.universe;
    api::get(root, "/api/screener?universe=" + universe,
             [this, universe](const QJsonValue &v) {
                 QVector<QJsonObject> rows;
                 for (const auto &rv : v.toArray()) rows << rv.toObject();
                 QJsonArray out;
                 if (universe == "us") {
                     QVector<QJsonObject> f;
                     for (const auto &r : rows)
                         if (numOf(r, "marketCap") > 0) f << r;
                     std::stable_sort(f.begin(), f.end(), [](const QJsonObject &a, const QJsonObject &b) {
                         return numOf(b, "marketCap") - numOf(a, "marketCap") < 0;
                     });
                     for (int i = 0; i < f.size() && i < 200; ++i) out.append(f[i]);
                 } else {
                     std::stable_sort(rows.begin(), rows.end(), [](const QJsonObject &a, const QJsonObject &b) {
                         return numOf(b, "volume") - numOf(a, "volume") < 0;
                     });
                     for (int i = 0; i < rows.size() && i < 100; ++i) out.append(rows[i]);
                 }
                 hm.rows = out;
                 drawHeatmap();
             },
             [this](const QString &m) { app->toast(m, "err"); });
}

void Panels::Private::drawHeatmap()
{
    if (!hm.canvas || !hm.rows) return;
    static_cast<HeatCanvas *>(hm.canvas.data())->drawn = true;
    hm.canvas->update();
}

} // namespace qtm
