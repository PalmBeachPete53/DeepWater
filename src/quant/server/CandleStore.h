#pragma once

// The last candles of each symbol and timeframe, on disk: a chart shows them at once and asks the network only for what
// came since (ChartPane::load). A full download still happens when they are too few, too old (a stock's history gets
// adjusted after a split) or missing.

#include "../core/CandleSet.h"

#include <QString>

#include <optional>

namespace qtm {

struct StoredCandles {
    CandleSet set;
    int limit = 0;        // the bars asked for by the download they come from
    qint64 fullAt = 0;    // when that was, seconds since the epoch
};

// What a chart has to fetch after showing stored candles.
struct CandleRefresh {
    bool full = true;     // everything again
    int recent = 0;       // or this many last bars
};
CandleRefresh planCandleRefresh(const StoredCandles &stored, const QString &tf, int limit, qint64 now);

class CandleStore
{
public:
    static constexpr int MaxFiles = 300;   // about 150 KB each, the oldest go first
    static constexpr qint64 FullAfter = 24 * 3600;

    explicit CandleStore(const QString &dir);
    std::optional<StoredCandles> read(const QString &symbol, const QString &tf) const;
    bool write(const QString &symbol, const QString &tf, const StoredCandles &stored) const;

private:
    QString fileOf(const QString &symbol, const QString &tf) const;
    void trim() const;
    QString m_dir;
};

} // namespace qtm
