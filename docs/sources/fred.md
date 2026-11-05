---
id: fred
name: FRED, Federal Reserve Bank of St. Louis
rows: [fred, research_fred, fred_freight]
catalogue: ["FRED", "Federal Reserve Bank of St. Louis"]
access: free key
probed: 2026-11-05
confidence: docs                      # no key in this inventory; API refused without one. Lists from FRED's public pages, API from its documentation
---

# FRED, Federal Reserve Bank of St. Louis

An aggregator: US and international economic series from many publishers, 853,320 series at the probe. For most US
series in the app this is the door, whichever body produces them.

## Contents
Lists read from the public pages (the API's `fred/releases`, `fred/sources`, `fred/category/children` need a key):
- [`catalog/fred_releases.tsv`](catalog/fred_releases.tsv): 336 releases (id, name).
- [`catalog/fred_sources.tsv`](catalog/fred_sources.tsv): 127 sources (id, name).
- [`catalog/fred_categories.tsv`](catalog/fred_categories.tsv): 4,375 categories (id, parent, depth, name, series count as the page
  shows it, whether the page was opened). Breadth first, stopped at 1,000 pages opened: the other 3,375 categories are listed but their children are not (mostly States, Indicators, Census Regions), so the tree is complete to depth 4 and partial below.

Top of the tree, rounded series counts as on `/categories`:
- **Money, Banking, & Finance** (20,000+): Interest Rates (970+), Exchange Rates (100+), Monetary Data (990+), Financial Indicators
  (11,000+), Banking (4,100+), Business Lending (1,900+), Foreign Exchange Intervention.
- **Population, Employment, & Labor Markets** (49,000+): Current Population Survey (6,900+), Current Employment Statistics (480+), ADP
  (10+), Education (4,600+), Income Distribution (290+), JOLTS (320+), Labor Market Conditions (390+), Population (11,000+),
  Productivity & Costs (20,000+), Minimum Wage, Weekly Initial Claims, Tax Data (1,400+).
- **National Accounts** (61,000+): NIPA (13,000+), Flow of Funds (43,000+), US Trade & International Transactions (4,600+).
- **Production & Business Activity** (87,000+): Business Cycle Expansions & Contractions (240+), Business Surveys (250+), Construction, ...
- **Prices**, **International Data**, **U.S. Regional Data**, **Academic Data**: see the file.

Examples of leaf counts one level down: Corporate Bonds 362, Commercial Paper 84, Factors Affecting Reserve Balances 655, By Country
(exchange rates) 81, Trade-Weighted Indexes 79, M1 and Components 72.

Series metadata (documentation, `fred/series`): `id, realtime_start, realtime_end, title, observation_start, observation_end, frequency,
frequency_short, units, units_short, seasonal_adjustment, seasonal_adjustment_short, last_updated, popularity, notes`. `fred/tags` filters
by `tag_group_id`. ALFRED keeps vintages (`realtime_start`, `vintage_dates`).

API endpoints, documentation, 31 in all: `fred/category` + `children, related, series, tags, related_tags`; `fred/releases`, `releases/dates`;
`fred/release` + `dates, series, sources, tags, related_tags, tables`; `fred/series` + `categories, observations, release, search, search/tags,
search/related_tags, tags, updates, vintagedates`; `fred/sources`, `fred/source`, `source/releases`; `fred/tags`, `related_tags`, `tags/series`.
A separate Maps API (`geofred`): shapes and regional data.

## How to call it
- API: `https://api.stlouisfed.org/fred/series/observations?series_id=UNRATE&api_key=<key>&file_type=json&observation_start=2020-01-01`;
  parameters also `units`, `frequency`, `aggregation_method`, `output_type`, `vintage_dates`, `sort_order`, `limit`, `offset`.
- Without a key: 400 `Variable api_key is not set` on all paths.
- Public CSV: `https://fred.stlouisfed.org/graph/fredgraph.csv?id=UNRATE&cosd=2026-01-01`.
- **This CSV answered 200 today, to `Deepwater/1.0` and to the app's own `Deepwater/0.1 (desktop app; research page)`.** The comment
  at `src/ResearchData.cpp:1184` ("FRED's public CSV turns away requests that don't come from a browser") did not hold on 2026-11-05.
  Refusal may vary by day or address, so one test is not a verdict.
- `fred.stlouisfed.org/robots.txt` timed out twice (60 s and 40 s) while all other pages answered: its rules could not be read. Hence one page
  every 2 s, no series pages crawled, and stopping at 1,000 category pages.

## What the app reads

Checked on 5 November 2026: the public CSV timed out three times from this machine, so the comment in `src/ResearchData.cpp` stands. The 200 seen earlier does not make it dependable.
- `fred` (Map): `PCEPI`, `HOUST`, `ICSA`, `BOPGSTB`, `UMCSENT`, keyed. `src/KeyedSources.cpp:91`.
- `fred_freight` (Map › Supply chains): `TRUCKD11`, `FRGSHPUSM649NCIS`, `TSIFRGHT`, `PCU483111483111`. `src/SupplyChain.cpp:665`, `:684`.
- `research_fred`: `Client::fredSeries`, API with the key, else CSV, `src/ResearchData.cpp:1156`. 50 series in Economy (`src/EconomyData.cpp:424`:
  `DGS10`, `T10Y2Y`, `VIXCLS`, `BAMLH0A0HYM2`, `CPIAUCSL`, `WALCL`, `NFCI`, ...), 180 in Countries › United States (`resources/country_indicators.json`,
  `src/CountryData.cpp:1218`).
- Desk › Forecasts: the SEP medians `FEDTARMD`, `GDPC1MD`, `UNRATEMD`, `PCECTPIMD`. `src/DeskViewsMacro.cpp:1360`.

Keeps `date` and `value`; `.` means missing (`src/KeyedSources.cpp:106`).

## Not read yet
- Vintages (`realtime_start`, `fred/series/vintagedates`): what was known at the time, for ML features with no look-ahead.
- `fred/series/updates`: what changed today, a cheaper refresh than one call per series.
- `fred/release/dates`: a release calendar for Calendar, from a single source.
- `fred/series/search`, `fred/tags/series`: browsing by country and topic in Economy.
- Maps API: US states and counties for a map layer.

## Calls made
- `GET https://fred.stlouisfed.org/robots.txt` → ERR (TimeoutError) ×2
- `GET https://api.stlouisfed.org/fred/category?category_id=0&file_type=json` → 400, 162 bytes
- `GET https://fred.stlouisfed.org/categories` → 200, 47,602 bytes
- `GET https://fred.stlouisfed.org/releases` → 200, 46,025 bytes
- `GET https://fred.stlouisfed.org/sources` → 200, 42,130 bytes
- `GET https://fred.stlouisfed.org/tags/series` → 200, 117,987 bytes
- `GET https://fred.stlouisfed.org/docs/api/fred/` → 200, 37,248 bytes
- `GET https://fred.stlouisfed.org/graph/fredgraph.csv?id=UNRATE&cosd=2026-01-01` → 200, 159 bytes ×2
- `GET https://fred.stlouisfed.org/categories/32991` → 200, 50,718 bytes
- `GET https://fred.stlouisfed.org/categories/22` → 200, 74,799 bytes
- `GET https://fred.stlouisfed.org/series/UNRATE` → 200, 94,165 bytes
- `GET https://fred.stlouisfed.org/docs/api/fred/series.html` → 200, 34,333 bytes
- `GET https://fred.stlouisfed.org/docs/api/fred/series_observations.html` → 200, 66,475 bytes
- `GET https://fred.stlouisfed.org/docs/api/fred/overview.html` → 200, 31,927 bytes
- `GET https://fred.stlouisfed.org/docs/api/terms_of_use.html` → 200, 49,491 bytes
- `GET https://fred.stlouisfed.org/docs/api/fred/tags.html` → 200, 43,020 bytes
- `GET https://fred.stlouisfed.org/releases` → 200 ×2
- `GET https://fred.stlouisfed.org/releases?pageID=2` → 200 ×2
- `GET https://fred.stlouisfed.org/releases?pageID=3` → 200 ×2
- `GET https://fred.stlouisfed.org/releases?pageID=4` → 200 ×2
- `GET https://fred.stlouisfed.org/releases?pageID=5` → 200 ×2
- `GET https://fred.stlouisfed.org/releases?pageID=6` → 200 ×2
- `GET https://fred.stlouisfed.org/releases?pageID=7` → 200 ×2
- `GET https://fred.stlouisfed.org/sources` → 200 ×2
- `GET https://fred.stlouisfed.org/sources?pageID=2` → 200 ×2
- `GET https://fred.stlouisfed.org/sources?pageID=3` → 200 ×2
- `GET https://fred.stlouisfed.org/categories` → 200 ×2
- `GET https://fred.stlouisfed.org/categories/<id>` → 200 ×1071
