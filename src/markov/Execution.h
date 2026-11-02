#pragma once

// Optimal execution of a sell order: how to split a quantity over a horizon when trading moves the
// price (impact) and waiting exposes the remainder to it (risk).
//
// The model is Almgren & Chriss (2000): N intervals of length τ; the price moves by σ √τ ξ each
// interval less a permanent impact γ per share traded; a trade of n shares executes at the interval's
// opening price less ε (half the spread) and a temporary impact η n / τ. The expected cost of a
// trajectory and its variance have closed forms, and minimising E + λ V has the analytic solution
// almgrenChriss(). The MDP (MdpPolicy::solve) minimises the same objective by dynamic programming on
// a grid of (intervals left, shares left, volatility regime): with one regime it must find the
// Almgren-Chriss trajectory up to the grid's resolution, which the tests check; with regimes it sells
// faster when the regime is turbulent and the regime chain is part of the state.
//
// Policies (TWAP, VWAP, Almgren-Chriss, the MDP) share the Policy interface and are compared by
// Monte Carlo on the same random paths (PathSimulator, also the environment a learning agent would
// train on: reset(), step(n), done()).

#include "Job.h"
#include "LinAlg.h"

#include <QString>
#include <QVector>

#include <memory>
#include <random>

namespace markov {

struct ExecutionParams {
    double quantity = 0;      // shares to sell
    double price = 0;         // S0, the decision price
    double horizon = 1;       // days
    int intervals = 20;       // N
    double sigma = 0;         // daily volatility of the price, in price units (price × daily vol)
    double lambda = 1e-6;     // risk aversion, 1 / currency
    double temporary = 0;     // η: price impact of trading at the market's own pace (participation 1), scaled
                              // by participation^exponent; with a flat volume that is η n / τ per share/day as in AC
    double permanent = 0;     // γ: lasting price impact per share traded
    double spread = 0;        // ε: half the spread, per share
    double adv = NaN;         // average daily volume, shares; NaN: no volume data, the impact goes by rate n / τ
    Vec profile;              // expected share of the horizon's volume in each interval (sums to 1); empty: flat
    double exponent = 1;      // δ: 1 linear impact, 0.5 square-root impact (in the participation n / V)
    double volumeNoise = 0;   // lognormal standard deviation of the simulated interval volumes (0: deterministic)

