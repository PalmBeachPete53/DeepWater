---
id: bls
name: U.S. Bureau of Labor Statistics
rows: [bls]
catalogue: ["U.S. Bureau of Labor Statistics", "Bureau of Labor Statistics"]
access: free, no key                  # v1 25 requests a day; v2 with a free key
probed: 2026-11-05
confidence: refused                   # the API's daily quota was spent for this address; www.bls.gov and download.bls.gov 403
---

# U.S. Bureau of Labor Statistics

US prices, jobs, wages and productivity. Nothing could be read on the probe date: the survey list needs an API call and
the API refused, the website refuses this client.

## Contents
**No call confirmed any of this.** What follows is from the app's own code and from BLS's public documentation as
remembered, and it must be checked before use.
- Series ids start with a two-letter survey code. As documented: `CU` CPI urban, `CW` CPI wage earners, `AP` average prices,
  `WP` PPI, `EI` import and export prices, `CE` payrolls national, `SM` payrolls state and metro, `LN` household survey,
  `LA` local unemployment, `JT` JOLTS, `CI` employment cost index, `PR` productivity, `OE` occupational wages, `EN` QCEW, `CX`
  consumer expenditure, `TU` time use.
- Series the app calls show the shape: `CUUR0000SA0`, `CUUR0000SA0L1E`, `LNS14000000`, `CES0000000001`, `CES0500000003`.
- An answer has `Results.series[].data[]` with `year`, `period` (`M01`...`M12`, `M13` the year's average), `value`
  (`src/KeyIndicators.cpp:267`).

FRED republishes BLS releases: see [`catalog/fred_releases.tsv`](catalog/fred_releases.tsv) and the FRED card.

## How to call it
- Survey list: `https://api.bls.gov/publicAPI/v2/surveys`. Today: `REQUEST_NOT_PROCESSED`, the daily threshold reached.
- Data: `https://api.bls.gov/publicAPI/v1/timeseries/data/LNS14000000` (v1, the app's call).
- `www.bls.gov` (`robots.txt`, `/data/`, `/help/`, `/developers/`, `/bls/`) and `download.bls.gov/pub/time.series/` all answer
  403 Access Denied to `Deepwater/1.0`. Not retried under another identity.
- `catalog.data.gov/api/3/action/package_search` answers 404: no catalogue to look through instead.

## What the app reads
`bls` (Map): v1, one request per series, the last three years: `CUUR0000SA0`, `CUUR0000SA0L1E`, `LNS14000000`,
`CES0000000001`, `CES0500000003`. Keeps `year`, `period`, `value`; year-on-year computed. `src/KeyIndicators.cpp:253`.

The app's quota is the same one: five series a day of the 25 this address gets. On a day like this one, the app's `bls` row
would get the same refusal once the quota is spent. Who spent it is not known.

## Not read yet
Unknown from a call. A free v2 key would allow more series per request. Repeat the survey call on a day the quota is intact
(one request) and rewrite this card.

## Calls made
- `GET https://api.bls.gov/publicAPI/v2/surveys` → 200, 224 bytes, body `REQUEST_NOT_PROCESSED` (daily threshold)
- `GET https://www.bls.gov/robots.txt` → 403, 1,325 bytes
- `GET https://download.bls.gov/pub/time.series/` → 403, 1,325 bytes
- `GET https://www.bls.gov/data/` → 403, 1,325 bytes
- `GET https://www.bls.gov/help/hlpforma.htm` → 403, 1,325 bytes
- `GET https://www.bls.gov/developers/api_faqs.htm` → 403, 1,325 bytes
- `GET https://www.bls.gov/bls/api_features.htm` → 403, 1,325 bytes
- `GET https://catalog.data.gov/api/3/action/package_search?fq=organization:bureau-of-labor-statistics&rows=5` → 404, 36 bytes
- `GET https://catalog.data.gov/api/3/action/package_search?fq=organization:bls-gov&rows=5` → 404, 36 bytes
- `GET https://catalog.data.gov/api/3/action/package_search?fq=organization:dol-gov&rows=5` → 404, 36 bytes
- `GET https://catalog.data.gov/api/3/action/package_search?fq=organization:department-of-labor&rows=5` → 404, 36 bytes
- `GET https://catalog.data.gov/api/3/action/package_search?q=%22Bureau+of+Labor+Statistics%22&rows=3` → 404, 36 bytes
