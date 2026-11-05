---
id: nbb
name: National Bank of Belgium (NBB.Stat, BelgoStat)
rows: []
catalogue: ["National Bank of Belgium", "Banque Nationale de Belgique", "BelgoStat"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# National Bank of Belgium

A .Stat Suite service: Belgian national accounts, prices, surveys, foreign trade, banking, public debt and financial accounts.

## Contents
223 dataflows (agency `BE2` 195, `IMF` 28, the SDDS Plus template), all in `catalog/nbb.tsv`, with dimensions, codelist sizes and available code counts in `catalog/nbb_structures.jsonl`. 31 are `_Part_*`, `_Overview` or `_TEST` views of another flow. Frequencies across flows: A 184, Q 117, M 107, D 35, S 4. Periods from 1900 to 2026-12. Structure and availability answered for all 223.

Families:
- **Business and consumer surveys**: `DF_BUSSURVM` monthly (FREQ, BUSSURVM_INDICATOR(23), BE_AREA(4), BUSSURV_SECTOR(44), BUSSURVM_ADJ(2); 1980-01 to 2026-10), `DF_BUSSURVQ`, `DF_BUSSURVH2`, `DF_CONSN` consumer (CONSN_INDIC(13), BE_AREA(4); 1985-01 to 2026-10), `DF_CONSTRUCTION`, `DF_AFCSURV`.
- **Prices**: `DF_NICP_2025`, `DF_NICP`, `DF_HISTO_NICP`, `DF_HICP_2025`, `DF_HICP` (1990-02 to 2025-12: the older base has stopped), `DF_PPI`, `DF_AGREED_WAGES`.
- **National accounts**: `DF_QNA_DISS` GDP by the 3 approaches, `DF_QNAP3_DISS`, `DF_NADETAIL_DISS`, `DF_NADET2008_DISS`, `DF_NADETP3_DISS`, `DF_NFQA_DISS`, `DF_NASECDET*`, `DF_SUTAP21/38/64_DISS`, regional `DF_REGACA*`, `DF_REGHHINC*`, `DF_REGTRD*`; `DF_EMPLOY_DISS`, `DF_INDPROD`, `DF_UNEMPLOYMENT`, `DF_UNEMPLOY_RATE`, `DF_CAPSTOCK2010_DISS`, `DF_POPULA`.
- **Public finance**: `DF_NFGOV*_DISS`, `DF_FINGOV*`, `DF_CGD*`, `DF_TREAS*`.
- **External**: `DF_BOPBPM6` (BOPRESERV_ITEM(83), BOPBPM6_FLOW(4), FREQ; 2000 to 2026-09), `DF_IIP`, `DF_EXTDEBT`, `DF_FDI_*`, `DF_ITSS`, `DF_RESERVBPM6`, `DF_STEC*`, `DF_IEGSGEO_DISS`; foreign trade `DF_EXTTRADEBECOM` and `DF_EXTTRADEBENAT` (over 190 million observations each; 2015-01 to 2026-07), by region `DF_EXTTRADEBXNAT`, `DF_EXTTRADEVLNAT`, `DF_EXTTRADEWLNAT`, `DF_EXTERNAL_TRADE_OVERVIEW`, `DF_EXTT_TEC_*`.
- **Rates and markets**: `DF_IRESCB` ESCB rates, `DF_IROLOBE2`, `DF_IROLOYLD`, `DF_IROLOYLD_2026` OLO yields, `DF_IRTRCERT` treasury certificates, `DF_AMOLO` (AMOLO_CODE(108); 1989 on), `DF_MIR`, `DF_MIRCCO`, `DF_EXR` (EXR_CURRENCY(40); A, D, M, Q, S; 1999 to 2026-11), `DF_MONAGG`, `DF_STOCCOM`, `DF_ISSHARES`, `DF_FISEC`.
- **Banks and credit**: `DF_BSICREDINS`, `DF_BSIMFI`, `DF_CREDINS*`, `DF_CREDNFC`, `DF_CCR`, `DF_CICR`, `DF_MORTGAGE`, `DF_BLS`, `DF_BLSCCO`, `DF_NBBFIN`, `DF_EUROSYS`, `DF_PAYSTAT`, `DF_FINACC2010*`, `DF_FAHHNFC*`.
- **Company accounts**: `DF_CBANACC*`, `DF_CBRATIOS*`, `DF_CBSOCBAL`.
- **SDDS Plus template**: `BOP_BPM6`, `CBS`, `CGD`, `CGO`, `CPI`, `DCS`, `DSE`, `EMP`, `EXD`, `EXR`, `FSI`, `GGD`, `GGO`, `IIP_BPM6`, `ILV1`, `ILV2`, `IND`, `INR`, `MET`, `NAG`, `OFS`, `POP`, `PPI`, `SBS`, `SPI`, `UEM`, `WOE`, `YGG`.

Belgium, with regions and, in foreign trade, partner countries.

## How to call it
- REST `https://nsidisseminate-stat.nbb.be/rest`, with `Accept: application/vnd.sdmx.structure+json;version=1.0`.
- Catalogue `/dataflow/all/all/latest`; structure `/dataflow/BE2/DF_AFCSURV/1.0?references=all`; availability `/availableconstraint/BE2,DF_AFCSURV,1.0/all/all/all`, which gives `obs_count`, codes per dimension and the time range. Portal: `https://dataexplorer.nbb.be/`.
- **The old `https://stat.nbb.be/` resets the connection**, so the catalogue's "NBB.Stat (CSV)" and `BelgoStat` URLs are out of date; `BelgoStat` is grouped here on the brief's instruction and could not be seen. Repeat with `tools/source-inventory/nbb.py`.

## What the app reads
Nothing directly. Belgium comes through Eurostat and the OECD.

## Not read yet
`DF_BUSSURVM` and `DF_CONSN`, the Bank's own confidence surveys; `DF_QNA_DISS`, `DF_NICP_2025`, `DF_INDPROD`, `DF_UNEMPLOY_RATE`, `DF_IROLOYLD`, for Countries › Belgium. `DF_IROLOYLD` could give the Desk › Rates page a Belgian spread.

## Calls made
Made on 2026-11-05, sequentially per host with a pause, `User-Agent: Deepwater/1.0`. 453 calls.

- `GET https://stat.nbb.be/` → ERR, 0 B
- `GET https://dataexplorer.nbb.be/` → 200, 197.5 KB
- `GET https://nsidisseminate-stat.nbb.be/rest/dataflow/all/all/latest` → 200, 252.8 KB
- `GET https://nsidisseminate-stat.nbb.be/rest/dataflow/all/all/latest` → 200, 252.8 KB
- `GET https://search-stat.nbb.be/api/` → 404, 31 B
- `GET https://nsidisseminate-stat.nbb.be/rest/availableconstraint/{id}/all/all/all` × 224 → 200: 224, 5.4 MB in all; first `https://nsidisseminate-stat.nbb.be/rest/availableconstraint/BE2,DF_AFCSURV,1.0/all/all/all`
- `GET https://nsidisseminate-stat.nbb.be/rest/dataflow/{id}/1.0?references=…` × 224 → 200: 224, 1217.1 MB in all; first `https://nsidisseminate-stat.nbb.be/rest/dataflow/BE2/DF_AFCSURV/1.0?references=all`
