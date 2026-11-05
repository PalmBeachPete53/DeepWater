// Binance trades shared by the chart panes (see TradeStream.h). A Deepwater addition.
#include "TradeStream.h"

#include "Async.h"
#include "BinanceSocket.h"
#include "Server.h"
#include "../core/Error.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

#include <algorithm>
#include <memory>

namespace qtm {

namespace {

// `work` on the UI's request pool, which the backend waits for before it goes (so no call outlives it); its result, or the message of what it threw, back on the UI thread if `context` still exists
// (as ui/Api.cpp's api::run).
template <typename T>
void offThread(QObject *context, std::function<T()> work, std::function<void(const T &)> ok, TradeSource::Fail fail)
{
    QPointer<QObject> guard(context);
    async::requestPool()->start([guard, work = std::move(work), ok = std::move(ok), fail = std::move(fail)] {
        T result{};
        QString error;
        bool good = true;
        try {
            result = work();
        } catch (const Error &e) {
            good = false;
            error = e.message();
        } catch (const std::exception &e) {
            good = false;
            error = QString::fromUtf8(e.what());
        }
        QObject *app = QCoreApplication::instance();
        if (!app) return;
        QMetaObject::invokeMethod(app, [guard, good, result, error, ok, fail] {
            if (!guard) return;
            if (good) ok(result);
            else if (fail) fail(error);
        }, Qt::QueuedConnection);
    });
}

} // namespace

// ---------------------------------------------------------------- Binance

TapeTrade BinanceTradeSource::parse(const QJsonObject &o)
{
    auto num = [&](const char *key) {
        const QJsonValue v = o.value(key);
        return v.isString() ? v.toString().toDouble() : v.toDouble();
    };
    TapeTrade r;
    r.id = o.value("a").toInteger(-1);
    r.t.time = o.value("T").toDouble() / 1000;
    r.t.seq = r.id;
    r.t.price = num("p");
    r.t.qty = num("q");
    r.t.side = footprint::binanceSide(o.value("m").toBool());
    return r;
}

void BinanceTradeSource::tickSize(QObject *context, const QString &ticker, std::function<void(double)> ok, Fail fail)
{
    Backend *b = m_backend;
    offThread<double>(context, [b, ticker] { return b->binanceTickSize(ticker); }, std::move(ok), std::move(fail));
}

void BinanceTradeSource::page(QObject *context, const QString &ticker, qint64 fromId, int limit,
                              std::function<void(const Page &)> ok, Fail fail)
{
    Backend *b = m_backend;
    offThread<Page>(context, [b, ticker, fromId, limit] {
        Page p;
        const QJsonArray arr = b->binanceAggTrades(ticker, fromId, limit, &p.us);
        p.trades.reserve(size_t(arr.size()));
        for (const QJsonValue v : arr) p.trades.push_back(parse(v.toObject()));   // parsed here, off the UI thread
        return p;
    }, std::move(ok), std::move(fail));
}

TradeSource::Live *BinanceTradeSource::live(QObject *parent, const QString &ticker, bool us,
                                            std::function<void(const TapeTrade &)> trade,
                                            std::function<void(bool)> closed)
{
    return new BinanceSocket(parent, ticker.toLower() + "@aggTrade", us,
        [trade = std::move(trade)](const QJsonObject &o, double) {
            if (o.value("e").toString() == "aggTrade") trade(parse(o));
        },
        {}, std::move(closed));
}

// ---------------------------------------------------------------- the stream

TradeStream::TradeStream(std::shared_ptr<TradeSource> source, const QString &ticker, Timing timing, QObject *parent)
    : QObject(parent), m_source(std::move(source)), m_ticker(ticker.toUpper()), m_timing(timing)
{
    m_retry = new QTimer(this);
    m_retry->setSingleShot(true);
    m_gen = 1;
}

TradeStream::~TradeStream()
{
    stop();
}

void TradeStream::setError(const QString &e)
{
    if (m_error == e) return;
    m_error = e;
    emit stateChanged();
}

void TradeStream::retry(void (TradeStream::*what)())
{
    m_retry->disconnect();
    connect(m_retry, &QTimer::timeout, this, what);
    m_retry->start(m_timing.retryMs);
}

void TradeStream::setNeeds(double start, size_t budget, bool active)
{
    budget = std::max<size_t>(budget, PAGE);
    const double oldStart = m_start;
    const size_t oldBudget = m_budget;
    m_start = start;
    m_budget = budget;
    if (start > oldStart) {   // every pane dropped its oldest bars: the trades before go
        const size_t before = m_tape.size();
        while (!m_tape.empty() && m_tape.front().t.time < start) m_tape.pop_front();
        if (m_tape.size() != before) emit changed();
    }
    if (active && m_stopped) resume();
    else if (!active && !m_stopped) stop();
    // A pane wants more history than the others did: an earlier first bar, a larger budget.
    if (start < oldStart || budget > oldBudget) {
        if (start < oldStart && ready()) {
            // The trades between the new start and the old one were fetched but not kept: fetch them again.
            m_loId = m_tape.empty() ? m_hiId + 1 : m_tape.front().id;
            m_oldestTime = m_tape.empty() ? NAN : m_tape.front().t.time;
        }
        backfillIfShort();
    }
}

// The history goes on (or again) from where it is, any batch on the way dropped (it was for the old needs).
void TradeStream::backfillIfShort()
{
    if (!ready() || m_stopped) return;   // not started: fetchLatest() starts it with the needs as they are then
    ++m_backToken;
    const bool was = m_backfilling;
    m_backfilling = true;
    fetchBack();
    if (m_backfilling != was) emit stateChanged();
}

void TradeStream::stop()
{
    m_stopped = true;
    ++m_token;
    m_forwardBusy = false;
    m_retry->stop();
    m_retry->disconnect();
    closeLive();
}

void TradeStream::resume()
{
    if (!m_stopped) return;
    m_stopped = false;
    if (!m_tickKnown) {
        fetchTickSize();
        return;
    }
    if (m_exchangeKnown) openLive();
    if (m_hiId < 0) fetchLatest();
    else if (m_backfilling) fetchBack();
    // A gap left while stopped shows with the first live trade (its id skips ahead): drain() fetches it.
}

// ---------------------------------------------------------------- requests

void TradeStream::fetchTickSize()
{
    const int token = m_token;
    m_source->tickSize(this, m_ticker,
        [this, token](double tick) {
            if (token != m_token) return;
            m_tick = tick;
            m_tickKnown = true;
            setError(QString());
            emit stateChanged();
            fetchLatest();   // opens the live stream once it knows which exchange answered
        },
        [this, token](const QString &e) {
            if (token != m_token) return;
            setError(e);
            retry(&TradeStream::fetchTickSize);
        });
}

void TradeStream::fetchLatest()
{
    const int token = m_token;
    m_source->page(this, m_ticker, -1, PAGE,
        [this, token](const TradeSource::Page &p) {
            if (token != m_token) return;
            if (!m_exchangeKnown || p.us != m_us || !m_live) {
                m_us = p.us;
                m_exchangeKnown = true;
                closeLive();
                m_pending.clear();
                openLive();
            }
            if (p.trades.empty()) {
                setError("No trades from Binance for " + m_ticker);
                return;
            }
            for (const TapeTrade &t : p.trades) {
                if (m_hiId >= 0 && t.id != m_hiId + 1) continue;   // ids go up one by one
                if (m_loId < 0) {
                    m_loId = t.id;
                    m_oldestTime = t.t.time;
                }
                if (keep(t)) m_tape.push_back(t);
                m_hiId = t.id;
            }
            m_backfilling = true;
            ++m_backToken;
            setError(QString());
            emit stateChanged();
            drain();
            emit changed();
            const int back = m_backToken;
            QTimer::singleShot(m_timing.pageGapMs, this, [this, token, back] {
                if (token == m_token && back == m_backToken) fetchBack();
            });
        },
        [this, token](const QString &e) {
            if (token != m_token) return;
            setError(e);
            retry(&TradeStream::fetchLatest);
        });
}

// Further back in time, until the budget or the earliest first bar: ids are consecutive, so the next pages are known
// in advance and asked for together (each request waits ~0.3 s on the network, not on Binance's limits).
void TradeStream::fetchBack()
{
    if (m_stopped || !m_backfilling) return;
    const bool reachedStart = !std::isnan(m_oldestTime) && m_oldestTime <= m_start;
    if (m_loId <= 0 || reachedStart || m_tape.size() >= m_budget) {
        m_backfilling = false;
        emit stateChanged();
        return;
    }
    struct Batch {
        std::vector<std::pair<qint64, int>> pages;   // fromId, limit
        std::vector<std::vector<TapeTrade>> got;
        int left = 0;
        QString error;
    };
    auto batch = std::make_shared<Batch>();
    for (int k = 0; k < PARALLEL_PAGES; ++k) {
        const qint64 to = m_loId - qint64(k) * PAGE;   // exclusive
        if (to <= 0) break;
        const qint64 from = std::max<qint64>(0, to - PAGE);
        batch->pages.push_back({from, int(to - from)});
    }
    batch->got.resize(batch->pages.size());
    batch->left = int(batch->pages.size());
    const int token = m_token, back = m_backToken;
    auto finish = [this, token, back, batch] {
        if (token != m_token || back != m_backToken) return;
        if (!batch->error.isEmpty()) {
            setError(batch->error);
            retry(&TradeStream::fetchBack);
            return;
        }
        const qint64 below = m_loId;
        const qint64 floor = batch->pages.back().first;
        qint64 lo = below;
        double oldest = NAN;
        std::vector<TapeTrade> older;
        for (const auto &page : batch->got)
            for (const TapeTrade &t : page) {
                if (t.id < floor || t.id >= below) continue;
                if (t.id < lo) {
                    lo = t.id;
                    oldest = t.t.time;
                }
                if (keep(t)) older.push_back(t);
            }
        // the pages arrive in any order: onto the tape's front newest first, so it stays in id order
        std::sort(older.begin(), older.end(), [](const TapeTrade &a, const TapeTrade &b) { return a.id < b.id; });
        for (auto it = older.rbegin(); it != older.rend(); ++it) m_tape.push_front(*it);
        if (lo == below) {   // nothing older: the pair's first trade
            m_loId = 0;
        } else {
            m_loId = lo;
            m_oldestTime = oldest;
        }
        if (!m_error.isEmpty()) setError(QString());
        emit changed();
        QTimer::singleShot(m_timing.pageGapMs, this, [this, token, back] {
            if (token == m_token && back == m_backToken) fetchBack();
        });
    };
    for (size_t k = 0; k < batch->pages.size(); ++k) {
        const auto [from, limit] = batch->pages[k];
        m_source->page(this, m_ticker, from, limit,
            [this, batch, k, finish](const TradeSource::Page &p) {
                if (p.us != m_us) batch->error = "Binance answered from another exchange (trying again)";
                else batch->got[k] = p.trades;
                if (--batch->left == 0) finish();
            },
            [batch, finish](const QString &e) {
                batch->error = e;
                if (--batch->left == 0) finish();
            });
    }
}

// The trades between the last one in and the first live one waiting.
void TradeStream::fetchForward()
{
    if (m_forwardBusy || m_stopped) return;
    m_forwardBusy = true;
    const qint64 from = m_hiId + 1;
    const int token = m_token;
    m_source->page(this, m_ticker, from, PAGE,
        [this, token](const TradeSource::Page &p) {
            if (token != m_token) return;
            m_forwardBusy = false;
            if (p.us != m_us) {   // the other exchange's ids: not these trades
                setError("Binance answered from another exchange (trying again)");
                QTimer::singleShot(m_timing.retryMs, this, [this, token] {
                    if (token == m_token) drain();
                });
                return;
            }
            for (const TapeTrade &t : p.trades) {
                if (t.id != m_hiId + 1) continue;
                if (keep(t)) m_tape.push_back(t);
                m_hiId = t.id;
            }
            emit changed();
            drain();
        },
        [this, token](const QString &e) {
            if (token != m_token) return;
            m_forwardBusy = false;
            setError(e);
            QTimer::singleShot(m_timing.retryMs, this, [this, token] {
                if (token == m_token) drain();
            });
        });
}

// ---------------------------------------------------------------- live

void TradeStream::openLive()
{
    if (m_stopped) return;
    closeLive();
    const int token = m_liveToken;
    m_live = m_source->live(this, m_ticker, m_us,
        [this, token](const TapeTrade &t) {
            if (token != m_liveToken) return;
            m_pending.push_back(t);
            drain();
        },
        [this, token](bool opened) {
            if (token != m_liveToken) return;
            closeLive();
            if (m_stopped) return;
            if (!opened) {
                setError("Binance's trade stream isn't reachable (trying again)");
                retry(&TradeStream::openLive);
            } else {
                m_retry->disconnect();
                connect(m_retry, &QTimer::timeout, this, &TradeStream::openLive);
                m_retry->start(m_timing.reconnectMs);
            }
        });
}

void TradeStream::closeLive()
{
    ++m_liveToken;
    if (m_live) {
        m_live->deleteLater();   // maybe from inside its own callback
        m_live = nullptr;
    }
}

// Live trades go in once every trade before them is in.
void TradeStream::drain()
{
    if (m_hiId < 0 || m_stopped) return;
    bool added = false;
    while (!m_pending.empty()) {
        const TapeTrade &r = m_pending.front();
        if (r.id <= m_hiId) {
            m_pending.pop_front();
            continue;
        }
        if (r.id == m_hiId + 1) {
            if (keep(r)) m_tape.push_back(r);
            m_hiId = r.id;
            m_pending.pop_front();
            added = true;
            continue;
        }
        // A gap. Longer than the whole budget (away for hours): start over from the latest trades.
        if (size_t(r.id - m_hiId) > m_budget) {
            m_tape.clear();
            ++m_gen;
            m_pending.clear();
            m_loId = m_hiId = -1;
            m_oldestTime = NAN;
            m_backfilling = false;
            ++m_backToken;
            fetchLatest();
            emit changed();
            return;
        }
        fetchForward();
        break;
    }
    if (!added) return;
    // Live, the oldest trades go once the tape is twice the budget, down to one and a half: a pane keeps up to one and
    // a half times its budget before it drops its oldest candles (whole), so the tape always holds every trade a pane
    // has.
    if (!m_backfilling && m_tape.size() > 2 * m_budget) {
        while (m_tape.size() > m_budget + m_budget / 2) m_tape.pop_front();
        m_loId = m_tape.front().id;
        m_oldestTime = m_tape.front().t.time;
    }
    emit changed();
}

// ---------------------------------------------------------------- the streams

struct TradeStreams::State {
    std::shared_ptr<TradeSource> source;
    TradeStream::Timing timing;
    struct Entry {
        TradeStream *stream = nullptr;
        std::vector<Subscription *> subs;
    };
    QHash<QString, Entry> streams;

