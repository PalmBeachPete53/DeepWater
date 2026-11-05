---
id: imports
name: What a user can import
rows: [imports]
catalogue: []
access: local
probed: 2026-11-05
confidence: called   # from the code; no import was run and the user's files were not opened
---

# What a user can import

Files the user brings: price bars and signals for Charts, custom indicators, trade statements for the Journal. Nothing here comes from the network.

## Contents

**Charts: bars.** A CSV of OHLCV, charted as `CSV:<NAME>` on its own timeframe and any longer one, usable in backtests.
- Delimiter `,`, `;` or tab.
- Column names (`src/quant/server/Imports.cpp:89-96`), any case: time `time`, `timestamp`, `datetime`, `date`, `t`, `opentime`, `datetimeutc`, `dateutc`, `timeutc`, `ts`, `unix`, `barstart`; `open`, `o`, `openprice`; `high`, `h`, `highprice`; `low`, `l`, `lowprice`; close `close`, `c`, `closeprice`, `last`, `price`; volume `volume`, `v`, `vol`, `basevolume`; `symbol`, `ticker`.
- Only time and close are required. With no header it assumes time, open, high, low, close, then volume (`:198`).
- Time: `2026-10-14 14:30:00` (UTC), ISO `2026-10-14T14:30:00Z`, or Unix seconds (`:171`).
- Name: cleaned to `[A-Z0-9_]`, so `btc 1m (Alpaca).csv` gives `BTC_1M_ALPACA` (`:26`).
- Stored: `<AppData>/quant-terminal/data/imports/<NAME>.csv`.

**Charts: signals.** A CSV of time and position; becomes an indicator, so the tester and the quant checks run on it.
- Column `position`, `pos`, `signal`, `side`, `target`, `direction` or `value`.
- Values: `1`, `0`, `-1`, or `long`, `buy`, `short`, `sell`, `flat`, `none`, `exit`, `close` (`:277-278`).
- With no header: first column time, last column position (`:270`).
- Stored as `custom/signal-<name>.js`, up to 100 MB read (`src/quant/server/Server.cpp:538`).

**Charts: custom indicators.** JavaScript or Pine Script text, saved in `custom/<slug>.js` or `.pine` (`src/quant/server/Server.cpp:646-657`). Pine needs Node.js and runs through PineTS.

**Journal: statements** (`src/desk/Journal.h:38`, `:47`). Format and encoding detected: MT5 writes UTF-16.
- `MetaTrader 4 HTML`: "Closed Transactions" (`src/desk/Journal.cpp:153`).
- `MetaTrader 5 HTML`: section "Positions" (`:189`).
- `cTrader CSV`: told by columns `opening direction`, `closing quantity…` (`:525`).
- `CSV`: any other, columns matched by name, corrected by the user in the preview.

Column names accepted for CSV (`src/desk/Journal.cpp:36-50`):

| Field | Names |
|---|---|
| Ticket | `ticket`, `position`, `position id`, `id`, `order`, `deal`, `trade id`, `order id` |
| Symbol | `symbol`, `instrument`, `item`, `market`, `pair`, `asset` |
| Side | `type`, `side`, `direction`, `action`, `opening direction`, `position type`, `buy/sell` |
| Volume | `volume`, `lots`, `lot`, `size`, `quantity`, `closing quantity`, `opening quantity`, `qty`, `amount lots` |
| Open time | `open time`, `opening time`, `entry time`, `open date`, `opening date`, `time open`, `entry date`, `open` |
| Close time | `close time`, `closing time`, `exit time`, `close date`, `closing date`, `time close`, `exit date`, `close` |
| Open price | `open price`, `entry price`, `opening price`, `price open`, `entry` |
| Close price | `close price`, `exit price`, `closing price`, `price close`, `exit` |
| Stop loss | `s/l`, `sl`, `stop loss`, `stoploss`, `stop` |
| Take profit | `t/p`, `tp`, `take profit`, `takeprofit`, `target` |
| Commission | `commission`, `commissions`, `fees`, `fee` |
| Swap | `swap`, `swaps`, `rollover`, `financing` |
| Profit | `profit`, `p/l`, `pl`, `pnl`, `net…`, `gross…`, `realized pnl` |
| Comment | `comment`, `comments`, `label`, `note`, `notes`, `strategy`, `tag` |

Times: `2026.01.15 10:30:00`, `2026-01-15 10:30`, `15/01/2026 10:30:00.123`, read as UTC. Amounts: `1 234,56`, `1,234.56`, `-12.5`, `(12.50)`, `$1,234` (`src/desk/Journal.h:51-53`). A trade with a known ticket is replaced, so importing twice adds nothing.

Stored: `<AppData>/journal/journal.json`, see [deepwater](deepwater.md).

## How to call it
- Bars: `POST /api/import/bars?name=`, list `GET /api/imports`, signals `POST /api/import/signal?name=`, all on the app's internal server (`src/quant/server/Server.cpp:534-536`).
- Journal: `ImportDialog` `src/JournalPage.cpp:86`, `desk::readStatement` `:120`. Test hook `--journal-sample <file>` (`src/main.cpp:68`).

## What the app reads
Row `imports`, `src/SourcesPage.cpp:858`, covers bars only; the Journal's statements have no row. `Imports` `src/quant/server/Imports.h:53`; `parseBars`, `parseSignals`, `csvCandles` in `Imports.cpp`.

## Not read yet
No import of positions or holdings other than trades, of a watchlist file, or of a broker's live account: statements only ("no broker connection", `src/desk/Journal.h:11`). The Journal's `Comment` field goes to strategy and notes (not traced further).

## Calls made
No network.
