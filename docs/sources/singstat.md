---
id: singstat
name: Singapore Department of Statistics (SingStat)
rows: [nat_singapore]
catalogue: ["Statistics Singapore", "Singapore Statistics"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# SingStat

The Table Builder API: every table of Singapore's statistics office, in JSON, by theme and subject.

## Contents
**2,835 tables** (`resourceid?keyword=%25&searchOption=all`), complete in [`catalog/singstat.tsv`](catalog/singstat.tsv): `id`, title,
theme, subject, topic, table type, and for the time series frequency, adjustment, first, last, rows, observations and last update.

- Themes: Population 979, Households 741, Economy & Prices 553, Industry 324, Society 140, Trade & Investment 98. 53 subjects, 207 topics.
- **1,050 time series (`M*` ids)**, all with metadata read: 3.1M observations, 1950 to 2026 Sep. Annual 700, Quarterly 173, Monthly 151,
  Half-Yearly 10, Ad-Hoc 10, others 6. The rest are census and survey cross-tabs, not opened: 1,302 with numeric ids, `C*` 433, `D*` 48, `T*` 2.

Subjects with the most time series: Labour, Employment, Wages and Productivity 87, National Accounts 84, Services 74, Producer and
International Trade Price Indices 53, Finance and Insurance 51, Merchandise Trade 44, Manufacturing 34, Consumer Price Index 30, Public
Finance 23, Building and Real Estate 20, Trade in Services 19, Foreign Direct Investment 16, Tourism 14.

Macro families:

| id | Table | Freq | First to last | Rows | Observations |
|---|---|---|---|---|---|
| `M213781` | CPI % over a year, 2024 base | M | 1962 Jan to 2026 Sep | 207 | 35,811 |
| `M213751` | CPI, 2024 base | M | 1961 Jan to 2026 Sep | 207 | 38,295 |
| `M015651` | GDP at current prices by industry | Q | 1975 1Q to 2026 2Q | 29 | 4,534 |
| `M015671` | Contribution to GDP growth, chained | Q | 1977 1Q to 2026 2Q | | |
| `M355351` | Index of Industrial Production, 2025 = 100 | M | 1983 Jan to 2026 Sep | | |
| `M451481`, `M451482` | Merchandise trade by commodity section | M | 1978 Jan to 2026 Sep | | |
| `M451301` | Non-oil domestic exports by market | M | 1978 Jan to 2026 Sep | | |
| `M450981` | Domestic exports of major non-oil products | M | 1997 Jan to 2026 Sep | | |
| `M183401` | Resident unemployment rate | A | 1992 to 2025 | | |
| `M212261`, `M210641` | Property price indices | Q | 1975 1Q to 2026 3Q | | |
| `M700071` | Banks' interest rates | M | 1988 Jan to 2026 Sep | | |
| `M700051` | Exchange rates, average | M | 1988 Jan to 2026 Sep | | |
| `M602151` | Retail Sales Index | A | 1985 to 2025 | | |
| `M085811` | FDI by source economy and industry | A | 2015 to 2025 | 374 | 3,645 |

Empty cells: the values are in the TSV. Singapore, with partner economies in trade and FDI.

## How to call it
- Base: `https://tablebuilder.singstat.gov.sg/api/table/`, `Accept: application/json`. No key.
- Catalogue: `GET resourceid?keyword=%25&searchOption=all` (746 KB).
- Metadata: `GET metadata/M213781`: `theme`, `subject`, `topic`, `id`, `title`, `frequency`, `adjustmentType`, `dataSource`, `footnote`,
  `dataLastUpdated`, `startPeriod`, `endPeriod`, `total`, `row` (`seriesNo`, `rowText`, ...).
- Data: `GET tabledata/M213781?seriesNoORrowNo=1&limit=1&sortBy=key%20desc`: `Data.row[].columns[]` with `key` ("2026 Sep") and `value`.
- Trap: `total` in the metadata counts observations of the whole table, across its rows, not per series.

## What the app reads
`nat_singapore`, row at `src/SourcesPage.cpp:727`; `src/NationalSources.cpp:174`: `M213781` row 1, the last point only, keeps the
`All Items` row's `key` and `value` (`src/NationalSources.cpp:190`). No history is requested though the call could ask for it. Map only.

## Not read yet
- Countries › Singapore (no `singstat` provider exists): `M015651`, `M355351`, `M451301`, `M183401`, `M212261`, `M700071`.
- Supply chains: `M451481`, `M451482` trade by commodity, monthly.
- History for the same table: omit `limit` in the app's call, it reaches 1962.

## Calls made
- `GET https://tablebuilder.singstat.gov.sg/api/table/resourceid?keyword=%25&searchOption=all` → 200, 746 KB
- `GET https://tablebuilder.singstat.gov.sg/api/table/metadata/M213781` → 200, 28 KB
- `GET https://tablebuilder.singstat.gov.sg/api/table/tabledata/M213781?seriesNoORrowNo=1&limit=1&sortBy=key%20desc` → 200, 1.1 KB
- `GET https://tablebuilder.singstat.gov.sg/api/table/metadata/<id>` for the 1,050 `M*` tables → all answered
