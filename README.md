# Deepwater

Qt 6 / C++ desktop app showing commodity markets on an interactive 3D globe.

## Build & run

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt
cmake --build build -j
open build/deepwater.app        # or ./build/deepwater.app/Contents/MacOS/deepwater
```

Needs Qt 6 with WebEngine (the News tab's article reader; on macOS the reader uses the system's WebKit and
WebEngine stays the fallback). A Rust toolchain (`cargo`, found in `~/.cargo/bin` too) is optional: with one,
CMake builds `third_party/adblock-ffi` and the reader gets Brave's ad blocker; without, it is left out.

`--screenshot out.png` renders the window to a PNG and exits (handy for UI checks); `--page <id>`, `--theme <id>`,
`--devtools`, `--country <ISO3>`, `--indicator <n>`, `--supply [port:<id>|oil:<id>|event:<id>]` (the supply chain layer's overview or one of its sheets), `--search <text>` (the Sources page's search box),
`--ai-search <text>` (the AI tab's Hub search), `--news-ticker <sym>`, `--news-article <n>`, `--news-favorite <n>` and `--news-url <url>` (the News
tab's filter, its reader, a card's star, any page in the reader), `--settings [0|1]` (opens the settings window on its language or theme
page; the screenshot then captures that window), `--scroll <0..1>`, `--scroll-end` and `--delay <ms>` choose what it
shows first.

The app icon's source is `resources/icon/deepwater.svg`. After editing it, render the 10 sizes of a macOS
iconset (16 to 1024 px, e.g. with QSvgRenderer) into `deepwater.iconset/` and run
`iconutil -c icns deepwater.iconset -o resources/icon/deepwater.icns`; also refresh
`resources/icon/deepwater-512.png` (the window icon). Avoid SVG clip paths and filters: Qt's SVG renderer ignores them.

### Windows 11

The sources build with MSVC (Qt WebEngine has no MinGW build on Windows); the port is done but has not been built
or run on Windows yet. What differs from macOS:

- **Article reader**: Qt WebEngine (Chromium), the fallback of `NewsWebHost`; no WKWebView.
- **Terminal tab**: PowerShell 7 if installed, else Windows PowerShell, else `cmd.exe`, on a ConPTY
  (`src/quant/ui/WinPty.*`, Windows 10 1809 or later). Ctrl sends control codes to the shell (Ctrl+C interrupts);
  Ctrl+Shift+C / V / A / K copy, paste, select all, clear, Ctrl+C copies when text is selected, Ctrl+V pastes;
  Ctrl+` shows or hides the terminal. The tab keeps the shell's name (no foreground process lookup).
- **Alert notifications**: Windows toasts through a tray icon (`src/quant/ui/Notifications.cpp`) instead of
  UserNotifications.
- **AI tab**: Python from python.org's installer (`%LOCALAPPDATA%\Programs\Python\Python3*`) or `PATH`; the venv's
  interpreter is `Scripts\python.exe`. Pine Script finds `node.exe` on `PATH`.
- **Settings** go to the registry (`HKEY_CURRENT_USER\Software\Deepwater\Deepwater`) and data to
  `%APPDATA%\Deepwater\Deepwater`, so `defaults export` has no equivalent: use `reg export` for UI checks.

Build, in the *x64 Native Tools Command Prompt for VS 2022* (Visual Studio 2022 with *Desktop development with C++*,
CMake 3.21 or later):

```bat
:: Qt 6 for MSVC 2022 64-bit with its add-on modules (same version as the macOS build, 6.12 at the time of writing),
:: from the Qt online installer or aqtinstall:
pip install aqtinstall
aqt install-qt windows desktop 6.12.0 win64_msvc2022_64 -O C:\Qt -m qtwebengine qtwebchannel qtpositioning qtwebsockets qthttpserver qtmultimedia
:: zlib: vcpkg's (recommended), or leave it out and CMake fetches zlib 1.3.1 from GitHub and builds it.
vcpkg install zlib:x64-windows

cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH=C:\Qt\6.12.0\msvc2022_64 ^
      -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
cmake --build build --config Release --target deepwater
:: A folder that runs on any Windows 11 PC: deepwater.exe, then windeployqt (Qt DLLs, plugins, QtWebEngineProcess,
:: its resources, translations, zlib1.dll from vcpkg).
cmake --install build --config Release --prefix dist
dist\deepwater.exe
```

Optional, as on macOS: Rust with the MSVC toolchain (`rustup`, the reader's ad blocker; CMake links the Windows
libraries a Rust static library needs), Node.js (Pine Script), Python 3.10 or later (AI tab). MSVC gets `/utf-8`
(the sources' strings are UTF-8), `/permissive-`, `_USE_MATH_DEFINES` and `NOMINMAX`; the executable icon is
`resources/icon/deepwater.ico` (rendered from `deepwater-512.png`, 16 to 256 px) through `deepwater.rc`.

## Live shipping (AIS)

Live vessel positions come from [aisstream.io](https://aisstream.io). Create an account, generate an API key, then
either paste it in the **Sources** tab (saved with `QSettings`) or launch with
`AISSTREAM_API_KEY=<key>` (takes precedence, not saved).

Only commodity carriers are shown, classified from the AIS ship type: **tankers** (types 80–89: crude, products,
LNG, chemicals) and **cargo ships** (70–79: dry bulk, containers). AIS does not report the cargo itself.
Vessels silent for 30 minutes are dropped; trails appear when zoomed in.

Ships broadcast their type only every ~6 minutes, so static data (type, name, size, destination) is cached in
`~/Library/Application Support/Deepwater/Deepwater/vessel_static.cache` and reused on the next launch: the
first session fills in slowly, later ones classify ships as soon as they appear. The file is rewritten (on the
thread pool, every two minutes at most) only when a report brings something new; each entry keeps the date the
ship was last heard of, refreshed daily, and ships not heard of for a month are forgotten. The aisstream feed delivers
~80 messages/s worldwide (measured; compression makes no difference), so each ship updates every minute or two.

### Chokepoints

Dashed zones on the globe track tankers and cargo ships through ten straits and canals; click one for its details
(passages over 48 h, hourly chart, daily transits). A *passage* is a ship seen inside a zone after more than 6 h away; events are kept 48 h in
`chokepoint_events.bin` next to the vessel cache. Counts are lower bounds, and some zones (Hormuz, Suez, Panama,
Bosphorus at the time of writing) get no reports at all from aisstream.io — the UI says so instead of showing zeros.

Historical daily transits per chokepoint come from **IMF PortWatch** (public ArcGIS endpoint, no key, built from
satellite AIS, published weekly with a few days' lag): tankers / dry bulk / other, tanker capacity, 7-day vs 90-day
trend. Cached in `portwatch.json`, refreshed every 12 h, retried with backoff (the service intermittently rejects
valid queries, and it rejects `orderByFields` / `outFields=*` when a filter is set).

### Probable routes

Selecting a ship resolves its free-text AIS destination (UN/LOCODE like `NL RTM`, port names, `FROM>TO`) to a port and
draws the shortest path on a maritime route network, with distance by sea and time at current speed.

Data: `resources/searoutes.json` is derived from Eurostat's *marnet* network (EUPL-1.2, via
[searoute-py](https://github.com/genthalili/searoute-py), Apache-2.0); `resources/ports.json` combines searoute-py's
major ports with UN/LOCODE seaports (UNECE). Check EUPL-1.2 obligations before redistributing the app.

## Commodity prices

The watchlist, top movers and commodity sheets show real prices, each with its source and time (row tooltip, note
under the chart):

- **Futures** for 20 commodities, from Yahoo Finance (`query1.finance.yahoo.com/v7/finance/spark`, no key, about 10
  minutes behind the exchange): NYMEX (WTI, Brent Last Day Financial, Henry Hub gas, platinum, palladium), COMEX (gold,
  silver, copper in USD/lb, aluminium), CBOT (wheat, corn, soybeans, rough rice), ICE US (coffee, cocoa, sugar No. 11,
  cotton). Three requests every five minutes, 20 symbols at most each: intraday bars over five days (the chart), over
  one day (the previous close: Yahoo's daily bars of a continuous symbol are spliced from other contract months and
  can't give it), and daily closes over three months for LNG (Platts JKM), iron ore 62% and palm oil, which Yahoo only
  carries as daily settlements. Yahoo answers 429 to most user agents, so these requests send a bare `Mozilla/5.0`.
  Cotton's continuous symbol is unreliable: its contract is rolled by hand (`CTZ26.NYB`, the first contract month at
  least two months ahead). The change is against the same contract's previous close; the top movers only rank these
  daily changes.
- **IMF monthly averages** (`IMF.RES/PCPS`, SDMX 2.1 CSV with `detail=dataonly`, ~95 KB for three years, daily check)
  for every commodity: the price shown for coal (Newcastle), uranium, nickel, zinc, lithium (metal, 99%; the IMF
  doesn't state the unit, the values are USD per tonne) and cobalt, which have no free, current futures quote, and for
  any contract without a quote in the last ten days; elsewhere a reference line under the futures price. The IMF
  publishes a month's average early the next month.

Both are cached (`cache_yahoo_commodity_futures.bin`, `cache_imf_pcps.bin`) and can be turned off in **Sources**. The
production sites are a hand-picked sample.

## Forex

The **Forex** page shows the 7 majors plus EUR/GBP, EUR/JPY and USD/CNY: last fix, daily change, 90-day sparkline, and
a correlation matrix of daily log returns (1M / 3M / 6M / 1Y). Source: European Central Bank reference rates via
[frankfurter.dev](https://frankfurter.dev) (no key, one fix per business day, no intraday), cached in `fx_rates.json`
and refreshed every 6 h.

- **Policy rates**: BIS central bank policy rates (daily series, `stats.bis.org`, no key) for the 9 currencies:
  current rate, last move, 1-year change, 3-year step chart; pair cards show the carry (base − quote rate).
- **Capital flows**: CFTC *Traders in Financial Futures* (`publicreporting.cftc.gov`, weekly, no key) for CME
  currency futures: net positions of asset managers and leveraged funds as % of open interest, weekly change and
  52-week history. Official balance-of-payments flows (IMF, quarterly) are a possible later addition.

## Macro indicators (map page)

The right sidebar lists 24 indicators by country in four groups (latest official figures and supply chains: see below). Economy: real GDP growth, inflation, unemployment, policy rate, real policy
rate, current account, budget balance, public debt, GDP per capita (PPP) and GDP; capital flows and trade (below). Each card shows the world median and
a histogram of country values. Clicking one colors the globe by country (legend bottom-left, 5th–95th percentile
scale) and shows the ranking in the left sidebar (the circled question mark by its title explains what the indicator
measures, how to read it and its pitfalls), with a year slider (2010 to the last IMF projection). Clicking a
country on the globe or in a ranking opens its economy sheet: headline figures, last year / this year / next year,
its economic sectors, policy rate history, growth and inflation bars, and the commodity sites located there.

**Economic sectors** (economy sheet): the primary (agriculture, forestry, fishing), secondary (industry: mining,
manufacturing, energy, construction) and tertiary (services) sectors as stacked bars of their share of GDP and of
jobs, with a table of the shares, the value added in dollars (share × the WEO's GDP for that year) and manufacturing
within the secondary sector, plus the change in the GDP shares over ten years (or since the oldest year at least
five years back). The shares of GDP don't add up to 100%: the blank end of the bar is taxes on products less
subsidies. The circled question mark by the section's title opens an explanation read from the country's own
figures: its profile (services, agricultural, extractive, industrial, mature or in transition), output per job of
each sector against the average (share of GDP / share of jobs) and how the shares moved over ten years
(deindustrialisation, early deindustrialisation, rebalancing, tertiarisation); then its history, cause and effect:
the shares of GDP since 1960 as a chart with the events of the written account numbered on it (56 larger
economies), and what the long series show for every country (the change since the first year, the rural exodus,
an industrial peak and whether manufacturing or extraction fell after it, industrialisation, a secondary sector that
swings with commodity prices, the rise of services, a sudden break); around what the three sectors are, why they
matter and their pitfalls. The popover scrolls when it is taller than the screen.

**Real estate** (economy sheet): real house prices (BIS, quarterly, since 1970 for many) over a year and ten years and
from their peak, as a chart (2010 = 100), and their valuation against incomes and rents (OECD, in % of the long-term
average: near its average, dear, very dear, cheap). **Debt**: households, non-financial companies and the government in
% of GDP, with the change over ten years and the median of the countries the same source covers, the total, a chart of
the three since the 1950s where the IMF has them, debt service (BIS, % of income, against its average), bank credit to
the private sector and bad loans (World Bank). Both have a help popover with a reading of the country's figures (boom,
bust, valuation, credit boom, deleveraging, heavy debt against the BIS thresholds, strained households, bad loans).

The **3D** button (globe controls, shown while an indicator is active) switches to a prism map: each country is
raised by its value on the same 5th–95th percentile scale as the colors (remembered with `QSettings`), and the camera
tilts 45° (looking north) so the sides show. Prisms sink toward the horizon so it doesn't turn into a wall of country
sides.

The **Capital flows and trade** group adds net capital inflows (the financial account with the sign flipped: positive
when the country imports capital), FDI and portfolio inflows (foreign purchases, net of sales), the trade balance and
trade openness in goods and services, all in % of GDP, plus export volume growth (WEO, with projections). Flows come
from the balance of payments, which has reported figures only: the year shown by default is the latest one at least
80% of reporting countries have filed (a year behind, ~150 countries).

The **⇄** button (globe controls, above 3D; remembered with `QSettings`) shows a flow map of trade in goods: the 50
largest bilateral flows, or with a country selected its 10 main customers and 10 main suppliers. Arcs follow the great
circle from exporter to importer, width by value, with particles moving toward the importer; hover one for the amount
and its share of both countries' trade. Source: **IMF International Trade in Goods by partner country** (`IMF.STA/IMTS`,
exports FOB as reported by the exporter, latest year at least 90% complete). The service ignores time filters, so the
download is ~27 MB (every year since 1948) and takes up to a minute; it is refreshed weekly and only that year is
cached (`cache_imf_trade_flows.bin`, ~450 KB).

- **IMF World Economic Outlook** via the IMF SDMX API (`api.imf.org`, dataflow `IMF.RES/WEO`, no key, ~1.4 MB),
  191 countries, refreshed daily. Figures for the current year are IMF estimates, later years projections. (The
  older DataMapper API rejects non-browser clients.)
- **IMF Balance of Payments** (`IMF.STA/BOP`, BPM6, same API, no key, ~1.8 MB): annual flows in US dollars, divided by
  the WEO's GDP in dollars. Refreshed daily.
- **Policy rates**: BIS monthly series (`WS_CBPOL`, all ~40 central banks); euro area members get the ECB rate.
- **Economic sectors**: World Bank World Development Indicators (`api.worldbank.org`, no key): value added of
  agriculture, industry, manufacturing and services in % of GDP (national accounts, a year or two behind, since 1960
  for most countries) and employment by sector (ILO modelled estimates, since 1991). One bulk download per series
  (a zip of CSV files, ~100 KB each, every year and country; unzipped with zlib), seven in a row; years whose three
  shares add up to less than 50% or more than 130% are gaps written as zeros or glitches and are dropped. Refreshed
  weekly, cached in a compact form (`cache_worldbank_sectors.bin`, ~1.2 MB).
- **Property and debt**: BIS (`stats.bis.org`, `WS_SPP` real residential property prices, `WS_TC` credit to
  households and non-financial corporations in % of GDP, `WS_DSR` debt service ratios; ~3 MB in three CSV requests,
  about 50 countries); OECD (`sdmx.oecd.org`, house price to income and to rent ratios, ~0.9 MB; its server fails on
  Qt's default `Accept-Language`, so that request sends `en`); IMF Global Debt Database (`IMF.FAD/GDD`, household,
  company and government debt, ~90 countries); World Bank (`FS.AST.PRVT.GD.ZS` bank credit to the private sector,
  `FB.AST.NPER.ZS` non-performing loans, nearly every country). All weekly, cached in a compact form
  (`src/DomesticFinance.*`).
- **Sector histories**: `resources/sector_history.json`, written accounts of the sector history of 56 larger
  economies (G20, most of the OECD, large emerging and oil economies), in English and French: a summary and 3–5 dated
  events, cause and effect, checked against the World Bank series they quote.
- Country names come from `resources/country_names.json` (ISO3 → ISO2, English name, from CLDR); French names are in
  `i18n/deepwater_data_fr.ts` (context `Country`).

### Latest official figures and the sources catalogue

Trading Economics builds its country pages from the national statistical offices and central banks, and from four
free compilations of their figures (its own *databases* page: national sources, FRED, the World Bank, UN Comtrade,
Eurostat). An audit of its 21,900 public pages (`docs/audit-tradingeconomics.md`, November 2026) lists every source
it credits; the free ones with a machine-readable API that cover many countries feed two additions to the map page:

- **Latest figures** (economy sheet, under the IMF table): the monthly and quarterly figures the markets follow, each
  with its previous value and period, from the most direct free source that covers the country (`src/KeyIndicators.*`,
  four `CountrySeriesSet`s, refreshed twice a day, compact caches `cache_oecd_key_indicators.bin`,
  `cache_eurostat_kei.bin`, `cache_imf_consumer_prices.bin`, `cache_bls_key_indicators.bin`):
  - **OECD Key Economic Indicators** (`sdmx.oecd.org`, dataflow `DSD_KEI@DF_KEI`, two CSV requests, ~2 MB, about 45
    economies): CPI, unemployment, industrial production, retail volume, business and consumer confidence, composite
    leading indicator, 3-month and 10-year rates, hourly earnings (monthly); GDP q/q and y/y, current account % of GDP
    (quarterly). Year-on-year rates (`GY`) and levels (`_Z`) only.
  - **Eurostat** (`ec.europa.eu/eurostat/api/dissemination/statistics/1.0`, nine JSON-stat requests of ~10 KB, EU,
    EFTA and candidates): HICP (`prc_hicp_minr`, the ECOICOP ver. 2 data set that replaced `prc_hicp_manr`, frozen at
    2025), unemployment, GDP q/q, industrial production, retail volume, consumer confidence, producer prices, quarterly
    government debt and deficit. Geo codes `EL` and `UK` are Greece and the United Kingdom.
  - **IMF consumer price database** (`api.imf.org`, dataflow `IMF.STA/CPI`, about 190 countries): inflation over a
    year and over a month. The service ignores `startPeriod` but honours `c[TIME_PERIOD]=ge:…` (300 KB instead of
    3.7 MB).
  - **US Bureau of Labor Statistics** (`api.bls.gov/publicAPI/v1`, no key, 25 requests a day per address, one per
    series): CPI and core CPI over a year (from the indexes), unemployment, the monthly change in nonfarm payrolls,
    hourly earnings over a year.
  The façade picks, for each indicator and country, the national source (BLS), then Eurostat, then the OECD, then the
  IMF, unless a later one is a quarter or more fresher (a frozen series, a lagging compilation). Times are fractional
  years at mid-period; monthly and quarterly series are told apart by the façade. Two heatmap indicators come from it,
  in the sidebar's **Latest official figures** group: inflation and unemployment over the latest month.
- **Official sources** (economy sheet, after the commodity sites): the bodies behind the country's statistics as
  credited on tradingeconomics.com, with the number of indicators each publishes, the kind of body, whether its data is
  free and how a program reads it (`resources/data_sources.json`, read by `src/OfficialSources.*`). The whole
  catalogue is part of **Sources**, as cards like the app's own sources, in five collapsed groups (statistical
  offices, central banks, ministries and agencies, international organisations, private institutes) built on demand,
  a hundred cards at a time; the page's search box and its country and access filters (free data by default) apply
  to every card. Each card shows what the body publishes, its access and API, the state of its site from the last
  inventory (`test` in the JSON: reachable, HTTP status, time, date; 401/403 means the site refuses a non-browser
  client), **Test** (its home page, timed) and **Open the site**. The user interface never names the website the
  catalogue was built from. The catalogue is rebuilt from the crawl described in the audit; the names, sites and attributions are
  Trading Economics's, the classification (statistical office, central bank, ministry, international organisation,
  private institute; free, headline free, paid) and the API notes are ours.

- **National offices read directly** (`src/NationalSources.*`, one `CountrySeriesSet` per country, tried before
  the compilations): Brazil (BCB SGS: IPCA; IBGE SIDRA tables 6381, 8888, 8880: unemployment, industrial
  production, retail), Argentina (INDEC through `apis.datos.gob.ar`: CPI and EMAE index levels, the % over a year
  computed here since the API's own transformation lags; quarterly unemployment), South Africa (SARB `HomePageRates`:
  CPI, PPI, repo rate; the answer only carries the latest month, earlier ones stay from the cache; the server is
  intermittent and wants `Accept: application/json`), Singapore (SingStat table `M213781`, `sortBy=key desc` or
  the oldest month comes back). China's NBS, India's MOSPI and Malaysia's DOSM were tried and left out (a
  non-browser client is refused, or no documented parameters).
- **Sources with a free key** (`src/KeyedSources.*`, `KeyedSeriesSet`: the key lives in the settings under
  `keys/<id>`, entered in Sources; without one the set reports "No API key" and never downloads): FRED (PCE
  inflation from the index, housing starts, initial claims, trade balance, Michigan sentiment), EIA (weekly crude
  stocks and production, gasoline price), BEA (NIPA T10101, GDP at an annual rate), Banxico (SIE: annual inflation,
  target rate), Bank of Korea (ECOS 901Y009, CPI index), Turkish central bank (EVDS `TP.FG.J0`, key in a `key`
  header). Their parsers follow the documented formats but were written without keys: the Sources **Test**
  button and the first download will tell.
- **UN Comtrade** (`ComtradeProducts`, on demand): the economy sheet's **Trade by product** section, exports and
  imports by HS chapter for the latest full year (two years back), with shares. Reporter codes come from
  `files/v1/app/reference/Reporters.json` (cached 90 days); with a key the data API is used (header
  `Ocp-Apim-Subscription-Key`), without one the public preview endpoint (500 records, a few requests a day). One
  cache per country (`cache_comtrade_<iso3>.bin`, 30 days).
- **Private surveys' headlines** (`src/SurveyHeadlines.*`): the last ten weeks of TradingView's economic calendar
  (the Calendar tab's source), the latest actual of each survey and country: PMIs (S&P Global with its sponsors,
  ISM, China's NBS), business climate (Ifo, NAB, ANZ, CBI, NFIB, NZIER), economic sentiment (ZEW, Sentix),
  consumer sentiment (Michigan, Conference Board, GfK, Westpac, ANZ Roy Morgan), ADP payrolls. One publisher per
  country and series (the first matching rule wins: the ISM over S&P Global for US PMIs); sub-indexes, weekly and
  "change" variants and preliminary estimates are skipped; the month is the one named in the event's period (a
  forward-looking survey released in October for November keeps November). Each publisher has a row in Sources
  with a switch; the cache keeps the publisher of each figure as `# ISO3/k id` lines.

### Supply chains

The **⚓** button (globe controls, remembered with `QSettings` as `supplyLayer`) shows the supply chain layer and
its overview in the left sidebar; the **Supply chains** group of the right sidebar colors the countries.

- **Ports** (dots): the ~2,000 ports of **IMF PortWatch** (`PortWatch_ports_database`: position, UN/LOCODE, vessels
  counted by type, main industries, share of the country's seaborne trade), colored by their port calls over the
  latest seven days published against the same weekdays 52 weeks before (`Daily_Ports_Data`; full color at ±40%,
  grey without figures), sized by their vessel count. The 120 busiest show from afar, the others as the view closes
  in (zoom 1.8, 3, 5). A port's sheet gives its calls, tonnes unloaded and loaded (cargo plus tanker fields; `import`
  and `export` alone are reserved words the service rejects), its profile and the disruptions that named it. The
  service allows 3,000 request units a minute, and a filter on anything but `year`/`month`/`day` (a port id) or a
  statistics query over several days times out (it answers 400 or 504); it also fails a valid query after ~12 s
  now and then and answers the same query at once right after. The client asks for one day at a time (~10 s each,
  three at once, 14 days in all), retries a step up to six times (3 s after a 400, 10 s after a network error, 65 s
  after a 429), and shows the port list and the disruptions as soon as they arrive. The port list is downloaded
  again after 30 days, the days daily; one compact cache (`supply_portwatch_ports`).
- **Disruptions** (rings): PortWatch's disruption events (`portwatch_disruptions_database`: GDACS cyclones,
  earthquakes, floods, wildfires and its own events such as the Strait of Hormuz), those running or ended less than
  30 days ago, colored by alert level, with the ports they hit.
- **Oil storage** (diamonds): 44 hand-picked trading hubs, strategic reserves and import or export terminals
  (`resources/supply_chain.json`, names and descriptions translated in `deepwater_data_fr.ts`, context `OilSites`;
  positions within a few kilometres). No free source publishes a site's own stocks except the **EIA** (weekly, the
  EIA key): Cushing (`W_EPC0_SAX_YCUOK_MBBL`), the Gulf Coast region (`WCESTP31`) and the Strategic Petroleum Reserve
  (`WCSSTUS1`, the total of its four sites); every site also shows its country's month-end crude stocks from
  **JODI-Oil** (`world_Primary_CSV.zip`, 23 MB, 287 MB unpacked: read as it is inflated with `forEachZipLine`,
  crude closing stocks in thousand barrels kept from 2015, compact cache). ARA, Fujairah and Singapore stocks are
  published weekly by Insights Global, S&P Global and Enterprise Singapore without a free API.
