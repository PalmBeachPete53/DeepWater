---
id: un
name: United Nations (UN Comtrade, UNdata, SDG database)
rows: [comtrade]
catalogue: []
access: free key
probed: 2026-11-05
confidence: called
---

# United Nations

Three services of the UN Statistics Division: **Comtrade** (goods and services trade by reporter, partner and product), **UNdata** (an SDMX service over national accounts, energy, SDG and other compilations) and the **SDG database**. Comtrade's catalogue and preview answer without a key; the keyed `data/v1/get` was not called because the key was not read, so its limits are not checked here.

## Contents

### Comtrade
Data availability from `getDA`, one row per reporter, period and classification:

| Type, frequency | Data sets | Reporters | First | Last |
|---|---|---|---|---|
| Goods, annual (`C/A/HS`) | 5,488 | 215 | 1988 | 2025 |
| Goods, monthly (`C/M/HS`) | 27,446 | 170 | 2000-01 | 2026-09 |
| Services, annual (`S/A/EB`) | 4,355 | 204 | 2000 | 2025 |

Goods sets by HS vintage, annual: H1 901, H2 888, H4 841, H3 840, H5 765, H0 739, H6 514. Services: EB02 2,020, EB10 1,800, EB10S 535. `C/A/SS` (SITC) answered with no row. First and last period of each reporter: [`catalog/un_comtrade_coverage.tsv`](catalog/un_comtrade_coverage.tsv) (218 reporters).

Reference lists (`ListofReferences.json`, 28 lists): Reporters 256, partner areas 310, HS combined 8,262 codes, HS 2022 `H6` 6,940, SITC combined 7,863, BEC 5 178, EBOPS 423, flows 10 (`M X DX FM MIP MOP RM RX XIP XOP`), frequencies 3 (`A Q M`), modes of transport 18, customs codes 17, modes of supply 5, quantity units 41, data items 50. Lists not fetched: `B4 EB02 EB10 EB10S H0 to H5 S1 to S4`.

Fields of a row (preview): `typeCode, freqCode, refPeriodId, refYear, refMonth, period, reporterCode, reporterISO, reporterDesc, flowCode, flowDesc, partnerCode, partnerISO, partnerDesc, partner2Code, partner2ISO, partner2Desc, classificationCode, classificationSearchCode, isOriginalClassification, cmdCode, cmdDesc, aggrLevel, isLeaf, customsCode, customsDesc, mosCode, motCode, motDesc, qtyUnitCode, qtyUnitAbbr, qty, isQtyEstimated, altQtyUnitCode, altQtyUnitAbbr, altQty, isAltQtyEstimated, netWgt, isNetWgtEstimated, grossWgt, isGrossWgtEstimated, cifvalue, fobvalue, primaryValue, legacyEstimationFlag, isReported, isAggregate`.

Fields of an availability row: `datasetCode, typeCode, freqCode, period, reporterCode, reporterISO, reporterDesc, classificationCode, classificationSearchCode, isOriginalClassification, isExtendedFlowCode, isExtendedPartnerCode, isExtendedPartner2Code, isExtendedCmdCode, isExtendedCustomsCode, isExtendedMotCode, totalRecords, datasetChecksum, firstReleased, lastReleased`.

`getComtradeReleases` lists the latest releases (114 rows): reporter, data set, new or revised, release date.

### UNdata
15 SDMX dataflows; `sdmx_metrics` is the service's own observation count. Explorer page: "22 databases and 10 table presentations". Full structures: [`catalog/un_undata_structures.jsonl`](catalog/un_undata_structures.jsonl).

