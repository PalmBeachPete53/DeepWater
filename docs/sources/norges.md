---
id: norges
name: Norges Bank
rows: []
catalogue: ["Norges Bank"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# Norges Bank

Norway's central bank over SDMX: policy rate, exchange rates, government debt, money market and liquidity.

## Contents
24 dataflows (agency `NB`), each with its dimensions, codelist sizes, last-observation row count and first and last period in
`catalog/norges_structures.jsonl`. The counts are of the rows returned by `lastNObservations=1`, i.e. series.

- `IR` policy rate, FREQ A/B/M, 9 series, 1982 on; `ANN_KPRA` policy rate announcements, 1986 on.
- `EXR` exchange rates: FREQ, BASE_CUR(44), QUOTE_CUR(44), TENOR(19), 123 series (B, M, A), 1960 on.
- `SHORT_RATES`: INSTRUMENT_TYPE, TENOR(17), 28 series, 2011-09 on.
- `GOVT_GENERIC_RATES` 21 series 2019 on; `GOVT_ZEROCOUPON` TENOR(19), 36 series, 2015 on; `GOVT_KEYFIGURES` 854, 1993 on; `GOVT_PRIMARY_MARKET`
  ISIN(128), 2,576, 1993 on; `GOVT_SECONDARY_MARKET` 2,149 monthly, 2013 on; `GOVT_IRS` 470, 2005-06 to 2014-11 (ended); `SEC` prices and yields by
  security, 188, 2020-07 on.
- `MONEY_MARKET`: MM_SEGMENT, COUNTERPART_TYPE, TRANSACTION_TYPE, MATURITY(18), RATE_TYPE, 35,881 series, business days, 2020-01 on.
- `LIQUIDITY_STATISTICS` 39, 2008 on; `LIQUIDITY_FORECAST` 5,120 daily, 2014 on, into future dates; `SETTLEMENT_STATISTICS` 18, 2009 on.
- `FINANCIAL_INDICATORS`: INDICATOR(5), 5 series, 2002 on. `REGNET` Regional Network survey: TOPIC(9), SECTOR(13), REGION(8), HORIZON(6), quarterly,
  83 series, 2003-Q1 on. `CBC_CALENDAR`, `CBC_INSTRUMENTS`, `CBC_TRANSACTIONS` central bank certificates, 2026 on. `FAUCTION` F-auctions, 12,109 rows. `ANN_FX_SPU`.
  `ANN_TEST` test messages. `LENDINGSURVEY`: FREQ, CODE(46), SEGMENT, PERIOD, TOPIC(5), QUESTION(27): defined, but its data call answered 404.
- `FAUCTION`'s last date reads 2929: a typing error at the source. Do not sort on it.

## How to call it
- Base `https://data.norges-bank.no/api`. Catalogue `/dataflow/all/all/latest?format=sdmx-json`; structure
  `/dataflow/NB/<flow>/<version>?references=all&format=sdmx-json`.
- Data `/data/EXR?format=csv&lastNObservations=1&locale=en` (`;` separated, `TIME_PERIOD`).

## What the app reads
Nothing. Norway's rates are not read from here.

## Not read yet
`IR` for the Norwegian policy rate, `GOVT_ZEROCOUPON` for a NOK curve in Desk › Rates, `EXR` for NOK crosses, `REGNET`
for a Norwegian business-climate line in Countries › Norway.

## Calls made
Made on 2026-11-05, sequentially per host with a pause, `User-Agent: Deepwater/1.0`. 73 calls.

- `GET https://data.norges-bank.no/api/dataflow/all/all/latest?format=sdmx-json` → 200, 19.5 KB
- `GET https://data.norges-bank.no/api/dataflow/{id}/1.0?references=…&format=…` × 22 → 200: 22, 4.2 MB in all; first `https://data.norges-bank.no/api/dataflow/NB/ANN_FX_SPU/1.0?references=all&format=sdmx-json`
- `GET https://data.norges-bank.no/api/data/{id}?format=…&lastNObservations=…&locale=…` × 24 → 200: 23, 404: 1, 10.7 MB in all; first `https://data.norges-bank.no/api/data/ANN_FX_SPU?format=csv&lastNObservations=1&locale=en`
- `GET https://data.norges-bank.no/api/data/{id}?format=…&firstNObservations=…&locale=…` × 24 → 200: 23, 404: 1, 10.6 MB in all; first `https://data.norges-bank.no/api/data/ANN_FX_SPU?format=csv&firstNObservations=1&locale=en`
- `GET https://data.norges-bank.no/api/dataflow/NB/IR/1.1?references=all&format=sdmx-json` → 200, 182.3 KB
- `GET https://data.norges-bank.no/api/dataflow/NB/SHORT_RATES/1.2?references=all&format=sdmx-json` → 200, 186.4 KB
