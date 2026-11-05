---
id: treasury
name: U.S. Department of the Treasury
rows: [research_treasury]
catalogue: ["U.S. Department of the Treasury", "U.S. Treasury", "Financial Management Service, United States", "Financial Management Service, US Treasury"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# U.S. Department of the Treasury

Three separate hosts: the daily rate tables on `home.treasury.gov`, federal cash, debt and auctions in the Fiscal Data
API, and foreign holdings in the TIC files on `ticdata.treasury.gov`.

## Contents
**Fiscal Data** (Bureau of the Fiscal Service). Its metadata call answers 56 data sets, 188 tables, 3,210 columns, about
19 million rows. Tables with endpoint, frequency, first and last date, row and column counts:
[`catalog/treasury.tsv`](catalog/treasury.tsv); every column with label and type:
[`catalog/treasury_fields.tsv`](catalog/treasury_fields.tsv). The families that matter to a terminal:

| Data set | Tables | Rhythm | Range | Endpoint under `/services/api/fiscal_service/` |
|---|---|---|---|---|
| Daily Treasury Statement | 9 | daily | 2005-10-03 on | `v1/accounting/dts/operating_cash_balance`, `deposits_withdrawals_operating_cash`, `public_debt_transactions`, `debt_subject_to_limit`, `income_tax_refunds_issued`, ... |
| Debt to the Penny | 1 | daily | 1993-04-01 on | `v2/accounting/od/debt_to_penny` |
| Monthly Treasury Statement | 18 | monthly | 1980-10-31 on (most tables 2015-03-31) | `v1/accounting/mts/mts_table_1` ... `mts_table_9`, `mts_receipts_outlays_deficit_surplus`, ... |
| Monthly Statement of the Public Debt | 7 | monthly | 2001-01-31 on | `v1/debt/mspd/mspd_table_1` ... `mspd_table_5`, `_3_market`, `_3_nonmarket` |
| Securities Auctions | 2 | as needed | 1979-11-15 on | `v1/accounting/od/auctions_query` (114 columns), `treasury_securities_auctions_v2` (181 columns, from 2026) |
| Upcoming Auctions | 2 | as needed | 2024 on | `v1/accounting/od/upcoming_auctions`, `upcoming_auctions_v2` |
| Securities Buybacks | 2 | as needed | 2000 on | `v1/accounting/od/buybacks_operations`, `buybacks_security_details` |
| Average Interest Rates | 1 | monthly | 2001-01-31 on | `v2/accounting/od/avg_interest_rates` |
| Interest Expense | 1 | monthly | 2010-05-31 on | `v2/accounting/od/interest_expense` |
| Reporting Rates of Exchange | 2 | quarterly | 2001-03-31 on | `v1/accounting/od/rates_of_exchange` |
| TIPS and CPI Data | 4 | monthly | 1998 on | `v1/accounting/od/tips_cpi_data_summary`, `tips_cpi_data_detail` |
| U.S. Treasury-Owned Gold | 1 | monthly | | see the catalogue file |
| Treasury Bulletin | 13 | quarterly | | see the catalogue file |

Other data sets (savings bonds, federal investments, trust funds, SLGS, certified rates, receivables): in the catalogue.

**Daily rates** (`home.treasury.gov`): five types, CSV and XML, one file per year.
- `daily_treasury_yield_curve`: `1 Mo, 1.5 Month, 2 Mo, 3 Mo, 4 Mo, 6 Mo, 1 Yr, 2 Yr, 3 Yr, 5 Yr, 7 Yr, 10 Yr, 20 Yr, 30 Yr`.
  The 1990 file has fewer maturities (`3 Mo` to `30 Yr`, 9 columns) and starts 01/02/1990.
- `daily_treasury_bill_rates`: 4, 6, 8, 13, 17, 26 and 52 weeks, each with bank discount and coupon equivalent.
- `daily_treasury_long_term_rate`: `LT COMPOSITE (>10 Yrs)`, `TREASURY 20-Yr CMT`.
- `daily_treasury_real_yield_curve`: `5, 7, 10, 20, 30 YR`.
- `daily_treasury_real_long_term`: `LT Real Average (10> Yrs)`.
The XML feeds carry the same data under other names (`BC_10YEAR`, `TC_10YEAR`, `ROUND_B1_CLOSE_4WK_2`).

**TIC** (`ticdata.treasury.gov/resource-center/data-chart-center/tic/Documents/`), files sampled:
`slt_table5.html` / `.txt` (major foreign holders of Treasury securities, by country, by month), `slt1d_globl.csv`
(foreign holdings of US long-term securities: Treasury, agency, corporate, equity, by country), `mfhhis01.txt` (the same
holders' history, one block per year). The file list itself was not walked.

## How to call it
- Fiscal Data catalogue: `GET https://api.fiscaldata.treasury.gov/services/dtg/metadata/` (1.3 MB, the site's own call;
  not documented as public).
- Data: `https://api.fiscaldata.treasury.gov/services/api/fiscal_service/v2/accounting/od/debt_to_penny?sort=-record_date&page[size]=1`.
  Answers `{data, meta, links}`; `meta` holds `count, labels, dataTypes, dataFormats, total-count, total-pages`.
- Yield curve: `https://home.treasury.gov/resource-center/data-chart-center/interest-rates/daily-treasury-rates.csv/2026/all?type=daily_treasury_yield_curve&field_tdr_date_value=2026&page&_format=csv`;
  XML: `.../interest-rates/pages/xml?data=daily_treasury_yield_curve&field_tdr_date_value=2026`.
- Trap: the same CSV with `all` instead of a year (`.../daily-treasury-rates.csv/all/all?...&field_tdr_date_value=all`)
  answered 403 Access Denied five times in a row while the yearly files kept working. Ask year by year.
- `/resource-center/data-chart-center` and `/.../interest-rates` alone answer 404; only their subpages exist.

## What the app reads
`research_treasury`: `daily_treasury_yield_curve` for the current year, all maturities, for Overview (`src/OverviewPage.cpp:744`)
and Desk › Rates (`src/DeskViewsMacro.cpp:515`). URL at `src/ResearchData.cpp:1075`.

## Not read yet
- `debt_to_penny`, `operating_cash_balance` (the TGA), `public_debt_transactions`: daily liquidity and issuance for Economy
  or Desk.
- `auctions_query` and `upcoming_auctions`: bid-to-cover, tails, the coming calendar. Calendar or Desk › Rates.
- `avg_interest_rates`, `interest_expense`, `mts_table_*`: the federal deficit for Economy.
- `daily_treasury_real_yield_curve`: real yields beside the nominal curve on Overview, without FRED.
- `daily_treasury_bill_rates`: 6-week and 17-week bills that FRED may lag.
- TIC `slt_table5`: who holds Treasuries, for Countries.

## Calls made
- `GET https://fiscaldata.treasury.gov/robots.txt` → 200, 128 bytes
- `GET https://fiscaldata.treasury.gov/data.json` → 404, 200,000 bytes
- `GET https://api.fiscaldata.treasury.gov/services/dtg/metadata/` → 200, 1,344,204 bytes
- `GET https://fiscaldata.treasury.gov/api-documentation/` → 200, 553,599 bytes
- `GET https://fiscaldata.treasury.gov/datasets/` → 200, 555,728 bytes
- `GET https://home.treasury.gov/robots.txt` → 200, 71 bytes
- `GET https://home.treasury.gov/resource-center/data-chart-center/interest-rates/TextView?type=daily_treasury_yield_curve&field_tdr_date_value=2026` → 200, 1,357,829 bytes
- `GET https://home.treasury.gov/resource-center/data-chart-center` → 404, 95,919 bytes
- `GET https://home.treasury.gov/resource-center/data-chart-center/interest-rates` → 404, 95,934 bytes
- `GET https://home.treasury.gov/data/treasury-international-capital-tic-system` → 200, 78,206 bytes
- `GET https://ticdata.treasury.gov/resource-center/data-chart-center/tic/Documents/slt_table5.html` → 200, 14,175 bytes ×2
- `GET https://home.treasury.gov/resource-center/data-chart-center/interest-rates/daily-treasury-rates.csv/2026/all?type=daily_treasury_yield_curve&field_tdr_date_value=2026&page&_format=csv` → 200, 17,282 bytes
- `GET https://home.treasury.gov/resource-center/data-chart-center/interest-rates/daily-treasury-rates.csv/2026/all?type=daily_treasury_bill_rates&field_tdr_date_value=2026&page&_format=csv` → 200, 17,548 bytes ×2
- `GET https://home.treasury.gov/resource-center/data-chart-center/interest-rates/daily-treasury-rates.csv/2026/all?type=daily_treasury_long_term_rate&field_tdr_date_value=2026&page&_format=csv` → 200, 4,502 bytes
- `GET https://home.treasury.gov/resource-center/data-chart-center/interest-rates/daily-treasury-rates.csv/2026/all?type=daily_treasury_real_yield_curve&field_tdr_date_value=2026&page&_format=csv` → 200, 7,674 bytes
- `GET https://home.treasury.gov/resource-center/data-chart-center/interest-rates/daily-treasury-rates.csv/2026/all?type=daily_treasury_real_long_term&field_tdr_date_value=2026&page&_format=csv` → 200, 3,424 bytes
- `GET https://home.treasury.gov/resource-center/data-chart-center/interest-rates/pages/xml?data=daily_treasury_yield_curve&field_tdr_date_value=2026` → 200, 328,213 bytes
- `GET https://home.treasury.gov/resource-center/data-chart-center/interest-rates/pages/xml?data=daily_treasury_bill_rates&field_tdr_date_value=2026` → 200, 822,790 bytes
- `GET https://home.treasury.gov/resource-center/data-chart-center/interest-rates/pages/xml?data=daily_treasury_long_term_rate&field_tdr_date_value=2026` → 200, 526,854 bytes
- `GET https://home.treasury.gov/resource-center/data-chart-center/interest-rates/pages/xml?data=daily_treasury_real_yield_curve&field_tdr_date_value=2026` → 200, 234,102 bytes
- `GET https://home.treasury.gov/resource-center/data-chart-center/interest-rates/pages/xml?data=daily_treasury_real_long_term&field_tdr_date_value=2026` → 200, 170,727 bytes
- `GET https://home.treasury.gov/data/treasury-international-capital-tic-system` → 200, 78,211 bytes
- `GET https://ticdata.treasury.gov/resource-center/data-chart-center/tic/Documents/slt1d_globl.csv` → 200, 30,000 bytes
- `GET https://ticdata.treasury.gov/resource-center/data-chart-center/tic/Documents/mfhhis01.txt` → 200, 30,000 bytes
- `GET https://home.treasury.gov/resource-center/data-chart-center/interest-rates/daily-treasury-rates.csv/all/all?type=daily_treasury_yield_curve&field_tdr_date_value=all&page&_format=csv` → 403, 505 bytes
- `GET https://home.treasury.gov/resource-center/data-chart-center/interest-rates/daily-treasury-rates.csv/all/all?type=daily_treasury_bill_rates&field_tdr_date_value=all&page&_format=csv` → 403, 505 bytes
- `GET https://home.treasury.gov/resource-center/data-chart-center/interest-rates/daily-treasury-rates.csv/all/all?type=daily_treasury_long_term_rate&field_tdr_date_value=all&page&_format=csv` → 403, 505 bytes
- `GET https://home.treasury.gov/resource-center/data-chart-center/interest-rates/daily-treasury-rates.csv/all/all?type=daily_treasury_real_yield_curve&field_tdr_date_value=all&page&_format=csv` → 403, 505 bytes
- `GET https://home.treasury.gov/resource-center/data-chart-center/interest-rates/daily-treasury-rates.csv/all/all?type=daily_treasury_real_long_term&field_tdr_date_value=all&page&_format=csv` → 403, 505 bytes
- `GET https://home.treasury.gov/resource-center/data-chart-center/interest-rates/daily-treasury-rates.csv/1990/all?type=daily_treasury_yield_curve&field_tdr_date_value=1990&page&_format=csv` → 200, 14,069 bytes
- `GET https://api.fiscaldata.treasury.gov/services/api/fiscal_service/v2/accounting/od/debt_to_penny?sort=-record_date&page%5Bsize%5D=1` → 200, 1,791 bytes
