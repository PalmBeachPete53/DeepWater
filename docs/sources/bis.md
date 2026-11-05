---
id: bis
name: Bank for International Settlements
rows: [bis_daily, bis_monthly, bis_credit, research_bis]
catalogue: ["Bank for International Settlements"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# Bank for International Settlements

Central banks' statistics gathered by the BIS: policy rates and balance sheets, credit, property prices, exchange rates,
international banking, debt securities, derivatives, payments. Twenty-nine SDMX dataflows on `stats.bis.org`, of which
28 hold data.

## Contents

29 dataflows, agency `BIS`, all version 1.0 but `WS_TC` (2.0). `catalog/bis.tsv`: one line per flow.
`catalog/bis_structures.jsonl`: description, dimensions, codelists, available codes with labels (up to 60 per dimension),
attributes. `catalog/bis_codes.tsv` (6,459 lines): every available code and label of every dimension but the area ones.

"Series" is the flow's `series_count`; the span comes from a first-observation and a last-observation query by frequency.
For the four flows marked *one area*, the span was measured on `US` only (the whole flow is over 120,000 series).

| Flow | Name | Series | Areas | Span by frequency | Other dimensions |
|---|---|---|---|---|---|
| `WS_CBPOL` | Central bank policy rates | 98 | 49 `REF_AREA` | D 1945-01-01..2026-11-03, M 1945-01..2026-10 | |
| `WS_CBTA` | Central bank total assets | 580 | 53 | A 1668..2025, Q 1840-Q2..2026-Q2, M 1840-06..2026-07 | `COMP_METHOD`, `UNIT_MEASURE` (USD, XDC, % GDP), `CURRENCY` 37, `TRANSFORMATION` |
| `WS_TC` | Total credit | 1,133 | 48 `BORROWERS_CTY` | Q 1940-Q2..2026-Q1 | `TC_BORROWERS` C non-financial, G government, H households, N companies, P private non-financial; `TC_LENDERS` A all, B banks; `VALUATION` M, N; `UNIT_TYPE` 770 % GDP, 799 % GDP PPP, USD, XDC; `TC_ADJUST` |
| `WS_CREDIT_GAP` | Credit-to-GDP gaps | 132 | 44 | Q 1947-Q4..2026-Q1 | `CG_DTYPE` A ratio, B trend, C gap |
| `WS_DSR` | Debt service ratios | 66 | 32 | Q 1999-Q1..2026-Q1 | `DSR_BORROWERS` H, N, P |
| `WS_GLI` | Global liquidity indicators | 225 | 25 | Q 1977-Q4..2026-Q2 | `CURR_DENOM` USD, EUR, JPY, all; `BORROWERS_SECTOR` 6; `LENDERS_SECTOR`; `L_POS_TYPE`; `L_INSTR`; `UNIT_MEASURE` 5 |
| `WS_SPP` | Selected residential property prices | 244 | 61 | Q 1927-Q1..2026-Q2 | `VALUE` N, R; `UNIT_MEASURE` 628 index 2010=100, 771 y/y % |
| `WS_DPP` | Detailed residential property prices | 349 | 61 | A 1819..2025, Q 1955-Q1..2026-Q3, M 1976-01..2026-09, H 2000-S1..2024-S1 | `COVERED_AREA` 13, `RE_TYPE` 15, `RE_VINTAGE`, `COMPILING_ORG`, `PRICED_UNIT`, `ADJUST_CODED` |
| `WS_CPP` | Commercial property prices | 68 | 26 | A 1984..2025, Q 1945-Q4..2026-Q3, M 1993-01..2026-08, H 2006-S1..2025-S2 | `COVERED_AREA` 6, `RE_TYPE` 8 (office, retail, industrial, agricultural…), others as `WS_DPP` |
| `WS_LONG_CPI` | Consumer prices | 252 | 63 | A 1661..2025, M 1913-01..2026-09 | `UNIT_MEASURE` 628, 771 |
| `WS_EER` | Effective exchange rates | 271 | 64 | D 1983-10-03..2026-11-03, M 1964-01..2026-09 | `EER_TYPE` N, R; `EER_BASKET` B 64 economies, N 27 |
| `WS_XRU` | US dollar exchange rates | 1,234 | 192 | D 1945-01-01..2026-11-03; M, Q, A from 1791 to 2026-10, 2026-Q3, 2025 | `CURRENCY` 147, `COLLECTION` A average, E end of period |
| `WS_LBS_D_PUB` | Locational banking | 610,300 | 50 `L_REP_CTY` × 251 `L_CP_COUNTRY` | Q 1977-Q4..2026-Q2 (one area) | `L_MEASURE` 4, `L_POSITION` claims, liabilities; `L_INSTR` 7; `L_DENOM` 8; `L_CURR_TYPE` 4; `L_PARENT_CTY` 52; `L_REP_BANK_TYPE` 4; `L_CP_SECTOR` 13; `L_POS_TYPE` 4 |
| `WS_CBS_PUB` | Consolidated banking | 229,388 | 33 `L_REP_CTY` × 252 `L_CP_COUNTRY` | Q 1983-Q4..2026-Q2 (one area) | `L_MEASURE`, `CBS_BANK_TYPE` 6, `CBS_BASIS` 4, `L_POSITION` 10, `L_INSTR` 4, `REM_MATURITY` 4, `CURR_TYPE_BOOK`, `L_CP_SECTOR` 8 |
| `WS_DEBT_SEC2_PUB` | International debt securities | 224,680 | 171 `ISSUER_RES`, 168 `ISSUER_NAT` | Q 1966-Q1..2026-Q2 (one area) | `ISSUER_BUS_IMM`, `_ULT` 10; `ISSUE_CUR_GROUP`; `ISSUE_CUR` 65; `ISSUE_OR_MAT`; `ISSUE_RE_MAT`; `ISSUE_RATE`; `MEASURE` gross issues, net issues, outstanding |
| `WS_NA_SEC_DSS` | Debt securities statistics | 146,117 | 60 | Q 1951-Q4..2026-Q1, A 1952..2025 (one area) | National-accounts shape, 18 dimensions: `REF_SECTOR` 14, `COUNTERPART_AREA` 3, `INSTR_ASSET` 7, `MATURITY` 9, `UNIT_MEASURE` 40, `VALUATION`, `CUST_BREAKDOWN` 21 |
| `WS_NA_SEC_C3` | BIS debt securities statistics | none | | | Same 18 dimensions. **No data**: availability and data both answer 404 |
| `WS_OTC_DERIV2` | OTC derivatives outstanding | 4,960 | 1 (`5J` all) | H 1998-S1..2025-S2 | `DER_TYPE` 10, `DER_INSTR` 15, `DER_RISK` 11, `DER_SECTOR_CPY` 11, `DER_CPC` 9, `DER_CURR_LEG1`, `2` 8, `DER_ISSUE_MAT`, `DER_RATING`, `DER_BASIS` |
| `WS_DER_OTC_TOV` | OTC derivatives and FX spot turnover | 79,786 | 57 `DER_REP_CTY` | A 1986..2025 | `DER_INSTR` 14, `DER_RISK` FX, rates; `DER_CURR_LEG1`, `2` 59, `DER_ISSUE_MAT` 10, `DER_EX_METHOD` 18, others |
| `WS_XTD_DERIV` | Exchange-traded derivatives | 1,296 | by exchange region: `XD_EXCHANGE` 7 | M 1993-01..2026-06, Q 1993-Q1..2026-Q2, A 1975..2025 | `OD_TYPE`, `OD_RISK_CAT` 4, `OD_INSTR` 3, `ISSUE_CUR` 25 |
| `WS_CPMI_CASHLESS` | Cashless payments | 1,528 | 26 `REP_CTY` | A 2012..2024 | 12 dimensions: instrument, direction, terminal, card function… |
| `WS_CPMI_CT1`, `_CT2` | Comparative tables | 5,303, 1,847 | 27, 28 | A 2012..2024 | `INDICATOR_CT`, `SYSTEM` 237… |
| `WS_CPMI_DEVICES` | Payment devices | 276 | 26 | A 2012..2024 | `DEVICE_TYPE` card, POS, EFTPOS, ATM… |
| `WS_CPMI_INSTITUT` | Institutions | 614 | 26 | A 2012..2024 | `INSTITUTION_TYPE` 17, `INDICATOR` |
| `WS_CPMI_MACRO` | Macro | 675 | 27 | A 2012..2024 | `BIS_TOPIC` 15, `BIS_SUFFIX` 32 |
| `WS_CPMI_PARTICIP` | Participants | 2,045 | 27 | A 2012..2024 | `SYSTEM` 220, `PART_TYPE` 25 |
| `WS_CPMI_SYSTEMS` | Systems | 3,138 | 28 | A 2012..2024 | `SYSTEM` 231, `INSTRUMENT_TYPE` 46 |
| `BIS_REL_CAL` | Release calendar | 38 | | D 2023-06-05..2027-05-24, plus M, Q, H, A | `CATEGORY` 20, `RELEASE_TYPE` 14: dates of the BIS's own releases, into the future |

The areas of each flow are in `catalog/bis_structures.jsonl`, under the dimension named in the table. Area codes are BIS
ISO alpha-2 plus aggregates (`XM` euro area, `5J`, `1E`…); three different area codelists are in use (`CL_BIS_GL_REF_AREA`
239 codes, `CL_BIS_IF_REF_AREA` 432, `CL_AREA` 101).

Each flow also comes as a static bulk file, at `https://data.bis.org/static/bulk/<flow>_csv_flat.zip`, with `_csv_col`,
`_csv_row`, `_sdmx-compact-2.1`, `_sdmx-generic-2.1`, `_sdmx_edi` variants depending on the flow (137 links on the bulk
page).

## How to call it

- Base: `https://stats.bis.org/api/v2` (SDMX 3.0 style) and `/api/v1` (SDMX 2.1 style). Swagger page: `/api-doc/v2/`.
- Catalogue: `GET /api/v2/structure/dataflow/BIS/*/+?detail=allstubs&format=sdmx-json`.
- Structure: `GET /api/v2/structure/dataflow/BIS/WS_CBPOL/+?references=descendants&detail=referencepartial&format=sdmx-json`.
- Availability: `GET /api/v2/availability/dataflow/BIS/WS_CBPOL/+/*?mode=exact&format=sdmx-json`: `series_count` and the
  codes in use; no time range.
- Data: `GET /api/v2/data/dataflow/BIS/WS_CBPOL/1.0/M.XM?format=csv&detail=dataonly&lastNObservations=1`;
  v1: `GET /api/v1/data/WS_DSR/Q.FR.H/all?startPeriod=2025&format=csv`.
- CSV columns with `detail=dataonly`: the dimensions, `TIME_PERIOD`, `OBS_VALUE`. Without: also `UNIT_MEASURE`, `UNIT_MULT`,
  `DECIMALS`, `TITLE_TS`, `OBS_CONF`, `OBS_STATUS`…

Traps: a key of wildcards only (`*.*`, `*.*.*.*`) answers **404**: use `*` or leave the key out, or fix one dimension
(`M.*`). `stats.bis.org/robots.txt` redirects to the portal `data.bis.org`, no robots file met. No key, no rate limit met at 1 call per 1.5 s over about 150 calls.

## What the app reads

- `bis_daily`: `WS_CBPOL` `D.<areas>`, the banks of `PolicyRates`, last 3 years, CSV (`src/MacroData.cpp:310`): area, day,
  rate. Forex tab. `src/SourcesPage.cpp:394`.
- `bis_monthly`: `WS_CBPOL` `M.`, 27 months, every area (`src/MacroData.cpp:1112`). Map heatmap. `src/SourcesPage.cpp:669`.
- `bis_credit`: `src/DomesticFinance.cpp:188`-`193`. `WS_SPP Q..R.628` real price index from 1970; `WS_TC
  Q..H+N.A.M.770.A` credit to households and companies, % GDP, market value, adjusted, from 1980; `WS_DSR Q..H+P`.
  `src/SourcesPage.cpp:676`.
- `research_bis`: Economy › Countries and Indicators, keys in `resources/country_indicators.json` (5 sources):
  `WS_CBPOL/1.0 M.{BIS}`, `WS_TC/2.0 Q.{ISO2}.P.A.M.770.A` and `Q.{ISO2}.H.A.M.770.A`, `WS_SPP/1.0 Q.{ISO2}.N.628` and
  `.771`; country call `src/CountryData.cpp:1316`, cross-country call `:2068`. `src/SourcesPage.cpp:509`.
- Same row: Desk › Notes reads the speeches feed `https://www.bis.org/doclist/cbspeeches.rss` (`src/DeskViewsTech.cpp:584`,
  not an SDMX flow).

The app reads 4 flows of 28.

## Not read yet

- `WS_EER`: nominal and real effective exchange rates, daily and monthly, 64 economies: Forex, Desk strength.
- `WS_XRU`: dollar rate of 192 areas, daily, long history: fallback for the Forex tab and Countries' currency row.
- `WS_CREDIT_GAP`: credit-to-GDP gap, 44 countries: a financial-stability heatmap on the Map beside `bis_credit`.
- `WS_GLI`: credit by currency of denomination (USD, EUR, JPY), borrower country and sector, quarterly: Economy, liquidity.
- `WS_CBTA`: central bank balance sheets, 53 areas: one call for all, where Countries reads IMF `MFS_CBS` country by country.
- `WS_LBS_D_PUB`, `WS_CBS_PUB`: cross-border bank claims between countries: arcs on the Map, as the trade layer does.
- `WS_DEBT_SEC2_PUB`, `WS_NA_SEC_DSS`: bond issuance by country, sector and currency.
- `WS_DPP`, `WS_CPP`: city and commercial property prices.
- `WS_OTC_DERIV2`, `WS_DER_OTC_TOV`, `WS_XTD_DERIV`: derivatives market size; a Quant or Research reference panel.
- `BIS_REL_CAL`: BIS release dates, for the calendar.

## Calls made

All 2026-11-05, User-Agent `Deepwater/1.0`, about 1.5 s apart.

- `GET https://stats.bis.org/api/v2/structure/dataflow/BIS/*/+?detail=allstubs&format=sdmx-json` → 200, 15,256 B, 29 flows
- `GET /api/v2/structure/dataflow/BIS/WS_CBPOL/+?references=all&format=sdmx-json` → 200, 612,557 B
- For each of the 29 flows, `tools/source-inventory/bis.py`: `GET /api/v2/structure/dataflow/BIS/<flow>/+?references=descendants&detail=referencepartial&format=sdmx-json` → 200 (WS_CBPOL: 499,702 B);
  `GET /api/v2/availability/dataflow/BIS/<flow>/+/*?mode=exact&format=sdmx-json` → 200 for 28, 404 for `WS_NA_SEC_C3`;
  `GET /api/v2/data/dataflow/BIS/<flow>/<version>/<key>?format=csv&detail=dataonly&firstNObservations=1` and `lastNObservations=1`, values discarded → 200 with key `*` or a fixed area, 404 for `WS_NA_SEC_C3`
- `GET /api/v2/availability/dataflow/BIS/WS_CBPOL/+/*` → 200, 1,863 B
- `GET /api/v1/availableconstraint/WS_CBPOL/all/all/all?mode=exact` → 200, 2,698 B (no time range either)
- `GET /api/v2/data/dataflow/BIS/WS_EER/1.0/*.*.*.*?format=csv&detail=dataonly&firstNObservations=1` → 404; `WS_CBPOL/1.0/*.*` → 404
- Same with key `M.*.*.*` → 200, 4,293 B; key `*` → 200, 6,668 B; no key → 200, 6,668 B; `WS_CBPOL/1.0/M.*` → 200, 877 B
- `GET /api/v1/data/WS_EER/all/all?format=csv&detail=dataonly&firstNObservations=1` → 200, 6,668 B
- `GET /api/v1/data/WS_DSR/Q.FR.H/all?startPeriod=2025&format=csv` → 200, 451 B (column names)
- `GET /api/v2/structure/codelist/BIS/CL_BIS_GL_REF_AREA/+?detail=allstubs&format=sdmx-json` → 200, 937 B
- `GET https://stats.bis.org/api-doc/v2/` → 200, 11,145 B
- `GET https://stats.bis.org/robots.txt` → 301 to `https://data.bis.org/`, 200, HTML
- `GET https://data.bis.org/bulkdownload` → 200, 330,101 B (twice)
- `GET https://data.bis.org/static/bulk/WS_CBPOL_csv_flat.zip` → 200, 4,145,567 B
