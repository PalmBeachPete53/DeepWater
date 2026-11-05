---
id: ine
name: National Statistics Institute of Spain (INE, Tempus 3)
rows: [research_ine]
catalogue: ["National Statistics Institute (INE)"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# National Statistics Institute of Spain

INEbase over the Tempus 3 JSON API: operations (surveys), their tables, series, variables and classifications for Spain.

## Contents
- **Operations**: 166 (`OPERACIONES_DISPONIBLES`, fields `Id`, `Cod_IOE`, `Nombre`, `Codigo`, `Url`). 110 of them list tables.
- **Tables**: 7,007, complete in `catalog/ine_tables.tsv` (operation, table id, name, periodicity id, first year, last
  reference). By periodicity id: 12 annual 5,368; 3 quarterly 678; 1 monthly 643; 105 at 152; 6 semi-annual 85; 113 at 55;
  106 at 16; 7 weekly 8. Largest operations: `EPA` labour force 1,039, `CENSOP` census 988, `ADRH` 540, `ICV` 508, `IDB` 342,
  `ECV` 254, `EM` 171.
- **Other lists**: `VARIABLES` 879, `PUBLICACIONES` 466, `PERIODICIDADES` (Minutely to Daily-business week, 14 and more),
  `CLASIFICACIONES`.
- A **series** (`SERIE/FREG337`): `Id`, `COD`, `FK_Operacion`, `Nombre`, `Decimales`, `FK_Periodicidad`, `FK_Publicacion`,
  `FK_Clasificacion`, `FK_Escala`, `FK_Unidad`.
- Macro operations by code: `IPC` CPI, `IPCA` HICP, `IPI` production, `IPRI` industrial prices, `ICM` retail, `ICN` and `ICNE`
  turnover, `IPRX-M` export and import prices, `CNTR2000`, `CNTR2010` quarterly accounts, `HPT` mortgages, `EOT` hotels, `EG`
  tourist spending, `FR` border tourists.
- Spain and its regions. The series count was not taken.

## How to call it
- Base `https://servicios.ine.es/wstempus/js/EN/`.
- Catalogue: `OPERACIONES_DISPONIBLES`, `TABLAS_OPERACION/<Id>`, `VARIABLES`, `PERIODICIDADES`, `PUBLICACIONES`, `CLASIFICACIONES`.
- Data: `DATOS_SERIE/FREG337?nult=1` (`nult` is the number of last periods) gives `{"Data": [{"Fecha": ms since 1970, "Valor": ...}]}`.
- `EN` labels are partly Spanish ("Semanal 1/1"). A slow first call took about 2 s; no limit met at one call a second.

## What the app reads
- `research_ine` (`src/SourcesPage.cpp:565`), loader `src/CountryData.cpp:1568`: one series, `FREG337` (Frontur total tourists,
  `resources/country_indicators.json:595`), `nult=2000`; keeps `Fecha` and `Valor`.

## Not read yet
Spain in Countries relies on other sources. Direct candidates: `IPC` and `IPCA` tables, `IPI`, `ICM`, `EPA` unemployment, `CNTR2010`
GDP, `HPT` mortgages, `ICN` turnover. `TABLAS_OPERACION` gives the table ids.

## Calls made
Made on 2026-11-05, sequentially per host with a pause, `User-Agent: Deepwater/1.0`. 173 calls.

- `GET https://servicios.ine.es/wstempus/js/EN/OPERACIONES_DISPONIBLES` → 200, 31.2 KB
- `GET https://servicios.ine.es/wstempus/js/EN/TABLAS_OPERACION/{id}` × 166 → 200: 166, 2.0 MB in all; first `https://servicios.ine.es/wstempus/js/EN/TABLAS_OPERACION/6`
- `GET https://servicios.ine.es/wstempus/js/EN/VARIABLES` → 200, 55.7 KB
- `GET https://servicios.ine.es/wstempus/js/EN/PERIODICIDADES` → 200, 1.2 KB
- `GET https://servicios.ine.es/wstempus/js/EN/PUBLICACIONES` → 200, 56.2 KB
- `GET https://servicios.ine.es/wstempus/js/EN/CLASIFICACIONES` → 200, 7.0 KB
- `GET https://servicios.ine.es/wstempus/js/EN/SERIE/FREG337` → 200, 217 B
- `GET https://servicios.ine.es/wstempus/js/EN/DATOS_SERIE/FREG337?nult=1` → 200, 223 B
