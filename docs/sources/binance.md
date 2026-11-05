---
id: binance
name: Binance
rows: [research_binance, binance]
catalogue: []
access: free, no key
probed: 2026-11-05
confidence: called          # through data-api.binance.vision and fapi.binance.com; api.binance.com and api.binance.us disallow robots and were not called
---

# Binance

Crypto spot market (3742 symbols) and USD-margined futures (942 contracts, a quarter of them on stocks, commodities,
indices and forex): candles, order book, trades, tickers, funding, open interest, long/short ratios, live streams.

**Hosts.** The app's spot chain is `api.binance.com`, then `data-api.binance.vision`, then `api.binance.us`
(`src/quant/server/Providers.cpp:308`). `robots.txt`:

| Host | `robots.txt` | Called here |
|---|---|---|
| `api.binance.com` | `User-agent: *`, `Disallow: /` | no |
| `api.binance.us` | `User-agent: *`, `Disallow: /` | no |
| `data-api.binance.vision` | none (404) | yes: spot |
| `fapi.binance.com` | disallows named search engines only | yes: USD-M futures |
| `data-stream.binance.vision`, `stream.binance.com`, `fstream.binance.com` | none (404) | streams on the first and third |

So spot shapes come from the mirror, which Binance's documentation describes as public market data only, no key. Binance.US is a separate
exchange (`src/quant/server/Providers.cpp:497`); it was not inventoried.

## Contents

### Instruments

From `exchangeInfo`, complete list in [`catalog/binance.tsv`](catalog/binance.tsv) (4684 rows: market, symbol, status, base, quote, contract, onboard date).

**Spot:** 3742 symbols, `TRADING` 1372, `BREAK` 2370. 848 base assets (519 trading). 26 quote assets among trading symbols, the largest USDT 518, TRY 310, USDC 255, U 48, USD1 31, EUR 29, FDUSD 29, IDR 29, JPY 27, BTC 26, BRL 18, ETH 12.

**USD-M futures:** 942 contracts, `TRADING` 804, `SETTLING` 137, `PENDING_TRADING` 1.
- `contractType`: `PERPETUAL` 703, `TRADIFI_PERPETUAL` 235, `CURRENT_QUARTER` 2, `NEXT_QUARTER` 2.
- `underlyingType`: `COIN` 703, `EQUITY` 197, `HK_EQUITY` 15, `COMMODITY` 8, `KR_EQUITY` 8, `PREMARKET` 5, `INDEX` 3, `CN_EQUITY` 2, `FX` 1.
- Quote and margin asset: USDT 897, USDC 39, USD1 3, U 2, BTC 1. `onboardDate` from 2019-09-08 to 2026-10-29.

### Spot REST, `/api/v3/` (14 paths answered on the mirror)

| Path | Answer | `limit` default/max, weight (documentation) |
|---|---|---|
| `ping`, `time` | `{}`, `{serverTime}` | |
| `exchangeInfo` | `{timezone, serverTime, rateLimits[], exchangeFilters[], symbols[]}`; 26 fields a symbol, 12 filter types. **17.8 MB in full**; `?symbol=` narrows it | |
| `klines`, `uiKlines` | rows of 12: open time, open, high, low, close, volume, close time, quote volume, trades, taker buy base volume, taker buy quote volume, ignore | 500/1000, weight 2 |
| `depth` | `{lastUpdateId, bids[[price,qty]], asks[]}` | 100/5000, weight 5 to 250 |
| `trades` | `{id, price, qty, quoteQty, time, isBuyerMaker, isBestMatch}` | 500/1000, weight 25 |
| `aggTrades` | `{a, p, q, f, l, T, m, M}` | 500/1000, weight 4 |
| `avgPrice` | `{mins, price, closeTime}` | weight 2 |
| `ticker/24hr` | 21 fields: changes, weighted average, last, bid, ask, OHLC, volumes, times, ids, count | weight 2, 80 without a symbol |
| `ticker`, `ticker/tradingDay` | 15 fields; `windowSize` on the first | weight 4 a symbol |
| `ticker/price`, `ticker/bookTicker` | `{symbol, price}`, `{symbol, bidPrice, bidQty, askPrice, askQty}` | weight 2 |

Documentation only: `historicalTrades` (**404 on the mirror**), `historicalBlockTrades`, `referencePrice`, `referencePrice/calculation`.

