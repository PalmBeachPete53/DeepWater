---
id: boe
name: Bank of England
rows: [research_boe]
catalogue: ["Bank of England"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# Bank of England

The Statistical Interactive Database (IADB): UK money, lending, bank balance sheets, deposits, capital issuance,
derivatives, wholesale rates, exchange rates, yields, household rates; plus yield curve workbooks and Bankstats tables.

## Contents
The IADB has no machine catalogue. Its structure comes from its browsing pages (`/boeapps/database/`):

- **Sections** (`BankStats.asp`): A Money and lending; B Monetary financial institutions' balance sheets, income and
  expenditure; C Further analyses of deposits and lending; D Public sector debt and money markets operations; E Capital
  issuance; F Financial derivatives; I Interest and exchange rates. Their table titles at `levels=3` were read: for I,
  Wholesale interest and discount rates, Spot exchange rates, Exchange rate indices, US dollar forward premium/discount
  rates, sterling against US dollar forward rates, Gold price, Yields, Quoted household interest rates, Effective Interest
  Rates; for A, M0, notes and coin, reserves balances, growth and components of M4 and M4 lending, M3 estimate.
- **Categories** (`CategoryIndex.asp`, A to Z): 1,592 entries in `catalog/boe_categories.tsv`: financial instruments
  (`CategId=6`, 998), countries and country groups (9, 288), category 5 (215) and category 4 (91). The page's links name
  sector, instrument and country as the three browse axes.
- **A series**: a code of up to 8 letters (`IUDSOIA`, `XUDLUSS`) and one long title carrying frequency, instrument, unit
  and seasonal adjustment (`FromShowColumns.asp?SearchText=<codes>`). Help: a request takes up to 250 codes; dates
  `DD/MON/YYYY`. Series counts per section were not derived.
- **Yield curves** (`/statistics/yield-curves`): `latest-yield-curve-data.zip` holds 4 workbooks, GLC Inflation, GLC
  Nominal, GLC Real and OIS, "daily data current month"; each has the sheets `info`, `1. fwds, short end`, `2. fwd curve`,
  `3. spot, short end`, `4. spot curve`. The same page links history zips: `glcnominalddata`, `glcrealddata`,
  `glcinflationddata`, `blcnomddata`, `oisddata` (daily), `glcnominalmonthedata`, `glcrealmonthedata`,
  `glcinflationmonthedata`, `blcnominalmonthedata`, `oismonthedata` (month end).
- **Bankstats**: `/statistics/tables` links 9,095 database URLs and one archive `bankstats-latest-tables.zip` (not opened).
- Countries: United Kingdom as subject, 288 country and group entries as counterparties.

## How to call it
- Browsing: `https://www.bankofengland.co.uk/boeapps/database/FromShowColumns.asp?Travel=NIx&SearchText=IUMBV34,LPMAUYN`
  answers with titles only.
- **robots.txt disallows `/boeapps/database/_iadb-FromShowColumns.asp`, `/boeapps/database/ShowChart.asp` and
  `/boeapps/iadb`.** The Help page nevertheless documents `_iadb-fromshowcolumns.asp?csv.x=yes|excel97.x=yes|html.x=yes`
  as the "automatic download" with parameters `Datefrom`, `Dateto`, `SeriesCodes`, `UsingCodes=Y`, `VPD`, `VFD`, and the
  app calls it in lower case. This inventory did not call it, so its CSV is known from the Help page and the app's code.
- Trap in the app's comments: a start before 1963 redirects to an error page served with a 200
  (`src/CountryData.cpp:1755`).
- The bare `index.asp` redirects to `ErrorPage.asp`; it needs `SectionRequired`.
- `/statistics/details` did not answer within 45 s.

## What the app reads
- `research_boe` (`src/SourcesPage.cpp:529`), probe `IUDBEDR`.
- Countries › United Kingdom: `IUMBV34` mortgage rate, `LPMAUYN` M4, `LPMVTVX` approvals
  (`resources/country_indicators.json:834`, `:933`, `:935`), loader `src/CountryData.cpp:1754`.
- Desk › Rates: `IUDSNZC`, `IUDMNZC`, `IUDLNZC` (`src/DeskViewsMacro.cpp:535`, parser `src/desk/CentralBanks.cpp:212`).
- OIS: `latest-yield-curve-data.zip`, workbook "OIS daily", sheet 2 (`src/DeskData.cpp:116`, `src/desk/CentralBanks.cpp:319`).
- Meeting dates: `/monetary-policy/upcoming-mpc-dates` (`src/DeskData.cpp:69`, `src/desk/CentralBanks.cpp:59`); minutes URL
  `src/desk/CentralBanks.cpp:188`.

## Not read yet
- Daily SONIA `IUDSOIA` and Bank Rate `IUDBEDR` for Desk › Rates; spot rates `XUDL*` (`XUDLUSS`, `XUDLERS`, `XUDLSKS`,
  `XUDLADS`); gilt curves from `glcnominalddata`, real and inflation curves for a break-even view; Effective Interest Rates
  and Quoted household rates tables for Countries › United Kingdom; Asset Purchase Facility series (`YWDB87Q` family).

## Calls made
Made on 2026-11-05, sequentially per host with a pause, `User-Agent: Deepwater/1.0`. 20 calls.

- `GET https://www.bankofengland.co.uk/robots.txt` → 200, 357 B
- `GET https://www.bankofengland.co.uk/boeapps/database/` → 200, 25.1 KB
- `GET https://www.bankofengland.co.uk/boeapps/database/index.asp` → 200, 18.7 KB
- `GET https://www.bankofengland.co.uk/boeapps/database/Help.asp` → 200, 68.9 KB
- `GET https://www.bankofengland.co.uk/boeapps/database/CategoryIndex.asp?Travel=NIxAZx&CategId=allcats&CategName=Combined%20A%20to%20Z` → 200, 572.2 KB
- `GET https://www.bankofengland.co.uk/boeapps/database/BankStats.asp?Travel=NIx` → 200, 22.2 KB
- `GET https://www.bankofengland.co.uk/boeapps/database/index.asp?first=yes&SectionRequired=I&HideNums=-1&ExtraInfo=true&Travel=NIx` → 200, 30.2 KB
- `GET https://www.bankofengland.co.uk/statistics/tables` → 200, 2.6 MB
- `GET https://www.bankofengland.co.uk/statistics/yield-curves` → 200, 109.9 KB
- `GET https://www.bankofengland.co.uk/boeapps/database/index.asp?first=…&SectionRequired=…&HideNums=…&ExtraInfo=…&Travel=…&levels=…` × 7 → 200: 7, 282.5 KB in all; first `https://www.bankofengland.co.uk/boeapps/database/index.asp?first=yes&SectionRequired=A&HideNums=-1&ExtraInfo=true&Travel=NIxSTx&levels=3`
- `GET https://www.bankofengland.co.uk/-/media/boe/files/statistics/yield-curves/latest-yield-curve-data.zip` → 200, 220.7 KB
- `GET https://www.bankofengland.co.uk/statistics/details` → ERR, 0 B
- `GET https://www.bankofengland.co.uk/boeapps/database/FromShowColumns.asp?Travel=&TopTenSeries=Y` → 200, 33.5 KB
- `GET https://www.bankofengland.co.uk/boeapps/database/FromShowColumns.asp?Travel=NIx&SearchText=IUMBV34,LPMAUYN,LPMVTVX,IUDSNZC,IUDMNZC,IUDLNZC` → 200, 29.9 KB