- **Overview**: the **New York Fed's Global Supply Chain Pressure Index** (the interactive chart's CSV, one column
  per vintage: the last one is read), the ports' week against a year before with the largest falls and rises among
  busy ports, the disruptions, **US freight** from FRED (ATA truck tonnage `TRUCKD11`, Cass shipments
  `FRGSHPUSM649NCIS`, freight services `TSIFRGHT`, deep sea freight PPI `PCU483111483111`; the FRED key) and the
  EIA's weekly stocks.
- **Heatmap indicators** (group *Supply chains*, ids 18–23): wheat, corn, rice and soybean **yields** against the
  year before (USDA **PSD** bulk CSV, `psd_grains_pulses_csv.zip` and `psd_oilseeds_csv.zip`, yield attribute 184,
  estimates and forecasts of the current marketing year revised monthly; the USDA gives the EU as a whole, so EU
  members come from **Eurostat** `apro_cpsh1`, yield in EU standard humidity; PSD country codes are mapped to ISO
  in `supply_chain.json`); **road freight** in tonne-km against the year before (**ITF** annual trends,
  `DSD_TRENDS@DF_TRENDSFREIGHT`, mode `ROAD`, ~45 countries, `Accept-Language: en`); **crude oil stocks** at the
  latest month against a year before (JODI). The slider reads "2025/26 marketing year"; the default year is the
  latest one with 90% of the best recent coverage (USDA forecasts come before Eurostat's harvest figures). The
  globe's tooltip adds the level (t/ha, tonne-km, barrels) and the source; a country sheet has a *Supply chains*
  table over three years.

All of it is in **Sources** › *Map · supply chains*, one switch per source.

Dev flags: `deepwater --country FRA --delay 20000 --screenshot out.png` opens an economy sheet,
`--indicator 16` turns a heatmap on (the id is the `MacroIndicators::Id` value), `--scroll 0.6` or `--scroll-end`
scrolls the sheet (or the shown page) before the shot, `--search brazil` fills the Sources page's search box.

## Charts (Quant Terminal port)

The **Charts** tab is a C++/Qt port of Quant Terminal 1.0.0 (MIT, Lewis Jackson; licence and notices in
`src/quant/LICENSE` and `src/quant/NOTICE`): the same trading terminal, UI and behaviour, without Node or a browser.
The original JavaScript sources are no longer in the repo. Code in `src/quant/` (see `src/quant/PORTING.md`):

| Library | Ports |
|---|---|
| `qtm_core` (`core/`) | `public/lib/*.js`: 87 indicators, series and chart-type transforms, seasonals, backtester, quant checks; custom JS indicators run unchanged on QJSEngine; Pine Script indicators run on PineTS (below) |
| `qtm_server` (`server/`) | `server.js` + `lib/*.js`: data providers and fallbacks, feeds, CSV imports, alerts (every 15 s), store, PTQ connector, and the same local HTTP API on 127.0.0.1:3777 for your scripts (`quant-terminal/examples` work against it) |
| `qtm_chart` (`chart/`) | Lightweight Charts, re-implemented with QPainter |
| `qtm_ui` (`ui/`) | `public/js/*.js`, `index.html`, `app.css`: the terminal UI |

The backend lives for the whole session (alerts keep firing on other tabs); its data is in
`~/Library/Application Support/Deepwater/Deepwater/quant-terminal/` (`data/`, `custom/`). Numbers were checked
against the JavaScript originals with Node: `tests/quant/{core,backtest,server,js}` (bit-exact), chart renders against
the real Lightweight Charts, and the UI against screenshots of the web app (harnesses `tests/quant/ui`,
`tests/quant/panels`, `tests/quant/chart`; audit checklists in `tests/quant/audit`). The C++ tests run on their stored
reference data and fixtures (`tests/quant/fixtures`); regenerating the references (`make_reference.mjs`) and the
side-by-side comparisons need a copy of quant-terminal at `quant-terminal/`. The terminal's texts stay in English.
The page takes Deepwater's theme (Settings) and follows it live; its own light/dark button only shows in the test
harnesses (`qtm_panels_demo --palette <theme id>` shows a Deepwater theme there).
Dev flags: `deepwater [--theme <id>] --page charts --delay 8000 --screenshot out.png` (`--theme` is for that run only,
not saved).

### Pine Script indicators (PineTS)

The custom indicator folder (`custom/`, above) also takes Pine Script files: a `name.pine` file (`//@version=6`,
`indicator(...)`, `plot(...)`) shows in the indicator editor and on the chart like a `name.js` one, under the id
`custom:name`. The editor saves a Pine source pasted in it as `.pine` (it recognises `//@version=` or an
`indicator()`/`strategy()` line without `export default`). The scripts run on [PineTS](https://github.com/LuxAlgo/PineTS)
(LuxAlgo's Pine Script runtime, v0.13.1, AGPL-3.0, vendored in `third_party/pinets/` with its licence and update
notes), which needs **Node.js 18 or newer** on the machine: the app looks at the `pinets/node` setting, the
`DEEPWATER_NODE` variable, the PATH, then Homebrew, `/usr/local`, nvm, volta and fnm. Without Node the `.pine`
file is listed with the reason in its error line; nothing else in the app depends on Node.

How it works (`src/quant/core/PineScript.{h,cpp}`, `PineIndicators.{h,cpp}`, `third_party/pinets/deepwater-pinets.mjs`):
each thread that computes indicators starts one `node deepwater-pinets.mjs` child (the bundle and the bridge are
compiled in as resources and copied to `~/Library/Application Support/Deepwater/Deepwater/pinets/` first) and talks
JSON lines to it; a run sends the chart's bars (`[time, open, high, low, close, volume]`), the script and the input
overrides, and gets back the declaration, the `input.*()` metadata and every plot with one value per bar. Loading a
`.pine` file runs it once on the test bars to build the chart definition: the `indicator()` title and `overlay`,
`input.int/float` → int/float inputs (with min/max), `input.bool` and inputs with `options` → select inputs, the
first `input.source` → the source input, other input types keep the script's defaults; `plot()` series → line or
histogram outputs (`style_histogram`/`style_columns`, colours, line width, per-bar colours on the first histogram),
`plotshape()`/`plotchar()` → markers (location → above/below/in bar, shape → arrow up/down, circle, square, the char
or text as label), `hline()` → levels. `bgcolor`, `fill`, `plotcandle`, labels, lines, boxes, tables and
`strategy.*` orders have no chart counterpart yet and are ignored. Inputs go back to PineTS by their `in_N` id, so
the chart's keys are the Pine variable names (`len`, `useEma`, …). The file is re-read when it changes, like `.js`
ones. Numbers match the C++ `ta.*` port (checked by `tests/quant/pinets`); a 5000-bar run takes about 60 ms.

The JavaScript PineTS runs for a script (an `async $ => { ... }` function over `$.data` and `$.pine`) is what
PineTS's transpiler produces: `deepwater --pine-js file.pine` prints it and exits (`qtm::pineIndicatorJs()` and
`qtm::pine::transpile()` in code). It needs the PineTS runtime to run; it is not a `custom/*.js` indicator.

Live charts, where the port departs from the JavaScript for speed (same pictures): the local time offset of intraday
charts is kept until the next daylight-saving transition instead of asked of the system for every bar (`mktime` was
most of a redraw); a tick on a simple chart is applied to both the raw and the shown bars, and a transformed chart
(Heikin Ashi, Renko…) is rebuilt at most every 1.5 s, the last tick of a burst a moment later; a chart left open drops
its oldest bars once 500 more than the bar setting have come in, the view staying on the same bars. The session
volume profile keeps its sessions and closed sessions' profiles between frames; custom JS indicators get their bars
as one buffer the JS side turns into objects; the watchlist keeps its rows when only the quotes change; Yahoo is asked
only for the days a request needs (a live poll's 5 bars: tens of days, not 20 years).

### Range slider

Under every chart pane (a Deepwater addition) a thin strip shows the closes of all the bars loaded as a small line,
and a window over the part the chart shows (the rest shaded, a handle on each edge; its track lines up with the time
axis). Drag inside the window to scroll the chart at the same zoom; click outside it to centre it there and keep
dragging; drag an edge to zoom (narrower = in), never under 10 bars nor past the data. Only the time axis moves: the
price scale fits the visible bars as usual. Scrolling or zooming the chart itself moves the window: there is one visible
range, the chart's (the slider reads it when the chart reports a change, at most once a frame, and sets it). New bars:
when the last bar is in view the chart follows them, and the window with it; otherwise both stay. A new symbol or
timeframe shows the last 160 bars, as before.

Details that matter: the strip spans the chart's time points, which with a compared symbol are every series' times
(US minutes reach further back than a crypto pair's, and sit among its bars): each close is drawn at its own index,
and the strip's right end is where a fresh chart's view ends (9 bars after the last). The window is in edges (bar i
spans i - 1/2 ... i + 1/2; the chart's own range names the edge bars' centres). A pan keeps the width (even under 10
bars, if the chart was zoomed in that far) and does nothing when every bar is already in view; a resize keeps the other
edge; the chart keeps two real bars in view, so a narrow window stops there. What a gesture holds is fixed when it
starts, so moves faster than the chart's frames don't drift. Edges grab 8 px outwards and at most a third of the window
inwards (a 10-bar window over thousands of bars is ~4 px wide and still has a middle to drag). Cursors: resize on the
edges, open / closed hand on the window. Qt keeps sending a drag to the widget it started on (what setPointerCapture
does on the web), so it goes on outside the strip. The line is drawn once into an image and only redrawn when the
bars, the size or the theme change: a frame of the strip takes 0.13-0.15 ms with 3,000 bars, a drag step (chart and
strip) 2.4-2.8 ms (M3 Pro, Retina). The test battery found and fixed: a half-bar offset (edges vs centres), the
chart's two-bar rule, a window drawn wider than it is hit, and in the chart itself the last-160-bars view, the
following of new bars and the shift when old bars go, which all took bar i for index i and so opened on old bars
with a compared symbol. The layers that draw at bars and inside them (footprint, Big Trades, the liquidity heatmap)
made the same assumption and were shifted sideways with a compared symbol: they now place each bar at its own index
and a time inside its bar's slot, which ends at the chart's next time point (`src/quant/ui/BarAxis.*`; the heatmap's
image is drawn in runs of bars that are side by side on the chart). Code: `src/quant/core/RangeSlider.*` (the rules:
pure), `src/quant/ui/ChartNavigator.*`,
`lw::Chart::visibleLogicalRangeChanged`, `TimeScale::pointCount` / `timeToLogical`; tests in `tests/quant/navigator`.

### Footprint (order flow)

**Footprint** in the chart type list (a Deepwater addition) shows, inside each candle, the volume traded at each
price row split by the aggressor's side: **bid** = aggressive sells (trades that hit the bid), **ask** = aggressive
buys. It works on Binance pairs (BTCUSDT…) from 1m to 1h: the trades are Binance's aggregated trades, the history
from REST (`/api/v3/aggTrades`, 1,000 a request, four requests at a time, newest first, back to the chart's first
bar or the history budget: 300,000 trades by default, about half a minute) and the live ones from the
`<pair>@aggTrade` WebSocket. Binance gives the aggressor (`m` = buyer is maker → a sell), so the sides are exact; the
classifier also has the quote rule (against the best bid/ask, Lee-Ready) and the tick rule for feeds without it, and
the legend and an "APPROXIMATE SIDES" badge say when sides were estimated. Trade ids are consecutive: a live trade
that skips ids (dropped socket, app hidden) has the gap fetched first, so nothing is counted twice or missed; away
longer than the budget, the history starts over. Where binance.com is blocked and the history comes from binance.us
(a separate exchange, with its own trades and ids), the live trades come from binance.us's stream too. Checked live:
each closed candle's footprint volume equals Binance's own candle volume (to 1e-14), across a dropped socket and a
minute with the app hidden. Other symbols and timeframes show plain candles and the reason in
the legend; candles before the oldest trade loaded stay plain (the legend says from when the footprint starts).

Per candle: delta per row (ask − bid) and in total, volume per row and the **POC** (most volume; ties go to the row
nearest the candle's middle, then the lower), **diagonal imbalances** (buy at P when ask(P) ≥ ratio × bid(P − 1
row), sell at P when bid(P) ≥ ratio × ask(P + 1 row); the row below the candle's low and above its high is no
comparison, an untraded row inside the candle counts as 0; the stronger side must reach the minimum volume, by
default the candle's own average volume per row) and **stacked imbalances** (N or more in a row on the same side,
drawn as a zone that runs right until a later bar trades into it).

Display (the gear next to the chart type, saved with the chart as `footprint` in the pane state): **Bid x Ask**
(`bid x ask` per row), **Delta** (one signed number per row, coloured by sign, stronger with size) or **Volume
profile** (a bar per row). Every mode outlines the POC, marks imbalances (a bar on the winning side, the number in
colour and bold), keeps a thin OHLC candle on the left of each column and has a strip under the candles with each
candle's delta and volume. The detail follows the zoom: numbers when they fit (rows ≥ 11 px), coloured cells when
rows are ≥ 1 px and candles leave ≥ 3 px of cells beside the thin candle (about 10 px wide), plain candles below that. **Row size** is Auto by default: the 1-2-5 number of
ticks (Binance's `PRICE_FILTER` tick) that makes rows about 14 px tall at the current vertical zoom (kept while rows
stay between 11 and 32 px; regrouping 300,000 trades takes 10–25 ms, once the zoom stops), or a fixed 1, 2, 4, 5,
10… ticks, so the POC and imbalances don't move with the zoom. Also set there: the imbalance ratio (3), minimum
volume (Auto), N for stacked imbalances (3), the buyers', sellers' and POC colours, and the history budget.

Prices are whole ticks (int64) everywhere, so rows never drift with floating point; each candle keeps its rows in a
dense array that grows at either end, and a trade only touches its row and the candle's totals. New trades redraw
the chart at most 15 times a second; a frame takes 1–3 ms with 10 to 400 candles on screen (M3 Pro, Retina).
Code: `src/quant/core/Footprint.*` (classification, ticks and rows, aggregation, analysis: pure, no Qt widgets),
`src/quant/ui/TradeFeed.*` (Binance trades, shared with Big Trades), `src/quant/ui/FootprintLayer.*` (the drawing, a series primitive
over a transparent main series), `Providers::binanceTickSize` / `binanceAggTrades`; tests in `tests/quant/footprint`.

### Big Trades

**Big Trades** in the indicator list (Volume group, a Deepwater addition) puts a bubble on the chart for each large
aggressive order, from the same Binance trades as the footprint (one feed per chart, `TradeFeed`; Binance pairs only,
any time-based chart type). Trades are first **aggregated**: consecutive trades on the same side less than the window
(50 ms) after the aggregate's first one, or only those of the same millisecond, at one price or, with *Sweep*, at prices
moving one way (up for a buy, down for a sell), become one aggregate with its start time, side, volume, volume-weighted
price, min/max and executions (Binance's aggTrades already merge one order's fills at one price). An aggregate is
**big** when its volume reaches the threshold of its moment and stays under the max (0 = none; stack several Big Trades
with min/max tranches):

- *Manual*: the min volume.
- *Auto* (default): the 99th percentile of the aggregates of the last 30 minutes (exact, kept sorted in blocks of 512:
  ~0.3 µs an aggregate, no full sort), times a volatility factor, ATR(14) / ATR(120) on one-minute bars built from the
  trades, kept between 0.5 and 2 (1 until two hours of trades; can be turned off). Under 100 aggregates in the window
  the min volume is used, 0 = nothing shown yet.

Each aggregate is judged when it closes, against the threshold computed without it, then joins the window: the history
never looks ahead. The legend shows the threshold now and how it was made (`≥ 0.29 (p99 of 30 min = 0.363 × 0.80
volatility) · 2817 big trades`). The history loaded with the chart is run in the background, again every 2 s while it
loads (bubbles appear from the newest back), then live trades go in one by one; an aggregate closes a second after its
window (trades arrive late).

Bubbles sit at the volume-weighted price and at the exact time inside the candle, coloured by side (buys / sells,
Settings › Colours, the theme's by default); area proportional to volume (radius = max × √(volume / biggest
aggregate so far), between the min and max sizes, the same reference for every tranche), opacity from the min opacity
up with √volume; circle, square, diamond (same area) or the volume in figures; optionally the volume inside a bubble
big enough. Hovering one shows its time to the millisecond, side, volume, average price, range, executions and how
long it took. Only visible bubbles are drawn (binary search on time; at most the 4,000 biggest), the biggest last, from
cached sprites; new ones redraw at most 15 times a second (1-4 ms a frame with ~500 to 2,800 on screen, M3 Pro).
Code: `src/quant/core/BigTrades.*` (aggregation, rolling percentile, minute ATR, detection, bubble sizes: pure),
`src/quant/ui/BigTradesLayer.*` (model per indicator, layer, tooltip); tests in `tests/quant/bigtrades`.

### Liquidity heatmap

**Liquidity Heatmap** in the indicator list (Volume group, a Deepwater addition) draws, under the
candles, the resting orders of the pair's Binance order book over time, Bookmap-style: each column is the book at one
instant, each cell the quantity resting in one price row, brighter for more. Binance pairs only, any time-based chart
type; best on 1s to 1m charts. The current book (the newest column) is shown as a compact DOM at the right edge of the
chart, one bar per heatmap row (bids in the buyers' colour, asks in the sellers', the row holding the spread in grey),
the quantity in figures when the row is tall enough; it sits right of the last bar when the chart leaves at least 56 px
there (the right offset, or a replay cursor), up to 96 px wide, and otherwise at the right edge, over the newest bars.
Settings: sampling interval (100 ms, 250 ms by default, 500 ms, 1 s), history kept (120 minutes), price rows grouped
(1, 2, 4, 5, 10, 20 or 50 storage rows summed into one), what a pixel shows when several cells fall in it zoomed out
(Max by default: a wall stays visible; or Mean: steady liquidity outweighs a moment's), the colours, the DOM on or
off, the trade bubbles, the stability filter, the pulled / absorbed marks and their thresholds, recording to disk and
how many days of recordings to keep, a debug overlay (feed, book, store, pyramid, texture, recordings, events, CPU and
latency figures).

**Trade bubbles** (on by default) put the aggressive orders over the liquidity they hit: they are Big Trades bubbles
(below), from the pane's trade feed (shared with the footprint and Big Trades), aggregated the same way (50 ms, same
price), buys in the buyers' colour and sells in the sellers', area proportional to volume (6 to 36 px), with the same
tooltip. Which ones: every aggregate from a min volume, or by default the top 10 % of the last 30 minutes' aggregates
(no volatility factor); the legend shows the threshold now (`bubbles ≥ 0.024`). The trade history loads with the
chart, so bubbles also reach back before the heatmap began. The price itself is the chart's candles (or a line: pick
the Line chart type).

