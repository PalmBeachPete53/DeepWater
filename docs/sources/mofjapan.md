---
id: mofjapan
name: Ministry of Finance, Japan
rows: [research_mof]
catalogue: ["Ministry of Finance, Japan"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# Ministry of Finance, Japan

No API: CSV and Excel files linked from statistics pages. Government bond yields, auctions, debt, reserves, intervention,
international securities flows, balance of payments, budget and tax revenue. The catalogue's `api` value "web" is accurate.

## Contents
The statistics page (`english/statistics/index.html`) has these families, with their format as found on each page opened.

| Family | Page | Files |
|---|---|---|
| **JGB yields** | `policy/jgbs/reference/interest_rate/index.htm` | `jgbcme.csv` (current month), `historical/jgbcme_all.csv` (1974-09-24 on). Columns `Date,1Y,2Y,3Y,4Y,5Y,6Y,7Y,8Y,9Y,10Y,15Y,20Y,25Y,30Y,40Y`, unit %, `-` when absent; daily |
| JGB auctions | `policy/jgbs/auction/past_auction_results/index.html` | `Auction_Results_for_JGBs.xls`, `Auction_Results_for_T-bills.xls`, `e-ryudousei_historical_data.xls` |
| Central government debt | `policy/jgbs/reference/gbb/index.htm` | `suii.xls`, and pages by year |
| STRIPS, others | `policy/jgbs/reference/jgbstat/index.htm`, `.../Others/index.html` | not opened |
| International reserves | `policy/international_policy/reference/official_reserve_assets/index.htm` | `historical.csv` (on the Japanese path), monthly pages by year |
| **Foreign exchange intervention** | `policy/international_policy/reference/feio/index.html` | `foreign_exchange_intervention_operations.csv`, quarterly and monthly releases |
| **International transactions in securities** | `policy/international_policy/reference/itn_transactions_in_securities/index.htm` | `week.csv`, `montha1.csv`, `montha2.csv`, `monthb1.csv` to `monthb4.csv`, `si.csv` (on the Japanese path) |
| Balance of payments | `policy/international_policy/reference/balance_of_payments/index.htm` | HTML pages: BoP, regional, direct investment, portfolio |
| Investment position, offshore accounts, flows to developing countries | `policy/international_policy/reference/iip/`, `offshore/`, `financial_flows_to_developing_countries/` | not opened |
| Budget and tax | `policy/budget/statistics/index.html`, `policy/tax_policy/taxes_and_stamp_revenues/`, `policy/exchequer/receipts_payments/`, `policy/filp/flf_balance/` | pages, IMF SDDS |
| Corporate statistics | `pri/reference/ssc/`, `pri/reference/bos/` | Financial Statements Statistics, Business Outlook Survey; not opened |
| Trade statistics | `customs.go.jp/toukei/info/index_e.htm` | linked, another site; not opened |

Country: Japan. Files carry their own headers, in English for the English path. Some CSVs live under `/policy/...`, without
`/english/`, and may be in Japanese.

## How to call it
- `GET https://www.mof.go.jp/english/policy/jgbs/reference/interest_rate/jgbcme.csv`, no key. Two header lines first; the
  title line says the month, the file holds about a month.
- Trap: `/english/policy/international_policy/reference/statistics/` and `/english/policy/customs_tariff/trade_statistics/`
  answer 404 with a full HTML page.

## What the app reads
`research_mof`, row at `src/SourcesPage.cpp:654`; Desk › Rates, `src/DeskViewsMacro.cpp:541` (`desk::parseMofYields`),
`jgbcme.csv`, the last day's yields.

## Not read yet
- `historical/jgbcme_all.csv`: the whole curve since 1974, for Desk › Rates history.
- `foreign_exchange_intervention_operations.csv`: dated interventions, for Forex and Calendar.
- `week.csv`: weekly foreign and Japanese flows in securities, for Desk.
- `Auction_Results_for_JGBs.xls`: auction history.

## Calls made
- `GET https://www.mof.go.jp/english/policy/jgbs/reference/interest_rate/jgbcme.csv` → 200, 447 B
- `GET https://www.mof.go.jp/english/policy/jgbs/reference/interest_rate/historical/jgbcme_all.csv` → 200, 1.2 MB
- `GET https://www.mof.go.jp/english/policy/jgbs/reference/interest_rate/index.htm` → 200, 16 KB
- `GET https://www.mof.go.jp/english/` → 200, 31 KB
- `GET https://www.mof.go.jp/english/statistics/index.html` → 200, 19 KB
- `GET https://www.mof.go.jp/english/policy/jgbs/index.html` → 200, 22 KB
- `GET https://www.mof.go.jp/english/policy/international_policy/index.html` → 200, 23 KB
- `GET https://www.mof.go.jp/english/policy/international_policy/reference/official_reserve_assets/index.htm` → 200, 22 KB
- `GET https://www.mof.go.jp/english/policy/international_policy/reference/feio/index.html` → 200, 17 KB
- `GET https://www.mof.go.jp/english/policy/international_policy/reference/balance_of_payments/index.htm` → 200, 21 KB
- `GET https://www.mof.go.jp/english/policy/international_policy/reference/itn_transactions_in_securities/index.htm` → 200, 22 KB
- `GET https://www.mof.go.jp/english/policy/jgbs/auction/past_auction_results/index.html` → 200, 17 KB
- `GET https://www.mof.go.jp/english/policy/jgbs/reference/gbb/index.htm` → 200, 18 KB
- `GET https://www.mof.go.jp/english/policy/budget/statistics/index.html` → 200, 16 KB
- `GET https://www.mof.go.jp/english/policy/international_policy/reference/statistics/index.htm` → 404
- `GET https://www.mof.go.jp/english/policy/customs_tariff/trade_statistics/index.htm` → 404
