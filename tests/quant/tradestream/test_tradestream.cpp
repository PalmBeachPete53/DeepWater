// The trades the chart panes share (server/TradeStream) and each pane's side of them (ui/TradeFeed), on a fake source:
// trade i is at T0 + i/10 s, the answers wait until the test lets them through (as the network would), and the live
// trades come when the test says. Plain checks, a count at the end, non-zero exit on any failure.
#include "server/TradeStream.h"
#include "ui/TradeFeed.h"

#include <QCoreApplication>
#include <QPointer>

#include <deque>
#include <functional>
#include <iostream>
#include <set>

using namespace qtm;
using footprint::Book;

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
void near(double want, double got, const QString &ctx)
{
    check(std::fabs(want - got) <= 1e-9 * std::max(1.0, std::fabs(want)),
          QStringLiteral("%1: want %2, got %3").arg(ctx).arg(want, 0, 'f', 6).arg(got, 0, 'f', 6));
}

constexpr double T0 = 1000020;   // trade 0's time (not on a minute: candles start mid-way)
constexpr double TF = 60;

class FakeSource : public TradeSource
{
public:
    struct FakeLive : Live {
        FakeLive(QObject *parent, FakeSource *src) : Live(parent), src(src) {}
        ~FakeLive() override { src->lives.erase(this); }
        FakeSource *src;
        bool us = false;
        std::function<void(const TapeTrade &)> trade;
        std::function<void(bool)> closed;
    };

    qint64 newest = 9999;   // the market's latest trade
    bool us = false;
    int failNext = 0;       // that many requests fail
    int tickCalls = 0, pageCalls = 0, liveOpens = 0;
    std::set<FakeLive *> lives;
    std::deque<std::function<void()>> answers;

    static TapeTrade at(qint64 id)
    {
        TapeTrade r;
        r.id = id;
        r.t.time = T0 + double(id) / 10;
        r.t.seq = id;
        r.t.price = 100 + double(id % 7) * 0.5;
        r.t.qty = 1 + double(id % 3);
        r.t.side = id % 2 ? footprint::Side::Buy : footprint::Side::Sell;
        return r;
    }

    void tickSize(QObject *context, const QString &, std::function<void(double)> ok, Fail fail) override
    {
        ++tickCalls;
        QPointer<QObject> g(context);
        const bool bad = failNext > 0 && failNext--;
        answers.push_back([g, ok, fail, bad] {
            if (!g) return;
            if (bad) fail("tick: no answer");
            else ok(0.5);
        });
    }
    void page(QObject *context, const QString &, qint64 fromId, int limit, std::function<void(const Page &)> ok,
              Fail fail) override
    {
        ++pageCalls;
        QPointer<QObject> g(context);
        const bool bad = failNext > 0 && failNext--;
        answers.push_back([this, g, fromId, limit, ok, fail, bad] {
            if (!g) return;
            if (bad) {
                fail("page: no answer");
                return;
            }
            Page p;
            p.us = us;
            const qint64 from = fromId < 0 ? std::max<qint64>(0, newest - limit + 1) : fromId;
            for (qint64 id = from; id <= newest && id < from + limit; ++id) p.trades.push_back(at(id));
            ok(p);
        });
    }
    Live *live(QObject *parent, const QString &, bool liveUs, std::function<void(const TapeTrade &)> trade,
               std::function<void(bool)> closed) override
    {
        ++liveOpens;
        auto *l = new FakeLive(parent, this);
        l->us = liveUs;
        l->trade = std::move(trade);
        l->closed = std::move(closed);
        lives.insert(l);
        return l;
    }

    // `n` new trades on the market; the live streams get them, but those in `missed` (a dropped message).
    void trade(int n, const std::set<qint64> &missed = {})
    {
        for (int k = 0; k < n; ++k) {
            ++newest;
            if (missed.count(newest)) continue;
            const std::set<FakeLive *> now = lives;
            for (FakeLive *l : now)
                if (lives.count(l)) l->trade(at(newest));
        }
    }
    // Every answer, timer and deferred deletion, until nothing is left.
    void settle()
    {
        for (int i = 0; i < 100000; ++i) {
            QCoreApplication::processEvents();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            if (answers.empty()) {
                QCoreApplication::processEvents();
                if (answers.empty()) return;
            }
            auto a = std::move(answers.front());
            answers.pop_front();
            a();
        }
        check(false, "settle: still busy");
    }
};

bool contiguous(const std::deque<TapeTrade> &t)
{
    for (size_t i = 1; i < t.size(); ++i)
        if (t[i].id != t[i - 1].id + 1) return false;
    return true;
}

