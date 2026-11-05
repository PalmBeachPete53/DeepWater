---
id: catalogue-api-audit
name: Audit of the catalogue's `api` field
catalogue: []
access: mixed
probed: 2026-11-05
confidence: called, except where a section says otherwise
---

# Audit of the catalogue's `api` field

`resources/data_sources.json` was not edited: this table is the proposed correction. The field was guessed from name similarity, so all 128 entries with an `api` other than `web` or `web tables` were compared with their country and site. Rows not listed here looked right by that comparison and weren't called in this audit.

| name | country | api claimed | api actual | how checked |
|---|---|---|---|---|
| Aruba Central Bureau of Statistics | ABW | CBS OData API, no key | web | unreached: cbs.aw answers a Sucuri JavaScript challenge (307); claim is the Dutch CBS product |
| Central Bank of Seychelles | SYC | CBS OData API, no key | statistical tables (XLSX), no key | called: statistics page lists XLSX tables |
| Sudan Central Bureau of Statistics | SDN | CBS OData API, no key | web | unreached: cbs.gov.sd does not resolve (DNS); press only |
| Central Statistics Office, Mauritius | MUS | PxStat API, no key | statistical tables (XLS/XLSX), no key | called: subject, monthly and NSDP pages |
| Central Statistics Office, Botswana | BWA | PxStat API, no key | NADA microdata catalogue API (JSON), no key; bulletins (PDF) | called: NADA search answered JSON; publications page |
| Central Statistics Office | BWA | PxStat API, no key | NADA microdata catalogue API (JSON), no key; bulletins (PDF) | same body and site as the row above |
| Central Statistical Office, Trinidad and Tobago | TTO | PxStat API, no key | statistical tables (XLSX), no key | called: National Accounts page lists XLSX |
| Central Statistical Office | TTO | PxStat API, no key | statistical tables (XLSX), no key | same body and site as the row above |
| Central Statistics Office, Yemen | YEM | PxStat API, no key | web | unreached: catalogue link is now an unrelated culture blog; cso-yemen.com refused (403) |
| Ministry of Finance, Yemen | YEM | PxStat API, no key | web | unreached: catalogue link is the same blog; mof.gov.ye fails certificate verification |
| Central Statistics Organization of Afghanistan | AFG | PxStat API, no key | web | unreached: cso.gov.af does not resolve; nsia.gov.af fails certificate verification |
| Institut National de la Statistique du Tchad (INSEED) | TCD | API Melodi (SDMX-JSON), no key | site API (JSON, undocumented), no key; NADA microdata catalogue API (JSON), no key | called: site backend and NADA search answered JSON |
| Institut National de la Statistique, des Etudes Economiques et Démographiques (INSEED), Tchad | TCD | API Melodi (SDMX-JSON), no key | site API (JSON, undocumented), no key; NADA microdata catalogue API (JSON), no key | same body and site |
| Institut National de la Statistique, des Etudes Economiques et Démographiques | TCD | API Melodi (SDMX-JSON), no key | site API (JSON, undocumented), no key; NADA microdata catalogue API (JSON), no key | same body and site |
| National Institute of Statistics, Economic and Demographic Studies of Chad (INSEED) | TCD | API Melodi (SDMX-JSON), no key | site API (JSON, undocumented), no key; NADA microdata catalogue API (JSON), no key | same body; this entry has no link |
| INSEED Comoros | COM | API Melodi (SDMX-JSON), no key | NADA microdata catalogue API (JSON), no key | called: NADA search answered JSON |
| INE, Cape Verde | CPV | INEbase JSON API, no key | web | called: JavaScript site; backend answers themes, 401 on indicators and statistics |
| INE - Instituto Nacional de Estatistica de Angola | AGO | INEbase JSON API, no key | statistical tables (XLSX), no key | called: publication pages carry PDF, note and Excel |
| INE, Sao Tome and Principe | STP | INEbase JSON API, no key | web | called: document library, no file formats visible in the pages read |
| INE Honduras | HND | INEbase JSON API, no key | microdata files (ZIP) and REDATAM portal, no key | called: Bases de Datos page, one REDATAM portal page |
| INE - Instituto Nicaragüense de Energía | NIC | INEbase JSON API, no key | web | called: an energy regulator; prices on the page, rules and tariffs in PDF |
| Treasury Department, Ministry of Finance and Economy | BRN | Fiscal Data API and daily yield curve CSV, no key | web | called: mofe.gov.bn redirects to mof.gov.bn; no data tables found on the pages read |
| Department of the Treasury of Puerto Rico | PRI | Fiscal Data API and daily yield curve CSV, no key | statistical tables (PDF), no key | called: revenue pages list PDF |
| Central Bureau of Statistics, Israel | ISR | CBS OData API, no key | CBS price index API (XML), no key | docs only: third-party descriptions; every call to its hosts timed out |
| Food and Agriculture Organization of the United Nations | WLD | UN Comtrade API, free key (limited); UNdata, no key | Food Price Index files (CSV/XLSX), no key; FAOSTAT API, authorization required | called: index page lists CSV and Excel; FAOSTAT API answered 401 |
| United Nations Economic Commission for Europe (UNECE) | multi | UN Comtrade API, free key (limited); UNdata, no key | PxWeb API, no key | called: PXWeb2015 API answered JSON |
| United Nations Refugee Agency | JPN,TUR | UN Comtrade API, free key (limited); UNdata, no key | Refugee Statistics API (JSON), no key | called: api.unhcr.org answered JSON and serves its docs |
| Freddie Mac | USA | FRED API, free key | PMMS and house price index files (XLSX/CSV), no key | called: PMMS and FMHPI pages link the files |
| BelgoStat | BEL | open data (CSV), no key | NBB.Stat (CSV), no key | partly: belgostat.be redirects (301) to stat.nbb.be, which reset the connection |
| Federal Reserve Bank of New York | USA,WLD | Data Download Program (CSV, no key); FRED API, free key | Markets Data API (JSON), no key | called: markets.newyorkfed.org answered JSON |
| Federal Reserve Bank of Cleveland | USA | Data Download Program (CSV, no key); FRED API, free key | data files (CSV), no key | called: Median CPI page links CSV |
| Federal Reserve Bank of Atlanta | USA | Data Download Program (CSV, no key); FRED API, free key | data files (XLSX/CSV), no key | called: GDPNow page links the files |
| Federal Reserve Bank of Philadelphia | USA | Data Download Program (CSV, no key); FRED API, free key | data files (XLSX), no key | called: Survey of Professional Forecasters page |
| Federal Reserve Bank of San Francisco | USA | Data Download Program (CSV, no key); FRED API, free key | data files (CSV/XLSX), no key | called: Daily News Sentiment page |
| Federal Reserve Bank of Richmond | USA | Data Download Program (CSV, no key); FRED API, free key | reports (PDF), no key | called: one page only, National Economic Indicators |
| Federal Reserve Bank of Dallas | USA | Data Download Program (CSV, no key); FRED API, free key | *not established, no correction proposed* | called: data page has no file links in its HTML; format not established |
| Federal Reserve Bank of Chicago | USA | Data Download Program (CSV, no key); FRED API, free key | *not established, no correction proposed* | called: NFCI page has no file links in its HTML; format not established |
| Federal Reserve Bank of Kansas City | USA | Data Download Program (CSV, no key); FRED API, free key | *not established, no correction proposed* | unreached: data page timed out |

