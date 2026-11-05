---
id: ine_pt
name: Statistics Portugal (INE)
rows: []
catalogue: ["Statistics Portugal"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# Statistics Portugal

INE Portugal's indicator database: each indicator is a code (`varcd`) with its dimensions, its periods and geography.

## Contents
- Catalogue API answer (`xml_indic.jsp?opc=3&lang=EN`): 326 indicators, in `catalog/ine_pt.tsv` (varcd, theme, subtheme, title,
  periodicity, last period, geography level, last update). **This is a selection, not the whole database:** `0008074` answers
  in the data API and is not among the 326. The full number of indicators was not found. Titles stay in Portuguese despite `lang=EN`.
- Themes: Population 33, Short-term trends 27, Environment 21, Enterprises 21, Transport and communication 20, Health 19,
  International trade 17, Labour market 16, Culture 15, Research and development 14; subthemes Domestic trade, Prices (12), Energy,
  Credit institutions, Supply of goods and services (9), Demand of goods and services (6).
- Frequency: annual 240, monthly 49, quarterly 21, ten-yearly 10, others 6.
- An indicator (`pindica.jsp?op=2&varcd=0008074&lang=EN`): `IndicadorCod`, `IndicadorDsg`, `MetaInfUrl`, `DataExtracao`,
  `DataUltimoAtualizacao`, `UltimoPref`, `Dados` keyed by period; a data row has `geocod`, `geodsg`, `dim_3`, `dim_3_t`,
  `ind_string`, `valor`. Geography goes down to municipalities.

## How to call it
- Catalogue `https://www.ine.pt/ine/xml_indic.jsp?opc=3&lang=EN` (XML). `opc=1` throws a Java stack trace in a 200, `opc=2`
  timed out at 45 s, `opc=4` redirects to the home page.
- Data `https://www.ine.pt/ine/json_indicador/pindica.jsp?op=2&varcd=<code>&lang=EN`, latest period by default; metadata
  `pindicaMeta.jsp?varcd=<code>&lang=EN`. In the catalogue each indicator gives its own `bdd_url`, `metainfo_url`, `json_dataset`.
- The API page (`xpgid=ine_api_v2`) lists Catalog, Database and Metadata APIs and terms of use; one usage page timed out at 45 s.
- `robots.txt` is 404. Answers under a second; a municipal indicator near 480 KB.

## What the app reads
Nothing.

## Not read yet
Short-term trends and Prices indicators for Countries › Portugal (see `ine_pt.tsv`, theme `Short-term trends`).

## Calls made
Made on 2026-11-05, sequentially per host with a pause, `User-Agent: Deepwater/1.0`. 10 calls.

- `GET https://www.ine.pt/robots.txt` → 404, 74 B
- `GET https://www.ine.pt/ine/json_indicador/pindica.jsp?op=2&varcd=0008074&lang=EN` → 200, 483.4 KB
- `GET https://www.ine.pt/ine/xml_indic.jsp?opc=…&lang=…` × 5 → 200: 4, ERR: 1, 1.1 MB in all; first `https://www.ine.pt/ine/xml_indic.jsp?opc=3&lang=EN`
- `GET https://www.ine.pt/xportal/xmain?xpid=INE&xpgid=ine_api&INST=322751522` → 200, 46.2 KB
- `GET https://www.ine.pt/xportal/xmain?xpid=INE&xpgid=ine_api&INST=607384938` → ERR, 0 B
- `GET https://www.ine.pt/xportal/xmain?xpid=INE&xpgid=ine_api_v2` → 200, 52.1 KB
