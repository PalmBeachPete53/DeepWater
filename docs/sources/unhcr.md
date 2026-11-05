---
id: unhcr
name: United Nations Refugee Agency
rows: []
catalogue: ["United Nations Refugee Agency"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# UNHCR

The Refugee Population Statistics Database: refugees, asylum seekers, internally displaced and stateless people, by country of origin and of asylum.

## Contents
Twelve endpoints; `/years/` lists 77 years, 1951 to 2027. Fields are from one row of each:

| Endpoint | Fields beyond the common ones |
|---|---|
| `/population/` | `refugees, asylum_seekers, returned_refugees, idps, returned_idps, stateless, ooc, oip, hst` |
| `/demographics/` | `f_0_4, f_5_11, f_12_17, f_18_59, f_60, f_other, f_total`, the same for `m_`, `total` |
| `/asylum-applications/` | `procedure_type, app_type, dec_level, app_pc, applied` |
| `/asylum-decisions/` | `procedure_type, dec_level, dec_pc, dec_recognized, dec_other, dec_rejected, dec_closed, dec_total` |
| `/solutions/` | `returned_refugees, resettlement, naturalisation, returned_idps` |
| `/idmc/` | `total` (internally displaced, from IDMC by its name) |
| `/unrwa/` | `total` |
| `/nowcasting/` | `refugees, asylum_seekers, source, month` |
| `/footnotes/` | `footnote, year, coo_id, coo, coa, coa_id, coo_iso, coa_iso, population_type` |
| `/countries/` | 232 rows: `id, code, iso, iso2, name, nameOrigin, nameLong, nameShort, nameFormal, nationality, majorArea, region, nameFr, majorAreaFr, regionFr` |
| `/regions/` | 6 rows: `id, name, name_fr` |
| `/years/` | `year` |

Common fields: `year, coo_id, coo_name, coo, coo_iso, coa_id, coa_name, coa, coa_iso` (`coo` origin, `coa` asylum). A row's fields are typed as strings where the source writes `-` for nothing, numbers otherwise. Without a filter, an endpoint returns one row per year.

## How to call it
- Base `https://api.unhcr.org/population/v1`, JSON, no key. Documentation: `https://api.unhcr.org/docs/refugee-statistics.html`.
- `/population/?limit=1&yearFrom=2024&yearTo=2024&coo_all=true`; parameters `limit, page, yearFrom, yearTo, year, download, coo, coa, coo_all, coa_all`; `cf_type` on the asylum endpoints. An answer holds `page, maxPages, total, items`.
- The catalogue's `url` `http://popstats.unhcr.org` timed out on its test, and its `api` says "UN Comtrade API…": wrong on both.

## What the app reads
Nothing.

## Not read yet
**Economy › Countries, Labour or Government tab:** refugees and asylum applications by country, for countries beyond Eurostat's `migr_asyappctzm`, which the app reads.

## Calls made
All on 2026-11-05 with `User-Agent: Deepwater/1.0`. Base `https://api.unhcr.org/population/v1`; values were not kept.
- `GET /` → 200, 104 bytes.
- `GET /population/?limit=1&yearFrom=2024&yearTo=2024&coo_all=true` → 200, 356 bytes.
- `GET /countries/?limit=1` → 200, 418. `/regions/?limit=1` → 200, 140. `/years/?limit=1` → 200, 80. `/footnotes/?limit=1` → 200, 257. `/population/?limit=1` → 200, 325. `/demographics/?limit=1` → 200, 431. `/asylum-applications/?limit=1` → 200, 286. `/asylum-decisions/?limit=1` → 200, 447. `/solutions/?limit=1` → 200, 370. `/idmc/?limit=1` → 200, 201. `/nowcasting/?limit=1` → 200, 275. `/unrwa/?limit=1` → 200, 199.
- `GET /years/?limit=200` → 200, 1,143 bytes.
- `GET https://api.unhcr.org/docs/refugee-statistics.html` → 200, 1,224,488 bytes.
