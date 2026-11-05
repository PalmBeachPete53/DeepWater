---
id: statcan
name: Statistics Canada
rows: [research_statcan]
catalogue: ["Statistics Canada"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# Statistics Canada

Every table of the national statistical office (labour, prices, national accounts, trade, housing, business, health,
census) through the Web Data Service (WDS). Bank of Canada financial tables are also republished here.

## Contents
**8,252 tables, 5,060 current and 3,192 archived** (`getAllCubesListLite`). The complete list is in
[`catalog/statcan.tsv.gz`](catalog/statcan.tsv.gz): `productId`, CANSIM id, title, start, end, frequency, `archived`
(`1` archived, `2` current), subjects, surveys. Dates run from 1851 to 2075 (projections). Canada, provinces,
territories and census areas. Code sets (`getCodeSets`): 622 subjects, 906 surveys, 17 frequencies.

Current tables by frequency: Occasional 2,584, Annual 1,375, Monthly 315, Every 5 years 217, Quarterly 200, Every 2 years
133, Semi-annual 71, Occasional Monthly 51, Occasional Quarterly 50, Every 3 years 39, Weekly 17, Daily 5, Occasional
Daily 2, Every 10 years 1.

Current tables by first subject level: Business performance and ownership 1,090, Health 436, Science and technology 355,
Agriculture and food 309, Economic accounts 246, Education 213, Labour 210, Crime and justice 191, Environment 188,
Business and consumer services 146, Income, pensions, spending and wealth 125, Transportation 120, Construction 93,
and smaller ones.

Families a macro terminal wants (structure from `getCubeMetadata`, all kept in
[`catalog/statcan_cubes.jsonl`](catalog/statcan_cubes.jsonl)):

| productId | Table | Span | Freq | Series | Points | Dimensions (members) |
|---|---|---|---|---|---|---|
| 14100287 | Labour force characteristics | 1976 to 2026-09 | M | 8,991 | 5.5M | Geography 11, characteristics 9, Gender 3, Age 9, Statistics 4, Data type 3 |
| 14100355 | Employment by industry | 1976 to 2026-09 | M | 1,176 | 690K | Geography 11, NAICS 21, Statistics 4, Data type 3 |
| 14100063 | Employee wages by industry | 1997 to 2026-09 | M | 37,620 | 13.4M | Geography 11, Wages 5, Type of work 3, NAICS 19, Gender 3, Age 4 |
| 14100320 | Hours and wages by characteristics | 2001 to 2026-09 | M | 190 | 59K | Geography 1, Characteristics 19, Hours and wages 10 |
| 18100004 | CPI, not seasonally adjusted | 1914 to 2026-09 | M | 2,139 | 1.2M | Geography 30, Products 359 |
| 18100006 | CPI, seasonally adjusted | 1992 to 2026-09 | M | 11 | 4.6K | Geography 1, Products 11 |
| 18100256 | Core inflation, Bank of Canada definitions | 1949 to 2026-09 | M | 13 | 10K | Geography 1, Alternative measures 13 |
| 18100205 | New housing price index | 1981 to 2026-09 | M | 120 | 66K | Geography 40, Indexes 3 |
| 34100158 | Housing starts (CMHC), annual rate | 1990 to 2026-09 | M | 13 | 5.7K | Geography 13 |
| 36100434 | GDP at basic prices by industry | 1997 to 2026-08 | M | 747 | 266K | Geography 1, Seasonal 2, Prices 2, NAICS 249 |
| 36100491 | Real-time vintages of that GDP | 1997 to 2026-07 | M | 119,646 | 41.3M | Geography 1, Seasonal 2, Prices 2, NAICS 321, Release 140 |
| 36100104 | GDP, expenditure-based | 1961 to 2026 Q2 | Q | 271 | 69K | Geography 1, Prices 7, Seasonal 2, Estimates 31 |
| 36100108 | Gross fixed capital formation | 1961 to 2026 Q2 | Q | 360 | 92K | Geography 1, Prices 5, Seasonal 2, Estimates 60 |
| 12100011 | Merchandise trade by partner | 1997 to 2026-09 | M | 261 | 93K | Geography 1, Trade 3, Basis 2, Seasonal 2, Partners 29 |
| 20100056 | Retail sales | 2017 to 2026-08 | M | 738 | 81K | Geography 23, NAICS 30, Sales 2, Adjustments 2 |
| 16100047 | Manufacturers' sales, inventories, orders | 1992 to 2026-08 | M | 2,840 | 1.1M | Geography 1, Statistics 9, Seasonal 2, NAICS 351 |
| 16100048 | Manufacturing sales by province | 1992 to 2026-08 | M | 3,595 | 1.3M | Geography 14, Statistics 1, Seasonal 2, NAICS 351 |
| 10100002 | Central government debt | 2009-04 to 2026-08 | M | 33 | 6.3K | Geography 1, Debt 33 |
| 10100122 | Financial market statistics (Bank of Canada) | 1934 to 2026-10 | M | 71 | 58K | Geography 1, Rates 73 |

## How to call it
- Base: `https://www150.statcan.gc.ca/t1/wds/rest/`. No key.
- Catalogue: `GET getAllCubesListLite` (5 MB, the whole list), `GET getAllCubesList` (heavier), `GET getCodeSets`.
- One table: `POST getCubeMetadata` with `[{"productId": 18100004}]`.
- A vector's identity: `POST getSeriesInfoFromVector` with `[{"vectorId": 41690973}]` gives `productId`, `coordinate`,
  `SeriesTitleEn`.
- Data: `GET getDataFromVectorByReferencePeriodRange?vectorIds=2062815&startRefPeriod=2026-01-01&endReferencePeriod=2026-12-31`.
  Whole tables as a zipped CSV: `getFullTableDownloadCSV/<productId>/en` (documented, not called).
- Traps: `archived` in the Lite list is `1` for archived and `2` for current. A `productId` that doesn't exist answers
  409 `{"message":"The product is not released yet"}`, which reads like a rate limit but isn't.

## What the app reads
- `research_statcan`, row at `src/SourcesPage.cpp:537`; Countries › Canada, `src/CountryData.cpp:1645`, one vector over
  its whole history, keeps `refPer` and `value`. Vectors in `resources/country_indicators.json`, provider `statcan`:
  - Table 14100287: `2062811` employment, `2062812` full-time, `2062814` unemployment, `2062815` unemployment rate,
    `2062816` participation, `2062817` employment rate.
  - Table 14100063: `2132579` hourly wage. Table 14100320: `105812645` average hourly wages.
  - Table 18100004: `41690973` CPI all items, `41691233` excluding food and energy. Table 18100006: `41690914` seasonally
    adjusted. Table 18100256: `108785714` CPI-median, `108785715` CPI-trim.
  - Table 34100158: `52300157` housing starts. Table 18100205: `111955442` new housing prices.

## Not read yet
- GDP by industry 36100434 and expenditure GDP 36100104: Countries › Canada GDP comes from elsewhere today.
- Real-time vintages 36100491: revisions, a rare free data set.
- Merchandise trade 12100011, retail 20100056, manufacturers' orders 16100047, the provincial cuts of the labour force
  (14100287) and CPI (18100004): Countries › Canada, Economy.

## Calls made
- `GET https://www150.statcan.gc.ca/t1/wds/rest/getAllCubesListLite` → 200, 5.0 MB
- `GET https://www150.statcan.gc.ca/t1/wds/rest/getCodeSets` → 200, 315 KB
- `POST https://www150.statcan.gc.ca/t1/wds/rest/getSeriesInfoFromVector` (the 15 vectors of the app) → 200, 6 KB
- `POST https://www150.statcan.gc.ca/t1/wds/rest/getCubeMetadata` for each of the 21 tables of
  `catalog/statcan_cubes.jsonl` → 200, 6 KB to 206 KB; for 36100222, 33100036, 10100139 → 409 "not released yet"
- `GET https://www150.statcan.gc.ca/robots.txt` → 200, 2 KB (the API path allowed)
