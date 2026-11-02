#include "LinAlg.h"

#include <algorithm>

namespace markov {

Mat Mat::identity(int n)
{
    Mat m(n, n);
    for (int i = 0; i < n; ++i)
        m(i, i) = 1;
    return m;
}

bool cholesky(const Mat &a, Mat *L)
{
    const int n = a.rows;
    if (n == 0 || a.cols != n)
        return false;
    Mat l(n, n);
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j <= i; ++j) {
            double s = a(i, j);
            for (int k = 0; k < j; ++k)
                s -= l(i, k) * l(j, k);
            if (i == j) {
                if (!(s > 0))
                    return false;
                l(i, i) = std::sqrt(s);
            } else {
                l(i, j) = s / l(j, j);
            }
        }
    }
    *L = l;
    return true;
}

double logDet(const Mat &L)
{
    double d = 0;
    for (int i = 0; i < L.rows; ++i)
        d += 2 * std::log(L(i, i));
    return d;
}

Vec solveCholesky(const Mat &L, const Vec &b)
{
    const int n = L.rows;
    Vec y(n), x(n);
    for (int i = 0; i < n; ++i) {   // L y = b
        double s = b[i];
        for (int k = 0; k < i; ++k)
            s -= L(i, k) * y[k];
        y[i] = s / L(i, i);
    }
    for (int i = n - 1; i >= 0; --i) {   // Lᵀ x = y
        double s = y[i];
        for (int k = i + 1; k < n; ++k)
            s -= L(k, i) * x[k];
        x[i] = s / L(i, i);
    }
    return x;
}

bool invert(const Mat &a, Mat *inv)
{
    const int n = a.rows;
    if (n == 0 || a.cols != n)
        return false;
    Mat m(n, 2 * n);
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j)
            m(i, j) = a(i, j);
        m(i, n + i) = 1;
    }
    for (int col = 0; col < n; ++col) {
        int pivot = col;
        for (int r = col + 1; r < n; ++r)
            if (std::fabs(m(r, col)) > std::fabs(m(pivot, col)))
                pivot = r;
        if (std::fabs(m(pivot, col)) < 1e-300)
            return false;
        if (pivot != col)
            for (int c = 0; c < 2 * n; ++c)
                std::swap(m(col, c), m(pivot, c));
        const double d = m(col, col);
        for (int c = 0; c < 2 * n; ++c)
            m(col, c) /= d;
        for (int r = 0; r < n; ++r) {
            if (r == col)
                continue;
            const double f = m(r, col);
            if (f == 0)
                continue;
            for (int c = 0; c < 2 * n; ++c)
                m(r, c) -= f * m(col, c);
        }
    }
    Mat out(n, n);
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            out(i, j) = m(i, n + j);
    *inv = out;
    return true;
}

double logSumExp(const Vec &v)
{
    double m = -INFINITY;
    for (double x : v)
        m = std::max(m, x);
    if (!std::isfinite(m))
        return m;
    double s = 0;
    for (double x : v)
        s += std::exp(x - m);
    return m + std::log(s);
}

double logSumExp(double a, double b)
{
    if (a == -INFINITY) return b;
    if (b == -INFINITY) return a;
    const double m = std::max(a, b);
    return m + std::log(std::exp(a - m) + std::exp(b - m));
}

double logNormal(const Vec &y, const Vec &mean, const Mat &L)
{
    const int d = y.size();
    Vec diff(d);
    for (int i = 0; i < d; ++i)
        diff[i] = y[i] - mean[i];
    // Mahalanobis distance through L: solve L z = diff, then |z|².
    double q = 0;
    Vec z(d);
    for (int i = 0; i < d; ++i) {
        double s = diff[i];
        for (int k = 0; k < i; ++k)
            s -= L(i, k) * z[k];
        z[i] = s / L(i, i);
        q += z[i] * z[i];
    }
    return -0.5 * (d * std::log(2 * M_PI) + logDet(L) + q);
}

} // namespace markov
