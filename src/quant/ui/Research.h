#pragma once

// Port of public/js/research.js: News (side tab), Seasonals and Financials (bottom tabs) for the
// active chart's symbol. `el` is the tab body the JS writes into.

#include <QString>

namespace qtm {

class App;
namespace panels { class Body; }

namespace research {

void renderNews(panels::Body *el, App *app);
void renderSeasonals(panels::Body *el, App *app, int years = 6);
void renderFinancials(panels::Body *el, App *app, const QString &freq = "annual", const QString &tab = "income");

} // namespace research
} // namespace qtm
