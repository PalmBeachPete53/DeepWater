// The local models' Trade skill (Deepwater's AI › Local, src/LocalTools.cpp): the strategy tester and the live paper
// account driven through the panels as a click would drive them. The tester shows the run and marks the chart; the
// paper panel shows the fills and orders. Nothing here reaches an exchange: the paper account is simulated, and the
// PTQ connector (real execution) is not touched.

#include "PanelsPrivate.h"

#include "Api.h"
#include "App.h"
#include "ChartPane.h"
#include "../core/Backtest.h"
#include "../core/Indicators.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QPointer>
#include <QTimer>

#include <cmath>

namespace qtm {

namespace {

const QStringList RULE_OPS = {">", "<", ">=", "<=", "crossesAbove", "crossesBelow", "crosses"};
const QStringList AGENT_PRICE_FIELDS = {"open", "high", "low", "close", "volume", "hl2", "hlc3", "ohlc4", "hlcc4"};
const QStringList RULE_GROUPS = {"entryLong", "exitLong", "entryShort", "exitShort"};

QJsonObject error(const QString &message)
{
    return {{"error", message}};
}

double tidy(double v)
{
    return std::isfinite(v) ? QString::number(v, 'g', 6).toDouble() : 0;
}

QString isoDate(double seconds)
{
    return QDateTime::fromMSecsSinceEpoch(qint64(seconds * 1000)).toString("yyyy-MM-dd HH:mm");
}

// Checks a strategy's rules against the operators and the indicators it declares: a clear message for the model
// instead of a tester error.
QString checkStrategy(const QJsonObject &s)
{
    bool entries = false;
    const QJsonObject inds = s.value("indicators").toObject();
    QStringList ids;
    for (const auto &def : indicators()) ids << def->id;
    for (auto it = inds.begin(); it != inds.end(); ++it) {
        const QString id = it.value().toObject().value("id").toString();
        if (!ids.contains(id) && !id.startsWith("custom:"))
            return QString("indicator \"%1\" has an unknown id \"%2\" (see backtest_help)").arg(it.key(), id);
    }
    auto operandOk = [&](const QJsonValue &v) -> QString {
        if (v.isDouble()) return {};
        if (!v.isString()) return "an operand must be a number or a name";
        const QString name = v.toString();
        if (AGENT_PRICE_FIELDS.contains(name) || inds.contains(name.section('.', 0, 0))) return {};
        return QString("operand \"%1\" is neither a price field nor a key of \"indicators\"").arg(name);
    };
    for (const QString &g : RULE_GROUPS) {
        const QJsonArray rules = s.value(g).toArray();
        if (g.startsWith("entry") && !rules.isEmpty()) entries = true;
        for (const QJsonValue &rv : rules) {
            const QJsonObject r = rv.toObject();
            if (!RULE_OPS.contains(r.value("op").toString()))
                return QString("%1: operator \"%2\" isn't one of %3").arg(g, r.value("op").toString(), RULE_OPS.join(", "));
            for (const char *side : {"left", "right"})
                if (const QString e = operandOk(r.value(side)); !e.isEmpty()) return g + ": " + e;
        }
    }
    if (!entries) return "the strategy needs at least one rule in entryLong or entryShort";
    return {};
}

} // namespace

QJsonObject Panels::agentStrategyHelp() const
{
    // One line per indicator, short enough for a small model's context: "macd(fast=12, slow=26, signal=9) → macd|signal|hist".
    QStringList inds;
    for (const auto &def : indicators()) {
        QStringList params;
        for (const auto &in : def->inputs)
            if (in.type == "int" || in.type == "float" || in.type == "select")
                params << in.key + '=' + (in.def.isString() ? in.def.toString() : QString::number(in.def.toDouble()));
        QStringList outputs;
        for (const auto &o : def->outputs) outputs << o.key;
        QString line = def->id + '(' + params.join(", ") + ')';
        if (outputs.size() > 1) line += QString::fromUtf8(" → ") + outputs.join('|');
        inds << line;
    }
    QJsonArray examples;
    for (const auto &e : exampleStrategies()) {
        QJsonObject o = e.second;
        o.insert("name", e.first);
        examples.append(o);
    }
    return {
        {"format", "JSON: {indicators: {key: {id, params, output?}}, entryLong: [rules], exitLong: [rules], entryShort: "
                   "[rules], exitShort: [rules], stopLossPct, takeProfitPct, trailingStopPct, maxBarsInTrade, "
                   "exitMatch: all|any, initialCapital, positionPct, feePct, slippagePct}. A rule is {left, op, right}; "
                   "all the rules of a group must hold. An operand is a number, a price field, an indicator key, or "
                   "key.output for an indicator with several outputs. Signals are read on a bar's close and filled at "
                   "the next bar's open."},
        {"operators", QJsonArray::fromStringList(RULE_OPS)},
        {"price_fields", QJsonArray::fromStringList(AGENT_PRICE_FIELDS)},
        {"timeframes", "1m, 5m, 15m, 30m, 1h, 4h, 1D, 1W, 1M"},
        {"examples", examples},
        {"saved_strategies", QJsonArray::fromStringList(d->app->state.strategies.keys())},
        {"indicators", QJsonArray::fromStringList(inds)},
    };
}

void Panels::agentBacktest(const QString &symbol, const QString &tf, const QJsonValue &strategyArg, AgentDone done)
{
    App *app = d->app;
    if (!app->isReady() || !app->active) return done(error("the Charts tab isn't ready yet"));
    QJsonObject strategy;
    QString name = "Local model";
    if (strategyArg.isString()) {
        const QString wanted = strategyArg.toString().trimmed();
        for (const auto &e : exampleStrategies())
            if (e.first.compare(wanted, Qt::CaseInsensitive) == 0) strategy = e.second, name = e.first;
        for (const QString &k : app->state.strategies.keys())
            if (k.compare(wanted, Qt::CaseInsensitive) == 0) strategy = app->state.strategies.value(k), name = k;
        if (strategy.isEmpty()) return done(error("no example or saved strategy named \"" + wanted + "\" (see backtest_help)"));
    } else if (strategyArg.isObject()) {
        strategy = strategyArg.toObject();
        if (strategy.value("name").isString()) name = strategy.value("name").toString();
    } else {
        return done(error("\"strategy\" must be a strategy object or the name of an example or saved strategy"));
    }
    if (const QString why = checkStrategy(strategy); !why.isEmpty()) return done(error(why));

    ChartPane *pane = app->active;
    const QString sym = symbol.trimmed().isEmpty() ? pane->cfg.symbol : symbol.trimmed();
    const QString frame = tf.trimmed().isEmpty() ? pane->cfg.tf : tf.trimmed();
    const bool move = sym != pane->cfg.symbol || frame != pane->cfg.tf;
    if (move && pane->replay) return done(error("the active chart is in bar replay: stop the replay to test another symbol"));

    QPointer<Panels> self(this);
    auto run = [self, strategy, name, done] {
        if (!self) return;
        Private *p = self->d.get();
        p->applyStrategy(strategy, "AI · " + name);
        self->showBottom("tester");
        p->renderTester();
        p->testerWaiters << [self, done](const QString &why) {
            if (!self) return;
            if (!why.isEmpty()) return done(error(why));
            const panels::TesterResult &r = *self->d->lastResult;
            const QJsonObject s = r.result.value("stats").toObject();
            QJsonObject stats;
            for (const char *k : {"netProfitPct", "buyHoldPct", "maxDrawdownPct", "totalTrades", "winRatePct",
                                  "profitFactor", "sharpe", "sortino", "avgTradePct", "exposurePct"})
                if (s.value(k).isDouble()) stats.insert(k, tidy(s.value(k).toDouble()));
            const QJsonArray trades = r.result.value("trades").toArray();
            QJsonArray last;
            for (qsizetype i = std::max<qsizetype>(0, trades.size() - 8); i < trades.size(); ++i) {
                const QJsonObject t = trades[i].toObject();
                last.append(QJsonObject {{"side", t.value("side")}, {"entry", isoDate(t.value("entryTime").toDouble())},
                                         {"exit", isoDate(t.value("exitTime").toDouble())},
                                         {"entryPrice", tidy(t.value("entryPrice").toDouble())},
                                         {"exitPrice", tidy(t.value("exitPrice").toDouble())},
                                         {"pnlPct", tidy(t.value("pnlPct").toDouble())}, {"exitReason", t.value("exitReason")}});
            }
            done({{"symbol", r.symbol}, {"tf", r.tf}, {"bars", int(r.bars.size())},
                  {"from", isoDate(r.bars.first().time)}, {"to", isoDate(r.bars.last().time)},
                  {"stats", stats}, {"trades", int(trades.size())}, {"last_trades", last},
                  {"note", "shown in the Charts tab's strategy tester; backtest_trades_csv exports every trade"}});
        };
        p->runTester();
    };
    if (!move) return run();

    // Switch the active chart as the toolbar would, then run once that load has ended (the provider may write the
    // symbol its own way: BTCUSDT → BINANCE:BTCUSDT).
    auto finished = std::make_shared<bool>(false);
    QPointer<ChartPane> target(pane);
    if (sym != pane->cfg.symbol) {
        pane->cfg.tf = frame;
        app->setSymbol(sym);
    } else {
        app->setTf(frame);
    }
    const int token = pane->currentLoad();
    app->paneLoadedListeners << [target, token, sym, frame, run, done, finished](ChartPane *loaded, const QString &why) {
        if (*finished || !target) return true;
        if (target->currentLoad() != token) {   // the user (or another call) switched the chart meanwhile
            *finished = true;
            done(error("the active chart was switched to something else meanwhile"));
            return true;
        }
        if (loaded != target) return false;
        *finished = true;
        if (!why.isEmpty()) done(error(QString("the chart couldn't load %1 on %2: %3").arg(sym, frame, why)));
        else if (target->bars.isEmpty()) done(error(QString("no data for %1 on %2").arg(sym, frame)));
        else QTimer::singleShot(0, [run] { run(); });   // after the pane's own handlers
        return true;
    };
    QTimer::singleShot(30000, this, [finished, done, sym, frame] {
        if (*finished) return;
        *finished = true;
        done(error(QString("the chart couldn't load %1 on %2 within 30 s").arg(sym, frame)));
    });
}

QJsonObject Panels::agentTradesCsv(const QString &path) const
{
    if (!d->lastResult) return error("no backtest has run yet in the tester");
    const panels::TesterResult &r = *d->lastResult;
    const QJsonArray trades = r.result.value("trades").toArray();
    const QString csv = tradesCsv(trades);
    QJsonObject out {{"symbol", r.symbol}, {"tf", r.tf}, {"rows", int(trades.size())}};
    // The figures of every trade, worked out here: a small model reading a long CSV miscounts.
    int wins = 0, losses = 0;
    double winSum = 0, lossSum = 0, best = -INFINITY, worst = INFINITY;
    for (const QJsonValue &v : trades) {
        const double pct = v.toObject().value("pnlPct").toDouble();
        if (pct > 0) ++wins, winSum += pct;
        else ++losses, lossSum += pct;
        best = std::max(best, pct);
        worst = std::min(worst, pct);
    }
    if (!trades.isEmpty())
        out.insert("summary", QJsonObject {{"trades", int(trades.size())}, {"winning", wins}, {"losing", losses},
                                           {"avgWinPct", wins ? tidy(winSum / wins) : 0},
                                           {"avgLossPct", losses ? tidy(lossSum / losses) : 0},
                                           {"bestPct", tidy(best)}, {"worstPct", tidy(worst)}});
    if (!path.isEmpty()) {
        QDir().mkpath(QFileInfo(path).path());
        QFile f(path);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            f.write(csv.toUtf8());
            out.insert("file", path);
        }
    }
    out.insert("csv", csv);
    return out;
}

