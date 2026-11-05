---
id: wgc
name: World Gold Council
rows: []
catalogue: ["World Gold Council"]
access: free account
probed: 2026-11-05
confidence: called
---

# World Gold Council

Goldhub: the gold market's prices, flows, demand and central bank holdings, published as pages with charts and Excel files. Some files download openly; the dashboards ask for a free registration.

## Contents
Goldhub Data lists 15 data sets, with last updates seen on 2026-11-05:

| Category | Data set | Page under `/goldhub/data/` | What the page says | Updated |
|---|---|---|---|---|
| Price and premiums | Gold spot prices | `gold-prices` | averages monthly, quarterly, annual, since 1978, major currencies | 4 Nov 2026 |
| | Local gold price premium/discount | `gold-premium` | India and China against the international price | 2 Nov 2026 |
| | Gold futures prices | `gold-futures-curves` | several exchanges and future dates | 4 Nov 2026 |
| Performance | Returns | `gold-returns` | monthly, gold and other assets, several currencies | 31 Oct 2026 |
| | Volatility | `gold-price-volatility` | annualised on the LBMA price | 31 Oct 2026 |
| | Correlations | `gold-correlation` | gold against major assets | 5 Nov 2026 |
| | Trading volumes | `gold-trading-volumes` | by venue | 4 Nov 2026 |
| Demand and supply | Historical demand and supply | `gold-demand-by-country` | demand by sector and country; supply by mine, recycling, hedging | 3 Nov 2026 |
| | Gold ETFs, holdings and flows | `gold-etfs-holdings-and-flows` | over 100 ETFs, tonnes and USD flows, by region and fund | 5 Nov 2026 |
| | Global mine production | `gold-production-by-country` | by country | 9 Jun 2026 |
| | Production costs | `aisc-gold` | all-in sustaining cost, cost curve | 6 Oct 2026 |
| | Futures positioning | `gold-open-interest` | open interest, nine exchanges | 27 Oct 2026 |
| | Above-ground stock | `how-much-gold` | series since 2010, below-ground estimate | 3 Nov 2026 |
| Central banks | Gold Reserves by Country | `gold-reserves-by-country` | from IMF IFS; official holdings and share of reserves | 4 Nov 2026 |
| | Central Bank Gold Reserves Survey | `/goldhub/research/central-banks-survey` | annual survey | not shown |

Files seen by name, not downloaded:
- `World_official_gold_holdings_as_of_Nov2026_IFS.xlsx`, `Changes_latest_as_of_Nov2026_IFS.xlsx`, `CBGA_sales.xlsx`, `Quarterly_gold_and_FX_Reserves_Q3_2026.xlsx` (series since 2000).
- `Gold_price_averages_in_a range_of_currencies_since_1978.xlsx`.
- `ETF_Flows_2026-11-03.xlsx`.

## How to call it
- Pages open, and links of the form `https://www.gold.org/download/file/<id>/<name>.xlsx`, whose id and name change with each release: take them from the page.
- The reserves page says login is required to filter, choose date ranges and download from the dashboard; registration is free. No public API found.
- Traps: since 18 March 2025 historical LBMA Gold Price data is gone from the site, at the request of ICE Benchmark Administration, which licenses it. The terms say all rights stay with WGC or its providers (`/terms-and-conditions`): check them before use in the app.

## What the app reads
Nothing.

## Not read yet
- **Desk › Markets, gold's sheet:** ETF flows, futures positioning, volumes.
- **Economy › Countries:** gold reserves by country and share of reserves.

## Calls made
All on 2026-11-05 with `User-Agent: Deepwater/1.0`.
- `GET https://www.gold.org/robots.txt` → 200, 2,005 bytes (twice).
- `GET https://www.gold.org/goldhub/data` → 200, 89,549 bytes (twice).
- `GET https://www.gold.org/goldhub/data/gold-reserves-by-country` → 200, 68,742 bytes.
- `GET https://www.gold.org/goldhub/data/gold-prices` → 200, 73,378 bytes.
- `GET https://www.gold.org/goldhub/data/gold-etfs-holdings-and-flows` → 200, 65,985 bytes.
- `GET https://www.gold.org/terms-and-conditions` → 200, 84,925 bytes.