**Reading the table.** 38 entries carry an `api` that isn't theirs: 29 name another body's system outright, and 9 Reserve Banks claim the Board of Governors' Data Download Program. 35 have a correction, in `catalog/api_corrections.tsv` (name, corrected api). `web` is the catalogue's own value for a body with no known machine access; on an *unreached* row it means the claim is rejected because the product belongs to someone else, not that the site was inspected.

**Limits of the corrections.**
- Reserve Banks: the rows drop "FRED API" because FRED timed out, so which of their series it carries wasn't checked. Some probably are there; add it back after checking.
- Richmond rests on one page. Dallas, Chicago and Kansas City get no correction.
- Israel rests on third-party descriptions only.
- BelgoStat also has the wrong link: `url` points at Statbel, while belgostat.be redirects to the National Bank's stat.nbb.be.
- `FRED` (YEM) is not wrong and isn't in the table. See its section.

**Not established because of a permission refusal.** `mof.gov.ye` and `nsia.gov.af` fail certificate verification ("unable to get local issuer certificate"). Reading them without verification was refused by the session's permission system, so neither was read, and no other route was tried. Their contents below come from search results only.

## Links to correct

| name | catalogue `url` | finding |
|---|---|---|
| Central Statistics Office, Yemen | http://www.cso-yemen.org | Now an unrelated culture and travel blog, though it passes the catalogue's `test`. UN and Springer references name this address for the CSO, so the domain changed hands. Real site not confirmed. |
| Ministry of Finance, Yemen | http://www.cso-yemen.org | Same blog. Candidate: mof.gov.ye (certificate failure). |
| Central Statistics Organization of Afghanistan | http://cso.gov.af | Does not resolve. The body is now the National Statistics and Information Authority, nsia.gov.af (certificate failure). |
| Sudan Central Bureau of Statistics | http://cbs.gov.sd | Does not resolve, with or without www. No replacement found. |
| Treasury Department, Ministry of Finance and Economy | https://www.mofe.gov.bn/ | Redirects to https://www.mof.gov.bn/. |
| INE Honduras | http://www.ine.gob.hn | Redirects to https://ine.gob.hn/. |
| BelgoStat | https://statbel.fgov.be | Should be https://stat.nbb.be (it reset the connection here). |
| United Nations Refugee Agency | http://popstats.unhcr.org | Timed out. The API lives at https://api.unhcr.org/. |
| National Institute of Statistics, Economic and Demographic Studies of Chad (INSEED) | *(empty)* | https://www.inseed.td/ |

# The bodies

Probed 2026-11-05. None of these hosts appears in `src/` except the New York and Philadelphia Feds, whose existing uses (`src/SourcesPage.cpp`, `src/SupplyChain.cpp`, `src/ResearchData.cpp`, `src/DeskViewsMacro.cpp`) weren't reviewed here. For the rest, *What the app reads* is nothing, and that heading is left out.

## Aruba Central Bureau of Statistics
Confidence: refused.
**Contents:** not read. Search results show monthly CPI press releases in PDF, as copies on third-party sites.
**How to call it:** `cbs.aw` and `www.cbs.aw` answer 307 with a Sucuri JavaScript challenge that sets a cookie. It was not solved, because that would mean getting past the block. No API evidence either way.

## Central Bank of Seychelles
Confidence: called.
**Contents:** four sectors (monetary and financial, external, real, government). `Statistics/StatisticsData.html` lists 20 XLSX tables: Monetary Survey, Interest Rates, Credit Distribution, Deposit Distribution, Financial Soundness Indicators, Money Multiplier, Balance of Payments, International Investment Position, External Debt, Merchandise Trade, Trade In Services, Exchange Rates (Monthly, Daily), International Reserves Template, Gross Official Reserves, Tourism, Prices, GDP, Government Finance, Other Economic Indicators. Auction tables link out: Deposit Auction Arrangement, Foreign Exchange Auction, Interbank FX Market, Treasury Bills. PDF series: Annual Reports, Monthly Reviews, Key Economic Developments, Statistical Bulletins, Monetary Policy Reports, Financial Surveillance Report. Survey: Financial Indicators Expectations for households. Frequency isn't stated per table, except for the daily and monthly exchange rates.
**How to call it:** `https://www.cbs.sc/Downloads/StaExcel/<Title>.xlsx`, no key. No published API. The home page feeds itself from `Controller/IndexController.jsp`, undocumented JSON of the latest values only: `mrr`, `reserve`, `autnotes`, `autdrfts`, `lendingrate`, `tBillRate`, `prevInflationRateResult`, `chartData`, `recentupdates`. It may change without notice.
**Not read yet:** BOP, reserves and FSI tables could feed a country sheet.

## Sudan Central Bureau of Statistics
Confidence: refused (no DNS), press and bibliographic records only.
**Contents:** press reports attribute monthly CPI and inflation releases to the bureau. A bibliographic database lists a Monthly Bulletin of Consumer Price Index by state and a "Sudan in Figures" series. Nothing confirmed at source.
**How to call it:** no working host found.

