# Getting started

Deepwater is a desktop terminal for commodity markets and the world economy: a globe of live shipping and prices, the economies of 190 countries, trading charts, an economic calendar, news, company research, quantitative tools and local AI models. Everything comes from public sources, most of them without an account; a few free keys unlock more (see below).

## The window

The header holds the brand, the tabs, the UTC clock and three icons: `</>` (the developer tools), `?` (this help) and the gear (settings). Each tab is a page:

| Tab | What it shows |
|---|---|
| [Map](help:map) | The globe: production sites and prices, live ships, straits and canals, country heatmaps, trade flows, supply chains, country sheets |
| [Overview](help:overview) | A one-minute snapshot of the markets: indices, sectors, bonds, crypto, FX, commodities, movers |
| [Economy](help:economy) | US economic dashboards, then the figures of every country and every indicator |
| [Forex](help:forex) | The majors, their correlations, policy rates and speculative positioning |
| [Charts](help:charts) | The trading terminal: charts, indicators, order flow, backtests, paper trading, alerts |
| [Calendar](help:calendar) | Economic releases, US corporate events, market hours |
| [News](help:news) | Headlines by category, a reader with an ad blocker, favorites |
| [Research](help:research) | One company at a time: metrics, statements, ownership, ratings, peers, sectors, supply chain |
| [Quant](help:quant) | Pairs, beta, COT, signals, Markov regimes, volatility forecasts, execution, portfolio risk |
| [Desk](help:desk) | A trader's reading: strength, sessions, recaps, wire, rates, positioning, risk appetite, seasonality, fundamentals, technicals, liquidity, notes, workspaces |
| [Journal](help:journal) | Your real trades from MetaTrader or cTrader statements, their statistics and a risk calculator |
| [AI](help:ai) | Local language models, their skills, the Claude and opencode integrations, machine learning |
| [Sources](help:sources) | Every data source, its state, its switch, your API keys, and a diagram of where the data goes |

The `?` icon opens this help on the chapter of the page you are on. The search box at the top of the sidebar looks through every chapter: the list keeps the chapters that mention the words, the text jumps to the first match, and Enter goes to the next one.

## Arranging the tabs

The pencil after the tabs turns on the edit mode: drag a tab to move it, click its × to hide it, and use the + to bring a hidden tab back. At least one tab always stays. The order and the hidden tabs are remembered.

## Settings

The gear opens a floating window with two pages:

- **Language**: English or French. The whole interface switches at once, market data included (commodity and country names, categories). Figures follow the language too: `1 234,5` in French, `1,234.5` in English; the Charts terminal keeps its English texts.
- **Theme**: nine palettes, applied at once to every page, the globe and the charts. Six dark ones (Ember, Abyss, Jade, Oxide, Nova, Bloomberg) and three light ones (Glacier, Dune, Sakura).

## Keys and data sources

Deepwater works without any key. These free ones add data; all are entered in [Sources](help:sources), next to the source they unlock:

| Key | What it adds |
|---|---|
| aisstream.io | Live ships on the globe (nothing is shown without it) |
| FRED | Most of the Economy dashboards, about 170 US series in Countries, US freight on the supply chain layer |
| EIA | Weekly US petroleum stocks and production, oil storage sites, Energy tabs of the country pages |
| Financial Modeling Prep | News with a ticker on every article (without it, free RSS feeds) |
| Hugging Face | Downloads of gated models (the open ones need nothing) |
| Anthropic | The Claude integration's settings and model list |
| UN Comtrade | Trade by product beyond the public preview's limits |
| BEA, Banxico, Bank of Korea, Turkish central bank, INEGI, BPS, e-Stat | National series for those countries |
| Alpaca, Polygon | Extra data providers for the Charts page |

Each source also has a **switch**: off, it makes no more requests, and the last copy saved on disk stays in use.

## Side panels

Two flaps hang on the right edge of every page:

- **DEV** (shown when the `</>` icon is on): the [developer tools](help:devtools).
- **AI**: the [AI side panel](help:ai), to ask a local model about the page you are looking at.

Escape closes them.

## Keyboard

| Key | What it does |
|---|---|
| `F1`, `⌘?` | This help |
| `Space` | Starts or stops the globe's rotation |
| `Esc` | Clears the selection on the map; a second press turns the indicator heatmap off; closes the side panels, the reader and the help |
| `Ctrl+\`` | The Charts page's terminal |
| `⌘Enter` | Sends the question in the AI side panel |

## Where things are kept

Caches, downloaded models, the Charts page's data, golden records and the AI journal live in `~/Library/Application Support/Deepwater/Deepwater/`. The settings, keys included, are in the app's preferences. Deleting the data folder gives a fresh start without losing your keys.
