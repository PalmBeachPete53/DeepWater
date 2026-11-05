---
id: eurostat
name: Eurostat, the statistical office of the European Union
rows: [crop_yields, eurostat, research_eurostat]
catalogue: ["EUROSTAT"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# Eurostat

The statistics of the EU, its members, EFTA and the candidate countries: national accounts, prices, labour, short-term business, trade, balance of payments, energy, population and much more, mostly country by country.

## Contents
The table of contents answered with **8,887 data set rows and 1,521 table rows** in 1,926 folders. A code can sit under several themes: the rows hold **6,659 distinct data sets and 944 distinct tables**. All of them, with theme path, last update, first and last period and number of values: [`catalog/eurostat.tsv.gz`](catalog/eurostat.tsv.gz) (10,408 rows).

Distinct data sets by theme (a set may count under two):

| Tree | Theme | Data sets |
|---|---|---|
| Database by themes | General and regional statistics | 782 |
| | Economy and finance | 276 |
| | Population and social conditions | 3,556 |
| | Industry, trade and services | 522 |
| | Agriculture, forestry and fisheries | 441 |
| | International trade | 35 |
| | Transport | 640 |
| | Environment and energy | 235 |
| | Science, technology, digital society | 627 |
| Cross cutting topics | EU policies | 145 |
| | Cross cutting topics | 1,006 |

Biggest code families: `ilc` 569 (living conditions), `hlth` 517, `earn` 351, `lfso` 323, `inn` 318, `trng` 226, `ef` 182, `rail` 177, `isoc` 173, `migr` 167, `educ` 156, `cens` 143, `mar` 139, `lfsa` 136, `road` 132, `tour` 129, `nrg` 119, `env` 116, `sbs` 107. For markets and macro: `nama` 34, `namq` 9, `nasq` 7, `prc` 52, `sts` 62, `gov` 28, `bop` 27, `irt` 26, `ert` 14, `ext` 35, `lfsi` 48, `lfsq` 87, `ei` 20.

Structure (dimensions, codes available, labels) of 475 data sets, the ones the app reads and the families `ei_ irt_ ert_ bop_ ext_st gov_10 sts_ prc_hicp prc_hpi prc_ppp une_ lfsi_ jvs_ lc_lci earn_mw nama_10_ namq_ nasq_ nasa_10_ nrg_cb nrg_pc apro_cpsh road_eqr_car proj_25n demo_pjan demo_gind tour_occ_ni migr_asyapp bd_ lfsq_ur ilc_di`: [`catalog/eurostat_structures.jsonl.gz`](catalog/eurostat_structures.jsonl.gz).

Areas: usually 30 to 46 `geo` codes (aggregates such as EU27_2020 and EA21, the members, EFTA, candidates). Frequencies appear as a code suffix: `_m`, `_q`, `_a`, `_d`; dates as `2026-09`, `2026-Q3`, `2026-S1`, `2026`, `2026-11-03`. The newest data update in the table of contents is dated 5 November 2026.

## How to call it
- Base: `https://ec.europa.eu/eurostat/api/dissemination`. No key.
- Catalogue: `GET /catalogue/toc/txt?lang=en` (2 MB, tab-separated, depth given by four spaces of indent per level).
- Availability: `GET /sdmx/2.1/contentconstraint/ESTAT/<code>` (the codes each dimension really uses).
- Structure: `GET /sdmx/2.1/dataflow/ESTAT/<code>?references=descendants&detail=referencepartial` (code lists trimmed to what is used, labelled).
- Data, JSON-stat: `GET /statistics/1.0/data/une_rt_m?geo=FR&s_adj=SA&age=TOTAL&unit=PC_ACT&sex=T&lastTimePeriod=1`; `sinceTimePeriod=2024-01`, `format=JSON`, `lang=EN`.
- Traps met in the app's code: one parameter per filter value, repeated (`crops=C1100&crops=C1500`); `prc_hicp_manr` is frozen at 2025 and `prc_hicp_minr` (ECOICOP ver. 2, dimension `coicop18`) took over (`src/KeyIndicators.cpp:191`). Filter hard: without one, `prc_hicp_minr` holds 22 million values and `migr_asyappctzm` 105 million.

## What the app reads

**`eurostat`** (`src/SourcesPage.cpp:698`), Map: nine JSON-stat requests from `src/KeyIndicators.cpp:192`, every country, keeping geo, period and value: `prc_hicp_minr` (`coicop18=TOTAL&unit=RCH_A`), `une_rt_m`, `namq_10_gdp` (`unit=CLV_PCH_PRE`), `sts_inpr_m`, `sts_trtu_m`, `ei_bssi_m_r2` (`indic=BS-CSMCI-BAL`), `sts_inppd_m`, `gov_10q_ggdebt`, `gov_10q_ggnfa`.

**`research_eurostat`** (`src/SourcesPage.cpp:497`), Economy › Countries and › Indicators: 27 data sets through 55 rows of `resources/country_indicators.json`, one country at a time (`src/CountryData.cpp:1304`) or every country (`src/CountryData.cpp:2083`), for the 39 areas of `eurostatGeos`.

**`crop_yields`** (`src/SourcesPage.cpp:356`), Map › supply chains, the Eurostat part: `apro_cpsh1` with `strucpro=YLD_HUMD_EU_T_HA` and `crops=C1100,C1500,C2000,I1130` from 2008, kept by country, crop and year (`src/SupplyChain.cpp:143`).

Every data set read:

| Code | Title | First | Last | Values | Dimensions (codes available) |
|---|---|---|---|---|---|
| `apro_cpsh1` | Crop production in EU standard humidity | 2000 | 2026 | 387075 | freq(1) crops(214) strucpro(5) geo(41) |
| `earn_mw_cur` | Monthly minimum wages - bi-annual data | 1999-S1 | 2026-S2 | 4573 | freq(1) currency(3) geo(39) |
| `ei_bsin_q_r2` | Industry survey results - quarterly data | 1980-Q1 | 2026-Q4 | 91124 | freq(1) indic(11) s_adj(2) geo(35) |
| `ei_bssi_m_r2` | Economic sentiment and confidence indicators by sector - month | 1980-01 | 2026-10 | 142662 | freq(1) indic(6) s_adj(2) geo(35) |
| `gov_10q_ggdebt` | Quarterly government debt | 1994-Q1 | 2026-Q2 | 610308 | freq(1) na_item(20) sector(8) unit(4) geo(32) |
| `gov_10q_ggnfa` | Quarterly non-financial accounts for general government | 1995-Q1 | 2026-Q2 | 1389286 | freq(1) unit(3) s_adj(3) sector(6) na_item(53) geo(34) |
| `ilc_lvho02` | Distribution of population by tenure status, type of household | 2003 | 2025 | 286671 | freq(1) rskpovth(3) hhcomp(17) tenure(7) unit(1) geo(46) |
| `jvs_q_nace2` | Job vacancy statistics by NACE Rev. 2 activity - quarterly dat | 2001-Q1 | 2025-Q4 | 724260 | freq(1) s_adj(2) nace_r2(30) sizeclas(2) indic_em(4) geo(36) |
| `lc_lci_r2_q` | Labour cost index by NACE Rev. 2 activity - nominal value, qua | 1996-Q1 | 2026-Q2 | 2879438 | freq(1) s_adj(3) unit(3) nace_r2(26) lcstruct(4) geo(40) |
| `migr_asyappctzm` | Asylum applicants by type, citizenship, age and sex - monthly  | 2008-01 | 2026-09 | 104985080 | freq(1) unit(1) citizen(206) sex(4) applicant(3) age(8) geo(34) |
| `nama_10_a64` | Gross value added and income by detailed industry (NACE Rev.2  | 1975 | 2025 | 9003751 | freq(1) unit(23) nace_r2(96) na_item(8) geo(44) |
| `namq_10_a10` | Gross value added and income by main industry (NACE Rev.2 ) -  | 1978-Q1 | 2026-Q3 | 6097372 | freq(1) unit(34) s_adj(4) nace_r2(12) na_item(4) geo(43) |
| `namq_10_gdp` | Gross domestic product (GDP) and main components (output, expe | 1978-Q1 | 2026-Q3 | 8409995 | freq(1) unit(34) s_adj(4) na_item(39) geo(44) |
| `nasq_10_ki` | Key indicators for sector accounts - quarterly data | 1980-Q1 | 2026-Q2 | 25726 | freq(1) unit(1) s_adj(2) na_item(12) sector(2) geo(22) |
| `nrg_cb_pem` | Net electricity generation by type of fuel - monthly data | 2008-01 | 2026-09 | 211015 | freq(1) siec(23) unit(2) geo(42) |
| `prc_hicp_minr` | Harmonised index of consumer prices (HICP) - ECOICOP ver.2 - i | 1996-01 | 2026-10 | 22481679 | freq(1) unit(5) coicop18(555) geo(46) |
| `prc_hpi_q` | House price index - quarterly data | 2005-Q1 | 2026-Q2 | 32753 | freq(1) purchase(3) unit(4) geo(38) |
| `road_eqr_carpda` | New passenger cars by type of motor energy | 2013 | 2025 | 6741 | freq(1) unit(1) mot_nrg(16) geo(43) |
| `sts_cobp_q` | Building permits by type of building - quarterly data | 1992-Q1 | 2026-Q3 | 273955 | freq(1) indic_bt(2) cpa2_1(9) s_adj(2) unit(4) geo(41) |
| `sts_copr_m` | Production in construction - monthly data | 1980-01 | 2026-09 | 338598 | freq(1) indic_bt(1) nace_r2(8) s_adj(3) unit(5) geo(33) |
| `sts_inpi_m` | Import prices in industry - monthly data | 1962-01 | 2026-09 | 3597575 | freq(1) indic_bt(3) cpa2_1(322) s_adj(1) unit(4) geo(32) |
| `sts_inppd_m` | Producer prices in industry, domestic market - monthly data | 1975-01 | 2026-10 | 7303309 | freq(1) indic_bt(1) nace_r2(355) s_adj(1) unit(5) geo(42) |
| `sts_inpr_m` | Production in industry - monthly data | 1953-01 | 2026-09 | 12879933 | freq(1) indic_bt(1) nace_r2(361) s_adj(3) unit(5) geo(42) |
| `sts_rb_q` | Business registration and bankruptcy index by NACE Rev. 2 acti | 2015-Q1 | 2026-Q3 | 135050 | freq(1) indic_bt(2) nace_r2(9) s_adj(2) unit(4) geo(39) |
| `sts_trtu_m` | Turnover and volume of sales in wholesale and retail trade - m | 1991-01 | 2026-09 | 4213307 | freq(1) indic_bt(2) nace_r2(38) s_adj(3) unit(5) geo(43) |
| `tour_occ_nim` | Nights spent at tourist accommodation establishments - monthly | 1990-01 | 2026-09 | 310938 | freq(1) c_resid(3) unit(4) nace_r2(5) geo(43) |
| `une_ltu_q` | Long-term unemployment by sex - quarterly data | 2003-Q1 | 2026-Q2 | 177312 | freq(1) indic_em(2) age(2) unit(3) s_adj(3) sex(3) geo(38) |
| `une_rt_m` | Unemployment by sex and age - monthly data | 1983-01 | 2026-09 | 759633 | freq(1) s_adj(3) age(3) unit(2) sex(3) geo(38) |

## Not read yet
All from `catalog/eurostat_structures.jsonl.gz`; the page each could feed is a suggestion.

| Code | Title | First | Last | Values | Dimensions (codes available) |
|---|---|---|---|---|---|
| `irt_euryld_d` | Euro yield curves - daily data | 2004-09-06 | 2026-11-03 | 12139157 | freq(1) yld_curv(3) maturity(358) bonds(2) geo(1) |
| `irt_euryld_m` | Euro yield curves - monthly data | 2004-09 | 2026-10 | 47880 | freq(1) yld_curv(3) bonds(2) maturity(30) geo(1) |
| `irt_lt_mcby_m` | EMU convergence criterion series - monthly data | 1980-01 | 2026-09 | 12003 | freq(1) int_rt(1) geo(30) |
| `irt_lt_gby10_m` | Government bond yields, 10 years' maturity - monthly data | 1950-01 | 2026-09 | 1826 | freq(1) maturity(1) geo(3) |
| `irt_st_m` | Money market interest rates - monthly data | 1970-01 | 2026-09 | 21268 | freq(1) int_rt(5) geo(16) |
| `ert_bil_eur_d` | Euro/ECU exchange rates - daily data | 1974-07-01 | 2026-11-04 | 332971 | freq(1) statinfo(1) unit(1) currency(37) |
| `ert_eff_ic_m` | Industrial countries' effective exchange rates - monthly data | 1994-01 | 2026-08 | 342954 | freq(1) exch_rt(10) unit(2) geo(45) |
| `bop_c6_m` | Balance of payments by country - monthly data (BPM6) | 1991-01 | 2026-08 | 10437306 | freq(1) currency(2) bop_item(63) sector10(9) sectpart(11) stk_flow(6) partner(12) geo(32) |
| `bop_c6_q` | Balance of payments by country - quarterly data (BPM6) | 1991-Q1 | 2026-Q2 | 68590354 | freq(1) currency(2) bop_item(200) sector10(13) sectpart(12) stk_flow(10) partner(50) geo(39) |
| `bop_iip6_q` | International investment position - quarterly and annual data  | 1991 | 2026-Q2 | 54079208 | freq(2) currency(2) bop_item(103) sector10(21) sectpart(16) stk_flow(15) partner(61) geo(44) |
| `bop_fdi6_pos` | EU direct investment positions by country, ultimate and immedi | 2007 | 2025 | 6955932 | freq(1) counterp(2) currency(1) nace_r2(81) stk_flow(4) entity(2) fdi_item(20) partner(308) geo(42) |
| `ext_st_eu27_2020sitc` | EU27 (from 2020) trade by SITC product group | 2002-01 | 2026-08 | 6397337 | freq(1) stk_flow(3) indic_et(11) partner(56) sitc06(16) geo(1) |
| `ext_st_easitc` | Euro area trade by SITC product group | 2002-01 | 2026-08 | 7305223 | freq(1) stk_flow(3) indic_et(11) partner(63) sitc06(16) geo(1) |
| `nrg_cb_gasm` | Gas supply, transformation and consumption - monthly data | 2008-01 | 2026-09 | 129255 | freq(1) nrg_bal(16) siec(1) unit(2) geo(41) |
| `nrg_cb_oilm` | Oil and petroleum products supply, transformation and consumpt | 2008-01 | 2026-09 | 2912904 | freq(1) nrg_bal(26) siec(33) unit(1) geo(38) |
| `nrg_cb_em` | Electricity supply, transformation and consumption - monthly d | 2008-01 | 2026-09 | 44548 | freq(1) nrg_bal(7) siec(1) unit(1) geo(41) |
| `nrg_pc_204` | Electricity prices for household consumers - bi-annual data | 1985-S1 | 2026-S1 | 83925 | freq(1) siec(1) nrg_cons(11) unit(1) tax(3) currency(3) geo(43) |
| `nrg_pc_202` | Gas prices for household consumers - bi-annual data | 1985-S1 | 2026-S1 | 72871 | freq(1) siec(1) nrg_cons(9) unit(2) tax(3) currency(3) geo(37) |
| `ei_bsco_m` | Consumers confidence indicator and survey results - monthly da | 1980-01 | 2026-10 | 317382 | freq(1) indic(12) s_adj(2) unit(1) geo(35) |
| `ei_bsfs_m` | Financial services confidence indicator and survey results - m | 2006-04 | 2023-03 | 9792 | freq(1) s_adj(1) nace_r2(4) indic(6) geo(2) |
| `namq_10_fcs` | Household final consumption expenditure by durability - quarte | 1978-Q1 | 2026-Q3 | 1132807 | freq(1) unit(19) s_adj(4) na_item(6) geo(41) |
| `namq_10_exi` | Exports and imports by counterpart area - quarterly data | 1978-Q1 | 2026-Q3 | 752340 | freq(1) unit(12) s_adj(4) na_item(12) geo(44) |
| `nasq_10_nf_tr` | Non-financial transactions - quarterly data | 1980-Q1 | 2026-Q2 | 3442030 | freq(1) unit(2) direct(2) sector(9) na_item(101) s_adj(2) geo(31) |
| `sts_setu_m` | Turnover in services - monthly data | 1994-01 | 2026-08 | 2024061 | freq(1) indic_bt(2) nace_r2(43) s_adj(3) unit(5) geo(40) |
| `lfsi_emp_q` | Employment and labour force by sex and age - quarterly data | 2003-Q1 | 2026-Q2 | 659688 | freq(1) indic_em(2) s_adj(3) sex(3) age(7) unit(2) geo(38) |
| `gov_10dd_edpt1` | Government deficit/surplus, debt and associated data | 1995 | 2025 | 56909 | freq(1) unit(3) sector(6) na_item(18) geo(31) |
| `prc_ppp_ind` | Purchasing power parities, price level indices, nominal and re | 1995 | 2024 | 1401572 | freq(1) na_item(24) ppp_cat(61) geo(55) |
| `proj_25np` | Population on 1st January by age, sex and type of projection | 2025 | 2100 | 5617920 | freq(1) sex(3) age(110) projection(7) unit(1) geo(32) |
| `demo_pjan` | Population on 1 January by age and sex | 1960 | 2025 | 743221 | freq(1) unit(1) age(103) sex(3) geo(59) |
| `bd_9ac_l_form_r2` | Business demography by legal form and NACE Rev. 2 activity (20 | 2004 | 2020 | 4158448 | freq(1) indic_sb(20) leg_form(4) nace_r2(194) geo(39) |

- **Desk › Rates:** `irt_euryld_d` (the euro curve at 358 maturity codes, daily since 2004), `irt_lt_mcby_m` (each member's convergence yield, 30 areas since 1980).
- **Forex:** `ert_bil_eur_d` reaches back to 1974, further than the Frankfurter data the app reads through v1; `ert_eff_ic_m` adds effective rates.
- **Countries:** `bop_c6_m`, `bop_iip6_q`, `namq_10_fcs`, `namq_10_exi`, `nasq_10_nf_tr`, `sts_setu_m`.
- **Supply chains:** `nrg_cb_gasm` and `nrg_cb_oilm` (monthly gas and oil balances), `nrg_pc_204` and `nrg_pc_202` (household prices).

## Calls made
All on 2026-11-05 with `User-Agent: Deepwater/1.0`, one at a time, a second apart.
- `GET https://ec.europa.eu/eurostat/api/dissemination/catalogue/toc/txt?lang=en` → 200, 1,980,640 bytes, 12,335 lines.
- `GET .../sdmx/2.1/contentconstraint/ESTAT/une_rt_m` → 200, 16,374 bytes.
- `GET .../sdmx/2.1/dataflow/ESTAT/une_rt_m?references=descendants&detail=referencepartial` → 200, 89,176 bytes.
- The same two calls for each of the 475 data sets, by `tools/source-inventory/eurostat.py` → no data set failed. A failed availability call would leave that set with its code list instead, and the script does not log it.
- The JSON-stat example above is the app's own (`src/SourcesPage.cpp:500`); it was not called for this card, and figures are not stored here.
