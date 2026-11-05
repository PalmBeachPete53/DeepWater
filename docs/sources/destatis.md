---
id: destatis
name: Federal Statistical Office of Germany (Destatis, GENESIS-Online)
rows: []
catalogue: ["Federal Statistical Office"]
access: free account
probed: 2026-11-05
confidence: docs     # the catalogue answers 401 without an account; none used. Structure from the official API manual.
---

# Federal Statistical Office of Germany

GENESIS-Online, Germany's official statistics database: statistics, tables, time series and cubes with their variables.

## Contents
The catalogue itself could not be read. From the API manual (GENESIS Anwenderdokumentation Webservice/API, version 5.1 of
2026-06-01, `GENESIS-Webservices_Einfuehrung.pdf`), the objects are:

- `catalogue/`: `cubes`, `cubes2statistic`, `cubes2variable`, `jobs`, `modifieddata`, `qualitysigns`, `results`, `statistics`,
  `statistics2variable`, `tables`, `tables2statistics`, `tables2variable`, `terms`, `timeseries`, `timeseries2statistic`,
  `timeseries2variable`, `values`, `values2variable`, `variables`, `variables2statistic`.
- `data/`: `cube`, `cubefile`, `table`, `tablefile`, `timeseries`, `timeseriesfile`, `result`, `resultfile`, and chart and map
  variants. `metadata/`: `cube`, `statistic`, `table`, and more. `find/find` searches. The manual's example statistic is
  `12411`.

No count of tables or series is known here. Many German indicators are available without an account as Bundesbank series
(see `bbk.md`: `BBDP1`, `BBDL1`, `BBDE1`, `BBDA1`). GovData's CKAN lists 0 data sets for the `destatis` organisation.

## How to call it
- Base `https://genesis.destatis.de/genesisWS/rest/2020/` (`www-genesis.destatis.de` redirects there).
- `helloworld/whoami` answers without credentials. `POST helloworld/logincheck` with no credentials answers 200 naming
  user `GAST`. `POST catalogue/statistics` with no credentials answers 401, code 15, "not allowed to call this service or the
  header of your request does not contain all the necessary information". A guest password is not in the manual and none
  was tried.
- The manual: credentials go in `username` and `password` headers, a personal token is accepted as `username`; GET with
  credentials was removed in favour of POST; SOAP was switched off; a limited number of requests may run in parallel, and
  `logincheck` clears hung ones.
- `GET catalogue/...` with a query string answers 405.

## What the app reads
Nothing directly. `research_bbk` reads German federal statistics through the Bundesbank (`src/SourcesPage.cpp:556`).

## Not read yet
All of it, after registering. Worth checking first: `catalogue/modifieddata` for a release signal, and anything the
Bundesbank does not mirror (regional tables, detailed wages).

## Calls made
Made on 2026-11-05, sequentially per host with a pause, `User-Agent: Deepwater/1.0`. 8 calls.

- `GET https://www-genesis.destatis.de/genesisWS/rest/2020/helloworld/whoami` → 200, 30 B
- `GET https://www-genesis.destatis.de/datenbank/online/docs/GENESIS-Webservices_Einfuehrung.pdf` → 200, 602.6 KB
- `GET https://www-genesis.destatis.de/datenbank/online/docs/` → 403, 3.2 KB
- `GET https://www-genesis.destatis.de/datenbank/online/` → 200, 2.5 KB
- `GET https://www-genesis.destatis.de/genesisWS/rest/2020/catalogue/statistics?selection=6*&pagelength=5` → 405, 0 B
- `POST https://genesis.destatis.de/genesisWS/rest/2020/catalogue/statistics` → 401, 203 B
- `GET https://genesis.destatis.de/genesisWS/rest/2020/helloworld/logincheck` → 200, 165 B
- `GET https://www.govdata.de/ckan/api/3/action/package_search?fq=organization:destatis&rows=0` → 200, 215 B