QJsonObject Panels::agentPaperAccount()
{
    const QJsonObject p = d->livePaper();
    const QJsonObject positions = p.value("positions").toObject();
    double held = 0;
    QJsonArray pos;
    for (auto it = positions.begin(); it != positions.end(); ++it) {
        const double qty = it.value().toObject().value("qty").toDouble();
        const double avg = it.value().toObject().value("avg").toDouble();
        const bool live = d->app->prices.contains(it.key());
        const double last = live ? d->app->prices.value(it.key()) : avg;
        held += qty * last;
        pos.append(QJsonObject {{"symbol", it.key()}, {"side", qty > 0 ? "long" : "short"}, {"qty", tidy(std::abs(qty))},
                                {"avgPrice", tidy(avg)}, {"lastPrice", tidy(last)}, {"livePrice", live},
                                {"pnl", tidy(qty * (last - avg))}});
    }
    QJsonArray orders;
    for (const QJsonValue &v : p.value("orders").toArray()) {
        const QJsonObject o = v.toObject();
        orders.append(QJsonObject {{"id", o.value("id")}, {"symbol", o.value("symbol")}, {"side", o.value("side")},
                                   {"type", o.value("type")}, {"price", tidy(o.value("price").toDouble())},
                                   {"qty", tidy(o.value("qty").toDouble())}, {"note", o.value("note")}});
    }
    QJsonArray fills;
    const QJsonArray history = p.value("history").toArray();
    for (int i = 0; i < history.size() && i < 10; ++i) {
        const QJsonObject h = history[i].toObject();
        fills.append(QJsonObject {{"time", isoDate(h.value("time").toDouble() / 1000)}, {"symbol", h.value("symbol")},
                                  {"side", h.value("side")}, {"qty", tidy(h.value("qty").toDouble())},
                                  {"price", tidy(h.value("price").toDouble())},
                                  {"realized", tidy(h.value("realized").toDouble())}});
    }
    const double cash = p.value("cash").toDouble(), initial = p.value("initial").toDouble();
    const double equity = cash + held;
    return {{"account", "live paper account (simulated: no real money, no exchange)"},
            {"initial", tidy(initial)}, {"cash", tidy(cash)}, {"equity", tidy(equity)},
            {"pnl", tidy(equity - initial)}, {"feePct", p.value("feePct")}, {"positions", pos},
            {"working_orders", orders}, {"latest_fills", fills}};
}

