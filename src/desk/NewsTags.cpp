#include "desk/NewsTags.h"

#include <QRegularExpression>

namespace desk {

namespace {

bool matches(const QString &text, const char *pattern)
{
    return QRegularExpression(QString("\\b(?:%1)\\b").arg(QLatin1String(pattern)), QRegularExpression::CaseInsensitiveOption).match(text).hasMatch();
}

} // namespace

NewsTags tagHeadline(const QString &title)
{
    NewsTags t;
    const struct { const char *topic, *pattern; } topics[] = {
        {"central_bank", "fed|fomc|federal reserve|ecb|boe|boj|snb|rba|rbnz|boc|central bank|powell|lagarde|bailey|ueda|rate (?:cut|hike|decision)s?|interest rates?"},
        {"inflation", "inflation|cpi|ppi|pce|consumer prices?|producer prices?|disinflation|deflation"},
        {"employment", "jobs?|payrolls?|nfp|unemployment|jobless|employment|labou?r market|wages?|layoffs?"},
        {"gdp", "gdp|gross domestic product|recession|economic growth"},
        {"pmi", "pmi|purchasing managers|ism manufacturing|ism services"},
        {"retail", "retail sales|consumer spending|consumer confidence|consumer sentiment"},
        {"trade", "tariffs?|trade (?:war|deal|deficit|surplus|balance)|exports?|imports?|sanctions?"},
        {"earnings", "earnings|revenue|quarterly results?|profit (?:warning|beat|miss)|eps|guidance|q[1-4] (?:results?|profit)"},
    };
    for (const auto &tp : topics)
        if (matches(title, tp.pattern))
            t.topics << tp.topic;

    // Market-wide events beat a company's news.
    if (matches(title, "fomc|rate (?:cut|hike|decision)s?|nfp|nonfarm|cpi|inflation|recession|default|war|invasion|missile|strikes?|crash|emergency|bank failure|sanctions?|tariffs?|opec"))
        t.impact = NewsImpact::High;
    else if (!t.topics.isEmpty() && !(t.topics.size() == 1 && t.topics.first() == "earnings"))
        t.impact = NewsImpact::Important;
    else if (matches(title, "fed|treasury|yields?|oil|gold|dollar|euro|yen|sterling|bitcoin|s&p|nasdaq|dow"))
        t.impact = NewsImpact::Important;
    return t;
}

} // namespace desk
