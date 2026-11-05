# Data and limits

What to know before trusting a figure: where it comes from, how fresh it is, and what is estimated.

## Freshness

| Data | How fresh |
|---|---|
| Commodity futures, stock quotes, Overview, Research, Quant | Yahoo Finance, delayed by about ten to fifteen minutes; the Overview refreshes every minute |
| Charts (Binance pairs) | Live, tick by tick; other symbols are polled every few seconds |
| Ships | Every minute or two per ship; dropped after 30 minutes of silence |
| Chokepoint history, ports | IMF PortWatch, published weekly with a few days' lag |
| Forex | The ECB's fix, once per business day, no intraday |
| Economic calendar | Every ten minutes while shown |
| Country figures, dashboards | From monthly (prices, jobs) to annual (national accounts); a compilation can run a quarter behind the national office |
| IMF World Economic Outlook | Twice a year; the current year is an estimate, later years projections |
| Balance of payments, trade flows | The latest year most countries have filed, about a year back |

## Estimates and experimental figures

- The stock market **Fear & Greed** dial (Economy › Sentiment) is tagged ESTIMATE: it is rebuilt from public series, not CNN's own figures.
- Research's **intrinsic value**, **factor grades** and **quant rating**, and the rebuilt **recommendations history**, are tagged EXPERIMENTAL: fixed assumptions, not validated against later prices.
- **Chokepoint passages** are lower bounds (ships reporting to aisstream.io only), and some straits get no reports at all.
- The **supply chain graph** (Research › SPLC) counts Wikidata products, not revenue; coverage is uneven.
- The liquidity heatmap's **stability filter** is an approximation: Binance's book is by price level, not by order.
- **Footprint** sides are exact on Binance (it reports the aggressor); the badge "APPROXIMATE SIDES" appears where they are estimated.
- A country's **Latest figures** pick the most direct source; a frozen series is replaced by a fresher compilation.
- Economy's **golden records** merge sources that agree; the tooltip says which were rebased or left out and why.

## Keys and privacy

Requests go straight from the app to each source; there is no server in between and no telemetry. Keys are stored in the app's preferences (the Charts keys in a file only you can read); they are sent only to the service they belong to. The News reader names itself `Deepwater` to the sites it loads. A local model runs in a sandbox with no network and no access to the preferences; what an AI reads goes through the journaled tools of its slot, and the Claude and opencode integrations send what their tools read to their providers.

## Rate limits worth knowing

FRED allows 120 requests a minute (the app spaces its own); the US Bureau of Labor Statistics 25 a day per address; the OECD about 20 a minute; UN Comtrade's public preview a few requests a day without a key; PortWatch times out on heavy queries, so the ports load over a dozen requests. A source that fails shows it in its row and the chart or table it feeds.

## Licences

Deepwater bundles Quant Terminal (MIT), Lightweight Charts' behaviour re-implemented, PineTS (AGPL-3.0), llama.cpp (MIT), libvterm (MIT), Brave's adblock-rust (MPL-2.0), the Eurostat marnet maritime network (EUPL-1.2) and UN/LOCODE. Their notices ship with the app.
