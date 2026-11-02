#pragma once

// providers.js cached(key, ttlMs, fn): a cache with in-flight de-duplication, so ten charts
// asking for the same thing make one request. Bounded: the oldest entries go first.
// A failure is not cached (the entry is dropped and every waiter gets the error).

#include "Json.h"

#include <QHash>
#include <QMutex>
#include <QString>

#include <any>
#include <condition_variable>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>

namespace qtm {

class Cache
{
public:
    static constexpr int MAX = 400;

    template <typename T>
    T get(const QString &key, double ttlMs, const std::function<T()> &fn)
    {
        std::shared_ptr<Slot> slot;
        bool mine = false;
        {
            QMutexLocker lock(&m_mutex);
            const double now = jsv::nowMs();
            auto it = m_entries.find(key);
            if (it != m_entries.end() && (it->slot->pending() || now - it->at < ttlMs)) {
                slot = it->slot;
            } else {
                if (m_entries.size() >= MAX) evict();
                slot = std::make_shared<Slot>();
                if (it != m_entries.end()) {
                    it->at = now;
                    it->slot = slot;
                } else {
                    m_entries.insert(key, Entry { now, ++m_seq, slot });
                }
                mine = true;
            }
        }
        if (mine) {
            try {
                T v = fn();
                {
                    QMutexLocker lock(&m_mutex);
                    auto it = m_entries.find(key);
                    if (it != m_entries.end() && it->slot == slot) it->at = jsv::nowMs();
                }
                slot->resolve(std::any(v));
                return v;
            } catch (...) {
                {
                    QMutexLocker lock(&m_mutex);
                    auto it = m_entries.find(key);
                    if (it != m_entries.end() && it->slot == slot) m_entries.erase(it);
                }
                slot->reject(std::current_exception());
                throw;
            }
        }
        return std::any_cast<T>(slot->wait());
    }

    void clear()
    {
        QMutexLocker lock(&m_mutex);
        m_entries.clear();
    }

private:
    struct Slot {
        std::mutex m;
        std::condition_variable cv;
        bool done = false;
        std::any value;
        std::exception_ptr error;
        bool pending()
        {
            std::lock_guard l(m);
            return !done;
        }
        void resolve(std::any v)
        {
            {
                std::lock_guard l(m);
                value = std::move(v);
                done = true;
            }
            cv.notify_all();
        }
        void reject(std::exception_ptr e)
        {
            {
                std::lock_guard l(m);
                error = e;
                done = true;
            }
            cv.notify_all();
        }
        std::any wait()
        {
            std::unique_lock l(m);
            cv.wait(l, [&] { return done; });
            if (error) std::rethrow_exception(error);
            return value;
        }
    };
    struct Entry {
        double at = 0;
        quint64 seq = 0;     // insertion order (JS Map order)
        std::shared_ptr<Slot> slot;
    };

    // Oldest first, skipping requests still in flight, until 10% is free.
    void evict()
    {
        QVector<QPair<quint64, QString>> order;
        for (auto it = m_entries.begin(); it != m_entries.end(); ++it)
            order.push_back({ it->seq, it.key() });
        std::sort(order.begin(), order.end());
        for (const auto &o : order) {
            if (m_entries.size() < MAX * 0.9) break;
            auto it = m_entries.find(o.second);
            if (!it->slot->pending()) m_entries.erase(it);
        }
    }

    QMutex m_mutex;
    QHash<QString, Entry> m_entries;
    quint64 m_seq = 0;
};

} // namespace qtm
