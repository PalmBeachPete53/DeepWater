#pragma once

#include <QTimer>
#include <QWidget>

#include "desk/Market.h"

class EconomicCalendar;
class QLabel;

// Four tiles from the Desk on Overview and Forex: the sessions open, the currencies' strength over a day, the risk
// appetite and the next high-impact release. Each one opens its section.
class DeskStrip : public QWidget
{
    Q_OBJECT
public:
    explicit DeskStrip(EconomicCalendar *calendar, QWidget *parent = nullptr);

signals:
    void openSection(int section);

protected:
    void showEvent(QShowEvent *) override;
    void hideEvent(QHideEvent *) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void fillClock();     // what follows the time: sessions, the next release
    void fetchMarkets();  // what follows prices: strength, appetite

    EconomicCalendar *m_calendar;
    QLabel *m_sessions, *m_sessionsSub, *m_strength, *m_strengthSub, *m_appetite, *m_appetiteSub, *m_next, *m_nextSub;
    QHash<QObject *, int> m_targets;
    QTimer m_clock;
    QTimer m_markets;
};
