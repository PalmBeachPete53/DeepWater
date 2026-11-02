#include "Dist.h"

#include <algorithm>
#include <cmath>

namespace markov {

namespace {

// Continued fraction of the incomplete beta function (Numerical Recipes, betacf).
double betacf(double a, double b, double x)
{
    const int maxIt = 300;
    const double eps = 3e-14, fpmin = 1e-300;
    const double qab = a + b, qap = a + 1, qam = a - 1;
    double c = 1, d = 1 - qab * x / qap;
    if (std::fabs(d) < fpmin) d = fpmin;
    d = 1 / d;
    double h = d;
    for (int m = 1; m <= maxIt; ++m) {
        const int m2 = 2 * m;
        double aa = m * (b - m) * x / ((qam + m2) * (a + m2));
        d = 1 + aa * d;
        if (std::fabs(d) < fpmin) d = fpmin;
        c = 1 + aa / c;
        if (std::fabs(c) < fpmin) c = fpmin;
        d = 1 / d;
        h *= d * c;
        aa = -(a + m) * (qab + m) * x / ((a + m2) * (qap + m2));
        d = 1 + aa * d;
        if (std::fabs(d) < fpmin) d = fpmin;
        c = 1 + aa / c;
        if (std::fabs(c) < fpmin) c = fpmin;
        d = 1 / d;
        const double del = d * c;
        h *= del;
        if (std::fabs(del - 1) < eps)
            break;
    }
    return h;
}

double betai(double a, double b, double x)
{
    if (x <= 0) return 0;
    if (x >= 1) return 1;
    const double bt = std::exp(std::lgamma(a + b) - std::lgamma(a) - std::lgamma(b) + a * std::log(x) + b * std::log(1 - x));
    if (x < (a + 1) / (a + b + 2))
        return bt * betacf(a, b, x) / a;
    return 1 - bt * betacf(b, a, 1 - x) / b;
}

// Quantile by bisection on a monotone CDF.
template <typename F>
double invert(F cdf, double p, double lo, double hi)
{
    for (int i = 0; i < 200 && hi - lo > 1e-12 * std::max(1.0, std::fabs(lo) + std::fabs(hi)); ++i) {
        const double mid = 0.5 * (lo + hi);
        if (cdf(mid) < p)
            lo = mid;
        else
            hi = mid;
    }
    return 0.5 * (lo + hi);
}

} // namespace

double normPdf(double x)
{
    return std::exp(-0.5 * x * x) / std::sqrt(2 * M_PI);
}

double normCdf(double x)
{
    return 0.5 * std::erfc(-x / std::sqrt(2.0));
}

double normInv(double p)
{
    if (p <= 0) return -INFINITY;
    if (p >= 1) return INFINITY;
    return invert(normCdf, p, -40, 40);
}

double studentPdf(double x, double nu)
{
    return std::exp(std::lgamma((nu + 1) / 2) - std::lgamma(nu / 2)) / std::sqrt(nu * M_PI) * std::pow(1 + x * x / nu, -(nu + 1) / 2);
}

double studentCdf(double x, double nu)
{
    const double t = nu / (nu + x * x);
    const double tail = 0.5 * betai(nu / 2, 0.5, t);
    return x >= 0 ? 1 - tail : tail;
}

double studentInv(double p, double nu)
{
    if (p <= 0) return -INFINITY;
    if (p >= 1) return INFINITY;
    return invert([nu](double x) { return studentCdf(x, nu); }, p, -1e6, 1e6);
}

double normPartial(double a)
{
    return -normPdf(a);
}

double studentPartial(double a, double nu)
{
    // ∫_{-∞}^{a} x f(x) dx = -f(a) (nu + a²) / (nu - 1) for nu > 1.
    return -studentPdf(a, nu) * (nu + a * a) / (nu - 1);
}

} // namespace markov
