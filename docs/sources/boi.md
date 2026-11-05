---
id: boi
name: Bank of Israel
rows: []
catalogue: ["Bank of Israel"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# Bank of Israel

An SDMX 2.1 and 3.0 service (Fusion Edge Server) with the Bank's statistics and many of the national ones: exchange rates,
rates, the yield curve and inflation expectations, prices, national accounts, labour, trade, public sector, banks, payments.
The catalogue's `api` value ("web API (JSON)") is right in spirit, and its address (`www.boi.org.il`) isn't the API's.

## Contents
**56 dataflows**, all with their structure in [`catalog/boi_structures.jsonl`](catalog/boi_structures.jsonl): 13 in
`BOI.BankingSupervision`, 43 in `BOI.STATISTICS`, version 1.0. Every one has `SERIES_CODE` and `FREQ`, plus its own
dimensions; period and series count below come from availability and first and last observation calls. Israel only, with
counterpart areas in trade and FX.

`BOI.STATISTICS` (names translated from Hebrew where the source gives only Hebrew):

| Flow | Holds | Freq | First to last | Series | Main dimensions (codes) |
|---|---|---|---|---|---|
| `BR` | BOI interest rate | D | 1994-01-27 to 2026-11-05 | 5 | IR_FV_TYPE 16 |
| `TLB` | Telbor rates (ended) | D | 1999-11 to 2025-06 | 7 | IR_FV_TYPE 16 |
| `ZCM` | Inflation expectations, zero curve | D, M | 1992-01 to 2026-11-04 | 30 | NOMINAL_REAL 3, TIME_PERIOD_TYPE 3, DATA_TYPE 37, MODEL_TYPE 9, TIME_TO_MATURITY 490 |
| `EXR` | Exchange rates | D, W, M | 1948-05 to 2026-11-05 | 35 | BASE_CURRENCY 283, COUNTER_CURRENCY 283, UNIT_MEASURE 520, DATA_TYPE 14 |
| `PRI` | Price indices | A, M | 1951-09 to 2026-12 | 2,418 | MAIN_SUBJECT 5, SUBJECT 1,092, PRICE_GROUP 4, ADJUSTMENT 5, ACTIVITY 1,099 |
| `NA` | National accounts | A, Q | 1961 to 2026 Q2 | 560 | MAIN_SUBJECT 10, SUBJECT 296, ADJUSTMENT 5, PRICES 16, REF_SECTOR 326 |
| `LBM` | Labour market | A, Q, M, W, D | 1958 to 2026 Q3 | 640 | MAIN_SUBJECT 11, SUBJECT 37, ADJUSTMENT 5, ACTIVITY 1,099 |
| `FTR` | Foreign trade | A, Q, M | 1960 to 2026 Q3 | 4,008 | MAIN_SUBJECT 9, SUBJECT 33, TRADE_FLOW 4, COUNTERPART_AREA 881, ACTIVITY 1,099 |
| `BOP` | Balance of payments | A, Q, M | 1975 Q1 to 2026 Q2 | 112 | MAIN_SUBJECT 5, SUBJECT 349, TRADE_FLOW 4, PRICES 16 |
| `EXS` | External statistics | Q | 2004 Q1 to 2026 Q1 | 54 | |
| `PS` | Public sector | A, Q, M | 1990 to 2026 Q3 | 55 | SUBJECT 4, DATA_ITEM 408, PRICES 16 |
| `DEBT_AGG` | Debt and credit aggregates | Q | 1992 Q3 to 2026 Q2 | 248 | DEBT_CREDIT 3, VALUATION 25, BORROWING_SECTOR 326, LENDING_SECTOR 326 |
| `MAG` | Monetary aggregates | A, Q, M | 1982-12 to 2026 Q3 | 42 | DATA_ITEM 80, REF_SECTOR 326 |
| `ECON_IND` | Economic activity indicators | M | 1975-08 to 2026-09 | 65 | INDEX_TYPE 5, IND_COMPONENT 67 |
| `BTS_7` | Business Tendency Survey | M | 2010-12 to 2026-09 | 5 | CHAPTER 5, QUESTION 52, ST_IND 34 |
| `MNF`, `REV`, `CONS`, `REAL_ES_DF` | Industry, revenues, construction, real estate | M | 1959, 1991, 1994, 1994 to 2026 | 2, 2, 3, 8 | ACTIVITY 1,099, GEO_AREA 18, APT_SZE 17 |
| `CCP` | Credit card purchases (daily activity) | D | 2016-01 to 2026-10-31 | 42 | ACTIVITY 86, TRANSFORMATION 61 |
| `CAP` | Capital stock | A | 1910 to 2026 | 24 | MAIN_SUBJECT 8, SUBJECT 490 |
| `DEM` | Demography | M | 1970 to 2026-03 | 3 | POP_GR 16, SEX 2, AGE_GR 24 |
| `ENR` | Energy (ended) | D | 2002 to 2023-12 | 2 | INDICATOR 4 |
| `SECDWH` | Securities on TASE | D, M | 1992 to 2026-11-04 | 87 | COMP_NAME 426, TIME_TO_MATURITY |
| `MF`, `INSINV`, `INSINV2` | Mutual funds, institutional investors | M, A, Q, D | 2008, 1962, 2000 to 2026 | 125, 68, 7 | INV_GRP 29 |
| `DRV` | Derivatives, trade turnover | D, Q, A | 2008 to 2026 Q2 | 30 | INSTR_ASSET 438, REF_AREA 881 |
| `BIR`, `BIR_MRTG_99`, `CCIR`, `BMB_99`, `BBS_99`, `BFR_99` | Bank rates, mortgages, card companies, balance sheets, financial statements | M, Q | 1992 to 2026 | 30, 113, 19, 31, 26, 1,402 | REP_ENTITY 54, DATA_ITEM 164, COUNTERPART_SECTOR 326 |
| `ACC`, `BKN`, `ATM`, `CARDS`, `CHEQUES`, `MASAV`, `ZAHAV` | The Bank's accounts, notes, payments | M, D, A | 1995 to 2026 | 14, 27, 7, 663, 9, 25, 26 | |
| `EZER`, `EZER_PIK_MOS` | Helper series | | | 67, 25 | |

`BOI.BankingSupervision`: `BCAP`, `B_DGTL`, `B_OPNB`, `B_STF_SRV`, `CCFR`, `CRDT` (ended 2023), `FEES_DASH`, `FEES_TRF`
(ended 2022), `INTEREST_DASH`, `WEB_M`, `WEB_OP`, `WEB_Q`, `WEB_Q_CC`: see the jsonl.

## How to call it
- Base: `https://edge.boi.gov.il/FusionEdgeServer/sdmx/v2/`. No key.
- Catalogue: `GET structure/dataflow/all/all/latest?format=sdmx-json`.
- Structure: `GET structure/dataflow/BOI.STATISTICS/EXR/1.0?format=sdmx-json&references=all`. Availability: `GET
  availability/dataflow/BOI.STATISTICS/EXR/1.0?format=sdmx-json` (dimension values and a `series_count` annotation, no dates).
- Data: `GET data/dataflow/BOI.STATISTICS/BR/1.0?lastNObservations=1&format=csv`, columns `SERIES_CODE,FREQ,IR_FV_TYPE,
  DATA_SOURCE,TIME_COLLECT,CONF_STATUS,PUB_WEBSITE,UNIT_MEASURE,UNIT_MULT,TIME_PERIOD,OBS_VALUE,RELEASE_STATUS`.
- Traps: names may be Hebrew only (`names` in the jsonl). `PRI` runs to 2026-12 (forecast marks). `SERIES_CODE` codelists
  are shared, so a codelist size overstates a flow; `series_count` is the real one.

## What the app reads
Nothing: no row in `src/SourcesPage.cpp`. Israel's data reach the Map through OECD KEI, IMF and BIS.

## Not read yet
- Desk › Rates: `BR`, `ZCM` (a zero curve with inflation expectations, daily since 1992).
- Forex: `EXR`, daily official rates since 1948.
- Countries › Israel: `PRI`, `NA`, `LBM`, `FTR`, `BOP`, `PS`, `MAG`, `ECON_IND`.
- `CCP`: a daily card-spending index, rare among central banks.

## Calls made
- `GET https://edge.boi.gov.il/FusionEdgeServer/sdmx/v2/structure/dataflow/all/all/latest?format=sdmx-json` → 200, 58 KB
- `GET .../structure/dataflow/BOI.STATISTICS/EXR/1.0?format=sdmx-json&references=all` → 200, 1.2 MB
- `GET .../availability/dataflow/BOI.STATISTICS/EXR/1.0?format=sdmx-json` → 200, 2.1 KB
- `GET .../data/dataflow/BOI.STATISTICS/BR/1.0?lastNObservations=1&format=csv` → 200, 445 B
- `GET .../structure/dataflow/<agency>/<id>/1.0?format=sdmx-json&references=all`, `GET .../availability/dataflow/<agency>/<id>/1.0?format=sdmx-json`,
  `GET .../data/dataflow/<agency>/<id>/1.0?firstNObservations=1&format=csv` and `...?lastNObservations=1&format=csv` for the
  56 flows → all 200