**Stability filter** (off by default; 500 ms to 10 s): only the liquidity that rested at least that long is drawn —
per row, the smallest quantity over the samples of that time, rows matched by price, nothing where a sample is
missing. It is an **approximation** (the setting and the legend say so): Binance's book is by price level, not by
order, so an order pulled and another placed at the same level in between look like one that stayed, and what happens
inside a sample interval isn't seen. Turning it on (or changing the zoomed-out aggregation) recomputes the history on a
worker thread from a copy of the samples: the UI stops for the copy only (15 ms for 2 hours, against 455 ms when it
was done on the UI thread), the previous picture stays until the new one is in, and the samples that came meanwhile
are added; then each sample is filtered as it comes.

**Pulled and absorbed liquidity** (on by default; from the samples themselves and the pane's trades,
`core/HeatmapEvents.*`):

- *Pulled* (an orange cross where it went, a dashed line to where the price got there): a big level (≥ 5 × the median
  level of the band, settable), 1 to 10 rows from the touch, loses at least 70 % in one sample, trades at it explain
  less than 30 % of the loss (cancelled, not filled), it doesn't come back, and the touch reaches its row within 5 s.
- *Absorbed* (a violet bar at the level's price while it held, a diamond where the price left): while the best bid
  stays on one tick, aggressive sells at that tick reach the size resting there (1 ×, settable: more traded than ever
  rested means it was refilled), for at least 0.5 s, and the price then leaves it upwards; the same for the best ask
  with aggressive buys. Ticks, not rows: each sample keeps the best bid's and best ask's own quantities, since near the
  price a ~2 $ row holds both sides.

**Recording and replay.** With *Record the order book to disk* on, what Binance sends is written as it comes
(`core/DepthRecord.*`): one file per pair and UTC hour, `quant-terminal/data/depth/<PAIR>/yyyy-MM-ddTHH.dwdepth`
(binance.us: `<PAIR>-US`), the raw diffs, the REST snapshots, a checkpoint of the book at the start of each file (so
each file reads on its own) and a mark where the socket was lost; prices as tick differences, quantities as whole
1e-8 units (exact), compressed in blocks written every half second (a crash loses at most that; a block cut short ends
the reading, what came before stays). About 6 MB an hour on BTCUSDT (400 kB for 4 minutes; ~140 MB a day recording
non-stop); files older than the days kept (7) are deleted when recording starts. Two charts of the same pair don't both
write: the second says so in the legend. Whenever a heatmap opens, the recordings that reach into its history window are
read first, on the feed's thread, and replayed through a book and a sampler of their own, exactly as live (the test
replays a session with buffered updates, a hole, a lost socket and a second session: every column identical), so the
heatmap starts with what was recorded (4 minutes: 949 columns in 60–73 ms), a hole between sessions, then live; the
legend says how many files were loaded. In the chart's **Replay**, the heatmap stays (the live stream stops): it shows the
recorded depth up to the cursor, the DOM shows the book at the cursor, and marks after the cursor are hidden. Pulled /
absorbed marks over recorded depth wait for the trade history to reach back to it (Binance's aggregated trades, the
chart's trade feed), so they come out as they did live (75 live, 77 replayed with one more minute). The tick size and
the price that sets the row size come from Binance even in replay, so opening one needs the network.

Checked live on BTCUSDT 1s over 4 minutes: 25 pulled, 1 absorbed (the debug overlay also counts the touches that held
and the best traded / resting ratio among them: 41 and 1.04; at the old 2 × threshold no touch in 4 minutes came
close, at 3 × the median level pulled marks came every 4 s). The legend shows the counts.

Colours: bids and asks share one scale (liquidity is liquidity; the candles and the DOM show which side is which). The
reference size is a percentile of the visible cells (95th by default, settable), so the picture reads the same on a
thin pair and on BTC. **Intensity** multiplies it (x = intensity × size / reference, capped at 1), **contrast** is a
gamma on x (above 1 only the big levels stand out, below 1 small ones show), and cells under a **threshold** × the
reference (0.02) aren't drawn at all, whatever the intensity and contrast. Palettes: **Deep** (dark blue → cyan →
yellow → white), **Thermal** (black → red → orange → yellow → white), **Mono** (white ink) and **Custom** (three colours,
Settings › Colours); faint liquidity is translucent over the chart's background, the strongest opaque. On a light
theme the presets darken instead (Deep: light blue → blue → violet → crimson; Thermal: yellow → orange → red → black;
Mono: black ink), so big walls stay the most visible; every preset gets steadily brighter (dark themes) or darker
(light ones) with the size.

