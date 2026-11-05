---
id: alpaca
name: Alpaca Market Data
rows: [alpaca]
catalogue: []
access: free account        # a free key for stocks; crypto answers without one
probed: 2026-11-05
confidence: docs            # the key is missing from this inventory: stocks, options and news come from the documentation; crypto was called
---

# Alpaca Market Data

US stocks, options, crypto, fixed income, news, corporate actions and logos. The app uses only stock bars. Stocks need a
key; crypto answered without one.

## Contents

**42 reference entries in 8 families**, in [`catalog/alpaca.tsv`](catalog/alpaca.tsv) (documentation index):

| Family | Entries | Holds |
|---|---|---|
| Stock | 18 | auctions, bars, quotes, trades, snapshots, latest, condition and exchange codes |
| Option | 8 | bars, trades, latest quotes and trades, snapshots, chain, codes |
| Crypto | 8 | bars, quotes, trades, latest, order book, snapshots |
| Fixed income | 2 | latest prices, quotes |
| Screener | 2 | most active, top movers |
| Corporate actions | 2 | list, event stream (SSE) |
| News | 1 | articles |
| Logos | 1 | logos |

The changelog notes "Market Data API reference adds a Stock v3 section" on 2026-11-04.

**Stock bars (documentation):** `GET /v2/stocks/bars`.
- Parameters: `symbols`, `timeframe`, `start`, `end`, `limit` (default 1000, max 10000 across all symbols), `adjustment` (`raw`, `split`, `dividend`, `spin-off`, `all`), `asof`, `feed`, `currency`, `page_token`, `sort`.
- Timeframes: `[1-59]Min`, `[1-23]Hour`, `1Day`, `1Week`, `[1,2,3,4,6,12]Month`.
- Fields: `t`, `o`, `h`, `l`, `c`, `v`, `n`, `vw`.
- Feeds: `sip` (all US exchanges), `iex` (one exchange, about 2.5% of volume, the only one usable without a subscription), `boats` (Blue Ocean ATS, evening), `overnight` (derived from BOATS), `otc`.
- History: since 2016 on both plans; Basic is restricted on the latest 15 minutes.

**Plans (documentation):** Basic, free: IEX in real time, 30 symbols on the WebSocket, 200 requests a minute. Algo Trader Plus: all US exchanges, OPRA, 10,000 requests a minute. Forex isn't mentioned.

**Crypto, called without a key, `/v1beta3/crypto/us/` (6 paths):**

| Path | Fields |
|---|---|
| `bars?symbols=&timeframe=&limit=` | `bars.<symbol>[].{c, h, l, n, o, t, v, vw}`, `next_page_token` |
| `trades?symbols=&limit=` | `trades.<symbol>[].{i, p, s, t, tks}`, `next_page_token` |
| `latest/trades` | `trades.<symbol>.{i, p, s, t, tks}` |
| `latest/quotes` | `quotes.<symbol>.{ap, as, bp, bs, t}` |
| `latest/orderbooks` | `orderbooks.<symbol>.{a[].{p, s}, b[], t}` |
| `snapshots` | `snapshots.<symbol>.{dailyBar, latestQuote, latestTrade, minuteBar, prevDailyBar}` |

The documentation says these now serve data from Alpaca's own exchange. Instrument counts and the crypto rate limit without a key were not established.

## How to call it

`GET https://data.alpaca.markets/v2/stocks/bars?symbols=AAPL&timeframe=1Day&limit=1`, headers `APCA-API-KEY-ID`, `APCA-API-SECRET-KEY`. Without them: 401, an HTML page from nginx; same for `v1beta1/news` and `robots.txt`. Pages with `page_token`.

Crypto, no key: `GET https://data.alpaca.markets/v1beta3/crypto/us/bars?symbols=BTC/USD&timeframe=1Day&limit=1`.

**Terms:** the documentation pages read hold no redistribution clause; Alpaca's agreements were not read.

## What the app reads

Row `src/SourcesPage.cpp:848`. Keys in `feeds.json`, never read for this card.
- `/v2/stocks/bars`: `symbols`, `timeframe` (`1Min`, `5Min`, `15Min`, `30Min`, `1Day`), `start`, `limit=10000`, `feed` (default `iex`), `adjustment=split`, `sort=desc`, `page_token`, up to 12 pages. Keeps `t`, `o`, `h`, `l`, `c`, `v`. `src/quant/server/Feeds.cpp:235`, `:259`, `:269`.
- Key check: bars of SPY. `src/quant/server/Feeds.cpp:449`.

## Not read yet

`n`, `vw` of the bars; the crypto endpoints as a keyless crypto source with `vw`; news, corporate actions, screener, logos, for a user with a free key; the option chain.

## Calls made

Documents read through a fetch tool: `https://docs.alpaca.markets/docs/about-market-data-api`, `…/reference/stockbars`, `…/us/docs/historical-stock-data-1.md`, `…/us/docs/historical-crypto-data-1.md`. `robots.txt` of `docs.alpaca.markets` allows these.

- `GET https://data.alpaca.markets/robots.txt` → 401, 172 bytes
- `GET https://data.alpaca.markets/v2/stocks/AAPL/bars?timeframe=1Day&limit=1` → 401, 172 bytes
- `GET https://data.alpaca.markets/v1beta3/crypto/us/bars?symbols=BTC/USD&timeframe=1Day&limit=1` → 200, 177 bytes
- `GET https://data.alpaca.markets/v1beta3/crypto/us/latest/orderbooks?symbols=BTC/USD` → 200, 5443 bytes
- `GET https://data.alpaca.markets/v1beta3/crypto/us/latest/trades?symbols=BTC/USD` → 200, 119 bytes
- `GET https://data.alpaca.markets/v1beta3/crypto/us/latest/quotes?symbols=BTC/USD` → 200, 119 bytes
- `GET https://data.alpaca.markets/v1beta3/crypto/us/snapshots?symbols=BTC/USD` → 200, 665 bytes
- `GET https://data.alpaca.markets/v1beta3/crypto/us/trades?symbols=BTC/USD&limit=1` → 200, 209 bytes
- `GET https://data.alpaca.markets/v2/stocks/bars?symbols=AAPL&timeframe=1Day&limit=1` → 401, 172 bytes
- `GET https://data.alpaca.markets/v1beta1/news?limit=1` → 401, 172 bytes
- `GET https://docs.alpaca.markets/llms.txt` → 200, 155415 bytes
- `GET https://docs.alpaca.markets/robots.txt` → 200, 219 bytes
