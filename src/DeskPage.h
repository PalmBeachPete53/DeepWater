#pragma once

#include <QWidget>

class CotReport;
class EconomicCalendar;
class PolicyRates;
class WorldEconomy;
class QButtonGroup;
class QStackedWidget;

// The Desk tab: reading the market across assets. Markets, currency strength, FX sessions, session recaps, the wire, Fed
// pricing, crowd positioning, risk appetite, seasonality, fundamental and technical heatmaps, stop pools, research notes
// and your own workspaces. Everything comes from public sources; what none publishes freely is said on the page.
class DeskPage : public QWidget
{
    Q_OBJECT
public:
    DeskPage(EconomicCalendar *calendar, PolicyRates *rates, WorldEconomy *economy, CotReport *cot, QWidget *parent = nullptr);

    void showSection(int index);

signals:
    // A question for the AI side panel. The prompt names what to read and holds no data: the model reads the page's
    // tables and charts through read_page, with its slot's skill.
    void askAi(const QString &prompt);

private:
    EconomicCalendar *m_calendar;
    PolicyRates *m_rates;
    WorldEconomy *m_economy;
    CotReport *m_cot;
    QButtonGroup *m_tabs;
    QStackedWidget *m_stack;
};

// The sections, also what a workspace is made of.
namespace deskui {

struct Context {
    EconomicCalendar *calendar = nullptr;
    PolicyRates *rates = nullptr;
    DeskPage *page = nullptr;
    WorldEconomy *economy = nullptr;
    CotReport *cot = nullptr;
};
QStringList sectionNames();
// A new view of a section; `index` in sectionNames(), the workspaces section excluded.
QWidget *makeSection(int index, const Context &context);
int sectionCount();

QWidget *makeMarkets(const Context &c);
QWidget *makeStrength(const Context &c);
QWidget *makeSessions(const Context &c);
QWidget *makeRecap(const Context &c);
QWidget *makeWire(const Context &c);
QWidget *makeRates(const Context &c);
QWidget *makePositioning(const Context &c);
QWidget *makeAppetite(const Context &c);
QWidget *makeSeasonality(const Context &c);
QWidget *makeFundamentals(const Context &c);
QWidget *makeTechnical(const Context &c);
QWidget *makeLiquidity(const Context &c);
QWidget *makeNotes(const Context &c);
QWidget *makeForecasts(const Context &c);
QWidget *makeIndicators(const Context &c);
QWidget *makeWorkspaces(const Context &c);

} // namespace deskui
