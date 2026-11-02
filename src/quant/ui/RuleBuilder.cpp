#include "RuleBuilder.h"

#include "PanelsKit.h"
#include "Theme.h"
#include "../core/Backtest.h"
#include "../core/JsCompat.h"

#include <QHBoxLayout>
#include <QJsonDocument>
#include <QPainter>
#include <QPainterPath>
#include <QAbstractButton>
#include <QPushButton>
#include <QRegularExpression>
#include <QVBoxLayout>

namespace qtm::rules {

const QStringList PRICE = {"close", "open", "high", "low", "volume", "hl2", "hlc3", "ohlc4"};

QString opLabel(const QString &op)
{
    static const QHash<QString, QString> labels = {
        {">", "is above"},          {"<", "is below"},          {">=", "is at or above"}, {"<=", "is at or below"},
        {"crossesAbove", "crosses above"}, {"crossesBelow", "crosses below"}, {"crosses", "crosses"},
    };
    return labels.value(op);
}

QStringList outputsOf(const IndicatorDef &def)
{
    QStringList out;
    for (const auto &o : def.outputs) out << o.key;
    if (def.id == "supertrend") out << "direction";
    return out;
}

const IndicatorInput *mainInput(const IndicatorDef &def)
{
    for (const auto &i : def.inputs)
        if (i.type == "int" || i.type == "float") return &i;
    return nullptr;
}

// ---------------------------------------------------------------- data

Operand Operand::fromJson(const QJsonObject &o)
{
    Operand r;
    r.kind = o.value("kind").toString("price");
    r.field = o.value("field").toString("close");
    r.v = o.value("v").toDouble(0);
    r.id = o.value("id").toString();
    r.params = o.value("params").toObject();
    r.paramOrder = r.params.keys();
    r.output = o.value("output").toString();
    r.line = o.value("line").toObject();
    r.label = o.value("label").toString();
    return r;
}

Rule Rule::fromJson(const QJsonObject &o)
{
    Rule r;
    r.left = Operand::fromJson(o.value("left").toObject());
    r.op = o.value("op").toString();
    r.right = Operand::fromJson(o.value("right").toObject());
    return r;
}

Rule defaultRule()
{
    Rule r;
    r.left.kind = "price";
    r.left.field = "close";
    r.op = "crossesAbove";
    r.right.kind = "ind";
    r.right.id = "ema";
    r.right.params = QJsonObject {{"length", 50}};
    r.right.paramOrder = {"length"};
    r.right.output = "value";
    return r;
}

Operand toOperand(const QJsonValue &x, const QJsonObject &indicators)
{
    Operand o;
    if (x.isObject() && x.toObject().contains("line") && !x.toObject().value("line").isNull()
        && !x.toObject().value("line").isUndefined()) {
        o.kind = "line";
        o.line = x.toObject().value("line").toObject();
        o.label = "Drawn line";
        return o;
    }
    if (x.isDouble()) {
        o.kind = "value";
        o.v = x.toDouble();
        return o;
    }
    const QString s = x.isString() ? x.toString() : x.isBool() ? (x.toBool() ? "true" : "false")
                    : x.isNull() ? "null" : x.isUndefined() ? "undefined" : "[object Object]";
    if (PRICE.contains(s)) {
        o.kind = "price";
        o.field = s;
        return o;
    }
    const QStringList parts = s.split('.');
    const QString k = parts.value(0), out = parts.size() > 1 ? parts[1] : QString();
    const QJsonValue sv = indicators.value(k);
    const auto def = sv.isObject() ? indicatorById(sv.toObject().value("id").toString()) : nullptr;
    if (!def) {
        o.kind = "value";
        o.v = 0;
        return o;
    }
    const QJsonObject spec = sv.toObject();
    o.kind = "ind";
    o.id = spec.value("id").toString();
    o.params = spec.value("params").toObject();
    o.paramOrder = o.params.keys();
    o.output = !out.isEmpty() ? out : !spec.value("output").toString().isEmpty() ? spec.value("output").toString()
                                                                                 : outputsOf(*def).value(0);
    return o;
}

QJsonArray Compiled::group(const QString &name) const
{
    for (const auto &g : groups)
        if (g.first == name) return g.second;
    return {};
}

Compiled compileRules(const QVector<QPair<QString, QVector<Rule>>> &groups)
{
    Compiled c;
    auto ref = [&](const Operand &o) -> QJsonValue {
        if (o.kind == "line") return QJsonObject {{"line", o.line}};
        if (o.kind == "value") return o.v;
        if (o.kind == "price") return o.field;
        QStringList vals;
        for (const QString &k : o.paramOrder)
            if (o.params.contains(k)) {
                const QJsonValue v = o.params.value(k);
                vals << (v.isDouble() ? js::numberToString(v.toDouble()) : v.toVariant().toString());
            }
        static const QRegularExpression bad("[^a-z0-9_]", QRegularExpression::CaseInsensitiveOption);
        QString key = QString("%1_%2").arg(o.id, vals.join("_"));
        key.replace(bad, "_");
        if (!c.indicatorOrder.contains(key)) c.indicatorOrder << key;
        c.indicators.insert(key, QJsonObject {{"id", o.id}, {"params", o.params}});
        return QString("%1.%2").arg(key, o.output);
    };
    for (const auto &g : groups) {
        QJsonArray arr;
        for (const Rule &r : g.second) {
            const QJsonValue left = ref(r.left);
            const QJsonValue right = ref(r.right);
            arr.append(QJsonObject {{"left", left}, {"op", r.op}, {"right", right}});
        }
        c.groups.push_back({g.first, arr});
    }
    return c;
}

QString describeAlert(const QJsonObject &a)
{
    if (!a.value("message").toString().isEmpty()) return a.value("message").toString();
    const QJsonObject inds = a.value("indicators").toObject();
    auto name = [&](const QJsonValue &x) -> QString {
        if (x.isDouble()) return theme::fmtPrice(x.toDouble());
        const QString s = x.isString() ? x.toString() : x.isNull() ? "null" : "undefined";
        if (PRICE.contains(s)) return s == "close" ? "Price" : s;
        const QStringList parts = s.split('.');
        const QString k = parts.value(0), out = parts.size() > 1 ? parts[1] : QString();
        const QJsonObject spec = inds.value(k).toObject();
        const auto def = indicatorById(spec.value("id").toString());
        if (!def) return s;
        QString nm = def->name;
        nm.remove(QRegularExpression(" \\(.*\\)$"));
        // Object.values(s.params) in JS key order. The order the alert was written in is gone once
        // it is a QJsonObject (sorted keys); the indicator's input order is what the app and agents
        // write, so it stands in for it.
        QStringList vals, keys;
        const QJsonObject params = spec.value("params").toObject();
        for (const auto &in : def->inputs)
            if (params.contains(in.key)) keys << in.key;
        for (auto it = params.begin(); it != params.end(); ++it)
            if (!keys.contains(it.key())) keys << it.key();
        for (const QString &k : keys) {
            const QJsonValue v = params.value(k);
            vals << (v.isDouble() ? js::numberToString(v.toDouble()) : v.toVariant().toString());
        }
        return QString("%1 %2%3").arg(nm, vals.join(" "),
                                     !out.isEmpty() && outputsOf(*def).size() > 1 ? " " + out : QString());
    };
    QJsonArray conds = a.value("conditions").toArray();
    if (conds.isEmpty())
        conds = QJsonArray {QJsonObject {{"left", a.value("left")}, {"op", a.value("op")}, {"right", a.value("right")}}};
    auto nm = [&](const QJsonValue &x) {
        return x.isObject() && x.toObject().contains("line") ? QString("your drawn line") : name(x);
    };
    QStringList parts;
    for (const auto &cv : conds) {
        const QJsonObject c = cv.toObject();
        const QString op = c.value("op").toString();
        parts << QString("%1 %2 %3").arg(nm(c.value("left")), opLabel(op).isEmpty() ? op : opLabel(op),
                                         nm(c.value("right")));
    }
    return parts.join(a.value("match").toString() == "any" ? " or " : " and ");
}

// ---------------------------------------------------------------- OperandWidget

OperandWidget::OperandWidget(const Operand &o, QWidget *parent) : QWidget(parent)
{
    auto *l = new QHBoxLayout(this);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(0);
    set(o);
}

void OperandWidget::set(const Operand &o)
{
    m_o = o;
    kit::clear(layout());
    m_kind = nullptr;
    m_value = m_param = nullptr;
    m_out = nullptr;
    auto *l = static_cast<QHBoxLayout *>(layout());
    const QString val = o.kind == "line" ? "l" : o.kind == "value" ? "v" : o.kind == "price" ? "p:" + o.field : "i:" + o.id;
    m_kind = new kit::Select(kit::Select::Rule);
    m_kind->setMaxWidthPx(150);
    if (o.kind == "line") m_kind->addOption("l", o.label.isEmpty() ? QString("Drawn line") : o.label);
    m_kind->addGroup("Price");
    for (const QString &f : PRICE) m_kind->addOption("p:" + f, f == "close" ? QString("Price (close)") : f);
    m_kind->endGroup();   // the "Value" option sits outside the groups
    m_kind->addOption("v", "Value");
    m_kind->addGroup("Indicators");
    for (const auto &d : indicators())
        if (!d->outputs.isEmpty()) m_kind->addOption("i:" + d->id, d->name);
    m_kind->setValue(val);
    l->addWidget(m_kind);
    if (o.kind == "value") {
        m_value = new kit::Input(kit::Input::Rule, true);
        m_value->setWidthPx(70);
        m_value->setText(js::numberToString(o.v));
        l->addWidget(m_value);
        connect(m_value, &QLineEdit::editingFinished, this, &OperandWidget::changed);
    }
    if (o.kind == "ind") {
        const auto def = indicatorById(o.id);
        const IndicatorInput *mi = def ? mainInput(*def) : nullptr;
        if (mi) {
            m_param = new kit::Input(kit::Input::Rule, true);
            m_param->setWidthPx(70);
            m_paramKey = mi->key;
            m_param->setToolTip(mi->label);
            const QJsonValue pv = o.params.contains(mi->key) && !o.params.value(mi->key).isNull()
                                      ? o.params.value(mi->key)
                                      : mi->def;
            m_param->setText(pv.isDouble() ? js::numberToString(pv.toDouble()) : pv.toVariant().toString());
            l->addWidget(m_param);
            connect(m_param, &QLineEdit::editingFinished, this, &OperandWidget::changed);
        }
        const QStringList outs = def ? outputsOf(*def) : QStringList();
        if (outs.size() > 1) {
            m_out = new kit::Select(kit::Select::Rule);
            m_out->setMaxWidthPx(150);
            for (const QString &k : outs) m_out->addOption(k, k);
            m_out->setValue(o.output);
            l->addWidget(m_out);
            connect(m_out, &kit::Select::changed, this, &OperandWidget::changed);
        }
    }
    connect(m_kind, &kit::Select::changed, this, [this](const QString &k) {
        Operand n;
        if (k == "v") {
            n.kind = "value";
            n.v = 0;
        } else if (k.startsWith("p:")) {
            n.kind = "price";
            n.field = k.mid(2);
        } else if (k == "l") {
            n = m_o;
        } else {
            n.kind = "ind";
            n.id = k.mid(2);
            const auto def = indicatorById(n.id);
            n.output = def ? outputsOf(*def).value(0) : QString();
        }
        // replace the operand after the combo's popup has closed
        QMetaObject::invokeMethod(this, [this, n] {
            set(n);
            emit changed();
        }, Qt::QueuedConnection);
    });
    updateGeometry();
}

Operand OperandWidget::read() const
{
    const QString k = m_kind ? m_kind->value() : QString();
    Operand o;
    if (k == "l") {
        o = m_o;
        o.kind = "line";
        return o;
    }
    if (k == "v") {
        o.kind = "value";
        const double n = m_value ? m_value->number() : 0;
        o.v = std::isnan(n) ? 0 : n;
        return o;
    }
    if (k.startsWith("p:")) {
        o.kind = "price";
        o.field = k.mid(2);
        return o;
    }
    o.kind = "ind";
    o.id = k.mid(2);
    if (m_param) {
        o.params.insert(m_paramKey, m_param->number());
        o.paramOrder = {m_paramKey};
    }
    const auto def = indicatorById(o.id);
    o.output = m_out && !m_out->value().isEmpty() ? m_out->value() : (def ? outputsOf(*def).value(0) : QString());
    return o;
}

// ---------------------------------------------------------------- RuleWidget

RuleWidget::RuleWidget(const Rule &r, QWidget *parent) : QWidget(parent)
{
    auto *fl = new kit::FlowLayout(this, 4, 4);
    fl->setContentsMargins(6, 6, 6, 6);   // padding 5 + border 1
    QSizePolicy sp(QSizePolicy::Preferred, QSizePolicy::Preferred);
    sp.setHeightForWidth(true);
    setSizePolicy(sp);
    m_left = new OperandWidget(r.left);
    m_op = new kit::Select(kit::Select::Rule);
    m_op->setMaxWidthPx(150);
    for (const QString &o : OPS) m_op->addOption(o, opLabel(o));
    m_op->setValue(r.op);
    m_right = new OperandWidget(r.right);
    // <button class="x" title="Remove">&times;</button>: border 0, no background, muted, UA padding 1px 6px
    class XButton : public QAbstractButton
    {
    public:
        XButton()
        {
            setCursor(Qt::PointingHandCursor);
            setFocusPolicy(Qt::NoFocus);
            setToolTip("Remove");
        }
        QSize sizeHint() const override
        {
            const QFontMetricsF fm(kit::uiFont(13));
            return {int(std::ceil(fm.horizontalAdvance(QChar(0xd7)) + 12)),
                    int(std::round(std::round(fm.ascent()) + std::round(fm.descent()) + 2))};
        }

    protected:
        void paintEvent(QPaintEvent *) override
        {
            QPainter p(this);
            p.setFont(kit::uiFont(13));
            p.setPen(theme::color("--muted"));
            const QFontMetricsF fm(kit::uiFont(13));
            p.drawText(QPointF(6, 1 + std::round(fm.ascent())), QString(QChar(0xd7)));
        }
    };
    auto *x = new XButton;
    x->setProperty("marginLeftAuto", true);
    m_x = x;
    fl->addWidget(m_left);
    fl->addWidget(m_op);
    fl->addWidget(m_right);
    fl->addWidget(x);
    connect(m_left, &OperandWidget::changed, this, [this] { updateGeometry(); emit changed(); });
    connect(m_right, &OperandWidget::changed, this, [this] { updateGeometry(); emit changed(); });
    connect(m_op, &kit::Select::changed, this, &RuleWidget::changed);
    connect(x, &QAbstractButton::clicked, this, &RuleWidget::removeRequested);
}

Rule RuleWidget::read() const
{
    Rule r;
    r.left = m_left->read();
    r.op = m_op->value();
    r.right = m_right->read();
    return r;
}

void RuleWidget::removeX()
{
    if (!m_x) return;
    m_x->hide();
    m_x->deleteLater();
    m_x = nullptr;
}

void RuleWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 6, 6);
    p.fillPath(path, theme::color("--panel"));
    p.setPen(QPen(theme::color("--border"), 1));
    p.drawPath(path);
}

// ---------------------------------------------------------------- RulesBox

RulesBox::RulesBox(QWidget *parent) : QWidget(parent)
{
    auto *l = new QVBoxLayout(this);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(6);
}

RuleWidget *RulesBox::addRule(const Rule &r)
{
    auto *w = new RuleWidget(r);
    layout()->addWidget(w);
    connect(w, &RuleWidget::changed, this, &RulesBox::changed);
    connect(w, &RuleWidget::removeRequested, this, [this, w] {
        layout()->removeWidget(w);
        w->hide();
        w->deleteLater();
        updateGeometry();
        emit changed();
    });
    updateGeometry();
    return w;
}

QVector<Rule> RulesBox::read() const
{
    QVector<Rule> out;
    for (int i = 0; i < layout()->count(); ++i)
        if (auto *w = qobject_cast<RuleWidget *>(layout()->itemAt(i)->widget())) out << w->read();
    return out;
}

int RulesBox::count() const
{
    int n = 0;
    for (int i = 0; i < layout()->count(); ++i)
        if (qobject_cast<RuleWidget *>(layout()->itemAt(i)->widget())) ++n;
    return n;
}

} // namespace qtm::rules
