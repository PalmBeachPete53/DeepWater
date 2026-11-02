#include "Async.h"

namespace qtm::async {

QThreadPool *pool()
{
    // Most tasks wait on the network, so the pool is wide. A task nobody picked up runs on the
    // thread that waits for it (Task::get), so the size is a speed limit, never a deadlock.
    static QThreadPool *p = [] {
        auto *tp = new QThreadPool;
        tp->setMaxThreadCount(48);
        tp->setExpiryTimeout(60000);
        return tp;
    }();
    return p;
}

QThreadPool *requestPool()
{
    static QThreadPool *p = [] {
        // Node answers every request the page sends at once; at start-up the Charts page fires
        // dozens (quotes, screeners, every pane's bars ...), each waiting on the network, so a
        // narrow pool queued them behind each other. The threads only wait, so many are cheap.
        auto *tp = new QThreadPool;
        tp->setMaxThreadCount(64);
        tp->setExpiryTimeout(60000);
        return tp;
    }();
    return p;
}

} // namespace qtm::async
