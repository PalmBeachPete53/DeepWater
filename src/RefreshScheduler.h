#pragma once

#include "fresh/Fresh.h"

#include <QHash>
#include <QObject>
#include <QTimer>

#include <functional>

class RemoteDataSet;

// One timer for every source the app keeps up to date on its own, whichever page is open. Each data set says how often
// it wants to be asked (RemoteDataSet::checkEverySecs); one is asked at a time, every two seconds at most, so that no
// server sees the app ask for everything at once. What only a page reads is read when that page is open.
class RefreshScheduler : public QObject
{
    Q_OBJECT
public:
    explicit RefreshScheduler(QObject *parent = nullptr);

    void add(RemoteDataSet *set);
    // A source of another kind: its own function decides whether it has anything to do.
    void add(const QString &id, qint64 everySecs, std::function<void()> check);

    static constexpr int TickMs = 2000;

private:
    void tick();

    struct Entry {
        std::function<QDateTime()> last;
        std::function<qint64()> every;
        std::function<void()> check;
    };
    QTimer m_timer;
    QVector<fresh::Job> m_jobs;
    QHash<QString, Entry> m_entries;
};
