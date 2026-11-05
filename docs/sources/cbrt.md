---
id: cbrt
name: Central Bank of the Republic of Türkiye (EVDS)
rows: [evds]
catalogue: ["Central Bank of the Republic of Turkey"]
access: free key
probed: 2026-11-05
confidence: called     # the catalogue answered through the site's anonymous routes; the keyed API refused, no key used
---

# Central Bank of the Republic of Türkiye

EVDS, the bank's data system: its own series and many from TurkStat, banks, the Treasury and others, in one tree of data groups.

## Contents
The tree of `categories/withDatagroups`: 154 categories, 680 data groups, complete in `catalog/cbrt.tsv` (group, title, category, frequency, source). `serieList/fe` for each group: 54,031 series, complete in `catalog/cbrt_series.tsv.gz` (series, group, name, frequency, default aggregation).

- Top categories (11): Expectation and tendency surveys; Growth, employment, public finance; Exchange rates and precious metals; Price indices; Central bank balance sheet and market data; Payment systems and emission; Balance of payments and external statistics; Monetary and financial statistics; Real sector statistics; International statistics; Archive.
- 228 of the 680 groups and 22,150 series are marked archive. **Check before using a code.**
- Data groups by frequency: monthly 287, quarterly 163, annual 103, weekly on Friday 87, working day 26, daily 10, semi-annual 2, twice monthly 1, weekly on Wednesday 1. Series: monthly 16,581, daily 11,466, annual 9,537, weekly 8,113, quarterly 7,414.
- Sources: CBRT 243 groups, TURKSTAT 158 (and 8 mixed), Banks 74, Ministry of Treasury and Finance 24, Central Registry and Settlement and Custody Bank 12, IMF 8, Freight Forwarders 7, Istanbul Chamber of Commerce 6, Borsa İstanbul 5, others.
- A group's fields: `DATAGROUP_CODE`, `DATAGROUP_TYPE_ENG`, `FREQUENCY_STR`, `DATASOURCE_ENG`, `BIRIMI_EN`, `LAST_UPDATED`, `METADATA_LINK_ENG`, `YEARABLE`...`DAYABLE` (aggregations it allows). A series' fields: `SERIE_CODE`, `SERIE_NAME_ENG`, `DEFAULT_AGG_METHOD`, `AVGABLE`, `SUMABLE`, `LASTABLE`, `SEVIYE`, `UST_SERIE_CODE`.
- Macro groups: `bie_tukfiy2025` CPI 2025=100, `bie_tukfiy2003` CPI general 2003=100, `bie_tukfiyarnd` seasonally adjusted, `bie_oktug2025` specified-coverage CPIs, `bie_urbeka` Survey of Market Participants, `bie_rktufey` real effective exchange rate. First and last dates per series were not collected.

## How to call it
- **`evds2.tcmb.gov.tr` no longer serves the API.** `https://evds2.tcmb.gov.tr/service/evds/series=TP.FG.J0&...` and `/service/evds/categories/type=json` answered a redirect to the home page of `https://evds3.tcmb.gov.tr/`, HTML with a 200. Called without a key; the app's call with one was not tested.
- The API now sits under `https://evds3.tcmb.gov.tr/igmevdsms-dis/`: `series=TP.FG.J0&startDate=01-01-2025&endDate=01-02-2025&type=json` and `categories/type=json` answer 403 `Required request header 'key' is not present`. The path matches the old one, so it is probably the replacement; not confirmed with a key.
- The site's own pages use anonymous routes under the same base, and these answered with no key: `categories/withDatagroups/type=json` (730 KB), `serieList/fe/type=json&code=bie_tukfiy2025`, `faq`, `genel-ayarlar/multiple?keys=...`, `documents/showDocument?docId=21` (terms). The user guide says anonymous users may browse; only members get the web service. These routes served this inventory's catalogue only, are not a documented API, and do not return values here. Do not build on them.
- Terms of use (`docId=21`): data may be used and published if the source is cited; where used in another product, commercial or not, **no additional fee may be charged to its users for it**; the CBRT may change or stop any data without notice. They also describe an **EVDS MCP** service, keyed the same way.

