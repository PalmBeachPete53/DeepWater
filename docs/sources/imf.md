---
id: imf
name: International Monetary Fund
rows: [portwatch, portwatch_ports, imf_pcps, imf_weo, imf_bop, imf_trade, imf_gdd, imf_cpi, research_imf]
catalogue: ["IMF", "International Monetary Fund", "International Monetary Fund (IMF)"]
access: free, no key
probed: 2026-11-05
confidence: called
---

# International Monetary Fund

Two separate services. The SDMX API at `api.imf.org` holds the Fund's statistics: 231 dataflows covering prices, output,
fiscal, external accounts, money and banks, the World Economic Outlook, and past vintages of many of them. An ArcGIS
organisation at `services9.arcgis.com` holds PortWatch (ships, ports, chokepoints, disruptions, trade estimated from AIS)
and the Climate Data Dashboard: 211 services.

## Contents

### SDMX API: 231 dataflows

`catalog/imf.tsv`: one line per flow (agency, id, version, series, first, last, dimensions and code counts, name).
`catalog/imf_structures.jsonl.gz`: description, last update, DSD, dimensions with their codelist and available codes (labels up
to 60 per dimension), attributes. `catalog/imf_codes.tsv.gz` (30,249 lines): every available code and label of every dimension
but the areas: grep it for an indicator. All 231 answered both structure and availability.

- By agency: `IMF.STA` 197, `IMF.RES` 11, `IMF.FAD` 8, `IMF.AFR` 3, `IMF.WHD` 3, `ISORA` 3, `IMF.APD` 2, `IMF.SPR` 2, `IMF.MCD` 1,
  `IMF.MCM` 1.
- **105 current flows** holding 9,058,440 series in all by their `series_count`.
- **126 vintages**, named `<FLOW>_<year>_<month>_VINTAGE`, from 2025-10 to 2026-09, of 36 flows (`MFS_FMP` 9, most others
  4, `WEO` 2): by their names copies of a flow as published that month, 91 with data and **35 empty** (`series_count` 0: 20 of May 2026,
  8 of April, 4 of February, 3 of January, such as `CPI_2026_MAY_VINTAGE`, whose data call answers an empty set).
- Dates below are the flow's own `time_period_start` and `time_period_end`, cut to the month. A last date after today
  is a projection (`WEO`, `FM`, the regional outlooks run to 2031-2032) or unexplained (`ER`, `LS`, `ITG` reach 2027).
- Areas: mostly `COUNTRY` in ISO3 plus IMF groups (`CL_COUNTRY`, 343 codes in the list; `G001` world). `PCPS` has one area,
  `G001`; `COFER` three.

**Prices**

| Flow | Name | Series | Areas | Freq | First..last | Other dimensions (codes) |
|---|---|---|---|---|---|---|
| `CPI` STA | Consumer Price Index | 28,643 | 202 | AMQ | 1900-01..2026-11 | `INDEX_TYPE` 2, `COICOP_1999` 13, `TYPE_OF_TRANSFORMATION` 8 |
| `PPI` STA | Producer Price Index | 1,157 | 105 | AMQ | 1948-01..2026-09 | `INDICATOR` 2, `TYPE_OF_TRANSFORMATION` 3 |
| `PCPS` RES | Primary Commodity Price System (PCPS) | 1,270 | 1 | AMQ | 1946-01..2026-10 | `INDICATOR` 124, `DATA_TRANSFORMATION` 4 |
| `CTOT` RES | Commodity Terms of Trade | 4,368 | 182 | AM | 1962-01..2026-06 | `INDICATOR` 6, `WGT_TYPE` 4 |
| `ECFIE` RES | Earnings-Calls-based Firm Inflation Expectations | 120 | 40 | Q | 2005-01..2026-10 | `INDICATOR` 3 |

**Output and labour**

| Flow | Name | Series | Areas | Freq | First..last | Other dimensions (codes) |
|---|---|---|---|---|---|---|
| `ANEA` STA | National accounts, Annual Data | 13,493 | 199 | A | 1950-01..2027-01 | `INDICATOR` 56, `PRICE_TYPE` 3, `TYPE_OF_TRANSFORMATION` 3 |
| `QNEA` STA | National accounts, Quarterly Data | 14,825 | 123 | Q | 1950-01..2026-10 | `INDICATOR` 55, `PRICE_TYPE` 3, `S_ADJUSTMENT` 2, `TYPE_OF_TRANSFORMATION` 3 |
| `NA_MAIN` STA | National Accounts Main Aggregates | 1,972 | 130 | AQ | 1950-01..2027-01 | `ADJUSTMENT` 2, `COUNTERPART_AREA` 3, `REF_SECTOR` 3, `COUNTERPART_SECTOR` 1, `ACCOUNTING_ENTRY` 3, `STO` 14, `INSTR_ASSET` 3, `ACTIVITY` 2, `EXPENDITURE` 2, `UNIT_MEASURE` 1, `PRICES` 2, `TRANSFORMATION` 1 |
| `PI` STA | Production Indexes | 3,826 | 118 | AMQ | 1948-01..2026-09 | `PRODUCTION_INDEX` 7, `TYPE_OF_TRANSFORMATION` 6 |
| `LS` STA | Labor Statistics | 4,750 | 194 | AMQ | 1946-01..2027-01 | `INDICATOR` 5, `TYPE_OF_TRANSFORMATION` 5 |
| `NSDP` STA | National Summary Data Page | 1,671 | 106 | AMQ | 1914-01..2026-04 | `NSDP_CAT` 8, `INDICATOR` 52, `TYPE_OF_TRANSFORMATION` 6 |

