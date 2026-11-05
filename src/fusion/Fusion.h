#pragma once

#include <QDate>
#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QVector>

#include <limits>

// Data fusion: several sources of one indicator for one country (the sources of a row of
// resources/country_indicators.json) made into a golden record, one series richer than each source, every point
// telling where it comes from. Qt Core only, no network: the sources come in already fetched (extraction is the
// loaders' work), the golden records go out; tested by tests/quant/fusion.
//
// The pipeline:
//   normalise   each source on its own: values that aren't numbers dropped, dates put on the first day of their
//               period, one point per period (the last one published wins), its frequency, its unit parsed
//               (magnitude, currency, price base).
//   compare     entity resolution: does a source measure the same thing as the record built so far? Checked on
//               the periods both have: the same values (Same), the same up to a factor or a shift (Rebased: a new
//               index base, constant prices of another year, a magnitude), something else (Distinct: a registered
//               unemployment rate beside a survey's), or not checkable (Unverified: no common periods).
//   dedupe      the points of all sources grouped by period, per frequency (frequencies are never mixed: a monthly
//               series isn't extended by annual points; there is one record per frequency).
//   fuse        sources in priority order (the catalogue's: a country's own office or central bank first): the
//               first is the anchor; each next source fills the periods the record lacks (its older history, a
//               gap, the months after the anchor stopped), rebased when it must be, and its disagreements with the
//               record on the periods both have are noted as conflicts (the record's value stays).
//   golden      the record: points with their source, the spliced ones marked with their adjustment; what each
//               source brought and why.
namespace fusion {

constexpr double NaN = std::numeric_limits<double>::quiet_NaN();

enum class Freq { Unknown, Daily, Weekly, Monthly, Quarterly, Annual };
QString freqName(Freq f);   // "monthly"…, for logs and files

struct Point {
    QDate date;
    double value = NaN;
};
using Series = QVector<Point>;

// A unit as the catalogue writes it, read: "EUR Billion", "USD Billion (2017)", "percent", "Thousand".
struct Unit {
    QString text;          // as given
    QString base;          // what is left once magnitude, currency and price year are taken out ("", "barrels"…)
    QString currency;      // "USD", "EUR"… empty when none
    QString priceYear;     // "2017" for constant prices of 2017
    double magnitude = 1;  // Thousand 1e3, Million 1e6, Billion 1e9
    bool rate = false;     // percent, percentage points, percent of GDP: values that rebasing would make meaningless
};
Unit parseUnit(const QString &text);

// One source's series as extraction gives it, already in the indicator's terms (scaled, transformed: what the
// row would show if this source were the only one).
struct Input {
    QString id;            // stable: "eurostat:une_rt_m?s_adj=SA&…"
    QString label;         // "Eurostat", for people
    int priority = 0;      // 0 the most trusted (the catalogue's order)
    QString unit;          // English, {CUR} filled in
    Series points;
    QDateTime fetched;
};

struct Options {
    int minOverlap = 3;            // common periods needed to compare two sources
    double levelTolerance = 0.01;  // Same: the median gap within 1 % of the level (levels, indexes)
    double rateTolerance = 0.1;    // Same: the median gap within 0.1 point (rates), rounding included
    double rebaseSpread = 0.02;    // Rebased: the factor (or shift) steady within 2 % over the common periods
    double minCorrelation = 0.95;  // Rebased: the two move together (rank correlation)
    double minAgreement = 0.9;     // Same, Rebased: the periods within twice the tolerance (a median alone lets a
                                   // third of the periods disagree)
    bool useUnverified = true;     // fill with a source that couldn't be checked (no common period, same unit)
};

// A source after normalisation.
struct Clean {
    Input input;
    Unit unit;
    Freq freq = Freq::Unknown;
    Series points;          // sorted, one per period, on the period's first day
    int dropped = 0;        // points left out (not a number, a period repeated)
};
Freq detectFreq(const Series &s);
QDate periodStart(const QDate &date, Freq f);
Clean normalise(const Input &in);

enum class Relation { Anchor, Same, Rebased, Distinct, Unverified, Incompatible };
QString relationName(Relation r);

// How `other` compares with `reference` on their common periods. Its values are taken × unitFactor (another
// magnitude: thousands against millions) × factor (Rebased: another index base or price year) before anything else.
struct Comparison {
    Relation relation = Relation::Unverified;
    double unitFactor = 1;
    double factor = 1;
    int overlap = 0;               // common periods
    double medianGap = NaN;        // median |reference − other| once adjusted: in points for rates, else relative
    double correlation = NaN;
    double agreement = NaN;        // the share of common periods within twice the tolerance, once adjusted
    QString why;                   // English, for the report
    bool rebased() const { return relation == Relation::Rebased; }
};
Comparison compare(const Series &reference, const Unit &referenceUnit, const Clean &other, const Options &o = {});

// A point of a golden record.
struct GoldenPoint {
    QDate date;
    double value = NaN;
    int source = -1;               // index in Golden::sources
    bool spliced = false;          // rebased from its source (Golden::sources[source].comparison says how)
    bool unverified = false;       // from a source no common period could check
};

// A period where a source disagreed with the record (the record's value stays: it comes from a source before it).
struct Conflict {
    QDate date;
    int kept = -1, other = -1;     // sources
    double keptValue = NaN, otherValue = NaN;   // otherValue after its adjustment
};

// What a source is in a record.
struct SourceUse {
    QString id, label;
    int priority = 0;
    Freq freq = Freq::Unknown;
    Comparison comparison;         // relation Anchor for the first
    int points = 0;                // after normalisation
    int used = 0;                  // points the record took from it
    int conflicts = 0;
    QDate first, last;
};

struct Golden {
    Freq freq = Freq::Unknown;
    QString unit;                  // the anchor's
    QVector<GoldenPoint> points;   // sorted
    QVector<SourceUse> sources;    // in priority order, every source of this frequency (unused ones included)
    QVector<Conflict> conflicts;
    int spliced = 0, unverified = 0;
    bool isEmpty() const { return points.isEmpty(); }
    Series series() const;
    // The source of the latest point ("" when empty).
    QString latestSource() const;
};

// The golden records of an indicator's sources: one per frequency found, the finest first.
QVector<Golden> fuse(const QVector<Input> &inputs, const Options &o = {});

} // namespace fusion
