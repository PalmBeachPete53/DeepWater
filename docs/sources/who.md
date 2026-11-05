---
id: who
name: World Health Organization
rows: []
catalogue: ["World Health Organization"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# World Health Organization

The Global Health Observatory: health indicators by country and year, from life expectancy and health spending to tobacco, immunisation and disease burden.

## Contents
**3,091 indicators and 202 dimensions**: all in [`catalog/who.tsv`](catalog/who.tsv). `COUNTRY` has 234 values.

Biggest code families: `SA` 460, `GDO` 220, `NCD` 176, `RSUD` 166, `GOE` 92, `TB` 76, `TOBACCO` 70, `R` 61, `FOODBORNE` 53, `FINPROTECTION` 49, `PRISON` 48, `M` 47, `NTD` 43, `HRH` 40. Others of interest: `WHS` 105, `AIR` 39, `MDG` 30, `HIV` 29, `GHED` 19, `WHOSIS` 15. No code starts with `COVID`.

Fields of a row: `Id, IndicatorCode, SpatialDimType, SpatialDim, ParentLocationCode, ParentLocation, TimeDimType, TimeDim, Dim1Type, Dim1, Dim2Type, Dim2, Dim3Type, Dim3, DataSourceDimType, DataSourceDim, Value, NumericValue, Low, High, Comments, Date, TimeDimensionValue, TimeDimensionBegin, TimeDimensionEnd`.

Two families sampled:
- `GHED_CHEGDP_SHA2011` (current health spending, % of GDP): 4,852 rows, 206 areas, 2000 to 2023. The `GHED` family holds 19 health expenditure indicators.
- `WHOSIS_000001` (life expectancy at birth): 14,112 rows, 196 areas, 2000 to 2023, by `Dim1` sex (`SEX_BTSX, SEX_FMLE, SEX_MLE`).

## How to call it
- Base `https://ghoapi.azureedge.net/api`, OData, JSON, no key.
- Catalogue: `/Indicator`, `/Dimension`, `/DIMENSION/COUNTRY/DimensionValues`. `/$metadata` is 12 MB.
- Data: `/GHED_CHEGDP_SHA2011?$select=SpatialDim,TimeDim`, with `$top`, `$filter`.
- Trap: the catalogue's `url` for this body is `https://coronavirus.data.gov.uk`, a UK site that redirects to `ukhsa-dashboard.data.gov.uk`, and its `api` says `web`: wrong on both counts.

## What the app reads
No row of the Sources list. The World Bank's WDI health rows reach the app through `worldbank`, not this API.

## Not read yet
- **Economy › Countries, Health:** `GHED_CHEGDP_SHA2011` and the `GHED_*` family, `WHOSIS_000001`.
- **Economy › Indicators:** any of the 3,091 with one value per country and year, from `catalog/who.tsv`.

## Calls made
All on 2026-11-05 with `User-Agent: Deepwater/1.0`.
- `GET https://ghoapi.azureedge.net/api/Indicator` → 200, 413,958 bytes.
- `GET https://ghoapi.azureedge.net/api/Dimension` → 200, 12,134 bytes.
- `GET https://ghoapi.azureedge.net/api/DIMENSION/COUNTRY/DimensionValues` → 200, 31,019 bytes.
- `GET https://ghoapi.azureedge.net/api/WHOSIS_000001?$top=1` → 200, 688 bytes; field names and types kept.
- `GET https://ghoapi.azureedge.net/api/WHOSIS_000001?$select=SpatialDim,TimeDim,Dim1` → 200, 758,464 bytes; axes kept.
- `GET https://ghoapi.azureedge.net/api/GHED_CHEGDP_SHA2011?$top=1&$select=SpatialDimType,SpatialDim,TimeDimType,TimeDim,Dim1Type` → 200, 249 bytes.
- `GET https://ghoapi.azureedge.net/api/GHED_CHEGDP_SHA2011?$select=SpatialDim,TimeDim` → 200, 175,121 bytes; axes kept.
- `GET https://ghoapi.azureedge.net/api/$metadata` → 200, 12,169,274 bytes.
- `GET https://www.who.int/robots.txt` → 200, 18,983 bytes.
- `GET https://coronavirus.data.gov.uk` → 200, 1,944,694 bytes, redirected to `ukhsa-dashboard.data.gov.uk`.
- `GET https://xmart-api-public.who.int/DEX_CMS/AI_INFO?$top=1` → 200, 38 bytes, "not found in mart".