**Outlooks and fiscal**

| Flow | Name | Series | Areas | Freq | First..last | Other dimensions (codes) |
|---|---|---|---|---|---|---|
| `WEO` RES | World Economic Outlook | 8,202 | 210 | A | 1980-01..2032-01 | `INDICATOR` 145 |
| `AFRREO` AFR | Sub-Saharan Africa Regional Economic Outlook | 1,652 | 64 | A | 2000-01..2031-01 | `INDICATOR` 26 |
| `APDREO` APD | Asia and Pacific Regional Economic Outlook | 250 | 41 | A | 1951-01..2032-01 | `INDICATOR` 7 |
| `MCDREO` MCD | Middle East and Central Asia Regional Economic Outlook | 624 | 45 | A | 1950-01..2032-01 | `INDICATOR` 16 |
| `WHDREO` WHD | Western Hemisphere Regional Economic Outlook | 292 | 49 | A | 1951-01..2032-01 | `INDICATOR` 6 |
| `FM` FAD | Fiscal Monitor | 1,367 | 214 | A | 1956-01..2032-01 | `INDICATOR` 8 |
| `GDD` FAD | Global Debt Database | 2,090 | 190 | A | 1950-01..2025-01 | `INDICATOR` 11 |
| `HPD` FAD | Historical Public Debt | 191 | 191 | A | 1800-01..2027-01 | `INDICATOR` 1 |
| `GFS_SOO` STA | GFS Statement of Operations | 715,824 | 195 | A | 1972-01..2026-01 | `SECTOR` 8, `GFS_GRP` 5, `INDICATOR` 232, `TYPE_OF_TRANSFORMATION` 2 |
| `GFS_BS` STA | GFS Balance Sheet | 297,673 | 195 | A | 1972-01..2026-01 | `SECTOR` 8, `GFS_GRP` 4, `INDICATOR` 97, `TYPE_OF_TRANSFORMATION` 2 |
| `GFS_COFOG` STA | GFS Government Expenditures by Function | 374,401 | 195 | A | 1972-01..2026-01 | `SECTOR` 8, `GFS_GRP` 1, `INDICATOR` 83, `TYPE_OF_TRANSFORMATION` 3 |
| `QGFS` STA | Quarterly Government Finance Statistics | 21,388 | 125 | AMQ | 1957-01..2026-09 | `ACCOUNTS` 3, `SECTOR` 3, `INDICATOR` 126 |
| `PSBS` FAD | Public Sector Balance Sheet | 14,018 | 86 | A | 1950-01..2028-01 | `SECTOR` 11, `INDICATOR` 158, `UNIT` 3 |
| `WORLD` FAD | World Revenue Longitudinal Database | 3,245 | 195 | A | 1980-01..2025-01 | `INDICATOR` 21, `TYPE_OF_TRANSFORMATION` 2 |
| `FFS` STA | Fossil Fuel Subsidies | 13,418 | 186 | A | 2015-01..2036-01 | `SUB_TYPE` 5, `FFS` 27, `TYPE_OF_TRANSFORMATION` 2 |

**External**

