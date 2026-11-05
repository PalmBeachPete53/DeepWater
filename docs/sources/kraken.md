---
id: kraken
name: Kraken
rows: [kraken]
catalogue: []
access: free, no key
probed: 2026-11-05
confidence: called
---

# Kraken

Crypto spot market with many EUR and USD pairs: 1470 pairs with ticker, candles, book and trades. Its candles are
short: the last 720 of an interval, however they are asked for.

## Contents

**Instruments.** 1470 pairs in [`catalog/kraken.tsv`](catalog/kraken.tsv): `online` 1348, `cancel_only` 105, `post_only` 17. 684 bases, 26 quotes: USD 682, EUR 547, USDC 48, USDT 48, XBT 31, GBP 26, ETH 18, AUD 14, CAD 11, JPY 10, CHF 7, and smaller ones. `aclass_base` and `aclass_quote` are `currency` on all of them. Assets: 861, `enabled` 859, `withdrawal_only` 2. Kraken's names: `XXBT`, `XXBTZUSD`, with `altname` (`XBTUSD`) and `wsname` (`XBT/USD`) on each pair.

**Public endpoints, `/0/public/` (12 answered).** All answer `{error[], result}`, with status 200 even on an error such as a bad interval.

| Path | `result` |
|---|---|
| `Time` | `{unixtime, rfc1123}` |
| `SystemStatus` | `{status, timestamp}` |
| `Assets?asset=` | `<asset>.{aclass, altname, decimals, display_decimals, collateral_value, status, margin_rate}` |
| `AssetPairs?pair=` | `<pair>.{altname, wsname, aclass_base, base, aclass_quote, quote, lot, cost_decimals, pair_decimals, lot_decimals, lot_multiplier, leverage_buy[], leverage_sell[], fees[], fees_maker[], fee_volume_currency, margin_call, margin_stop, ordermin, costmin, tick_size, status, execution_venue, long_position_limit?, short_position_limit?}` |
| `Ticker?pair=` | `<pair>.{a, b, c, v, p, t, l, h, o}`: ask and bid `[price, whole lot volume, lot volume]`, last `[price, lot volume]`, and volume, VWAP, trade count, low, high as `[today, 24 h]`, today's open (documentation). With no `pair`: all, 1499 keys, more than `AssetPairs` lists |
| `OHLC?pair=&interval=&since=` | `<pair>[[time, open, high, low, close, vwap, volume, count]]`, `last` |
| `Depth?pair=&count=` | `<pair>.{asks[], bids[]}`, rows of 3: two numeric strings and an integer |
| `Trades?pair=&count=&since=` | `<pair>[[price, volume, time, buy/sell, market/limit, misc, trade id]]`, `last` (documentation for the order). Count 1 to 1000 |
| `Spread?pair=` | `<pair>[n=250][int, numeric string, numeric string]`, `last` |
| `GroupedBook?pair=&depth=&grouping=` | `{pair, bids[].{price, qty}, asks[], grouping}` |
| `PreTrade?symbol=BTC/USD` | transparency: `{symbol, description, base_asset, base_notation, base_dti_code, base_dti_short_name, quote_*, venue, system, bids[].{side, price, qty, count, publication_ts, submission_ts}, asks[]}` |
| `PostTrade?symbol=BTC/USD&count=` | `{last_ts, count, trades[].{trade_id, price, quantity, symbol, description, base_*, quote_*, trade_venue, trade_ts, publication_venue, publication_ts}}` |

`Level3` on this path → 404: the documentation places level 3 on the WebSocket.

**Candles.** Intervals in minutes: 1, 5, 15, 30, 60, 240, 1440, 10080, 21600 (documentation); `7` → an error in the array. 721 rows came back at both 1 and 1440: 720 plus the current one. `since=0` changed nothing: the documentation says older data can't be retrieved this way. So on the day of the call 1440 reached back to 2024-11-15 and 1 back 12 hours. `Trades` takes a `since` timestamp (documentation), so older candles could be built from it; not tried.

**Other hosts (documentation, not called):** `wss://ws.kraken.com/v2` (book, ticker, trade, executions, balances, level3), `wss://ws.kraken.com` (legacy), `https://futures.kraken.com/derivatives/api/v3` and `wss://futures.kraken.com/ws/v1` (derivatives). Other OpenAPI families named on `api.kraken.com`: Account Data, Trading, Algo Trading, Funding, Subaccounts, Earn, Transparency; not looked at.

