---
id: bot
name: Bank of Thailand
rows: []
catalogue: ["Bank of Thailand"]
access: free account
probed: 2026-11-05
confidence: docs
---

# Bank of Thailand

Thailand's central bank: rates, exchange rates, monetary, external, fiscal and real-sector statistics. An API gateway with a public catalogue of products and an
account for the token, plus statistics pages of Excel files and table viewers. The token is missing, so the API's content comes from the portal's catalogue.

## Contents
**API products** (`portal.api.bot.or.th/portal/catalogue-products`, all "REST, Auth Token", OpenAPI files for logged-in users):
- Exchange Rates: `average-exchange-rate-thb--foreign-currency`, Weighted-average Interbank Exchange Rate THB/USD.
- Interest Rates: `spot-rate-usdthb`, Interbank Transaction Rates, Thai Baht Implied Interest Rates, External Interest Rates, BIBOR, Swap point Onshore, Policy Rate,
  Loan Interest Rates of Commercial Banks, Deposit Interest Rates for Individuals.
- **Statistics: `Stat Category`, `Observations`, `search-series`**: a general route to the published series, described as "Economic, monetary, public finance
  and financial statistics".
- Debt Securities Auction Result (Current).
- Others: Financial Institutions' Holidays.
- BOT License Check Public API.

**Statistics pages** (`www.bot.or.th/en/statistics/*.html`): Financial Market, Monetary, Financial Institutions, Payment, External Sector, Fiscal Sector, Real Sector,
Economic and Financial Index and Indicators, Regional (North, Northeast, South), Discontinued Series. From them:
- Table viewers (`app.bot.or.th/BTWS_STAT/statistics/BOTWEBSTAT.aspx?reportID=<n>&language=ENG`) by code: `EC_XT_047_S2` balance of payments (953), `EC_XT_051`
  financial account (648), `EC_XT_030` reserves (80), `EC_XT_032_S2` external debt (659), `EC_PF_009_S2` sources and uses of cash (822), `EC_PF_002_S2` government
  domestic debt (40), `EC_MB_004_S2` monetary aggregates (7), `EC_MB_003_S2` depository corporations (5), `EC_MB_039_S2` loans to households (985), `FM_RT_001_S2`
  interest rates in financial market (223, January 2005 to July 2025, monthly), `FM_FX_001_S3` exchange rates (123, 2002 to present), and 121, 122 for the older periods.
- Excel: production index (2 weights), shipment, inventory, inventory ratio, labour, productivity, capacity utilisation (`Industrial index/indexes_eng/month/*.xlsx`);
  Deposit Rate and Loan Rate (`content/dam/bot/documents/en/statistics/`, January 2017 to June 2023); and a daily rate file on `www.thaifxrates.net/ER_XLS_EN.xls`.
- The Real Sector page regroups data from the Ministry of Labour, the National Statistical Office, the Office of Agricultural Economics, the Rubber Authority,
  NESDB, the Office of Industrial Economics and the Trade Policy Office.

## How to call it
- Portal: `https://portal.api.bot.or.th/`. Gateway: `https://gateway.api.bot.or.th/`. Header `Authorization`, an account since 2025-09-17; old users have to sign up again.
- Example, unauthenticated: `GET https://gateway.api.bot.or.th/Stat-ReferenceRate/v2/DAILY_REF_RATE/?start_period=2026-10-01&end_period=2026-10-02` → 401
  "Authorization field missing". The path comes from memory of the older API, and this gateway didn't say whether it exists.
- `apiportal.bot.or.th`, the old address on the statistics page, no longer resolves.
- Traps: `BOTWEBSTAT.aspx` pages are 500 KB of HTML with form postbacks.

## What the app reads
Nothing: no row in `src/SourcesPage.cpp`.

## Not read yet
- Desk › Rates and Forex: the Policy Rate, BIBOR, THB exchange rates.
- Countries › Thailand: `Observations` with `search-series` could carry all the tables above, once a token exists.

## Calls made
- `GET https://www.bot.or.th/en/statistics.html` → 200, 106 KB
- `GET https://www.bot.or.th/en/statistics/<page>.html` for real-sector, exchange-rate, interest-rate, monetary-statistic, financial-market-statistics, external-sector,
  fiscal-sector, economic-and-financial-index-and-indicators → all 200, 73 to 105 KB
- `GET https://portal.api.bot.or.th/` → 200, 16 KB
- `GET https://portal.api.bot.or.th/portal/catalogue-products` → 200, 36 KB
- `GET https://portal.api.bot.or.th/terms` → 200, 20 KB
- `GET https://app.bot.or.th/BTWS_STAT/statistics/BOTWEBSTAT.aspx?reportID=223&language=ENG` → 200, 588 KB
- `GET https://gateway.api.bot.or.th/Stat-ReferenceRate/v2/DAILY_REF_RATE/?start_period=2026-10-01&end_period=2026-10-02` → 401
- `GET https://gateway.api.bot.or.th/robots.txt` → 404
- `GET https://apiportal.bot.or.th/bot/public/` → name not resolved
