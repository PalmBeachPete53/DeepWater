#pragma once

// Synthetic samples from known models, for the parameter-recovery tests and the execution simulator.

#include "Hmm.h"
#include "MsGarch.h"

#include <random>

namespace markov {

// A path of states and observations from an HMM; `states` gets the hidden path if given.
Mat simulateHmm(const HmmParams &params, int steps, std::mt19937 &rng, QVector<int> *states = nullptr);

// Returns from a Markov-switching GARCH (one regime: a plain GARCH); `states` gets the regime path.
Vec simulateMsGarch(const MsGarchParams &params, int steps, std::mt19937 &rng, QVector<int> *states = nullptr);

} // namespace markov