    double tau() const { return horizon / intervals; }
    double dailyVolume() const { return ok(adv) && adv > 0 ? adv : 1.0; }
    // Expected volume of interval k (shares; "shares per day × τ" when there is no volume data).
    double expectedVolume(int k) const;
    // The temporary price impact of selling n shares against a volume V: η (n / V)^δ scaled to the day's pace.
    double temporaryImpact(double n, double volume) const;
    // The linear η that costs the same as the model at the TWAP pace (η itself when the impact is linear).
    double linearEta() const;
    // η̃ = η_lin − ½ γ τ, the quadratic coefficient Almgren-Chriss keeps once the permanent impact's constant is set aside.
    double etaTilde() const { return linearEta() - 0.5 * permanent * tau(); }
    bool isValid() const;
};

// A deterministic liquidation schedule and its Almgren-Chriss figures.
struct Trajectory {
    QString name;
    Vec holdings;             // N + 1 values, quantity down to 0
    Vec trades;               // N values, what is sold in each interval
    double expectedCost = NaN;   // E, currency
    double variance = NaN;       // V, currency²
    double objective = NaN;      // E + λ V
};

Trajectory almgrenChriss(const ExecutionParams &p);
Trajectory twap(const ExecutionParams &p);
// Trades proportional to a volume profile (weights over the N intervals).
Trajectory vwap(const ExecutionParams &p, const Vec &profile);
// E, V and the objective of any schedule.
Trajectory evaluate(const ExecutionParams &p, const Vec &trades, const QString &name);
// Almgren-Chriss's κ: the urgency of the optimal schedule (per day).
double kappa(const ExecutionParams &p);

// Volatility regimes over the horizon: σ per regime (price units, daily), the transition matrix per
// interval, and where the chain starts.
struct RegimeModel {
    Vec sigma;
    Mat transition;
    Vec initial;
    int count() const { return sigma.size(); }
    bool isValid() const { return !sigma.isEmpty() && transition.rows == sigma.size() && initial.size() == sigma.size(); }
    // A daily transition matrix brought to an interval of τ days: P^τ for whole days, I + τ (P − I) within a day.
    static Mat perInterval(const Mat &daily, double tau);
};

// What a policy sees before each interval.
struct ExecState {
    int step = 0;             // 0-based interval
    int stepsLeft = 0;        // intervals including this one
    double remaining = 0;     // shares still to sell
    int regime = 0;
    double price = 0;         // the interval's opening price
};

class Policy
{
public:
    virtual ~Policy() = default;
    virtual QString name() const = 0;
    // Shares to sell in this interval (clamped to the remainder by the simulator; the last interval sells it all).
    virtual double trade(const ExecState &state) const = 0;
};

// A fixed schedule, scaled to what is left when the price path doesn't matter (TWAP, VWAP, Almgren-Chriss).
class SchedulePolicy : public Policy
{
public:
    explicit SchedulePolicy(const Trajectory &t) : m_t(t) {}
    QString name() const override { return m_t.name; }
    double trade(const ExecState &s) const override;
    const Trajectory &trajectory() const { return m_t; }

private:
    Trajectory m_t;
};

// Dynamic programming on (intervals left, shares left on a grid, regime).
class MdpPolicy : public Policy
{
public:
    // levels: the quantity grid has levels + 1 points from 0 to the quantity.
    static std::shared_ptr<MdpPolicy> solve(const ExecutionParams &p, int levels, const RegimeModel *regimes = nullptr,
                                            const Progress &progress = {});
    QString name() const override { return "MDP"; }
    double trade(const ExecState &s) const override;
    // The schedule the policy follows when the regime stays `regime` (its expected trajectory in that regime).
    Trajectory trajectory(int regime = 0) const;
    double value(int stepsLeft, double remaining, int regime) const;   // the objective still to pay
    bool cancelled = false;
    int levels() const { return m_levels; }

private:
    ExecutionParams m_p;
    RegimeModel m_regimes;
    int m_levels = 0;
    // value[k][r][i] and action[k][r][i] (shares to sell) for k intervals left, regime r, level i.
    QVector<QVector<Vec>> m_value, m_action;
    int levelOf(double remaining) const;
};

// One price path at a time: the environment of the Monte Carlo comparison and of any learning agent.
class PathSimulator
{
public:
    PathSimulator(const ExecutionParams &p, const RegimeModel *regimes, unsigned seed);
    void reset();
    const ExecState &state() const { return m_state; }
    bool done() const { return m_state.stepsLeft == 0; }
    // Sells n shares (clamped; everything on the last interval), moves the price, returns the interval's cost
    // against the decision price in currency.
    double step(double n);
    double cost() const { return m_cost; }                // so far, currency
    double costBps() const { return 1e4 * m_cost / (m_p.quantity * m_p.price); }
    const Vec &holdings() const { return m_holdings; }
    // The mean-variance objective's step penalty for the last step: λ σ² τ (shares left)², what a learning agent adds
    // to the realized cost to optimise the same thing as the MDP.
    double lastRiskPenalty() const { return m_lastRisk; }

private:
    ExecutionParams m_p;
    RegimeModel m_regimes;
    std::mt19937 m_rng;
    std::normal_distribution<double> m_normal {0, 1};
    ExecState m_state;
    double m_cost = 0, m_lastRisk = 0;
    Vec m_holdings;
};

// A tabular Q-learning agent trained on the simulator: state (intervals left, shares left on a coarse grid,
// regime), action a fraction of the remainder to sell, reward minus the interval's realized cost and risk
// penalty in basis points. Model-free: it never sees the impact formula, only what each path costs.
class QLearningPolicy : public Policy
{
public:
    struct Options {
        int episodes = 100000;
        int levels = 20;          // quantity grid
        int actions = 21;         // fractions 0, 0.05 … 1 of the remainder
        double learningRate = 0.2;         // falls to a tenth of it by the last episode
        double epsilonStart = 0.5, epsilonEnd = 0.02;
        unsigned seed = 3;
    };
    static std::shared_ptr<QLearningPolicy> train(const ExecutionParams &p, const RegimeModel *regimes, const Options &options,
                                                  const Progress &progress = {});
    QString name() const override { return "Q-learning"; }
    double trade(const ExecState &s) const override;
    // The greedy schedule when the regime stays `regime`.
    Trajectory trajectory(int regime = 0) const;
    const Vec &learningCurve() const { return m_curve; }   // mean cost in bp per block of episodes
    bool cancelled = false;
    int episodes = 0;

private:
    ExecutionParams m_p;
    int m_levels = 0, m_actions = 0, m_regimes = 1;
    QVector<double> m_q;      // [stepsLeft][regime][level][action]
    double &q(int k, int r, int level, int a) { return m_q[((k * m_regimes + r) * (m_levels + 1) + level) * m_actions + a]; }
    double q(int k, int r, int level, int a) const { return m_q[((k * m_regimes + r) * (m_levels + 1) + level) * m_actions + a]; }
    int levelOf(double remaining) const;
    int greedy(int k, int r, int level) const;
    Vec m_curve;
};

struct SimulationResult {
    QString name;
    Vec costsBps;             // one per path: implementation shortfall in basis points of quantity × price
    double meanBps = NaN, stdBps = NaN, p95Bps = NaN;
    Vec meanHoldings;         // N + 1, averaged over the paths
    double expectedCostBps = NaN;   // the analytic E of a schedule policy, NaN otherwise
};

// All policies on the same `paths` random paths.
QVector<SimulationResult> simulate(const ExecutionParams &p, const QVector<std::shared_ptr<Policy>> &policies, int paths,
                                   unsigned seed, const RegimeModel *regimes = nullptr, const Progress &progress = {});

} // namespace markov
