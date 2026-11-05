#include "DeskPage.h"

#include "QuantViews.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QSettings>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace deskui {

QStringList sectionNames()
{
    return {DeskPage::tr("Markets"), DeskPage::tr("Strength"), DeskPage::tr("Sessions"), DeskPage::tr("Recap"), DeskPage::tr("Wire"), DeskPage::tr("Rates"),
            DeskPage::tr("Positioning"), DeskPage::tr("Risk appetite"), DeskPage::tr("Seasonality"), DeskPage::tr("Fundamentals"), DeskPage::tr("Technicals"),
            DeskPage::tr("Liquidity"), DeskPage::tr("Notes"), DeskPage::tr("Forecasts"), DeskPage::tr("Workspaces")};
}

int sectionCount()
{
    return 14;   // the workspaces aren't a panel of a workspace
}

QWidget *makeSection(int index, const Context &c)
{
    using Maker = QWidget *(*)(const Context &);
    static const Maker makers[] = {makeMarkets, makeStrength, makeSessions, makeRecap, makeWire, makeRates, makePositioning,
                                   makeAppetite, makeSeasonality, makeFundamentals, makeTechnical, makeLiquidity, makeNotes, makeForecasts};
    return index >= 0 && index < sectionCount() ? makers[index](c) : nullptr;
}

} // namespace deskui

DeskPage::DeskPage(EconomicCalendar *calendar, PolicyRates *rates, WorldEconomy *economy, CotReport *cot, QWidget *parent)
    : QWidget(parent)
    , m_calendar(calendar)
    , m_rates(rates)
    , m_economy(economy)
    , m_cot(cot)
{
    setObjectName("quantPage");
    setAttribute(Qt::WA_StyledBackground);
    setFont(quant::mono(12));
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);

    auto *sub = new QWidget;
    sub->setObjectName("qtSubBar");
    sub->setAttribute(Qt::WA_StyledBackground);
    sub->setFixedHeight(36);
    auto *sh = new QHBoxLayout(sub);
    sh->setContentsMargins(6, 0, 12, 0);
    sh->setSpacing(2);
    m_tabs = new QButtonGroup(this);
    const QStringList names = deskui::sectionNames();
    for (int i = 0; i < names.size(); ++i) {
        auto *b = new QToolButton;
        b->setObjectName("qtSubTab");
        b->setText(names[i].toUpper());
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedHeight(36);
        m_tabs->addButton(b, i);
        sh->addWidget(b);
    }
    sh->addStretch(1);
    v->addWidget(sub);

    // Each section is built the first time it is shown: they download a lot.
    m_stack = new QStackedWidget;
    for (int i = 0; i < names.size(); ++i)
        m_stack->addWidget(new QWidget);
    v->addWidget(m_stack, 1);

    connect(m_tabs, &QButtonGroup::idClicked, this, &DeskPage::showSection);
    showSection(std::clamp(QSettings().value("desk/section", 0).toInt(), 0, m_stack->count() - 1));
}

void DeskPage::showSection(int index)
{
    if (index < 0 || index >= m_stack->count())
        return;
    m_tabs->button(index)->setChecked(true);
    QWidget *current = m_stack->widget(index);
    if (!current->layout()) {
        const deskui::Context c {m_calendar, m_rates, this, m_economy, m_cot};
        auto *layout = new QVBoxLayout(current);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(index == deskui::sectionCount() ? deskui::makeWorkspaces(c) : deskui::makeSection(index, c));
    }
    m_stack->setCurrentIndex(index);
    QSettings().setValue("desk/section", index);
}
