#pragma once

// One order for several accounts: prepare() prices it on each, send() sends the plan once, then checks on each account that
// the position really opened. The plan is the only way to an order, it expires quickly, and sending uses it up.

#include "broker/MetaCopierClient.h"

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QTimer>

namespace broker {

enum class LineState {
    Planned,
    Sending,
    Accepted,       // taken by the service, being looked for on the account
    Confirmed,      // the position is on the account
    Rejected,       // refused: nothing opened
    NotConfirmed,   // not seen on the account: look before sending again
};

struct SentLine {
    PlanLine plan;
    LineState state = LineState::Planned;
    int requestId = -1;
    QString positionId;
    QString message;
};

class TradeDesk : public QObject
{
    Q_OBJECT
public:
    static constexpr int PlanLifetimeMs = 30000;

    explicit TradeDesk(MetaCopierClient *client, QObject *parent = nullptr);

    void setAllowRealMoney(bool on) { m_allowReal = on; }
    bool allowRealMoney() const { return m_allowReal; }

    void prepare(const Intent &intent, const QVector<Account> &accounts);
    bool canSend() const;   // something to send, in time, and not sent yet
    bool send();            // false, and nothing leaves, without canSend()
    const QVector<SentLine> &lines() const { return m_lines; }
    void clear();

    // The delays of the checks on an order, in ms (shorter in the tests).
    void setCheckDelays(const QVector<int> &delays) { m_checkDelays = delays; }

signals:
    void prepared();
    void changed();          // a line's state
    void finished();         // every line is settled

private:
    void priceLine(int generation, int index, const Intent &intent);
    void lineDone(int generation);
    void sendLine(int index);
    void checkLine(int index, int attempt);
    int nextRequestId(const QString &accountId);
    void settle();

    MetaCopierClient *m_client;
    QVector<SentLine> m_lines;
    QHash<QString, QVector<SymbolSpec>> m_symbols;   // per account, for the session: they don't change while trading
    QVector<int> m_checkDelays {1500, 3000, 6000, 12000};
    QElapsedTimer m_plannedAt;
    int m_generation = 0;
    int m_pending = 0;
    bool m_sent = false;
    bool m_allowReal = false;
};

} // namespace broker
