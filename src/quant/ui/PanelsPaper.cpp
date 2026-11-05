// Port of panels.js PAPER TRADING (lines 1238-1504) and checkReplayOrders (1608-1623).
// While the active chart is in bar replay, paper trading uses a separate practice account that
// fills at the replayed bar's prices, so replay trades never mix with the live paper account.

#include "PanelsPrivate.h"

#include "Api.h"
#include "Theme.h"
#include "../core/JsCompat.h"

#include <QDateTime>
#include <QVBoxLayout>

#include <cmath>

namespace qtm {

namespace {

// JS keeps p.positions in insertion order (a new symbol goes last, closing one removes it).
// QJsonObject sorts its keys, so the order is kept beside it in p.positionOrder.
QStringList positionKeys(const QJsonObject &p)
{
    const QJsonObject positions = p.value("positions").toObject();
    QStringList keys;
    for (const auto &v : p.value("positionOrder").toArray())
        if (positions.contains(v.toString()) && !keys.contains(v.toString())) keys << v.toString();
    for (auto it = positions.begin(); it != positions.end(); ++it)
        if (!keys.contains(it.key())) keys << it.key();
    return keys;
}

} // namespace


using namespace qtm::kit;
using theme::esc;
using theme::fmtNum;
using theme::fmtPct;
using theme::fmtPrice;

namespace {

QJsonObject freshAccount()
{
    return QJsonObject {{"initial", 100000}, {"cash", 100000},      {"feePct", 0.05},
                        {"positions", QJsonObject()}, {"orders", QJsonArray()}, {"history", QJsonArray()}};
}

double numOf(const QJsonObject &o, const QString &k)
{
    const QJsonValue v = o.value(k);
    return v.isDouble() ? v.toDouble() : NaN;
}

QString nowId()
{
    return QString::number(QDateTime::currentMSecsSinceEpoch());
}

} // namespace

ChartPane *Panels::Private::replayPane() const
{
    return app->active && app->active->replay ? app->active : nullptr;
}

QJsonObject Panels::Private::livePaper()
{
    if (!app->state.paper.isObject()) app->state.paper = freshAccount();
    return app->state.paper.toObject();
}

QJsonObject Panels::Private::paper()
{
    if (replayPane()) {
        if (!app->state.replayPaper.isObject()) app->state.replayPaper = freshAccount();
        return app->state.replayPaper.toObject();
    }
    return livePaper();
}

void Panels::Private::savePaper(const QJsonObject &acct)
{
    if (replayPane()) app->state.replayPaper = acct;
    else app->state.paper = acct;
}

std::optional<double> Panels::Private::price(const QString &s) const
{
    ChartPane *rp = replayPane();
    if (rp && s == rp->cfg.symbol) {
        const int i = rp->replay->index;
        if (i >= 0 && i < rp->bars.size()) return rp->bars[i].close;
        return std::nullopt;
    }
    if (rp) return std::nullopt;
    if (app->prices.contains(s)) return app->prices.value(s);
    return std::nullopt;
}

QStringList Panels::Private::paperSymbols() const
{
    const QJsonValue pv = app->state.paper;
    if (!pv.isObject()) return {};
    const QJsonObject p = pv.toObject();
    QStringList out = positionKeys(p);
    for (const auto &o : p.value("orders").toArray()) out << o.toObject().value("symbol").toString();
    return out;
}

void Panels::Private::fill(const QString &sym, const QString &side, double qty, double px, const QString &note,
                           bool liveAccount)
{
    QJsonObject p = liveAccount ? livePaper() : paper();
    ChartPane *rp = liveAccount ? nullptr : replayPane();
    const double sgn = side == "buy" ? qty : -qty;
    QJsonObject positions = p.value("positions").toObject();
    QStringList order = positionKeys(p);
    const QJsonObject pos = positions.contains(sym) ? positions.value(sym).toObject()
                                                    : QJsonObject {{"qty", 0}, {"avg", 0}};
    const double pq = pos.value("qty").toDouble(), pavg = pos.value("avg").toDouble();
    const double fee = std::abs(qty * px) * (numOf(p, "feePct") / 100);
    double realized = 0;
    if (pq != 0 && js::sign(pq) != js::sign(sgn)) {
        const double closing = js::sign(sgn) * js::min(std::abs(sgn), std::abs(pq));
        realized = -closing * (px - pavg);
    }
    const double nq = pq + sgn;
    if (nq == 0) positions.remove(sym);
    else if (pq == 0 || js::sign(nq) != js::sign(pq)) positions.insert(sym, QJsonObject {{"qty", nq}, {"avg", px}});
    else if (js::sign(sgn) == js::sign(pq))
        positions.insert(sym, QJsonObject {{"qty", nq}, {"avg", (pq * pavg + sgn * px) / nq}});
    else positions.insert(sym, QJsonObject {{"qty", nq}, {"avg", pavg}});
    p.insert("positions", positions);
    if (!positions.contains(sym)) order.removeAll(sym);
    else if (!order.contains(sym)) order << sym;
    p.insert("positionOrder", QJsonArray::fromStringList(order));
    p.insert("cash", numOf(p, "cash") - (sgn * px + fee));
    QJsonArray history = p.value("history").toArray();
    double time = double(QDateTime::currentMSecsSinceEpoch());
    if (rp) {
        const int i = rp->replay->index;
        time = (i >= 0 && i < rp->bars.size() ? rp->bars[i].time : NaN) * 1000;
    }
    history.prepend(QJsonObject {{"time", time}, {"symbol", sym}, {"side", side}, {"qty", qty}, {"price", px},
                                 {"fee", fee}, {"realized", realized - fee}, {"note", note}});
    while (history.size() > 500) history.removeLast();
    p.insert("history", history);
    if (liveAccount) app->state.paper = p;
    else savePaper(p);
    app->persist();
    app->toast(QString("Paper %1 %2 %3 at %4").arg(side, fmtNum(qty, 4), theme::ticker(sym), fmtPrice(px)));
}

namespace {
// x !== o && (!o.oco || x.oco !== o.oco)
QJsonArray dropFilled(const QJsonArray &orders, const QJsonObject &o)
{
    QJsonArray out;
    const QJsonValue oco = o.value("oco");
    const bool hasOco = oco.isString() && !oco.toString().isEmpty();
    for (const auto &xv : orders) {
        const QJsonObject x = xv.toObject();
        if (x == o) continue;
        if (hasOco && x.value("oco") == oco) continue;
        out.append(x);
    }
    return out;
}
} // namespace

void Panels::Private::checkOrders(const QString &sym, double px)
{
    if (!app->state.paper.isObject()) return;
    if (app->state.paper.toObject().value("orders").toArray().isEmpty()) return;
    bool changed = false;
    const QJsonArray snapshot = app->state.paper.toObject().value("orders").toArray();
    for (const auto &ov : snapshot) {
        const QJsonObject o = ov.toObject();
        if (o.value("symbol").toString() != sym) continue;
        const double op = numOf(o, "price");
        const bool buy = o.value("side").toString() == "buy";
        const bool hit = o.value("type").toString() == "limit" ? (buy ? px <= op : px >= op) : (buy ? px >= op : px <= op);
        if (!hit) continue;
        QJsonObject p = app->state.paper.toObject();
        p.insert("orders", dropFilled(p.value("orders").toArray(), o));
        app->state.paper = p;
        const QString note = o.value("note").toString();
        fill(sym, o.value("side").toString(), numOf(o, "qty"), o.value("type").toString() == "limit" ? op : px,
             !note.isEmpty() ? note : o.value("type").toString() + " order", true);
        changed = true;
    }
    if (changed && bottomTab == "paper") renderPaper();
}

double Panels::Private::equity()
{
    const QJsonObject p = paper();
    double sum = 0;
    const QJsonObject positions = p.value("positions").toObject();
    for (const QString &sym : positionKeys(p)) {
        const QJsonObject pos = positions.value(sym).toObject();
        sum += pos.value("qty").toDouble() * price(sym).value_or(pos.value("avg").toDouble());
    }
    return numOf(p, "cash") + sum;
}

void Panels::Private::drawPaperNumbers()
{
    const QJsonObject p = paper();
    const double eq = equity();
    const double initial = numOf(p, "initial");
    if (ppEq) {
        ppEq->setItems({
            {"Equity", fmtPrice(eq), ""},
            {"Cash", fmtPrice(numOf(p, "cash")), ""},
            {"Total P&L", QString("%1 (%2)").arg(fmtPrice(eq - initial), fmtPct(((eq - initial) / initial) * 100)),
             eq >= initial ? "up" : "down"},
        });
    }
    if (ppPos) {
        QVector<Row> rows;
        const QJsonObject positions = p.value("positions").toObject();
        for (const QString &s : positionKeys(p)) {
            const QJsonObject x = positions.value(s).toObject();
            const double qty = x.value("qty").toDouble(), avg = x.value("avg").toDouble();
            const double px = price(s).value_or(avg), pnl = qty * (px - avg);
            Row r;
            r.key = s;
            r.cells << Cell {"<b>" + esc(theme::ticker(s)) + "</b>"};
            r.cells << Cell {qty > 0 ? "Long" : "Short"};
            r.cells << Cell {fmtNum(std::abs(qty), 4)};
            r.cells << Cell {fmtPrice(avg)};
            r.cells << Cell {fmtPrice(px)};
            Cell c;
            c.html = QString("%1 (%2)").arg(fmtPrice(pnl), fmtPct(((px - avg) / avg) * 100 * js::sign(qty)));
            c.cls = pnl >= 0 ? "up" : "down";
            r.cells << c;
            Cell b;
            b.button = "Close";
            b.data = s;
            r.cells << b;
            rows << r;
        }
        if (rows.isEmpty()) {
            Cell c;
            c.html = "No open positions.";
            c.cls = "l muted";
            c.colspan = 7;
            rows << Row {{c}, {}};
        }
        ppPos->setRows(rows);
    }
}

void Panels::Private::renderPaper()
{
    const QJsonObject p = paper();
    const QString sym = app->active ? app->active->cfg.symbol : QString();
    std::optional<double> pxo = price(sym);
    if (!pxo && app->active && !app->active->bars.isEmpty()) pxo = app->active->bars.last().close;
    const double px = pxo.value_or(NaN);
    const bool hasPx = pxo && js::truthy(px);

    auto *split = new Split;
    QWidget *left = split->left->body();
    // order ticket
    Text *title = h4(QString::fromUtf8("Order ticket · %1 %2")
                         .arg(esc(theme::ticker(sym)), hasPx ? "<span class=\"muted\">" + fmtPrice(px) + "</span>" : ""));
    add(left, title, 0, 4);
    auto *row1 = rowFlex();
    auto *ppType = new kit::Select(kit::Select::Field);
    ppType->addOption("market", "Market");
    ppType->addOption("limit", "Limit");
    ppType->addOption("stop", "Stop");
    auto *ppPrice = new Input(Input::Field, true);
    if (hasPx) ppPrice->setText(numPrecision(px, 6));
    ppPrice->setWidthPx(110);
    auto *ppAmt = new Input(Input::Field, true);
    ppAmt->setText("1000");
    ppAmt->setWidthPx(100);
    row1->layout()->addWidget(field("Type", ppType));
    row1->layout()->addWidget(field("Price", ppPrice));
    row1->layout()->addWidget(field("Amount ($)", ppAmt));
    add(left, row1);
    auto *row2 = rowFlex();
    auto *ppSl = new Input(Input::Field, true);
    ppSl->setWidthPx(90);
    auto *ppTp = new Input(Input::Field, true);
    ppTp->setWidthPx(90);
    row2->layout()->addWidget(field("Stop loss % (optional)", ppSl));
    row2->layout()->addWidget(field("Take profit % (optional)", ppTp));
    add(left, row2, 6, 0);
    auto *row3 = rowFlex();
    auto colored = [](QPushButton *b, const QString &var) {
        const QColor c = theme::color(var);
        QColor h = QColor::fromHsvF(c.hsvHueF(), c.hsvSaturationF(), std::min(1.0f, c.valueF() * 1.1f));
        b->setStyleSheet(QString("QPushButton { background: %1; border-color: %1; color: %3; }"
                                 "QPushButton:hover { background: %2; border-color: %2; }")
                             .arg(c.name(), h.name(), theme::textOn(c).name()));
    };
    auto *ppBuy = btn("Buy", "btn primary");
    colored(ppBuy, "--up");
    auto *ppSell = btn("Sell / short", "btn primary");
    colored(ppSell, "--down");
    row3->layout()->addWidget(ppBuy);
    row3->layout()->addWidget(ppSell);
    add(left, row3, 10, 0);
    // account
    add(left, h4("Account"), 10, 4);
    auto *row4 = rowFlex();
    auto *ppInit = new Input(Input::Field, true);
    ppInit->setText(js::numberToString(numOf(p, "initial")));
    ppInit->setWidthPx(110);
    auto *ppFee = new Input(Input::Field, true);
    ppFee->setText(js::numberToString(numOf(p, "feePct")));
    ppFee->setWidthPx(80);
    auto *ppReset = btn("Reset account", "btn danger");
    ppReset->setProperty("alignSelf", "end");
    row4->layout()->addWidget(field("Starting balance", ppInit));
    row4->layout()->addWidget(field("Fee % per trade", ppFee));
    row4->layout()->addWidget(ppReset);
    add(left, row4);
    add(left,
        hint(replayPane() ? QString("<b>Replay trading.</b> Orders fill at the replayed bars' prices in a separate practice "
                                    "account. Step or play the replay to fill limit and stop orders. Exit replay to go "
                                    "back to your live paper account.")
                          : QString("Simulated fills at the live price. No real money, no exchange connection.")),
        12, 12);

    // right: stats, positions, working orders, history (flex column, gap 10)
    split->right->layout()->setSpacing(10);
    QWidget *right = split->right->body();
    auto rh4 = [](const QString &s) {
        Text *t = h4(s);
        t->setContentsMargins(0, 10, 0, 4);
        return t;
    };
    auto *stats = new Stats;
    right->layout()->addWidget(stats);
    right->layout()->addWidget(rh4("Positions"));
    auto *posT = new Table;
    posT->setWrap(true);
    posT->setHeader({{"Symbol"}, {"Side"}, {"Qty"}, {"Avg price"}, {"Last"}, {"P&amp;L"}, {""}});
    right->layout()->addWidget(posT);
    right->layout()->addWidget(rh4("Working orders"));
    auto *ordT = new Table;
    ordT->setWrap(true);
    ordT->setHeader({{"Symbol"}, {"Side"}, {"Type"}, {"Price"}, {"Qty"}, {""}});
    QVector<Row> orows;
    for (const auto &ov : p.value("orders").toArray()) {
        const QJsonObject o = ov.toObject();
        Row r;
        r.cells << Cell {"<b>" + esc(theme::ticker(o.value("symbol").toString())) + "</b>"};
        r.cells << Cell {esc(o.value("side").toString())};
        const QJsonValue oco = o.value("oco");
        r.cells << Cell {esc(o.value("type").toString()) + (oco.isString() && !oco.toString().isEmpty() ? " (bracket)" : "")};
        r.cells << Cell {fmtPrice(numOf(o, "price"))};
        r.cells << Cell {fmtNum(numOf(o, "qty"), 4)};
        Cell b;
        b.button = "Cancel";
        b.data = o.value("id").toString();
        r.cells << b;
        orows << r;
    }
    if (orows.isEmpty()) {
        Cell c;
        c.html = "None.";
        c.cls = "l muted";
        c.colspan = 6;
        orows << Row {{c}, {}};
    }
    ordT->setRows(orows);
    right->layout()->addWidget(ordT);
    right->layout()->addWidget(rh4("History"));
    auto *histT = new Table;
    histT->setWrap(true);
    histT->setHeader({{"Time"}, {"Symbol"}, {"Side"}, {"Qty"}, {"Price"}, {"Realised"}});
    QVector<Row> hrows;
    const QJsonArray history = p.value("history").toArray();
    for (int i = 0; i < history.size() && i < 100; ++i) {
        const QJsonObject h = history[i].toObject();
        const double realized = numOf(h, "realized");
        Row r;
        r.cells << Cell {localeString(numOf(h, "time"))};
        r.cells << Cell {esc(theme::ticker(h.value("symbol").toString()))};
        r.cells << Cell {esc(h.value("side").toString())};
        r.cells << Cell {fmtNum(numOf(h, "qty"), 4)};
        r.cells << Cell {fmtPrice(numOf(h, "price"))};
        Cell c;
        c.html = js::truthy(realized) ? fmtPrice(realized) : QString();
        c.cls = realized >= 0 ? "up" : "down";
        r.cells << c;
        hrows << r;
    }
    histT->setRows(hrows);
    right->layout()->addWidget(histT);

    ppEq = stats;
    ppPos = posT;
    bottom->setContent(split, true);
    drawPaperNumbers();

    QPointer<kit::Select> typeP(ppType);
    QPointer<Input> amtP(ppAmt), slP(ppSl), tpP(ppTp), priceP(ppPrice), initP(ppInit), feeP(ppFee);
    auto place = [this, sym, typeP, amtP, slP, tpP, priceP, split](const QString &side) {
        if (!typeP || !amtP) return;
        const QString type = typeP->value();
        const double amt = amtP->number();
        auto go = [this, sym, type, amt, slP, tpP, priceP, side](double cur) {
            const QString error = placePaperOrder(sym, side, type, amt, priceP ? priceP->number() : 0,
                                                  slP ? slP->number() : 0, tpP ? tpP->number() : 0, cur);
            if (!error.isEmpty()) app->toast(error, "err");
        };
        std::optional<double> cur = price(sym);
        if (!cur && app->active && !app->active->bars.isEmpty()) cur = app->active->bars.last().close;
        if (cur && js::truthy(*cur)) return go(*cur);
        api::quotes(split, {sym},
                    [go, sym](const QJsonValue &v) {
                        const QJsonValue pv = v.toObject().value(sym).toObject().value("price");
                        go(pv.isDouble() ? pv.toDouble() : NaN);
                    },
                    [go](const QString &) { go(NaN); });
    };
    QObject::connect(ppBuy, &QPushButton::clicked, split, [place] { place("buy"); });
    QObject::connect(ppSell, &QPushButton::clicked, split, [place] { place("sell"); });
    QObject::connect(ppReset, &QPushButton::clicked, split, [this, initP, feeP] {
        if (!kit::confirm(app, "Reset the paper account? Positions, orders and history are cleared.")) return;
        const double init = initP ? initP->number() : 0, fee = feeP ? feeP->number() : 0;
        const double start = js::truthy(init) ? init : 100000;
        const QJsonObject acct {{"initial", start}, {"cash", start}, {"feePct", js::orZero(fee)},
                                {"positions", QJsonObject()}, {"orders", QJsonArray()}, {"history", QJsonArray()}};
        if (replayPane()) app->state.replayPaper = acct;
        else app->state.paper = acct;
        app->persist();
        renderPaper();
    });
    QObject::connect(ppFee, &QLineEdit::editingFinished, split, [this, feeP] {
        if (!feeP) return;
        QJsonObject acct = paper();
        acct.insert("feePct", js::orZero(feeP->number()));
        savePaper(acct);
        app->persist();
    });
    QObject::connect(posT, &Table::buttonClicked, split, [this](int, int, const QString &close) {
        if (close.isEmpty()) return;
        const QJsonObject pos = paper().value("positions").toObject().value(close).toObject();
        const double qty = pos.value("qty").toDouble();
        const double px2 = price(close).value_or(pos.value("avg").toDouble());
        fill(close, qty > 0 ? "sell" : "buy", std::abs(qty), px2, "closed");
        QJsonObject acct = paper();
        QJsonArray kept;
        for (const auto &o : acct.value("orders").toArray())
            if (o.toObject().value("symbol").toString() != close) kept.append(o);
        acct.insert("orders", kept);
        savePaper(acct);
        app->persist();
        QMetaObject::invokeMethod(q, [this] { renderPaper(); }, Qt::QueuedConnection);
    });
    QObject::connect(ordT, &Table::buttonClicked, split, [this](int, int, const QString &cancel) {
        if (cancel.isEmpty()) return;
        QJsonObject acct = paper();
        QJsonArray kept;
        for (const auto &o : acct.value("orders").toArray())
            if (o.toObject().value("id").toString() != cancel) kept.append(o);
        acct.insert("orders", kept);
        savePaper(acct);
        app->persist();
        QMetaObject::invokeMethod(q, [this] { renderPaper(); }, Qt::QueuedConnection);
    });
}

QString Panels::Private::placePaperOrder(const QString &sym, const QString &side, const QString &type, double amt,
                                        double entryPrice, double sl, double tp, double cur, const QString &note)
{
    if (!js::truthy(cur) || !(amt > 0)) return "Need a live price and an amount.";
    const double qty = amt / cur;
    const double entry = type == "market" ? cur : entryPrice;
    const QString exitSide = side == "buy" ? "sell" : "buy";
    const double dir = side == "buy" ? 1 : -1;
    if (type != "market" && !(entry > 0)) return "Set a price for the order.";
    auto pushOrder = [this](const QJsonObject &o) {
        QJsonObject acct = paper();
        QJsonArray orders = acct.value("orders").toArray();
        orders.append(o);
        acct.insert("orders", orders);
        savePaper(acct);
    };
    auto addBracket = [&] {
        const QJsonValue oco = sl > 0 && tp > 0 ? QJsonValue("oco" + nowId()) : QJsonValue(QJsonValue::Null);
        if (sl > 0)
            pushOrder({{"id", "o" + nowId() + "s"}, {"symbol", sym}, {"side", exitSide}, {"type", "stop"},
                       {"price", entry * (1 - (dir * sl) / 100)}, {"qty", qty}, {"oco", oco}, {"note", "stop loss"}});
        if (tp > 0)
            pushOrder({{"id", "o" + nowId() + "t"}, {"symbol", sym}, {"side", exitSide}, {"type", "limit"},
                       {"price", entry * (1 + (dir * tp) / 100)}, {"qty", qty}, {"oco", oco}, {"note", "take profit"}});
    };
    if (type == "market") {
        fill(sym, side, qty, cur, note);
        addBracket();
    } else {
        QJsonObject o {{"id", "o" + nowId()}, {"symbol", sym}, {"side", side}, {"type", type}, {"price", entry},
                       {"qty", amt / entry}};
        if (!note.isEmpty()) o.insert("note", note);
        pushOrder(o);
        addBracket();
        app->toast(QString("%1 %2 order placed.").arg(type, side));
    }
    app->persist();
    if (bottomTab == "paper") renderPaper();
    refreshQuotes();
    return {};
}

// Limit and stop orders in the replay account fill when a replayed bar's range reaches them.
void Panels::Private::checkReplayOrders(ChartPane *pane, const Bar &bar)
{
    if (!app->state.replayPaper.isObject()) return;
    const QJsonArray snapshot = app->state.replayPaper.toObject().value("orders").toArray();
    if (snapshot.isEmpty()) return;
    for (const auto &ov : snapshot) {
        const QJsonObject o = ov.toObject();
        if (o.value("symbol").toString() != pane->cfg.symbol) continue;
        const QString type = o.value("type").toString(), side = o.value("side").toString();
        const double op = numOf(o, "price");
        std::optional<double> px;
        if (type == "limit" && side == "buy" && bar.low <= op) px = js::min(op, bar.open);
        if (type == "limit" && side == "sell" && bar.high >= op) px = js::max(op, bar.open);
        if (type == "stop" && side == "buy" && bar.high >= op) px = js::max(op, bar.open);
        if (type == "stop" && side == "sell" && bar.low <= op) px = js::min(op, bar.open);
        if (!px) continue;
        QJsonObject p = app->state.replayPaper.toObject();
        p.insert("orders", dropFilled(p.value("orders").toArray(), o));
        app->state.replayPaper = p;
        const QString note = o.value("note").toString();
        fill(o.value("symbol").toString(), side, numOf(o, "qty"), *px, !note.isEmpty() ? note : type + " order");
    }
}

} // namespace qtm
