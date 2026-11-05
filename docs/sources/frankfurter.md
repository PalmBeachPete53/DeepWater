---
id: frankfurter
name: Frankfurter exchange rate API
rows: [fx]
catalogue: []
access: free, no key
probed: 2026-11-05
confidence: called
---

# Frankfurter

An open source API over official exchange rates. Its v1, which the app reads, serves the European Central Bank's reference rates; **its root now calls v1 `deprecated`**. Its v2 blends 108 central banks and official sources.

## Contents
**v1**: `GET /v1/` answers `"status":"deprecated"`. 30 currencies in `/v1/currencies`. Daily on business days, from 1999-01-04 (a 1999 date answers, `1990-01-02` gives 404). Fields: `amount: number, base: string, date: string, rates: {currency: number}`.

**v2**: 108 providers, 181 currencies today (crypto such as ADA among them) and 239 with `scope=all` (58 legacy ones such as DEM, FRF, ITL, XEU). The currency table starts 1948-06-21 by its `start_date`; the site says rates go back to 1948. Providers and currencies, with dates: [`catalog/frankfurter.tsv`](catalog/frankfurter.tsv).

- Provider fields: `key, name, country_code, rate_type, pivot_currency, data_url, terms_url, start_date, end_date, publish_cadence, frequency, publishes_missed, currencies, unknown_currencies`.
- Providers by frequency: daily 103, monthly 3, weekly 1, quarterly 1. By rate type: reference 22, official 15, indicative 13, average 3, unstated 11, others the rest. They span 100 country codes.
- Earliest providers: `BIS` 1900-01-31, `BBK` 1948-06-21, `FRED` 1971-01-04, `DNB` 1977-01-03. `ECB`: 1999-01-04 on, 45 currencies, reference rate.
- Rate fields: `date, base, quote, rate`.
- Currency fields: `iso_code, iso_numeric, name, symbol, start_date, end_date`.

## How to call it
- Base `https://api.frankfurter.dev`. No key. OpenAPI: `/v1/openapi.json`, `/v2/openapi.json`.
- v1 as the app does: `/v1/2026-01-01..2026-11-04?base=USD&symbols=EUR,JPY`.
- v2: `/v2/rates?base=usd&quotes=eur`, with `date, from, to, providers, group (week, month), expand=providers`; `/v2/rate/{base}/{quote}`; `/v2/providers`, `/v2/providers/{provider}/rates`; `/v2/currencies?scope=all`; `/v2/currency/{code}`; `/v2/coverage`. `.csv` suffix or `Accept: text/csv`; `Accept: application/x-ndjson` for long series (from the site's documentation).
- Trap: by default v2 **blends** providers. `providers=ecb` keeps the v1 meaning.

## What the app reads

Corrected on 5 November 2026: the app now reads `/v2/rates` with `providers=ECB`, the same rates as v1 on the days compared. The API root calls v1 `frozen` and v2 `current`.
**`fx`** (`src/SourcesPage.cpp:391`), Forex: pairs, sparklines, correlations. `/v1/{start}..{end}?base=USD&symbols=…` for the currencies of the pairs (`src/FxData.cpp:68`), keeping `rates` by date (`src/FxData.cpp:100`). The row's test is `/v1/latest?base=USD&symbols=EUR`. The Sources diagram draws this row to the ECB (`src/SourceFlow.cpp:184`).

## Not read yet
- **Forex:** v2's currencies the ECB does not fix (181 against 30), history before 1999 through `BBK`, `FRED`, `DNB` and others, `rate_type` per provider, and `/v2/coverage` to show what a pair really has.
- **Move to v2 before v1 goes:** the v1 root says deprecated, without a date in its answer.

## Calls made
All on 2026-11-05 with `User-Agent: Deepwater/1.0`, base `https://api.frankfurter.dev`.
- `GET /v1/currencies` → 200, 703 bytes.
- `GET /v1/latest?base=USD&symbols=EUR` → 200, 71 bytes; field types kept.
- `GET /v1/1999-01-04?base=EUR&symbols=USD` → 200, 70 bytes.
- `GET /v1/1990-01-02` → 404, 23 bytes.
- `GET /v1/` → 200, 104 bytes.
- `GET /v2/currencies` → 200, 23,197 bytes.
- `GET /v2/currencies?scope=all` → 200, 31,053 bytes.
- `GET /v2/providers` → 200, 67,124 bytes.
- `GET /v2/providers/ecb` → 200, 756 bytes.
- `GET /v2/rates?quotes=eur&base=usd` → 200, 65 bytes; field names kept.
- `GET /v2/openapi.json` → 200, 28,567 bytes.
- `GET https://frankfurter.dev/` → 200, 48,884 bytes (documentation).
