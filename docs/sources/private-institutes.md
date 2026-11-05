---
id: private-institutes
name: Private surveys and institutes of the catalogue
rows: [surveys, survey_spglobal, survey_ism, survey_nbs, survey_ifo, survey_zew, survey_gfk, survey_michigan, survey_conferenceboard, survey_sentix, survey_nab, survey_westpac, survey_anz, survey_nzier, survey_cbi, survey_nfib, survey_adp]
catalogue: ["ACAP", "ACEA - European Automobile Manufacturers' Association", "ADEFA - The Association of Automotive Makers, Argentina", "AKAMAI", "ANFAC - Asociacion Espanola de Fabricantes de Automoviles y Camiones", "ANFAVEA, Brazil", "ANFIA - Italian Association of the Automotive Industry", "ANZ - Indeed Australian Job Ads", "Adimark GfK", "American Petroleum Institute (API)", "Asociacion Nacional de Empresarios de Colombia (ANDI)", "Asociación de Fábricas de Automotores (ADEFA)", "Association of European Businesses", "Association of Job Information of Japan", "Australia and New Zealand Banking Group", "Australian Industry Group", "Automatic Data Processing, Inc", "BRC - British Retail Consortium", "Baker Hughes Company", "Banks Association of Turkey", "Bureau for Economic Research (BER)", "Business New Zealand", "CCFA - Comite des Constructeurs Francais d'Automobiles", "CFIB - Canadian Federation of Independent Business", "CKGSB Knowledge", "Centre for European Economic Research (ZEW)", "Ceylon Tourist Board", "Challenger, Gray and Christmas, Inc", "Chamber of Automotive Manufacturers of the Philippines (CAMPI)", "Chilean Benchmark Facility", "Chilean Chamber of Construction", "China Association of Automobile Manufacturers", "Circle K, Lithuania", "Comisión Nacional de Energía, Chile", "Confederacao Nacional da Industria (CNI)", "Confederation of British Industry", "Confederation of Finnish Industries (EK)", "Cotality, Australia", "Czech Financial Benchmark Facility", "DILF - Danish Purchasing and Logistics Forum", "Daily NK", "Danish Financial Benchmark Facility", "Departamento de Asuntos del Consumidor (DACO)", "Dun & Bradstreet", "Eiendom Norge/Real Estate Norway", "Electricite du Liban", "Enemed, Malta", "Europace AG, Germany", "Europe's Energy Portal", "European Automobile Manufacturers Association", "European Automobile Manufacturers’ Association (ACEA)", "European Banking Federation", "European Commssion", "FEBIAC", "FENALCO, Federación Nacional de Comerciantes Empresarios", "FGV - Fundacao Getulio Vargas", "FRED", "FUNCEX, Brazil", "Federal Chamber of Automotive Industries", "Federal Chamber of Automotive Industries (FCAI)", "Fedesarrollo", "Finans Norge & Verian", "FipeZAP", "Fit Small Business", "Fitch Auto ABS Indices", "Freddie Mac", "GFK", "GKI Economic Research Co", "Gallup/Capacent", "Getulio Vargas Foundation", "GfK Group", "HALPIM", "Halifax and Bank of Scotland", "Hong Kong Association of Banks", "INS, Ivory Coast", "ISSH, Albania", "Iceland Automobile Association", "Ifo Institute", "Info Sapiens", "Institute for Supply Management", "Ipsos", "Iran Vehicle Manufacturers Association", "Irish League of Credit Unions", "Japan Automobile Dealers Association", "Japan Automobile Manufacturers Association (JAMA)", "Japan Cement Association (JCA)", "Japan Machine Tool Builders' Association (JMTBA)", "Japanese Bankers Association", "Korea Automobile Manufacturers Association (KAMA)", "Korea Electric Power Corporation (KEPCO)", "Korea Productivity Center", "Logistics Managers' Index", "MNI Deutsche Börse Group", "Malaysian Automotive Association", "Manheim Consulting Inc", "Maybank Group", "Melbourne Institute", "Mortgage Bankers Association of America", "NAAMSA, South Africa", "NIMA/DNB Markets", "National Association of Realtors", "National Australia Bank", "Nationwide Building Society, UK", "Nationwide Building Society, United Kingdom", "Nielsen", "OICA - International Organization of Motor Vehicle Manufacturers", "Oficina del Comisionado de Instituciones Financieras", "Olerex AS", "Petrojam Limited", "Petrol", "Petropar", "Quotable Value", "Redbook Research Inc", "Retailers Association of India (RAI)", "Royal Institution of Chartered Surveyors (RICS)", "S&P Global", "SIAM - Society of Indian Automobile Manufacturers", "Secretaría de Energía de Panamá", "Secretaría de Energía, Honduras", "Secretaría de Energía, República Argentina", "Securities Consultants Ltd., Bangladesh", "Shell Luxembourg", "Shell Suisse", "Shell i Danmark", "Sinyi Realty Inc", "Standard & Poor's", "Swedbank", "Swedish Financial Benchmark Facility", "TOBB - Union of Chambers and Commodity Exchanges of Turkey", "Tankan Sponsored by Thomson Reuters", "Technometrica Market Intelligence/RealClearMarkets", "Terna - Rete Elettrica Nazionale, Italy", "The China Passenger Car Association (CPCA)", "The Conference Board", "The Federation of Thai Industries", "The ISET Policy Institute", "The Society of Motor Manufacturers and Traders Limited (SMMT)", "The Transparency Board", "Truckstop", "Turkish Automotive Manufacturers Association (OSD)", "Türkiye Petrolleri", "Universidad Torcuato di Tella", "University of Michigan", "VDA - German Association of the Automotive Industry", "Vietnam Automobile Manufacturers Association", "Wards Intelligence", "Westpac Banking Corporation, McDermott Miller", "Westpac Banking Corporation, Melbourne Institute", "YouGov/Citigroup", "procure.ch & Credit Suisse", "procure.ch & UBS"]
access: scraped web page
probed: 2026-11-05
confidence: called   # one data page per body: 11 refused or failed, 2 gave no content, others answered; what a body publishes is from its page's labels or, where marked, from general knowledge
---

