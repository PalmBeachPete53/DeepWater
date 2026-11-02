# Audit checklist: research.js, quantui.js, ptq.js (+ seas-*, fin-*, news, gold-*, ptq-* CSS)

Port: `src/quant/ui/Research.cpp`, `QuantUi.cpp`, `Ptq.cpp`. Status: **OK** (same as the JS),
**DIFF** (different, fixed unless noted), **MISSING** (fixed unless noted), **OTHER** (difference
outside these files, reported). How checked: C = code read side by side, E = exercised in both apps
(screenshots / text dumps, `qtm_panels_demo` vs the web app on a temp data dir, fake PTQ server).

## research.js: news (side tab)

| # | Item | Status | How |
|---|---|---|---|
| N1 | Header row: `<b>` ticker (spread ticker form via ticker()), spacer, `Refresh` small button | OK | C E |
| N2 | `Loading headlines…` note while fetching | OK | C |
| N3 | GET `/api/news?symbol=<encodeURIComponent(sym)>` | OK | C |
| N4 | Stale answer dropped when the active symbol changed meanwhile | OK | C |
| N5 | Note text = `r.note` (crypto: "Headlines mentioning BTC" / "Few headlines mention…"; US: "Headlines for X from Nasdaq" / "No headlines found for X"; others: "Headlines are available for crypto and US stocks."; offline: "") | OK | C E (BTC, NVDA, EURUSD) |
| N6 | Card per item: title (600, line-height 1.35), sub = source + ` · ` + (n.ago or ago(time)) only when time or ago, summary clamped to 2 lines, no summary line when empty | OK | C E |
| N7 | ago(t): "" when no t; `Nm ago` < 60; `Nh ago` (rounded) < 1440; else toLocaleDateString | OK | C |
| N8 | Card is a link (new window), hover border accent | OK | C |
| N9 | Error: note = `Headlines unavailable: <message>` | OK | C |
| N10 | Refresh re-renders (and re-fetches) | OK | C |

## research.js: seasonals (bottom tab)

| # | Item | Status | How |
|---|---|---|---|
| S1 | Layout `.seas`: grid 1.3fr / 1fr, left border-right, right column scrolls on its own | OK | C E |
| S2 | Title `<ticker> by calendar year`, `Years` field select 3/6/10/20 (default 6, current selected) | OK | C E |
| S3 | Years change re-renders with that count | OK | C E |
| S4 | `Loading daily history…` note; candles(sym, "1D", 8000) | OK | C |
| S5 | Candle error: note = message, nothing else | OK | C |
| S6 | Stale answer dropped (symbol changed / element gone) | OK | C |
| S7 | Note `N years of daily bars · average of M complete years` | OK | C E |
| S8 | Chart options: bg/text/grid/border colours from the theme, font 10, tick and crosshair time `Mon D` (UTC), price `+x.x%` | OK | C E |
| S9 | Last `Years` years, one line each: palette by index (10 colours), current UTC year in --text width 3, others width 1, no price line/last value/crosshair marker; duplicate day-of-year points dropped; time = 2000-01-01 + (doy-1) days | OK | C E |
| S10 | Average line #9598a1, width 2, dashed, only when average has points | OK | C E |
| S11 | fitContent | OK | C E |
| S12 | Legend: swatch + `YEAR <b class=up/down>fmtPct(total)</b>`, class from (total ?? 0) >= 0, fmtPct(null) = "—"; `Average of M years` | OK | C E |
| S13 | Legend overflow: chart keeps its height from before the legend was filled, the legend hangs below and the **panel scrolls** to it | DIFF (fixed: legend was clipped and unreachable) | E (probe of the DOM sizes) |
| S14 | Monthly table: header Year, Jan..Dec, Year; rows newest first; year cell bold, left | OK | C E |
| S15 | Cell: null -> muted `·`; else `+x.x`/`-x.x`, background rgba(8,153,129 / 242,54,69, 0.12 + 0.55·min(1,|v|/15)) | OK | C E |
| S16 | Partial years: missing months show `·` | OK | E (2016 AAPL row) |
| S17 | Summary rows (600, top border): Average (cell colouring of month avg, `·` when none) and `% of years up` (Math.round(upPct)%, `·` when none), empty last cells | OK | C E |
| S18 | Footer hint `Monthly moves in %. Past seasonality doesn't make next year's move.` (pad) | OK | C E |
| S19 | No daily history (empty candles): 0 years, empty chart, table with summary rows only | OK | C |

## research.js: financials (bottom tab)

