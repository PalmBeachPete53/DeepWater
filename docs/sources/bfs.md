---
id: bfs
name: Swiss Federal Statistical Office (FSO, BFS)
rows: []
catalogue: ["Swiss Federal Statistical Office"]
access: free, no key
probed: 2026-11-05
confidence: called     # stopped on a 429 from the PxWeb API before all tables had a title
---

# Swiss Federal Statistical Office

Swiss official statistics: part of them in a PxWeb API, all of them catalogued on opendata.swiss.

## Contents
- **PxWeb**: 633 databases, one table each, ids `px-x-<domain><...>_<n>`, in `catalog/bfs_px.tsv`. 138 have their title, the others only the id (see *How to call it*). By FSO domain, the 2 digits after `px-x-`: 15 164; 01 124; 13 66; 07 56; 09 50; 21 42; 06 35; 40 34; 14 17; 03 13; 10 8; 17 8; 11 6; 02 5; 16 3; 19 2. **Domains 04 and 05 are absent from PxWeb**; in the FSO's numbering, as generally known and not read here, they are the national economy and prices. Titled examples: population by canton, marriages, Tourism Satellite Accounts, turnover statistics in services (quarterly, yearly), environment accounts.
- **opendata.swiss**: 3,395 packages of the organisation `bundesamt-fur-statistik-bfs`, in `catalog/bfs_opendata.tsv` (name, title, groups, formats, modified). Groups: society 1,333, economy 699, education 468, justice 292, government 217, agriculture 158, health 149, transport 114, environment 112, regions 18, energy 11. Formats: HTML 3,384, XLS 1,728, SERVICE 855, PDF 481, CSV 314, ODS 254, JSON 123, WMS 17. So most are links and spreadsheets. The consumer price index appears here as XLS and HTML (`landesindex-der-konsumentenpreise-lik-2021`, `lik-dezember-2015100-...`), and as a cube of the SNB (`plkopr`, source SFSO; see `snb.md`).

## How to call it
- PxWeb `https://www.pxweb.bfs.admin.ch/api/v1/en/` lists the databases; `.../en/<db>` gives its table name.
- **443 of the 583 databases called answer 400 at `/en/`**, and `.../<db>/<db>.px` as well, perhaps since they exist only in other languages (not tested). **A 429 came after 583 calls at one per 1.2 s; the walk was stopped there.**
- CKAN `https://ckan.opendata.swiss/api/3/action/package_search?fq=organization:bundesamt-fur-statistik-bfs&rows=1000&start=0`.

## What the app reads
Nothing directly. Swiss CPI comes through the SNB (`snb.md`).

## Not read yet
Domain 15 and 13 tables on PxWeb for regional detail. For prices and national accounts, the SNB and the OECD are simpler.

## Calls made
Made on 2026-11-05, sequentially per host with a pause, `User-Agent: Deepwater/1.0`. 588 calls.

- `GET https://www.pxweb.bfs.admin.ch/api/v1/en/` → 200, 38.0 KB
- `GET https://ckan.opendata.swiss/api/3/action/package_search?fq=organization:bundesamt-fur-statistik-bfs&rows=0` → 200, 222 B
- `GET https://www.pxweb.bfs.admin.ch/api/v1/en/{db}` × 581 → 400: 442, 200: 138, 429: 1, 31.3 KB in all; first `https://www.pxweb.bfs.admin.ch/api/v1/en/px-x-0304010000_101`
- `GET https://www.pxweb.bfs.admin.ch/api/v1/en/px-x-0304010000_101/px-x-0304010000_101.px` → 400, 11 B
- `GET https://ckan.opendata.swiss/api/3/action/package_search?fq=…&rows=…&start=…` × 4 → 200: 4, 60.9 MB in all; first `https://ckan.opendata.swiss/api/3/action/package_search?fq=organization:bundesamt-fur-statistik-bfs&rows=1000&start=0`