| Flow | Name | Series | Areas | Freq | First..last | Other dimensions (codes) |
|---|---|---|---|---|---|---|
| `BOP` STA | Balance of Payments | 405,565 | 213 | AQ | 1948-01..2026-10 | `BOP_ACCOUNTING_ENTRY` 9, `INDICATOR` 721, `UNIT` 3 |
| `BOP_AGG` STA | Balance of Payments and International Investment Position Stat | 7,839 | 208 | A | 2005-01..2025-01 | `INDICATOR` 41, `TYPE_OF_TRANSFORMATION` 8 |
| `IIP` STA | International Investment Position | 135,694 | 184 | AQ | 1948-01..2026-07 | `BOP_ACCOUNTING_ENTRY` 3, `INDICATOR` 312, `UNIT` 3 |
| `IIPCC` STA | Currency Composition of the International Investment Position | 10,724 | 23 | AQ | 2000-01..2026-07 | `BOP_ACCOUNTING_ENTRY` 2, `INDICATOR` 63, `CURRENCY` 10, `UNIT` 3 |
| `DIP` STA | Direct Investment Positions by Counterpart Economy | 819,948 | 259 | A | 2009-01..2025-01 | `DV_TYPE` 2, `INDICATOR` 20, `COUNTERPART_COUNTRY` 259 |
| `PIP` STA | Portfolio Investment Positions by Counterpart Economy | 3,123,398 | 248 | AS | 1997-01..2026-01 | `ACCOUNTING_ENTRY` 2, `INDICATOR` 60, `SECTOR` 15, `COUNTERPART_SECTOR` 11, `COUNTERPART_COUNTRY` 248 |
| `SPE` STA | Special Purpose Entities | 12,328 | 31 | AQ | 2006-01..2026-07 | `BOP_ACCOUNTING_ENTRY` 10, `INDICATOR` 70, `UNIT` 3 |
| `ITG` STA | International Trade in Goods | 5,406 | 196 | AMQ | 1948-01..2027-01 | `INDICATOR` 8, `TYPE_OF_TRANSFORMATION` 12 |
| `IMTS` STA | International Trade in Goods (by partner country) | 473,595 | 237 | AMQ | 1948-01..2026-08 | `INDICATOR` 4, `COUNTERPART_COUNTRY` 245 |
| `ITS` RES | International Trade in Services | 7,412 | 160 | A | 1960-01..2016-01 | `INDICATOR` 45, `TYPE_OF_TRANSFORMATION` 14 |
| `IL` STA | International Liquidity | 9,581 | 210 | AMQ | 1945-01..2026-11 | `INDICATOR` 17, `UNIT` 3 |
| `IRFCL` STA | International Reserves and Foreign Currency Liquidity | 58,891 | 96 | ADMQ | 1995-01..2026-11 | `INDICATOR` 273, `SECTOR` 3 |
| `COFER` STA | Currency Composition of Official Foreign Exchange Reserves | 140 | 3 | AQ | 1995-01..2026-07 | `INDICATOR` 4, `FXR_CURRENCY` 14, `TYPE_OF_TRANSFORMATION` 2 |
| `ER` STA | Exchange Rates | 10,730 | 227 | AMQ | 1924-01..2027-04 | `INDICATOR` 10, `TYPE_OF_TRANSFORMATION` 2 |
| `EER` STA | Effective Exchange Rate | 732 | 100 | AMQ | 1979-01..2026-10 | `INDICATOR` 4 |
| `WPFXI` STA | Working Paper Foreign Exchange Intervention (WPFXI) A Dataset  | 2,326 | 124 | MQ | 2000-01..2025-01 | `INDICATOR` 15 |
| `WPCPER` STA | Crypto-based Parallel Exchange Rates | 164 | 64 | M | 2019-01..2025-11 | `CURRENCY` 64, `INDICATOR` 5 |

**Money, banks, markets**

| Flow | Name | Series | Areas | Freq | First..last | Other dimensions (codes) |
|---|---|---|---|---|---|---|
| `MFS_IR` STA | MFS, Interest Rate | 3,413 | 171 | AMQ | 1950-01..2026-10 | `INDICATOR` 45 |
| `MFS_MA` STA | MFS, Monetary Aggregates | 3,016 | 156 | AMQ | 1950-01..2026-10 | `INDICATOR` 8, `UNIT` 3 |
| `MFS_CBS` STA | MFS, Central Bank Data | 16,664 | 179 | AMQ | 1997-01..2026-10 | `INDICATOR` 58, `TYPE_OF_TRANSFORMATION` 4 |
| `MFS_DC` STA | MFS, Depository Corporations | 37,175 | 181 | AMQ | 1997-01..2026-10 | `INDICATOR` 91, `TYPE_OF_TRANSFORMATION` 8 |
| `MFS_ODC` STA | MFS, Other Depository Corporations | 16,379 | 181 | AMQ | 1997-01..2026-10 | `INDICATOR` 56, `TYPE_OF_TRANSFORMATION` 3 |
| `MFS_OFC` STA | MFS, Other Financial Corporations | 4,827 | 80 | AMQ | 1997-10..2026-10 | `INDICATOR` 21, `TYPE_OF_TRANSFORMATION` 3 |
| `MFS_FC` STA | MFS, Financial Corporations | 8,493 | 54 | AMQ | 1997-01..2026-10 | `INDICATOR` 53, `UNIT` 4 |
| `MFS_FMP` STA | MFS: Financial Market Prices | 279 | 70 | AMQ | 1950-01..2026-10 | `INDICATOR` 11, `TYPE_OF_TRANSFORMATION` 2 |
| `MFS_NSRF` STA | MFS,  Non-Standard Data | 28,906 | 192 | AMQ | 1950-01..2025-11 | `MFS_SRVY` 6, `INDICATOR` 231, `TYPE_OF_TRANSFORMATION` 5 |
| `FSIC` STA | FSI, Core and Additional Indicators | 33,327 | 157 | AMQ | 1995-01..2026-09 | `SECTOR` 11, `INDICATOR` 398 |
| `FSIBSIS` STA | FSI, Balance Sheet, Income Statement and Memorandum Series | 44,365 | 145 | AMQ | 1995-01..2027-01 | `SECTOR` 11, `INDICATOR` 553 |
| `FAS` STA | Financial Access Survey | 32,459 | 195 | A | 2004-01..2026-01 | `INDICATOR` 38, `SECTOR` 11, `COUNTERPARTY_SECTOR` 9, `SEX` 3, `TYPE_OF_TRANSFORMATION` 10 |
| `FDI` MCM | Financial Development Index | 1,728 | 192 | A | 1980-01..2021-01 | `INDICATOR` 9 |
| `FA` STA | Fund Accounts | 16,811 | 208 | AMQ | 1945-01..2026-10 | `INDICATOR` 27 |
| `MPFT` RES | Monetary Policy Frameworks Toolkit | 1,924 | 53 | A | 2007-01..2019-01 | `INDICATOR` 19, `TYPE_OF_TRANSFORMATION` 2 |

**Other signals**

