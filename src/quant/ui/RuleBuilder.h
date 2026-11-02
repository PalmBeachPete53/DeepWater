#pragma once

// The operand / rule builder of panels.js, shared by the strategy tester, the alert dialog and
// the screener's technical filter: operandHtml/readOperand/ruleHtml/bindRules/readRules/
// compileRules/toOperand/defaultRule, as widgets.

#include "../core/Indicators.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>
#include <QWidget>

#include <functional>

namespace qtm::kit { class Select; class Input; class FlowLayout; }

namespace qtm::rules {

extern const QStringList PRICE;
QString opLabel(const QString &op);                       // OP_LABEL
QStringList outputsOf(const IndicatorDef &def);           // outputs + "direction" for Supertrend
const IndicatorInput *mainInput(const IndicatorDef &def); // first int/float input

// A UI operand: { kind: "line" | "value" | "price" | "ind", ... } as panels.js keeps them.
struct Operand {
    QString kind = "price";
    QString field = "close";          // price
    double v = 0;                     // value
    QString id;                       // ind
    QJsonObject params;               // ind (JS key order kept in paramOrder)
    QStringList paramOrder;
    QString output;                   // ind
    QJsonObject line;                 // line
    QString label;                    // line
    QStringList lineOrder;            // key order of `line` as the app wrote it

    static Operand fromJson(const QJsonObject &o);   // the UI shape (preset rules from the app)
};
struct Rule {
    Operand left;
    QString op = "crossesAbove";
    Operand right;
    static Rule fromJson(const QJsonObject &o);
};

Rule defaultRule();
// toOperand(x, indicators): a strategy operand (number, price field, "key.output", {line}) back to UI.
Operand toOperand(const QJsonValue &x, const QJsonObject &indicators);

// compileRules(groups): { indicators, [group]: [{ left, op, right }] }. `indicatorOrder` gets the
// indicator keys in JS insertion order (QJsonObject sorts them).
struct Compiled {
    QJsonObject indicators;
    QStringList indicatorOrder;
    QVector<QPair<QString, QJsonArray>> groups;
    QJsonArray group(const QString &name) const;
};
Compiled compileRules(const QVector<QPair<QString, QVector<Rule>>> &groups);

// describeAlert(a) from panels.js.
QString describeAlert(const QJsonObject &a);

// <span class="operand">: kind select + value / main parameter / output.
class OperandWidget : public QWidget
{
    Q_OBJECT
public:
    explicit OperandWidget(const Operand &o, QWidget *parent = nullptr);
    void set(const Operand &o);
    Operand read() const;

signals:
    void changed();

private:
    Operand m_o;
    kit::Select *m_kind = nullptr;
    kit::Input *m_value = nullptr;
    kit::Input *m_param = nullptr;
    QString m_paramKey;
    kit::Select *m_out = nullptr;
};

// <div class="rule">: operand, operator, operand, remove button.
class RuleWidget : public QWidget
{
    Q_OBJECT
public:
    explicit RuleWidget(const Rule &r, QWidget *parent = nullptr);
    Rule read() const;
    void removeX();   // the screener removes the .x button

signals:
    void changed();
    void removeRequested();

protected:
    void paintEvent(QPaintEvent *) override;

private:
    OperandWidget *m_left, *m_right;
    kit::Select *m_op;
    QWidget *m_x;
};

// <div class="rules">: a column of rules (gap 6, margin 4px 0 10px is the caller's).
class RulesBox : public QWidget
{
    Q_OBJECT
public:
    explicit RulesBox(QWidget *parent = nullptr);
    RuleWidget *addRule(const Rule &r);
    QVector<Rule> read() const;   // readRules(container)
    int count() const;

signals:
    void changed();
};

} // namespace qtm::rules
