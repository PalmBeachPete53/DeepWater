#include "MsGarch.h"

#include "Dist.h"
#include "Optim.h"

#include <algorithm>
#include <random>

namespace markov {

namespace {

constexpr double NuFloor = 2.05;

// --- Reparametrisation: a vector of reals <-> the model.

int thetaSize(int K, Innovations inn)
{
    return 1 + 3 * K + K * (K - 1) + (inn == Innovations::Student ? 1 : 0);
}

// The bounds of the reparametrisation.
struct Bounds {
    double maxPersistence = 0.99;
    double minStay = 0.8;
    double minOmega = 0;
};

MsGarchParams unpack(const Vec &x, int K, Innovations inn, const Bounds &bounds)
{
    MsGarchParams p;
    p.regimes = K;
    p.innovations = inn;
    int i = 0;
    p.mu = x[i++];
    p.garch.resize(K);
    for (int k = 0; k < K; ++k) {
        const double lw = x[i++], a = x[i++], b = x[i++];
        const double ea = std::exp(a), eb = std::exp(b), z = 1 + ea + eb;
        p.garch[k].omega = bounds.minOmega + std::exp(lw);
        p.garch[k].alpha = bounds.maxPersistence * ea / z;
        p.garch[k].beta = bounds.maxPersistence * eb / z;
    }
    p.transition = Mat(K, K);
    const double minStay = K == 1 ? 1.0 : bounds.minStay;
    for (int k = 0; k < K; ++k) {
        // Row k: P(stay) = minStay + (1 - minStay) softmax_k, the others (1 - minStay) softmax_j.
        Vec logits(K, 0.0);
        for (int j = 0; j + 1 < K; ++j)
            logits[j] = x[i++];
        const double z = logSumExp(logits);
        for (int j = 0; j < K; ++j)
            p.transition(k, j) = (1 - minStay) * std::exp(logits[j] - z) + (j == k ? minStay : 0.0);
    }
    if (inn == Innovations::Student)
        p.nu = NuFloor + std::exp(x[i++]);
    return p;
}

Vec pack(const MsGarchParams &p, const Bounds &bounds)
{
    Vec x;
    x.push_back(p.mu);
    for (const GarchRegime &g : p.garch) {
        const double a = std::min(g.alpha, bounds.maxPersistence * 0.98) / bounds.maxPersistence;
        const double b = std::min(g.beta, bounds.maxPersistence * 0.98) / bounds.maxPersistence;
        const double rest = std::max(1e-6, 1 - a - b);
        x.push_back(std::log(std::max(1e-300, g.omega - bounds.minOmega)));
        x.push_back(std::log(std::max(1e-6, a) / rest));
        x.push_back(std::log(std::max(1e-6, b) / rest));
    }
    const double minStay = p.regimes == 1 ? 1.0 : bounds.minStay;
    for (int k = 0; k < p.regimes; ++k) {
        // Back out the softmax shares from the row, the floor removed from P(stay).
        Vec share(p.regimes);
        for (int j = 0; j < p.regimes; ++j)
            share[j] = std::max(1e-9, (p.transition(k, j) - (j == k ? minStay : 0.0)) / std::max(1e-9, 1 - minStay));
        for (int j = 0; j + 1 < p.regimes; ++j)
            x.push_back(std::log(share[j] / share[p.regimes - 1]));
    }
    if (p.innovations == Innovations::Student)
        x.push_back(std::log(std::max(1e-6, p.nu - NuFloor)));
    return x;
}

// log density of ε given the variance, for normal or standardized Student innovations; the parts that
// don't depend on ε are computed once per pass (Density::prepare).
struct Density {
    Innovations inn = Innovations::Normal;
    double nu = NaN;
    double logConst = 0;   // Student: lgamma((ν+1)/2) - lgamma(ν/2) - ½ log(νπ) + ½ log(ν / (ν - 2))
    void prepare(const MsGarchParams &p)
    {
        inn = p.innovations;
        nu = p.nu;
        if (inn == Innovations::Student)
            logConst = std::lgamma((nu + 1) / 2) - std::lgamma(nu / 2) - 0.5 * std::log(nu * M_PI) + 0.5 * std::log(nu / (nu - 2));
    }
    double operator()(double eps, double sigma2) const
    {
        if (inn == Innovations::Normal)
            return -0.5 * (std::log(2 * M_PI) + std::log(sigma2) + eps * eps / sigma2);
        // ε = s z with s = σ √((ν-2)/ν): log f(z) - log s.
        const double s2 = sigma2 * (nu - 2) / nu;
        return logConst - 0.5 * std::log(sigma2) - (nu + 1) / 2 * std::log1p(eps * eps / (nu * s2));
    }
};

double sampleVariance(const Vec &r, double *mean, int *count)
{
    double m = 0;
    int n = 0;
    for (double v : r)
        if (ok(v)) {
            m += v;
            ++n;
        }
    m /= std::max(1, n);
    double s = 0;
    for (double v : r)
        if (ok(v))
            s += (v - m) * (v - m);
    if (mean) *mean = m;
    if (count) *count = n;
    return s / std::max(1, n - 1);
}

// Hamilton's filter. Fills the fit's matrices when `full`, else only the log-likelihood (the objective).
double hamilton(const Vec &r, const MsGarchParams &p, double startVariance, MsGarchFit *fit)
{
    const int T = r.size(), K = p.regimes;
    const bool full = fit != nullptr;
    if (full) {
        fit->predicted = Mat(T, K);
        fit->filtered = Mat(T, K);
        fit->regimeVariance = Mat(T, K);
        fit->variance = Vec(T, NaN);
    }
    Vec xi = p.stationary();           // ξ_{t-1|t-1}, starts at the long-run distribution
    Vec sigma2(K, startVariance);      // σ²_{k,t}
    double eps2 = startVariance;       // ε²_{t-1}
    double logL = 0;
    Vec pred(K), logf(K);
    Density density;
    density.prepare(p);
    for (int t = 0; t < T; ++t) {
        // Predicted regime probabilities and this step's variances.
        for (int j = 0; j < K; ++j) {
            double s = 0;
            for (int k = 0; k < K; ++k)
                s += p.transition(k, j) * xi[k];
            pred[j] = s;
        }
        if (t > 0)
            for (int k = 0; k < K; ++k)
                sigma2[k] = p.garch[k].omega + p.garch[k].alpha * eps2 + p.garch[k].beta * sigma2[k];
        double mix = 0;
        for (int k = 0; k < K; ++k)
            mix += pred[k] * sigma2[k];
        if (full) {
            for (int k = 0; k < K; ++k) {
                fit->predicted(t, k) = pred[k];
                fit->regimeVariance(t, k) = sigma2[k];
            }
            fit->variance[t] = mix;
        }
        if (!ok(r[t])) {
            // A missing return: nothing to learn, the expected squared innovation carries the recursion.
            xi = pred;
            eps2 = mix;
            if (full)
                for (int k = 0; k < K; ++k)
                    fit->filtered(t, k) = xi[k];
            continue;
        }
        const double eps = r[t] - p.mu;
        for (int k = 0; k < K; ++k)
            logf[k] = (pred[k] > 0 ? std::log(pred[k]) : -INFINITY) + density(eps, sigma2[k]);
        const double lf = logSumExp(logf);
        if (!ok(lf))
            return NaN;
        logL += lf;
        for (int k = 0; k < K; ++k)
            xi[k] = std::exp(logf[k] - lf);
        eps2 = eps * eps;
        if (full)
            for (int k = 0; k < K; ++k)
                fit->filtered(t, k) = xi[k];
    }
    if (full)
        fit->lastEps2 = eps2;
    return logL;
}

MsGarchParams startParams(int K, Innovations inn, double mean, double variance, std::mt19937 *rng)
{
    MsGarchParams p;
    p.regimes = K;
    p.innovations = inn;
    p.mu = mean;
    p.nu = inn == Innovations::Student ? 6 : NaN;
    p.garch.resize(K);
    p.transition = Mat(K, K);
    std::uniform_real_distribution<double> ua(0.03, 0.15), ub(0.75, 0.93), uf(0.7, 1.4), up(0.9, 0.99);
    for (int k = 0; k < K; ++k) {
        // Regimes spread from half to twice the sample variance (one regime: the sample variance).
        double factor = K == 1 ? 1.0 : std::pow(2.0, -1.0 + 2.0 * k / (K - 1));
        double alpha = 0.08, beta = 0.88;
        if (rng) {
            factor *= uf(*rng);
            alpha = ua(*rng);
            beta = ub(*rng);
        }
        p.garch[k].alpha = alpha;
        p.garch[k].beta = beta;
        p.garch[k].omega = factor * variance * (1 - alpha - beta);
    }
    for (int k = 0; k < K; ++k) {
        const double stay = K == 1 ? 1.0 : rng ? up(*rng) : 0.97;
        for (int j = 0; j < K; ++j)
            p.transition(k, j) = k == j ? stay : (1 - stay) / (K - 1);
    }
    return p;
}

} // namespace

// ---------------------------------------------------------------- params

int MsGarchParams::parameterCount() const
{
    return 1 + 3 * regimes + regimes * (regimes - 1) + (innovations == Innovations::Student ? 1 : 0);
}

double MsGarchParams::expectedDuration(int k) const
{
    const double p = transition(k, k);
    return p < 1 ? 1.0 / (1.0 - p) : INFINITY;
}

Vec MsGarchParams::stationary() const
{
    // Power iteration from uniform: enough for a small ergodic chain.
    Vec pi(regimes, 1.0 / regimes), next(regimes);
    for (int it = 0; it < 500; ++it) {
        double diff = 0;
        for (int j = 0; j < regimes; ++j) {
            double s = 0;
            for (int k = 0; k < regimes; ++k)
                s += transition(k, j) * pi[k];
            next[j] = s;
            diff += std::fabs(s - pi[j]);
        }
        pi = next;
        if (diff < 1e-12)
            break;
    }
    return pi;
}

MsGarchParams MsGarchParams::sortedByVariance() const
{
    QVector<int> order(regimes);
    for (int k = 0; k < regimes; ++k)
        order[k] = k;
    auto key = [this](int k) {
        const double u = garch[k].unconditionalVariance();
        return ok(u) ? u : garch[k].omega * 1e6;   // explosive regimes last
    };
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return key(a) < key(b); });
    MsGarchParams out = *this;
    for (int k = 0; k < regimes; ++k) {
        out.garch[k] = garch[order[k]];
        for (int j = 0; j < regimes; ++j)
            out.transition(k, j) = transition(order[k], order[j]);
    }
    return out;
}

