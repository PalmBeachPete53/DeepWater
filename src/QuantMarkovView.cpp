#include "QuantMarkovViews.h"

#include "ResearchCharts.h"
#include "Theme.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QRegularExpression>
#include <QSettings>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace quant {

QStringList regimeKeys(int states)
{
    switch (states) {
    case 1: return {QT_TRANSLATE_NOOP("QObject", "Single")};
    case 2: return {QT_TRANSLATE_NOOP("QObject", "Calm"), QT_TRANSLATE_NOOP("QObject", "Turbulent")};
    case 3: return {QT_TRANSLATE_NOOP("QObject", "Calm"), QT_TRANSLATE_NOOP("QObject", "Normal"), QT_TRANSLATE_NOOP("QObject", "Turbulent")};
    default: return {QT_TRANSLATE_NOOP("QObject", "Calm"), QT_TRANSLATE_NOOP("QObject", "Normal"), QT_TRANSLATE_NOOP("QObject", "Volatile"), QT_TRANSLATE_NOOP("QObject", "Crisis")};
    }
}

QString regimeLabelText(const QString &key)
{
    return key.isEmpty() ? key : QObject::tr(key.toUtf8().constData());
}

QStringList regimeLabels(int states)
{
    QStringList out;
    for (const QString &key : regimeKeys(states))
        out << regimeLabelText(key);
    return out;
}

QString modelText(const QString &model)
{
    static const QRegularExpression ms("^MS-GARCH K=(\\d+) · (Student-t|normal)$"), single("^GARCH\\(1,1\\) · (Student-t|normal)$");
    const auto innovations = [](const QString &s) { return s == "normal" ? QObject::tr("normal") : QObject::tr("Student-t"); };
    if (const auto m = ms.match(model); m.hasMatch())
        return QObject::tr("MS-GARCH K=%1 · %2").arg(m.captured(1), innovations(m.captured(2)));
    if (const auto m = single.match(model); m.hasMatch())
        return QObject::tr("GARCH(1,1) · %1").arg(innovations(m.captured(1)));
    return model;   // a record saved translated by an earlier build
}

QVector<QColor> regimeColors(int states)
{
    const Palette &c = Theme::current();
    switch (states) {
    case 1: return {c.TextDim};
    case 2: return {c.Up, c.Down};
    case 3: return {c.Up, research::palette::yellow(), c.Down};
    default: return {c.Up, research::palette::yellow(), research::palette::orange(), c.Down};
    }
}

MarkovView::MarkovView(QWidget *parent)
    : QWidget(parent)
{
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);
    auto *sub = new QWidget;
    sub->setObjectName("qtSubBar");
    sub->setAttribute(Qt::WA_StyledBackground);
    auto *h = new QHBoxLayout(sub);
    h->setContentsMargins(12, 6, 12, 6);
    h->addWidget(segments({tr("HMM regimes"), tr("MS-GARCH"), tr("Execution")}, &m_tabs));
    h->addStretch(1);
    h->addWidget(label(tr("Hidden Markov regimes · Markov-switching GARCH volatility · optimal execution (MDP)"), "qtNote"));
    v->addWidget(sub);
    m_stack = new QStackedWidget;
    m_stack->addWidget(new HmmView);
    m_stack->addWidget(new MsGarchView);
    m_stack->addWidget(new ExecutionView);
    v->addWidget(m_stack, 1);
    connect(m_tabs, &QButtonGroup::idClicked, this, [this](int i) {
        m_stack->setCurrentIndex(i);
        QSettings().setValue("quant/markov/section", i);
    });
    const int i = std::clamp(QSettings().value("quant/markov/section", 0).toInt(), 0, m_stack->count() - 1);
    m_tabs->button(i)->setChecked(true);
    m_stack->setCurrentIndex(i);
}

} // namespace quant
