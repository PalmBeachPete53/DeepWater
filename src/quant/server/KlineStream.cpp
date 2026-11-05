// The live candles shared by the chart panes (see KlineStream.h). A Deepwater addition.
#include "KlineStream.h"

#include "BinanceSocket.h"

#include <QJsonObject>
#include <QTimer>

#include <algorithm>
#include <vector>

namespace qtm {

namespace {
const QHash<QString, QString> INTERVALS = {
    {"1s", "1s"}, {"1m", "1m"}, {"3m", "3m"}, {"5m", "5m"}, {"15m", "15m"}, {"30m", "30m"},
    {"1h", "1h"}, {"2h", "2h"}, {"4h", "4h"}, {"6h", "6h"}, {"8h", "8h"}, {"12h", "12h"},
    {"1D", "1d"}, {"3D", "3d"}, {"1W", "1w"}, {"1M", "1M"},
};
} // namespace

QString KlineStreams::interval(const QString &tf)
{
    return INTERVALS.value(tf);
}

// ---------------------------------------------------------------- Binance

KlineSource::Live *BinanceKlineSource::live(QObject *parent, const QString &ticker, const QString &interval,
                                            std::function<void(const Bar &)> bar, std::function<void()> opened,
                                            std::function<void(bool)> closed)
{
    return new BinanceSocket(parent, ticker.toLower() + "@kline_" + interval, false,
        [bar = std::move(bar)](const QJsonObject &o, double) {
            const QJsonObject k = o.value("k").toObject();
            if (k.isEmpty()) return;
            auto num = [&](const char *key) {
                const QJsonValue v = k.value(key);
                return v.isString() ? v.toString().toDouble() : v.toDouble();
            };
            Bar b;
            b.time = k.value("t").toDouble() / 1000;
            b.open = num("o");
            b.high = num("h");
            b.low = num("l");
            b.close = num("c");
            b.volume = num("v");
            bar(b);
        },
        std::move(opened), std::move(closed));
}

// ---------------------------------------------------------------- the stream

KlineStream::KlineStream(std::shared_ptr<KlineSource> source, const QString &ticker, const QString &interval,
                         int reconnectMs, QObject *parent)
    : QObject(parent), m_source(std::move(source)), m_ticker(ticker.toUpper()), m_interval(interval),
      m_reconnectMs(reconnectMs)
{
    m_reconnect = new QTimer(this);
    m_reconnect->setSingleShot(true);
    connect(m_reconnect, &QTimer::timeout, this, &KlineStream::open);
    open();
}

KlineStream::~KlineStream()
{
    close();
}

void KlineStream::start()
{
    if (m_live || m_reconnect->isActive()) return;
    open();
}

void KlineStream::close()
{
    ++m_liveToken;
    m_connected = false;
    if (m_live) {
        m_live->deleteLater();   // maybe from inside its own callback
        m_live = nullptr;
    }
}

void KlineStream::open()
{
    close();
    m_failed = false;
    const int token = m_liveToken;
    m_live = m_source->live(this, m_ticker, m_interval,
        [this, token](const Bar &b) {
            if (token == m_liveToken) emit bar(b);
        },
        [this, token] {
            if (token != m_liveToken) return;
            m_connected = true;
            emit opened();
        },
        [this, token](bool opened) {
            if (token != m_liveToken) return;
            close();
            if (opened) {
                m_reconnect->start(m_reconnectMs);   // dropped: again shortly
            } else {
                m_failed = true;                     // no host: the panes poll instead
                emit failedToOpen();
            }
        });
}

// ---------------------------------------------------------------- the streams

struct KlineStreams::State {
    std::shared_ptr<KlineSource> source;
    int reconnectMs = 3000;
    struct Entry {
        KlineStream *stream = nullptr;
        std::vector<KlineSubscription *> subs;
    };
    QHash<QString, Entry> streams;   // "BTCUSDT|1m"
};

KlineStreams::KlineStreams(std::shared_ptr<KlineSource> source, int reconnectMs) : m_state(std::make_shared<State>())
{
    m_state->source = std::move(source);
    m_state->reconnectMs = reconnectMs;
}

KlineStreams::~KlineStreams()
{
    for (auto &e : m_state->streams) delete e.stream;   // subscriptions still out see a null stream()
}

std::unique_ptr<KlineSubscription> KlineStreams::subscribe(const QString &ticker, const QString &interval)
{
    const QString key = ticker.toUpper() + "|" + interval;
    State::Entry &e = m_state->streams[key];
    if (!e.stream) e.stream = new KlineStream(m_state->source, ticker, interval, m_state->reconnectMs);
    else if (e.stream->failed()) e.stream->start();   // a new pane: try again
    std::unique_ptr<KlineSubscription> s(new KlineSubscription);
    s->m_stream = e.stream;
    QObject::connect(e.stream, &KlineStream::bar, s.get(), &KlineSubscription::bar);
    QObject::connect(e.stream, &KlineStream::opened, s.get(), &KlineSubscription::opened);
    QObject::connect(e.stream, &KlineStream::failedToOpen, s.get(), &KlineSubscription::failed);
    std::weak_ptr<State> weak = m_state;
    s->m_release = [weak, key](KlineSubscription *sub) {
        const std::shared_ptr<State> st = weak.lock();
        if (!st) return;
        auto it = st->streams.find(key);
        if (it == st->streams.end()) return;
        auto &subs = it->subs;
        subs.erase(std::remove(subs.begin(), subs.end(), sub), subs.end());
        if (subs.empty()) {   // the last one: the stream goes once whatever it is doing has returned
            KlineStream *stream = it->stream;
            st->streams.erase(it);
            stream->deleteLater();
        }
    };
    e.subs.push_back(s.get());
    return s;
}

KlineSubscription::~KlineSubscription()
{
    if (m_release) m_release(this);
}

int KlineStreams::streamCount() const
{
    return int(m_state->streams.size());
}

KlineStream *KlineStreams::find(const QString &ticker, const QString &interval) const
{
    const auto it = m_state->streams.constFind(ticker.toUpper() + "|" + interval);
    return it == m_state->streams.constEnd() ? nullptr : it->stream;
}

} // namespace qtm
