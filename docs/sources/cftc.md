---
id: cftc
name: U.S. Commodity Futures Trading Commission
rows: [cftc, cftc_legacy, research_cftc]
catalogue: []                         # the CFTC is not in data_sources.json
access: free, no key
probed: 2026-11-05
confidence: called
---

# U.S. Commodity Futures Trading Commission

Weekly Commitments of Traders: who holds what in every US futures market, by trader class. Served by a Socrata portal,
so every data set answers the same query language and describes its own columns.

## Contents
`publicreporting.cftc.gov/api/views.json` lists 22 assets: 5 base tables, 7 filter views on them and 10 stories (help
pages). Every column of every table is in [`catalog/cftc.tsv`](catalog/cftc.tsv) (field, label, type;
1,363 rows). Weekly; last report at the probe 2026-10-27.

| id | name | kind | columns | rows | first report | contract codes |
|---|---|---|---|---|---|---|
| `srt6-5q2f` | Legacy_All | table | 130 | 572,265 | 1986-01-15 | 1,007 |
| `6dca-aqww` | Legacy - Futures Only | view | 133 | 291,531 | 1986-01-15 | 964 |
| `jun7-fc8e` | Legacy - Combined | view | 133 | 280,734 | 1995-03-21 | 954 |
| `rxbv-e226` | Disaggregated_All | table | 191 | 379,637 | 2006-06-13 | 692 |
| `72hh-3qpy` | Disaggregated - Futures Only | view | 194 | 186,744 | 2006-06-13 | 662 |
| `kh3c-gbw2` | Disaggregated - Combined | view | 194 | 192,893 | 2006-06-13 | 692 |
| `udgc-27he` | TFF_All | table | 87 | 94,505 | 2006-06-13 | 150 |
| `gpe5-46if` | TFF - Futures Only | view | 90 | 47,226 | 2006-06-13 | 149 |
| `yw9f-hn96` | TFF - Combined | view | 90 | 47,279 | 2006-06-13 | 150 |
| `j83k-qyrd` | CIT_All | table | 57 | 13,753 | 2006-01-03 | 13 |
| `4zgm-a668` | Supplemental - CIT | view | 60 | 13,753 | 2006-01-03 | 13 |
| `rj6x-va3z` | ProductHierarchy | table | 4 | 31,419 | no date | n/a |

Shared keys: `id`, `market_and_exchange_names`, `contract_market_name`, `report_date_as_yyyy_mm_dd`,
`yyyy_report_week_ww`, `cftc_contract_market_code`, `cftc_market_code`, `cftc_region_code`, `cftc_commodity_code`,
`commodity_name` (166 distinct in Legacy_All), `contract_units`. The `_All` tables add `futonly_or_combined` (`FutOnly`,
`Combined`).

Trader classes by report:
- **Legacy**: non-commercial (`noncomm_positions_*`), commercial (`comm_positions_*`), total reportable, non-reportable;
  plus changes, percent of open interest, trader counts (`traders_*`), concentration (`conc_gross_le_4_tdr_*`,
  `conc_gross_le_8_tdr_*`). Each block repeats for all, old and other crop (`_all`, `_old`, `_other`, or `_1`, `_2`).
- **Disaggregated** (agriculture, natural resources): producer/merchant (`prod_merc_*`), swap dealers (`swap_*`), managed
  money (`m_money_*`), other reportable.
- **TFF** (financials): dealers (`dealer_*`), asset managers (`asset_mgr_*`), leveraged funds (`lev_money_*`), other
  reportable.
- **CIT** (13 agricultural contracts): commercial and non-commercial without index traders, plus index traders.

`ProductHierarchy` fields: `contract_market_code`, `commodity_name`, `commodity_subgroup_name` (31 distinct, such as
GRAINS, BASE METALS, CURRENCY, DIGITAL ASSET, EMISSIONS, `Interest Rates - U.S. Treasury`), `commodity_group_name` (3:
AGRICULTURE, FINANCIAL INSTRUMENTS, NATURAL RESOURCES).

Outside the portal, `www.cftc.gov/MarketReports` links (index read, not their contents): Commitments of Traders
historical compressed and viewable files, Bank Participation Reports, Cotton On-Call, Swaps Reports, Cleared Margin
Reports, financial data for FCMs, Net Position Changes, Staff Reports.

## How to call it
- Base: `https://publicreporting.cftc.gov/resource/<id>.json` with SoQL (`$select`, `$where`, `$order`, `$limit`).
- Catalogue: `/api/views.json`, `/data.json` (DCAT, 15 data sets). One data set's columns: `/api/views/<id>.json`.
- Working example: `/resource/6dca-aqww.json?$select=count(*) as n,min(report_date_as_yyyy_mm_dd) as first`.
- No key. `robots.txt` asks `Crawl-delay: 1`. `api.us.socrata.com/api/catalog/v1?domains=publicreporting.cftc.gov` answers
  an empty list: the discovery API does not know this portal.
