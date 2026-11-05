---
id: insee
name: INSEE (France)
rows: [research_insee]
catalogue: ["INSEE, France"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# INSEE

France's statistics office, two services: the BDM macro-economic database (SDMX, time series) and Melodi (data sets as cubes, mostly the census and local data).

## Contents
**BDM**: 244 dataflows (agency `FR1`), all in `catalog/insee_bdm.tsv`, dimensions and codelist sizes in `catalog/insee_bdm_structures.jsonl`. The source's own annotation counts 374,505 series over the flows, of which `SERIES_BDM` is the union of all (187,167 series, one dimension `IDBANK`). Periods from 1901 to 2027 (a forward survey period). Frequency codes in the last-observation answers: A, M, T (quarterly), Q, S, B.

Families, by the first token (flows; series, from the annotation):
- `CNA` 39 flows; 65,994 annual national accounts in bases 2010, 2014, 2020. `CNT` 12; 5,016 quarterly accounts: `CNT-2020-PIB-EQB-RF` GDP balance (FREQ, INDICATEUR, SECT-INST, OPERATION, CNA_PRODUIT, NATURE, REF_AREA, VALORISATION, UNIT_MEASURE...; 1949-Q1 to 2026-Q3).
- `IPC` 6; 14,651 consumer prices: `IPC-2025` (4,523; FORME-VENTE, COICOP2018, PRIX_CONSO, MENAGES_IPC), `IPC-2015`, `IPC-1998`, `IPC-PM-2015` average prices. `IPCH-2025`, `IPCH-2015` (ended 2025-12), `IPCH-2005`. `IPPI` 4; 9,272 producer prices. `IPPS`, `IPAGRI`, `IPEA`, `IPLA`, `IPPMP`, `IPGD`, `IPS`, `ICT`, `ICHT`, `ILC`, `IRL`, `INDEX`, rent indices.
- `IPI-2021` 1,092, production, 1990 to 2026-09 (older bases `IPI-2015`, `-2010`, `-2005`, `-1990` ended). `ICA` 14; 13,613 turnover. `VOV` sales volume. `CONSO` household consumption. `CONSTRUCTION`. `IC`.
- `ENQ` 11; 5,768 business and household surveys: `ENQ-CONJ-MENAGES`, `ENQ-CONJ-ACT-IND`, `ENQ-CONJ-IND-BAT`, `ENQ-CONJ-SERV`, `ENQ-CONJ-COM-DET`, `ENQ-CONJ-COM-GROS`, `ENQ-CONJ-TP`, `ENQ-CONJ-PROMO-IMMO`, and ended `ENQ-CONJ-TRES-IND` (2016-S2). `CLIMAT-AFFAIRES` 3, 1977-01 to 2026-10.
- Labour: `CHOMAGE-TRIM-NATIONAL` 261 (SEXE, AGE; 1975-Q1 to 2026-Q2), `EMPLOI-BIT-TRIM` 365, `EMPLOI-SALARIE-TRIM-NATIONAL` 80 (NAF2; 1970-Q4 to 2026-Q3), `DEMANDES-EMPLOIS-NATIONALES` 2,833, `SALAIRES` 3, `SMIC`.
- External and public: `BALANCE-PAIEMENTS` 197 (ends 2024-05), `COM-EXT`, `DETTE-*`, `COMPTES`.
- Regional `TCRED` 45; 35,055. Business `CREATIONS`, `DEFAILLANCES`. Demographic `DECES`, `NAISSANCES`, `MARIAGES`, `POPULATION`. `ODD` 17, `TOURISME-FRANCE-METHODE-REDRESSEMENT-2019` 3,944, `TRANSPORTS`, `PARC`.
- Dimensions present in nearly all: `FREQ`, `INDICATEUR` (1,358 codes), `NATURE`, `REF_AREA`, `UNIT_MEASURE`, `CORRECTION`. Often `SERIE_ARRETEE`, `BASIND`, `OPERATION`, `SECT-INST`, `NAF2`.
- 15 flows have no DSD behind this path: the IMF SDDS templates `CGD`, `CGO`, `CPI`, `EMP`, `FSI`, `GGD`, `GGO`, `IND`, `MET`, `NAG_GNA`, `POP`, `PPI`, `UEM`, `WOE`, and `STES_COLLECT`. 24 flows are too big for first and last period (see below): `IPC-2025`, `IPC-2015`, `IPC-1998`, `IPPI-2015`, `CNA-2020-ERE`, `COM-EXT`, `DEMANDES-EMPLOIS-NATIONALES`, `SERIES_BDM`...

**Melodi**: 163 data sets, all in `catalog/insee_melodi.tsv` (id, title, geography, start, end, series, observations, frequency, DSD, modified, components); 810.7 million observations. Annual 142, others 21. From 1900 on. Most are the census (`DS_RP_*`), firm demography (`DS_SIDE_*`, `DS_FLORES_*`), filosofi income, and the following macro ones: `DD_CNA_AGREGATS` (GDP, 228,727 observations; 1949 to 2025; components ACCOUNTING_ENTRY, ACTIVITY, COUNTERPART_AREA, EXPENDITURE, INSTR_ASSET, PRODUCT, PRICES, REF_YEAR_PRICE, REF_SECTOR, STO, TRANSFORMATION, UNIT_MEASURE...), `DD_CNA_*`, `DS_CNT_PIB_EQB_RF`, `DS_CNT_OPERATIONS`, `DD_EEC_SERIES`, `DS_ICA`, `DS_IPC_PRINC`, `DS_IPCH`, `DS_IPC_IND_CAT`, `DS_IPC_PM`, `DS_COEFF_EURO_FRANC`.

## How to call it
- BDM base `https://bdm.insee.fr/series/sdmx`. Catalogue `/dataflow`; structure `/datastructure/FR1/<flow>?references=children`; data `/data/SERIES_BDM/001565530?lastNObservations=1`, `/data/<flow>?lastNObservations=1&detail=dataonly`. SDMX-ML, `TIME_PERIOD`, `OBS_VALUE`; a series carries `TITLE_EN`, `LAST_UPDATE`, `IDBANK`.
- **A flow whose answer would be too large gets 413** (24 flows: use a key). Paced at 2.3 s, nothing else was refused.
- Melodi `https://api.insee.fr/melodi/catalog/all` (1.5 MB), `/melodi/range/DD_CNA_AGREGATS` (codes and labels per concept), files at `/melodi/file/<id>/<id>_CSV_FR`.
- Repeat with `tools/source-inventory/insee.py`.

## What the app reads
Row `research_insee` (`src/SourcesPage.cpp:561`), loader `src/CountryData.cpp:1547`, by idbank, keeping `TIME_PERIOD` and `OBS_VALUE`; all four updated in October 2026:
- `001796854` private payroll employment, quarterly, 2026-Q3 (`resources/country_indicators.json:123`).
- `001565530` business climate, all sectors; `001587668` household confidence; `011794487` household consumption of goods, up to `:758`.

## Not read yet
`IPC-2025` idbanks by COICOP, `IPI-2021`, `CHOMAGE-TRIM-NATIONAL`, `DETTE-TRIM-APU-2020`, `CNT-2020-PIB-EQB-RF` for Countries › France; `DEFAILLANCES`, `CREATIONS`, `ENQ-CONJ-*` for detail on the cycle. `SERIES_BDM` is the way to one series by idbank.

## Calls made
Made on 2026-11-05, sequentially per host with a pause, `User-Agent: Deepwater/1.0`. 743 calls.

- `GET https://bdm.insee.fr/series/sdmx/dataflow` → 200, 215.1 KB
- `GET https://bdm.insee.fr/series/sdmx/dataflow` → 200, 215.1 KB
- `GET https://api.insee.fr/melodi/catalog/all` → 200, 1.5 MB
- `GET https://bdm.insee.fr/series/sdmx/datastructure/{id}?references=…` × 245 → 200: 230, 404: 15, 166.2 MB in all; first `https://bdm.insee.fr/series/sdmx/datastructure/FR1/BALANCE-PAIEMENTS?references=children`
- `GET https://bdm.insee.fr/series/sdmx/data/BALANCE-PAIEMENTS?detail=nodata` → 200, 115.3 KB
- `GET https://bdm.insee.fr/series/sdmx/data/{id}?lastNObservations=…&detail=…` × 245 → 200: 221, 413: 24, 26.0 MB in all; first `https://bdm.insee.fr/series/sdmx/data/BALANCE-PAIEMENTS?lastNObservations=1&detail=dataonly`
- `GET https://api.insee.fr/melodi/range/DD_CNA_AGREGATS` → 200, 83.5 KB
- `GET https://bdm.insee.fr/series/sdmx/data/SERIES_BDM/001796854?lastNObservations=1` → 200, 2.0 KB
- `GET https://bdm.insee.fr/series/sdmx/data/SERIES_BDM/001565530?lastNObservations=1` → 200, 1.9 KB
- `GET https://bdm.insee.fr/series/sdmx/data/SERIES_BDM/001587668?lastNObservations=1` → 200, 2.0 KB
- `GET https://bdm.insee.fr/series/sdmx/data/SERIES_BDM/011794487?lastNObservations=1` → 200, 2.0 KB
- `GET https://bdm.insee.fr/series/sdmx/data/{id}?firstNObservations=…&detail=…` × 244 → 200: 220, 413: 24, 25.9 MB in all; first `https://bdm.insee.fr/series/sdmx/data/BALANCE-PAIEMENTS?firstNObservations=1&detail=dataonly`
