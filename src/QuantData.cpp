#include "QuantData.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

#include <algorithm>
#include <array>

namespace quant {

double mean(const QVector<double> &v)
{
    if (v.isEmpty())
        return NaN;
    double s = 0;
    for (double x : v)
        s += x;
    return s / v.size();
}

double stdev(const QVector<double> &v)
{
    if (v.size() < 2)
        return NaN;
    const double m = mean(v);
    double s = 0;
    for (double x : v)
        s += (x - m) * (x - m);
    return std::sqrt(s / (v.size() - 1));
}

double normCdf(double x)
{
    return 0.5 * std::erfc(-x / std::sqrt(2.0));
}

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

// Regularized incomplete beta I_x(a, b).
double betai(double a, double b, double x)
{
    if (x <= 0) return 0;
    if (x >= 1) return 1;
    const double bt = std::exp(std::lgamma(a + b) - std::lgamma(a) - std::lgamma(b) + a * std::log(x) + b * std::log(1 - x));
    if (x < (a + 1) / (a + b + 2))
        return bt * betacf(a, b, x) / a;
    return 1 - bt * betacf(b, a, 1 - x) / b;
}

// Least squares with several regressors (columns of X, the first one all ones): the coefficients and
// their standard errors. Normal equations solved by Gauss-Jordan; p is small (at most 5 here).
bool solve(const QVector<QVector<double>> &X, const QVector<double> &y, QVector<double> *coef, QVector<double> *se,
           double *seResid = nullptr)
{
    const int n = y.size(), p = X.size();
    if (p == 0 || n <= p)
        return false;
    QVector<QVector<double>> a(p, QVector<double>(2 * p, 0.0));   // [X'X | I]
    QVector<double> xty(p, 0.0);
    for (int i = 0; i < p; ++i) {
        for (int j = 0; j < p; ++j) {
            double s = 0;
            for (int k = 0; k < n; ++k)
                s += X[i][k] * X[j][k];
            a[i][j] = s;
        }
        a[i][p + i] = 1;
        double s = 0;
        for (int k = 0; k < n; ++k)
            s += X[i][k] * y[k];
        xty[i] = s;
    }
    for (int col = 0; col < p; ++col) {
        int pivot = col;
        for (int r = col + 1; r < p; ++r)
            if (std::fabs(a[r][col]) > std::fabs(a[pivot][col]))
                pivot = r;
        if (std::fabs(a[pivot][col]) < 1e-12)
            return false;
        std::swap(a[col], a[pivot]);
        const double d = a[col][col];
        for (double &v : a[col])
            v /= d;
        for (int r = 0; r < p; ++r) {
            if (r == col)
                continue;
            const double f = a[r][col];
            if (f == 0)
                continue;
            for (int c = 0; c < 2 * p; ++c)
                a[r][c] -= f * a[col][c];
        }
    }
    coef->resize(p);
    for (int i = 0; i < p; ++i) {
        double s = 0;
        for (int j = 0; j < p; ++j)
            s += a[i][p + j] * xty[j];
        (*coef)[i] = s;
    }
    double rss = 0;
    for (int k = 0; k < n; ++k) {
        double fit = 0;
        for (int i = 0; i < p; ++i)
            fit += (*coef)[i] * X[i][k];
        rss += (y[k] - fit) * (y[k] - fit);
    }
    const double sigma2 = rss / (n - p);
    se->resize(p);
    for (int i = 0; i < p; ++i)
        (*se)[i] = std::sqrt(std::max(0.0, sigma2 * a[i][p + i]));
    if (seResid)
        *seResid = std::sqrt(sigma2);
    return true;
}

} // namespace

double studentCdf(double t, double df)
{
    if (!ok(t) || df <= 0)
        return NaN;
    const double x = df / (df + t * t);
    const double tail = 0.5 * betai(df / 2, 0.5, x);
    return t >= 0 ? 1 - tail : tail;
}

