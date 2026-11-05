---
id: ecb
name: European Central Bank
rows: [fx, research_ecb]
catalogue: ["European Central Bank"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# European Central Bank

The ECB Data Portal: money, credit and bank rates, exchange rates, the yield curve, financial market indicators, surveys and banking statistics of the euro area and its members. Its site adds reference rate files, feeds and calendars. **The inventory is incomplete: the portal answered many calls with a 504, listed below.**

## Contents
**105 dataflows.** All with DSD, series count, frequencies, first and last period, how the range was obtained, and dimensions: [`catalog/ecb.tsv`](catalog/ecb.tsv). Dimensions with labels: [`catalog/ecb_structures.jsonl`](catalog/ecb_structures.jsonl).

How complete each flow is:
- **78 flows have structure and series count** (2.8 million series in all). Their ranges: all series for 23, five series spread through the flow for 44, and 11 lost their first or last call, which is left empty rather than guessed. A sampled range may be narrower than the real one.
- **11 have dimensions but no series count:** `CLIFS DD MIR OFI RA RESC RESH RESV SSI STP SUR`. The numbers shown are the size of the code lists, not what is used (`DD` answered 404 for its keys).
- **16 have only a name and a DSD, their structure call failing twice:** `CAR CCP CMT DWA ECS FVC IFI MFI OMO PCP PFB PFBM RESR RPP SESFOD SHS`.

Flows that matter most (series; range as in the tsv):

| Flow | Name | Series | Freq | First | Last | Dimensions (codes used) |
|---|---|---|---|---|---|---|
| `BSI` | Balance Sheet Items | 69,011 | A M Q | 1980-01 | call failed | FREQ(3) REF_AREA(31) ADJUSTMENT(2) BS_REP_SECTOR(13) BS_ITEM(105) MATURITY_ORIG(17) DATA_TYPE(11) COUNT_AREA(38) BS_COUNT_SECTOR(42) CURRENCY_TRANS(20) BS_SUFFIX(10) |
| `MIR` | MFI Interest Rate Statistics | keys 504 | M | see below | | FREQ REF_AREA BS_REP_SECTOR BS_ITEM MATURITY_NOT_IRATE DATA_TYPE_MIR AMOUNT_CAT BS_COUNT_SECTOR CURRENCY_TRANS IR_BUS_COV |
| `ILM` | Internal Liquidity Management | 1,276 | D M W | 2016-06, sampled | 2026-W44 | FREQ(3) REF_AREA(23) BS_REP_SECTOR(3) BS_ITEM(75) COUNT_AREA(3) CURRENCY_TRANS(5) |
| `EXR` | Exchange Rates | 4,027 | A D H M Q | call failed | 2026-S1 | FREQ(5) CURRENCY(54) CURRENCY_DENOM(62) EXR_TYPE(12) EXR_SUFFIX(6) |
| `YC` | Yield curve | 2,165 | B | 2004-09-06 | 2026-11-05 | FREQ(1) REF_AREA(1) CURRENCY(1) PROVIDER_FM(1) INSTRUMENT_FM(3) PROVIDER_FM_ID(1) DATA_TYPE_FM(1083) |
| `EST` | Euro Short-Term Rate | 15 | B | 2019-10-01, sampled | 2026-11-05 | FREQ(1) BENCHMARK_ITEM(7) DATA_TYPE_EST(11) |
| `SPF` | Survey of Professional Forecasters | 506,458 | A Q | 2013, sampled | 2028 | FREQ REF_AREA FCT_TOPIC FCT_BREAKDOWN FCT_HORIZON SURVEY_FREQ FCT_SOURCE |
| `FM` | Financial market data | 115 | A B D M Q | 1900-01 | 2026-Q3 | FREQ(5) REF_AREA(6) CURRENCY(6) PROVIDER_FM(3) INSTRUMENT_FM(7) PROVIDER_FM_ID(52) DATA_TYPE_FM(9) |
| `CISS` | Composite Indicator of Systemic Stress | 60 | D M | 1973-01-05 | 2026-11-04 | FREQ(2) REF_AREA(20) PROVIDER_FM_ID(20) DATA_TYPE_FM(2) … |
| `CES` | Consumer Expectations Survey | 3,912 | M Q | 2020-04 | 2026-Q3 | FREQ(2) REF_AREA(13) CES_BREAKDOWN(9) CES_VARIABLE(29) CES_ANSWER(7) CES_DENOM(7) … |
| `EWT` | ECB wage tracker | 12 | A M Q | 2013, sampled | 2027-Q3 | FREQ(3) EWT_CONCEPT(4) TRANSFORMATION(2) … |
| `HICP` | Indices of Consumer Prices | 90,177 | A M | 1999-01, sampled | 2026-09 | FREQ(2) REF_AREA(32) ADJUSTMENT(2) ICP_ITEM(491) DATA_PROVIDER(2) ICP_SUFFIX(17) |
| `BLS` | Bank Lending Survey | 23,491 | Q | 2022-Q2, sampled | 2026-Q4 | see tsv |
| `SAFE` | Access to Finance of SMEs | 292,659 | H Q | 2009-S1, sampled | 2026-S1 | see tsv |
| `CBD2` | Consolidated Banking data | 66,139 | A Q | 2007 | 2026-Q1 | REF_AREA(30) CB_ITEM(352) … |
| `SUP` | Supervisory Banking Statistics | 28,344 | H Q | 2015-Q2, sampled | 2026-S1 | see tsv |
| `TGB` | Target Balances | 46 | M | 2001-01 | call failed | FREQ(1) REF_AREA(23) … |
| `WTS` | Trade weights | 60,383 | A | 1995, sampled | 2024 | REF_AREA(64) COUNT_AREA(66) TRADE_WEIGHT(4) … |
| `RTD` | Real Time Database | 278 | A M Q | 1913-01 | 2026-Q3 | REF_AREA(3) RT_ECON_CONCEPT(247) RT_DENOM(8) … |
| `AME` | AMECO copy | 124 | A | 1960 | 2027 | AME_REF_AREA(31) … |
| `IRS` | Interest rate statistics | 40 | M | 1983-05 | 2026-09 | REF_AREA(33) IR_TYPE(3) … |
| `MMSR` | Money Market Statistical Reporting | 446 | B Q | call failed | 2026-Q2 | MATURITY(28) SETTL_PERIOD(17) DATA_TYPE_MM(10) … |

Some flows seem to stop early, on five sampled series only: `BOP` 2014-Q2, `GST` 2014-Q1, `ESA` 2014-Q2, `MPD` 2018. Check the last period before use.

For `MIR`, one series was checked directly: France's `M.FR.B.A2C.A.R.A.2250.EUR.N` runs 2000-01 to 2026-09.

**Site files**, `www.ecb.europa.eu`:
- Reference rates: `/stats/eurofxref/eurofxref-daily.xml`, `eurofxref-hist-90d.xml`, `eurofxref-hist.zip`. The zip's CSV has `Date` and about 41 currency columns (old ones such as CYP, EEK included), 7,129 days, 1999-01-04 to 2026-11-04.
- RSS: 33 currency feeds `/rss/fxref-<cur>.html`, and `blog, operations, press, procurements, pub, rbu, statpress, wppub, yc`.
- Governing Council calendar: `/press/calendars/mgcgc/html/index.en.html`.

## How to call it
- Base `https://data-api.ecb.europa.eu/service`. No key.
- Catalogue: `/dataflow/ECB`. Structure: `/dataflow/ECB/EXR?references=all` (with a `<FLOW>_CONSTRAINTS` constraint, also at `/contentconstraint/ECB/EXR_CONSTRAINTS`).
- Series keys: `/data/EXR?detail=serieskeysonly&format=csvdata`.
- Data: `/data/MIR/M.FR.B.A2C.A.R.A.2250.EUR.N?format=csvdata&lastNObservations=1`. Columns `KEY`, the dimensions, `TIME_PERIOD, OBS_VALUE`; more with no `detail=dataonly`. `startPeriod` works (`src/CountryData.cpp:2072`).
- **Traps:**
  - A 504 HTML page after 30 seconds on heavy or wide calls: `?detail=allstubs`, `detail=nodata` and series keys of many flows. Some passed on a retry (`SPF`, `STS`, `TRD`, `SUP`, `SAFE`, `BLS`), others failed repeatedly.
  - After hours of sequential calls, even single series failed (`EXR/D.USD.EUR.SP00.A`, `YC`, `BSI`, `ILM`) while one `MIR` series answered. The portal's load may have been high or these calls may have added to it; **the job was stopped there rather than pressed.** Repeat the card's gaps on another day, more slowly.
  - First and last calls on a whole flow take long: `WTS`, 60,000 series, took 40 seconds.
- `robots.txt` of the site closes language variants of content pages and names a sitemap.

## What the app reads
**`research_ecb`** (`src/SourcesPage.cpp:502`), Economy › Countries and › Indicators: 13 rows of `resources/country_indicators.json`, one country (`src/CountryData.cpp:1328`) or all (`src/CountryData.cpp:2071`), keeping period and value.
- `BSI`: `M.U2.Y.V.M10.X.1.U2.2300.Z01.E`, `M20`, `M30` (M1, M2, M3), `M.U2.Y.U.A20T.A.1.U2.2240.Z01.E` and `M.{ECB}.N.A.A20.A.1.U2.2240.Z01.E` (loans).
- `MIR`: `M.{ECB}.B.A2A.A.R.A.2240.EUR.N` (lending), `…A2C.A.R.A.2250…` (mortgage), `…L22.A.R.A.2250…` (deposit).
- `ILM`: `W.U2.C.T000000.Z5.Z01`, `M.{ECB}.N.T000000.Z5.Z01` (balance sheet).

**The same row, Desk:** `EST/B.EU000A2X2A25.WT` (€STR, `src/DeskViewsMacro.cpp:454`), `YC/B.U2.EUR.4F.G_N_A.SV_C_YM.SR_<tenor>` (`src/DeskViewsMacro.cpp:523`), `SPF/Q.U2.HICP+RGDP+UNEM.POINT.…` (`src/DeskViewsMacro.cpp:1315`), the meeting dates of `mgcgc` (`src/DeskData.cpp:68`), the press feed `/rss/press.html` (`src/DeskViewsTech.cpp:582`).

**`fx`** (`src/SourcesPage.cpp:391`): the ECB's reference rates, read through Frankfurter's v1, not from the ECB (`frankfurter.md`, `src/FxData.cpp:68`). The diagram draws it to the ECB (`src/SourceFlow.cpp:184`).

## Not read yet
- **Forex:** `eurofxref-hist.zip` directly, in case Frankfurter's v1, marked deprecated, goes; `EXR` for the effective exchange rate types (`EXR_TYPE` 12); `WTS` for trade weights.
- **Desk › Rates, Overview:** `FM` (market rates), `CISS` (daily stress index), `MMSR`, `TGB`.
- **Desk › Forecasts, Economy:** `CES` (consumers' expectations), `EWT` (wage tracker with a forward horizon, to 2027), `BLS`, `SAFE`, `RTD` (data as first published).
- **Economy › Countries:** `HICP`, `STS`, `TRD`, `CBD2`, `SUP`, `SEC`.

## Calls made
All on 2026-11-05 with `User-Agent: Deepwater/1.0`.
- `GET https://data-api.ecb.europa.eu/service/dataflow/ECB?detail=allstubs` → 504, 12,059 bytes. `GET …/dataflow/ECB` → 200, 39,607 bytes, then each run of the script.
- `GET …/dataflow/ECB/EXR?references=all` → 200, 497,308 bytes. `GET …/contentconstraint/ECB/EXR_CONSTRAINTS` → 200, 5,193 bytes.
- `GET …/data/EXR?detail=serieskeysonly` → 504. `GET …/data/WTS?detail=serieskeysonly&format=csvdata` → 200, 3,100,592 bytes. `GET …/data/EXR?detail=serieskeysonly&format=csvdata` → 200, 157,107 bytes. `GET …/data/EXR?detail=nodata&format=csvdata` → 504. `GET …/data/WTS?firstNObservations=1&detail=dataonly&format=csvdata` → 200, 4,049,304 bytes, 40 seconds.
- `tools/source-inventory/ecb.py`, sequential, two seconds apart: for each of the 105 flows `/dataflow/ECB/<id>?references=all`, `/data/<id>?detail=serieskeysonly&format=csvdata`, and `firstNObservations` and `lastNObservations=1` on the flow or on five series of it. Dates kept, values dropped. Results per flow in the jsonl: `error`, `series_error`, `period_errors`. Then `--repair`, stopped by hand part-way through the list, and `--retry-errors`, a budget of 12 minutes, in which `MIR` and `RPP` again gave a 504 and `CPP, SAFE, SPF, STS, SUP, TRD, BLS, BNT` passed. The `period_basis` column was added to the tsv by a one-off step; a rerun of the script writes the table without it.
- `GET …/data/EXR/D.USD.EUR.SP00.A?firstNObservations=1&detail=dataonly&format=csvdata` → 504. `GET …/data/MIR/M.FR.B.A2C.A.R.A.2250.EUR.N?firstNObservations=1&detail=dataonly&format=csvdata` → 200, 223 bytes; `lastNObservations=1` → 200, 223 bytes; values not kept. `GET …/data/SPF/Q.U2.HICP.POINT.2026.Q.?lastNObservations=1&…` → 504. `GET …/data/BSI/M.U2.Y.V.M30.X.1.U2.2300.Z01.E?firstNObservations=1&…` → 504. `GET …/data/ILM/W.U2.C.T000000.Z5.Z01?firstNObservations=1&…` → 504. `GET …/data/YC/B.U2.EUR.4F.G_N_A.SV_C_YM.SR_10Y?lastNObservations=1&…` → 504.
- `GET https://www.ecb.europa.eu/stats/eurofxref/eurofxref-daily.xml` → 200, 1,547 bytes. `…/eurofxref-hist.zip` → 200, 642,539 bytes; header and dates kept. `…/eurofxref-hist-90d.xml` → 200, 69,509 bytes.
- `GET https://www.ecb.europa.eu/robots.txt` → 200, 1,141 bytes. `GET …/rss/press.html` → 200, 5,901 bytes. `GET …/home/html/rss.en.html` → 200, 105,137 bytes. `GET …/press/calendars/mgcgc/html/index.en.html` → 200, 106,176 bytes.