## Central Statistics Office, Mauritius (Statistics Mauritius)
Confidence: called.
**Contents:** 31 subject pages under `Pages/Statistics/By_Subject/`, including agriculture, construction, CPI, businesses, education, energy and water, environment, external trade, labour, manufacturing, migration, national accounts, population, poverty, public finance, Rodrigues, SDGs, social security, tourism, transport. Monthly series under `Statistics/Monthly/` (CPI, PPI manufacturing, trade, social security, tourists). Economic and Social Indicators releases (CPI, national accounts, quarterly accounts, labour), each in DOC, XLS or XLSX and PDF. National accounts history runs from 1948 by annual block, quarterly from Q1 1999, with supply-use and input-output tables. The NSDP page (`NSDP/Pages/nsdp.aspx`) gives real, fiscal, financial and external sector series as XLS or XLSX, plus methodological notes; some items link to the Bank of Mauritius.
**How to call it:** static files whose names carry the release date (`Trade_M_Aug26_161026.xlsx`), so they must be discovered from the listing page rather than predicted. No API. `data.govmu.org` (the government portal) disallows this client in robots.txt and wasn't read.
**Not read yet:** the NSDP page is the densest single entry point.

## Central Statistics Office, Botswana (two names)
Confidence: called; the national data portal refused.
**Contents:** Statistics Botswana. 61 listing pages of publications (`latest-publications?page=60` is the last): monthly CPI, merchandise trade digest and food and beverage imports; quarterly GDP, PPI, mining index, water, electricity, transport; annual tourism, mortality and environment reports. Mostly PDF, with one rebased CPI XLS linked from the home page. Microdata catalogue: 47 studies, such as the Quarterly Multi-Topic Household Survey.
**How to call it:** `GET https://microdata.statsbots.org.bw/index.php/api/catalog/search?ps=5` returns JSON with `found`, `rows[id, idno, title, nation, year_start, year_end, varcount, ...]`, no key. This describes surveys, not macro series. `botswana.opendataforafrica.org`, the portal the site names for statistics, answered 403 with a Cloudflare challenge on the page and on `/api/1.0/meta/dataset`; `cb-botswana.opendataforafrica.org` (NSDP) refused robots.txt with 403. A Power BI dashboard is linked but wasn't read.

## Central Statistical Office, Trinidad and Tobago (two names)
Confidence: called.
**Contents:** subjects: population and vital statistics, crime, education, traffic, environment, BMI, CPI, PPI, retail sales, travel and tourism, national accounts, business and industry, international trade, labour force, agriculture. National Accounts lists quarterly and annual GDP at constant and current prices as TT$ millions, percentage change and contribution, plus summary sheets, all XLSX, under `wp-content/uploads/<year>/<month>/`. The Publications page is older PDF reports. The MICS and SDG portal is Data NeTT (`datanett.info`, redirected from `cso.gov.tt/data/`), with a JavaScript visualizer at `visualizer.datanett.info`.
**How to call it:** XLSX by URL from the subject page, no key. No statistical API. WordPress's `/wp-json/` answers and lists site routes (one is `/wp/v2/core_statistics`, not called); that is site plumbing, not a data service.

## Central Statistics Office, Yemen
Confidence: refused, references only.
**Contents:** references describe an annual statistical yearbook and "Yemen in Figures"; the Arab Development Portal hosts digitised editions (2003, 2006, 2011 seen in search results). Current output not confirmed.
**How to call it:** the catalogue link is no longer the body. `cso-yemen.com` (a guess) refused robots.txt with 403, so it wasn't read and its owner is unconfirmed.

## Ministry of Finance, Yemen
Confidence: refused.
**Contents:** not read. The entry's link is the CSO's old domain, now the blog above.
**How to call it:** `mof.gov.ye` and `www.mof.gov.ye` fail certificate verification.

## Central Statistics Organization of Afghanistan
Confidence: refused, secondary sources only.
**Contents:** now the National Statistics and Information Authority. Search results show the Afghanistan Statistical Yearbook (2019 and 2020 editions seen, on third-party hosts) and a Trade Statistics Yearbook. Newer editions not confirmed.
**How to call it:** `cso.gov.af` does not resolve; `nsia.gov.af` fails certificate verification.

## INSEED Chad (four names)
Confidence: called.
**Contents:** 26 indicators: 6 annual national accounts, 1 quarterly GDP, 4 monthly IHPC, 15 demographic and social (census RGPH2-2009, MICS6-2019, ECOSIT4-2018). 146 publications, in PDF on the entries sampled, in three menus: economic (economic bulletins for ICA, CPI, industrial production, construction cost, quarterly notes; annual and quarterly accounts; foreign trade), social and demographic (ECOSIT, EDS, MICS, TBES), yearbooks (national, health, education, higher education, agriculture). Microdata archive: 8 studies. Portal list: 7 links, among them OpenDataForAfrica, ANAD, Goal Tracker, PND, Tchad Data.
**How to call it:** the site is a JavaScript application over `https://inseed.ssn-tchad.td/api`, no key for reads: `/indicateurs` (fields `libelle`, `typeIndicateur`, `periodicite`, `sourcePrincipale`, `uniteDeMesure`), `/statistiques-menu/tree`, `/publications?limit=5` (`total`, `pages`, `fichier.url`), `/data-portal`. It is undocumented and could change. `/indicateurs/with-latest-obs` appears in the site's code and should return values; it wasn't called. Microdata: `GET https://anad.inseed.td/index.php/api/catalog/search?ps=2`.
**Not read yet:** monthly IHPC and quarterly GDP indicators, the only Chad series with a real endpoint found here. Ask the institute about stability before relying on it.

## INSEED Comoros
Confidence: called.
**Contents:** publication database (`/publications`, filters `theme`, `collection`): monthly IHPC bulletin, annual national accounts, the EHCVM household survey, demographic statistics. Microdata catalogue: 15 studies, including the agricultural census RGA-2 2025 and EHCVM 2024.
**How to call it:** `GET https://www.nada.inseed-comores.org/index.php/api/catalog/search?ps=5`, JSON, no key, surveys only. Publication file formats weren't established.

