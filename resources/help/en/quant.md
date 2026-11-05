# Quant

Quantitative tools on daily prices from Yahoo Finance (through the Research client, listed in [Sources](help:sources) under Research) and the CFTC's reports. The section shown is remembered; numbers keep the English grouping in every language.

## Compare

Up to eight tickers (chips, × removes; remembered) on one chart of their change since the start of the range (1 year, 6, 3 or 1 month; **Change %** off shows prices rebased to 100), and a table of their figures by section: key stats, valuation ratios, profitability and margins, earnings and financials, growth, balance sheet, dividends, ownership. In each row the best figure is green and the worst red (lower is better for valuation ratios, beta, debt and short interest).

## Correlation

A pair for pairs trading, Y (bought) against X (sold), over the range, with daily closes. The **hedge ratio** β is the least-squares slope of Y on X; the **spread** is Y − βX. **Cointegration**: an augmented Dickey-Fuller test of the spread, STATIONARY below a p-value of 0.05. **Mean reversion**: the half-life of the spread. **Signal**: the z-score of the spread against its moving average over the window (20 days by default): beyond ±2σ sell or buy the spread, beyond ±0.5σ WATCH, else NEUTRAL. Charts: both prices on their own axes, the spread with its average and ±2σ band, the z-score with the zones, and the scatter of daily returns with the regression line and its statistics.

## Risk & Beta

The regression of a stock's returns on an index's (SPY by default), daily or weekly, over 1 to 5 years: the scatter with the regression line, the histograms of both with their densities, and the statistics: raw beta, adjusted beta (Blume), alpha, R², correlation, standard deviations and errors, the t statistic of beta and its p-value. Below, the cumulative performance of both since the start of the range, green where the stock leads and red where it lags, or their spread.

## COT

The CFTC's weekly *Commitments of Traders* report (legacy format, futures only; positions as of Tuesday, published Friday) for the S&P 500, Nasdaq-100 and Dow consolidated contracts, the Russell 2000 E-mini, bitcoin, gold and silver: positions, week-on-week change, share of open interest and number of traders of non-commercial, commercial and non-reportable traders; long against short per group; net positions over two years; the weekly history.

## Signals

A scan of 48 large US stocks (the Dow and the biggest of the S&P 500, plus your Compare tickers) on five timeframes (5m, 15m, 1h, 4h, 1d): RSI(14) beyond 30 or 70, streaks of five or more candles one way, and gaps between a candle's open and the previous close. The most recent occurrence of each per symbol and timeframe, newest first, filtered by symbol, indicator and timeframe; the scan runs when the section is shown and every five minutes while it is.

## Markov

Three Markov models behind one set of segments; the estimations run in the background with a progress bar and Cancel.

- **HMM regimes**: a Gaussian hidden Markov model of the daily returns, alone or with the realized volatility as a second variable, with 2, 3 or 4 states, over 1 to 10 years or everything. States are sorted by rising volatility, so state 0 is always the calm one. Shown: the current regime with its probability and a one-line reading (days in the regime, expected visit, most likely switch), the fit's log-likelihood, AIC and BIC against K = 2, 3, 4 (lowest BIC in bold), the price coloured by regime, the stacked regime probabilities, the transition matrix and a table of the regimes (daily mean and volatility, annualized volatility, expected duration, share of time). By default everything is **filtered** — the probability given the data up to each day, usable as a signal; the **A posteriori** box switches to the smoothed probabilities and the most likely path, which use the whole sample. **Window** estimates on the last N days only.
- **MS-GARCH**: a Markov-switching GARCH(1,1) of the daily returns with 1, 2 or 3 regimes (1: a plain GARCH) and normal or Student-t innovations. Shown: the next day's volatility (daily and annualized) and the regime it comes from, the volatility over the horizon (1 to 250 days), VaR and expected shortfall at 95 % and 99 % for one day and over the horizon, a position-size calculator (capital × target daily risk ÷ forecast volatility), the model against a plain GARCH on the same sample, the regimes (persistence, long-run volatility, expected duration), the conditional volatility against the realized one, the filtered regime probabilities and the forecast cone.
- **Execution**: how to sell a quantity over a horizon (Almgren & Chriss): the symbol's decision price, average daily volume and volatility, the quantity, horizon and intervals, the risk aversion, the impacts and half spread, Monte Carlo paths, and the **regimes** of the HMM or the MS-GARCH when one is fitted. Strategies run on the same random paths: TWAP, VWAP (with the expected intraday volume profile), the analytic Almgren-Chriss schedule, a dynamic program over quantity and regime, and optionally a **Q-learning** agent trained on the simulator. For each: the mean, standard deviation and 95th percentile of the implementation shortfall in basis points, the trajectories and the histogram of costs.
- **Shared state**: after each fit, the symbol's current regime and volatility forecast are published to the other sections (a strip under the controls of Correlation, Risk & Beta and Execution, and in Research's symbol bar) and kept. A **background refresh** re-estimates every kept record older than the last trading day, a minute after launch and every 30 minutes; it has a row in Sources (section Quant) with a switch and a Refresh.

## Portfolio

Positions (symbol, quantity; negative for a short; kept) with their quote, value and weight, each symbol's regime and volatility forecast from the shared state (the realized one-year volatility until the refresher has estimated it; **Estimate missing** queues them), the standalone one-day VaR 95 % of each position, the share of the value in each regime, and the one-day portfolio risk four ways (VaR and expected shortfall at 95 and 99 %, in currency and % of value): parametric (from the correlations of a year of daily returns and the forecast volatilities, with the diversification ratio), historical (the portfolio's daily P&L over the year with today's positions), filtered historical (rescaled by forecast over realized volatility) and a regime Monte Carlo (each symbol draws its regime and takes its volatility; shocks correlated through the historical correlations), with the histogram of daily P&L, history against simulation.
