---
id: european_commission
name: European Commission, DG Economic and Financial Affairs (AMECO, business and consumer surveys, forecasts)
rows: []
catalogue: ["European Commission"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# European Commission

DG ECFIN's own statistics: AMECO (annual macro data with the Commission's forecasts), the business and consumer surveys, the forecast tables and tax-and-benefit indicators. All are served by **Redisstat**, an SDMX service on the same platform as Eurostat's, which is separate from Eurostat and has its own agency, `ECFIN`.

## Contents
**303 dataflows**, 211 without the saved views (`X$Y` is a view of `X`, 92 of them). All listed with series, observations, first and last period, last update and dimensions: [`catalog/european_commission.tsv`](catalog/european_commission.tsv). Structures with labels: [`catalog/european_commission_structures.jsonl`](catalog/european_commission_structures.jsonl).

| Family | Flows | Observations | What |
|---|---|---|---|
| `AMECO_CHAPTER<nn>-<nn>` | 106 | 1,427,062 | AMECO by chapter and sub-chapter; annual, 1960 to 2027 on the examples read, so forecasts included |
| `BCS_*` | 28 | 24,831,172 | Business and consumer surveys |
| `FDMS_*` | 62 | 99,336 | Forecast tables: GDP deflators, current account, net lending, structural balance, debt, HICP, effective exchange rates; annual, 1961 to 2027 on the example read |
| `TAB_*` | 14 | 15,789,566 | Tax and benefit: earnings, net replacement rate, inactivity, low wage and unemployment traps, tax wedge |
| `DF_AGEING` | 1 | 192,454 | Ageing Report, 2022 to 2070 |

Examples:
- `AMECO_CHAPTER01-01` Population: 358 series, 1960 to 2027, updated 2026-06-03; `FREQ(1) REF_AREA(52) TRANSFORMATION(1) AGG_METHOD(1) UNIT(1) REFERENCE(1) ITEM(7)`.
- `AMECO_CHAPTER06-01` Gross domestic product: 994 series, 45,408 observations; `REF_AREA(57)` in three-letter codes with `EU27` and `EA21`, `ITEM(8)` such as `OVGD`, `UVGD`, `PVGD`.
- `BCS_MAIN_INDICATORS`: 242 series, 1985-01 to 2026-10, monthly; `REF_AREA(35) ACTIVITY(1) INDICATOR(7) SEASONAL_ADJUST(1) FREQ(1)`, indicators `ESI, EEI, INDU_COF, SERV_COF, CONS_COF, RETA_COF, BUIL_COF`.
- `BCS_CONS_M`: 3,283 series, 1,130,525 observations, 1985-01 to 2026-10; `REF_AREA(35) INDICATOR(13) UNIT_MEASURE(7) SEASONAL_ADJUST(2)`.
- `BCS_CONS_SUB_5_6`: consumers' inflation perceptions and expectations by demographic category, 12,250 series, 1986-04 to 2026-10, `ACTIVITY(25) INDICATOR(2)`.

The 28 BCS flows: `BCS_MAIN_INDICATORS`, `BCS_UNC_INDICATORS` (uncertainty), `BCS_LH_INDICATORS` (labour hoarding), `BCS_INDU_M/Q`, `BCS_INDU_SUB_M/Q`, `BCS_MIG_M/Q`, `BCS_INDU_PINV/SINV/FINV`, `BCS_SERV_M/Q`, `BCS_SERV_SUB_M/Q`, `BCS_SERV_PINV/SINV/FINV`, `BCS_CONS_M/Q`, `BCS_CONS_SUB_M/Q`, `BCS_CONS_SUB_5_6`, `BCS_RETA_M`, `BCS_RETA_SUB_M`, `BCS_BUIL_M`, `BCS_BUIL_SUB_M`.

AMECO's list of variables is also a spreadsheet of about 430 rows (chapter, sub-chapter, AMECO code, description), dated 21 January 2025. Next update stated on the site: 17 November 2026 (Autumn 2026 forecast). AMECO changes only with each full forecast.

