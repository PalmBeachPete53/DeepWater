---
id: polygon
name: Polygon (Massive)
rows: [polygon]
catalogue: []
access: paid                # needs a key; a Basic plan is named in the documentation, its price and limits were not found
probed: 2026-11-05
confidence: docs            # the key is missing from this inventory: all three calls without one answered 401
---

# Polygon (Massive)

Polygon, now Massive: US stocks, options, indices, futures, forex and crypto, with fundamentals, SEC filings, news,
economy series and partner data sets, over REST, WebSocket and S3 flat files. The app uses only bars.

**Everything here but the 401s comes from Massive's documentation.**

## Contents

The documentation index `https://massive.com/docs/llms.txt` lists 238 pages, all in [`catalog/polygon.tsv`](catalog/polygon.tsv) (kind, family, doc path, title). A page's API path is on that page, not in the catalogue.

| Family | REST pages | WebSocket pages | Flat file pages |
|---|---|---|---|
| Stocks | 51 | 8 | 5 |
| Crypto | 21 | 6 | 4 |
| Forex | 21 | 5 | 4 |
| Options | 20 | 6 | 5 |
| Partners | 16 | | |
| Indices | 14 | 4 | 4 |
| Futures | 10 | 5 | 17 |
| Economy | 6 | | |
| Alternative | 3 | | |

Stocks REST groups: aggregates, snapshots, trades and quotes, tickers, corporate actions (dividends, splits, IPOs, ticker events), fundamentals (balance sheets, cash flows, income statements, ratios, float, short interest, short volume), filings (10-K sections, 8-K, 13-F, forms 3 and 4, insider activity and holdings, risk factors), technical indicators (SMA, EMA, MACD, RSI), market operations, news.

Economy: treasury yields, inflation, inflation expectations, labor market, funding conditions.

Futures: aggregates, contracts, products, schedules, exchanges, market status, snapshot, trades, quotes.

Partners: Benzinga (news, earnings, ratings, consensus, guidance, analysts, firms), ETF Global (analytics, constituents, fund flows, profiles, taxonomies), TMX (corporate events). Alternative: consumer spending by merchant.

**Custom bars:** `GET /v2/aggs/ticker/{stocksTicker}/range/{multiplier}/{timespan}/{from}/{to}`, parameters `adjusted`, `sort`, `limit` (default 5000, max 50000). Fields: `ticker`, `adjusted`, `queryCount`, `request_id`, `resultsCount`, `status`, `next_url`, `results[].{c, h, l, n, o, otc, t, v, vw}`. Records back to 2003-09-10; depth by plan: Basic 2 years, Starter 5, Developer 10, Advanced and Business all.

**Plans:** the pricing page as read showed Stocks Advanced, Financials & Ratios, NYSE Order Imbalances, partner data sets. Free tier limits, calls per minute and delays were not on any page read. Instrument counts were not established: `v3/reference/tickers` needs a key.

## How to call it

Base `https://api.massive.com`. Key in `Authorization: Bearer <key>` or `?apiKey=` (the app uses the header, so the key stays out of URLs). Pages through `next_url`. Without a key: 401, `{status, request_id, error}`; `robots.txt` 404.

## What the app reads

Row `src/SourcesPage.cpp:850`. Key in `feeds.json`, never read for this card.
- `/v2/aggs/ticker/{t}/range/{mult}/{span}/{from}/{to}?adjusted=true&sort=desc&limit=50000`: minutes 1, 5, 15, 30 and day; `t`, then bar fields; follows `next_url` on the same host, 6 pages at most. `src/quant/server/Feeds.cpp:298`, `:340`.
- `/futures/v1/aggs/{contract}?resolution=&window_start.gte=&sort=window_start.desc&limit=50000`: `window_start`, `session_end_date`, `open`, `high`, `low`, `close`, `volume`. `src/quant/server/Feeds.cpp:412`.
- Key check: bars of SPY. `src/quant/server/Feeds.cpp:458`.

## Not read yet

For a user holding a key, pages to read first: fundamentals and filings for Research, `economy/treasury-yields` for Economy, `stocks/snapshots/full-market-snapshot` for Overview, Benzinga ratings and earnings for Research. The plan each needs is on its page.

## Calls made

Documents read through a fetch tool: `https://massive.com/docs/rest/quickstart`, `…/rest/quickstart.md`, `…/rest/stocks/aggregates/custom-bars.md`, `https://massive.com/pricing`. `GET https://massive.com/llms.txt` → redirected to `/docs/llms.txt`, 200, 52463 bytes; downloaded once more for the catalogue.

- `GET https://api.massive.com/robots.txt` → 404, 18 bytes
- `GET https://api.massive.com/v2/aggs/ticker/AAPL/prev` → 401, 101 bytes
- `GET https://api.massive.com/v3/reference/tickers?limit=1` → 401, 101 bytes
- `GET https://api.massive.com/v1/marketstatus/now` → 401, 101 bytes
- `GET https://massive.com/llms.txt` → 200, 52463 bytes
