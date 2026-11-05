---
id: eia
name: U.S. Energy Information Administration
rows: [eia, research_eia, eia_stocks]
catalogue: ["U.S. Energy Information Administration"]
access: free key
probed: 2026-11-05
confidence: docs                      # the API's route tree needs a key this inventory must not read; the key-free bulk files answered and their structure is from a call
---

# U.S. Energy Information Administration

US and world energy: petroleum, gas, electricity, coal, nuclear, outlooks. The API wants a key, even for its route tree,
but EIA also publishes the same series as bulk files with no key, and those carry their own category tree.

## Contents
`api.eia.gov/bulk/manifest.txt` lists 29 bulk data sets. Downloaded and reduced to structure by
`tools/source-inventory/eia_bulk.py`, about 1.6 million series:

| Bulk data set | Title | Series | Categories | Frequencies (series) | First | Last | Units |
|---|---|---|---|---|---|---|---|
| `PET` | Petroleum and other liquid fuels | 181,216 | 14,077 | A 94,773, M 84,817, W 1,159, 4-week 436, D 31 | 1859 | 2026-11-03 | 29 |
| `PET_IMPORTS` | Petroleum Imports | 46,176 | 1,056 | M 23,166, A 23,010 | 2009 | 2026-08 | 1 |
| `NG` | Natural Gas | 16,094 | 2,098 | A 12,911, M 3,165, W 13, D 5 | 1900 | 2026-11-03 | 20 |
| `ELEC` | Electricity | 784,932 | 18,333 | M 262,519, Q 261,652, A 260,761 | 2001 | 2026Q2 | 19 |
| `COAL` | Coal | 188,658 | 12,224 | A 102,205, Q 86,453 | 2000 | 2026Q1 | 7 |
| `INTL` | International Energy Data | 104,434 | 232 | A 100,459, M 2,186, Q 1,789 | 1949 | 2026Q2 | 17 |
| `SEDS` | State Energy Data System | 48,046 | 970 | A | 1960 | 2024 | 31 |
| `TOTAL` | Total Energy (Monthly Energy Review) | 2,041 | 119 | A 1,057, M 984 | 1949 | 2028-08 | 43 |
| `STEO` | Short-Term Energy Outlook | 4,420 | 177 | A, Q, M, about 1,470 each | 1960 | 2027Q4 | 53 |
| `EMISS` | Emissions | 4,920 | 118 | A | 1970 | 2021 | 3 |
| `NUC_STATUS` | Nuclear outages | 522 | 66 | D | 2007-01-01 | 2026-11-05 | 2 |
| `IEO` | International Energy Outlook | 41,401 | 2,863 | A | 2010 | 2050 | 28 |
| `AEO.2026` | Annual Energy Outlook 2026 | 161,844 | 9,306 | A | 2024 | 2050 | 94 |

Not downloaded: `EBA` (hourly electric system operating data, the largest), older vintages `AEO.2014` to `AEO.2025`, `IEO.2017`,
`IEO.2019`, `IEO.2021`, `IEO.2023`.

Catalogue files, long, for grep:
- [`catalog/eia_bulk.jsonl`](catalog/eia_bulk.jsonl): the table above with top units.
- [`catalog/eia_categories.tsv.gz`](catalog/eia_categories.tsv.gz): every category of the tree (bulk set, id, parent, name, direct
  series), 61,638 rows. Example path in `PET`: Summary › Supply and Disposition.
- [`catalog/eia_series_families.tsv`](catalog/eia_series_families.tsv): series grouped by id family, with count, frequencies,
  units, first and last, one example name (5,894 rows). `INTL.57-1` is product 57, activity 1, for instance.