// The pane's footprint, built bit by bit (history newest first, live, trims), against its trades built at once.
bool sameAsRebuilt(const TradeFeed &f, double tf = TF)
{
    Book b(footprint::TickGrid{f.tickSize(), f.book().grid().rowTicks});
    for (const TapeTrade &t : f.tape()) b.add(footprint::candleOpen(t.t.time, tf), t.t, {t.t.side, false});
    const Book &a = f.book();
    if (a.tradeCount() != b.tradeCount() || a.candles().size() != b.candles().size()) return false;
    for (const auto &[ct, x] : a.candles()) {
        const footprint::CandleFootprint *y = b.candle(ct);
        if (!y || y->trades() != x.trades() || y->lowRow() != x.lowRow() || y->highRow() != x.highRow()
            || y->openTick() != x.openTick() || y->closeTick() != x.closeTick()
            || std::fabs(y->bidVolume() - x.bidVolume()) > 1e-9 || std::fabs(y->askVolume() - x.askVolume()) > 1e-9)
            return false;
        for (int64_t row = x.lowRow(); row <= x.highRow(); ++row)
            if (std::fabs(y->level(row).bid - x.level(row).bid) > 1e-9 || std::fabs(y->level(row).ask - x.level(row).ask) > 1e-9)
                return false;
    }
    return true;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    auto src = std::make_shared<FakeSource>();
    TradeStreams streams(src, TradeStream::Timing{0, 0, 0});
    const double start0 = FakeSource::at(0).t.time;   // the charts reach back to the pair's first trade

    // ------------------------------------------------ one pane: history newest first, up to the budget
    CAT = "history";
    auto *a = new TradeFeed("btcusdt", TF, start0, 5000, nullptr, &streams);
    src->settle();
    TradeStream *s = streams.find("BTCUSDT");
    check(s != nullptr, "the pair has a stream");
    eq(1, streams.streamCount(), "one stream");
    eq(1, src->tickCalls, "tick size asked once");
    eq(5, src->pageCalls, "latest page + one batch of 4 back");
    eq(1, src->liveOpens, "live stream opened once");
    eq<size_t>(5000, s->tape().size(), "the tape holds the budget");
    eq<qint64>(5000, s->tape().front().id, "oldest trade: 10,000 - 5,000");
    eq<qint64>(9999, s->tape().back().id, "newest trade");
    check(contiguous(s->tape()), "no gap in the tape");
    check(!s->loadingHistory(), "history done (budget)");
    check(a->ready(), "pane ready");
    check(!a->loadingHistory(), "pane: history done");
    eq<size_t>(5000, a->book().tradeCount(), "pane: the budget's trades");
    eq<size_t>(5000, a->tape().size(), "pane: its tape");
    near(footprint::candleOpen(FakeSource::at(5000).t.time, TF) + TF, a->completeFrom(),
         "complete from the candle after the oldest trade's");
    check(sameAsRebuilt(*a), "footprint = rebuilt");

    // ------------------------------------------------ live trades, a missed one fetched from REST
    CAT = "live";
    src->trade(10);
    src->settle();
    eq<qint64>(10009, s->tape().back().id, "live trades in");
    eq<size_t>(5010, a->book().tradeCount(), "pane: live trades in");
    src->trade(20, {10011, 10012, 10013, 10014, 10015});
    src->settle();
    eq(6, src->pageCalls, "the gap: one REST page");
    eq<qint64>(10029, s->tape().back().id, "every trade in after the gap");
    check(contiguous(s->tape()), "no gap after the missed messages");
    eq<size_t>(5030, a->book().tradeCount(), "pane: every trade");
    check(sameAsRebuilt(*a), "footprint = rebuilt after live trades");

    // ------------------------------------------------ a second pane on the pair: the same stream
    CAT = "shared";
    const quint64 genA = a->tapeGeneration();
    auto *b = new TradeFeed("BTCUSDT", 5 * TF, start0, 5000, nullptr, &streams);
    src->settle();
    eq(1, streams.streamCount(), "still one stream");
    eq(1, src->liveOpens, "still one live stream");
    eq(6, src->pageCalls, "no history fetched again");
    check(b->ready(), "second pane ready at once");
    eq<size_t>(5000, b->book().tradeCount(), "second pane: its budget, newest first");
    eq<qint64>(10029, b->tape().back().id, "second pane: up to the newest");
    eq<qint64>(5030, b->tape().front().id, "second pane: back to its budget");
    check(genA == a->tapeGeneration(), "first pane unaffected");
    check(a->tapeGeneration() != b->tapeGeneration(), "each pane its own generation");
    check(sameAsRebuilt(*b, 5 * TF), "second pane's footprint (5 min candles) = rebuilt");

    // ------------------------------------------------ a pane wanting more history: the stream goes further back
    CAT = "more history";
    auto *c = new TradeFeed("BTCUSDT", TF, start0, 8000, nullptr, &streams);
    src->settle();
    eq(10, src->pageCalls, "one more batch of 4");
    check(s->tape().size() >= 8000, "the tape holds the larger budget");
    check(contiguous(s->tape()), "no gap");
    eq<size_t>(8000, c->book().tradeCount(), "third pane: its budget");
    eq<size_t>(5030, a->book().tradeCount(), "first pane keeps its own budget");
    eq<qint64>(5000, a->tape().front().id, "first pane's tape unchanged");
    check(sameAsRebuilt(*c), "third pane's footprint = rebuilt");

    // ------------------------------------------------ a pane whose first bar is recent: its history is all there
    CAT = "first bar";
    const double start9500 = FakeSource::at(9500).t.time;
    auto *d = new TradeFeed("BTCUSDT", TF, start9500, 1000, nullptr, &streams);
    src->settle();
    eq(10, src->pageCalls, "nothing fetched");
    eq<size_t>(530, d->book().tradeCount(), "trades from its first bar: 9,500 to 10,029");
    near(start9500, d->completeFrom(), "complete from its first bar");
    check(!d->loadingHistory(), "no history to load");

    // ------------------------------------------------ the app hidden: every pane stops, the stream too
    CAT = "pause";
    for (TradeFeed *f : {a, b, c, d}) f->stop();
    src->settle();
    check(!s->running(), "stream stopped");
    check(src->lives.empty(), "live stream closed");
    src->trade(50);   // while away
    for (TradeFeed *f : {a, b, c, d}) f->resume();
    src->settle();
    eq(2, src->liveOpens, "live stream opened again");
    src->trade(1);
    src->settle();
    eq<qint64>(10080, s->tape().back().id, "caught up");
    check(contiguous(s->tape()), "no gap after the pause");
    eq<size_t>(5081, a->book().tradeCount(), "pane: the trades from while away");
    check(sameAsRebuilt(*a), "footprint = rebuilt after the pause");

    // ------------------------------------------------ one pane paused (replay), the others live
    CAT = "one paused";
    b->stop();
    src->trade(5);
    src->settle();
    check(s->running(), "stream still running for the others");
    eq<qint64>(10080, b->tape().back().id, "paused pane: frozen");
    eq<qint64>(10085, a->tape().back().id, "live pane: on");
    b->resume();
    eq<qint64>(10085, b->tape().back().id, "paused pane caught up on resume");
    check(sameAsRebuilt(*b, 5 * TF), "paused pane's footprint = rebuilt");

    // ------------------------------------------------ live: the footprints keep their budget, whole candles
    CAT = "trim";
    src->trade(4000);
    src->settle();
    check(a->book().tradeCount() <= 5000 + 5000 / 2, "first pane: never past one and a half times its budget");
    check(a->tape().front().id > 5000, "first pane: its oldest candles went");
    check(a->tape().front().t.time >= a->book().candles().begin()->first, "first pane: whole candles went");
    eq<size_t>(a->book().tradeCount(), a->tape().size(), "first pane's tape = its footprints' trades");
    check(sameAsRebuilt(*a), "footprint = rebuilt after the trim");
    check(a->completeFrom() >= a->book().candles().begin()->first, "complete from its first candle on, at least");
    check(s->tape().size() <= 2 * 8000, "the tape: never past twice the largest budget");
    for (TradeFeed *f : {a, b, c, d}) check(s->tape().front().id <= f->tape().front().id, "the tape holds every pane's trades");

    // ------------------------------------------------ the chart drops its oldest bars
    CAT = "chart start";
    const double cut = footprint::candleOpen(FakeSource::at(13000).t.time, TF);
    c->setChartStart(cut);
    src->settle();
    check(c->book().candles().begin()->first >= cut, "candles before the cut gone");
    check(c->tape().front().t.time >= cut, "its tape too");
    check(sameAsRebuilt(*c), "footprint = rebuilt after the cut");

    // ------------------------------------------------ away for hours: a gap longer than the budget starts over
    CAT = "reset";
    const quint64 streamGen = s->generation(), gen = a->tapeGeneration();
    for (TradeFeed *f : {a, b, c, d}) f->stop();
    src->trade(20000);
    for (TradeFeed *f : {a, b, c, d}) f->resume();
    src->settle();
    src->trade(1);
    src->settle();
    check(s->generation() != streamGen, "stream: new generation");
    check(a->tapeGeneration() != gen, "pane: new generation");
    eq<qint64>(src->newest, s->tape().back().id, "the latest trades");
    check(contiguous(s->tape()), "no gap");
    eq<size_t>(5000, a->book().tradeCount(), "pane refilled to its budget");
    check(sameAsRebuilt(*a), "footprint = rebuilt after starting over");

    // ------------------------------------------------ the last pane leaves: the stream closes
    CAT = "close";
    delete a;
    delete b;
    delete c;
    src->settle();
    eq(1, streams.streamCount(), "one pane left: the stream stays");
    delete d;
    src->settle();
    eq(0, streams.streamCount(), "no pane: no stream");
    check(src->lives.empty(), "live stream closed");

    // ------------------------------------------------ binance.us, and requests that fail once
    CAT = "us";
    src->us = true;
    src->failNext = 2;
    auto *e = new TradeFeed("ETHUSD", TF, start0, 1000, nullptr, &streams);
    src->settle();
    check(e->ready(), "ready after failed requests");
    check(e->error().isEmpty(), "error cleared");
    eq<size_t>(1, src->lives.size(), "one live stream");
    check(!src->lives.empty() && (*src->lives.begin())->us, "binance.us's live stream");
    delete e;
    src->settle();
    eq(0, streams.streamCount(), "closed");

    std::cout << checks << " checks, " << fails << " failed\n";
    return fails ? 1 : 0;
}
