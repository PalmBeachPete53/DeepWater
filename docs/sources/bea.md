---
id: bea
name: U.S. Bureau of Economic Analysis
rows: [bea]
catalogue: ["U.S. Bureau of Economic Analysis", "U.S. Bureau of Economic Analysis (BEA)", "Bureau of Economic Analysis (BEA)", "U.S. Bureau of Economics Analysis"]
access: free key
probed: 2026-11-05
confidence: docs                      # no key in this inventory; the API's own metadata calls could not be made. Contents from BEA's API User Guide (April 2026)
---

# U.S. Bureau of Economic Analysis

US national accounts, GDP by industry and region, trade in services, international position.

## Contents
From BEA's user guide, not from a call. `GetDataSetList` returns 13 data sets:
| Data set | Description (BEA's) | Parameters named in the guide |
|---|---|---|
| `NIPA` | Standard NIPA tables | `TableName`, `Frequency` (A, Q, M), `Year` |
| `NIUnderlyingDetail` | Standard NI underlying detail tables | `TableName`, `Frequency`, `Year` |
| `FixedAssets` | Standard Fixed Assets tables | `TableName`, `Year` |
| `MNE` | Multinational Enterprises | `DirectionOfInvestment`, `OwnershipLevel`, `NonbankAffiliatesOnly`, `Classification`, `Country`, `Industry`, `Year`, `State`, `SeriesID`, `GetFootnotes` |
| `GDPbyIndustry` | GDP by Industry | `TableID`, `Frequency`, `Year`, `Industry` |
| `UnderlyingGDPbyIndustry` | Underlying GDP by Industry | `TableID`, `Frequency`, `Year`, `Industry` |
| `ITA` | International Transactions Accounts | `Indicator`, `AreaOrCountry`, `Frequency`, `Year` |
| `IIP` | International Investment Position | `TypeOfInvestment`, `Component`, `Frequency`, `Year` |
| `InputOutput` | Input-Output Data | `TableID`, `Year` |
| `IntlServTrade` | International Services Trade | `TypeOfService`, `TradeDirection`, `Affiliation`, `AreaOrCountry`, `Year` |
| `IntlServSTA` | International Services Supplied Through Affiliates | `Channel`, `Destination`, `Industry`, `AreaOrCountry`, `Year` |
| `Regional` | Regional data sets (income, product, employment by state and local area) | `TableName`, `LineCode`, `GeoFips`, `Year` |
| `APIDatasetMetaData` | Metadata about other API data sets | |

Metadata methods: `GetDataSetList`, `GetParameterList`, `GetParameterValues`, `GetParameterValuesFiltered`. Values of `TableName` and the
others can only be listed with a key. `RegionalData`, `RegionalIncome`, `RegionalProduct` are deprecated in favour of `Regional`.

Without a key, one number from a file: `apps.bea.gov/national/Release/TXT/TablesRegister.txt` has 321 NIPA tables (fields `TableId,
TableTitle`) and `SeriesRegister.txt` about 10,750 series (fields `%SeriesCode, SeriesLabel, MetricName, CalculationType, DefaultScale,
TableId:LineNo, SeriesCodeParents`). `apps.bea.gov/robots.txt` allows only a short list of paths, and `/national/` is not on it: these
two files were fetched once, before the robots file was read, and not again. Their content is not reproduced here; a crawler must leave them
alone, and the way in is the API with a key.

## How to call it
- Form: `https://apps.bea.gov/api/data/?UserID=<key>&method=GetData&datasetname=NIPA&TableName=T10101&Frequency=Q&Year=2025,2026&ResultFormat=JSON`.
- Limits, guide: 100 requests, 100 MB or 30 errors per minute per user, then errors for the timeout period (currently one minute).
  Single-threaded clients advised.
- Without a key, `GetDataSetList` answers HTTP 200 with an empty body.
- Answer: `BEAAPI.Results.Data[]`; the app reads `LineNumber`, among others.

## What the app reads
`bea` (Map): `NIPA` table `T10101`, quarterly, last two years, line 1 (percent change in real GDP). `src/KeyedSources.cpp:163`, `:172`.
Other BEA series reach the app through FRED (`A191RL1Q225SBEA`, `DPCERL1Q225SBEA`, `PCEPI`, ...).

## Not read yet
All by key, codes to confirm with `GetParameterValues`:
- `NIPA`: GDP components and contributions (`T10102`), personal income and PCE, at the source.
- `Regional`: GDP and income by US state.
- `ITA`, `IIP`: current account and net international position, with partner countries.
- `GDPbyIndustry`: value added by sector.

## Calls made
- `GET https://apps.bea.gov/api/data/?method=GetDataSetList&ResultFormat=JSON` → 200
- `GET https://apps.bea.gov/API/signup/` → 200, 43,554 bytes
- `GET https://apps.bea.gov/api/_pdf/bea_web_service_api_user_guide.pdf` → 200, 1,376,062 bytes
- `GET https://apps.bea.gov/national/Release/TXT/TablesRegister.txt` → 200, 30,329 bytes
- `GET https://apps.bea.gov/national/Release/TXT/SeriesRegister.txt` → 200, 968,442 bytes
- `GET https://www.bea.gov/robots.txt` → 200, 2,027 bytes
- `GET https://apps.bea.gov/robots.txt` → 200, 335 bytes
- `GET https://www.bea.gov/data` → 200, 35,734 bytes
- `GET https://apps.bea.gov/iTable/` → 200, 30,646 bytes
- `GET https://www.bea.gov/open-data` → 200, 40,117 bytes
