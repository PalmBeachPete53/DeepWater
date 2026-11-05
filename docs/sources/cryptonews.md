---
id: cryptonews
name: Crypto news feeds (Cointelegraph, Decrypt, CoinDesk)
rows: [cryptonews]
catalogue: []
access: free, no key
probed: 2026-11-05
confidence: called
---

# Crypto news feeds

Three RSS feeds of crypto news. Charts' News tab reads Cointelegraph and Decrypt; the News page reads those two, CoinDesk and Investing.com's crypto feed (see [newsfeeds](newsfeeds.md)).

## Contents
All three are RSS 2.0, sampled once with the News page's headers. Their channels declare `sy:updatePeriod` hourly, `sy:updateFrequency` 1.

| Feed | Items | Span (h) | Median gap (h) | Item elements beyond `title`, `link`, `guid`, `pubDate`, `description`, `dc:creator` |
|---|---|---|---|---|
| `https://cointelegraph.com/rss` | 30 | 32.0 | 0.99 | `atom:updated`, `category`, `enclosure`, `media:content` (with `media:credit`, `media:description`) |
| `https://decrypt.co/feed` | 36 | 48.0 | 0.81 | `category` (repeated), `enclosure`, `media:thumbnail` |
| `https://www.coindesk.com/arc/outboundfeeds/rss/` | 25 | 23.9 | 0.48 | `category` (`domain` attribute, repeated), `content:encoded` (empty), `dc:description` (empty), `media:content` (23 of 25); `dc:creator` repeats |

`description` averages 451 characters at Cointelegraph, 150 at Decrypt, 157 at CoinDesk. Dates are RFC 2822 with a numeric zone. CoinDesk's channel adds `ttl` 5.

## How to call it
`GET` the URL, no key. robots.txt of the three allows them. **CoinDesk answers 308** from the slash form to `https://www.coindesk.com/arc/outboundfeeds/rss`: follow redirects.

## What the app reads
Row `cryptonews`, `src/SourcesPage.cpp:843`.
- Charts: `MarketInfo::cryptoNews` `src/quant/server/MarketInfo.cpp:163`, Cointelegraph and Decrypt (`:168-169`), cached 10 minutes, User-Agent `Mozilla/5.0 QuantTerminal`. `parseRss` `:137` keeps `title`, `link`, `pubDate`, `description` cut to 280 characters; the source name is added. Items are filtered by coin name in `MarketInfo::news`.
- Row check: `src/quant/server/Providers.cpp:1733`.
- News page: `src/NewsData.cpp:46-48`, parsed by `Wire::parseFeed`, see [newsfeeds](newsfeeds.md).

`parseRss` matches `<item>` with no attributes (`:140`): both feeds fit today.

## Not read yet
- `category` of the three feeds: tags that could replace guessing the coin from the text (`guessSymbol`, `src/NewsData.cpp`).
- CoinDesk in Charts' News tab, which only reads two of the three.
- `media:content`, `enclosure`: article images.

## Calls made
- `GET https://cointelegraph.com/robots.txt` → 200, 444 bytes.
- `GET https://cointelegraph.com/rss` → 200, 52642 bytes.
- `GET https://decrypt.co/robots.txt` → 200, 160 bytes.
- `GET https://decrypt.co/feed` → 200, 40293 bytes.
- `GET https://www.coindesk.com/robots.txt` → 200, 4345 bytes.
- `GET https://www.coindesk.com/arc/outboundfeeds/rss/` → 308, 15 bytes.
- `GET https://www.coindesk.com/arc/outboundfeeds/rss/` → 200, 30972 bytes.

Sampled with the News page's User-Agent (`Mozilla/5.0 (Macintosh) Deepwater/0.1`), not Charts' `Mozilla/5.0 QuantTerminal`.
