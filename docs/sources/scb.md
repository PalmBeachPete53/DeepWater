---
id: scb
name: Statistics Sweden (SCB)
rows: [research_pxweb]
catalogue: ["Statistics Sweden"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# Statistics Sweden

The Statistical Database over PxWeb API 2: Swedish statistics, including tables from other authorities (the Riksbank, the
Financial Supervisory Authority).

## Contents
2,801 tables at the current host, complete in `catalog/scb.tsv` (id, label, subject, path, time unit, first, last, updated, variables,
source). The old `v2beta` host reports 2,826. Annual 2,020, Monthly 311, Quarterly 284, Other 182, Weekly 4. 1,610 updated in 2025-2026.
Sources: Statistics Sweden 2,520; National Mediation Office 84; The Riksbank 65; Environmental Protection Agency 45; Energy
Administration 31; Financial Supervisory Authority 30; Board of Agriculture 12; Transport Analysis 8.

By subject: Labour market 670; Education and research 306; Population 269; Environment 210; Housing, construction and building 209;
Living conditions 209; National accounts 164; Business activities 162; Democracy 121; Household finances 95; Financial markets 89; Prices and
Consumption 75; Public finances 73; Trade in goods and services 48; Energy 41; General statistics 40; Agriculture 12; Transport 8.

Macro tables:
- `TAB6593` CPIF excluding energy, 2020=100, `ContentsCode`(3), `Tid`(477), 1987M01 to 2026M09.
- `TAB5737` CPI 1980=100: label says "no update after 2025M12".
- `TAB5782` lending rates by purpose, `Referenssektor`(5), `Motpartssektor`(3), `Avtal`(2), `Andamal`(4), `Tid`(253), 2005M09 to 2026M09;
  `TAB5780`, `TAB5781`, `TAB5783`, `TAB6773`, `TAB6535` mortgage rates; `TAB6541` money supply; `TAB1872` industrial production; `TAB2893`,
  `TAB2901`, `TAB2907` unemployment.

## How to call it
- Current API: `https://statistikdatabasen.scb.se/api/v2/tables?lang=en&pageSize=1000`, `.../tables/TAB6593/metadata?lang=en`,
  `.../tables/TAB6593/data?lang=en&outputFormat=json-stat2&valuecodes[ContentsCode]=000007ZV&valuecodes[Tid]=*` (0.5 s).
- **The app's host is the slow one.** `https://api.scb.se/OV0104/v2beta/api/v2/...` answered three times out of three after 60 to 76 s,
  then with a 200. The other host answers the same request in under a second.

## What the app reads

Corrected on 5 November 2026: the app now calls `statistikdatabasen.scb.se/api/v2`. The old host's 60-76 s did not recur: it answered in 0.5 s that day.
Row `research_pxweb` (`src/SourcesPage.cpp:568`), loader `src/CountryData.cpp:1589`, base at `src/CountryData.cpp:1592`, `flow: scb`:
- `TAB6593` `ContentsCode=000007ZV`: Sweden core inflation, CPIF excluding energy (`resources/country_indicators.json:325`).
- The request's transfer timeout is 45 s (`src/ResearchData.cpp:849`), shorter than the delay measured on that host, so this
  series can be expected to fail in the app. Not checked in the running app. `statistikdatabasen.scb.se/api/v2` is a drop-in host.

## Not read yet
`TAB6541` money supply, `TAB5782` and `TAB5783` lending rates, `TAB2893` unemployment, `TAB1872` production, for Countries › Sweden.

## Calls made
Made on 2026-11-05, sequentially per host with a pause, `User-Agent: Deepwater/1.0`. 12 calls.

- `GET https://statistikdatabasen.scb.se/api/v2/tables?lang=…&pageSize=…&pageNumber=…` × 4 → 200: 4, 3.4 MB in all; first `https://statistikdatabasen.scb.se/api/v2/tables?lang=en&pageSize=100&pageNumber=1`
- `GET https://api.scb.se/OV0104/v2beta/api/v2/tables?lang=en&pageSize=100&pageNumber=1` → 200, 110.8 KB
- `GET https://api.scb.se/OV0104/v2beta/api/v2/tables?lang=en&pageSize=1&pageNumber=1` → 200, 1.5 KB
- `GET https://statistikdatabasen.scb.se/api/v2/tables/{id}?lang=…` × 4 → 200: 4, 76.8 KB in all; first `https://statistikdatabasen.scb.se/api/v2/tables/TAB6593/metadata?lang=en`
- `GET https://api.scb.se/OV0104/v2beta/api/v2/tables/TAB6593/data?lang=en&outputFormat=json-stat2&valuecodes[ContentsCode]=000007ZV&valuecodes[Tid]=*` → 200, 19.8 KB
- `GET https://statistikdatabasen.scb.se/api/v2/tables/TAB6593/data?lang=en&outputFormat=json-stat2&valuecodes[ContentsCode]=000007ZV&valuecodes[Tid]=*` → 200, 20.0 KB