void Panels::agentPaperOrder(const QJsonObject &o, AgentDone done)
{
    if (d->replayPane())
        return done(error("the active chart is in bar replay, where orders go to the practice account; a local model "
                          "trades the live paper account only: stop the replay first"));
    const QString sym = o.value("symbol").toString().trimmed();
    const QString side = o.value("side").toString().toLower();
    const QString type = o.value("type").toString("market").toLower();
    const double amount = o.value("amount").toDouble();
    const double price = o.value("price").toDouble();
    const double sl = o.value("stop_loss_pct").toDouble(), tp = o.value("take_profit_pct").toDouble();
    if (sym.isEmpty()) return done(error("\"symbol\" is required"));
    if (side != "buy" && side != "sell") return done(error("\"side\" is buy or sell"));
    if (type != "market" && type != "limit" && type != "stop") return done(error("\"type\" is market, limit or stop"));
    if (!(amount > 0)) return done(error("\"amount\" (in the account's currency) must be above 0"));
    if (type != "market" && !(price > 0)) return done(error("a " + type + " order needs a \"price\""));
    if (sl < 0 || tp < 0 || sl >= 100) return done(error("stop_loss_pct and take_profit_pct are percentages above 0"));
    const double equity = agentPaperAccount().value("equity").toDouble();
    if (amount > equity)
        return done(error(QString("%1 is more than the account's equity (%2)").arg(amount).arg(tidy(equity))));

    QPointer<Panels> self(this);
    auto go = [self, sym, side, type, amount, price, sl, tp, done](double cur) {
        if (!self) return;
        if (!(cur > 0)) return done(error("no live price for " + sym + ": check the symbol (as the Charts tab writes it)"));
        const QString why = self->d->placePaperOrder(sym, side, type, amount, price, sl, tp, cur, "local model");
        if (!why.isEmpty()) return done(error(why));
        QJsonObject out {{"ok", true}, {"symbol", sym}, {"side", side}, {"type", type}, {"amount", amount}};
        if (type == "market") {
            out.insert("filled_at", tidy(cur));
            out.insert("qty", tidy(amount / cur));
            out.insert("done", QString("%1 %2 units of %3 at %4, worth %5 in the account's currency")
                                   .arg(side == "buy" ? "bought" : "sold").arg(tidy(amount / cur)).arg(sym)
                                   .arg(tidy(cur)).arg(amount));
        } else {
            out.insert("working_at", tidy(price));
            out.insert("last_price", tidy(cur));
            out.insert("done", QString("a %1 %2 order for %3 worth %4 waits at %5")
                                   .arg(type, side, sym).arg(amount).arg(tidy(price)));
        }
        if (sl > 0) out.insert("stop_loss_pct", sl);
        if (tp > 0) out.insert("take_profit_pct", tp);
        done(out);
    };
    App *app = d->app;
    if (app->prices.contains(sym) && app->prices.value(sym) > 0) return go(app->prices.value(sym));
    if (app->active && app->active->cfg.symbol == sym && !app->active->bars.isEmpty())
        return go(app->active->bars.last().close);
    api::quotes(this, {sym},
                [go, sym](const QJsonValue &v) {
                    const QJsonValue pv = v.toObject().value(sym).toObject().value("price");
                    go(pv.isDouble() ? pv.toDouble() : NAN);
                },
                [go](const QString &) { go(NAN); });
}