## How to call it

`GET https://api.kraken.com/0/public/OHLC?pair=XBTUSD&interval=1440`. No key. JSON. Served by Cloudflare.

- `robots.txt`: 404, the API's own JSON error.
- Rate limits (documentation): a call counter per API key, by tier (max 15 to 20, decay 0.33 to 1 per second). The page read says nothing specific for public, keyless calls; none was hit here.
- The app maps `BTC` to `XBT` and `DOGE` to `XDG` (`src/quant/server/Providers.cpp:623`, `:652`).

**Terms:** not read.

## What the app reads

Row `src/SourcesPage.cpp:835`.
- `AssetPairs`: `wsname`, `altname`, `status`, for search and symbols. `src/quant/server/Providers.cpp:664`, `:644`.
- `OHLC`: time, open, high, low, close, volume (positions 0 to 4 and 6); intervals 1 to 1440. No paging: the 720 limit is the chart's limit. `src/quant/server/Providers.cpp:694`.
- `Ticker`: `b`, `a` (`:1350`); all pairs, `c`, `o`, as the fallback browse list when Binance is blocked (`:1675`).
- `Time`: status check (`:1725`, `:1761`).

## Not read yet

`Depth`, `Trades` for order flow on EUR pairs; `v[1]`, `p` of the Ticker; `vwap` and `count` in `OHLC`; `PreTrade` and `PostTrade`, which add venue and DTI identifiers; intervals 10080 and 21600 (7 and 15 days), which reach further back in 720 rows.

## Calls made

Documents read through a fetch tool: `https://docs.kraken.com/api/docs/rest-api/get-ohlc-data/`, `…/get-server-time/`, `…/get-recent-trades/`, `…/get-ticker-information/`, `…/guides/spot-rest-ratelimits/`. `GET https://docs.kraken.com/llms.txt` → 200, 6211 bytes.

`tools/source-inventory/coinbase_kraken_products.py`: `GET https://api.kraken.com/0/public/AssetPairs` → 200, 683768 bytes; `GET https://api.kraken.com/0/public/Assets` → 200, 99172 bytes.

- `GET https://api.kraken.com/robots.txt` → 404, 38 bytes
- `GET https://api.kraken.com/0/public/Time` → 200, 87 bytes
- `GET https://api.kraken.com/0/public/SystemStatus` → 200, 76 bytes
- `GET https://api.kraken.com/0/public/Assets?asset=XBT` → 200, 167 bytes
- `GET https://api.kraken.com/0/public/AssetPairs?pair=XBTUSD` → 200, 561 bytes
- `GET https://api.kraken.com/0/public/Ticker?pair=XBTUSD` → 200, 310 bytes
- `GET https://api.kraken.com/0/public/OHLC?pair=XBTUSD&interval=1440` → 200, 62318 bytes
- `GET https://api.kraken.com/0/public/OHLC?pair=XBTUSD&interval=1` → 200, 57283 bytes
- `GET https://api.kraken.com/0/public/OHLC?pair=XBTUSD&interval=1440&since=0` → 200, 62318 bytes
- `GET https://api.kraken.com/0/public/OHLC?pair=XBTUSD&interval=7` → 200, 40 bytes
- `GET https://api.kraken.com/0/public/Depth?pair=XBTUSD&count=2` → 200, 194 bytes
- `GET https://api.kraken.com/0/public/Trades?pair=XBTUSD&count=1` → 200, 134 bytes
- `GET https://api.kraken.com/0/public/Trades?pair=XBTUSD&since=0&count=1` → 200, 124 bytes
- `GET https://api.kraken.com/0/public/Spread?pair=XBTUSD` → 200, 10304 bytes
- `GET https://api.kraken.com/0/public/GroupedBook?pair=XBTUSD&depth=10&grouping=1` → 200, 930 bytes
- `GET https://api.kraken.com/0/public/Level3?pair=XBTUSD` → 404, 38 bytes
- `GET https://api.kraken.com/0/public/PreTrade?symbol=BTC/USD` → 200, 3629 bytes
- `GET https://api.kraken.com/0/public/PostTrade?symbol=BTC/USD&count=1` → 200, 557 bytes
- `GET https://api.kraken.com/0/public/Ticker` → 200, 420562 bytes
