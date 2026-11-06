# Journal

Your real trades and your risk, kept on this computer in a file of the app's data. Nothing is sent anywhere. The Journal never connects to a broker and never asks for a password: trades come in by file or by hand. Sending orders is [MetaTrader](help:metatrader)'s business, a separate page.

The bar at the top picks the account (you can keep several, each with its currency and balance) and filters by year, instrument, side and strategy.

## Importing

**Import trades** reads a statement exported from your platform:

- **MetaTrader 4 and 5**: the HTML account statement or report.
- **cTrader**: its CSV history.
- **Any other CSV**: the columns are recognised by name, and the dialog lets you say which is which.

A trade already there isn't imported twice, so a new statement can overlap the old one. Symbols with a broker's suffix (`EURUSD.m`) are matched to the same market. **+ Log trade** enters one by hand.

## Dashboard

Net P&L, win rate, profit factor, average win against average loss, expectancy per trade, maximum drawdown, recovery factor, the longest streaks and the average holding time, with the cumulative P&L, the last trades, the month and the quarter. The performance score is this app's own mean of six measures, each on a plain scale: not a standard. Positions still open are valued at the terminal's delayed quotes, with the contract sizes of a standard account.

## Trade log

Every trade: dates, symbol, side, volume, prices, time held, costs, profit, net, strategy and notes. A trade can be edited, to add its strategy and a note, or deleted.

## Advanced details

The result by weekday, by time of day and by holding time, the five best symbols, and your five best and five worst trades.

## Risk calculator

A position calculator. The account's balance and currency, the instrument, the entry (or the live price), the stop, an optional target and the risk per trade, in percent or in money, give the position size in lots, the money at risk, the reward and the ratio, drawn on a small map of the trade. Contract sizes are those of a standard account. **Check your broker's contract size and margin**: they differ from one broker to the next, and the calculator can't know yours.
