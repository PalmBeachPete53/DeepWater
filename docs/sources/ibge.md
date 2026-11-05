---
id: ibge
name: Instituto Brasileiro de Geografia e Estatística
rows: [nat_brazil]
catalogue: ["Instituto Brasileiro de Geografia e Estatística (IBGE)", "Brazilian Institute of Geography and Statistics (IBGE)"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# IBGE

Brazil's national statistics: prices, labour survey, GDP, industry, retail, services, agriculture, through SIDRA's table API.

## Contents
**The list of tables could not be read.** `servicodados.ibge.gov.br/api/v3/agregados` timed out three times, and SIDRA's
own pages (`sidra.ibge.gov.br`, `apisidra.ibge.gov.br/desctabapi.aspx`, the API's home) answer a Cloudflare challenge
(403). So this card holds the tables sampled by number, not the count.

SIDRA's data route does answer, and a call with `v/all` shows a table's variables, dimensions and period. Sixteen were
read, in [`catalog/ibge_tables.jsonl`](catalog/ibge_tables.jsonl):

| Table | Holds | Period kind | First to last | Variables | Other dimension |
|---|---|---|---|---|---|
| 1737 | IPCA, the long series | month | 1979-12 to 2026-09 | 6 | |
| 7060 | IPCA by group, subgroup, item | month | 2020-01 to 2026-09 | 4 | Geral, grupo, subgrupo, item, subitem |
| 6381 | PNAD unemployment rate | rolling quarter | 2012 Q1 to 2026 Jul-Sep | 6 | |
| 6318 | PNAD labour force by status | rolling quarter | 2012 to 2026 | 8 | Condição |
| 6022 | PNAD population, rolling | rolling quarter | 2012 to 2026 | 8 | |
| 4093 | PNAD by sex | quarter | 2012 Q1 to 2026 Q2 | 34 | Sexo |
| 6461 | PNAD participation rate | quarter | 2012 Q1 to 2026 Q2 | 6 | |
| 8888 | PIM-PF industrial production | month | 2002-01 to 2026-09 | 6 | CNAE 2.0, 27 members at national level |
| 8880 | PMC retail | month | 2000-01 to 2026-08 | 6 | Tipos de índice 2 |
| 8881 | PMC broad retail | month | 2003-01 to 2026-08 | 6 | Tipos de índice |
| 8688 | PMS services | month | 2011-01 to 2026-08 | 6 | Tipos de índice, Atividades |
| 5932 | GDP, rates | quarter | 1996 Q1 to 2026 Q2 | 4 | Setores e subsetores |
| 1846 | GDP, values | quarter | 1996 Q1 to 2026 Q2 | 1 | Setores e subsetores |
| 2072 | Income and saving accounts | quarter | 2000 Q1 to 2026 Q2 | 12 | |
| 3939 | Livestock (PPM) | year | 1974 to 2025 | 1 | Tipo de rebanho |
| 5457 | Crops (PAM) | year | 1974 to 2025 | 8 | Produto |

All are Brazil at level `n1` here; SIDRA also offers regions, states and municipalities through `n2`, `n3`, `n6` (from
documentation, not checked one by one).

## How to call it
- `GET https://apisidra.ibge.gov.br/values/t/6381/n1/all/v/4099/p/last%2015`, no key. JSON array: the first object labels
  the columns (`NC`, `NN`, `MC`, `MN`, `V`, `D1C`, `D1N`, `D2C`, `D2N`, `D3C`, `D3N`...), the rest are rows, values as text.
- `v/all`, `p/first%201`, `p/last%201`, a category filter such as `c544/129314` or `c544/all`.
- Traps: `apisidra` answers the `values/` route to a program but gives a 403 challenge to everything else, `robots.txt`
  included (404). A wrong table number answers 400 "Tabela inválida". The body probe also got a 403 from `www.ibge.gov.br`.

## What the app reads
`nat_brazil`, row at `src/SourcesPage.cpp:718`; `src/NationalSources.cpp:38`: table 6381 variable 4099 (unemployment),
8888 variable 11602 with `c544/129314` (industry, % over a year), 8880 variable 11709 with `c11046/56734` (retail, % over a
year), each last 15 periods. Keeps `D3C` and `V` (`src/NationalSources.cpp:63`). The row's first call is SGS: see `bcb.md`.

## Not read yet
- Countries › Brazil reads these through the BCB today. SIDRA has them at the source and with more detail: 7060 (IPCA by
  item), 8688 (services), 5932 and 1846 (GDP by sector), 4093 (labour by sex).
- Commodities: 5457 and 3939 (crops and livestock by product), and by state through `n3`.

## Calls made
- `GET https://servicodados.ibge.gov.br/api/v3/agregados` → timeout (twice)
- `GET https://servicodados.ibge.gov.br/api/v3/agregados/6381/metadados` → timeout
- `GET https://servicodados.ibge.gov.br/api/v3/agregados?periodicidade=P5` → timeout
- `GET https://servicodados.ibge.gov.br/robots.txt` → timeout
- `GET https://sidra.ibge.gov.br/ajax/Tabela/Lista` → 403, Cloudflare challenge
- `GET https://apisidra.ibge.gov.br/desctabapi.aspx?c=6381` → 403, Cloudflare challenge
- `GET https://apisidra.ibge.gov.br/` → 403, Cloudflare challenge; `.../robots.txt` → 404
- `GET https://apisidra.ibge.gov.br/values/t/6381/n1/all/v/4099/p/last%201` → 200, 683 B
- `GET https://apisidra.ibge.gov.br/values/t/8888/n1/all/v/all/p/last%201/c544/all` → 200, 63 KB
- `GET https://apisidra.ibge.gov.br/values/t/8880/n1/all/v/all/p/last%201/c11046/all` → 200, 5 KB
- `GET https://apisidra.ibge.gov.br/values/t/<table>/n1/all/v/all/p/last%201` and `.../p/first%201` for the 16 tables
  above → all 200
- `GET https://apisidra.ibge.gov.br/values/t/9999999/n1/all/v/all/p/last%201` → 400