| Flow | Name | Series | Areas | Freq | First..last | Other dimensions (codes) |
|---|---|---|---|---|---|---|
| `RSUI` WHD | Reported Social Unrest Index | 266 | 133 | M | 1985-01..2026-09 | `INDICATOR` 2 |
| `GPT` SPR | IMF Global Policy Tracker: How Countries are Responding to the | 1,099 | 5 | M | 2026-02..2026-10 | `INCOME_GROUP` 3, `INDICATOR` 69, `DURATION` 4, `TARGET` 3, `FUEL_STATUS` 3, `TYPE_OF_TRANSFORMATION` 1, `TERMINATION` 2 |
| `CCI` STA | Climate Change Indicators | 10,513 | 263 | ADM | 1958-03..2026-01 | `INDICATOR` 47, `GEOGRAPHIC_MARINE_ZONES_AND_CLIMATIC_CLASSIFICATIONS` 29 |
| `UNFCCC` STA | Greenhouse Gas Emissions | 42,808 | 239 | A | 1970-01..2036-01 | `INDUSTRY` 97, `INDICATOR` 9, `GAS_TYPE` 6, `TYPE_OF_TRANSFORMATION` 11 |
| `RE` STA | Renewable Energy | 3,708 | 247 | A | 2000-01..2024-01 | `INDICATOR` 3, `ENERGY_SOURCE` 11 |
| `ESG_FINANCE` STA | Sustainable Bonds | 363 | 109 | A | 2006-01..2025-01 | `INDICATOR` 13 |


The remaining 41 current flows:

- **Climate and energy**: `AEA`, `CFBL`, `CO2E`, `CO2EEIEM`, `CRBRATE`, `ENVTX`, `FLTRI`, `GENENVPROEXP`, `INFORMRISK`, `NDGAIN`, `TEG`
  (trade in low-carbon goods, bilateral, 139,973 series), `ESG_FINANCE_CURRENCY`, `ESG_FINANCE_ISSUER`.
- **Gender**: `GS_ATF`, `GS_CGI`, `GS_ED`, `GS_HEALTH`, `GS_LEPM`, `GS_LGRGHTS`, `GS_LI`, `GS_SDO`.
- **Fiscal detail**: `GFS_SFCP`, `GFS_SOEF`, `GFS_SSUC`, `FD`, `ICSD` (capital stock), `TAXFIT` (1,048,575 series, 19 countries),
  `ISORA_2016_DATA_PUB`, `ISORA_2018_DATA_PUB`, `ISORA_LATEST_DATA_PUB` (tax administration, agency `ISORA`, dimension
  `JURISDICTION`).
- **Aggregates**: `CPI_WCA`, `PI_WCA`, `QGDP_WCA` (world and country groups); `FSICDM`, `FSI_COUNTRY_METADATA_TABLE_2`.
- **Research sets**: `ED` export diversification, `EQ` export quality, `SRD` structural reforms, `WPAIAX` AI adoption index,
  `CSR` surveillance review survey, `SDG`.

Things worth knowing in the codes (all in `catalog/imf_codes.tsv.gz`):
- `WEO` `INDICATOR` 145: national accounts (`NGDP_RPCH`, `NGDPD`, `PPPGDP`, `PPPPC`), prices (`PCPIPCH`, `PCPIE`), `LUR`, `LP`,
  government (`GGXWDG_NGDP`, `GGXCNL_NGDP`, `GGR_NGDP`, `GGX_NGDP`, `GGSB`…), balance of payments (`BCA_NGDPD`, `BFD`, `BFP`,
  `BFRA`, `D`, `DS`…), and **commodity prices with their projections** (75 codes: `POILWTI`, `POILBRE`, `POILDUB`, `PNGASEU`, `PNGASUS`,
  `PNGASJP`, `PCOPP`, `PALUM`, `PNICK`, `PIORECR`, `PWHEAMT`…; `POILWTI` is annual, area `G001`, 1980 to 2031).
- `PCPS` `INDICATOR` 124: unit prices and indices; `DATA_TRANSFORMATION` `USD`, `INDEX`, `INDEX_PCH`, `INDEX_PCHY`.
- `MFS_IR` `INDICATOR` 45: central bank facilities, discount, money market, deposit, lending, T-bill rates, government bond yields, harmonized euro area rates.
- `EER`: `NEER_IX_RY2010_ACW`, `NEER_IX_RY2010_AEW`, `REER_IX_RY2010_ACW_RCPI`, `REER_IX_RY2010_AEW_RULC`.
- `COFER` `FXR_CURRENCY` 14: reserves by currency, world and two groups.
- `GDD` 11 indicators: debt of households, companies, general and central government, % of GDP.

### ArcGIS organisation: 211 services

`catalog/imf_portwatch.tsv`: one line per layer (service, layer id and name, table or geometry, rows, date field, first, last, number
of fields, the field names). `catalog/imf_portwatch.jsonl` adds the field types. 211 services: 210 FeatureServer and one MapServer
(`PortWatch_ports_final_tilelayer`). **203 with their layers and fields read, 204 layers, 4,585 fields.** 7 services did not answer in
six passes over an hour: `Indicator_1_2_melted_new`, `Indicator_11_3___Map_Layer`, `Indicator_16_1`,
`Indicator_7_3_2020_layer`, `Indicator_8_melted_new`, `Land_Cover_Perc_Layer`, `Low_Carbon_Technology_Products_2019`. Rows and dates
were asked for the 57 services around PortWatch below, not for the 154 others.

