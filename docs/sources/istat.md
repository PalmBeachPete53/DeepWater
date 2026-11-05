---
id: istat
name: National Institute of Statistics of Italy (ISTAT)
rows: []
catalogue: ["National Institute of Statistics (ISTAT)"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# National Institute of Statistics of Italy

IstatData over SDMX: every Italian statistical domain, in a very long list of dataflows.

## Contents
4,982 dataflows (agency `IT1`), complete in `catalog/istat.tsv` (flow, version, English name). 3,868 ids contain `_DF_`: they are views of a parent flow, for example `167_745_DF_DCSP_NIC1B2025_1` of `167_745`. 545 start with `DF_`, for example `DF_BES_TERRIT` well-being indicators by territory. The first token groups them: 144 groups. Largest: `742` ownership and governance by NACE 364; `83` agricultural enterprises 193; `183` local units 189; `68` trips 188; `150` employment 177; `73` prosecutions 157; `27` separations 154; `6` household income 141; `29` foreigners 123; `34` poverty 106; `172` unemployment indicators 94; `174`; `24`; `164` population 81; `151` unemployment 69; `101` crops 67; `152` inactive population 66.

Structure was fetched for 6 macro flows only, paced at 30 s since calls are slow; the other flows are by list:

| Flow | Name | Dimensions (available codes) | Observations | Period |
|---|---|---|---|---|
| `167_745` | NIC consumer prices, base 2025 | FREQ M, REF_AREA(124), DATA_TYPE(4), MEASURE(3), ECOICOP_2(865) | 1,078,821 | 1996-01 to 2026-10 |
| `163_184` | GDP and main components | FREQ Q, REF_AREA(1), DATA_TYPE_AGGR(11), VALUATION(3), ADJUSTMENT(3), EDITION(11) | 75,666 | 1995 to 2026-06 |
| `151_874` | Unemployment rate, monthly | FREQ M, REF_AREA(1), DATA_TYPE(1), ADJUSTMENT(2), SEX(3), AGE(8), EDITION(58) | 283,620 | 2004-01 to 2026-09 |
| `30_264` | Consumer confidence | FREQ M, Q; REF_AREA(5), DATA_TYPE(218), ADJUSTMENT(2), AGE(5), SEX(3), EDU_LEV_HIGHEST(4), LABPROF_STATUS_A_FID(5) | 255,036 | 1982-01 to 2026-12 |
| `120_337_DF_DCSC_COMMDET_1_12` | Retail sales, base 2021 | FREQ M, REF_AREA(1), DATA_TYPE(1), ADJUSTMENT(2), PRODUCT_SECTOR(3), TYPE_OF_SALES(1), PERS_EMPL_SIZE_CLASS(4) | 2,709 | 2000-01 to 2026-09 |
| `115_333_DF_DCSC_INDXPRODIND_1_6` | Industrial production, base 2021 | DSD `DCSC_INDXPRODIND_1`; availability timed out | | |

Other macro flows by name in the list: `167_744` NIC base 2015, `163_1226` quarterly accounts, `163_156`, `115_333`, `120_337`, `111_263` manufacturing confidence, `111_40` construction, `117_262` services, `117_266` retail. An `EDITION` dimension (58 codes in `151_874`) suggests vintages are kept. Italy, with regions in some flows.

## How to call it
- `https://esploradati.istat.it/SDMXWS/rest/dataflow/IT1/all/latest?detail=allstubs` with `Accept: application/vnd.sdmx.structure+json;version=1.0` (1.8 MB, 87 s; a first try at 60 s timed out).
- Structure `/dataflow/IT1/<flow>/1.0?references=children`; availability `/availableconstraint/<flow>/all/all/all`; one timed out at 240 s.
- Slow: allow several minutes per call, keep it sequential. `robots.txt` redirects to the data browser, so it states no rule. ISTAT's own usage limit was not read here; calls stayed 30 s apart and none was refused.

## What the app reads
Nothing directly. Italy comes through Eurostat and the OECD.

## Not read yet
`30_264` consumer confidence, `111_263` business confidence (ISTAT's own, beyond the EU survey), `151_874`, `115_333`, for Countries › Italy. Editions could give real-time vintages for backtests.

## Calls made
Made on 2026-11-05, sequentially per host with a pause, `User-Agent: Deepwater/1.0`. 15 calls.

- `GET https://esploradati.istat.it/SDMXWS/rest/dataflow/IT1/all/latest?detail=allstubs` → ERR, 0 B
- `GET https://esploradati.istat.it/SDMXWS/rest/dataflow/IT1/all/latest?detail=allstubs` → 200, 1.8 MB
- `GET https://esploradati.istat.it/robots.txt` → 200, 5.3 KB
- `GET https://esploradati.istat.it/SDMXWS/rest/dataflow/{id}/1.0?references=…` × 6 → 200: 6, 49.0 KB in all; first `https://esploradati.istat.it/SDMXWS/rest/dataflow/IT1/167_745/1.0?references=children`
- `GET https://esploradati.istat.it/SDMXWS/rest/availableconstraint/{id}/all/all/all` × 6 → 200: 5, ERR: 1, 16.6 KB in all; first `https://esploradati.istat.it/SDMXWS/rest/availableconstraint/167_745/all/all/all`
