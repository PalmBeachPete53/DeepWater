---
id: beac
name: Bank of Central African States (BEAC)
rows: []
catalogue: ["Bank of Central African States", "Bank of Central African States (BEAC)"]
access: scraped web page
probed: 2026-11-05
confidence: called
---

# BEAC

The central bank of CEMAC: Cameroon, the Central African Republic, Congo, Gabon, Equatorial Guinea, Chad. It publishes files by country on web pages: Excel and PDF, without an API.

## Contents
Eight pages under `/economie-stats/`; file links counted on each:

| Page | xls/xlsx | pdf | What the links are |
|---|---|---|---|
| `statistiques-monetaires` | 344 | 1 | monetary statistics by country and CEMAC, per period, and forecasts of them ("Prévisions des statistiques monétaires") |
| `statistiques-economiques` | 83 | 4 | basic statistics, government financial operations, GDP by sector, supply and use of resources, main indicators, by country |
| `statistiques-de-balance-paiements` | 178 | 7 | balance of payments by country and analytic quarterly |
| `statistiques-marche-monetaire` | 11 | 4 | interbank rates, interbank transactions, BEAC intervention rates and volumes, tender results, daily injections, lending rates |
| `statistiques-titres-publics` | 10 | 272 | government securities, yield curves per country |
| `statistiques-de-lemission-monetaire` | 7 | 1 | currency in circulation, bank deposits and withdrawals of notes |
| `statistiques-systemes-de-paiement` | 0 | 4 | payment systems, electronic money |
| `statistiques-dinclusion-financiere` | 0 | 14 | lending rate reports |

Each file is one period for one country, with names that change, e.g. `BULLETIN_CAMEROUN_mars-2026.xls`, `deofe_tchad-1.xlsx`, `debsbe_gabon-1.xlsx`. File contents were not opened.

## How to call it
- `https://www.beac.int/economie-stats/statistiques-monetaires/` and the others above, then the file links under `/wp-content/uploads/`. French.
- `robots.txt` blocks only `/wp-admin/`. Slow: 4 to 6 seconds per page in the catalogue's test.
- `/economie-stats/base-de-donnees/` gives 404; the earlier probe found a link named "Base de Données Économiques, Monétaires et Financières" that was not followed.

## What the app reads
Nothing.

## Not read yet
**Economy › Countries:** the six CEMAC countries, money and external accounts, only through parsing per-period files. **Desk › Rates:** the money market's intervention rates.

## Calls made
All on 2026-11-05 with `User-Agent: Deepwater/1.0`, base `https://www.beac.int`.
- `GET /robots.txt` → 200, 67 bytes.
- `GET /economie-stats/statistiques-monetaires/` → 200, 222,647 bytes (twice).
- `GET /economie-stats/statistiques-economiques/` → 200, 130,331 bytes (twice).
- `GET /economie-stats/statistiques-de-lemission-monetaire/` → 200, 104,616.
- `GET /economie-stats/statistiques-de-balance-paiements/` → 200, 172,661.
- `GET /economie-stats/statistiques-marche-monetaire/` → 200, 106,710.
- `GET /economie-stats/statistiques-systemes-de-paiement/` → 200, 102,060.
- `GET /economie-stats/statistiques-titres-publics/` → 200, 206,369.
- `GET /economie-stats/statistiques-dinclusion-financiere/` → 200, 106,596.
- `GET /economie-stats/base-de-donnees/` → 404, 96,127.
