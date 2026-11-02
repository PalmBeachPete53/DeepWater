#pragma once

// What the JS gets from promises, for blocking code on worker threads.
//
//   spawn(fn)          starts fn() on the backend's thread pool and returns a Task. task.get()
//                      waits for it and rethrows its exception. If no pool thread has picked the
//                      task up yet, get() runs it on the calling thread instead, so nested
//                      parallel work can never starve the pool (no deadlock however deep).
//   all(n, fn)         Promise.all over fn(0..n-1): runs them in parallel, returns the results in
//                      order, rethrows the first failure (by index) once all have finished.
//   mapLimit(n, k, fn) server.js mapLimit: at most k at a time.

#include <QThreadPool>
#include <QVector>

#include <atomic>
#include <condition_variable>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <type_traits>

namespace qtm::async {

QThreadPool *pool();
// The UI's calls into the backend (ui/Api.cpp): a narrower pool of their own. The backend waits for
// both pools when it is destroyed, so no thread still works for it once it's gone.
QThreadPool *requestPool();

template <typename T>
class Task
{
public:
    struct State {
        std::function<T()> fn;
        std::atomic<bool> claimed { false };
        std::mutex m;
        std::condition_variable cv;
        bool done = false;
        std::optional<T> value;
        std::exception_ptr error;

        void run()
        {
            try {
                value.emplace(fn());
            } catch (...) {
                error = std::current_exception();
            }
            fn = nullptr;
            {
                std::lock_guard lock(m);
                done = true;
            }
            cv.notify_all();
        }
        bool claim()
        {
            bool expected = false;
            return claimed.compare_exchange_strong(expected, true);
        }
    };

    Task() = default;
    explicit Task(std::shared_ptr<State> s) : m_s(std::move(s)) {}

    // Waits for the result (running the task here if nobody started it); rethrows its error.
    T get() const
    {
        wait();
        if (m_s->error)
            std::rethrow_exception(m_s->error);
        return *m_s->value;
    }
    void wait() const
    {
        if (m_s->claim()) {
            m_s->run();
            return;
        }
        std::unique_lock lock(m_s->m);
        m_s->cv.wait(lock, [&] { return m_s->done; });
    }
    bool valid() const { return bool(m_s); }

private:
    std::shared_ptr<State> m_s;
};

template <typename F>
auto spawn(F fn) -> Task<std::invoke_result_t<F>>
{
    using T = std::invoke_result_t<F>;
    static_assert(!std::is_void_v<T>, "spawn() needs a value; return a bool for side effects");
    auto s = std::make_shared<typename Task<T>::State>();
    s->fn = std::move(fn);
    pool()->start([s] {
        if (s->claim())
            s->run();
    });
    return Task<T>(s);
}

// Promise.all(items.map(fn)): every task finishes before this returns, so fn may capture the
// caller's locals by reference.
template <typename F>
auto all(int n, F fn) -> QVector<std::invoke_result_t<F, int>>
{
    using T = std::invoke_result_t<F, int>;
    QVector<Task<T>> tasks;
    tasks.reserve(n);
    for (int i = 0; i < n; ++i)
        tasks.push_back(spawn([fn, i] { return fn(i); }));
    QVector<T> out;
    out.reserve(n);
    std::exception_ptr first;
    for (auto &t : tasks) {
        try {
            out.push_back(t.get());
        } catch (...) {
            if (!first) first = std::current_exception();
            out.push_back(T {});
        }
    }
    if (first)
        std::rethrow_exception(first);
    return out;
}

// server.js mapLimit(items, n, fn): at most `limit` running at once, results in order.
template <typename F>
auto mapLimit(int count, int limit, F fn) -> QVector<std::invoke_result_t<F, int>>
{
    using T = std::invoke_result_t<F, int>;
    auto out = std::make_shared<QVector<T>>(count);
    auto next = std::make_shared<std::atomic<int>>(0);
    const int workers = std::min(limit, count);
    QVector<Task<bool>> tasks;
    for (int w = 0; w < workers; ++w)
        tasks.push_back(spawn([out, next, fn, count] {
            for (int k = (*next)++; k < count; k = (*next)++)
                (*out)[k] = fn(k);
            return true;
        }));
    std::exception_ptr first;
    for (auto &t : tasks) {
        try {
            t.get();
        } catch (...) {
            if (!first) first = std::current_exception();
        }
    }
    if (first)
        std::rethrow_exception(first);
    return *out;
}

} // namespace qtm::async