| Service | What it holds | Kind | Rows | Dates | Fields |
|---|---|---|---|---|---|
| `Daily_Chokepoints_Data` | Transits per day and chokepoint: `n_container, n_dry_bulk, n_general_cargo, n_roro, n_tanker, n_cargo, n_total`, `capacity_*` same types, `portid, portname` | table | 79,940 | 2019-01-01..2026-10-25 | 21 |
| `Daily_Ports_Data` | Per day and port: `portcalls_*`, `import_*`, `export_*` by vessel type, `portid, portname, country, ISO3` | table | 5,905,900 | 2019-01-01..2026-10-30 | 30 |
| `PortWatch_ports_database` | Port profile: `portid, portname, country, ISO3, continent, fullname, lat, lon, vessel_count_*` (5 types and total), `industry_top1..3, share_country_maritime_import, _export, LOCODE` | points | 2,065 | | 23 |
| `PortWatch_chokepoints_database` | Same fields, for chokepoints | points | 28 | | 23 |
| `portwatch_disruptions_database` | `eventid, eventtype, eventname, htmlname, htmldescription, alertlevel, country, fromdate, todate, severitytext, lat, long, affectedports, n_affectedports, affectedpopulation` | polygons | 132 | `fromdate` 2018-10-21..2026-08-14 | 21 |
| `disruptions_with_ports`, `disruptions_view`, `disruptions_ExportFeatures` | Variants of the disruptions | points, polygons | 490, 48, 69 | | 11, 19, 19 |
| `Daily_Trade_Data_REG` | Trade estimated from AIS per day and `ISO3`, `country`: `portcalls_*, import_*, export_*, shipment`, 30-day means and year-on-year of some | table | 560,560 | 2019-01-01..2026-10-30 | 34 |
| `Daily_Trade_Data_WLD`, `Daily_Trade_Data_TEST4` | The same, world, stopped | table | 481, 2,314 | to 2025-04-25, 2025-05-02 | 23, 30 |
| `Daily_Regional_Data` | Same by sub-national `regionid`, stopped | table | 384,842 | 2019-01-01..2024-06-16 | 28 |
| `Monthly_TradeNow` | Monthly by country and region: `ais_portcalls_*, ais_import_*, ais_export_*`, `value_import_total, value_export_total, volume_*, trade_value, trade_volume` | table | 18,228 | 2019-01-01..2026-09-01 | 25 |
| `Container_Metrics` | One column per port (`port101`, `port1021`…), monthly | table | 465 | `date_in` 2019-01-01..2026-09-01 | 155 |
| `geopulse_events`, `geopulse_episodes`, `geopulse_exposures_by_country` | Events (`eventtype, alertlevel, iscurrent, fromdate, todate`); their footprints; what they hit by country (`airports, buildings, capitalstock_res, cropland, gdp, population, ports, urbangdp` with `_pct`) | points, polygons, table | 615, 16,828, 1,407 | `computed_at` 2026-08-20..2026-10-28 for the last | 21, 22, 24 |
| `gdacs_events`, `events_wp_2` | Disaster events | points | 571, 514 | `fromdate` 2018-06-21..2026-07-12 for the first | 31, 9 |
| `spillovers_port_level_impact`, `_geo`, `_v2` | From a port to a port: `average_transit_days, daily_capacity_at_risk, relative_capacity_at_risk` | table, points | 226,904, 154,911, 226,904 | | 16, 17, 22 |
| `spillovers_trade`, `_geo` | From a port to a country, by `industry, hs_section`: `daily_export_value_at_risk, daily_import_value_at_risk` | table, points | 1,589,787, 166,815 | | 17, 121 |
| `spillovers_supplychain`, `_geo`, `_level_impact_geo` | From a port to a country: `daily_consumption_at_risk, daily_industryoutput_at_risk` | table, points | 3,395,490, 242,535, 242,535 | | 17, 121, 129 |
| `spillovers_country`, `spillovers_country_level_impact_geo` | The same by country | table, points | 1,120, 178,787 | | 7, 129 |
| `Spillover_Simulator_Maritime_Connections` | Links between ports: `from_id, to_id, distance, transit_days, input_days_1..200` | table | 155,521 | | 23 |
| `scenarios_trade`, `_geo`, `_network_geo` | Scenarios: `from_ISO3, to_portid, scenario, flow, industry, hs_section, days_downtime_at_port, trade_value_at_risk` | table, points | 1,699,001, 33,033, 34,598 | | 17, 241, 209 |
| `portsfacts` | `portid, ISO3, portfact`: a text per port | table | 540 | | 4 |
| `airports_latest`, `FR24_airports` | Airports with `iata_code`, lat, lon | points | 7,033, 9,332 | | 8, 12 |
| `Global_Shipping_Routes` | Global routes as one polyline set | polyline | 1 | | 14 |
| `PortWatch_boundaries`, `PortWatch_countries`, `PortWatch_Countries_Simplified`, `PortWatch_countries_database` | Country shapes and centres | polygons, points | 236, 237, 245, 196 | | 10, 7, 7, 14 |
| `Bilateral_Trade_EG`, `Bilateral_Trade_LCT` | Bilateral trade, by the names in environmental and low-carbon goods: `Country, Counterpart_Country, Trade_Flow, Year, Value` | table | 23,791, 18,229 | | 9 |

