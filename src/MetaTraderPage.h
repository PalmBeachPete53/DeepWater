#pragma once

#include <QTimer>
#include <QVector>
#include <QWidget>

#include "broker/TradeDesk.h"

class QButtonGroup;
class QCheckBox;
class QDoubleSpinBox;
class QFrame;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

// MetaTrader: the accounts connected through MetaCopier, one order sized for each by its own balance, and their positions.
// An order goes through a plan the user reads, a confirmation, and a check that it opened. No AI reads or uses this page
// (MainWindow::aiReadPage, aiOpenPage), and the key lives in the system's secret store. See README › MetaTrader.
class MetaTraderPage : public QWidget
{
    Q_OBJECT
public:
    explicit MetaTraderPage(QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *) override;
    void hideEvent(QHideEvent *) override;

private:
    QWidget *buildKeyBar();
    QWidget *buildAccounts();
    QWidget *buildTicket();
    QWidget *buildPlan();
    QWidget *buildPositions();
    void saveKey();
    void forgetKey();
    void loadAccounts();
    void refreshAccounts();
    void refreshPositions();
    void fillAccounts();
    void fillPositions();
    void fillPlan();
    void updateActions();
    void prepare();
    void sendPlan();
    void closePosition(const broker::Account &account, const QString &positionId);
    void closeAllArmed();
    void askRealMoney(bool on);
    QVector<broker::Account> armedAccounts() const;
    void setStatus(const QString &text, bool error = false);
    QString refusalText(const broker::PlanLine &line) const;
    QString stateText(const broker::SentLine &line) const;

    broker::MetaCopierClient *m_client;
    broker::TradeDesk *m_desk;
    QVector<broker::Account> m_accounts;
    QSet<QString> m_armed;
    struct Held { broker::Account account; broker::Position position; };
    QVector<Held> m_positions;
    QTimer m_refresh;
    QTimer m_planClock;
    int m_generation = 0;
    int m_ticks = 0;

    QLabel *m_status;
    QLineEdit *m_key;
    QPushButton *m_saveKey, *m_forgetKey;
    QTableWidget *m_accountsTable;
    QLabel *m_capNote;
    QCheckBox *m_real;
    QLineEdit *m_symbol, *m_stop, *m_target;
    QButtonGroup *m_side;
    QDoubleSpinBox *m_risk;
    QPushButton *m_prepare, *m_send, *m_closeAll;
    QTableWidget *m_planTable;
    QLabel *m_planNote;
    QTableWidget *m_positionsTable;
};
