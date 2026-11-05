---
id: fao
name: Food and Agriculture Organization of the United Nations (FAOSTAT)
rows: []
catalogue: ["Food and Agriculture Organization of the United Nations"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# FAO

FAOSTAT: crops, livestock, food balances, agricultural trade and prices, land, emissions and a few macro tables, plus the monthly Food Price Index.

## Contents
**69 data sets** in FAOSTAT's bulk catalogue, 174 million file rows in all. All, with update date, rows, size, topic and bulk file: [`catalog/fao.tsv`](catalog/fao.tsv).

By group: Land, Inputs and Sustainability 12; Climate Change 10; Discontinued archives 7; Food Balances 5; Prices 4; Investment 4; Food and Diet 4; Trade 4; Population and Employment 3; Production 3; Agricultural Science 2; Macro-Economic Indicators 2; Forestry 2; one each for Healthy Diet Cost, Food Security, Food Value Chain, Rural Livelihoods, SDG, Gender, World Census of Agriculture.

For a macro and markets terminal:

| Code | Name | Rows | Updated |
|---|---|---|---|
| `QCL` | Production: Crops and livestock products | 4,209,110 | 2025-12-31 |
| `TM` | Trade: Detailed trade matrix | 52,410,630 | 2025-12-23 |
| `TCL` | Trade: Crops and livestock products | 17,143,873 | 2026-07-24 |
| `PP` | Prices: Producer Prices | 1,319,583 | 2026-10-09 |
| `CP` | Prices: Consumer Price Indices (food and all items, monthly) | 266,292 | 2026-10-20 |
| `PE` | Prices: Exchange rates | 151,212 | 2026-09-16 |
| `PD` | Prices: Deflators | 93,659 | 2026-11-03 |
| `MK` | Macro Indicators (GDP, GFCF, value added of agriculture) | 456,877 | 2026-11-03 |
| `CS` | Capital Stock | 144,944 | 2025-07-10 |
| `FBS` | Food Balances (2010-) | 5,399,213 | 2025-10-28 |
| `RFB` | Fertilizers by Product | 312,305 | 2026-07-30 |
| `FS` | Suite of Food Security Indicators | 287,735 | 2026-07-21 |
| `SDGB` | SDG Indicators | 478,304 | 2026-09-29 |

Bulk files, normalised, as read on `PE`: columns `Area Code, Area Code (M49), Area, Element Code, Element, ISO Currency Code, Currency, Year Code, Year, Months Code, Months, Unit, Value, Flag`, with companion files of area codes, currencies, elements and flags. `PE` covers 213 countries and territories, 1970 to 2025, by its topic text. Only this domain's file was opened.

**FAO Food Price Index** (`ffpi-data-2026-10.xlsx`), a separate file: nominal and real indices, 2014-2016 = 100, from 1990, monthly and annual. Sheets `Indices_Monthly`, `Annual`, `Indices_Monthly_Real`, `Annual_Real`; columns Date, Food Price Index, Meat, Dairy, Cereals, Oils, Sugar.

## How to call it
- Catalogue: `https://bulks-faostat.fao.org/production/datasets_E.json`, fields `DatasetCode, DatasetName, Topic, DatasetDescription, Contact, Email, DateUpdate, CompressionFormat, FileType, FileSize, FileRows, FileLocation`.
- Data: `FileLocation`, e.g. `https://bulks-faostat.fao.org/production/Exchange_rate_E_All_Data_(Normalized).zip`. Sizes from 12 KB to 411 MB (`TM`).
- Index: the link sits on `https://www.fao.org/worldfoodsituation/foodpricesindex/en`; its name carries the month.
- Refused or down: `faostatservices.fao.org/api/v1/en/definitions/domain` answers 401 "Missing Authorization Header", and `fenixservices.fao.org/faostat/api/v1/en/dimensions/QCL/` answers 521, so filtered queries were not tested.
- The catalogue's `api` for this body says "UN Comtrade API, free key (limited); UNdata, no key": wrong.

## What the app reads
Nothing.

## Not read yet
- **Desk › Markets, grains and softs; Overview:** the Food Price Index and its five groups, monthly.
- **Map › supply chains:** `QCL` (world production beside the USDA and Eurostat yields of `crop_yields`), `TM` (who exports to whom), `RFB`.
- **Economy › Countries:** `CP` (food CPI by country, monthly), `PP`.

## Calls made
All on 2026-11-05 with `User-Agent: Deepwater/1.0`.
- `GET https://bulks-faostat.fao.org/production/datasets_E.json` → 200, 107,398 bytes.
- `GET https://bulks-faostat.fao.org/production/Exchange_rate_E_All_Data_(Normalized).zip` → 200, 1,274,156 bytes; file names and header rows kept.
- `GET https://www.fao.org/worldfoodsituation/foodpricesindex/en` → 200, 64,216 bytes.
- `GET https://www.fao.org/media/docs/worldfoodsituationlibraries/wfs-library/ffpi-data-2026-10.xlsx?sfvrsn=63809b16_131` → 200, 126,461 bytes; sheets and header text kept.
- `GET https://www.fao.org/robots.txt` → 200, 1,056 bytes.
- `GET https://faostatservices.fao.org/api/v1/en/definitions/domain` → 401, 28 bytes.
- `GET https://fenixservices.fao.org/faostat/api/v1/en/dimensions/QCL/` → 521, 16 bytes.