QJsonObject Panels::agentPaperClose(const QString &symbol)
{
    if (d->replayPane()) return error("the active chart is in bar replay: stop the replay first");
    const QString sym = symbol.trimmed();
    const QJsonObject pos = d->livePaper().value("positions").toObject().value(sym).toObject();
    const double qty = pos.value("qty").toDouble();
    if (qty == 0) return error("no open position in " + sym);
    const bool live = d->app->prices.contains(sym);
    const double px = live ? d->app->prices.value(sym) : pos.value("avg").toDouble();
    d->fill(sym, qty > 0 ? "sell" : "buy", std::abs(qty), px, "closed by local model", true);
    QJsonObject acct = d->livePaper();
    QJsonArray kept;
    for (const auto &o : acct.value("orders").toArray())
        if (o.toObject().value("symbol").toString() != sym) kept.append(o);
    acct.insert("orders", kept);
    d->app->state.paper = acct;
    d->app->persist();
    if (d->bottomTab == "paper") d->renderPaper();
    QJsonObject out {{"ok", true}, {"symbol", sym}, {"closed_qty", tidy(std::abs(qty))}, {"price", tidy(px)},
                     {"pnl", tidy(qty * (px - pos.value("avg").toDouble()))}};
    if (!live) out.insert("note", "no live price: closed at the average price, as the panel does");
    return out;
}

QJsonObject Panels::agentPaperCancel(const QString &orderId)
{
    if (d->replayPane()) return error("the active chart is in bar replay: stop the replay first");
    QJsonObject acct = d->livePaper();
    QJsonArray kept;
    bool found = false;
    for (const auto &o : acct.value("orders").toArray()) {
        if (o.toObject().value("id").toString() == orderId) found = true;
        else kept.append(o);
    }
    if (!found) return error("no working order with id " + orderId + " (see paper_account)");
    acct.insert("orders", kept);
    d->app->state.paper = acct;
    d->app->persist();
    if (d->bottomTab == "paper") d->renderPaper();
    return {{"ok", true}, {"cancelled", orderId}};
}

} // namespace qtm
