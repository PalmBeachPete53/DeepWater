---
id: sipri
name: Stockholm International Peace Research Institute
rows: []
catalogue: ["SIPRI"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# SIPRI

Military spending, arms transfers, the arms industry and peace operations. Data comes in Excel files and web query tools, without an API.

## Contents
Databases on `/databases`: Arms Industry, Arms Transfers, Military Expenditure, Multilateral Peace Operations, Arms embargoes, Financial value of the global arms trade, National Budget Archive, National reports on arms exports, Mapping ATT-Relevant Cooperation.

**Military Expenditure** (`SIPRI-Milex-data-1949-2025_v1.2.xlsx`), 1949 to 2025, annual, "updated annually", 10 sheets:

| Sheet | Size | What |
|---|---|---|
| `Regional totals` | A1:AR32 | by region, constant 2024 US$ bn |
| `Local currency financial years` | A1:CC201 | by country, Country, Currency, Notes, years |
| `Local currency calendar years` | A1:CB200 | same, calendar years |
| `Constant (2024) US$` | A1:CB199 | US$ m at constant 2024 prices |
| `Current US$` | A1:CA199 | US$ m |
| `Share of GDP` | A1:CA199 | % of GDP |
| `Per capita` | A1:AN199 | current US$ |
| `Share of Govt. spending` | A1:AP202 | % of general government expenditure |
| `Footnotes` | A1:C117 | notes |
| `Front page` | B2 | title |

Countries are grouped by region and subregion, about 190 rows a sheet. Markers explained at the head of each sheet: `...` no data, `xxx` a country-related marker.

**Arms Transfers**: all transfers of major conventional arms from 1950 to the last full year, through the query tool `https://armstransfers.sipri.org` (not called).

**Arms Industry**: the 100 largest companies, 2002 to 2024, two Excel files: `SIPRI-Top-100-2002-2024 (2).xlsx` and `Total-arms-revenues-SIPRI-Top-100-2002-2024.xlsx` (linked, not fetched).

## How to call it
- `https://www.sipri.org/sites/default/files/SIPRI-Milex-data-1949-2025_v1.2.xlsx`; the name carries the years and version, so take it from `/databases/milex`.
- `robots.txt` asks for `crawl-delay: 10`; the calls here waited 10 seconds. Terms at `/databases/terms`.

## What the app reads
Nothing directly. Its World Bank rows include military spending and arms exports attributed to SIPRI (`README.md:882`).

## Not read yet
**Economy › Countries, Government tab:** the `Share of GDP` and `Constant (2024) US$` sheets go back to 1949. **Research, defence companies:** the Top 100 file.

## Calls made
All on 2026-11-05 with `User-Agent: Deepwater/1.0`, 10 seconds apart.
- `GET https://www.sipri.org/robots.txt` → 200, 2,268 bytes.
- `GET https://www.sipri.org/databases` → 200, 57,336 bytes.
- `GET https://www.sipri.org/databases/milex` → 200, 59,108 bytes.
- `GET https://www.sipri.org/databases/armstransfers` → 200, 60,175 bytes.
- `GET https://www.sipri.org/databases/armsindustry` → 200, 58,237 bytes.
- `GET https://www.sipri.org/databases/terms` → 200, 61,373 bytes.
- `GET https://www.sipri.org/sites/default/files/SIPRI-Milex-data-1949-2025_v1.2.xlsx` → 200, 922,552 bytes; sheet names, sizes and header text kept.
