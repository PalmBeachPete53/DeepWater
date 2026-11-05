---
id: yahoo
name: Yahoo Finance
rows: [yahoo_futures, yahoo_sessions, research_yahoo, yahoo]
catalogue: []
access: scraped web page    # undocumented JSON endpoints, no key, a session crumb on two of them
probed: 2026-11-05
confidence: refused         # query1, query2 and feeds.finance.yahoo.com answer `User-agent: * / Disallow: /`, so no data call was made; contents below come from the app's code and one third-party list
---

# Yahoo Finance

Prices, company data, statements, analysts and screeners for stocks, ETFs, indices, futures, forex and crypto
worldwide. No official API or public documentation exists for these endpoints; the app relies on them as they are.

**Nothing on this card was observed in an answer.** All three API hosts forbid robots in `robots.txt` and Yahoo's terms
forbid automated collection, so the inventory stopped at `robots.txt`. Fields below are what the app's code expects
(`file:line`), and may be out of date or incomplete. Instrument counts, history depth and the full field lists of
each module are **unknown**, not absent.

## Contents

Endpoints known from the app's code (10, plus the cookie host):

| Endpoint | Parameters the app sends | Fields the app expects | Code |
|---|---|---|---|
| `query1 /v8/finance/chart/{symbol}` | `interval`, `range`, `includePrePost`, `events=split` | `chart.result[0].{meta.gmtoffset, timestamp[], indicators.quote[0].{open,high,low,close,volume}[], events.splits{}.{numerator,denominator,date}}`, `chart.error.description` | `src/quant/server/Providers.cpp:765`, `:778`; `src/ResearchData.cpp:1400`, `:1429`, `:1448` |
| `query1 /v7/finance/spark` | `symbols` (20 at most, says the code), `range`, `interval` | `spark.result[].{symbol, response[0].{meta.currentTradingPeriod.regular.{start,end}, timestamp[], indicators.quote[0].close[]}}` | `src/CommodityPrices.cpp:65`; `src/CalendarData.cpp:484`, `:498`; `src/ResearchData.cpp:1496` |
| `query1 /v10/finance/quoteSummary/{symbol}` | `modules`, `crumb` | `quoteSummary.result[0].<module>`, `quoteSummary.error.description` | `src/ResearchData.cpp:955` |
| `query1 /v7/finance/quote` | `symbols`, `crumb` | `quoteResponse.result[]`, see fields below | `src/ResearchData.cpp:1012` |
| `query1 /v1/finance/screener/predefined/saved` | `scrIds`, `count` | `finance.result[0].quotes[]` | `src/ResearchData.cpp:1029` |
| `query1 /v1/finance/search` | `q`, `quotesCount`, `newsCount`, `listsCount` | `quotes[].{symbol, longname, shortname, quoteType, exchDisp, typeDisp}` | `src/ResearchData.cpp:1827`; `src/quant/server/Providers.cpp:1459` |
| `query1 /ws/fundamentals-timeseries/v1/finance/timeseries/{symbol}` | `symbol`, `type=annual<Line>,quarterly<Line>,…`, `period1`, `period2` | keyed by `type`; 142 statement lines in the app's layout | `src/ResearchData.cpp:575`, `statementLayout` |
| `query2 /v6/finance/recommendationsbysymbol/{symbol}` | | `finance.result[].recommendedSymbols[].symbol` | `src/ResearchData.cpp:1580` |
| `query1 /v1/test/getcrumb` | session cookie | the crumb, as plain text | `src/ResearchData.cpp:808` |
| `feeds.finance.yahoo.com /rss/2.0/headline` | `s`, `region=US`, `lang=en-US` | RSS items | `src/NewsData.cpp:458` |
| `fc.yahoo.com /` | | answers 404 with the consent cookie, says the code | `src/ResearchData.cpp:802` |

**quoteSummary modules.** The list of valid modules below comes from the open-source `yfinance` library
(`yfinance/const.py`, read 2026-11-05): third-party and unofficial, not confirmed by Yahoo or by a call. 37 modules;
**app** marks the 15 the app requests (`src/ResearchData.cpp:952`).

