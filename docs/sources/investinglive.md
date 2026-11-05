---
id: investinglive
name: InvestingLive
rows: [research_investinglive]
catalogue: []
access: scraped web page
probed: 2026-11-05
confidence: called
---

# InvestingLive

A market news site for forex and macro traders, with short headlines through the day. The app reads its public RSS feed.

## Contents
**Feed** `https://investinglive.com/feed/` (RSS 2.0, 165 KB in this sample):
- 25 items, covering 11.6 hours, a median of 0.54 hours between items.
- Channel: `title`, `link`, `description`, `language`, `copyright`, `category`, `generator`, `image`, `lastBuildDate`, `atom:id`, `atom:link`, `sy:updatePeriod` (hourly), `sy:updateFrequency` (30).
- Item, all in 25/25: `title`, `link`, `guid` (`isPermaLink`), `pubDate` (RFC 2822, zone `GMT`), `category`, `comments`, `dc:creator` (repeated), `description`.
- `description` is the whole article as HTML (5,530 characters on average), not a summary.
- `category` labels seen: Commodities, News, Stocks, Education, Cryptocurrency, Central Banks, Technical Analysis, Forex Orders, Stock market update.

**Site** (home page links): `/live-feed/`, `/technical-analysis/`, `/directory/`, `/premium/`, `/rss/`, a Spanish edition `/es/`, and sections such as `/commodities/`, `/cryptocurrency/`.

## How to call it
`GET https://investinglive.com/feed/`, no key. robots.txt allows it.

**Terms** (`/terms-of-use/`): content "must not be copied, transmitted and/or published in any form whatsoever and/or make any commercial and/or non-personal use that is not consistent with the Website's purposes and these Terms of Use without the Company's prior written consent". Reading a public feed for one user's own screen is not obviously outside this; redistribution would be.

## What the app reads
Row `research_investinglive`, `src/SourcesPage.cpp:644`.
- Desk › Recap `src/DeskViewsMarket.cpp:584`, reused for 5 minutes; Desk › Wire `src/DeskViewsMarket.cpp:865`, each minute. Both through `deskdata::loadFeed` `src/DeskData.cpp:51` and `news::Wire::parseFeed`.
- Kept: `title`, `link`, `description` as plain text cut to 400 characters (`src/NewsData.cpp:263-268`), `pubDate`, `dc:creator`.

## Not read yet
- `category`: the feed labels each item, which Desk › Wire could use for its filter beside its own tags.
- The rest of `description`: the full text, for a local model through a skill. Check the terms first.
- `/rss/` lists other feeds; that page was not called.

## Calls made
- `GET https://investinglive.com/robots.txt` → 200, 1381 bytes.
- `GET https://investinglive.com/feed/` → 200, 165810 bytes.
- `GET https://investinglive.com/` → 200, 721328 bytes.
- `GET https://investinglive.com/terms-and-conditions/` → 404, 215289 bytes.
- `GET https://investinglive.com/terms-of-use/` → 200, 394311 bytes.
