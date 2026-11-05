---
id: statistik_at
name: Statistics Austria
rows: []
catalogue: ["Statistics Austria", "Statistik Austria"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# Statistics Austria

The open data catalogue `data.statistik.gv.at`: 543 CSV data sets with JSON metadata. STATcube, its larger analysis tool, was not reached.

## Contents
543 data sets, complete in `catalog/statistik_at.tsv` (id, title in German, updated). Prefix `OGD_` and, for 71 of them, `OGDEXT_`.

Families by id: `vpi58`...`vpi25c18` CPI for every base year (1958, 1966, 1976, 1986, 1996, 2000, 2005, 2010, 2015, 2020, 2025), `hvpi25` HICP, `konjunkturmonitor` the monthly business-cycle monitor (production index industry and others;
`KonMon_1`), `konjidxdl21`, `konjidxhan21` turnover and employment indices in services and trade (monthly), `kjeunt25m`, `kjebet25m`, `kjeunt25mmde`, `kjebet25mmde` production sector (from 2025, monthly), `vgr111` national accounts flashes, `itgs`
foreign trade, `touextsai` tourism (monthly, from November 1973), `watlas` economic atlas by year, `bevstandjbab2002` and `bevjahresanf` population, `gesausgaben` health spending, `bildungsausgaben` education spending, and by id `ahlgrhs`, `gem`, `indquot001`, `krankenbewegungen`.

A data set has: `<id>.csv`, `<id>_HEADER.csv`, one `<id>_C-<var>-0.csv` per classification; metadata has `title`, `notes`, `tags`, `maintainer`, `license`, `resources`, `extras` (`update_frequency`, `begin_datetime`, `categorization`,
`attribute_description`, which names each `C-` classification and `F-` figure), for example `F-UIDXNOM` nominal turnover index, `C-NACEDL-0`.

## How to call it
- Catalogue page `https://data.statistik.gv.at/web/catalog.jsp` (about 1 MB of HTML, 543 distinct ids).
- Metadata `https://data.statistik.gv.at/ogd/json?dataset=OGD_konjidxhan21_KJIX_H_21_1`. Data `https://data.statistik.gv.at/data/<id>.csv`.
- `www.data.gv.at/katalog/api/3/action/organization_show?id=statistik-austria` is 404. STATcube's API was not called. The catalogue's "STATcube (CSV), no key" is unconfirmed; the keyless route that was verified is the open data here.

## What the app reads
Nothing directly. Austria comes through Eurostat and the OECD.

## Not read yet
`konjunkturmonitor`, `vpi25c18`, `hvpi25`, `kjeunt25m` for Countries › Austria.

## Calls made
Made on 2026-11-05, sequentially per host with a pause, `User-Agent: Deepwater/1.0`. 7 calls.

- `GET https://data.statistik.gv.at/web/catalog.jsp` → 200, 1.1 MB
- `GET https://www.data.gv.at/katalog/api/3/action/organization_show?id=statistik-austria` → 404, 5.1 KB
- `GET https://data.statistik.gv.at/ogd/json?dataset=…` × 4 → 200: 4, 18.5 KB in all; first `https://data.statistik.gv.at/ogd/json?dataset=OGD_watlas23_WATLAS_23`
- `GET https://www.statistik.at/services/tools/services/statcube` → 404, 87.7 KB