double studentScale(double sigma, double nu)
{
    return sigma * std::sqrt((nu - 2) / nu);
}

double MsGarch::aic(double logL, int parameters)
{
    return 2 * parameters - 2 * logL;
}

double MsGarch::bic(double logL, int parameters, int n)
{
    return parameters * std::log(double(n)) - 2 * logL;
}

// ---------------------------------------------------------------- fit and filter

MsGarchFit MsGarch::filter(const Vec &returns, const MsGarchParams &p)
{
    MsGarchFit fit;
    int n = 0;
    const double var = sampleVariance(returns, nullptr, &n);
    fit.params = p;
    fit.observations = n;
    fit.logLikelihood = hamilton(returns, p, var, &fit);
    fit.aic = aic(fit.logLikelihood, p.parameterCount());
    fit.bic = bic(fit.logLikelihood, p.parameterCount(), n);
    return fit;
}

MsGarchFit MsGarch::fit(const Vec &returns, const MsGarchOptions &o, const Progress &progress)
{
    MsGarchFit best;
    double mean;
    int n;
    const double var = sampleVariance(returns, &mean, &n);
    const int K = std::max(1, o.regimes);
    if (n < 50 * K) {
        best.error = "Not enough returns for the number of regimes";
        return best;
    }
    if (!(var > 0)) {
        best.error = "The returns don't vary";
        return best;
    }
    std::mt19937 rng(o.seed);
    const int starts = std::max(1, o.starts);
    const Bounds bounds {o.maxPersistence, std::clamp(o.minStay, 0.0, 0.999), o.minOmegaFraction * var};
    Objective negLogL = [&](const Vec &x) {
        const double l = hamilton(returns, unpack(x, K, o.innovations, bounds), var, nullptr);
        return ok(l) ? -l : INFINITY;
    };
    OptimOptions oo;
    oo.maxIterations = o.maxIterations;
    oo.tolerance = 1e-9;
    // Nelder-Mead only brings BFGS near the optimum: run to a tight tolerance on 9 or 10 parameters it
    // took most of the evaluations for little. (1e-6 lost the best optimum of one test series.)
    oo.nelderMeadTolerance = 1e-7;
    oo.nelderMeadMaxIterations = std::min(o.maxIterations, 200 * int(pack(startParams(K, o.innovations, mean, var, nullptr), bounds).size()));
    // The starts are drawn in order (the same as one after the other), then optimised side by side.
    QVector<Vec> startPoints;
    for (int s = 0; s < starts; ++s)
        startPoints.push_back(pack(startParams(K, o.innovations, mean, var, s == 0 ? nullptr : &rng), bounds));
    QVector<OptimResult> results(starts);
    std::atomic<int> done = 0;
    parallelFor(starts, [&](int s) {
        if (progress.cancelled()) {
            results[s].cancelled = true;
            return;
        }
        Progress sub;
        sub.cancel = progress.cancel;
        sub.report = [&progress, &done, s, starts](int, const QString &text) {
            progress(100 * done.load() / starts, QString("Start %1/%2 · %3").arg(s + 1).arg(starts).arg(text));
        };
        results[s] = minimize(negLogL, startPoints[s], oo, sub);
        ++done;
    });
    bool any = false;
    for (int s = 0; s < starts; ++s) {
        const OptimResult &r = results[s];
        if (r.cancelled) {
            best.cancelled = true;
            break;
        }
        if (!ok(r.value))
            continue;
        if (!any || -r.value > best.logLikelihood) {
            any = true;
            best.params = unpack(r.x, K, o.innovations, bounds);
            best.logLikelihood = -r.value;
            best.evaluations = r.evaluations;
            best.converged = r.converged;
        }
    }
    if (!any) {
        if (best.error.isEmpty() && !best.cancelled)
            best.error = "The likelihood could not be maximised";
        return best;
    }
    const bool cancelled = best.cancelled, converged = best.converged;
    const int evaluations = best.evaluations;
    best = filter(returns, best.params.sortedByVariance());
    best.cancelled = cancelled;
    best.converged = converged;
    best.evaluations = evaluations;
    progress(100, "Done");
    return best;
}

