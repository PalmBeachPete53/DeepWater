---
id: tradingview
name: TradingView economic calendar
rows: [tradingview_calendar, surveys]
catalogue: []
access: scraped web page    # undocumented JSON, no key, wants a tradingview.com Origin
probed: 2026-11-05
confidence: called
---

# TradingView economic calendar

Economic releases by country, each with actual, forecast and previous, the publisher, a link to its page, the reference
period and an importance level. The only host used is `economic-calendar.tradingview.com`, with one endpoint.

## Contents

**`/events`** answers `{status, result[], total}`; `status` alone, with no `result`, when the range holds no event.

An event (23 fields, `?` when not on every event):
`id`, `title`, `indicator`, `country`, `currency`, `category`, `period`, `referenceDate`, `referencePeriod?`, `date`, `importance`, `actual`, `previous`, `forecast`, `actualRaw`, `previousRaw`, `forecastRaw`, `unit?`, `scale?`, `source`, `source_url`, `ticker?`, `comment?`.

Types: figures are number or null; `date` and `referenceDate` are ISO datetimes; `id` is a digit string.

Labels seen:
- `importance`: `-1`, `0`, `1`.
- `category` (11): `bnd`, `bsnss`, `cnsm`, `enrg`, `gdp`, `gov`, `hse`, `lbr`, `mny`, `prce`, `trd`.
- `unit` includes `$`, `%`; `scale` includes `B`, `K`, `M` (from the 2-day US sample only).
- `source`: 132 publisher names in the one sampled week, written freely (`Bureau of Labour Statistics`, `Bur Census` and `Census Bureau` all occur).

Coverage, from one unfiltered week (2026-10-26 to 2026-11-02, 504 events):
- 63 `country` codes, 47 `currency` codes; ISO two-letter codes plus `EU`. The README says ~130 countries over time (line 980); a single week shows only those that publish.
- History: an event answered for 2010-01-04 (US). The first date was not searched.
- Future: a range a year ahead held no event; how far ahead the calendar reaches was not measured.
- A cap of 2000 events an answer is recorded in the app (`src/SurveyHeadlines.cpp:108`, README 1292), not tested here.

**`www.tradingview.com`:** the app fetches no page there. The name only appears as the `Origin` value (`src/CalendarData.cpp:99`, `src/IndicatorData.cpp:100`, `src/SurveyHeadlines.cpp:115`, `src/SourcesPage.cpp:1488`). `robots.txt` was the only call. It publishes a sitemap, and disallows `/widgetembed/`, `/embed-widget/`, `/options/chain/` and more, and `/ideas/*`, `/chart/*` for a list of AI crawlers.

## How to call it

```
GET https://economic-calendar.tradingview.com/events?from=2026-11-02T00:00:00.000Z&to=2026-11-04T00:00:00.000Z&countries=US
Origin: https://www.tradingview.com
```

- `from`, `to`: ISO datetimes. `countries`: comma list; leave it out for all.
- **No `Origin` → 403**, an HTML page.
- `robots.txt`: `Disallow: /`, `Allow: /events`. The one endpoint is explicitly open to robots.
- No rate-limit header seen. Answers were small: 26 KB for two days of one country, 400 KB for a week of all.

**Terms:** TradingView's policies, section 3, restrict this data strongly: market data "licensed for exclusive display-only use"; "we do not permit commercial usage of any of our services or APIs"; "creating products or services based on TradingView content" prohibited; Data Provider agreements "strictly forbid the sublicensing, assigning, transferring, selling, loaning, or any distribution of TradingView content"; consequences include "the blocking of the user or visitor". No clause names scraping of the site itself. Weigh this before relying on it further: the app keeps this data in a cache on disk (README 742).

## What the app reads

- `tradingview_calendar`: 22 countries (`src/CalendarData.cpp:61`), Monday of last week to the Monday after next, every 10 minutes while shown. Keeps `date`, `country`, `title`, `period`, `importance`, `actual`, `forecast`, `previous`, `unit`, `scale`, `source`, `source_url`. `src/CalendarData.cpp:81`, `:107`; row `src/SourcesPage.cpp:403`.
- `surveys`: three windows over the last ten weeks, filtered by title to 16 publishers. `src/SurveyHeadlines.cpp:105`, `:124`; row `src/SourcesPage.cpp:767`.
- Economy › Indicators: all countries, week by week, keeps `indicator`, `date`, `country`, `period` for next releases. `src/IndicatorData.cpp:91`, `:113`.

## Not read yet

- `ticker`: a TradingView symbol for the indicator, on part of the events.
- `comment`: a text about the indicator, on part of the events.
- `category`: could group or filter the Calendar page without title matching.
- `actualRaw`, `previousRaw`, `forecastRaw`, `currency`, `referenceDate`, `referencePeriod`.

## Calls made

Document read through a fetch tool: `https://www.tradingview.com/policies/`.

- `GET https://economic-calendar.tradingview.com/robots.txt` → 200, 41 bytes
- `GET https://www.tradingview.com/robots.txt` → 200, 1153 bytes
- `GET https://economic-calendar.tradingview.com/events?from=2026-11-03T00:00:00.000Z&to=2026-11-04T00:00:00.000Z&countries=DE` → 200, 15 bytes
- `GET https://economic-calendar.tradingview.com/events?from=2026-11-03T00:00:00.000Z&to=2026-11-04T00:00:00.000Z&countries=DE` → 403, 146 bytes
- `GET https://economic-calendar.tradingview.com/events?from=2026-11-02T00:00:00.000Z&to=2026-11-04T00:00:00.000Z&countries=US` → 200, 25832 bytes
- `GET https://economic-calendar.tradingview.com/events?from=2026-10-26T00:00:00.000Z&to=2026-11-02T00:00:00.000Z` → 200, 399351 bytes
- `GET https://economic-calendar.tradingview.com/events?from=2010-01-04T00:00:00.000Z&to=2010-01-05T00:00:00.000Z&countries=US` → 200, 3973 bytes
- `GET https://economic-calendar.tradingview.com/events?from=2027-10-01T00:00:00.000Z&to=2027-10-02T00:00:00.000Z&countries=US` → 200, 15 bytes
