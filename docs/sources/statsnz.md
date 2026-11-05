---
id: statsnz
name: Statistics New Zealand
rows: []
catalogue: ["Statistics New Zealand"]
access: free key
probed: 2026-11-05
confidence: called
---

# Statistics New Zealand

Two doors: Aotearoa Data Explorer, an SDMX API whose list is open and whose content wants a subscription key; and a page of bulk CSV files per release
with the macro series. The catalogue's "Infoshare and Aotearoa Data Explorer (CSV), no key" is half right.

## Contents
**Data Explorer: 913 dataflows**, agency `STATSNZ` (`dataflow/all/all/latest?detail=allstubs`), complete in
[`catalog/statsnz.tsv`](catalog/statsnz.tsv). Families by code: `CEN` 705 (censuses), `LEED` 59 (linked employer-employee), `POPPR` 33 (population
projections), `IWI` 24, `POPES` 23 (population estimates), `INC` 16 (income), `BDS` 14 (business demography), `CORR` 10, `JUS` 10, `HES` 7 (household
expenditure), `INJ` 6, `AGR` 3, `PRD` 3 (productivity). **No CPI, GDP, trade or labour force flow.** Structures for the 208 non-census flows were attempted:
18 answered and 190 got 401, each with its status in [`catalog/statsnz_structures.jsonl`](catalog/statsnz_structures.jsonl).

**CSV files: 72 distinct ones** on `large-datasets/csv-files-for-download`, grouped by Business, Census, Economy, Effects of COVID-19 on trade, Environment,
Government finance, Health, Industries, Labour market, Population, Society, Tatauranga umanga Māori; list in [`catalog/statsnz_csv.tsv`](catalog/statsnz_csv.tsv).
Names carry the release: `gross-domestic-product-june-2026-quarter`, `consumers-price-index-september-2026-quarter-index-numbers`,
`-seasonally-adjusted`, `-infoshare-data`, `selected-price-indexes-september-2026`, `household-living-costs-price-indexes-september-2026-quarter-time-series`,
`balance-of-payments-and-international-investment-position-june-2026-quarter`, `overseas-trade-indexes-june-2026-quarter-provisional`,
`na-isal-june-2026-quarter-institutional-sector-accounts`, `productivity-statistics-1978-2025`, `regional-gross-domestic-product-year-ended-march-2025`,
`annual-enterprise-survey-2025`, `local-authority-statistics-june-2026-quarter`, `international-migration-august-2026`.

## How to call it
- `GET https://api.data.stats.govt.nz/rest/dataflow/all/all/latest?detail=allstubs`, no key, XML.
- Everything else: `401 "Access denied due to missing subscription key"`, so a key from the portal is needed (the portal wasn't visited). A few
  `references=all` calls answered 200 (likely a cache), `availableconstraint` never did.
- CSVs: the page's data lists file names, the URL changes each release. Scrape the page each time, or expect 404.

## What the app reads
Nothing: no row in `src/SourcesPage.cpp`. New Zealand reaches the Map through OECD KEI, IMF and BIS.

## Not read yet
- Countries › New Zealand: the CSV files named above, read from the page's list.
- With a key: `LEED`, `BDS`, `HES`, `PRD`.

## Calls made
- `GET https://api.data.stats.govt.nz/rest/dataflow/all/all/latest?detail=allstubs` → 200, 655 KB
- `GET https://api.data.stats.govt.nz/rest/dataflow/STATSNZ/<id>/<version>?references=all` for the 208 non-census flows → 18 at 200, 190 at 401
- `GET https://api.data.stats.govt.nz/rest/availableconstraint/STATSNZ,<id>,<version>/all/all/all?mode=exact` for the same → 401 for all
- `GET https://www.stats.govt.nz/large-datasets/csv-files-for-download/` → 200, 215 KB
- `GET https://explore.data.stats.govt.nz/` → 200, 81 KB
