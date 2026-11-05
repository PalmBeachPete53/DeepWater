#include "fusion/Fusion.h"

#include <QHash>
#include <QMap>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <cmath>

namespace fusion {

namespace {

double median(QVector<double> v)
{
    if (v.isEmpty())
        return NaN;
    std::sort(v.begin(), v.end());
    const int n = int(v.size());
    return n % 2 ? v[n / 2] : (v[n / 2 - 1] + v[n / 2]) / 2;
}

double pearson(const QVector<double> &a, const QVector<double> &b)
{
    const int n = int(a.size());
    if (n < 3)
        return NaN;
    double ma = 0, mb = 0;
    for (int i = 0; i < n; ++i) {
        ma += a[i];
        mb += b[i];
    }
    ma /= n;
    mb /= n;
    double sab = 0, saa = 0, sbb = 0;
    for (int i = 0; i < n; ++i) {
        sab += (a[i] - ma) * (b[i] - mb);
        saa += (a[i] - ma) * (a[i] - ma);
        sbb += (b[i] - mb) * (b[i] - mb);
    }
    if (saa <= 0 || sbb <= 0)
        return NaN;   // a flat series: nothing to correlate
    return sab / std::sqrt(saa * sbb);
}

QVector<double> ranks(const QVector<double> &v)
{
    QVector<int> order(v.size());
    for (int i = 0; i < order.size(); ++i)
        order[i] = i;
    std::sort(order.begin(), order.end(), [&](int x, int y) { return v[x] < v[y]; });
    QVector<double> r(v.size());
    for (int i = 0; i < order.size();) {
        int j = i;
        while (j + 1 < order.size() && v[order[j + 1]] == v[order[i]])
            ++j;
        for (int k = i; k <= j; ++k)
            r[order[k]] = (i + j) / 2.0;   // ties share their mean rank
        i = j + 1;
    }
    return r;
}

// Spearman's: on ranks, so one wrong point among many (the conflicts fusion reports) doesn't make two sources of
// the same thing look unrelated.
double correlation(const QVector<double> &a, const QVector<double> &b)
{
    return pearson(ranks(a), ranks(b));
}

QString number(double v)
{
    return QString::number(v, 'g', 4);
}

} // namespace

QString freqName(Freq f)
{
    switch (f) {
    case Freq::Daily: return QStringLiteral("daily");
    case Freq::Weekly: return QStringLiteral("weekly");
    case Freq::Monthly: return QStringLiteral("monthly");
    case Freq::Quarterly: return QStringLiteral("quarterly");
    case Freq::Annual: return QStringLiteral("annual");
    default: return QStringLiteral("unknown");
    }
}

QString relationName(Relation r)
{
    switch (r) {
    case Relation::Anchor: return QStringLiteral("anchor");
    case Relation::Same: return QStringLiteral("same");
    case Relation::Rebased: return QStringLiteral("rebased");
    case Relation::Distinct: return QStringLiteral("distinct");
    case Relation::Unverified: return QStringLiteral("unverified");
    case Relation::Incompatible: return QStringLiteral("incompatible");
    }
    return {};
}

// ---------------------------------------------------------------- normalise

Unit parseUnit(const QString &text)
{
    Unit u;
    u.text = text;
    QString t = text;
    static const QRegularExpression year(QStringLiteral("\\((\\d{4})\\)"));
    if (const auto m = year.match(t); m.hasMatch()) {
        u.priceYear = m.captured(1);
        t.remove(m.capturedStart(), m.capturedLength());
    }
    const QString lower = t.toLower();
    u.rate = lower.contains(QLatin1String("percent"));
    // Three capitals that are a currency: ISO 4217 codes the catalogue uses, and LCU (local currency units).
    static const QSet<QString> notCurrencies {QStringLiteral("BBL"), QStringLiteral("GDP"), QStringLiteral("TIV"),
                                              QStringLiteral("CPI"), QStringLiteral("PPP")};
    static const QHash<QString, double> magnitudes {{QStringLiteral("thousand"), 1e3}, {QStringLiteral("thousands"), 1e3},
                                                    {QStringLiteral("million"), 1e6}, {QStringLiteral("millions"), 1e6},
                                                    {QStringLiteral("billion"), 1e9}, {QStringLiteral("billions"), 1e9},
                                                    {QStringLiteral("trillion"), 1e12}};
    static const QRegularExpression words(QStringLiteral("[A-Za-z0-9%,.]+|[/]"));
    QStringList rest;
    auto it = words.globalMatch(t);
    while (it.hasNext()) {
        const QString w = it.next().captured(0);
        static const QRegularExpression code(QStringLiteral("^[A-Z]{3}$"));
        if (u.currency.isEmpty() && code.match(w).hasMatch() && !notCurrencies.contains(w)) {
            u.currency = w;
            continue;
        }
        if (const auto m = magnitudes.constFind(w.toLower()); m != magnitudes.constEnd()) {
            u.magnitude *= m.value();
            continue;
        }
        rest << w.toLower();
    }
    u.base = rest.join(QLatin1Char(' '));
    return u;
}

Freq detectFreq(const Series &s)
{
    if (s.size() < 2)
        return Freq::Annual;
    QVector<qint64> gaps;
    for (int i = std::max(1, int(s.size()) - 24); i < s.size(); ++i)
        gaps << s[i - 1].date.daysTo(s[i].date);
    std::sort(gaps.begin(), gaps.end());
    const qint64 g = gaps[gaps.size() / 2];
    if (g <= 4) return Freq::Daily;
    if (g <= 10) return Freq::Weekly;
    if (g <= 45) return Freq::Monthly;
    if (g <= 120) return Freq::Quarterly;
    return Freq::Annual;
}

QDate periodStart(const QDate &date, Freq f)
{
    switch (f) {
    case Freq::Annual: return QDate(date.year(), 1, 1);
    case Freq::Quarterly: return QDate(date.year(), (date.month() - 1) / 3 * 3 + 1, 1);
    case Freq::Monthly: return QDate(date.year(), date.month(), 1);
    case Freq::Weekly: return date.addDays(1 - date.dayOfWeek());   // the Monday: sources date weeks differently
    default: return date;
    }
}

Clean normalise(const Input &in)
{
    Clean c;
    c.input = in;
    c.unit = parseUnit(in.unit);
    Series pts;
    pts.reserve(in.points.size());
    for (const Point &p : in.points) {
        if (p.date.isValid() && std::isfinite(p.value))
            pts.push_back(p);
        else
            ++c.dropped;
    }
    std::stable_sort(pts.begin(), pts.end(), [](const Point &a, const Point &b) { return a.date < b.date; });
    c.freq = detectFreq(pts);
    // One point per period: the last one given (a revision listed after the first estimate).
    QMap<QDate, double> byPeriod;
    for (const Point &p : std::as_const(pts)) {
        const QDate d = periodStart(p.date, c.freq);
        if (byPeriod.contains(d))
            ++c.dropped;
        byPeriod.insert(d, p.value);
    }
    c.points.reserve(byPeriod.size());
    for (auto it = byPeriod.cbegin(); it != byPeriod.cend(); ++it)
        c.points.push_back({it.key(), it.value()});
    c.input.points.clear();   // not kept twice
    return c;
}

// ---------------------------------------------------------------- compare

Comparison compare(const Series &reference, const Unit &ru, const Clean &other, const Options &o)
{
    Comparison c;
    const Unit &ou = other.unit;
    if (ru.rate != ou.rate || ru.base != ou.base || ru.currency != ou.currency) {
        c.relation = Relation::Incompatible;
        c.why = QStringLiteral("another unit (%1 against %2)").arg(ou.text, ru.text);
        return c;
    }
    c.unitFactor = ou.magnitude / ru.magnitude;
    const bool priceBase = ru.priceYear != ou.priceYear;

    QHash<QDate, double> ref;
    ref.reserve(reference.size());
    for (const Point &p : reference)
        ref.insert(p.date, p.value);
    QVector<double> a, b;
    for (const Point &p : other.points)
        if (const auto it = ref.constFind(p.date); it != ref.constEnd()) {
            a << it.value();
            b << p.value * c.unitFactor;
        }
    c.overlap = int(a.size());
    if (c.overlap < o.minOverlap) {
        if (priceBase) {
            c.relation = Relation::Incompatible;
            c.why = QStringLiteral("prices of %1 against %2, too few common periods to rebase (%3)")
                        .arg(ou.priceYear.isEmpty() ? QStringLiteral("another year") : ou.priceYear,
                             ru.priceYear.isEmpty() ? QStringLiteral("current prices") : ru.priceYear)
                        .arg(c.overlap);
            return c;
        }
        c.relation = Relation::Unverified;
        c.why = QStringLiteral("%1 common periods: not enough to check").arg(c.overlap);
        return c;
    }

    QVector<double> abs;
    for (double v : std::as_const(a))
        abs << std::fabs(v);
    double level = median(abs);
    if (!(level > 0))
        level = 1;
    c.correlation = correlation(a, b);

    const double tolerance = ru.rate ? o.rateTolerance : o.levelTolerance;
    // The median gap once `other` is × factor, and the share of periods within twice the tolerance (per point:
    // relative to the point's level for levels, as fuse() counts conflicts).
    auto gapOf = [&](double factor, double *agreement = nullptr) {
        QVector<double> g;
        int within = 0;
        for (int i = 0; i < c.overlap; ++i) {
            const double d = std::fabs(a[i] - b[i] * factor);
            g << d;
            within += (ru.rate ? d : d / std::max(std::fabs(a[i]), 1e-12)) <= 2 * tolerance;
        }
        if (agreement)
            *agreement = double(within) / c.overlap;
        const double m = median(g);
        return ru.rate ? m : m / level;
    };
    auto percent = [](double share) { return QString::number(std::round(share * 100)); };

    if (!priceBase) {
        c.medianGap = gapOf(1, &c.agreement);
        if (c.medianGap <= tolerance && c.agreement >= o.minAgreement) {
            c.relation = Relation::Same;
            c.why = QStringLiteral("the same over %1 common periods (median gap %2%3, %4 % agree)")
                        .arg(c.overlap).arg(number(ru.rate ? c.medianGap : c.medianGap * 100), ru.rate ? QString() : QStringLiteral(" %"),
                                            percent(c.agreement));
            return c;
        }
        if (c.medianGap <= tolerance) {
            // Close in the middle, far too often: another series that shares some of its values.
            c.relation = Relation::Distinct;
            c.why = QStringLiteral("agrees on %1 % of %2 common periods only").arg(percent(c.agreement)).arg(c.overlap);
            return c;
        }
    }
    if (ru.rate) {
        // A rate shifted or scaled is another definition (registered against surveyed unemployment), not a base.
        c.relation = Relation::Distinct;
        c.why = QStringLiteral("rates %1 points apart over %2 common periods").arg(number(c.medianGap)).arg(c.overlap);
        return c;
    }
    // A steady ratio: another index base, another price year, another magnitude the units didn't say.
    QVector<double> ratios;
    bool signs = true;
    for (int i = 0; i < c.overlap; ++i) {
        if (a[i] == 0 || b[i] == 0 || (a[i] > 0) != (b[i] > 0)) {
            signs = false;
            break;
        }
        ratios << a[i] / b[i];
    }
    if (signs) {
        const double f = median(ratios);
        QVector<double> spread;
        for (double r : std::as_const(ratios))
            spread << std::fabs(r / f - 1);
        const double s = median(spread);
        const bool together = !std::isfinite(c.correlation) || c.correlation >= o.minCorrelation;
        double agreement = 0;
        const double gap = gapOf(f, &agreement);
        if (s <= o.rebaseSpread && together && agreement >= o.minAgreement) {
            c.relation = Relation::Rebased;
            c.factor = f;
            c.medianGap = gap;
            c.agreement = agreement;
            c.why = QStringLiteral("× %1 over %2 common periods (steady within %3 %%4)")
                        .arg(number(f)).arg(c.overlap).arg(number(s * 100),
                                                          priceBase ? QStringLiteral(", another price base") : QString());
            return c;
        }
    }
    c.relation = Relation::Distinct;
    if (!std::isfinite(c.medianGap))
        c.medianGap = gapOf(1);
    c.why = QStringLiteral("%1 % apart over %2 common periods, no steady ratio (correlation %3)")
                .arg(number(c.medianGap * 100)).arg(c.overlap).arg(number(c.correlation));
    return c;
}

// ---------------------------------------------------------------- fuse

Series Golden::series() const
{
    Series s;
    s.reserve(points.size());
    for (const GoldenPoint &p : points)
        s.push_back({p.date, p.value});
    return s;
}

QString Golden::latestSource() const
{
    if (points.isEmpty())
        return {};
    const int i = points.back().source;
    return i >= 0 && i < sources.size() ? sources[i].id : QString();
}

QVector<Golden> fuse(const QVector<Input> &inputs, const Options &o)
{
    // Normalise, then group by frequency, each group in priority order (the inputs' order between equals).
    QVector<Clean> cleaned;
    for (const Input &in : inputs) {
        Clean c = normalise(in);
        if (!c.points.isEmpty())
            cleaned.push_back(std::move(c));
    }
    std::stable_sort(cleaned.begin(), cleaned.end(), [](const Clean &a, const Clean &b) { return a.input.priority < b.input.priority; });
    QMap<int, QVector<const Clean *>> groups;
    for (const Clean &c : cleaned)
        groups[int(c.freq)].push_back(&c);

    QVector<Golden> out;
    for (auto g = groups.cbegin(); g != groups.cend(); ++g) {
        Golden golden;
        golden.freq = Freq(g.key());
        const QVector<const Clean *> &list = g.value();
        const Clean &anchor = *list.front();
        golden.unit = anchor.input.unit;
        QMap<QDate, GoldenPoint> record;
        const double tolerance = anchor.unit.rate ? o.rateTolerance : o.levelTolerance;

        for (int k = 0; k < list.size(); ++k) {
            const Clean &src = *list[k];
            SourceUse use;
            use.id = src.input.id;
            use.label = src.input.label;
            use.priority = src.input.priority;
            use.freq = src.freq;
            use.points = int(src.points.size());
            use.first = src.points.front().date;
            use.last = src.points.back().date;
            if (k == 0) {
                use.comparison.relation = Relation::Anchor;
                use.comparison.why = QStringLiteral("the most trusted source");
            } else {
                Series current;
                current.reserve(record.size());
                for (auto it = record.cbegin(); it != record.cend(); ++it)
                    current.push_back({it.key(), it->value});
                use.comparison = compare(current, anchor.unit, src, o);
            }
            const Relation rel = use.comparison.relation;
            const bool usable = rel == Relation::Anchor || rel == Relation::Same || rel == Relation::Rebased
                                || (rel == Relation::Unverified && o.useUnverified);
            if (usable) {
                const double factor = use.comparison.unitFactor * use.comparison.factor;
                for (const Point &p : src.points) {
                    const double v = p.value * factor;
                    const auto it = record.constFind(p.date);
                    if (it != record.constEnd()) {
                        // Already known from a more trusted source: kept; a disagreement is noted.
                        const double gap = anchor.unit.rate ? std::fabs(it->value - v)
                                                            : std::fabs(it->value - v) / std::max(std::fabs(it->value), 1e-12);
                        if (gap > 2 * tolerance) {
                            golden.conflicts.push_back({p.date, it->source, k, it->value, v});
                            ++use.conflicts;
                        }
                        continue;
                    }
                    GoldenPoint gp;
                    gp.date = p.date;
                    gp.value = v;
                    gp.source = k;
                    gp.spliced = use.comparison.rebased();
                    gp.unverified = rel == Relation::Unverified;
                    record.insert(p.date, gp);
                    ++use.used;
                    golden.spliced += gp.spliced;
                    golden.unverified += gp.unverified;
                }
            }
            golden.sources.push_back(use);
        }
        golden.points.reserve(record.size());
        for (const GoldenPoint &p : std::as_const(record))
            golden.points.push_back(p);
        std::sort(golden.conflicts.begin(), golden.conflicts.end(), [](const Conflict &a, const Conflict &b) { return a.date < b.date; });
        out.push_back(std::move(golden));
    }
    return out;
}

} // namespace fusion
