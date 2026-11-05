---
id: coinbase
name: Coinbase Exchange
rows: [coinbase]
catalogue: []
access: free, no key
probed: 2026-11-05
confidence: called
---

# Coinbase Exchange

Crypto spot market: 845 products with candles from 2015, full order book, trades and statistics, on the public side of
Coinbase Exchange's API.

## Contents

**Instruments.** 845 products in [`catalog/coinbase.tsv`](catalog/coinbase.tsv): `online` 516, `delisted` 329. 500 base currencies (409 online). Online by quote: USD 404, EUR 33, GBP 23, USDT 20, BTC 19, USDC 5, INR 4, ETH 4, SGD, CAD, AUD, BRL 1 each. Flags: `limit_only` 25, `fx_stablecoin` 32, `trading_disabled` 329, none `post_only`, `cancel_only`, `auction_mode` or `margin_enabled`.

**Currencies.** 515: `online` 424, `delisted` 91; `crypto` 506, `fiat` 9.

**Endpoints answering without a key (13):**

| Path | Fields |
|---|---|
| `/products` | `{id, base_currency, quote_currency, quote_increment, base_increment, display_name, min_market_funds, margin_enabled, post_only, limit_only, cancel_only, status, status_message, trading_disabled, fx_stablecoin, max_slippage_percentage, auction_mode, high_bid_limit_percentage}` |
| `/products/{id}` | same, one |
| `/products/{id}/ticker` | `{ask, bid, volume, trade_id, price, size, time, rfq_volume}` |
| `/products/{id}/stats` | `{open, high, low, last, volume, volume_30day, rfq_volume_24hour, rfq_volume_30day}` |
| `/products/{id}/book?level=1`, `level=2` | `{bids[[price, size, int]], asks[], sequence, auction_mode, auction, time}`. Level 2 of BTC-USD: about 40,000 levels, 1 MB |
| `/products/{id}/trades?limit=` | `{trade_id, side, size, price, time}` |
| `/products/{id}/candles?granularity=&start=&end=` | rows `[time, low, high, open, close, volume]` (order from the documentation) |
| `/products/stats` | map of 516 products: `{stats_30day.{volume, rfq_volume?}, stats_24hour.{open, high, low, last, volume, rfq_volume?}}` |
| `/products/volume-summary` | 3139 rows `{id, base_currency, quote_currency, display_name, market_types[], spot_volume_24hour, spot_volume_30day, rfq_volume_24hour, rfq_volume_30day, conversion_volume_24hour, conversion_volume_30day}` |
| `/currencies`, `/currencies/{id}` | `{id, name, min_size, status, message, max_precision, convertible_to[], details.{type, symbol, network_confirmations, sort_order, crypto_address_link, crypto_transaction_link, push_payment_methods[], group_types[], display_name, processing_time_seconds, min_withdrawal_amount, max_withdrawal_amount}, default_network, supported_networks[], display_name}` |
| `/time` | `{iso, epoch}` |
| `/wrapped-assets` | `wrapped_assets[].{id, circulating_supply, total_supply, conversion_rate, apy, redeem_time_estimate_days}` |

**Candles.**
- Granularity in seconds: 60, 300, 900, 3600, 21600, 86400 (documentation); 7 → 400.
- BTC-USD daily starts on 2015-07-20; a range in 2015 before that date returned an empty list.
- The documentation says 300 rows at most and that wider requests are rejected: a 2-year daily range → 400, yet a request with no dates returned 350 rows. Keep to 300.
- Intervals without trades are absent, and rates "may be incomplete" (documentation).

**WebSocket (documentation, not called):** `wss://ws-feed.exchange.coinbase.com`. Without authentication: `heartbeat`, `status`, `auctionfeed`, `matches`, `rfq_matches`, `ticker`, `ticker_batch`, `level2_batch`. Authentication required: `full`, `user`, `level2`, `level3`, `balance`, and the `wss://ws-direct.exchange.coinbase.com` host. Subscribe within 5 seconds of connecting.

## How to call it