**API v2 routes.** Not listed by a call: the root answers 403 `API_KEY_MISSING`. From the app's own URLs: `/petroleum/stoc/wstk/data/`,
`/international/data/`, `/seriesid/<v1 id>`. From EIA's API documentation: `/electricity/retail-sales/data/`. As remembered from
EIA's dashboard, to be confirmed with a key: `aeo`, `co2-emissions`, `coal`, `crude-oil-imports`, `densified-biomass`,
`electricity`, `ieo`, `international`, `natural-gas`, `nuclear-outages`, `petroleum`, `seds`, `steo`, `total-energy`.
The documentation says each node answers its children and its modalities, frequencies and facets when called without `/data`.

## How to call it
- Manifest, no key: `https://api.eia.gov/bulk/manifest.txt`. Files: `https://www.eia.gov/opendata/bulk/PET.zip` (56 MB; `ELEC` 302 MB),
  one JSON object a line, either a series (`series_id, name, units, f, start, end, data`) or a category (`category_id,
  parent_category_id, name, notes, childseries`).
- API, with a key (documented form): `https://api.eia.gov/v2/petroleum/stoc/wstk/data/?api_key=<key>&frequency=weekly&data[0]=value&facets[series][]=WCESTUS1&start=2024-01-01&length=5000`.
- `/v2/seriesid/PET.WCESTUS1.W` translates v1 ids. Limits from the documentation: 5,000 rows per call (300 in XML), then page with
  `offset`. Values are strings since v2.1.6.
- `robots.txt` excludes `/global/`, `/archive/`, `/cfapps/` and others, not `/opendata/`.

## What the app reads
- `eia` (Map): `PET.WCESTUS1.W`, `PET.WCRFPUS2.W`, `PET.EMM_EPM0_PTE_NUS_DPG.W` via `/v2/seriesid/`. `src/KeyedSources.cpp:131`.
- `eia_stocks` (Map › Supply chains): `/v2/petroleum/stoc/wstk/data/`, series `W_EPC0_SAX_YCUOK_MBBL`, `WCESTP31`, `WCSSTUS1`, `WCESTUS1`, three
  years. `src/SupplyChain.cpp:572`.
- `research_eia` (Countries, Indicators): 11 ids in `resources/country_indicators.json` (`PET.WGTSTUS1.W`, `PET.WDISTUS1.W`,
  `NG.NW2_EPG0_SWO_R48_BCF.W`, `PET.MCRFPUS2.M`, `INTL.57-1-{ISO3}-TBPD.M`, ...), `src/CountryData.cpp:1903`; every country at once through
  `/v2/international/data/`, `src/CountryData.cpp:2143`.

## Not read yet
- `STEO`: EIA's own forecasts to 2027, for Desk › Forecasts.
- `INTL`: 104,434 series by product and activity, far beyond crude production, for Countries and Map.
- `NG` storage by region, `PET` imports by origin (`PET_IMPORTS`), for Map › Supply chains.
- `ELEC`, `NUC_STATUS`: daily nuclear outages and US power generation.
- `EBA`: hourly grid data.

## Calls made
- `GET https://www.eia.gov/robots.txt` → 200, 718 bytes
- `GET https://api.eia.gov/v2/` → 403, 163 bytes
- `GET https://api.eia.gov/bulk/manifest.txt` → 200, 28,398 bytes
- `GET https://www.eia.gov/opendata/documentation.php` → 200, 127,328 bytes
- `GET https://www.eia.gov/opendata/browser/` → 200, 98,345 bytes
- `GET https://www.eia.gov/opendata/bulkfiles.php` → 200, 72,027 bytes
- `GET https://www.eia.gov/opendata/browser/main-QSMAUV7W.js` → 200, 1,637,793 bytes
- Bulk ZIPs by `tools/source-inventory/eia_bulk.py`: `GET https://www.eia.gov/opendata/bulk/<PET, PET_IMPORTS, NG, TOTAL, STEO, INTL, SEDS, COAL, EMISS, NUC_STATUS, ELEC, IEO, AEO2026>.zip` → 200, sizes in `catalog/eia_bulk.jsonl` (one failed first pass on `COAL` was the script's own bug, not the source).
