---
id: gus
name: Statistics Poland (GUS)
rows: []
catalogue: ["Central Statistical Office of Poland (GUS)"]
access: free, no key
probed: 2026-11-05
confidence: called     # stopped on a 429 before the whole subject tree was read
---

# Statistics Poland

Three APIs: BDL (Local Data Bank, down to municipalities), DBW (domain knowledge bases, national) and SDP.

## Contents
- **BDL** `subjects`: 33 top subjects, in `catalog/gus_subjects.tsv` with the children of those read: PRICES, FINANCES OF ENTERPRISES (QUARTERLY), PUBLIC FINANCE, EUROPEAN FUNDS, HOUSING ECONOMY, SOCIAL ECONOMY, TRADE AND CATERING, INVESTMENTS, CULTURE, SPORT, FORESTRY, POPULATION, NATIONAL CENSUSES, SCIENCE AND TECHNOLOGY, HEALTH CARE, STATE AND JUSTICE, ENTITIES OF THE NATIONAL ECONOMY,
  TERRITORIAL DIVISION, AGRICULTURAL CENSUSES, INDUSTRY AND CONSTRUCTION, REGIONAL ACCOUNTS, AGRICULTURE, FUEL ENERGY AND MATERIALS, REAL ESTATE, LABOUR MARKET, LOCAL GOVERNMENT, ENVIRONMENT, EDUCATION, HIGHER EDUCATION, TRANSPORT, TOURISM, PRE-PRIMARY EDUCATION, WAGES. Only `K15`, `K43`, `K27`, `K3` had their children read.
- BDL `levels`: 8, Poland, macroregions, voivodeships, regions, subregions, powiats, gminas and one more. `years`: 32, 1995 to 2026, with a `quarterly` flag. Number of variables not read: `/variables` timed out at 60 s.
- **DBW** `area/area-area`: 254 nodes: 32 Domains, 95 Topics, 124 Information scopes, 3 Thematic blocks. Domains include Prices, Construction, Public finances, Maritime and inland economy, Energy statistics, Internal market, International exchange, Business and consumer tendency, Science and technology, Industry, Non-financial enterprises, National and regional accounts, Agriculture, Digital economy.
  Fields `id`, `nazwa`, `id-nadrzedny-element`, `id-poziom`, `nazwa-poziom`, `czy-zmienne`.
- **SDP** `api-sdp.stat.gov.pl/api/1.1.0/area/area-area` answers in the same format, opening with block "Economy".

## How to call it
- BDL `https://bdl.stat.gov.pl/api/v1/subjects?lang=en&format=json&page-size=100`, `subjects?parent-id=K15...`, `levels`, `years`, `variables?subject-id=...`. **Slow and rate-limited**: answers took 1.5 to 52 s, two calls timed out at 60 s, and the 12th BDL call answered **429** though calls were at least 10 s apart; BDL was stopped there.
  Works with no key: the catalogue's "free key" is optional, not required. The JSON's `links` name an internal host `vmacor12.stat.intra`: do not follow them.
- DBW `https://api-dbw.stat.gov.pl/api/1.1.0/area/area-area?lang=en`. `area/area-variable?id-obszaru=186` and `variable/variable-section` are 404: those routes were guesses.
- SDP `https://api-sdp.stat.gov.pl/api/1.1.0/area/area-area?lang=en`.

## What the app reads
Nothing directly. Poland comes through Eurostat and the OECD.

## Not read yet
DBW Domains `Prices`, `Business and consumer tendency`, `Industry` for Countries › Poland. BDL for regional detail.

## Calls made
Made on 2026-11-05, sequentially per host with a pause, `User-Agent: Deepwater/1.0`. 18 calls.

- `GET https://bdl.stat.gov.pl/api/v1/subjects?lang=en&format=json&page-size=100` → 200, 5.5 KB
- `GET https://api-dbw.stat.gov.pl/api/1.1.0/area/area-area?lang=en` → 200, 46.9 KB
- `GET https://bdl.stat.gov.pl/api/v1/levels?lang=en&format=json` → 200, 330 B
- `GET https://bdl.stat.gov.pl/api/v1/years?lang=en&format=json` → 200, 1.6 KB
- `GET https://bdl.stat.gov.pl/api/v1/subjects?parent-id=…&lang=…&format=…&page-size=…` × 7 → 200: 5, ERR: 1, 429: 1, 7.5 KB in all; first `https://bdl.stat.gov.pl/api/v1/subjects?parent-id=K15&lang=en&format=json&page-size=100`
- `GET https://bdl.stat.gov.pl/api/v1/variables?subject-id=P2425&lang=en&format=json&page-size=5` → ERR, 0 B
- `GET https://bdl.stat.gov.pl/api/v1/variables?lang=en&format=json&page-size=1` → 429, 81 B
- `GET https://api-dbw.stat.gov.pl/api/1.1.0/variable/variable-area?id-obszaru=186&lang=en` → 404, 0 B
- `GET https://api-sdp.stat.gov.pl/api/variable/variable-section?lang=en` → 404, 0 B
- `GET https://api-dbw.stat.gov.pl/api/1.1.0/variable/variable-section?id-obszaru=186&lang=en` → 404, 0 B
- `GET https://api-dbw.stat.gov.pl/api/1.1.0/area/area-variable?id-obszaru=186&lang=en` → 404, 201 B
- `GET https://api-sdp.stat.gov.pl/api/1.1.0/area/area-area?lang=en` → 200, 55.8 KB
