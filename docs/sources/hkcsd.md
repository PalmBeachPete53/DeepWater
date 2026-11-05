---
id: hkcsd
name: Census and Statistics Department, Hong Kong
rows: []
catalogue: ["Census and Statistics Department, Hong Kong"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# Census and Statistics Department, Hong Kong

Hong Kong's official statistics: prices, GDP, labour, trade, retail, population, and the rest, table by table. A JSON web API per table, listed on the government's
data portal.

## Contents
**984 C&SD packages on data.gov.hk**, 7,046 resources (CSV 2,494, XLSX 2,334, JSON 1,924, XML 192, XLS 54, API 45, RSS 3), complete in
[`catalog/hkcsd.tsv`](catalog/hkcsd.tsv): table id (642 packages have one), name, title, update frequency, groups, resources. C&SD's own count isn't given. Hong Kong only,
some by district.

- Groups: population 281, commerce and industry 233, employment and labour 161, finance 99, IT and broadcasting 63, miscellaneous 52, transport 27, health 19, others.
- Update frequency: as needed 261, annual 257, quarterly 221, one-off 115, monthly 111, half-yearly 13.
- Titles: Consumer Price 12, Unemployment 77, Gross Domestic 36, Retail 20, Trade 61. Also Census, Census geography (94).

Tables opened, with their fields:

| Table | Holds | Freq | First to last | Rows | Dimensions |
|---|---|---|---|---|---|
| `510-60001` | Consumer Price Indices | M, Y | 1974-07 to 2026-09 | 7,932 | `sv` Index, month-to-month %, year-on-year % |
| `310-30001` | GDP % change and major components | Q, Y | 1961 to 2026 Q3 | 2,520 | `GDP_COMPONENT`, `sv` |
| `210-06101` | Labour force, unemployment, underemployment | M3M, Y | 1985 to 2026-09 | 13,500 | `SEX`, `sv` |
| `410-50001` | Merchandise trade totals | M, Y | 1952 to 2026-09 | 5,586 | `sv` |
| `620-67001` | Total retail sales | M, Y | 2004 to 2026-09 | 1,430 | `sv` |

Row fields: `freq`, `period`, `sv`, `svDesc`, `figure`, `sd_value`, plus a code and `...Desc` per dimension (`SEX`, `SEXDesc`).

## How to call it
- List: `GET https://data.gov.hk/en-data/api/3/action/package_search?fq=organization:hk-censtatd&rows=1000` (CKAN, 8.6 MB).
- Data: `GET https://www.censtatd.gov.hk/api/get.php?id=510-60001&lang=en&full_series=1`. Answer `{"header": {status, title, tablenote, source, count}, "dataSet": [...]}`.
- Web: `web_table.html?id=510-60001`, help `...&api_popup=1`; Excel: `...&full_series=1&download_excel=1`. A `param` query form also exists.
- Traps: `id` and `lang` alone answer 200 with `status.code` 1 "Parameter is not defined". `period` mixes `1975` and `197507`. `M3M` is a moving three months.

## What the app reads
Nothing: no row in `src/SourcesPage.cpp`. Hong Kong reaches the Map through IMF and BIS (HKG).

## Not read yet
- Countries › Hong Kong: the five tables above, and `210-06xxx` by sector, `410-500xx` by country, `620-670xx` by outlet.
- Supply chains: trade by commodity.

## Calls made
- `GET https://data.gov.hk/en-data/api/3/action/package_search?fq=organization:hk-censtatd&rows=1` → 200, 9.5 KB
- `GET https://data.gov.hk/en-data/api/3/action/package_search?fq=organization:hk-censtatd&rows=1000` → 200, 8.6 MB
- `GET https://www.censtatd.gov.hk/api/get.php?id=510-60001&lang=en` → 200, status "Parameter is not defined"
- `GET https://www.censtatd.gov.hk/api/get.php?id=510-60001&lang=en&param=N4KABGBEDGD2AmBTAhgFwJawHYC4oCMsAaKMAG2WSMggDckAnfASVQMrUzCe4HVgAXyA` → 200, 2.4 KB
- `GET https://www.censtatd.gov.hk/api/get.php?id=<table>&lang=en&full_series=1` for the five tables above → all 200, 292 KB to 3.4 MB
- `GET https://www.censtatd.gov.hk/en/web_table.html?id=510-60001` → 200, 19 KB; `...&api_popup=1` → 200, 19 KB
- `GET https://www.censtatd.gov.hk/en/page_8000.html` → 200, 3.6 KB
- `GET https://www.censtatd.gov.hk/js/api_host.js` → 200, 24 B