The other 154 services are mostly the Climate Data Dashboard, tables named `Indicator_<n>_…` (95 of them, in versions `_melted_new`,
`_dashboard`, `_selector`, `_SM`, `_CountryView`) plus `INFORMRISK_*`, `NDGAIN_*`, `ngfs_*`, `coastal_*_table`, `riverine_*_table`,
`temperature_*`, `green_bonds*`: the fields are in the catalog. The same subjects are in SDMX (`CCI`, `UNFCCC`, `ENVTX`, `INFORMRISK`,
`NDGAIN`, `TEG`, `ESG_FINANCE`), the proper place to read them. A few look like tests or side work
(`ATM_NY_State2`, `Pasadena_Fire_Area`, `web_scene_test__WFL1`).

## How to call it

**SDMX.** Base `https://api.imf.org/external/sdmx`.
- Catalogue: `GET /3.0/structure/dataflow/*/*/+?detail=allstubs` with `Accept: application/json`.
- Structure: `GET /3.0/structure/dataflow/IMF.STA/CPI/+?references=descendants&detail=referencepartial` (about 1 MB: it carries
  every codelist in full, including 1,653 organisations).
- Availability: `GET /2.1/availableconstraint/IMF.STA,CPI/all/all/all`: `series_count`, `time_period_start`, `time_period_end` and
  the codes in use. **The 3.0 `/availability/…` path answers 404**.
- Data, 3.0: `GET /3.0/data/dataflow/IMF.STA/CPI/+/KEN.CPI._T.IX.M?c%5BTIME_PERIOD%5D=ge:2026-M01&attributes=none&measures=all`:
  SDMX-JSON, series keyed by indices into `structures[0].dimensions`.
- Data, 2.1: `GET /2.1/data/IMF.RES,PCPS/G001.PURAN.USD.M?startPeriod=2026-01&detail=dataonly` with
  `Accept: application/vnd.sdmx.data+csv;version=1.0.0`; the CSV carries many descriptive columns (`DATAFLOW`, `COUNTRY`,
  `INDICATOR`, `DATA_TRANSFORMATION`, `FREQUENCY`, `TIME_PERIOD`, `OBS_VALUE`, `SCALE`, `PRECISION`, then metadata).

Traps: on 3.0, `startPeriod` is ignored (the same key answered 10,840 B with it and 1,241 B with `c[TIME_PERIOD]=ge:`). Monthly
time filters are written `2026-M01`. Key order differs by flow: read the DSD. A few flows use `FREQ` or `REF_AREA`
(`NA_MAIN`, `SDG`, `RSUI`, `GPT`, `CSR`) where the others say `FREQUENCY` and `COUNTRY`. In a concept reference, `4.0+.0` is a
wildcard version: the concept scheme returned may be 4.1.0. No key, no rate limit met at one call per 1.5 s over about 520
calls, no limit header seen. `api.imf.org/robots.txt` answers 502. **`www.imf.org` and `data.imf.org` answer 403 Access Denied**
to this client, robots file included, and were left alone; the catalogue's three IMF rows test those
hosts and read as failing while the API is fine.

**PortWatch.** Base `https://services9.arcgis.com/weJ1QsnbMYJlCHdG/arcgis/rest/services`.
- Catalogue: `GET ?f=json`: 211 services.
- A layer's structure: `GET /<service>/FeatureServer/0` (HTML, no `f`), which answers reliably.
- Data: `GET /Daily_Chokepoints_Data/FeatureServer/0/query?where=year%3D2026%20AND%20month%3D10&outFields=date,portid,n_total&returnGeometry=false&f=json`.
  1,000 rows at most per call (`maxRecordCount`), then `resultOffset`.

Traps: the directory's JSON is unreliable on this organisation. `FeatureServer?f=json`, `layers?f=json` and `0?f=json` often answer
**HTTP 200 with `{"error":{"code":400,"message":"Invalid URL"}}`**, for minutes on a given service; `layers?f=json` also answered
`{"layers":[],"tables":[]}` for services that have layers; 121 of 247 JSON calls failed in the first pass, retried three times. A
query that fails the same way answers on a later try: test `error` in the body, not the status. The app's comment
(`src/SupplyChain.cpp:856`) reports the same message for an unencoded `1=1`. From the app's comments, not retested: `orderByFields`
and `outFields=*` are refused with a filter (`src/PortWatch.cpp:76`); on `Daily_Ports_Data` a filter on the port id or a sum over
several days times out, and `import`, `export` are reserved words (`src/SupplyChain.cpp:888`).

## What the app reads

- `portwatch`: `Daily_Chokepoints_Data`, last 95 days, `date, portid, n_tanker, n_dry_bulk, n_container, n_general_cargo, n_roro,
  n_total, capacity_tanker, capacity` (`src/PortWatch.cpp:18`, `:87`). Map, chokepoints. `src/SourcesPage.cpp:337`.
