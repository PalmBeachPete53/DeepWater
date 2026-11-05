---
id: statbel
name: Statistics Belgium (Statbel)
rows: []
catalogue: ["Statistics Belgium"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# Statistics Belgium

Statbel publishes open data files and BeStat, a table tool whose views are listed by an open JSON call.

## Contents
- **Open data**: 282 data sets, listed in `catalog/statbel.tsv` (path, title, period, description, status, file types), each page read. Files: zip 270, xlsx 252, csv 11. Only 53 pages state a `Period` in text ("since 1920" for the CPI); the rest give it in the title or the file. Examples: `consumer-price-index-and-health-index`, `indexes-product-group`, `hvd-harmonised-index-consumer-prices-hicp-goods-and-services-purchased-households`,
  `hvd-unemployment-rate-people-aged-15-74`, `hvd-employment-rate-people-aged-20-64`, `building-permits-month-region-province-district-and-municipality`, `monthly-evolution-bankruptcies-according-nace`, population by municipality, first names, cadastre, NUTS and statistical sectors.
- **BeStat** `views`: 1,377 views, in `catalog/statbel_bestat.tsv` (id, locale, name, data source, last change, last publish), 184 data sources behind them. Locale: fr 506, nl 504, en 228, de 139, so one table appears up to four times. The view list does not carry
  dimensions. `locale=en` in the query does not filter.
- A page often links a variables list, as `List variables.xlsx`, not opened here. Belgium only.

The catalogue's `BelgoStat` row points to `statbel.fgov.be` and "open data (CSV)"; `BelgoStat` is grouped with the National Bank of Belgium instead, see `nbb.md`.

## How to call it
- `https://statbel.fgov.be/en/open-data?page=<n>` (10 a page), then each data set's page for its files, under `/sites/default/files/files/opendata/`. robots.txt allows these paths, it disallows `/search/`.
- `https://bestat.statbel.fgov.be/bestat/api/views`.

## What the app reads
Nothing directly. Belgium comes through Eurostat and the OECD.

## Not read yet
`consumer-price-index-and-health-index` (the health index exists only here), `monthly-evolution-bankruptcies-according-nace`, `building-permits-...`, for Countries › Belgium.

## Calls made
Made on 2026-11-05, sequentially per host with a pause, `User-Agent: Deepwater/1.0`. 322 calls.

- `GET https://statbel.fgov.be/robots.txt` → 200, 2.0 KB
- `GET https://statbel.fgov.be/en/open-data` → 200, 116.9 KB
- `GET https://bestat.statbel.fgov.be/bestat/api/views` → 200, 539.4 KB
- `GET https://bestat.statbel.fgov.be/bestat/api/views?locale=en` → 200, 539.4 KB
- `GET https://statbel.fgov.be/en/open-data?page=…` × 35 → 200: 35, 4.1 MB in all; first `https://statbel.fgov.be/en/open-data?page=0`
- `GET https://statbel.fgov.be/en/open-data/{slug}` × 283 → 200: 283, 26.1 MB in all; first `https://statbel.fgov.be/en/open-data/consumer-price-index-and-health-index`