// ---------------------------------------------------------------- forecast and risk

VolForecast MsGarch::forecast(const MsGarchFit &fit, int horizon)
{
    VolForecast f;
    const MsGarchParams &p = fit.params;
    const int T = fit.filtered.rows, K = p.regimes;
    if (T == 0 || horizon < 1)
        return f;
    f.variance = Vec(horizon);
    f.regimeVariance = Mat(horizon, K);
    f.regimeProb = Mat(horizon, K);
    Vec xi = fit.filtered.row(T - 1), sigma2 = fit.regimeVariance.row(T - 1);
    double eps2 = fit.lastEps2;   // the last squared innovation (its expectation when the last return was missing)
    f.totalVariance = 0;
    for (int j = 0; j < horizon; ++j) {
        Vec next(K);
        for (int l = 0; l < K; ++l) {
            double s = 0;
            for (int k = 0; k < K; ++k)
                s += p.transition(k, l) * xi[k];
            next[l] = s;
        }
        xi = next;
        for (int k = 0; k < K; ++k)
            sigma2[k] = p.garch[k].omega + p.garch[k].alpha * eps2 + p.garch[k].beta * sigma2[k];
        double mix = 0;
        for (int k = 0; k < K; ++k) {
            mix += xi[k] * sigma2[k];
            f.regimeVariance(j, k) = sigma2[k];
            f.regimeProb(j, k) = xi[k];
        }
        f.variance[j] = mix;
        f.totalVariance += mix;
        eps2 = mix;   // E[ε²] over the mixture for the next step
    }
    return f;
}