## What the app reads

Corrected on 5 November 2026: the app now calls `evds3.tcmb.gov.tr/igmevdsms-dis/` for `TP.GENENDEKS.T1`. Not tested with a key: none is saved.
- Row `evds` (`src/SourcesPage.cpp:736`), Map: `TP.FG.J0` (`src/KeyedSources.cpp:274`), last 2 years, field `TP_FG_J0` and `Tarih`, key in a `key` header (`src/KeyedSources.cpp:278`).
- Countries' loader for `evds` (`src/CountryData.cpp:1425`, URL `:1428`) has no series in `resources/country_indicators.json`.
- **Two problems, for a check with a key:** the host is the old one (above), and **`TP.FG.J0` is in the archive** (`bie_tukfiy4`, "Consumer Price Index (2003=100) (Archive)"). The live general index is `TP.GENENDEKS.T1` in `bie_tukfiy2003`.

## Not read yet
With a key, for Countries › Türkiye (none at present): `bie_tukfiy2025`, the Survey of Market Participants `bie_urbeka`, exchange rates, Central Bank balance sheet groups, GDP and labour groups from TURKSTAT.

## Calls made
Made on 2026-11-05, sequentially per host with a pause, `User-Agent: Deepwater/1.0`. 715 calls.

- `GET https://evds2.tcmb.gov.tr/service/evds/categories/type=json` → 200, 1.4 KB
- `GET https://evds2.tcmb.gov.tr/robots.txt` → 200, 1.4 KB
- `GET https://www.tcmb.gov.tr/kurlar/today.xml` → 200, 9.1 KB
- `GET https://evds2.tcmb.gov.tr/service/evds/series=TP.FG.J0&startDate=01-01-2025&endDate=01-02-2025&type=json` → 200, 1.4 KB
- `GET https://evds3.tcmb.gov.tr/igmevdsms-dis/categories/type=json` → 403, 73 B
- `GET https://evds3.tcmb.gov.tr/robots.txt` → 200, 1.4 KB
- `GET https://evds3.tcmb.gov.tr/assets/index-BSAVda7t.js` → 200, 1.1 MB
- `GET https://evds3.tcmb.gov.tr/igmevdsms-dis/genel-ayarlar/multiple?keys=DOC_ID_EVDS_KULLANIM_SARTLARI,DOC_ID_EVDS_TERMS_OF_USE,DOC_ID_EVDS_GIZLILIK_POLITIKASI,DOC_ID_EVDS_PRIVACY_POLICY` → 200, 575 B
- `GET https://evds3.tcmb.gov.tr/igmevdsms-dis/documents/showDocument?docId=21` → 200, 97.5 KB
- `GET https://evds3.tcmb.gov.tr/igmevdsms-dis/documents/showDocument?docId=1` → 200, 335.5 KB
- `GET https://evds3.tcmb.gov.tr/igmevdsms-dis/documents/showDocument?docId=20` → 200, 152.9 KB
- `GET https://evds3.tcmb.gov.tr/igmevdsms-dis/documents?lang=en` → 400, 160 B
- `GET https://evds3.tcmb.gov.tr/igmevdsms-dis/categories/withDatagroups/type=json` → 200, 731.1 KB
- `GET https://evds3.tcmb.gov.tr/igmevdsms-dis/serieList/fe/{id}` × 699 → 200: 699, 24.7 MB in all; first `https://evds3.tcmb.gov.tr/igmevdsms-dis/serieList/fe/type=json&code=bie_dtihfb10`
- `GET https://evds2.tcmb.gov.tr/` → 200, 1.4 KB
- `GET https://evds3.tcmb.gov.tr/igmevdsms-dis/faq` → 200, 22.1 KB
- `GET https://evds3.tcmb.gov.tr/igmevdsms-dis/series=TP.FG.J0&startDate=01-01-2025&endDate=01-02-2025&type=json` → 403, 73 B
