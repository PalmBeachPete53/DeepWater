---
id: indec
name: INDEC, through Argentina's time series API (datos.gob.ar)
rows: [nat_argentina]
catalogue: ["Instituto Nacional de Estadística y Censos (INDEC)", "INDEC"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# INDEC, through Argentina's time series API

The national open data API `apis.datos.gob.ar/series` serves INDEC's series and those of the central bank, the Treasury,
tourism, crime and more. The app reaches INDEC only this way. The catalogue's `api` value "web" misses it.

## Contents
**34,599 series in 398 data sets** from 19 catalogues, about 10.9 million observations in all (`serie_valores_cant`).
The full list is in [`catalog/indec.tsv.gz`](catalog/indec.tsv.gz) (6 MB): `serie_id`, frequency, title, units, first, last,
discontinued, catalogue, data set, theme. Argentina and its provinces, plus world commodity prices from UNCTAD and IMF.

- Frequency: annual `R/P1Y` 14,928; monthly `R/P1M` 8,775; quarterly `R/P3M` 8,085; daily `R/P1D` 2,266; half-yearly
  `R/P6M` 545. Dates run from 1875 to 2126, as the dump states them.
- Discontinued: 3,258. Not updated in the last cycle (`serie_actualizada` false): 18,365.
- Catalogues (`catalogo_id`): `sspm` 20,680 (macroeconomic programming, the INDEC core), `snic` 4,000 (crime), `obras`
  2,810, `turismo` 1,651, `bcra` 1,550, `siep` 1,148, `justicia` 938, `jgm` 578, `agroindustria` 392, `smn` 240,
  `modernizacion` 239, `salud` 180, `energia` 77, and smaller ones.
- Themes: Finanzas Públicas 5,430, Actividad 4,620, Delitos 3,800, Sector Externo 3,593, Dinero y Bancos 3,293, Precios
  2,608, Empleo e Ingresos 1,789, Economía Internacional 793, Mercados Financieros 354.
- Source: **INDEC 10,593 series in 151 data sets** (`dataset_fuente`), Treasury 5,058, BCRA 4,288, and others. INDEC's by
  theme: Actividad 3,454, Sector Externo 2,645, Precios 2,222, Empleo e Ingresos 1,580.

Families of INDEC and others, by data set title and series count: CPI national base December 2016 (570) and GBA (236);
EMAE activity; gross value added by branch (408); manufacturing production (267); exports and imports by NCM (294 each),
by country (174), by province (336); public-sector cash accounts ("Esquema Ahorro - Inversión - Financiamiento", about
500 per perimeter); BCRA "Dinero y crédito" (1,102), retail exchange rates (300), central bank assets and liabilities
(169); market expectations "Relevamiento de Expectativas de Mercado" (162, to 2025-12); bonds "Títulos Públicos de Deuda"
(228); balance of payments current account (194), international investment position (140), FDI (172), wages by sector
(130 each).

Series the app reads: `148.3_INIVELNAL_DICI_M_26` monthly from 2016-12, `143.3_NO_PR_2004_A_21` monthly from 2004-01,
`42.3_EPH_PUNTUATAL_0_M_30` quarterly from 2003-01.

## How to call it
- Base: `https://apis.datos.gob.ar/series/api/`. No key.
- Whole catalogue: `GET dump/series-tiempo-metadatos.csv` (26 MB, redirects to a signed storage URL), with fields
  `catalogo_id`, `dataset_id`, `distribucion_id`, `serie_id`, `indice_tiempo_frecuencia`, `serie_titulo`,
  `serie_unidades`, `serie_descripcion`, `dataset_titulo`, `dataset_fuente`, `dataset_tema`, `serie_indice_inicio`,
  `serie_indice_final`, `serie_valores_cant`, `serie_discontinuada`, `consultas_*`.
- Search: `GET search/?q=ipc&limit=2`.
- Data: `GET series/?ids=148.3_INIVELNAL_DICI_M_26&limit=1&sort=desc&format=json` gives `data`, `count`, `meta`,
  `params`; `meta[1]` holds catalogue, data set, distribution and field.
- Trap: the dump lags the live series. It stated 2026-07 as the last CPI month, while the live call returned a later one.

## What the app reads
`nat_argentina`, row at `src/SourcesPage.cpp:721`; `src/NationalSources.cpp:84`: CPI level and EMAE level (27 months,
computing the % over a year itself because "the API's own transformation lags"), unemployment rate (8 quarters). Keeps
`data` date and value and `meta[0].frequency`. Shown on the Map via `src/KeyIndicators.cpp:396`.

## Not read yet
- Countries › Argentina: wages, trade by country and NCM, gross value added by branch, public accounts, reserves and
  `bcra` money series.
- `Relevamiento de Expectativas de Mercado`: Desk › Forecasts for Argentina.
- Commodities: UNCTAD and IMF price indices already republished here ("Índices de precios internacionales", 192).

## Calls made
- `GET https://apis.datos.gob.ar/series/api/dump/series-tiempo-metadatos.csv` → 200, 26.4 MB
- `GET https://apis.datos.gob.ar/series/api/search/?q=ipc&limit=2` → 200, 1.2 KB
- `GET https://apis.datos.gob.ar/series/api/series/?ids=148.3_INIVELNAL_DICI_M_26&limit=1&sort=desc&format=json` → 200, 1.2 KB
