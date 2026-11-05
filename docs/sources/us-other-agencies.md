---
id: us-other-agencies
name: Other US government bodies of the catalogue
rows: []
catalogue: ["Social Security Administration", "Internal Revenue Service", "Federal Housing Finance Agency", "Office of Travel and Tourism Industries", "Administrative Office of the U.S. Courts", "CNBC/National Retail Federation", "Department of Labor", "U.S. Department of Labor", "National Association of Home Builders", "National Federation of Independent Business", "Office of Management and Budget, The White House", "US Department of Justice", "United States International Trade Commission"]
access: free, no key                  # mixed: see each section
probed: 2026-11-05
confidence: called                    # some refused: see each section
---

# Other US government bodies of the catalogue

`resources/data_sources.json` has 26 bodies with `USA` in `countries` and kind `gov` or `stat`. The others are on the cards `eia`, `bls`, `census`,
`bea`, `treasury`, `usda`. Here, the rest. None is read by the app.

## Contents
**Department of Labor** (`Department of Labor`, `U.S. Department of Labor`). `www.dol.gov` answers 403 to this client. Its open API does answer:
`apiprod.dol.gov/v4/datasets`, 45 data sets in [`catalog/us-other-agencies_dol.tsv`](catalog/us-other-agencies_dol.tsv) (agency, table, name, frequency,
category, API name): MSHA 15, OSHA 11, ILAB 7, ETA 4 (`ETA_ui_national_weekly_claims`, `ETA_ui_state_weekly_claims`, apprenticeship, trade
adjustment), OWCP 2, and EBSA, WHD, VETS, ODEP, TRNG, WB 1 each. Data rows need a key (`/v4/get/ETA/ui_national_weekly_claims/json` answers 401). Key
from `dataportal.dol.gov`. Weekly claims also at `oui.doleta.gov/unemploy/claims.asp` (a form, by nation or state), no key.

**Federal Housing Finance Agency** (`www.fhfa.gov`). `/data/hpi/datasets` links 50 files: `hpi_master.csv` (`hpi_type, hpi_flavor, frequency,
level, place_name, place_id, yr, period, index_nsa, index_sa, rstderr, note`, at `/hpi/download/monthly/`), purchase-only and all-transactions by US and
census division, state, metro, ZIP3, ZIP5, county, tract, non-metro, distress-free, manufactured housing; `hpi_at_state.csv` (no header row: state, year,
quarter, index). `/data` also lists National Mortgage Database (`/data/nmdb`), NSMO, MIRS, Public Use Database (`/data/pudb`), UAD, Conforming Loan Limit, FHLB
stress tests. `robots.txt` excludes `/document`, `/media`, `/sites/default/files`.

**Office of Management and Budget** (`www.whitehouse.gov/omb/information-resources/budget/historical-tables/`). Historical Tables, 57 distinct files, `hist01z1_fy2027.xlsx` and
following, zip `hist_fy2027.zip`: receipts, outlays, surpluses or deficits from 1789, as percent of GDP from 1930, in current and constant dollars.
Annual, one vintage per budget.

**Internal Revenue Service** (`www.irs.gov/statistics`). Statistics of Income: pages for individual, business, corporation, estate and gift, international,
charitable and exempt organisations, Data Book, historical tables, tax gap. Excel and CSV by topic, no API found; not walked. **Catalogue error:** its `url`
is `taxpolicycenter.org`, not the IRS, and that site answers a 403 challenge page.

**United States International Trade Commission**. `www.usitc.gov` answers 403. `dataweb.usitc.gov` (trade by HTS code, a web application, `robots.txt` readable; the
documentation page is a script shell, not read). `hts.usitc.gov/reststop/exportList?format=JSON&from=0101&to=0102&styles=false` answers without a key: the tariff schedule,
fields `htsno, indent, description, superior, units, general, special, other, footnotes, quotaQuantity, additionalDuties`.

**US Department of Justice** (`www.justice.gov/data.json`). 3,267 DCAT data sets: Bureau of Justice Statistics 1,510, National Institute of Justice 1,293, US Trustees
232, OJJDP 153, Drug Enforcement Administration 12, Office of Justice Programs 12, Antitrust Division 11, FBI 11, EOIR 9, JMD 9. Antitrust workload is the only one near
markets. Nothing for this app's pages.

**Administrative Office of the U.S. Courts** (`www.uscourts.gov/data-news/reports`, `/statistics-reports`). Judicial vacancies, reports. The bankruptcy filings the
catalogue points to are in those reports (PDF and Excel); not walked. `/data-news/data-tables` 404.

**Social Security Administration** (`www.ssa.gov`). 403 Access Denied on `robots.txt`, `/open/data/`, `/data/`, `/policy/docs/statcomps/`, `/oact/STATS/`. Contents not known from
a call.

