---
id: jodi
name: Joint Organisations Data Initiative
rows: [jodi]
catalogue: []                         # no entry in data_sources.json holds JODI
access: free, no key                  # the CSV files; the REST API needs a free account
probed: 2026-11-05
confidence: called
---

# Joint Organisations Data Initiative

Monthly oil and natural gas statistics reported by countries, gathered by the International Energy Forum: production,
trade, refinery activity, demand and stocks, by product. Everything the app needs is three flat CSV files; there is one
long format, one row per country, month, product, flow and unit.

## Contents

Three files, same seven columns: `REF_AREA` (ISO alpha-2), `TIME_PERIOD` (`yyyy-mm`), `ENERGY_PRODUCT`, `FLOW_BREAKDOWN`,
`UNIT_MEASURE`, `OBS_VALUE` (a number, or one of the tokens below), `ASSESSMENT_CODE`.

| File | Rows | Rows with a number | Areas | Months | Product × flow × unit |
|---|---|---|---|---|---|
| JODI-Oil primary, `world_Primary_CSV.zip` (23.6 MB, 287 MB inside) | 6,985,600 | 2,612,048 | 118 in the file, 115 with a number | 2002-01 to 2026-08 (296) | 200 |
| JODI-Oil secondary, `world_Secondary_CSV.zip` (58.9 MB, 655 MB inside) | 15,717,600 | 8,481,403 | 118 | 2002-01 to 2026-08 (296) | 450 |
| JODI-Gas, `GAS_world_NewFormat.zip` (1.7 MB, 13 MB inside; the site calls it beta) | 317,336 | 317,336 | 94 | 2009-01 to 2026-09 (213) | 30 |

`catalog/jodi.tsv`: for each file and area, the first and last month with a number and how many product-flow-unit items
it has.

**Oil primary.** Products: `CRUDEOIL` crude oil, `NGL`, `OTHERCRUDE` other, `TOTCRUDE` total. Flows: `INDPROD`
production, `OSOURCES` from other sources, `TOTIMPSB` imports, `TOTEXPSB` exports, `TRANSBAK` products transferred or
backflows, `DIRECUSE` direct use, `STOCKCH` stock change, `STATDIFF` statistical difference, `REFINOBS` refinery intake,
`CLOSTLV` closing stocks.

**Oil secondary.** Products: `LPG`, `NAPHTHA`, `GASOLINE` motor and aviation gasoline, `KEROSENE`, `JETKERO` of which
kerosene type jet fuel, `GASDIES` gas/diesel oil, `RESFUEL` fuel oil, `ONONSPEC` other oil products, `TOTPRODS` total
oil products. Flows: `REFGROUT` refinery output, `RECEIPTS`, `TOTIMPSB`, `TOTEXPSB`, `PTRANSF` products transferred,
`IPTRANSF` interproduct transfers, `STOCKCH`, `STATDIFF`, `TOTDEMO` demand, `CLOSTLV`.

Oil units: `KBD` thousand barrels a day, `KBBL` thousand barrels, `KL` thousand kilolitres, `KTONS` thousand metric
tons, `CONVBBL` conversion factor barrels per kiloton (labels from the JODI item-names guide, 2017).

**Gas.** One product, `NATGAS`, in three units: `M3` million m³, `TJ` terajoules, `KTONS` thousand tons (LNG). Flows:
`INDPROD` production, `OSOURCES` receipts from other sources, `TOTIMPSB` imports, `IMPLNG`, `IMPPIP` imports by LNG and
pipeline, `TOTEXPSB` exports, `EXPLNG`, `EXPPIP`, `STOCKCH`, `TOTDEMC` gross inland deliveries calculated, `STATDIFF`,
`TOTDEMO` gross inland deliveries observed, `MAINTOT` of which electricity and heat generation, `CLOSTLV` closing stocks.
The guide (2025) and the API manual also list a `CONVER` flow and an `M3_T` unit; the file holds neither.

`ASSESSMENT_CODE`: 1 reasonable comparability, 2 use with caution, 3 not assessed (all three appear in all three files;
the 2017 guide also lists 4, under verification, and the CSV does not hold it). In the oil files `OBS_VALUE` is also one
of `-`, `x`, `N/A`, `..` for a cell with no number: 63% of the primary rows and 46% of the secondary rows.

Other files, from the download pages: yearly copies of each oil file (`annual-csv/primary/2002.csv` … `2025.csv`,
`primaryyear2026.csv`, same for `secondary/`; the 2026 primary is 7.7 MB, same columns), Beyond 20/20 `.ivt` versions
(`world_ext.zip`; gas `ivt.zip`, `ivt-merged.zip`), monthly availability by country (`jodi_oil_data_availability_by_country.xlsx`,
one sheet A1:DN297; `jodi-data-availability-by-country.csv` for gas: 95 country columns, 213 months, `YES`/`NO`), country
notes (`jodi-oil-country-note.xlsx`, not opened).

Update rhythm, from the site's overview pages: around the 20th of each month. The zip files on the probe date were dated
2026-10-21 (oil) and 2026-10-19 (gas). The site says "more than 90" oil countries and "around 80" gas countries, under the
counts in the files.

## How to call it

- Oil, whole files: `GET https://www.jodidata.org/_resources/files/downloads/oil-data/world_Primary_CSV.zip` and
  `…/world_Secondary_CSV.zip`. **These two are no longer linked from the download page**, which offers only the yearly
  CSVs, the `.ivt` and the availability sheet; they still answer and are still refreshed.
