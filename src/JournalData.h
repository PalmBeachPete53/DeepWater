#pragma once

#include "desk/Journal.h"

#include <QHash>
#include <QObject>
#include <QStringList>
#include <QVector>

// The trades of the Journal tab, by account, kept in one file in the app's data folder. Nothing leaves the Mac.
class JournalStore : public QObject
{
    Q_OBJECT
public:
    struct Account {
        QString name;
        QString currency = "USD";
        double balance = 10000;     // the balance the statistics start from
        QDateTime lastImport;
        QVector<desk::Trade> trades;
    };

    static JournalStore *instance();

    const QVector<Account> &accounts() const { return m_accounts; }
    int current() const { return m_current; }
    void setCurrent(int index);
    const Account &account() const { return m_accounts[m_current]; }
    void addAccount(const QString &name, const QString &currency, double balance);
    void editAccount(const QString &name, const QString &currency, double balance);
    void removeAccount();

    // Returns how many were added and updated.
    void importTrades(const QVector<desk::Trade> &trades, int *added, int *updated);
    void saveTrade(const desk::Trade &trade);     // adds it, or replaces the one with its id
    void removeTrade(const QString &id);

    QStringList symbols() const;
    QStringList strategies() const;
    QVector<int> years() const;

    // Dev flag --journal-sample: reads a statement for this run only and never writes the file.
    void loadSampleForThisRun(const QString &path);

signals:
    void changed();

private:
    explicit JournalStore(QObject *parent = nullptr);
    void load();
    void save() const;

    QVector<Account> m_accounts;
    int m_current = 0;
    bool m_memoryOnly = false;
};
