#pragma once

// The estimations behind the Markov views, as plain functions on prices, so the views, the background
// refresher (QuantMarketState.h) and the execution view run the same code: an HMM or an MS-GARCH job
// from a spec (what the view's controls say), and the shared-state records to publish from a result.

#include "QuantMarkovViews.h"
#include "QuantMarketState.h"

#include <QJsonObject>

namespace quant {

struct HmmSpec {
    QString range = "5y";      // Yahoo range of the daily closes
    int states = 3;
    bool withVol = false;      // second variable: realized volatility
    int window = 0;            // rolling estimation on the last N days (0: all)
    bool compareK = true;      // also fit the other K for the comparison table
    QJsonObject toJson() const;
    static HmmSpec fromJson(const QJsonObject &o);
};

struct GarchSpec {
    QString range = "5y";
    int regimes = 2;
    bool student = false;
    int horizon = 10;
    bool benchmark = true;     // also fit the plain GARCH(1,1)
    QJsonObject toJson() const;
    static GarchSpec fromJson(const QJsonObject &o);
};

HmmResult runHmm(const Prices &prices, const HmmSpec &spec, const markov::Progress &progress);
GarchResult runGarch(const Prices &prices, const GarchSpec &spec, const markov::Progress &progress);

RegimeInfo regimeInfo(const QString &symbol, const HmmResult &r, const HmmSpec &spec);
RegimeInfo regimeInfo(const QString &symbol, const GarchResult &r, const GarchSpec &spec);
VolInfo volInfo(const QString &symbol, const GarchResult &r, const GarchSpec &spec);

} // namespace quant