Data: the book is rebuilt from Binance's depth (a REST snapshot of 5000 levels a side and the `<pair>@depth@100ms`
diffs, market by price: one quantity per price level, not per order), with Binance's resync rules: updates before the
snapshot are buffered, a hole in the update ids or a crossed book (best bid ≥ best ask) stops the book until a new
snapshot (3 s apart at least). Binance sends the diffs every 100 ms, so an order placed and pulled within 100 ms is
never seen, and it has no history of the book: the heatmap fills in from when the chart opens. The feed runs on a
thread of its own (`DepthFeed`, socket, book and sampler); it follows the trade feed's rule for binance.us. Every
interval the book is sampled into a column: the quantity per price row (~2 $ on BTCUSDT: 2e-5 of the price, rounded to
1-2-5 ticks; bids and asks on the same scale) in a band of 800 rows around the price (±800 $ on BTCUSDT, inside the
snapshot's reach), stored as 16-bit log codes (0.07 % off at most) in a ring of columns: 23.5 MB an hour at 250 ms
(47 MB for 2 hours). A column repeats the last state when no update came, and the book drops levels far from the price
so it stays bounded.

Drawing: the chart is painted with QPainter, so the heatmap is an image (`HeatmapTexture`) of texels in time × price.
Rows are grouped first (summed), then a texel is a power of two of samples and of grouped rows chosen so it is about a
device pixel, the cells in it reduced by max or mean; it is opaque (blended over the chart's background once, so
drawing it is a scaled copy). Zoomed out it reads the store's **time pyramid** (`ColumnPyramid`: levels of 1 s, 4 s,
16 s … at 250 ms, each column the max or the mean of the samples in it, holes left out, ~a third of the store's memory)
instead of the samples: a 2-hour view reads ~450 columns, not 28,800. Max over a pyramid level is exact for single
rows, an upper bound for grouped ones (the sum of each row's max). The texture covers the view plus half of it on each
side in time and a quarter in price; scrolling or zooming inside it only changes where it is drawn, new samples only
redo their own texel column, and it is rebuilt when the view leaves it or the texel size changes. The colour scale is
relative to the 95th percentile of the visible texels (kept until it moves by 8 %). New columns redraw the chart at
most 60 times a second. Measured on BTCUSDT (M3 Pro, Retina, full-width chart): live on 1s, the heatmap adds 0.4–0.6 ms
to a 2.3 ms chart frame, still, panning or zooming; on 1m over 2 hours of history (made up for the test), ~0.9 ms a
frame, a rebuild 1.5–2 ms, the worst zoom frame 7 ms (125 ms without the pyramid); the DOM 0.3–0.8 ms; the depth
thread uses 0.5 % of a core; a column reaches the screen 25 ms after it is made.

Code: `src/quant/core/OrderBook.*`, `src/quant/core/Heatmap.*` (book, sampling, store, time pyramid, stability filter, grouping, normalisation, palettes:
pure), `src/quant/core/HeatmapEvents.*` (pulled and absorbed liquidity: pure), `src/quant/core/DepthRecord.*` (recordings:
format, reader, replayer: pure),
`src/quant/ui/DepthFeed.*` (Binance depth on its thread, the recorder, loading recordings), `src/quant/ui/HeatmapLayer.*` (texture, layer, DOM, debug overlay),
`Providers::binanceDepth`; tests in `tests/quant/heatmap` (a minute at 1,000 updates a second: ~1.1 µs an update, flat
from the first seconds to the last).

### Terminal

The Charts page's bottom panel has a **Terminal** tab (a Deepwater addition): your login shell (`$SHELL`, else zsh)
in a pseudo-terminal, like VS Code's integrated terminal. ``Control+` `` opens it and focuses it, or hides the panel
when it already has the focus; several sessions can run side by side (`+`, the bin kills one), and they keep running
while you are on another tab, until the app quits. Escape codes are decoded by libvterm 0.3.3 (MIT, vendored unchanged
in `third_party/libvterm/`): 256 colours and true colour, the alternate screen (vim, htop, less), mouse modes,
bracketed paste. Scrollback of 5000 lines (wheel, `Shift+PageUp/PageDown`), select with the mouse (double click: a
word, triple click: a line), `Cmd+C` / `Cmd+V`, `Cmd+K` clears. Option types what your keyboard layout gives it
(`@ [ { | ~` on French Macs); `Option+←/→` jumps words, `Cmd+←/→` goes to the start / end of the line. Drag the top
edge of the bottom panel to resize it (remembered). Code: `src/quant/ui/Terminal.*` (forkpty), `src/quant/ui/WinPty.*`
(ConPTY, Windows).

## Calendar

The **Calendar** tab has a sub-header with three tabs (the last one shown is remembered):

- **Economic**: releases of 21 large economies and the euro area from TradingView's public economic calendar
  (`economic-calendar.tradingview.com/events`, no key, it wants a `tradingview.com` Origin): last week, yesterday,
  today, tomorrow, this week, next week or a date picked, filtered by country and impact (both remembered). Actual,
  forecast and previous with the unit and scale as published; the actual is green above the forecast, red below. One
  request covers last week to the end of next week (~1 MB, ~1000 events), kept in a compact cache
  (`cache_tradingview_economic_calendar.bin`) and refreshed every 10 minutes while the tab is shown; a date outside
  that window is downloaded on its own. Times are in your time zone.
- **Corporate**: US listings from Nasdaq's public calendars (`api.nasdaq.com`, no key, browser user agent and a
  nasdaq.com Origin): earnings five weekdays back (recent: EPS, consensus, Nasdaq's surprise; a result without a
  surprise yet is shown dim, Nasdaq hasn't confirmed it) and ahead (upcoming: time, consensus, last year), dividends
  going ex over the next seven weekdays, upcoming splits (reverse splits in red) and IPOs of last month, this month and
  next. About 25 requests one after the other (~25 s the first time), refreshed every 3 h, cached as received.
- **Market hours**: 21 stock exchanges and 5 futures markets, live: open / break (lunch, daily halt) / closed, the time
  to the next close or open, and when it happens in your time. Regular sessions are written in the code
  (`MarketHoursView::stockExchanges`, `futuresMarkets`) in each exchange's IANA time zone, so daylight saving is handled;
  B3 closes at 18:00 instead of 17:00 while New York is on standard time. Holidays are caught on the day: the last
  session of an index of each exchange from Yahoo (`v7/finance/spark`, `meta.currentTradingPeriod`, every 15 minutes
  while shown) older than today, a quarter of an hour after the exchange should have opened. Future holidays, early
  closes and pre / after-market sessions aren't known (Yahoo's session ends include closing auctions and are wrong for
  a few exchanges, so they aren't used for the hours).

The three are in **Sources** (section Calendar) and can be turned off there.

## Overview

The **Overview** tab is a snapshot of the markets, refreshed every minute while it is shown (each answer is kept
55 seconds, so coming back to the tab doesn't download again). Click a stock (largest companies, movers, index
movers) to open it in **Research**. Code: `src/OverviewPage.*` (data and layout), `src/OverviewWidgets.*`
(painted panels, tables, chart and statistics).

- **Index performance**: SPY, QQQ, DIA and IWM as % change over today (5-minute bars from the previous session's
  close), 1 week (15 min), 1 month (hourly), 3 months, YTD, 1 year (daily), 3 or 5 years (weekly) from the range's
  first open; the area from zero to the highest series is green, to the lowest red; each series can be hidden.
  Overnight gaps are left out of the time axis (New York time). **Statistics**: per ETF, the range's return, high,
  low and the standard deviation of its bar returns, and the correlations of the bar returns pair by pair.
- **Sectors**: the eleven Select Sector SPDR funds (XLK, XLF…), today (Yahoo's change) or over 1 week, 1 / 3 months,
  YTD (the live price against the last daily close on or before the period's start), sorted, with bars scaled on the
  largest move.
- **US indices** (S&P 500, Nasdaq-100, Nasdaq Composite, Dow, Russell 2000), **Global** (Nikkei, Euro Stoxx 50,
  DAX, FTSE, CAC, Hang Seng, TSX, IBEX, the dollar index, VIX, VXN), **Crypto** (14 coins), **FX** (12 pairs
  against the dollar) and **Commodities** (energy, metals, grains, sugar; grains in US cents, as quoted): Yahoo
  quotes, one request for all of them, in each quote's currency.
- **US bonds**: the US Treasury's daily par yield curve (3 months to 30 years, `home.treasury.gov`, the year's
  CSV; last year's too in early January) and the change in points since the day before. Published after the
  close, so it's the last business day's.
- **Largest companies**: the 30 largest US listings by market cap in Nasdaq's screener (one line per company, so
  not both GOOGL and GOOG), with Yahoo's live price, change and market cap, ten per page.
- **Gainers / Losers / Active**: Yahoo's predefined screeners `day_gainers`, `day_losers`, `most_actives` (US
  stocks above about $2B of market cap), the first ten.
- **Top index gainers / losers**: today's best and worst twelve members of the S&P 500, the Nasdaq-100 or the Dow
  (members as in Research › Sectors; Yahoo quotes, 100 symbols per request).

## Economy

The **Economy** tab is a board of US (and a few global) economic indicators in ten sections, then **Countries** (below): **Sentiment**,
**Crypto**, **Growth**, **Inflation**, **Employment**, **Rates**, **Housing**, **Consumer**, **Liquidity** and
**Global**. Each panel is one indicator: an area chart with its own ranges (1M … 5Y for daily series, 1Y … 10Y for
monthly ones, up to MAX for quarterly ones), the last value and its change since the observation before (in
points, "pp", for rates and spreads; in percent for levels), the date of the last observation, the source and a
"?" explaining the indicator; hovering shows the date and value. A section is built and downloaded the first time
it is opened; **Refresh** downloads it again; "Updating…" / "Unavailable: n" in the sub-header say how it went
(the tooltip gives the last error). The section is remembered (`economy/section`; for a screenshot,
`defaults write com.deepwater.Deepwater economy.section -int N`, then `deepwater --page economy --delay 15000
--screenshot out.png`). Code: `src/EconomyData.*` (the catalogue, loading, the Fear & Greed estimate),
`src/EconomyPage.*` (charts, dials, page); downloads go through `research::Client` (cache and Sources rows).

- **FRED** (St. Louis Fed) for most series: VIX, Michigan sentiment, St. Louis financial stress, Chicago NFCI, ICE
  high-yield and investment-grade spreads (FRED keeps three years of them), policy uncertainty, GDP, real
  consumption, capacity utilization, durable goods, CFNAI, manufacturing jobs, CPI, core CPI, PPI, PCE prices,
  Michigan and Cleveland Fed inflation expectations, unemployment, payrolls, jobless claims, participation, JOLTS,
  the effective fed funds rate, 2- and 10-year yields, the 10Y-2Y and 10Y-3M curves, housing starts, permits, new
  home sales, Case-Shiller, Realtor.com listings, the 30-year mortgage rate, the Sahm rule, the debt service ratio,
  consumer credit, the savings rate, real retail sales, the Fed's balance sheet, the Treasury general account,
  reverse repo, retail money funds, M2 and its velocity, USD/JPY, Japan's 10-year yield and euro area HICP.
  Through FRED's API when a key is saved in Sources (Map · databases with a free key › FRED, the same key as the
  map's), else the public CSV of FRED's graphs (`fredgraph.csv`), which turns away requests that don't come from
  a browser (checked 2026-11-01: HTTP/2 stream reset to the app's own user agent); the panels then say a key is
  needed. Kept six hours; each request starts on January 1st so its cached copy holds all year.
- **OECD** key economic indicators: US consumer confidence (`USA.M.CCICP`), a day.
- **Yahoo Finance**: the dollar index (`DX-Y.NYB`).
- **Crypto**: alternative.me's Fear & Greed index; CoinMarketCap's altcoin season index (since March 2024), total
  market cap and bitcoin dominance (daily for a year, weekly since 2022), kept an hour. The 0…100 indexes show
  their classes as bands (extreme fear … extreme greed, bitcoin / altcoin season), the line coloured by class.
- **Stock market Fear & Greed** (tagged ESTIMATE): CNN's own figures answer HTTP 418 to anything but a browser,
  so the dial is Deepwater's estimate built the same way: momentum (S&P 500 against its 125-day average),
  volatility (VIX against its 50-day average, inverted), safe haven demand (SPY less TLT over 20 days), breadth
  (equal-weight RSP less SPY over 20 days, standing in for CNN's McClellan volume) and junk bond demand (high
  yield less investment grade spread, inverted, when FRED answers), each ranked against its last 252 days and
  averaged (three components at least). CNN's put/call and 52-week highs/lows components are left out. The
  components are listed next to the dial.

### Countries

The **Countries** sub-tab lists about 190 economies by group (G20, America, Europe, Africa, Asia, Oceania; a search
box filters them, Enter opens the first one left). Clicking one opens its page: **Overview**, **GDP**, **Labour**,
**Prices**, **Money**, **Trade**, **Government** (public finances, then taxes), **Business**, **Consumer**, **Housing**,
**Energy** and **Health**, each a table of the indicators a free source publishes for that country: the last value,
the one before, the highest and lowest of the series' whole history, its unit, its period and its source (the
tooltip names the series and what it measures). Negative values are red. A tab is downloaded the first time it is
opened; **Refresh** downloads the country again, **Export CSV** saves the shown tab. Rows no free source publishes for
the country are left out, counted under the table (their names in the tooltip); rows that need a free key (FRED, EIA)
say so. The last tab shown is remembered (`economy/countryTab`).

The rows and their sources are in `resources/country_indicators.json` (the format is described in
`src/CountryData.h`). Each indicator lists sources in order; the first that covers the country and answers wins,
unless its series stopped more than 400 days ago, in which case a later source that goes on is preferred (an office
that stopped reporting to a compilation). Sources:

- **FRED** (United States, with the key saved in Sources): about 170 series, national accounts, payrolls by
  sector, claims, JOLTS, prices (CPI and its parts, PCE, PPI, import and export prices, Cleveland Fed median and
  trimmed mean), money and the Fed's balance sheet, trade, the Treasury's monthly statement, industry, regional Fed
  surveys, consumer spending and retail sales by store type, housing. FRED allows 120 requests a minute: the client
  spaces its FRED requests by 550 ms and waits out a 429.
- **World Bank** WDI (every country, annual; one request per country for every indicator): GDP in dollars, at
  constant prices, per head and in PPP, GNP, investment, value added by sector, population, ILO modelled labour
  figures, lending and deposit rates, credit, external debt, terms of trade, tourism, military spending and arms
  exports (SIPRI), energy, health. And its **Global Economic Monitor** (monthly, `source=15`, `frequency=M`; dates
  "2026M07"): core prices, industrial production, merchandise exports and imports, reserves, retail sales volume,
  stock markets, terms of trade, for countries the other compilations miss (Kenya, Vietnam, Morocco…).
- **IMF**: the World Economic Outlook (public debt, budget balance, spending and revenue, current account in % of
  GDP, unemployment; reported years only, the current year's estimate and the projections are dropped) and the
  statistics department's data sets read one country at a time, one request per data set: CPI (headline, food and
  the COICOP groups), PPI, production indexes, labour statistics, quarterly national accounts, international
  liquidity (reserves, gold in troy ounces turned into tonnes), exchange rates, trade in goods, balance of payments,
  share prices, monetary aggregates (broad money, national M1), central bank balance sheets (total assets, currency
  in circulation), banks' claims on the private sector and interest rates (policy-related, lending, deposit,
  interbank, money market, government bonds). Money is in the country's currency: a unit's `{CUR}` is filled in.
- **OECD**: the key economic indicators (business and consumer confidence, leading indicator, industrial
  production, retail volume, unemployment, employment, hourly earnings, unit labour costs, interbank and 10-year
  rates, exports and imports, GDP growth; one request per country), detailed consumer prices (`DF_PRICES_ALL`:
  headline, core, food, energy) and the tax database (combined corporate rate, top personal income tax rate,
  withholding tax on dividends).
- **Eurostat** (EU, EFTA, candidates; the euro area is `EA21` since Bulgaria joined in 2026): GDP growth, HICP and
  its parts, unemployment, youth and long-term unemployment, job vacancy rate, labour cost index, minimum wages
  (half-yearly), production, producer and import prices, retail, confidence (consumers, industry, services),
  sentiment, capacity utilisation, construction, building permits (quarterly), house prices, home ownership,
  quarterly debt, deficit, revenue and expenditure, household saving, tourist nights, car registrations.
- **ECB** Data Portal (euro area and members; `{ECB}` is U2 for the euro area): M1, M2, M3 and the Eurosystem's
  balance sheet for the euro area (`only: ["EMU"]`), national central banks' balance sheets, loans to companies,
  mortgage, lending and deposit rates (MIR) by country.
- **BIS**: policy rates (the ECB's for euro members), residential property prices, household and private
  non-financial debt in % of GDP.
- **ILO** (ILOSTAT's bulk service `rplumber.ilo.org`, the one its R package reads): unemployment, employment and
  participation rates, unemployed, employed and labour force from the national labour force surveys (quarterly and
  annual, both sexes, aged 15+; 15-24 for youth), statutory monthly minimum wages in local currency.
- **ONS** (United Kingdom, first for it): CPI, CPIH components, core and services inflation, labour market (LFS
  unemployment, employment, vacancies, earnings), GDP, retail sales, public sector net debt, trade, current account,
  production, saving ratio, productivity, by series path. **Bank of England** (statistical database CSV, start no
  earlier than 1963 or it redirects to an error page served with a 200): M4, mortgage approvals, the 2-year fixed
  mortgage rate.
- **National offices and central banks read directly**, each first for its own country: Banco Central do Brasil
  (SGS JSON; daily series ten years at most per request; money before the real, July 1994, cut off with `since`),
  Statistics Canada (Web Data Service, one vector over its history) and the Bank of Canada (Valet), the Bank of
  Japan (its time-series API, `getDataCode` by data base and code; dates 202609, 202603 for the third quarter,
  20261029), the Swiss National Bank (data portal cubes as CSV), the Australian Bureau of Statistics (SDMX, like the
  OECD's); in Europe, the Bundesbank (SDMX `sdmx_csv`, much of the Federal Statistical Office's figures:
  registered unemployment, employment, vacancies, production, factory orders, retail, CPI a month before Eurostat,
  producer prices, trade), INSEE (BDM SDMX by idbank: business climate, household confidence, private payrolls,
  consumption of goods), Spain's INE (Tempus: tourist arrivals), Statistics Norway and Statistics Sweden (PxWeb API 2
  as JSON-stat 2, time dimension `Tid`, quarters "1978K1": CPI-ATE, Mainland GDP, CPIF excluding energy). Eurostat
  already carries the rest of the European offices' figures; their APIs (CBS, CSO, Statistics Denmark, Norges Bank,
  Riksbank, GUS, Statistik Austria, NBB) answer without a key, ISTAT timed out. With the user's free key (the same
  `keys/<id>` as the map's national sources; without it these rows fall back to the compilations): the Bank of
  Korea's ECOS (`STAT/CYCLE/ITEM` ids, checked with ECOS's `sample` key: base rate, CPI, GDP, current account,
  10-year yield, reserves, KB house prices, business and consumer surveys, unemployment), Banxico's SIE and INEGI's
  BIE (Mexico), the Turkish central bank's EVDS (key in a `key` header), Statistics Indonesia's web API (BPS: at most
  three years per request, so four requests from 2015; a cell's key is vervar + var + turvar + year id + period id;
  monthly inflation, unemployment, exports, imports, tourist visits), e-Stat (Japan: the 2025-base CPI table
  0004052037 filtered by `q`, all items, less fresh food, less fresh food and energy, services; times YYYY00MMMM). Banxico's APIIF (public token)
  serves the financial system's data (DISF), not the macro series. Tested and left for later: India's MOSPI API (one request per year), China's NBS (refuses a non-browser
  client), the RBI's DBIE (a web application without a documented API); Korea, Mexico, Turkey, Indonesia and
  Japan's e-Stat need a free key.
