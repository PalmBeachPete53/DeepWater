---
id: nyfed
name: Federal Reserve Bank of New York
rows: [nyfed_gscpi]
catalogue: ["Federal Reserve Bank of New York"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# Federal Reserve Bank of New York

Money market reference rates, the Fed's open market operations and holdings, primary dealers (a real JSON API), plus
research series published as files: supply chain pressure, household debt, consumer expectations, r-star.

## Contents
**Markets Data API** (`markets.newyorkfed.org`, OpenAPI at `/static/docs/markets-api.yml`): 54 paths in 10 tags.
| Family | Paths | What |
|---|---|---|
| `rates` | 8 | Types `SOFR`, `BGCR`, `TGCR` (secured), `EFFR`, `OBFR` (unsecured), `SOFRAI`. Fields `effectiveDate, type, percentRate, percentPercentile1/25/75/99, volumeInBillions, targetRateFrom, targetRateTo, average30day, average90day, average180day, index, revisionIndicator`. EFFR from 2000-07-03, SOFR from 2018-04-02, daily. |
| `soma` | 13 | Weekly holdings of the Fed's portfolio, 1,217 dates from 2003-07-09. `summary`: `asOfDate, bills, notesbonds, tips, tipsInflationCompensation, frn, mbs, agencies, cmbs, total`. By security (`tsy/get/asof/{date}`, `agency/get/asof/{date}`, `.../cusip/{cusip}`): `asOfDate, cusip, securityType, issuer, coupon, spread, maturityDate, parValue, inflationCompensation, percentOutstanding, changeFromPriorWeek, changeFromPriorYear`. Security types `NotesBonds, Bills, TIPS, FRNs, MBS, CMBS, Agency Debts`. |
| `pd` | 8 | Primary dealer statistics (FR 2004). 1,539 time series, full list in [`catalog/nyfed_pd_timeseries.tsv`](catalog/nyfed_pd_timeseries.tsv) (key id, description); families `PDSOR`, `PDSIR` (234 each), `PDMEF` (176), `PDPOS` (90), `PDCAF`, `PDCBF` (88 each), ... Weekly, 1,973 dates, 1998-01-28 on, with 6 series breaks (`SBP2001`, `SBP2013`, `SBN2013`, `SBN2015`, `SBN2022`, `SBN2024`). |
| `rp` | 5 | Repo and reverse repo operations, and RRP propositions. |
| `ambs` | 4 | Agency MBS operations. |
| `tsy` | 4 | Treasury operations: `operationId, auctionStatus, operationDate, settlementdate, maturityRangeStart, maturityRangeEnd, operationDirection, auctionMethod, totalParAmtSubmitted, totalParAmtAccepted`, details by CUSIP. |
| `seclending` | 4 | Securities lending operations. |
| `fxs` | 4 | Central bank liquidity swaps, counterparties. |
| `marketshare` | 2 | Primary dealer market share, quarterly and year to date. |
| `guidesheets` | 2 | Operation guide sheets. |

**Research files** (`www.newyorkfed.org`):
- **GSCPI**: `medialibrary/research/interactives/data/gscpi/gscpi_interactive_data.csv`. One column per vintage (`Date`, then
  `Jan-22` ... `Oct-26`, 59 columns), 358 rows, monthly from `30-Sep-1997`.
- **Household Debt and Credit**: `medialibrary/interactives/householdcredit/data/xls/HHD_C_Report_2026Q2.xlsx`, 73 sheets, one
  "Page N Data" per chart (total debt and composition, accounts by loan type, delinquency, ...), quarterly. The file name
  carries the quarter.
- **Survey of Consumer Expectations**: `medialibrary/interactives/sce/sce/downloads/data/frbny-sce-data.xlsx`, 43 sheets
  (inflation, home price, commodity, earnings, job separation, job finding, unemployment expectations, income, spending,
  taxes, ... with a `Demo` twin by demographic), plus 3 microdata files (`frbny-sce-public-microdata-*`).
- **r-star**: `Laubach_Williams_current_estimates.xlsx`, `Holston_Laubach_Williams_current_estimates.xlsx` and real-time
  versions, under `medialibrary/media/research/economists/williams/data/`.
- **Listed, not opened**: Staff Nowcast (`/research/policy/nowcast`), Yield Curve as a Leading Indicator
  (`/research/capital_markets/ycfaq`), Home Price Index, Economic Heterogeneity Indicators,
  College Labor Market, Early Benchmarked Employment, Reserve Demand Elasticity, Natural Disaster Losses, Banking Research
  Data Sets (`/research/banking_research/datasets`: CRSP-FRB link, historical bank balance sheets).

## How to call it
- Rates: `https://markets.newyorkfed.org/api/rates/all/latest.json`; history
  `.../api/rates/secured/sofr/search.json?startDate=2018-01-01&endDate=2018-06-30`.
- SOMA: `.../api/soma/summary.json`, `.../api/soma/asofdates/list.json`. Dealers: `.../api/pd/list/timeseries.json`,
  `.../api/pd/get/all/timeseries.csv` (columns `As Of Date, Time Series, Value (millions)`).
- No key. `www.newyorkfed.org/robots.txt` only excludes a few paths; `markets.newyorkfed.org/robots.txt` 404.
- Trap: `marketshare/qtrly/latest.json` is not valid JSON: a bare `*` stands as a value (`"dailyAvgVolInMillions": *`).
  A strict parser fails; the CSV form was not tried.
- Trap: Household Debt file names change each quarter; the page `/microeconomics/hhdc` does not list it in plain links.

## What the app reads
`nyfed_gscpi` (Map › Supply chains): the GSCPI CSV, last column (latest vintage) by date. `src/SupplyChain.cpp:623`.

## Not read yet
- `rates`: SOFR and EFFR with percentiles and volumes, without FRED or a key. Economy or Desk › Rates.
- `soma/summary`, `rp/reverserepo`: Fed balance sheet composition and RRP usage, weekly and daily.
- `pd`: dealer positions by Treasury maturity. A liquidity panel on Desk.
- SCE `Inflation expectations`: the 1-year and 3-year medians for Desk or Economy.
- r-star estimates beside the Fed funds rate.

## Calls made
- `GET https://www.newyorkfed.org/robots.txt` → 200, 211 bytes
- `GET https://markets.newyorkfed.org/robots.txt` → 404
- `GET https://markets.newyorkfed.org/static/docs/markets-api.yml` → 200, 143,746 bytes
- `GET https://markets.newyorkfed.org/static/docs/markets-api.html` → 200, 6,404 bytes
- `GET https://www.newyorkfed.org/data-and-statistics` → 200, 114,313 bytes
- `GET https://www.newyorkfed.org/research/policy/gscpi` → 200, 105,177 bytes
- `GET https://www.newyorkfed.org/medialibrary/research/interactives/data/gscpi/gscpi_interactive_data.csv` → 200, 118,513 bytes
- `GET https://www.newyorkfed.org/markets/data-hub` → 200, 106,049 bytes
- `GET https://www.newyorkfed.org/microeconomics/hhdc` → 200, 104,741 bytes
- `GET https://www.newyorkfed.org/microeconomics/sce` → 200, 119,158 bytes
- `GET https://www.newyorkfed.org/research/policy/rstar` → 200, 116,224 bytes
- `GET https://www.newyorkfed.org/research/policy/nowcast` → 200, 99,012 bytes
- `GET https://www.newyorkfed.org/research/capital_markets/ycfaq` → 200, 99,184 bytes
- `GET https://markets.newyorkfed.org/static/docs/markets-api.json` → 404
- `GET https://markets.newyorkfed.org/api/rates/all/latest.json` → 200, 1,427 bytes ×2
- `GET https://markets.newyorkfed.org/api/pd/list/timeseries.json` → 200, 335,616 bytes
- `GET https://markets.newyorkfed.org/api/pd/list/seriesbreaks.json` → 200, 716 bytes
- `GET https://markets.newyorkfed.org/api/pd/list/asof.json` → 200, 102,624 bytes
- `GET https://markets.newyorkfed.org/api/rates/secured/sofr/search.json?startDate=1990-01-01&endDate=2000-01-01` → 200, 19 bytes
- `GET https://markets.newyorkfed.org/api/rates/secured/sofr/search.json?startDate=2000-01-01&endDate=2018-06-30` → 200, 15,186 bytes
- `GET https://markets.newyorkfed.org/api/rates/unsecured/effr/search.json?startDate=1990-01-01&endDate=2000-01-01` → 200, 19 bytes
- `GET https://markets.newyorkfed.org/api/rates/unsecured/effr/search.json?startDate=2000-01-01&endDate=2018-06-30` → 200, 938,113 bytes
- `GET https://markets.newyorkfed.org/api/soma/asofdates/list.json` → 200, 15,846 bytes
- `GET https://markets.newyorkfed.org/api/soma/summary.json` → 200, 321,475 bytes
- `GET https://markets.newyorkfed.org/api/soma/tsy/get/asof/2026-10-28.json` → 200, 136,621 bytes
- `GET https://markets.newyorkfed.org/api/soma/agency/get/asof/2026-10-28.json` → 200, 1,725,794 bytes
- `GET https://markets.newyorkfed.org/api/marketshare/qtrly/latest.json` → 200, 59,553 bytes ×2
- `GET https://www.newyorkfed.org/medialibrary/interactives/sce/sce/downloads/data/frbny-sce-data.xlsx` → 200, 1,100,751 bytes
- `GET https://www.newyorkfed.org/medialibrary/interactives/householdcredit/data/xls/HHD_C_Report_2026Q2.xlsx` → 200, 962,992 bytes
- `GET https://www.newyorkfed.org/research/banking_research/datasets` → 200, 108,409 bytes
- `GET https://markets.newyorkfed.org/api/pd/get/all/timeseries.csv` → 200, 2,000,000 bytes
