#include "RefreshScheduler.h"

#include "MacroData.h"

RefreshScheduler::RefreshScheduler(QObject *parent)
    : QObject(parent)
{
    connect(&m_timer, &QTimer::timeout, this, &RefreshScheduler::tick);
    m_timer.start(TickMs);
}

void RefreshScheduler::add(RemoteDataSet *set)
{
    const QString id = QString("set:%1").arg(reinterpret_cast<quintptr>(set));
    m_entries.insert(id, {[set] { return set->fetchedAt(); }, [set] { return set->checkEverySecs(); }, [set] { set->refreshIfStale(); }});
    m_jobs.push_back({id, set->checkEverySecs(), set->fetchedAt(), {}});
}

void RefreshScheduler::add(const QString &id, qint64 everySecs, std::function<void()> check)
{
    m_entries.insert(id, {{}, [everySecs] { return everySecs; }, std::move(check)});
    // Started now: what started the app has just asked it.
    m_jobs.push_back({id, everySecs, {}, QDateTime::currentDateTimeUtc()});
}

void RefreshScheduler::tick()
{
    for (fresh::Job &job : m_jobs) {
        const Entry &e = m_entries[job.id];
        job.everySecs = e.every();
        if (e.last)
            job.last = e.last();
    }
    for (const QString &id : fresh::takeDue(m_jobs, QDateTime::currentDateTimeUtc(), 1))
        m_entries[id].check();
}