double mixtureVar(const Vec &w, const Vec &means, const Vec &scales, double nu, double level)
{
    auto cdf = [&](double x) {
        double s = 0;
        for (int k = 0; k < w.size(); ++k) {
            const double z = (x - means[k]) / scales[k];
            s += w[k] * (ok(nu) ? studentCdf(z, nu) : normCdf(z));
        }
        return s;
    };
    // The loss quantile at `level` is minus the return quantile at 1 - level.
    double lo = -1e6, hi = 1e6;
    for (int i = 0; i < 300 && hi - lo > 1e-10 * std::max(1.0, std::fabs(lo) + std::fabs(hi)); ++i) {
        const double mid = 0.5 * (lo + hi);
        if (cdf(mid) < 1 - level)
            lo = mid;
        else
            hi = mid;
    }
    return -0.5 * (lo + hi);
}

double mixtureEs(const Vec &w, const Vec &means, const Vec &scales, double nu, double level)
{
    const double q = -mixtureVar(w, means, scales, nu, level);   // return threshold
    double tail = 0;
    for (int k = 0; k < w.size(); ++k) {
        const double z = (q - means[k]) / scales[k];
        const double F = ok(nu) ? studentCdf(z, nu) : normCdf(z);
        const double partial = ok(nu) ? studentPartial(z, nu) : normPartial(z);
        // E[-r · 1{r < q}] for this component.
        tail += w[k] * (-means[k] * F - scales[k] * partial);
    }
    return tail / (1 - level);
}

