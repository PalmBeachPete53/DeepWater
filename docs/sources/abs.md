---
id: abs
name: Australian Bureau of Statistics
rows: [research_abs]
catalogue: ["Australian Bureau of Statistics", "Australian Bureau of Statistic"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# Australian Bureau of Statistics

Australia's national statistics through an SDMX 2.1 API: labour force, prices, national accounts, trade, lending,
building, business indicators, population, and about a thousand census cross-tabs.

## Contents
**1,228 dataflows**, agency `ABS` (`dataflow/ABS?detail=allstubs`). Complete list in [`catalog/abs.tsv`](catalog/abs.tsv).
About 970 are census or regional cross-tabs (`C21_*`, `C16_*`, `ABS_C*`). The other **256 have their dimensions, codelist
sizes, first and last period and observation counts** in
[`catalog/abs_structures.jsonl`](catalog/abs_structures.jsonl), 309 million observations in all. All Australia, with
states and territories, capital cities and smaller areas.

Macro families:

| Flow | Version | Name | First to last | F | Observations | Dimensions (codes) |
|---|---|---|---|---|---|---|
| `CPI` | 2.0.0 | Consumer Price Index | 1948-07 to 2026-09 | M, Q | 361,043 | MEASURE 7, INDEX 166, TSEST 5, REGION 9, FREQ 9 |
| `CPI_M` | 1.2.0 | Monthly CPI indicator (**ended 2025-09**) | 2017-09 to 2025-09 | M | 6,455 | MEASURE 5, INDEX 170, TSEST 5, REGION 9, FREQ 9 |
| `CPI_Q` | 1.0.0 | CPI quarterly | 1972-07 to 2026-09 | Q | 16,321 | MEASURE 5, INDEX 168, TSEST 5, REGION 9, FREQ 9 |
| `PPI`, `PPI_FD` | 1.1.3, 1.1.0 | Producer prices by industry, final demand | 1966-07, 1998-07 to 2026-09 | Q | 76,752, 599 | MEASURE 5, INDEX 509 or 830, TYPE, SOURCE, DESTINATION |
| `ITPI_EXP`, `ITPI_IMP` | | Trade price indexes | see the jsonl | Q | | |
| `WPI` | 1.2.0 | Wage Price Index | 1997-07 to 2026-06 | Q | 45,936 | MEASURE 5, INDEX 4, SECTOR 7, INDUSTRY 826, TSEST 5, REGION 10 |
| `AWE` | 1.0.0 | Average Weekly Earnings | 1994-07 to 2026-06 | S | 36,333 | MEASURE 3, ESTIMATE_TYPE 2, SEX 3, SECTOR 7, INDUSTRY 826, TSEST 5, REGION 10 |
| `LCI` | | Labour Cost Index | see the jsonl | | | |
| `LF` | 1.0.0 | Labour Force | 1978-02 to 2026-09 | M | 434,496 | MEASURE 31, SEX 3, AGE 31, TSEST 5, REGION 10, FREQ 9 |
| `LF_UNDER`, `LF_HOURS` | 1.0.1, 1.0.0 | Underemployment, hours worked | 1978 to 2026-09 | M | 1.05M, 18,528 | as `LF`, plus PARM_ITEM 43 or HOURS 7 |
| `JV` | 1.0 | Job Vacancies | 1979-04 to 2026-09 | Q | 13,194 | MEASURE 2, SECTOR 7, INDUSTRY 826, TSEST 5, REGION 10 |
| `ANA_AGG` | 1.0.0 | National accounts, key aggregates | 1959-07 to 2026-06 | Q | 23,976 | MEASURE 7, DATA_ITEM 18, TSEST 5, REGION 10 |
| `ANA_EXP`, `ANA_INC`, `ANA_IND_GVA`, `ANA_SFD` | 1.0.0 | Expenditure, income, GVA by industry, state final demand | 1959-07 (SFD 1983-07) to 2026-06 | Q | 152K, 23K, 66K, 563K | MEASURE 16, DATA_ITEM 9 to 59, SECTOR 35, INDUSTRY 22 |
| `BOP`, `BOP_GOODS`, `BOP_FACTOR`, `BOP_STATE`, `IIP` | 1.0.0 | Balance of payments, investment position | 1959-07 to 2026 | Q, A | 189K, 115K | MEASURE 5 or 7, DATA_ITEM 333 or 215, TSEST 5 |
| `ITGS` | 1.2.0 | International Trade in Goods | 1971-07 to 2026-09 | M | 98,248 | MEASURE 2, DATA_ITEM 54, TSEST 5, REGION 10 |
| `MERCH_EXP`, `MERCH_IMP` | 1.0.0 | Merchandise by SITC commodity and country | 1995-07, 2000-07 to 2026-09 | M | 18.8M, 17.2M | COMMODITY_SITC 349, COUNTRY 262, STATE 11 |
| `HSI_M`, `HSI_Q` | 1.6.0, 1.2.0 | Household Spending Indicator | 2012-07 to 2026-09 | M, Q | 194K, 82K | MEASURE 12, CATEGORY 64, PRICE_ADJUSTMENT 3, TSEST 5, STATE 10 |
| `RT` | 1.0.0 | Retail Trade (**ended 2025-06**) | 1982-04 to 2025-06 | M, Q | 346,761 | MEASURE 4, INDUSTRY 22, TSEST 5, REGION 10 |
| `BUSINESS_TURNOVER` | 1.3.0 | Monthly business turnover (**ended 2025-09**) | 2010-01 to 2025-09 | M | 62,370 | MEASURE 2, PRICE_ADJUSTMENT 3, TSEST 5, INDUSTRY 826, REGION 10 |
| `QBIS` | 1.0.0 | Business Indicators | 1984-04 to 2026-06 | Q | 145,810 | MEASURE 13, PRICE_ADJUSTMENT 3, INDUSTRY 826, SCOPE 3 |
| `CAPEX`, `CAPEX_EST` | 1.0.0 | Private capital expenditure and expectations | 1987-04 to 2026-06 | Q | 127,371 | MEASURE 3, ASSET 3, INDUSTRY 742, STATE 10 |
| `LEND_HOUSING`, `LEND_BUSINESS`, `LEND_PERSONAL` | 1.1 | Lending indicators | 2002-07 to 2026-06 | Q | 50K, 5.9K, 1.2K | MEASURE 5, DATA_ITEM 31, LOAN_TYPE 10, LOAN_PURPOSE 43, LENDER_TYPE 4 |
| `RES_DWELL`, `RES_DWELL_ST` | 1.0.0 | Residential dwellings: medians, stock value | 2002-01 to 2026-06 | Q | 5,616 | MEASURE 4, REGION 26 |
| `RPPI` | 1.0.0 | Residential Property Price Index (**ended 2021**) | 2002-01 to 2021-12 | Q | 6,372 | MEASURE 3, PROPERTY_TYPE 3, REGION 26 |
| `BUILDING_ACTIVITY` | 1.0.0 | Building Activity | 1955-01 to 2026-06 | Q | 673,139 | MEASURE 8, REGION 10, PRICE_ADJ 3, BLD_WORK_TYPE 9, SECTOR_OWN 3, TYPE_BLDG 32 |
| `ERP_Q` | 1.0.0 | Quarterly population | 1981-07 to 2026-03 | Q | 1.74M | MEASURE 3, SEX 3, AGE 136, REGION 10 |
| `MIN_EXP`, `PET_EXP` | | Mining and petroleum exploration | see the jsonl | Q | | |
| `AUSTRALIAN_INDUSTRY`, `SECURITISERS`, `MODELLERS_DB`, `LABOUR_ACCT_Q`, `TRADE_SERV_*` | | | see the jsonl | | | |

Attributes on the flows checked: `UNIT_MEASURE`, `OBS_STATUS`, `DECIMALS`, `OBS_COMMENT`, `BASE_PERIOD`, `UNIT_MULT`.

## How to call it
- Base: `https://data.api.abs.gov.au/rest/`. No key. NSI Web Service 8.19.
- Catalogue: `GET dataflow/ABS?detail=allstubs` with `Accept: application/vnd.sdmx.structure+json`.
- Structure: `GET dataflow/ABS/CPI/2.0.0?references=all`, availability and observation count: `GET
  availableconstraint/ABS,CPI,2.0.0/all/all/all?mode=exact`.
- Data: `GET data/ABS,LF/M13.3.1599.20.AUS.M?startPeriod=2026-08&format=csvfile` gives
  `DATAFLOW,MEASURE,SEX,AGE,TSEST,REGION,FREQ,TIME_PERIOD,OBS_VALUE,UNIT_MEASURE,UNIT_MULT,OBS_STATUS,OBS_COMMENT,DECIMALS`.
- Traps: a version matters where a flow has been rebased (`CPI,2.0.0`, the unversioned call takes the latest). The monthly
  CPI moved from `CPI_M` into `CPI` 2.0.0. `TEST` is a real dataflow id.
- Refresh with `tools/source-inventory/abs.py`.

## What the app reads
`research_abs`, row at `src/SourcesPage.cpp:552`; Countries › Australia, `src/CountryData.cpp:1610`, keeps the time and
value of `format=csvfile`. Keys in `resources/country_indicators.json`, provider `abs`:
- `LF`: `M13.3.1599.20.AUS.M` (unemployment rate), `M3` (employed), `M6` (unemployed), `M9` (labour force), `M12`
  (participation), `M16` (employment rate), `M1` (full-time), `M2` (part-time).
- `ANA_AGG`: `M2.GPM.20.AUS.Q`, `M1.GPM.20.AUS.Q` (GDP), `M5.GPM_PHW.20.AUS.Q` (per hour worked), `M7.HSR.20.AUS.Q`
  (saving ratio).
- `JV`: `M1.7.TOT.20.AUS.Q`. `HSI_M`: `8.TOT.CUR.20.AUS.M`.
- `CPI,2.0.0`: `3.10001.10.50.M`, `2.999901.20.50.M`, `1.10001.10.50.M`, `3.999902.20.50.M` (trimmed mean).

## Not read yet
- Countries › Australia: `WPI`, `ITGS`, `BOP`, `PPI`, `BUILDING_ACTIVITY`, `LEND_HOUSING`, `RES_DWELL`, `CAPEX`, `QBIS`.
- Supply chains and Map trade arcs: `MERCH_EXP`, `MERCH_IMP` (by SITC, by country, monthly).
- Desk: `CPI` by the 166 groups and capital cities for seasonal views; `LF_UNDER` underemployment.

## Calls made
- `GET https://data.api.abs.gov.au/rest/dataflow/ABS?detail=allstubs` → 200, 603 KB (twice, once by the script)
- `GET https://data.api.abs.gov.au/rest/dataflow/all?detail=allstubs` → 200, 792 KB (XML)
- `GET https://data.api.abs.gov.au/rest/dataflow/ABS/CPI/2.0.0?references=all` → 200, 106 KB
- `GET https://data.api.abs.gov.au/rest/availableconstraint/ABS,CPI,2.0.0/all/all/all?mode=exact` → 200, 2.4 KB
- `GET https://data.api.abs.gov.au/rest/data/ABS,LF/M13.3.1599.20.AUS.M?startPeriod=2026-08&format=csvfile` → 200, 249 B
- `GET dataflow/ABS/<id>/<version>?references=all` and `GET availableconstraint/ABS,<id>,<version>/all/all/all?mode=exact`
  for the 256 flows of `catalog/abs_structures.jsonl` → all 200