    // The needs of every subscription together: the earliest first bar, the largest budget, running if one runs.
    void update(const QString &key)
    {
        auto it = streams.find(key);
        if (it == streams.end()) return;
        double start = NAN;
        size_t budget = 0;
        bool active = false;
        for (const Subscription *s : it->subs) {
            if (!std::isnan(s->m_start) && !(start <= s->m_start)) start = s->m_start;
            budget = std::max(budget, s->m_budget);
            active = active || s->m_active;
        }
        it->stream->setNeeds(start, budget, active);
    }
};

TradeStreams::TradeStreams(std::shared_ptr<TradeSource> source, TradeStream::Timing timing)
    : m_state(std::make_shared<State>())
{
    m_state->source = std::move(source);
    m_state->timing = timing;
}

TradeStreams::~TradeStreams()
{
    for (auto &e : m_state->streams) delete e.stream;   // subscriptions still out see a null stream()
}

std::unique_ptr<TradeStreams::Subscription> TradeStreams::subscribe(const QString &ticker, double start, size_t budget,
                                                                    bool active)
{
    const QString key = ticker.toUpper();
    State::Entry &e = m_state->streams[key];
    if (!e.stream) e.stream = new TradeStream(m_state->source, key, m_state->timing);
    std::unique_ptr<Subscription> s(new Subscription);
    s->m_state = m_state;
    s->m_stream = e.stream;
    s->m_start = start;
    s->m_budget = budget;
    s->m_active = active;
    e.subs.push_back(s.get());
    m_state->update(key);
    return s;
}

int TradeStreams::streamCount() const
{
    return int(m_state->streams.size());
}

TradeStream *TradeStreams::find(const QString &ticker) const
{
    const auto it = m_state->streams.constFind(ticker.toUpper());
    return it == m_state->streams.constEnd() ? nullptr : it->stream;
}

TradeStreams::Subscription::~Subscription()
{
    const std::shared_ptr<State> st = m_state.lock();
    if (!st || !m_stream) return;
    const QString key = m_stream->ticker();
    auto it = st->streams.find(key);
    if (it == st->streams.end()) return;
    auto &subs = it->subs;
    subs.erase(std::remove(subs.begin(), subs.end(), this), subs.end());
    if (subs.empty()) {
        // The last one: the stream stops now and goes once whatever it is doing has returned.
        TradeStream *s = it->stream;
        st->streams.erase(it);
        s->setNeeds(s->start(), s->budget(), false);
        s->deleteLater();
    } else {
        st->update(key);
    }
}

void TradeStreams::Subscription::setNeeds(double start, size_t budget)
{
    m_start = start;
    m_budget = budget;
    if (const auto st = m_state.lock(); st && m_stream) st->update(m_stream->ticker());
}

void TradeStreams::Subscription::setActive(bool active)
{
    if (m_active == active) return;
    m_active = active;
    if (const auto st = m_state.lock(); st && m_stream) st->update(m_stream->ticker());
}

} // namespace qtm
