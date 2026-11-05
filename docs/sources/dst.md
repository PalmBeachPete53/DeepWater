---
id: dst
name: Statistics Denmark
rows: []
catalogue: ["Statistics Denmark"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# Statistics Denmark

StatBank Denmark: every Danish statistical table, each with its variables and period range in one catalogue call.

## Contents
2,319 active tables, complete in `catalog/dst.tsv` (id, text, unit, updated, first, last, active, variables). Earliest first period 1769. 10 subjects: People, Labour and income, Economy, Social
conditions, Education and research, Business, Transport, Culture and leisure, Environment and energy, About Statistics Denmark. Prefix counts: `KV2` 100, `LIG` 80, `LAB` 65, `BIL` 51, `LBE` 48, `STR` 42, `SKI` 40, `IDR` 33, `BYG` 31, `FOR` 28.

Macro tables:
- Prices: `PRIS01`, `PRIS02`, `PRIS8`, `PRIS9`, `PRIS03` CPI, `PRIS07`-`PRIS09` HICP. `PRIS01`: `VAREGR`(434), `ENHED`(3), `Tid`(310), 2000M12 to 2026M09.
- Exchange rates: `DNVALD` daily (`VALUTA`(55), `KURTYP`(4), `Tid`(12,579), 1977M01D03 to 2026M11D04), `DNVALM`, `DNVALQ`, `DNVALA`. 107 tables carry the prefix `DN`: central government debt `DNSOSB`, `DNSOST`; balance of payments `DNBOPM`; portfolio `DNPI`; investment position `DNIIP`; direct investment `DNFDI*`, exchange rates. These read like Danmarks Nationalbank's own, which the catalogue lists as "web" only.
- Labour: `AUS07`, `AUS08`, `AUS09` unemployed, seasonally adjusted, 2007M01 to 2026M09; `AUF01`, `AUP01`; `AKU510K`, `AKU520K`. `IPOP21` production.

## How to call it
- Base `https://api.statbank.dk/v1/`. `subjects?lang=en&format=JSON`, `tables?lang=en&format=JSON` (the catalogue, about 540 KB), `tableinfo/PRIS01?lang=en&format=JSON` (fields `id`, `text`,
  `description`, `unit`, `suppressedDataValue`, `updated`, `active`, `contacts`, `documentation`, `footnote`, `variables`).
- The data route was not called. `PRIS111` is not a table (it answered with an error).

## What the app reads
Nothing directly.

## Not read yet
`DNVALD` for DKK and other crosses, the `DN*` family for rates and banks, `PRIS01` and `AUS08` for Countries › Denmark. Search `dst.tsv` for `DN`.

## Calls made
Made on 2026-11-05, sequentially per host with a pause, `User-Agent: Deepwater/1.0`. 5 calls.

- `GET https://api.statbank.dk/v1/subjects?lang=en&format=JSON` → 200, 903 B
- `GET https://api.statbank.dk/v1/tables?lang=en&format=JSON` → 200, 537.9 KB
- `GET https://api.statbank.dk/v1/tableinfo/PRIS111?lang=en&format=JSON` → 200, 35.6 KB
- `GET https://api.statbank.dk/v1/tableinfo/PRIS01?lang=en&format=JSON` → 200, 39.2 KB
- `GET https://api.statbank.dk/v1/tableinfo/DNVALD?lang=en&format=JSON` → 200, 507.1 KB
