---
id: oecd
name: Organisation for Economic Co-operation and Development
rows: [itf_road, research_oecd, oecd_housing, oecd_kei]
catalogue: ["OECD", "Organisation for Economic Co-operation and Development"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# OECD

One SDMX service holding **1,552 dataflows** from every directorate of the OECD, from monthly macro indicators to
municipal finance, tax wedges and health. The app reads a few dozen.

## Contents

The complete list is `catalog/oecd.tsv` (agency, id, version, name); `catalog/oecd_structures.jsonl.gz` gives, for each
flow, its dimensions with their code lists (up to 60 codes per dimension, with labels), observation count, first and
last period. 1,524 flows answered with a structure; 1,484 state an observation count, 1,419 a period.

Flows by directorate (the second part of the agency id; the descriptions are ours, from the flows' names):

| Agency | Flows | Holds |
|---|---|---|
| `SDD` | 326 | statistics: national accounts, prices, labour, trade, BoP, short-term indicators |
| `CTP` | 213 | tax |
| `CFE` | 162 | regions, cities, SMEs, tourism |
| `ELS` | 159 | employment, health, social, migration |
| `EDU` | 141 | education |
| `STI` | 110 | science, technology, digital, value added in trade |
| `TAD` | 103 | trade and agriculture |
| `DEV` | 78 | development (Africa, Latin America, Asia) |
| `ENV` | 67 | environment |
| `GOV` | 58 | public governance |
| `DAF` | 34 | investment, capital markets, competition |
| `WISE` | 28 | well-being, inequality |
| `DCD` | 25 | development aid |
| `ITF` | 22 | transport |
| `ECO` | 12 | economic outlook, house prices, GDP long-term |
| `FATF` | 8 | money laundering |
| `SWAC` | 3 | Sahel and West Africa |
| `IAEG-SDGs` | 2 | SDG indicators |
| `ESTAT` | 1 | Eurostat environmental accounts |

The flows that matter most to a macro and markets terminal:

| Flow | Name | Observations | Period |
|---|---|---|---|
| `OECD.SDD.STES,DSD_KEI@DF_KEI` | Key short-term economic indicators | 1,598,501 | 1914-2026 |
| `OECD.ECO.MAD,DSD_EO@DF_EO` | Economic Outlook 119 | 1,303,640 | 1960-2027 |
| `OECD.SDD.STES,DSD_STES@DF_CLI` | Composite leading indicators | 240,605 | 1947-2026 |
| `OECD.SDD.STES,DSD_STES@DF_BTS` | Business tendency surveys | 367,734 | 1950-2026 |
| `OECD.SDD.STES,DSD_STES@DF_FINMARK` | Financial market | 213,234 | 1950-2026 |
| `OECD.SDD.NAD,DSD_NAMAIN1@DF_QNA` | Quarterly National Accounts (for 'Developer API') | 3,806,639 | 1947-2026 |
| `OECD.SDD.TPS,DSD_PRICES@DF_PRICES_ALL` | Consumer price indices (CPIs, HICPs), COICOP 1999 | 2,774,659 | 1914-2026 |
| `OECD.SDD.TPS,DSD_G20_PRICES@DF_G20_PRICES` | G20 - Consumer price indices, all items | 47,551 | 1914-2026 |
| `OECD.SDD.TPS,DSD_LFS@DF_IALFS_UNE_M` | Monthly unemployment rates | 464,651 | 1953-2026 |
| `OECD.SDD.TPS,DSD_BOP@DF_BOP` | Balance of payments | 830,833 | 1955-2026 |
| `OECD.SDD.STES,DSD_STES_REVISIONS@DF_STES_REVISIONS` | Short-term economic statistics revisions | 78,658,803 | 1914-2026 |
| `OECD.SDD.TPS,DSD_BATIS@DF_BATIS` | Balanced trade in services (BaTIS) | 100,951,234 | 2005-2024 |
| `OECD.DAF.INV,DSD_FDI@DF_FDI_INC_AGGR` | FDI income main aggregates, BMD4 | 68,332,824 | 2005-2026 |
| `OECD.ECO.MPD,DSD_AN_HOUSE_PRICES@DF_HOUSE_PRICES` | Analytical house prices indicators | 59,215 | 1956-2026 |
| `OECD.ITF,DSD_TRENDS@DF_TRENDSFREIGHT` | Annual freight transport | 16,558 | 1970-2025 |

**Key Economic Indicators** (`DSD_KEI@DF_KEI`), key `REF_AREA.FREQ.MEASURE.UNIT_MEASURE.ACTIVITY.ADJUSTMENT.TRANSFORMATION`:
monthly, quarterly and annual, 54 areas: ARG, AUS, AUT, BEL, BRA, CAN, CHE, CHL, CHN, COL, CRI, CZE, DEU, DNK, EA, EA20, ESP, EST, EU, FIN, FRA, G20, G7, GBR, GRC, HRV, HUN, IDN, IND, IRL, ISL, ISR, ITA, JPN, KOR, LTU, LUX, LVA, MEX, NLD, NOR, NZL, OECD, OECDE, POL, PRT, RUS, SAU, SVK, SVN, SWE, TUR, USA, ZAF.
30 measures: `B1GQ_Q` Gross domestic product, volume; `BCICP` Composite business confidence; `CA_GDP` Current account balance as percentage of GDP; `CC` Nominal exchange rates; `CCICP` Composite consumer confidence; `CP` Consumer prices; `EMP` Employment; `EX` Merchandise exports; `H_EARN` Hourly earnings; `IM` Merchandise imports; `IR3TIB` Short-term interest rates; `IRLT` Long-term interest rates; `IRSTCI` Immediate interest rates, call money, interbank rate; `LI` Composite leading indicator (CLI); `MABM` M3; `MANM` M1; `NODW` Permits issued for dwellings; `P3_S13_Q` Final consumption expenditure of general government, volume; `P3_S1M_Q` Final consumption expenditure of households and non profit institutions serving households, volume; `P51G_Q` Gross fixed capital formation, volume; `P6_Q` Exports of goods and services, volume; `P7_Q` Imports of goods and services, volume; `PP` Producer prices; `PRVM` Production volume; `RS` Reference series (GDP); `SHARE` Share prices; `TOCAPA` Passenger car registrations; `TOVM` Retail trade volume; `ULC` Unit labour cost; `UNEMP` Unemployment.

**Economic Outlook** (`DSD_EO@DF_EO`): 65 areas, 302 measures, annual and quarterly, with projections to 2027.

**House prices** (`DSD_AN_HOUSE_PRICES@DF_HOUSE_PRICES`): 51 areas, quarterly and annual, measures `HPI`, `RHP`, `RPI`,
`HPI_RPI`, `HPI_YDH`, `HPI_RPI_AVG`, `HPI_YDH_AVG`.

**Freight** (`DSD_TRENDS@DF_TRENDSFREIGHT`): 56 areas, annual, tonne-kilometres by `TRANSPORT_MODE`: `COASTAL`, `RAIL`,
`ROAD`, `OWN`, `PIPE`, `TOT_INL`, `IWW`, `HIRING`.

## How to call it

- Base: `https://sdmx.oecd.org/public/rest`. No key.
- List of flows: `GET /dataflow/all/all/latest?detail=allstubs`, header
  `Accept: application/vnd.sdmx.structure+json;version=1.0`.
- One flow's structure and availability: `GET /dataflow/<agency>/<id>/latest?references=all&detail=referencepartial`.
- Data: `GET /data/OECD.SDD.STES,DSD_KEI@DF_KEI,4.0/USA.M.CCICP.....?startPeriod=2026-01&format=csv`.
- Limit: about 20 requests a minute (a project trap, see the Economy work); the inventory ran at 13 without a block,
  apart from the one 403 below.
- Traps: the `to` period of two flows is the placeholder 2999. Dimensions differ from flow to flow, so read the
  structure before writing a key; the position of each part in the key is the order in `dimensions`.

## What the app reads

- `oecd_kei`: `DSD_KEI@DF_KEI`, monthly `CP UNEMP PRVM TOVM BCICP CCICP LI IR3TIB IRLT H_EARN`, quarterly `B1GQ_Q CA_GDP`,
  `src/KeyIndicators.cpp:135`.
- `research_oecd`: `CCICP` for the United States and other KEI keys, `src/ResearchData.cpp:1194`.
- `oecd_housing`: `HPI_YDH_AVG`, `HPI_RPI_AVG`, `src/DomesticFinance.cpp:232`.
- `itf_road`: `DSD_TRENDS@DF_TRENDSFREIGHT`, all keys, `src/SupplyChain.cpp:341`.
- Desk: `DSD_KEI@DF_KEI` for Australia and New Zealand rates `src/DeskViewsMacro.cpp:552`; `DSD_EO@DF_EO`
  `GDPV_ANNPCT`, `UNR` `src/DeskViewsMacro.cpp:1091`.
- Economy › Countries, from `resources/country_indicators.json` through `src/CountryData.cpp:1295` and `:2065`:

| Flow | Name | Series |
|---|---|---|
| `OECD.CTP.TPS,DSD_TAX_CIT@DF_CIT,2.0` | Corporate income tax (CIT) - statutory and targeted small business rates | 1 |
| `OECD.CTP.TPS,DSD_TAX_PIT@DF_PIT_AV,1.0` | Personal income tax (PIT) and social security contributions (SSC) - average rates on gross wage earnings | 3 |
| `OECD.CTP.TPS,DSD_TAX_PIT@DF_PIT_TOP_EARN_THRESH,1.0` | Personal income tax (PIT) - top statutory and marginal tax rate | 1 |
| `OECD.CTP.TPS,DSD_WHT@DF_WHT_STANDARD,1.0` | Standard withholding tax rates - Corporate tax statistics | 1 |
| `OECD.ECO.MPD,DSD_AN_HOUSE_PRICES@DF_HOUSE_PRICES,1.0` | Analytical house prices indicators | 1 |
| `OECD.ELS.HD,DSD_HEALTH_REAC_HOSP@DF_BEDS_ICU,1.1` | Intensive care unit (ICU) beds and use | 1 |
| `OECD.ELS.HD,DSD_HEALTH_REAC_HOSP@DF_HOSP,1.1` | Hospitals | 1 |
| `OECD.ELS.SAE,DSD_EARNINGS@AV_AN_WAGE,1.0` | Average annual wages | 1 |
| `OECD.ELS.SPD,DSD_PAG@DF_PAG,1.0` | Pensions at a glance | 2 |
| `OECD.SDD.NAD,DSD_HHDASH@DF_HHDASH_INDIC,1.1` | Household indicators dashboard – indicator view | 2 |
| `OECD.SDD.NAD,DSD_NAMAIN10@DF_TABLE6,2.0` | Annual value added and its components by economic activity | 5 |
| `OECD.SDD.NAD,DSD_NAMAIN1@DF_QNA_BY_ACTIVITY_OUTPUT,1.1` | Quarterly GDP and components - output approach | 2 |
| `OECD.SDD.STES,DSD_KEI@DF_KEI,4.0` | Key short-term economic indicators | 26 |
| `OECD.SDD.TPS,DSD_PRICES@DF_PRICES_ALL,1.0` | Consumer price indices (CPIs, HICPs), COICOP 1999 | 4 |

## Not read yet

- `DSD_STES@DF_CLI`, `DSD_STES@DF_BTS`, `DSD_STES@DF_FINMARK`: composite leading indicators, business tendency surveys and
  financial markets in full, where KEI carries only headlines: Economy, Desk.
- `DSD_STES_REVISIONS@DF_STES_REVISIONS`: how short-term statistics were revised, for the data fusion's checks.
- `DSD_EO@DF_EO`: the other 300 measures, projections for Desk and Economy.
- `DSD_BOP@DF_BOP`, `DSD_BATIS@DF_BATIS`, BIMTS: balance of payments and balanced trade for the map's trade flows.
- `DSD_AN_HOUSE_PRICES@DF_HOUSE_PRICES`: `RHP`, `RPI`.
- `DSD_TRENDS@DF_TRENDSFREIGHT`: rail, pipeline, waterways and coastal shipping, beside road, for Supply chains.

## Calls made

- `GET /dataflow/all/all/latest?detail=allstubs` → 200, 1.7 MB.
- `GET /dataflow/<agency>/<id>/latest?references=all&detail=referencepartial`, once per flow, 1,552 calls by
  `tools/source-inventory/oecd_structures.py`, 5 November 2026:
  - 1,524 gave a structure.
  - 27 answered without a data structure under this detail level (so this inventory has no dimensions for them): the
    development aid `CRS` family, BIMTS at HS 4 and 6 digits, productivity `PDB`, `TIVA`, `ICIO_GHG`, `BTIGE`, `ITIC`.
    Their entries carry `error` in `oecd_structures.jsonl.gz`; the names are in `oecd.tsv`. They were not retried.
  - 1 refused with 403: `DSD_REG_ENV@DF_ENV`.