| Flow | Agency | Name | First | Last | Observations | Dimensions (codes available) |
|---|---|---|---|---|---|---|
| `NA_MAIN` | ESTAT | NA Main Aggregates | 1947 | 2025-09 | 1,223,766 | FREQ(2) ADJUSTMENT(2) REF_AREA(218) COUNTERPART_AREA(3) REF_SECTOR(7) COUNTERPART_SECTOR(1) ACCOUNTING_ENTRY(4) STO(39) INSTR_ASSET(3) ACTIVITY(30) EXPENDITURE(2) UNIT_MEASURE(4) PRICES(6) TRANSFORMATION(2) |
| `NASEC_IDCFINA_A` | ESTAT | Annual financial accounts | 1950 | 2024 | 11,122,161 | FREQ ADJUSTMENT REF_AREA(115) COUNTERPART_AREA(6) REF_SECTOR(37) … STO(55) INSTR_ASSET(41) … |
| `NASEC_IDCFINA_Q` | ESTAT | Quarterly financial accounts | 1952 | 2025-06 | 7,994,128 | REF_AREA(40) REF_SECTOR(27) INSTR_ASSET(42) … |
| `NASEC_IDCNFSA_A` | ESTAT | Annual non-financial accounts | 1949 | 2017 | 2,950,303 | REF_AREA(157) REF_SECTOR(39) STO(99) … |
| `NASEC_IDCNFSA_Q` | ESTAT | Quarterly non-financial accounts | 1947 | 2020-06 | 2,544,403 | REF_AREA(39) REF_SECTOR(8) STO(78) … |
| `DF_SEEA_AEA` | ESTAT | Air emission accounts | 1990 | 2022 | 134,248 | REF_AREA(7) INTERACTORS(107) AIRPOL(16) … |
| `DF_SEEA_ENERGY` | ESTAT | Physical energy flow accounts | 2000 | 2022 | 1,134,569 | REF_AREA(38) INTERACTORS(84) ENERGY_FLOWS(37) … |
| `DF_SDG_GLH` | IAEG-SDGs | SDG Harmonized Global Dataflow | 1963 | 2026 | 2,917,712 | SERIES(713) REF_AREA(406) SEX(6) AGE(107) … |
| `DF_UNDATA_COUNTRYDATA` | UNSD | SDMX-CountryData | 1964 | 2015 | 15,375 | FREQ(2) SERIES(100) UNIT(18) REF_AREA(98) … |
| `DF_UNDATA_ENERGY` | UNSD | UNSD Energy Statistics | 1990 | 2024 | 1,919,234 | FREQ(1) REF_AREA(245) COMMODITY(75) TRANSACTION(216) |
| `DF_UNData_EnergyBalance` | UNSD | Energy Balance DataFlow | 1990 | 2023 | 1,045,891 | REF_AREA(218) COMMODITY(11) TRANSACTION(53) UNIT(1) |
| `DF_UNDATA_MDG` | IAEG | SDMX-MDGs | availability call failed | | | code lists: FREQ(6) SERIES(126) UNIT(23) REF_AREA(269) … |
| `DF_UNData_UIS` | UIS | SDMX_UIS_UNData | availability call failed | | | code lists: FREQ(6) SERIES(351) UNIT(14) REF_AREA(256) … |
| `DF_UNData_UNFCC` | UNSD | SDMX_GHG_UNDATA | availability call failed | | | code lists: FREQ(8) INDICATOR(7) REF_AREA(43) UNIT(1) |
| `DF_UNDATA_WDI` | WB | WB World Development Indicators | availability call failed | | | code lists: FREQ(6) SERIES(1300) REF_AREA(252) |

### SDG database
713 series under the 17 goals, release `2026.Q3.G.01`, each with its goal, target, indicator and description: [`catalog/un_sdg.tsv`](catalog/un_sdg.tsv).

## How to call it
**Comtrade**, `https://comtradeapi.un.org`:
- References: `/files/v1/app/reference/ListofReferences.json`, then `Reporters.json`, `partnerAreas.json`, `HS.json`, ….
- Availability, no key: `/public/v1/getDA/C/A/HS?reporterCode=251&period=2023`; whole lists are large (`C/M/HS` 14.8 MB).
- Preview, no key: `/public/v1/preview/C/A/HS?reporterCode=251&period=2023&partnerCode=0&flowCode=X&cmdCode=AG2&motCode=0&customsCode=C00&partner2Code=0&includeDesc=true`.
- Keyed, by the app's code: `/data/v1/get/…` with header `Ocp-Apim-Subscription-Key`. Not called.
- Traps: `/public/v1/preview/S/A/EB?…cmdCode=200`, `/public/v1/getMBS?…` and `/public/v1/previewTariffline/…` answered 200 with no row for the filters tried. The app's source row calls the preview "a few requests a day" (`src/SourcesPage.cpp:760`) and its reply handler treats a 429 as the quota used up; no limit was hit here.

**UNdata**, `https://data.un.org/ws/rest` (redirects to `/legacy/ws/rest`, SDMX-RI, REST and SOAP):
- `/dataflow/`, `/dataflow/UNSD/DF_UNDATA_COUNTRYDATA/1.4?references=all`, `/availableconstraint/UNSD,DF_UNDATA_COUNTRYDATA,1.4/all/all/all`, `/data/UNSD,DF_UNDATA_COUNTRYDATA,1.4/all?detail=serieskeysonly`.
- Trap: `availableconstraint` answers 500 on the four flows above.

**SDG**: `https://unstats.un.org/sdgapi/v1/sdg/Series/List?allreleases=false`.

