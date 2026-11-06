# Economy

The Economy page has a sub-header of sections: ten dashboards of US (and a few global) indicators, then **Countries** and **Indicators**, which cover the world. The section shown is remembered.

## Dashboards

**Sentiment**, **Crypto**, **Growth**, **Inflation**, **Employment**, **Rates**, **Housing**, **Consumer**, **Liquidity** and **Global**. Each panel is one indicator: an area chart with its own ranges (1M to 5Y for daily series, 1Y to 10Y for monthly ones, up to MAX for quarterly ones), the last value and its change since the observation before (in points for rates and spreads, in percent for levels), the date of the last observation, the source, and a `?` explaining the indicator. Hover the chart for the date and value.

A section is downloaded the first time it is opened; **Refresh** downloads it again. "Updating…" and "Unavailable: n" in the sub-header say how it went (the tooltip gives the last error).

Without a free **FRED** key the US charts can't load: a banner at the top of the page links to the key and takes it right there.

The VIX comes from Yahoo Finance. Most other series come from **FRED** (the St. Louis Fed): consumer sentiment, financial stress, credit spreads, GDP, consumption, CPI, PCE, payrolls, claims, JOLTS, the fed funds rate, yields and curves, housing, the Fed's balance sheet, M2… FRED's public download turns away requests that don't come from a browser, so these panels need a **FRED key**, entered once in [Sources](help:sources) (it serves the map and the country pages too); without it, the panels say so. The rest: the OECD (US consumer confidence), Yahoo Finance (the dollar index), alternative.me and CoinMarketCap (the crypto indexes, with their classes as bands: extreme fear to extreme greed, bitcoin or altcoin season).

The **stock market Fear & Greed** dial is tagged **ESTIMATE**: CNN's own figures are not available to programs, so the dial is rebuilt the same way from momentum (the S&P 500 against its 125-day average), volatility (the VIX against its 50-day average), safe haven demand (stocks less bonds over 20 days), breadth (equal-weight against cap-weight) and junk bond demand (the high yield spread, when FRED answers), each ranked against its last year. The components are listed next to the dial.

## Countries

About 190 economies by group (G20, America, Europe, Africa, Asia, Oceania); the search box filters them and Enter opens the first one left. A country's page has twelve tabs: **Overview**, **GDP**, **Labour**, **Prices**, **Money**, **Trade**, **Government**, **Business**, **Consumer**, **Housing**, **Energy** and **Health**, each a table of the indicators a free source publishes for that country: the last value, the one before, the highest and lowest of the whole history, the unit, the period and the source. Negative values are red; the tooltip names the series and what it measures. A tab is downloaded the first time it is opened; **Refresh** downloads the country again and **Export CSV** saves the shown tab.

Rows no free source publishes for the country are left out and counted under the table (their names in the tooltip); rows that need a free key (FRED for the United States, EIA for energy) say so.

**Several sources for one figure.** Each indicator lists its sources in order (a national office first, then the compilations); the first that covers the country wins, unless its series stopped more than a year ago. Once a row is shown, the other sources of the row are read in the background and compared with it: a source that agrees on the common periods is merged into a **golden record**, one that is the same series on another base (another index year) is rebased, one that measures something else is left out. The row then shows the record: the latest value from the most trusted source, "+1" in the Source column for each other source that gave it points, and a history that often reaches further back (France's food inflation back to 1971, its GDP growth to 1960). The tooltip lists the sources, how each relates to the record and the disagreements. Currencies are never mixed.

Sources: FRED (United States, about 170 series), the World Bank (every country, annual, plus its monthly Global Economic Monitor for the countries the others miss), the IMF (the World Economic Outlook and its statistics department's data sets: prices, production, labour, national accounts, reserves, exchange rates, trade, balance of payments, money, banks, interest rates), the OECD, Eurostat, the ECB, the BIS, the ILO, the ONS and the Bank of England, and national offices and central banks read directly (Brazil, Canada, Japan, Switzerland, Australia, Germany, France, Spain, Norway, Sweden; with a free key Korea, Mexico, Turkey, Indonesia and Japan's e-Stat), Yahoo Finance for the index and the currency, the EIA for energy, and the PMIs from the economic calendar. Each group of hosts has a row in Sources.

## Indicators

The other way round: the indicators by section (Main indicators, Markets, Labour, Government, Prices, Money, Trade, GDP, Housing, Business, Energy, Taxes, Consumer, Climate, Health; a search box), and clicking one opens its table of countries: **Country**, **Last**, **Previous**, **Reference** (the period of the last value), **Unit** and **Next release**, sorted by any column and filtered by region (All, G20, America, Europe, Africa, Asia, Oceania). Countries no free source covers are left out, counted under the table with those a free key would add; hover a row for its source and series code. **Export CSV** saves the rows shown.

Indicators without a free source are greyed, with the reason in their tooltip (credit ratings, the corruption index, rig counts…). The values are the Countries page's, downloaded for every country at once over the last ten years, so the highest and lowest aren't shown. **Next release** comes from the economic calendar ([Calendar](help:calendar)) when it lists releases of the indicator.
