# What the sources hold: index

See [README.md](README.md) for what this folder is and how far to trust it. Before deciding that a piece of data
doesn't exist, or adding a source: check the topic below, then grep `catalog/` (`zgrep` for the `.gz` lists).

```sh
zgrep -il "bank lending" docs/sources/catalog/*       # which source has a data set by that name
grep -i "port calls" docs/sources/*.md                # the cards
```

## Who has what

Start from the topic, then open the card. `catalogue-bodies.md` covers the bodies of the catalogue by country, for what no card
holds (a country's tax office, a national labour ministry, a small central bank).

| Looking for | Cards |
|---|---|
| Inflation, CPI, PPI | [oecd](oecd.md) (54 areas, `CP`, `PP`), [imf](imf.md), [eurostat](eurostat.md), [ilo](ilo.md), [bls](bls.md), national offices |
| Unemployment, jobs, wages | [oecd](oecd.md), [ilo](ilo.md), [eurostat](eurostat.md), [bls](bls.md), [fred](fred.md), national offices |
| GDP, national accounts | [oecd](oecd.md), [imf](imf.md), [worldbank](worldbank.md), [eurostat](eurostat.md), [bea](bea.md), [european_commission](european_commission.md) (AMECO), national offices |
| Forecasts, projections | [imf](imf.md) (WEO), [oecd](oecd.md) (Economic Outlook), [european_commission](european_commission.md), [phillyfed](phillyfed.md) (SPF, Livingston), [boc](boc.md), [ing](ing.md), [bcb](bcb.md) (Focus) |
| Policy rates, money markets | [bis](bis.md), [nyfed](nyfed.md), [fedboard](fedboard.md), [ecb](ecb.md), [boe](boe.md), [boj](boj.md), [boc](boc.md), [snb](snb.md), [riksbank](riksbank.md), [norges](norges.md), [cbrt](cbrt.md) |
| Government yields, debt, auctions | [treasury](treasury.md), [boc](boc.md), [boe](boe.md), [mofjapan](mofjapan.md), [boi](boi.md), [oecd](oecd.md), [fred](fred.md) |
| Exchange rates | [frankfurter](frankfurter.md), [ecb](ecb.md), [bis](bis.md), [boc](boc.md), [riksbank](riksbank.md), [norges](norges.md), [bcb](bcb.md) |
| Credit, banks, balance sheets | [bis](bis.md), [fedboard](fedboard.md), [ecb](ecb.md), [boe](boe.md), [bbk](bbk.md), [bdf](bdf.md), [boc](boc.md), [boj](boj.md), [snb](snb.md) |
| House prices, housing | [oecd](oecd.md), [bis](bis.md), [freddiemac](freddiemac.md), [census](census.md), [fred](fred.md) |
| Surveys, confidence, PMI | [oecd](oecd.md) (`BCICP`, `CCICP`, BTS), [european_commission](european_commission.md), [boj](boj.md) (Tankan), [phillyfed](phillyfed.md), [fed-regional](fed-regional.md), [private-institutes](private-institutes.md) (S&P Global, ISM, ZEW, Ifo, Michigan: mostly refused or headline only) |
| Trade flows, balance of payments | [un](un.md) (Comtrade), [imf](imf.md), [oecd](oecd.md), [eurostat](eurostat.md), [census](census.md), [treasury](treasury.md) (TIC) |
| Positioning, sentiment | [cftc](cftc.md) (COT), [myfxbook](myfxbook.md), [alternative](alternative.md), [coinmarketcap](coinmarketcap.md), [binance](binance.md) (long-short, open interest) |
| Energy, oil stocks | [eia](eia.md), [jodi](jodi.md), [opec](opec.md) (refused), [eurostat](eurostat.md), [private-institutes](private-institutes.md) (Baker Hughes) |
| Commodities, agriculture | [imf](imf.md) (PCPS, WEO projections), [yahoo](yahoo.md), [usda](usda.md) (PSD), [fao](fao.md), [wgc](wgc.md), [worldsteel](worldsteel.md), [boc](boc.md) (BCPI) |
| Shipping, ports, freight | [aisstream](aisstream.md), [imf](imf.md) (PortWatch), [oecd](oecd.md) (freight by mode), [nyfed](nyfed.md) (GSCPI), [fred](fred.md), [bundled](bundled.md) |
| Stocks, company data, filings | [yahoo](yahoo.md), [nasdaq](nasdaq.md), [sec](sec.md), [polygon](polygon.md), [alpaca](alpaca.md), [fmp](fmp.md), [wikidata](wikidata.md), [wikipedia](wikipedia.md) (index members) |
| Crypto markets | [binance](binance.md), [coinbase](coinbase.md), [kraken](kraken.md), [coinmarketcap](coinmarketcap.md), [alternative](alternative.md), [cryptonews](cryptonews.md) |
| Order book, trades, futures | [binance](binance.md), [coinbase](coinbase.md), [kraken](kraken.md), [polygon](polygon.md) |
| Economic calendar | [tradingview](tradingview.md), [forexfactory](forexfactory.md), [nasdaq](nasdaq.md) (corporate) |
| News, research notes | [newsfeeds](newsfeeds.md), [investinglive](investinglive.md), [ing](ing.md), [cryptonews](cryptonews.md), [fmp](fmp.md), [nasdaq](nasdaq.md), [alpaca](alpaca.md) |
| Development, health, society | [worldbank](worldbank.md), [who](who.md), [unhcr](unhcr.md), [sesric](sesric.md), [unece](unece.md), [transparency](transparency.md), [iep](iep.md), [wef](wef.md), [sipri](sipri.md) |
| Regions: Africa | [afdb](afdb.md) (refused), [bceao](bceao.md), [beac](beac.md), [sarb](sarb.md), [statssa](statssa.md) (refused) |
| Regions: Latin America | [secmca](secmca.md), [iadb](iadb.md), [bcb](bcb.md), [ibge](ibge.md), [banxico](banxico.md), [inegi](inegi.md), [indec](indec.md) |
| Regions: Asia-Pacific | [adb](adb.md), [boj](boj.md), [bok](bok.md), [abs](abs.md), [singstat](singstat.md), [hkcsd](hkcsd.md), [psa](psa.md), [mospi](mospi.md), [statsnz](statsnz.md), [bps](bps.md), [bot](bot.md) |
| Local: bundled, computed, imported | [bundled](bundled.md), [deepwater](deepwater.md), [imports](imports.md) |
| Not data: services | [anthropic](anthropic.md), [huggingface](huggingface.md), [adblock](adblock.md) |

## The cards

134 cards. **called**: its catalogue or a sample answered (100). **docs**: public documentation and code only, usually for lack of a key (17). **refused**: it refused or failed, so the card says what could not be seen (16).

| Card | Holds | Access | Confidence |
|---|---|---|---|
| [abs](abs.md) | Australia's national statistics through an SDMX 2.1 API: | free, no key | called |
| [adb](adb.md) | KIDB: | free, no key | called |
| [adblock](adblock.md) | The 28 text lists the news reader's ad blocker downloads: | free, no key | called |
| [afdb](afdb.md) | Statistics of African countries through its Africa Information Highway portal. | scraped web page | refused |
| [aisstream](aisstream.md) | A live WebSocket feed of AIS messages: | free key | docs |
| [alpaca](alpaca.md) | US stocks, options, crypto, fixed income, news, corporate actions and logos. | free account | docs |
| [alternative](alternative.md) | The crypto Fear & Greed index, daily since February 2018, plus a small free crypto ticker API. | free, no key | called |
| [anthropic](anthropic.md) | The Claude models' API. | paid | docs |
| [banxico](banxico.md) | The SIE (Sistema de Información Económica): | free key | called |
| [bbk](bbk.md) | The Bundesbank's time series database over SDMX 2.1: | free, no key | called |
| [bcb](bcb.md) | Brazil's central bank has three public doors: | free, no key | called |
| [bceao](bceao.md) | The central bank of the eight West African Monetary Union countries. | scraped web page | called |
| [bdf](bdf.md) | Webstat: | free key | called |
| [bea](bea.md) | US national accounts, GDP by industry and region, trade in services, international position. | free key | docs |
| [beac](beac.md) | The central bank of CEMAC: | scraped web page | called |
| [bfs](bfs.md) | Swiss official statistics: | free, no key | called |
| [binance](binance.md) | Crypto spot market (3742 symbols) and USD-margined futures (942 contracts, a quarter of them on stocks, commodities, indices and forex): | free, no key | called |
| [bis](bis.md) | Central banks' statistics gathered by the BIS: | free, no key | called |
| [bls](bls.md) | US prices, jobs, wages and productivity. | free, no key | refused |
| [boc](boc.md) | Valet, the Bank's own API: | free, no key | called |
| [boe](boe.md) | The Statistical Interactive Database (IADB): | free, no key | called |
| [boi](boi.md) | An SDMX 2.1 and 3.0 service (Fusion Edge Server) with the Bank's statistics and many of the national ones: | free, no key | called |
| [boj](boj.md) | The Time-Series Data Search with its JSON API: | free, no key | called |
| [bok](bok.md) | The Economic Statistics System: | free key | called |
| [bot](bot.md) | Thailand's central bank: | free account | docs |
| [bps](bps.md) | Indonesia's national statistics through a Web API: | free key | docs |
| [bundled](bundled.md) | Ten JSON files in `resources/`, compiled into the app by `resources/resources.qrc` and read as `:/<name>.json`. | local | called |
| [catalogue-api-audit](catalogue-api-audit.md) | `resources/data_sources.json` was not edited: | mixed | called, except where a section says otherwise |
| [cbrt](cbrt.md) | EVDS, the bank's data system: | free key | called |
| [cbs](cbs.md) | CBS StatLine over OData: | free, no key | called |
| [cbsisrael](cbsisrael.md) | Israel's national statistics. | free, no key | refused |
| [census](census.md) | People, firms, housing starts, retail sales, trade. | free key | called |
| [cftc](cftc.md) | Weekly Commitments of Traders: | free, no key | called |
| [coinbase](coinbase.md) | Crypto spot market: | free, no key | called |
| [coinmarketcap](coinmarketcap.md) | Crypto market aggregates: | scraped web page | called |
| [cryptonews](cryptonews.md) | Three RSS feeds of crypto news. | free, no key | called |
| [cso](cso.md) | PxStat: | free, no key | called |
| [deepwater](deepwater.md) | Data no outside source holds: | local | called |
| [destatis](destatis.md) | GENESIS-Online, Germany's official statistics database: | free account | docs |
| [dnb](dnb.md) | The Netherlands' central bank. | free key | refused |
| [dst](dst.md) | StatBank Denmark: | free, no key | called |
| [ecb](ecb.md) | The ECB Data Portal: | free, no key | called |
| [eccb](eccb.md) | The central bank of the Eastern Caribbean Currency Union. | scraped web page | refused |
| [eia](eia.md) | US and world energy: | free key | docs |
| [estat](estat.md) | Japan's portal of government statistics, from every ministry. | free key | docs |
| [european_commission](european_commission.md) | DG ECFIN's own statistics: | free, no key | called |
| [eurostat](eurostat.md) | The statistics of the EU, its members, EFTA and the candidate countries: | free, no key | called |
| [fao](fao.md) | FAOSTAT: | free, no key | called |
| [fed-regional](fed-regional.md) | Seven Banks, each publishing a small shelf of indicators as files on web pages, with no API. | free, no key | called |
| [fedboard](fedboard.md) | Rates, money, bank balance sheets, the Fed's own balance sheet, industrial production, consumer credit, financial accounts. | free, no key | called |
| [fmp](fmp.md) | A commercial API of market and company data. | free key | docs |
| [forexfactory](forexfactory.md) | The current week's economic calendar of Forex Factory as a static file: | free, no key | called |
| [frankfurter](frankfurter.md) | An open source API over official exchange rates. | free, no key | called |
| [fred](fred.md) | An aggregator: | free key | docs |
| [freddiemac](freddiemac.md) | US mortgage rates (the weekly PMMS), a house price index, and the loan-level data of its book. | free, no key | called |
| [github](github.md) | The app's own updates: | free key | called |
| [gus](gus.md) | Three APIs: | free, no key | called |
| [hkcsd](hkcsd.md) | Hong Kong's official statistics: | free, no key | called |
| [huggingface](huggingface.md) | A catalogue of machine-learning models, data sets and demo apps, with their files. | free, no key | called |
| [iadb](iadb.md) | A research data portal: | free, no key | called |
| [ibge](ibge.md) | Brazil's national statistics: | free, no key | called |
| [ieom](ieom.md) | The central bank of the French Pacific territories, and the site its statistics share with the French overseas departments. | scraped web page | called |
| [iep](iep.md) | Publisher of the Global Peace Index and related indices, as reports and a map site. | free account | called |
| [ilo](ilo.md) | ILOSTAT: | free, no key | called |
| [imf](imf.md) | Two separate services. | free, no key | called |
| [imports](imports.md) | Files the user brings: | local | called |
| [indec](indec.md) | The national open data API `apis.datos.gob.ar/series` serves INDEC's series and those of the central bank, the Treasury, tourism, crime and more. | free, no key | called |
| [ine](ine.md) | INEbase over the Tempus 3 JSON API: | free, no key | called |
| [ine_pt](ine_pt.md) | INE Portugal's indicator database: | free, no key | called |
| [inegi](inegi.md) | Mexico's statistics office: | free key | docs |
| [ing](ing.md) | The research site of ING: | scraped web page | called |
| [insee](insee.md) | France's statistics office, two services: | free, no key | called |
| [investinglive](investinglive.md) | A market news site for forex and macro traders, with short headlines through the day. | scraped web page | called |
| [istat](istat.md) | IstatData over SDMX: | free, no key | called |
| [itc](itc.md) | A joint UN and WTO agency, by general knowledge rather than a call, behind the trade tools Trade Map and Market Access Map. | scraped web page | refused |
| [jodi](jodi.md) | Monthly oil and natural gas statistics reported by countries, gathered by the International Energy Forum: | free, no key | called |
| [kosis](kosis.md) | Korea's national statistics portal, for every statistics-producing agency, run by the renamed Ministry of Data and Statistics (국가데이터처, `mods.go.kr`). | free key | docs |
| [kraken](kraken.md) | Crypto spot market with many EUR and USD pairs: | free, no key | called |
| [mas](mas.md) | Singapore's central bank. | free account | refused |
| [metacopier](metacopier.md) | A paid cloud service that holds connections to MetaTrader and other trading accounts and exposes them over a REST API. | paid | docs |
| [mofjapan](mofjapan.md) | No API: | free, no key | called |
| [mospi](mospi.md) | India's official statistics on eSankhyiki: | free, no key | called |
| [myfxbook](myfxbook.md) | A site where retail traders link their forex accounts. | free account | docs |
| [nasdaq](nasdaq.md) | US listings: | scraped web page | refused |
| [nbb](nbb.md) | A .Stat Suite service: | free, no key | called |
| [nbschina](nbschina.md) | China's national statistics. | scraped web page | refused |
| [newsfeeds](newsfeeds.md) | Every news or press feed in the code: | free, no key | called |
| [norges](norges.md) | Norway's central bank over SDMX: | free, no key | called |
| [nyfed](nyfed.md) | Money market reference rates, the Fed's open market operations and holdings, primary dealers (a real JSON API), plus research series published as files: | free, no key | called |
| [oecd](oecd.md) | One SDMX service holding **1,552 dataflows** from every directorate of the OECD, from monthly macro indicators to municipal finance, tax wedges and health. | free, no key | called |
| [ons](ons.md) | UK official statistics: | free, no key | called |
| [opec](opec.md) | Oil statistics: | scraped web page | refused |
| [other-hosts](other-hosts.md) | The hosts in `src/` that no other card owns: | scraped web page | called |
| [phillyfed](phillyfed.md) | The home of forecasts data: | free, no key | called |
| [polygon](polygon.md) | Polygon, now Massive: | paid | docs |
| [private-institutes](private-institutes.md) | The 151 bodies of kind `private` in `resources/data_sources.json`: | scraped web page | called |
| [psa](psa.md) | The Philippines' national statistics in a PxWeb database: | free, no key | called |
| [rba](rba.md) | Australia's central bank. | scraped web page | refused |
| [rbi](rbi.md) | India's central bank. | scraped web page | refused |
| [rbnz](rbnz.md) | New Zealand's central bank. | scraped web page | refused |
| [riksbank](riksbank.md) | Sweden's central bank: | free, no key | called |
| [sarb](sarb.md) | The JSON web service behind resbank.co.za's statistics pages: | free, no key | called |
| [scb](scb.md) | The Statistical Database over PxWeb API 2: | free, no key | called |
| [sec](sec.md) | Every filing of US-listed companies, and the figures tagged inside them. | free, no key | called |
| [secmca](secmca.md) | The secretariat of the Central American Monetary Council: | free account | called |
| [sesric](sesric.md) | The statistics body of the Organisation of Islamic Cooperation. | free, no key | called |
| [singstat](singstat.md) | The Table Builder API: | free, no key | called |
| [sipri](sipri.md) | Military spending, arms transfers, the arms industry and peace operations. | free, no key | called |
| [snb](snb.md) | The data portal `data.snb.ch`: | free, no key | called |
| [ssb](ssb.md) | StatBank over PxWeb API 2: | free, no key | called |
| [stat_ee](stat_ee.md) | The Estonian statistical database over PxWeb. | free, no key | called |
| [statbel](statbel.md) | Statbel publishes open data files and BeStat, a table tool whose views are listed by an open JSON call. | free, no key | called |
| [statcan](statcan.md) | Every table of the national statistical office (labour, prices, national accounts, trade, housing, business, health, census) through the Web Data Service (WDS). | free, no key | called |
| [statfi](statfi.md) | PxWeb databases of Finnish official statistics, by statistic and table. | free, no key | called |
| [statistik_at](statistik_at.md) | The open data catalogue `data.statistik.gv.at`: | free, no key | called |
| [statsnz](statsnz.md) | Two doors: | free key | called |
| [statssa](statssa.md) | South Africa's national statistics office. | scraped web page | refused |
| [tradingview](tradingview.md) | Economic releases by country, each with actual, forecast and previous, the publisher, a link to its page, the reference period and an importance level. | scraped web page | called |
| [transparency](transparency.md) | The Corruption Perceptions Index: | free, no key | called |
| [treasury](treasury.md) | Three separate hosts: | free, no key | called |
| [tuik](tuik.md) | The Data Portal `veriportali.tuik.gov.tr` (the old `data.tuik.gov.tr` redirects to it) and an SDMX web service behind it. | free key | docs |
| [un](un.md) | Three services of the UN Statistics Division: | free key | called |
| [unece](unece.md) | The statistical database of UNECE, whose tables hold 56 countries and up to 64 with groups: | free, no key | called |
| [unhcr](unhcr.md) | The Refugee Population Statistics Database: | free, no key | called |
| [us-other-agencies](us-other-agencies.md) | `resources/data_sources.json` has 26 bodies with `USA` in `countries` and kind `gov` or `stat`. | free, no key | called |
| [usda](usda.md) | World supply and demand of crops and livestock (FAS PSD), trade and export sales (FAS), US farm statistics (NASS, ERS). | free, no key | called |
| [wef](wef.md) | Report-based indices. | scraped web page | refused |
| [wgc](wgc.md) | Goldhub: | free account | called |
| [who](who.md) | The Global Health Observatory: | free, no key | called |
| [wikidata](wikidata.md) | A structured knowledge base. | free, no key | called |
| [wikipedia](wikipedia.md) | The app reads two pages of it through the MediaWiki Action API, as the source of index members: | free, no key | called |
| [worldbank](worldbank.md) | A single API over 71 thematic databases: | free, no key | called |
| [worldsteel](worldsteel.md) | Steel production, use and trade. | paid | called |
| [yahoo](yahoo.md) | Prices, company data, statements, analysts and screeners for stocks, ETFs, indices, futures, forex and crypto worldwide. | scraped web page | refused |
