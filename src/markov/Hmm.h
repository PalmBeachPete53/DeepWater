#pragma once

// A hidden Markov model with Gaussian observations (one or several variables per step), for market
// regimes: estimated by Baum-Welch (EM) in log space, started from k-means and random draws, with
// its states sorted by the volatility of the first variable so "state 0" stays the calm one from one
// estimation to the next.
//
// Inference: filter() gives P(state | observations up to now), the only output to use for signals
// and backtests. smooth() and viterbi() use the whole sample and are for looking back only.
//
// A row of NaN in the observations is a missing step: it carries no information (the transition
// alone moves the state) and is left out of the parameter updates.

#include "Job.h"
#include "LinAlg.h"

#include <QVector>

namespace markov {

struct HmmParams {
    int states = 0, dims = 0;
    Vec initial;                   // P(s_0 = k)
    Mat transition;                // states × states, rows sum to 1
    QVector<Vec> means;            // per state
    QVector<Mat> covariances;      // per state, dims × dims

    bool isValid() const { return states > 0 && dims > 0; }
    double volatility(int state) const { return std::sqrt(covariances[state](0, 0)); }
    // Expected length of a visit to the state, 1 / (1 - p_kk).
    double expectedDuration(int state) const;
    // The same model with the states reordered by rising volatility of the first variable.
    HmmParams sortedByVolatility() const;
    int parameterCount() const;
};

struct HmmOptions {
    int states = 2;
    int maxIterations = 300;
    double tolerance = 1e-6;       // relative change of the log-likelihood that ends EM
    int initializations = 6;       // k-means first, then random starts
    unsigned seed = 7;
    double minVariance = 1e-12;    // absolute floor on the diagonal of each covariance
    double minVarianceFraction = 0.01;   // and relative to each variable's sample variance: no state narrower than that
};

struct HmmFit {
    HmmParams params;
    double logLikelihood = NaN;
    double aic = NaN, bic = NaN;
    int iterations = 0;
    bool converged = false;
    bool cancelled = false;
    QString error;                 // why there is no model
    Vec history;                   // log-likelihood after each EM iteration of the best start
    int observations = 0;
};

class GaussianHmm
{
public:
    // obs: one row per step, one column per variable.
    static HmmFit fit(const Mat &obs, const HmmOptions &options, const Progress &progress = {});
    // EM from given parameters (a rolling re-estimation starts from the last ones).
    static HmmFit refit(const Mat &obs, const HmmParams &start, const HmmOptions &options, const Progress &progress = {});

    // Filtered probabilities P(s_t | y_1..t), one row per step; the log-likelihood of the sample if asked.
    static Mat filter(const Mat &obs, const HmmParams &params, double *logLikelihood = nullptr);
    // Smoothed probabilities P(s_t | y_1..T): a posteriori.
    static Mat smooth(const Mat &obs, const HmmParams &params);
    // The most likely path of states: a posteriori.
    static QVector<int> viterbi(const Mat &obs, const HmmParams &params);
    // log N(y_t; μ_k, Σ_k) per step and state; 0 on missing steps.
    static Mat logDensities(const Mat &obs, const HmmParams &params);

    // Criteria for a fit of n observations.
    static double aic(double logL, int parameters);
    static double bic(double logL, int parameters, int n);
};

} // namespace markov
