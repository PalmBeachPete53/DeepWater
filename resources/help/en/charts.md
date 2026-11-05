# Charts

The Charts page is a trading terminal: a port of Quant Terminal (MIT licence) with Deepwater's additions, order flow and the liquidity heatmap among them. Its texts stay in English whatever the language. It takes the app's theme.

## Layout

- **The chart panes**: one or several, each with its symbol, timeframe and chart type (candles, bars, line, area, Heikin Ashi, Renko, Kagi, point & figure, **Footprint**…). The toolbar holds the symbol box, the timeframe, the chart type, **Indicators**, **Compare** (another symbol on the same chart), **Replay** (bar replay with a cursor), the CSV export and the settings. A thin **range slider** under each pane shows all the bars loaded and the window the chart shows (below).
- **The side panel**: **Watchlist**, **Alerts**, **Calendar** and **News**.
- **The bottom panel**: **Strategy tester**, **Screener**, **Heatmap**, **Seasonals**, **Financials**, **Paper trading**, **Indicator editor**, **Terminal** and **PTQ**. Drag its top edge to resize it.

The backend runs for the whole session: alerts keep firing and the paper account keeps filling while you are on another tab.

## Data

Crypto pairs come from Binance (binance.us where binance.com is blocked), stocks, indices, FX and futures from Yahoo Finance; your own **Alpaca** and **Polygon** keys, entered in [Sources](help:sources), add their feeds. When a provider is turned off in Sources the page moves on to the next in its chain, and the chart says so ("Binance is turned off in Sources, so this chart uses Coinbase"). Binance pairs get the forming candle from a live stream; elsewhere the pane polls the last bars every few seconds. CSV files can be imported as symbols.

The terminal also serves a local HTTP API on `127.0.0.1:3777` for your own scripts, the same as Quant Terminal's.

## Indicators

87 built-in indicators, series and chart-type transforms, with their settings and colours. Two kinds of your own, kept in the `custom/` folder of the Charts data (`~/Library/Application Support/Deepwater/Deepwater/quant-terminal/`), edited in the **Indicator editor** and reloaded when the file changes:

- **JavaScript** indicators, Quant Terminal's format.
- **Pine Script** indicators (`//@version=6`, `indicator(...)`, `plot(...)`): a `.pine` file shows in the list like a `.js` one. `input.*` become the indicator's settings, `plot()` lines or histograms, `plotshape()` and `plotchar()` markers, `hline()` levels; `bgcolor`, `fill`, labels, tables and `strategy.*` orders are ignored. They run on PineTS, which needs **Node.js 18 or newer** on the machine; without it the file is listed with the reason.

## Range slider

The strip under each pane shows the closes of every bar loaded and a window over the part the chart shows. Drag inside the window to scroll at the same zoom, click outside it to centre it there, drag an edge to zoom (never under ten bars). Scrolling or zooming the chart moves the window too. When the last bar is in view the chart follows new bars; otherwise it stays.

## Footprint

**Footprint** in the chart type list shows, inside each candle, the volume traded at each price row split by the aggressor: **bid** for aggressive sells, **ask** for aggressive buys. Binance pairs only, 1m to 1h; Binance reports the aggressor, so the sides are exact (a badge says so when they are estimated for another feed). The history is fetched back to the chart's first bar or the budget (300,000 trades by default); candles before the oldest trade loaded stay plain.

Per candle: the delta per row and in total, the **POC** (the row with most volume, outlined), **diagonal imbalances** (a side at least *ratio* times the other side one row diagonally, marked with a bar and bold figures) and **stacked imbalances** (N or more in a row, drawn as a zone that runs right until a later bar trades into it). The gear next to the chart type chooses the display — **Bid x Ask**, **Delta** or **Volume profile** —, the row size (**Auto** follows the zoom, or a fixed number of ticks), the imbalance ratio, the minimum volume, N, the colours and the history budget. Numbers appear when the rows are tall enough, coloured cells below that, plain candles when the candles are too narrow.

## Big Trades

**Big Trades** in the indicator list (Volume group) puts a bubble on the chart for each large aggressive order, from the same Binance trades. Consecutive trades on the same side within 50 ms at one price (or, with *Sweep*, at prices moving one way) are one aggregate; it is big when its volume reaches the threshold: **Manual** (a minimum volume) or **Auto** (the 99th percentile of the last 30 minutes' aggregates, times a volatility factor between 0.5 and 2). The legend shows the threshold and how it was made. Bubbles sit at the volume-weighted price and the exact time inside the candle, coloured by side, their area proportional to volume; hover one for its time to the millisecond, side, volume, average price, range and executions. Stack several Big Trades with min/max tranches for different sizes.

## Liquidity heatmap

**Liquidity Heatmap** in the indicator list (Volume group) draws, under the candles, the resting orders of the pair's Binance order book over time: each column is the book at one instant, each cell the quantity resting in one price row, brighter for more. Binance pairs only, best on 1s to 1m charts. The current book is a compact **DOM** at the right edge (bids in the buyers' colour, asks in the sellers'). Binance has no history of the book, so the heatmap fills in from when the chart opens, unless recordings exist (below).

Settings: the sampling interval (250 ms by default), the history kept (120 minutes), price rows grouped, what a pixel shows zoomed out (**Max**: a wall stays visible; **Mean**: steady liquidity outweighs a moment's), the colours (**Deep**, **Thermal**, **Mono** or custom; intensity, contrast and a threshold under which nothing is drawn), the DOM, and:

- **Trade bubbles** (on): the aggressive orders over the liquidity they hit, the top 10 % of the last 30 minutes by default.
- **Stability filter** (off): only the liquidity that rested at least that long (500 ms to 10 s). An approximation, since Binance's book is by price level, not by order.
- **Pulled and absorbed liquidity** (on): an orange cross where a big level vanished as the price approached (cancelled, not filled), a violet bar where the best bid or ask held while aggressive orders of its size traded into it, and a diamond where the price then left.
- **Record the order book to disk**: what Binance sends is written as it comes, one file per pair and hour (about 6 MB an hour on BTCUSDT), kept for a number of days. When a heatmap opens, the recordings that reach into its history are replayed first, so it starts with what was recorded; in the chart's **Replay** it shows the recorded depth up to the cursor.
- A **debug overlay** with the feed's, book's and drawing's figures.

## Alerts

The **Alerts** tab of the side panel creates price and indicator alerts on any symbol; they are checked every 15 seconds and fire on every tab.

## Strategy tester and paper trading

The **Strategy tester** backtests a strategy written as rules over indicators (the editor's help gives the format; a few examples are built in) on the active chart, with the trades marked on it, the statistics and a CSV export. **Paper trading** is a simulated account: market, limit and stop orders with optional stop loss and take profit, fills, fees, positions and history; the bar replay has a practice account of its own. A local model with the *Trade* skill can drive both (see [AI](help:ai)). **PTQ** sends a strategy to the PTQ connector for real execution.

## Terminal

The bottom panel's **Terminal** tab is your login shell in a pseudo-terminal, like an editor's integrated terminal. ``Ctrl+` `` opens and focuses it, or hides the panel when it already has the focus; `+` opens another session, the bin closes one; sessions keep running while you are on another tab. 256 colours and true colour, the alternate screen (vim, htop, less), mouse modes, 5,000 lines of scrollback (wheel, `Shift+PageUp/PageDown`), selection with the mouse, `⌘C` / `⌘V`, `⌘K` clears, `Option+←/→` jumps words.
