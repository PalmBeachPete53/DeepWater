#include "Execution.h"

#include <algorithm>

namespace markov {

// ---------------------------------------------------------------- params

double ExecutionParams::expectedVolume(int k) const
{
    const double total = dailyVolume() * horizon;
    if (profile.size() == intervals && k >= 0 && k < intervals)
        return std::max(1e-9 * total, total * profile[k]);
    return total / intervals;
}

double ExecutionParams::temporaryImpact(double n, double volume) const
{
    if (n <= 0 || volume <= 0)
        return 0;
    return temporary * dailyVolume() * std::pow(n / volume, exponent);
}

double ExecutionParams::linearEta() const
{
    if (exponent == 1)
        return temporary;
    // Same cost as the model when selling at the TWAP pace into the flat expected volume.
    const double n = quantity / intervals, v = dailyVolume() * tau();
    return n > 0 ? tau() * temporaryImpact(n, v) / n : temporary;
}

bool ExecutionParams::isValid() const
{
    if (!(quantity > 0 && price > 0 && horizon > 0 && intervals > 0 && sigma >= 0 && temporary >= 0 && exponent > 0))
        return false;
    if (exponent == 1) {
        // The step cost must stay convex in every interval: η adv / V̄_k > ½ γ.
        for (int k = 0; k < intervals; ++k)
            if (temporary * dailyVolume() / expectedVolume(k) - 0.5 * permanent <= 0)
                return false;
    }
    return etaTilde() > 0;
}

// ---------------------------------------------------------------- schedules

Trajectory evaluate(const ExecutionParams &p, const Vec &trades, const QString &name)
{
    Trajectory t;
    t.name = name;
    const int N = trades.size();
    t.trades = trades;
    t.holdings = Vec(N + 1);
    t.holdings[0] = p.quantity;
    for (int k = 0; k < N; ++k)
        t.holdings[k + 1] = t.holdings[k] - trades[k];
    const double tau = p.tau();
    // E = ½ γ X² − ½ γ Σ n² + ε X + Σ n h(n, V̄_k); V = σ² τ Σ x_k² over the holdings after each interval but the last.
    double impact = 0, sumN2 = 0, sumX2 = 0;
    for (int k = 0; k < N; ++k) {
        impact += trades[k] * p.temporaryImpact(trades[k], p.expectedVolume(k));
        sumN2 += trades[k] * trades[k];
        if (k + 1 < N)
            sumX2 += t.holdings[k + 1] * t.holdings[k + 1];
    }
    t.expectedCost = 0.5 * p.permanent * (p.quantity * p.quantity - sumN2) + p.spread * p.quantity + impact;
    t.variance = p.sigma * p.sigma * tau * sumX2;
    t.objective = t.expectedCost + p.lambda * t.variance;
    return t;
}

double kappa(const ExecutionParams &p)
{
    const double tau = p.tau();
    const double k2 = p.lambda * p.sigma * p.sigma / p.etaTilde();   // κ̃²
    return std::acosh(0.5 * k2 * tau * tau + 1) / tau;
}

Trajectory almgrenChriss(const ExecutionParams &p)
{
    const int N = p.intervals;
    const double tau = p.tau(), T = p.horizon;
    Vec trades(N);
    const double k = kappa(p);
    if (!(k > 1e-12)) {
        for (double &n : trades)
            n = p.quantity / N;
    } else {
        // x_j = X sinh(κ (T − t_j)) / sinh(κ T); n_j = x_{j−1} − x_j.
        auto x = [&](int j) { return p.quantity * std::sinh(k * (T - j * tau)) / std::sinh(k * T); };
        for (int j = 0; j < N; ++j)
            trades[j] = x(j) - x(j + 1);
    }
    return evaluate(p, trades, "Almgren-Chriss");
}

Trajectory twap(const ExecutionParams &p)
{
    return evaluate(p, Vec(p.intervals, p.quantity / p.intervals), "TWAP");
}

Trajectory vwap(const ExecutionParams &p, const Vec &profile)
{
    const int N = p.intervals;
    Vec trades(N, p.quantity / N);
    double sum = 0;
    for (int i = 0; i < std::min(N, int(profile.size())); ++i)
        sum += std::max(0.0, profile[i]);
    if (profile.size() >= N && sum > 0)
        for (int i = 0; i < N; ++i)
            trades[i] = p.quantity * std::max(0.0, profile[i]) / sum;
    return evaluate(p, trades, "VWAP");
}

double SchedulePolicy::trade(const ExecState &s) const
{
    // The planned share of what is left: keeps the shape if an earlier interval was clamped.
    double planned = 0;
    for (int k = s.step; k < m_t.trades.size(); ++k)
        planned += m_t.trades[k];
    if (planned <= 0 || s.step >= m_t.trades.size())
        return s.remaining / std::max(1, s.stepsLeft);
    return s.remaining * m_t.trades[s.step] / planned;
}

// ---------------------------------------------------------------- regimes

Mat RegimeModel::perInterval(const Mat &daily, double tau)
{
    const int K = daily.rows;
    if (tau >= 1) {
        const int n = std::max(1, int(std::lround(tau)));
        Mat r = Mat::identity(K);
        for (int i = 0; i < n; ++i) {
            Mat next(K, K);
            for (int a = 0; a < K; ++a)
                for (int b = 0; b < K; ++b)
                    for (int c = 0; c < K; ++c)
                        next(a, b) += r(a, c) * daily(c, b);
            r = next;
        }
        return r;
    }
    Mat r(K, K);
    for (int a = 0; a < K; ++a)
        for (int b = 0; b < K; ++b)
            r(a, b) = std::max(0.0, (a == b ? 1.0 : 0.0) + tau * (daily(a, b) - (a == b ? 1.0 : 0.0)));
    return r;
}

// ---------------------------------------------------------------- MDP

std::shared_ptr<MdpPolicy> MdpPolicy::solve(const ExecutionParams &p, int levels, const RegimeModel *regimes, const Progress &progress)
{
    auto m = std::make_shared<MdpPolicy>();
    m->m_p = p;
    m->m_levels = std::max(2, levels);
    if (regimes && regimes->isValid()) {
        m->m_regimes = *regimes;
    } else {
        m->m_regimes.sigma = {p.sigma};
        m->m_regimes.transition = Mat(1, 1, 1.0);
        m->m_regimes.initial = {1.0};
    }
    const int N = p.intervals, R = m->m_regimes.count(), L = m->m_levels;
    const double tau = p.tau(), dq = p.quantity / L;
    m->m_value = QVector<QVector<Vec>>(N + 1, QVector<Vec>(R, Vec(L + 1, 0.0)));
    m->m_action = m->m_value;
    // Nothing left to pay with no intervals left and nothing left to sell; impossible otherwise.
    for (int r = 0; r < R; ++r)
        for (int i = 1; i <= L; ++i)
            m->m_value[0][r][i] = INFINITY;
    for (int k = 1; k <= N; ++k) {
        if (progress.cancelled()) {
            m->cancelled = true;
            return m;
        }
        for (int r = 0; r < R; ++r) {
            const double sig2 = m->m_regimes.sigma[r] * m->m_regimes.sigma[r];
            for (int i = 0; i <= L; ++i) {
                double best = INFINITY, bestN = 0;
                const double q = i * dq;
                // Sell j levels now (all of them on the last interval).
                const int jMin = k == 1 ? i : 0;
                for (int j = jMin; j <= i; ++j) {
                    const double n = j * dq, left = q - n;
                    double future = 0;
                    for (int s = 0; s < R; ++s)
                        future += m->m_regimes.transition(r, s) * m->m_value[k - 1][s][i - j];
                    if (!std::isfinite(future))
                        continue;
                    const double cost = p.spread * n - 0.5 * p.permanent * n * n + n * p.temporaryImpact(n, p.expectedVolume(N - k))
                                        + p.lambda * sig2 * tau * left * left + future;
                    if (cost < best) {
                        best = cost;
                        bestN = n;
                    }
                }
                m->m_value[k][r][i] = best;
                m->m_action[k][r][i] = bestN;
            }
        }
        progress(100 * k / N, QString("MDP: %1 / %2 intervals").arg(k).arg(N));
    }
    // The constant part of the permanent impact, shared by every schedule, goes in the value for comparability.
    const double constant = 0.5 * p.permanent * p.quantity * p.quantity;
    Q_UNUSED(tau);
    for (auto &byRegime : m->m_value)
        for (Vec &v : byRegime)
            for (double &x : v)
                if (std::isfinite(x))
                    x += constant;
    return m;
}

int MdpPolicy::levelOf(double remaining) const
{
    return std::clamp(int(std::lround(remaining / (m_p.quantity / m_levels))), 0, m_levels);
}

double MdpPolicy::trade(const ExecState &s) const
{
    const int k = std::clamp(s.stepsLeft, 1, m_p.intervals), r = std::clamp(s.regime, 0, m_regimes.count() - 1);
    if (k == 1)
        return s.remaining;
    const double dq = m_p.quantity / m_levels;
    const int i = levelOf(s.remaining);
    // Off the grid, keep the same share of the remainder as the grid point would sell.
    const double n = m_action[k][r][i];
    return i > 0 ? s.remaining * n / (i * dq) : 0;
}

double MdpPolicy::value(int stepsLeft, double remaining, int regime) const
{
    const int k = std::clamp(stepsLeft, 0, m_p.intervals), r = std::clamp(regime, 0, m_regimes.count() - 1);
    return m_value[k][r][levelOf(remaining)];
}

Trajectory MdpPolicy::trajectory(int regime) const
{
    const int N = m_p.intervals;
    Vec trades(N);
    ExecState s;
    s.remaining = m_p.quantity;
    s.regime = regime;
    s.price = m_p.price;
    for (int k = 0; k < N; ++k) {
        s.step = k;
        s.stepsLeft = N - k;
        trades[k] = std::min(s.remaining, trade(s));
        s.remaining -= trades[k];
    }
    Trajectory t = evaluate(m_p, trades, name());
    return t;
}

// ---------------------------------------------------------------- simulation

PathSimulator::PathSimulator(const ExecutionParams &p, const RegimeModel *regimes, unsigned seed)
    : m_p(p)
    , m_rng(seed)
{
    if (regimes && regimes->isValid()) {
        m_regimes = *regimes;
    } else {
        m_regimes.sigma = {p.sigma};
        m_regimes.transition = Mat(1, 1, 1.0);
        m_regimes.initial = {1.0};
    }
    reset();
}

void PathSimulator::reset()
{
    m_state = ExecState();
    m_state.stepsLeft = m_p.intervals;
    m_state.remaining = m_p.quantity;
    m_state.price = m_p.price;
    std::discrete_distribution<int> first(m_regimes.initial.begin(), m_regimes.initial.end());
    m_state.regime = first(m_rng);
    m_cost = 0;
    m_holdings = Vec(m_p.intervals + 1, 0.0);
    m_holdings[0] = m_p.quantity;
}

double PathSimulator::step(double n)
{
    if (done())
        return 0;
    const double tau = m_p.tau();
    n = std::clamp(n, 0.0, m_state.remaining);
    if (m_state.stepsLeft == 1)
        n = m_state.remaining;
    // The interval's volume around its expectation, then execution at the opening price less the spread and the
    // temporary impact of the participation; the shortfall against S0.
    double volume = m_p.expectedVolume(m_state.step);
    if (m_p.volumeNoise > 0)
        volume *= std::exp(m_p.volumeNoise * m_normal(m_rng) - 0.5 * m_p.volumeNoise * m_p.volumeNoise);
    const double executed = m_state.price - m_p.spread - m_p.temporaryImpact(n, volume);
    const double cost = n * (m_p.price - executed);
    m_cost += cost;
    m_state.remaining -= n;
    {
        const double sigma = m_regimes.sigma[m_state.regime];
        m_lastRisk = m_p.lambda * sigma * sigma * tau * m_state.remaining * m_state.remaining;
    }
    // Then the price moves: diffusion in the current regime, less the permanent impact of what was sold.
    const double sigma = m_regimes.sigma[m_state.regime];
    m_state.price += sigma * std::sqrt(tau) * m_normal(m_rng) - m_p.permanent * n;
    const Vec row = m_regimes.transition.row(m_state.regime);
    std::discrete_distribution<int> next(row.begin(), row.end());
    m_state.regime = next(m_rng);
    ++m_state.step;
    --m_state.stepsLeft;
    m_holdings[m_state.step] = m_state.remaining;
    return cost;
}

// ---------------------------------------------------------------- Q-learning

std::shared_ptr<QLearningPolicy> QLearningPolicy::train(const ExecutionParams &p, const RegimeModel *regimes, const Options &o,
                                                        const Progress &progress)
{
    auto a = std::make_shared<QLearningPolicy>();
    a->m_p = p;
    a->m_levels = std::max(2, o.levels);
    a->m_actions = std::max(2, o.actions);
    a->m_regimes = regimes && regimes->isValid() ? regimes->count() : 1;
    const int N = p.intervals;
    a->m_q = QVector<double>((N + 1) * a->m_regimes * (a->m_levels + 1) * a->m_actions, 0.0);
    PathSimulator sim(p, regimes, o.seed);
    std::mt19937 rng(o.seed + 1);
    std::uniform_real_distribution<double> u(0, 1);
    std::uniform_int_distribution<int> pick(0, a->m_actions - 1);
    const double scale = 1e4 / (p.quantity * p.price);   // currency to basis points
    const int block = std::max(1, o.episodes / 50);
    double blockCost = 0;
    for (int e = 0; e < o.episodes; ++e) {
        if (progress.cancelled()) {
            a->cancelled = true;
            break;
        }
        const double frac = double(e) / std::max(1, o.episodes - 1);
        const double eps = o.epsilonStart + (o.epsilonEnd - o.epsilonStart) * frac;
        const double rate = o.learningRate * (1 - 0.9 * frac);
        sim.reset();
        while (!sim.done()) {
            const ExecState &s = sim.state();
            const int k = s.stepsLeft, r = std::min(s.regime, a->m_regimes - 1), level = a->levelOf(s.remaining);
            const int action = k == 1 ? a->m_actions - 1 : (u(rng) < eps ? pick(rng) : a->greedy(k, r, level));
            const double n = s.remaining * action / (a->m_actions - 1);
            const double cost = sim.step(n);
            const double reward = -(cost + sim.lastRiskPenalty()) * scale;
            double next = 0;
            if (!sim.done()) {
                const ExecState &s2 = sim.state();
                const int k2 = s2.stepsLeft, r2 = std::min(s2.regime, a->m_regimes - 1), l2 = a->levelOf(s2.remaining);
                next = a->q(k2, r2, l2, a->greedy(k2, r2, l2));
            }
            double &qv = a->q(k, r, level, action);
            qv += rate * (reward + next - qv);
        }
        blockCost += sim.costBps();
        if ((e + 1) % block == 0) {
            a->m_curve.push_back(blockCost / block);
            blockCost = 0;
            progress(100 * (e + 1) / o.episodes, QString("Q-learning: episode %1 / %2").arg(e + 1).arg(o.episodes));
        }
        a->episodes = e + 1;
    }
    return a;
}

int QLearningPolicy::levelOf(double remaining) const
{
    return std::clamp(int(std::lround(remaining / m_p.quantity * m_levels)), 0, m_levels);
}

int QLearningPolicy::greedy(int k, int r, int level) const
{
    int best = 0;
    for (int a = 1; a < m_actions; ++a)
        if (q(k, r, level, a) > q(k, r, level, best))
            best = a;
    return best;
}

double QLearningPolicy::trade(const ExecState &s) const
{
    if (s.stepsLeft <= 1)
        return s.remaining;
    const int k = std::clamp(s.stepsLeft, 1, m_p.intervals), r = std::clamp(s.regime, 0, m_regimes - 1);
    return s.remaining * greedy(k, r, levelOf(s.remaining)) / (m_actions - 1);
}

Trajectory QLearningPolicy::trajectory(int regime) const
{
    const int N = m_p.intervals;
    Vec trades(N);
    ExecState s;
    s.remaining = m_p.quantity;
    s.regime = regime;
    for (int k = 0; k < N; ++k) {
        s.step = k;
        s.stepsLeft = N - k;
        trades[k] = std::min(s.remaining, trade(s));
        s.remaining -= trades[k];
    }
    return evaluate(m_p, trades, name());
}

QVector<SimulationResult> simulate(const ExecutionParams &p, const QVector<std::shared_ptr<Policy>> &policies, int paths,
                                   unsigned seed, const RegimeModel *regimes, const Progress &progress)
{
    QVector<SimulationResult> out(policies.size());
    const int N = p.intervals;
    for (int i = 0; i < policies.size(); ++i) {
        out[i].name = policies[i]->name();
        out[i].costsBps.reserve(paths);
        out[i].meanHoldings = Vec(N + 1, 0.0);
        if (auto *sp = dynamic_cast<const SchedulePolicy *>(policies[i].get()))
            out[i].expectedCostBps = 1e4 * sp->trajectory().expectedCost / (p.quantity * p.price);
    }
    for (int path = 0; path < paths; ++path) {
        if (progress.cancelled())
            break;
        // The same random numbers for every policy: the comparison is paired.
        for (int i = 0; i < policies.size(); ++i) {
            PathSimulator sim(p, regimes, seed + 7919u * path);
            while (!sim.done())
                sim.step(policies[i]->trade(sim.state()));
            out[i].costsBps.push_back(sim.costBps());
            for (int k = 0; k <= N; ++k)
                out[i].meanHoldings[k] += sim.holdings()[k] / paths;
        }
        if (path % 100 == 0)
            progress(100 * path / std::max(1, paths), QString("Monte Carlo: %1 / %2 paths").arg(path).arg(paths));
    }
    for (SimulationResult &r : out) {
        const int n = r.costsBps.size();
        if (n == 0)
            continue;
        double m = 0;
        for (double v : r.costsBps)
            m += v / n;
        double s = 0;
        for (double v : r.costsBps)
            s += (v - m) * (v - m);
        r.meanBps = m;
        r.stdBps = n > 1 ? std::sqrt(s / (n - 1)) : NaN;
        Vec sorted = r.costsBps;
        std::sort(sorted.begin(), sorted.end());
        r.p95Bps = sorted[std::min(n - 1, int(0.95 * n))];
    }
    return out;
}

} // namespace markov
