#pragma once

// Small dense linear algebra for the Markov models: the matrices are tiny (a few states, a few
// variables), so plain loops beat any library here and keep the engine free of dependencies.

#include <QVector>

#include <cmath>
#include <limits>

namespace markov {

constexpr double NaN = std::numeric_limits<double>::quiet_NaN();
inline bool ok(double v) { return std::isfinite(v); }

using Vec = QVector<double>;

// Row-major dense matrix.
struct Mat {
    int rows = 0, cols = 0;
    QVector<double> a;

    Mat() = default;
    Mat(int r, int c, double fill = 0.0) : rows(r), cols(c), a(r * c, fill) {}
    double &operator()(int i, int j) { return a[i * cols + j]; }
    double operator()(int i, int j) const { return a[i * cols + j]; }
    Vec row(int i) const { return a.mid(i * cols, cols); }
    void setRow(int i, const Vec &v) { for (int j = 0; j < cols; ++j) a[i * cols + j] = v[j]; }
    bool isEmpty() const { return rows == 0 || cols == 0; }
    static Mat identity(int n);
};

// a = L Lᵀ for a symmetric positive definite a; false when it isn't (then nothing is written).
bool cholesky(const Mat &a, Mat *L);
// log |a| from its Cholesky factor.
double logDet(const Mat &L);
// Solves (L Lᵀ) x = b.
Vec solveCholesky(const Mat &L, const Vec &b);
// Gauss-Jordan inverse; false when singular.
bool invert(const Mat &a, Mat *inv);

double logSumExp(const Vec &v);
double logSumExp(double a, double b);

// log N(y; mean, Σ) with Σ given by its Cholesky factor.
double logNormal(const Vec &y, const Vec &mean, const Mat &L);

} // namespace markov