**Office of Travel and Tourism Industries** (`travel.trade.gov`). No answer on any of 4 calls. Contents not known from a call.

**National Association of Home Builders** (`www.nahb.org`, not government). `/news-and-economics/housing-economics/indices/housing-market-index` links 6 files, tables and history
named by month (`t2-national-hmi-history-202610`, `t3-national-hmi-components-...`, `t4-regional-hmi-history-...`).

**CNBC/National Retail Federation** (`nrffoundation.org`, not government). The URL is the NRF Foundation's careers site. `nrf.com`'s two guessed paths answer 404.

**National Federation of Independent Business** (`www.nfib.com`, not government). 403 challenge page, `robots.txt` included.

## How to call it
See each section. Example: `GET https://apiprod.dol.gov/v4/datasets?page=2` (10 a page, 5 pages).

## What the app reads
Nothing here. `HSN1F`, `CSUSHPINSA` and the like come through FRED; `ICSA`, `CCSA` (weekly claims) too.

## Not read yet
- FHFA `hpi_master.csv`: house prices by state and metro, no key; a US layer for Map or Countries.
- OMB Historical Tables: long fiscal history for Countries › United States.
- DOL `ETA_ui_state_weekly_claims` (free key), or `oui.doleta.gov`: claims by state, FRED has them too.
- HTS schedule: tariff rates by code, for trade pages.
- NAHB `t3-national-hmi-components`: builders' sentiment components.

## Attribution checks in `resources/data_sources.json`
- `Internal Revenue Service`: `url` is taxpolicycenter.org. Wrong.
- `U.S. Department of Labor`: `url` is bls.gov, `api` is `web`: it duplicates BLS. `Department of Labor`: `api` is `web`, but `apiprod.dol.gov/v4` exists.
- `National Association of Home Builders`, `National Federation of Independent Business`, `CNBC/National Retail Federation`: kind `gov`, they are private.
- `Financial Management Service, United States`, `Financial Management Service, US Treasury`: `api` `web`, but the Fiscal Data API is theirs.
- `U.S. Bureau of Economics Analysis`: `api` `web`, kind `gov`: a misspelt BEA, should be BEA API.
- `US Department of Agriculture`: `api` `web`, but PSD and FAS Open Data exist.
- `U.S. Census Bureau`: `api` "no key for light use": data calls now answer "Missing Key".
- `Federal Reserve Bank of *`: `api` "Data Download Program (CSV, no key); FRED API": the DDP belongs to the Board alone. No regional Bank has it.
- `Federal Reserve Bank of New York`: same, and it has its own Markets API, unnamed.
- `Treasury Department, Ministry of Finance and Economy` (Brunei), `Department of the Treasury of Puerto Rico`: `api` says Fiscal Data API; a name match, wrong.
- `Freddie Mac`: `api` "FRED API", `access` headline: it publishes its own CSVs freely.

