---
id: bundled
name: Data files bundled with the app
rows: [bundled]
catalogue: []
access: local
probed: 2026-11-05
confidence: called   # the files were read from resources/; provenance comes from README.md and the files' own comments, not checked against their origins
---

# Data files bundled with the app

Ten JSON files in `resources/`, compiled into the app by `resources/resources.qrc` and read as `:/<name>.json`. No network.

## Contents

| File | Bytes | Records | Schema | Provenance |
|---|---|---|---|---|
| `countries.json` | 1,256,139 | 242 countries, 1,620 rings | list of `{n: name, iso: ISO3, r: [ring]}`, a ring being a flat `[lon, lat, lon, lat…]` | Natural Earth 1:50m, public domain, geometry only (README:1788) |
| `country_names.json` | 5,661 | 191 | `{ISO3: [ISO2, English name, continent code]}` | not stated |
| `country_indicators.json` | 93,011 | 313 indicators, 12 tabs, 6 groups | see below | written for the app (README:841) |
| `indicator_list.json` | 13,905 | 15 sections, 175 items | `sections[]`: `name`, `column`, `star` (one section), `items[]`: `name` (Trading Economics' names), `ids` (rows of `country_indicators`), `why` (18 items with no free source), `calendar` (1) | written for the app (README:967) |
| `markets.json` | 32,431 | 6 categories, 26 commodities, 166 sites | `categories[]`: `id`, `name`, `color`. `commodities[]`: `id`, `name`, `cat`, `imf` {`code`, `unit`, `what`}, `future` {`symbol`, `market`, `unit`} (20 of 26). `sites[]`: `name`, `country`, `lat`, `lon`, `commodity`, `type` | hand-kept sample (README:1787) |
| `ports.json` | 560,520 | 14,233 | list of `[LOCODE, name, lat, lon, major]`, `major` 1 on 3,923 ports, read at `src/SeaRoutes.cpp:149` | searoute-py's major ports with UN/LOCODE seaports, UNECE (README:174) |
| `searoutes.json` | 309,625 | 9,682 nodes, 15,917 edges | `nodes`: flat `[lon, lat…]` to 19,364 numbers; `edges`: flat node index pairs to 31,834 | Eurostat *marnet* (EUPL-1.2) via searoute-py (Apache-2.0); README:173 says to check EUPL-1.2 before redistributing |
| `sector_history.json` | 128,401 | 56 countries, 244 events, 1945 to 2025 | `{ISO3: {summary: {en, fr}, events[]: {en, fr, from, to}}}` | written accounts, checked against World Bank series (README:285) |
| `supply_chain.json` | 12,949 | 166 PSD countries, 44 oil sites | `psdCountries`: `{USDA PSD code: ISO3}`; `oilSites[]`: `id`, `name`, `iso3`, `lat`, `lon`, `kind` (`export` 14, `hub` 13, `import` 10, `reserve` 7), `eia`, `about` | hand-kept, positions within a few kilometres (README:383) |
| `data_sources.json` | 792,561 | 1,468 bodies, 213 countries, 7 areas; 21,917 pages, 17,473 indicator pages | top level `generated`, `about`, `pages`, `indicatorPages`, `sources`, `countries`, `areas`, `tested`. `sources[]`: `name`, `url`, `kind` (`gov` 701, `stat` 335, `cb` 232, `private` 151, `intl` 49), `access`, `api`, `countries`, `pages`, `test` {`date`, `ok`, `ms`, `status`, `host`}. `countries`: `{ISO3: {te, indicators: {slug: count}}}` | crawl of tradingeconomics.com's sitemap (its own `about`; generated 2026-11-01) |

**`country_indicators.json`**: `_comment`, `groups[]` (`name`, `countries`: G20 23, America 36, Europe 48, Africa 54, Asia 49, Oceania 10), `extraNames` (16), `euroMembers` (21), `oecdAreas` (50), `eurostatGeos` (39), `currencies` (191), `stockIndexes` (42), `tabs[]` (`id`, `name`, `tables[][]`: overview 22 rows, gdp 27, labour 59, prices 46, money 21, trade 20, government 20, business 39, consumer 30, housing 26, energy 16, health 7), `indicators`: `{id: {name, unit, src[]}}`.

`src[]` items are tried in order. Keys: `p` (provider, 680 entries), `id` 447, `flow` 200, `key` 181, `t` 161, `note` 143, `scale` 101, `unit` 65, `q` 65, `ds` 55, `source` 14, `since` 7, `only` 6, `classif` 4, `minus` 4, `kind` 3, `div` 1, `plus` 1.

Sources by provider (the three with `kind` counted under it): `fred` 214, `wb` 62, `imf` 60, `eurostat` 55, `oecd` 51, `ons` 29, `bcb` 22, `abs` 21, `boj` 21, `bbk` 20, `statcan` 19, `ilo` 17, `ecb` 13, `ecos` 12, `eia` 11, `estat` 7, `boc` 7, `weo` 6, `snb` 6, `bps` 5, `bis` 5, `insee` 4, `pxweb` 3, `survey` 3, `boe` 3, `currency`, `stock`, `daily`, `ine` 1 each.

**`markets.json`** commodities: `wti`, `brent`, `natgas`, `lng`, `coal`, `uranium`, `gold`, `silver`, `platinum`, `palladium`, `copper`, `aluminium`, `nickel`, `zinc`, `ironore`, `lithium`, `cobalt`, `wheat`, `corn`, `soy`, `palmoil`, `rice`, `coffee`, `cocoa`, `sugar`, `cotton`. Categories: `energy`, `precious`, `industrial`, `agri`, `softs`, `hub`. Site types include `Mine` 42, `Growing Region` 31, `Exchange` 13, `Export Terminal` 11, `Oil Field` 9, `LNG Terminal`, `Smelter`, `Offshore Field` 7 each.

Also in `resources/`, not data: `style.qss`, `icon/`, `help/` (16 chapters in en and fr), `ml/` (2 Python scripts), `quant/` (one JavaScript indicator). In code, not in files: RBA and RBNZ meeting dates, `desk::bundledMeetings` `src/desk/CentralBanks.h:70`, updated by hand each year.

## How to call it
`QFile(":/<name>.json")` in the app; `resources/<name>.json` on disk. French names of `markets.json` and `supply_chain.json` are in `i18n/deepwater_data_fr.ts`.

## What the app reads
Row `bundled`, `src/SourcesPage.cpp:855` (names four of the ten).
- `countries.json`: `src/GlobeWidget.cpp:73`.
- `markets.json`: `src/main.cpp:55`.
- `ports.json`, `searoutes.json`: `src/MainWindow.cpp:167-168`, `src/SeaRoutes.cpp:34`, `:135`.
- `country_names.json`: `src/CountryData.cpp:57`, `src/OfficialSources.cpp:138`, `src/DomesticFinance.cpp:56`, `src/MacroData.cpp:502`, `src/SupplyChain.cpp:90`.
- `country_indicators.json`: `src/CountryData.cpp:598`.
- `indicator_list.json`: `src/IndicatorData.cpp:24`.
- `sector_history.json`: `src/DetailPanel.cpp:287`.
- `supply_chain.json`: `src/SupplyChain.cpp:32`.
- `data_sources.json`: `src/OfficialSources.cpp:20`.

## Not read yet
`data_sources.json`'s `countries[…].indicators` gives page counts by indicator, which no view uses as a coverage table. `country_indicators.json`'s `note`, `since`, `only`: read by the loaders, not all shown; unchecked.

## Calls made
No network. Read `resources/*.json` with a local script on 2026-11-05; record counts above are from it.
