---
id: statfi
name: Statistics Finland
rows: []
catalogue: ["Statistics Finland"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# Statistics Finland

PxWeb databases of Finnish official statistics, by statistic and table.

## Contents
- 11 databases: `StatFin`, `StatFin_Passiivi` (archive), `Check`, `Hyvinvointialueet`, `Kokeelliset_tilastot`, `Kuntien_avainluvut`, `Kuntien_talous_ja_toiminta`,
  `Maahanmuuttajat_ja_kotoutuminen`, `NOVI-fi`, `Postinumeroalueittainen_avoin_tieto`, `SDG`.
- `StatFin`: 135 statistics, 1,539 tables, complete in `catalog/statfi.tsv` (table, path, title with its period, published). Largest: `tyonv` labour offices 72, `alvaa` 69, `eot` 65,
  `kvaa` 54, `rpk` 52, `tyti` labour force 39, `evaa` 39, `tkke` 39, `vaerak` 32, `yrti` 30. `StatFin_Passiivi` 2,522 tables, `Kuntien_talous_ja_toiminta` 122,
  `Kokeelliset_tilastot` 63, `Kuntien_avainluvut` 15, `Maahanmuuttajat_ja_kotoutuminen` 13, the others 1 each or not counted (`NOVI-fi`, `Postinumeroalueittainen_avoin_tieto`).
- Macro tables: `khi/15b5.px` CPI 2025=100, `timeperiod_m`(381), `coicop_46_20231201`(390), `contentscode`(5), 1995M01 to 2026M09. `ntp/132h.px` quarterly GDP, `timeperiod_q`(146),
  `taloustoimi_1_20180101`(57), `contentscode`(14), 1990Q1 to 2026Q2. `ktkk/132f.px` trend indicator of output, `toimiala_79_20180101`(4), `contentscode`(8), 1995M01 to 2026M09.
  `tyti/135z.px` labour force key indicators; `kihi/11jb.px`, `11jf.px` house prices; `ati/14um.px` wages.

## How to call it
- `https://pxdata.stat.fi/PXWeb/api/v1/en/StatFin` lists statistics. `.../StatFin?query=*&filter=*` returns every table at once (path, title, published).
- Metadata `GET .../StatFin/khi/15b5.px`. Data is a POST on the same URL in PxWeb v1 (not called). The table file name needs no `statfin_..._pxt_` prefix: `statfin_khi_pxt_11xb.px` is 400.

## What the app reads
Nothing directly. Finland comes through Eurostat and the OECD in Countries.

## Not read yet
`15b5`, `132h`, `132f` for Countries › Finland.

## Calls made
Made on 2026-11-05, sequentially per host with a pause, `User-Agent: Deepwater/1.0`. 18 calls.

- `GET https://pxdata.stat.fi/PXWeb/api/v1/en/` → 200, 613 B
- `GET https://pxdata.stat.fi/PXWeb/api/v1/en/StatFin` → 200, 8.5 KB
- `GET https://pxdata.stat.fi/PXWeb/api/v1/en/StatFin?query=*&filter=*` → 200, 285.7 KB
- `GET https://pxdata.stat.fi/PXWeb/api/v1/?config` → 404, 1.2 KB
- `GET https://pxdata.stat.fi/PXWeb/api/v1/en/Check?query=*&filter=*` → 200, 185 B
- `GET https://pxdata.stat.fi/PXWeb/api/v1/en/Hyvinvointialueet?query=*&filter=*` → 200, 167 B
- `GET https://pxdata.stat.fi/PXWeb/api/v1/en/Kokeelliset_tilastot?query=*&filter=*` → 200, 11.2 KB
- `GET https://pxdata.stat.fi/PXWeb/api/v1/en/Kuntien_avainluvut?query=*&filter=*` → 200, 2.1 KB
- `GET https://pxdata.stat.fi/PXWeb/api/v1/en/Kuntien_talous_ja_toiminta?query=*&filter=*` → 200, 23.0 KB
- `GET https://pxdata.stat.fi/PXWeb/api/v1/en/Maahanmuuttajat_ja_kotoutuminen?query=*&filter=*` → 200, 2.7 KB
- `GET https://pxdata.stat.fi/PXWeb/api/v1/en/SDG?query=*&filter=*` → 200, 135 B
- `GET https://pxdata.stat.fi/PXWeb/api/v1/en/StatFin_Passiivi?query=*&filter=*` → 200, 509.8 KB
- `GET https://pxdata.stat.fi/PXWeb/api/v1/en/StatFin/khi/statfin_khi_pxt_11xb.px` → 400, 11 B
- `GET https://pxdata.stat.fi/PXWeb/api/v1/en/StatFin/kvtp/statfin_ntp_pxt_132h.px` → 400, 11 B
- `GET https://pxdata.stat.fi/PXWeb/api/v1/en/StatFin/tyti/statfin_tyti_pxt_135z.px` → 400, 11 B
- `GET https://pxdata.stat.fi/PXWeb/api/v1/en/StatFin/khi/15b5.px` → 200, 29.2 KB
- `GET https://pxdata.stat.fi/PXWeb/api/v1/en/StatFin/ntp/132h.px` → 200, 7.5 KB
- `GET https://pxdata.stat.fi/PXWeb/api/v1/en/StatFin/ktkk/132f.px` → 200, 8.7 KB
