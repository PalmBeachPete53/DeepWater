---
id: iadb
name: Inter-American Development Bank
rows: []
catalogue: ["Inter-American Development Bank"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# Inter-American Development Bank

A research data portal: one-off data sets of Latin America and the Caribbean behind IDB publications, survey micro-data and a few recurring databases. Not a time-series service.

## Contents
**224 packages**, all under the organisation "IDB Datasets": [`catalog/iadb.tsv`](catalog/iadb.tsv) (name, title, formats, resources, last modified, tags). Formats (packages holding one): CSV 147, PDF 89, XLSX 88, DTA 57, DOCX 20, XLS 14, DO 6, DOC 5. The file contents were not opened, so fields and coverage of a package are in its title only.

Recurring or useful for macro:
- `Latin Macro Watch Dataset - Macroeconomic and Financial Indicators for Latin America and the Caribbean`.
- `Latin America and the Caribbean Standardized Public Debt Database`, one package per vintage, June 2019 to December 2024.
- `Database of Equivalent Fiscal Pressure in Latin America and the Caribbean`, 1990-2015, 1990-2018, 1990-2021.
- `Structural Fiscal Balances Database for LAC`.
- `Public Debt around the World`, central government debt 1970-2005.
- `Tax Reforms in Latin America in an Era of Democracy`.
- `Pension Indicators for Latin America and the Caribbean`, 2019, 2020.
- `IDB Group Data Wall 2026: Latin America and the Caribbean in Numbers`, tables and figures.
- `IDB Climate Finance Database`, one package a year from 2016 to 2022 at least.

Many others are replication data of a book or a survey.

## How to call it
- CKAN API: `https://data.iadb.org/api/3/action/package_list`, `/package_show?id=<name>`, no key. Each package's `resources` carry the file links.
- Trap: a title or a tag may be a string or an object by language.
- `www.iadb.org/robots.txt` answers 403 to `Deepwater/1.0` although the home page passed the earlier probe; `data.iadb.org` answers. The catalogue's `api: web` misses this portal.

## What the app reads
Nothing.

## Not read yet
**Economy › Countries:** Latin Macro Watch, and the debt database, whose fields and coverage should be read first from their files.

## Calls made
All on 2026-11-05 with `User-Agent: Deepwater/1.0`.
- `GET https://data.iadb.org/api/3/action/package_list` → 200, 14,688 bytes.
- `tools/source-inventory/iadb.py`: `package_show` on each of the 224 packages → no error in the table. Its first run stopped at the first package whose title was an object; the second ran through.
- `GET https://www.iadb.org/robots.txt` → 403, 4,566 bytes.
