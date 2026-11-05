---
id: mas
name: Monetary Authority of Singapore
rows: []
catalogue: ["Monetary Authority of Singapore", "Monetary Authority, Singapore"]
access: free account
probed: 2026-11-05
confidence: refused
---

# Monetary Authority of Singapore

Singapore's central bank. Its statistics page refused this inventory with a maintenance notice, and its API sits behind a portal
without a public list. The catalogue's "MAS API, no key" could not be confirmed.

## Contents
Nothing observed first-hand. Rates, exchange rates, money and banking figures of Singapore are also in SingStat's `Finance and Insurance`
subject (51 time series, among them `M700071` banks' interest rates, `M700051` exchange rates; see `docs/sources/singstat.md`). Which of them come
from MAS wasn't checked.

From general knowledge, not from a call: MAS publishes SORA, SGS yields, MAS bills, the Monthly Statistical Bulletin and Foreign Exchange
data through its API gateway (`eservices.mas.gov.sg/apimg-gw/...`), where a free subscription key may be asked. Treat it as a lead only.

## How to call it
- `www.mas.gov.sg/statistics` answered 200 with a page of 854 KB whose text says "Sorry, this service is currently unavailable", with a
  request ID. robots.txt itself sets `Crawl-delay: 2` and forbids nothing, so this looks like a WAF on the page, not a rule.
- `eservices.mas.gov.sg/statistics/` answered 200 with an empty body.
- `eservices.mas.gov.sg/apimg-portal/` is a 4 MB JavaScript application; its code names `/api-catalog`, loaded behind sign-in.
- `eservices.mas.gov.sg/apimg-gw/server/monthly_statistical_bulletin_non610mssql/exchange_rates_end_of_period_daily/views/exchange_rates_end_of_period_daily`
  answered 404 with a SOAP fault. The path comes from memory of older documentation.
- The old `api/action/datastore/search.json` answered a maintenance page.

## What the app reads
Nothing. No row in `src/SourcesPage.cpp`. Desk › Rates has no Singapore curve.

## Not read yet
Unknown. A browser and a subscription on the portal would tell.

## Calls made
- `GET https://www.mas.gov.sg/robots.txt` → 200, 74 B
- `GET https://www.mas.gov.sg/statistics` → 200, 854 KB, unavailable notice
- `GET https://eservices.mas.gov.sg/statistics/` → 200, empty
- `GET https://eservices.mas.gov.sg/apimg-portal/` → 200, 456 B
- `GET https://eservices.mas.gov.sg/apimg-portal/static/js/main.2acfbd14.js` → 200, 4.1 MB
- `GET https://eservices.mas.gov.sg/apimg-gw/server/monthly_statistical_bulletin_non610mssql/exchange_rates_end_of_period_daily/views/exchange_rates_end_of_period_daily` → 404, SOAP fault
- `GET https://eservices.mas.gov.sg/api/action/datastore/search.json?resource_id=95932927-c8bc-4e7a-b484-68a66a24edfe&limit=1` → 200, 7 KB, maintenance page