- `portwatch_ports`: `PortWatch_ports_database` 16 fields, `portwatch_disruptions_database` 11 fields, `Daily_Ports_Data` day by day
  `portid, portcalls, import_cargo, import_tanker, export_cargo, export_tanker` (`src/SupplyChain.cpp:866`, `:875`, `:882`, `:890`).
  Map, supply chain layer. `src/SourcesPage.cpp:352`.
- `imf_pcps`: `PCPS`, 26 codes from `resources/markets.json` (`POILWTI`, `POILBRE`, `PNGASUS`, `PNGASJP`, `PCOALAU`, `PURAN`, `PGOLD`,
  `PSILVER`, `PPLAT`, `PPALLA`, `PCOPP`, `PALUM`, `PNICK`, `PZINC`, `PIORECR`, `PLITH`, `PCOBA`, `PWHEAMT`, `PMAIZMT`, `PSOYB`, `PPOIL`,
  `PRICENPQ`, `PCOFFOTM`, `PCOCO`, `PSUGAISA`, `PCOTTIND`), USD, monthly, three years (`src/CommodityPrices.cpp:197`). Map, commodities.
  `src/SourcesPage.cpp:384`.
- `imf_weo`: `WEO`, 9 codes `NGDP_RPCH, PCPIPCH, LUR, BCA_NGDPD, GGXCNL_NGDP, GGXWDG_NGDP, NGDPD, PPPPC, TX_RPCH` (`src/MacroData.cpp:488`,
  `:560`). Map, macro; also the `imf_weo` tool of the models (`src/LocalTools.cpp:150`). `src/SourcesPage.cpp:660`.
- `imf_bop`: `BOP`, `*.L_NIL_T+NNAFANIL_T+CD_T+DB_T.D_F+P_F+FAB+GS.USD.A` (`src/MacroData.cpp:683`). `src/SourcesPage.cpp:663`.
- `imf_trade`: `IMTS`, `*.XG_FOB_USD.*.A` (`src/MacroData.cpp:821`). Map, trade arcs. `src/SourcesPage.cpp:666`.
- `imf_gdd`: `GDD`, `F3T4_S14_POGDP_PT`, `F3T4_S11_POGDP_PT`, `FL_S13_POGDP_PT` (`src/DomesticFinance.cpp:266`). `src/SourcesPage.cpp:684`.
- `imf_cpi`: `CPI`, `*.CPI._T.YOY_PCH_PA_PT+POP_PCH_PA_PT.M` (`src/KeyIndicators.cpp:225`). `src/SourcesPage.cpp:702`.
- `research_imf`: Economy › Countries and Indicators, `resources/country_indicators.json`, 54 distinct keys on 14 flows: `CPI` 12, `LS` 8, `MFS_IR`
  6, `BOP` 6, `PPI` 5, `ITG` 4, `QNEA` 2, `MFS_CBS` 2, `MFS_MA` 2, `IL` 2, `PI` 2, `ER`, `MFS_FMP`, `MFS_DC` 1 each; and `WEO` 6 under provider
  `weo` (`LUR`, `BCA_NGDPD`, `GGXWDG_NGDP`, `GGXCNL_NGDP`, `GGX_NGDP`, `GGR_NGDP`). By country `src/CountryData.cpp:1259`, `:1284`, cross-country
  `:2023`, `:2047`. `src/SourcesPage.cpp:490`.

18 flows of 105 current ones; of the ArcGIS organisation, 4 services.

## Not read yet

- `WEO` commodity price projections and the rest of its 145 codes, in the flow the app already calls: Map, commodities.
- `COFER`: reserves by currency: Forex, Desk.
- `EER`, `CTOT`, `WPFXI`, `WPCPER` (crypto-based parallel exchange rates, 64 countries): Forex.
- `IRFCL` (reserves template, daily for some), `IIP`, `DIP`, `PIP` (bilateral direct and portfolio positions): Map, capital arcs beside trade.
- `FSIC`, `FSIBSIS`: bank soundness by country, including non-performing loans: Map, domestic finance.
- `QGFS`, `GFS_SOO`, `FM`, `HPD`, `PSBS`: public finance, quarterly and historical: Economy › Countries.
- `AFRREO`, `APDREO`, `MCDREO`, `WHDREO`: regional outlooks beside WEO: Desk › Forecasts.
- `RSUI` social unrest index, monthly: Map.
- `ECFIE`: firm inflation expectations from earnings calls: Economy.
- The vintages: data as first published, for backtests in Quant.
- PortWatch, `Monthly_TradeNow` and `Daily_Trade_Data_REG`: imports and exports by country estimated from AIS, days behind, with
  value and volume totals for the month: the Map's trade layer and Economy › Countries.
- PortWatch, `geopulse_events`, `geopulse_exposures_by_country`: fresher than the disruptions the app reads (`portwatch_disruptions_database`
  was last edited 2026-08-15, `geopulse_events` 2026-10-28): the Map's disruptions.
- PortWatch, `spillovers_*`, `Spillover_Simulator_Maritime_Connections`: what a port's closure costs elsewhere: the port's detail panel.
- PortWatch, `Daily_Ports_Data` `portcalls_*` by vessel type (the app keeps `portcalls` and cargo only); `portsfacts`: the port sheet.

## Calls made

