---
id: phillyfed
name: Federal Reserve Bank of Philadelphia
rows: [research_philly]
catalogue: ["Federal Reserve Bank of Philadelphia"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# Federal Reserve Bank of Philadelphia

The home of forecasts data: Survey of Professional Forecasters, Livingston, real-time vintages, and regional surveys.
All files, no API (`/api/` is excluded by `robots.txt` and serves the site's search).

## Contents
`/surveys-and-data` links 42 pages in 6 groups: real-time data research, regional economic analysis, macroeconomic data,
consumer finance, community development, banking. Those read, with their files, are in
[`catalog/phillyfed.tsv`](catalog/phillyfed.tsv).

**SPF** (quarterly; files under
`/-/media/FRBP/Assets/Surveys-And-Data/survey-of-professional-forecasters/data-files/files/`):
- 39 variables on `/surveys-and-data/data-files`, each with its own page: `NGDP, PGDP, CPROF, UNEMP, EMP, INDPROD, HOUSING,
  TBILL, BOND, BAABOND, TBOND, RGDP, RCONSUM, RNRESIN, RRESINV, RFEDGOV, RSLGOV, RCBI, REXPORT, CPI, CORECPI, PCE, COREPCE, CPI5YR,
  PCE5YR, CPI10, PCE10, RGDP10, PROD10, STOCK10, BOND10, BILL10, UBAR`, probabilities `PRGDP, PRPGDP, PRCCPI, PRCPCE, PRUNEMP, RECESS`.
- By variable: `Individual_<V>.xlsx` (columns `YEAR, QUARTER, ID, INDUSTRY, <V>1 ... <V>6, <V>A, <V>B, <V>C`; `Individual_CPI` has 9,281
  rows), `Mean_<V>_Level.xlsx`, `Median_<V>_Level.xlsx` (232 quarterly rows for CPI). `Dispersion_1.xlsx`: 48 sheets, one per
  variable or spread, `P25`, `P75`, `D1` by horizon T to T+4.
- Also: `prob.xlsx`, `Inflation.xlsx`, `Additional-CPIE10.xlsx`, `anxious_index_chart.xlsx`, `annual-historical-values-by-survey-date-selected-variables.xlsx`.

**Livingston** (twice a year): `individualdata.xlsx`, `MeanGrowthRate.xlsx`, `MedianGrowthRate.xlsx`, `Dispersion1-3.xlsx`,
`SPI_Means.xls`, `SPI_Medians.xls`, `SPI_Raw.xls`, 12 files in all.

**Real-time data**: Real-Time Data Set for Macroeconomists (no direct file links on the two pages read);
`first-second-third` page, 59 files (`employ_level_`, `hstarts_`, `ipm_`, ...); `GDPplus_Vintages.xlsx`; ADS index,
`ADS_Index_Most_Current_Vintage.xlsx`, `ADS_All_Vintages-zip.zip`; `ATSIX_Vintages.xlsx` (term structure of inflation
expectations); Tealbook data sets (no direct file links on the page); `MPR-Projections-All.zip` (monetary policy report).

**Regional**: MBOS `bos_history.csv`, `bos_dif.csv` (and `.txt`, `.xls` twins); NBOS `nboshistory.xlsx`;
`coincident-revised.xls` and `state-business-cycle-dates.xlsx` (state coincident indexes); `pies-data.xlsx` (Price and
Inflation Expectations Survey); `early-benchmark-2026-q1-data.xlsx`; Third District history, 32 files by metro area.

**Other**: `monetary-policy-benchmarks-data.xlsx`, `e2e-2026-09.xlsx` (job-to-job transitions), large bank credit card and
mortgage CSVs named by quarter (`26Q2-CreditCardBalances.csv`, ...), `LIFEDataDownload_2026_7.csv`, HMDA lender files.

## How to call it
`GET https://www.philadelphiafed.org/-/media/FRBP/Assets/Surveys-And-Data/survey-of-professional-forecasters/data-files/files/Individual_CPI.xlsx`.
No key, no rate limit met. `robots.txt` excludes `/api/` and search. Files whose names hold a date change each release:
read the page first.

## What the app reads
`research_philly` (Desk › Forecasts): `Individual_CPI.xlsx`, `Individual_RGDP.xlsx`, `Individual_UNEMP.xlsx`, keeps the latest
survey's spread between forecasters. `src/DeskViewsMacro.cpp:1321`. The row's probe calls `/surveys-and-data/cpi-spf`
(`src/SourcesPage.cpp:653`).

## Not read yet
- `Mean_<V>_Level`, `Median_<V>_Level`: the consensus level itself for all variables, and `RECESS` / `prob.xlsx`, the odds of a
  recession. Desk › Forecasts.
- `Inflation.xlsx`, `ATSIX_Vintages.xlsx`: long-term inflation expectations.
- `ADS_Index_Most_Current_Vintage.xlsx`: daily business conditions, for Economy.
- `bos_history.csv`: Philly Fed manufacturing index with its components, for Economy.
- `coincident-revised.xls`: an activity index for each US state.

## Calls made
- `GET https://www.philadelphiafed.org/robots.txt` → 200, 190 bytes
- `GET https://www.philadelphiafed.org/surveys-and-data` → 200, 45,324 bytes
- `GET https://www.philadelphiafed.org/surveys-and-data/real-time-data-research/survey-of-professional-forecasters` → 200, 74,707 bytes
- `GET https://www.philadelphiafed.org/surveys-and-data/real-time-data-research/spf-q4-2025` → 200, 53,106 bytes
- `GET https://www.philadelphiafed.org/surveys-and-data/cpi-spf` → 200, 24,141 bytes
- `GET https://www.philadelphiafed.org/surveys-and-data/real-time-data-research/livingston-survey` → 200, 48,210 bytes
- `GET https://www.philadelphiafed.org/surveys-and-data/real-time-data-research` → 200, 26,960 bytes
- `GET https://www.philadelphiafed.org/surveys-and-data/regional-economic-analysis` → 200, 29,622 bytes
- `GET https://www.philadelphiafed.org/surveys-and-data/regional-economic-analysis/manufacturing-business-outlook-survey` → 200, 279,134 bytes ×2
- `GET https://www.philadelphiafed.org/surveys-and-data/regional-economic-analysis/nonmanufacturing-business-outlook-survey` → 200, 154,660 bytes ×2
- `GET https://www.philadelphiafed.org/surveys-and-data/real-time-data-research/ads` → 200, 851,264 bytes ×2
- `GET https://www.philadelphiafed.org/surveys-and-data/real-time-data-research/real-time-data-set-for-macroeconomists` → 200, 35,913 bytes
- `GET https://www.philadelphiafed.org/-/media/FRBP/Assets/Surveys-And-Data/survey-of-professional-forecasters/data-files/files/Individual_CPI.xlsx` → 200, 458,695 bytes
- `GET https://www.philadelphiafed.org/-/media/FRBP/Assets/Surveys-And-Data/survey-of-professional-forecasters/data-files/files/Mean_CPI_Level.xlsx` → 200, 19,000 bytes
- `GET https://www.philadelphiafed.org/-/media/FRBP/Assets/Surveys-And-Data/survey-of-professional-forecasters/data-files/files/Median_RGDP_Level.xlsx` → 200, 23,605 bytes
- `GET https://www.philadelphiafed.org/-/media/FRBP/Assets/Surveys-And-Data/survey-of-professional-forecasters/data-files/files/Dispersion_1.xlsx` → 200, 669,021 bytes
- `GET https://www.philadelphiafed.org/surveys-and-data/data-files` → 200, 31,613 bytes
- `GET https://www.philadelphiafed.org/surveys-and-data/real-time-data-research/probability-variables` → 200, 28,495 bytes
- `GET https://www.philadelphiafed.org/surveys-and-data/real-time-data-research/inflation-forecasts` → 200, 29,075 bytes
- `GET https://www.philadelphiafed.org/surveys-and-data/real-time-data-research/gdpplus` → 200, 55,718 bytes
- `GET https://www.philadelphiafed.org/surveys-and-data/real-time-data-research/atsix` → 200, 36,868 bytes
- `GET https://www.philadelphiafed.org/surveys-and-data/real-time-data-research/greenbook` → 200, 35,067 bytes
- `GET https://www.philadelphiafed.org/surveys-and-data/real-time-data-research/livingston-historical-data` → 200, 31,859 bytes
- `GET https://www.philadelphiafed.org/surveys-and-data/real-time-data-research/real-time-data-set-full-time-series-history` → 200, 44,499 bytes
- `GET https://www.philadelphiafed.org/surveys-and-data/real-time-data-research/anxious-index` → 200, 29,330 bytes
- `GET https://www.philadelphiafed.org/surveys-and-data/real-time-data-research/first-second-third` → 200, 41,106 bytes
- `GET https://www.philadelphiafed.org/surveys-and-data/real-time-data-research/monetary-policy-projections` → 200, 35,797 bytes
- `GET https://www.philadelphiafed.org/surveys-and-data/regional-economic-analysis/state-coincident-indexes` → 200, 52,254 bytes
- `GET https://www.philadelphiafed.org/surveys-and-data/regional-economic-analysis/price-and-inflation-expectations-survey` → 200, 141,510 bytes
- `GET https://www.philadelphiafed.org/surveys-and-data/regional-economic-analysis/third-district-historical-data` → 200, 42,031 bytes
- `GET https://www.philadelphiafed.org/surveys-and-data/regional-economic-analysis/early-benchmark-revisions` → 200, 40,689 bytes
- `GET https://www.philadelphiafed.org/surveys-and-data/macroeconomic-data/monetary-policy-benchmarks` → 200, 136,693 bytes
- `GET https://www.philadelphiafed.org/surveys-and-data/macroeconomic-data/prism-ii` → 200, 35,518 bytes
- `GET https://www.philadelphiafed.org/surveys-and-data/macroeconomic-data/employer-to-employer-transition-probability` → 200, 61,489 bytes
- `GET https://www.philadelphiafed.org/surveys-and-data/large-bank-credit-card-and-mortgage-data` → 200, 1,066,536 bytes
- `GET https://www.philadelphiafed.org/surveys-and-data/consumer-finance-data/life-survey` → 200, 43,807 bytes
- `GET https://www.philadelphiafed.org/surveys-and-data/consumer-finance-data/home-mortgage-disclosure-act-lender-file` → 200, 42,712 bytes