- Traps: numbers are strings; field names keep the source's typos (`noncomm_postions_spread_all`,
  `swap__positions_short_all`, CIT's `ncomm_postions_*`); contract codes contain `+`, which must be percent-encoded
  (`src/QuantData.cpp:403`). `commodity_group_name` is only in `ProductHierarchy`, join on the contract code.

## What the app reads
- `cftc` (Forex): `gpe5-46if`, keeps report date, contract code, `open_interest_all`, asset manager and leveraged fund
  long and short. `src/MacroData.cpp:441`.
- `cftc_legacy` (Quant): `6dca-aqww`, two years, 23 markets; report date, code, market name, `contract_units`, open
  interest, non-commercial, commercial, reportable and non-reportable positions, trader counts. `src/QuantData.cpp:404`.
- `research_cftc` (Quant › COT): `gpe5-46if`, all columns, one market, two years. `src/QuantCotView.cpp:396`.

## Not read yet
- Disaggregated (`72hh-3qpy`, `kh3c-gbw2`): managed money in oil, gold, grains. It is what Quant's COT lacks for
  commodities, where the app only shows the Legacy classes.
- Combined views (`jun7-fc8e`, `yw9f-hn96`): positions with options.
- CIT (`4zgm-a668`): index traders in 13 agricultural contracts.
- `conc_gross_le_4_tdr_*`: concentration of the largest traders.
- `rj6x-va3z`: a ready sector mapping for a positioning heatmap.

