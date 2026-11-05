---
id: alternative
name: Alternative.me
rows: [research_alternative]
catalogue: []
access: free, no key
probed: 2026-11-05
confidence: called
---

# Alternative.me

The crypto Fear & Greed index, daily since February 2018, plus a small free crypto ticker API.

## Contents

| Endpoint | Fields | Extent |
|---|---|---|
| `/fng/` | `name`, `data[].{value, value_classification, timestamp, time_until_update?}`, `metadata.error`. All values are strings | `limit=0`: 3196 daily rows, 2018-02-01 to the day of the call. `time_until_update` on the latest row only |
| `/v2/listings/` | `data[].{id, name, symbol, website_slug}`, `metadata.{timestamp, num_cryptocurrencies, error}` | 189 coins |
| `/v2/ticker/` | `data.<id>.{id, name, symbol, website_slug, rank, circulating_supply, total_supply, max_supply, quotes.USD.{price, volume_24h, market_cap, percentage_change_1h, _24h, _7d, percent_change_1h, _24h, _7d}, last_updated}`, `metadata` | `limit`, `start`, `convert`, `structure`, `sort` (documented) |
| `/v2/ticker/{id or slug}/` | same, `convert=EUR` adds `quotes.EUR` | one coin |
| `/v2/global/` | `data.{active_cryptocurrencies, active_markets, bitcoin_percentage_of_market_cap, quotes.USD.{total_market_cap, total_volume_24h}, last_updated}`, `metadata` | latest |

From the documentation, not tested:
- Index components: volatility 25%, momentum and volume 25%, social media 15%, surveys 15% (paused), dominance 10%, trends 10%.
- `/fng/` parameters: `limit` (default 1, 0 for all), `format` (`json`, `csv`), `date_format` (`us`, `cn`, `kr`, `world`).
- `convert`: USD, EUR, GBP, RUB, JPY, CAD, KRW, PLN and BTC, ETH, XRP, LTC, BCH. v1 endpoints still listed as legacy.
- Prices update every 5 minutes. There is no price history.

## How to call it

`GET https://api.alternative.me/fng/?limit=0&format=json`. No key. `robots.txt` on this host: 404; on `alternative.me`, nothing disallowed.

- Rate limit (documentation): 60 requests a minute over a 10-minute window.
- Trap: `format=csv` answers with content type JSON and a body that isn't valid JSON (a brace-wrapped block, 11 lines for 2 rows).

**Terms:** the two pages differ. The index page says attribution is required ("must properly acknowledge the source of the data and prominently reference it accordingly", right next to the displayed data for commercial use). The API page says attribution isn't required but appreciated. Keep the stricter one: the app labels the series "alternative.me" (`src/EconomyPage.cpp:100`). The API may be changed or withdrawn at any time, says the same page.

## What the app reads

- `research_alternative`: `/fng/?limit=0&format=json`, an hour; `value`, `timestamp`. `src/ResearchData.cpp:1240`; row `src/SourcesPage.cpp:475`.

## Not read yet

`value_classification` (the index's own label, the app could show it instead of its own); `/v2/global/` for market cap and BTC dominance without CoinMarketCap's terms (latest only, no history).

## Calls made

Documents read through a fetch tool: `https://alternative.me/crypto/fear-and-greed-index/`, `https://alternative.me/crypto/api/`.

- `GET https://api.alternative.me/robots.txt` → 404, 255 bytes
- `GET https://alternative.me/robots.txt` → 200, 24 bytes
- `GET https://api.alternative.me/fng/?limit=1` → 200, 211 bytes
- `GET https://api.alternative.me/fng/?limit=2&format=csv&date_format=world` → 200, 160 bytes
- `GET https://api.alternative.me/v2/listings/` → 200, 19111 bytes
- `GET https://api.alternative.me/v2/ticker/?limit=1` → 200, 776 bytes
- `GET https://api.alternative.me/v2/ticker/bitcoin/?convert=EUR` → 200, 1042 bytes
- `GET https://api.alternative.me/v2/global/` → 200, 335 bytes
- `GET https://api.alternative.me/fng/?limit=0&format=json` → 200, 301443 bytes