- Profile: `assetProfile` app, `summaryProfile`, `fundProfile`, `quoteType` app, `price` app, `summaryDetail` app, `defaultKeyStatistics` app, `financialData` app, `esgScores`, `secFilings`, `calendarEvents` app
- Statements: `incomeStatementHistory`, `incomeStatementHistoryQuarterly`, `balanceSheetHistory`, `balanceSheetHistoryQuarterly`, `cashFlowStatementHistory`, `cashFlowStatementHistoryQuarterly`
- Earnings: `earnings` app, `earningsHistory`, `earningsHistoryGaap`, `earningsHistoryNonGaap`, `earningsTrend` app, `earningsTrendGaap`, `earningsTrendNonGaap`
- Analysts and trends: `recommendationTrend` app, `upgradeDowngradeHistory` app, `industryTrend`, `indexTrend`, `sectorTrend`
- Ownership: `institutionOwnership` app, `fundOwnership`, `majorDirectHolders`, `majorHoldersBreakdown` app, `insiderTransactions` app, `insiderHolders`, `netSharePurchaseActivity` app
- Derivatives: `futuresChain`

**Fields the app reads, per module** (from the code; each module surely holds more, unknown here):

- `assetProfile`: `companyOfficers`, `country`, `fullTimeEmployees`, `industry`, `industryDisp`, `longBusinessSummary`, `sector`, `sectorDisp`, `website`
- `summaryDetail`: `beta`, `dividendRate`, `dividendYield`, `exDividendDate`, `fiftyTwoWeekHigh`, `fiftyTwoWeekLow`, `forwardPE`, `marketCap`, `payoutRatio`, `priceToSalesTrailing12Months`, `trailingAnnualDividendRate`, `trailingPE`
- `defaultKeyStatistics`: `52WeekChange`, `SandP52WeekChange`, `beta`, `enterpriseToEbitda`, `enterpriseToRevenue`, `enterpriseValue`, `floatShares`, `forwardEps`, `forwardPE`, `heldPercentInsiders`, `heldPercentInstitutions`, `impliedSharesOutstanding`, `netIncomeToCommon`, `pegRatio`, `priceToBook`, `sharesOutstanding`, `shortPercentOfFloat`, `trailingEps`
- `financialData`: `currentPrice`, `currentRatio`, `debtToEquity`, `earningsGrowth`, `ebitda`, `ebitdaMargins`, `financialCurrency`, `freeCashflow`, `grossMargins`, `grossProfits`, `numberOfAnalystOpinions`, `operatingCashflow`, `operatingMargins`, `profitMargins`, `quickRatio`, `recommendationMean`, `returnOnAssets`, `returnOnEquity`, `revenueGrowth`, `targetHighPrice`, `targetLowPrice`, `targetMeanPrice`, `totalCash`, `totalDebt`, `totalRevenue`
- `price`: `currency`, `exchangeName`, `longName`, `regularMarketChange`, `regularMarketChangePercent`, `regularMarketPrice`, `shortName`
- `quoteType`: `firstTradeDateEpochUtc`
- `majorHoldersBreakdown`: `insidersPercentHeld`, `institutionsPercentHeld`
- `calendarEvents.earnings`, `earningsTrend.trend[]`, `recommendationTrend.trend[]`, `upgradeDowngradeHistory.history[]`: read as lists (`src/ResearchCompanyViews.cpp:762`, `:1603`, `:1700`; `src/ResearchData.cpp:2150`)
- `earnings`, `netSharePurchaseActivity`, `insiderTransactions`, `institutionOwnership`: requested; no reader was located for the last three by name

**Quote fields seen in the app's code** (`v7/finance/quote`, chart `meta`): `chartPreviousClose`, `fiftyTwoWeekHigh`, `fiftyTwoWeekLow`, `forwardPE`, `gmtoffset`, `longName`, `marketCap`, `regularMarketChange`, `regularMarketChangePercent`, `regularMarketPrice`, `regularMarketTime`, `regularMarketVolume`, `shortName`, `trailingPE`.

