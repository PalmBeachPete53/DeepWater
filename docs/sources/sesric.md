---
id: sesric
name: Statistical, Economic and Social Research and Training Centre for Islamic Countries (SESRIC)
rows: []
catalogue: ["SESRIC", "SESRIC - The Statistical, Economic & Social Res. & Training Centre for Islamic Countries"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# SESRIC

The statistics body of the Organisation of Islamic Cooperation. **OICStat** compiles indicators for its member countries, largely from other international bodies.

## Contents
**2,024 indicators**: [`catalog/sesric.tsv`](catalog/sesric.tsv) (code, English name, category, unit, private flag, source). 1,897 are public and 127 are marked `Private`.

Categories: Sustainable Development Goals 823, Demography 152, International Trade 117, Health 94, National Accounts 75, Agriculture 73, Labour and Social Protection 60, Education 58, Gender and Family 53, Industry and Manufacturing 51, Science, Technology and Innovation 49, Youth 44, Energy 42, Private Sector 38, Water 31, International Finance 30, Government Finance 26, Tobacco Control 26, Transportation and Communication 25, Environment 24, Disasters and Emergency Events 24, Islamic Banking and Finance 22, Banking, Money and Prices 21, Crime and Justice 20, Digital Transformation and AI 20, Tourism 13, Global Indices 12, Social Development 1.

Sources it compiles from: UNSD Global SDG Database 814, World Bank WDI 91, ILOSTAT 91, UN Population Division 140 with SESRIC's calculations, WHO GHO 62, UNESCO UIS 58, UNIDO INDSTAT 46, FAOSTAT 45, WIPO 40. So most of it is available at the source; **Islamic Banking and Finance** is the part to look at first.

Regions (`/region-mappings/`): 25, such as OIC, GCC, D8, OIC-MENA, OIC-SSA, Arab League, ECO, Turkic States, OIC-WAEMU. Fields of the indicator list: code, names and descriptions in English, French and Arabic, category, secondary categories, unit, `Is Private`, aggregation method and formula, type, source. Country and year coverage of the data was not read.

## How to call it
- The site `https://oicstat.sesric.org/` is a JavaScript app. Its code calls an API on port 8000 of the same host, without a key, undocumented publicly:
- `https://oicstat.sesric.org:8000/indicators/indicators-csv/download` (the whole list, CSV), `/indicators/categories/`, `/indicators/indicator-details/all`, `/region-mappings/`; the code also names `/indicators/sdg-goals/`, `/sources/all`, `/units/summary`, `countries/by-type/…`, `regions-countries/…`.
- `robots.txt` of `www.sesric.org` closes only admin folders. An undocumented API can change without notice: ask SESRIC before relying on it.

## What the app reads
Nothing.

## Not read yet
**Economy › Countries:** the Islamic finance indicators in `catalog/sesric.tsv`, not found in any other card of this batch.

## Calls made
All on 2026-11-05 with `User-Agent: Deepwater/1.0`.
- `GET https://www.sesric.org/robots.txt` → 200, 327 bytes.
- `GET https://www.sesric.org/oicstat.php` → 200, 932 bytes, redirected to `oicstat.sesric.org`. `GET https://oicstat.sesric.org/` → 200, 932 bytes. `GET https://oicstat.sesric.org/indicators_list` → 200, 932 bytes.
- Planned, no result: `https://www.sesric.org/query.php` and `/sesric-department-statistics.php`, in a batch that was interrupted.
- `GET https://oicstat.sesric.org/assets/index-DehhA1FF.js` → 200, 1,885,601 bytes, read for the API's paths.
- `GET https://oicstat.sesric.org:8000/indicators/categories/` → 200, 18,908 bytes.
- `GET https://oicstat.sesric.org:8000/indicators/indicator-details/all` → 200, 3,073,632 bytes.
- `GET https://oicstat.sesric.org:8000/indicators/indicators-csv/download` → 200, 2,269,297 bytes.
- `GET https://oicstat.sesric.org:8000/region-mappings/` → 200, 10,658 bytes.