| # | Item | Status | How |
|---|---|---|---|
| F1 | Non-US symbol: hint `Company financials are for US-listed stocks. Open one (AAPL, NVDA, JPM…) to see …` | OK | C E (EURUSD) |
| F2 | `Loading financials…`; GET `/api/financials?symbol=..&frequency=annual|quarterly` | OK | C E |
| F3 | Error: pad hint with the message (e.g. `Nasdaq has no financials for QQQ (ETFs and funds don't file them).`) | OK | C E |
| F4 | Stale answer dropped | OK | C |
| F5 | Left: `<ticker> key stats` h4 (margin-top 0), stats grid 2 columns gap 6/12, muted label over bold value | OK | C E |
| F6 | nextEarnings hint only when present | OK | C E |
| F7 | Bars only when a Total revenue row and an income table exist; net income = `^net income$` else first `net income` | OK | C E |
| F8 | toNum: strips $ , ( ), parseFloat semantics, "(…)" negative, non-finite -> null | OK | C |
| F9 | Bars oldest first; max = max(|rev|, 1); rev accent, net income up / down when < 0, min height 1px, radius 3 top; widths 38%, gap 3 | OK | C E |
| F10 | Under each: period and `$x.xB · $y.yB` (`—` for null), 10px, ellipsis; tooltips `Revenue $..`, `Net income $..` | OK | C E |
| F11 | Hint `Blue: revenue. Green: net income.` | OK | C E |
| F12 | Right: 4 tab buttons (Income statement, Balance sheet, Cash flow, Ratios), active primary, disabled when table missing | OK | C E |
| F13 | Annual/Quarterly select keeps the value, change re-renders with the same tab | OK | C E |
| F14 | Tab click re-renders with the same frequency | OK | C E |
| F15 | Table: empty first header, periods, rows label + values (escaped, wrap) | OK | C E |
| F16 | Missing table: `Not available for this company.` | OK | C |
| F17 | Footer `<note> Source: Nasdaq.` | OK | C E |

## quantui.js: gold quant checks box

