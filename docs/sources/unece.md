---
id: unece
name: United Nations Economic Commission for Europe
rows: []
catalogue: ["United Nations Economic Commission for Europe (UNECE)"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# UNECE

The statistical database of UNECE, whose tables hold 56 countries and up to 64 with groups: economy, population and gender, forestry, transport. A PxWeb API.

## Contents
**242 tables** in 8 folders, each with variables, value counts and first and last period: [`catalog/unece.tsv`](catalog/unece.tsv).

| Folder | Tables |
|---|---|
| Population and Gender | 87 |
| Transport | 56 |
| Forestry | 30 |
| Economy | 29 |
| Millennium Development Goals | 19 |
| Sustainable Development Goals | 17 |
| Thematic Overviews | 3 |
| Country Profiles | 1 |

Periods run from 1960 to 2026-Q1 across the base. Economy sub-folders: External economic relations, Globalization indicators, Industrial production, Labour force & wages, National accounts, Overview tables, Price indices. Transport sub-folders: Inland waterway, Oil pipeline transport, Railway traffic and vehicles, Road safety, traffic and vehicle fleet, Transport infrastructure.

Monthly and quarterly tables of Economy:

| Table | Variables | First | Last |
|---|---|---|---|
| Exchange Rate by Country, Indicator and Month | Indicator(3) Country(56) Month(435) | 1990M01 | 2026M03 |
| Exchange Rate by Country, Indicator and Quarter | Indicator(3) Country(56) Quarter(145) | 1990Q1 | 2026Q1 |
| Industrial Production, Original and Seasonally Adjusted by Country and Month | Indicator(4) Country(56) Month(434) | 1990M01 | 2026M02 |
| Unemployment Rate by Country and Month | Measurement(2) Country(56) Month(434) | 1990M01 | 2026M02 |
| Employment by Country and Quarter | Measurement(6) Country(64) Quarter(104) | 2000Q1 | 2025Q4 |
| GDP: Expenditure Approach, in National Currency, by Country, Expenditure and Quarter | Expenditure(16) Measurement(12) Country(56) Quarter(104) | 2000Q1 | 2025Q4 |
| GDP: Output Approach, in National Currency, by Quarter (two tables) | see tsv | 2000Q1 | 2025Q4 |
| Price Indices by Country and Month | see tsv | 1990M01 | 2026M03 |
| Price Indices by Country and Quarter | see tsv | 1990Q1 | 2026Q1 |

Also `Country Overview by Country and Time` (Indicator 79, Country 56, Year 37, 1990 to 2026).

## How to call it
- API: `https://w3.unece.org/PXWeb2015/api/v1/en/STAT/` lists folders; each `id` extends the path; a `.px` table answers with `variables`, each with `code, values, valueTexts`.
- Example: `.../STAT/20-ME/<sub-folder id>/30_en_MECCExchPPPsNEWY_r.px` answers with the table's variables; a POST with a selection returns data (standard PxWeb, not called).
- The catalogue's `api` for this body says "UN Comtrade API…": wrong. Its `url`, `https://w3.unece.org/pxweb/en/`, is right.

## What the app reads
Nothing.

## Not read yet
**Economy › Countries:** the members outside Eurostat's and the OECD's coverage (the catalogue names Armenia, Azerbaijan and Belarus among them), through the monthly tables. **Map › supply chains:** the Transport folder, Oil pipeline transport.

## Calls made
All on 2026-11-05 with `User-Agent: Deepwater/1.0`.
- `GET https://w3.unece.org/PXWeb2015/api/v1/en/STAT/` → 200, 474 bytes.
- `tools/source-inventory/unece.py`: one GET per folder and per table under that base, a second apart → 242 tables, no error. Run twice, the second time to take the months and quarters.
- Earlier probe of `https://w3.unece.org/pxweb/en/`: `catalog/bodies_probe.jsonl.gz`.
