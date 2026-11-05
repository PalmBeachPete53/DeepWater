#pragma once

#include "desk/History.h"

#include <QObject>
#include <QTimer>

class PolicyRates;

// Notes once a weekday, while the app is open, what no free source keeps: the Myfxbook community's long share and what
// the market prices for the Fed's and the Bank of England's next meetings. It stays on this computer
// (AppData/desk/history.json), and the history starts the day this first runs: nothing can be filled in afterwards.
class DeskHistory : public QObject
{
    Q_OBJECT
public:
    static DeskHistory *instance();

    void start(PolicyRates *rates);
    const desk::History &history() const { return m_history; }

    // Dev flag --history-sample <file>: a history in memory for this run; nothing is collected, read or written.
    void loadSampleForThisRun(const QString &path);

signals:
    void changed();

private:
    explicit DeskHistory(QObject *parent = nullptr);
    void collect();
    void noteCrowd();
    void noteMeetings(const QString &currency, const QVector<std::pair<QDate, double>> &moves, const QDate &date);
    void save();

    desk::History m_history;
    PolicyRates *m_rates = nullptr;
    QTimer m_clock;
    bool m_sample = false;
};
