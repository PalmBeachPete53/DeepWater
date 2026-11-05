---
id: ilo
name: International Labour Organization (ILOSTAT)
rows: [research_ilo]
catalogue: ["International Labour Organization (ILO)", "International Labor Organization (ILO)", "ILO"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# International Labour Organization

ILOSTAT: labour force surveys, wages, hours, informality, migration, labour income, and also consumer prices and modelled estimates, for every country.

## Contents
**1,964 data sets** (indicator + frequency) from **1,213 indicators**; all in [`catalog/ilo.tsv`](catalog/ilo.tsv), with label, breakdowns, unit, first and last year, last update, records and number of areas.

- Frequency: annual 1,204 (1914 to 2030, modelled projections included), quarterly 590 (1914 to 2026), monthly 170 (1914 to 2026).
- Areas: 327 `ref_area` codes annually, 207 quarterly and 207 monthly (`/metadata/toc/ref_area`, 741 rows); an indicator reaches up to 319.
- Size: 431 million records over the table of contents. Last updates in it run from 2022 to 4 November 2026.

Databases (data sets): Labour Force Statistics `LFS` 405, Wages and Working Time `COND` 262, Education and Mismatch `EMI` 253, Rural and Urban `RURBAN` 215, Disability `DLMI` 191, Gender `GEND` 182, Worker and Sector Profiles `PROFILES` 96, Labour Migration `ILMS` 71, Youth `YouthSTATS` 68, Work Statistics 19th ICLS `WORK` 56, Modelled Estimates `ILOEST` 47, Child Labour `CHILD` 35, SDG `ILOSDG` 25, Safety and Health `OSH` 19, Sectoral `ILOSECTOR` 18, Prices, Costs and Currency Conversions `PRICES` 15, Industrial Relations `IRdata` 6.

Subjects with the most data sets: unemployment and underutilization 457, informal economy 450, employment 245, hours of work 146, earnings and income 129, employees 102, labour force 93, population 67, migrant stock 46, child labour 38.

Dimensions of a data file: `ref_area, source, indicator, sex, classif1, classif2, time, obs_value, obs_status, note_classif, note_indicator, note_source`. Dictionaries: `sex` has 4 codes, `classif1` 1,352.

## How to call it
- Base: `https://rplumber.ilo.org`, OpenAPI at `/openapi.json` (ILOSTAT API 1.1.0). No key.
- Catalogue: `/metadata/toc/indicator/?format=.csv&lang=en`, `/metadata/toc/ref_area/?format=.csv&lang=en`, dictionaries `/metadata/dic/?var=classif1&format=.csv&lang=en`.
- Data: `/data/indicator/?id=UNE_DEAP_SEX_AGE_RT_A&ref_area=FRA&format=.csv&timefrom=2024&type=code`. Filters: `ref_area, source, sex, classif1, classif2, time, timefrom, timeto, latestyear, best_source, type, region`. Also `/data/ref_area` (one country, every indicator), `/data/search` (marked experimental), `/aggregate/data`, `/aggregate/indicator`, `/aggregate/region`.
- Formats: `.csv`, `.dta`, Excel by the documentation's example. Trap: the CSV starts with a byte order mark (`src/CountryData.cpp:2104`). Ask with filters: the app's comment notes a whole indicator weighs tens of megabytes.

## What the app reads
**`research_ilo`** (`src/SourcesPage.cpp:514`), Economy › Countries and › Indicators: 17 rows of `resources/country_indicators.json`, one country (`src/CountryData.cpp:1822`) or every country (`src/CountryData.cpp:2100`), keeping area, time and value for both sexes at age 15+ unless the row's `classif` says otherwise.

`UNE_DEAP_SEX_AGE_RT_A`, `_Q` (unemployment, youth unemployment); `UNE_TUNE_SEX_AGE_NB_A`, `_Q` (unemployed persons); `EMP_DWAP_SEX_AGE_RT_A`, `_Q` (employment rate); `EMP_TEMP_SEX_AGE_NB_A`, `_Q` (employed persons, yearly change); `EAP_DWAP_SEX_AGE_RT_A`, `_Q` (participation); `EAP_TEAP_SEX_AGE_NB_Q` (labour force); `EAR_EMTA_SEX_NB_A` (gross monthly wages); `EAR_INEE_NOC_NB_A` (minimum wages).

## Not read yet
- **Countries, Prices:** `CPI_XCPI_COI_RT_M`, `CPI_NCYR_COI_RT_M`, `CPI_NCPD_COI_RT_M` (CPI by COICOP, about 199 areas, from 1914 and 1915), `CPI_NWGT_COI_RT_A` (country weights).
- **Countries, Labour:** `HOW_TEMP_SEX_ECO_NB_M`, `_Q`, `_A` (weekly hours by activity), `LAP_2GDP_NOC_RT_A` (labour income share, 276 areas), `GDP_2HRW_NOC_NB_A` and `GDP_211P_NOC_NB_A` (output per hour and per worker), `LAP_2LID_QTL_RT_A` (labour income by decile).
- **Forex, Countries:** `CCF_XOXR_CUR_RT_A`, `CCF_XPPP_CUR_RT_A` (exchange rate and PPP conversions).
- **Monthly series:** 170 data sets, listed with `grep -P '\tM\t' catalog/ilo.tsv`; the app reads only annual and quarterly ones.

## Calls made
All on 2026-11-05 with `User-Agent: Deepwater/1.0`.
- `GET https://rplumber.ilo.org/metadata/toc/indicator/?format=.csv&lang=en` → 200, 794,624 bytes.
- `GET https://rplumber.ilo.org/metadata/toc/ref_area/?format=.csv&lang=en` → 200, 161,768 bytes (twice).
- `GET https://rplumber.ilo.org/openapi.json` → 200, 28,615 bytes (twice).
- `GET https://rplumber.ilo.org/__docs__/openapi.json` → 200, 2,253 bytes, an HTML page.
- `GET https://rplumber.ilo.org/metadata/dic/?var=sex&format=.csv&lang=en` → 200, 85 bytes.
- `GET https://rplumber.ilo.org/metadata/dic/?var=classif1&format=.csv&lang=en` → 200, 86,889 bytes.
- `GET https://rplumber.ilo.org/metadata/?format=.json&lang=en` → 404, 114 bytes.
- `GET https://rplumber.ilo.org/data/indicator/?id=UNE_DEAP_SEX_AGE_RT_A&ref_area=FRA&format=.csv&timefrom=2024&type=code` → 200, 15,795 bytes; header row only kept.
- Not called: the catalogue's own test in `resources/data_sources.json` records `ilostat.ilo.org/data/` at 403 and `www.ilo.org` at 200; neither is needed, since the API answers.
