#pragma once

// Unconstrained minimisation for the maximum-likelihood fits: Nelder-Mead to get near the optimum
// from a rough start, then BFGS with numerical gradients to polish it. Constraints are handled by
// the callers through reparametrisation (logs, logits, softmax), so the objective sees R^n.

#include "Job.h"
#include "LinAlg.h"

#include <functional>

namespace markov {

using Objective = std::function<double(const Vec &x)>;

struct OptimResult {
    Vec x;
    double value = NaN;
    int iterations = 0;
    int evaluations = 0;
    bool converged = false;
    bool cancelled = false;
};

struct OptimOptions {
    int maxIterations = 2000;
    double tolerance = 1e-8;       // on the objective (Nelder-Mead: simplex spread; BFGS: step)
    double initialStep = 0.2;      // Nelder-Mead: first simplex size per coordinate
    // minimize(): Nelder-Mead only has to get near the optimum for BFGS to polish it, so it can stop
    // earlier. NaN / 0: the same as above.
    double nelderMeadTolerance = NaN;
    int nelderMeadMaxIterations = 0;
};

OptimResult nelderMead(const Objective &f, const Vec &start, const OptimOptions &options = {}, const Progress &progress = {});
OptimResult bfgs(const Objective &f, const Vec &start, const OptimOptions &options = {}, const Progress &progress = {});
// Both in turn.
OptimResult minimize(const Objective &f, const Vec &start, const OptimOptions &options = {}, const Progress &progress = {});

// Central differences (2n evaluations); forwardGradient: one-sided (n evaluations, less precise).
Vec numericGradient(const Objective &f, const Vec &x, double fx);
Vec forwardGradient(const Objective &f, const Vec &x, double fx);

} // namespace markov
