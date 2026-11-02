#include "Optim.h"

#include <algorithm>

namespace markov {

namespace {

double safe(double v)
{
    return ok(v) ? v : INFINITY;
}

} // namespace

Vec numericGradient(const Objective &f, const Vec &x, double fx)
{
    const int n = x.size();
    Vec g(n);
    for (int i = 0; i < n; ++i) {
        const double h = 1e-5 * std::max(1.0, std::fabs(x[i]));
        Vec xp = x, xm = x;
        xp[i] += h;
        xm[i] -= h;
        const double fp = safe(f(xp)), fm = safe(f(xm));
        if (ok(fp) && ok(fm))
            g[i] = (fp - fm) / (2 * h);
        else if (ok(fp))
            g[i] = (fp - fx) / h;
        else if (ok(fm))
            g[i] = (fx - fm) / h;
        else
            g[i] = 0;
    }
    return g;
}

Vec forwardGradient(const Objective &f, const Vec &x, double fx)
{
    const int n = x.size();
    Vec g(n);
    for (int i = 0; i < n; ++i) {
        const double h = 1e-5 * std::max(1.0, std::fabs(x[i]));
        Vec xs = x;
        xs[i] += h;
        double fs = safe(f(xs));
        if (ok(fs)) {
            g[i] = (fs - fx) / h;
            continue;
        }
        xs[i] = x[i] - h;   // at a boundary of the domain: the other side
        fs = safe(f(xs));
        g[i] = ok(fs) ? (fx - fs) / h : 0;
    }
    return g;
}

OptimResult nelderMead(const Objective &f, const Vec &start, const OptimOptions &o, const Progress &progress)
{
    OptimResult r;
    const int n = start.size();
    QVector<Vec> simplex(n + 1, start);
    Vec values(n + 1);
    for (int i = 1; i <= n; ++i)
        simplex[i][i - 1] += o.initialStep;
    for (int i = 0; i <= n; ++i)
        values[i] = safe(f(simplex[i]));
    r.evaluations = n + 1;
    const double alpha = 1, gamma = 2, rho = 0.5, sigma = 0.5;
    QVector<int> order(n + 1);
    for (int it = 0; it < o.maxIterations; ++it) {
        if (progress.cancelled()) {
            r.cancelled = true;
            break;
        }
        for (int i = 0; i <= n; ++i)
            order[i] = i;
        std::sort(order.begin(), order.end(), [&](int a, int b) { return values[a] < values[b]; });
        const int best = order[0], worst = order[n], second = order[n - 1];
        r.iterations = it + 1;
        if (ok(values[best]) && ok(values[worst]) && std::fabs(values[worst] - values[best]) <= o.tolerance * std::max(1.0, std::fabs(values[best]))) {
            r.converged = true;
            break;
        }
        Vec centroid(n, 0.0);
        for (int i = 0; i <= n; ++i)
            if (i != worst)
                for (int j = 0; j < n; ++j)
                    centroid[j] += simplex[i][j] / n;
        auto along = [&](double t) {
            Vec x(n);
            for (int j = 0; j < n; ++j)
                x[j] = centroid[j] + t * (simplex[worst][j] - centroid[j]);
            return x;
        };
        const Vec xr = along(-alpha);
        const double fr = safe(f(xr));
        ++r.evaluations;
        if (fr < values[best]) {
            const Vec xe = along(-gamma);
            const double fe = safe(f(xe));
            ++r.evaluations;
            if (fe < fr) {
                simplex[worst] = xe;
                values[worst] = fe;
            } else {
                simplex[worst] = xr;
                values[worst] = fr;
            }
        } else if (fr < values[second]) {
            simplex[worst] = xr;
            values[worst] = fr;
        } else {
            const bool outside = fr < values[worst];
            const Vec xc = along(outside ? -rho : rho);
            const double fc = safe(f(xc));
            ++r.evaluations;
            if (fc < (outside ? fr : values[worst])) {
                simplex[worst] = xc;
                values[worst] = fc;
            } else {
                for (int i = 0; i <= n; ++i) {
                    if (i == best)
                        continue;
                    for (int j = 0; j < n; ++j)
                        simplex[i][j] = simplex[best][j] + sigma * (simplex[i][j] - simplex[best][j]);
                    values[i] = safe(f(simplex[i]));
                    ++r.evaluations;
                }
            }
        }
        if (it % 50 == 0)
            progress(0, QString("Nelder-Mead iteration %1").arg(it));
    }
    int best = 0;
    for (int i = 1; i <= n; ++i)
        if (values[i] < values[best])
            best = i;
    r.x = simplex[best];
    r.value = values[best];
    return r;
}

OptimResult bfgs(const Objective &f, const Vec &start, const OptimOptions &o, const Progress &progress)
{
    OptimResult r;
    const int n = start.size();
    Vec x = start;
    double fx = safe(f(x));
    r.evaluations = 1;
    if (!ok(fx)) {
        r.x = x;
        r.value = fx;
        return r;
    }
    // One-sided gradients (n evaluations) while the steps are large; central ones (2n, more precise) near
    // the optimum: before trusting a small gradient, after a failed line search or a negligible step.
    bool central = false;
    auto gradient = [&](const Vec &at, double fat) {
        r.evaluations += central ? 2 * n : n;
        return central ? numericGradient(f, at, fat) : forwardGradient(f, at, fat);
    };
    Vec g = gradient(x, fx);
    Mat H = Mat::identity(n);
    for (int it = 0; it < o.maxIterations; ++it) {
        if (progress.cancelled()) {
            r.cancelled = true;
            break;
        }
        r.iterations = it + 1;
        // Direction d = -H g.
        Vec d(n, 0.0);
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j)
                d[i] -= H(i, j) * g[j];
        double slope = 0;
        for (int i = 0; i < n; ++i)
            slope += d[i] * g[i];
        if (slope >= 0) {   // not a descent direction: restart from steepest descent
            H = Mat::identity(n);
            for (int i = 0; i < n; ++i)
                d[i] = -g[i];
            slope = 0;
            for (int i = 0; i < n; ++i)
                slope += d[i] * g[i];
        }
        double gnorm = 0;
        for (double v : g)
            gnorm = std::max(gnorm, std::fabs(v));
        if (gnorm < o.tolerance * 100) {
            if (!central) {
                central = true;
                g = gradient(x, fx);
                continue;
            }
            r.converged = true;
            break;
        }
        // Backtracking line search (Armijo).
        double step = 1;
        Vec xn(n);
        double fn = INFINITY;
        bool found = false;
        for (int ls = 0; ls < 40; ++ls) {
            for (int i = 0; i < n; ++i)
                xn[i] = x[i] + step * d[i];
            fn = safe(f(xn));
            ++r.evaluations;
            if (ok(fn) && fn <= fx + 1e-4 * step * slope) {
                found = true;
                break;
            }
            step *= 0.5;
        }
        if (!found) {
            if (!central) {
                central = true;
                g = gradient(x, fx);
                H = Mat::identity(n);
                continue;
            }
            r.converged = gnorm < 1e-3;
            break;
        }
        const Vec gn = gradient(xn, fn);
        Vec s(n), y(n);
        double sy = 0;
        for (int i = 0; i < n; ++i) {
            s[i] = xn[i] - x[i];
            y[i] = gn[i] - g[i];
            sy += s[i] * y[i];
        }
        const double improvement = fx - fn;
        x = xn;
        fx = fn;
        g = gn;
        if (sy > 1e-12) {
            // H ← (I - ρ s yᵀ) H (I - ρ y sᵀ) + ρ s sᵀ
            const double rho = 1 / sy;
            Vec Hy(n, 0.0);
            for (int i = 0; i < n; ++i)
                for (int j = 0; j < n; ++j)
                    Hy[i] += H(i, j) * y[j];
            double yHy = 0;
            for (int i = 0; i < n; ++i)
                yHy += y[i] * Hy[i];
            for (int i = 0; i < n; ++i)
                for (int j = 0; j < n; ++j)
                    H(i, j) += rho * ((1 + rho * yHy) * s[i] * s[j] - Hy[i] * s[j] - s[i] * Hy[j]);
        }
        if (improvement >= 0 && improvement <= o.tolerance * std::max(1.0, std::fabs(fx))) {
            if (!central) {
                central = true;
                g = gradient(x, fx);
                continue;
            }
            r.converged = true;
            break;
        }
        if (it % 10 == 0)
            progress(0, QString("BFGS iteration %1").arg(it));
    }
    r.x = x;
    r.value = fx;
    return r;
}

OptimResult minimize(const Objective &f, const Vec &start, const OptimOptions &o, const Progress &progress)
{
    OptimOptions near = o;
    if (ok(o.nelderMeadTolerance))
        near.tolerance = o.nelderMeadTolerance;
    if (o.nelderMeadMaxIterations > 0)
        near.maxIterations = o.nelderMeadMaxIterations;
    OptimResult a = nelderMead(f, start, near, progress);
    if (a.cancelled || !ok(a.value))
        return a;
    OptimResult b = bfgs(f, a.x, o, progress);
    b.iterations += a.iterations;
    b.evaluations += a.evaluations;
    if (!ok(b.value) || b.value > a.value) {
        b.x = a.x;
        b.value = a.value;
        b.converged = a.converged;
    }
    return b;
}

} // namespace markov
