#pragma once

#include <QString>
#include <QStringList>

// Reading a headline: its topics and how much it can move markets, by keywords. It reads words, not meaning, and the
// News page says so.
namespace desk {

enum class NewsImpact { Normal, Important, High };

struct NewsTags {
    NewsImpact impact = NewsImpact::Normal;
    QStringList topics;   // "central_bank", "inflation", "employment", "gdp", "pmi", "retail", "trade", "earnings"
};
NewsTags tagHeadline(const QString &title);

} // namespace desk
