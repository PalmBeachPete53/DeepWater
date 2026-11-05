// A chart pane's side of the shared order book depth (see DepthFeed.h). A Deepwater addition.
#include "DepthFeed.h"

#include "../server/Server.h"

#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QTimer>
#include <QtConcurrent>

#include <algorithm>
#include <atomic>

namespace qtm {

DepthFeed::DepthFeed(const QString &ticker, const Params &p, QObject *parent, DepthStreams *streams)
    : QObject(parent), m_ticker(ticker.toUpper()), m_p(p)
{
    if (!streams && Backend::instance()) streams = Backend::instance()->depthStreams();
    if (!streams) return;
    m_sub = streams->subscribe(m_ticker, {m_p.intervalMs, m_p.historyMinutes, m_p.record, m_p.keepDays, m_active});
    DepthStream *s = m_sub->stream();
    connect(s, &DepthStream::columns, this, &DepthFeed::onColumns);
    connect(s, &DepthStream::configured, this, [this] {
        checkStore();
        emit stateChanged();
    });
    connect(s, &DepthStream::stateChanged, this, &DepthFeed::stateChanged);
    checkStore();   // the pair's stream may run already (another pane): its samples show at once
    // Whoever made this connects to it next: tell them then.
    QTimer::singleShot(0, this, [this] {
        emit stateChanged();
        emit changed();
    });
}

DepthFeed::~DepthFeed() = default;   // the subscription goes: the stream too, if no other pane uses it

void DepthFeed::subscribeNeeds()
{
    if (m_sub) m_sub->setNeeds({m_p.intervalMs, m_p.historyMinutes, m_p.record, m_p.keepDays, m_active});
}

void DepthFeed::stop()
{
    if (!m_active) return;
    m_active = false;
    subscribeNeeds();
}

void DepthFeed::resume()
{
    if (m_active) return;
    m_active = true;
    subscribeNeeds();
}

void DepthFeed::setRecording(bool on, int keepDays)
{
    if (on == m_p.record && keepDays == m_p.keepDays) return;
    m_p.record = on;
    m_p.keepDays = keepDays;
    subscribeNeeds();
}

const heatmap::ColumnStore *DepthFeed::rawStore() const
{
    const DepthStream *s = stream();
    return s ? s->store(m_p.intervalMs) : nullptr;
}

quint64 DepthFeed::rawId() const
{
    const DepthStream *s = stream();
    return s ? s->rawId(m_p.intervalMs) : 0;
}

double DepthFeed::tickSize() const
{
    const DepthStream *s = stream();
    return s ? s->tickSize() : 0;
}

int DepthFeed::rowTicks() const
{
    const DepthStream *s = stream();
    return s ? s->rowTicks() : 0;
}

const DepthStats &DepthFeed::stats() const
{
    static const DepthStats none;
    const DepthStream *s = stream();
    return s ? s->stats() : none;
}

double DepthFeed::newestMadeAtMs() const
{
    const DepthStream *s = stream();
    return s ? s->newestMadeAt(m_p.intervalMs) : NAN;
}

size_t DepthFeed::capacity() const
{
    return size_t(std::max(1.0, double(m_p.historyMinutes) * 60000.0 / m_p.intervalMs));
}

void DepthFeed::setHistoryMinutes(int minutes)
{
    m_p.historyMinutes = std::max(1, minutes);
    subscribeNeeds();   // the samples keep the longest history among the panes
    if (m_stable) m_stable->setCapacity(capacity());
    if (m_pyramid) m_pyramid->setBaseCapacity(capacity());
}

void DepthFeed::checkStore()
{
    const quint64 id = rawId();
    if (!rawStore() || id == m_seenRawId) return;
    m_seenRawId = id;
    m_displayEnd = 0;
    rebuildPyramid();
}

void DepthFeed::newStoreId()
{
    static std::atomic<quint64> ids{0};
    m_storeId = ++ids;
}

int DepthFeed::stableSamples() const
{
    return m_p.stableMs > 0 ? std::max(2, int(std::lround(double(m_p.stableMs) / m_p.intervalMs)) + 1) : 0;
}

void DepthFeed::addToDisplay(Display &d, const heatmap::ColumnStore &raw, uint64_t index, int intervalMs,
                             std::vector<heatmap::Code> &scratch)
{
    if (d.stable) {
        heatmap::stableColumn(raw, index, d.k, intervalMs, scratch);
        d.stable->append(raw.meta(index), scratch.data());
        d.pyramid->add(raw.meta(index), scratch.data());
    } else {
        d.pyramid->add(raw.meta(index), raw.rows(index));
    }
    d.end = index + 1;
}

std::shared_ptr<DepthFeed::Display> DepthFeed::buildDisplay(const heatmap::ColumnStore &raw, int intervalMs, size_t capacity,
                                                            heatmap::Agg agg, int k)
{
    QElapsedTimer t;
    t.start();
    auto d = std::make_shared<Display>();
    d->k = k;
    d->pyramid = std::make_unique<heatmap::ColumnPyramid>(raw.bandRows(), intervalMs, capacity, agg);
    if (k) d->stable = std::make_unique<heatmap::ColumnStore>(raw.bandRows(), capacity);
    std::vector<heatmap::Code> scratch;
    d->end = raw.firstIndex();
    for (uint64_t i = raw.firstIndex(); i < raw.endIndex(); ++i) addToDisplay(*d, raw, i, intervalMs, scratch);
    d->end = raw.endIndex();
    d->ms = double(t.nsecsElapsed()) / 1e6;
    return d;
}

void DepthFeed::install(Display &&d)
{
    m_stable = std::move(d.stable);
    m_pyramid = std::move(d.pyramid);
    m_displayK = d.k;
    m_displayEnd = d.end;
    m_displayRebuildMs = d.ms;
    if (m_stable) m_stable->setCapacity(capacity());   // the history may have changed meanwhile
    m_pyramid->setBaseCapacity(capacity());
    newStoreId();
}

void DepthFeed::rebuildPyramid()
{
    const heatmap::ColumnStore *raw = rawStore();
    if (!raw) return;
    const quint64 gen = ++m_rebuildGen;
    const int k = stableSamples(), iv = m_p.intervalMs;
    const size_t cap = capacity();
    const heatmap::Agg agg = m_p.agg;
    if (raw->size() < 2000) {   // ~10 ms at most: at once
        m_rebuilding = false;
        install(std::move(*buildDisplay(*raw, iv, cap, agg, k)));
        return;
    }
    // A copy of the samples (~10 ms for 2 hours) for the worker; the samples keep coming in meanwhile.
    auto copy = std::make_shared<const heatmap::ColumnStore>(*raw);
    const quint64 from = rawId();
    m_rebuilding = true;
    auto *w = new QFutureWatcher<std::shared_ptr<Display>>(this);
    connect(w, &QFutureWatcherBase::finished, this, [this, w, gen, iv, from] {
        std::shared_ptr<Display> d = w->result();
        w->deleteLater();
        const heatmap::ColumnStore *raw = rawStore();
        // Another setting came meanwhile (its rebuild wins), or the samples were made again.
        if (gen != m_rebuildGen || !d || !raw || rawId() != from) return;
        // The samples that came during the rebuild (the oldest may have gone from the ring meanwhile).
        for (uint64_t i = std::max(d->end, raw->firstIndex()); i < raw->endIndex(); ++i)
            addToDisplay(*d, *raw, i, iv, m_stableScratch);
        install(std::move(*d));
        m_rebuilding = false;
        emit changed();
    });
    w->setFuture(QtConcurrent::run([copy, iv, cap, agg, k] { return buildDisplay(*copy, iv, cap, agg, k); }));
}

void DepthFeed::setStableMs(int ms)
{
    ms = std::max(0, ms);
    if (ms == m_p.stableMs) return;
    m_p.stableMs = ms;
    rebuildPyramid();
    emit changed();
}

void DepthFeed::setAgg(heatmap::Agg agg)
{
    if (agg == m_p.agg) return;
    m_p.agg = agg;
    rebuildPyramid();
    emit changed();
}

// New samples at some interval: this pane's display takes those at its own.
void DepthFeed::onColumns(int intervalMs)
{
    if (intervalMs != m_p.intervalMs) return;
    const heatmap::ColumnStore *raw = rawStore();
    if (!raw) return;
    if (rawId() != m_seenRawId) {
        checkStore();
        emit changed();
        return;
    }
    if (!m_pyramid) return;
    const int k = m_displayK;   // the shown store's (a rebuild for another setting may be under way)
    for (uint64_t i = std::max(m_displayEnd, raw->firstIndex()); i < raw->endIndex(); ++i) {
        if (m_stable) {
            heatmap::stableColumn(*raw, i, k, m_p.intervalMs, m_stableScratch);
            m_stable->append(raw->meta(i), m_stableScratch.data());
            m_pyramid->add(raw->meta(i), m_stableScratch.data());
        } else {
            m_pyramid->add(raw->meta(i), raw->rows(i));
        }
    }
    m_displayEnd = raw->endIndex();
    emit changed();
}

} // namespace qtm