# Private surveys and institutes

The 151 bodies of kind `private` in `resources/data_sources.json`: companies, trade associations and institutes credited by Trading Economics. Most publish a headline for free and sell the series.

## Contents
All 151 with pages, countries, access, the `api` column and the earlier probe: `catalog/private-institutes.tsv`. Total 849 pages. `access`: `headline` 142, `paid` 9. `api`: `web` 148. One section below for each body with 3 pages or more, spellings merged, plus GfK, ADP and The Conference Board, named in the request or read by the app. "Answer" is this card's one call. Nothing here gives a figure.

"General knowledge" marks what the page refused to confirm: it comes from me and is unverified on this date.

### AKAMAI (388 pages, 194 countries)
Answer 403, `www.akamai.com/internet-station`. No `url` in the catalogue. Two pages per country; the catalogue's indicator slugs include `internet-speed` and `ip-addresses`, presumably these. General knowledge: its State of the Internet reports. Not checkable here.

### S&P Global (86 pages, 47 countries)
Answer 403 CloudFront, `pmi.spglobal.com`, as the earlier probe. General knowledge: PMIs (manufacturing, services, composite, with sponsors HCOB, au Jibun Bank and others), press releases free, series paid. Its index arm `spglobal.com/spdji` (`Standard & Poor's`, 4 pages; catalogue link `us.spindices.com` doesn't resolve) answered 403 too: General knowledge, Case-Shiller home prices.

### ACEA (53 pages under 3 spellings, up to 31 countries)
Answer 200, `acea.auto/reliable-data-statistics/`. Free: press releases of new passenger car and commercial vehicle registrations, by a yearly press release calendar. Figures by tag: `production-of-vehicles`, `registrations-of-vehicles`. A paid offer, "ACEA Intelligence". HTML and PDF.

### Baker Hughes Company (16 pages, 15 countries)
Answer 403, `rigcount.bakerhughes.com`. General knowledge: rig count, weekly for North America, monthly international; free spreadsheet downloads. If so, the catalogue's `web` understates it.

### Institute for Supply Management (15 pages)
Answer: the report page redirects to an SSO login, `ecommerce.ismworld.org`, with no content for a program. General knowledge: manufacturing and services PMI, monthly; headline press release free, subscription for detail. The app reads it through TradingView's calendar.

### Europe's Energy Portal (13 pages, 13 countries)
Answer 200, `energy.eu`. A paid data set builder: "Select commodities", "Choose end-user profiles", "Choose markets or EU Average", "Choose date range", "Complete your purchase". End-user energy prices from a network of respondents. Paid; the catalogue says `headline`.

### American Petroleum Institute (8 pages)
Answer 200, `api.org/energy-insights/statistics`. Sections: "Statistics Overview", "Weekly Statistical Bulletin" (`/statistics/wsb`), "Data and Reports" (`/statistics/survey`). HTML, no file links on this page. General knowledge: weekly crude stock figures are sold to subscribers.

### MNI Deutsche Börse Group (8 pages, 5 countries)
No `url` in the catalogue. `www.mni-indicators.com` redirects to `mninews.marketnews.com/product-info/mni-indicators`, 502. General knowledge: Chicago PMI and business indicators.

### Automatic Data Processing (6 pages)
Answer 200, `adpemploymentreport.com`, 1.3 KB of JavaScript shell, nothing to read without a browser. National Employment Report. The app reads it through TradingView's calendar.

### ZEW (6 pages, 5 countries)
Answer 404 at my first guess, then 200 at `zew.de/en/publications/zew-expertises-research-reports/research-reports/business-cycle/zew-financial-market-survey`. ZEW Indicator of Economic Sentiment, monthly: "Detailed results of the latest survey", "Historical time series", publication dates, issues by month. **Free files**: `ftp.zew.de/pub/zew-docs/div/konjunktur.xls` (time series), `download.zew.de/e_current_table.pdf`, `ftp.zew.de/pub/zew-docs/frep/<MMYYYY>.pdf`, dates PDF. The catalogue's `web` understates it.

### Mortgage Bankers Association (6 pages)
Answer 403, Cloudflare block, `mba.org` ("Sorry, you have been blocked"). Catalogue link `mortgagebankers.org` is out of date, per the earlier probe's 403. General knowledge: weekly mortgage applications.

### National Association of Realtors (6 pages)
Answer 200, `nar.realtor/research-and-statistics/housing-statistics`. Labels: "Housing Statistics", "State & Metro Area Data", "Commercial Research", "Research Reports", "Statistical News Release Schedule". HTML. Free or paid per item wasn't established.

### Nielsen (6 pages, 6 countries)
Answer 200, `nielsen.com/data-center/`. **Now TV audiences**: "The Gauge", "Top 10", streaming and broadcast rankings, sports. Whatever consumer indicator the catalogue's pages meant isn't on this page. The catalogue's body and today's site may not match.

### Australian Industry Group (5 pages)
Answer 200, `australianindustrygroup.com.au/resourcecentre/research-economics/`. "Australian Industry Index" monthly, a 2026 release schedule PDF, "Detailed Index Data"; a link to email for "purchasing data". Headline free, data paid.

### Logistics Managers' Index (5 pages)
Answer 200, `the-lmi.com`. Monthly HTML reports, one page per month (`/<month>-<year>-logistics-managers-index.html`), archived from 2016, 16 tables on the page. Free in HTML.

### SMMT (5 pages)
Answer 200, `smmt.co.uk/vehicle-data/smmt-vehicle-data/`. Car, LCV, EV, bus and coach, HGV registrations, car and commercial vehicle manufacturing, engines, used cars, car colours; "MVRIS reports", from its database of new registrations; "Vehicle Parc", outlooks. HTML. Free or paid per item wasn't established.

### University of Michigan (5 pages)
Answer 200, `data.sca.isr.umich.edu`. "Surveys of Consumers": `charts.php` and `demographic-tables.php` by age, income, education, region, gender; `tables.php`; `reports.php`; a time series miner `data-archive/mine.php`; `sda.php`; `subset/`; `fetchdoc.php?docid=` for PDFs; an `/agreement.php`, not read; `sponsor-login.php`. **Far more than the catalogue's `web`**: a free structured archive, under terms to read first.

### Bureau for Economic Research (4 pages)
Answer 200, `dataportal.ber.ac.za`: "BER Macro Dashboard", 956 bytes of JavaScript shell. The earlier probe saw `numBERs`, `BER FORECAST DATA`, a data playground. South African confidence surveys, general knowledge.

### Confederation of British Industry (4 pages)
Answer 200, redirected to `cbi.org.uk/economics/surveys/`. Sector surveys, upcoming releases, "License our survey data". Paid data. The page is aimed at respondents.

### Eiendom Norge (4 pages)
Answer 200, `eiendomnorge.no/boligprisstatistikk/`. Norwegian house price statistics, monthly, with "Nøkkeltall" and an English page. **Terms** ("Bruksregler"): all rights belong to Eiendom Norge and Eiendomsverdi; commercial use or any other reuse, republishing or compiling requires written consent.

### Halifax and Bank of Scotland (4 pages)
Answer 200, `halifax.co.uk/media-centre/house-price-index.html`, but the page says "Internet Banking - Error": no content, also in the earlier probe. General knowledge: monthly UK House Price Index, with a free historical spreadsheet.

### Petrol (4 pages, 4 countries)
Answer 404 at my guess `petrol.si/za-voznike/cene-goriv`. A Slovenian fuel retailer. The earlier probe saw only investor publications. Presumably pump prices. Unconfirmed.

### Quotable Value (4 pages)
Answer 200, `qv.co.nz/price-index/`. QV House Price Index, monthly; average price, 3-month and 12-month change; regional changes. HTML.

### The Federation of Thai Industries (4 pages)
Answer 200, `fti.or.th/ids`, "Industry Data Space". Economic overview, a monthly pulse of industry, automotive statistics at `/ids/automotive-stats`, CEO surveys, `/ids/data-update`, in Thai and English. HTML.

### Business New Zealand (3 pages)
Answer 403, "Just a moment…" challenge, `businessnz.org.nz`. General knowledge: Performance of Manufacturing and of Services indexes.

### Chilean Chamber of Construction (3 pages)
Answer 200, `cchc.cl/indicadores`: a selector of indicators with "Descargar todos", "Descargar selección"; units of quantity, m², m³, tonnes, so construction costs and materials, it seems. The indicator list isn't in the HTML. Terms PDF linked (`terminos-y-condiciones-cchc.pdf`), not read.

### China Association of Automobile Manufacturers (3 pages)
Answer 200, `caam.org.cn/tjsj`, in Chinese. Monthly press conferences, "产销" (production and sales), "进出口" (imports and exports), by world region.

### CNI, Brazil (3 pages)
Answer 200, redirected to `portaldaindustria.com.br/cni/estatisticas/`. Sondagem Industrial, Sondagem Indústria da Construção, ICEI (business confidence, with sector results), Panorama da Pequena Indústria, Sondagem Especial, Retratos da Sociedade Brasileira; "Indicadores Econômicos CNI: Séries históricas" at `indicadores.sistemaindustria.com.br`. Free; not checked for file formats.

### FGV, Brazil (3 pages, and `Getulio Vargas Foundation`, 1)
Answer: TLS handshake failure, `portalibre.fgv.br`, and earlier on `portal.fgv.br`. General knowledge: IGP price indexes and confidence surveys, mostly paid through FGV IBRE.

### FUNCEX, Brazil (3 pages)
Answer 200, `funcex.org.br`, the home page: consulting and education; a foundation for trade. No data page found. General knowledge: Brazilian trade price and volume indexes.

### FCAI, Australia (3 pages under one spelling, 1 under another)
Answer 200, `fcai.com.au/news-and-media/publications/`. Reports and policy responses, the newest listed from 2022. Vehicle sales ("VFACTS") aren't on this page.

### Ifo Institute (3 pages)
Answer 404 at my first guess, then 200 at `ifo.de/en/survey/ifo-business-climate-index-germany`, a link on the 404 page. "Current Results", "Upcoming Release Dates", method and seasonal adjustment. No file on this page; `/en/survey-results` and `/en/data-and-news` go further, not called. Read by the app through TradingView's calendar.

### Ipsos (3 pages, 3 countries)
Answer 404 at my guess `ipsos.com/en/global-consumer-confidence`. The earlier probe saw an insights hub. General knowledge: Consumer Confidence Index, monthly, in PDF.

### Melbourne Institute (3 pages)
Answer 403, redirected to `/research/macroeconomics/macro-archive/`; the earlier probe also 403. General knowledge: Westpac consumer sentiment, inflation gauge.

### National Australia Bank (3 pages)
Answer 200, `business.nab.com.au/tag/economic-commentary`. Monthly and quarterly business surveys, SME survey, consumer sentiment, wellbeing survey, Monthly Data Insights, Online Retail Sales Index, housing update, The Forward View; articles, some PDFs. Free. Read by the app through TradingView's calendar.

### Nationwide (3 + 1 pages)
Answer 200; `nationwidehousepriceindex.co.uk` redirects to `nationwide.co.uk/media/hpi`. National and regional House Price Index, monthly, `/media/hpi/reports`, `/media/hpi/charts`, "Historic Data" at `/media/hpi/resources`. Free.

### SIAM, India (3 pages)
Answer 200, `siam.in/knowledge/industry-data/industry-trends`, 3 tables. **Undertaking** on the page: the visitor agrees not to "share, publish, upload, transmit, reproduce, redistribute" its statistics without prior written permission. "Subscription Based Report" for the detail.

### GfK (2 + 1 + 1 pages)
`gfk.com` now redirects to `nielseniq.com`: GfK is part of NIQ. Answer 200, marketing pages, no consumer climate data. `nim.org` (the Nuremberg Institute for Market Decisions, "Founder of GfK") has a "History of the GfK Consumer Climate" page and reports. Read by the app through TradingView's calendar.

### The Conference Board (2 pages)
Answer 200, `conference-board.org/topics/consumer-confidence/`. Consumer Confidence Index, Leading Economic Indicators (`/topics/business-cycle-indicators`), CEO Confidence, "Data Central" `/data`, member login. Headline free, data for members.

## How to call it
Web pages only; no body here offers an API on the page called, unless noted. Most answers need a browser or refuse programs. The earlier `catalog/bodies_probe.jsonl.gz` holds each body's home page.

Wrong or weak in the catalogue, not edited:
- `api` `web` understates: University of Michigan (a time series miner), ZEW (`konjunktur.xls`), CChC (downloads), Baker Hughes (general knowledge), BER (portal), CNI (a historical series site).
- `access` `headline` but wholly paid: Europe's Energy Portal.
- Link out of date: Mortgage Bankers (`mortgagebankers.org`), `Standard & Poor's` (`us.spindices.com` doesn't resolve), GfK (`gfk.com/de/`, now NIQ), Melbourne Institute.
- Likely mismatched: `Nielsen` (TV ratings today), `European Banking Federation` (url `suomenpankki.fi`, the Bank of Finland), `Electricite du Liban` (url `bdl.gov.lb`, the central bank of Lebanon).
- `Freddie Mac`'s and `FRED`'s `FRED API, free key`: not called here, the FRED card can confirm it.

## What the app reads
Rows `surveys` and `survey_<id>`, `src/SourcesPage.cpp:767-775`. The app reads none of these sites: headlines come from TradingView's economic calendar, sorted to publishers by event name (`src/SurveyHeadlines.cpp:22-48`, publishers `:84-99`): S&P Global, ISM, NBS, Ifo, ZEW, GfK, Michigan, Conference Board, Sentix, NAB, Westpac, ANZ, NZIER, CBI, NFIB, ADP. Catalogue: `src/OfficialSources.cpp:20`.

## Not read yet
Free and structured, for Economy › Countries and the Map:
- ZEW's `konjunktur.xls`.
- University of Michigan's time series, after reading `agreement.php`.
- Logistics Managers' Index, monthly HTML.
- NAB's surveys and Nationwide's historic data.
- CNI's historical series, for Brazil.
- FTI's automotive statistics, for Thailand.

## Calls made
- `GET https://www.akamai.com/robots.txt` → 403, 380 bytes.
- `GET https://www.akamai.com/internet-station` → 403, 386 bytes.
- `GET https://www.pmi.spglobal.com/robots.txt` → 403, 919 bytes.
- `GET https://www.pmi.spglobal.com/Public/Release/PressReleases` → 403, 919 bytes.
- `GET https://www.acea.auto/robots.txt` → 200, 2846 bytes.
- `GET https://www.acea.auto/reliable-data-statistics/` → 200, 118482 bytes.
- `GET https://rigcount.bakerhughes.com/robots.txt` → 403, 392 bytes.
- `GET https://rigcount.bakerhughes.com/` → 403, 378 bytes.
- `GET https://www.ismworld.org/robots.txt` → 200, 3996 bytes.
- `GET https://www.ismworld.org/supply-management-news-and-reports/reports/ism-report-on-business/` → 200, 922 bytes.
- `GET https://energy.eu/robots.txt` → 200, 50 bytes.
- `GET https://energy.eu/` → 200, 44069 bytes.
- `GET https://www.api.org/robots.txt` → 200, 1025 bytes.
- `GET https://www.api.org/energy-insights/statistics` → 200, 34971 bytes.
- `GET https://www.mni-indicators.com/robots.txt` → 502, 150 bytes.
- `GET https://www.mni-indicators.com/` → 502, 150 bytes.
- `GET https://adpemploymentreport.com/robots.txt` → 200, 1341 bytes.
- `GET https://adpemploymentreport.com/` → 200, 1341 bytes.
- `GET https://www.zew.de/robots.txt` → 200, 148 bytes.
- `GET https://www.zew.de/en/publications/zew-indicator-of-economic-sentiment` → 404, 230587 bytes.
- `GET https://www.mba.org/robots.txt` → 403, 4926 bytes.
- `GET https://www.mba.org/news-and-research/research-and-economics` → 403, 6040 bytes.
- `GET https://www.nar.realtor/robots.txt` → 200, 216 bytes.
- `GET https://www.nar.realtor/research-and-statistics/housing-statistics` → 200, 505136 bytes.
- `GET https://www.nielsen.com/robots.txt` → 200, 205 bytes.
- `GET https://www.nielsen.com/data-center/` → 200, 1159291 bytes.
- `GET https://www.australianindustrygroup.com.au/robots.txt` → 200, 600 bytes.
- `GET https://www.australianindustrygroup.com.au/resourcecentre/research-economics/` → 200, 142665 bytes.
- `GET https://www.the-lmi.com/robots.txt` → 200, 5199 bytes.
- `GET https://www.the-lmi.com/` → 200, 113859 bytes.
- `GET https://www.smmt.co.uk/robots.txt` → 200, 172 bytes.
- `GET https://www.smmt.co.uk/vehicle-data/smmt-vehicle-data/` → 200, 193896 bytes.
- `GET https://data.sca.isr.umich.edu/robots.txt` → 200, 13880 bytes.
- `GET https://data.sca.isr.umich.edu/` → 200, 13880 bytes.
- `GET https://dataportal.ber.ac.za/robots.txt` → 200, 73 bytes.
- `GET https://dataportal.ber.ac.za` → 200, 956 bytes.
- `GET https://www.cbi.org.uk/robots.txt` → 200, 120 bytes.
- `GET https://www.cbi.org.uk/what-we-do/economic-surveys/` → 200, 38617 bytes.
- `GET https://eiendomnorge.no/robots.txt` → 200, 212 bytes.
- `GET https://eiendomnorge.no/boligprisstatistikk/` → 200, 89443 bytes.
- `GET https://www.halifax.co.uk/robots.txt` → 200, 18281 bytes.
- `GET https://www.halifax.co.uk/media-centre/house-price-index.html` → 200, 18281 bytes.
- `GET https://www.petrol.si/robots.txt` → 200, 360 bytes.
- `GET https://www.petrol.si/za-voznike/cene-goriv` → 404, 1586695 bytes.
- `GET https://www.qv.co.nz/robots.txt` → 200, 782 bytes.
- `GET https://www.qv.co.nz/price-index/` → 200, 125710 bytes.
- `GET https://www.spglobal.com/robots.txt` → 403, 384 bytes.
- `GET https://www.spglobal.com/spdji/en/index-family/indicators/sp-corelogic-case-shiller/` → 403, 2011 bytes.
- `GET https://fti.or.th/robots.txt` → 200, 362 bytes.
- `GET https://fti.or.th/ids` → 200, 250934 bytes.
- `GET https://businessnz.org.nz/robots.txt` → 403, 5448 bytes.
- `GET https://businessnz.org.nz/` → 403, 5397 bytes.
- `GET https://cchc.cl/robots.txt` → 200, 61 bytes.
- `GET https://cchc.cl/indicadores` → 200, 80950 bytes.
- `GET http://www.caam.org.cn/robots.txt` → 404, 170 bytes.
- `GET http://www.caam.org.cn/tjsj` → 200, 96497 bytes.
- `GET https://www.portaldaindustria.com.br/robots.txt` → 200, 400 bytes.
- `GET https://www.portaldaindustria.com.br/estatisticas/` → 200, 117484 bytes.
- `GET https://portalibre.fgv.br/robots.txt` → failed: URLError(SSLError(1, '[SSL: SSLV3_ALERT_HANDSHAKE_FAILURE] s.
- `GET https://portalibre.fgv.br/estudos-e-pesquisas/indices-de-precos` → failed: URLError(SSLError(1, '[SSL: SSLV3_ALERT_HANDSHAKE_FAILURE] s.
- `GET https://www.funcex.org.br/robots.txt` → 200, 491 bytes.
- `GET https://www.funcex.org.br/` → 200, 1304617 bytes.
- `GET https://www.fcai.com.au/robots.txt` → 200, 692 bytes.
- `GET https://www.fcai.com.au/news-and-media/publications/` → 200, 82984 bytes.
- `GET https://www.ifo.de/robots.txt` → 404, 186 bytes.
- `GET https://www.ifo.de/en/survey/ifo-business-climate-germany` → 404, 63960 bytes.
- `GET https://www.ipsos.com/robots.txt` → 200, 2800 bytes.
- `GET https://www.ipsos.com/en/global-consumer-confidence` → 404, 38389 bytes.
- `GET https://melbourneinstitute.unimelb.edu.au/robots.txt` → 200, 152 bytes.
- `GET https://melbourneinstitute.unimelb.edu.au/publications/macroeconomic-reports` → 403, 1194 bytes.
- `GET https://business.nab.com.au/robots.txt` → 404, 543 bytes.
- `GET https://business.nab.com.au/tag/economic-commentary` → 200, 104658 bytes.
- `GET https://www.nationwidehousepriceindex.co.uk/robots.txt` → 200, 27 bytes.
- `GET https://www.nationwidehousepriceindex.co.uk/` → 200, 40110 bytes.
- `GET https://www.siam.in/robots.txt` → 404, 1245 bytes.
- `GET https://www.siam.in/knowledge/industry-data/industry-trends` → 200, 100231 bytes.
- `GET https://www.gfk.com/robots.txt` → 200, 1113 bytes.
- `GET https://www.gfk.com/` → 200, 394896 bytes.
- `GET https://www.nim.org/robots.txt` → 200, 231 bytes.
- `GET https://www.nim.org/en/` → 200, 330874 bytes.
- `GET https://www.conference-board.org/robots.txt` → 200, 2224 bytes.
- `GET https://www.conference-board.org/topics/consumer-confidence` → 200, 332268 bytes.
- `GET https://www.ifo.de/en/survey/ifo-business-climate-index-germany` → 200, 102065 bytes.
- `GET https://www.zew.de/en/publications/zew-expertises-research-reports/research-reports/business-cycle/zew-financial-market-survey` → 200, 261435 bytes.

Sections marked "General knowledge" rest on no call.
