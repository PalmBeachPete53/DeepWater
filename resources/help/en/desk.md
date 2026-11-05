# Desk

A trader's reading of the markets in fourteen sections, under the sub-header; the last one shown is remembered. Prices come from Yahoo Finance, delayed by about fifteen minutes. Nothing here needs a key or an account, and every source is listed in [Sources](help:sources) › Desk.

The texts are written by fixed rules, not by a model. Where a section has an **Ask the AI** button, it opens the [AI side panel](help:ai) and asks your local model to read the section through its tool: the question holds no figures.

## Markets

109 markets in five columns: forex, indices, commodities, stocks and crypto. Search by name, change the period (1 day, 1 week, 1 month) and the order. The tiles give the breadth, the average move, the leader and the laggard. A click on a row opens that market's file in Technicals.

## Strength

Unweighted currency indices: each of the eight major currencies against the seven others, every pair counting the same. A chart since the first bar on screen, on 15 minutes, 1 hour, 4 hours or daily bars and the ranking. It reads relative moves, not the economy. Trade-weighted indices are a different measure.

## Sessions

Sydney, Tokyo, London and New York on a timeline in your own time, with what is open now, the overlaps, and the time to each open and close. Daylight saving is handled. The hours are the usual trading hours, not an exchange's official ones.

## Recap

What happened in a session or a week: the moves of the main markets from open to close, on 15-minute bars for sessions, the largest move drawn, the calendar's releases and the wire's headlines of that window. The summary is a fixed text built from those figures. **Ask the AI** has your local model read it.

## Wire

One list of three things, newest first: headlines from InvestingLive's public feed, refreshed every minute; markets crossing another 3% step on the day; and releases from the calendar. It is a feed, not a professional squawk: minutes behind at best. **Live TV** opens a channel's live stream on YouTube in your browser.

## Rates

- **The Fed's path**: what CME 30-day fed funds futures imply at each meeting of the Fed's own calendar: the rate after the meeting, the change, the cumulative change, and the probabilities of −50, −25, hold, +25 and +50 basis points. Each move is split between the two nearest quarter points, the usual simplification. It reads what is priced, not what will happen.
- **Central banks**: nine policy rates from the BIS, the last move and its date, the change over a year, and the next decision when it is known. The economic calendar only lists some decisions ahead, so the column is often empty outside the Fed.
- **Yield curves**: US Treasury par yields and the ECB's AAA euro area spot curve, latest published.

Only the Fed has probabilities: no free source publishes rate futures for the other banks.

## Positioning

Two lists, for two crowds. For futures positions, see [Quant](help:quant) › COT.

- **Forex and CFDs**: Myfxbook's community outlook, through its official API and your free account: the share of lots long and short, the number of positions and the average prices by symbol, the most traded first. The community includes demo accounts, and the share is shown above the list. Half the symbols have only a few dozen positions and read 0% or 100%: they stay in the list, dimmed, but the tiles only count symbols with 500 positions or more. Connect here with your email and password. The password goes once to Myfxbook, over HTTPS, to open a session, and isn't kept. The session ends after a month or when your internet address changes, and the section then asks again. Figures are asked at most every half hour, and there is no history. It is one community of retail traders, not the whole market.
- **Crypto**: Binance's public ratio of accounts long and short on twelve USDT perpetuals, with the last 30 days: a measure of the crowd, not of money.

## Risk appetite

A composite from 0 to 100 of seven markets on daily closes: the VIX, the S&P 500's trend, high yield against investment grade, AUD/JPY, copper against gold, stocks against gold, and bitcoin. Each is ranked against its own last 252 days and the ranks are averaged; under 40 reads risk-off, over 60 risk-on. It is this app's own estimate, not a published index.

## Seasonality

How a market behaved over the calendar: the average path of the year against this year, the win rate and the mean return of the period, the mean return by month and by weekday, and the period year by year. Choose 5, 10, 15 or 20 years, the full year, a half or a quarter, and whether to leave out extreme years beyond 1.5 standard deviations. The current year is left out of the averages. Past seasonality doesn't predict a given year.

## Fundamentals

Eight currencies on six themes: rates, inflation, growth, labour, activity and sentiment, from each economy's latest releases on the economic calendar and the BIS's policy rates. Each theme reads strong, neutral or weak by fixed rules, shown under the table, and the score is their mean from −100 to +100. A theme without a recent release is left out. The comparator sets two currencies side by side. It is a simple reading, not a forecast, and **Ask the AI** has your local model comment on it.

## Technicals

Every market on six signals: trend (EMA 20 against EMA 50), momentum (RSI), volatility, volume, structure (higher highs, lower lows) and liquidity. Filter by class, trend and name; a click opens the market's signal file with its radar. Volume is the market's own, or an ETF's as a proxy for FX and some commodities; a dash means there is none.

## Liquidity

Stop pools read from price alone: equal highs and lows on 1-hour or daily bars, where stops tend to sit. The chart draws the pools, the previous day's high and low; the tables give each pool's touches, distance and state, the recent sweeps and the sessions' extremes. It is an estimate from candles, not an order book. For the real depth, see the heatmap in [Charts](help:charts).

## Notes

Titles from ING Think, the Fed, the ECB, the Bank of England and the BIS's central bankers' speeches, by publisher and with a search. A click opens the note in your browser. **The large banks' notes and FX forecasts are for their clients**, with no public feed, so they aren't here.

## Workspaces

Your own screens: up to six live panels of Desk sections, in as many named workspaces as you want. Panels load when the workspace is shown. They run in less room than the section, so a wide table may be cut at the panel's edge.
