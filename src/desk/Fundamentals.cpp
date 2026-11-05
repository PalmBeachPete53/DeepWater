#include "desk/Fundamentals.h"

#include <algorithm>
#include <cmath>

namespace desk {

namespace {

bool has(double v) { return !std::isnan(v); }

Cell cell(Reading r, const char *key, double value, double previous = NoValue)
{
    Cell c;
    c.reading = r;
    c.key = key;
    c.value = value;
    c.change = has(previous) ? value - previous : NoValue;
    return c;
}

double valueOf(Reading r)
{
    switch (r) {
    case Reading::Strong: return 1;
    case Reading::Weak: return -1;
    default: return 0;
    }
}

} // namespace

Score scoreCurrency(const Macro &m)
{
    Score s;
    s.currency = m.currency;
    Cell *c = s.cells;
    if (has(m.policyRate)) {
        const double move = has(m.policyChange) ? m.policyChange : 0;
        c[int(FundTheme::Rates)] = move > 0 ? cell(Reading::Strong, "hike", m.policyRate)
                             : move < 0 ? cell(Reading::Weak, "cut", m.policyRate)
                                        : cell(Reading::Neutral, "hold", m.policyRate);
        c[int(FundTheme::Rates)].change = move;
    }
    if (has(m.cpi)) {
        // For a currency, inflation above target points to tighter policy; far below it, to easier policy.
        c[int(FundTheme::Inflation)] = m.cpi > 2.5 ? cell(Reading::Strong, "above_target", m.cpi, m.cpiPrevious)
                                 : m.cpi < 1.5 ? cell(Reading::Weak, "below_target", m.cpi, m.cpiPrevious)
                                               : cell(Reading::Neutral, "near_target", m.cpi, m.cpiPrevious);
    }
    if (has(m.gdp)) {
        c[int(FundTheme::Growth)] = m.gdp > 0.3 ? cell(Reading::Strong, "expanding", m.gdp, m.gdpPrevious)
                              : m.gdp < 0 ? cell(Reading::Weak, "contracting", m.gdp, m.gdpPrevious)
                                          : cell(Reading::Neutral, "flat", m.gdp, m.gdpPrevious);
    }
    if (has(m.unemployment)) {
        const double d = has(m.unemploymentPrevious) ? m.unemployment - m.unemploymentPrevious : 0;
        c[int(FundTheme::Labour)] = d > 0.05 ? cell(Reading::Weak, "softening", m.unemployment, m.unemploymentPrevious)
                              : d < -0.05 ? cell(Reading::Strong, "tight", m.unemployment, m.unemploymentPrevious)
                                          : cell(Reading::Neutral, "stable", m.unemployment, m.unemploymentPrevious);
    }
    if (has(m.pmi)) {
        c[int(FundTheme::Activity)] = m.pmi > 50.5 ? cell(Reading::Strong, "expansion", m.pmi, m.pmiPrevious)
                                : m.pmi < 49.5 ? cell(Reading::Weak, "contraction", m.pmi, m.pmiPrevious)
                                               : cell(Reading::Neutral, "flat", m.pmi, m.pmiPrevious);
    }
    if (has(m.confidence)) {
        const double d = has(m.confidencePrevious) ? m.confidence - m.confidencePrevious : 0;
        c[int(FundTheme::Sentiment)] = d > 0 ? cell(Reading::Strong, "improving", m.confidence, m.confidencePrevious)
                                 : d < 0 ? cell(Reading::Weak, "weakening", m.confidence, m.confidencePrevious)
                                         : cell(Reading::Neutral, "stable", m.confidence, m.confidencePrevious);
    }
    double sum = 0;
    for (int i = 0; i < int(FundTheme::Count); ++i)
        if (c[i].reading != Reading::None) {
            sum += valueOf(c[i].reading);
            ++s.covered;
        }
    s.score = s.covered ? int(std::round(sum / s.covered * 100.0)) : 0;
    s.bias = s.score >= 25 ? Score::Bullish : s.score <= -25 ? Score::Bearish : Score::Neutral;
    return s;
}

QVector<Score> scoreAll(const QVector<Macro> &macros)
{
    QVector<Score> out;
    for (const Macro &m : macros)
        out.push_back(scoreCurrency(m));
    std::stable_sort(out.begin(), out.end(), [](const Score &a, const Score &b) { return a.score > b.score; });
    return out;
}

PairVerdict comparePair(const Score &a, const Score &b)
{
    PairVerdict v;
    for (int i = 0; i < int(FundTheme::Count); ++i) {
        const Reading ra = a.cells[i].reading, rb = b.cells[i].reading;
        if (ra == Reading::None || rb == Reading::None)
            continue;
        ++v.compared;
        if (valueOf(ra) > valueOf(rb)) ++v.leadsA;
        else if (valueOf(rb) > valueOf(ra)) ++v.leadsB;
    }
    // On the themes both have: two scores can rest on different themes, so they don't decide.
    if (v.leadsA != v.leadsB)
        v.favoured = v.leadsA > v.leadsB ? a.currency : b.currency;
    return v;
}

} // namespace desk
