#pragma once

// Distributions of the innovations: normal and Student-t, densities, cumulative probabilities and
// quantiles, plus the partial expectations the expected shortfall needs.

namespace markov {

double normPdf(double x);
double normCdf(double x);
double normInv(double p);

// Student-t with nu degrees of freedom (not standardized: variance nu / (nu - 2)).
double studentPdf(double x, double nu);
double studentCdf(double x, double nu);
double studentInv(double p, double nu);

// E[X · 1{X < a}] for a standard normal / standard t: what the expected shortfall integrates.
double normPartial(double a);
double studentPartial(double a, double nu);

} // namespace markov