Ols ols(const QVector<double> &x, const QVector<double> &y)
{
    Ols o;
    const int n = std::min(x.size(), y.size());
    QVector<double> xs, ys;
    for (int i = 0; i < n; ++i)
        if (ok(x[i]) && ok(y[i])) {
            xs.push_back(x[i]);
            ys.push_back(y[i]);
        }
    o.n = xs.size();
    if (o.n < 3)
        return o;
    QVector<double> coef, se;
    if (!solve({QVector<double>(o.n, 1.0), xs}, ys, &coef, &se, &o.seResid))
        return o;
    o.alpha = coef[0];
    o.beta = coef[1];
    o.seAlpha = se[0];
    o.seBeta = se[1];
    o.sdX = stdev(xs);
    o.sdY = stdev(ys);
    o.r = o.sdX > 0 && o.sdY > 0 ? o.beta * o.sdX / o.sdY : NaN;
    o.r2 = ok(o.r) ? o.r * o.r : NaN;
    o.t = o.seBeta > 0 ? o.beta / o.seBeta : NaN;
    o.p = ok(o.t) ? 2 * (1 - studentCdf(std::fabs(o.t), o.n - 2)) : NaN;
    return o;
}

Adf adf(const QVector<double> &s, int lags)
{
    Adf out;
    out.lags = std::max(0, lags);
    const int n = s.size();
    const int start = out.lags + 1;
    if (n - start < 8 + out.lags)
        return out;
    QVector<double> dy, level;
    QVector<QVector<double>> X;
    X.push_back({});   // constant
    X.push_back({});   // level s(t-1)
    for (int l = 1; l <= out.lags; ++l)
        X.push_back({});
    for (int t = start; t < n; ++t) {
        dy.push_back(s[t] - s[t - 1]);
        X[0].push_back(1.0);
        X[1].push_back(s[t - 1]);
        for (int l = 1; l <= out.lags; ++l)
            X[1 + l].push_back(s[t - l] - s[t - l - 1]);
    }
    QVector<double> coef, se;
    if (!solve(X, dy, &coef, &se) || se[1] <= 0)
        return out;
    out.stat = coef[1] / se[1];
    // MacKinnon (1994): the p-value is Φ of a polynomial in the statistic, one fit for the lower tail and
    // one for the upper (constant, no trend, one variable), as in statsmodels' mackinnonp.
    constexpr double starC = -1.61, minC = -18.83, maxC = 2.74;
    constexpr std::array<double, 3> smallP {2.1659, 1.4412, 0.038269};
    constexpr std::array<double, 4> largeP {1.7339, 0.93202, -0.12376, -0.0096262};
    const double t = out.stat;
    if (t > maxC)
        out.p = 1;
    else if (t < minC)
        out.p = 0;
    else if (t <= starC)
        out.p = normCdf(smallP[0] + smallP[1] * t + smallP[2] * t * t);
    else
        out.p = normCdf(largeP[0] + largeP[1] * t + largeP[2] * t * t + largeP[3] * t * t * t);
    return out;
}

double halfLife(const QVector<double> &s)
{
    QVector<double> lag, d;
    for (int i = 1; i < s.size(); ++i) {
        lag.push_back(s[i - 1]);
        d.push_back(s[i] - s[i - 1]);
    }
    const Ols o = ols(lag, d);
    if (!ok(o.beta) || o.beta >= 0)
        return NaN;
    return -std::log(2.0) / o.beta;
}

QVector<double> sma(const QVector<double> &s, int window)
{
    QVector<double> out(s.size(), NaN);
    if (window < 1)
        return out;
    double sum = 0;
    for (int i = 0; i < s.size(); ++i) {
        sum += s[i];
        if (i >= window)
            sum -= s[i - window];
        if (i >= window - 1)
            out[i] = sum / window;
    }
    return out;
}

QVector<double> rollingStd(const QVector<double> &s, int window)
{
    QVector<double> out(s.size(), NaN);
    if (window < 2)
        return out;
    for (int i = window - 1; i < s.size(); ++i) {
        double m = 0;
        for (int k = i - window + 1; k <= i; ++k)
            m += s[k];
        m /= window;
        double v = 0;
        for (int k = i - window + 1; k <= i; ++k)
            v += (s[k] - m) * (s[k] - m);
        out[i] = std::sqrt(v / (window - 1));
    }
    return out;
}

