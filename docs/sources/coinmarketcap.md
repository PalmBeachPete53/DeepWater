---
id: coinmarketcap
name: CoinMarketCap
rows: [research_coinmarketcap]
catalogue: []
access: scraped web page    # the site's own undocumented data-api, no key; the documented Pro API needs a key
probed: 2026-11-05
confidence: called
---

# CoinMarketCap

Crypto market aggregates: total capitalisation and dominance, its own fear and greed index, altcoin season index, and
listings of cryptocurrencies. `api.coinmarketcap.com/data-api` answers without a key but has no public documentation.

**Terms restrict this.** CoinMarketCap's terms (24 Nov 2025) §5.1.4 forbid "any data mining, crawling, "scraping", robot or
similar automated" methods and point to the paid API for certain data; §5.1.5 forbids excessive requests. They don't name
undocumented APIs. Calls stopped once this was read. `robots.txt` disallows `/v1/` and `/v2/` only, not `/data-api/`.

## Contents

**`data-api` answering without a key (6 endpoints called).** Every answer: `{data, status: {timestamp, error_code, error_message, elapsed, credit_count}}`.

| Endpoint | `data` holds | History observed |
|---|---|---|
| `v3/fear-greed/chart?start=&end=` | `dataList[].{score, name, timestamp, btcPrice, btcVolume}`, `dialConfig[].{start, end, name}`, `historicalValues.{now, yesterday, lastWeek, lastMonth, yearlyHigh, yearlyLow}.{score, name, timestamp}` | daily, from 2023-06-29 |
| `v3/altcoin-season/chart?start=&end=` | `points[].{name, altcoinIndex, altcoinMarketcap, timestamp}`, `historicalValues` (same six), `dialConfigs[]`, `topCryptos[n=100].{id, name, symbol, slug, percentChange}` | daily, from 2024-03-22 |
| `v4/global-metrics/quotes/historical?convertId=2781&range=` | `points[].{marketCap, volume, btcValue, ethValue, stableValue, otherValue, timestamp}`, `historicalValues`, `yearlyPerformance.{high, low}`, `thirtyDaysPercentage` | `7d` about 15-minute steps; `1y` daily; `all` weekly from 2022-06-20 |
| `v3/global-metrics/quotes/latest` | dominance, counts (`activeCryptoCurrencies`, `totalCryptoCurrencies`, `activeMarketPairs`, `activeExchanges`, `totalExchanges`, `totalCryptoDexCurrencies`), DeFi, stablecoin and derivatives volume and cap, `quotes[]` of 23 fields, `etherscanGas`, listing increments | latest |
| `v3/cryptocurrency/listing?start=&limit=` | `cryptoCurrencyList[].{id, name, symbol, slug, tags[], marketPairCount, circulatingSupply, selfReportedCirculatingSupply, totalSupply, maxSupply?, isActive, lastUpdated, dateAdded, cmcRank?, platform?, isAudited, badges[], quotes[]}`, `totalCount`. `quotes[]` has price, volume, cap, dominance, turnover, changes over 1h to 1y | latest |
| `v3/map/all?listing_status=active&limit=` | `cryptoCurrencyMap[].{id, name, symbol, slug, is_active, status, first_historical_data, last_historical_data, rank}`, `exchangeMap[].{id, name, slug, is_active, status, first_historical_data, last_historical_data}` | gives each asset's first date |

Traps: `listing?limit=1` returned 2 rows. `convertId=2781` is the app's conversion id (USD by CoinMarketCap's own numbering, not stated by the answer). `timestamp` is a digit string in `v3`.

**Pro API (`pro-api.coinmarketcap.com`), documentation only, no key here.** `v1/key/info` without a key → 401 with `{status: {timestamp, error_code, error_message, elapsed, credit_count, notice}}`. The documentation page names 4 families (market data, exchange data, DEX data, market intelligence) and "90+ endpoints"; plan limits are on the pricing page, not read.

## How to call it

`GET https://api.coinmarketcap.com/data-api/v4/global-metrics/quotes/historical?convertId=2781&range=1y`, no key, no special header. No rate-limit header seen.

## What the app reads

- `research_coinmarketcap`:
  - `v3/altcoin-season/chart`: `altcoinIndex`, `timestamp`; an hour. `src/ResearchData.cpp:1260`.
  - `v4/global-metrics/quotes/historical`: `range=1y` then `all`; `marketCap`, `btcValue`, `timestamp`; dominance computed from them. `src/ResearchData.cpp:1295`, `:1279`.
  - Row and its check: `src/SourcesPage.cpp:471`.

## Not read yet

`stableValue`, `ethValue`, `otherValue` and `volume` of the historical series; `topCryptos`; `fear-greed/chart` (the app's crypto index is alternative.me's); `map/all`. All carry the terms above; prefer the Pro API with a key for anything new.

## Calls made

Documents read through a fetch tool: `https://coinmarketcap.com/terms/`, `https://coinmarketcap.com/api/documentation`.

- `GET https://api.coinmarketcap.com/robots.txt` → 200, 44 bytes
- `GET https://api.coinmarketcap.com/data-api/v3/fear-greed/chart?start=1700000000&end=1700100000` → 200, 996 bytes
- `GET https://api.coinmarketcap.com/data-api/v3/altcoin-season/chart?start=1700000000&end=1700100000` → 200, 11007 bytes
- `GET https://api.coinmarketcap.com/data-api/v4/global-metrics/quotes/historical?convertId=2781&range=7d` → 200, 131272 bytes
- `GET https://api.coinmarketcap.com/data-api/v3/fear-greed/chart?start=1500000000&end=1790000000` → 200, 125440 bytes
- `GET https://api.coinmarketcap.com/data-api/v3/altcoin-season/chart?start=1577836800&end=1790000000` → 200, 101395 bytes
- `GET https://api.coinmarketcap.com/data-api/v4/global-metrics/quotes/historical?convertId=2781&range=1y` → 200, 71757 bytes
- `GET https://api.coinmarketcap.com/data-api/v4/global-metrics/quotes/historical?convertId=2781&range=all` → 200, 45157 bytes
- `GET https://api.coinmarketcap.com/data-api/v3/cryptocurrency/listing?start=1&limit=1` → 200, 3378 bytes
- `GET https://api.coinmarketcap.com/data-api/v3/map/all?listing_status=active&limit=1` → 200, 571 bytes
- `GET https://api.coinmarketcap.com/data-api/v3/global-metrics/quotes/latest` → 200, 2728 bytes
- `GET https://pro-api.coinmarketcap.com/robots.txt` → 404, 153 bytes
- `GET https://pro-api.coinmarketcap.com/v1/key/info` → 401, 147 bytes
