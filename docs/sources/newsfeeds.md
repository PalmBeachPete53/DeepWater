---
id: newsfeeds
name: RSS and Atom feeds read by the app
rows: [news_feeds, research_investinglive, research_ing, cryptonews]
catalogue: []
access: free, no key
probed: 2026-11-05
confidence: called   # 20 of 23 feed URLs answered; 3 not called because robots.txt forbids them
---

# RSS and Atom feeds

Every news or press feed in the code: 23 URLs from 17 publishers, over three readers. All the fields, feed by feed, are in `catalog/newsfeeds.tsv`.

## Contents
Each row is one sample of one feed at one moment: counts and rhythm move. "Span" is newest to oldest item. "Gap" is the median between items.

| Feed | Read by | Items | Span h | Gap h | Beyond `title`, `link`, a date |
|---|---|---|---|---|---|
| CNBC top news, `search.cnbc.com` | News General | not called | | | |
| CNBC finance, `search.cnbc.com` | News General | not called | | | |
| MarketWatch top stories | News General | 10 | 1.5 | 0.17 | `guid`, `description`, `dc:creator`, `media:content` |
| BBC business | News General | 48 | 3062 | 4.56 | `guid`, `description`, `media:thumbnail` |
| Nasdaq Stocks | News Stocks | 15 | 0.9 | 0.00 | `guid`, `description`, `dc:creator`, `category`, `nasdaq:tickers`, `nasdaq:partnerlink` (13 of 15) |
| MarketWatch MarketPulse | News Stocks | 30 | 9433 | 94.5 | `guid`, `description` (empty), `dc:creator`. **Stale**: newest item about 490 days old |
| Seeking Alpha market currents | News Stocks | 7 | 0.1 | 0.01 | `guid`, `category` with `domain` (repeats). No description |
| Investing.com news_25 | News Stocks | 10 | 0.4 | 0.03 | `author`, `enclosure`. No description, no guid |
| Nasdaq Markets | News Stocks | 15 | 0.1 | 0.01 | as Nasdaq Stocks, no partnerlink |
| Cointelegraph | News Crypto, Charts | 30 | 32 | 0.99 | see [cryptonews](cryptonews.md) |
| Decrypt | News Crypto, Charts | 36 | 48 | 0.81 | see [cryptonews](cryptonews.md) |
| CoinDesk | News Crypto | 25 | 23.9 | 0.48 | see [cryptonews](cryptonews.md) |
| Investing.com news_301 | News Crypto | 10 | 27 | 3.0 | as news_25 |
| FXStreet news | News Forex | 30 | 8.1 | 0.09 | `guid`, `description`, `enclosure` |
| Investing.com news_1 | News Forex | 10 | 17.8 | 0.42 | as news_25 |
| Yahoo Finance, one ticker | News ticker | not called | | | |
| Nasdaq, one ticker | News ticker | 15 | 27.8 | 1.8 | as Nasdaq Stocks |
| InvestingLive | Desk Wire, Recap | 25 | 11.6 | 0.54 | see [investinglive](investinglive.md) |
| ING Think | Desk Notes | 10 | 23.5 | 1.22 | see [ing](ing.md) |
| Federal Reserve monetary | Desk Notes, Rates | 15 | 3864 | 252 | `guid`, `description`, `category` |
| ECB press | Desk Notes | 15 | 290 | 10 | `guid`. No description |
| Bank of England news | Desk Notes | **0** | | | **An empty channel**: 210 bytes, no item, to this request |
| BIS speeches | Desk Notes | 50 | 336 | 0 (dated by day) | RSS 1.0 (`rdf:RDF`): `dc:creator`, `dc:date`, `dc:title`, `description`, and `cb:speech` (`cb:person`, `cb:institutionAbbrev`, `cb:occurrenceDate`, `cb:publicationDate`, `cb:byline`, `cb:simpleTitle`, `cb:resource`) |

Date forms met: RFC 2822 with `GMT`, `EST`-like or numeric zone; ISO 8601 at ING and BIS; `yyyy-MM-dd HH:mm:ss` at Investing.com, GMT per the parser's comment.

Channel `ttl`: BBC 15, MarketWatch 60, CoinDesk 5. Others give `sy:updatePeriod` (hourly) or nothing.

## How to call it
Plain `GET`, no key. Headers of the News page's reader: `User-Agent: Mozilla/5.0 (Macintosh) Deepwater/0.1`, `Accept: application/rss+xml, application/atom+xml, application/xml, text/xml, */*`, redirects followed (`src/NewsData.cpp:471-473`). The Desk reader sends `Deepwater/0.1 (desktop app; research page)` (`src/ResearchData.cpp:853`).

Limits on use met:
- **`search.cnbc.com`** and **`feeds.finance.yahoo.com`**: robots.txt says `User-agent: *`, `Disallow: /`. Not called. The app reads both, on the user's demand.
- **Investing.com** terms: "It is prohibited to use, store, reproduce, display, modify, transmit or distribute the data contained in this website without the explicit prior written permission of Fusion Media and/or the data provider." Its robots.txt answered 403; its feeds and the feed list answered 200. See [other-hosts](other-hosts.md).
- `feeds.content.dowjones.io` robots.txt answered 403, so nothing is known from it.

## What the app reads

Corrected on 5 November 2026: MarketPulse left the app (newest item July 2025). The Bank of England's empty answer did not recur: 50 items that day under every User-Agent tried, and it appears in Desk › Notes.
Row `news_feeds`, `src/SourcesPage.cpp:792`. Lists `src/NewsData.cpp:32-54`, ticker feeds `:458-459`.
- `Wire::parseFeed` `src/NewsData.cpp:245`, regex over `<item|entry>`:
  - title `:255`; link, else Atom `href`, else `guid` `:257-261`; `description`, else `summary`, `content:encoded`, `content`, cut to 400 characters `:263-268`.
  - date: `pubDate`, `published`, `updated`, `dc:date` `:269-273`; publisher: `dc:creator`, else `author` `:274-275`.
  - symbol: `nasdaq:tickers`, else Seeking Alpha's `category domain`, else a guess from the text `:278-291`.
