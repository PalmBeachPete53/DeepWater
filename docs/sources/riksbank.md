---
id: riksbank
name: Sveriges Riksbank
rows: []
catalogue: ["Sveriges Riksbank"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# Sveriges Riksbank

Sweden's central bank: SWEA interest and exchange rates, the SWESTR benchmark and its monetary policy forecasts.

## Contents
- **SWEA** `Series`: 117 series, complete in `catalog/riksbank.tsv` (id, group, name, first, last); 40 closed. First date 1900-01-02, last the
  current day. Fields: `seriesId`, `source`, `shortDescription`, `midDescription`, `longDescription`, `groupId`, `observationMinDate`,
  `observationMaxDate`, `seriesClosed`.
- **SWEA** `Groups`, a tree of 25: Currencies against Swedish kronor 53 series; International Government Bonds 10 years 10, 5 years 7; STIBOR 8; Euro
  Market Rates 3 months 6, 6 months 6; Riksbank key interest rates 5 (`SECBREPOEFF` policy rate, `SECBDEPOEFF`, `SECBLENDEFF`...); Treasury Bills 4;
  Government Bonds 4; Swedish Fixing Rates 3; Other Riksbank rates 2; Mortgage Bonds 2; Commercial Paper 2; Forward Premiums 2; TCW, KIX and SDR 1 each.
- **SWESTR** `/swestr/v1/latest/SWESTR`: fields `rate`, `date`, `pctl12_5`, `pctl87_5`, `volume`, `alternativeCalculation`, `publicationTime`,
  `republication`, `numberOfTransactions`, `numberOfAgents`.
- **Monetary policy data** `/monetary_policy_data/v1/forecasts/policy_rounds`: the policy rounds from `2015:1` on. Other routes of this API were not called.

## How to call it
- Base `https://api.riksbank.se`. `GET /swea/v1/Series`, `GET /swea/v1/Groups`. All calls answered at one every 15 s; a limit was not tested.
  `/swea/v1/CrossRates`, `/swea/v1/CalendarDays`, `/forexreserves/v1/` are 404 as guessed: no such routes.

## What the app reads
Nothing. Sweden's money-market figures are not read from here, and `src/SourcesPage.cpp` has no Riksbank row.

## Not read yet
`SECBREPOEFF` and the policy-round forecasts for Desk › Rates, `SEGVB2YC`, `SEGVB5YC`, `SEGVB7YC` government bonds for a SEK curve, STIBOR, SWESTR, the KIX index.

## Calls made
Made on 2026-11-05, sequentially per host with a pause, `User-Agent: Deepwater/1.0`. 7 calls.

- `GET https://api.riksbank.se/swea/v1/Groups` → 200, 8.1 KB
- `GET https://api.riksbank.se/swea/v1/Series` → 200, 35.7 KB
- `GET https://api.riksbank.se/swea/v1/CrossRates` → 404, 54 B
- `GET https://api.riksbank.se/swea/v1/CalendarDays` → 404, 54 B
- `GET https://api.riksbank.se/swestr/v1/latest/SWESTR` → 200, 257 B
- `GET https://api.riksbank.se/monetary_policy_data/v1/forecasts/policy_rounds` → 200, 1.1 KB
- `GET https://api.riksbank.se/forexreserves/v1/` → 404, 54 B
