#pragma once

// Markov-switching GARCH(1,1) in the parametrisation of Haas, Mittnik & Paolella (2004): one GARCH
// variance process per regime, all driven by the same squared innovation, so the likelihood needs no
// integration over regime paths and Hamilton's filter gives it exactly. With one regime it is a plain
// GARCH(1,1), the benchmark. Innovations are normal or Student-t (standardized to unit variance).
// Estimated by maximum likelihood over an unconstrained reparametrisation: ω = exp(·), (α, β) through
// a softmax so α, β > 0 and α + β ≤ maxPersistence in every regime, transition rows through a softmax with
// a floor on P(stay), ν = 2.05 + exp(·).
//
// Returns are in whatever unit they come in (% here); the variance series and the forecasts are in
// that unit squared.

#include "Job.h"
#include "LinAlg.h"

#include <QString>
#include <QVector>

namespace markov {

enum class Innovations { Normal, Student };

struct GarchRegime {
    double omega = 0, alpha = 0, beta = 0;
    double persistence() const { return alpha + beta; }
    // ω / (1 - α - β): the variance the regime would settle at if it lasted.
    double unconditionalVariance() const { return persistence() < 1 ? omega / (1 - persistence()) : NaN; }
};

struct MsGarchParams {
    int regimes = 0;
    double mu = 0;                     // common mean of the returns
    QVector<GarchRegime> garch;        // one per regime
    Mat transition;                    // regimes × regimes, rows sum to 1
    Innovations innovations = Innovations::Normal;
    double nu = NaN;                   // Student degrees of freedom

    bool isValid() const { return regimes > 0; }
    int parameterCount() const;
    double expectedDuration(int regime) const;
    Vec stationary() const;            // the long-run regime probabilities
    // Regimes reordered by rising unconditional variance (ω when a regime isn't stationary).
    MsGarchParams sortedByVariance() const;
};

struct MsGarchOptions {
    int regimes = 2;
    Innovations innovations = Innovations::Normal;
    int starts = 3;                    // one deterministic start, the rest perturbed
    unsigned seed = 7;
    int maxIterations = 3000;
    // Keep the regimes regimes. Without bounds the likelihood of index returns likes degenerate solutions: a
    // one-day "jump" regime (P(stay) near 0, α + β at 1) or an EWMA regime (ω = 0, α + β = 1). So: a floor on
    // every P(stay), a regime lasting at least 1 / (1 - minStay) days on average (0.8: five days); a cap on
    // α + β short of the integrated boundary; a floor on ω as a fraction of the sample variance, which keeps every
    // regime's long-run variance at least minOmegaFraction / (1 - maxPersistence) of it (0.1 with the defaults).
    double minStay = 0.8;
    double maxPersistence = 0.99;
    double minOmegaFraction = 0.001;
};

struct MsGarchFit {
    MsGarchParams params;
    double logLikelihood = NaN, aic = NaN, bic = NaN;
    int observations = 0;              // returns that are numbers
    int evaluations = 0;               // likelihood evaluations of the best start
    bool converged = false;
    bool cancelled = false;
    QString error;
    Mat predicted;                     // T × K: P(regime_t | r_1..t-1)
    Mat filtered;                      // T × K: P(regime_t | r_1..t)
    Mat regimeVariance;                // T × K: σ²_{k,t}
    Vec variance;                      // T: the variance predicted for day t, Σ_k predicted(t, k) σ²_{k,t}
    double lastEps2 = NaN;             // the last squared innovation, where the forecast starts
};

struct VolForecast {
    Vec variance;                      // one per day ahead
    Mat regimeVariance;                // horizon × K
    Mat regimeProb;                    // horizon × K
    double totalVariance = NaN;        // over the whole horizon
};

// Losses at the 95% and 99% levels, in return units, positive.
struct Risk {
    double var95 = NaN, var99 = NaN, es95 = NaN, es99 = NaN;
};

class MsGarch
{
public:
    static MsGarchFit fit(const Vec &returns, const MsGarchOptions &options, const Progress &progress = {});
    // Hamilton's filter for given parameters: the probabilities, variances and log-likelihood.
    static MsGarchFit filter(const Vec &returns, const MsGarchParams &params);
    static VolForecast forecast(const MsGarchFit &fit, int horizon);
    // One day ahead: the exact quantile of the regime mixture. Over the horizon: the summed variance as one
    // normal or Student component.
    static Risk riskOneDay(const MsGarchFit &fit, const VolForecast &forecast);
    static Risk riskHorizon(const MsGarchFit &fit, const VolForecast &forecast);

    static double aic(double logL, int parameters);
    static double bic(double logL, int parameters, int n);
};

// Loss quantile and expected shortfall of a mixture of normals (nu = NaN) or standardized Students: the
// return is mean + scale_k × Z with probability weight_k.
double mixtureVar(const Vec &weights, const Vec &means, const Vec &scales, double nu, double level);
double mixtureEs(const Vec &weights, const Vec &means, const Vec &scales, double nu, double level);
// The scale of a Student innovation with unit variance: σ √((ν - 2) / ν).
double studentScale(double sigma, double nu);

} // namespace markov
