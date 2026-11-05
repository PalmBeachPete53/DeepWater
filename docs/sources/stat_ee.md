---
id: stat_ee
name: Statistics Estonia
rows: []
catalogue: ["Statistics Estonia"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# Statistics Estonia

The Estonian statistical database over PxWeb.

## Contents
Two databases: `stat` and `statsql` (one branch, `majandus` Economy, not walked). `stat`: 4,998 tables, complete in `catalog/stat_ee.tsv` (table, path, title, published); 2,857 of them are in
`Lepetatud_tabelid` (discontinued). Current: `rahvaloendus` census 676, `sotsiaalelu` social life 604, `majandus` economy 491, `eri-valdkondade-statistika` 163, `rahvastik` population 157, `keskkond` environment 50.

Economy branch: `pellumajandus` 82, `rahvamajanduse-arvepidamine` national accounts 47, `teadus-tehnoloogia-innovatsioon` 47, `majandusuksused` 44, `infotehnoloogia` 44, `palk-ja-toojeukulu` wages 41, `transport` 31,
`hinnad` prices 30, `turism-ja-majutus` 28, `rahandus` 22, `energeetika` 17, `ehitus` 14.

Macro tables: `RAA0012` GDP and GNI (quarters), `Aasta`(32), `Kvartal`(5), `Sesoonne korrigeerimine`(2), `Näitaja`(6), 1995 to 2026; `IA001` CPI change over previous year, `Aasta`(27), `Kaubagrupp`(13), 1999 to 2025;
`IA002`, `IA021`, `IA029`, `IA039`, `IA40`, `IA43`; `sotsiaalelu/tooturg` 103 labour tables.

## How to call it
- `https://andmed.stat.ee/api/v1/en/stat` lists branches; `.../stat?query=*&filter=*` returns every table at once. Metadata `GET .../stat/majandus/hinnad/IA001.px`. Paths keep their parentheses
  (`sisemajanduse-koguprodukt-(skp)`); `IA001.px` answers where the catalogue prints `IA001.PX`. A wrong path is 400.

## What the app reads
Nothing directly. Estonia comes through Eurostat and the OECD.

## Not read yet
`RAA0012`, `IA021`, `hinnad` for Countries › Estonia.

## Calls made
Made on 2026-11-05, sequentially per host with a pause, `User-Agent: Deepwater/1.0`. 8 calls.

- `GET https://andmed.stat.ee/api/v1/en/` → 200, 67 B
- `GET https://andmed.stat.ee/api/v1/en/stat` → 200, 420 B
- `GET https://andmed.stat.ee/api/v1/en/stat?query=*&filter=*` → 200, 1.3 MB
- `GET https://andmed.stat.ee/api/v1/?config` → 404, 1.2 KB
- `GET https://andmed.stat.ee/api/v1/en/statsql` → 200, 47 B
- `GET https://andmed.stat.ee/api/v1/en/stat/majandus/hinnad/IA001.px` → 200, 933 B
- `GET https://andmed.stat.ee/api/v1/en/stat/majandus/rahvamajanduse-arvepidamine/sisemajanduse-koguprodukt-skp/SKP/RAA0012.px` → 400, 11 B
- `GET https://andmed.stat.ee/api/v1/en/stat/majandus/rahvamajanduse-arvepidamine/sisemajanduse-koguprodukt-(skp)/pehilised-rahvamajanduse-arvepidamise-naitajad/RAA0012.PX` → 200, 1.4 KB