QVector<double> zscore(const QVector<double> &s, int window)
{
    const QVector<double> m = sma(s, window), sd = rollingStd(s, window);
    QVector<double> out(s.size(), NaN);
    for (int i = 0; i < s.size(); ++i)
        if (ok(m[i]) && ok(sd[i]) && sd[i] > 0)
            out[i] = (s[i] - m[i]) / sd[i];
    return out;
}

QVector<double> rsi(const QVector<double> &close, int period)
{
    QVector<double> out(close.size(), NaN);
    if (close.size() <= period || period < 1)
        return out;
    double gain = 0, loss = 0;
    for (int i = 1; i <= period; ++i) {
        const double d = close[i] - close[i - 1];
        if (d > 0) gain += d; else loss -= d;
    }
    gain /= period;
    loss /= period;
    auto value = [](double g, double l) { return l == 0 ? 100.0 : 100.0 - 100.0 / (1 + g / l); };
    out[period] = value(gain, loss);
    for (int i = period + 1; i < close.size(); ++i) {
        const double d = close[i] - close[i - 1];
        gain = (gain * (period - 1) + std::max(d, 0.0)) / period;
        loss = (loss * (period - 1) + std::max(-d, 0.0)) / period;
        out[i] = value(gain, loss);
    }
    return out;
}

QVector<double> returns(const QVector<double> &prices)
{
    QVector<double> out;
    for (int i = 1; i < prices.size(); ++i)
        out.push_back(prices[i - 1] > 0 ? 100.0 * (prices[i] / prices[i - 1] - 1) : NaN);
    return out;
}

void align(Prices *a, Prices *b)
{
    Prices oa, ob;
    int i = 0, j = 0;
    while (i < a->dates.size() && j < b->dates.size()) {
        if (a->dates[i] == b->dates[j]) {
            oa.dates.push_back(a->dates[i]);
            oa.close.push_back(a->close[i]);
            ob.dates.push_back(b->dates[j]);
            ob.close.push_back(b->close[j]);
            ++i;
            ++j;
        } else if (a->dates[i] < b->dates[j]) {
            ++i;
        } else {
            ++j;
        }
    }
    *a = oa;
    *b = ob;
}

Prices weekly(const Prices &daily)
{
    Prices out;
    for (int i = 0; i < daily.dates.size(); ++i) {
        int y0 = 0, y1 = 0;
        const int w0 = daily.dates[i].weekNumber(&y0);
        const bool lastOfWeek = i + 1 == daily.dates.size() || daily.dates[i + 1].weekNumber(&y1) != w0 || y1 != y0;
        if (lastOfWeek) {
            out.dates.push_back(daily.dates[i]);
            out.close.push_back(daily.close[i]);
        }
    }
    return out;
}

} // namespace quant

// ---------------------------------------------------------------- CotReport

CotReport::CotReport(QObject *parent)
    : RemoteDataSet("cftc_legacy", 12 * 3600, parent)
{
    m_markets = {
        {"sp500", "S&P 500", "13874+"}, {"nasdaq", "NASDAQ", "20974+"}, {"dow", "DOW", "12460+"},
        {"russell", "RUSSELL", "239742"}, {"bitcoin", "BITCOIN", "133741"}, {"gold", "GOLD", "088691"},
        {"silver", "SILVER", "084691"},
    };
    loadCached();
}

const CotReport::Series &CotReport::series(const QString &id) const
{
    static const Series none;
    auto it = m_series.constFind(id);
    return it == m_series.constEnd() ? none : *it;
}

