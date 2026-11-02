#include "Hmm.h"

#include <algorithm>
#include <numeric>
#include <random>

namespace markov {

namespace {

bool missing(const Mat &obs, int t)
{
    for (int j = 0; j < obs.cols; ++j)
        if (!ok(obs(t, j)))
            return true;
    return false;
}

// Cholesky factors of every state's covariance; false when one isn't positive definite.
bool factors(const HmmParams &p, QVector<Mat> *L)
{
    L->resize(p.states);
    for (int k = 0; k < p.states; ++k)
        if (!cholesky(p.covariances[k], &(*L)[k]))
            return false;
    return true;
}

Mat logMatrix(const Mat &m)
{
    Mat out(m.rows, m.cols);
    for (int i = 0; i < m.a.size(); ++i)
        out.a[i] = m.a[i] > 0 ? std::log(m.a[i]) : -INFINITY;
    return out;
}

// Forward pass in log space: logAlpha(t, k) = log P(y_1..t, s_t = k).
Mat forward(const Mat &logB, const HmmParams &p, const Mat &logA, double *logL)
{
    const int T = logB.rows, K = p.states;
    Mat la(T, K);
    for (int k = 0; k < K; ++k)
        la(0, k) = (p.initial[k] > 0 ? std::log(p.initial[k]) : -INFINITY) + logB(0, k);
    Vec tmp(K);
    for (int t = 1; t < T; ++t)
        for (int k = 0; k < K; ++k) {
            for (int j = 0; j < K; ++j)
                tmp[j] = la(t - 1, j) + logA(j, k);
            la(t, k) = logSumExp(tmp) + logB(t, k);
        }
    if (logL)
        *logL = logSumExp(la.row(T - 1));
    return la;
}

Mat backward(const Mat &logB, const HmmParams &p, const Mat &logA)
{
    const int T = logB.rows, K = p.states;
    Mat lb(T, K);
    Vec tmp(K);
    for (int t = T - 2; t >= 0; --t)
        for (int k = 0; k < K; ++k) {
            for (int j = 0; j < K; ++j)
                tmp[j] = logA(k, j) + logB(t + 1, j) + lb(t + 1, j);
            lb(t, k) = logSumExp(tmp);
        }
    return lb;
}

// Sample covariance of the rows of obs (missing rows skipped), floored on its diagonal.
Mat sampleCovariance(const Mat &obs, const Vec &mean, double floor)
{
    const int D = obs.cols;
    Mat cov(D, D);
    int n = 0;
    for (int t = 0; t < obs.rows; ++t) {
        if (missing(obs, t))
            continue;
        ++n;
        for (int i = 0; i < D; ++i)
            for (int j = 0; j < D; ++j)
                cov(i, j) += (obs(t, i) - mean[i]) * (obs(t, j) - mean[j]);
    }
    for (double &v : cov.a)
        v /= std::max(1, n - 1);
    for (int i = 0; i < D; ++i)
        cov(i, i) = std::max(cov(i, i), floor);
    return cov;
}

Vec sampleMean(const Mat &obs)
{
    Vec m(obs.cols, 0.0);
    int n = 0;
    for (int t = 0; t < obs.rows; ++t) {
        if (missing(obs, t))
            continue;
        ++n;
        for (int j = 0; j < obs.cols; ++j)
            m[j] += obs(t, j);
    }
    for (double &v : m)
        v /= std::max(1, n);
    return m;
}

// Starting parameters from k-means (k-means++ seeding, Lloyd iterations), or from random rows of the
// sample around a sticky transition matrix.
HmmParams initialParams(const Mat &obs, int K, bool kmeans, std::mt19937 &rng, double floor)
{
    const int D = obs.cols;
    QVector<int> rows;
    for (int t = 0; t < obs.rows; ++t)
        if (!missing(obs, t))
            rows.push_back(t);
    HmmParams p;
    p.states = K;
    p.dims = D;
    p.initial = Vec(K, 1.0 / K);
    p.transition = Mat(K, K, 0.1 / std::max(1, K - 1));
    for (int k = 0; k < K; ++k)
        p.transition(k, k) = K == 1 ? 1.0 : 0.9;
    const Vec globalMean = sampleMean(obs);
    const Mat globalCov = sampleCovariance(obs, globalMean, floor);
    p.means.resize(K);
    p.covariances.resize(K);
    std::uniform_int_distribution<int> pick(0, std::max(0, int(rows.size()) - 1));
    // k-means++: each next centre drawn with probability proportional to its squared distance.
    QVector<Vec> centres;
    centres.push_back(obs.row(rows[pick(rng)]));
    auto dist2 = [&](int t) {
        double best = INFINITY;
        for (const Vec &c : centres) {
            double d = 0;
            for (int j = 0; j < D; ++j)
                d += (obs(t, j) - c[j]) * (obs(t, j) - c[j]);
            best = std::min(best, d);
        }
        return best;
    };
    while (centres.size() < K) {
        Vec w;
        for (int t : rows)
            w.push_back(dist2(t));
        std::discrete_distribution<int> draw(w.begin(), w.end());
        centres.push_back(obs.row(rows[draw(rng)]));
    }
    if (kmeans) {
        QVector<int> label(obs.rows, -1);
        for (int it = 0; it < 30; ++it) {
            bool changed = false;
            for (int t : rows) {
                int best = 0;
                double bd = INFINITY;
                for (int k = 0; k < K; ++k) {
                    double d = 0;
                    for (int j = 0; j < D; ++j)
                        d += (obs(t, j) - centres[k][j]) * (obs(t, j) - centres[k][j]);
                    if (d < bd) {
                        bd = d;
                        best = k;
                    }
                }
                if (label[t] != best) {
                    label[t] = best;
                    changed = true;
                }
            }
            for (int k = 0; k < K; ++k) {
                Vec m(D, 0.0);
                int n = 0;
                for (int t : rows)
                    if (label[t] == k) {
                        ++n;
                        for (int j = 0; j < D; ++j)
                            m[j] += obs(t, j);
                    }
                if (n > 0)
                    for (int j = 0; j < D; ++j)
                        centres[k][j] = m[j] / n;
            }
            if (!changed)
                break;
        }
        for (int k = 0; k < K; ++k) {
            p.means[k] = centres[k];
            Mat cov(D, D);
            int n = 0;
            for (int t : rows)
                if (label[t] == k) {
                    ++n;
                    for (int i = 0; i < D; ++i)
                        for (int j = 0; j < D; ++j)
                            cov(i, j) += (obs(t, i) - centres[k][i]) * (obs(t, j) - centres[k][j]);
                }
            if (n < D + 2) {
                cov = globalCov;
            } else {
                for (double &v : cov.a)
                    v /= n - 1;
                for (int i = 0; i < D; ++i)
                    cov(i, i) = std::max(cov(i, i), floor);
            }
            p.covariances[k] = cov;
        }
    } else {
        // Random: the k-means++ draws as means, the global covariance scaled around each.
        std::uniform_real_distribution<double> scale(0.5, 1.5);
        for (int k = 0; k < K; ++k) {
            p.means[k] = centres[k];
            Mat cov = globalCov;
            const double s = scale(rng);
            for (double &v : cov.a)
                v *= s;
            p.covariances[k] = cov;
        }
    }
    return p;
}

// One run of EM from `start`.
// Variance floors per variable: the absolute one, or a fraction of the sample variance.
Vec floors(const Mat &obs, const HmmOptions &o)
{
    const Vec mean = sampleMean(obs);
    const Mat cov = sampleCovariance(obs, mean, 0);
    Vec f(obs.cols);
    for (int j = 0; j < obs.cols; ++j)
        f[j] = std::max(o.minVariance, o.minVarianceFraction * cov(j, j));
    return f;
}

HmmFit em(const Mat &obs, HmmParams p, const HmmOptions &o, const Progress &progress, int runIndex, int runs)
{
    const Vec floor = floors(obs, o);
    HmmFit fit;
    const int T = obs.rows, K = p.states, D = p.dims;
    double prev = -INFINITY;
    QVector<Mat> L;
    for (int it = 0; it < o.maxIterations; ++it) {
        if (progress.cancelled()) {
            fit.cancelled = true;
            break;
        }
        if (!factors(p, &L)) {
            fit.error = "A state's covariance is not positive definite";
            break;
        }
        const Mat logB = GaussianHmm::logDensities(obs, p);
        const Mat logA = logMatrix(p.transition);
        double logL;
        const Mat la = forward(logB, p, logA, &logL);
        if (!ok(logL)) {
            fit.error = "The likelihood is not finite";
            break;
        }
        const Mat lb = backward(logB, p, logA);
        fit.history.push_back(logL);
        fit.iterations = it + 1;
        // E step: state posteriors and expected transitions.
        Mat gamma(T, K);
        for (int t = 0; t < T; ++t)
            for (int k = 0; k < K; ++k)
                gamma(t, k) = std::exp(la(t, k) + lb(t, k) - logL);
        Mat xi(K, K);
        for (int t = 0; t + 1 < T; ++t)
            for (int i = 0; i < K; ++i)
                for (int j = 0; j < K; ++j)
                    xi(i, j) += std::exp(la(t, i) + logA(i, j) + logB(t + 1, j) + lb(t + 1, j) - logL);
        // M step.
        HmmParams n = p;
        for (int k = 0; k < K; ++k)
            n.initial[k] = std::max(1e-12, gamma(0, k));
        {
            double s = std::accumulate(n.initial.begin(), n.initial.end(), 0.0);
            for (double &v : n.initial)
                v /= s;
        }
        for (int i = 0; i < K; ++i) {
            double s = 0;
            for (int j = 0; j < K; ++j)
                s += xi(i, j);
            for (int j = 0; j < K; ++j)
                n.transition(i, j) = s > 0 ? std::max(1e-12, xi(i, j) / s) : p.transition(i, j);
            double r = 0;
            for (int j = 0; j < K; ++j)
                r += n.transition(i, j);
            for (int j = 0; j < K; ++j)
                n.transition(i, j) /= r;
        }
        for (int k = 0; k < K; ++k) {
            double w = 0;
            Vec m(D, 0.0);
            for (int t = 0; t < T; ++t) {
                if (missing(obs, t))
                    continue;
                w += gamma(t, k);
                for (int j = 0; j < D; ++j)
                    m[j] += gamma(t, k) * obs(t, j);
            }
            if (w < 1e-8)   // a state nothing belongs to any more: keep its parameters
                continue;
            for (double &v : m)
                v /= w;
            Mat cov(D, D);
            for (int t = 0; t < T; ++t) {
                if (missing(obs, t))
                    continue;
                for (int i = 0; i < D; ++i)
                    for (int j = 0; j < D; ++j)
                        cov(i, j) += gamma(t, k) * (obs(t, i) - m[i]) * (obs(t, j) - m[j]);
            }
            for (double &v : cov.a)
                v /= w;
            for (int i = 0; i < D; ++i)
                cov(i, i) = std::max(cov(i, i), floor[i]);
            n.means[k] = m;
            n.covariances[k] = cov;
        }
        p = n;
        fit.logLikelihood = logL;
        if (it > 0 && std::fabs(logL - prev) <= o.tolerance * std::max(1.0, std::fabs(logL))) {
            fit.converged = true;
            break;
        }
        prev = logL;
        if (it % 10 == 0)
            progress(int(100.0 * (runIndex + double(it) / o.maxIterations) / runs),
                     QString("Start %1/%2, iteration %3").arg(runIndex + 1).arg(runs).arg(it + 1));
    }
    fit.params = p;
    return fit;
}

void finish(HmmFit *fit, int T)
{
    if (!ok(fit->logLikelihood))
        return;
    fit->observations = T;
    fit->params = fit->params.sortedByVolatility();
    const int k = fit->params.parameterCount();
    fit->aic = GaussianHmm::aic(fit->logLikelihood, k);
    fit->bic = GaussianHmm::bic(fit->logLikelihood, k, T);
}

} // namespace

double HmmParams::expectedDuration(int state) const
{
    const double p = transition(state, state);
    return p < 1 ? 1.0 / (1.0 - p) : INFINITY;
}

HmmParams HmmParams::sortedByVolatility() const
{
    QVector<int> order(states);
    std::iota(order.begin(), order.end(), 0);
    std::stable_sort(order.begin(), order.end(), [this](int a, int b) { return covariances[a](0, 0) < covariances[b](0, 0); });
    HmmParams out = *this;
    for (int k = 0; k < states; ++k) {
        out.initial[k] = initial[order[k]];
        out.means[k] = means[order[k]];
        out.covariances[k] = covariances[order[k]];
        for (int j = 0; j < states; ++j)
            out.transition(k, j) = transition(order[k], order[j]);
    }
    return out;
}

int HmmParams::parameterCount() const
{
    return (states - 1) + states * (states - 1) + states * dims + states * dims * (dims + 1) / 2;
}

double GaussianHmm::aic(double logL, int parameters)
{
    return 2 * parameters - 2 * logL;
}

double GaussianHmm::bic(double logL, int parameters, int n)
{
    return parameters * std::log(double(n)) - 2 * logL;
}

Mat GaussianHmm::logDensities(const Mat &obs, const HmmParams &p)
{
    const int T = obs.rows, K = p.states;
    Mat logB(T, K);
    QVector<Mat> L;
    if (!factors(p, &L)) {
        for (double &v : logB.a)
            v = NaN;
        return logB;
    }
    for (int t = 0; t < T; ++t) {
        if (missing(obs, t))
            continue;   // 0: the step tells nothing about the state
        const Vec y = obs.row(t);
        for (int k = 0; k < K; ++k)
            logB(t, k) = logNormal(y, p.means[k], L[k]);
    }
    return logB;
}

HmmFit GaussianHmm::fit(const Mat &obs, const HmmOptions &o, const Progress &progress)
{
    HmmFit best;
    const int T = obs.rows;
    int valid = 0;
    for (int t = 0; t < T; ++t)
        valid += missing(obs, t) ? 0 : 1;
    if (o.states < 1 || obs.cols < 1 || valid < 5 * o.states * obs.cols) {
        best.error = "Not enough observations for the number of states";
        return best;
    }
    std::mt19937 rng(o.seed);
    const int runs = std::max(1, o.initializations);
    // The initializations are drawn in order (the same as one after the other), then run side by side.
    QVector<HmmParams> starts;
    for (int r = 0; r < runs; ++r)
        starts.push_back(initialParams(obs, o.states, r == 0, rng, floors(obs, o)[0]));
    QVector<HmmFit> fits(runs);
    parallelFor(runs, [&](int r) {
        if (progress.cancelled()) {
            fits[r].cancelled = true;
            return;
        }
        fits[r] = em(obs, starts[r], o, progress, r, runs);
    });
    for (int r = 0; r < runs; ++r) {
        const HmmFit &fit = fits[r];
        if (fit.cancelled) {
            best.cancelled = true;
            if (!ok(best.logLikelihood))
                best = fit;
            break;
        }
        if (ok(fit.logLikelihood) && (!ok(best.logLikelihood) || fit.logLikelihood > best.logLikelihood))
            best = fit;
        else if (!ok(best.logLikelihood))
            best.error = fit.error;
    }
    finish(&best, T);
    progress(100, "Done");
    return best;
}

HmmFit GaussianHmm::refit(const Mat &obs, const HmmParams &start, const HmmOptions &o, const Progress &progress)
{
    HmmFit fit;
    if (!start.isValid() || start.dims != obs.cols) {
        fit.error = "The starting parameters don't match the observations";
        return fit;
    }
    fit = em(obs, start, o, progress, 0, 1);
    finish(&fit, obs.rows);
    progress(100, "Done");
    return fit;
}

Mat GaussianHmm::filter(const Mat &obs, const HmmParams &p, double *logLikelihood)
{
    const Mat logB = logDensities(obs, p);
    const Mat la = forward(logB, p, logMatrix(p.transition), logLikelihood);
    Mat out(la.rows, la.cols);
    for (int t = 0; t < la.rows; ++t) {
        const double z = logSumExp(la.row(t));
        for (int k = 0; k < la.cols; ++k)
            out(t, k) = std::exp(la(t, k) - z);
    }
    return out;
}

Mat GaussianHmm::smooth(const Mat &obs, const HmmParams &p)
{
    const Mat logB = logDensities(obs, p), logA = logMatrix(p.transition);
    double logL;
    const Mat la = forward(logB, p, logA, &logL), lb = backward(logB, p, logA);
    Mat out(la.rows, la.cols);
    for (int t = 0; t < la.rows; ++t)
        for (int k = 0; k < la.cols; ++k)
            out(t, k) = std::exp(la(t, k) + lb(t, k) - logL);
    return out;
}

QVector<int> GaussianHmm::viterbi(const Mat &obs, const HmmParams &p)
{
    const int T = obs.rows, K = p.states;
    if (T == 0)
        return {};
    const Mat logB = logDensities(obs, p), logA = logMatrix(p.transition);
    Mat delta(T, K);
    QVector<QVector<int>> back(T, QVector<int>(K, 0));
    for (int k = 0; k < K; ++k)
        delta(0, k) = (p.initial[k] > 0 ? std::log(p.initial[k]) : -INFINITY) + logB(0, k);
    for (int t = 1; t < T; ++t)
        for (int k = 0; k < K; ++k) {
            double best = -INFINITY;
            int arg = 0;
            for (int j = 0; j < K; ++j) {
                const double v = delta(t - 1, j) + logA(j, k);
                if (v > best) {
                    best = v;
                    arg = j;
                }
            }
            delta(t, k) = best + logB(t, k);
            back[t][k] = arg;
        }
    QVector<int> path(T);
    int last = 0;
    for (int k = 1; k < K; ++k)
        if (delta(T - 1, k) > delta(T - 1, last))
            last = k;
    path[T - 1] = last;
    for (int t = T - 1; t > 0; --t)
        path[t - 1] = back[t][path[t]];
    return path;
}

} // namespace markov