`GET https://api.exchange.coinbase.com/products/BTC-USD/candles?granularity=86400&start=2015-07-01T00:00:00Z&end=2015-08-15T00:00:00Z`. No key. JSON. Served by Cloudflare.

- `robots.txt` answers 401 with `{"message":"Unauthorized."}`, so there is no robots file to read.
- Rate limit (documentation): 10 requests a second per IP, bursts to 15, then 429.

**Terms:** the documentation says market data use is subject to Coinbase's Market Data Terms of Use; they were not read.

## What the app reads

Row `src/SourcesPage.cpp:833`.
- `/products`: `id` where `status` is `online`, for search. `src/quant/server/Providers.cpp:635`.
- `/products/{id}/candles`: all six columns, granularities 60 to 86400, 300 rows a page, up to 1500 bars. `src/quant/server/Providers.cpp:593`.
- `/products/{id}/ticker`: `bid`, `ask`. `src/quant/server/Providers.cpp:1345`; also the status check `:1724`, `:1760`.

## Not read yet

`/products/{id}/book` and `/trades`, for an order book or footprint on USD pairs that Binance doesn't list; `/products/volume-summary` for volumes across every pair in one call; `/products/stats`; the WebSocket feed.

## Calls made

Documents read through a fetch tool: `https://docs.cdp.coinbase.com/exchange/rest-api/rate-limits`, `…/api-reference/exchange-api/rest-api/products/get-product-candles`, `…/exchange/introduction/welcome`, `…/exchange/websocket-feed/overview`, `…/exchange/websocket-feed/channels`.

`tools/source-inventory/coinbase_kraken_products.py`: `GET https://api.exchange.coinbase.com/products` → 200, 356325 bytes; `GET https://api.exchange.coinbase.com/currencies` → 200, 565295 bytes.

- `GET https://api.exchange.coinbase.com/robots.txt` → 401, 27 bytes
- `GET https://api.exchange.coinbase.com/products` → 200, 356325 bytes
- `GET https://api.exchange.coinbase.com/products/BTC-USD` → 200, 420 bytes
- `GET https://api.exchange.coinbase.com/products/BTC-USD/ticker` → 200, 188 bytes
- `GET https://api.exchange.coinbase.com/products/BTC-USD/stats` → 200, 197 bytes
- `GET https://api.exchange.coinbase.com/products/BTC-USD/book?level=1` → 200, 174 bytes
- `GET https://api.exchange.coinbase.com/products/BTC-USD/book?level=2` → 200, 1052106 bytes
- `GET https://api.exchange.coinbase.com/products/BTC-USD/trades?limit=1` → 200, 120 bytes
- `GET https://api.exchange.coinbase.com/products/BTC-USD/candles?granularity=86400&start=2015-01-01T00:00:00Z&end=2015-01-02T00:00:00Z` → 200, 2 bytes
- `GET https://api.exchange.coinbase.com/currencies` → 200, 565295 bytes
- `GET https://api.exchange.coinbase.com/currencies/BTC` → 200, 2545 bytes
- `GET https://api.exchange.coinbase.com/time` → 200, 57 bytes
- `GET https://api.exchange.coinbase.com/products/volume-summary` → 200, 889891 bytes
- `GET https://api.exchange.coinbase.com/products/stats` → 200, 108324 bytes
- `GET https://api.exchange.coinbase.com/wrapped-assets` → 200, 203 bytes
- `GET https://api.exchange.coinbase.com/products/BTC-USD/candles?granularity=86400&start=2015-07-01T00:00:00Z&end=2015-08-15T00:00:00Z` → 200, 1459 bytes
- `GET https://api.exchange.coinbase.com/products/BTC-USD/candles?granularity=60` → 200, 20642 bytes
- `GET https://api.exchange.coinbase.com/products/BTC-USD/candles?granularity=7` → 400, 37 bytes
- `GET https://api.exchange.coinbase.com/products/BTC-USD/candles?granularity=86400&start=2024-01-01T00:00:00Z&end=2025-12-31T00:00:00Z` → 400, 109 bytes