QUrl CotReport::url() const
{
    QStringList codes;
    for (const Market &m : m_markets)
        codes << QString("'%1'").arg(m.code);
    // Two years of weekly reports, plus a few weeks so the first one shown has a previous week.
    const QString since = QDate::currentDate().addDays(-(2 * 365 + 21)).toString(Qt::ISODate);
    const QString select = "report_date_as_yyyy_mm_dd,cftc_contract_market_code,market_and_exchange_names,contract_units,"
                           "open_interest_all,noncomm_positions_long_all,noncomm_positions_short_all,noncomm_postions_spread_all,"
                           "comm_positions_long_all,comm_positions_short_all,tot_rept_positions_long_all,tot_rept_positions_short,"
                           "nonrept_positions_long_all,nonrept_positions_short_all,traders_tot_all,traders_noncomm_long_all,"
                           "traders_noncomm_short_all,traders_noncomm_spread_all,traders_comm_long_all,traders_comm_short_all,"
                           "traders_tot_rept_long_all,traders_tot_rept_short_all";
    const QString where = QString("cftc_contract_market_code in(%1) AND report_date_as_yyyy_mm_dd >= '%2'").arg(codes.join(','), since);
    // Contract codes contain '+', which the server would read as a space unless it's percent-encoded.
    QUrl url("https://publicreporting.cftc.gov/resource/6dca-aqww.json");
    url.setQuery(QString("$select=%1&$where=%2&$order=report_date_as_yyyy_mm_dd&$limit=5000")
                     .arg(QString::fromLatin1(QUrl::toPercentEncoding(select, ",")),
                          QString::fromLatin1(QUrl::toPercentEncoding(where))),
                 QUrl::StrictMode);
    return url;
}

bool CotReport::parse(const QByteArray &body)
{
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (!doc.isArray()) {
        m_error = doc.object().value("message").toString(tr("Unexpected CFTC response"));
        return false;
    }
    QHash<QString, QString> idOf;
    for (const Market &m : std::as_const(m_markets))
        idOf.insert(m.code, m.id);
    // Socrata returns numbers as strings.
    auto num = [](const QJsonObject &o, const char *key) { return o.value(QLatin1String(key)).toString().toLongLong(); };
    QHash<QString, Series> series;
    for (const QJsonValue &v : doc.array()) {
        const QJsonObject o = v.toObject();
        const QString id = idOf.value(o.value("cftc_contract_market_code").toString());
        if (id.isEmpty())
            continue;
        Week w;
        w.date = QDate::fromString(o.value("report_date_as_yyyy_mm_dd").toString().left(10), Qt::ISODate);
        if (!w.date.isValid())
            continue;
        w.openInterest = num(o, "open_interest_all");
        w.ncLong = num(o, "noncomm_positions_long_all");
        w.ncShort = num(o, "noncomm_positions_short_all");
        w.ncSpread = num(o, "noncomm_postions_spread_all");
        w.cLong = num(o, "comm_positions_long_all");
        w.cShort = num(o, "comm_positions_short_all");
        w.trLong = num(o, "tot_rept_positions_long_all");
        w.trShort = num(o, "tot_rept_positions_short");
        w.nrLong = num(o, "nonrept_positions_long_all");
        w.nrShort = num(o, "nonrept_positions_short_all");
        w.traders = int(num(o, "traders_tot_all"));
        w.tNcLong = int(num(o, "traders_noncomm_long_all"));
        w.tNcShort = int(num(o, "traders_noncomm_short_all"));
        w.tNcSpread = int(num(o, "traders_noncomm_spread_all"));
        w.tCLong = int(num(o, "traders_comm_long_all"));
        w.tCShort = int(num(o, "traders_comm_short_all"));
        w.tTrLong = int(num(o, "traders_tot_rept_long_all"));
        w.tTrShort = int(num(o, "traders_tot_rept_short_all"));
        Series &s = series[id];
        s.name = o.value("market_and_exchange_names").toString().trimmed();
        s.units = o.value("contract_units").toString().trimmed();
        s.weeks.push_back(w);
    }
    if (series.isEmpty()) {
        m_error = tr("No CFTC positioning data");
        return false;
    }
    for (Series &s : series) {
        std::sort(s.weeks.begin(), s.weeks.end(), [](const Week &a, const Week &b) { return a.date < b.date; });
        // The same week can come twice (a corrected report): keep the last one.
        QVector<Week> dedup;
        for (const Week &w : std::as_const(s.weeks)) {
            if (!dedup.isEmpty() && dedup.last().date == w.date)
                dedup.last() = w;
            else
                dedup.push_back(w);
        }
        s.weeks = dedup;
    }
    m_series = series;
    return true;
}
