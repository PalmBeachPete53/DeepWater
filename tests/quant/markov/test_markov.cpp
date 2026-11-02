// qtm_test_markov: the Markov engine against data simulated from known models. Same style as the
// other test programs: plain checks, a count at the end, non-zero exit on any failure.
#include "markov/Dist.h"
#include "markov/Execution.h"
#include "markov/Hmm.h"
#include "markov/MsGarch.h"
#include "markov/Optim.h"
#include "markov/Simulate.h"

#include <QElapsedTimer>
#include <QString>

#include <cstdlib>
#include <iostream>
#include <random>

using namespace markov;

namespace {

int checks = 0, fails = 0;
QString CAT = "misc";

void check(bool ok, const QString &msg)
{
    ++checks;
    if (!ok) {
        ++fails;
        std::cout << "FAIL [" << CAT.toStdString() << "] " << msg.toStdString() << "\n";
    }
}
void checkNear(double want, double got, double tol, const QString &ctx)
{
    check(std::isfinite(got) && std::fabs(want - got) <= tol,
          QStringLiteral("%1: want %2 ± %3, got %4").arg(ctx).arg(want).arg(tol).arg(got));
}

HmmParams twoStates(double m0, double s0, double m1, double s1, double stay0, double stay1)
{
    HmmParams p;
    p.states = 2;
    p.dims = 1;
    p.initial = {0.5, 0.5};
    p.transition = Mat(2, 2);
    p.transition(0, 0) = stay0;
    p.transition(0, 1) = 1 - stay0;
    p.transition(1, 0) = 1 - stay1;
    p.transition(1, 1) = stay1;
    p.means = {{m0}, {m1}};
    p.covariances = {Mat(1, 1, s0 * s0), Mat(1, 1, s1 * s1)};
    return p;
}

// ---------------------------------------------------------------- HMM

void testHmmRecovery()
{
    CAT = "hmm/recovery";
    std::mt19937 rng(11);
    const HmmParams truth = twoStates(0.06, 0.6, -0.08, 1.8, 0.97, 0.92);
    QVector<int> states;
    const Mat obs = simulateHmm(truth, 4000, rng, &states);
    HmmOptions o;
    o.states = 2;
    const HmmFit fit = GaussianHmm::fit(obs, o);
    check(fit.error.isEmpty(), "fit: " + fit.error);
    check(fit.converged, "EM converged");
    checkNear(0.06, fit.params.means[0][0], 0.06, "mean of the calm state");
    checkNear(-0.08, fit.params.means[1][0], 0.2, "mean of the turbulent state");
    checkNear(0.6, fit.params.volatility(0), 0.06, "vol of the calm state");
    checkNear(1.8, fit.params.volatility(1), 0.18, "vol of the turbulent state");
    checkNear(0.97, fit.params.transition(0, 0), 0.02, "p00");
    checkNear(0.92, fit.params.transition(1, 1), 0.04, "p11");
    check(fit.params.volatility(0) < fit.params.volatility(1), "states sorted by volatility");
    // The filtered log-likelihood is the fit's; the posteriors sum to one; Viterbi mostly right.
    double logL;
    const Mat filtered = GaussianHmm::filter(obs, fit.params, &logL);
    checkNear(fit.logLikelihood, logL, 1e-6 * std::fabs(logL), "filter log-likelihood");
    const Mat smoothed = GaussianHmm::smooth(obs, fit.params);
    bool sums = true;
    for (int t = 0; t < obs.rows; ++t)
        sums = sums && std::fabs(filtered(t, 0) + filtered(t, 1) - 1) < 1e-9 && std::fabs(smoothed(t, 0) + smoothed(t, 1) - 1) < 1e-9;
    check(sums, "probabilities sum to one");
    const QVector<int> path = GaussianHmm::viterbi(obs, fit.params);
    int agree = 0;
    for (int t = 0; t < obs.rows; ++t)
        agree += path[t] == states[t];
    check(agree > 0.85 * obs.rows, QString("viterbi agrees with the true path on %1%").arg(100.0 * agree / obs.rows, 0, 'f', 1));
    // Criteria: two states beat one; the three-state model doesn't beat two by BIC.
    HmmOptions o1 = o;
    o1.states = 1;
    HmmOptions o3 = o;
    o3.states = 3;
    const HmmFit f1 = GaussianHmm::fit(obs, o1), f3 = GaussianHmm::fit(obs, o3);
    check(fit.bic < f1.bic, "BIC prefers two states to one");
    check(fit.bic <= f3.bic + 1e-6 || f3.logLikelihood - fit.logLikelihood < 20,
          QString("BIC doesn't prefer three states (K=2 %1, K=3 %2)").arg(fit.bic).arg(f3.bic));
    check(fit.params.expectedDuration(0) > 20 && fit.params.expectedDuration(0) < 60, "calm duration ≈ 33");

    // Two variables (return and volatility proxy), three states.
    CAT = "hmm/multivariate";
    HmmParams p3;
    p3.states = 3;
    p3.dims = 2;
    p3.initial = {1.0 / 3, 1.0 / 3, 1.0 / 3};
    p3.transition = Mat(3, 3, 0.03);
    for (int k = 0; k < 3; ++k)
        p3.transition(k, k) = 0.94;
    p3.means = {{0.05, 0.5}, {0.0, 1.0}, {-0.1, 2.5}};
    p3.covariances = {Mat(2, 2), Mat(2, 2), Mat(2, 2)};
    const double vols[3] = {0.5, 1.0, 2.5};
    for (int k = 0; k < 3; ++k) {
        p3.covariances[k](0, 0) = vols[k] * vols[k];
        p3.covariances[k](1, 1) = 0.04 * vols[k] * vols[k];
        p3.covariances[k](0, 1) = p3.covariances[k](1, 0) = 0.1 * vols[k] * vols[k];
    }
    const Mat obs3 = simulateHmm(p3, 5000, rng);
    o3.states = 3;
    const HmmFit fit3 = GaussianHmm::fit(obs3, o3);
    check(fit3.error.isEmpty(), "fit3: " + fit3.error);
    for (int k = 0; k < 3; ++k) {
        checkNear(vols[k], fit3.params.volatility(k), 0.12 * vols[k], QString("state %1 vol").arg(k));
        checkNear(p3.means[k][1], fit3.params.means[k][1], 0.15, QString("state %1 second mean").arg(k));
        checkNear(0.94, fit3.params.transition(k, k), 0.03, QString("state %1 stay").arg(k));
    }
    // Warm restart from the solution ends at once at the same likelihood.
    const HmmFit again = GaussianHmm::refit(obs3, fit3.params, o3);
    check(again.iterations <= 3 && std::fabs(again.logLikelihood - fit3.logLikelihood) < 1e-3 * std::fabs(fit3.logLikelihood),
          "refit from the solution is stable");
}

void testHmmStability()
{
    CAT = "hmm/stability";
    std::mt19937 rng(3);
    const HmmParams truth = twoStates(0.0, 1.0, 0.0, 3.0, 0.98, 0.95);
    // Long series.
    {
        const Mat obs = simulateHmm(truth, 100000, rng);
        HmmOptions o;
        o.initializations = 2;
        const HmmFit fit = GaussianHmm::fit(obs, o);
        check(fit.error.isEmpty() && std::isfinite(fit.logLikelihood), "100k observations: finite likelihood");
        checkNear(3.0, fit.params.volatility(1), 0.1, "100k: turbulent vol");
    }
    // Extreme scales: the same series × 1e6 and × 1e-6 give the same regimes.
    {
        Mat obs = simulateHmm(truth, 2000, rng);
        Mat big = obs, small = obs;
        for (double &v : big.a) v *= 1e6;
        for (double &v : small.a) v *= 1e-6;
        HmmOptions o;
        o.minVariance = 0;   // only the relative floor applies here
        const HmmFit fb = GaussianHmm::fit(big, o), fs = GaussianHmm::fit(small, o);
        check(fb.error.isEmpty() && fs.error.isEmpty(), "extreme scales fit");
        checkNear(3.0, fb.params.volatility(1) / 1e6, 0.15, "scale 1e6");
        checkNear(3.0, fs.params.volatility(1) * 1e6, 0.15, "scale 1e-6");
    }
    // Missing steps: 10% of rows NaN.
    {
        Mat obs = simulateHmm(truth, 3000, rng);
        std::uniform_real_distribution<double> u(0, 1);
        int gaps = 0;
        for (int t = 0; t < obs.rows; ++t)
            if (u(rng) < 0.1) {
                obs(t, 0) = NaN;
                ++gaps;
            }
        const HmmFit fit = GaussianHmm::fit(obs, HmmOptions {});
        check(fit.error.isEmpty() && std::isfinite(fit.logLikelihood), QString("%1 missing steps: finite likelihood").arg(gaps));
        checkNear(3.0, fit.params.volatility(1), 0.2, "missing steps: turbulent vol");
        const Mat f = GaussianHmm::filter(obs, fit.params);
        bool finite = true;
        for (double v : f.a)
            finite = finite && std::isfinite(v);
        check(finite, "filter is finite through the gaps");
    }
    // Degenerate inputs don't crash: constant series, a few points, outliers.
    {
        Mat constant(500, 1, 1.0);
        const HmmFit fit = GaussianHmm::fit(constant, HmmOptions {});
        check(!fit.cancelled, "constant series handled");
        Mat few(4, 1, 0.0);
        const HmmFit f2 = GaussianHmm::fit(few, HmmOptions {});
        check(!f2.error.isEmpty(), "too few points reported: " + f2.error);
        Mat spikes = simulateHmm(truth, 1000, rng);
        spikes(500, 0) = 1e4;
        const HmmFit f3 = GaussianHmm::fit(spikes, HmmOptions {});
        check(f3.error.isEmpty() && std::isfinite(f3.logLikelihood), "an outlier of 1e4 keeps the likelihood finite");
    }
    // Cancellation stops EM early.
    {
        const Mat obs = simulateHmm(truth, 20000, rng);
        std::atomic<bool> stop {false};
        Progress p;
        p.cancel = &stop;
        p.report = [&stop](int, const QString &) { stop = true; };
        QElapsedTimer t;
        t.start();
        const HmmFit fit = GaussianHmm::fit(obs, HmmOptions {}, p);
        check(fit.cancelled, "cancel flag honoured");
        check(t.elapsed() < 2000, QString("cancelled quickly (%1 ms)").arg(t.elapsed()));
    }
}

void timeHmm()
{
    CAT = "hmm/timing";
    std::mt19937 rng(5);
    HmmParams p;
    p.states = 3;
    p.dims = 2;
    p.initial = {1.0 / 3, 1.0 / 3, 1.0 / 3};
    p.transition = Mat(3, 3, 0.03);
    for (int k = 0; k < 3; ++k)
        p.transition(k, k) = 0.94;
    p.means = {{0.05, 0.5}, {0.0, 1.0}, {-0.1, 2.5}};
    p.covariances = {Mat(2, 2), Mat(2, 2), Mat(2, 2)};
    for (int k = 0; k < 3; ++k) {
        p.covariances[k](0, 0) = 1;
        p.covariances[k](1, 1) = 0.1;
    }
    for (int n : {2520, 4700, 25000}) {
        const Mat obs = simulateHmm(p, n, rng);
        HmmOptions o;
        o.states = 3;
        QElapsedTimer t;
        t.start();
        const HmmFit fit = GaussianHmm::fit(obs, o);
        std::cout << "timing: HMM K=3 D=2, " << n << " obs, " << o.initializations << " starts: " << t.elapsed() << " ms ("
                  << fit.iterations << " EM iterations on the best start)\n";
        check(fit.error.isEmpty(), "timing fit ok");
    }
}


// ---------------------------------------------------------------- optimiser and distributions

void testOptim()
{
    CAT = "optim";
    // Rosenbrock from a poor start.
    Objective rosen = [](const Vec &x) { return 100 * std::pow(x[1] - x[0] * x[0], 2) + std::pow(1 - x[0], 2); };
    const OptimResult r = minimize(rosen, {-1.2, 1.0});
    checkNear(1, r.x[0], 1e-3, "rosenbrock x");
    checkNear(1, r.x[1], 1e-3, "rosenbrock y");
    // A quadratic in 8 dimensions.
    Objective quad = [](const Vec &x) {
        double s = 0;
        for (int i = 0; i < x.size(); ++i)
            s += (i + 1) * std::pow(x[i] - i, 2);
        return s;
    };
    const OptimResult q = minimize(quad, Vec(8, 5.0));
    for (int i = 0; i < 8; ++i)
        checkNear(i, q.x[i], 1e-3, QString("quadratic x%1").arg(i));

    CAT = "dist";
    checkNear(1.6449, normInv(0.95), 1e-3, "normal 95% quantile");
    checkNear(2.3263, normInv(0.99), 1e-3, "normal 99% quantile");
    checkNear(2.0150, studentInv(0.95, 5), 1e-3, "t(5) 95% quantile");
    checkNear(0.975, studentCdf(2.228, 10), 1e-3, "t(10) cdf at 2.228");
    // Losses of a unit normal: VaR 1.645, ES 2.063; of a standardized t(5): VaR 1.561, ES larger.
    checkNear(1.6449, mixtureVar({1}, {0}, {1}, NaN, 0.95), 1e-3, "VaR95 normal");
    checkNear(2.0627, mixtureEs({1}, {0}, {1}, NaN, 0.95), 1e-3, "ES95 normal");
    const double s5 = studentScale(1, 5);
    checkNear(2.0150 * s5, mixtureVar({1}, {0}, {s5}, 5, 0.95), 1e-3, "VaR95 standardized t(5)");
    const double es5 = mixtureEs({1}, {0}, {s5}, 5, 0.95);
    check(es5 > 2.0150 * s5 && es5 < 3, QString("ES95 t(5) = %1").arg(es5));
    // Fat tails: at 99% the t(5) loss exceeds the normal's.
    check(mixtureVar({1}, {0}, {s5}, 5, 0.99) > mixtureVar({1}, {0}, {1}, NaN, 0.99), "t(5) 99% VaR above normal");
    // A mixture sits between its components.
    const double mixVar = mixtureVar({0.5, 0.5}, {0, 0}, {1, 2}, NaN, 0.95);
    check(mixVar > 1.6449 && mixVar < 3.2897, QString("mixture VaR %1 between its components").arg(mixVar));
    check(mixtureEs({0.5, 0.5}, {0, 0}, {1, 2}, NaN, 0.95) > mixVar, "mixture ES above its VaR");
}

// ---------------------------------------------------------------- MS-GARCH

MsGarchParams garchTruth()
{
    MsGarchParams p;
    p.regimes = 1;
    p.mu = 0.02;
    p.garch = {{0.05, 0.08, 0.90}};
    p.transition = Mat(1, 1, 1.0);
    return p;
}

MsGarchParams msGarchTruth()
{
    MsGarchParams p;
    p.regimes = 2;
    p.mu = 0.03;
    p.garch = {{0.02, 0.05, 0.90}, {0.5, 0.12, 0.80}};   // long-run variances 0.4 and 6.25
    p.transition = Mat(2, 2);
    p.transition(0, 0) = 0.98;
    p.transition(0, 1) = 0.02;
    p.transition(1, 0) = 0.04;
    p.transition(1, 1) = 0.96;
    return p;
}

void testGarchRecovery()
{
    CAT = "garch/recovery";
    std::mt19937 rng(21);
    const MsGarchParams truth = garchTruth();
    const Vec r = simulateMsGarch(truth, 5000, rng);
    MsGarchOptions o;
    o.regimes = 1;
    const MsGarchFit fit = MsGarch::fit(r, o);
    check(fit.error.isEmpty(), "garch fit: " + fit.error);
    checkNear(0.08, fit.params.garch[0].alpha, 0.03, "alpha");
    checkNear(0.90, fit.params.garch[0].beta, 0.04, "beta");
    checkNear(2.5, fit.params.garch[0].unconditionalVariance(), 0.6, "long-run variance");
    checkNear(0.02, fit.params.mu, 0.05, "mu");
    const MsGarchFit atTruth = MsGarch::filter(r, truth);
    check(fit.logLikelihood >= atTruth.logLikelihood - 1, QString("fit at least as likely as the truth (%1 vs %2)")
                                                              .arg(fit.logLikelihood).arg(atTruth.logLikelihood));
    // The variance series is the one-step-ahead forecast: finite, positive, and about the sample variance on average.
    double mean = 0;
    bool finite = true;
    for (double v : fit.variance) {
        finite = finite && std::isfinite(v) && v > 0;
        mean += v / fit.variance.size();
    }
    check(finite, "variance series finite and positive");
    checkNear(2.5, mean, 0.6, "mean predicted variance");
    // Long forecasts settle at the long-run variance.
    const VolForecast f = MsGarch::forecast(fit, 600);
    checkNear(fit.params.garch[0].unconditionalVariance(), f.variance.last(), 0.02 * fit.params.garch[0].unconditionalVariance(),
              "600-day forecast at the long-run variance");
    check(f.totalVariance > 0 && std::isfinite(f.totalVariance), "total variance finite");
    const Risk one = MsGarch::riskOneDay(fit, f), ten = MsGarch::riskHorizon(fit, MsGarch::forecast(fit, 10));
    check(one.var99 > one.var95 && one.es95 > one.var95 && one.es99 > one.var99, "one-day risk ordered");
    check(ten.var95 > one.var95, "ten-day VaR above one-day");

    CAT = "garch/student";
    MsGarchParams tt = truth;
    tt.innovations = Innovations::Student;
    tt.nu = 5;
    const Vec rt = simulateMsGarch(tt, 6000, rng);
    MsGarchOptions os;
    os.regimes = 1;
    os.innovations = Innovations::Student;
    const MsGarchFit fs = MsGarch::fit(rt, os);
    check(fs.error.isEmpty(), "student fit: " + fs.error);
    check(fs.params.nu > 3.5 && fs.params.nu < 8, QString("nu recovered: %1").arg(fs.params.nu));
    checkNear(0.08, fs.params.garch[0].alpha, 0.03, "student alpha");
    checkNear(0.90, fs.params.garch[0].beta, 0.04, "student beta");
    // The Student model fits its own data better than the normal one by BIC.
    MsGarchOptions on = os;
    on.innovations = Innovations::Normal;
    const MsGarchFit fn = MsGarch::fit(rt, on);
    check(fs.bic < fn.bic, QString("student BIC %1 < normal %2").arg(fs.bic).arg(fn.bic));
}

void testMsGarchRecovery()
{
    CAT = "msgarch/recovery";
    std::mt19937 rng(23);
    const MsGarchParams truth = msGarchTruth();
    QVector<int> states;
    const Vec r = simulateMsGarch(truth, 6000, rng, &states);
    MsGarchOptions o;
    o.regimes = 2;
    const MsGarchFit fit = MsGarch::fit(r, o);
    check(fit.error.isEmpty(), "ms fit: " + fit.error);
    const MsGarchFit atTruth = MsGarch::filter(r, truth);
    check(fit.logLikelihood >= atTruth.logLikelihood - 2, QString("fit at least as likely as the truth (%1 vs %2)")
                                                              .arg(fit.logLikelihood).arg(atTruth.logLikelihood));
    checkNear(0.4, fit.params.garch[0].unconditionalVariance(), 0.15, "calm long-run variance");
    checkNear(6.25, fit.params.garch[1].unconditionalVariance(), 2.0, "turbulent long-run variance");
    checkNear(0.90, fit.params.garch[0].beta, 0.08, "calm beta");
    checkNear(0.80, fit.params.garch[1].beta, 0.10, "turbulent beta");
    checkNear(0.98, fit.params.transition(0, 0), 0.02, "p00");
    checkNear(0.96, fit.params.transition(1, 1), 0.03, "p11");
    // The filtered regime agrees with the true path most of the time.
    int agree = 0;
    for (int t = 0; t < r.size(); ++t)
        agree += (fit.filtered(t, 1) > 0.5 ? 1 : 0) == states[t];
    check(agree > 0.80 * r.size(), QString("filtered regime right on %1%").arg(100.0 * agree / r.size(), 0, 'f', 1));
    // Two regimes beat one on this data.
    MsGarchOptions o1 = o;
    o1.regimes = 1;
    const MsGarchFit single = MsGarch::fit(r, o1);
    check(fit.bic < single.bic, QString("MS-GARCH BIC %1 < GARCH %2").arg(fit.bic).arg(single.bic));
    const VolForecast f = MsGarch::forecast(fit, 60);
    bool finite = true;
    for (double v : f.variance)
        finite = finite && std::isfinite(v) && v > 0;
    check(finite, "forecast finite and positive");
    double sum = 0;
    for (int k = 0; k < 2; ++k)
        sum += f.regimeProb(59, k);
    checkNear(1, sum, 1e-9, "forecast regime probabilities sum to one");
}

void testMsGarchStability()
{
    CAT = "msgarch/stability";
    std::mt19937 rng(29);
    const MsGarchParams truth = msGarchTruth();
    {
        Vec r = simulateMsGarch(truth, 20000, rng);
        MsGarchOptions o;
        o.starts = 1;
        const MsGarchFit fit = MsGarch::fit(r, o);
        check(fit.error.isEmpty() && std::isfinite(fit.logLikelihood), "20k returns");
    }
    {
        // Returns in basis points and in fractions give the same regimes.
        Vec r = simulateMsGarch(truth, 3000, rng);
        Vec big = r, small = r;
        for (double &v : big) v *= 100;
        for (double &v : small) v *= 0.01;
        MsGarchOptions o;
        o.starts = 2;
        const MsGarchFit fb = MsGarch::fit(big, o), fs = MsGarch::fit(small, o);
        check(fb.error.isEmpty() && fs.error.isEmpty(), "scaled fits");
        checkNear(0.96, fb.params.transition(1, 1), 0.04, "scale 100: p11");
        checkNear(0.96, fs.params.transition(1, 1), 0.04, "scale 0.01: p11");
    }
    {
        Vec r = simulateMsGarch(truth, 3000, rng);
        std::uniform_real_distribution<double> u(0, 1);
        for (double &v : r)
            if (u(rng) < 0.05)
                v = NaN;
        const MsGarchFit fit = MsGarch::fit(r, MsGarchOptions {});
        check(fit.error.isEmpty() && std::isfinite(fit.logLikelihood), "5% missing returns");
        bool finite = true;
        for (double v : fit.variance)
            finite = finite && std::isfinite(v);
        check(finite, "variance finite through the gaps");
    }
    {
        const MsGarchFit c = MsGarch::fit(Vec(500, 0.0), MsGarchOptions {});
        check(!c.error.isEmpty(), "constant returns reported: " + c.error);
        const MsGarchFit few = MsGarch::fit(Vec(30, 1.0), MsGarchOptions {});
        check(!few.error.isEmpty(), "too few returns reported");
    }
    {
        const Vec r = simulateMsGarch(truth, 20000, rng);
        std::atomic<bool> stop {false};
        Progress p;
        p.cancel = &stop;
        p.report = [&stop](int, const QString &) { stop = true; };
        QElapsedTimer t;
        t.start();
        const MsGarchFit fit = MsGarch::fit(r, MsGarchOptions {}, p);
        check(fit.cancelled, "cancel honoured");
        check(t.elapsed() < 3000, QString("cancelled quickly (%1 ms)").arg(t.elapsed()));
    }
}

void timeMsGarch()
{
    CAT = "msgarch/timing";
    std::mt19937 rng(31);
    const MsGarchParams truth = msGarchTruth();
    for (int n : {2520, 4700}) {
        const Vec r = simulateMsGarch(truth, n, rng);
        for (Innovations inn : {Innovations::Normal, Innovations::Student}) {
            MsGarchOptions o;
            o.innovations = inn;
            QElapsedTimer t;
            t.start();
            const MsGarchFit fit = MsGarch::fit(r, o);
            std::cout << "timing: MS-GARCH K=2 " << (inn == Innovations::Normal ? "normal" : "student") << ", " << n << " obs, "
                      << o.starts << " starts: " << t.elapsed() << " ms (" << fit.evaluations << " likelihood evaluations on the best start)\n";
            check(fit.error.isEmpty(), "timing fit ok");
        }
    }
}


// ---------------------------------------------------------------- execution

ExecutionParams execParams()
{
    ExecutionParams p;
    p.quantity = 1e6;
    p.price = 50;
    p.horizon = 5;
    p.intervals = 20;
    p.sigma = 0.95;         // 1.9% daily on 50
    p.lambda = 2e-6;
    p.temporary = 2.5e-6;   // 0.3 bps... in the paper's units: $/share per share/day
    p.permanent = 2.5e-7;
    p.spread = 0.0625;
    return p;
}

void testExecution()
{
    CAT = "execution/almgren-chriss";
    const ExecutionParams p = execParams();
    const Trajectory ac = almgrenChriss(p), tw = twap(p);
    double sold = 0;
    for (double n : ac.trades)
        sold += n;
    checkNear(p.quantity, sold, 1e-6 * p.quantity, "AC sells everything");
    check(ac.trades.first() > ac.trades.last(), "AC front-loads with risk aversion");
    check(ac.objective < tw.objective, "AC objective below TWAP's");
    check(ac.expectedCost > tw.expectedCost && ac.variance < tw.variance, "AC pays more impact for less variance");
    // No risk aversion: TWAP.
    ExecutionParams p0 = p;
    p0.lambda = 0;
    const Trajectory ac0 = almgrenChriss(p0);
    for (int k = 0; k < p0.intervals; ++k)
        checkNear(p0.quantity / p0.intervals, ac0.trades[k], 1e-6 * p0.quantity, QString("λ = 0 trade %1 is TWAP").arg(k));
    // The paper's formulas: with these units the optimal schedule has κ ≈ 0.6 / day.
    const double k = kappa(p);
    check(k > 0.3 && k < 1.5, QString("kappa %1 per day").arg(k));

    CAT = "execution/mdp";
    const auto mdp = MdpPolicy::solve(p, 400);
    const Trajectory mt = mdp->trajectory();
    double maxDiff = 0;
    for (int j = 0; j <= p.intervals; ++j)
        maxDiff = std::max(maxDiff, std::fabs(mt.holdings[j] - ac.holdings[j]));
    check(maxDiff < 0.01 * p.quantity, QString("MDP trajectory within 1% of Almgren-Chriss (max %1%)").arg(100 * maxDiff / p.quantity, 0, 'f', 2));
    check(mt.objective <= ac.objective * 1.001, QString("MDP objective %1 vs AC %2").arg(mt.objective).arg(ac.objective));
    checkNear(ac.objective, mdp->value(p.intervals, p.quantity, 0), 0.002 * ac.objective, "MDP value at the start equals the AC objective");
    // Finer grid, closer.
    const auto fine = MdpPolicy::solve(p, 2000);
    double maxFine = 0;
    for (int j = 0; j <= p.intervals; ++j)
        maxFine = std::max(maxFine, std::fabs(fine->trajectory().holdings[j] - ac.holdings[j]));
    check(maxFine < maxDiff, "a finer grid gets closer");
    // Regimes: starting turbulent sells faster than starting calm.
    RegimeModel rm;
    rm.sigma = {0.6, 1.8};
    rm.transition = RegimeModel::perInterval([] {
        Mat d(2, 2);
        d(0, 0) = 0.97; d(0, 1) = 0.03; d(1, 0) = 0.06; d(1, 1) = 0.94;
        return d;
    }(), p.tau());
    rm.initial = {0.5, 0.5};
    const auto rmdp = MdpPolicy::solve(p, 300, &rm);
    const Trajectory calm = rmdp->trajectory(0), turb = rmdp->trajectory(1);
    check(turb.trades.first() > calm.trades.first(), QString("turbulent start sells %1 first vs calm %2").arg(turb.trades.first()).arg(calm.trades.first()));
    check(rmdp->value(p.intervals, p.quantity, 1) > rmdp->value(p.intervals, p.quantity, 0), "turbulent start costs more");
    // Cancellation.
    {
        std::atomic<bool> stop {false};
        Progress pr;
        pr.cancel = &stop;
        pr.report = [&stop](int, const QString &) { stop = true; };
        const auto c = MdpPolicy::solve(p, 3000, nullptr, pr);
        check(c->cancelled, "MDP cancel honoured");
    }

    CAT = "execution/simulation";
    QVector<std::shared_ptr<Policy>> policies {std::make_shared<SchedulePolicy>(tw), std::make_shared<SchedulePolicy>(ac), mdp};
    const QVector<SimulationResult> res = simulate(p, policies, 4000, 11);
    check(res.size() == 3 && res[0].costsBps.size() == 4000, "one result per policy, one cost per path");
    // The Monte Carlo mean matches the analytic expected cost of each schedule.
    for (int i = 0; i < 2; ++i)
        checkNear(res[i].expectedCostBps, res[i].meanBps, 0.1 * res[i].stdBps / std::sqrt(4000.0) * 4 + 0.05 * res[i].expectedCostBps,
                  res[i].name + " mean cost ≈ analytic E");
    check(res[1].stdBps < res[0].stdBps, QString("AC std %1 < TWAP std %2").arg(res[1].stdBps).arg(res[0].stdBps));
    check(res[1].meanBps > res[0].meanBps, "AC mean cost > TWAP's");
    checkNear(res[1].meanBps, res[2].meanBps, 0.1 * res[1].meanBps, "MDP mean cost ≈ AC");
    checkNear(res[1].stdBps, res[2].stdBps, 0.1 * res[1].stdBps, "MDP std ≈ AC");
    checkNear(0, res[0].meanHoldings.last(), 1e-9, "everything sold by the end");
    // The environment interface: a path of a hand-written policy ends with nothing left.
    PathSimulator sim(p, nullptr, 3);
    while (!sim.done())
        sim.step(sim.state().remaining * 0.3);
    checkNear(0, sim.state().remaining, 1e-9, "simulator sells the rest on the last interval");
    check(sim.costBps() > 0, "a path has a positive shortfall with impact");
}


void testExecutionExtensions()
{
    CAT = "execution/volume";
    ExecutionParams p = execParams();
    p.adv = 5e6;
    // A U-shaped profile over 4 days of 5 intervals: VWAP follows it, and it changes the MDP's schedule.
    p.profile = Vec(p.intervals);
    for (int k = 0; k < p.intervals; ++k) {
        const int slot = k % 5;
        p.profile[k] = slot == 0 || slot == 4 ? 0.3 : 0.4 / 3;
    }
    double sum = 0;
    for (double v : p.profile) sum += v;
    for (double &v : p.profile) v /= sum;
    check(p.isValid(), "params with a profile valid");
    const Trajectory vw = vwap(p, p.profile), tw = twap(p);
    checkNear(p.quantity * p.profile[0], vw.trades[0], 1e-6 * p.quantity, "VWAP sells the profile's share");
    check(vw.expectedCost < tw.expectedCost, "VWAP into the volume profile costs less impact than TWAP");
    const auto mdp = MdpPolicy::solve(p, 300);
    const Trajectory mt = mdp->trajectory();
    check(mt.objective <= vw.objective + 1e-9 && mt.objective <= tw.objective + 1e-9, "MDP beats VWAP and TWAP on their own objective");
    // Flat profile and no noise: still Almgren-Chriss.
    ExecutionParams flat = execParams();
    flat.adv = 5e6;
    const Trajectory ac = almgrenChriss(flat);
    const auto fm = MdpPolicy::solve(flat, 400);
    double maxDiff = 0;
    for (int j = 0; j <= flat.intervals; ++j)
        maxDiff = std::max(maxDiff, std::fabs(fm->trajectory().holdings[j] - ac.holdings[j]));
    check(maxDiff < 0.01 * flat.quantity, "with ADV and a flat profile the MDP still lands on Almgren-Chriss");
    // Random volumes: the Monte Carlo mean stays near the analytic E (a little above: Jensen on 1 / V).
    flat.volumeNoise = 0.4;
    const auto res = simulate(flat, {std::make_shared<SchedulePolicy>(twap(flat))}, 4000, 5);
    check(res[0].meanBps > 0.9 * res[0].expectedCostBps && res[0].meanBps < 1.3 * res[0].expectedCostBps,
          QString("noisy volumes: mean %1 bp vs analytic %2 bp").arg(res[0].meanBps).arg(res[0].expectedCostBps));

    CAT = "execution/sqrt";
    ExecutionParams sq = execParams();
    sq.adv = 5e6;
    sq.exponent = 0.5;
    check(sq.isValid(), "square-root params valid");
    const Trajectory tws = twap(sq);
    // At the TWAP pace the linearized η reproduces the model's cost.
    ExecutionParams lin = sq;
    lin.exponent = 1;
    lin.temporary = sq.linearEta();
    checkNear(twap(lin).expectedCost, tws.expectedCost, 1e-6 * tws.expectedCost, "linearized η matches at the TWAP pace");
    // Concave impact: spreading out saves less than under linear impact, so the MDP front-loads more than AC.
    const Trajectory acs = almgrenChriss(sq);
    const auto ms = MdpPolicy::solve(sq, 300);
    const Trajectory mts = ms->trajectory();
    check(mts.objective <= acs.objective + 1e-9, "sqrt: MDP objective at most the linearized AC's");
    check(mts.trades.first() >= acs.trades.first() * 0.9, QString("sqrt: MDP first trade %1 vs AC %2").arg(mts.trades.first()).arg(acs.trades.first()));
    const auto rs = simulate(sq, {std::make_shared<SchedulePolicy>(tws), ms}, 2000, 9);
    check(rs[1].meanBps + rs[1].stdBps * 0.5 < rs[0].meanBps + rs[0].stdBps * 0.5 || rs[1].meanBps < rs[0].meanBps * 1.5,
          "sqrt: MDP comparable to TWAP in simulation");

    CAT = "execution/q-learning";
    const ExecutionParams q = execParams();
    const Trajectory acq = almgrenChriss(q), twq = twap(q);
    QLearningPolicy::Options o;
    o.episodes = 150000;
    QElapsedTimer t;
    t.start();
    const auto agent = QLearningPolicy::train(q, nullptr, o);
    const qint64 trainMs = t.elapsed();
    const Trajectory at = agent->trajectory();
    check(!agent->cancelled && agent->episodes == o.episodes, "agent trained");
    check(at.objective < twq.objective, QString("agent objective %1 below TWAP %2").arg(at.objective).arg(twq.objective));
    check(at.objective < 1.15 * acq.objective, QString("agent objective %1 within 15% of AC %2").arg(at.objective).arg(acq.objective));
    check(agent->learningCurve().size() >= 40 && agent->learningCurve().last() < agent->learningCurve().first(),
          "learning curve falls");
    std::cout << "timing: Q-learning " << o.episodes << " episodes × " << q.intervals << " intervals: " << trainMs << " ms\n";
    {
        std::atomic<bool> stop {false};
        Progress pr;
        pr.cancel = &stop;
        pr.report = [&stop](int, const QString &) { stop = true; };
        const auto c = QLearningPolicy::train(q, nullptr, o, pr);
        check(c->cancelled, "Q-learning cancel honoured");
    }
}

void timeExecution()
{
    CAT = "execution/timing";
    const ExecutionParams p = execParams();
    RegimeModel rm;
    rm.sigma = {0.6, 1.8};
    rm.transition = Mat::identity(2);
    rm.initial = {1, 0};
    QElapsedTimer t;
    t.start();
    const auto m = MdpPolicy::solve(p, 500, &rm);
    const qint64 solveMs = t.elapsed();
    t.restart();
    simulate(p, {m, std::make_shared<SchedulePolicy>(twap(p))}, 5000, 1, &rm);
    std::cout << "timing: MDP 20 intervals × 500 levels × 2 regimes: " << solveMs << " ms; Monte Carlo 5000 paths × 2 policies: "
              << t.elapsed() << " ms\n";
    check(m->levels() == 500, "timing solve ok");
}

} // namespace

int main(int argc, char **argv)
{
    const QString only = argc > 1 ? argv[1] : QString();
    if (only.isEmpty() || only == "hmm") {
        testHmmRecovery();
        testHmmStability();
        timeHmm();
    }
    if (only.isEmpty() || only == "garch") {
        testOptim();
        testGarchRecovery();
        testMsGarchRecovery();
        testMsGarchStability();
        timeMsGarch();
    }
    if (only.isEmpty() || only == "execution") {
        testExecution();
        testExecutionExtensions();
        timeExecution();
    }
    std::cout << checks << " checks, " << fails << " failures\n";
    return fails ? 1 : 0;
}
