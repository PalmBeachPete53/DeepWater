#pragma once

#include <QString>
#include <QVector>
#include <limits>

// Currencies scored on their fundamentals: each theme of a currency's latest releases reads as supportive, neutral or
// weak, by fixed rules stated in the page. An own, simple reading: not a forecast and not a standard.
namespace desk {

constexpr double NoValue = std::numeric_limits<double>::quiet_NaN();

struct Macro {
    QString currency;
    double policyRate = NoValue, policyChange = NoValue;   // the last move, in percentage points
    double cpi = NoValue, cpiPrevious = NoValue, cpiCore = NoValue;
    double gdp = NoValue, gdpPrevious = NoValue;
    double unemployment = NoValue, unemploymentPrevious = NoValue;
    double pmi = NoValue, pmiPrevious = NoValue;
    double confidence = NoValue, confidencePrevious = NoValue;
    double retail = NoValue;
};

enum class FundTheme { Rates, Inflation, Growth, Labour, Activity, Sentiment, Count };
enum class Reading { None, Weak, Neutral, Strong };

struct Cell {
    Reading reading = Reading::None;
    QString key;       // untranslated state: "hike", "hold", "cut", "above_target", "near_target", "below_target",
                       // "expanding", "flat", "contracting", "tight", "stable", "softening", "expansion", "contraction", "improving", "weakening"
    double value = NoValue, change = NoValue;
};
struct Score {
    QString currency;
    Cell cells[int(FundTheme::Count)];
    int score = 0;      // -100 to +100
    int covered = 0;    // themes with data
    enum Bias { Bearish, Neutral, Bullish } bias = Neutral;
};
Score scoreCurrency(const Macro &m);
// Strongest first.
QVector<Score> scoreAll(const QVector<Macro> &macros);

struct PairVerdict {
    QString favoured;   // empty: too close to call
    int leadsA = 0, leadsB = 0, compared = 0;
};
PairVerdict comparePair(const Score &a, const Score &b);

} // namespace desk