## How to call it
- Base `https://webgate.ec.europa.eu/ecfin/redisstat/api/dissemination`. No key. Description files: `/catalogue/catalogue.wadl`, `/sdmx/2.1/sdmx-rest.wadl`, `/sdmx/3.0/sdmx-rest.wadl`.
- Catalogue: `/sdmx/2.1/dataflow/ECFIN/all?detail=allstubs`. Each flow: `/sdmx/2.1/dataflow/ECFIN/<id>?references=descendants&detail=referencepartial`, whose annotations give `SERIES_COUNT, OBS_COUNT, OBS_PERIOD_OVERALL_OLDEST, OBS_PERIOD_OVERALL_LATEST, UPDATE_DATA`; availability `/sdmx/2.1/contentconstraint/ECFIN/<id>`.
- Data: `/sdmx/2.1/data/BCS_MAIN_INDICATORS?format=SDMX-CSV&lastTimePeriod=1` (columns `DATAFLOW, LAST UPDATE, REF_AREA, ACTIVITY, INDICATOR, SEASONAL_ADJUST, FREQ, TIME_PERIOD, OBS_VALUE`). With a key, one position per dimension: `AMECO_CHAPTER01-01/A.FRA.....`.
- Traps: AMECO area codes have three letters (`FRA`; `FR` gives 400) and the surveys two; a wrong number of dots gives 400; the catalogue's `toc/txt` and `metabase.txt.gz`, advertised by the WADL, give 404, unlike Eurostat's.
- Bulk files as TSV, SDMX-CSV and SDMX 2.1, and zipped txt, csv, xlsx of AMECO: pages on `economy-finance.ec.europa.eu`, read but not fetched.
- The catalogue's site `http://ec.europa.eu` answered 403 to the earlier probe; `economy-finance.ec.europa.eu` and Redisstat answer. The body's `api: web` undersells it.

## What the app reads
Nothing from Redisstat. The confidence indicators it reads through Eurostat (`ei_bssi_m_r2`, `ei_bsin_q_r2`; see `eurostat.md`) come from these surveys.

## Not read yet
- **Economy › Countries:** the Commission's own **forecasts to 2027** (`AMECO_CHAPTER*`, `FDMS_*`); structural balance `FDMS_UBLGAPS`, potential output in chapter 17.
- **Desk › Forecasts:** `FDMS_ZCPIH`, `FDMS_UBCA`, `FDMS_UDGG` by country; a public source of forecasts beside the ECB's survey.
- **Desk, Economy:** `BCS_CONS_SUB_5_6` (inflation expectations), `BCS_UNC_INDICATORS` (uncertainty), `BCS_LH_INDICATORS`, none of them in Eurostat's `ei_` family as read by the app.

## Calls made
All on 2026-11-05 with `User-Agent: Deepwater/1.0`. `R` is `https://webgate.ec.europa.eu/ecfin/redisstat/api/dissemination`.
- `GET https://economy-finance.ec.europa.eu/economic-research-and-databases/economic-databases/ameco-database_en` → 200, 83,011 bytes (twice).
- `GET https://economy-finance.ec.europa.eu/economic-research-and-databases/economic-databases/ameco-database/bulk-downloads-and-api-access_en` → 200, 72,309 bytes.
- `GET https://economy-finance.ec.europa.eu/document/download/5cb507d5-10ac-4470-ad93-66c91a89bd72_en?filename=Ameco%20Online%20list%20of%20variables%2020250121.xlsx` → 200, 33,490 bytes.
- `GET https://economy-finance.ec.europa.eu/robots.txt` → 200, 1,651 bytes.
- `GET https://economy-finance.ec.europa.eu/economic-forecasts-and-surveys/business-and-consumer-surveys/download-business-and-consumer-survey-data_en` → 404. `…/economic-forecasts-and-surveys/business-and-consumer-surveys_en` → 404.
- `GET R/catalogue/toc/txt?lang=en` → 404, 9 bytes; `?lang=EN` → 404. `GET R/catalogue/metabase.txt.gz` → 404, 9 bytes. `GET R/catalogue/catalogue.wadl` → 200, 15,836 bytes.
- `GET R/sdmx/2.1/dataflow/ECFIN/all?detail=allstubs` → 200, 186,808 bytes.
- `GET R/sdmx/2.1/contentconstraint/ECFIN/BCS_CONS_M` → 200, 15,704 bytes. `GET R/sdmx/2.1/dataflow/ECFIN/BCS_CONS_M?references=descendants&detail=referencepartial` → 200, 23,960 bytes.
- `GET R/sdmx/2.1/data/AMECO_CHAPTER1-01?format=SDMX-CSV&detail=serieskeysonly` → 404. `GET R/sdmx/2.1/data/AMECO_CHAPTER01-01/A.FRA......?format=SDMX-CSV&detail=dataonly` → 400. `GET R/sdmx/2.1/data/AMECO_CHAPTER01-01/A.FR.....?format=SDMX-CSV&lastTimePeriod=1` → 400. `GET R/sdmx/2.1/data/AMECO_CHAPTER01-01/A.FRA.....?format=SDMX-CSV&lastTimePeriod=1` → 200, 47,631 bytes; header kept (`DATAFLOW, LAST UPDATE, FREQ, REF_AREA, TRANSFORMATION, AGG_METHOD, UNIT, REFERENCE, ITEM, TIME_PERIOD, OBS_VALUE, OBS_STATUS, OBS_CONF, COMMENT, UNIT_MULT`).
- `GET R/sdmx/2.1/data/BCS_MAIN_INDICATORS?format=SDMX-CSV&lastTimePeriod=1` → 200, 7,378,088 bytes; header kept.
- `tools/source-inventory/european_commission.py`: the two structure calls for each of the 211 flows → no failure.
