---
id: statssa
name: Statistics South Africa
rows: []
catalogue: ["Statistics South Africa"]
access: scraped web page
probed: 2026-11-05
confidence: refused
---

# Statistics South Africa

South Africa's national statistics office. Its site sits behind Incapsula, which answered this inventory with a script
challenge instead of a page, so nothing here describes its content first-hand.

## Contents
Nothing observed. Every page tried answered the same 212-byte script challenge (`/_Incapsula_Resource`, marked
`noindex,nofollow`), or a similar 1 KB one. The catalogue's description ("web tables (XLSX)") could not be verified, and
the body probe of 2026-11-05 also saw only a JavaScript page (`catalog/bodies_probe.jsonl.gz`).

From general knowledge, not from a call: it publishes statistical releases by P-number (P0141 CPI, P0142.1 PPI, P0211
QLFS, P0441 GDP, P3041.2 manufacturing, P6242.1 retail) with Excel time-series files on `www.statssa.gov.za`. Treat this
as a lead only. Part of the same figures comes through the SARB (`docs/sources/sarb.md`), the IMF and the OECD.

## How to call it
No programmatic route was found. Solving the challenge would mean presenting as a browser, which this inventory doesn't
do. A user with a browser can reach the site.

## What the app reads
Nothing.

## Not read yet
Unknown, for the reason above. South African CPI and PPI reach the app through the SARB (`nat_south_africa`).

## Calls made
- `GET https://www.statssa.gov.za/` → 200, 212 B, Incapsula script only
- `GET https://www.statssa.gov.za/?page_id=1854` → 200, 212 B, the same script
- `GET https://www.statssa.gov.za/?page_id=1847` → 200, 1,157 B, another challenge page
- `GET https://www.statssa.gov.za/timeseriesdata/Excel/P0141%20-%20CPI(COICOP)%20from%20January%202008%20(202509).zip`
  → 404 (a guessed file name)
- `GET https://nesstar.statssa.gov.za/` → TLS error, unrecognized name
