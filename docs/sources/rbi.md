---
id: rbi
name: Reserve Bank of India
rows: []
catalogue: ["Reserve Bank of India"]
access: scraped web page
probed: 2026-11-05
confidence: refused
---

# Reserve Bank of India

India's central bank. Its Database on Indian Economy (DBIE) is a JavaScript application whose gateway answered "Bad Request" to a plain call, so its report list couldn't be read. The catalogue's
"DBIE (CSV), no key" holds for a browser only. **39 of its external-sector tables can be read openly through MoSPI's server**.

## Contents
Observed:
- `www.rbi.org.in/Scripts/Statistics.aspx` lists publications by frequency, among them: Household Financial Savings, External Debt, International Investment Position, Industrial Outlook Survey, Private
  Corporate Business, Basic Statistical Returns (BSR) 1 and 2, Order Books, Inventories and Capacity Utilisation Survey, Deposits and Credit of Scheduled Commercial Banks, Services and Infrastructure Outlook
  Survey, Sources of Variation in Reserves, ATMs by state, Survey of Professional Forecasters, Foreign Exchange Reserves.
- `data.rbi.org.in/DBIE/` is the application (Angular, `main.js` of 15 MB). Its gateways: `/CIMS_Gateway_DBIE/GATEWAY/SERVICES`, `_FRBS`, `_RBIDATA`, `_REPORTS`, `_SAARC`, `_RSA`, `_LOGIN`. Service names in its
  code: `dbie_getAllDBIEReports`, `dbie_getPopularReports`, `dbie_foreignExchangeReserves`, `dbie_getBSRStateWiseData`, `dbie_getEBRApiSubscriptionData`, `REPORTS_getAllFilters`, `REPORTS_getWSSReport`, and more.
  The names `getEBRApiSubscriptionData` and `getEBRApiSubscriptionElements` suggest an API subscription exists; the page says nothing further.

Through MoSPI's `RBI` data set (`docs/sources/mospi.md`, codes in `catalog/mospi.tsv`): balance of payments overall, quarterly and annual (Rupees, USD), invisibles, key components; direction of foreign trade;
exports and imports of major commodities; exports to principal countries; external debt; foreign exchange reserves, monthly, annual; NRI deposits; exchange rates vs SDR, USD, GBP, EUR, JPY; monthly average for 155 currencies; month
highs and lows; forward premia; sale and purchase of USD by the RBI; turnover in the foreign exchange market.

## How to call it
- `GET https://data.rbi.org.in/DBIE/` answers 200. `POST /CIMS_Gateway_DBIE/GATEWAY/SERVICES/dbie_getAllDBIEReports` with `{}` answers 200 `{"header":{"errorMessage":"Bad Request","errorCode":"400"}}` in HTML entities. The app's code
  holds a lot of encryption calls, its home disables copy and paste, so the request format seems meant for the application alone. No further attempt.
- `www.rbi.org.in/robots.txt` answers 418 "You are not authorized to view this page". `Scripts/Statistics.aspx` answers 200.

## What the app reads
Nothing: no row in `src/SourcesPage.cpp`. India's policy rate comes from the BIS.

## Not read yet
The MoSPI route first. The Bulletin's and DBIE's money, credit and yields tables stay out of reach of a program unless a subscription exists.

## Calls made
- `GET https://data.rbi.org.in/DBIE/` → 200, 51 KB
- `GET https://data.rbi.org.in/DBIE/main.3d843aa6360772f1.js` → 200, 15 MB
- `GET https://data.rbi.org.in/robots.txt` → 404
- `POST https://data.rbi.org.in/CIMS_Gateway_DBIE/GATEWAY/SERVICES/dbie_getAllDBIEReports` → 200, 201 B, "Bad Request"
- `GET https://www.rbi.org.in/robots.txt` → 418, 627 B
- `GET https://www.rbi.org.in/Scripts/Statistics.aspx` → 200, 85 KB
