// The order book depth the chart panes share (server/DepthStream) and each pane's side of it (ui/DepthFeed), on a fake
// source: tick 0.01 around a price of 100, update ids one by one, a book update every 50 ms of real time (the samplers
// need the clock to move), the answers on the stream's thread as the network's would be. Plain checks, a count at the
// end, non-zero exit on any failure.
#include "server/DepthStream.h"
#include "ui/DepthFeed.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QMutex>
#include <QPointer>
#include <QTemporaryDir>
#include <QTimer>

#include <atomic>
#include <cstring>
#include <functional>
#include <iostream>
#include <set>

using namespace qtm;

namespace {

int checks = 0, fails = 0;
QString CAT = "misc";

void check(bool ok, const QString &msg)
{
    ++checks;
    if (!ok) {
        ++fails;
        std::cout << "FAIL [" << CAT.toStdString() << "] " << msg.toStdString() << "\n";
    }
}
template <typename T>
void eq(T want, T got, const QString &ctx)
{
    check(want == got, QStringLiteral("%1: want %2, got %3").arg(ctx).arg(want).arg(got));
}

constexpr double TICK = 0.01;

class FakeSource : public DepthSource
{
public:
    struct FakeLive : Live {
        FakeLive(QObject *parent, FakeSource *src) : Live(parent), src(src) {}
        ~FakeLive() override
        {
            QMutexLocker lock(&src->mutex);
            src->lives.erase(this);
        }
        FakeSource *src;
        std::function<void(const depth::DepthUpdate &, double)> update;
    };

    std::atomic<int> configCalls{0}, snapshotCalls{0}, liveOpens{0};
    std::atomic<int64_t> lastId{1000};
    QMutex mutex;
    std::set<FakeLive *> lives;

    static std::vector<depth::Level> side(int64_t from, int step, int64_t id)
    {
        std::vector<depth::Level> out;
        for (int k = 0; k < 20; ++k) out.push_back({from + step * k, 1 + double((id + k) % 9)});
        return out;
    }

