---
id: worldbank
name: World Bank
rows: [worldbank, worldbank_credit, research_worldbank]
catalogue: ["World Bank"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# World Bank

A single API over 71 thematic databases: development indicators, debt, education, health, gender, financial inclusion,
the monthly Global Economic Monitor. Almost everything is country × series × time; a few databases add a counterpart area,
a classification, a sector or a vintage.

## Contents

- **71 databases** (`/sources`). `catalog/worldbank_sources.tsv`: id, code, name, last update, areas, series, periods,
  first and last period, other dimensions. `catalog/worldbank_sources.jsonl` adds a sample of the members.
- **29,544 indicators** from 62 of the databases (`/indicator`). `catalog/worldbank.tsv.gz`: id, database, topics (5,575 of
  them carry one), unit, name, source organisation.
- **36,139 series by database** (`/sources/<id>/series`), 29,565 distinct ids: `catalog/worldbank_series.tsv.gz`. Grep it to
  know which database holds an indicator: the same code can live in several.
- **296 areas** (`/country`): 217 economies and 79 aggregates. Fields: `id` (ISO3), `iso2Code`, `name`, `region`,
  `adminregion`, `incomeLevel`, `lendingType`, `capitalCity`, `longitude`, `latitude`.
- **Groups.** 7 regions of economies (East Asia & Pacific, Europe & Central Asia, Latin America & Caribbean, Middle East,
  North Africa, Afghanistan & Pakistan, North America, South Asia, Sub-Saharan Africa); `/region` lists 44 region codes
  (with `EMU` euro area, `EUU`, `OED`, `ARB`, `SST`, `WLD`…). `/incomelevel` 7: `HIC`, `UMC`, `LMC`, `LIC`, `LMY`, `MIC`, `INX`.
  `/lendingtype` 4: `IBD`, `IDB`, `IDX`, `LNX`.
- **21 topics**: Agriculture & Rural Development, Aid Effectiveness, Economy & Growth, Education, Energy & Mining,
  Environment, Financial Sector, Health, Infrastructure, Social Protection & Labor, Poverty, Private Sector, Public
  Sector, Science & Technology, Social Development, Urban Development, Gender, Millennium development goals, Climate
  Change, External Debt, Trade.

Databases that matter to a macro and markets terminal (all from `catalog/worldbank_sources.tsv`):

| Id | Code | Name | Updated | Areas | Series | Periods | Other |
|---|---|---|---|---|---|---|---|
| 2 | WDI | World Development Indicators | 2026-10-08 | 266 | 1,498 | YR1960..YR2025 | |
| 15 | GEM | Global Economic Monitor | 2026-09-08 | 220 | 36 | A, Q, M from 1987 to 2026 (675 periods: `1987`, `1987M01`, `1987Q1`) | |
| 27 | GEP | Global Economic Prospects | 2026-06-11 | 163 | 1 (GDP growth) | 2023..2028: forecasts | |
| 6 | IDS | International Debt Statistics | 2025-12-03 | 134 | 578 | YR1970..YR2033 | Counterpart-Area 304 |
| 81 | DSI | International Debt Statistics: DSSI | 2025-12-03 | 68 | 578 | YR1970..YR2028-M12 | Counterpart-Area 304 |
| 20 | PSD | Quarterly Public Sector Debt | 2026-10-29 | 107 | 564 | YR1995Q1..YR2026Q2 | |
| 22 | QDS | Quarterly External Debt Statistics SDDS | 2026-11-03 | 130 | 1,800 | YR1998Q1..YR2026Q2 | |
| 23 | QDG | Quarterly External Debt Statistics GDDS | 2026-11-03 | 62 | 256 | YR2002Q1..YR2026Q2 | |
| 54 | JED | Joint External Debt Hub | 2026-09-09 | 261 | 28 | YR1990Q1..YR2026Q1 | |
| 32 | GFD | Global Financial Development | 2022-09-23 | 225 | 108 | YR1960..YR2021 | |
| 3 | WGI | Worldwide Governance Indicators | 2026-09-25 | 216 | 36 | YR1996..YR2025 | |
| 40 | HPP | Population estimates and projections | 2026-07-01 | 265 | 187 | YR1960..YR2050 | |
| 43, 59 | WAT, WAC | Adjusted Net Savings, Wealth Accounts | 2023, 2024 | 266, 151 | 36, 146 | to YR2021, YR2020 | |
| 66 | LPI | Logistics Performance Index | 2024-03-26 | 170 | 19 | YR2007..YR2023 | |
| 75 | ESG | Environment, Social and Governance | 2026-06-24 | 260 | 80 | YR1960..YR2025 | |
| 86 | JON | Global Jobs Indicators | 2023-05-22 | 168 | 746 | YR1970..YR2021 | |
| 88, 93 | FPN, FPA | Food Prices for Nutrition, archive | 2026-07-21 | 192 | 54, 63 | YR2017..YR2025 | Classification 1, 8 |
| 87 | CCD | Country Climate and Development Report | 2022-10-07 | 220 | 432 | YR1960..YR2100 | |
| 62, 71, 78, 90 | ICP, IC5, IC7, IC2 | International Comparison Program (PPP) 2011, 2005, 2017, 2021 | 2020..2024 | 195..222 | 47, 47, 47, 48 | | Classification 16 or 17 |
| 57 | WDA | WDI Database Archives | 2025-10-29 | 292 | 2,836 | YR1960..YR2026 | Version 142 (from `198904`): past vintages of WDI |

**GEM, all 36 series**: `CORENS`, `CORESA` core CPI; `CPTOTNSXN`, `CPTOTSAXN` CPI; `CPTOTSAXMZGY`, `CPTOTSAXNZGY` CPI y/y;
`DMGSRMRCH…` imports and `DXGSRMRCH…` exports of merchandise (`NSCD`, `NSKD`, `NSXD`, `SACD`, `SAKD`, `SAXD`: current or
constant US$, or price, not or seasonally adjusted); `DPANUSLCU`, `DPANUSSPB`, `DPANUSSPF` exchange rates; `DSTKMKTXD`,
`DSTKMKTXN` stock markets; `IMPCOV` import cover; `IPTOTNSKD`, `IPTOTSAKD` industrial production; `NEER`, `REER`;
`NYGDPMKTPSACD`, `SACN`, `SAKD`, `SAKN` GDP; `RETSALESSA` retail sales; `TOT` terms of trade; `TOTRESV` reserves;
`UNEMPSA_` unemployment.

**WDI families by code prefix** (1,498): `SE` education 156, `SH` health 148, `SL` labour 129, `SP` population 98, `NY`
national accounts 94, `DT` debt and aid 84, `IC` business 76, `NE` expenditure 65, `EN` environment 63, `GC` government 45,
`TM` imports 45, `DC` 38, `NV` value added 35, `TX` exports 30, `AG` agriculture 27, `EG` energy 27, `IQ` 27, `SI` 26, `BX`
21, `ER` 15, `BM` 14, others under 14. Labels of the prefixes are this card's reading; the series names are the source's.

The other databases are thematic or old: Doing Business (1, last updated 2021), Education Statistics (12, 8,450 series,
to YR2100), Gender (14), Findex (28), Atlas of Social Protection (29), Health (16, 39, 65, 58), MDG (19), SDG (46),
Enterprise Surveys (13), Jobs (25), PEFA (67, 68, 79, 85, 91), subnational sets for India (41), Indonesia (45) and others
(5, 38, 50). No database name among the 71 is about commodity prices, and a search of the series for "commodity" finds
export volumes and aid: look elsewhere for prices.

## How to call it

- Base `https://api.worldbank.org/v2`. Always `format=json`. Paged: `per_page`, `page`; the first array or object holds
  `page, pages, per_page, total`.
- Catalogue: `/sources`, `/indicator`, `/topic`, `/country`, `/region`, `/incomelevel`, `/lendingtype`.
- One database: `/sources/15/concepts`, then `/sources/15/{country|series|time}`; `/sources/2/metatypes` and
  `/sources/2/series/SP.POP.TOTL/metadata` for the documentation fields.
- Data: `GET /country/KEN/indicator/CPTOTNSXN?source=15&format=json&mrv=1&frequency=M`. Fields: `indicator {id,value}`,
  `country {id,value}`, `countryiso3code`, `date`, `value`, `unit`, `obs_status`, `decimal`. Several indicators separated
  by `;` go with `source=`, as the app calls them (`src/CountryData.cpp:1239`); `country/all` takes every area (`:2001`).
- Other form: `/sources/15/country/KEN/series/CPTOTNSXN/time/all?format=json`, which walks every period of every
  frequency at once (675 rows here).
- Bulk: `GET /en/indicator/FS.AST.PRVT.GD.ZS?downloadformat=csv`: a zip of three CSV, one with the data wide by year,
  `Metadata_Indicator_…`, `Metadata_Country_…`.

Traps: time codes are not uniform (`YR1960`, `YR1998Q1`, `1987M01`, `2023`, `YR200506`, `TG2009`, `YR2028-M12`). The app asks
GEM with `frequency=M&date=1960M01:<year>M12`, in that format. Database 16 calls its time concept `Year`. `/sources/5/adminregion` and
`/sources/77/receivingcountries` answer 400: their area concepts (`Admin Region`, `Receiving Countries `) could not be
listed as `/sources/<id>/<concept>` written in lower case with spaces removed. The country counts of a database include
aggregates, and of subnational databases their regions (`Subnational Population` 2,033). The series calls take seconds per
page on big databases; no key; no limit met at one call a second over a few hundred calls. `api.worldbank.org/robots.txt`: 404.

## What the app reads

- `worldbank`: Map, sectors. Seven indicators, bulk zips: `NV.AGR.TOTL.ZS`, `NV.IND.TOTL.ZS`, `NV.IND.MANF.ZS`,
  `NV.SRV.TOTL.ZS`, `SL.AGR.EMPL.ZS`, `SL.IND.EMPL.ZS`, `SL.SRV.EMPL.ZS` (`src/MacroData.cpp:955`, URL `:999`).
  `src/SourcesPage.cpp:672`.
- `worldbank_credit`: Map, `FS.AST.PRVT.GD.ZS` and `FB.AST.NPER.ZS`, bulk zips (`src/DomesticFinance.cpp:327`), also fed to
  the ML data (`src/MlData.cpp:126`). `src/SourcesPage.cpp:687`.
- `research_worldbank`: Economy › Countries and Indicators, ids in `resources/country_indicators.json`: 46 WDI, 9 GEM
  (`CORENS`, `CPTOTNSXN`, `DMGSRMRCHNSCD`, `DSTKMKTXN`, `DXGSRMRCHNSCD`, `IPTOTSAKD`, `RETSALESSA`, `TOT`, `TOTRESV`), 1 WGI
  (`GOV_WGI_CC.SC`). One request by country and database (`src/CountryData.cpp:1239`), one by indicator for the
  cross-country table (`:2001`); keeps indicator id, date, value. `src/SourcesPage.cpp:484`.

64 distinct series of 29,565, from 3 databases of 71: 56 in Research, the 7 sectors, `FB.AST.NPER.ZS` (`FS.AST.PRVT.GD.ZS` is in both credit and Research).

## Not read yet

- GEM's `REER`, `NEER`, `UNEMPSA_`, `NYGDPMKTPSA*`, `IMPCOV`, `DPANUSLCU`: already in the database the app calls; Economy ›
  Countries, Forex.
- `GEP` (27): GDP growth forecasts for 163 countries to 2028: Desk › Forecasts, beside the ECB's survey and WEO.
- External and public debt by quarter: `QDS` (22), `QDG` (23), `PSD` (20), `JED` (54); debt by creditor: `IDS` (6) with its
  304 counterpart areas: Map, Economy › Countries.
- `GFD` (32): banking system depth and stability, to 2021: Map's domestic finance layer.
- The other 35 `WGI` series: governance heatmap on the Map.
- `LPI` (66): logistics scores by country: the supply chain layer.
- `HPP` (40): population projections to 2050: Economy › Countries.
- `FPN` (88): cost of a healthy diet by country.
- `WDA` (57): WDI vintages, for backtests on data as first published (Quant).

## Calls made

All 2026-11-05, User-Agent `Deepwater/1.0`, about one a second.

- `GET https://api.worldbank.org/v2/sources?format=json&per_page=300` → 200, 13,048 B, total 71
- `GET /v2/indicator?format=json&per_page=2` → 200, total 29,544; then `per_page=2000`, pages 1 to 15 → 200 (`tools/source-inventory/worldbank.py`)
- `GET /v2/topic?format=json&per_page=100` → 200, 13,358 B (twice), total 21
- `GET /v2/region?format=json&per_page=100` → 200, 3,564 B (twice), total 44
- `GET /v2/incomelevel?format=json` → 200, total 7; `GET /v2/lendingtype?format=json` → 200, total 4
- `GET /v2/country?format=json&per_page=2` → 200, total 296; `per_page=400` → 200; `GET /v2/countries?format=json&per_page=1` → 200
- `GET /v2/sources/2/concepts?format=json`, `/sources/15/concepts` → 200; `GET /v2/sources/2/country?format=json&per_page=2`, `/series`, `/time` → 200; `GET /v2/sources/2/metatypes?format=json` → 200, 5,267 B
- For each of the 71 databases: `GET /v2/sources/<id>/concepts?format=json`, then `GET /v2/sources/<id>/<concept>?format=json&per_page=5000&page=<n>` → 200, except `/sources/5/adminregion` and `/sources/77/receivingcountries` → 400, each tried three times
- `GET /v2/sources/15/time?format=json&per_page=700` → 200; `GET /v2/sources/22/time?format=json&per_page=5` → 200; `GET /v2/sources/16/year?format=json&per_page=100` → 200
- `GET /v2/sources/15/country/KEN/series/CPTOTNSXN/time/all?format=json&per_page=1` → 200, total 675
- `GET /v2/sources/2/series/SP.POP.TOTL/metadata?format=json` → 200
- `GET /v2/country/FRA/indicator/NV.SRV.TOTL.ZS?format=json&per_page=1` → 200 (field names and types kept)
- `GET /v2/country/KEN/indicator/CPTOTNSXN?source=15&format=json&mrv=1&frequency=M` → 200 (field names and types kept)
- `GET /v2/en/indicator/FS.AST.PRVT.GD.ZS?downloadformat=csv` → 200, 104,453 B, zip of 3 CSV (file names kept)
- `GET https://api.worldbank.org/robots.txt` → 404
