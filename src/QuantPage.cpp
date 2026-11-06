#include "QuantPage.h"

#include "QuantMarkovViews.h"
#include "QuantViews.h"

#include "quant/ui/PanelsKit.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QSettings>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

QuantPage::QuantPage(CotReport *cot, QWidget *parent)
    : QWidget(parent)
{
    setObjectName("quantPage");
    setAttribute(Qt::WA_StyledBackground);
    setFont(quant::mono(12));
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);

    // Sections, right under the main header.
    auto *sub = new QWidget;
    sub->setObjectName("qtSubBar");
    sub->setAttribute(Qt::WA_StyledBackground);
    sub->setFixedHeight(36);
    auto *sh = new qtm::kit::WrapBarLayout(sub, 2);
    sh->setContentsMargins(6, 0, 12, 0);
    m_tabs = new QButtonGroup(this);
    const QStringList names {tr("Compare"), tr("Correlation"), tr("Risk & Beta"), "COT", tr("Signals"), tr("Markov"), tr("Portfolio")};
    for (int i = 0; i < names.size(); ++i) {
        auto *b = new QToolButton;
        b->setObjectName("qtSubTab");
        b->setText(names[i].toUpper().replace("&", "&&"));
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedHeight(36);
        m_tabs->addButton(b, i);
        sh->addWidget(b);
    }
    m_tabs->button(3)->setToolTip(tr("Commitments of Traders"));
    v->addWidget(sub);

    m_stack = new QStackedWidget;
    m_stack->addWidget(new quant::CompareView);
    m_stack->addWidget(new quant::CorrelationView);
    m_stack->addWidget(new quant::RiskBetaView);
    auto *cotView = new quant::CotView(cot);
    connect(cotView, &quant::CotView::askAi, this, &QuantPage::askAi);
    m_stack->addWidget(cotView);
    m_stack->addWidget(new quant::SignalsView);
    m_stack->addWidget(new quant::MarkovView);
    m_stack->addWidget(new quant::PortfolioView);
    v->addWidget(m_stack, 1);

    connect(m_tabs, &QButtonGroup::idClicked, this, &QuantPage::showSection);
    showSection(std::clamp(QSettings().value("quant/section", 0).toInt(), 0, m_stack->count() - 1));
}

void QuantPage::showSection(int index)
{
    if (index < 0 || index >= m_stack->count())
        return;
    m_tabs->button(index)->setChecked(true);
    m_stack->setCurrentIndex(index);
    QSettings().setValue("quant/section", index);
}
