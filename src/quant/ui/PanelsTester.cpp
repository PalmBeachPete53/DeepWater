// The strategy tester of panels.js (lines 548-866): presets, saved strategies, the rule groups,
// settings, live spread, CSV imports, Save / Edit as JSON, running on the active chart (with the
// bar magnifier), and the result: stats, equity chart with buy & hold, quant checks, PTQ check,
// trades table, CSV export and the trade marks on the chart.

#include "PanelsPrivate.h"

#include "Api.h"
#include "Ptq.h"
#include "QuantUi.h"
#include "Theme.h"
#include "../chart/Chart.h"
#include "../core/Backtest.h"
#include "../core/Error.h"
#include "../core/JsCompat.h"
#include "../core/Series.h"
#include "../server/Json.h"

#include <QApplication>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <cmath>
#include <functional>
#include <utility>

namespace qtm {

using namespace qtm::kit;
using theme::esc;
using theme::fmtPrice;
using theme::ticker;

namespace {

const QStringList GROUPS = {"entryLong", "exitLong", "entryShort", "exitShort"};
const QStringList MONEY = {"initialCapital", "positionPct", "feePct", "slippagePct", "spreadPct", "intrabar"};

// `${v}` of a JSON value as the template literal shows it.
QString show(const QJsonValue &v)
{
    if (v.isDouble()) return js::numberToString(v.toDouble());
    if (v.isString()) return v.toString();
    if (v.isBool()) return v.toBool() ? "true" : "false";
    if (v.isNull()) return "null";
    if (v.isUndefined()) return "undefined";
    return "[object Object]";
}

// JS `v >= 0` for a JSON value (null counts as 0, undefined and NaN are false).
bool geZero(const QJsonValue &v)
{
    if (v.isNull()) return true;
    if (v.isDouble()) return v.toDouble() >= 0;
    if (v.isString()) {
        bool ok = false;
        const double d = v.toString().trimmed().isEmpty() ? 0 : v.toString().toDouble(&ok);
        return (v.toString().trimmed().isEmpty() || ok) && d >= 0;
    }
    if (v.isBool()) return true;
    return false;
}

bool nullish(const QJsonValue &v)
{
    return v.isNull() || v.isUndefined();
}

// The SyntaxError message of JSON.parse(s) as the browser (V8) words it, or an empty string when
// `s` is valid JSON. Positions count UTF-16 units like JS; line breaks are \n, \r and \r\n.
QString jsonParseError(const QString &s)
{
    const int n = s.size();
    int pos = 0;
    QString err;
    auto where = [&](int p, const char *in = " in JSON") {
        int line = 1, start = 0;
        for (int i = 0; i < p && i < n; ++i)
            if (s[i] == '\n' || s[i] == '\r') {
                if (s[i] == '\r' && i + 1 < p && s[i + 1] == '\n') ++i;
                ++line;
                start = i + 1;
            }
        return QString("%1 at position %2 (line %3 column %4)").arg(QLatin1String(in)).arg(p).arg(line).arg(p - start + 1);
    };
    // ReportUnexpectedToken: a given message wins, then end of input / number, then the token in context.
    auto fail = [&](int p, const QString &message = QString(), bool number = false) {
        if (!err.isEmpty()) return false;
        if (!message.isEmpty()) err = message + where(p, message.startsWith("Unexpected non") ? "" : " in JSON");
        else if (p >= n) err = QStringLiteral("Unexpected end of JSON input");
        else if (number || s[p] == '-' || (s[p] >= '0' && s[p] <= '9')) err = "Unexpected number" + where(p);
        else if (s[p] == '"') err = "Unexpected string" + where(p);
        else if (s == "NaN" || s == "Infinity" || s == "undefined" || s == "[object Object]")
            err = QString("\"%1\" is not valid JSON").arg(s);
        else {
            const QString c = s.mid(p, 1);
            if (n < 21) err = QString("Unexpected token '%1', \"%2\" is not valid JSON").arg(c, s);
            else if (p < 10) err = QString("Unexpected token '%1', \"%2\"... is not valid JSON").arg(c, s.left(p + 10));
            else if (p < n - 10)
                err = QString("Unexpected token '%1', ...\"%2\"... is not valid JSON").arg(c, s.mid(p - 10, 20));
            else err = QString("Unexpected token '%1', ...\"%2\" is not valid JSON").arg(c, s.mid(p - 10));
        }
        return false;
    };
    auto ws = [&] {
        while (pos < n && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\n' || s[pos] == '\r')) ++pos;
    };
    auto digit = [&](int p) { return p < n && s[p] >= '0' && s[p] <= '9'; };
    auto str = [&]() -> bool {   // at the opening quote
        ++pos;
        while (true) {
            if (pos >= n) return fail(pos, "Unterminated string");
            const QChar c = s[pos];
            if (c == '"') {
                ++pos;
                return true;
            }
            if (c.unicode() < 0x20) return fail(pos, "Bad control character in string literal");
            if (c == '\\') {
                ++pos;
                if (pos >= n) return fail(pos);
                const QChar e = s[pos];
                if (e == 'u') {
                    for (int k = 1; k <= 4; ++k) {
                        if (pos + k >= n) return fail(pos + k, "Bad Unicode escape");
                        const QChar h = s[pos + k];
                        if (!((h >= '0' && h <= '9') || (h >= 'a' && h <= 'f') || (h >= 'A' && h <= 'F')))
                            return fail(pos + k, "Bad Unicode escape");
                    }
                    pos += 5;
                    continue;
                }
                if (!QStringLiteral("\"\\/bfnrt").contains(e)) return fail(pos, "Bad escaped character");
                ++pos;
                continue;
            }
            ++pos;
        }
    };
    std::function<bool()> value = [&]() -> bool {
        ws();
        if (pos >= n) return fail(pos);
        const QChar c = s[pos];
        if (c == '"') return str();
        if (c == '-' || digit(pos)) {
            if (c == '-') {
                ++pos;
                if (!digit(pos)) return fail(pos, "No number after minus sign");
            }
            if (s[pos] == '0') {
                ++pos;
                if (digit(pos)) return fail(pos, QString(), true);
            } else
                while (digit(pos)) ++pos;
            if (pos < n && s[pos] == '.') {
                ++pos;
                if (!digit(pos)) return fail(pos, "Unterminated fractional number");
                while (digit(pos)) ++pos;
            }
            if (pos < n && (s[pos] == 'e' || s[pos] == 'E')) {
                ++pos;
                if (pos < n && (s[pos] == '+' || s[pos] == '-')) ++pos;
                if (!digit(pos)) return fail(pos, "Exponent part is missing a number");
                while (digit(pos)) ++pos;
            }
            return true;
        }
        for (const char *lit : {"true", "false", "null"}) {
            if (c != QChar(lit[0])) continue;
            for (int k = 0; lit[k]; ++k, ++pos)
                if (pos >= n || s[pos] != QChar(lit[k])) return fail(pos);
            return true;
        }
        if (c == '[') {
            ++pos;
            ws();
            if (pos < n && s[pos] == ']') {
                ++pos;
                return true;
            }
            while (true) {
                if (!value()) return false;
                ws();
                if (pos < n && s[pos] == ',') {
                    ++pos;
                    continue;
                }
                if (pos < n && s[pos] == ']') {
                    ++pos;
                    return true;
                }
                return fail(pos, "Expected ',' or ']' after array element");
            }
        }
        if (c == '{') {
            ++pos;
            ws();
            if (pos < n && s[pos] == '}') {
                ++pos;
                return true;
            }
            if (pos >= n || s[pos] != '"') return fail(pos, "Expected property name or '}'");
            while (true) {
                if (!str()) return false;
                ws();
                if (pos >= n || s[pos] != ':') return fail(pos, "Expected ':' after property name");
                ++pos;
                if (!value()) return false;
                ws();
                if (pos < n && s[pos] == ',') {
                    ++pos;
                    ws();
                    if (pos >= n || s[pos] != '"') return fail(pos, "Expected double-quoted property name");
                    continue;
                }
                if (pos < n && s[pos] == '}') {
                    ++pos;
                    return true;
                }
                return fail(pos, "Expected ',' or '}' after property value");
            }
        }
        return fail(pos);
    };
    if (value()) {
        ws();
        if (pos < n) fail(pos, "Unexpected non-whitespace character after JSON");
    }
    return err;
}

// The equity box (.equity: height 150, border, radius 6). In the browser it is a flex item of the
// result column, which is always taller than the panel, so it shrinks to its minimum height (the
// chart's time axis). EquityBox reproduces that: 150 px minus the column's overflow, at least 30.
class EquityBox : public Box
{
public:
    explicit EquityBox(QWidget *parent = nullptr) : Box(parent)
    {
        bg = QColor(0, 0, 0, 0);
        radius = 6;
        setFixedHeight(150);
    }
    lw::Chart *chart = nullptr;

protected:
    void resizeEvent(QResizeEvent *) override
    {
        if (chart) chart->setGeometry(rect().adjusted(1, 1, -1, -1));
    }
};

} // namespace

// ---------------------------------------------------------------- state

void Panels::Private::initTester()
{
    tState.settings = {
        {"direction", "long"}, {"stopLossPct", 0},  {"takeProfitPct", 0}, {"trailingStopPct", 0},
        {"exitMatch", "all"},  {"maxBarsInTrade", 0}, {"initialCapital", 10000}, {"positionPct", 100},
        {"feePct", 0.1},       {"slippagePct", 0.05}, {"spreadPct", 0},   {"intrabar", "worst"},
    };
    for (const auto &ex : exampleStrategies())
        if (ex.first == "EMA cross (9/21)") strategyToUi(ex.second);
}

void Panels::Private::strategyToUi(const QJsonObject &s)
{
    const QJsonObject ind = s.value("indicators").toObject();
    tState.groups.clear();
    for (const QString &k : GROUPS) {
        QVector<rules::Rule> rs;
        for (const auto &rv : s.value(k).toArray()) {
            const QJsonObject r = rv.toObject();
            rules::Rule rule;
            rule.left = rules::toOperand(r.value("left"), ind);
            rule.op = r.value("op").toString();
            rule.right = rules::toOperand(r.value("right"), ind);
            rs << rule;
        }
        tState.groups.push_back({k, rs});
    }
    for (const QString &k : {QString("stopLossPct"), QString("takeProfitPct"), QString("trailingStopPct"),
                             QString("maxBarsInTrade")}) {
        const QJsonValue v = s.value(k);
        const bool truthy = v.isDouble() ? js::truthy(v.toDouble()) : v.isString() ? !v.toString().isEmpty()
                                                                                    : v.isBool() ? v.toBool()
                                                                                                 : (v.isObject() || v.isArray());
        tState.set(k, truthy ? v : QJsonValue(0));
    }
    tState.set("exitMatch", s.value("exitMatch").toString() == "any" ? "any" : "all");
}

QJsonObject Panels::Private::readTester(QStringList *order)
{
    QVector<QPair<QString, QVector<rules::Rule>>> groups;
    for (const QString &k : GROUPS) {
        QVector<rules::Rule> rs;
        if (auto box = testerGroups.value(k)) rs = box->read();
        else
            for (const auto &g : tState.groups)
                if (g.first == k) rs = g.second;
        groups.push_back({k, rs});
    }
    tState.groups = groups;
    for (auto it = testerInputs.begin(); it != testerInputs.end(); ++it) {
        if (!it.value()) continue;
        if (auto *in = qobject_cast<Input *>(it.value().data())) tState.set(it.key(), in->number());
        else if (auto *sel = qobject_cast<kit::Select *>(it.value().data())) tState.set(it.key(), sel->value());
    }
    const rules::Compiled c = rules::compileRules(groups);
    QJsonObject out;
    out.insert("indicators", c.indicators);
    for (const auto &g : c.groups) out.insert(g.first, g.second);
    for (const auto &s : tState.settings) out.insert(s.first, s.second);
    if (order) *order = c.indicatorOrder;
    return out;
}

// The JS key order of readTester()'s object, for the JSON modal.
static QHash<QString, QStringList> strategyOrder(const QStringList &indicatorOrder, const panels::TesterState &st)
{
    QStringList top = {"indicators"};
    top << GROUPS;
    for (const auto &s : st.settings) top << s.first;
    QHash<QString, QStringList> o;
    o.insert("", top);
    o.insert("indicators", indicatorOrder);
    for (const QString &k : indicatorOrder) o.insert("indicators." + k, {"id", "params"});
    for (const QString &g : GROUPS) {
        o.insert(g + "[]", {"left", "op", "right"});
        o.insert(g + "[].left", {"line"});
        o.insert(g + "[].right", {"line"});
        o.insert(g + "[].left.line", {"t1", "p1", "t2", "p2", "extend"});
        o.insert(g + "[].right.line", {"t1", "p1", "t2", "p2", "extend"});
    }
    return o;
}

void Panels::Private::applyStrategy(const QJsonObject &s, const QString &name)
{
    strategyToUi(s);
    if (!name.isNull()) tState.name = name;
    for (const QString &k : MONEY)
        if (!nullish(s.value(k))) tState.set(k, s.value(k));
}

// ---------------------------------------------------------------- render

void Panels::Private::renderTester()
{
    auto *split = new Split;
    testerRoot = split;
    testerGroups.clear();
    testerInputs.clear();
    QWidget *left = split->left->body();
    auto *ll = split->left->layout();

    // preset + run
    auto *top = rowFlex();
    auto *preset = new kit::Select(kit::Select::In);
    preset->addOption("", QString::fromUtf8("Load a strategy…"));
    preset->addGroup("Examples");
    for (const auto &ex : exampleStrategies()) preset->addOption("ex:" + ex.first, ex.first);
    const QStringList saved = app->state.strategies.keys();
    if (!saved.isEmpty()) {
        preset->endGroup();
        preset->addGroup("Saved");
        for (const QString &n : saved) preset->addOption("sv:" + n, n);
    }
    preset->setValue("");
    auto *run = btn("Run on chart", "btn primary");
    top->layout()->addWidget(preset);
    top->layout()->addWidget(run);
    ll->addWidget(top);
    add(left, hint("Entry conditions must all be true. Exits can need all of theirs or just one. Leave a section empty "
                   "to skip it. Signals use the bar's close and fill at the next bar's open."),
        12, 0);

    auto num = [this](const QString &k, const QString &label) {
        auto *in = new Input(Input::Field, true);
        in->setText(show(tState.setting(k)));
        testerInputs.insert(k, in);
        return field(label, in);
    };
    const QVector<QPair<QString, QString>> titles = {{"entryLong", "Buy (enter long) when"},
                                                     {"exitLong", "Sell (exit long) when"},
                                                     {"entryShort", "Enter short when"},
                                                     {"exitShort", "Exit short when"}};
    int pendingMargin = 12;   // the hint's margin-bottom, collapsing with the next h4's 10
    for (const auto &[k, title] : titles) {
        add(left, h4(title), qMax(pendingMargin, 10), 4);
        auto *box = new rules::RulesBox;
        for (const auto &g : tState.groups)
            if (g.first == k)
                for (const auto &r : g.second) box->addRule(r);
        testerGroups.insert(k, box);
        add(left, box, 0, 10);
        auto *addB = btn("+ Condition", "btn small");
        auto *line = new QWidget;   // the anonymous line box around the inline button
        auto *hl = new QHBoxLayout(line);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->addWidget(addB);
        hl->addStretch(1);
        ll->addWidget(line);
        QPointer<rules::RulesBox> bp(box);
        QObject::connect(addB, &QPushButton::clicked, line, [bp] {
            if (bp) bp->addRule(rules::defaultRule());
        });
        pendingMargin = 10;
    }
    // Exits
    add(left, h4("Exits"), 10, 4);
    {
        auto *row = rowFlex();
        auto *sel = new kit::Select(kit::Select::Field);
        sel->addOption("all", "all exit conditions are true");
        sel->addOption("any", "any one exit condition is true");
        sel->setValue(show(tState.setting("exitMatch")) == "any" ? "any" : "all");
        testerInputs.insert("exitMatch", sel);
        row->layout()->addWidget(field("Exit when", sel));
        row->layout()->addWidget(num("maxBarsInTrade", "Exit after this many bars (0 = off)"));
        ll->addWidget(row);
    }
    add(left, h4("Risk"), 10, 4);
    {
        auto *row = rowFlex();
        row->layout()->addWidget(num("stopLossPct", "Stop loss %"));
        row->layout()->addWidget(num("takeProfitPct", "Take profit %"));
        row->layout()->addWidget(num("trailingStopPct", "Trailing stop %"));
        ll->addWidget(row);
    }
    add(left, h4("Money"), 10, 4);
    QPushButton *spreadB = nullptr;
    {
        auto *row = rowFlex();
        row->layout()->addWidget(num("initialCapital", "Starting capital"));
        row->layout()->addWidget(num("positionPct", "Position size %"));
        row->layout()->addWidget(num("feePct", "Fee % per side"));
        row->layout()->addWidget(num("slippagePct", "Slippage % per side"));
        row->layout()->addWidget(num("spreadPct", "Spread % (bid to ask)"));
        spreadB = btn("Live spread", "btn small");
        spreadB->setToolTip("Read the live bid and ask (crypto)");
        row->layout()->addWidget(spreadB);
        ll->addWidget(row);
    }
    add(left, h4("Fills inside a bar"), 10, 4);
    {
        auto *row = rowFlex();
        auto *sel = new kit::Select(kit::Select::Field);
        sel->addOption("worst", "Stop first when a bar touches both (cautious)");
        sel->addOption("ohlc", "TradingView's path: open, nearer extreme, far extreme, close");
        sel->addOption("magnifier", "Bar magnifier: real lower-timeframe bars inside each bar");
        sel->setValue(show(tState.setting("intrabar")));
        testerInputs.insert("intrabar", sel);
        row->layout()->addWidget(field("Stops and targets", sel));
        ll->addWidget(row);
    }
    add(left, h4("Your own data"), 10, 4);
    auto *impBars = btn("Import bars (CSV)", "btn small");
    auto *impSig = btn("Import signal (CSV)", "btn small");
    {
        auto *row = rowFlex();
        row->layout()->addWidget(impBars);
        row->layout()->addWidget(impSig);
        ll->addWidget(row);
    }
    add(left, hint("Bars: time, open, high, low, close and volume, one symbol per file (an Alpaca export works as it "
                   "is). They chart as CSV:NAME on their own timeframe and longer. Signal: time and position, 1 long, 0 "
                   "flat, -1 short, from that bar's close. It loads here as rules, so Run on chart and the quant checks "
                   "test your strategy as written."),
        12, 12);
    auto *saveB = btn("Save strategy", "btn");
    auto *jsonB = btn("Edit as JSON", "btn");
    {
        auto *row = rowFlex();
        row->layout()->addWidget(saveB);
        row->layout()->addWidget(jsonB);
        ll->addWidget(row);
    }
    ll->addStretch(1);

    // right: #tOut
    tOut = split->right->body();
    split->right->layout()->setSpacing(10);
    split->right->layout()->addWidget(
        hint("Pick or build a strategy, then press <b>Run on chart</b>. It runs on the active chart's symbol and "
             "timeframe."));
    bottom->setContent(split, true);

    QObject::connect(preset, &kit::Select::changed, split, [this](const QString &v) {
        if (v.isEmpty()) return;
        QJsonObject s;
        if (v.startsWith("ex:")) {
            for (const auto &ex : exampleStrategies())
                if (ex.first == v.mid(3)) s = ex.second;
        } else {
            s = app->state.strategies.value(v.mid(3));
        }
        // renderTester() replaces the widget that emitted this signal
        QMetaObject::invokeMethod(q, [this, s, v] {
            applyStrategy(s, v.mid(3));
            renderTester();
        }, Qt::QueuedConnection);
    });
    QObject::connect(run, &QPushButton::clicked, split, [this] { runTester(); });

    auto pickFile = [this, split](const QString &kind) {
        // Test hook: the UI harness sets the qApp property "qtmOpenFile" to stand in for the dialog's pick.
        QString path = qApp->property("qtmOpenFile").toString();
        if (!path.isEmpty()) qApp->setProperty("qtmOpenFile", QVariant());
        else path = QFileDialog::getOpenFileName(app, QString(), QString(), "CSV (*.csv *.txt)");
        if (path.isEmpty()) return;
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) return app->toast(f.errorString(), "err");
        const QByteArray body = f.readAll();
        const QString name = QFileInfo(path).fileName();
        app->toast(QString::fromUtf8("Reading %1…").arg(esc(name)));
        api::postText(q, QString("/api/import/%1?name=%2").arg(kind, QString::fromLatin1(QUrl::toPercentEncoding(name))), body,
                      [this, kind](const QJsonValue &v) {
                          const QJsonObject r = v.toObject();
                          if (kind == "bars") {
                              ChartPane *pane = app->active;
                              try {
                                  if (pane && parseTf(pane->cfg.tf).seconds < parseTf(r.value("tf").toString()).seconds)
                                      pane->cfg.tf = r.value("tf").toString();
                              } catch (const Error &) {
                              }
                              app->setSymbol(r.value("symbol").toString());
                              const double skipped = r.value("skipped").toDouble();
                              app->toast(QString("Imported %1 %2 bars as %3%4. Pick any timeframe from %2 up.")
                                             .arg(intLocale(r.value("bars").toDouble()), r.value("tf").toString(),
                                                  r.value("symbol").toString(),
                                                  js::truthy(skipped) ? QString(" (skipped %1 rows with no price)").arg(show(r.value("skipped")))
                                                                      : QString()));
                          } else {
                              if (!app->importCustom(r.value("file").toString())) return;
                              strategyToUi(r.value("strategy").toObject());
                              tState.name = "Signal " + show(r.value("name"));
                              renderTester();
                              app->toast(QString("Loaded your signal %1: %2 position changes. Open the bars it was made on, "
                                                 "then press Run on chart.")
                                             .arg(show(r.value("name")), intLocale(r.value("changes").toDouble())));
                          }
                      },
                      [this](const QString &m) { app->toast(m, "err"); });
        Q_UNUSED(split);
    };
    QObject::connect(impBars, &QPushButton::clicked, split, [pickFile] { pickFile("bars"); });
    QObject::connect(impSig, &QPushButton::clicked, split, [pickFile] { pickFile("signal"); });