All 2026-11-05, User-Agent `Deepwater/1.0`, one host at a time, about 1 to 1.5 s apart.

SDMX:
- `GET https://api.imf.org/external/sdmx/3.0/structure/dataflow/*/*/+?detail=allstubs` → 200, 49,486 B, 231 flows
- For each flow, `tools/source-inventory/imf_structures.py`: `GET /3.0/structure/dataflow/<agency>/<id>/+?references=descendants&detail=referencepartial` → 200 (CPI: 1,047,735 B); `GET /2.1/availableconstraint/<agency>,<id>/all/all/all` → 200 (CPI: 2,879 B). 22 flows twice, after a parser fix
- `GET /3.0/availability/dataflow/IMF.STA/CPI/+/*?mode=exact`, `/+/*.*.*.*.*`, `/*/*` → 404
- `GET /2.1/availableconstraint/IMF.STA,CPI,+/all/all` → 200, 2,879 B
- `GET /3.0/structure/dataconstraint/IMF.STA/*/+` → 204
- `GET /3.0/structure/dataflow/IMF.STA/EER/+?references=descendants&detail=referencepartial` → 200
- `HEAD /3.0/structure/dataflow/IMF.STA/CPI/+?detail=allstubs` → 404 (HEAD is not served)
- `GET /3.0/data/dataflow/IMF.STA/CPI/+/KEN.CPI._T.IX.M?c%5BTIME_PERIOD%5D=ge:2026-M01&attributes=none&measures=all` → 200, 1,241 B
- `GET /3.0/data/dataflow/IMF.STA/CPI/+/KEN.CPI._T.IX.M?startPeriod=2026-01&attributes=none&measures=all` → 200, 10,840 B
- `GET /3.0/data/dataflow/IMF.STA/CPI_2026_MAY_VINTAGE/+/KEN.CPI._T.IX.M?c%5BTIME_PERIOD%5D=ge:2026-M01&attributes=none&measures=all` → 200, 850 B, empty
- `GET /2.1/availableconstraint/IMF.STA,CPI_2026_MAY_VINTAGE/all/all/all` → 200, 1,198 B
- `GET /2.1/data/IMF.RES,PCPS/G001.PURAN.USD.M?startPeriod=2026-01&detail=dataonly` → 200, 1,457 B (column names kept)
- `GET https://api.imf.org/robots.txt` → 502
- `GET https://www.imf.org/robots.txt` → 403; `GET https://data.imf.org/robots.txt` → 403

PortWatch, `tools/source-inventory/imf_portwatch.py` and by hand:
- `GET https://services9.arcgis.com/weJ1QsnbMYJlCHdG/ArcGIS/rest/services?f=json` → 200, 37,443 B, 211 services
- `GET /<service>/FeatureServer/layers?f=json`, every service, first pass, three tries each (lower case path): 126 calls ok, 121 "Invalid URL" in HTTP 200; second pass stopped after 15 services, same failures; some answers `{"layers":[],"tables":[]}`, wrong
- `GET /<service>/FeatureServer` and `GET /<service>/FeatureServer/<id>` (HTML), every service, up to six passes → 200; 203 services parsed
- `GET /<service>/FeatureServer/<id>/query?where=1%3D1&returnCountOnly=true&f=json` and `outStatistics` min and max of the first date field, the 57 services around PortWatch → 200, with "Invalid URL" on part of them; both spellings of `ArcGIS` tried
- `GET /Daily_Chokepoints_Data/FeatureServer/0?f=json`, `/0?f=pjson` → 200 "Invalid URL"; `/0/` → 200, "does not exist or is inaccessible"; `/0` → 200, 7,194 B, HTML
- `GET /Daily_Chokepoints_Data/FeatureServer/0/query?where=1%3D0&outFields=year&f=json` → 200, 131 B and 200 "Invalid URL" in turn (the app's own probe)
- `GET /PortWatch_ports_database/FeatureServer/0/query?where=1%3D0&outFields=portid&f=json` → 200, 131 B (the app's own probe)
- `GET /Daily_Chokepoints_Data/FeatureServer/0/query?where=1%3D1&returnCountOnly=true&f=json` → 200, 79,940 rows at the third try; min and max of `date` → 200 at the second and fifth
- `GET /Daily_Chokepoints_Data/FeatureServer/0/query?where=year%3D2026%20AND%20month%3D10&outFields=date,portid,n_total&returnGeometry=false&resultRecordCount=1&f=json` → 200, "Invalid URL" three times, "Object reference not set" once, then 631 B at the fifth try (field names kept)
- `GET /Daily_Ports_Data/FeatureServer/0/query` count, min and max of `date` → 200 at the first try, about 2 s
- `GET /Daily_Trade_Data_REG/FeatureServer/0/query?where=1%3D1&returnDistinctValues=true&outFields=ISO3&returnCountOnly=true&returnGeometry=false&f=json` → 504 after 59 s
- `GET /Monthly_TradeNow/FeatureServer/0/query?where=1%3D1&returnDistinctValues=true&outFields=region&returnGeometry=false&f=json` → 200, "Invalid query parameters"
SDMX, added: `GET /3.0/data/dataflow/IMF.RES/WEO/+/*.POILWTI.A?attributes=none&measures=all` → 200, 2,620 B, one series on `G001`, periods kept, values dropped
