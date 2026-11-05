---
id: bbk
name: Deutsche Bundesbank
rows: [research_bbk]
catalogue: ["Deutsche Bundesbank", "https://www.bundesbank.de/en"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# Deutsche Bundesbank

The Bundesbank's time series database over SDMX 2.1: German banking, securities, external sector, money markets and
payments, plus short-term indicators that mirror the Federal Statistical Office (prices, labour, production, trade).

## Contents
94 dataflows on 84 DSDs (agency `BBK`), all in `catalog/bbk.tsv`, dimensions and codelist sizes in
`catalog/bbk_structures.jsonl`. 81 flows answered with data: 56,779 series, 23.3 million observations, first period 1945,
last 2026-11. 9 flows are defined but answer 404 (`BBBK13`, `BBBK20`, `BBBP2`, `BBBPS`, `BBDC1`, `BBDG1`, `BBK10`, `BBSF3`,
`BBXP1`); 4 refuse a whole-flow download with 413 and need a key (`BBBK7`, `BBFBOPV`, `BBFFDIPV`, `BBKRT`).

Families, by flow code:

- Short-term indicators, `BBD*`: `BBDP1` prices (81 series, 1948-06 to 2026-Q3; dimensions FREQ, AREA(59), ADJUSTMENT(9),
  DOPR_CONCEPT(8), DOPR_CLASSIFICATION(4), DOPR_CLASSIFICATION_DETAIL(19), BASIS(11), SUFFIX(4)); `BBDL1` labour (54, 1949-12
  to 2026-Q3); `BBDE1` economic sectors (608, 1950 to 2026-W44); `BBDA1` foreign trade; `BBDB2`, `BBDR1`, `BBDY1`, `BBDZ1`.
- Banking, `BBBK*` and `BBBEK*`: `BBBK1` BSI banks in Germany (9,863, 1948-01 on), `BBBK3` borrowers (1,945), `BBBEK1`
  AUSTA banks (2,355), `BBBEK4` consolidated groups (1,788), `BBBU2`, `BBBZ1`, `BBBS2`.
- Securities and funds: `BBSIS` yields, term structure and issues (978; FREQ, ITEM, VALUATION, CURRENCY, ISSUER_CLASS,
  SECURITY_CLASS, MATURITY(101), INTEREST_TYPE...; 1948-01 to the current day), `BBSAP`, `BBSDI`, `BBSDP`, `BBSEI`, `BBSHI`,
  `BBSSP`, `BBSSY`, `BBSF2` investment funds (2,609).
- External: `BBEX3` exchange rates (3,240; CURRENCY(219), PARTNER_CURRENCY(219), SERIES_TYPE, RATE_TYPE, SUFFIX; 1945 on),
  `BBFFDITV` direct investment (2,376), `BBFEPOEV` external positions of enterprises (1,696), `BBFI1`, `BBFI3`, `BBEE1`, `BBEE5`.
- Financial accounts and insurers: `BBAF3` (1,814), `BBAF4`, `BBAI3`, `BBASV` Solvency (9,510), `BBAPV` pension funds (1,520).
- Money markets `BBMMB`, `BBMME`, `BBMMS`, `BBMMU`, `BBMF1`, `BBMFK1`; payments `BBZVS01`-`BBZVS12`, `BBZVSSSI`; others `BBGFS1`,
  `BBIB1`, `BBIG1`, `BBIM1`, `BBIN1`, `BBNZ1`, `BBQFS`, `BBTHB`, `BBUMF`, `BBWCAF1`, `BBXE1`, `BBXF1`, `BBXL3`, `BBXN1`, `BBXP2`, `BBXS1`.

Columns of a data row: the dimensions, `TIME_PERIOD`, `OBS_VALUE`, `TIME_FORMAT`, `BBK_DECIMALS`, `BBK_ID`, `BBK_UNIT`,
`BBK_UNIT_MULT`, `BBK_TITLE`/`BBK_TITLE_ENG`, `BBK_COMM_SRC`, `WEB_CATEGORY`, `BBK_DIFF`, `BBK_DIFF_Y`, `OBS_STATUS`. Mostly
Germany, with world codelists for exchange rates and external flows.

## How to call it
- Base `https://api.statistiken.bundesbank.de/rest`. Catalogue `GET /metadata/dataflow/BBK`; structure
  `GET /metadata/datastructure/BBK/<DSD>?references=children` (SDMX-ML).
- Data, one series: `GET /data/BBDP1/M.DE.N.VPI.C.A00000.VGJ.LV?format=sdmx_csv&lang=en&lastNObservations=1`.
- Traps: **a flow with no key ignores `detail=` values and `lastNObservations`** and sends every observation (`BBAF3` is
  about 44 MB). `detail=serieskeysonly` is 400. `availableconstraint` does not exist (404). `lang=en` makes the CSV
  comma-separated, without it `;`. Answers are fast, under a second for most flows; no limit was met at a one-second pace.
- Repeat with `tools/source-inventory/bbk.py`.

## What the app reads
- `research_bbk` (`src/SourcesPage.cpp:556`), loader `src/CountryData.cpp:1535`, 20 series for Countries › Germany
  (`resources/country_indicators.json:107` to `:951`).
- `BBDL1` `M.DE.Y.UNE.UBA000.A0000.A01.D00.0.R00.A` and `.ABA.A` unemployment, `...EMP.EAA000...D10.0.ABA.A` employment,
  `...VAC.VBA000...` vacancies. `BBDP1` `M.DE.N.VPI.C.A00000.VGJ.LV` and `.I20.A` CPI, `M.DE.N.EPG.G.GP19SA000000.I21.A` producer
  prices. `BBDA1` `M.DE.Y.EX.S.A.W1.A.V.ABA.A` and `.IM.` trade. `BBDE1` `M.DE.Y.BAA1.A2P000000.G.C.I21.A` production,
  `M.DE.Y.AEA1.A2P300000.F.C.I21.A` orders, `M.DE.Y.GUA1.N2G470000.A.C.I21.A` retail. Keeps period and value.

## Not read yet
- `BBSIS` the Bund yield curve and securities yields, for Desk › Rates; `BBEX3` exchange rates; `BBBK1` and `BBBK3` credit to
  firms and households for Countries › Germany; `BBAF3` household wealth; `BBMMB`-`BBMMU` money markets; `BBDY1` and `BBDR1`.

## Calls made
Made on 2026-11-05, sequentially per host with a pause, `User-Agent: Deepwater/1.0`. 197 calls.

- `GET https://api.statistiken.bundesbank.de/rest/metadata/dataflow/{id}` × 4 → 200: 4, 175.3 KB in all; first `https://api.statistiken.bundesbank.de/rest/metadata/dataflow/BBK`
- `GET https://api.statistiken.bundesbank.de/rest/metadata/datastructure/{id}?references=…` × 89 → 200: 89, 13.9 MB in all; first `https://api.statistiken.bundesbank.de/rest/metadata/datastructure/BBK/BBK_ACIP?references=children`
- `GET https://api.statistiken.bundesbank.de/rest/availableconstraint/BBDP1` → 404, 167 B
- `GET https://api.statistiken.bundesbank.de/rest/data/BBDP1?detail=serieskeysonly&format=sdmx_csv` → 400, 213 B
- `GET https://api.statistiken.bundesbank.de/rest/data/BBDP1?detail=nodata&format=sdmx_csv` → 200, 9.0 MB
- `GET https://api.statistiken.bundesbank.de/rest/metadata/dataflow/BBK/BBDP1?references=all` → 200, 1.5 KB
- `GET https://api.statistiken.bundesbank.de/rest/data/BBDP1?lastNObservations=1&detail=dataonly&format=sdmx_csv` → 200, 9.0 MB
- `GET https://api.statistiken.bundesbank.de/rest/data/BBDP1?firstNObservations=1&detail=dataonly&format=sdmx_csv` → 200, 9.0 MB
- `GET https://api.statistiken.bundesbank.de/rest/data/BBDP1/M.DE.N.VPI.C.A00000.VGJ.LV?format=sdmx_csv&lang=en&lastNObservations=1` → 200, 529 B
- `GET https://api.statistiken.bundesbank.de/rest/data/{id}?format=…&lang=…` × 97 → 200: 81, HTTPError:: 13, ValueError:: 3; first `https://api.statistiken.bundesbank.de/rest/data/BBAF3?format=sdmx_csv&lang=en`
