---
id: afdb
name: African Development Bank Group
rows: []
catalogue: ["African Development Bank Group", "AFDB", "African Development Bank"]
access: scraped web page
probed: 2026-11-05
confidence: refused
---

# African Development Bank

Statistics of African countries through its Africa Information Highway portal. Both the bank's site and the portal answer a Cloudflare challenge page with a 403, so nothing here was verified at the source.

## Contents
From third-party documentation only:
- The Africa Information Highway is the bank's continental data platform, with economic, social and development statistics for all African countries. A directory lists CSV, Excel and API access with periodic updates and "Open (varies)" licences. Knoema hosts many of its data sets, among them ICT, electricity, CPI and the Africa Infrastructure Development Index. National "Open Data for Africa" platforms exist for about 20 countries.
- Data set count, API documentation and access rules were not found.
- The catalogue counts 14 + 5 + 2 Trading Economics pages for this body, for countries such as Benin, Burkina Faso, Botswana, Congo and Zimbabwe.

## How to call it
No call answered. `www.afdb.org` and `dataportal.opendataforafrica.org`, the latter also on its `/api/1.0/meta/dataset`, give 403 "Just a moment…" to `User-Agent: Deepwater/1.0`. The block was left alone, as asked.

## What the app reads
Nothing. The bank's CPIA is one of the columns of Transparency International's workbook (`transparency.md`).

## Not read yet
**Economy › Countries:** African countries' rows, if the bank opens programmatic access.

## Calls made
All on 2026-11-05 with `User-Agent: Deepwater/1.0`.
- `GET https://www.afdb.org/en/knowledge/statistics` → 403, 5,503 bytes.
- `GET https://dataportal.opendataforafrica.org/` → 403, 5,475 bytes.
- `GET https://dataportal.opendataforafrica.org/api/1.0/meta/dataset` → 403, 5,536 bytes.
- Web search "African Development Bank Africa Information Highway data portal API datasets; ITC Trade Map registration free data coverage Market Access Map": result titles and summaries only, among them `knoema.com/atlas/sources/AIH`, `atlas.co/data-portals/open-data-for-africa-af-db/`, `www.afdb.org/en/news-and-events/the-afdb-is-launching-africas-information-highway-11505`.
