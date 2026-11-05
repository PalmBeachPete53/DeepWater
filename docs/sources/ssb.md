---
id: ssb
name: Statistics Norway (SSB)
rows: [research_pxweb]
catalogue: ["Statistics Norway"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# Statistics Norway

StatBank over PxWeb API 2: every Norwegian statistical table with its variables, including money and interest rates.

## Contents
3,773 tables, complete in `catalog/ssb.tsv.gz` (id, label with its period, subject, path, time unit, first, last, updated, variables).
Annual 3,150, Other 252, Quarterly 237, Monthly 131, Weekly 3. 3,266 updated in 2025-2026.

By top subject: Immigration and immigrants 719; Labour market and earnings 388; Population 286; Agriculture, forestry, hunting and
fishing 246; Health 235; Culture and recreation 222; Establishments, enterprises and accounts 219; Construction, housing and property 210;
Nature and the environment 189; Income and consumption 164; Social conditions, welfare and crime 144; Public sector 127; Banking and
financial markets 100; Transport and tourism 88; Energy and manufacturing 86; Svalbard 59; Technology and innovation 58; External economy
55; Elections 55; Education 51; National accounts and business cycles 49; Wholesale and retail trade 18; Prices and price indices 5.

Structure of macro tables, from their metadata:
- `14706` derived CPI (CPI-AT, CPI-ATE, CPI-AE, CPI-AEL, CPI-XADM), 2025=100, 1995M01 to 2026M09. `14708` seasonally adjusted CPI and
  CPI-ATE, `14702`, `14704`.
- `09190` final expenditure and GDP, `Makrost`(51), `ContentsCode`(9), `Tid`(194) quarters, 1978K1 to 2026K2; `11721` the same with
  `ContentsCode`(11), `Tid`(126).
- `13760` labour force, `Kjonn`(3), `Alder`(3), `Justering`(4), `ContentsCode`(8), `Tid`(249) months, 2006M01 on.
- `03013` CPI by consumption group: **closed series**, `Konsumgrp`(339), `ContentsCode`(4), to 2025M12.
- `10945`-`10949` M1, M2, M3, base money; `10729`, `10738`, `10745`, `10746`, `10748` lending rates; `11018`, `11019` deposit rates.

## How to call it
- Current API: `https://data.ssb.no/api/pxwebapi/v2/tables?lang=en&pageSize=1000&pageNumber=1` (catalogue, also `&query=CPI-ATE`),
  `.../tables/09190/metadata?lang=en`, `.../tables/09190/data?lang=en&outputFormat=json-stat2&valuecodes[ContentsCode]=...&valuecodes[Tid]=*`.
- The app's `v2-beta` path still answers the same way. Closed tables are not in the catalogue list but keep answering by id.

## What the app reads

Corrected on 5 November 2026: the app now reads `14706` (`KPIavledetSerie=KPI-JAE`), since `05327` closed at 2025M12, on the current `v2` API.
Row `research_pxweb` (`src/SourcesPage.cpp:568`), loader `src/CountryData.cpp:1589`, `flow: ssb`:
- `05327` `Konsumgrp=JAE_TOTAL`, `ContentsCode=Tolvmanedersendring`: Norway core inflation, CPI-ATE (`resources/country_indicators.json:324`).
  **This table is closed: its label says "(closed series) 1995M01-2025M12", updated 2026-01-09, and it is absent from the v2 catalogue.
  The continuation is `14706` (base 2025). So the app's figure stops at December 2025.**
- `09190` `Makrost=bnpb.nr23_9fn`, `ContentsCode=VolumEndrSesJust`: Mainland GDP growth (`resources/country_indicators.json:48`).
  Keeps period and value from JSON-stat.

## Not read yet
`14706` to fix the above; `10945`-`10949` money; `10748` mortgage rates; `13760` unemployment, for Countries › Norway.

## Calls made
Made on 2026-11-05, sequentially per host with a pause, `User-Agent: Deepwater/1.0`. 16 calls.

- `GET https://data.ssb.no/api/pxwebapi/v2/tables?lang=…&pageSize=…&pageNumber=…` × 5 → 200: 5, 5.8 MB in all; first `https://data.ssb.no/api/pxwebapi/v2/tables?lang=en&pageSize=100&pageNumber=1`
- `GET https://data.ssb.no/api/pxwebapi/v2-beta/tables?lang=en&pageSize=100&pageNumber=1` → 200, 171.0 KB
- `GET https://data.ssb.no/api/pxwebapi/v2/tables/{id}?lang=…` × 8 → 200: 8, 150.5 KB in all; first `https://data.ssb.no/api/pxwebapi/v2/tables/05327/metadata?lang=en`
- `GET https://data.ssb.no/api/pxwebapi/v2-beta/tables/05327/metadata?lang=en` → 200, 18.1 KB
- `GET https://data.ssb.no/api/pxwebapi/v2/tables?lang=en&query=CPI-ATE` → 200, 11.2 KB
