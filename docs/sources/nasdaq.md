---
id: nasdaq
name: Nasdaq
rows: [research_nasdaq, nasdaq_calendars, nasdaq]
catalogue: []
access: scraped web page    # the JSON behind nasdaq.com, no key, undocumented; RSS feeds are published as such
probed: 2026-11-05
confidence: refused         # api.nasdaq.com answers `User-agent: * / Disallow: /`, so it was not called; its contents come from the app's code. Only the RSS feeds of www.nasdaq.com were called
---

# Nasdaq

US listings: quotes, daily history, the screener of every listing, corporate calendars, 13F holders, Form 4 insiders,
financials, and news feeds carrying tickers. `api.nasdaq.com` is the undocumented backend of nasdaq.com.

**Only the RSS section was observed.** `api.nasdaq.com/robots.txt` disallows everything, with this comment: "disallows
all web crawlers from indexing any pages in this API application to prevent search engine indexing of API endpoints".
The stated aim is indexing, but the rule covers every client, so no call was made. Fields below are what the app's code
expects. Listing counts and history depth are **unknown**, not absent.

## Contents

**`api.nasdaq.com/api/` (17 endpoints known from the app's code, none called):**

| Endpoint | Fields the app expects | Code |
|---|---|---|
| `calendar/earnings?date=` | `data.rows[].{symbol, name, time, eps, epsForecast, surprise, lastYearEPS, marketCap, noOfEsts, fiscalQuarterEnding}` | `src/CalendarData.cpp:311`, `src/quant/server/MarketInfo.cpp:58` |
| `calendar/dividends?date=` | `data.calendar.rows[].{symbol, companyName, dividend_Ex_Date, payment_Date, record_Date, dividend_Rate, indicated_Annual_Dividend}` | `src/CalendarData.cpp:314`, `src/quant/server/MarketInfo.cpp:97` |
| `calendar/splits` | `data.rows[].{symbol, name, executionDate, ratio}` | `src/CalendarData.cpp:315` |
| `ipo/calendar?date=YYYY-MM` | `data.priced.rows[]`, `data.upcoming.upcomingTable.rows[]`, each `{proposedTickerSymbol, companyName, proposedExchange, proposedSharePrice, dollarValueOfSharesOffered}` | `src/CalendarData.cpp:317` |
| `quote/{t}/chart?assetclass=` | `data.chart[].{x, y}`, `data.volume` | `src/quant/server/Providers.cpp:873` |
| `quote/{t}/historical?assetclass=&fromdate=&limit=9999&todate=` | `data.tradesTable.rows[].{date, open, high, low, close, volume}` | `src/quant/server/Providers.cpp:903` |
| `quote/{t}/summary?assetclass=stocks` | used for financials | `src/quant/server/MarketInfo.cpp:308` |
| `quote/{t}/info?assetclass=stocks` | status check only | `src/quant/server/Providers.cpp:1726` |
| `quote/watchlist?` | `data[].{lastSalePrice, percentageChange, volume, marketStatus}` | `src/quant/server/Providers.cpp:1039` |
| `quote/list-type/nasdaq100` | the Nasdaq-100 | `src/ResearchData.cpp:1965` |
| `screener/{stocks,etf}?tableonly=true&limit=&offset=0&download=true` | `data.rows[]` or `data.data.rows[]`, `{symbol, name or companyName, lastsale or lastSalePrice, pctchange or percentageChange, volume, marketCap, sector, industry, country}` | `src/quant/server/Providers.cpp:957`, `src/ResearchData.cpp:1851` |
| `company/{t}/institutional-holdings?limit=25&type=TOTAL&sortColumn=marketValue&sortOrder=DESC` | `data.ownershipSummary.SharesOutstandingPCT.value`, `data.holdingsTransactions.{totalRecords, table.rows[].{ownerName, date, sharesHeld, sharesChange, sharesChangePCT, marketValue}}` | `src/ResearchData.cpp:1523` |
| `company/{t}/insider-trades?limit=200&type=ALL&sortColumn=lastDate&sortOrder=DESC` | `data.transactionTable.table.rows[].{lastDate, insider, relation, transactionType, sharesTraded, lastPrice, sharesHeld}` | `src/ResearchData.cpp:1550` |
| `company/{t}/financials?frequency=` | statements | `src/quant/server/MarketInfo.cpp:305` |
| `analyst/{t}/earnings-date` | next earnings date | `src/quant/server/MarketInfo.cpp:310` |
| `news/topic/articlebysymbol?q={t}|{cls}&offset=0&limit=30&fallback=true` | `data.rows[].{url, created, …}` | `src/quant/server/MarketInfo.cpp:239` |
| `autocomplete/slookup/10?search=` | `data[].{symbol, name, asset, subCategory, exchange}` | `src/quant/server/Providers.cpp:1444` |

Traps the code records: numbers come as text with separators and signs (`nasdaqNumber`), `marketValue` is in thousands (`src/ResearchData.cpp:1541`), `/` and `^` in tickers need rewriting (`src/ResearchData.cpp:1862`), the insider list stops at the 200 asked (README 1090).

**`www.nasdaq.com/feed/rssoutbound` (called, RSS 2.0, 15 items an answer):** `?category=Stocks`, `?category=Markets`, `?symbol={lowercase ticker}`. Channel: `title`, `link`, `description`, `language`. Item: `title`, `link`, `description`, `pubDate`, `guid`, `dc:creator`, `category`, `nasdaq:tickers`; `nasdaq:partnerlink` appears on some.

## How to call it

- RSS: `GET https://www.nasdaq.com/feed/rssoutbound?category=Stocks`, with the app's User-Agent `Mozilla/5.0 (Macintosh) Deepwater/0.1` (`src/NewsData.cpp:471`). `robots.txt` asks `Crawl-delay: 30` for all agents; calls were spaced 30 seconds apart. It disallows `/api/v1/*` and localized copies, not `/feed/`.
- API, as the app does it (not repeated here): `User-Agent: Mozilla/5.0`, `Accept: application/json`, `Origin: https://www.nasdaq.com`, `Referer: https://www.nasdaq.com/` (`src/CalendarData.cpp:363`, `src/ResearchData.cpp:851`, `src/quant/server/Providers.cpp:199`).

**Terms:** `https://www.nasdaq.com/about/terms-use` timed out twice, so they were not read. Check them before building more on this source.

## What the app reads

- `nasdaq_calendars`: earnings (five weekdays back and ahead), dividends, splits, IPOs (month before, this one, next). `src/SourcesPage.cpp:407`, `src/CalendarData.cpp:303`.
- `research_nasdaq`: holders, insiders, every listing with sector and industry, the Nasdaq-100. `src/SourcesPage.cpp:434`, `src/ResearchData.cpp:1523`, `:1550`, `:1851`, `:1965`.
- `nasdaq`: intraday chart, daily history, screener, heatmap, watchlist, search, earnings, dividends, news, financials. `src/SourcesPage.cpp:839`, `src/quant/server/Providers.cpp`, `src/quant/server/MarketInfo.cpp`.
- News: two RSS categories and the feed per symbol, with `nasdaq:tickers` for the ticker badge. `src/NewsData.cpp:39`, `:43`, `:278`, `:459`.

## Not read yet

- RSS item `description`, `category` per item: available in the three feeds. Whether the app's parser keeps them was not checked further than `dc:creator` and `nasdaq:tickers`.
- Beyond the API paths above, nothing is known here: the inventory did not explore `api.nasdaq.com`.

## Calls made

`api.nasdaq.com`: `robots.txt` only. One document attempted through a fetch tool: `https://www.nasdaq.com/about/terms-use`, timeout, twice.

- `GET https://api.nasdaq.com/robots.txt` → 200, 187 bytes
- `GET https://www.nasdaq.com/robots.txt` → 200, 2132 bytes
- `GET https://www.nasdaq.com/feed/rssoutbound?category=Stocks` → 200, 20711 bytes
- `GET https://www.nasdaq.com/feed/rssoutbound?symbol=aapl` → 200, 13418 bytes
- `GET https://www.nasdaq.com/feed/rssoutbound?category=Markets` → 200, 11406 bytes
