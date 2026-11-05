#pragma once

// The order book of the liquidity heatmap (a Deepwater addition to the Charts tab, not in Quant Terminal): the resting
// quantity at each price level, bids and asks, rebuilt from a snapshot and the incremental updates that follow it.
// Pure code, no network: the stream (server/DepthStream) parses the exchange's messages into DepthSnapshot / DepthUpdate and
// asks for a new snapshot when apply() says so.
//
// Market by price (MBP): one quantity per level, not per order. Prices are whole ticks (int64) like the footprint's.
// Updates carry the level's new absolute quantity (0 = the level is gone), and a range of update ids
// [firstId, lastId] that must follow the previous one's without a hole: Binance's diff depth stream
// (`<pair>@depth@100ms`, U..u) and its REST snapshot (`/api/v3/depth`, lastUpdateId). The resync rules are Binance's:
//   - updates that come before the snapshot is in are buffered;
//   - a snapshot older than the first buffered update leaves a hole: ask for another one;
//   - buffered updates the snapshot already contains (lastId ≤ snapshot id) are dropped, the others applied;
//   - from then on each update must start at most one id after the last one applied (firstId ≤ last + 1), and
//     updates entirely before it are stale and ignored. A hole, or a book whose best bid reaches its best ask
//     (crossed: a level was missed), means the book can't be trusted: it stops being synced until a new snapshot.

#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace qtm::depth {

struct Level {
    int64_t tick = 0;
    double qty = 0;   // 0 = remove the level
};

struct DepthUpdate {
    int64_t firstId = 0, lastId = 0;   // Binance U, u
    int64_t prevLastId = -1;           // Binance futures' pu (the previous update's u); -1 = not given (spot)
    double time = 0;                   // exchange event time, UTC seconds
    std::vector<Level> bids, asks;
};

struct DepthSnapshot {
    int64_t lastUpdateId = 0;
    double time = 0;
    std::vector<Level> bids, asks;
};

class OrderBook
{
public:
    enum class Result {
        Applied,        // in the book
        Stale,          // already in the book (before the last update applied): ignored
        Buffered,       // waiting for a snapshot
        NeedSnapshot,   // a hole in the update ids, or a snapshot too old for the buffered updates: fetch one
        Crossed,        // the best bid reached the best ask: fetch a snapshot
    };

    // Updates kept while waiting for a snapshot; beyond that the oldest go (the snapshot will be too old for them
    // anyway and another one is asked for).
    static constexpr size_t MAX_BUFFERED = 5000;

    bool synced() const { return m_synced; }
    int64_t lastUpdateId() const { return m_lastId; }
    double lastTime() const { return m_lastTime; }

    // Replaces the book. Buffered updates the snapshot doesn't have yet are applied after it (the result is that of
    // the last one: NeedSnapshot when the snapshot is older than they are, Crossed…); Applied when all is in.
    Result setSnapshot(const DepthSnapshot &s);
    Result apply(const DepthUpdate &u);
    // Forget everything (a new symbol, a closed socket): not synced, nothing buffered.
    void reset();
    // Not synced any more: updates are buffered until the next snapshot (the feed lost its socket).
    void desync();

    std::optional<int64_t> bestBid() const;
    std::optional<int64_t> bestAsk() const;
    double bidQty(int64_t tick) const;
    double askQty(int64_t tick) const;
    // Ascending ticks on both sides: bids end at the best bid, asks start at the best ask.
    const std::map<int64_t, double> &bids() const { return m_bids; }
    const std::map<int64_t, double> &asks() const { return m_asks; }
    size_t levelCount() const { return m_bids.size() + m_asks.size(); }
    size_t bufferedCount() const { return m_buffer.size(); }

    // Drops the levels more than `maxTicks` away from `centerTick`: deep levels that will be updated again when they
    // matter, so the book stays bounded however far the price travels. Returns how many went.
    size_t trim(int64_t centerTick, int64_t maxTicks);

    // Counters for the debug overlay.
    struct Stats {
        uint64_t applied = 0, stale = 0, gaps = 0, crossed = 0, snapshots = 0;
    };
    const Stats &stats() const { return m_stats; }

private:
    void applyLevels(const DepthUpdate &u);
    bool crossed() const;
    Result applySynced(const DepthUpdate &u);
    void lose();   // not synced, the buffer emptied (the next updates fill it)

    std::map<int64_t, double> m_bids, m_asks;
    std::vector<DepthUpdate> m_buffer;
    int64_t m_lastId = -1;
    double m_lastTime = 0;
    bool m_synced = false;
    uint64_t m_sinceSnapshot = 0;   // updates applied since the last snapshot
    Stats m_stats;
};

} // namespace qtm::depth