**Intervals and ranges the app assumes** (`src/quant/server/Providers.cpp:739`): `1m` over `7d`; `2m`, `5m`, `15m`, `30m` over `60d`; `60m` over `730d`; `1d` over `20y`; `1wk` and `1mo` over `max`. Assumptions in code, not a statement of Yahoo's limits.

**Symbols.** Yahoo's suffix forms appear in the code: `CL=F` (future), `DX-Y.NYB`, `CAD=X` (forex; answered under `USDCAD=X`, `src/ResearchData.cpp:1019`), pence-quoted currencies `GBp`, `ZAc`, `ILA` (`src/ResearchData.cpp:985`). No count of instruments is available: Yahoo publishes no symbol list the inventory could read.

## How to call it

Not called for this card. What the code does, for reference:

- `v7/finance/quote` and `v10/finance/quoteSummary` need a session: cookie from `fc.yahoo.com`, crumb from `v1/test/getcrumb`, `crumb=` on the call (`src/ResearchData.cpp:796`, `:837`).
- User-Agent: the app sends a bare `Mozilla/5.0`, with this comment: "Yahoo answers 429 to any full browser User-Agent (seen 15 Oct 2026) but 200 to a bare one" (`src/quant/server/Providers.cpp:27`). After a 429 it stops asking for 10 minutes (`:731`).

**`robots.txt`:** `query1`, `query2` and `feeds.finance.yahoo.com` all answer `User-agent: *` / `Disallow: /`. On `finance.yahoo.com`, the file lists AI crawlers by name in a block of their own.

**Terms:** Yahoo's terms, section 2(d) item 9, forbid to "access or collect data, or attempt to access or collect data, from our Services using any automated means", naming "robots, spiders, scrapers, data mining tools". Item 10 forbids using the content to create a "database, archive, mobile application, data feed, widget" competing with the Services. Section 2(h) forbids reproducing or distributing any portion without written permission.

## What the app reads

- `yahoo_futures`: `v7/finance/spark`, close and timestamps of 20 commodities, 20 symbols a request. `src/SourcesPage.cpp:379`, `src/CommodityPrices.cpp:65`.
- `yahoo_sessions`: `v7/finance/spark?range=1d&interval=1h`, `meta.currentTradingPeriod.regular`. `src/SourcesPage.cpp:410`, `src/CalendarData.cpp:498`.
- `research_yahoo`: summary, quotes, statements, prices, candles, spark, search, peers, splits, screeners `day_gainers`, `day_losers`, `most_actives` (`src/OverviewPage.cpp:861`). `src/SourcesPage.cpp:430`, `src/ResearchData.cpp`.
- `yahoo`: chart and search for the Charts tab. `src/SourcesPage.cpp:837`, `src/quant/server/Providers.cpp:765`, `:1459`.
- News by ticker: the RSS feed. `src/NewsData.cpp:458`.
- Also Economy (`DX-Y.NYB`, stock indices and currencies, README lines 817 and 939) and Quant › Markov (`src/SourcesPage.cpp:610`).

## Not read yet

Listed for the record, none verified here and no recommendation to add any: the 22 modules of the list above the app does not request. Options (`v7/finance/options`) are not used by the app; its existence is from general knowledge, not from this inventory. Any new dependency on this source sits under the terms above, and the app's fundamentals already read the SEC first (`src/ResearchData.cpp:1415`).

## Calls made

No data call: `robots.txt` only. Documents read through a fetch tool: `https://legal.yahoo.com/us/en/yahoo/terms/otos/index.html` (first 100,000 of 126,993 characters), `https://raw.githubusercontent.com/ranaroussi/yfinance/main/yfinance/const.py`.

- `GET https://query1.finance.yahoo.com/robots.txt` → 200, 26 bytes
- `GET https://finance.yahoo.com/robots.txt` → 200, 2539 bytes
- `GET https://query2.finance.yahoo.com/robots.txt` → 200, 26 bytes
- `GET https://feeds.finance.yahoo.com/robots.txt` → 200, 26 bytes
