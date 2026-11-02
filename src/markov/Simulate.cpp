#include "Simulate.h"

namespace markov {

Mat simulateHmm(const HmmParams &p, int steps, std::mt19937 &rng, QVector<int> *states)
{
    Mat obs(steps, p.dims);
    QVector<Mat> L(p.states);
    for (int k = 0; k < p.states; ++k)
        cholesky(p.covariances[k], &L[k]);
    std::normal_distribution<double> normal(0, 1);
    std::discrete_distribution<int> first(p.initial.begin(), p.initial.end());
    QVector<std::discrete_distribution<int>> next;
    for (int k = 0; k < p.states; ++k) {
        const Vec row = p.transition.row(k);
        next.push_back(std::discrete_distribution<int>(row.begin(), row.end()));
    }
    int s = first(rng);
    if (states)
        states->resize(steps);
    for (int t = 0; t < steps; ++t) {
        if (t > 0)
            s = next[s](rng);
        if (states)
            (*states)[t] = s;
        Vec z(p.dims);
        for (double &v : z)
            v = normal(rng);
        for (int i = 0; i < p.dims; ++i) {
            double y = p.means[s][i];
            for (int j = 0; j <= i; ++j)
                y += L[s](i, j) * z[j];
            obs(t, i) = y;
        }
    }
    return obs;
}

Vec simulateMsGarch(const MsGarchParams &p, int steps, std::mt19937 &rng, QVector<int> *states)
{
    const int K = p.regimes;
    Vec r(steps);
    std::normal_distribution<double> normal(0, 1);
    std::student_t_distribution<double> student(p.innovations == Innovations::Student ? p.nu : 5.0);
    const Vec pi = p.stationary();
    std::discrete_distribution<int> first(pi.begin(), pi.end());
    QVector<std::discrete_distribution<int>> next;
    for (int k = 0; k < K; ++k) {
        const Vec row = p.transition.row(k);
        next.push_back(std::discrete_distribution<int>(row.begin(), row.end()));
    }
    // Every regime's variance starts at its long-run level (the sample mean of them when explosive).
    Vec sigma2(K);
    for (int k = 0; k < K; ++k) {
        const double u = p.garch[k].unconditionalVariance();
        sigma2[k] = ok(u) ? u : p.garch[k].omega * 10;
    }
    double eps2 = sigma2[0];
    int s = first(rng);
    if (states)
        states->resize(steps);
    for (int t = 0; t < steps; ++t) {
        if (t > 0) {
            s = next[s](rng);
            for (int k = 0; k < K; ++k)
                sigma2[k] = p.garch[k].omega + p.garch[k].alpha * eps2 + p.garch[k].beta * sigma2[k];
        }
        if (states)
            (*states)[t] = s;
        // A standardized innovation: unit variance in both cases.
        const double z = p.innovations == Innovations::Student ? student(rng) * std::sqrt((p.nu - 2) / p.nu) : normal(rng);
        const double eps = std::sqrt(sigma2[s]) * z;
        r[t] = p.mu + eps;
        eps2 = eps * eps;
    }
    return r;
}

} // namespace markov