## INE, Cape Verde
Confidence: called, partly closed.
**Contents:** 14 themes: environment, foreign trade, economic conditions, culture and sport, national accounts, businesses, gender, governance, CPI, infographics, labour market, population and living conditions, tourism, other. The site also has routes for indicators, a calendar and censuses.
**How to call it:** a JavaScript site over `https://bdmi.ine.cv/site_deploy_api/api`. `/Theme` answers JSON with no key; `/Publication` answered an empty page without parameters; `/Indicator` and `/Statistic` answer 401. That is a login boundary and was left alone, so this isn't a usable API. Its file formats weren't established.

## INE Angola
Confidence: called.
**Contents:** monthly CPI national (IPCN), wholesale prices (IPG), producer prices (IPP), construction materials prices (IPMC), industrial production (IPI), foreign trade (ECE); employment survey yearbooks by province; 2014 and 2024 censuses; a publication calendar. A publication detail page offers the publication, a methodology note and an Excel file, in PDF and XLSX.
**How to call it:** listing at `/publicacoes/todas`, detail at `/publicacoes/detalhes/<base64 id>`, files at `/Arquivos/arquivosCarregados//Carregados/Publicacao_<number>.xlsx`, so names can't be predicted. No API. Other data by request form (`/pedido-de-dados/criar`).

## INE, Sao Tome and Principe
Confidence: called.
**Contents:** themes: demography, economic conditions, foreign trade, tourism, CPI, employment, education, economy. Surveys: IOF, IDS, MICS, QUIBB. Document library categories: atlas, statistical information, surveys, locality projections, censuses, ENDE, STPemNumeros, business census, newsletter. Frequency not stated on the pages read.
**How to call it:** a Joomla document library (`/index.php/publicacao/documentos/category/<id>`). One PDF preview seen; other formats not established because links have no extension. Data by request form at `app.ine.st`. No API.

## INE Honduras
Confidence: called.
**Contents:** household survey EPHPM (yearly microdata), ENDESA/MICS, migration and remittances, specialised surveys; censuses of population (2013, 2025), agriculture 2024, cooperatives 2025; Statistical Yearbook; foreign trade yearbook; national accounts and other categories on the site; population projections.
**How to call it:** microdata ZIP files in `wp-content/uploads/`, listed at `/bases-de-datos/`. REDATAM portal at `http://181.115.7.199/binhnd/RpWebEngine.exe/Portal?BASE=<EPH2017|PROYPOB|CPVHND2001...>`, an interactive query tool, not an API. Two dashboards (`dashboardendesa`, `dashboardenesvmn`). WordPress `/wp-json/` lists categories only.

## Instituto Nicaragüense de Energía
Confidence: called.
**Contents:** an energy regulator, not a statistical institute; the catalogue's `INE` match is a coincidence of initials. Official fuel and LPG prices in Managua on the home page, weekly price monitoring, electricity rules and tariffs (*pliegos tarifarios*), subsidies, in HTML and PDF.
**How to call it:** pages only, no API.

## Treasury Department, Ministry of Finance and Economy, Brunei
Confidence: called.
**Contents:** the Treasury pages cover its role, the TAFIS accounting system, forms and past exam papers. No statistical tables found on the pages read. National statistics sit with the Department of Economic Planning and Statistics (`deps.mofe.gov.bn` redirects to `www.deps.gov.bn`, PDF links seen).
**How to call it:** no API.

## Department of the Treasury of Puerto Rico
Confidence: called.
**Contents:** under *Estadísticas y Recaudos*: General Fund net revenues (monthly by fiscal year), sales and use tax revenues, gaming revenues, alcoholic beverage production and imports, crude oil and derivatives revenues, individual and corporate income tax return statistics. Also financial statements, revenue press releases, tax expenditure reports, a sales tax economic indicators panel.
**How to call it:** PDF by URL, with irregular names, from the listing pages. No API. The Fiscal Data API belongs to the U.S. Treasury and doesn't cover this department's series.

## FRED (entry with country YEM)
Confidence: refused (timed out).
**Contents:** not read. Its `api`, "FRED API, free key", is FRED's own product, so the attribution isn't wrong; the odd part is `countries: [YEM]` on a worldwide database, and whether FRED carries Yemen series wasn't confirmed. Worth checking whether this row duplicates the St. Louis Fed row.
**How to call it:** `fred.stlouisfed.org` timed out on three calls with `Deepwater/1.0`. The project's memory notes that FRED's CSV endpoint refuses the app's User-Agent, which may be related; no other identity was tried, and the app's saved key wasn't used.

## Central Bureau of Statistics, Israel
Confidence: docs, third-party descriptions, with why: `www.cbs.gov.il/en/Pages/Api.aspx`, `apis.cbs.gov.il` and `api.cbs.gov.il` all timed out here, though the bureau's home page answered the earlier site probe.
**Contents:** price index chapters: CPI, housing, producer prices, construction inputs.
**How to call it:** reported routes: `api.cbs.gov.il/index/catalog/catalog` and `/index/data/price`, XML reported, JSON unconfirmed. It is the bureau's own API, not OData, which is the Dutch CBS. Verify before use.

## Food and Agriculture Organization
Confidence: called.
**Contents:** Food Price Index page: nominal and real indices from 1990 (monthly and annual, Excel), nominal from 1990 (monthly, CSV), nominal and real from 1961 (annual, Excel). FAOSTAT is separate.
**How to call it:** files from `fao.org/worldfoodsituation/foodpricesindex/en`, which is also the entry's link. FAOSTAT's `faostatservices.fao.org/api/v1/en/definitions/domain/` answered 401 "Missing Authorization Header". `bulks-faostat.fao.org` refused robots.txt with 403.

## United Nations Economic Commission for Europe
Confidence: called.
**Contents:** 8 top-level folders: Country Profiles, Economy, Forestry, Millennium Development Goals, Population and Gender, Sustainable Development Goals, Thematic Overviews, Transport.
**How to call it:** `GET https://w3.unece.org/PXWeb2015/api/v1/en/STAT/`, JSON (`id`, `type`, `text`), no key. Walk down the folders from there.

