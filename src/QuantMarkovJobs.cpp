#include "QuantMarkovJobs.h"

namespace quant {

namespace {
constexpr int VolWindow = 10;
constexpr int RealizedWindow = 20;
constexpr double Annual = 15.874507866387544;
}

QJsonObject HmmSpec::toJson() const
{
    return {{"range", range}, {"states", states}, {"withVol", withVol}, {"window", window}};
}

HmmSpec HmmSpec::fromJson(const QJsonObject &o)
{
    HmmSpec s;
    if (o.isEmpty())
        return s;
    s.range = o.value("range").toString(s.range);
    s.states = o.value("states").toInt(s.states);
    s.withVol = o.value("withVol").toBool(s.withVol);
    s.window = o.value("window").toInt(s.window);
    return s;
}

QJsonObject GarchSpec::toJson() const
{
    return {{"range", range}, {"regimes", regimes}, {"student", student}, {"horizon", horizon}};
}

GarchSpec GarchSpec::fromJson(const QJsonObject &o)
{
    GarchSpec s;
    if (o.isEmpty())
        return s;
    s.range = o.value("range").toString(s.range);
    s.regimes = o.value("regimes").toInt(s.regimes);
    s.student = o.value("student").toBool(s.student);
    s.horizon = o.value("horizon").toInt(s.horizon);
    return s;
}

HmmResult runHmm(const Prices &prices, const HmmSpec &spec, const markov::Progress &progress)
{
    HmmResult r;
    // Observations: daily returns in %, from the second price on; the realized volatility over the last VolWindow
    // returns as a second variable (missing on the first days: the HMM skips those rows).
    const int n = prices.dates.size() - 1;
    if (n < 30) {
        r.fit.error = QObject::tr("Not enough prices");
        return r;
    }
    r.dates = prices.dates.mid(1);
    r.price = prices.close.mid(1);
    const QVector<double> ret = returns(prices.close);
    r.obs = markov::Mat(n, spec.withVol ? 2 : 1);
    const QVector<double> vol = spec.withVol ? rollingStd(ret, VolWindow) : QVector<double>();
    for (int t = 0; t < n; ++t) {
        r.obs(t, 0) = ret[t];
        if (spec.withVol)
            r.obs(t, 1) = vol[t];
    }
    markov::Mat sample = r.obs;
    if (spec.window > 0 && spec.window < n) {
        sample = markov::Mat(spec.window, r.obs.cols);
        for (int t = 0; t < spec.window; ++t)
            sample.setRow(t, r.obs.row(n - spec.window + t));
        r.window = spec.window;
    }
    // The shown model gets the full search; the other K a quicker one, for the comparison table.
    QVector<int> ks {spec.states};
    if (spec.compareK)
        ks = {2, 3, 4};
    int step = 0;
    for (int k : ks) {
        if (progress.cancelled())
            return r;
        markov::HmmOptions o;
        o.states = k;
        o.initializations = k == spec.states ? 8 : 3;
        const int base = 100 * step / ks.size(), span = 100 / ks.size();
        markov::Progress sub;
        sub.cancel = progress.cancel;
        sub.report = [&](int pct, const QString &text) { progress(base + pct * span / 100, QString("K = %1 · %2").arg(k).arg(text)); };
        const markov::HmmFit fit = markov::GaussianHmm::fit(sample, o, sub);
        r.others.push_back(fit);
        if (k == spec.states)
            r.fit = fit;
        ++step;
    }
    if (!r.fit.params.isValid())
        return r;
    r.filtered = markov::GaussianHmm::filter(r.obs, r.fit.params);
    r.smoothed = markov::GaussianHmm::smooth(r.obs, r.fit.params);
    r.viterbi = markov::GaussianHmm::viterbi(r.obs, r.fit.params);
    return r;
}

GarchResult runGarch(const Prices &prices, const GarchSpec &spec, const markov::Progress &progress)
{
    GarchResult r;
    r.horizonDays = spec.horizon;
    if (prices.dates.size() < 120) {
        r.fit.error = QObject::tr("Not enough prices");
        return r;
    }
    r.dates = prices.dates.mid(1);
    r.returns = returns(prices.close);
    const QVector<double> sd = rollingStd(r.returns, RealizedWindow);
    r.realized.resize(sd.size());
    for (int i = 0; i < sd.size(); ++i)
        r.realized[i] = ok(sd[i]) ? sd[i] * Annual : NaN;
    markov::MsGarchOptions o;
    o.regimes = spec.regimes;
    o.innovations = spec.student ? markov::Innovations::Student : markov::Innovations::Normal;
    markov::Progress sub;
    sub.cancel = progress.cancel;
    sub.report = [&](int pct, const QString &text) { progress(pct * 3 / 4, QString("MS-GARCH K = %1 · %2").arg(spec.regimes).arg(text)); };
    r.fit = markov::MsGarch::fit(r.returns, o, sub);
    if (progress.cancelled() || !r.fit.params.isValid())
        return r;
    if (spec.regimes > 1 && spec.benchmark) {
        markov::MsGarchOptions o1 = o;
        o1.regimes = 1;
        o1.starts = 2;
        sub.report = [&](int pct, const QString &text) { progress(75 + pct / 4, QString("GARCH(1,1) · %1").arg(text)); };
        r.single = markov::MsGarch::fit(r.returns, o1, sub);
    } else {
        r.single = r.fit;
    }
    r.forecast = markov::MsGarch::forecast(r.fit, spec.horizon);
    r.oneDay = markov::MsGarch::riskOneDay(r.fit, r.forecast);
    r.horizon = markov::MsGarch::riskHorizon(r.fit, r.forecast);
    return r;
}

RegimeInfo regimeInfo(const QString &symbol, const HmmResult &r, const HmmSpec &spec)
{
    RegimeInfo info;
    const markov::HmmParams &p = r.fit.params;
    if (!p.isValid() || r.filtered.rows == 0)
        return info;
    const int K = p.states, n = r.filtered.rows;
    int f = 0;
    for (int k = 1; k < K; ++k)
        if (r.filtered(n - 1, k) > r.filtered(n - 1, f))
            f = k;
    info.symbol = symbol;
    info.source = "HMM";   // label and model are stored untranslated: see regimeKeys()
    info.states = K;
    info.regime = f;
    info.label = regimeKeys(K)[f];
    info.probability = r.filtered(n - 1, f);
    info.dailyVol = p.volatility(f);
    for (int k = 0; k < K; ++k) {
        info.vols.push_back(p.volatility(k));
        info.probabilities.push_back(r.filtered(n - 1, k));
    }
    info.transition = p.transition.a;
    info.asOf = r.dates.last();
    info.observations = r.dates.size();
    info.computedAt = QDateTime::currentDateTime();
    info.spec = spec.toJson();
    return info;
}

RegimeInfo regimeInfo(const QString &symbol, const GarchResult &r, const GarchSpec &spec)
{
    RegimeInfo info;
    const markov::MsGarchParams &p = r.fit.params;
    if (!p.isValid() || p.regimes < 2 || r.forecast.variance.isEmpty())
        return info;
    const int K = p.regimes;
    int f = 0;
    for (int k = 1; k < K; ++k)
        if (r.forecast.regimeProb(0, k) > r.forecast.regimeProb(0, f))
            f = k;
    info.symbol = symbol;
    info.source = "MS-GARCH";
    info.states = K;
    info.regime = f;
    info.label = regimeKeys(K)[f];
    info.probability = r.forecast.regimeProb(0, f);
    // Each regime's volatility for the next day, from its own recursion (not the long-run one).
    for (int k = 0; k < K; ++k) {
        info.vols.push_back(std::sqrt(r.forecast.regimeVariance(0, k)));
        info.probabilities.push_back(r.forecast.regimeProb(0, k));
    }
    info.dailyVol = info.vols[f];
    info.transition = p.transition.a;
    info.asOf = r.dates.last();
    info.observations = r.dates.size();
    info.computedAt = QDateTime::currentDateTime();
    info.spec = spec.toJson();
    return info;
}

VolInfo volInfo(const QString &symbol, const GarchResult &r, const GarchSpec &spec)
{
    VolInfo info;
    if (!r.fit.params.isValid() || r.forecast.variance.isEmpty())
        return info;
    const int K = r.fit.params.regimes;
    const bool student = r.fit.params.innovations == markov::Innovations::Student;
    info.symbol = symbol;
    info.model = K > 1 ? QString("MS-GARCH K=%1 · %2").arg(K).arg(student ? "Student-t" : "normal")   // translated by modelText()
                       : QString("GARCH(1,1) · %1").arg(student ? "Student-t" : "normal");
    info.dailyVol = std::sqrt(r.forecast.variance[0]);
    info.annualVol = info.dailyVol * Annual;
    info.horizonDays = r.horizonDays;
    info.horizonVol = std::sqrt(r.forecast.totalVariance);
    info.var95 = r.oneDay.var95;
    info.var99 = r.oneDay.var99;
    info.es95 = r.oneDay.es95;
    info.es99 = r.oneDay.es99;
    info.asOf = r.dates.last();
    info.observations = r.dates.size();
    info.computedAt = QDateTime::currentDateTime();
    info.spec = spec.toJson();
    return info;
}

} // namespace quant
