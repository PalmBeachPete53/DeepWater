---
id: adb
name: Asian Development Bank (Key Indicators Database)
rows: []
catalogue: ["Asian Development Bank (ADB)", "Asian Development Bank", "ADB - Asian Development Bank"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# Asian Development Bank

KIDB: annual indicators of Asia and the Pacific, national and subnational, from population to national accounts, money, debt and trade. A documented SDMX API.

## Contents
**66 dataflows** (the top one `DF_KIDB` holds "All KIDB Indicators", the others are its parts), **691 indicators**, 642 economy codes in the API page's list: 62 national or group codes and 580 subnational. Flows with their rows, indicators, economies and periods: [`catalog/adb.tsv`](catalog/adb.tsv). Indicators with labels: [`catalog/adb_indicators.tsv`](catalog/adb_indicators.tsv).

Annual only: asking `Q..` or `M..` on `DF_PRI` returned the same annual rows. Asked for its annual data, `DF_KIDB` gave 550,159 rows, 680 indicators, 624 economies, 2000 to 2025. **The API's window starts in 2000:** `startPeriod=1950` and `1990` answer 422 "The start period must be at least 2000". Earlier years would be in the site's per-economy files, which were not opened.

| Flow | Name | Rows | Indicators | Economies |
|---|---|---|---|---|
| `DF_NA` | National Accounts | 137,547 | 143 | 416 |
| `DF_LFE` | Labor Force and Employment | 64,294 | 24 | 503 |
| `DF_GOV` | Government and Governance | 47,406 | 104 | 49 |
| `DF_PRI` | Prices | 42,216 | 41 | 367 |
| `DF_GLOB_BOP` | Balance of Payments | 34,799 | 33 | 50 |
| `DF_PPSI` | Population, Poverty, and Social Indicators | 30,287 | 38 | 50 |
| `DF_KIDB_ITG` | International Trade in Goods by partner | 26,642 | 2 | 49 |
| `DF_MF` | Money and Finance | 22,366 | 49 | 46 |
| `DF_EXT` | External Indebtedness | 14,912 | 19 | 44 |
| `DF_ENE` | Energy | 10,532 | 18 | 50 |
| `DF_IL` | International Liquidity | 5,281 | 5 | 44 |
| `DF_ER` | Exchange Rates | 4,950 | 4 | 50 |
| `DF_GLOB_FDI` | Foreign Direct Investment | 2,255 | 2 | 48 |
| `DF_SDG` | Sustainable Development Goals (with `DF_SDG_01` to `_17`) | 82,623 | 140 | 50 |

Columns: `DATAFLOW, FREQ, INDICATOR, ECONOMY_CODE, TIME_PERIOD, OBS_VALUE, UNIT, UNIT_MULT, DECIMALS, FOOTNOTE, OBS_STATUS, REF_YEAR, BASE_YEAR, DATA_SOURCE, METHODOLOGY`.

The site also keeps one xlsx and one pdf per economy for 50 countries (`data.adb.org/media/<id>/download`).

## How to call it
- Base `https://kidb.adb.org/api/v5/sdmx`. No key. Documentation: `https://kidb.adb.org/api`; the flow list sits in the page's own data.
- Data: `/data/ADB,DF_NA/A.NGDP_XDC.PHI?format=sdmx-csv&startPeriod=2024`. Formats `sdmx-csv`, `sdmx-json`, `sdmx-structure-xml`, `sdmx-generic-xml` with `version=2.1`. Key `FREQ.INDICATOR.ECONOMY_CODE`; `DF_KIDB_ITG` has five positions.
- Code list: `/structure/codelist/ADB/CL_KIDB_INDICATORS/+?format=sdmx-json`. One flow: `/structure/dataflow/ADB/DF_NA/+?format=sdmx-json`.
- Traps: ADB's own economy codes are not ISO (`PHI`, `SIN`, `PRC`, `BAN`, `BHU`, `FIJ`); `/structure/dataflow/ADB` gives 404 and `*` gives 422; `..` as a key gives 404, `A..` works; `DF_GLOB_ET` returned a single row and `DF_SDG_10` 50: see the tsv before relying on a flow.
- `www.adb.org` answered 403 to the earlier probe; `kidb.adb.org` answers. Terms (`/terms`): data may be reused for commercial and non-commercial purposes at no cost, with attribution to "Asian Development Bank: Key Indicators Database Online", third-party content excepted.

## What the app reads
Nothing.

## Not read yet
**Economy › Countries:** the Pacific and South Asian countries other sources miss (Cook Islands, Bhutan, Fiji…), with `DF_NA`, `DF_PRI`, `DF_MF`, `DF_EXT`, `DF_ER`. **Economy, subnational:** the 580 regional codes.

## Calls made
All on 2026-11-05 with `User-Agent: Deepwater/1.0`.
- `GET https://kidb.adb.org/robots.txt` → 200, 24 bytes.
- `GET https://kidb.adb.org/` → 200, 63,858 bytes. `/economies` → 200, 58,771. `/api` → 200, 135,555 (several times). `/terms` → 200, 60,128. `/metadata` → 200, 63,858, the home page. `/themes` → 404.
- `GET https://kidb.adb.org/js/web.js?id=4ec1c851dc49e0ed99bed06fb28e3cf2` → 200, 1,394,897 bytes, to read the documentation text of the API page.
- `GET https://kidb.adb.org/api/v2/sdmx/structure/dataflow/ADB` → 404. `…/api/v5/sdmx/structure/dataflow/ADB` → 404. `…/api/v5/sdmx/structure/dataflow/ADB/*/+?format=sdmx-json` → 422.
- `GET https://kidb.adb.org/api/v5/sdmx/structure/dataflow/ADB/DF_NA/+?format=sdmx-json` → 200, 601 bytes.
- `GET https://kidb.adb.org/api/v5/sdmx/structure/codelist/ADB/CL_KIDB_INDICATORS/+?format=sdmx-json` → 200, 399,395 bytes.
- `GET https://kidb.adb.org/api/v5/sdmx/data/ADB,DF_NA/A.NGDP_XDC.PHI?format=sdmx-csv&startPeriod=2024` → 200, 358 bytes; header kept.
- `GET https://kidb.adb.org/api/v5/sdmx/data/ADB,DF_PRI/Q..?format=sdmx-csv` → 200, 10,244,377 bytes. `…/M..?format=sdmx-csv` → 200, 10,244,377; axes kept.
- `GET https://kidb.adb.org/api/v5/sdmx/data/ADB,DF_ER/A..?format=sdmx-csv&startPeriod=1950` → 422, 122 bytes. `…&startPeriod=1990` → 422, 122 bytes.
- `tools/source-inventory/adb.py`: `/data/ADB,<flow>/A..?format=sdmx-csv` for the 66 flows, read for its axes and thrown away → all answered on the second run; the first, with the key `..`, got 404 on each flow it reached.