- **Yahoo Finance**: the main stock index and the currency (the dollar index for the United States, the currency
  against the dollar elsewhere), monthly history and the last month's daily closes.
- **EIA** (with a key): the weekly petroleum and gas stocks, production, refinery runs and net crude imports
  (imports less exports, as Trading Economics shows them), monthly crude production.
- **Private surveys** (PMIs): the latest and previous values from the economic calendar (`SurveyHeadlines`), no
  history.

Each of these groups of hosts has a row in Sources › Economy · countries (state, test, switch). Coverage measured
on 2026-11-02 (rows with data out of about 300, the Overview's repeats included): United States 276, Germany 138, France 129, United Kingdom 119,
Japan 113, Brazil 109, China 104, Euro area 103, India 100, Kenya 91, Vietnam 84, Nigeria 82; with the national
sources: Switzerland 123, Japan 120, Canada 118, Brazil 116, Australia 114, Germany 142, France 132,
Sweden 132, Spain 129, Norway 122 (many national series replace a
compilation's on a row already filled, fresher and at the source, rather than filling a new one).

Dev flag: `deepwater --economy-country FRA --economy-tab 3 --delay 30000 --screenshot out.png` opens a country on a
tab (0 Overview … 11 Health). Code: `src/CountryData.*` (catalogue, providers, parsing, transformations, the loader),
`src/CountriesView.*` (the list and the country page).

## News

The **News** tab is a wire of headlines: **General**, **Stocks**, **Crypto**, **Forex** or your **Favorites**, with a ticker filter
("NVDA", "BTCUSD", "EURUSD"), refreshed every five minutes while it is shown (each answer is kept five minutes) and
on **Refresh**. Two icons next to Refresh choose the **grid** (as many 420 px columns as fit, the default) or the
**column** view (one compact card per line: one-line title, two-line summary); the choice is kept (`news/view`). Each card is a headline, its summary, the date, how long ago, the article's site and the ticker as a
badge. A click opens the article in the **reader**: a floating panel over the page (the rest dimmed) with the web
page embedded through Qt WebEngine, the title, site, date and ticker, a back button for links followed inside,
**Open in the browser**, and × or Escape or a click outside to close; a middle click or Cmd / Ctrl + click on a card
opens the browser directly. The star in a card's corner, or in the reader's header, keeps the article in the
**Favorites** tab (as it was when starred: `news-favorites.json` in the app data folder, newest star first; the
ticker filter applies to their badge and text). The web view is created the first time it is needed (WebEngine starts a helper process)
and loads a blank page when the reader closes. Code: `src/NewsData.*` (providers, parsing, the symbol guess),
`src/NewsPage.*` (sub-header, strip, cards and grid), `src/NewsReader.*` (the reader), `src/AdBlock.*` (its ad blocker).

**Ad blocker.** The reader runs Brave's engine, [adblock-rust](https://github.com/brave/adblock-rust), through a
small C ABI in `third_party/adblock-ffi` (a Rust crate built by cargo during the CMake build; without a Rust
toolchain the app builds without it and the reader blocks nothing). It is fed with the lists Brave turns on by
default (`list_catalog.json` of brave/adblock-resources, less the Android and iOS ones): uBlock Origin's filters
(yearly, general, badware, resource abuse, unbreak, quick fixes, link shorteners, privacy, cookie notices),
EasyList, EasyPrivacy, URLhaus, Brave's own (unbreak, specific, social, SugarCoat, first-party), EasyList Cookie
and Fanboy's mobile notifications. The lists are downloaded once a day (HttpCache `adblock-list-<n>`, about 8.6
MB in all) when the News tab is first opened, and the engine is compiled on the thread pool (under 100 ms); until
then nothing is blocked. Every sub-resource of the embedded page goes through a `QWebEngineUrlRequestInterceptor`
on the reader's own off-the-record profile (the article's main document is never blocked); the page's cosmetic
rules (element hiding) are injected when the document is ready, and the generic ones once the page has loaded,
from the classes and ids it carries (and again 2.5 s later for what scripts add). The shield in the reader's
header counts the requests blocked on the page. Sources › News › **Ad blocking filter lists** shows the lists'
state, refreshes them, or turns the blocker off. Scriptlet injection (Brave's `resources.json`) is not done.

**Which web engine.** On macOS the reader renders with the system's WebKit (`WKWebView`, the engine of Safari,
`src/NewsWebHostMac.mm`); elsewhere with Qt WebEngine (`src/NewsWebHostQt.cpp`); `src/NewsWebHost.*` is the
small interface the reader talks to, and `DEEPWATER_WEB_ENGINE=qt` forces Qt WebEngine on macOS for comparisons.
The reason: some bot protections accept only the network fingerprint of a real browser engine. www.nasdaq.com's
Akamai answers HTTP 403 "Access Denied" to Qt WebEngine whatever it declares (checked 2026-11-01 with the ad
blocker off, Chromium's user agent without the `QtWebEngine/x` token, the app's locale as `Accept-Language` and
client hints naming "Deepwater" next to "Chromium", as Brave names "Brave"), while it serves Safari, and so
WebKit, HTTP 200. Nothing is done to impersonate another browser: the WebKit view names itself `Deepwater/0.1`
in its user agent. With WebKit the ad blocker is WebKit's own content blocking: adblock-rust converts Brave's
lists to WebKit's rule format (about 180,000 rules, 150 ms, cached in HttpCache `adblock-webkit` under the
lists' hash), split into lists of 60,000 blocking rules each followed by every exception rule (WebKit's
`ignore-previous-rules` only reaches back inside its own list), compiled by `WKContentRuleListStore` (a few
seconds the first time, then found in its cache, about 0.4 s) and attached to the page; a page loaded while
they compile is reloaded once they are ready. WebKit applies them without reporting, so the shield reads
"filters active" instead of a count. The `--screenshot` flag composites the WebKit view's own snapshot, since a
native view doesn't paint through `QWidget::grab`. When a site still answers 401, 403, 429 or 451, the reader
shows a notice under its header and **Open in the browser** is the way.

- **Providers**: with a Financial Modeling Prep key saved in Sources › News, its `stable/news/*-latest` endpoints
  (and `news/stock|crypto|forex?symbols=` for the filter) give every article a ticker; without one, free RSS feeds,
  merged, deduplicated by URL and sorted newest first (60 at most): CNBC top news and finance, MarketWatch and BBC
  Business (general); Nasdaq Stocks and Markets, MarketWatch market pulse, Seeking Alpha market currents and
  Investing.com (stocks); Cointelegraph, Decrypt, CoinDesk and Investing.com (crypto); FXStreet and Investing.com
  (forex). A failed FMP request falls back to the feeds and says why in the strip.
- **Ticker badge** with the feeds: the feed's own ticker list (Nasdaq's `nasdaq:tickers`, Seeking Alpha's symbol
  category), else guessed from the text: "(NASDAQ: NFLX)" or "$NVDA" for stocks, the coin named first (Bitcoin →
  BTCUSD) for crypto, a pair ("EUR/USD") or the currency named first (euro → EURUSD, yen → USDJPY, gold → XAUUSD,
  "dollar" alone → DXY) for forex.
- **Ticker filter** with the feeds: stocks read Yahoo Finance's and Nasdaq's feeds about that symbol (every card
  wears it); the other categories keep the articles whose badge or text names it.
- Feed bodies are read as HTML (often escaped twice: "S&amp;amp;P") and the Motley Fool's "Key Points" heading is
  dropped. Dates: RFC 2822 (with "GMT" and the US zone names), ISO 8601, Investing.com's "yyyy-MM-dd HH:mm:ss" (GMT).
- Dev flags: `deepwater --page news --news-ticker NVDA --delay 9000 --screenshot out.png`; `--news-article 1` opens the
  reader on the second card once the list is loaded; `--news-favorite 1` toggles its star; `--news-url https://…` opens
  the reader on any page (to see how a site renders, or its request headers on httpbin.org/headers).

## Research

