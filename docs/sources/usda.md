---
id: usda
name: U.S. Department of Agriculture
rows: [crop_yields]
catalogue: ["US Department of Agriculture"]
access: free, no key                  # PSD downloads; the FAS and NASS APIs want a free key
probed: 2026-11-05
confidence: called
---

# U.S. Department of Agriculture

World supply and demand of crops and livestock (FAS PSD), trade and export sales (FAS), US farm statistics (NASS, ERS).
USDA is many sites; `www.usda.gov` and `www.fas.usda.gov` refuse this client.

## Contents
**PSD, Production, Supply and Distribution** (`apps.fas.usda.gov/psdonline`). `psd_alldata_csv.zip` holds one CSV of
2,095,339 rows, columns `Commodity_Code, Commodity_Description, Country_Code, Country_Name, Market_Year, Calendar_Year,
Month, Attribute_ID, Attribute_Description, Unit_ID, Unit_Description, Value`. 63 commodities
([`catalog/usda_psd_commodities.tsv`](catalog/usda_psd_commodities.tsv), with countries, attributes, first and last year),
69 attributes ([`catalog/usda_psd_attributes.tsv`](catalog/usda_psd_attributes.tsv)), 214 countries
([`catalog/usda_psd_countries.tsv`](catalog/usda_psd_countries.tsv)), 11 units (`1000 MT`, `1000 HA`, `MT/HA`, `1000 HEAD`,
`1000 60 KG BAGS`, `1000 480 lb. Bales`, ...). Market years 1960 to 2027: the last ones are USDA's forecasts.
Commodities: grains (wheat, corn, rice, barley, sorghum, oats, rye, millet), oilseeds, meals and oils (soy, palm,
rapeseed, sunflower, ...), meats, dairy, coffee, cotton, sugar, fruits, nuts. Attributes: `Production`, `Area Harvested`,
`Yield`, `Beginning Stocks`, `Ending Stocks`, `Exports`, `Imports`, `Domestic Consumption`, `Crush`, `Stocks-to-Use`, ...

Files by group, checked with HEAD: `psd_grains_pulses`, `psd_oilseeds`, `psd_coffee`, `psd_cotton`, `psd_dairy`, `psd_juice`,
`psd_livestock`, `psd_sugar` answer 200; `psd_fruits`, `psd_nuts` answer 404 under that name (they are in `psd_alldata`).

**FAS Open Data** (`api.fas.usda.gov`, swagger read, 35 paths, header `X-Api-Key`):
- ESR, export sales: `/api/esr/regions`, `countries`, `commodities`, `unitsOfMeasure`, `datareleasedates`,
  `/api/esr/exports/commodityCode/{c}/allCountries/marketYear/{y}`.
- GATS, trade from Census and the UN: `/api/gats/censusImports|censusExports|censusReExports/partnerCode/{p}/year/{y}/month/{m}`,
  `customsDistrictExports|Imports|ReExports`, `UNTradeImports|Exports|ReExports/reporterCode/{r}/year/{y}`, `commodities`,
  `HS6Commodities`, `customsDistricts`.
- PSD: `/api/psd/commodity/{c}/country/all/year/{y}`, `.../world/year/{y}`, `/api/psd/commodities`, `countries`, `regions`,
  `unitsOfMeasure`, `commodityAttributes`, `dataReleaseDates`.

**ERS** data products: 99 linked from `www.ers.usda.gov/data-products`, in
[`catalog/usda_ers_products.tsv`](catalog/usda_ers_products.tsv) (Farm Income and Wealth Statistics, Food Price Outlook,
Agricultural Baseline Database, Agricultural Exchange Rate Data Set, ...). Only titles.

**NASS Quick Stats** (`quickstats.nass.usda.gov/api`): `get_param_values` answers 401 without a key; contents not
reachable. **AMS Market News** (`marsapi.ams.usda.gov`): 403 without credentials; `mymarketnews.ams.usda.gov` did not
answer.

## How to call it
- `https://apps.fas.usda.gov/psdonline/downloads/psd_alldata_csv.zip` (10 MB, 202 MB unzipped). No key, no `robots.txt` (404).
- Swagger: `https://apps.fas.usda.gov/opendatawebV2/assets/swagger/swagger.json`.
- `apps.fas.usda.gov/OpenData/api/psd/commodities` without a key answers 403 `"Bad API Key"`.
- `Month` takes `00` to `12`; `Calendar_Year` runs 1960 to 2026, `Market_Year` to 2027. Read USDA's notes before using them.

