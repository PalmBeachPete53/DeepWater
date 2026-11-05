---
id: sec
name: U.S. Securities and Exchange Commission (EDGAR)
rows: [research_sec]
catalogue: []                         # the SEC is not in data_sources.json
access: free, no key
probed: 2026-11-05
confidence: called                    # data.sec.gov and efts.sec.gov answered; www.sec.gov refused
---

# U.S. Securities and Exchange Commission (EDGAR)

Every filing of US-listed companies, and the figures tagged inside them. `data.sec.gov` and `efts.sec.gov` answered
`Deepwater/1.0`; `www.sec.gov` refused it, so its lists and documentation could not be read.

## Contents
- **Submissions** `data.sec.gov/submissions/CIK##########.json`. Company: `cik, entityType, sic, sicDescription, ownerOrg,
  insiderTransactionForOwnerExists, insiderTransactionForIssuerExists, name, tickers, exchanges, ein, lei, description,
  website, investorWebsite, category, fiscalYearEnd, stateOfIncorporation, stateOfIncorporationDescription, addresses,
  phone, flags, formerNames`. `filings.recent` is a table of columns: `accessionNumber, filingDate, reportDate,
  acceptanceDateTime, act, form, fileNumber, filmNumber, items, core_type, size, isXBRL, isInlineXBRL, isXBRLNumeric,
  primaryDocument, primaryDocDescription` (1,003 filings in the sample); older ones in `filings.files[]`, reaching 1994.
- **Company facts** `data.sec.gov/api/xbrl/companyfacts/CIK##########.json`: `facts.<taxonomy>.<tag> = {label, description,
  units.<unit>[]}`, each point `{end, val, accn, fy, fp, form, filed, frame}`. The sample (Apple) has `dei` (2 tags) and
  `us-gaap` (512 tags), units `USD`, `shares`, `pure`, `USD/shares`, `Year`, `Store`, forms `10-K`, `10-K/A`, `10-Q`, `8-K`,
  fiscal years from 2009. Foreign filers use `ifrs-full` (from the app's code).
- **Company concept** `.../companyconcept/CIK##########/us-gaap/<Tag>.json`: one tag's history for one company.
- **Frames** `.../frames/us-gaap/<Tag>/USD/CY2019Q1I.json`: one tag across all companies for one period. Fields `taxonomy,
  tag, ccp, uom, label, description, pts, data[{accn, cik, entityName, loc, end, val}]`. The sample has 3,390 companies.
- **Full-text search** `efts.sec.gov/LATEST/search-index?q=...`: hits `_source {ciks, period_ending, file_num, display_names,
  xsl, sequence, root_forms, file_date, biz_states, sics, form, adsh, film_num, biz_locations, file_type, file_description,
  inc_states, items}`, aggregations `entity_filter`, `sic_filter`, `biz_states_filter`, `form_filter`. Filters seen: `forms`,
  `dateRange=custom`, `startdt`, `enddt`. Total capped at `10000, gte`.
- **Entity lookup** `efts.sec.gov/LATEST/search-index?keysTyped=AAPL`: `_source {entity, entity_words, tickers, rank}`, `_id` = CIK.

Not reachable: `www.sec.gov/files/company_tickers.json`, the API documentation, `Archives/`, the bulk `companyfacts.zip`,
Financial Statement Data Sets. By documentation only, unverified today: these exist under `www.sec.gov`.

## How to call it
No key. The SEC asks automated clients for a contact in `User-Agent` and a limit of 10 requests a second (its public fair
access rule, from documentation). This inventory sent `Deepwater/1.0`, no e-mail, one call every 2 s.
- `www.sec.gov/*` answers 403 "Request Rate Threshold Exceeded" to it, `robots.txt` included. Not retried under another identity.
- `data.sec.gov/robots.txt` 404; `efts.sec.gov/robots.txt` 403.
- CIK is zero-padded to 10 digits in file names.

## What the app reads
`research_sec` (Research):
- `efts.sec.gov/LATEST/search-index?keysTyped=` for a ticker's CIK: `src/ResearchData.cpp:1352`.
- `companyfacts`, 18 statement items, each by priority of `us-gaap` or `ifrs-full` tag (`Revenues`,
  `RevenueFromContractWithCustomerExcludingAssessedTax`, `NetIncomeLoss`, ...): `src/ResearchData.cpp:206`, `:313`, `:1415`.
- The row's health probe calls `submissions/CIK0000320193.json`: `src/SourcesPage.cpp:440`.

## Not read yet
- `frames`: one figure for all companies at a date, for sector screens in Research.
- `submissions` fields `sic`, `sicDescription`, `formerNames`, and `recent.form`: a filings feed for News or a company page
  (8-K items, insider Form 4, 13F).
- Full-text search: which companies mention a word this quarter.

## Calls made
- `GET https://www.sec.gov/robots.txt` → 403, 1,923 bytes
- `GET https://data.sec.gov/submissions/CIK0000320193.json` → 200, 164,080 bytes
- `GET https://data.sec.gov/api/xbrl/companyfacts/CIK0000320193.json` → 200, 3,858,323 bytes
- `GET https://efts.sec.gov/LATEST/search-index?keysTyped=AAPL` → 200, 885 bytes
- `GET https://www.sec.gov/files/company_tickers.json` → 403, 1,924 bytes
- `GET https://www.sec.gov/edgar/sec-api-documentation` → 403, 1,924 bytes
- `GET https://data.sec.gov/api/xbrl/companyconcept/CIK0000320193/us-gaap/AccountsPayableCurrent.json` → 200, 19,748 bytes
- `GET https://data.sec.gov/api/xbrl/frames/us-gaap/AccountsPayableCurrent/USD/CY2019Q1I.json` → 200, 442,844 bytes
- `GET https://efts.sec.gov/LATEST/search-index?q=%22inflation%22&forms=10-K&dateRange=custom&startdt=2025-01-01&enddt=2025-12-31` → 200, 58,672 bytes
- `GET https://efts.sec.gov/LATEST/search-index?q=%22inflation%22` → 200, 61,774 bytes
- `GET https://data.sec.gov/robots.txt` → 404, 301 bytes
- `GET https://efts.sec.gov/robots.txt` → 403, 23 bytes
- `GET https://data.sec.gov/submissions/` → 404, 282 bytes
