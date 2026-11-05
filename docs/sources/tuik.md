---
id: tuik
name: Turkish Statistical Institute (TurkStat, TÜİK)
rows: []
catalogue: ["Turkish Statistical Institute"]
access: free key
probed: 2026-11-05
confidence: docs     # the SDMX service refuses without a token; no key used. From the portal's own documentation
---

# Turkish Statistical Institute

The Data Portal `veriportali.tuik.gov.tr` (the old `data.tuik.gov.tr` redirects to it) and an SDMX web service behind it.

## Contents
Dataflows could not be listed. The documentation, read from the portal's application bundle (`main-C0Z3qMtl.js`, route `/en/sdmx-web-service-documentation`), shows:

- SDMX 2.1 NSI web service, agency `TR`. Dataflows `DF_*` (`DF_EMISYON_DEGISIM_ORAN` 1.1, key `A.TR.13.1_A_1.C_ROCGHG.3`), DSDs `DSD_*` (`DSD_SERA_GAZ_EMISYON_IST` 1.5), codelists `CL_*` (`CL_GOSTERGE` 7.2 indicator, `CL_SIKLIK` 1.0 frequency, `CL_IBBS` 1.1 regions, `CL_DEGISIM` 3.0, `CL_GIZLILIK` 1.0), concept schemes (`CROSS_DOMAIN_CONCEPTS_TUIK`, `CS_CEVRE`), `categoryscheme`, `categorisation`.
- Output: SDMX-ML 2.1, SDMX-CSV 1.0, JSON-stat.
- The portal's data browser has dashboards for Population and demography, Labour force, Environment, R&D, Education, Transportation, Agriculture indicators, plus press releases, statistical themes, bulk download, data comparison. `nip.tuik.gov.tr` is the population statistics portal; `cip.tuik.gov.tr` and `databrowser2.tuik.gov.tr` are linked.
- Turkish CPI, GDP and labour series also appear in EVDS with source TURKSTAT (158 data groups, see `cbrt.md`), which is easier.

## How to call it
- `https://nsiws.tuik.gov.tr/rest/dataflow/TR/all/latest?detail=full`, answered 401 here without a token. Per the documentation: sign in on the portal, verify a phone number by SMS, generate an API key (users with no Turkish number write to `info@tuik.gov.tr`), then `POST https://giris.tuik.gov.tr/realms/web/protocol/openid-connect/token` with `grant_type=password`, `client_id=nsi-ws-consumer`,
  `api_key=...`, and send `Authorization: Bearer <access_token>`.
- Routes named in the portal's bundle, `/api/en/dataflows`, `/api/tr/dataflows`, `POST /api/en/data/search`, are 404 from this client.
- robots.txt allows everything.

## What the app reads
Nothing.

## Not read yet
All of it, once a key exists. Prefer EVDS for headline series.

## Calls made
Made on 2026-11-05, sequentially per host with a pause, `User-Agent: Deepwater/1.0`. 16 calls.

- `GET https://data.tuik.gov.tr/robots.txt` → 200, 502 B
- `GET https://data.tuik.gov.tr/` → 200, 3.7 KB
- `GET https://veriportali.tuik.gov.tr/sitemap.xml` → 200, 1.9 KB
- `GET https://veriportali.tuik.gov.tr/` → 200, 3.7 KB
- `GET https://veriportali.tuik.gov.tr/en/sdmx-web-service-documentation` → 200, 3.7 KB
- `GET https://veriportali.tuik.gov.tr/assets/index-DHVaePSb.js` → 200, 31.0 KB
- `GET https://veriportali.tuik.gov.tr/assets/main-C0Z3qMtl.js` → 200, 1.1 MB
- `GET https://nsiws.tuik.gov.tr/rest/dataflow/TR/all/latest` → 401, 48 B
- `GET https://nsiws.tuik.gov.tr/rest/dataflow/TR/DF_EMISYON_DEGISIM_ORAN/1.1/` → 401, 48 B
- `GET https://nsiws.tuik.gov.tr/rest/dataflow/TR/all/latest?detail=full` → 401, 48 B
- `GET https://veriportali.tuik.gov.tr/assets/dataflows-CWs-fWWZ.js` → 200, 734 B
- `GET https://veriportali.tuik.gov.tr/api/en/categories` → 404, 17 B
- `GET https://veriportali.tuik.gov.tr/api/en/dataflows` → 404, 17 B
- `GET https://veriportali.tuik.gov.tr/api/tr/dataflows` → 404, 17 B
- `POST https://veriportali.tuik.gov.tr/api/en/data/search` → 404, 17 B
- `GET https://nip.tuik.gov.tr/` → 200, 16.5 KB
