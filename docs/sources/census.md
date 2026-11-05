---
id: census
name: U.S. Census Bureau
rows: []
catalogue: ["U.S. Census Bureau"]
access: free key
probed: 2026-11-05
confidence: called                    # catalogue and variable lists answered; data calls refuse without a key
---

# U.S. Census Bureau

People, firms, housing starts, retail sales, trade. The Data API's catalogue is open; since at least this probe its data
rows want a key.

## Contents
`api.census.gov/data.json` lists 1,817 data sets: [`catalog/census.tsv`](catalog/census.tsv) (path, vintage, title, kind,
temporal, spatial, modified). By kind: 962 microdata, 756 aggregate, 89 time series, 10 other. Largest families by path:
`cps` 707, `acs` 276, `sipp` 179, `timeseries` 89, `pep` 80, `dec` 64, `cbp` 38, `nonemp` 28, `zbp` 25, `pdb` 23, `ecn` 12,
`popproj` 10.

The 89 time series, each with its variables (name, label, predicate type, required) and geography levels:
[`catalog/census_timeseries.jsonl`](catalog/census_timeseries.jsonl).
- **Economic indicators** `timeseries/eits/*` (21): `advm3`, `bfs` (business formation), `ftd`, `ftdadv`, `hv` (vacancies,
  homeownership), `m3` (factory orders), `marts` (advance retail), `mhs`, `mhs2`, `mrts`, `mrtsadv`, `mtis`, `mwts`,
  `mwtsadv`, `qfr`, `qpr`, `qss`, `qtax`, `resconst` (starts, permits), `ressales` (new home sales), `vip` (construction
  spending). All but `qtax` have the same 11 variables: `time`, `data_type_code`, `time_slot_id`, `seasonally_adj`,
  `program_code`, `category_code`, `geo_level_code`, `time_slot_date`, `time_slot_name`, `cell_value`, `error_data`.
  Geography `us` (`qtax`: `state`, `us`).
- **International trade** `timeseries/intltrade/*` (36): imports and exports by `hs`, `naics`, `sitc`, `enduse`, `hitech`,
  `usda`, `porths`, `statehs`, `statenaics`, and `BETA` twins (`hsimport`, `hsexport`, ...). `exports/hs` has 41 variables,
  `imports/hs` 70: quantities, values, weights by air, vessel, containerised, by commodity level, country and district.
- **Others**: `asm/*` (manufactures), `qwi/*` (workforce), `bds`, `poverty/*`, `healthins/sahie`, `idb/1year`, `idb/5year`
  (International Database, every country's population by age and sex), `pseo/*`, `hps`, `hhpulse`, `soma`, `aies`, `govs*`.

## How to call it
- Catalogue: `https://api.census.gov/data.json` (5.3 MB, no key).
- One data set: `https://api.census.gov/data/timeseries/eits/marts/variables.json`, `.../geography.json`.
- Data, documented form: `https://api.census.gov/data/timeseries/eits/resconst?get=cell_value&for=us:*&time=2025-01&category_code=APERMITS&data_type_code=TOTAL&seasonally_adj=yes&key=<key>`.
- **Trap:** without `key=`, data calls answer HTTP 200 with an HTML page titled "Missing Key", not JSON. Tried on
  `dec/pl`, `eits/marts`, `eits/resconst`. Signup: `api.census.gov/data/key_signup.html`. A client must check the body.
- `variables/category_code.json` gives the variable's description, not its code list: the codes need a key.

## What the app reads
Nothing directly. Census's indicators reach it through FRED (`HOUST`, `PERMIT`, `HSN1F`, `RRSFS`, ...).

## Not read yet
- `eits/marts`, `resconst`, `ressales`, `m3`, `vip`, `ftdadv`: the same releases at the source, with all their categories.
  Economy, Countries › United States.
- `intltrade/*`: US imports and exports by HS code and partner, monthly. It would give the trade flows on the map.
- `eits/bfs`, `hv`: business formation, homeownership.
- `idb/*`: population by age for every country, for Countries.

## Calls made
- `GET https://api.census.gov/data.json` → 200, 5,270,169 bytes
- `GET https://api.census.gov/data/timeseries/eits/marts/variables/category_code.json` → 200, 175 bytes ×2
- `GET https://api.census.gov/data/timeseries/eits/marts/variables/data_type_code.json` → 200, 172 bytes
- `GET https://api.census.gov/data/timeseries/eits/resconst/variables/category_code.json` → 200, 175 bytes
- `GET https://api.census.gov/data/timeseries/eits/marts?get=cell_value,time_slot_id&for=us:*&time=from+1992&category_code=44X72&data_type_code=SM&seasonally_adj=yes&time_slot_id=0` → 200, 8,764 bytes
- `GET https://api.census.gov/data/timeseries/eits/marts?get=cell_value,time_slot_id&for=us:*&time=2025&category_code=44X72&data_type_code=SM&seasonally_adj=yes` → 200, 8,764 bytes
- `GET https://api.census.gov/data/2020/dec/pl?get=NAME&for=state:01` → 200, 8,764 bytes, HTML "Missing Key"
- `GET https://api.census.gov/data/timeseries/eits/resconst?get=cell_value&for=us:*&time=2025-01&category_code=APERMITS&data_type_code=TOTAL&seasonally_adj=yes` → 200, 8,764 bytes
- `GET https://api.census.gov/data/key_signup.html` → 200, 10,287 bytes
- `GET https://api.census.gov/data/<each of the 89 time series>/variables.json` → 200 ×89
- `GET https://api.census.gov/data/<each of the 89 time series>/geography.json` → 200 ×89
