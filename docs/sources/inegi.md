---
id: inegi
name: Instituto Nacional de Estadística y Geografía (INEGI)
rows: [research_inegi]
catalogue: ["Instituto Nacional de Estadística y Geografía (INEGI)", "Instituto Nacional de Estadistica, Geografia e Informatica, Mexico", "INEGI - Instituto Nacional de Estadistica, Geografia e Informatica, Mexico", "INEGI, Mexico"]
access: free key
probed: 2026-11-05
confidence: docs
---

# INEGI

Mexico's statistics office: the Banco de Información Económica (BIE) and the Banco de Indicadores (BISE) through one API.
Documentation only here: the token is missing, so no catalogue call could be made.

## Contents
From the API's documentation page (`servicios/api_indicadores.html`, in Spanish), not from a catalogue answer:
- Two sources: `BISE` (Banco de Indicadores) and `BIE` (Banco de Información Económica), version `2.0`.
- Indicators at national (`00`), state (`99`) and municipal (`999`) level, depending on the indicator.
- Output of the indicator method: a `Header` (`Name`, `Email`) and `Series`: `INDICADOR`, `FREQ`, `TOPIC`, `UNIT`, `NOTE`,
  `SOURCE`, `LASTUPDATE`, `OBSERVATIONS` (`TIME_PERIOD`, `OBS_VALUE`, `OBS_STATUS`...).
- Catalogues by method: `CL_INDICATOR`, `CL_UNIT`, `CL_NOTE`, `CL_SOURCE`, `CL_TOPIC`, `CL_FREQ`, `CL_GEO_AREA`, each giving
  `CODE` with `Value` and `Description`.
- Also offered: PC-Axis, JSON-stat and SDMX forms on the same page.

The number of indicators is not stated in the documentation. It's the Constructor de Consultas
(`app/querybuilder2/default.html`) that browses them, through a theme tree loaded by a script that answered 403.

## How to call it
- `GET https://www.inegi.org.mx/app/api/indicadores/desarrolladores/jsonxml/INDICATOR/<id>/en/0700/false/BIE/2.0/<token>?type=json`.
- `.../jsonxml/CL_INDICATOR/<id or null>/es/BIE/2.0/<token>?type=json` for a catalogue.
- Parameters by position: indicator, language (`es`, `en`), area, latest only (`true`) or whole series (`false`), source,
  version, token. Formats `json`, `jsonp`, `xml`.
- Traps: in the app's parser, `TIME_PERIOD` `2026/03` is a month for a monthly series and a quarter for a quarterly one.
  `app/api/indicadores/` alone answers 403, which the Sources row uses as its probe (`src/SourcesPage.cpp:582`).
- `app/api/indicadores/` also has an `interna` path named in the portal's script. It isn't documented and wasn't called.

## What the app reads

Corrected on 5 November 2026: the Sources row was removed, since no series in `resources/country_indicators.json` reads INEGI. The loader remains for when one does.
`research_inegi`, row at `src/SourcesPage.cpp:580`; `src/CountryData.cpp:1400`, BIE, keeps `TIME_PERIOD` and `OBS_VALUE`. No
entry with `p: inegi` exists in `resources/country_indicators.json` today, so nothing reaches Countries › Mexico from it,
despite the row's text.

## Not read yet
Unknown in detail without the token. With one, `CL_TOPIC` and `CL_INDICATOR` on `BIE` would give the list and the
Countries › Mexico entries that the JSON lacks: CPI, IGAE, industrial production, retail, unemployment.

## Calls made
- `GET https://www.inegi.org.mx/servicios/api_indicadores.html` → 200, 235 KB
- `GET https://www.inegi.org.mx/app/api/indicadores/` → 403, 58 B
- `GET https://www.inegi.org.mx/app/querybuilder2/default.html` → 200, 7 KB
- `GET https://www.inegi.org.mx/app/querybuilder2/app/Service/service.min.js` → 200, 1 KB
- `GET https://www.inegi.org.mx/componentes/biinegi/config.min.js` → 200, 3.5 KB
- `GET https://www.inegi.org.mx/componentes/biinegi/treeTemasIndicadoresV1_3.min.js` → 403
- `GET https://www.inegi.org.mx/app/api/indicadores/desarrolladores/jsonxml/CL_INDICATOR/216064/es/BIE/2.0/?type=json` (no token) → 404