The **Research** tab studies one company at a time. A symbol picker sits under the main header (type a ticker, or a
company name for Yahoo's suggestions; any Yahoo symbol works: `MC.PA`, `7203.T`…), with the name, price and day's
change next to it; a sub-header lists the sections. The symbol and the section shown are remembered. Everything is
downloaded on demand, kept on disk (`cache_research_*.bin`, from 15 minutes for quotes to 30 days for head counts)
and shown in **Sources** (section Research), where each group of hosts can be turned off. A symbol's summary, the
Nasdaq listings and the index members are parsed once per cached copy and shared by the sections asking for them;
the sections fill once per pass of the event loop however many answers arrive in it, and Key metrics refills its
cards in place rather than building them again. The page is translated; figures and names that come from the
data (Yahoo's sector, industry and description, holders, analysts) stay as published, except the country, read
through the `Country` context of `i18n/deepwater_data_fr.ts`, and the GICS sector names (Sectors).

- **Key metrics**: profile (sector, industry, country, CEO, description, website, first trade, employees), general
  info and the indexes it belongs to, head count over the years (Wikidata, plus Yahoo's current figure), market cap
  against the trailing year's revenue and profit, and fifteen cards of ratios: valuation, earnings, per share, balance
  sheet, margins, profitability, leverage, dividends, ownership, growth (3- and 5-year CAGR from the reported yearly
  figures), efficiency and liquidity. Every balance figure (and the enterprise value: market cap + debt − cash and
  short-term investments) comes from the latest quarterly balance sheet; ratios of yearly flows to balances (ROIC,
  ROCE, turnovers, interest coverage) take all their figures from one fiscal year, the newest that has them all (the
  tooltip names it). Statements in another currency than the quote (ADRs) are converted at Yahoo's rate for the pair
  before any ratio to the market cap, and per-share figures use the quoted shares (market cap / price). The head-count
  line is Wikidata's only; Yahoo's current figure is in the title.
- **Financials**: income statement, balance sheet and cash flow as Yahoo lays them out, annual (four years) or
  quarterly (five quarters), as reported.
- **Financial charts**: revenue, gross profit, operating and net income; margins; diluted EPS; cash flow; balance
  sheet, yearly or quarterly over 5, 10, 15 years or everything, with the change on each bar. US filers: SEC EDGAR
  XBRL company facts (`data.sec.gov`, every 10-K / 10-Q since about 2009; the fourth quarter is the year less the
  first nine months, approximate for EPS; ticker to CIK through `efts.sec.gov`); others: Yahoo's four years and five
  quarters. Filings restate per-share figures only for the splits before them, so EPS and dividends per share are
  divided by the splits after each figure was filed (split history from Yahoo's chart events): NVDA's FY2019 EPS of
  $6.63 shows as $0.17.
- **Ownership**: institutions / insiders / public, the 25 largest institutional holders (Nasdaq, from 13F filings)
  with their change and report dates, insiders' open-market sales and purchases per quarter (gifts, withholdings and
  option exercises left out; a quarter cut off by Nasdaq's 200-trade limit isn't shown) and their last 50 trades
  (Nasdaq, from Form 4).
- **Ratings**: this month's analyst counts and their gauge, Yahoo's mean recommendation, high / mean / low price
  targets over a year of monthly closes, the rating changes of every firm, and recommendations history rebuilt from
  those changes (each month counts each firm's last rating of the twelve months before it; Yahoo keeps only four
  months of its own counts). The intrinsic value, factor grades and quant rating are Deepwater's own, explained in
  their tooltips and tagged **EXPERIMENTAL** (also the matching Compare columns and the rebuilt recommendations
  history): they are kept to be improved, but haven't been validated against later prices and move a lot with their
  fixed assumptions (one point of discount rate moves Apple's value by ~17 %). They are: a two-stage discounted cash
  flow (the mean of trailing free cash flow and net income, growing at the mean of revenue growth and next year's
  expected EPS growth for five years, fading to 2.5 % by year ten, discounted at 4.25 % + beta × 5 %, plus cash less
  debt; "N/M" when free cash flow is negative, as for Amazon and Alphabet at the end of October 2026, their capex above
  their operating cash flow), letter grades on fixed thresholds, and a 1–5 rating weighing
  valuation, profitability, financial health and momentum. Statements in another currency than the quote (ADRs) are
  converted at Yahoo's rate for the pair.
- **Compare**: peers (Yahoo's related symbols, then the largest companies of the same industry), the company's sector
  or the whole market (filters by sector and industry), on six sets of columns. Click a row to open that company.
- **Compare charts**: one reported figure of two companies, quarterly or yearly, as values or growth against the
  same period a year before.
- **Sectors**: the S&P 500, the Nasdaq-100 and the Dow by GICS sector, today, over 1 week, 1 / 3 months or since the
  start of the year: a ring weighted by market cap (by price for the Dow; share classes like GOOGL / GOOG split the
  company's cap) coloured by change, and the sectors with their members. Members: Wikipedia (S&P 500 list and Dow
  template, with the GICS sector), Nasdaq (Nasdaq-100; members outside the S&P 500 get the nearest GICS sector from
  Nasdaq's classification). Prices: Yahoo daily closes (five days, refreshed every 5 minutes, for the last price and
  today's change; a year, every 3 hours, for the periods; ~520 stocks, 20 per request). Nasdaq's screener only gives
  the market caps: its prices can be a session behind. The official index's change is shown next to the recomputed
  one as a check (31 October 2026: ^GSPC −1.82 %, recomputed −1.78 %; the index weighs by free float).
- **ICS**: every US listing in Nasdaq's sector › industry classification (~7,000 stocks, the screener in one ~2 MB
  request; notes, preferreds, warrants and units left out), with totals and each industry's members (prices and
  last session's change from Yahoo daily closes, for the 300 largest). The search box matches industries and
  companies (a symbol it starts, a name it contains): matching companies are listed under their industry (the 30
  largest), a click shows that industry with the company's row selected, a double click opens the company; a
  single match is shown without a click. `--search <text>` fills the box when the Research page is shown.
- **SPLC**: the supply chain graph: suppliers, the company, customers, from Wikidata (SPARQL, no key) through
  products. Suppliers make the company's products (P176, manufacturer); customers sell products it makes (P1716 brand,
  P178 developer). Left out: a manufacturer that is also the product's brand or developer, products of another brand
  (Apple's chips made by TSMC aren't TSMC's own), products with three or more makers (shared process nodes), the
  company's subsidiaries, and a company found on both sides stays on the side with more products. A brand that isn't a
  company counts for its listed owner. Each link shows how many products make it, not a share of revenue: Wikidata is
  edited by volunteers, coverage is uneven (Apple: Foxconn 40 products, TSMC 24, Samsung, Quanta…; Tesla: almost
  nothing) and no revenue figure is recorded. Listings are turned into Yahoo symbols (US first, then home exchanges:
  `2317.TW`, `005930.KS`…) for prices and to open them. Revenue shares from 10-K customer disclosures would need the
  XBRL US API (a free account per user) — not done yet.

## Quant

The **Quant** tab has a sub-header with five sections (the one shown is remembered). Prices and company figures
come from Yahoo Finance through the Research client (`research::Client`, cached on disk, listed in **Sources**
under Research). The page is translated; numbers keep the English grouping (`1,234.5`) in every language. Code: `src/QuantPage.*`, `src/QuantViews.h` and the
`src/Quant*View*.cpp` files, charts in `src/QuantCharts.*`, statistics and the COT data set in `src/QuantData.*`.

- **Compare**: up to eight tickers (chips, × removes; the list is remembered) on one chart of their change since the
  start of the range (1 year, 6, 3 or 1 month; the **Change %** button off shows prices rebased to 100), the last
  value of each written at its end, and a table of their figures by section: key stats, valuation ratios,
  profitability and margins, earnings and financials, growth, balance sheet, dividends, ownership. In each row the
  best figure is green and the worst red (lower is better for valuation ratios, beta, debt and short interest; a
  negative valuation ratio counts as the worst). P/FCF is market cap / free cash flow and ROI/ROIC is net income /
  (debt + book equity, from the market cap and P/B); the other figures are Yahoo's as published (ASML's enterprise
  value and P/B are wrong at Yahoo's, for instance).
- **Correlation**: a pair for pairs trading, Y (bought) against X (sold), over the range, with daily closes. The
  hedge ratio β is the least-squares slope of Y on X prices; the spread is Y − βX. Cointegration: an augmented
  Dickey-Fuller test of the spread (constant, one lagged difference) with MacKinnon's (1994) approximate p-value,
  STATIONARY below 0.05. Mean reversion: the half-life −ln 2 / λ from Δspread regressed on the lagged spread
  (∞ when λ ≥ 0). Signal: the z-score of the spread against its moving average over the window (20 days by default,
  **Set** applies a new one): beyond ±2σ sell / buy the spread, beyond ±0.5σ WATCH, else NEUTRAL. Charts: both
  prices on their own axes (**Min/Max** marks their highs and lows), the spread with its SMA and ±2σ band, the
  z-score with the sell and buy zones (the **Z-Score** box hides it) and the scatter of daily returns with the
  regression line and its statistics (β, α, Pearson r, R², standard errors).
- **Risk & Beta**: the regression of a stock's returns on an index's (SPY by default), daily or weekly closes
  (weekly: the last close of each week) over 1, 2, 3 or 5 years: the scatter with the regression line and the
  histograms of both (with a Gaussian kernel density, Silverman's bandwidth), and the statistics: raw beta,
  adjusted beta (Blume: 0.67 β + 0.33), alpha, R², r, standard deviations, standard errors, the t statistic of beta
  and its two-sided p-value (Student's t), the number of points. Below, the cumulative performance of both since
  the start of the range, green between the curves where the stock leads and red where it lags, or their spread.
- **COT**: the CFTC's weekly *Commitments of Traders* report, legacy format, futures only
  (`publicreporting.cftc.gov`, dataset `6dca-aqww`, no key; positions as of Tuesday, published Friday), for the
  S&P 500, NASDAQ-100 and DJIA consolidated contracts, the Russell 2000 E-mini, bitcoin, gold and silver: positions,
  week-on-week change, share of open interest and number of traders of non-commercial (long, short, spreads),
  commercial, total reportable and non-reportable traders; long against short per group; net positions over two
  years; and the weekly history (open interest, its change, net positions). One request for the seven contracts
  over two years (~800 rows), refreshed every 12 hours, cached in `cache_cftc_legacy.bin`; in **Sources**
  (section Quant). The RELEASE date is the Tuesday plus three days (a day later on holiday weeks).
- **Signals**: a scan of 48 large US stocks (the Dow and the biggest of the S&P 500, plus the Compare tickers) on
  five timeframes (5m and 15m over five days, 1h over a month, 4h built from the hourly candles of each day, 1d
  over six months; Yahoo chart candles, intraday ones cached five minutes): RSI(14) beyond 30 / 70, streaks of
  five or more candles closing higher or lower than the one before, and gaps between a candle's open and the
  previous close (2 % daily, 1 % 4h, 0.75 % 1h, 0.5 % 15m, 0.3 % 5m). The most recent occurrence of each per
  symbol and timeframe, newest first, filtered by symbol, indicator and timeframe; the scan (about 200 requests,
  under a minute) runs when the section is shown and again every five minutes while it is.

- **Markov**: three Markov models behind one set of segments; the engines are a pure library (`src/markov/`, Qt Core
  only, namespace `markov`, tested by `tests/quant/markov`), the estimations run on a worker thread with a progress
  bar and a Cancel button (`src/QuantJob.h`).
  - **HMM regimes**: a Gaussian hidden Markov model of the daily returns (in %), alone or with the realized
    volatility (standard deviation of the last 10 returns) as a second variable, with 2, 3 or 4 states, over 1 to 10
    years or everything. Baum-Welch (EM) in log space, started from k-means++ then random draws (8 starts for the
    shown K, 3 for the others), the best log-likelihood kept; each state's variance is floored at 1 % of the sample's
    so EM can't isolate a one-day spike as a state; states sorted by rising volatility, so state 0 is always the calm
    one. Missing days (NaN) carry no information and are skipped in the updates. The **Window** spin box estimates on
    the last N days only and applies the model to the whole range (warm restarts are in the engine, `refit()`).
    Shown: the current regime with its filtered probability and a one-line reading (days in the regime, expected
    visit, most likely switch), log-likelihood / AIC / BIC / parameters / iterations, a table of K = 2, 3, 4 (lowest
    BIC in bold), the price coloured by regime, the stacked regime probabilities, the transition matrix and a table
    of the regimes (daily mean and volatility, annualized volatility, expected duration 1 / (1 − p_kk), share of
    time). By default everything is **filtered** (P(state | data up to the day), usable as a signal); the
    **A posteriori** box switches to the smoothed probabilities and the Viterbi path, which use the whole sample.
    Estimation times (tests, K = 3, two variables, 6 starts): 10 years of daily data 0.13 s, 4,700 five-minute
    candles 0.26 s, 25,000 observations 1.3 s.
  - **MS-GARCH**: a Markov-switching GARCH(1,1) of the daily returns (in %) with 1, 2 or 3 regimes (1: a plain
    GARCH(1,1)) and normal or Student-t innovations, in the parametrisation of Haas, Mittnik & Paolella (2004): one
    variance process per regime, all fed by the same squared innovation, so Hamilton's filter gives the exact
    likelihood. Maximum likelihood over an unconstrained reparametrisation (ω = floor + exp, (α, β) by a softmax
    capped at α + β ≤ 0.99, transition rows by a softmax with P(stay) ≥ 0.8, ν = 2.05 + exp), Nelder-Mead then BFGS
    with numerical gradients, one deterministic start and two perturbed ones. Nelder-Mead stops early (tolerance
    1e-7, at most 200 iterations per parameter): it only brings BFGS near the optimum; BFGS takes one-sided
    gradients while its steps are large and central ones near the end. The starts (and the HMM's initializations)
    are drawn in order and run side by side, one thread each, which gives the same fit as one after the other. A
    new horizon alone recomputes the forecast and the risk from the fit in place. The bounds matter: without them the
    likelihood of index returns picks a one-day "jump" regime or an EWMA regime (ω = 0, α + β = 1); they are engine
    options (`MsGarchOptions`). Missing days carry the expected squared innovation through the recursion. Shown:
    the next day's volatility (daily and annualized) and the regime it comes from, the volatility over the horizon
    (1 to 250 days, forecast by iterating the regime probabilities and the per-regime recursions with E[ε²] = the
    mixture variance), VaR and expected shortfall at 95 % and 99 % for one day (the exact quantile and tail
    expectation of the regime mixture) and over the horizon (the summed variance as one normal or Student
    component), a position-size calculator (capital × target daily risk ÷ forecast daily volatility), the model
    against GARCH(1,1) on the same sample (log-likelihood, AIC, BIC), the regimes (ω, α, β, persistence, long-run
    volatility, P(stay), expected duration), the conditional volatility against the realized 20-day one, the
    filtered regime probabilities and the forecast cone (the mixture between the lowest and highest regime).
    Estimation times (tests, 2 regimes, 3 starts): 10 years daily 0.8 s normal / 0.9 s Student, 4,700 candles
    1.1 s / 1.9 s.
  - **Execution**: how to sell a quantity over a horizon (Almgren & Chriss 2000: N intervals, price diffusion σ√τ,
    permanent impact γ per share, execution at the opening price less half the spread ε and a temporary impact of
    the participation n / V, linear or **square-root** (η (n/V)^½, with Almgren-Chriss linearized at the TWAP pace);
    objective E + λ V). Inputs: symbol (decision price, average daily volume and daily volatility from three months
    of Yahoo daily candles), quantity, horizon, intervals, λ (×1e-6, in 1 / currency), impacts in basis points at a
    participation of 1 % of ADV (temporary: per day of trading at that pace; permanent: per 1 % of ADV traded), half
    spread, Monte Carlo paths, and **Regimes**: none, the HMM's or the MS-GARCH's two regimes (each regime's
    volatility, the transition matrix brought to the interval, P^τ for whole days or I + τ(P − I) within a day, and
    the current regime), read from the shared state when its record is current, else estimated on two years of
    daily returns and published. Volumes: the expected profile over the horizon is the average 30-minute shape of
    the last month repeated each day (VWAP works over several days; flat when there is no intraday data), and the
    simulated paths draw each interval's volume around it (lognormal, σ = 0.35), so every strategy pays the impact of
    its participation in random volume. Strategies: TWAP, VWAP, the analytic Almgren-Chriss schedule (κ, half-life,
    E, √V), the MDP (dynamic programming on intervals left × 300 quantity levels × regime with the same costs and the
    per-interval expected volumes; without regimes and with a flat profile it lands on Almgren-Chriss, which the
    tests check at 1 %), and, when the box is ticked, a **Q-learning** agent: tabular, state (intervals left, 20
    quantity levels, regime), 21 actions (fractions of the remainder), reward minus the realized cost and risk
    penalty in basis points, ε-greedy with decaying learning rate, trained on the simulator for the chosen number of
    episodes (100,000 by default, about 0.3 s per 150,000 × 20 intervals; the tests check it lands within 15 % of
    Almgren-Chriss's objective). All are run on the same random paths: mean, standard deviation and 95th percentile
    of the implementation shortfall in basis points of the order's value, the trajectories (MDP and agent per
    regime) and the histogram of costs. `PathSimulator` (reset / step / done, `lastRiskPenalty()`) is the
    environment the agent trains on and `Policy` the interface every strategy implements.
  - **Shared state** (`src/QuantMarketState.*`): after each fit the HMM view publishes the symbol's current
    filtered regime and the MS-GARCH view its regime for the next day and its volatility forecast (next day,
    annualized, over the horizon, VaR and ES at 95 / 99 %), through `quant::MarketState` (a singleton like
    `research::Client`, signals `regimeChanged` / `volatilityChanged`). A regime record carries every regime's
    volatility, the daily transition matrix and the current probabilities, so the execution MDP reuses the chain
    instead of refitting; records are kept per symbol and source ("HMM", "MS-GARCH") in `QSettings` with their
    dates and the spec they were estimated with (`HmmSpec` / `GarchSpec`, `src/QuantMarkovJobs.*`, the same
    functions the views run). **Background refresh** (`StateRefresher`): a minute after launch and every 30 minutes,
    every record older than the last trading day is re-estimated with its own spec, one symbol at a time on a
    worker thread, and republished (unless the prices still end on the day and count the days the record was
    estimated on: a market holiday, a feed a day behind); the records are written once per burst of updates; the Portfolio tab queues the symbols it lacks. The refresher is a row of
    **Sources** (section Quant): last run, running or queued, a Refresh that re-estimates every symbol now, and a
    switch that stops the automatic re-estimation (a view asking for a symbol still gets it). `MarketStateStrip` shows a
    symbol's regime and forecast (or that it is being estimated, or where to get it): under the controls of
    Correlation (Y), Risk & Beta (the stock) and Execution, and compactly in Research's symbol bar.
- **Portfolio**: positions (symbol, quantity; negative for a short; kept in the settings) with their Yahoo quote,
  value and weight, each symbol's regime and volatility forecast from the shared state (the realized one-year
  volatility when there is no forecast yet, while the refresher estimates it in the background; **Estimate missing**
  queues them again), the standalone one-day VaR 95 % of each position, the share of the value in each regime, and
  the one-day portfolio risk four ways (VaR and expected shortfall at 95 / 99 %, in currency and % of value):
  parametric (σ_p = √(wᵀ Σ w), Σ from the correlations of a year of daily returns and the forecast volatilities,
  normal quantiles, plus the sum of standalone VaRs and the diversification ratio); historical (the portfolio's daily
  P&L over the year with today's positions, on the dates every position has); filtered historical (the same returns
  rescaled by forecast / realized volatility, Hull & White 1998); and a regime Monte Carlo (20,000 days: each symbol
  draws its regime with the shared state's current probabilities and takes that regime's volatility, scaled so the
  mixture matches the forecast; the shocks are correlated through the Cholesky factor of the historical
  correlations), with the histogram of daily P&L, history against simulation.

## AI

The **AI** tab has two sections (the one shown is remembered, `ai/section`). Keys live in **Sources › AI**
(`keys/anthropic`, `keys/huggingface`), with a **Test** that asks the API with the key.

- **Models**: the models on this computer (`~/Library/Application Support/Deepwater/Deepwater/models/<org>/<name>/`,
  scanned on show: files, size, date; open the folder, delete) and the Hugging Face Hub to get more
  (`src/AiModels.*`): a search (`/api/models?search=…&sort=downloads`, 30 results: task, downloads, likes, date;
  gated repositories marked), the files of the chosen repository with their sizes (`/api/models/<id>?blobs=true`;
  weights and small files ticked, files already here unticked), downloads of the ticked files
  (`/<id>/resolve/main/<file>`, redirects followed to the CDN, written to `<file>.part` and renamed when complete, a
  `Range` header resumes an interrupted one; progress, speed and Cancel per file). Two files download at a time
  (`HuggingFaceHub::MaxParallel`), the rest wait: the line is shared, so more in parallel only delays each file.
  LFS files carry their SHA-256 in the Hub's answer; it is checked after the download on the thread pool
  (`QtConcurrent`), and a mismatch is reported as an error. The reply's read buffer is 8 MB so the UI thread wakes
  less often. Measured on 1 November 2026 against the Hub's CDN (`xet-bridge-us`) with a 469 MB GGUF: one connection
  21 MB/s with curl, the app the same (491 MB in under 30 s, hash matching); 4 and 8 parallel byte ranges 20 and
  18 MB/s, so segmented downloads weren't added, the line being the limit. An interrupted download (127 MB of 415)
  resumed and matched the Hub's hash. The Hub token, when set, goes in an `Authorization: Bearer` header and opens
  gated models whose terms were accepted. Nothing runs the models yet: this is the store.
- **Claude** (`src/ClaudeClient.*`): a conversation with Claude through the Messages API
  (`api.anthropic.com/v1/messages`, API version `2026-10-21`, raw HTTP since there is no C++ SDK), streamed as
  server-sent events (`text_delta` shown as it comes, `thinking_delta` summaries kept for the status line's tooltip).
  Model (the API's list when a key is set, else Opus 5.5, Sonnet 5.5, Haiku 5.5, Fable 5.1; `claude-opus-5-5` by
  default), effort (`output_config.effort`, `medium` by default), an optional system prompt (remembered), 16,000
  `max_tokens`. Thinking is adaptive with a summarized display; on Opus 5.5, Opus 5, Sonnet 5.5 and Fable 5.1 the
  request carries `fallbacks: "default"` with the `server-side-fallback-2026-07-01` beta header, so a safety refusal
  is re-run by the server on a fallback model inside the same call (the status line names the model that answered,
  the tokens in and out, the time, a refusal or a cut at the token limit). The conversation is kept in memory for
  the session (**New conversation** clears it); a failed turn drops the question so it can be sent again.

- **Machine learning** (`src/AiMlView.*`, `src/MlEnvironment.*`, `src/MlData.*`, scripts in `resources/ml/`): training
  classical models on Deepwater's data from the app.
  - **Python environment**: a private virtual environment the app creates at `<app data>/ml/venv` from a system
    interpreter (python.org's framework, Homebrew, `/usr/local`, Apple's; newest first, remembered as `ml/python`)
    with `python -m venv`, then `pip install numpy pandas pyarrow scikit-learn lightgbm xgboost torch joblib`; the
    pip output streams to the log pane. `check_env.py` reports the versions (and whether PyTorch sees the Apple GPU).
    Checked on 1 November 2026: Python 3.15.0, scikit-learn 1.9.1, LightGBM 4.7.0, XGBoost 3.4.1, torch 2.15.0 with
    MPS. Apple's Python 3.9 is listed last since current wheels want 3.10 or newer. The bundled scripts are copied to
    `ml/scripts/` at every launch so they follow the app's version.
  - **Data**: `Export Deepwater's data` writes CSV files to `ml/data/`: `country_series.csv` (long form: source, iso3,
    series, time, value, for every `CountrySeriesSet`: OECD, Eurostat, IMF CPI, BLS, national offices, keyed sources,
    surveys, BIS property and credit, OECD housing, IMF debt, World Bank credit), `weo.csv`, `policy_rates.csv`,
    `fx_daily.csv`, `cot_legacy.csv`, and `prices_<symbol>.csv` (ten years of daily candles from Yahoo through the
    Research client) for the symbols in `ml/symbols` (SPY, QQQ, GLD, USO, TLT, ^VIX, EURUSD=X, BTC-USD by default).
  - **Training** (`train.py`): framework (scikit-learn: random forest, gradient boosting, ridge / logistic; LightGBM;
    XGBoost; PyTorch: a multi-layer perceptron on standardised inputs, Adam, early stopping, MPS when available),
    task, data set, target column, derived target for time series (next period's return / up-or-down / value /
    change of the column, which also adds the column's return as a feature), lags of every feature, columns to leave
    out, test share (the last rows when the file has a time column, random otherwise), hyper-parameters as JSON.
    The script prints `PROGRESS i/n`, `LOG …` and a final `RESULT {…}` line that `MlEnvironment` relays. Each run
    gets a folder in `ml/runs/<stamp>-<framework>/` with the model (`model.joblib`, plus `model.txt` for LightGBM and
    `model.json` for XGBoost; `model.pt` for PyTorch with the scaler and layout) and `run.json` (settings, rows,
    split, metrics, feature importances: the models' own, or permutation importance for PyTorch, versions, time).
    Metrics: RMSE, MAE, R² and the direction hit rate for regressions; accuracy, F1 and AUC for classifications. The
    runs table lists them all; double-click opens the folder. A first run on `prices_spy.csv` (LightGBM, next-day
    return, 5 lags) took 1.1 s on 2,507 rows and, as expected, explained nothing (R² ≈ 0, direction 54 %).

Dev flags: `deepwater --ai-search "qwen2.5-0.5b-instruct-gguf" --delay 9000 --screenshot out.png` runs a Hub search and
opens its first result; `--ai-download <repo> <file>` downloads one file; `--ml-export` exports the data;
`--ml-train prices_spy.csv close` trains with the form's defaults.

## Sources

The **Sources** tab lists every place the app gets data from, grouped by the page it feeds (map and shipping,
forex, macroeconomics, official statistics, national offices, databases with a free key, private surveys, the
Overview's Treasury yields, the News tab's feeds, its ad blocker's filter lists and its optional Financial Modeling Prep key, the AI tab's keys, the Charts providers, your own Charts keys, the data bundled with the app), each with its
live state (up to date / downloading / error, the last update and how often it is checked; for the Charts providers
the last answer the backend got), a **Test** button (a small request, timed; data sets are cut off after the
response headers) and, for downloaded data sets, **Refresh**. The switch turns a source off: it makes no more
requests — the map and forex data sets keep the last copy saved on disk, and the Charts page moves on to the next
provider in its fallback chain (the chart says "Binance is turned off in Sources, so this chart uses Coinbase").
Kept in `QSettings` (`sources/disabled`) and applied at startup. API keys are entered there too: aisstream (it
replaces the former Settings › Data sources page), the Charts page's own Alpaca and Polygon keys, written to
`quant-terminal/data/feeds.json` (mode 0600), and the free keys of FRED, EIA, BEA, Banxico, the Bank of Korea, the
Turkish central bank and UN Comtrade, kept in the settings (`keys/<id>`); a keyed source without a key says so and
makes no request. The survey publishers' rows only carry a switch: off, their figures leave the country sheets. The states refresh once a second, and only while the tab is shown.

## Developer tools

The `</>` icon left of the gear shows a small **DEV** flap on the right edge of the page area (both
persisted: `devtools/enabled`, `devtools/open`). Clicking the flap slides a full-width tabbed panel over the
pages. One tab for now:

- **Codebase** — the project's code graph as built by Graphify (`graphify-out/graph.json`, located by
  walking up from the app bundle or the working directory; `DEEPWATER_GRAPH_ROOT` or the
  `devtools/graphRoot` setting override that) as graph.html in 3D. The dots, sizes, Tableau colours
  and dashed, arrowed cross-community links are read from graph.html itself (`RAW_NODES`,
  `RAW_EDGES`; rebuilt from the JSON with the same rules when the HTML is missing), and laid out by
  vis.js's ForceAtlas2 rules (gravitational constant −60, central gravity 0.005, spring length 120,
  overlap avoidance 0.8, damping 0.4) run in three dimensions and flattened into a disk, framed on the
  90th-percentile radius since isolated communities drift far out. As in graph.html, names show only
  on hover (the dot turns white, its links and neighbours are named) or selection; the sidebar on the
  right is graph.html's: a node search with its result list, the *Node info* panel (type, community,
  source or members, degree, clickable neighbours), the *Communities* legend with a checkbox per
  community to hide it (*Select all*), and the node, edge and community counts. Click a dot to inspect
  it (the rest dims), double-click to open the community: its nodes in its colour, sized by degree,
  with the internal edges, the legend then listing its files. Drag to orbit, wheel to zoom, `Esc`
  clears the selection. The scene is cached and re-rendered only when the
  view changes (the render time is in the status line); the files are parsed off the UI thread and
  reloaded on their own when `graphify update` rewrites them, so the view follows the Stop hook.

`deepwater --devtools` opens the panel at launch (screenshots).

## Controls

Drag to spin · scroll to zoom · click a dot, cluster or country · `Space` toggles rotation · `Esc` clears the
selection, a second `Esc` turns the indicator heatmap off.

## Layout

| Path | What |
|---|---|
| `src/HeaderBar.*` | Top bar: brand, navigation tabs (a tab added since the saved order takes its default place), UTC clock, developer tools and settings icons |
| `src/MainWindow.*` | Page stack; `addPage(id, label, widget)` registers a new nav tab + page, `addPage(id, label, factory)` one built the first time it is shown (Overview, Forex, Calendar, News, Research, Quant, AI and Sources are; the map and Charts are built at launch, Charts because it plays the alerts) |
| `src/SettingsDialog.*` | Floating settings window (gear icon): language, theme |
| `src/DevTools.*` | Developer tools: the flap on the right edge and the full-width tabbed panel it opens (`DevToolsDock`) |
| `src/CodeGraphView.*` | The Codebase tab: `codegraph::load` (graph.json and graph.html's nodes and edges), `GraphCanvas` (graph.html in 3D: ForceAtlas2 layout, vis-style dots, labels and dashed links, cached scene, orbit, hover, filter) and `CodeGraphView` (communities ↔ one community's nodes, status, file watcher) |
| `src/SourcesPage.*` | The Sources tab: every data source, its state, test / refresh, on/off switches and API keys |
| `src/CalendarData.*` | Calendar data: economic releases (TradingView), US corporate events (Nasdaq), exchange sessions (Yahoo); the economic and corporate calendars are parsed (and the corporate one's answers joined) on the thread pool when they arrive |
| `src/CalendarPage.*` | The Calendar tab: sub-header, Economic, Corporate and Market hours views |
| `src/ResearchData.*` | Research data: the cached on-demand client (Yahoo session and crumb, Nasdaq, SEC, Wikipedia, Wikidata), SEC XBRL reduction, statement layouts, valuation and grades, formatting |
| `src/ResearchCharts.*` | Research charts (QPainter): grouped bars, lines, ring, gauge, stacked bars, supply chain graph |
| `src/ResearchPage.*`, `src/ResearchViews.h`, `src/Research{Company,Market}Views.cpp` | The Research tab: symbol picker, sub-header and its ten sections |
| `src/OverviewPage.*` | The Overview tab: index performance and statistics, sectors, indices, bonds, crypto, FX, commodities, largest companies, movers, index movers; refreshed every minute while shown |
| `src/OverviewWidgets.*` | Overview building blocks (QPainter): panel, table (with a bar column and clickable rows), performance chart with the green / red envelope and crosshair, statistics cards |
| `src/EconomyData.*` | The Economy tab's catalogue (sections, indicators, sources, units, ranges), loading through `research::Client`, the stock market Fear & Greed estimate |
| `src/EconomyPage.*` | The Economy tab: sub-header, panels, area charts with ranges, zones and hover, Fear & Greed dials |
| `src/CountryData.*` | Economy › Countries: the catalogue (`resources/country_indicators.json`), providers (FRED, World Bank, IMF, OECD, Eurostat, BIS, Yahoo, EIA, surveys), parsing, transformations, the per-country loader |
| `src/CountriesView.*` | Economy › Countries: the list of countries by group with search, the country page with its tabs and tables, CSV export |
| `src/NewsData.*` | News data: `news::Wire` (Financial Modeling Prep with a key, else the free RSS feeds merged; five-minute cache; the Sources page's state per provider), the RSS / Atom parser, the ticker guess and `news::Favorites` (the starred articles, `news-favorites.json`) |
| `src/NewsPage.*` | The News tab: categories and ticker filter, the "News wire" strip, the painted cards (`newsui::Card`, with their favorites star) and their grid (`newsui::Grid`, as many columns as 420 px fit) |
| `src/NewsReader.*` | The article reader (`newsui::Reader`): the floating panel over the News page with the article's web page, back, the favorites star, open in the browser, close, the ad blocker's shield and the HTTP-refusal notice |
| `src/NewsWebHost.*`, `src/NewsWebHostQt.cpp`, `src/NewsWebHostMac.mm` | The reader's web view behind one interface (`newsui::WebHost`): the system's WebKit on macOS (WKWebView in a native child widget, Brave's lists as compiled WebKit content rules, snapshots for `--screenshot`), Qt WebEngine elsewhere (own profile, request interceptor, injected cosmetic rules) |
| `src/AdBlock.*` | The reader's ad blocker: `adblock::Blocker` (Brave's default filter lists downloaded daily, the adblock-rust engine built on the thread pool, request checks from the IO thread, cosmetic rules) and `adblock::Interceptor` (the profile's request interceptor) |
| `third_party/adblock-ffi/` | A Rust crate (`staticlib`) exposing Brave's `adblock` crate as a C ABI (`include/adblock_ffi.h`): build, check a request, cosmetic resources, hidden selectors, (de)serialization; built by cargo from CMake |
| `src/QuantPage.*`, `src/QuantViews.h`, `src/Quant{Compare,Pair,Cot,Signals}View*.cpp` | The Quant tab: sub-header and its five sections (compare, correlation, risk & beta, COT, signals) |
| `src/QuantCharts.*` | Quant charts (QPainter): multi-line with end labels, dual-axis overlay, spread and band, z-score, scatter with regression and marginal histograms, long/short bars, step chart; the multi-line chart keeps everything but the hover crosshair and tooltip as an image, redrawn when its data, size or theme change |
| `src/QuantData.*` | Quant statistics (OLS, ADF with MacKinnon p-values, half-life, z-score, RSI, Student's t) and the CFTC legacy COT report |
| `src/QuantJob.*` | Estimations off the UI thread: `JobRunner` (QtConcurrent, typed result, progress relayed to the UI thread, cancellation) and `JobBar` |
| `src/QuantMarkovViews.h`, `src/QuantMarkov*View.cpp` | The Markov section of the Quant tab: HMM regimes, MS-GARCH, execution |
| `src/QuantMarketState.*` | The regime (with its chain) and volatility forecast per symbol and source published by the Markov views, the background refresher, and the strip the other tabs show (`MarketState`, `StateRefresher`, `MarketStateStrip`) |
| `src/QuantMarkovJobs.*` | The HMM and MS-GARCH estimations as functions of prices and a spec, shared by the views, the refresher and the execution view |
| `src/QuantPortfolioView.cpp` | The Portfolio tab: positions, parametric risk, regimes |
| `src/AiPage.*` | The AI tab: Models (Hub search, files, downloads, the models on this computer) and Claude (the conversation) |
| `src/AiModels.*` | `HuggingFaceHub` (search, files, resumable downloads) and `LocalModels` (the models folder) |
| `src/ClaudeClient.*` | The Messages API client: streamed requests, the models list |
| `src/AiMlView.*` | The Machine learning section: environment, data export, training form, metrics, runs |
| `src/MlEnvironment.*` | The app's Python virtual environment: interpreters, creation, checks, scripts run as child processes |
| `src/MlData.*` | The CSV export of Deepwater's data for the scripts |
| `resources/ml/` | `train.py` (scikit-learn, LightGBM, XGBoost, PyTorch) and `check_env.py`, copied into the environment |
| `src/markov/` | The Markov engine, no UI: `LinAlg` (small dense matrices, Cholesky), `Dist` (normal and Student densities, quantiles, partial expectations), `Optim` (Nelder-Mead, BFGS), `Hmm` (Gaussian HMM: EM, filter, smooth, Viterbi, criteria), `MsGarch` (GARCH and MS-GARCH: Hamilton filter, MLE, forecast, VaR/ES), `Execution` (Almgren-Chriss, MDP policy, TWAP/VWAP schedules, volume profiles, square-root impact, Q-learning agent, path simulator and Monte Carlo), `Simulate` (synthetic samples), `Job.h` (progress and cancel contract) |
| `tests/quant/markov/` | `qtm_test_markov [hmm|garch|execution]`: parameter recovery on simulated data (HMM, GARCH, MS-GARCH, Student), optimiser and distribution checks, the MDP against Almgren-Chriss and the Monte Carlo against the analytic costs, volume profiles, square-root impact and the Q-learning agent, stability (long series, extreme scales, missing steps, outliers), cancellation, timings |
| `tests/quant/footprint/` | `qtm_test_footprint`: the footprint aggregation on hand-made trades worked out by hand (sides from the feed, quotes and tick rule, ticks and rows, delta, POC and ties, diagonal imbalances at the candle's edges and around empty rows, minimum volume and its Auto, stacked imbalances, regrouping, incremental vs rebuilt, trimming) |
| `tests/quant/bigtrades/` | `qtm_test_bigtrades`: Big Trades on hand-made trades worked out by hand (aggregation window from the first trade, side changes, same price vs sweep VWAP and min/max, exact timestamp, flush, tick-rule fallback, rolling percentile against a full sort and its speed, minute ATR and the bounded factor, manual min/max with max = 0, auto percentile, warm-up fallback, bubble sizes and opacity) |
| `tests/quant/heatmap/` | `qtm_test_heatmap [load]`: the liquidity heatmap's data on hand-made depth updates worked out by hand (levels added, changed, removed; buffering before the snapshot, stale updates, holes and resync, a snapshot older than the stream, futures `pu`, crossed books, trimming far levels; 16-bit codes; sampling at fixed times before an update, repeated columns without updates, holes while unsynced, catch-up cap, rows shared by bids and asks, the band's anchor; the column ring, resizing; the time pyramid (max, mean over the samples present, a band moving inside a bucket, nesting, bounded levels); row grouping over negative rows; percentile, gain, contrast, threshold, colour table; palettes: background at 0, brightness steadily up on dark backgrounds and down on light ones, a stop worked out by hand, Custom's three colours; the stability filter over a moved band and a hole; pulled liquidity (reached, filled instead, never reached, too far, back again, an ask, too small); absorbed (held, broken through, too little traded, another tick, buys at a bid, too short, an ask); filling the store in steps; recordings: exact round trip, a file cut short or damaged, other versions, a session recorded then replayed column for column), then a minute at 1,000 updates a second (book against a reference copy, bounded memory, flat time per update) |
| `tests/quant/navigator/` | `qtm_test_navigator`: the range slider. `core/RangeSlider` on hand-worked cases (span, hit zones of wide, narrow and sub-pixel windows, clip, bounds, the two-bar rule, pans, resizes, clicks outside, frozen pans, ranges past the data) and 100,000 random gestures (1.9 million moves) against its invariants; then `ChartNavigator` on a real chart, offscreen, with real mouse events: pans and resizes, bursts faster than the chart's frames, drags out of the strip, wheel and chart drags followed, live bars at and away from the end, new data, a compared series adding times (indexes and the line), no data, one bar, the line's cache, resizing, cursors, the chart going away; `BarAxis` (bars at their own chart index with a compared symbol going further back or sitting between the bars, a time inside its slot) |
| `src/Theme.*` | Palettes (Ember, Abyss, Jade, Oxide, Nova, Bloomberg; Glacier, Dune and Sakura — light); `resources/style.qss` is a template using `@Token` colors; `Theme::installedFamilies` keeps only installed fonts in a font list (`@MonoFamilies` in the stylesheet), since Qt spends tens of milliseconds the first time it is asked for a missing one |
| `src/AisStream.*` | aisstream.io WebSocket client (subscription, reconnect/backoff, error reporting); the socket is created once the system's TLS certificates are loaded on the thread pool, so launching doesn't wait for them |
| `src/VesselStore.*` | Live vessel state, classification, trails, pruning |
| `src/Chokepoints.*`, `src/ChokepointsPage.*` | Passage detection; the ships in each zone now (counts and list, one pass over the vessels for all zones, kept until the store reports a change or for a second); the charts of a chokepoint's details (the `ChokepointsPage` dashboard itself is no longer shown) |
| `src/PortWatch.*` | IMF PortWatch daily chokepoint transits (history) |
| `src/SupplyChain.*` | Map › supply chains: crop yields (USDA PSD, Eurostat), road freight (ITF), crude stocks (JODI, EIA weekly), the New York Fed's pressure index, FRED freight indices, PortWatch ports and disruptions (`PortActivity`), the bundled oil sites; `SupplyChain` holds them together |
| `src/FxData.*`, `src/ForexPage.*` | FX rates (ECB via Frankfurter), pair cards and correlation matrix |
| `src/MacroData.*`, `src/HttpCache.*` | BIS policy rates, CFTC positioning, IMF WEO, World Bank sectors, shared download cache (written on a background thread, one file after the other in the order asked; the app waits for the writes before it exits). `RemoteDataSet` reads and parses every cached copy on the thread pool at launch, all at once (`MainWindow` waits for them before reading the data); the large downloads (BIS policy rates, WEO, balance of payments, trade flows, World Bank sectors) are also parsed there when they arrive, through `parser()`, and only published on the UI thread |
| `src/DomesticFinance.*` | Property and debt data sets (BIS, OECD, IMF Global Debt Database, World Bank) |
| `src/KeyIndicators.*` | The latest official figures: OECD Key Economic Indicators, Eurostat, the IMF's consumer prices and the BLS as `CountrySeriesSet`s, and the `KeyIndicators` façade that picks a source per indicator and country |
| `src/OfficialSources.*` | The catalogue of the sources credited on tradingeconomics.com (`resources/data_sources.json`): per country, the body behind each indicator, its site, kind, access and API |
| `src/NationalSources.*` | National offices read directly, no key: Brazil, Argentina, South Africa, Singapore |
| `src/KeyedSources.*` | Sources with a user's free key (`KeyedSeriesSet`): FRED, EIA, BEA, Banxico, Bank of Korea, Turkish central bank; `ComtradeProducts` (trade by product, on demand, key optional) |
| `src/SurveyHeadlines.*` | The private surveys' headline figures from the economic calendar, with their publishers |
| `src/MacroIndicators.*` | Indicator definitions, values, formatting and heatmap color scales |
| `src/MacroPanel.*` | Right sidebar indicator cards, country ranking list, annual bar chart |
| `src/SeaRoutes.*` | Sea-route graph (Dijkstra) and AIS destination → port resolution |
| `src/GlobeWidget.*` | Orthographic globe (QPainter): land, borders, graticule, clustered markers, interaction. The globe image is projected once per view change, then painted in horizontal bands on a thread pool (shapes keep their bounding box so each band only strokes what crosses it); its radial gradients (backdrop, halo, ocean, limb shade) are cached and only redrawn when the globe moves on screen or the theme changes; markers, vessels and flow particles are drawn on top every frame. Ships are sprites: each shape (a tanker or cargo triangle for each of 128 headings, a moored dot, an unidentified ship's dot) is rasterized once at the screen's resolution, with half-pixel phases on a standard screen, then copied (filling thousands of small paths cost tens of milliseconds a frame); the last three ship sizes are kept for zooming; the frame timer only runs while something moves (spin, inertia, a selection's pulse, flow particles) and the globe is shown |
| `src/GeoData.*` | Loads country polygons as unit vectors |
| `src/MarketData.*` | Categories, commodities, sites and the price shown for each commodity |
| `src/CommodityPrices.*` | Commodity futures (Yahoo) and IMF monthly prices |
| `src/LeftPanel.*` | Left sidebar home: overview, top movers, layer toggles, market watchlist |
| `src/DetailPanel.*` | Left sidebar details over the home panel: cluster, site, commodity, vessel, chokepoint, indicator ranking, country; `DetailPanelSupply.cpp` holds the supply chain sheets (overview, port, oil site, disruption) |
| `resources/markets.json` | Commodities (futures symbol, IMF series, units) and sample sites (edit to add data) |
| `resources/countries.json` | Natural Earth 1:50m countries (public domain), stripped to geometry |
| `resources/data_sources.json` | Catalogue of the data sources named on tradingeconomics.com, built by `docs/audit-tradingeconomics.md`'s crawl |

## Translations

- `i18n/deepwater_fr.ts` — UI strings. After adding `tr()` strings, run
  `cmake --build build --target deepwater_lupdate`, then translate the new entries (Qt Linguist or by hand).
- `i18n/deepwater_data_fr.ts` — strings from `markets.json` (categories, commodities, site types, countries),
  maintained by hand since lupdate can't see them, plus the `Country` context (French names of the IMF countries,
  also used for the Calendar's countries and the Research company country) and the Overview's commodity names.
- Strings saved on disk are stored untranslated and translated when shown: the regime names of the shared Markov
  state (`quant::regimeKeys()` / `regimeLabelText()`) and the MS-GARCH model name (`quant::modelText()`), so a
  state fitted in one language reads in the other. `quant::CotTable` is declared in a `.cpp` and needs `Q_OBJECT`
  (and the `.moc` include) for its `tr()` context to match the `.ts`.

Saved with `QSettings` between launches: language, theme, the AIS key, the sources turned off, the height of the Charts bottom panel, the visible layers (site categories and vessel classes), the rotation on/off (as last set with its button or `Space`; flying to a selection stops it without changing the setting), the flow map and the prism view, the Calendar tab shown and its economic filters (period, country, impact), the Overview's chart range, hidden series, sector period and the index of each index-movers panel, the Economy section, the Research symbol and section and
their choices (statement, period, index, compare mode and metric), the Quant section and its choices (Compare tickers
and range, Correlation range, window, symbols and boxes, Risk & Beta range, period and symbols, COT market, Markov segment, the HMM's symbol, range, states, inputs, window and
a-posteriori box, the MS-GARCH's symbol, range, regimes, innovations, horizon, capital and target risk, the execution's symbol and
parameters, the portfolio's positions, the shared regimes and forecasts).

Commodity prices are real (see Commodity prices); sites are illustrative sample data.
