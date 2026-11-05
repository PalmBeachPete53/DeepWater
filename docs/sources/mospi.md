---
id: mospi
name: Ministry of Statistics and Programme Implementation, India (eSankhyiki)
rows: []
catalogue: ["Ministry of Statistics and Programme Implementation (MOSPI)"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# MoSPI, India

India's official statistics on eSankhyiki: national accounts, CPI, WPI, IIP, labour force, and the NSS survey rounds. It offers a public **MCP server**, with a catalogue of data
sets and indicators, and it also republishes the Reserve Bank's external-sector tables. The catalogue's "eSankhyiki (CSV), no key" doesn't mention it.

## Contents
**31 data sets, 500+ indicators by its own count** (`list_datasets`). Labels by data set are in [`catalog/mospi.tsv`](catalog/mospi.tsv) (code, label), the whole answer per data set in
[`catalog/mospi.jsonl`](catalog/mospi.jsonl).

| Code | Name | Structure |
|---|---|---|
| `PLFS` | Periodic Labour Force Survey | 8 indicators; `frequency_code` 1 annual, 2 quarterly, 3 monthly select which |
| `CPI` | Consumer Price Index | Base years 2010, 2012, 2024; `level` Group or Item; `series` Current or Back. 2024: 2026 on, 2012: to 2025, by its note |
| `IIP` | Index of Industrial Production | Base 1993-94, 2004-05, 2011-12; monthly, annual. Base 2011-12: years 2012 to 2026, 3 types, 10 categories, 23 subcategories |
| `ISP` | Index of Service Production | Base 2024-25; monthly, yearly; 19 sub-sectors |
| `WPI` | Wholesale Price Index | Base 2022-23 (2023 to 2026), 2011-12, 2004-05, 1993-94; 5 major groups, 29 groups, 142 sub-groups, 953 items |
| `NAS` | National Accounts | 22 national annual and quarterly, 12 state indicators; base 2011-12 and 2022-23 (4 years, 15 industries) |
| `ASI` | Annual Survey of Industries | 57 indicators, NIC classifications 1987 to 2008 |
| `CPIALRL` | CPI for agricultural and rural labourers | 2 |
| `ASUSE` | Unincorporated enterprises | 35 annual, 15 quarterly |
| `RBI` | **RBI Statistics** | 39 external-sector indicators: trade by country and commodity, balance of payments, external debt, reserves, NRI deposits, 155 exchange rates, forward premia |
| `ENERGY`, `MNRE` | Energy balance; renewable capacity | 2, 5 |
| `GENDER`, `ENVSTATS`, `NFHS`, `HCES`, `TUS`, `AISHE`, `UDISE`, `EC` | Gender 154, environment 126, health 21, consumption 9, time use 41, higher education 9, schools 46, economic census 3 | |
| `NSS71`, `NSS72`, `NSS72T`, `NSS73`, `NSS74`, `NSS75E`, `NSS76`, `NSS77`, `NSS78`, `NSS79`, `NSS80` | NSS survey rounds | 46, 11, 37, 6, 21, 13, 25, 55, 14, 35, 38 |

India and states; some by district. The eSankhyiki portal also lists its products at `macroindicators?product=<nas|cpi|iip|plfs|asi|hces|...>` (24 in `llms.txt`), with `ai-catalog.json` and
`.well-known/ard.json` as machine lists.

## How to call it
- MCP, JSON-RPC over HTTP POST at `https://mcp.mospi.gov.in/`, `Accept: application/json, text/event-stream`. Server "MoSPI Data Server" 2.3.0, protocol 2025-03-26. Answers as `event: message` lines.
- Tools, in order: `list_datasets`, `get_indicators(dataset)`, `get_metadata(dataset, indicator_code, ...)`, `get_data(dataset, filters)`.
- `api.mospi.gov.in` is a Swagger page with `main.js`, and it needs a TLS client that allows legacy renegotiation (`OP_LEGACY_SERVER_CONNECT`). Its paths: `/plfs`, `/nss78`, `/nss77`, `/asuse`,
  `/gender`, `/hces`, `/energy`, `/cpialrl`. Not called further.
- Traps: **every tool has an optional `user_query`, "the user's original question, stored for the engagement report"**. Leave it out from the app: it sends the user's text to a third party.
  `.well-known/mcp/server-card.json` answers 405 to GET. Filter codes differ by data set: read `get_metadata`. A few answers (`PLFS`, `CPI`, `RBI`) wanted more filters than tried, so no years came.

## What the app reads
Nothing: no row in `src/SourcesPage.cpp`. India reaches the Map through OECD KEI, IMF, BIS and the World Bank.

## Not read yet
- Countries › India: `CPI`, `WPI`, `IIP`, `PLFS`, `NAS`, at the source, monthly.
- `RBI` on this server: trade by country, reserves and 155 exchange rates, where the RBI's own site isn't open (`docs/sources/rbi.md`).
- The app already speaks MCP on the serving side (Integration › opencode): a client call would be small.

## Calls made
- `GET https://esankhyiki.mospi.gov.in/` → 200, 1.2 KB
- `GET https://esankhyiki.mospi.gov.in/llms.txt` → 200, 7 KB
- `GET https://esankhyiki.mospi.gov.in/ai-catalog.json` → 200, 25 KB
- `GET https://mcp.mospi.gov.in/.well-known/mcp/server-card.json` → 405
- `POST https://mcp.mospi.gov.in/` `initialize` → 200, 361 B
- `POST https://mcp.mospi.gov.in/` `tools/list` (twice) → 200, 10 KB
- `POST https://mcp.mospi.gov.in/` `tools/call` `list_datasets` → 200
- `POST https://mcp.mospi.gov.in/` `tools/call` `get_indicators` for the 31 data sets → all 200
- `POST https://mcp.mospi.gov.in/` `tools/call` `get_metadata` for PLFS, CPI, IIP, NAS, WPI, RBI → all 200
- `GET https://api.mospi.gov.in/` → TLS error twice, then 200, 486 B with legacy renegotiation allowed
- `GET https://api.mospi.gov.in/swagger-ui/` → 200, 486 B
- `GET https://api.mospi.gov.in/static/js/main.addddd18.js` → 200, 201 KB