**Intervals (documentation):** `1s`, `1m`, `3m`, `5m`, `15m`, `30m`, `1h`, `2h`, `4h`, `6h`, `8h`, `12h`, `1d`, `3d`, `1w`, `1M`.

**History:** BTCUSDT daily from 2017-08-17, minute from the same day; `limit=5000` gave 1000 rows. `aggTrades?fromId=0` on the mirror returned a trade dated exactly two years before the call: older trades may not be served by id 0 there (one call, cause unknown). Bulk files exist at `data.binance.vision` (linked by Binance's site, not called).

### USD-M futures REST, `fapi.binance.com` (30 paths answered)

| Path | Fields |
|---|---|
| `/fapi/v1/ping`, `time`, `exchangeInfo` | `{}`, `{serverTime}`; `{timezone, serverTime, futuresType, rateLimits[], exchangeFilters[], assets[], symbols[]}`, 25 fields a symbol; 1.2 MB |
| `klines`, `continuousKlines`, `indexPriceKlines`, `markPriceKlines`, `premiumIndexKlines` | rows of 12. Max 1500 (documentation). `continuousKlines` takes `pair` and `contractType` |
| `depth` | `{lastUpdateId, E, T, bids, asks}`; limits 5 to 1000 |
| `trades` | `{id, price, qty, quoteQty, time, isBuyerMaker, isRPITrade}` |
| `aggTrades` | `{a, p, q, nq, f, l, T, m}`; past 48 hours only (documentation) |
| `premiumIndex` | `{symbol, markPrice, indexPrice, estimatedSettlePrice, lastFundingRate, interestRate, nextFundingTime, time}` |
| `fundingRate` | `{symbol, fundingTime, fundingRate, markPrice, rateType}`; BTCUSDT from 2019-09-10. **`startTime=0` returned the latest row, not the first**; a real date works |
| `fundingInfo` | 820 rows `{symbol, adjustedFundingRateCap, adjustedFundingRateFloor, fundingIntervalHours, disclaimer, updateTime}` |
| `ticker/24hr`, `ticker/price`, `/fapi/v2/ticker/price`, `ticker/bookTicker` | 16; 3; 3; 7 fields |
| `openInterest` | `{symbol, openInterest, time}` |
| `indexInfo`, `constituents`, `assetIndex` | composite index weights; price sources per exchange; multi-asset rates |
| `insuranceBalance` | `{symbols[], assets[].{asset, marginBalance, updateTime}}` |
| `/futures/data/openInterestHist` | `{symbol, sumOpenInterest, sumOpenInterestValue, CMCCirculatingSupply, timestamp}` |
| `/futures/data/globalLongShortAccountRatio`, `topLongShortAccountRatio`, `topLongShortPositionRatio` | `{symbol, longAccount, longShortRatio, shortAccount, timestamp}` |
| `/futures/data/takerlongshortRatio` | `{buySellRatio, sellVol, buyVol, timestamp}` |
| `/futures/data/basis` | `{indexPrice, contractType, basisRate, futuresPrice, annualizedBasisRate, basis, pair, timestamp}` |
| `/futures/data/delivery-price` | `{deliveryTime, deliveryPrice}` |

**`/futures/data/*` keep about a month**: `period=1d&limit=500` returned 31 rows; the documentation says latest 30 days. `period` in the documentation: `5m` to `12h`, one value cut off in what was read; `1d` answered. Limit 500, IP limit 1000 requests per 5 minutes.

Refused without a key: `historicalTrades` 401. Not found: `allForceOrders` 404. Documentation only: `symbolAdlRisk`, `rpiDepth`. COIN-M (`dapi`) and options were not looked at.

### Streams

Called, one message each, on `wss://data-stream.binance.vision/ws/`:
- `<pair>@aggTrade`: `{e, E, s, a, p, q, f, l, T, m, M}`
- `<pair>@depth@100ms`: `{e, E, s, U, u, b[], a[]}`
- `<pair>@kline_1m`: `{e, E, s, k.{t, T, s, i, f, L, o, c, h, l, v, n, x, q, V, Q, B}}`
- `<pair>@bookTicker`: `{u, s, b, B, a, A}`
- `<pair>@ticker`: 23 fields

Documentation, spot: `@trade`, `@miniTicker`, `!miniTicker@arr`, `@ticker_<window>`, `!ticker_<window>@arr`, `@avgPrice`, `@depth<levels>@<speed>`, `@referencePrice`, `@blockTrade`, `@kline_<i>@+08:00`.

Futures, `wss://fstream.binance.com`: `wss://…/ws/btcusdt@markPrice` connected, no message in 15 seconds; the documentation now shows `/public/ws/<stream>`, not tried. Documentation lists: book tickers, depth (100, 250, 500 ms), RPI depth, aggregate trades, liquidation orders, mini tickers, tickers, composite index, continuous klines, contract info, klines, mark price, asset index, trading session.

## How to call it

`GET https://data-api.binance.vision/api/v3/klines?symbol=BTCUSDT&interval=1d&limit=2`. `GET https://fapi.binance.com/futures/data/globalLongShortAccountRatio?symbol=BTCUSDT&period=1d&limit=30`.

- No key for any of the above. The mirror's documentation states it needs no key and serves public market data only.
- Rate limits, from `exchangeInfo`: spot `REQUEST_WEIGHT` 6000 a minute, `RAW_REQUESTS` 300000 per 5 minutes; futures `REQUEST_WEIGHT` 2400 a minute. Spot answers carry `x-mbx-used-weight-1m`.
- The mirror took about a second per call from this machine; fapi about 0.3 s.

**Terms:** the page `https://www.binance.com/en/terms` as fetched held no clause text, so they were not read.

## What the app reads

- `research_binance`: `globalLongShortAccountRatio?period=1d&limit=30`, `longAccount`. `src/DeskData.cpp:125`; row `src/SourcesPage.cpp:626`.
- `binance` (row `src/SourcesPage.cpp:831`), through `Providers::binanceGet`, `src/quant/server/Providers.cpp:491`:
  - `exchangeInfo`: `symbol`, `status`, `baseAsset`, `quoteAsset` (`:289`); `PRICE_FILTER.tickSize` (`:316`).
  - `klines`: positions 0 to 5, 1000 a page (`:546`).
  - `aggTrades` (`:329`), `depth` to 5000 levels (`:340`).
  - `ticker/24hr`: `symbol`, `lastPrice`, `priceChangePercent`, `quoteVolume` (`:1283`, `:1580`, `:1650`); `ticker/bookTicker`: `bidPrice`, `askPrice` (`:1340`); `ping` (`:1723`).
  - Streams: `@aggTrade` (`src/quant/server/TradeStream.cpp:94`), `@depth@100ms` (`src/quant/server/DepthStream.cpp:144`), `@kline_<i>` (`src/quant/server/KlineStream.cpp:33`), on the hosts of `src/quant/server/BinanceSocket.cpp:15`.

**Worth knowing:** every spot path the app uses is in the mirror's documented list, and the mirror has no `robots.txt`, while `api.binance.com`, which the app asks first, disallows robots.

## Not read yet

- Every futures price and positioning series except the one ratio: `fundingRate`, `premiumIndex`, `openInterest`, `openInterestHist`, `topLongShort*`, `takerlongshortRatio`, `basis`. They could feed Desk (positioning) or the Charts panes. History for the `/futures/data/*` family must be kept locally: a month is all it serves.
- `TRADIFI_PERPETUAL` contracts: 235 perpetuals on stocks, commodities, indices and forex. The app reads no `fapi` candles, so they are absent from Charts. Which futures endpoints serve them was not tried, except that the documentation says their `constituents` prices show as -1.
- Spot `1s` candles, `trades`, `ticker` rolling windows, `@bookTicker`.

## Calls made

Documents read through a fetch tool: `https://developers.binance.com/docs/binance-spot-api-docs/rest-api/market-data-endpoints`, `…/faqs/market_data_only`, `…/web-socket-streams`, `https://developers.binance.com/docs/derivatives/usds-margined-futures/market-data/rest-api`, `…/market-data/rest-api/Long-Short-Ratio` (served the Open Interest page), `…/websocket-market-streams`, `https://www.binance.com/en/terms`.

Instrument lists, `tools/source-inventory/binance_symbols.py`:
- `GET https://data-api.binance.vision/api/v3/exchangeInfo` → 200, 17816860 bytes
- `GET https://fapi.binance.com/fapi/v1/exchangeInfo` → 200, 1172239 bytes

Streams, one message each: `wss://data-stream.binance.vision:443/ws/btcusdt@aggTrade`, `@depth@100ms`, `@kline_1m`, `@bookTicker`, `@ticker` → 101 and a message; `wss://fstream.binance.com:443/ws/btcusdt@markPrice` → no message, read timed out after 15 s.

- `GET https://api.binance.com/robots.txt` → 200, 26 bytes
- `GET https://fapi.binance.com/robots.txt` → 200, 138 bytes
- `GET https://data-api.binance.vision/robots.txt` → 404, 146 bytes
- `GET https://api.binance.us/robots.txt` → 200, 26 bytes
- `GET https://www.binance.com/robots.txt` → 200, 6722 bytes
- `GET https://data-api.binance.vision/api/v3/ping` → 200, 2 bytes
- `GET https://data-api.binance.vision/api/v3/time` → 200, 28 bytes
- `GET https://data-api.binance.vision/api/v3/klines?symbol=BTCUSDT&interval=1d&limit=2` → 200, 372 bytes
- `GET https://data-api.binance.vision/api/v3/klines?symbol=BTCUSDT&interval=1d&startTime=0&limit=1` → 200, 172 bytes
- `GET https://data-api.binance.vision/api/v3/uiKlines?symbol=BTCUSDT&interval=1m&limit=1` → 200, 170 bytes
- `GET https://data-api.binance.vision/api/v3/depth?symbol=BTCUSDT&limit=5` → 200, 368 bytes
- `GET https://data-api.binance.vision/api/v3/trades?symbol=BTCUSDT&limit=1` → 200, 148 bytes
- `GET https://data-api.binance.vision/api/v3/aggTrades?symbol=BTCUSDT&limit=1` → 200, 123 bytes
- `GET https://data-api.binance.vision/api/v3/historicalTrades?symbol=BTCUSDT&limit=1` → 404, 146 bytes
- `GET https://data-api.binance.vision/api/v3/avgPrice?symbol=BTCUSDT` → 200, 61 bytes
- `GET https://data-api.binance.vision/api/v3/ticker/24hr?symbol=BTCUSDT` → 200, 557 bytes
- `GET https://data-api.binance.vision/api/v3/ticker/tradingDay?symbol=BTCUSDT` → 200, 399 bytes
- `GET https://data-api.binance.vision/api/v3/ticker/price?symbol=BTCUSDT` → 200, 45 bytes
- `GET https://data-api.binance.vision/api/v3/ticker/bookTicker?symbol=BTCUSDT` → 200, 121 bytes
- `GET https://data-api.binance.vision/api/v3/ticker?symbol=BTCUSDT&windowSize=1d` → 200, 400 bytes
- `GET https://fapi.binance.com/fapi/v1/ping` → 200, 2 bytes
- `GET https://fapi.binance.com/fapi/v1/time` → 200, 28 bytes
- `GET https://fapi.binance.com/fapi/v1/klines?symbol=BTCUSDT&interval=1d&limit=1` → 200, 149 bytes
- `GET https://fapi.binance.com/fapi/v1/continuousKlines?pair=BTCUSDT&contractType=PERPETUAL&interval=1d&limit=1` → 200, 149 bytes
- `GET https://fapi.binance.com/fapi/v1/indexPriceKlines?pair=BTCUSDT&interval=1d&limit=1` → 200, 125 bytes
- `GET https://fapi.binance.com/fapi/v1/markPriceKlines?symbol=BTCUSDT&interval=1d&limit=1` → 200, 125 bytes
- `GET https://fapi.binance.com/fapi/v1/premiumIndexKlines?symbol=BTCUSDT&interval=1d&limit=1` → 200, 111 bytes
- `GET https://fapi.binance.com/fapi/v1/depth?symbol=BTCUSDT&limit=5` → 200, 295 bytes
- `GET https://fapi.binance.com/fapi/v1/trades?symbol=BTCUSDT&limit=1` → 200, 133 bytes
- `GET https://fapi.binance.com/fapi/v1/aggTrades?symbol=BTCUSDT&limit=1` → 200, 116 bytes
- `GET https://fapi.binance.com/fapi/v1/premiumIndex?symbol=BTCUSDT` → 200, 231 bytes
- `GET https://fapi.binance.com/fapi/v1/fundingRate?symbol=BTCUSDT&limit=1` → 200, 127 bytes
- `GET https://fapi.binance.com/fapi/v1/fundingRate?symbol=BTCUSDT&startTime=0&limit=1` → 200, 127 bytes
- `GET https://fapi.binance.com/fapi/v1/fundingInfo` → 200, 140202 bytes
- `GET https://fapi.binance.com/fapi/v1/ticker/24hr?symbol=BTCUSDT` → 200, 373 bytes
- `GET https://fapi.binance.com/fapi/v1/ticker/price?symbol=BTCUSDT` → 200, 60 bytes
- `GET https://fapi.binance.com/fapi/v2/ticker/price?symbol=BTCUSDT` → 200, 60 bytes
- `GET https://fapi.binance.com/fapi/v1/ticker/bookTicker?symbol=BTCUSDT` → 200, 149 bytes
- `GET https://fapi.binance.com/fapi/v1/openInterest?symbol=BTCUSDT` → 200, 68 bytes
- `GET https://fapi.binance.com/futures/data/openInterestHist?symbol=BTCUSDT&period=1d&limit=1` → 200, 171 bytes
- `GET https://fapi.binance.com/futures/data/topLongShortAccountRatio?symbol=BTCUSDT&period=1d&limit=1` → 200, 121 bytes
- `GET https://fapi.binance.com/futures/data/topLongShortPositionRatio?symbol=BTCUSDT&period=1d&limit=1` → 200, 121 bytes
- `GET https://fapi.binance.com/futures/data/globalLongShortAccountRatio?symbol=BTCUSDT&period=1d&limit=500` → 200, 3721 bytes
- `GET https://fapi.binance.com/futures/data/takerlongshortRatio?symbol=BTCUSDT&period=1d&limit=1` → 200, 100 bytes
- `GET https://fapi.binance.com/futures/data/basis?pair=BTCUSDT&contractType=PERPETUAL&period=1d&limit=1` → 200, 199 bytes
- `GET https://fapi.binance.com/futures/data/delivery-price?pair=BTCUSDT` → 200, 1119 bytes
- `GET https://fapi.binance.com/fapi/v1/indexInfo?symbol=BTCDOMUSDT` → 200, 2193 bytes
- `GET https://fapi.binance.com/fapi/v1/constituents?symbol=BTCUSDT` → 200, 764 bytes
- `GET https://fapi.binance.com/fapi/v1/assetIndex?symbol=BTCUSD` → 200, 321 bytes
- `GET https://fapi.binance.com/fapi/v1/insuranceBalance?symbol=BTCUSDT` → 200, 592 bytes
- `GET https://fapi.binance.com/fapi/v1/allForceOrders?symbol=BTCUSDT&limit=1` → 404, 1434 bytes
- `GET https://fapi.binance.com/fapi/v1/historicalTrades?symbol=BTCUSDT&limit=1` → 401, 46 bytes
- `GET https://data-api.binance.vision/api/v3/klines?symbol=BTCUSDT&interval=1m&startTime=0&limit=1` → 200, 158 bytes
- `GET https://data-api.binance.vision/api/v3/klines?symbol=BTCUSDT&interval=1s&limit=2` → 200, 321 bytes
- `GET https://data-api.binance.vision/api/v3/klines?symbol=BTCUSDT&interval=1d&limit=5000` → 200, 187323 bytes
- `GET https://fapi.binance.com/fapi/v1/klines?symbol=BTCUSDT&interval=1d&startTime=1&limit=1` → 200, 126 bytes
- `GET https://fapi.binance.com/fapi/v1/fundingRate?symbol=BTCUSDT&startTime=1500000000000&limit=1` → 200, 113 bytes
- `GET https://fapi.binance.com/futures/data/openInterestHist?symbol=BTCUSDT&period=5m&limit=500` → 200, 85001 bytes
- `GET https://data-api.binance.vision/api/v3/aggTrades?symbol=BTCUSDT&fromId=0&limit=1` → 200, 122 bytes
- `GET https://data-stream.binance.vision/robots.txt` → 404, 20 bytes
- `GET https://stream.binance.com/robots.txt` → 404, 20 bytes
- `GET https://fstream.binance.com/robots.txt` → 404, 20 bytes
