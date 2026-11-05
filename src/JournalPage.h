#pragma once

#include "desk/Journal.h"

#include <QWidget>

#include <functional>

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
    // The form of a trade a model described, for the user to check: saved only if they press Save; `done` says which.
    // `emptied` names the fields left out as not stated (desk::keepStated); `checked`: there was a question to compare with.
    static void draftTrade(const desk::Trade &trade, const QStringList &emptied, bool checked, QWidget *parent, std::function<void(bool saved, const desk::Trade &)> done);

private:
    QButtonGroup *m_tabs;
    QStackedWidget *m_stack;
};
