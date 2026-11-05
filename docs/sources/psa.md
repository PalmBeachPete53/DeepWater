---
id: psa
name: Philippine Statistics Authority (OpenSTAT)
rows: []
catalogue: ["Philippine Statistics Authority"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# Philippine Statistics Authority (OpenSTAT)

The Philippines' national statistics in a PxWeb database: national accounts, prices, labour, trade, agriculture, poverty, regional and provincial cuts.

## Contents
**3,560 tables in 26 subjects**, complete in [`catalog/psa.tsv`](catalog/psa.tsv): path, subject, table file, title, updated, and for **2,258 of them** (macro subjects) variables, values per variable,
first and last period. Last update seen 2026-11-05. Philippines, with regions, provinces and highly urbanized cities (`Geolocation`).

Tables by subject: Economic Accounts 1,170, Philippine Statistical Yearbook 517, Trade 326, Labor and Employment 228, Population and Vital Statistics 196, Prices 194, Agriculture, Forestry, Fisheries 179,
Sustainable Development Goals 174, Gender 156, Poverty 94, Decent Work 83, Services 72, Tourism 49, Mining, Manufacturing, Construction 25, Living Conditions 24, Environment 15, Income and Consumption 14,
Macroeconomic Statistics 9, Energy 8, Food Security 8, Digital Connectivity 5, Education 4, Health 4, Information Society 4, Housing 1, Labor Cost 1. (Metadata read for all tables of Economic Accounts, Trade, Labor,
Prices, Agriculture, Decent Work, Tourism, Income, Macroeconomic, Digital, Labor Cost.)

Families by path:

| Path | Holds | Example | Variables (values) | Span |
|---|---|---|---|---|
| `2M/PI/CPI/2018NEW` | CPI, all income households, 2018 base | `0012M4ACP22.px`, `0012M4ACP23.px` (% over a year) | Geolocation 120, Commodity 354, Year 9, Period 13 | 2018 to 2026 |
| `2M/PI/CPI/2012`, `2M/PI/WPI/2012`, `2M/PI/BIH/2012`, `2M/PI/PPI/PPIA` | Older CPI, general wholesale, CPI for the bottom 30% of households, agricultural producer prices | `0062M4APPI1.px` | Region 17, Commodity 18, Year 9, Period 5 | 2018 to 2026 |
| `2M/2018NEW`, `2M/NFG`, `2M/FG`, `2M/RP`, `2M/NRP`, `2M/WS`, `2M/NWS` | Farmgate, retail and wholesale prices of farm commodities, by place | `0042M4ARN01.px` | Geolocation 119, Commodity 14, Year 9, Period 13 | 2018 to 2026 |
| `2B/NA/QT`, `2B/NA/AN` | National accounts, quarterly and annual | `0012B5BEXQ1.px` | Expenditure 20, Valuation 2, Year 27, Period 4 | 2000 to 2026 |
| `2B/CA`, `2B/FI`, `2B/AA` | Consolidated accounts, approved foreign investments, value of production by region | `0012B5BCA01.px` | Item 14, Year 26 | 2000 to 2025 |
| `2B/SA/*` | Satellite accounts: health `HA`, tourism `TA`, `MST`, creative economy `PCE`, digital `DE`, ocean `POE` | | | |
| `2A/PPA` | GDP by province and city | `0012A5FPPA0.px` | Geolocation 135, Valuation 2, Year 8 | 2018 to 2025 |
| `1B/LFS` | Labour Force Survey: levels and rates | `0011B3FKEI1.px`, `0021B3FKEI2.px` | Year 22, Month 13, Levels 5 or Rates 4, Sex 3 | 2005-04 to 2026-08 |
| `1B/ISLE/*` | Integrated Survey on Labor and Employment | | | one survey round |
| `2L/IMT/FOB`, `CIF`, `GKE`, `GKI`, `QPE`, `QPI`, `AVE`, `AVI` | Merchandise trade: value, group, quantity, unit value, by commodity and country (one table per year, 36) | `0012L4DXVD5.px` | Commodity, Country 219, Year 1, Period 9 | to 2026 |
| `2L/IMT/UVI` | Trade unit value indices, 2023 | | | |
| `2L/DTR` | Domestic trade: commodity flow by station, air, coastwise | `0012L4DDAA0.px` | Commodity 258, Origin 39, Destination 49, Year 31 | 1995 to 2025 |
| `2I`, `2I/MST` | Tourism satellite accounts | `0012I5DINB1.px` | Products 10, Year 25 | 2000 to 2024 |
| `3K/E*` | Decent Work indicators, older | `0633K3E7050.px` | Indicator, Item, Year 24 | 1995 to 2018 |

## How to call it
- Base: `https://openstat.psa.gov.ph/PXWeb/api/v1/en/DB`. No key.
- Tree: `GET .../DB`, then `.../DB/2M`, `.../DB/2M/PI/CPI/2018NEW`: `[{"id","type":"l"|"t","text","updated"}]`.
- Metadata: `GET .../DB/2M/PI/CPI/2018NEW/0012M4ACP22.px`: `title`, `variables` (`code`, `text`, `values`, `valueTexts`, `time`).
- Data, from PxWeb's standard, not called: `POST` the same URL with `{"query":[...],"response":{"format":"json-stat2"}}`.
- Traps: some variables have no `values` list in the answer (`Commodity Code` shows 0 in the TSV), and `0012L4DDCA0.px` likewise. `www.psa.gov.ph` answers 403 in the body probe, while this host answers.

## What the app reads
Nothing: no row in `src/SourcesPage.cpp`. The Philippines reach the Map through IMF and BIS.

## Not read yet
- Countries › Philippines: `2M/PI/CPI/2018NEW`, `1B/LFS`, `2B/NA/QT`.
- Supply chains: `2L/IMT/*` by commodity and country, monthly; `2L/DTR` domestic flows.
- Commodities: `2M/*` farm prices by province, monthly.

## Calls made
- `GET https://openstat.psa.gov.ph/PXWeb/api/v1/en` → 200, 27 B
- `GET https://openstat.psa.gov.ph/PXWeb/api/v1/en/DB` → 200, 1.4 KB
- `GET https://openstat.psa.gov.ph/PXWeb/api/v1/en/DB/<path>` for every level, and for each of the 2,258 tables with metadata → 2,874 calls in all, all 200; a first run stopped on a parser error of this inventory's script and was restarted
