---
id: boj
name: Bank of Japan
rows: [research_boj]
catalogue: ["Bank of Japan"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# Bank of Japan

The Time-Series Data Search with its JSON API: rates, money, prices, the Tankan, flow of funds, balance of payments, bank
balance sheets, BIS statistics for Japan.

## Contents
**50 databases, 326,502 series**, each with code, name, unit, frequency, category, first and last period. Summary per
database with its headings: [`catalog/boj_databases.jsonl`](catalog/boj_databases.jsonl). Series by series:
[`catalog/boj.tsv.gz`](catalog/boj.tsv.gz), the 45 databases under 20,000 series (34,024 rows, names cut at 140 characters). The
five largest, `CO`, `FF`, `BIS`, `PR01`, `PR03`, would make the file 83 MB, so they appear by category and frequency in
[`catalog/boj_large_categories.tsv`](catalog/boj_large_categories.tsv), and their list is one `getMetadata` call away.

| DB | Holds | Series | First to last | Frequencies |
|---|---|---|---|---|
| `IR01` | Basic discount rates | 2 | 1882-10 to 2026-11 | D, M |
| `IR02`, `IR03`, `IR04` | Deposit rates, time deposits, lending rates | 111, 112, 63 | 1985, 1985, 1976 to 2026 | M, W |
| `FM01` | Uncollateralized overnight call rate | 4 | 1998-01-05 to 2026-11-02 | D |
| `FM02` | Call and Tokyo repo rates | 52 | 1960-01 to 2026-11 | D, M |
| `FM03`, `FM04`, `FM05`, `FM06` | CDs, call market outstanding, bond issuance, government bond trading | 12, 195, 51, 15 | 1979, 1956, 1965, 1979 to 2026-09 | M |
| `FM07` | Government bond counter sales (ended) | 18 | 1991 to 2004 | M |
| `FM08` | Foreign exchange rates | 58 | 1971-01 to 2026-11 | D 22, M 36 |
| `FM09` | Effective exchange rate | 4 | 1970-01 to 2026-09 | M |
| `MD01`, `MD02`, `MD03`, `MD05` to `MD09` | Monetary base, money stock, monetary survey, currency, current accounts | 14, 154, 120, 20, 102, 5, 93, 128 | 1953 to 2026 | M |
| `MD04` | Former money stock (ended) | 34 | 1970 to 2003 | M |
| `MD10` to `MD14` | Deposits by depositor, loans, by prefecture, time deposits | 1,214, 787, 1,656, 36, 242 | 1953 to 2026 | M, Q |
| `LA01` to `LA05` | Loans by sector, by rate, commitments, Senior Loan Officer Survey | 3,599, 8, 111, 4, 12 | 1953 to 2026 | Q, M |
| `BS01`, `BS02` | BOJ accounts, bank balance sheets | 38, 1,395 | 1998, 1979 to 2026 | M |
| `PR01` | Corporate Goods Price Index, export, import | 31,254 | 1900-10 to 2026-09 | M |
| `PR02` | Services Producer Price Index | 2,822 | 1985 to 2026-09 | M, Q |
| `PR03` | Input-Output price index (**discontinued**) | 27,154 | 1975 to 2022-04 | M |
| `PR04` | Final and intermediate demand prices | 224 | 1970 to 2026-09 | M |
| `CO` | **Tankan** | 166,513 | 1962 Q2 to 2028 | Q, FY, H |
| `FF` | Flow of Funds (2008, 1993, 1968 SNA) | 33,887 | 1964 Q1 to 2026 Q2 | Q, FY |
| `BP01` | Balance of Payments BPM6, regional, IIP | 17,989 | 1985 to 2027 | M, Q, A |
| `BIS` | BIS locational banking statistics in Japan | 33,670 | 1990 to 2026 | Q |
| `DER` | Derivatives (FX and rates), notional | 2,238 | 1998 to 2026 | H |
| `PF01`, `PF02` | Treasury accounts, national government debt | 123, 16 | 1983, 1982 to 2026 | M |
| `OB01`, `OB02`, `PS01`, `PS02`, `OT` | Transactions with the government, collateral, payments, fails, aggregated sales (ended) | 48, 77, 9, 7, 2 | 2001 to 2026 | M |

`LAST_UPDATE` and `NOTES` are present in `getMetadata`; the headings give each database's hierarchy (`LAYER1`..`LAYER5`).

## How to call it
- Base: `https://www.stat-search.boj.or.jp/api/v1/`. No key.
- Catalogue: `GET getMetadata?format=json&lang=en&db=FM01`, one database at a time. The database list isn't an API call: it
  comes from `index_en.html`.
- Data: `GET getDataCode?format=json&lang=en&db=FM01&code=STRDCLUCON&startDate=202610`: `PARAMETER`, `NEXTPOSITION`,
  `RESULTSET` with `SERIES_CODE`, `NAME_OF_TIME_SERIES`, `UNIT`, `FREQUENCY`, `CATEGORY`, `LAST_UPDATE`, `VALUES`
  (`SURVEY_DATES`, `VALUES` in parallel). `startPosition` pages through `NEXTPOSITION`.
- Trap: an unknown database answers 400 `M181005E` (`TK`, `MA01`, `MA02`, though `MA02` and `TK9...` appear in the site's page
  script). Dates mean different things by frequency: `202603` is the third quarter in `QUARTERLY`, see
  `src/CountryData.cpp:1689`.
- Limits are in `info/api_manual_en.pdf` and `info/api_notice_en.pdf`, downloaded but unread here (no PDF tool).

## What the app reads
- `research_boj`, row at `src/SourcesPage.cpp:545`; Countries › Japan, `src/CountryData.cpp:1686`, keeps `SURVEY_DATES` and
  `VALUES`. Provider `boj` in `resources/country_indicators.json`: `PR01` `PRCG20_2200000000`, `_2400000000`, `_2600000000`;
  `FM01` `STRDCLUCON`; `MD01` `MABS1AN11`; `MD02` `MAM1NAM2M2MO`, `MAM1NAM3M3MO`; `BS01` `MABJMTA`; `MD11` `DLCLAEDBLTTO`;
  `IR04` `DLLR2CIDBNL1`; `BP01` `BPBP6JYNTB`, `BPBP6JYNCB`; `CO` `TK99F1000601GCQ01000`.
- Desk › Rates meeting dates, `src/DeskData.cpp:71`: the page `boj.or.jp/en/mopo/mpmsche_minu/index.htm`, not this API.

## Not read yet
- Forex: `FM08`, `FM09`. Desk › Rates: `FM02` (repo), `IR02`.
- Countries › Japan: `FF` (flow of funds), `LA05`, `PR02`, `PF02`, `BS02`.
- Desk: the other Tankan series of `CO`, including price and exchange-rate projections and the outlook items.
- Map trade: the regional BoP in `BP01`.

## Calls made
- `GET https://www.stat-search.boj.or.jp/index_en.html` → 200, 59 KB
- `GET https://www.stat-search.boj.or.jp/api/v1/getMetadata?format=json&lang=en&db=<db>` for the 50 databases → all 200
- Same call for `TK`, `MA01`, `MA02` → 400, `M181005E`
- `GET https://www.stat-search.boj.or.jp/api/v1/getDataCode?format=json&lang=en&db=FM01&code=STRDCLUCON&startDate=202610` → 200, 1 KB
- `GET https://www.stat-search.boj.or.jp/info/api_manual_en.pdf` → 200, 857 KB
- `GET https://www.stat-search.boj.or.jp/info/api_notice_en.pdf` → 200, 110 KB
- `GET https://www.stat-search.boj.or.jp/robots.txt` → 404
- `GET https://www.boj.or.jp/en/statistics/index.htm` → 200, 38 KB
- `GET https://www.boj.or.jp/en/statistics/outline/notice_2026/not260218a.htm` → 200, 37 KB