## United Nations Refugee Agency
Confidence: called.
**Contents:** Refugee Statistics API, with an OpenAPI page at `api.unhcr.org/docs/refugee-statistics.html`. `www.unhcr.org/refugee-statistics/` answered 403.
**How to call it:** `GET https://api.unhcr.org/population/v1/years/?limit=1`, JSON (`page`, `maxPages`, `items`), no key. The entry's `countries` (JPN, TUR) looks arbitrary for a worldwide body.

## Freddie Mac
Confidence: called.
**Contents:** Primary Mortgage Market Survey, weekly since 1971 by the page's wording (`pmms/docs/historicalweeklydata.xlsx`). House Price Index master file (`fmac-resources/research/docs/fmhpi_master_file.csv`). Also listed: Single Family Loan-Level Dataset, Cost of Funds Index.
**How to call it:** files by URL, no API seen. FRED's name resembles Freddie's, and FRED is a separate St. Louis Fed product.

## BelgoStat
Confidence: partly called.
**Contents:** not read. `belgostat.be` redirects to `stat.nbb.be`, so this is the National Bank of Belgium's database, not Statbel.
**How to call it:** `stat.nbb.be` reset the connection. The correction reuses the string on the National Bank of Belgium row.

## Federal Reserve Banks (nine entries)
Confidence: called, except Kansas City (timed out).
**Contents:** the Data Download Program belongs to the Board of Governors. No bank page read links to it except Cleveland's Median CPI page, which mentions it. What each bank holds:
- **New York:** reference rates and other market data.
- **Cleveland:** median CPI, median PCE, inflation expectations, inflation nowcasting (CSV).
- **Atlanta:** GDPNow, market probability tracker (XLSX, CSV).
- **Philadelphia:** Survey of Professional Forecasters, Livingston Survey, real-time data (XLSX, XLS, PDF).
- **San Francisco:** Daily News Sentiment, total factor productivity, rate probability distributions (CSV, XLSX).
- **Richmond:** national economic indicators (PDF on the page read).
- **Dallas:** regional, U.S., international and energy data, Database of Global Economic Indicators, International House Price Database; no file links in the HTML read.
- **Chicago:** CFNAI, NFCI, labour market indicators, regional surveys; no file links in the HTML read.
**How to call it:** only New York has an API confirmed here: `GET https://markets.newyorkfed.org/api/rates/all/latest.json`, JSON, no key.

# Calls made

2026-11-05, sequential with a 2 s pause, `User-Agent: Deepwater/1.0`, 15 to 25 s timeouts, robots.txt read first on each host. A robots.txt answering 401 or 403 was treated as a refusal. `SKIP` means not called because of robots.txt. `ERR` means no HTTP answer. Figures fetched were not kept here; JSON answers were read for field names and types, and labels and counts only.

Search (no call to the body): Yemen CSO; NSIA Afghanistan; Sudan CBS; Aruba CBS; Israel CBS API. Results were used only where a section says so.

Attempted and refused by the session's permission system, so no request went out: reading `https://mof.gov.ye/` and `https://nsia.gov.af/` without certificate verification.

