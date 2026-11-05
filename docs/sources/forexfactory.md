---
id: forexfactory
name: Forex Factory (Fair Economy feed)
rows: [calendar]
catalogue: []
access: free, no key
probed: 2026-11-05
confidence: called
---

# Forex Factory (Fair Economy feed)

The current week's economic calendar of Forex Factory as a static file: event, currency, time, impact, forecast and
previous. It has no actual figure and no history.

## Contents

One data set in three formats, 115 events in the week called:

| File | Format | Fields |
|---|---|---|
| `ff_calendar_thisweek.json` | JSON array | `title`, `country`, `date` (ISO datetime), `impact`, `forecast`, `previous` |
| `ff_calendar_thisweek.xml` | XML `weeklyevents/event` | `title`, `country`, `date`, `time`, `impact`, `forecast`, `previous`, `url` |
| `ff_calendar_thisweek.csv` | CSV | `Title,Country,Date,Time,Impact,Forecast,Previous,URL` |

- `country` holds a currency code, 9 of them: `AUD`, `CAD`, `CHF`, `CNY`, `EUR`, `GBP`, `JPY`, `NZD`, `USD`.
- `impact`: `High`, `Medium`, `Low`, `Holiday`.
- `forecast`, `previous`: text, empty when there is none.
- **No `actual`** in any format. XML and CSV add a `url` to the event's page; JSON does not.
- History: none. `ff_calendar_nextweek.json` answers 404.

## How to call it

`GET https://nfs.faireconomy.media/ff_calendar_thisweek.json`. No key, no special header (`Deepwater/1.0` answered). Served by Cloudflare. `robots.txt`: `User-agent: *`, empty `Disallow`. No rate-limit header seen.

**Terms:** `https://www.forexfactory.com/terms-of-service` answered 403 to the fetch tool, so they were not read.

## What the app reads

- `calendar`: the JSON, cached 30 minutes; keeps all 6 fields. `src/quant/server/Providers.cpp:1706`; row `src/SourcesPage.cpp:841`. Status checks at `:1728`, `:1764`.

## Not read yet

- `url` of the XML and CSV: a link to each event's page, for the Charts › Calendar tab.

## Calls made

Document attempted through a fetch tool: `https://www.forexfactory.com/terms-of-service` → 403.

- `GET https://nfs.faireconomy.media/robots.txt` → 200, 25 bytes
- `GET https://nfs.faireconomy.media/ff_calendar_thisweek.json` → 200, 15650 bytes
- `GET https://nfs.faireconomy.media/ff_calendar_nextweek.json` → 404, 146 bytes
- `GET https://nfs.faireconomy.media/ff_calendar_thisweek.xml` → 200, 40070 bytes
- `GET https://nfs.faireconomy.media/ff_calendar_thisweek.csv` → 200, 14501 bytes
