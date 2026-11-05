# Map

The map is a globe. Drag to spin it, scroll to zoom, click a dot, a cluster or a country. `Space` starts and stops the rotation; `Esc` clears the selection, and a second `Esc` turns an indicator heatmap off. The globe's own controls sit at its corner: **3D**, **⇄** and **⚓**, explained below.

## Commodities

The dots are production sites, mines, fields and farms of about forty commodities, coloured by category (energy, precious and industrial metals, grains, softs). They cluster as you zoom out; click a cluster to open it. The left sidebar holds the **watchlist** and the **top movers**; clicking a commodity opens its sheet: price, change, chart, and the sites producing it.

Prices are real, each with its source and time (the row's tooltip, the note under the chart):

- **Futures** for 20 commodities (crude, Brent, natural gas, gold, silver, copper, platinum, palladium, aluminium, wheat, corn, soybeans, rice, coffee, cocoa, sugar, cotton, LNG, iron ore, palm oil), from Yahoo Finance, about ten minutes behind the exchange. The change is against the same contract's previous close; the top movers rank these daily changes.
- **IMF monthly averages** for every commodity: the only price for coal, uranium, nickel, zinc, lithium and cobalt, which have no free futures quote; elsewhere a reference line under the futures price. The IMF publishes a month's average early the next month.

## Live shipping

Ships need a free **aisstream.io** key, entered in [Sources](help:sources). Only commodity carriers are shown, classified from the type they broadcast: **tankers** (crude, products, LNG, chemicals) and **cargo ships** (dry bulk, containers); AIS does not say what is on board. A ship silent for 30 minutes disappears; trails appear when zoomed in. Ships broadcast their name and type only every few minutes, so the first session fills in slowly; what they said is remembered and later sessions classify them as soon as they appear.

Clicking a ship opens its sheet. Its AIS destination (a port code, a port name, `FROM>TO`) is resolved to a port and the **probable route** is drawn on a maritime network, with the distance by sea and the time at the current speed.

## Chokepoints

Dashed zones mark ten straits and canals. Click one for its passages over the last 48 hours, an hourly chart and the daily transits. A passage is a ship seen inside the zone after more than six hours away; counts are lower bounds. Some zones get no reports at all from aisstream.io (Hormuz, Suez, Panama, the Bosphorus at the time of writing): the sheet says so rather than showing zeros. The daily history comes from the IMF's PortWatch (satellite AIS, published weekly with a few days' lag): tankers, dry bulk, other, tanker capacity, 7-day against 90-day trend.

## Economic indicators

The right sidebar lists 24 indicators in four groups:

- **Economy**: real GDP growth, inflation, unemployment, policy rate, real policy rate, current account, budget balance, public debt, GDP per capita (PPP) and GDP.
- **Capital flows and trade**: net capital inflows, FDI and portfolio inflows, trade balance, trade openness, export volume growth. These come from the balance of payments, which has reported figures only: the default year is the latest that most countries have filed, about a year back.
- **Latest official figures**: inflation and unemployment over the latest month, from the national offices and the free compilations.
- **Supply chains**: crop yields, road freight, crude oil stocks (below).

Each card shows the world median and a histogram of the countries. Clicking one colours the globe (legend at the bottom left, on a 5th–95th percentile scale) and shows the ranking in the left sidebar, with a year slider from 2010 to the last IMF projection. The circled `?` by the ranking's title explains what the indicator measures, how to read it and its pitfalls. Figures for the current year are IMF estimates, later years projections.

- **3D** raises each country by its value, on the same scale as the colours, and tilts the view.
- **⇄** shows the trade in goods as a flow map: the 50 largest bilateral flows, or with a country selected its ten main customers and suppliers, arcs from exporter to importer with particles moving toward the importer. Hover an arc for the amount and its share of both countries' trade. The source is the IMF's trade by partner country, latest complete year; the first download is large (every year since 1948) and can take up to a minute.

## Country sheet

Click a country on the globe or in a ranking. Its economy sheet gives:

- The headline figures, last year / this year / next year, growth and inflation bars, the policy rate's history.
- **Economic sectors**: agriculture, industry and services as shares of GDP and of jobs, the value added in dollars, manufacturing within industry, and the change over ten years. The circled `?` reads the country's own figures: its profile (services, agricultural, extractive, industrial, mature or in transition), output per job of each sector, how the shares moved (deindustrialisation, rebalancing, tertiarisation), then for 56 larger economies a written history with its events numbered on the chart.
- **Real estate**: real house prices over a year, ten years and from their peak, and their valuation against incomes and rents. **Debt**: households, companies and the government in % of GDP, debt service, bank credit, bad loans. Both have a `?` with a reading of the figures (boom, bust, credit boom, deleveraging, strained households).
- **Latest figures**: the monthly and quarterly figures the markets follow (CPI, unemployment, GDP, production, retail, confidence, PMIs…), each with its previous value and period, from the most direct free source that covers the country.
- **Trade by product**: exports and imports by product chapter for the latest full year, with shares (UN Comtrade; a key lifts the public preview's limits).
- **Supply chains**: yields, road freight and crude stocks over three years.
- The commodity sites located there, and the **official sources** behind the country's statistics: statistical office, central bank, ministries, with what each publishes, whether its data is free and how a program reads it.

## Supply chains

The **⚓** button shows the supply chain layer and its overview in the left sidebar:

- **Ports** (dots): about 2,000 ports from the IMF's PortWatch, coloured by their port calls over the latest seven days against the same week a year before (grey without figures), sized by their traffic. The busiest show from afar, the others as you zoom in. A port's sheet gives its calls, tonnes unloaded and loaded, its profile and the disruptions that named it.
- **Disruptions** (rings): cyclones, earthquakes, floods, wildfires and events such as the Strait of Hormuz, running or ended less than 30 days ago, coloured by alert level, with the ports they hit.
- **Oil storage** (diamonds): 44 trading hubs, strategic reserves and terminals. The EIA publishes the stocks of Cushing, the Gulf Coast and the Strategic Petroleum Reserve (with an EIA key); every site also shows its country's month-end crude stocks (JODI).
- **Overview**: the New York Fed's Global Supply Chain Pressure Index, the ports' week against a year before with the largest falls and rises, the disruptions, US freight (truck tonnage, shipments, freight prices; with a FRED key) and the EIA's weekly stocks.

The **Supply chains** group of the right sidebar colours the countries by wheat, corn, rice and soybean **yields** against the year before (USDA forecasts, Eurostat for EU members; the slider reads marketing years), **road freight** in tonne-km (about 45 countries) and **crude oil stocks** at the latest month against a year before.

## Sources

The IMF's World Economic Outlook and Balance of Payments, the BIS (policy rates, property prices, credit, debt service), the World Bank (sectors, credit, bad loans), the OECD (house price ratios, key indicators), Eurostat, national offices (BLS, Brazil, Argentina, South Africa, Singapore and more), the IMF's trade and consumer price data sets, UN Comtrade, PortWatch, JODI, the USDA, the ITF, the New York Fed, FRED and the EIA. Every one has a row and a switch in [Sources](help:sources); the ones with a key say so until you enter one.
