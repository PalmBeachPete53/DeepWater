#pragma once

#include <QWidget>

class QButtonGroup;
class QStackedWidget;

// The Journal tab: your real trades and your risk. Dashboard, Trade log, Advanced details and the Risk calculator. Trades
// come from statements of MetaTrader 4 and 5 and cTrader (files, no broker connection) or are logged by hand; every figure
// is computed from what was entered, and nothing leaves the Mac.
class JournalPage : public QWidget
{
    Q_OBJECT
public:
    explicit JournalPage(QWidget *parent = nullptr);

    void showSection(int index);

private:
    QButtonGroup *m_tabs;
    QStackedWidget *m_stack;
};
