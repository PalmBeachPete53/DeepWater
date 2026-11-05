---
id: cso
name: Central Statistics Office Ireland (PxStat)
rows: []
catalogue: ["Central Statistics Office Ireland"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# Central Statistics Office Ireland

PxStat: every Irish table in one catalogue call, its dimensions and all its periods included.

## Contents
13,188 tables, complete in `catalog/cso.tsv.gz` (matrix, label, frequency, time label, first, last, number of periods, updated, dimensions, copyright). Annual `A1` 11,900; quarterly `Q1` 831; monthly `M1` 376; weekly `W1` 45; daily `D1` 29; half-yearly `H1` 7. Earliest period 1841 (census), last update the call's day.
Matrix code first letters: `S` 2,181, `C` 1,759, `E` 1,324, `I` 1,205, `H` 1,052, `F` 791, `B` 727, `T` 603, `A` 589, `P` 500, `N` 395, `G` 289, `D` 266, `R` 239, `M` 232, `V` 221, `W` 196, `O` 194, `Q` 159, `U` 124, `L` 82, `K` 37, `J` 16, `Y` 7.

Macro tables still current: `NAQ03` GDP and GNP, `NAQ05` modified domestic demand, 1995 on; `QLF18`, `QLF02`, `QLF27`, `QLF31`, `QLF51` labour force, 1998 on; `LRM01` Live Register, 1967 on, with `LRM06`-`LRM21`; `DIPM01`, `DIPQ01` production; `MIQ05`; `HPQ01` residential property prices. CPI: `CPM02` 1922 to 2026-09, `CPM20`, `CPM21`, `CPM24`, `CPM25`, `CPM22` contributions, `CPM23` HICP, `CPM12` average prices, 1996 to 2026-09. The earlier base stopped at 2025-12: `CPM01` (`STATISTIC`(8), `TLIST(M1)`(602), `C01779V03424`(13)), `CPM03`, `CPM15`, `CPM18`, `CPM19`. `CBM03` daily card payments and `SLI01` are one-off pandemic series, ended 2021.

## How to call it
- Catalogue `https://ws.cso.ie/public/api.restful/PxStat.Data.Cube_API.ReadCollection` (46 MB of JSON-stat; each item has `id`, `label`, `updated`, `dimension`, `extension.matrix`).
- Table `https://ws.cso.ie/public/api.restful/PxStat.Data.Cube_API.ReadDataset/CPM01/JSON-stat/2.0/en`, or `/CSV/1.0/en`, `/PX/2013/en`, `/XLSX/2007/en`.
- **Trap: 10,137 of the catalogue's `href`s point to `dev-ws2.cso.ie`, which does not resolve** (2,968 to `ws.cso.ie`, 83 to `data.cso.ie`). Build URLs from `extension.matrix` on `ws.cso.ie`.

## What the app reads
Nothing directly. Ireland comes through Eurostat and the OECD.

## Not read yet
`NAQ03` and `NAQ05` (GDP beside modified domestic demand), `CPM02`, `LRM01`, `HPQ01`, for Countries › Ireland.

## Calls made
Made on 2026-11-05, sequentially per host with a pause, `User-Agent: Deepwater/1.0`. 3 calls.

- `GET https://ws.cso.ie/public/api.restful/PxStat.Data.Cube_API.ReadCollection` → 200, 45.9 MB
- `GET https://ws.cso.ie/public/api.restful/PxStat.Data.Cube_API.ReadDataset/CPM01/JSON-stat/2.0/en` → 200, 344.3 KB
- `GET https://dev-ws2.cso.ie/public/api.restful/PxStat.Data.Cube_API.ReadDataset/B0101/CSV/1.0/en` → ERR, 0 B
