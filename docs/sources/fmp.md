---
id: fmp
name: Financial Modeling Prep
rows: [fmp]
catalogue: []
access: free key
probed: 2026-11-05
confidence: docs   # no key was used, and the documentation site answered 403, so only the refusal of the API is first-hand; the families below come from a web search and the app's code
---

# Financial Modeling Prep

A commercial API of market and company data. The app uses only its news endpoints, and only when the user has saved a key.

## Contents
**What was verified:** the shape of the refusal (below) and the fields the app's code reads.

**What was not:** the documentation site `site.financialmodelingprep.com/developer/docs` answered HTTP 403 ("Request blocked", CloudFront) to `Deepwater/1.0` on the three pages tried. No other identity was tried, so endpoint counts and plan contents are unknown here.

A web search on 2026-11-05, reading other sites and FMP's quickstart page as summarised by the search engine, not the site itself, gave: base `https://financialmodelingprep.com/stable/`, "over 100 endpoints", and these paths in community client libraries:
- Quotes and profile: `quote`, `profile`.
- Statements: `income-statement`, `cash-flow-statement`, balance sheet, `ratios`, key metrics, growth, `enterprise-values`, `financial-reports-dates`, as-reported and segmentation variants.

Treat that list as a lead and check it with a key.

News endpoints the app calls, from its code: `news/general-latest`, `news/stock-latest`, `news/crypto-latest`, `news/forex-latest`, and with a ticker `news/stock`, `news/crypto`, `news/forex`.

Item fields the app reads: `title`, `text`, `url`, `site`, `publisher`, `symbol`, `publishedDate` ("yyyy-MM-dd HH:mm:ss", UTC).

## How to call it
- `GET https://financialmodelingprep.com/stable/news/general-latest?page=0&limit=1&apikey=<key>`: the key is a query parameter.
- With no key: HTTP 401, JSON `{"Error Message": "Invalid API KEY. ..."}`. The quickstart, per the search summary, says 403 for a bad key; the app treats both, and 402 for an endpoint outside the plan (`src/NewsData.cpp:398-401`).
- The search summary reports 429 for rate limits. Plan limits were not read.
- Terms were not read: the site refused.

## What the app reads
Row `fmp`, `src/SourcesPage.cpp:817`. Key check `src/SourcesPage.cpp:1446`.
- `Wire::fetchFmp` `src/NewsData.cpp:374`. With a ticker in Stocks, Crypto or Forex: `/stable/news/<path>?symbols=<ticker>&limit=60` (`:379`). Otherwise `/stable/news/<path>-latest?page=0&limit=60` (`:382`). Path from `fmpPath`, `:56`.
- Fields kept `src/NewsData.cpp:414-426`. Falls back to the free feeds on any error (`:446`).

## Not read yet
Unverified without a key: statements and ratios for Research, and quotes as a fallback for Charts. Research reads those elsewhere today, so check whether FMP adds anything first.

## Calls made
- `GET https://financialmodelingprep.com/robots.txt` → 403, 919 bytes.
- `GET https://financialmodelingprep.com/stable/news/general-latest?page=0&limit=1` → 401, 184 bytes.
- `GET https://site.financialmodelingprep.com/robots.txt` → 403, 919 bytes.
- `GET https://site.financialmodelingprep.com/developer/docs` → 403, 919 bytes.
- `GET https://site.financialmodelingprep.com/developer/docs/stable` → 403, 919 bytes.
- `GET https://site.financialmodelingprep.com/developer/docs/pricing` → 403, 919 bytes.

WebSearch 2026-11-05, query on FMP stable endpoint categories. Pages it used: [FMP quickstart](https://site.financialmodelingprep.com/developer/docs/quickstart), [fmp-rs checklist](https://docs.rs/crate/fmp-rs/latest/source/ENDPOINT_CHECKLIST.md), [fmp-rs client](https://docs.rs/crate/fmp-rs/latest/source/src/client.rs), [rusty-fmp endpoints](https://docs.rs/crate/rusty-fmp/0.5.2/source/src/endpoint.rs), [dltHub](https://dlthub.com/context/source/financial-modeling-prep). These pages were not opened directly.
