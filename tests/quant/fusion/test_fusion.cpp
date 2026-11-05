// Data fusion (src/fusion) on made-up sources. Plain checks, a count at the end, non-zero exit on any failure.
#include "fusion/Fusion.h"
#include "fusion/Store.h"

#include <QFile>
#include <QTemporaryDir>

#include <QCoreApplication>
#include <QTimeZone>

#include <cmath>
#include <functional>
#include <iostream>

using namespace fusion;

namespace {

int checks = 0, fails = 0;
QString CAT = "misc";

void check(bool ok, const QString &msg)
{
    ++checks;
    if (!ok) {
        ++fails;
        std::cout << "FAIL [" << CAT.toStdString() << "] " << msg.toStdString() << "\n";
    }
}
template <typename T>
void eq(T want, T got, const QString &ctx)
{
    check(want == got, QStringLiteral("%1: want %2, got %3").arg(ctx).arg(want).arg(got));
}
void eqs(const QString &want, const QString &got, const QString &ctx)
{
    check(want == got, QStringLiteral("%1: want \"%2\", got \"%3\"").arg(ctx, want, got));
}
void near(double want, double got, double tol, const QString &ctx)
{
    check(std::fabs(want - got) <= tol, QStringLiteral("%1: want %2, got %3").arg(ctx).arg(want, 0, 'g', 10).arg(got, 0, 'g', 10));
}

// Monthly points from `from` for `n` months, value f(i).
Series monthly(QDate from, int n, const std::function<double(int)> &f)
{
    Series s;
    for (int i = 0; i < n; ++i)
        s.push_back({from.addMonths(i), f(i)});
    return s;
}
Series annual(int fromYear, int n, const std::function<double(int)> &f)
{
    Series s;
    for (int i = 0; i < n; ++i)
        s.push_back({QDate(fromYear + i, 1, 1), f(i)});
    return s;
}
Input input(const QString &id, int priority, const QString &unit, const Series &points)
{
    Input in;
    in.id = id;
    in.label = id;
    in.priority = priority;
    in.unit = unit;
    in.points = points;
    return in;
}
const Golden *withFreq(const QVector<Golden> &gs, Freq f)
{
    for (const Golden &g : gs)
        if (g.freq == f)
            return &g;
    return nullptr;
}
double valueAt(const Golden &g, QDate d, int *source = nullptr, bool *spliced = nullptr)
{
    for (const GoldenPoint &p : g.points)
        if (p.date == d) {
            if (source) *source = p.source;
            if (spliced) *spliced = p.spliced;
            return p.value;
        }
    return NaN;
}
// A wavy level, so two sources have something to correlate.
double wave(int i) { return 100 + 10 * std::sin(i / 5.0) + i * 0.3; }

void units()
{
    CAT = "units";
    Unit u = parseUnit("EUR Billion");
    eqs("EUR", u.currency, "EUR Billion: currency");
    near(1e9, u.magnitude, 0, "EUR Billion: magnitude");
    eqs("", u.base, "EUR Billion: base");
    u = parseUnit("USD Billion (2017)");
    eqs("2017", u.priceYear, "constant prices: year");
    eqs("USD", u.currency, "constant prices: currency");
    u = parseUnit("percent of GDP");
    check(u.rate, "percent of GDP is a rate");
    eqs("percent of gdp", u.base, "percent of GDP: base");
    u = parseUnit("Thousand");
    near(1e3, u.magnitude, 0, "Thousand: magnitude");
    eqs("", u.base, "Thousand: base");
    u = parseUnit("Thousand Barrels Per Day");
    eqs("barrels per day", u.base, "barrels: base");
    eqs("", u.currency, "barrels: no currency");
    u = parseUnit("BBL/D/1K");
    eqs("", u.currency, "BBL is no currency");
    u = parseUnit("points");
    check(!u.rate && u.currency.isEmpty() && u.magnitude == 1, "points: plain");
}

void normalisation()
{
    CAT = "normalise";
    Input in = input("a", 0, "points", {});
    // Mid-month dates, a NaN, a month given twice (the revision after the first estimate), out of order.
    in.points = {{QDate(2024, 3, 15), 3}, {QDate(2024, 1, 31), 1}, {QDate(2024, 2, 10), NaN}, {QDate(2024, 2, 1), 2},
                 {QDate(2024, 4, 1), 4}, {QDate(2024, 4, 20), 4.5}, {QDate(2024, 5, 1), 5}};
    const Clean c = normalise(in);
    check(c.freq == Freq::Monthly, "monthly detected");
    eq(5, int(c.points.size()), "one point per month");
    eq(2, c.dropped, "a NaN and a repeated month dropped");
    check(c.points.front().date == QDate(2024, 1, 1), "dates on the first day of the month");
    near(4.5, c.points[3].value, 0, "the later of two points of a month wins");
    // Weeks dated on different days land on the same Monday.
    Series sat, fri;
    for (int i = 0; i < 10; ++i) {
        sat.push_back({QDate(2026, 1, 3).addDays(7 * i), double(i)});    // Saturdays
        fri.push_back({QDate(2026, 1, 2).addDays(7 * i), double(i)});    // Fridays
    }
    const Clean a = normalise(input("sat", 0, "Thousand", sat)), b = normalise(input("fri", 1, "Thousand", fri));
    check(a.freq == Freq::Weekly, "weekly detected");
    check(a.points.front().date == b.points.front().date, "Saturday and Friday weeks share their Monday");
    eq(1, a.points.front().date.dayOfWeek(), "a week starts on Monday");
}

void same()
{
    CAT = "same";
    // The anchor from 2015, a second source the same from 2000: the record goes back to 2000, nothing spliced.
    const Series a = monthly({2015, 1, 1}, 120, [](int i) { return 5 + 0.01 * i; });
    const Series b = monthly({2000, 1, 1}, 240, [](int i) { return 5 + 0.01 * (i - 180); });
    const QVector<Golden> gs = fuse({input("office", 0, "percent", a), input("oecd", 1, "percent", b)});
    eq(1, int(gs.size()), "one frequency");
    const Golden &g = gs.front();
    eq(int(Relation::Same), int(g.sources[1].comparison.relation), "the same series");
    eq(120 + 180, int(g.points.size()), "history from 2000");
    check(g.points.front().date == QDate(2000, 1, 1), "first point in 2000");
    eq(0, g.spliced, "nothing spliced");
    eq(0, int(g.conflicts.size()), "no conflict");
    eq(180, g.sources[1].used, "OECD brought the 15 years before");
    int src = -1;
    valueAt(g, QDate(2020, 1, 1), &src);
    eq(0, src, "the office's value where both have one");
}

void rebased()
{
    CAT = "rebased";
    // A CPI with base 2015 = 100 from 2015, the same index with base 2010 = 100 from 1990: the old history
    // rebased by the ratio on their common years, and marked.
    const auto cpi = [](int i) { return 60 * std::pow(1.002, i) * (1 + 0.003 * std::sin(i / 3.0)); };   // month i from 1990
    const double base2015 = cpi(25 * 12), base2010 = cpi(20 * 12);
    Series a, b;
    for (int i = 25 * 12; i < 36 * 12; ++i) a.push_back({QDate(1990, 1, 1).addMonths(i), 100 * cpi(i) / base2015});
    for (int i = 0; i < 34 * 12; ++i) b.push_back({QDate(1990, 1, 1).addMonths(i), 100 * cpi(i) / base2010});
    const Golden g = fuse({input("office", 0, "points", a), input("imf", 1, "points", b)}).front();
    const Comparison &c = g.sources[1].comparison;
    eq(int(Relation::Rebased), int(c.relation), "another base");
    near(base2010 / base2015, c.factor, 1e-9, "the factor between the bases");
    eq(25 * 12, g.spliced, "the 25 years before 2015 spliced");
    bool spliced = false;
    int src = -1;
    const double v = valueAt(g, QDate(1995, 6, 1), &src, &spliced);
    near(100 * cpi(5 * 12 + 5) / base2015, v, 1e-9, "1995 on the 2015 base");
    check(spliced && src == 1, "1995 from the IMF, spliced");
    eq(0, int(g.conflicts.size()), "no conflict once rebased");
    // One wrong month among the common ones: still rebased, the month a conflict.
    Series bad = b;
    bad[26 * 12].value *= 1.2;
    const Golden gb = fuse({input("office", 0, "points", a), input("imf", 1, "points", bad)}).front();
    eq(int(Relation::Rebased), int(gb.sources[1].comparison.relation), "one outlier doesn't break the relation");
    eq(1, int(gb.conflicts.size()), "the outlier is a conflict");
    // A rate is never rebased: one shifted by a point is another definition.
    const Series r1 = monthly({2010, 1, 1}, 100, [](int i) { return 5 + std::sin(i / 7.0); });
    const Series r2 = monthly({2000, 1, 1}, 220, [](int i) { return 6 + std::sin((i - 120) / 7.0); });
    const Golden gr = fuse({input("survey", 0, "percent", r1), input("other", 1, "percent", r2)}).front();
    eq(int(Relation::Distinct), int(gr.sources[1].comparison.relation), "a shifted rate is distinct");
    eq(100, int(gr.points.size()), "nothing taken from it");
}

void distinct()
{
    CAT = "distinct";
    // Registered unemployment beside a survey's: the gap moves, no steady ratio.
    const Series survey = monthly({2010, 1, 1}, 150, [](int i) { return 8 + 2 * std::sin(i / 9.0); });
    const Series registered = monthly({2000, 1, 1}, 270, [](int i) { return 10 + 4 * std::cos((i - 120) / 6.0); });
    const Golden g = fuse({input("eurostat", 0, "Thousand", survey), input("agency", 1, "Thousand", registered)}).front();
    eq(int(Relation::Distinct), int(g.sources[1].comparison.relation), "distinct definitions");
    eq(0, g.sources[1].used, "not used");
    eq(150, int(g.points.size()), "the record is the anchor alone");
    // The same values on two periods out of three, far off on the third: the median gap is nil, yet it isn't
    // the same series.
    const Series m1 = monthly({2000, 1, 1}, 300, [](int i) { return std::sin(i / 4.0); });
    const Series m2 = monthly({1990, 1, 1}, 420, [](int i) { const int j = i - 120; return (j % 3 == 0 ? 2.5 : 0) + std::sin(j / 4.0); });
    const Golden gm = fuse({input("eurostat", 0, "percent", m1), input("wb", 1, "percent", m2)}).front();
    eq(int(Relation::Distinct), int(gm.sources[1].comparison.relation), "a third of the periods off");
    near(2.0 / 3, gm.sources[1].comparison.agreement, 0.01, "agreement");
}

void incompatible()
{
    CAT = "units in fusion";
    const Series usd = annual(2010, 15, [](int i) { return 100 + i; });
    const Series eur = annual(1995, 30, [](int i) { return 90 + i; });
    Golden g = fuse({input("wb", 0, "USD Billion", usd), input("ecb", 1, "EUR Billion", eur)}).front();
    eq(int(Relation::Incompatible), int(g.sources[1].comparison.relation), "euros against dollars");
    eq(15, int(g.points.size()), "no euros in a dollar record");
    // Thousands against millions: a unit conversion, not a splice.
    const Series th = annual(2010, 15, [](int i) { return 1000.0 * (5 + i); });
    const Series mi = annual(1990, 35, [](int i) { return 5.0 + (i - 20); });
    g = fuse({input("a", 0, "Thousand", th), input("b", 1, "Million", mi)}).front();
    eq(int(Relation::Same), int(g.sources[1].comparison.relation), "millions are thousands × 1000");
    near(1000, g.sources[1].comparison.unitFactor, 0, "unit factor");
    near(1000.0 * (5 - 20), valueAt(g, QDate(1990, 1, 1)), 1e-9, "1990 in thousands");
    eq(0, g.spliced, "a conversion isn't a splice");
    // Constant prices of another year with no common period: can't be rebased.
    const Series p17 = annual(2015, 10, [](int i) { return 100.0 + i; });
    const Series p10 = annual(1990, 20, [](int i) { return 80.0 + i; });
    g = fuse({input("fred", 0, "USD Billion (2017)", p17), input("wb", 1, "USD Billion (2010)", p10)}).front();
    eq(int(Relation::Incompatible), int(g.sources[1].comparison.relation), "another price year, no overlap");
}

void conflicts()
{
    CAT = "conflicts";
    Series a = monthly({2020, 1, 1}, 48, wave), b = monthly({2018, 1, 1}, 72, [](int i) { return wave(i - 24); });
    b[24 + 10].value *= 1.05;   // one month 5 % off
    const Golden g = fuse({input("office", 0, "points", a), input("oecd", 1, "points", b)}).front();
    eq(int(Relation::Same), int(g.sources[1].comparison.relation), "still the same series");
    eq(1, int(g.conflicts.size()), "one conflict");
    if (!g.conflicts.isEmpty()) {
        const Conflict &c = g.conflicts.front();
        check(c.date == QDate(2020, 11, 1), "on the month that differs");
        eq(0, c.kept, "the office's value kept");
        near(a[10].value, valueAt(g, c.date), 0, "the record holds the office's value");
    }
    eq(1, g.sources[1].conflicts, "counted for the source");
}

void frequencies()
{
    CAT = "frequencies";
    const Series m = monthly({2015, 1, 1}, 120, [](int i) { return 5 + 0.01 * i; });
    const Series y = annual(1980, 46, [](int i) { return 6 + 0.1 * i; });
    const QVector<Golden> gs = fuse({input("annual", 0, "percent", y), input("monthly", 1, "percent", m)});
    eq(2, int(gs.size()), "a record per frequency");
    check(gs.size() == 2 && gs[0].freq == Freq::Monthly && gs[1].freq == Freq::Annual, "the finest first");
    const Golden *mg = withFreq(gs, Freq::Monthly);
    check(mg && mg->points.size() == 120, "no annual point in the monthly record");
}

void staleAnchor()
{
    CAT = "stale anchor";
    // The most trusted source stopped in 2019; the next one goes on to 2026: the record does too.
    const Series a = monthly({2000, 1, 1}, 240, wave);
    const Series b = monthly({2010, 1, 1}, 200, [](int i) { return wave(i + 120); });
    const Golden g = fuse({input("office", 0, "points", a), input("imf", 1, "points", b)}).front();
    check(g.points.back().date == QDate(2010, 1, 1).addMonths(199), "goes on with the IMF");
    eqs("imf", g.latestSource(), "the latest point from the IMF");
    eq(80, g.sources[1].used, "the months after the office stopped");
}

void gapsAndUnverified()
{
    CAT = "gaps, unverified";
    Series a = monthly({2020, 1, 1}, 36, wave);
    a.remove(10, 3);   // three months missing
    const Series b = monthly({2020, 1, 1}, 36, wave);
    Golden g = fuse({input("office", 0, "points", a), input("oecd", 1, "points", b)}).front();
    eq(36, int(g.points.size()), "the gap filled");
    eq(3, g.sources[1].used, "three months from OECD");
    // No common period: used when allowed, marked.
    const Series old = monthly({1990, 1, 1}, 60, wave);
    g = fuse({input("office", 0, "points", b), input("archive", 1, "points", old)}).front();
    eq(int(Relation::Unverified), int(g.sources[1].comparison.relation), "nothing to compare");
    eq(60, g.unverified, "marked unverified");
    Options strict;
    strict.useUnverified = false;
    g = fuse({input("office", 0, "points", b), input("archive", 1, "points", old)}, strict).front();
    eq(36, int(g.points.size()), "left out when not allowed");
}

void priority()
{
    CAT = "priority";
    const Series a = monthly({2020, 1, 1}, 24, [](int) { return 1.0; });
    const Series b = monthly({2020, 1, 1}, 24, [](int) { return 1.5; });
    // Given in the wrong order: priority decides.
    const Golden g = fuse({input("second", 1, "percent", b), input("first", 0, "percent", a)}).front();
    eqs("first", g.sources[0].id, "the priority-0 source anchors");
    near(1.0, g.points.front().value, 0, "its values");
}

void store()
{
    CAT = "store";
    Series a = monthly({2020, 1, 1}, 48, wave), b = monthly({2010, 1, 1}, 160, [](int i) { return 1.1 * wave(i - 120); });
    b[130].value *= 1.2;
    Record r;
    r.country = "FRA";
    r.indicator = "cpi";
    r.fused = QDateTime(QDate(2026, 11, 3), QTime(12, 0), QTimeZone::UTC);
    r.inputs = {input("insee", 0, "points", a), input("imf", 1, "points", b)};
    r.golden = fuse(r.inputs);
    QTemporaryDir dir;
    QString error;
    check(save(dir.path(), r, &error), "saved: " + error);
    check(QFile::exists(rawPath(dir.path(), "FRA", "cpi")), "the sources beside it");
    const std::optional<Record> back = load(dir.path(), "FRA", "cpi");
    check(back.has_value(), "read back");
    if (!back)
        return;
    check(back->fused == r.fused, "fusion time");
    eq(int(r.golden.size()), int(back->golden.size()), "records");
    const Golden &g = r.golden.front(), &h = back->golden.front();
    eq(int(g.points.size()), int(h.points.size()), "points");
    bool same = true;
    for (int i = 0; i < g.points.size() && i < h.points.size(); ++i)
        same = same && g.points[i].date == h.points[i].date && g.points[i].value == h.points[i].value
               && g.points[i].source == h.points[i].source && g.points[i].spliced == h.points[i].spliced;
    check(same, "every point, its source and flag");
    eq(g.spliced, h.spliced, "spliced count");
    eq(int(g.conflicts.size()), int(h.conflicts.size()), "conflicts");
    eq(int(Relation::Rebased), int(h.sources[1].comparison.relation), "relation");
    near(g.sources[1].comparison.factor, h.sources[1].comparison.factor, 0, "factor");
    eqs(g.sources[1].comparison.why, h.sources[1].comparison.why, "why");
    check(!load(dir.path(), "FRA", "gdp").has_value(), "no file, no record");
    // The index: one entry per country and indicator, replaced by the next save.
    QVector<Summary> index = loadIndex(dir.path());
    eq(1, int(index.size()), "one entry");
    if (!index.isEmpty()) {
        const Summary &x = index.front();
        eqs("FRA", x.country, "index: country");
        eqs("cpi", x.indicator, "index: indicator");
        eq(1, x.records, "index: records of two sources or more");
        eq(g.sources[1].used, x.added, "index: points added after the most trusted");
        eq(g.spliced, x.spliced, "index: spliced");
        check(x.providers == QStringList({"insee", "imf"}), "index: providers " + x.providers.join(','));
    }
    r.indicator = "gdp";
    save(dir.path(), r);
    save(dir.path(), r);
    eq(2, int(loadIndex(dir.path()).size()), "a second indicator, saved twice: two entries");
    QFile::remove(indexPath(dir.path()));
    eq(2, int(loadIndex(dir.path()).size()), "the index rebuilt from the records");
    check(QFile::exists(indexPath(dir.path())), "and written");
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    units();
    normalisation();
    same();
    rebased();
    distinct();
    incompatible();
    conflicts();
    frequencies();
    staleAnchor();
    gapsAndUnverified();
    priority();
    store();
    std::cout << checks - fails << "/" << checks << " checks passed\n";
    return fails ? 1 : 0;
}
