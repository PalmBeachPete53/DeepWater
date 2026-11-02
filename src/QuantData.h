#pragma once

#include "MacroData.h"

#include <QDate>
#include <QDateTime>
#include <QString>
#include <QVector>

#include <cmath>
#include <limits>

// Data and statistics of the Quant page.
namespace quant {

constexpr double NaN = std::numeric_limits<double>::quiet_NaN();
inline bool ok(double v) { return std::isfinite(v); }

// --- Statistics (plain functions on vectors; NaN when there isn't enough data).

double mean(const QVector<double> &v);
double stdev(const QVector<double> &v);   // sample standard deviation

// Ordinary least squares of y on x with an intercept: y = alpha + beta x.
struct Ols {
    double alpha = NaN, beta = NaN;
    double r = NaN, r2 = NaN;           // Pearson correlation and its square
    double seAlpha = NaN, seBeta = NaN; // standard errors of the coefficients
    double seResid = NaN;               // standard error of the regression (std dev of the residuals)
    double t = NaN, p = NaN;            // t statistic of beta and its two-sided p-value
    double sdX = NaN, sdY = NaN;
    int n = 0;
};
Ols ols(const QVector<double> &x, const QVector<double> &y);

// Augmented Dickey-Fuller test of a series (constant, no trend, `lags` lagged differences):
// the t statistic of the unit-root coefficient and MacKinnon's (1994) approximate p-value.
struct Adf {
    double stat = NaN, p = NaN;
    int lags = 0;
};
Adf adf(const QVector<double> &s, int lags = 1);
// Half-life of mean reversion in observations, from Δs on s(t-1): -ln 2 / λ; NaN when λ >= 0.
double halfLife(const QVector<double> &s);

QVector<double> sma(const QVector<double> &s, int window);
QVector<double> rollingStd(const QVector<double> &s, int window);
// (s - sma) / rolling std over the window.
QVector<double> zscore(const QVector<double> &s, int window);
// Wilder's RSI of a series of closes.
QVector<double> rsi(const QVector<double> &close, int period = 14);
// Simple returns in %: 100 (p[i] / p[i-1] - 1), one fewer than prices.
QVector<double> returns(const QVector<double> &prices);

double normCdf(double x);
double studentCdf(double t, double df);   // P(T <= t)

// --- Daily prices of a symbol, aligned on common dates.

struct Prices {
    QVector<QDate> dates;
    QVector<double> close;
};
// The dates both have, and the closes on those dates.
void align(Prices *a, Prices *b);
// Weekly closes (the last close of each week).
Prices weekly(const Prices &daily);

} // namespace quant

// The CFTC's legacy Commitments of Traders report, futures only (publicreporting.cftc.gov, dataset 6dca-aqww,
// no key, weekly, Tuesday positions published Friday): non-commercial, commercial, total reportable and
// non-reportable positions of a few index, crypto and metal contracts, over about two years. Refreshed every
// 12 hours, cached on disk.
class CotReport : public RemoteDataSet
{
    Q_OBJECT
public:
    struct Market {
        QString id;      // "sp500"
        QString label;   // "S&P 500"
        QString code;    // CFTC contract market code, "13874+"
    };
    struct Week {
        QDate date;
        qint64 openInterest = 0;
        qint64 ncLong = 0, ncShort = 0, ncSpread = 0;   // non-commercial
        qint64 cLong = 0, cShort = 0;                   // commercial
        qint64 trLong = 0, trShort = 0;                 // total reportable
        qint64 nrLong = 0, nrShort = 0;                 // non-reportable
        int traders = 0;
        int tNcLong = 0, tNcShort = 0, tNcSpread = 0, tCLong = 0, tCShort = 0, tTrLong = 0, tTrShort = 0;
        qint64 ncNet() const { return ncLong - ncShort; }
        qint64 cNet() const { return cLong - cShort; }
        qint64 nrNet() const { return nrLong - nrShort; }
    };
    struct Series {
        QString name;    // "S&P 500 Consolidated - CHICAGO MERCANTILE EXCHANGE"
        QString units;   // "(S&P 500 INDEX X $50.00)"
        QVector<Week> weeks;   // oldest first
    };

    explicit CotReport(QObject *parent = nullptr);

    const QVector<Market> &markets() const { return m_markets; }
    const Series &series(const QString &id) const;

protected:
    QUrl url() const override;
    bool parse(const QByteArray &body) override;

private:
    QVector<Market> m_markets;
    QHash<QString, Series> m_series;   // by market id
};