- `Wire::fetchRss` `:453`: all feeds of a category merged, one copy per URL, newest first, 60 at most. Answers reused 5 minutes (`:28`).
- Desk: `deskdata::loadFeed` `src/DeskData.cpp:51`, list `src/DeskViewsTech.cpp:580-584`, hourly; InvestingLive `src/DeskViewsMarket.cpp:584`, `:865`.
- Charts: `MarketInfo::parseRss` `src/quant/server/MarketInfo.cpp:137`.

Consequences seen on this sample:
- Bank of England never appears in Desk › Notes while its feed answers empty.
- MarketWatch MarketPulse only adds items older than a year to News › Stocks, which the 60-item cut usually drops.

## Not read yet
- BIS `cb:person`, `cb:institutionAbbrev`, `cb:occurrenceDate`: who spoke, for which bank, when. Desk › Notes shows the title only.
- Other categories at publishers already read: Investing.com lists 27 feeds, among them `news_95` economic indicators, `news_14` economy, `news_11` commodities, `news_356` company news, `news_1061` analyst ratings, `news_1062` earnings, `news_1063` transcripts, `news_1064` SEC filings ([other-hosts](other-hosts.md), subject to its terms).
- Seeking Alpha's feed page lists `/feed.xml`, `/tag/<x>.xml`, `/sector/<x>.xml`, `/api/sa/combined/<TICKER>.xml`: a per-ticker feed. Its robots.txt disallows only some `/api/v3/` paths, not this one. Not called.

## Calls made
- `GET https://search.cnbc.com/robots.txt` → 200, 383 bytes.
- `https://search.cnbc.com/rs/search/combinedcms/view.xml?partnerId=wrss01&id=100003114` → not called, robots.txt disallows it for `Mozilla/5.0 (Macintosh) Deepwater/0.1`.
- `GET https://feeds.content.dowjones.io/robots.txt` → 403, 111 bytes.
- `GET https://feeds.content.dowjones.io/public/rss/mw_topstories` → 200, 8555 bytes.
- `GET https://feeds.bbci.co.uk/robots.txt` → 200, 904 bytes.
- `GET https://feeds.bbci.co.uk/news/business/rss.xml` → 200, 35190 bytes.
- `https://search.cnbc.com/rs/search/combinedcms/view.xml?partnerId=wrss01&id=10000664` → not called, robots.txt disallows it for `Mozilla/5.0 (Macintosh) Deepwater/0.1`.
- `GET https://www.nasdaq.com/robots.txt` → 200, 2132 bytes.
- `GET https://www.nasdaq.com/feed/rssoutbound?category=Stocks` → 200, 20711 bytes.
- `GET https://feeds.content.dowjones.io/public/rss/mw_marketpulse` → 200, 13275 bytes.
- `GET https://seekingalpha.com/robots.txt` → 200, 4847 bytes.
- `GET https://seekingalpha.com/market_currents.xml` → 200, 5419 bytes.
- `GET https://www.investing.com/robots.txt` → 403, 5555 bytes.
- `GET https://www.investing.com/rss/news_25.rss` → 200, 4536 bytes.
- `GET https://www.nasdaq.com/feed/rssoutbound?category=Markets` → 200, 11406 bytes.
- `GET https://cointelegraph.com/robots.txt` → 200, 444 bytes.
- `GET https://cointelegraph.com/rss` → 200, 52642 bytes.
- `GET https://decrypt.co/robots.txt` → 200, 160 bytes.
- `GET https://decrypt.co/feed` → 200, 40293 bytes.
- `GET https://www.coindesk.com/robots.txt` → 200, 4345 bytes.
- `GET https://www.coindesk.com/arc/outboundfeeds/rss/` → 308, 15 bytes.
- `GET https://www.investing.com/rss/news_301.rss` → 200, 5337 bytes.
- `GET https://www.fxstreet.com/robots.txt` → 200, 1966 bytes.
- `GET https://www.fxstreet.com/rss/news` → 200, 25488 bytes.
- `GET https://www.investing.com/rss/news_1.rss` → 200, 4643 bytes.
- `GET https://feeds.finance.yahoo.com/robots.txt` → 200, 26 bytes.
- `https://feeds.finance.yahoo.com/rss/2.0/headline?s=AAPL&region=US&lang=en-US` → not called, robots.txt disallows it for `Mozilla/5.0 (Macintosh) Deepwater/0.1`.
- `GET https://www.nasdaq.com/feed/rssoutbound?symbol=aapl` → 200, 13418 bytes.
- `GET https://www.coindesk.com/arc/outboundfeeds/rss/` → 200, 30972 bytes.
- `GET https://www.federalreserve.gov/robots.txt` → 404, 81196 bytes.
- `GET https://www.federalreserve.gov/feeds/press_monetary.xml` → 200, 9687 bytes.
- `GET https://www.ecb.europa.eu/robots.txt` → 200, 1141 bytes.
- `GET https://www.ecb.europa.eu/rss/press.html` → 200, 5901 bytes.
- `GET https://www.bankofengland.co.uk/robots.txt` → 200, 357 bytes.
- `GET https://www.bankofengland.co.uk/rss/news` → 200, 210 bytes.
- `GET https://www.bis.org/robots.txt` → 200, 2486 bytes.
- `GET https://www.bis.org/doclist/cbspeeches.rss` → 200, 66025 bytes.