| # | Item | Status | How |
|---|---|---|---|
| Q1 | Gold box (1.5px gold border, radius 8, panel bg, 3px gold-soft ring), badge `Bonus` + `Quant checks` | OK | C E |
| Q2 | Intro hint text | OK | C E |
| Q3 | `Run quant checks` gold button -> disabled `Checking…` -> after 30 ms runs quantChecks(bars, strat, {subBars}) -> `Run again`, enabled | OK | C E |
| Q4 | Error: `<p class="down">message</p>` | OK | C |
| Q5 | Verdict pill `Passed P of T checks`, class up (all) / warn (>= half, #f7a600) / down | OK | C E |
| Q6 | Verdict sentence: all / none / some | OK | C E |
| Q7 | Gates table: bold label, muted value, Pass (up) / Fail (down) | OK | C E (numbers equal to the JS) |
| Q8 | Walk-forward: h4, hint `… N of M stretches made money.`, table Stretch (`Mon YYYY to Mon YYYY`, local time) / Settings picked (`a / b` or `as set`) / Return (fmtPct, up/down) / Sharpe (toFixed 2) | OK | C E |
| Q9 | Nearby settings, 2 tunables: hint, heat table `A \ B` header, row values bold, cells fmtPct(netPct ?? 0) with colour alpha 0.12+0.55·min(1,|n|/100), base cell outlined gold 2px, bold | OK | C E |
| Q10 | Nearby settings, 1 tunable: header values with ` (yours)`, `Net return` row up/down | OK | C |
| Q11 | Monte Carlo sentence: trades, runs (toLocaleString), p5/p50/p95 fmtPct, ddP50/ddP95 toFixed(1), lossProb toFixed(0) | OK | C E (numbers equal) |
| Q12 | No tunables: hint `Walk-forward, overfitting and nearby settings need a whole-number setting…` | OK | C |
| Q13 | Footer `Probabilistic Sharpe x.xxx` (`n/a` when null), trials, sr0 toFixed(2), disclaimer | OK | C E |

## ptq.js: status, "Check with PTQ" box

| # | Item | Status | How |
|---|---|---|---|
| P1 | ptqStatus cache 15 s; error -> {connected:false, reason} | OK | C |
| P2 | Not connected box (split, 2 columns from 760px): head (logo, PART-TIME QUANT), title `Trade it for real.`, lead, `Trade on PTQ ↗` (site link), `New to PTQ? Start here.`, `I already have PTQ` + `Connect your PTQ account to this tool.`, steps 01/02/03 | OK | C E |
| P3 | Retry: force status, re-mount with tried -> `Not found. Open PTQ on this computer, then try again.` | OK | C E |
| P4 | Connected box: status `PTQ <version> connected` (+ ` · no licence found on this computer` when no lease), lead with ticker + tf, `Check with PTQ` | OK | C E |
| P5 | Click: disabled `PTQ is testing…`, hint `This can take up to a minute…`, POST /api/ptq/check ctx | OK | C E |
| P6 | Done (ok or error): enabled `Check again` | OK | C E |
| P7 | Error line `<p class="down">` in the brand box (= brand body colour): out of credits `PTQ couldn't charge this run: …`, engine 422 `Backtest refused: its engine can't run these settings.`, 500 (`engine crashed: …`), no licence `PTQ needs your licence…`, expired `Your PTQ licence lease has run out…`, PTQ down `PTQ isn't running on this computer…` | OK | C E |
| P8 | Simulated preview warning when provenance != real-engine | OK | C E |
| P9 | Verdict pill approve/reduce/block/none (`No verdict`) with brand colours | OK | C E |
| P10 | Gates table, else validationError line | OK | C E |
| P11 | Stats: PTQ net return (`+x%`/`—`), Sharpe (`—`), max drawdown (`-x%`/`—`), trades | OK | C E |
| P12 | `PTQ tested <instrument> on <timeframe> …`; notes as hints | OK | C E |
| P13 | esc(null/undefined) is "" (gate label/value, instrument, timeframe, notes, strategy name, arm name `strategyName \|\| strategyId`), join() writes null symbols as "" | DIFF (fixed: printed "null"/"undefined") | C |
| P14 | `Saved in your PTQ as <b>name</b>. Open it in PTQ` link | OK | C E |
| P15 | block: `PTQ wouldn't trade this as it stands…`; else if validation: arm box (3 steps, Integrations link, command `ptq arm-strategy <id> --max-notional 100` + Copy -> `Copied`, hint with code spans, Autopilot link, `Live trading in PTQ is new…`) | OK | C E |
| P16 | No validation object: neither block hint nor arm box | OK | C |

## ptq.js: PTQ tab

| # | Item | Status | How |
|---|---|---|---|
| T1 | Status fetched (forced) before drawing; old content stays meanwhile | OK | C |
| T2 | Not connected: dark ground, centred box max 1040, retry -> tried text | OK | C E |
| T3 | Overview error: pad down line | OK | C |
| T4 | Head: logo, status `<version> connected · <url>` (+ ` · no licence on this computer`), Refresh ghost pill, `Open PTQ` pill (links.app) | OK | C E |
| T5 | Trading: engaged -> `Stopped. PTQ's stop is on…`; else autopilot running/paused (+ ` (unattended live is switched off)` when === false), `Stop all PTQ trading`, hint | OK | C E |
| T6 | Kill: confirm text, POST /api/ptq/kill-switch {}, toast `PTQ stopped. No new orders.` / error toast, re-render | OK | C E |
| T7 | errLine for killSwitch, portfolio, arms (object with error) | OK | C E |
| T8 | Account: connected -> Equity, Cash, Today (`—`), Exposure (`x%`/`—`), `From <source or your broker>.`; else message or `No exchange connected in PTQ yet.` + `Connect one in PTQ` link | OK | C E |
| T9 | Positions: instrument or symbol, qty fmtNum 4, entry avgPrice ?? entry ?? avgEntryPrice, value marketValue ?? value, P&L up/down from unrealizedPnl ?? pnl ?? 0; empty `No open positions.` | OK | C E |
| T10 | Armed strategies: name or id, ` halted: reason` (down), markets joined, max per order fmtNum 2, Yes / No (unattended), expires toLocaleString or `—`, Disarm (danger) or muted `disarmed`; empty text | OK | C E |
| T11 | Disarm: confirm, POST /api/ptq/arms/<encoded id>/disarm, toast `Disarmed.` / error, re-render | OK | C E |
| T12 | Recent PTQ orders (max 25): time, market, side, qty filledQty ?? qty, price fillPrice ?? limitPrice, status (+ ` (simulated)` / ` (live)`), rejected in down; empty `No orders yet.` | OK | C E |
| T13 | Auto refresh every 15 s while the tab is shown | OK | C |
| T14 | Brand CSS: ground glows, pills, h4 mono uppercase, table header mono, stat cards, code spans, links #6cb8ff | OK | C E |

## Summary

89 items: 87 OK, 2 DIFF (S13, P13), both fixed in `Research.cpp` / `Ptq.cpp`. 0 MISSING.
Numbers checked equal to the JS: seasonal table and legend, financial statements/bars, every quant
gate value, walk-forward folds, heat table, Monte Carlo sentence, PTQ requests (same payloads seen by
the fake PTQ for check, kill switch and disarm).

## Differences outside these files (reported, not fixed)

- PanelsKit `Table` sticky header: in the browser a sticky `thead` sticks at the scroller's content
  edge (10px under the top of `.tester .right`, rows show through the padding above it); the port
  sticks it at the very top. Seen in Financials and in the PTQ tab's tables when scrolled.
- PanelsKit fonts: weight 600 on the fallback fonts maps to Helvetica Neue Medium in Qt, the browser
  uses Bold (PTQ stat values, step titles, `Trade it`, pill labels): the port looks lighter.
- Chart engine: no TradingView attribution logo (the web charts, including the seasonals chart, show
  it bottom-left). Probably deliberate; noted for the chart owners.
- Text wrapping/column widths differ by 1-2px where the fallback fonts' metrics differ (accepted).
