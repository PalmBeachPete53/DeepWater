---
id: snb
name: Swiss National Bank (data portal)
rows: [research_snb]
catalogue: ["Swiss National Bank"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# Swiss National Bank

The data portal `data.snb.ch`: rates, exchange rates, the Bank's balance sheet, monetary aggregates, banking statistics, the balance of payments, and Swiss economic data from other offices, in cubes.

## Contents
The portal's sitemap lists 1,154 cubes, all in `catalog/snb.tsv` (cube, kind, group, title, publishing, source, dimensions, series, first, last, public since), dimensions in `catalog/snb_structures.jsonl`.

- **Topic cubes**, 242, fully read: 63,874 series over all, periods from 1914-01 to forecasts in 2030. Groups: `banken` banks 55, `aube` external sector 50, `uvo` 46, `ziredev` rates and exchange rates 39, `snb` the Bank 21, `finma` 18, `cross` 13. Publications: Economic data 58; Annual banking statistics 33; Balance of payments and investment position 25; Monthly banking statistics 21; Direct Investment 19; Financial Accounts 16; SNB balance sheet items 13; Exchange rate indices 13; Quarterly banking statistics 10; Interest rates and exchange rates 10; IMF SDDS Plus 9; Foreign exchange and derivatives trading 6; Important monetary policy data 3; Quarterly Bulletin 3; Data no longer updated 2.
  Sources: SNB 190; SFSO 14; SECO 8; FINMA 3; SNB with BIS 3; Customs (FOCBS), SIX, Wüest Partner, Fahrländer Partner with IAZI, IMF, OECD at low counts.
- Key cubes: `zimoma` money market rates (13 series, 1972-01 on), `snboffzisa` official rates, `snbgwdzid` interest rates and threshold factor, `snbgwdchfsgw` sight deposits, `snbbipo` balance sheet (28, 1996-12 on), `snbmoba` monetary base, `snbmonagg` M1, M2, M3 (Level/change(2) by Monetary aggregates(8); 1984-12 on), `rendeiduebd` spot rates on Confederation and euro area bonds and CHF issues (Bond categories(10) by Maturity(12); 1988-01 on, daily), `devkum` exchange rates (Monthly average/End of month(2) by Currency(28); 1914-01 on), `devwkieffid` effective indices, daily, `bopoverq` balance of payments (42, 1985-Q1 on), `snbiprogq`, `snbiproga` conditional inflation forecast, `snbkosiq` business cycle signals, `snbimfra`, `plkopr` CPI (2, 1921-01 on).
- **Warehouse cubes**, 912, kinds `BSTA` 670, `ZAST` 166, `ZAHL` 28, `DDUM` 16, `KRED` 14, `SNB1A` 11, `WKI` 7. Only titles were asked, ids like `BSTA@SNB.AUR_U.ODF`. **The last 138 of them, from `ZAST@SNB.IEA.BOPC.1.AB.070000` on, answered 503**, after 1,500 calls at a 0.6 s pace, so they have no title. No retry was made.

## How to call it
- A cube: `https://data.snb.ch/api/cube/plkopr/dimensions/en` (JSON), `/api/cube/plkopr/data/csv/en` (`;`, a few metadata lines, then `"Date";"D0"[;"D1"];"Value"`), `?fromDate=...` (the app's use, not called here), `/api/cube/plkopr/lastUpdate`.
- `robots.txt` has no disallow and points to `https://data.snb.ch/sitemap`, which is the only full list found.
- Title and source: `https://data.snb.ch/json/table/getCubeInfo?lang=en&isWarehouse=false&cubeId=plkopr&pageViewTime=<yyyymmdd_hhmmss>`, from the portal's own front end.
- `/api/warehouse/cube/BIZ@SNB.DDUM.FXD/dimensions/en` answers 400 "warehouse cubeId contains illegal characters"; the warehouse route was not worked out. `getNavigationTree` without `topicId` is 400.
- `plkopr` key `LD2010100` still says 2010 in its code, while its label is "National index – December 2025 = 100".

## What the app reads
Row `research_snb` (`src/SourcesPage.cpp:549`), loader `src/CountryData.cpp:1716`; it fetches the whole cube and keeps one key.
- `plkopr` `VVP`, `LD2010100` (`resources/country_indicators.json:284`, `:310`); `zimoma` `SARON` (`:458`); `snbmonagg` `B.GM1`, `B.GM2`, `B.GM3` (`:475`, `:483`, `:919`).
- Desk › Rates: `rendeiduebd` with `fromDate` (`src/DeskViewsMacro.cpp:545`, `src/desk/CentralBanks.cpp:414`). Meeting dates: `www.snb.ch/en/services-events/digital-services/event-schedule` (`src/DeskData.cpp:71`, `src/desk/CentralBanks.cpp:104`).

## Not read yet
`snboffzisa` and `snbgwdzid` the policy rate, `snbiprogq` the conditional forecast for Desk › Rates; `snbbipo` and `snbgwdchfsgw` sight deposits; `devwkieffid`; `snbkosiq`; `bopoverq`; the banking cubes in `banken`, for Countries › Switzerland.

## Calls made
Made on 2026-11-05, sequentially per host with a pause, `User-Agent: Deepwater/1.0`. 1653 calls.

- `GET https://data.snb.ch/en/topics` → 200, 3.5 KB
- `GET https://data.snb.ch/api/cube/{id}/dimensions/en` × 244 → 200: 244, 366.9 KB in all; first `https://data.snb.ch/api/cube/plkopr/dimensions/en`
- `GET https://data.snb.ch/sitemap.xml` → 404, 1.4 KB
- `GET https://data.snb.ch/robots.txt` → 200, 53 B
- `GET https://data.snb.ch/en/help/api` → 200, 3.5 KB
- `GET https://data.snb.ch/sitemap` → 200, 1.9 MB
- `GET https://data.snb.ch/main-GLJC6ALH.js` → 200, 996.8 KB
- `GET https://data.snb.ch/api/warehouse/cube/BIZ@SNB.DDUM.FXD/dimensions/en` → 400, 71 B
- `GET https://data.snb.ch/api/cube/plkopr/lastUpdate` → 200, 65 B
- `GET https://data.snb.ch/json/application/properties` → 200, 158 B
- `GET https://data.snb.ch/json/structure/getNavigationTree?lang=en` → 400, 1.4 KB
- `GET https://data.snb.ch/json/table/getCubeInfo?lang=…&isWarehouse=…&cubeId=…&pageViewTime=…` × 1156 → 200: 1018, 503: 138, 767.2 KB in all; first `https://data.snb.ch/json/table/getCubeInfo?lang=en&isWarehouse=false&cubeId=plkopr&pageViewTime=20261105_144141`
- `GET https://data.snb.ch/api/cube/{id}/data/{id}/en` × 243 → 200: 243, 433.2 MB in all; first `https://data.snb.ch/api/cube/plkopr/data/csv/en`
