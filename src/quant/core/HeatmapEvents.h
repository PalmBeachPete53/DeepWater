#pragma once

// What the liquidity heatmap marks (a Deepwater addition to the Charts tab, not in Quant Terminal): liquidity pulled
// just before the price got to it, and liquidity that absorbed a lot of aggressive volume without giving way. Pure
// code: it reads the heatmap's columns (Heatmap.h) and the trades (Binance's, aggressor known), both in time order;
// the chart layer (ui/HeatmapLayer) draws what comes out.
//
// Binance's book is by price level, not by order, so both are read from quantities per level:
//
//   Pulled   a big level (≥ bigFactor × the column's median non-empty row) 1 to nearRows rows from the touch loses at
//            least dropFraction of its size in one sample, trades at it explain less than tradedShare of the loss
//            (cancelled, not filled), it doesn't come back, and the touch (best bid for a bid level, best ask for an
//            ask level) reaches its row within reachSeconds. Rows, so on BTCUSDT a level is ~2 $ of book.
//   Absorbed while the best bid stays on one tick, aggressive sells at that tick reach absorbRatio × the size resting
//            there (on average over the touch), for at least absorbMinSeconds, and the touch then leaves it upwards
//            (the level held); the same for the best ask with aggressive buys, leaving downwards. Ticks: the best
//            levels' own quantities are kept with each column, since a row mixes both sides near the price.

#include "Footprint.h"
#include "Heatmap.h"

#include <deque>
#include <vector>

namespace qtm::heatmap {

struct EventParams {
    double bigFactor = 5;
    double dropFraction = 0.7;
    double tradedShare = 0.3;
    int nearRows = 10;
    double reachSeconds = 5;
    double absorbRatio = 1;
    double absorbMinSeconds = 0.5;
};

struct LiquidityEvent {
    enum class Kind { Pulled, Absorbed };
    Kind kind = Kind::Pulled;
    int side = 0;          // 1 a bid level, −1 an ask level
    int64_t row = 0;       // storage row (pulled), or the row of the tick (absorbed)
    int64_t tick = NO_TICK;   // absorbed: the price level itself
    double t0 = 0, t1 = 0; // pulled: when it went, when the price got there; absorbed: the touch's start and end
    double qty = 0;        // pulled: the size that went; absorbed: the mean size resting at the touch
    double traded = 0;     // volume traded there meanwhile (pulled: during the drop; absorbed: hitting the level)
};

class EventDetector
{
public:
    EventDetector(int rowTicks, const EventParams &p = {}) : m_rowTicks(std::max(1, rowTicks)), m_p(p) {}
    const EventParams &params() const { return m_p; }
    void setParams(const EventParams &p) { m_p = p; }   // for what comes next

    // Trades in time order (price in ticks, the aggressor's side). A trade belongs to the interval that ends at the
    // first column at or after it.
    void addTrade(double time, int64_t tick, double qty, footprint::Side aggressor);
    // Columns in time order: the trades up to its time go in first.
    void addColumn(const ColumnMeta &m, const Code *rows, int band);

    const std::deque<LiquidityEvent> &events() const { return m_events; }
    // For the debug overlay: touches that held long enough, and the best traded / resting ratio among them.
    struct Diag {
        uint64_t heldTouches = 0;
        double bestRatio = 0;
    };
    const Diag &diag() const { return m_diag; }
    double lastColumnTime() const { return m_hasPrev ? m_prev.time : -INFINITY; }
    void dropBefore(double t);   // events ending before t go
    size_t maxEvents = 20000;    // the oldest go beyond

private:
    struct Trade {
        double time;
        int64_t tick;
        double qty;
        footprint::Side side;
    };
    struct Candidate {
        int64_t row;
        int side;
        double t0, qty, before, traded, deadline;
    };
    struct Episode {
        bool on = false;
        int64_t tick = 0;
        double t0 = 0, sumQty = 0, traded = 0;
        int n = 0;
    };
    void push(const LiquidityEvent &e);
    void episodeStep(Episode &e, int side, int64_t touch, double qty, double traded, double tStart);
    void episodeEnd(Episode &e, int side, int64_t newTouch, double t);
    double qtyAt(int64_t row) const;   // in the current column (0 outside its band)

    int m_rowTicks;
    EventParams m_p;
    std::deque<Trade> m_trades;
    std::deque<LiquidityEvent> m_events;
    Diag m_diag;
    std::vector<Candidate> m_candidates;
    Episode m_bidEp, m_askEp;
    bool m_hasPrev = false;
    ColumnMeta m_prev;
    std::vector<double> m_prevQty, m_curQty;   // decoded rows of the previous and current column
    int64_t m_curAnchor = 0;
};

} // namespace qtm::heatmap
