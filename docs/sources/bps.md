---
id: bps
name: Statistics Indonesia (Badan Pusat Statistik)
rows: [research_bps]
catalogue: ["Statistics Indonesia"]
access: free key
probed: 2026-11-05
confidence: docs
---

# Statistics Indonesia (BPS)

Indonesia's national statistics through a Web API: dynamic and static tables, publications, press releases, trade by HS code,
SDGs. The key is missing, so the content comes from its OpenAPI file and guides.

## Contents
From `documentation/openapi.yaml` (OpenAPI 3.1, "BPS Statistical Data API", 186 KB) and the portal's home:
- **549 domains**: 1 headquarters (`0000`), 34 provinces, 514 regencies. Every list takes a `domain`.
- Dynamic tables: `model/subject`, `subcat`, `var`, `vervar` (vertical variable), `turvar` (derived variable), `th` (year),
  `turth` (derived period), `unit`, `data`. A cell key in `datacontent` joins var, vervar, year and turvar.
- Static tables: `model/statictable`, `view/.../statictable/lang/{lang}/id/{id}`.
- Classified: `model/subcatcsa`, `subjectcsa`, `tablestatistic`.
- `model/pressrelease`, `publication`, `indicators` (strategic indicators), `infographic`, `news`, `newscategory`, `glosarium`.
- **Foreign trade**: `dataexim/sumber/{sumber}/periode/{periode}/kodehs/{kodehs}/jenishs/{jenishs}/tahun/{tahun}`, with `year-exim-data`,
  `port`, `country`, `hs-code` lists.
- `model/sdgs`, `sdds`; `/v1/sdgs/goals`, `indicators`, `datasets`.
- `interoperabilitas/datasource/sensus/id/37..40` (censuses), `simdasi/id/22..36` (regional tables).
- `/v1/searching/content/domain/{domain}/model/{model}`.

Variable counts and years aren't in the documentation. They're a `model/var` call away with a key.

Pagination fields: `page`, `pages`, `per_page`, `count`, `total`; `nopage=1` on derived variables, `perpage` on some.

## How to call it
- `GET https://webapi.bps.go.id/v1/api/list/model/data/lang/eng/domain/0000/var/{var}/th/{a}:{b}/key/{key}/`. The key is the last path
  segment; `th` counts years from 1900.
- Answer: `status`, `data-availability`, `data` (list calls) or `var`, `turvar`, `vervar`, `tahun`, `turtahun`, `datacontent`.
- Traps: `/v1/api/list/model/var/...` without key answers 400 "Missing required parameter: key". Each user may hold two or three keys
  (guide). `src/SourcesPage.cpp:585` notes three years per request.

## What the app reads
`research_bps`, row at `src/SourcesPage.cpp:584`; Countries › Indonesia, `src/CountryData.cpp:1485`, reads `datacontent` and `tahun`, for
domain `0000`. Provider `bps` in `resources/country_indicators.json`: variable `543` (unemployment), `1` (monthly inflation), `196`
(exports), `497` (imports), `1150` with key `36/0` (foreign tourist visits).

## Not read yet
With the key:
- `dataexim` by HS code and country: Supply chains, Commodities (Indonesian palm oil, coal, nickel).
- `model/indicators`, `sdds`: a list of headline indicators with their variable codes.
- Provinces (34 domains), for a regional view of the Map.

## Calls made
- `GET https://webapi.bps.go.id/` → 200, redirect to `/developer/`, 82 KB
- `GET https://webapi.bps.go.id/documentation/` → 200, 11 KB
- `GET https://webapi.bps.go.id/documentation/assets/guides.js` → 200, 60 KB
- `GET https://webapi.bps.go.id/documentation/openapi.yaml` → 200, 186 KB
- `GET https://webapi.bps.go.id/v1/api/list/model/domain/lang/eng/key/` → 400, "'domain' parameter is required"
- `GET https://webapi.bps.go.id/v1/api/list/model/var/lang/eng/domain/0000/key/` → 400, "Missing required parameter: key"