- Oil, one year: `GET https://www.jodidata.org/_resources/files/downloads/oil-data/annual-csv/primary/2025.csv`
  (`primaryyear2026.csv` for the current year).
- Gas: the page is filled by script from `GET https://api.publisher.jodidata.org/web/files/gas` →
  `{"publicationId":28,"files":[…]}`, then `GET https://www.jodidata.org/jodi-publisher/gas/28/GAS_world_NewFormat.zip`.
  The publication id changes with each release: read it first. The same call for `oil` answers an empty list.
- REST API (documentation: `jodi-rest-api-v1.2.pdf`, not tried beyond the refusal below, no account): `POST
  https://www.jodidata.org/api/token` with a username and password gives a one-hour JWT; `GET /api/oildata`, `/api/gasdata`
  with `dateFrom`, `dateTo` (`yyyy-MM`), `countries`, `flows`, `products`, `units`, `pageSize` (100 to 100,000),
  `pageNumber`; `GET /api/oilmodified`, `/api/gasmodified` with `date` for what changed since (up to a year back). Rows:
  `country, product, flow, unit, date, quantity, quality`. 100 requests a minute per user. Its country list has 128 codes
  in the manual's tables as parsed here, including territories with no data in the files.
- `robots.txt` forbids only `/webresource.axd`, `/_resources/controls`, `/templates`, `/xsl`, `/App_Code`, `/App_Data`,
  `/aspnet_client`: the download folders are allowed. No key, no rate limit met.

## What the app reads

- `jodi`: Map › supply chains, month-end crude stocks by country. `world_Primary_CSV.zip` once a week
  (`src/SupplyChain.cpp:436`), keeps only rows with `,CRUDEOIL,CLOSTLV,KBBL,` (`:445`) from 2015 on (`:448`) with a
  number in `OBS_VALUE`: `REF_AREA` turned into ISO3, month, value. `src/SourcesPage.cpp:363`, shown in
  `src/DetailPanelSupply.cpp:498`.

One item out of 650 in the two oil files, and the app downloads 23 MB for it.

## Not read yet

- Crude production and exports by country (`CRUDEOIL` × `INDPROD`, `TOTEXPSB`) and refinery intake (`REFINOBS`), monthly
  in the same file the app already downloads: the Map's supply chain layer and country sheets.
- Oil demand by product (`TOTDEMO` × `GASOLINE`, `GASDIES`, `JETKERO`, `TOTPRODS`) and product stocks (`CLOSTLV`) in the
  secondary file: Economy › Countries energy rows, Desk.
- Natural gas: production, LNG and pipeline trade, stocks, power generation use, in the small gas file: the Map's
  supply chain layer.
- A lighter call for the app's own row: the yearly primary CSVs (uncompressed, 7.7 MB this year) against the 23 MB zip,
  and `oilmodified` if an account were ever acceptable.

## Calls made

All 2026-11-05, User-Agent `Deepwater/1.0`.

- `GET https://www.jodidata.org/robots.txt` → 200
- `HEAD https://www.jodidata.org/_resources/files/downloads/oil-data/world_Primary_CSV.zip` → 200, 23,625,568 B
- `GET https://www.jodidata.org/_resources/files/downloads/oil-data/world_Primary_CSV.zip` → 200, 23,625,568 B
- `GET https://www.jodidata.org/_resources/files/downloads/oil-data/world_Secondary_CSV.zip` → 200, 58,943,908 B
- `GET https://www.jodidata.org/_resources/files/downloads/oil-data/annual-csv/primary/primaryyear2026.csv` → 200, 7,699,910 B
- `GET https://www.jodidata.org/_resources/files/downloads/oil-data/jodi_oil_data_availability_by_country.xlsx?iid=183` → 200, 117,142 B
- `GET https://www.jodidata.org/_resources/files/downloads/oil-data/jodi-oil-wdb-item-names-ver2017.pdf` → 200, 606,233 B
- `GET https://www.jodidata.org/_resources/files/downloads/gas-data/jodi-gas-wdb-short--long-names-ver2025.pdf` → 200, 950,986 B
- `GET https://www.jodidata.org/_resources/files/downloads/jodi-rest-api-v1.2.pdf` → 200, 399,083 B
- `GET https://www.jodidata.org/oil/database/data-downloads.aspx` → 200, 32,660 B
- `GET https://www.jodidata.org/gas/database/data-downloads.aspx` → 200, 26,330 B
- `GET https://www.jodidata.org/oil/`, `/gas/` → 200
- `GET https://www.jodidata.org/oil/database/overview.aspx` → 200, 26,404 B; `/gas/database/overview.aspx` → 200, 27,416 B
- `GET https://www.jodidata.org/assets/index-BTM4UJLo.js`, `FileList-8t2jo0qY.js`, `publication-D4VfpGlK.js` → 200 (how the gas page finds its files)
- `GET https://api.publisher.jodidata.org/web/files/gas` → 200, 311 B
- `GET https://api.publisher.jodidata.org/web/files/oil` → 200, 30 B (empty list)
- `GET https://www.jodidata.org/jodi-publisher/gas/28/GAS_world_NewFormat.zip` → 200, 1,706,814 B
- `GET https://www.jodidata.org/jodi-publisher/gas/28/jodi-data-availability-by-country.csv` → 200, 78,580 B
- `GET https://www.jodidata.org/api/oildata?pageSize=100` → 401, no token (no account)
- Scan of the three zips: `tools/source-inventory/jodi.py`.
