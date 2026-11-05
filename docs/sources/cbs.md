---
id: cbs
name: Statistics Netherlands (CBS)
rows: []
catalogue: ["Statistics Netherlands", "Statistics Netherlands - CBS"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# Statistics Netherlands

CBS StatLine over OData: every Dutch table with its periods, row counts and themes.

## Contents
5,979 catalogue entries, complete in `catalog/cbs.tsv` (identifier, language, title, frequency, period, records, columns, modified, status): 4,907 Dutch, 1,072 English. Status: `Regulier` 1,326 and `Regular` 405 are live; `Gediscontinueerd` 3,581 and
`Discontinued` 667 stopped. A table's English twin ends in `ENG` or `eng`. Frequency (Dutch and English labels are mixed): Stopgezet/Discontinued 4,140; Perjaar/Yearly 947; Eenmalig 244; Perkwartaal/Quarterly 165; Permaand/Monthly 162; Pertweejaar 57.

A table's structure (`DataProperties`), for example `86141ENG` CPI 2025=100, 2009 December to 2026 October: `Dimension` `ExpenditureCategories`, `TimeDimension` `Periods`, 7 `Topic` columns (`CPI_1`, `DerivedCPI_2`, `MonthlyRateOfChangeCPI_3`, `AnnualRateOfChangeCPI_5`, `WeightingCoefficient_7`...).
`Themes` and `Tables_Themes` give the tree.

English live macro tables: `86141ENG`-`86143ENG` CPI, `86144ENG`, `86145ENG` HICP; `80590eng` monthly labour participation and unemployment, 2003 on; `83693ENG`, `83694ENG` consumer confidence, 1986 on; `81234eng` producer confidence, 1985 on; `85773ENG`, `85791ENG`,
`85819ENG`, `85822ENG` house prices; `82242ENG`, `83085ENG` bankruptcies; `84765ENG`, `85427ENG`-`85429ENG` trade; `85817ENG` services prices.

`83131ENG` (CPI 2015=100) ended in 2025 December: the base changed.

## How to call it
- Catalogue `https://opendata.cbs.nl/ODataCatalog/Tables?$format=json&$select=Identifier,Title,Frequency,Period,RecordCount,ColumnCount,Language,Modified,ShortTitle,Catalog,OutputStatus`; it pages with `odata.nextLink` (2 calls). A `$select` with an unknown name (`Status`) is 400. `ODataCatalog/` lists Featured, Tables, Themes,
  Tables_Themes.
- A table `https://opendata.cbs.nl/ODataApi/OData/86141ENG` with `/TableInfos`, `/DataProperties`, `/UntypedDataSet`. In `TableInfos`, `RecordCount` is rejected: use it from the catalogue. A wrong id (`85436ENG`, `80072ENG`) is 404.

## What the app reads
Nothing directly. The Netherlands comes through Eurostat and the OECD.

## Not read yet
`86141ENG`, `80590eng`, `83693ENG`, `81234eng`, `85773ENG` for Countries › Netherlands.

## Calls made
Made on 2026-11-05, sequentially per host with a pause, `User-Agent: Deepwater/1.0`. 13 calls.

- `GET https://opendata.cbs.nl/ODataCatalog/?$format=json` → 200, 276 B
- `GET https://opendata.cbs.nl/ODataCatalog/Tables?$format=json&$select=Identifier,Title,Frequency,Period,RecordCount,ColumnCount,Language,Status,Modified,ShortTitle` → 400, 1.1 KB
- `GET https://opendata.cbs.nl/ODataCatalog/Tables?$format=json&$select=Identifier,Title,Frequency,Period,RecordCount,ColumnCount,Language,Modified,ShortTitle,Catalog,OutputStatus` → 200, 2.0 MB
- `GET https://opendata.cbs.nl/ODataCatalog/Themes?$format=json&$select=ID,Title,ParentID,Language` → 200, 95.2 KB
- `GET https://opendata.cbs.nl/ODataApi/OData/83131ENG/TableInfos?$format=json` → 200, 17.4 KB
- `GET https://opendata.cbs.nl/ODataApi/OData/83131ENG/DataProperties?$format=json&$select=Key,Title,Type` → 200, 1.3 KB
- `GET https://opendata.cbs.nl/ODataApi/OData/84046ENG/TableInfos?$format=json` → 200, 7.5 KB
- `GET https://opendata.cbs.nl/ODataApi/OData/84046ENG/DataProperties?$format=json&$select=Key,Title,Type` → 200, 1.0 KB
- `GET https://opendata.cbs.nl/ODataApi/OData/85436ENG/TableInfos?$format=json` → 404, 2.0 KB
- `GET https://opendata.cbs.nl/ODataApi/OData/80072ENG/TableInfos?$format=json` → 404, 2.0 KB
- `GET https://opendata.cbs.nl/ODataApi/OData/83131ENG` → 200, 869 B
- `GET https://opendata.cbs.nl/ODataApi/OData/86141ENG/DataProperties?$format=json&$select=Key,Title,Type` → 200, 1.3 KB
- `GET https://opendata.cbs.nl/ODataApi/OData/86141ENG/TableInfos?$format=json&$select=Identifier,Title,Frequency,Period,RecordCount,ColumnCount,Modified,Language` → 500, 117 B