## Calls made
- `GET https://www.ssa.gov/robots.txt` → 403, 379 bytes
- `GET https://www.ssa.gov/open/data/` → 403, 383 bytes
- `GET https://www.ssa.gov/data/` → 403, 374 bytes
- `GET https://www.ssa.gov/policy/docs/statcomps/` → 403, 399 bytes
- `GET https://www.ssa.gov/oact/STATS/` → 403, 384 bytes
- `GET https://www.irs.gov/robots.txt` → 200, 3,899 bytes
- `GET https://www.irs.gov/statistics` → 200, 112,548 bytes
- `GET https://www.irs.gov/statistics/soi-tax-stats-all-topics` → 404, 86,717 bytes
- `GET https://www.taxpolicycenter.org/robots.txt` → 403, 5,518 bytes
- `GET https://www.taxpolicycenter.org/statistics` → 403, 5,518 bytes
- `GET https://www.fhfa.gov/robots.txt` → 200, 2,873 bytes
- `GET https://www.fhfa.gov/data` → 200, 79,913 bytes
- `GET https://www.fhfa.gov/data/hpi` → 200, 89,100 bytes
- `GET https://www.fhfa.gov/data/hpi/datasets` → 200, 550,184 bytes
- `GET https://www.fhfa.gov/data/national-mortgage-database-nmdb` → 404, 74,990 bytes
- `GET https://www.fiscal.treasury.gov/robots.txt` → 200, 2,027 bytes
- `GET https://www.fiscal.treasury.gov/data-transparency/` → 200, 63,514 bytes
- `GET https://www.fiscal.treasury.gov/reports-statements/` → 200, 119,021 bytes
- `GET https://travel.trade.gov/robots.txt` → ERR (URLError)
- `GET https://travel.trade.gov/` → ERR (URLError)
- `GET https://travel.trade.gov/research/index.asp` → ERR (URLError)
- `GET https://travel.trade.gov/view/m-2017-I-001/index.asp` → ERR (URLError)
- `GET https://www.uscourts.gov/robots.txt` → 200, 1,782 bytes
- `GET https://www.uscourts.gov/data-news/data-tables` → 404, 200,000 bytes
- `GET https://www.uscourts.gov/data-news/reports` → 200, 225,099 bytes
- `GET https://www.uscourts.gov/statistics-reports` → 200, 227,774 bytes
- `GET https://www.dol.gov/robots.txt` → 403, 375 bytes
- `GET https://www.dol.gov/agencies/eta/ui-data` → 403, 393 bytes
- `GET https://www.dol.gov/agencies/ebsa/researchers/statistics` → 403, 409 bytes
- `GET https://www.dol.gov/data` → 403, 365 bytes
- `GET https://apiprod.dol.gov/v4/datasets` → 200, 11,725 bytes
- `GET https://apiprod.dol.gov/v4/get/bls/ces` → 403, 42 bytes
- `GET https://oui.doleta.gov/robots.txt` → 200
- `GET https://oui.doleta.gov/unemploy/claims.asp` → 200, 11,358 bytes
- `GET https://dataportal.dol.gov/robots.txt` → 200, 75 bytes
- `GET https://dataportal.dol.gov/datasets` → 200, 9,768 bytes
- `GET https://dataportal.dol.gov/api-keys` → 200, 9,768 bytes
- `GET https://nrffoundation.org/robots.txt` → 200, 65 bytes
- `GET https://nrffoundation.org/` → 200, 380,172 bytes
- `GET https://nrf.com/research-insights/holiday-data-and-trends` → 404, 192,778 bytes
- `GET https://nrf.com/research-insights/retail-monitor` → 404, 192,607 bytes
- `GET https://www.nahb.org/robots.txt` → 200, 77 bytes
- `GET https://www.nahb.org/news-and-economics/housing-economics/indices/housing-market-index` → 200, 61,653 bytes
- `GET https://www.nahb.org/news-and-economics/housing-economics` → 200, 61,086 bytes
- `GET https://www.nfib.com/robots.txt` → 403, 1,360 bytes
- `GET https://www.nfib.com/surveys/small-business-economic-trends/` → 403, 1,505 bytes
- `GET https://www.nfib.com/foundations/research-center/` → 403, 1,450 bytes
- `GET https://www.whitehouse.gov/robots.txt` → 200, 111 bytes
- `GET https://www.whitehouse.gov/omb/information-resources/budget/historical-tables/` → 200, 254,121 bytes
- `GET https://www.whitehouse.gov/omb/budget/historical-tables/` → 200, 254,121 bytes
- `GET https://www.justice.gov/robots.txt` → 200, 2,651 bytes
- `GET https://www.justice.gov/data.json` → 200, 10,455,182 bytes
- `GET https://www.justice.gov/data` → 200, 73,413 bytes
- `GET https://www.usitc.gov/robots.txt` → 403, 381 bytes
- `GET https://www.usitc.gov/applications/dataweb/` → 403, 396 bytes
- `GET https://www.usitc.gov/data_and_tools` → 403, 389 bytes
- `GET https://dataweb.usitc.gov/robots.txt` → 200, 5,246 bytes
- `GET https://dataweb.usitc.gov/api-documentation` → 200, 9,531 bytes
- `GET https://dataweb.usitc.gov/` → 200, 9,531 bytes
- `GET https://hts.usitc.gov/robots.txt` → 200, 6,774 bytes
- `GET https://hts.usitc.gov/reststop/exportList?format=JSON&from=0101&to=0102&styles=false` → 200, 3,826 bytes
- `GET https://dataweb.usitc.gov/assets/swagger.json` → 200, 5,246 bytes
- `GET https://datatools.dws.dol.gov/` → ERR (URLError)
- `GET https://apiprod.dol.gov/v4/get/ETA/ui_national_weekly_claims/json?limit=1` → 401, 188 bytes
- `GET https://www.fhfa.gov/hpi/download/monthly/hpi_master.csv` → 200, 4,000,000 bytes
- `GET https://www.fhfa.gov/hpi/download/quarterly_datasets/hpi_at_state.csv` → 200, 187,977 bytes
- `GET https://www.nahb.org/-/media/NAHB/news-and-economics/docs/housing-economics/hmi/202610/t2-national-hmi-history-202610.pdf` → 200, 32,201 bytes
- `GET https://apiprod.dol.gov/v4/datasets?page=1` → 200, 11,725 bytes
- `GET https://apiprod.dol.gov/v4/datasets?page=2` → 200, 8,367 bytes
- `GET https://apiprod.dol.gov/v4/datasets?page=3` → 200, 11,194 bytes
- `GET https://apiprod.dol.gov/v4/datasets?page=4` → 200, 11,543 bytes
- `GET https://apiprod.dol.gov/v4/datasets?page=5` → 200, 6,591 bytes
