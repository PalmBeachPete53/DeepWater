---
id: fedboard
name: Board of Governors of the Federal Reserve System
rows: [research_fed]
catalogue: ["Federal Reserve"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# Board of Governors of the Federal Reserve System

Rates, money, bank balance sheets, the Fed's own balance sheet, industrial production, consumer credit, financial
accounts. The Data Download Program serves each release as a full SDMX ZIP.

## Contents
**Data Download Program**: 18 releases, 54,353 series. Each series with name, frequency code, unit code, first and last
date, description: [`catalog/fedboard_ddp_series.tsv.gz`](catalog/fedboard_ddp_series.tsv.gz). Each release's dimensions, code
lists (first 60 codes) and preformatted packages: [`catalog/fedboard_ddp.jsonl`](catalog/fedboard_ddp.jsonl).

| `rel` | Release | Series | First | Last | Dimensions besides FREQ, UNIT, CURRENCY |
|---|---|---|---|---|---|
| `H15` | Selected Interest Rates | 263 | 1919-01-31 | 2026-11-03 | INSTRUMENT (25), MATURITY (18) |
| `H41` | Factors Affecting Reserve Balances | 1,332 | 2002-12-18 | 2026-10-28 | CATEGORY (6), SUBCATEGORY (27), COMPONENT (153), DISTRIBUTION (21), SERIESTYPE (6) |
| `H8` | Assets and Liabilities of Commercial Banks | 1,411 | 1947-01-31 | 2026-10-21 | BG, bank group (5), ITEM (41), SA, CATEGORY, H8_UNITS |
| `H6` | Money Stock Measures | 165 | 1959-01-31 | 2026-10-05 | ADJUSTED |
| `H10` | Foreign Exchange Rates (G.5 / H.10) | 87 | 1971-01-04 | 2026-10-31 | FX (29: 23 currencies and 6 dollar indexes) |
| `G17` | Industrial Production and Capacity Utilization | 2,681 | 1919-01-31 | 2026-09-30 | SERIES_CODE (352), SA; code lists by market and industry group |
| `G19` | Consumer Credit | 118 | 1943-01-31 | 2026-08-31 | CREDTYP, HOLDER, DATAREP, ISSUE, TERMS, SA |
| `G20` | Finance Companies | 156 | 1943-01-31 | 2026-08-31 | ASSET, ITEM, SERIES, CATEGORY, LOANTERM, SA |
| `Z1` | Financial Accounts | 41,716 | 1945-12-31 | 2026-06-30 | SERIES_PREFIX (10), SERIES_SECTOR (77), SERIES_INSTRUMENT (603), SERIES_TYPE (5); quarterly and annual |
| `CP` | Commercial Paper | 202 | 1954-05-05 | 2026-11-03 | CP_TERM, CP_TYPE, CP_MAT_RANGE, CP_VOL_TYPE, CP_CAT, CP_SA |
| `CHGDEL` | Charge-off and Delinquency Rates | 264 | 1985-03-31 | 2026-06-30 | CHGDEL, COMPONENT, LOANTYPE (11), SIZE, SA |
| `SLOOS` | Senior Loan Officer Survey | 638 | 1966-09-30 | 2026-12-31 | LOANTYPE (25), MEASURE, TERMS (15), DIRECTION, REASON (17), BANKSIZE, PANEL, IMPORTANCE |
| `SCOOS` | Senior Credit Officer Survey | 2,796 | 2010-06-30 | 2026-09-30 | QUEST_GRP (5) |
| `PRATES` | Policy Rates | 3 | 2008-10-09 | 2026-11-05 | INT_RATES_PAID |
| `DSR` | Household Debt Service Ratios | 3 | 2005-03-31 | 2026-06-30 | DEBT, TYPE |
| `FOR` | Debt Service and Financial Obligations (discontinued) | 4 | 1980-03-31 | 2024-03-31 | DEBT, TYPE |
| `H3` | Aggregate Reserves (ended 2020) | 89 | 1959-01-31 | 2020-09-09 | RESERVES, RESERVES_DETAIL |
| `E2` | Terms of Business Lending (ended 2017) | 2,425 | 1997-06-30 | 2017-06-30 | PANEL, COLUMN, MATURITY, RISK, SIZE, ... |

H.15 instruments include Federal funds, commercial paper, CDs, Eurodollars, bank prime, discount window, bills, constant
maturities (nominal and inflation-indexed), swaps, Moody's Aaa and Baa, state and local bonds, conventional mortgages;
maturities from Overnight to 30-year. Z.1 prefixes: transactions SAAR and NSA, level NSA and SA, market value, revaluation,
other volume, seasonal factor, growth rate, index. Frequency codes are numeric (`8` daily, `9` business day, `19`, `20`,
`21` weekly by day, `129` monthly, `162` quarterly, `203` annual).

**Elsewhere on the site**
- FOMC Summary of Economic Projections: HTML table, one page per meeting, `monetarypolicy/fomcprojtabl<yyyymmdd>.htm` (47
  links on the calendar page; the latest is 20260916). Table 1: change in real GDP, unemployment rate, PCE inflation,
  core PCE inflation, federal funds rate; median, central tendency, range; four years and longer run, with the previous
  projection. Table 2: error ranges. Figures: the dot plot and risks. No CSV linked.
- FOMC calendar: `monetarypolicy/fomccalendars.htm`, with meetings, statements, minutes.
- RSS: 23 feeds under `/feeds/` (`press_monetary.xml`, `press_all.xml`, `speeches.xml`, `testimony.xml`, one per governor,
  `feds.xml`, `ifdp.xml`, `feds_notes.xml`, ...).
- `data.htm` also lists, no DDP: Assets and Liabilities of US Branches of Foreign Banks, New Security Issues, International
  Summary Statistics, Securities Holdings and Transactions, Mortgage Debt Outstanding, Survey of Consumer Finances
  (`econres/scfindex.htm`: `scf2025.zip`, historical tables), Yield Curve Models and Data, Micro and Macro Data Collections
  (MDRM), National Information Center, Financial Accounts Guide (`apps/fof/`).
- H.4.1 and H.8 keep their own ZIPs: `/releases/h41/data/FRB_h41_xml.zip`, `/releases/h8/data/FRB_h8_xml.zip`.

## How to call it
- A package: `https://www.federalreserve.gov/datadownload/Choose.aspx?rel=H15` (lists packages), full release as SDMX
  `https://www.federalreserve.gov/datadownload/Output.aspx?rel=H15&filetype=zip` (`H15_data.xml`, `H15_struct.xml`).
- No key. `robots.txt` is a 404 page. Sizes from 22 KB (`DSR`) to 37 MB (`Z1`).
- Trap: `Choose.aspx` lists Z.1's 435 packages; series names hold `%` and dots (`STFBQD%STFBAIL_XEOP_XS1A2_MA.Q`), encode them.
- Trap: `/releases/` redirects by META to `/econresdata/releases/statisticsdata.htm`, which answered a near-empty page: use `data.htm`.

## What the app reads
`research_fed`: the FOMC calendar page for meeting dates (`src/DeskData.cpp:67`, `:95`), the feed `press_monetary.xml` for
Desk › Notes (`src/DeskViewsMacro.cpp:232`) and the News list (`src/DeskViewsTech.cpp:581`).
The SEP in Desk › Forecasts comes through FRED (`FEDTARMD`, `GDPC1MD`, `UNRATEMD`, `PCECTPIMD`, `src/DeskViewsMacro.cpp:1360`).
Most DDP series reach the app through FRED, under other ids (`WALCL`, `DFF`, `TOTALSL`, `TCU`).

## Not read yet
- `H15`, `H41`, `H6`, `H8`, `G17`, `G19` directly: free of FRED's key.
- `Z1`, 41,716 series: household and corporate balance sheets, the deepest macro set here; nothing in the app reads it.
- `SLOOS`: lending standards, a leading series for Economy.
- `CHGDEL`, `DSR`: credit stress for Economy.
- `/feeds/speeches.xml`: the speakers for Calendar or News.

## Calls made
- `GET https://www.federalreserve.gov/robots.txt` → 404, 81,196 bytes
- `GET https://www.federalreserve.gov/data.htm` → 200, 102,661 bytes
- `GET https://www.federalreserve.gov/datadownload/` → 200, 86,537 bytes
- `GET https://www.federalreserve.gov/releases/` → 200, 1,431 bytes
- `GET https://www.federalreserve.gov/datadownload/Choose.aspx?rel=H15` → 200, 84,177 bytes
- `GET https://www.federalreserve.gov/datadownload/Build.aspx?rel=H15` → 200, 90,400 bytes
- `GET https://www.federalreserve.gov/feeds/feeds.htm` → 200, 90,205 bytes
- `GET https://www.federalreserve.gov/monetarypolicy/fomccalendars.htm` → 200, 166,350 bytes
- `GET https://www.federalreserve.gov/monetarypolicy/fomc_projectionstabl20250917.htm` → 404, 82,310 bytes
- `GET https://www.federalreserve.gov/econresdata/releases/statisticsdata.htm` → 200, 1,457 bytes
- `GET https://www.federalreserve.gov/monetarypolicy/fomcprojtabl20260916.htm` → 200, 241,407 bytes ×2
- `GET https://www.federalreserve.gov/data/yield-curve-models.htm` → 200, 79,509 bytes
- `GET https://www.federalreserve.gov/releases/h41/` → 200, 78,354 bytes
- `GET https://www.federalreserve.gov/releases/h8/current/default.htm` → 200, 664,543 bytes
- `GET https://www.federalreserve.gov/apps/fof/` → 200, 83,577 bytes
- `GET https://www.federalreserve.gov/data/intlsumm/default.htm` → 200, 113,761 bytes
- `GET https://www.federalreserve.gov/data/scfindex.htm` → 404, 82,310 bytes
- `GET https://www.federalreserve.gov/econres/scfindex.htm` → 200, 105,871 bytes
- `GET https://www.federalreserve.gov/datadownload/Choose.aspx?rel=<each of the 18 releases>` → 200 ×18
- `GET https://www.federalreserve.gov/datadownload/Output.aspx?rel=<each of the 18 releases>&filetype=zip` → 200 ×18
