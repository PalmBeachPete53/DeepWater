---
id: cbsisrael
name: Central Bureau of Statistics, Israel
rows: []
catalogue: ["Central Bureau of Statistics, Israel"]
access: free, no key
probed: 2026-11-05
confidence: refused
---

# Central Bureau of Statistics, Israel

Israel's national statistics. Every address of the bureau timed out from this inventory's machine on 2026-11-05, on two rounds about an hour apart, though the site
probe of the same morning had reached it. Nothing here describes its content first-hand.

## Contents
Nothing observed, beyond what the body probe recorded (`catalog/bodies_probe.jsonl.gz`): a home page in Hebrew and a sitemap.

From general knowledge, not from a call: it offers a series API at `apis.cbs.gov.il/series/catalog/level?id=<n>&format=json&lang=en` and `.../data/list`, and a price index API at
`api.cbs.gov.il/index/...`. The catalogue calls this "CBS OData API": could not be checked.

**Many of its figures are readable through the Bank of Israel's SDMX service**: `PRI` prices, `NA` national accounts, `LBM` labour, `FTR` foreign trade, `DEM` demography. See
`docs/sources/boi.md`.

## How to call it
Unknown from here. A later try, or a try from another network, may answer.

## What the app reads
Nothing: no row in `src/SourcesPage.cpp`.

## Not read yet
Unknown.

## Calls made
- `GET https://apis.cbs.gov.il/series/catalog/level?id=1&format=json&download=false&lang=en` → timeout, 40 s (twice)
- `GET https://api.cbs.gov.il/index/catalog/catalog?format=json&download=false&lang=en` → timeout, 40 s
- `GET https://www.cbs.gov.il/en/Pages/Api-interface.aspx` → timeout, 40 s
- `GET https://www.cbs.gov.il/` → timeout, 30 s
- `GET https://www.cbs.gov.il/robots.txt` → timeout, 30 s
