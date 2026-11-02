#pragma once

// What data.getCandles() returns: { symbol, provider, notes, candles }.

#include "Bar.h"

#include <QStringList>

namespace qtm {

struct CandleSet {
    QString symbol;        // resolved id, e.g. "BINANCE:BTCUSDT"
    QString provider;      // "Binance", "Nasdaq", ... (r.provider)
    QStringList notes;     // r.notes, shown under the chart legend
    Bars candles;
};

} // namespace qtm
