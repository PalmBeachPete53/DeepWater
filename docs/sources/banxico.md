---
id: banxico
name: Banco de México
rows: [banxico]
catalogue: ["Banco de México", "Banco de Mexico", "Bank of Mexico"]
access: free key
probed: 2026-11-05
confidence: called
---

# Banco de México

The SIE (Sistema de Información Económica): exchange rates, rates, government securities, money, credit, prices, production,
labour, balance of payments, surveys. The API needs a free token; the same structures can be browsed without one.

## Contents
**20 information sectors, 1,177 entries found on their directories**, in [`catalog/banxico.tsv`](catalog/banxico.tsv)
(sector, `idCuadro`, kind, English name). Codes: `CA*` analytic tables, `CF*`, `CP*`, `CE*`, `CI*`, `CR*`, `CL*` structures,
each holding series coded `SF*`, `SP*`, `SI*`, `SR*`, `SL*`, `SE*`. Mexico, plus foreign rates and FX crosses.

| Sector | Name (as the site gives it) | Structures stated | Entries found |
|---|---|---|---|
| 4 | Banco de México | 13 | 14 |
| 23 | Información histórica del mercado cambiario y de valores | 11 | 9 |
| 6 | Tipos de cambio y resultados de las subastas | 17 | 21 |
| 18 | Tasas y precios de referencia | 18 | 20 |
| 22 | Subastas y colocación de valores | 16 | 15 |
| 7 | Valores en circulación | 14 | 81 |
| 3 | Agregados monetarios y activos financieros internos | 15 | 18 |
| 19 | Financiamiento e información financiera de intermediarios | 264 | 263 |
| 11 | Billetes y monedas | 25 | 31 |
| 9 | Finanzas públicas | 11 | 11 |
| 8 | Índices de Precios al Consumidor y UDIS | 7 | 7 |
| 20 | Índices de Precios Productor y de Comercio Exterior | 12 | 6 |
| 2 | Producción | 107 | 37 |
| 10 | Laboral | 10 | 13 |
| 1 | Balanza de pagos | 78 | 89 |
| 5 | Sistemas de pago | 14 | 14 |
| 21 | Sistemas de pago de bajo valor | 30 | 30 |
| 24 | Encuestas | 32 | 0 (its page links them differently; "Available only in Spanish") |
| 25 | Fondo Mexicano del Petróleo | 6 | 468 |
| 27 | Sistemas de Pagos e Infraestructuras | 24 | 30 |

Series inside the structures opened, in [`catalog/banxico_structures.jsonl`](catalog/banxico_structures.jsonl), with their
codes and the span of the year selector:

| Structure | Holds | Series | Years |
|---|---|---|---|
| `CF102` | Daily exchange rates (`SF43718`, `SF43717`, `SF60653` among them) | 10 | 1991 to 2026 |
| `CF373` | Daily USD/MXN historical | 1 (`SF63528`) | 1954 to 2026 |
| `CF86` | Exchange rate, monthly average | 22 | 1968 to 2026 |
| `CF336` | Global foreign exchange | 144 | 2000 to 2026 |
| `CR60` | Real exchange rate, 111 countries | 5 | 1990 to 2026 |
| `CF101` | Money market representative rates | 14 | 1995 to 2026 |
| `CF111` | Interbank rates | 5 | 2006 to 2026 |
| `CF114` | Public securities | 42 | 1978 to 2026 |
| `CF300` | Prices of sovereign securities on the run | 180 | 2003 to 2026 |
| `CI34` | Foreign interest rates | 15 | 1953 to 2026 |
| `CF807` | Monetary aggregates | 54 | 2000 to 2026 |
| `CF120` | Monetary base | 3 | 1996 to 2026 |
| `CP154` | CPI, monthly | 15 | 1969 to 2026 |
| `CP150` | UDI value | 1 | 1995 to 2026 |

## How to call it
- Browse: `GET https://www.banxico.org.mx/SieInternet/consultarDirectorioInternetAction.do?sector=18&accion=consultarDirectorioCuadros&locale=en`,
  then `...&accion=consultarCuadro&idCuadro=CF101&locale=en` (or `consultarCuadroAnalitico`). HTML, ISO-8859-1; series
  codes are in the page. No key.
- Data: `GET https://www.banxico.org.mx/SieAPIRest/service/v1/series/SP30578,SF61745/datos/2024-01-01/2026-11-05?token=<token>`,
  also `/datos`, `/datos/oportuno`. Answer `{"bmx":{"series":[{"idSerie","titulo","datos":[{"fecha":"dd/MM/yyyy","dato":"1,234.5"}]}]}}`,
  `N/E` where there is none (the fields from the app's parser and docs; the token wasn't used here).
- Without a token, the API answers 400 `{"error":{"mensaje":"Token inválido"}}`. `.../doc/catalogoSeries` returns `{}`.
- The site's export says at most 5,000 series at once.

## What the app reads
- `banxico`, row at `src/SourcesPage.cpp:730`; `src/KeyedSources.cpp:198`: `SP30578` (annual inflation) and `SF61745`
  (target rate), keeps `fecha` and `dato`, for the Map.
- Countries › Mexico, `src/CountryData.cpp:1378`, one series with `provider` `banxico`; none is listed in
  `resources/country_indicators.json` today (the `p` value `banxico` appears 0 times).

## Not read yet
- Forex: `CF102` daily exchange rates, `CR60` real exchange rate.
- Desk › Rates: `CF101` money market, `CF300` sovereign securities, `CF114` public securities.
- Countries › Mexico: `CF807` aggregates, `CP154` prices, and the structures of sectors 2, 10 and 1.
- Encuestas (sector 24): the survey of private-sector expectations, not listed through the directory page.

## Calls made
- `GET https://www.banxico.org.mx/SieInternet/` → 200, 41 KB
- `GET https://www.banxico.org.mx/SieInternet/consultarDirectorioInternetAction.do?sector=<n>&accion=consultarDirectorioCuadros&locale=en`
  for the 20 sectors → all 200
- `GET .../consultarDirectorioInternetAction.do?sector=18&accion=consultarCuadroAnalitico&idCuadro=CA51&locale=en` → 200, 80 KB
- `GET .../consultarDirectorioInternetAction.do?sector=<n>&accion=consultarCuadro&idCuadro=<c>&locale=en` for CF102, CF86,
  CF373, CF336, CR60, CF101, CF111, CF114, CF300, CI34, CF807, CF120, CP154, CP150, CE1, CL291 → all 200 (CE1 and CL291
  with no series found in the page)
- `GET https://www.banxico.org.mx/SieAPIRest/service/v1/` → 200, `{}`
- `GET https://www.banxico.org.mx/SieAPIRest/service/v1/doc/catalogoSeries` → 200, `{}`
- `GET https://www.banxico.org.mx/SieAPIRest/service/v1/series/SP30578/datos/oportuno` (no token) → 400, "Token inválido"