## What the app reads
**`comtrade`** (`src/SourcesPage.cpp:761`), Map, a country's economy sheet: annual goods at HS 2-digit (`cmdCode=AG2`), partner World, flows `X,M`, two years back, preview without a key and `data/v1/get` with one (`src/KeyedSources.cpp:408`). Keeps `cmdCode`, `cmdDesc`, `primaryValue`, `flowCode` (`src/KeyedSources.cpp:459`). The row's test and the reporter codes come from `Reporters.json` (`src/KeyedSources.cpp:320`). Its User-Agent is `Deepwater/0.1`.

UNdata and the SDG database are not read.

## Not read yet
- **Map, country sheet:** bilateral partners (`partnerCode` over 310 areas), monthly goods (`C/M/HS`, 170 reporters to 2026-09), services (`S/A/EB`, EBOPS), weight and modes of transport fields.
- **Sources:** `getDA` and `getComtradeReleases` tell, without a key, when a country's data arrives.
- **Economy › Countries:** UNdata `NA_MAIN` (218 areas, two frequencies) and `DF_UNData_EnergyBalance`.
- **Economy:** SDG series, 713, from `catalog/un_sdg.tsv`.

## Calls made
All on 2026-11-05 with `User-Agent: Deepwater/1.0`. Comtrade reference base `https://comtradeapi.un.org/files/v1/app/reference`.
- `GET …/ListofReferences.json` → 200, 6,605 bytes.
- `GET …/TradeDataItems.json` → 200, 13,319. `…/Frequency.json` → 200, 199. `…/tradeRegimes.json` → 200, 779. `…/ModeOfTransportCodes.json` → 200, 1,205. `…/partnerAreas.json` → 200, 96,277. `…/Reporters.json` → 200, 81,177. `…/QuantityUnits.json` → 200, 4,579. `…/ModeOfSupply.json` → 200, 391. `…/CustomsCodes.json` → 200, 1,060. `…/HS.json` → 200, 1,952,940. `…/H6.json` → 200, 1,750,838. `…/SS.json` → 200, 2,120,785. `…/B5.json` → 200, 35,396. `…/EB.json` → 200, 44,762.
- `GET https://comtradeapi.un.org/public/v1/preview/C/A/HS?reporterCode=251&period=2023&partnerCode=0&flowCode=X&cmdCode=AG2&motCode=0&customsCode=C00&partner2Code=0&includeDesc=true` → 200, 96,056 bytes, 97 rows; field names kept.
- `GET …/public/v1/preview/S/A/EB?reporterCode=251&period=2022&partnerCode=0&flowCode=X&cmdCode=200` → 200, 58 bytes, no row.
- `GET …/public/v1/getDA/C/A/HS?reporterCode=251&period=2023` → 200, 593 bytes.
- `GET …/public/v1/getDA/C/A/HS` → 200, 2,945,913. `…/getDA/C/M/HS` → 200, 14,815,053. `…/getDA/C/A/SS` → 200, 58, no row. `…/getDA/S/A/EB` → 200, 2,324,174. `…/getDA/C/A/H6` → 200, 276,570.
- `GET …/public/v1/getComtradeReleases` → 200, 13,814.
- `GET …/public/v1/getMBS?series_type=T35&year=2023&country_code=251` → 200, 58, no row.
- `GET …/public/v1/previewTariffline/C/A/HS?reporterCode=251&period=2023&partnerCode=0&flowCode=X&cmdCode=0101` → 200, 58, no row.
- `GET https://data.un.org/ws/rest/dataflow/` → 200, 6,403 bytes, via `/legacy/`. `GET https://data.un.org/WS/rest/dataflow/all/all/latest` → 200, 6,403.
- `GET https://data.un.org/Host.aspx?Content=API` → 200, 17,418. `GET https://data.un.org/Explorer.aspx` → 200, 7,243. `GET https://data.un.org/Datamart.aspx` → 404, 3,769. `GET https://data.un.org/robots.txt` → 404, 3,769.
- `GET https://data.un.org/ws/rest/datastructure/UNSD/CountryData/latest/?references=children` → 200, 107,667. `…/contentconstraint/all/all/latest` → 200, 1,414. `…/data/UNSD,DF_UNDATA_COUNTRYDATA,1.4/all?detail=serieskeysonly` → 200, 967,242.
- `tools/source-inventory/un_undata.py`: `/dataflow/{agency}/{id}/{version}?references=all` and `/availableconstraint/{agency},{id},{version}/all/all/all` for the 15 flows → structure 15 of 15; availability 11 of 15, 500 on `DF_UNDATA_MDG`, `DF_UNData_UIS`, `DF_UNData_UNFCC`, `DF_UNDATA_WDI`.
- `GET https://unstats.un.org/sdgapi/v1/sdg/Series/List?allreleases=false` → 200, 181,379 bytes.