Risk MsGarch::riskOneDay(const MsGarchFit &fit, const VolForecast &f)
{
    Risk r;
    const MsGarchParams &p = fit.params;
    if (f.variance.isEmpty())
        return r;
    const int K = p.regimes;
    Vec w(K), means(K, p.mu), scales(K);
    for (int k = 0; k < K; ++k) {
        w[k] = f.regimeProb(0, k);
        const double sigma = std::sqrt(f.regimeVariance(0, k));
        scales[k] = p.innovations == Innovations::Student ? studentScale(sigma, p.nu) : sigma;
    }
    const double nu = p.innovations == Innovations::Student ? p.nu : NaN;
    r.var95 = mixtureVar(w, means, scales, nu, 0.95);
    r.var99 = mixtureVar(w, means, scales, nu, 0.99);
    r.es95 = mixtureEs(w, means, scales, nu, 0.95);
    r.es99 = mixtureEs(w, means, scales, nu, 0.99);
    return r;
}

Risk MsGarch::riskHorizon(const MsGarchFit &fit, const VolForecast &f)
{
    Risk r;
    const MsGarchParams &p = fit.params;
    if (f.variance.isEmpty())
        return r;
    const double sigma = std::sqrt(f.totalVariance);
    const Vec w {1.0}, means {p.mu * f.variance.size()};
    const Vec scales {p.innovations == Innovations::Student ? studentScale(sigma, p.nu) : sigma};
    const double nu = p.innovations == Innovations::Student ? p.nu : NaN;
    r.var95 = mixtureVar(w, means, scales, nu, 0.95);
    r.var99 = mixtureVar(w, means, scales, nu, 0.99);
    r.es95 = mixtureEs(w, means, scales, nu, 0.95);
    r.es99 = mixtureEs(w, means, scales, nu, 0.99);
    return r;
}

} // namespace markov
