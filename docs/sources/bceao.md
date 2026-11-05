---
id: bceao
name: Central Bank of West African States (BCEAO)
rows: []
catalogue: ["Central Bank of West African States (BCEAO)", "BCEAO", "Central Bank of West African States", "Banque Centrale des Etats de l’Afrique de l’Ouest (BCEAO)"]
access: scraped web page
probed: 2026-11-05
confidence: called
---

# BCEAO

The central bank of the eight West African Monetary Union countries. Its statistics live in a data warehouse, **EDEN** (*Entrepôt de Données Economiques et fiNancières*), reached through web forms. No API found.

## Contents
EDEN's home page, at `edenpub.bceao.int`, lists its predefined tables and a multi-criteria search:

- Areas: Bénin, Burkina Faso, Côte d'Ivoire, Guinée-Bissau, Mali, Niger, Sénégal, Togo, UEMOA.
- Frequencies offered: daily, weekly, monthly, quarterly, half-yearly, annual, none.
- Years offered by the form: 1960 to 2025. That is the form's range; a series' own range was not read.
- Sectors: real, monetary and financial, public finance, external, social.

Predefined tables:

| Sector | Group | Tables |
|---|---|---|
| Real | Prices | CPI annual, CPI monthly, inflation annual average, inflation year on year |
| | National accounts | GDP and uses at constant prices, in nominal value; main macro indicators |
| Monetary and financial | Monetary statistics MSMF 2000, monthly | money aggregates, central bank, other depository institutions, net claims on central government, claims on other sectors, net foreign assets |
| | MSMF 2000, annual | the same six |
| | MSMF 1984, annual and monthly | monetary institutions, central bank, net government position, bank assets and liabilities, foreign assets, central bank interventions |
| | Markets | exchange rates monthly, annual; money and interbank market; STAR-UMOA indicators; central bank treasury |
| | Banks | overall margin and ratios, bank interest rates, sector credit from the risk centre, monthly and annual |
| | Decentralised finance | financial inclusion, microfinance |
| Public finance | | TOFE (government financial operations), external public debt |
| External | | balance of payments, 4th, 5th and 6th editions; international investment position |

The main site also publishes rates of foreign currencies against the CFA franc for transfers (`/fr/cours/…`, a page), reports and research.

## How to call it
- `https://edenpub.bceao.int/` for the forms (a data dictionary and a user guide are linked). Fields are posted through its PHP forms; those were not exercised.
- `https://www.bceao.int/fr` for the site. The `robots.txt` of `www.bceao.int` does not close its statistics pages; that of `edenpub` was not fetched. French only in the warehouse.
- Trap: the catalogue's `http://www.bceao.int` failed its DNS at the earlier probe; `https://www.bceao.int` answers. Four names in the catalogue are this one body.

## What the app reads
Nothing.

## Not read yet
**Economy › Countries:** monthly CPI, money aggregates and rates for the eight countries, which the international compilations cover less well (not compared here).

## Calls made
All on 2026-11-05 with `User-Agent: Deepwater/1.0`.
- `GET https://www.bceao.int/robots.txt` → 200, 1,594 bytes.
- `GET https://edenpub.bceao.int/` → 200, 79,904 bytes (twice).
- `GET https://www.bceao.int/fr/cours/cours-des-devises-contre-franc-cfa-appliquer-aux-transferts` → 200, 123,606 bytes.
- Earlier probe of `www.bceao.int` and its variants: `catalog/bodies_probe.jsonl.gz`.
