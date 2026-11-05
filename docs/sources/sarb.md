---
id: sarb
name: South African Reserve Bank
rows: [nat_south_africa]
catalogue: ["South African Reserve Bank"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# South African Reserve Bank

The JSON web service behind resbank.co.za's statistics pages: market rates, exchange rates, gold, money and banking,
credit, national government finance, capital market, business cycle indicators. South Africa only, with rand crosses.
There is no published API documentation; the endpoints below come from hidden `input` fields of the Bank's own pages.

## Contents
Under `https://custom.resbank.co.za/SarbWebApi/WebIndicators/`:

| Endpoint | Holds | Fields |
|---|---|---|
| `HomePageRates` | 13 latest values: CPI, PPI, policy rate, prime, 2 bonds, Sabor, Zaronia, 4 rand rates, NEER | `Name`, `SectionId`, `SectionName`, `TimeseriesCode`, `Date`, `Value`, `UpDown`, `FormatNumber`, `FormatDate` |
| `CurrentMarketRates` | 24 latest values: money market (13, including T-bills, NCDs, Jibar), capital market (4), rand rates (5), gold (2) | same |
| `HistoricalExchangeRatesDaily/` | 34 latest values: 25 crosses, NEER, USD crosses, rates | same |
| `HistoricalExchangeRatesMonthly/` | 3: `BOP5396M` NEER, `BOP5398M` REER, `MMSM001R` real prime | same |
| `OtherIndicators/` | 8 latest: foreign debt, government balance, real GDP growth, saving, current account, import cover, M3, private credit | `TsCode`, `MeasureName`, `TheValue`, `ValueDate`, `Description` |
| `OtherIndicators/HistoricalDatesOfRateChanges/` | `MRDPRIME`, `MRDREPOR`: dates the rates changed | same |
| `Shared/GetTimeseriesObservations/<code>` | The last 25 observations of any code above | `Period`, `Timeseries`, `Description`, `Value`, `FormatNumber`, `FormatDate` |
| `ReleaseOfSelectedData/` | The list of 17 tables below | `DataType`, `Indicator`, `LatestDate`, `LastPeriod` |
| `ReleaseOfSelectedData/MonthlyIndicators/<table>` | That table's latest figures | `TimeSeriesCode`, `CategoryCode`, `CategoryName`, `MeasureName`, `Value`, `Period`, `Description` |
| `ReleaseOfSelectedData/MonthlyIndicatorsAll/<table>` | **That table's whole history** | same; `Period` as `1960/01/31 00:00:00` |

**217 entries, 192 distinct series**, in [`catalog/sarb.tsv`](catalog/sarb.tsv): table, category, code, measure, first,
last, observation count, description.

| Table | Name | Series | First to last | Categories |
|---|---|---|---|---|
| `MRDMA` | Money and banking | 33 | 1960-01 to 2026-09 | Monetary aggregates, counterparts of M3, credit, 12-month changes |
| `MRDBM` | Banks and mutual banks | 21 | 1964-01 to 2026-09 | Banks, Land Bank |
| `MRDIE` | International economic data | 18 | 1960-01 to 2026-09 | Gold price, reserves, exchange rates, forex turnover |
| `MRDFG` | National government finance | 14 | 1990-01 to 2026-09 | Revenue, expenditure, borrowing, financing |
| `MRDCM` | Capital market | 14 | 1949-01 to 2026-09 | Shares, fixed interest, derivatives, non-resident flows |
| `MRDEI` | Economic indicators, SA | 12 | 1946-01 to 2026-10 | Composite business cycle indicators, sales, production |
| `MRGMA`, `MRGIE`, `MRGCM`, `MRGFG`, `MRGEI` | Graph series | 5, 5, 4, 3, 7 | 1960 to 2026 | |
| `CDACSQ`, `CDACSM` | Credit detail, Q and M | 17, 24 | 1965 to 2026-09 | Households, corporates |
| `CDASA` | Seasonally adjusted credit | 9 | 1965-03 to 2026-09 | |
| `CDADS` | Deposit detail | 22 | 1965-03 to 2026-09 | Households, corporates, financial corporates |
| `CDACA`, `CDACM3` | Securitisation, counterparts of M3 | 4, 5 | 1965-03 to 2026-09 | |

Not listed in those tables: national accounts and balance of payments as series (`KBP*` codes are seen in
`OtherIndicators/`). resbank.co.za's Online Statistical Query page mentions the Quarterly Bulletin series, but no
endpoint for it appears in its page source.

## How to call it
- `GET https://custom.resbank.co.za/SarbWebApi/WebIndicators/HomePageRates` with `Accept: application/json` (XML
  otherwise). No key.
- `GET .../Shared/GetTimeseriesObservations/CPI1000F`: 25 rows, newest first.
- `GET .../ReleaseOfSelectedData/MonthlyIndicatorsAll/MRDMA`: 12 MB, 23,663 rows.
- Traps: `Value` in `MonthlyIndicators*` is a formatted string with a no-break space as thousands separator, while
  `HomePageRates` gives a number. `MonthlyIndicators/` without a table answers 404. The root and `/Help` give 403 and 404.
  `resbank.co.za` itself reset a connection in the first site probe, and answered 200 here.

## What the app reads
- `nat_south_africa`, row at `src/SourcesPage.cpp:724`; `src/NationalSources.cpp:135`, `HomePageRates` only: `CPI1000F`
  (CPI), `PPI1000F` (PPI), `MMRD002A` (policy rate), keeping `TimeseriesCode`, `Date`, `Value`
  (`src/NationalSources.cpp:152`). Shown on the Map through `src/KeyIndicators.cpp:396`.

## Not read yet
- **History**: `src/NationalSources.cpp:134` says "no history: the previous value comes from the cache", yet
  `GetTimeseriesObservations/<code>` gives 25 points and `MonthlyIndicatorsAll/<table>` the whole series.
- Countries › South Africa, which has no SARB provider today: `MRDMA` (M0 to M3, credit), `MRDIE` (reserves, gold),
  `MRDEI` (leading indicators), `MRDFG` (government finance), `CDACSM` (household and corporate credit).
- `CurrentMarketRates` for Desk › Rates: T-bills, Jibar, Zaronia, bond yields.

## Calls made
- `GET https://custom.resbank.co.za/SarbWebApi/WebIndicators/HomePageRates` → 200, 2.7 KB
- `GET .../WebIndicators/CurrentMarketRates` → 200, 5.3 KB
- `GET .../WebIndicators/EndOfDayRates` → 404
- `GET .../WebIndicators/Shared/GetTimeseriesObservations/CPI1000F` → 200, 4.7 KB; `.../MMRD002A` → 200, 6.7 KB
- `GET .../WebIndicators/OtherIndicators/` → 200, 3.5 KB; `.../OtherIndicators/HistoricalDatesOfRateChanges/` → 200, 896 B
- `GET .../WebIndicators/ReleaseOfSelectedData/` → 200, 1.9 KB; `.../MonthlyIndicators/` and `.../MonthlyIndicatorsAll/`
  → 404; `.../MonthlyIndicators/MRDMA` → 200, 80 KB
- `GET .../MonthlyIndicatorsAll/<table>` for the 17 tables above → all 200, 87 KB to 11.9 MB
- `GET .../WebIndicators/HistoricalExchangeRatesDaily/` and `.../HistoricalExchangeRatesMonthly/` → 200
- `GET https://custom.resbank.co.za/SarbWebApi/Help` → 404; `.../swagger/docs/v1` → 404; `.../SarbData/ReleaseOfSelectedData` → 404
- `GET https://custom.resbank.co.za/` → 403; `.../robots.txt` → 404
- `GET https://www.resbank.co.za/en/home/what-we-do/statistics/releases/selected-statistics` → 200, 85 KB
- `GET https://www.resbank.co.za/en/home/what-we-do/statistics/releases/online-statistical-query` → 200, 111 KB
- `GET https://www.resbank.co.za/en/home/what-we-do/statistics/key-statistics` → 200, 82 KB
- `GET https://www.resbank.co.za/en/home/what-we-do/statistics/key-statistics/selected-historical-rates` → 200, 85 KB
