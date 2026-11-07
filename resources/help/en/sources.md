# Sources

Every place the app gets data from, in two sub-tabs: the **List** and the **Data** diagram.

## List

The sources are grouped by the page they feed: the map and shipping, forex, macroeconomics, official statistics, national offices, databases with a free key, private surveys, the Overview's Treasury yields, the News feeds and the ad blocker's lists, the AI keys, the Charts providers and your own Charts keys, the data bundled with the app. Each row shows:

- Its **state**: up to date, downloading or error, the last check, the last time it had something new and how often it is checked. What only one page reads is read when that page is open.
- **Test**: a small request, timed.
- **Refresh**, for downloaded data sets.
- A **switch**: off, the source makes no more requests. The map and forex data sets keep the last copy saved on disk; the Charts page moves on to the next provider in its chain and says so on the chart. The survey publishers' rows only carry a switch: off, their figures leave the country sheets.
- Its **API key**, where one applies: aisstream, FRED, EIA, BEA, Banxico, the Bank of Korea, the Turkish central bank, UN Comtrade, Financial Modeling Prep, Hugging Face, Anthropic, Alpaca, Polygon and GitHub (the app's updates). A keyed source without a key says so and makes no request.

The search box filters the rows. The states refresh once a second while the tab is shown.

**The official catalogue.** Below the app's own sources, five collapsed groups list the bodies behind the world's statistics — statistical offices, central banks, ministries and agencies, international organisations, private institutes, about 1,300 — as cards: what each publishes, its access (free, headline free, paid) and API, the state of its site at the last inventory, **Test** and **Open the site**. The search box and the country and access filters apply to them too.

## Data

A diagram of where the data goes. On the left the bodies and companies the app reads (about 64), grouped: international bodies, US agencies, national offices and central banks, markets and data companies, web, news and AI, this computer; on the right the parts of the app (the map's ships, supply chains, commodities, heatmaps and country sheets, trade flows, Overview, Forex, the Calendar's views, Economy's dashboards, Countries and Indicators, Research, Quant, Charts, News, AI). A link for each pair, as thick as the data sets it carries; dots and links take the list's states (working, failing, not checked yet, dashed when off). The **Fusion** block, between the sources and Economy's Countries and Indicators, is the golden records.

Hover a source, a part or a link to bring its data out and list its data sets in the side card; click to pin it; **Show in the list** opens the List with only those data sets. Under the sources, **Listed, not read by the app** are the catalogue's bodies the app doesn't read, with their access and names by country.

**Zoom out: disc** lays the whole diagram on a tilted disc, the sources along its left arc and the parts along its right one; **Zoom out: plates** stands it on thin plates one behind the other. Hovering and clicking still work.

Under the disc, the **underground floor** shows the AIs and what each reaches: the slots holding a model, opencode, Claude, Machine learning on the right; on the left the country series, tables and charts, golden records, IMF WEO, policy rates, exchange rates, COT positions, prices (the skill *Read Deepwater's data*), the strategy tester and the paper account (*Trade*), and the page in view (the side panel's context line). A link is dashed when the access is granted but not in force (a suspended slot, opencode not served); shafts rise to what each reads on the disc above. The **up and down arrows** move between the floors; a **click** on any item looks at it closely, with what it comes from on its left and what it goes to on its right; the **left and right arrows** go to the previous and next item; **Escape** goes back.
