---
id: bok
name: Bank of Korea (ECOS)
rows: [ecos]
catalogue: ["The Bank of Korea", "Bank of Korea"]
access: free key
probed: 2026-11-05
confidence: called
---

# Bank of Korea (ECOS)

The Economic Statistics System: money, rates, national accounts, balance of payments, exchange rates, prices, firms' accounts, surveys, payments, and
much of the real economy republished from other offices. The API needs a free key. Its demonstration key `sample` answered every catalogue call used here,
so this card rests on real answers without the user's key.

## Contents
**844 entries in the table tree, 617 of them searchable tables**, complete in [`catalog/bok.tsv`](catalog/bok.tsv): code, parent, name, cycle, searchable, number of
item rows, and the cycle, start and end of the table's *first item row* only (often the annual one, so it isn't the table's whole span). The other 227 are folders.
Table cycles: A 251, M 231, Q 126, D 7, S 1, SM 1. Korea, plus section 9.

| Section | Searchable tables | Subsections |
|---|---|---|
| 1. Monetary Financial Statistics | 139 | Aggregates, deposits and loans, interest rates, the Bank's accounts, securities and government finance, household debt |
| 2. National Accounts | 157 | National income and product, input-output, flow of funds, national balance sheets, balance of payments, investment position and external debt |
| 3. Exchange Rate, Reserves, Trade | 24 | Exchange rates, external trade, trade indices, direct investment, reserves |
| 4. Price Indices | 33 | Producer, consumer, export and import, estate |
| 5. Financial Statement Analysis | 100 | Firms' indicators, balance sheet, income, cash flow, regional |
| 6. Sentiment Indices | 24 | Business, consumer, economic sentiment, **news sentiment (experimental)**, lending survey |
| 7. Payments Systems | 42 | Clearing, BOK-Wire, giro, BIS standard, credit cards, e-banking |
| 8. Industry, Household Economy, other | 71 | Industry, agriculture, manufacturing, construction, service, employment and wages, household |
| 9. Foreign Countries, North Korea | 27 | International key statistics, **economic indicators of North Korea** |

Some tables, with the number of item rows and the first row's span:

| Code | Table | Cycle | Item rows | First row |
|---|---|---|---|---|
| `817Y002` | Market Interest Rates (Daily) | D | 27 | D 1995-01-03 to 2026-11-04 |
| `731Y001` | Arbitraged rates of major currencies against won | D | 53 | D 1964-05-04 to 2026-11-05 |
| `722Y001` | Base rate and the Bank's lending rates | D | 48 | A 1999 to 2025 |
| `200Y102` | Main quarterly indicators, reference 2020 | Q | 53 | Q 1960 Q1 to 2026 Q3 |
| `301Y013` | Balance of Payments | M | 861 | A 1980 to 2025 |
| `732Y001` | International Reserves | M | 15 | A 1960 to 2025 |
| `901Y009` | Consumer Price indices | M | 1,743 | A 1965 to 2025 |
| `404Y014` | Producer Price Indices | M | 1,577 | A 1965 to 2025 |
| `403Y001` | Export Value Indices | M | 716 | A 1988 to 2025 |
| `901Y062` | House Price Index (KB) | M | 6 | M 1986-01 to 2026-10 |
| `901Y027` | Economically active population | M | 177 | A 2000 to 2025 |
| `901Y033` | Index of all industry production | M | 20 | A 2000 to 2025 |
| `512Y014` | Business tendency, national | M | 27 | M 2003-02 to 2026-11 |
| `511Y002` | Consumer tendency survey | M | 52 | M 2008-07 to 2026-10 |
| `161Y001` | M1 by type | M | 12 | A 2003 to 2025 |
| `104Y016` | Loans and discounts of banks | M | 132 | A 1960 to 2025 |
| `121Y002` | Lending rates, newly extended | M | 81 | A 1996 to 2025 |

`KeyStatisticList`: 101 headline statistics in classes such as Money Supply, Exchange Rates; fields `CLASS_NAME`, `KEYSTAT_NAME`, `DATA_VALUE`, `CYCLE`, `UNIT_NAME`.

## How to call it
- Base: `https://ecos.bok.or.kr/api/<service>/<key>/json/en/<from>/<to>/...`.
- Catalogue: `StatisticTableList/<key>/json/en/1/1000/`; fields `P_STAT_CODE`, `STAT_CODE`, `STAT_NAME`, `CYCLE`, `SRCH_YN`, `ORG_NAME`.
- Items: `StatisticItemList/<key>/json/en/1/2000/901Y009`; fields `GRP_CODE`, `GRP_NAME`, `ITEM_CODE`, `ITEM_NAME`, `P_ITEM_CODE`, `CYCLE`, `START_TIME`, `END_TIME`, `DATA_CNT`, `UNIT_NAME`, `WEIGHT`. One row
  per item and cycle, which is why item rows overstate the number of series.
- Data: `StatisticSearch/<key>/json/en/1/2/901Y009/M/202601/202602/0`; fields `STAT_CODE`, `ITEM_CODE1`..`4`, `ITEM_NAME1`..`4`, `UNIT_NAME`, `WGT`, `TIME`, `DATA_VALUE`. Periods `202609`, `2026Q3`, `2026`, `20261030`.
- With key `sample`, a call returns at most 10 rows, and `1/10`, `11/20`... page through it (87 calls for the tree).
- `https://ecos.bok.or.kr/api/#/` is a JavaScript documentation page. `www.bok.or.kr` itself forbids robots (body probe); `ecos.bok.or.kr` doesn't.

## What the app reads
- `ecos`, row at `src/SourcesPage.cpp:733`; `src/KeyedSources.cpp:239`: `901Y009`, item `0`, for the Map, computes the % over a year, keeps `TIME` and `DATA_VALUE`.
- Countries › South Korea, `src/CountryData.cpp:1347`, provider `ecos` in `resources/country_indicators.json`: `200Y102/Q/10111`, `901Y027/M/I61BC`, `901Y009/M/0`, `722Y001/M/0101000`, `817Y002/D/010210000`,
  `732Y001/M/99`, `301Y013/M/000000`, `512Y014/M/99988/BA`, `511Y002/M/FME/99988`, `901Y062/M/P63AC`.

## Not read yet
- Desk › Rates: the rest of `817Y002` (27 market rates). Forex: `731Y001`.
- Countries › South Korea: `403Y001`, `404Y014`, `104Y016`, `901Y033`, household debt in 1.6, flow of funds in 2.3.
- Section 6.4, the news sentiment index; section 9.2, North Korea.

## Calls made
- `GET https://ecos.bok.or.kr/robots.txt` → 200, 70 B
- `GET https://ecos.bok.or.kr/api/StatisticTableList/sample/json/en/<n>/<n+9>/` for the 844 rows → 87 calls, all 200
- `GET https://ecos.bok.or.kr/api/StatisticItemList/sample/json/en/1/1/<code>` for the 617 searchable tables → all 200
- `GET https://ecos.bok.or.kr/api/StatisticItemList/sample/json/en/1/10/901Y009` → 200, 3.3 KB
- `GET https://ecos.bok.or.kr/api/KeyStatisticList/sample/json/en/1/10/` → 200, 1.4 KB
- `GET https://ecos.bok.or.kr/api/StatisticSearch/sample/json/en/1/2/901Y009/M/202601/202602/0` → 200, 635 B
- `GET https://ecos.bok.or.kr/api/#/` → 200, 4.4 KB