## Calls made
- `GET https://publicreporting.cftc.gov/robots.txt` → 200, 1,755 bytes
- `GET https://publicreporting.cftc.gov/data.json` → 200, 15,708 bytes
- `GET https://api.us.socrata.com/api/catalog/v1?domains=publicreporting.cftc.gov&limit=200` → 200, 129 bytes
- `GET https://publicreporting.cftc.gov/api/views.json?limit=200` → 200, 99,212 bytes
- `GET https://publicreporting.cftc.gov/api/views/4zgm-a668.json` → 200, 99,493 bytes
- `GET https://publicreporting.cftc.gov/resource/4zgm-a668.json?$select=count(*)%20as%20n,min(report_date_as_yyyy_mm_dd)%20as%20first,max(report_date_as_yyyy_mm_dd)%20as%20last` → 200, 83 bytes
- `GET https://publicreporting.cftc.gov/resource/4zgm-a668.json?$select=count(distinct%20cftc_contract_market_code)%20as%20n` → 200, 13 bytes
- `GET https://publicreporting.cftc.gov/api/views/yw9f-hn96.json` → 200, 152,720 bytes
- `GET https://publicreporting.cftc.gov/resource/yw9f-hn96.json?$select=count(*)%20as%20n,min(report_date_as_yyyy_mm_dd)%20as%20first,max(report_date_as_yyyy_mm_dd)%20as%20last` → 200, 83 bytes
- `GET https://publicreporting.cftc.gov/resource/yw9f-hn96.json?$select=count(distinct%20cftc_contract_market_code)%20as%20n` → 200, 14 bytes
- `GET https://publicreporting.cftc.gov/api/views/gpe5-46if.json` → 200, 152,773 bytes
- `GET https://publicreporting.cftc.gov/resource/gpe5-46if.json?$select=count(*)%20as%20n,min(report_date_as_yyyy_mm_dd)%20as%20first,max(report_date_as_yyyy_mm_dd)%20as%20last` → 200, 83 bytes
- `GET https://publicreporting.cftc.gov/resource/gpe5-46if.json?$select=count(distinct%20cftc_contract_market_code)%20as%20n` → 200, 14 bytes
- `GET https://publicreporting.cftc.gov/api/views/kh3c-gbw2.json` → 200, 329,693 bytes
- `GET https://publicreporting.cftc.gov/resource/kh3c-gbw2.json?$select=count(*)%20as%20n,min(report_date_as_yyyy_mm_dd)%20as%20first,max(report_date_as_yyyy_mm_dd)%20as%20last` → 200, 84 bytes
- `GET https://publicreporting.cftc.gov/resource/kh3c-gbw2.json?$select=count(distinct%20cftc_contract_market_code)%20as%20n` → 200, 14 bytes
- `GET https://publicreporting.cftc.gov/api/views/72hh-3qpy.json` → 200, 329,590 bytes
- `GET https://publicreporting.cftc.gov/resource/72hh-3qpy.json?$select=count(*)%20as%20n,min(report_date_as_yyyy_mm_dd)%20as%20first,max(report_date_as_yyyy_mm_dd)%20as%20last` → 200, 84 bytes
- `GET https://publicreporting.cftc.gov/resource/72hh-3qpy.json?$select=count(distinct%20cftc_contract_market_code)%20as%20n` → 200, 14 bytes
- `GET https://publicreporting.cftc.gov/api/views/jun7-fc8e.json` → 200, 226,891 bytes
- `GET https://publicreporting.cftc.gov/resource/jun7-fc8e.json?$select=count(*)%20as%20n,min(report_date_as_yyyy_mm_dd)%20as%20first,max(report_date_as_yyyy_mm_dd)%20as%20last` → 200, 84 bytes
- `GET https://publicreporting.cftc.gov/resource/jun7-fc8e.json?$select=count(distinct%20cftc_contract_market_code)%20as%20n` → 200, 14 bytes
- `GET https://publicreporting.cftc.gov/api/views/6dca-aqww.json` → 200, 227,035 bytes
- `GET https://publicreporting.cftc.gov/resource/6dca-aqww.json?$select=count(*)%20as%20n,min(report_date_as_yyyy_mm_dd)%20as%20first,max(report_date_as_yyyy_mm_dd)%20as%20last` → 200, 84 bytes
- `GET https://publicreporting.cftc.gov/resource/6dca-aqww.json?$select=count(distinct%20cftc_contract_market_code)%20as%20n` → 200, 14 bytes
- `GET https://publicreporting.cftc.gov/api/views/rj6x-va3z.json` → 200, 9,247 bytes
- `GET https://publicreporting.cftc.gov/resource/rj6x-va3z.json?$select=count(*)%20as%20n` → 200, 16 bytes
- `GET https://publicreporting.cftc.gov/api/views/srt6-5q2f.json` → 200, 219,127 bytes
- `GET https://publicreporting.cftc.gov/resource/srt6-5q2f.json?$select=count(*)%20as%20n,min(report_date_as_yyyy_mm_dd)%20as%20first,max(report_date_as_yyyy_mm_dd)%20as%20last` → 200, 84 bytes
- `GET https://publicreporting.cftc.gov/resource/srt6-5q2f.json?$select=count(distinct%20cftc_contract_market_code)%20as%20n` → 200, 15 bytes
- `GET https://publicreporting.cftc.gov/api/views/rxbv-e226.json` → 200, 319,923 bytes
- `GET https://publicreporting.cftc.gov/resource/rxbv-e226.json?$select=count(*)%20as%20n,min(report_date_as_yyyy_mm_dd)%20as%20first,max(report_date_as_yyyy_mm_dd)%20as%20last` → 200, 84 bytes
- `GET https://publicreporting.cftc.gov/resource/rxbv-e226.json?$select=count(distinct%20cftc_contract_market_code)%20as%20n` → 200, 14 bytes
- `GET https://publicreporting.cftc.gov/api/views/udgc-27he.json` → 200, 146,718 bytes
- `GET https://publicreporting.cftc.gov/resource/udgc-27he.json?$select=count(*)%20as%20n,min(report_date_as_yyyy_mm_dd)%20as%20first,max(report_date_as_yyyy_mm_dd)%20as%20last` → 200, 83 bytes
- `GET https://publicreporting.cftc.gov/resource/udgc-27he.json?$select=count(distinct%20cftc_contract_market_code)%20as%20n` → 200, 14 bytes
- `GET https://publicreporting.cftc.gov/api/views/j83k-qyrd.json` → 200, 94,594 bytes
- `GET https://publicreporting.cftc.gov/resource/j83k-qyrd.json?$select=count(*)%20as%20n,min(report_date_as_yyyy_mm_dd)%20as%20first,max(report_date_as_yyyy_mm_dd)%20as%20last` → 200, 83 bytes
- `GET https://publicreporting.cftc.gov/resource/j83k-qyrd.json?$select=count(distinct%20cftc_contract_market_code)%20as%20n` → 200, 13 bytes
- `GET https://publicreporting.cftc.gov/resource/rj6x-va3z.json?$limit=2` → 200, 265 bytes
- `GET https://publicreporting.cftc.gov/resource/srt6-5q2f.json?$select=distinct%20futonly_or_combined&$limit=2000` → 200, 72 bytes
- `GET https://publicreporting.cftc.gov/resource/srt6-5q2f.json?$select=distinct%20commodity_name&$limit=2000` → 200, 6,505 bytes
- `GET https://publicreporting.cftc.gov/resource/rxbv-e226.json?$select=distinct%20futonly_or_combined&$limit=2000` → 200, 72 bytes
- `GET https://publicreporting.cftc.gov/resource/udgc-27he.json?$select=distinct%20futonly_or_combined&$limit=2000` → 200, 72 bytes
- `GET https://www.cftc.gov/MarketReports/CommitmentsofTraders/index.htm` → 200, 85,685 bytes
- `GET https://www.cftc.gov/MarketReports/index.htm` → 200, 41,325 bytes
- `GET https://www.cftc.gov/robots.txt` → 200, 4,817 bytes
- `GET https://publicreporting.cftc.gov/resource/rj6x-va3z.json?$select=distinct%20commodity_group_name&$limit=3000` → 200, 137 bytes
- `GET https://publicreporting.cftc.gov/resource/rj6x-va3z.json?$select=distinct%20commodity_subgroup_name&$limit=3000` → 200, 1,500 bytes
- `GET https://publicreporting.cftc.gov/resource/rj6x-va3z.json?$select=distinct%20commodity_name&$limit=3000` → 200, 127,240 bytes