## What the app reads
`crop_yields`: `psd_grains_pulses_csv.zip` and `psd_oilseeds_csv.zip` (`src/SupplyChain.cpp:141`), keeps attribute 184
(`Yield`) of wheat `0410000`, corn `0440000`, rice `0422110`, soybeans `2222000`, by country and market year
(`src/SupplyChain.cpp:107`, `:174`). Eurostat's part of that row is on the Eurostat card.

## Not read yet
- `Production`, `Ending Stocks`, `Stocks-to-Use`, `Exports` in the same files already downloaded: the app throws them away
  at `src/SupplyChain.cpp:174`. World stocks-to-use for Map › Supply chains at no new call.
- `psd_coffee`, `psd_sugar`, `psd_cotton`, `psd_livestock`: other crops for the same map.
- FAS ESR: weekly US export sales, a key market mover (free key).
- ERS `agricultural-baseline-database`: ten-year projections.

## Calls made
- `GET https://apps.fas.usda.gov/robots.txt` → 404
- `GET https://apps.fas.usda.gov/psdonline/app/index.html` → 200, 2,303 bytes
- `GET https://apps.fas.usda.gov/psdonline/app/index.html#/app/downloads` → 200, 2,303 bytes
- `GET https://apps.fas.usda.gov/psdonline/downloads/psd_alldata_csv.zip` → 200, 10,458,148 bytes
- `GET https://apps.fas.usda.gov/OpenData/swagger/ui/index` → 200, 6,615 bytes
- `GET https://apps.fas.usda.gov/OpenData/swagger/docs/v1` → 200, 40,895 bytes
- `GET https://apps.fas.usda.gov/OpenData/api/psd/commodities` → 403, 13 bytes
- `GET https://apps.fas.usda.gov/psdonline/api/psd/commodity` → 404
- `GET https://apps.fas.usda.gov/opendatawebV2/` → 200, 1,510 bytes
- `GET https://apps.fas.usda.gov/opendatawebV2/assets/swagger/swagger.json` → 200, 40,937 bytes
- `GET https://apps.fas.usda.gov/PSDOnlineApi/api/downloadableData/GetAllDownloadableFiles` → 404, 156 bytes
- `GET https://www.usda.gov/robots.txt` → 403, 380 bytes
- `GET https://www.usda.gov/` → 403, 366 bytes
- `GET https://www.fas.usda.gov/data` → 403, 376 bytes
- `GET https://quickstats.nass.usda.gov/api/get_param_values/?param=commodity_desc` → 401, 26 bytes
- `GET https://www.ers.usda.gov/data-products` → 200, 249,138 bytes
- `GET https://www.ams.usda.gov/market-news` → 200, 44,405 bytes
- `GET https://mymarketnews.ams.usda.gov/public_data` → ERR (TimeoutError)
- `GET https://marsapi.ams.usda.gov/services/v1.2/reports` → 403, 253 bytes
- `HEAD https://apps.fas.usda.gov/psdonline/downloads/psd_grains_pulses_csv.zip` → 200
- `HEAD https://apps.fas.usda.gov/psdonline/downloads/psd_oilseeds_csv.zip` → 200
- `HEAD https://apps.fas.usda.gov/psdonline/downloads/psd_coffee_csv.zip` → 200
- `HEAD https://apps.fas.usda.gov/psdonline/downloads/psd_cotton_csv.zip` → 200
- `HEAD https://apps.fas.usda.gov/psdonline/downloads/psd_dairy_csv.zip` → 200
- `HEAD https://apps.fas.usda.gov/psdonline/downloads/psd_fruits_csv.zip` → 404
- `HEAD https://apps.fas.usda.gov/psdonline/downloads/psd_juice_csv.zip` → 200
- `HEAD https://apps.fas.usda.gov/psdonline/downloads/psd_livestock_csv.zip` → 200
- `HEAD https://apps.fas.usda.gov/psdonline/downloads/psd_nuts_csv.zip` → 404
- `HEAD https://apps.fas.usda.gov/psdonline/downloads/psd_sugar_csv.zip` → 200