    void config(QObject *context, const QString &, std::function<void(const Config &)> ok, Fail) override
    {
        ++configCalls;
        QMetaObject::invokeMethod(context, [ok] { ok(Config{TICK, false, 100}); }, Qt::QueuedConnection);
    }
    void snapshot(QObject *context, const QString &, double, int, std::function<void(const Snapshot &)> ok, Fail) override
    {
        ++snapshotCalls;
        QMetaObject::invokeMethod(context, [this, ok] {
            Snapshot s;
            s.book.lastUpdateId = lastId.load();
            s.book.bids = side(9999, -1, s.book.lastUpdateId);   // 99.99 and down
            s.book.asks = side(10001, 1, s.book.lastUpdateId);   // 100.01 and up
            ok(s);
        }, Qt::QueuedConnection);
    }
    Live *live(QObject *parent, const QString &, bool, double, std::function<void(const depth::DepthUpdate &, double)> update,
               std::function<void(bool)>) override
    {
        ++liveOpens;
        auto *l = new FakeLive(parent, this);
        l->update = std::move(update);
        QMutexLocker lock(&mutex);
        lives.insert(l);
        return l;
    }
    int liveCount()
    {
        QMutexLocker lock(&mutex);
        return int(lives.size());
    }
    // One book update, now, to every live stream (on its thread).
    void push()
    {
        depth::DepthUpdate u;
        u.firstId = u.lastId = ++lastId;
        u.time = double(QDateTime::currentMSecsSinceEpoch()) / 1000;
        u.bids = side(9999, -1, u.lastId);
        u.asks = side(10001, 1, u.lastId);
        QMutexLocker lock(&mutex);
        for (FakeLive *l : lives) {
            auto f = l->update;
            QMetaObject::invokeMethod(l, [f, u] { f(u, u.time * 1000); }, Qt::QueuedConnection);
        }
    }
};

FakeSource *g_src = nullptr;

// Runs the event loop (book updates every 50 ms) until `done` or `ms` have passed.
bool pumpUntil(const std::function<bool()> &done, int ms)
{
    QElapsedTimer t;
    t.start();
    QElapsedTimer lastPush;
    lastPush.start();
    while (t.elapsed() < ms) {
        if (lastPush.elapsed() >= 50) {
            g_src->push();
            lastPush.restart();
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        if (done && done()) return true;
    }
    return done ? done() : true;
}
void pump(int ms) { pumpUntil({}, ms); }

bool sameColumn(const heatmap::ColumnStore &a, uint64_t i, const heatmap::ColumnStore &b, uint64_t j)
{
    const auto &ma = a.meta(i), &mb = b.meta(j);
    return ma.time == mb.time && ma.anchorRow == mb.anchorRow && ma.bestBid == mb.bestBid && ma.bestAsk == mb.bestAsk
           && std::memcmp(a.rows(i), b.rows(j), sizeof(heatmap::Code) * size_t(a.bandRows())) == 0;
}

// What the two stores have in common (by time) is the same, column for column.
bool sameOverlap(const heatmap::ColumnStore *a, const heatmap::ColumnStore *b, size_t *compared)
{
    *compared = 0;
    if (!a || !b || a->empty() || b->empty()) return false;
    for (uint64_t j = b->firstIndex(); j < b->endIndex(); ++j) {
        const uint64_t i = a->lowerBound(b->meta(j).time);
        if (i == a->endIndex() || a->meta(i).time != b->meta(j).time) continue;
        if (!sameColumn(*a, i, *b, j)) return false;
        ++*compared;
    }
    return *compared > 0;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir root;
    auto src = std::make_shared<FakeSource>();
    g_src = src.get();
    std::unique_ptr<DepthStreams> streams = std::make_unique<DepthStreams>(src, root.path());
    auto params = [](int iv, int stableMs = 0, bool record = false) {
        DepthFeed::Params p;
        p.intervalMs = iv;
        p.historyMinutes = 10;
        p.stableMs = stableMs;
        p.record = record;
        return p;
    };

    // ------------------------------------------------ two panes on a pair at one interval: one stream, one store
    CAT = "shared";
    auto *a = new DepthFeed("btcusdt", params(250, 0, true), nullptr, streams.get());
    auto *b = new DepthFeed("BTCUSDT", params(250, 500, true), nullptr, streams.get());
    const bool live = pumpUntil([&] { return a->rawStore() && a->rawStore()->size() >= 8 && a->stats().synced; }, 8000);
    check(live, "columns come in, the book is synced");
    eq(1, streams->streamCount(), "one stream");
    eq(1, src->configCalls.load(), "configured once");
    eq(1, src->liveOpens.load(), "one live stream");
    eq(1, src->liveCount(), "one socket open");
    check(a->rawStore() == b->rawStore(), "the same samples");
    check(a->rawId() == b->rawId() && a->rawId() != 0, "the same samples' id");
    check(a->tickSize() == TICK, "tick size");
    check(a->store() == a->rawStore(), "no filter: the samples are shown");
    check(b->store() != b->rawStore() && b->store() && !b->store()->empty(), "stability filter: a store of its own");
    check(a->pyramid() && b->pyramid() && a->pyramid() != b->pyramid(), "a pyramid each");
    check(a->storeId() != b->storeId(), "a display id each");

    // ------------------------------------------------ one recorder per pair
    CAT = "recording";
    pump(800);   // stats every half second
    check(a->stats().recording, "recording");
    check(a->stats().recordNote.isEmpty(), "no 'another chart' note: " + a->stats().recordNote);
    const QString dir = DepthStream::recordDir(root.path(), "BTCUSDT", false);
    eq<qsizetype>(1, QDir(dir).entryList({"*.dwdepth"}, QDir::Files).size(), "one file");

    // ------------------------------------------------ another interval: another sampler and store, the same socket
    CAT = "intervals";
    auto *c = new DepthFeed("BTCUSDT", params(1000), nullptr, streams.get());
    check(pumpUntil([&] { return c->rawStore() && c->rawStore()->size() >= 2; }, 6000), "1 s columns come in");
    eq(1, src->liveOpens.load(), "still one live stream");
    eq(1, src->configCalls.load(), "still configured once");
    check(c->rawStore() != a->rawStore(), "a store of its own for 1 s");
    check(c->rawStore()->meta(c->rawStore()->endIndex() - 1).time - c->rawStore()->meta(c->rawStore()->firstIndex()).time
              >= 1.0 * double(c->rawStore()->size() - 1) - 1e-9,
          "its columns are 1 s apart");

    // ------------------------------------------------ a pane joining late: what is there shows at once
    CAT = "late";
    pump(1500);
    const size_t before = a->rawStore()->size();
    auto *d = new DepthFeed("BTCUSDT", params(250, 500), nullptr, streams.get());
    check(d->rawStore() == a->rawStore() && d->rawStore()->size() >= before, "the samples there already");
    check(d->store() && d->store()->size() + 1 >= before, "its filtered store built at once");
    pump(1500);
    size_t compared = 0;
    check(sameOverlap(b->store(), d->store(), &compared), "filter built at once = built bit by bit");
    check(compared >= 10, QStringLiteral("columns compared: %1").arg(compared));

    // ------------------------------------------------ the app hidden: every pane stops, the socket closes
    CAT = "pause";
    for (DepthFeed *f : {a, b, c, d}) f->stop();
    check(pumpUntil([&] { return src->liveCount() == 0; }, 3000), "socket closed");
    const size_t frozen = a->rawStore()->size();
    pump(1000);
    eq(frozen, a->rawStore()->size(), "no columns while stopped");
    check(a->rawStore() != nullptr, "the samples stay (replay shows them)");
    // one pane back: the stream runs for it
    a->resume();
    check(pumpUntil([&] { return src->liveCount() == 1 && a->rawStore()->size() > frozen + 2; }, 6000), "live again");
    eq(2, src->liveOpens.load(), "socket opened again");
    c->resume();   // another interval resumes too
    b->resume();
    d->resume();
    pump(500);
    eq(1, src->liveCount(), "still one socket");

    // ------------------------------------------------ the last pane leaves: the stream goes
    CAT = "close";
    delete a;
    delete b;
    delete d;
    pump(300);
    eq(1, streams->streamCount(), "one pane left: the stream stays");
    delete c;
    check(pumpUntil([&] { return streams->streamCount() == 0 && src->liveCount() == 0; }, 3000), "no pane: no stream");

    // ------------------------------------------------ a pane starting in replay: the recordings, no socket
    CAT = "replay";
    const int opens = src->liveOpens.load();
    auto *r = new DepthFeed("BTCUSDT", params(250), nullptr, streams.get());
    r->stop();   // what ChartPane does in replay
    check(pumpUntil([&] { return r->rawStore() && !r->rawStore()->empty(); }, 5000), "the recorded columns shown");
    check(r->stats().loadedFiles >= 1, "read from the recordings");
    eq(opens, src->liveOpens.load(), "no socket opened");
    delete r;
    pump(300);
    eq(0, streams->streamCount(), "closed");

    streams.reset();
    std::cout << checks << " checks, " << fails << " failed\n";
    return fails ? 1 : 0;
}
