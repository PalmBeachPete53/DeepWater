#pragma once

// How a long estimation talks to whoever started it: it reports where it is and checks whether it
// should stop. Both are optional; a default Progress reports nothing and is never cancelled. The
// engine never touches a thread or a widget: the UI layer decides where the callback lands.

#include <QString>

#include <atomic>
#include <functional>
#include <thread>
#include <vector>

namespace markov {

struct Progress {
    std::atomic<bool> *cancel = nullptr;
    std::function<void(int percent, const QString &text)> report;

    bool cancelled() const { return cancel && cancel->load(); }
    void operator()(int percent, const QString &text = {}) const
    {
        if (report)
            report(percent, text);
    }
};

// Runs f(0) … f(n - 1) at once, one thread each, and returns when all are done: the independent starts of
// a fit. f must only share read-only data between calls.
template <typename F>
void parallelFor(int n, F &&f)
{
    std::vector<std::thread> threads;
    for (int i = 1; i < n; ++i)
        threads.emplace_back([&f, i] { f(i); });
    if (n > 0)
        f(0);
    for (std::thread &t : threads)
        t.join();
}

} // namespace markov
