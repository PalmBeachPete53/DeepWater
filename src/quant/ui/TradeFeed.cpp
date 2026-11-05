// A chart pane's side of the shared Binance trades (see TradeFeed.h). A Deepwater addition.
#include "TradeFeed.h"

#include "../server/Server.h"

#include <QTimer>

#include <algorithm>
#include <atomic>

namespace qtm {

namespace {
std::atomic<quint64> g_tapeGen{0};   // generations unique across feeds, so a reader can't mistake a new feed for its old one

bool idBelow(const TapeTrade &t, qint64 id) { return t.id < id; }
bool idAbove(qint64 id, const TapeTrade &t) { return id < t.id; }
} // namespace

TradeFeed::TradeFeed(const QString &ticker, double tfSeconds, double chartStart, int budget, QObject *parent,
                     TradeStreams *streams)
    : QObject(parent), m_tf(tfSeconds), m_start(chartStart),
      m_budget(size_t(std::max(TradeStream::PAGE, budget)))
{
    m_tapeGen = ++g_tapeGen;
    if (!streams && Backend::instance()) streams = Backend::instance()->tradeStreams();
    if (!streams) return;
    m_sub = streams->subscribe(ticker, chartStart, m_budget);
    connect(m_sub->stream(), &TradeStream::changed, this, &TradeFeed::streamChanged);
    connect(m_sub->stream(), &TradeStream::stateChanged, this, &TradeFeed::streamChanged);
    // Another pane may run the stream already: its trades are taken once the caller has connected to this.
    QTimer::singleShot(0, this, &TradeFeed::streamChanged);
}

TradeFeed::~TradeFeed() = default;   // the subscription goes: the stream too, if no other pane uses it

void TradeFeed::stop()
{
    if (m_stopped) return;
    m_stopped = true;
    if (m_sub) m_sub->setActive(false);
}

void TradeFeed::resume()
{
    if (!m_stopped) return;
    m_stopped = false;
    if (m_sub) m_sub->setActive(true);
    streamChanged();   // what came in while this pane was stopped (other panes kept the stream going)
}

void TradeFeed::setChartStart(double t)
{
    if (!(t > m_start)) return;
    m_start = t;
    m_book.dropBefore(t);
    if (!std::isnan(m_loTime) && m_loTime < t) moveLoTo(t);
    if (m_sub) m_sub->setNeeds(t, m_budget);
}

TradeFeed::Tape TradeFeed::tape() const
{
    const TradeStream *s = stream();
    if (!s || m_lo < 0 || m_hi < 0 || s->generation() != m_streamGen) return {};
    const std::deque<TapeTrade> &t = s->tape();
    return Tape(std::lower_bound(t.begin(), t.end(), m_lo, idBelow), std::upper_bound(t.begin(), t.end(), m_hi, idAbove));
}

bool TradeFeed::covered() const
{
    const TradeStream *s = stream();
    return s && (s->reachedFirstTrade() || s->oldestTime() <= m_start);
}

bool TradeFeed::loadingHistory() const
{
    const TradeStream *s = stream();
    return s && s->loadingHistory() && !m_full && !covered();
}

double TradeFeed::completeFrom() const
{
    if (m_hi < 0) return NaN;
    double from = NaN;
    if (!m_full && covered()) from = m_start;
    else if (!std::isnan(m_loTime)) from = footprint::candleOpen(m_loTime, m_tf) + m_tf;
    if (!std::isnan(m_trimmedFrom)) from = std::isnan(from) ? m_trimmedFrom : std::max(from, m_trimmedFrom);
    return std::isnan(from) ? from : std::max(from, m_start);
}

QString TradeFeed::error() const
{
    const TradeStream *s = stream();
    return s ? s->error() : QString();
}

void TradeFeed::restart()
{
    m_book.clear();
    m_lo = m_hi = -1;
    m_loTime = m_trimmedFrom = NaN;
    m_full = false;
    m_tapeGen = ++g_tapeGen;
}

// The oldest trade taken moves up to the first one at `time` or after (the oldest candles dropped).
void TradeFeed::moveLoTo(double time)
{
    const TradeStream *s = stream();
    if (!s) return;
    const std::deque<TapeTrade> &t = s->tape();
    auto it = std::lower_bound(t.begin(), t.end(), time, [](const TapeTrade &x, double v) { return x.t.time < v; });
    if (it != t.end() && it->id <= m_hi) {
        if (it->id > m_lo) {
            m_lo = it->id;
            m_loTime = it->t.time;
        }
    } else {
        m_lo = -1;   // nothing left
        m_loTime = NaN;
    }
}

bool TradeFeed::take()
{
    TradeStream *s = stream();
    if (!s || m_stopped) return false;
    bool moved = false;
    if (s->tickSize() > 0 && m_book.grid().tickSize != s->tickSize()) {
        m_book = footprint::Book(footprint::TickGrid{s->tickSize(), m_book.grid().rowTicks});
        restart();
        moved = true;
    }
    if (!s->ready() || !(m_book.grid().tickSize > 0)) return moved;
    // The stream started over, or dropped trades this pane hadn't taken (it was stopped a long while): so does the pane.
    if (s->generation() != m_streamGen || (m_hi >= 0 && s->firstId() > m_hi + 1)) {
        if (m_hi >= 0) {
            restart();
            moved = true;
        }
        m_streamGen = s->generation();
    }
    const std::deque<TapeTrade> &tape = s->tape();
    auto add = [this](const TapeTrade &t) { m_book.add(footprint::candleOpen(t.t.time, m_tf), t.t, {t.t.side, false}); };
    const bool first = m_hi < 0;
    // Live: what came in after the newest one taken.
    if (!first) {
        for (auto it = std::upper_bound(tape.begin(), tape.end(), m_hi, idAbove); it != tape.end(); ++it) {
            if (it->t.time < m_start) continue;
            add(*it);
            if (m_lo < 0) {
                m_lo = it->id;
                m_loTime = it->t.time;
            }
            moved = true;
        }
    }
    m_hi = s->lastId();
    // History: older than the oldest one taken, newest first, back to the pane's first bar or its budget.
    if (!m_full) {
        auto it = std::lower_bound(tape.begin(), tape.end(), m_lo >= 0 ? m_lo : m_hi + 1, idBelow);
        while (it != tape.begin()) {
            --it;
            if (it->t.time < m_start) break;
            if (m_book.tradeCount() >= m_budget) break;
            add(*it);
            m_lo = it->id;
            m_loTime = it->t.time;
            moved = true;
        }
        if (m_book.tradeCount() >= m_budget) m_full = true;
    }
    // Live, the footprints keep about the budget: the oldest candles go, whole, once it's half as much again.
    if (!loadingHistory() && m_book.tradeCount() > m_budget + m_budget / 2) {
        const double kept = m_book.trimToTrades(m_budget);
        m_full = true;
        if (!std::isnan(kept)) {
            m_trimmedFrom = std::isnan(m_trimmedFrom) ? kept : std::max(m_trimmedFrom, kept);
            moveLoTo(kept);
        }
        moved = true;
    }
    return moved;
}

void TradeFeed::streamChanged()
{
    if (m_stopped) return;
    if (take()) emit changed();
    const bool ready = this->ready(), loading = loadingHistory();
    const QString error = this->error();
    if (ready != m_shownReady || loading != m_shownLoading || error != m_shownError) {
        m_shownReady = ready;
        m_shownLoading = loading;
        m_shownError = error;
        emit stateChanged();
    }
}

} // namespace qtm
