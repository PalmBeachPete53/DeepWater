---
id: boc
name: Bank of Canada
rows: [research_boc]
catalogue: ["Bank of Canada"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# Bank of Canada

Valet, the Bank's own API: policy and market rates, Government of Canada yields, exchange rates, the BCPI commodity
index, its balance sheet, chartered banks' balance sheets, monetary aggregates, its surveys, and the data behind every
chart of its publications.

## Contents
**2,616 groups and 16,380 series** (`/valet/lists/*`, 2026-11-05). The full lists are in
[`catalog/boc.tsv.gz`](catalog/boc.tsv.gz) (`kind`, `code`, `label`, `description`). Canada only, apart from the FX pairs,
USD-based tables and claims by region. Nearly every series is daily, weekly, monthly, quarterly or annual, in one unit
per series (`metadata.unit`, `metadata.frequency`, `dateFormat`).

Two different kinds of group share the namespace:
- **Publication charts** (about 2,300 groups, one per chart): `SAN_*` (690, Staff Analytical Notes), `BOS_*` (333),
  `MPR_*` (327, Monetary Policy Report), `CSCE_*` (221, consumer survey), `FSR_*` (154), `FSS_*` (100), `CES_*` (82),
  `SPEECH_*` (82), `SABA_*` (78), `FSHUB_*` (48), `AUC_*` (35, auctions), `FVI_*` (35), `TCFD_*` (27), `SDP_*`, `SWP-*`
  and so on. Short samples frozen at publication, not maintained series.
- **Banking and Financial Statistics and market tables** (structure of 66 of them in
  [`catalog/boc_groups.jsonl`](catalog/boc_groups.jsonl): series count, frequency, first and last period, labels).

| Family | Groups | Series | First to last |
|---|---|---|---|
| Rates | `CORRA`, `CORRA_COMPOUNDED_INDEX`, `MONEY_MARKET`, `TBILL_ALL`, `CHARTERED_BANK_INTEREST` | 14, 1, 7, 8, 10 | 1997-08 / 2020-06 / 1997-08 / 2000-01 / 1967-04 to 2026-11 |
| Bond yields | `bond_yields_all`, `bond_yields_benchmark`, `bond_yields_marketable` | 11, 7, 4 | 2001-01-02 to 2026-11-03, business daily |
| Exchange rates | `FX_RATES_DAILY`, `FX_RATES_MONTHLY`, `FX_RATES_ANNUAL`, `CEER_DAILY`, `CEER_MONTHLY_NOMINAL`, `CEER_MONTHLY_REAL` | 27, 27, 27, 4, 4, 4 | FX from 2017; CEER from 1999 |
| Commodities | `BCPI_WEEKLY`, `BCPI_MONTHLY`, `BCPI_ANNUAL`, `BCPI_WEIGHTS` | 7, 7, 7, 9 | 1972 to 2026 |
| Inflation | `CPI_MONTHLY`, `ATABLE_INFLATIOON_INDICATORS` (sic), `ATABLE_INFLATION_CONTROL_TARGET` | 9, 8, 3 | 1995/1996 to 2026 |
| Summary "A" tables | `ATABLE_MONETARY_AGGREGATES`, `ATABLE_MONETARY_CONDITIONS`, `ATABLE_POLICY_INSTRUMENT` | 3, 1, 2 | 1996/1999 to 2026 |
| Lending | `A4_FUNDS_MONTHLY`, `A4_RATES_MONTHLY` (and `_CONSUMER`, `_MORTGAGES`, `_VARIABLE`, `_EXTENDED`) | 51, 51 | 2013 to 2026-08 |
| The Bank's balance sheet | `B1_MONTHLY`, `B2_WEEKLY`, `SETTLEMENT_BALANCES`, `K1_ANNUAL`, `GOCCADDEP_WEEKLY` | 32, 24, 7, 16, 3 | 1935 / 1980 / 1999 / 1978 / 1980 to 2026 |
| Chartered banks | `C1_MONTHLY`, `C2_MONTHLY`, `C5_QUARTERLY`, `C6_QUARTERLY`, `C7_QUARTERLY`, `C9_MONTHLY`, `C10_QUARTERLY`, `K12_QUARTERLY`, `J2_QUARTERLY` | 20, 18, 325, 233, 86, 14, 354, 64, 37 | 1946 to 2026 |
| Money and credit | `E1_MONTHLY` (35), `E2_MONTHLY` (48, ends 2020-09) | | 1946, 1969 |
| Government debt | `F4F10_MONTHLY`, `K8_MONTHLY`, `G4_MONTHLY`, `G4_WEEKLY`, `G6_MONTHLY`, `G7_MONTHLY`, `DEBT_20xx`, `GOC_OUTSTANDING` | 129, 274, 20, 20, 17, 14 | 1946 to 2026 |
| Reserves and real bonds | `OFFICIAL_INT_RES`, `RRB_INDEX_RATIO` | 7, 10 | 1999, 1998 to 2026 |
| Surveys | `bos` (35), `SLOS` (9), `CES_*`, `PUBLICSURVEY_*` | | 1998-07, 1999-04, 2014-10 to 2026 |
| Forecasts in Staff Working Papers | `SWP-*` (GDP, CPI, rates, unemployment) | | not checked |
| Ended, history only | `C3`, `C4`, `C8`, `F2`, `K9` (to 2020), `F11` to `F14` (to 2017), `B3` (to 2022), `B4` (to 2018) | | |

The group behind each row above was read on the date in *Calls made*; the other groups were not opened.

## How to call it
- Base: `https://www.bankofcanada.ca/valet`, docs at `/valet/docs`. Lists: `/lists/groups/json`, `/lists/series/json`.
- One series's metadata: `/series/V39079/json`. One group's: `/groups/bond_yields_all/json`.
- Data: `/observations/V39079/json?recent=1`, `/observations/group/bond_yields_all/json?recent=1`, also `csv` and `xml`;
  several series with commas; `start_date`, `end_date`, `recent`, `recent_weeks`, `order_dir`.
- Answer: `{"terms", "seriesDetail": {code: {label, description, dimension, dateFormat, metadata}}, "observations":
  [{"d": "YYYY-MM-DD", code: {"v": "text"}}]}`. Values are strings.
- No key, no published quota. The terms (`/terms/`) forbid excessive request frequency and name Valet explicitly.
- Traps: group names mix case (`bond_yields_all`, `CEER`). `V41552796` is labelled "M2 (gross) (Seasonally adjusted)"
  but described "M3 (gross) - M2 (gross)". `GOC_OUTSTANDING` and `TERM_REPOS` answer with series but no observations.

## What the app reads

Checked on 5 November 2026: `V41552796` is M2, as its label says and as its size fits; its description is a grouping heading, so the app's use is right.
- `research_boc`, row at `src/SourcesPage.cpp:542`, probe `V39079`.
- Countries › Canada, `src/CountryData.cpp:1666`: `resources/country_indicators.json` provider `boc`: `V39079` (target
  rate), `BD.CDN.10YR.DQ.YLD`, `V41552796` (M2), `V36610` (total assets), `V80691311` (prime), `V80691335` (5-year
  mortgage). Keeps `d` and `v`.
- Desk › Rates, `src/DeskViewsMacro.cpp:558`: `/observations/group/bond_yields_all/json?recent=1`, of which
  `src/desk/CentralBanks.cpp:230` keeps the 2Y, 5Y, 10Y and long benchmarks.
- Meeting dates, `src/DeskData.cpp:72`: the HTML page `core-functions/monetary-policy/key-interest-rate/`, not Valet.

## Not read yet
- `FX_RATES_DAILY` (27 pairs, official CAD fixings) and `CEER_DAILY`: Forex, Desk.
- `CORRA` and `CORRA_COMPOUNDED_INDEX`, `TBILL_ALL`, `MONEY_MARKET`: Desk › Rates, the short end of the Canadian curve.
- `BCPI_WEEKLY` (commodity index by energy, metals, forestry, agriculture, fish): Commodities.
- `bos` and `SLOS` (business and lending surveys), `CSCE_*` (consumer expectations): Countries › Canada.
- `B2_WEEKLY` weekly balance sheet, `OFFICIAL_INT_RES`, `E1_MONTHLY` aggregates, `C10_QUARTERLY` banks' claims by region.

## Calls made
- `GET https://www.bankofcanada.ca/valet/lists/groups/json` → 200, 1.3 MB
- `GET https://www.bankofcanada.ca/valet/lists/series/json` → 200, 3.7 MB
- `GET https://www.bankofcanada.ca/valet/groups/bond_yields_all/json` → 200, 5 KB
- `GET https://www.bankofcanada.ca/valet/series/V39079/json` → 200, 495 B
- `GET https://www.bankofcanada.ca/valet/observations/V39079/json?recent=1` → 200, 792 B
- `GET https://www.bankofcanada.ca/valet/docs` → 200, 3 KB (a JavaScript page)
- `GET https://www.bankofcanada.ca/terms/` → 200, 145 KB
- `GET https://www.bankofcanada.ca/valet/observations/group/<code>/json` for the 66 codes of `catalog/boc_groups.jsonl`
  → all 200
- `GET https://www.bankofcanada.ca/robots.txt` → 200, 120 B (Valet allowed)
