// The live candles the chart panes share (server/KlineStream) on a fake source the test drives: it says when a stream
// opens, sends bars, drops a connection or finds no host. Plain checks, a count at the end, non-zero exit on any failure.
#include "server/KlineStream.h"

#include <QCoreApplication>

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

class FakeSource : public KlineSource
{
public:
    struct FakeLive : QObject {
        FakeLive(QObject *parent, FakeSource *src) : QObject(parent), src(src) {}
        ~FakeLive() override { src->lives.erase(this); }
        FakeSource *src;
        QString key;
        std::function<void(const Bar &)> bar;
        std::function<void()> opened;
        std::function<void(bool)> closed;
    };
    int opens = 0;
    std::set<FakeLive *> lives;

    Live *live(QObject *parent, const QString &ticker, const QString &interval, std::function<void(const Bar &)> bar,
               std::function<void()> opened, std::function<void(bool)> closed) override
    {
        ++opens;
        auto *l = new FakeLive(parent, this);
        l->key = ticker + "|" + interval;
        l->bar = std::move(bar);
        l->opened = std::move(opened);
        l->closed = std::move(closed);
        lives.insert(l);
        return l;
    }
    FakeLive *liveFor(const QString &key)
    {
        for (FakeLive *l : lives)
            if (l->key == key) return l;
        return nullptr;
    }
};

void settle()
{
    for (int i = 0; i < 5; ++i) {
        QCoreApplication::processEvents();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
}

Bar bar(double t, double c)
{
    Bar b;
    b.time = t;
    b.open = b.high = b.low = b.close = c;
    b.volume = 1;
    return b;
}

// What a pane hears.
struct Pane {
    std::unique_ptr<KlineSubscription> sub;
    int bars = 0, opened = 0, failed = 0;
    double last = 0;
    void attach(std::unique_ptr<KlineSubscription> s)
    {
        sub = std::move(s);
        QObject::connect(sub.get(), &KlineSubscription::bar, [this](const Bar &b) {
            ++bars;
            last = b.close;
        });
        QObject::connect(sub.get(), &KlineSubscription::opened, [this] { ++opened; });
        QObject::connect(sub.get(), &KlineSubscription::failed, [this] { ++failed; });
    }
};

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    auto src = std::make_shared<FakeSource>();
    KlineStreams streams(src, 0);

    CAT = "timeframes";
    eq(QString("1m"), KlineStreams::interval("1m"), "1m");
    eq(QString("1d"), KlineStreams::interval("1D"), "1D → 1d");
    eq(QString("1M"), KlineStreams::interval("1M"), "1M (a month) stays");
    check(KlineStreams::interval("2D").isEmpty(), "2D: no Binance interval");

    // ------------------------------------------------ two panes on BTCUSDT 1m: one stream
    CAT = "shared";
    Pane a, b, c;
    a.attach(streams.subscribe("btcusdt", "1m"));
    b.attach(streams.subscribe("BTCUSDT", "1m"));
    eq(1, streams.streamCount(), "one stream");
    eq(1, src->opens, "one socket");
    check(!a.sub->connected(), "not connected yet");
    FakeSource::FakeLive *l = src->liveFor("BTCUSDT|1m");
    check(l != nullptr, "the socket's stream");
    if (!l) return 1;
    l->opened();
    eq(1, a.opened, "first pane: opened");
    eq(1, b.opened, "second pane: opened");
    check(a.sub->connected(), "connected");
    l->bar(bar(60, 101));
    eq(1, a.bars, "first pane: the bar");
    eq(1, b.bars, "second pane: the bar");
    check(a.last == 101 && b.last == 101, "the same bar");

    // ------------------------------------------------ another timeframe: its own stream
    CAT = "timeframe";
    c.attach(streams.subscribe("BTCUSDT", "5m"));
    eq(2, streams.streamCount(), "two streams");
    eq(2, src->opens, "two sockets");
    FakeSource::FakeLive *l5 = src->liveFor("BTCUSDT|5m");
    l5->opened();
    l5->bar(bar(300, 102));
    eq(1, c.bars, "5m pane: its bar");
    eq(1, a.bars, "1m pane: not the 5m bar");

    // ------------------------------------------------ the connection drops: opened again, the panes keep hearing
    CAT = "drop";
    l->closed(true);
    check(!a.sub->connected(), "not connected after the drop");
    settle();   // the reconnect timer (0 ms here, 3 s live) and the old socket's deletion
    eq(3, src->opens, "opened again");
    l = src->liveFor("BTCUSDT|1m");
    check(l != nullptr, "the new socket");
    if (!l) return 1;
    l->opened();
    l->bar(bar(120, 103));
    eq(2, a.bars, "first pane: the bar after the drop");
    eq(2, b.bars, "second pane: the bar after the drop");
    eq(0, a.failed, "no failure");

    // ------------------------------------------------ no host at all: the panes poll; a new pane tries again
    CAT = "no host";
    l5->closed(false);
    eq(1, c.failed, "5m pane: failed (it polls)");
    c.sub.reset();   // what ChartPane does
    settle();
    eq(1, streams.streamCount(), "5m stream gone with its pane");
    Pane d;
    a.sub.reset();
    settle();
    eq(1, streams.streamCount(), "1m stream stays for the second pane");
    d.attach(streams.subscribe("BTCUSDT", "5m"));
    eq(4, src->opens, "a new 5m pane: a new try");
    // A pane joining a stream that found no host while another still holds it: a new try too.
    src->liveFor("BTCUSDT|5m")->closed(false);
    eq(1, d.failed, "failed again");
    check(streams.find("BTCUSDT", "5m")->failed(), "the stream is idle");
    Pane e;
    e.attach(streams.subscribe("BTCUSDT", "5m"));
    eq(5, src->opens, "joining an idle stream starts it again");
    check(!streams.find("BTCUSDT", "5m")->failed(), "trying");
    e.sub.reset();

    // ------------------------------------------------ the last panes leave
    CAT = "close";
    b.sub.reset();
    d.sub.reset();
    settle();
    eq(0, streams.streamCount(), "no stream");
    check(src->lives.empty(), "no socket");

    std::cout << checks << " checks, " << fails << " failed\n";
    return fails ? 1 : 0;
}