| url | status | bytes | note |
|---|---|---|---|
| http://www.cbs.aw/robots.txt | 301 | 166 |  |
| https://www.cbs.aw/robots.txt | 404 | 355 |  |
| http://www.cbs.aw | 307 | 1347 |  |
| https://cbs.aw/robots.txt | 404 | 355 |  |
| https://cbs.aw/wp/ | 307 | 1359 |  |
| https://www.cbs.sc/robots.txt | 404 | 3803 |  |
| https://www.cbs.sc/ | 200 | 46433 |  |
| https://www.cbs.sc/robots.txt | 404 | 3803 |  |
| https://www.cbs.sc/Statistics/Survey.html | 200 | 7542 |  |
| https://www.cbs.sc/marketinfo/CAR.html | 200 | 8100 |  |
| https://www.cbs.sc/Publications/ | 404 | 3803 |  |
| https://www.cbs.sc/Statistics/ | 404 | 3803 |  |
| https://www.cbs.sc/robots.txt | 404 | 3803 |  |
| https://www.cbs.sc/js/app-index.js | 200 | 12205 |  |
| https://www.cbs.sc/robots.txt | 404 | 3803 |  |
| https://www.cbs.sc/Menu/menu.html | 200 | 25381 |  |
| https://www.cbs.sc/robots.txt | 404 | 3803 |  |
| https://www.cbs.sc/Controller/IndexController.jsp | 200 | 4536 |  |
| https://www.cbs.sc/robots.txt | 404 | 3803 |  |
| https://www.cbs.sc/Statistics/StatisticsData.html | 200 | 10698 |  |
| https://www.cbs.sc/Statistics/StatisticsOverview.html | 200 | 6027 |  |
| https://www.cbs.sc/Publications/StatisticalBulletins.html | 200 | 5902 |  |
| http://cbs.gov.sd/robots.txt | ERR | 0 | URLError:<urlopen error [Errno 8] nodename nor servname provided, or n |
| http://cbs.gov.sd | ERR | 0 | gaierror:[Errno 8] nodename nor servname provided, or not known |
| https://cbs.gov.sd/robots.txt | ERR | 0 | URLError:<urlopen error [Errno 8] nodename nor servname provided, or n |
| https://cbs.gov.sd/ | ERR | 0 | gaierror:[Errno 8] nodename nor servname provided, or not known |
| https://cbs.gov.sd/en/ | ERR | 0 | gaierror:[Errno 8] nodename nor servname provided, or not known |
| http://www.cbs.gov.sd/robots.txt | ERR | 0 | URLError:<urlopen error [Errno 8] nodename nor servname provided, or n |
| http://www.cbs.gov.sd/ | ERR | 0 | gaierror:[Errno 8] nodename nor servname provided, or not known |
| https://statsmauritius.govmu.org/robots.txt | 404 | 666 |  |
| https://statsmauritius.govmu.org/SitePages/Index.aspx | 200 | 117773 |  |
| https://statsmauritius.govmu.org/Pages/Statistics/By_Subject/By_Subject.aspx | 404 | 714 |  |
| https://statsmauritius.govmu.org/robots.txt | 404 | 666 |  |
| https://statsmauritius.govmu.org/Pages/Statistics/statsbysubj.aspx | 200 | 81953 |  |
| https://statsmauritius.govmu.org/Pages/Statistics/ESI/CPI/CPI_3Qtr26.aspx | 200 | 70688 |  |
| https://data.govmu.org/robots.txt | 200 | 4125 |  |
| https://data.govmu.org/ | SKIP | 0 | robots disallow |
| https://statsmauritius.opendataforafrica.org/robots.txt | ERR | 0 | URLError:<urlopen error [Errno 8] nodename nor servname provided, or n |
| https://statsmauritius.opendataforafrica.org/ | ERR | 0 | gaierror:[Errno 8] nodename nor servname provided, or not known |
| https://statsmauritius.govmu.org/robots.txt | 404 | 666 |  |
| https://statsmauritius.govmu.org/NSDP/Pages/nsdp.aspx | 200 | 991761 |  |
| https://statsmauritius.govmu.org/Pages/Statistics/Monthly/Monthly-Trade.aspx | 200 | 75201 |  |
| https://statsmauritius.govmu.org/Pages/Statistics/By_Subject/National_Accounts/SB_National_Accounts.aspx | 200 | 127467 |  |
| https://www.statsbots.org.bw/robots.txt | 200 | 2189 |  |
| https://www.statsbots.org.bw/ | 200 | 195680 |  |
| https://www.statsbots.org.bw/data-portal | 200 | 37120 |  |
| https://www.statsbots.org.bw/latest-publications | 200 | 60457 |  |
| https://cso.gov.tt/robots.txt | 200 | 31 |  |
| https://cso.gov.tt/ | 200 | 384623 |  |
| https://cso.gov.tt/data/ | 301 | 0 |  |
| https://cso.gov.tt/data-nett/ | 301 | 0 |  |
| https://datanett.info/robots.txt | 200 | 2027 |  |
| https://datanett.info/ | 200 | 27615 |  |
| https://botswana.opendataforafrica.org/robots.txt | 200 | 372 |  |
| https://botswana.opendataforafrica.org/ | 403 | 5474 |  |
| https://botswana.opendataforafrica.org/api/1.0/meta/dataset | 403 | 5555 |  |
| https://cb-botswana.opendataforafrica.org/robots.txt | 403 | 5528 |  |
| https://cb-botswana.opendataforafrica.org/nanhvtg/national-summary-data-page-nsdp | SKIP | 0 | robots.txt refused |
| https://microdata.statsbots.org.bw/robots.txt | 404 | 1245 |  |
| https://microdata.statsbots.org.bw/index.php/api/catalog/search?ps=5 | 200 | 2951 |  |
| https://microdata.statsbots.org.bw/ | 200 | 0 |  |
| https://cso.gov.tt/robots.txt | 200 | 31 |  |
| https://cso.gov.tt/subjects/national-accounts/ | 200 | 386538 |  |
| https://cso.gov.tt/publications/ | 200 | 302981 |  |
| https://cso.gov.tt/wp-json/ | 200 | 773648 |  |
| https://visualizer.datanett.info/robots.txt | 404 | 7001 |  |
| https://visualizer.datanett.info/en | 200 | 14801 |  |
| http://www.cso-yemen.org/robots.txt | 301 | 0 |  |
| http://cso-yemen.org/robots.txt | 200 | 117 |  |
| http://www.cso-yemen.org | 301 | 0 |  |
| https://cso-yemen.org/robots.txt | 200 | 117 |  |
| https://cso-yemen.org/ | 200 | 232974 |  |
| https://www.cso-yemen.com/robots.txt | 403 | 4571 |  |
| https://www.cso-yemen.com/ | SKIP | 0 | robots.txt refused |
| https://cso-yemen.com/robots.txt | 403 | 4571 |  |
| https://cso-yemen.com/ | SKIP | 0 | robots.txt refused |
| https://www.mof.gov.ye/robots.txt | ERR | 0 | URLError:<urlopen error [SSL: CERTIFICATE_VERIFY_FAILED] certificate v |
| https://www.mof.gov.ye/ | ERR | 0 | SSLCertVerificationError:[SSL: CERTIFICATE_VERIFY_FAILED] certificate  |
| https://mof.gov.ye/robots.txt | ERR | 0 | URLError:<urlopen error [SSL: CERTIFICATE_VERIFY_FAILED] certificate v |
| https://mof.gov.ye/ | ERR | 0 | SSLCertVerificationError:[SSL: CERTIFICATE_VERIFY_FAILED] certificate  |
| http://cso.gov.af/robots.txt | ERR | 0 | URLError:<urlopen error [Errno 8] nodename nor servname provided, or n |
| http://cso.gov.af | ERR | 0 | gaierror:[Errno 8] nodename nor servname provided, or not known |
| https://nsia.gov.af/robots.txt | ERR | 0 | URLError:<urlopen error [SSL: CERTIFICATE_VERIFY_FAILED] certificate v |
| https://nsia.gov.af/ | ERR | 0 | SSLCertVerificationError:[SSL: CERTIFICATE_VERIFY_FAILED] certificate  |
| https://www.nsia.gov.af/robots.txt | ERR | 0 | URLError:<urlopen error [SSL: CERTIFICATE_VERIFY_FAILED] certificate v |
| https://www.nsia.gov.af/home | ERR | 0 | SSLCertVerificationError:[SSL: CERTIFICATE_VERIFY_FAILED] certificate  |
| https://www.inseed.td/robots.txt | 200 | 789 |  |
| https://www.inseed.td/index.php | 200 | 3239 |  |
| https://www.inseed-comores.org/robots.txt | 404 | 77862 |  |
| https://www.inseed-comores.org/ | 200 | 242467 |  |
| https://ine.cv/robots.txt | 200 | 10697 |  |
| https://ine.cv/ | 200 | 10697 |  |
| https://www.ine.gov.ao/robots.txt | 404 | 0 |  |
| https://www.ine.gov.ao/ | 200 | 150179 |  |
| http://www.ine.st/robots.txt | 301 | 795 |  |
| https://www.ine.st/robots.txt | 200 | 842 |  |
| http://www.ine.st | 301 | 795 |  |
| https://www.ine.st/robots.txt | 200 | 842 |  |
| https://www.ine.st/ | 200 | 71759 |  |
| https://www.nada.inseed-comores.org/robots.txt | 404 | 290 |  |
| https://www.nada.inseed-comores.org/index.php/api/catalog/search?ps=5 | 200 | 3623 |  |
| https://www.inseed-comores.org/robots.txt | 404 | 77862 |  |
| https://www.inseed-comores.org/publications?theme=105 | 200 | 119437 |  |
| https://www.ine.gov.ao/robots.txt | 404 | 0 |  |
| https://www.ine.gov.ao/publicacoes/todas | 200 | 94753 |  |
| https://www.ine.gov.ao/publicacoes/detalhes/NTg1MTk%3D | 200 | 119195 |  |
| https://www.inseed.td/robots.txt | 200 | 789 |  |
| https://www.inseed.td/assets/index-CYU6Yl47.js | 200 | 1615872 |  |
| https://ine.cv/robots.txt | 200 | 10697 |  |
| https://ine.cv/main.93862c2013bb61af.js | 200 | 1421929 |  |
| https://inseed.ssn-tchad.td/robots.txt | 404 | 162 |  |
| https://inseed.ssn-tchad.td/api/indicateurs | 200 | 11924 |  |
| https://inseed.ssn-tchad.td/api/statistiques-menu/tree | 200 | 6343 |  |
| https://inseed.ssn-tchad.td/api/publications?limit=5 | 200 | 2843 |  |
| https://inseed.ssn-tchad.td/api/data-portal | 201 | 1092 |  |
| https://bdmi.ine.cv/robots.txt | 404 | 12554 |  |
| https://bdmi.ine.cv/site_deploy_api/api/Indicator | 401 | 0 |  |
| https://bdmi.ine.cv/site_deploy_api/api/Statistic | 401 | 0 |  |
| https://bdmi.ine.cv/site_deploy_api/api/Theme | 200 | 5204 |  |
| https://bdmi.ine.cv/site_deploy_api/api/Publication | 200 | 84 |  |
| https://www.ine.st/robots.txt | 200 | 842 |  |
| https://www.ine.st/index.php/informacoes-estatisticas/ipc | 200 | 39567 |  |
| https://www.ine.st/index.php/publicacao/documentos | 200 | 44625 |  |
| http://www.ine.gob.hn/robots.txt | 200 | 31 |  |
| http://www.ine.gob.hn | 301 | 0 |  |
| https://ine.gob.hn/robots.txt | 200 | 31 |  |
| https://ine.gob.hn/ | 200 | 243186 |  |
| https://anad.inseed.td/robots.txt | 404 | 355 |  |
| https://anad.inseed.td/index.php/api/catalog/search?ps=2 | 200 | 1142 |  |
| https://ine.gob.hn/robots.txt | 200 | 31 |  |
| https://ine.gob.hn/bases-de-datos/ | 200 | 177894 |  |
| https://www.ine.gob.ni/robots.txt | 404 | 277 |  |
| https://www.ine.gob.ni | 200 | 234303 |  |
| https://www.mofe.gov.bn/robots.txt | 301 | 0 |  |
| https://www.mof.gov.bn/robots.txt | 404 | 146 |  |
| https://www.mofe.gov.bn/ | 301 | 0 |  |
| https://www.mof.gov.bn/robots.txt | 404 | 146 |  |
| https://www.mof.gov.bn/ | 200 | 380895 |  |
| https://hacienda.pr.gov/robots.txt | 200 | 2189 |  |
| https://hacienda.pr.gov/ | 200 | 45828 |  |
| https://ine.gob.hn/robots.txt | 200 | 31 |  |
| https://ine.gob.hn/wp-json/wp/v2/categories?per_page=40&_fields=name,count | 200 | 1592 |  |
| https://www.ine.gob.ni/robots.txt | 404 | 277 |  |
| https://www.ine.gob.ni/?page_id=108819 | 200 | 243174 |  |
| https://www.ine.gob.ni/?page_id=109411 | 200 | 319622 |  |
| https://www.mof.gov.bn/robots.txt | 404 | 146 |  |
| https://www.mof.gov.bn/treasury_about-us/ | 200 | 174949 |  |
| https://www.mof.gov.bn/fiscal-and-corporate/ | 200 | 173307 |  |
| https://hacienda.pr.gov/robots.txt | 200 | 2189 |  |
| https://hacienda.pr.gov/inversionistas/estadisticas-y-recaudos-statistics-and-revenues | 200 | 38957 |  |
| https://hacienda.pr.gov/panel-de-indicadores-economicos-del-ivu | 200 | 29577 |  |
| https://hacienda.pr.gov/robots.txt | 200 | 2189 |  |
| https://hacienda.pr.gov/inversionistas/estadisticas-y-recaudos-statistics-and-revenues/ingresos-netos-al-fondo-general-general-fund-net-revenues | 200 | 55715 |  |
| https://fred.stlouisfed.org/robots.txt | 200 | 292 |  |
| https://fred.stlouisfed.org/tags/series?t=yemen | ERR | 0 | TimeoutError:[Errno 60] Operation timed out |
| https://fred.stlouisfed.org/docs/api/fred/ | ERR | 0 | TimeoutError:The read operation timed out |
| https://www.freddiemac.com/robots.txt | 200 | 2302 |  |
| https://www.freddiemac.com/pmms | 200 | 102456 |  |
| https://www.freddiemac.com/research/indices/house-price-index | 200 | 106911 |  |
| https://api.unhcr.org/robots.txt | 404 | 162 |  |
| https://api.unhcr.org/population/v1/years/?limit=1 | 200 | 80 |  |
| https://w3.unece.org/robots.txt | 404 | 1245 |  |
| https://w3.unece.org/PXWeb2015/api/v1/en/STAT/ | 200 | 474 |  |
| https://faostatservices.fao.org/robots.txt | 404 | 0 |  |
| https://faostatservices.fao.org/api/v1/en/definitions/domain/ | 401 | 28 |  |
| https://apis.cbs.gov.il/robots.txt | ERR | 0 | URLError:<urlopen error timed out> |
| https://apis.cbs.gov.il/series/catalog/level?id=1&format=json&download=false | ERR | 0 | TimeoutError:timed out |
| https://api.cbs.gov.il/robots.txt | ERR | 0 | URLError:<urlopen error timed out> |
| https://api.cbs.gov.il/index/catalog/catalog?format=json&download=false | ERR | 0 | TimeoutError:timed out |
| https://www.fao.org/robots.txt | 200 | 1056 |  |
| https://www.fao.org/worldfoodsituation/foodpricesindex/en | 200 | 64216 |  |
| http://popstats.unhcr.org/robots.txt | ERR | 0 | URLError:<urlopen error timed out> |
| http://popstats.unhcr.org | ERR | 0 | TimeoutError:timed out |
| https://www.cbs.gov.il/robots.txt | ERR | 0 | URLError:<urlopen error timed out> |
| https://www.cbs.gov.il/en/Pages/Api.aspx | ERR | 0 | TimeoutError:timed out |
| http://www.belgostat.be/robots.txt | 301 | 227 |  |
| http://www.belgostat.be/robots.txt | ERR | 0 | ConnectionResetError:[Errno 54] Connection reset by peer |
| http://www.belgostat.be | 301 | 227 |  |
| https://stat.nbb.be/robots.txt | ERR | 0 | ConnectionResetError:[Errno 54] Connection reset by peer |
| https://stat.nbb.be | ERR | 0 | ConnectionResetError:[Errno 54] Connection reset by peer |
| https://www.dallasfed.org/robots.txt | 200 | 177 |  |
| https://www.dallasfed.org/research/indicators | 200 | 34498 |  |
| https://markets.newyorkfed.org/robots.txt | 404 | 0 |  |
| https://markets.newyorkfed.org/api/rates/all/latest.json | 200 | 1427 |  |
| https://www.chicagofed.org/robots.txt | 200 | 3 |  |
| https://www.chicagofed.org/research/data/index | 200 | 87192 |  |
| https://www.philadelphiafed.org/robots.txt | 200 | 190 |  |
| https://www.philadelphiafed.org/surveys-and-data | 200 | 45324 |  |
| https://www.clevelandfed.org/robots.txt | 200 | 87 |  |
| https://www.clevelandfed.org/indicators-and-data | 200 | 143956 |  |
| https://www.atlantafed.org/robots.txt | 200 | 284 |  |
| https://www.atlantafed.org/research-and-data/data | 200 | 156651 |  |
| https://www.richmondfed.org/robots.txt | 200 | 661 |  |
| https://www.richmondfed.org/research/national_economy | 200 | 76281 |  |
| https://www.kansascityfed.org/robots.txt | 200 | 217 |  |
| https://www.kansascityfed.org/data-and-trends/ | ERR | 0 | TimeoutError:The read operation timed out |
| https://www.frbsf.org/robots.txt | 200 | 117 |  |
| https://www.frbsf.org/research-and-insights/data-and-indicators/ | 200 | 161264 |  |
| https://api.unhcr.org/robots.txt | 404 | 162 |  |
| https://api.unhcr.org/docs/refugee-statistics.html | 200 | 1224488 |  |
| https://www.unhcr.org/robots.txt | 200 | 5459 |  |
| https://www.unhcr.org/refugee-statistics/ | 403 | 182580 |  |
| https://bulks-faostat.fao.org/robots.txt | 403 | 111 |  |
| https://bulks-faostat.fao.org/production/datasets_E.json | SKIP | 0 | robots.txt refused |
| https://www.dallasfed.org/robots.txt | 200 | 177 |  |
| https://www.dallasfed.org/research/econdata | 200 | 42156 |  |
| https://www.chicagofed.org/robots.txt | 200 | 3 |  |
| https://www.chicagofed.org/research/data/nfci/current-data | 200 | 80121 |  |
| https://www.philadelphiafed.org/robots.txt | 200 | 190 |  |
| https://www.philadelphiafed.org/surveys-and-data/real-time-data-research/survey-of-professional-forecasters | 200 | 74707 |  |
| https://www.atlantafed.org/robots.txt | 200 | 284 |  |
| https://www.atlantafed.org/research-and-data/data/gdpnow | 200 | 2403747 |  |
| https://www.clevelandfed.org/robots.txt | 200 | 87 |  |
| https://www.clevelandfed.org/indicators-and-data/median-cpi | 200 | 162202 |  |
| https://www.richmondfed.org/robots.txt | 200 | 661 |  |
| https://www.richmondfed.org/research/national_economy/national_economic_indicators | 200 | 101897 |  |
| https://www.frbsf.org/robots.txt | 200 | 117 |  |
| https://www.frbsf.org/research-and-insights/data-and-indicators/daily-news-sentiment-index/ | 200 | 112715 |  |
| https://www.newyorkfed.org/robots.txt | 200 | 211 |  |
| https://www.newyorkfed.org/markets/data-hub | 200 | 106048 |  |
| https://fred.stlouisfed.org/robots.txt | 200 | 292 |  |
| https://fred.stlouisfed.org/docs/api/fred/ | ERR | 0 | TimeoutError:The read operation timed out |
| https://ine.gob.hn/robots.txt | 200 | 31 |  |
| https://ine.gob.hn/wp-json/wp/v2/categories?per_page=40&_fields=name,count | 200 | 1592 |  |
| http://181.115.7.199/robots.txt | 404 | 1282 |  |
| http://181.115.7.199/binhnd/RpWebEngine.exe/Portal?BASE=PROYPOB&lang=ESP | 200 | 2093 |  |
| https://www.ine.st/robots.txt | 200 | 842 |  |
| https://www.ine.st/index.php/publicacao/documentos/category/49-informacoes-estatisticas | 200 | 40439 |  |
| https://www.ine.st/index.php/component/phocadownload/category/78-ipc-ano | 200 | 40692 |  |
| https://deps.mofe.gov.bn/robots.txt | 301 | 0 |  |
| https://www.deps.gov.bn/robots.txt | 404 | 146 |  |
| https://deps.mofe.gov.bn/ | 301 | 0 |  |
| https://www.deps.gov.bn/robots.txt | 404 | 146 |  |
| https://www.deps.gov.bn/ | 200 | 217942 |  |
| https://www.ine.gov.ao/robots.txt | 404 | 0 |  |
| https://www.ine.gov.ao/pedido-de-dados/criar | 200 | 237069 |  |
