---
id: fed-regional
name: Regional Federal Reserve Banks (Dallas, Chicago, Kansas City, Richmond, Cleveland, Atlanta, San Francisco)
rows: []
catalogue: ["Federal Reserve Bank of Dallas", "Federal Reserve Bank of Chicago", "Federal Reserve Bank of Kansas City", "Federal Reserve Bank of Richmond", "Federal Reserve Bank of Cleveland", "Federal Reserve Bank of Atlanta", "Federal Reserve Bank of San Francisco"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# Regional Federal Reserve Banks

Seven Banks, each publishing a small shelf of indicators as files on web pages, with no API. Grouped on one card: per
Bank it is a list of pages, each with one or two workbooks. New York, Philadelphia and St. Louis have their own cards;
Boston and Minneapolis were not asked for and not probed.

## Contents
[`catalog/fed-regional.tsv`](catalog/fed-regional.tsv): 108 pages (bank, path, status, title, the data files they link).
Some pages show no file link in their HTML, Chicago's `current-data` pages and Cleveland's Inflation Nowcasting and Yield
Curve among them: their files were not found.

**Dallas** (`dallasfed.org`): Trimmed Mean PCE (`pcedata.xlsx`, `tmrates.txt`), Weekly Economic Index (`weekly-economic-index.xlsx`),
Index of Global Real Economic Activity, Database of Global Economic Indicators (DGEI), International House Price Database
(118 linked vintages `hpYYQQ.xlsx`), Real-Time OECD data, Market Value of Government Debt; Texas Manufacturing, Service
Sector, Energy, Banking Conditions, Agricultural surveys (`/data` pages); Texas: Leading Index, employment, unemployment,
real GDP, retail, dollar; Energy Indicators.

**Chicago** (`chicagofed.org`): CFNAI, NFCI, CARTS (Advance Retail Trade Summary), CFSEC (Survey of Economic Conditions), Labor
Market Indicators, MuniTrack, Ag Conditions (`land-values-7th-district-xls.xls`, `credit-conditions-7th-district-xls.xls`),
Beige Book, release calendar.

**Kansas City** (`kansascityfed.org`): Financial Stress Index (`kcresearch-share.kansascityfed.org/kc-fsi/kcfsi.csv`: `DATE, KCFSI`,
monthly from 1990-02; `kcfsi_contributions.csv`, `KCFSI_History.csv`), Labor Market Conditions (`lmcicharts-*.xlsx`, dated name),
Divisional LMCI, Commercial Real Estate Index, Model-Based Natural Rate, Policy Rate Skew, Policy Rate Uncertainty, Risk-On
Risk-Off Index, Databooks. Surveys: `/surveys/manufacturing-survey/`, `services-survey`, `energy-survey`, `small-business-lending-survey`.

**Richmond** (`richmondfed.org`): CFO Survey, Natural Rate of Interest (`lubik_matthes_natural_rate_interest.xlsx`: `Date, Lower Bound,
Median, Upper Bound`), Non-Employment Index, Partisan Conflict Index (`partisan_conflict_index.csv`), SOS Recession Indicator,
Manufacturing and Service surveys (`mfg_historicaldata.xlsx`, `nmf_historicaldata.xlsx`). `robots.txt` excludes `/surveys/` and any
URL with a `?`.

**Cleveland** (`clevelandfed.org`): Median CPI (`mcpi_revised.csv`, `cpi.csv`, `core.csv`, `trim_revised.csv`, `mediancpireleasedata.zip`,
`mediancpi_component_table.csv`: `Component, 1-Month Annualized Percent Change, Relative Importance (Normalized), Cumulative
Relative Importance`, 46 rows), Median PCE, Inflation Expectations (`inflation-expectations.xlsx`, 4 sheets: Expected Inflation, Real
Interest Rate, ...), Inflation Nowcasting, Simple Monetary Policy Rules, SoFIE, SORCE, BOTS, Systemic Risk Indicator, Yield Curve and
GDP Growth, Credit Easing.

**Atlanta** (`atlantafed.org`): 26 entries on `/research-and-data/data`.
- GDPNow, `GDPTrackingModelDataAndForecasts.xlsx`, 56 sheets (`Factor`, `RLSweights`, `QtrlyBVARForecasts`, `BridgeEqnCoeffs`,
  `MonthlyLevels`, ...), and `GDPNowcastDataReleaseDates.xlsx`.
- Wage Growth Tracker, `wage-growth-data.xlsx`, 20 sheets (`data_overall`, `Race`, `Education`, `Age`, `Sex`, `Occupation`, `Industry`,
  `Census Divisions`, `Full-Time or Part-Time`, `Job Switcher`, ...).
- Sticky-Price CPI, `sticky-price-data.xlsx`, 4 sheets.
- Market Probability Tracker, `mpt_histdata.xlsx`, sheets `LICENSE`, `DICTIONARY`, `DATA`: rate percentiles, mean, mode, `Prob: cut`,
  `Prob: hike`, probability by 25 bp bucket. Carries a licence sheet: read it before redistributing.
- Taylor Rule Utility, Wu-Xia Shadow Rate, Underlying Inflation Dashboard, GDP-Based Recession Indicator, Deflation
  Probabilities (archived), Jobs Calculator, Labor Force Participation Dynamics, Labor Report First Look, Unemployment Claims
  Monitor, CRE Market Index, Home Ownership Affordability Monitor, China's Macroeconomy; Business Inflation Expectations
  (`bie-data.xlsx`), Survey of Business Uncertainty.

**San Francisco** (`frbsf.org`): 25 indicators. Daily News Sentiment (`news_sentiment_data.xlsx`, sheets `Methodology`, `Data`),
Cyclical and Acyclical Core PCE, Supply- and Demand-Driven PCE, Proxy Funds Rate, Treasury Yield Premiums, Treasury Yield Skewness,
Interest Rate Probability Distributions, Market-Based Monetary Policy Uncertainty, Monetary Policy Surprises, US Monetary Policy
Event-Study Database, Labor Market Stress Indicator, TFP, China Cyclical Activity Tracker, Weather-Adjusted Employment, Inflation
Shock Momentum, Zero Lower Bound Probabilities, Regional Indicators, Twelfth District Business Sentiment.

## How to call it
Plain GETs of the files. No key. Robots read for each Bank before its pages; Kansas City's `robots.txt` did not answer.
File names with a date or vintage change at each release, so read the page first.
Example: `GET https://kcresearch-share.kansascityfed.org/kc-fsi/kcfsi.csv`.
Traps: Dallas's WEI link has no leading slash in its markup and needs care to resolve; Kansas City's surveys are under `/surveys/`
(`/data-and-trends/*-survey/` and `/surveys/agricultural-credit-survey/` answer 404).

## What the app reads
Nothing directly. FRED carries some of these: the app reads `CFNAI`, `NFCI` and `GDPNOW`, `WEI`, `BACTSAMFRBDAL` through FRED
(`resources/country_indicators.json:91`, `:95`; `src/EconomyData.cpp`).

## Not read yet
- Atlanta Market Probability Tracker: market odds of Fed moves for Desk › Rates (licence sheet first).
- Cleveland Inflation Expectations and Median CPI, Dallas Trimmed Mean PCE, Atlanta Sticky-Price: underlying inflation for Economy.
- Atlanta GDPNow components: the nowcast's parts, where the app has only the headline.
- SF Daily News Sentiment, KC Stress Index, Richmond Natural Rate.
- Dallas International House Price Database: for Countries.
- Dallas Energy Survey (breakeven prices): for Map › Supply chains.

## Calls made
- `GET https://www.dallasfed.org/robots.txt` → 200, 177 bytes
- `GET https://www.dallasfed.org/research` → 200, 65,076 bytes
- `GET https://www.dallasfed.org/research/surveys` → 200, 37,538 bytes
- `GET https://www.dallasfed.org/research/pce` → 200, 43,216 bytes
- `GET https://www.dallasfed.org/research/wei` → 200, 35,968 bytes
- `GET https://www.dallasfed.org/research/surveys/tmos` → 200, 42,396 bytes
- `GET https://www.dallasfed.org/research/surveys/des` → 200, 43,355 bytes
- `GET https://www.dallasfed.org/institute/houseprice` → 200, 96,377 bytes
- `GET https://www.dallasfed.org/research/econdata` → 200, 42,156 bytes
- `GET https://www.chicagofed.org/robots.txt` → 200, 3 bytes
- `GET https://www.chicagofed.org/research/data/index` → 200, 87,192 bytes
- `GET https://www.chicagofed.org/research/data/cfnai/current-data` → 200, 76,479 bytes
- `GET https://www.chicagofed.org/research/data/nfci/current-data` → 200, 80,121 bytes
- `GET https://www.chicagofed.org/research/data/carts/current-data` → 200, 72,664 bytes
- `GET https://www.chicagofed.org/research/data/cfsbc/current-data` → 404, 50,713 bytes
- `GET https://www.kansascityfed.org/robots.txt` → ERR (TimeoutError)
- `GET https://www.kansascityfed.org/data-and-trends/` → 200, 605,433 bytes
- `GET https://www.kansascityfed.org/data-and-trends/kansas-city-financial-stress-index/` → 200, 619,782 bytes
- `GET https://www.kansascityfed.org/data-and-trends/labor-market-conditions-indicators/` → 200, 667,516 bytes
- `GET https://www.kansascityfed.org/data-and-trends/manufacturing-survey/` → 404, 200,000 bytes
- `GET https://www.kansascityfed.org/data-and-trends/energy-survey/` → 404, 200,000 bytes
- `GET https://www.kansascityfed.org/data-and-trends/services-survey/` → 404, 200,000 bytes
- `GET https://www.richmondfed.org/robots.txt` → 200, 661 bytes
- `GET https://www.richmondfed.org/research` → 200, 97,546 bytes
- `GET https://www.richmondfed.org/research/data_and_analysis` → 404, 56,933 bytes
- `GET https://www.richmondfed.org/region_communities/regional_data_analysis/surveys/manufacturing` → 200, 110,804 bytes
- `GET https://www.richmondfed.org/research/national_economy/cfo_survey` → 200, 27,092 bytes
- `GET https://www.richmondfed.org/research/national_economy` → 200, 76,281 bytes
- `GET https://www.clevelandfed.org/robots.txt` → 200, 87 bytes
- `GET https://www.clevelandfed.org/indicators-and-data` → 200, 143,956 bytes
- `GET https://www.clevelandfed.org/indicators-and-data/inflation-nowcasting` → 200, 197,601 bytes
- `GET https://www.clevelandfed.org/indicators-and-data/median-cpi` → 200, 162,202 bytes
- `GET https://www.clevelandfed.org/indicators-and-data/inflation-expectations` → 200, 189,714 bytes
- `GET https://www.clevelandfed.org/indicators-and-data/trimmed-mean-pce` → 404, 123,140 bytes
- `GET https://www.atlantafed.org/robots.txt` → 200, 284 bytes
- `GET https://www.atlantafed.org/research-and-data` → 200, 187,826 bytes
- `GET https://www.atlantafed.org/research-and-data/data` → 200, 156,651 bytes
- `GET https://www.atlantafed.org/cqer/research/gdpnow` → 200, 2,403,747 bytes
- `GET https://www.atlantafed.org/chcs/wage-growth-tracker` → 200, 1,338,316 bytes
- `GET https://www.atlantafed.org/research/inflationproject/stickyprice` → 200, 1,120,137 bytes
- `GET https://www.atlantafed.org/research/inflationproject/bie` → 200, 293,705 bytes
- `GET https://www.frbsf.org/robots.txt` → 200, 117 bytes
- `GET https://www.frbsf.org/research-and-insights/data-and-indicators/` → 200, 161,264 bytes
- `GET https://www.frbsf.org/research-and-insights/data-and-indicators/daily-news-sentiment-index/` → 200, 112,715 bytes
- `GET https://www.frbsf.org/economic-research/indicators-data/` → 200, 161,264 bytes
- `GET https://www.atlantafed.org/-/media/Project/Atlanta/FRBA/Documents/research-and-data/data/gdpnow/GDPTrackingModelDataAndForecasts.xlsx` → 200, 10,982,878 bytes
- `GET https://www.atlantafed.org/-/media/Project/Atlanta/FRBA/Documents/research-and-data/data/wage-growth-tracker/wage-growth-data.xlsx` → 200, 339,572 bytes
- `GET https://www.atlantafed.org/-/media/Project/Atlanta/FRBA/Documents/research-and-data/data/sticky-price/sticky-price-data.xlsx` → 200, 309,428 bytes
- `GET https://www.atlantafed.org/-/media/Project/Atlanta/FRBA/Documents/research-and-data/data/market-probability-tracker/mpt_histdata.xlsx` → 200, 7,199,234 bytes
- `GET https://www.clevelandfed.org/-/media/files/webcharts/inflationexpectations/inflation-expectations.xlsx` → 200, 187,240 bytes
- `GET https://www.clevelandfed.org/-/media/files/webcharts/mediancpi/mediancpi_component_table.csv` → 200, 1,947 bytes
- `GET https://www.dallasfed.org-/media/documents/research/wei/weekly-economic-index.xlsx` → ERR (URLError)
- `GET https://www.dallasfed.org/-/media/documents/research/pce/pcedata.xlsx` → 200, 19,870 bytes
- `GET https://kcresearch-share.kansascityfed.org/kc-fsi/kcfsi.csv` → 200, 13,027 bytes
- `GET https://www.frbsf.org/wp-content/uploads/news_sentiment_data.xlsx` → 200, 419,150 bytes
- `GET https://www.richmondfed.org/-/media/RichmondFedOrg/research/economists/bios/data/lubik_matthes_natural_rate_interest.xlsx` → 200, 41,076 bytes
- `GET https://www.dallasfed.org/<pages listed in catalog/fed-regional.tsv, with their status>` → 200 ×27
- `GET https://www.chicagofed.org/<pages listed in catalog/fed-regional.tsv, with their status>` → 200 ×10
- `GET https://www.kansascityfed.org/<pages listed in catalog/fed-regional.tsv, with their status>` → timed out ×1 (its robots.txt)
- `GET https://www.kansascityfed.org/<pages listed in catalog/fed-regional.tsv, with their status>` → 200 ×14
- `GET https://www.kansascityfed.org/<pages listed in catalog/fed-regional.tsv, with their status>` → 404 ×1
- `GET https://www.richmondfed.org/<pages listed in catalog/fed-regional.tsv, with their status>` → 200 ×11
- `GET https://www.clevelandfed.org/<pages listed in catalog/fed-regional.tsv, with their status>` → 200 ×12
- `GET https://www.atlantafed.org/<pages listed in catalog/fed-regional.tsv, with their status>` → 200 ×20
- `GET https://www.frbsf.org/<pages listed in catalog/fed-regional.tsv, with their status>` → 200 ×19