    QObject::connect(spreadB, &QPushButton::clicked, split, [this] {
        const QString sym = app->active ? app->active->cfg.symbol : QString("undefined");
        QPointer<QWidget> in = testerInputs.value("spreadPct");
        api::get(q, "/api/spread?symbol=" + QString::fromLatin1(QUrl::toPercentEncoding(sym)),
                 [this, in](const QJsonValue &v) {
                     const QJsonObject r = v.toObject();
                     const QString pct = numPrecision(r.value("spreadPct").toDouble(), 2);
                     if (auto *i = qobject_cast<Input *>(in.data())) i->setText(pct);
                     app->toast(QString("%1 on %2: bid %3, ask %4. Spread set to %5%.")
                                    .arg(ticker(r.value("symbol").toString()), r.value("source").toString(),
                                         fmtPrice(r.value("bid").toDouble()), fmtPrice(r.value("ask").toDouble()), pct));
                 },
                 [this](const QString &m) { app->toast(m, "err"); });
    });
    QObject::connect(saveB, &QPushButton::clicked, split, [this] {
        bool ok = false;
        const QString name = kit::prompt(app, "Name this strategy:", &ok);
        if (name.isEmpty()) return;
        app->state.strategies[name] = readTester();
        app->persist();
        app->toast(QString("Saved \"%1\".").arg(name));
    });
    QObject::connect(jsonB, &QPushButton::clicked, split, [this] {
        QStringList order;
        const QJsonObject s = readTester(&order);
        auto *body = block();
        add(body, hint("The same format your AI agent can write. Operands: numbers, price fields, indicator keys, or "
                       "key.output."),
            12, 12);
        auto *ta = new CodeArea;
        ta->setCode(panels::stringify(s, strategyOrder(order, tState)));
        ta->setFixedHeight(int(std::round(22 * 12 * 1.5 + 16 + 2)));   // rows="22"
        vbox(body)->addWidget(ta);
        addSpace(body, 5);   // the inline textarea sits on a line box: the strut's descent below it
        auto *foot = new QWidget;
        auto *fl = new QHBoxLayout(foot);
        fl->setContentsMargins(0, 0, 0, 0);
        fl->addStretch(1);
        auto *ok = btn("Apply", "btn primary");
        fl->addWidget(ok);
        app->modal("Strategy JSON", body, foot, true);
        QPointer<CodeArea> taP(ta);
        QObject::connect(ok, &QPushButton::clicked, body, [this, taP] {
            if (!taP) return;
            // JSON.parse and the TypeError strategyToUi(null) throws both land in the catch.
            const QString text = taP->toPlainText();
            const QString err = jsonParseError(text);
            if (!err.isEmpty()) {
                app->toast(esc("Invalid JSON: " + err), "err");
                return;
            }
            const QJsonValue v = jsv::parse(text.toUtf8());
            if (v.isNull()) {
                app->toast(esc("Invalid JSON: Cannot read properties of null (reading 'indicators')"), "err");
                return;
            }
            // numbers, strings and arrays have none of the strategy's keys: an empty strategy
            const QJsonObject s = v.toObject();
            applyStrategy(s, QString());
            app->closeModal();
            renderTester();
        });
    });
    if (lastResult) showResult(*lastResult);
}

// ---------------------------------------------------------------- run

void Panels::Private::runTester()
{
    ChartPane *p = app->active;
    if (!p || p->bars.isEmpty()) {
        app->toast("The active chart has no data yet.", "err");
        return testerFinished("The active chart has no data yet.");
    }
    QStringList order;
    QJsonObject strat = readTester(&order);
    if (strat.value("entryLong").toArray().isEmpty() && strat.value("entryShort").toArray().isEmpty()) {
        app->toast("Add at least one entry condition.", "err");
        return testerFinished("Add at least one entry condition.");
    }
    const Bars bars = p->replay ? p->bars.mid(0, p->replay->index + 1) : p->bars;
    QPointer<ChartPane> pane(p);
    const QString symbol = p->cfg.symbol, tf = p->cfg.tf;
    auto show = [this, pane, bars, strat, order, symbol, tf](const QJsonObject &r, const std::optional<Bars> &subBars) {
        try {
            panels::TesterResult res;
            res.result = r;
            res.symbol = symbol;
            res.tf = tf;
            res.bars = bars;
            res.strat = strat;
            res.indicatorOrder = order;
            res.subBars = subBars;
            lastResult = res;
            if (pane) {
                QVector<IndicatorMarker> marks;
                for (const auto &tv : r.value("trades").toArray()) {
                    const QJsonObject t = tv.toObject();
                    const bool lng = t.value("side").toString() == "long";
                    IndicatorMarker a;
                    a.time = t.value("entryTime").toDouble();
                    a.position = lng ? "belowBar" : "aboveBar";
                    a.shape = lng ? "arrowUp" : "arrowDown";
                    a.color = lng ? "--accent" : "--short";   // theme variables, resolved when drawn
                    a.text = lng ? "Buy" : "Short";
                    IndicatorMarker b;
                    b.time = t.value("exitTime").toDouble();
                    b.position = lng ? "aboveBar" : "belowBar";
                    b.shape = "circle";
                    b.color = t.value("pnl").toDouble() >= 0 ? "--up" : "--down";
                    const double pp = t.value("pnlPct").toDouble();
                    b.text = QString("%1%2%").arg(pp >= 0 ? "+" : "", js::toFixed(pp, 1));
                    marks << a << b;
                }
                pane->extraMarkers = marks;
                pane->render(false);
            }
            showResult(*lastResult);
            testerFinished(QString());
        } catch (const Error &e) {
            app->toast(e.message(), "err");
            testerFinished(e.message());
        } catch (const std::exception &e) {
            app->toast(QString::fromUtf8(e.what()), "err");
            testerFinished(QString::fromUtf8(e.what()));
        }
    };
    // The backtest runs on a worker thread (long histories, the bar magnifier and custom
    // indicators take a while); its result is shown on the UI thread, if it's still the latest run.
    const int run = ++testerRun;
    auto finish = [this, run, show, bars, strat](std::optional<Bars> subBars) {
        api::run(q, [bars, strat, subBars] { return backtest(bars, strat, subBars ? &*subBars : nullptr); },
                 [this, run, show, subBars](const QJsonObject &r) {
                     if (run == testerRun) show(r, subBars);
                 },
                 [this, run](const QString &message) {
                     if (run != testerRun) return;
                     app->toast(message, "err");
                     testerFinished(message);
                 });
    };
    try {
        if (strat.value("intrabar").toString() == "magnifier") {
            const qint64 sec = parseTf(tf).seconds;
            const double span = bars.last().time - bars.first().time + sec;
            const MagnifierTf m = magnifierTf(sec, span);
            if (m.tf.isEmpty()) {
                app->toast("This chart is already as fine as the bar magnifier goes, so it uses the OHLC path.");
            } else {
                app->toast(QString::fromUtf8("Loading %1 bars for the bar magnifier…").arg(m.tf));
                const double first = bars.first().time, last = bars.last().time;
                api::candles(q, symbol, m.tf, int(std::ceil(span / m.seconds)) + 10,
                             [finish, first, last, sec](const CandleSet &cs) {
                                 Bars sub;
                                 for (const Bar &c : cs.candles)
                                     if (c.time >= first && c.time < last + sec) sub << c;
                                 finish(sub);
                             },
                             [this](const QString &msg) {
                                 app->toast(msg, "err");
                                 testerFinished(msg);
                             });
                return;
            }
        }
    } catch (const Error &e) {
        app->toast(e.message(), "err");
        return testerFinished(e.message());
    }
    finish(std::nullopt);
}

void Panels::Private::testerFinished(const QString &error)
{
    for (const auto &waiter : std::exchange(testerWaiters, {})) waiter(error);
}

// ---------------------------------------------------------------- result

void Panels::Private::showResult(const panels::TesterResult &r)
{
    QWidget *out = tOut;
    if (!out) return;
    auto *ol = static_cast<QVBoxLayout *>(out->layout());
    clear(ol);
    const QJsonObject s = r.result.value("stats").toObject();
    const QJsonArray trades = r.result.value("trades").toArray();

    // header row
    auto *head = rowFlex();
    head->layout()->addWidget(text(QString("<b>%1 %2</b>").arg(esc(ticker(r.symbol)), esc(r.tf))));
    head->layout()->addWidget(hint(QString("%1 bars · %2 to %3 · %4 · %5")
                                       .arg(r.bars.size())
                                       .arg(localeDateString(r.bars.first().time * 1000),
                                            localeDateString(r.bars.last().time * 1000),
                                            esc(show(s.value("feesAndSlippage"))), esc(show(s.value("fills"))))));
    head->layout()->addWidget(spacer());
    auto *csv = btn("Export trades (CSV)", "btn small");
    auto *clr = btn("Clear chart marks", "btn small");
    head->layout()->addWidget(csv);
    head->layout()->addWidget(clr);
    ol->addWidget(head);

    // stats
    auto stat = [](const QString &k, const QJsonValue &v, const QString &cls = QString()) {
        return Stat {k, nullish(v) ? QString::fromUtf8("—") : show(v), cls};
    };
    auto pc = [](const QJsonValue &v) -> QJsonValue {
        if (nullish(v)) return QString::fromUtf8("—");
        return QString("%1%2%").arg(geZero(v) ? "+" : "", show(v));
    };
    auto *stats = new Stats;
    stats->setItems({
        stat("Net profit", pc(s.value("netProfitPct")), geZero(s.value("netProfitPct")) ? "up" : "down"),
        stat("Buy and hold", pc(s.value("buyHoldPct")), geZero(s.value("buyHoldPct")) ? "up" : "down"),
        stat("Max drawdown",
             nullish(s.value("maxDrawdownPct")) ? QJsonValue(QString::fromUtf8("—"))
                                                : QJsonValue(QString("-%1%").arg(show(s.value("maxDrawdownPct")))),
             "down"),
        stat("Trades", s.value("totalTrades")),
        stat("Win rate", nullish(s.value("winRatePct")) ? QJsonValue(QString::fromUtf8("—"))
                                                        : QJsonValue(QString("%1%").arg(show(s.value("winRatePct"))))),
        stat("Profit factor", s.value("profitFactor")),
        stat("Sharpe", s.value("sharpe")),
        stat("Sortino", s.value("sortino")),
        stat("Avg trade", pc(s.value("avgTradePct"))),
        stat("Time in market", nullish(s.value("exposurePct")) ? QJsonValue(QString::fromUtf8("—"))
                                                               : QJsonValue(QString("%1%").arg(show(s.value("exposurePct"))))),
    });
    ol->addWidget(stats);

    // equity chart
    auto *eqBox = new EquityBox;
    eqBox->border = theme::color("--border");
    ol->addWidget(eqBox);
    // quant checks and PTQ (margin-top 4 inside the flex column's 10 gap)
    auto *tQuant = new QWidget;
    auto *qL = new QVBoxLayout(tQuant);
    qL->setContentsMargins(0, 4, 0, 0);
    qL->setSpacing(0);
    ol->addWidget(tQuant);
    auto *tPtq = new QWidget;
    auto *pL = new QVBoxLayout(tPtq);
    pL->setContentsMargins(0, 4, 0, 0);
    pL->setSpacing(0);
    ol->addWidget(tPtq);

    // trades
    auto *table = new Table;
    table->setWrap(true);
    table->setHeader({{"Side"}, {"Entry"}, {"Exit"}, {"Entry price"}, {"Exit price"}, {"P&amp;L %"}, {"Bars"},
                      {"Exit reason", "l"}});
    QVector<Row> rows;
    for (int i = trades.size() - 1, n = 0; i >= 0 && n < 300; --i, ++n) {
        const QJsonObject t = trades[i].toObject();
        Row row;
        row.key = show(t.value("entryTime"));
        row.cells = {
            {show(t.value("side"))},
            {localeString(t.value("entryTime").toDouble() * 1000)},
            {localeString(t.value("exitTime").toDouble() * 1000)},
            {fmtPrice(t.value("entryPrice").toDouble())},
            {fmtPrice(t.value("exitPrice").toDouble())},
            {js::toFixed(t.value("pnlPct").toDouble(), 2) + "%", t.value("pnl").toDouble() >= 0 ? "up" : "down"},
            {show(t.value("bars"))},
            {esc(show(t.value("exitReason"))), "l"},
        };
        rows << row;
    }
    table->setRows(rows);
    ol->addWidget(table);
    ol->addStretch(1);

    const QString sym = r.symbol, tf = r.tf;
    const QJsonArray tradesCopy = trades;
    QObject::connect(csv, &QPushButton::clicked, out, [this, sym, tf, tradesCopy] {
        download(app, QString("%1-%2-trades.csv").arg(ticker(sym), tf), tradesCsv(tradesCopy).toUtf8());
    });
    QObject::connect(clr, &QPushButton::clicked, out, [this] {
        if (!app->active) return;
        app->active->extraMarkers.clear();
        app->active->render(false);
    });

    // LW.createChart(el, {...}) with the strategy area and the buy & hold line
    lw::ChartOptions o;
    o.background = theme::color("--bg");
    o.textColor = theme::color("--text");
    o.fontSize = 10;
    o.vertLinesVisible = false;
    o.horzLinesColor = theme::color("--grid");
    o.rightPriceScaleBorderVisible = false;
    o.timeScaleBorderVisible = false;
    o.crosshairLineColor = theme::color("--crosshair");
    o.crosshairLabelBackgroundColor = theme::color("--crosshair-label");
    if (theme::hasPalette()) o.labelTextColor = [](const QColor &bg) { return theme::textOn(bg); };
    auto *chart = new lw::Chart(o, eqBox);
    eqBox->chart = chart;
    equityChart = chart;
    lw::SeriesOptions ao;
    ao.lineColor = theme::color("--accent");
    ao.topColor = theme::color("--accent", 0.3);
    ao.bottomColor = theme::color("--accent", 0);
    ao.lineWidth = 2;
    ao.title = "Strategy";
    lw::Series *eq = chart->addSeries(lw::SeriesType::Area, ao);
    QVector<lw::DataItem> eqData;
    for (const auto &ev : r.result.value("equity").toArray()) {
        lw::DataItem it;
        it.time = ev.toObject().value("time").toDouble();
        it.value = ev.toObject().value("value").toDouble();
        eqData << it;
    }
    eq->setData(eqData);
    lw::SeriesOptions bo;
    bo.color = theme::color("--muted");
    bo.lineWidth = 1;
    bo.title = "Buy & hold";
    bo.priceLineVisible = false;
    lw::Series *bh = chart->addSeries(lw::SeriesType::Line, bo);
    const double c0 = r.bars.first().close;
    const double cap = r.strat.value("initialCapital").toDouble();
    QVector<lw::DataItem> bhData;
    for (const Bar &b : r.bars) {
        lw::DataItem it;
        it.time = b.time;
        it.value = cap * b.close / c0;
        bhData << it;
    }
    bh->setData(bhData);
    chart->timeScale()->fitContent();

    quantui::mountQuantChecks(tQuant, r);
    QJsonObject ctx {{"strategy", r.strat}, {"symbol", r.symbol}, {"tf", r.tf},
                     {"name", !tState.name.isEmpty() ? tState.name : QString("%1 %2").arg(ticker(r.symbol), r.tf)}};
    ptq::mountPtqCheck(tPtq, app, ctx);

    // flex-shrink of the equity box: recompute whenever the column's content changes
    class Fit : public QObject
    {
    public:
        Fit(QWidget *col, QScrollArea *area, EquityBox *box) : QObject(col), m_col(col), m_area(area), m_box(box)
        {
            col->installEventFilter(this);
            area->viewport()->installEventFilter(this);
            QTimer::singleShot(0, this, [this] { fit(); });
        }
        void fit()
        {
            if (!m_col || !m_area || !m_box) return;
            auto *l = m_col->layout();
            const int w = m_col->width();
            int other = 20;   // the column's padding (10 top, 10 bottom)
            int n = 0;
            for (int i = 0; i < l->count(); ++i) {
                QWidget *x = l->itemAt(i)->widget();
                if (!x || x == m_box || x->isHidden()) continue;
                other += x->hasHeightForWidth() ? x->heightForWidth(w) : x->sizeHint().height();
                ++n;
            }
            other += 10 * n;   // gaps (n other items + the equity box -> n gaps)
            const int h = qBound(30, m_area->viewport()->height() - other, 150);
            if (m_box->height() != h) m_box->setFixedHeight(h);
        }

    protected:
        bool eventFilter(QObject *, QEvent *e) override
        {
            if (e->type() == QEvent::LayoutRequest || e->type() == QEvent::Resize)
                QTimer::singleShot(0, this, [this] { fit(); });
            return false;
        }

    private:
        QPointer<QWidget> m_col;
        QPointer<QScrollArea> m_area;
        QPointer<EquityBox> m_box;
    };
    QScrollArea *area = nullptr;
    for (QWidget *w = out->parentWidget(); w && !area; w = w->parentWidget()) area = qobject_cast<QScrollArea *>(w);
    if (area) new Fit(out, area, eqBox);
}

} // namespace qtm
