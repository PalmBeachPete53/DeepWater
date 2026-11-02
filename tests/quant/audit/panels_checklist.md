# Audit: public/js/panels.js -> src/quant/ui/Panels*.cpp, RuleBuilder.*, PanelsKit.*, PanelsStyle.cpp

Method: line-by-line read of panels.js against the C++; exercised with `tests/quant/panels`
(qtm_panels_demo) and the reference web app (node server.js + tests/quant/tools/webshot.mjs), screenshots
compared side by side; numbers compared (tester stats and trades, paper account after a scripted replay
session, JSON.parse error messages on 3055 generated inputs).

Status: OK = same behaviour/output; FIXED = was DIFFERENT/MISSING, fixed in this audit;
DIFFERENT = remaining difference (reason given); OTHER = belongs to another area (listed in the report).

## 1. Operand / rule builder (RuleBuilder.cpp)

| # | Item | Status | Note |
|---|---|---|---|
| 1 | PRICE list close, open, high, low, volume, hl2, hlc3, ohlc4 | OK | same order |
| 2 | OP_LABEL texts (is above, is below, is at or above, is at or below, crosses above, crosses below, crosses) | OK | |
| 3 | outputsOf: outputs + "direction" for supertrend | OK | |
| 4 | mainInput: first int/float input | OK | |
| 5 | operandHtml kind value l / v / p:field / i:id | OK | |
| 6 | "Drawn line" option first, only for line operands, label or "Drawn line" | OK | |
| 7 | optgroup "Price", "Price (close)" label for close, others raw | OK | |
| 8 | "Value" option between the groups | OK | |
| 9 | optgroup "Indicators": indicators with outputs (custom included), def.name | OK | |
| 10 | value input (class num, width 70, step any), value `o.v ?? 0` | OK | |
| 11 | main parameter input, title = input label, value params[key] ?? default | OK | |
| 12 | output select only when > 1 outputs, selected output | OK | |
| 13 | .rule select/input max-width 150, height 26 | OK | screenshots |
| 14 | readOperand line -> {kind line, line, label} | OK | |
| 15 | readOperand value `Number(value || 0)` (empty/invalid -> 0) | OK | Input::number |
| 16 | readOperand price | OK | |
| 17 | readOperand ind: params only the main input (Number(""): 0), output or first output | OK | |
| 18 | ruleHtml: operand, op select (OPS order), operand, x button title "Remove" | OK | |
| 19 | bindRules: kind change rebuilds the operand (value 0 / price / ind with first output, params {}) | OK | |
| 20 | bindRules: x removes the rule | OK | |
| 21 | readRules order | OK | |
| 22 | compileRules key `${id}_${values.join("_")}` with [^a-z0-9_] -> _ | OK | |
| 23 | compileRules ref: line -> {line}, value -> Number, price -> field, ind -> key.output | OK | |
| 24 | compileRules indicators[key] = {id, params}, JS key order kept for the JSON modal | OK | indicatorOrder |
| 25 | toOperand: {line} object, number, price field, key.output, unknown -> value 0 | OK | |
| 26 | toOperand output: out || s.output || first output | OK | |
| 27 | defaultRule close crossesAbove EMA 50 value | OK | |
| 28 | .rule box: panel bg, border, radius 6, padding 5, gap 4, wrap | OK | screenshots |
| 29 | .rules column gap 6, margin 4px 0 10px | OK | |

## 2. Tabs and frames (Panels.cpp)

| # | Item | Status | Note |
|---|---|---|---|
| 30 | side tabs Watchlist, Alerts, Calendar, News | OK | |
| 31 | bottom tabs Strategy tester, Screener, Heatmap, Seasonals, Financials, Paper trading, Indicator editor, PTQ | OK | |
| 32 | side tab click -> showSide (active class, render) | OK | |
| 33 | bottom tab click -> showBottom(t, toggle): same open tab collapses | OK | |
| 34 | #bottomToggle toggles collapsed, renders on expand; title "Show / hide panel" | OK | |
| 35 | chevron rotated when collapsed | OK | |
| 36 | bottom starts collapsed, tester tab active | OK | |
| 37 | .bottom height 320 / collapsed 39 | OK | |
| 38 | .side width 300, border-left | OK | |
| 39 | @media (max-width: 900px): .side hidden, .bottom height 45% | FIXED | was missing; Panels::Private::applySizes |
| 40 | renderSide dispatch (news -> research) | OK | |
| 41 | renderBottom dispatch only when expanded | OK | |
| 42 | initial showSide("watchlist") | OK | |
| 43 | hooks: showBottom, openAlertDialog exported | OK | |
| 44 | onActiveChange: drawRows on watchlist tab | OK | |
| 45 | onActiveChange: symbol change -> news / seasonals / financials re-render | OK | |
| 46 | onActiveChange: paper tab re-render | OK | |
| 47 | onAlertFired: alerts tab re-render | OK | |
| 48 | onTheme: renderBottom | OK | C++ also re-renders the side panel (colours are baked into widgets; CSS variables do it in the browser) |
| 49 | onReplayBar: only active pane, checkReplayOrders, paper re-render throttled 250 ms | OK | |
| 50 | onReplayChange: paper re-render | OK | |

## 3. Watchlist

| # | Item | Status | Note |
|---|---|---|---|
| 51 | list select with lists in insertion order, active selected | OK | OrderedMap |
| 52 | "+ List" (title "New list"), "Delete" (title "Delete this list") | OK | |
| 53 | input placeholder "Add symbol (Enter)" | OK | |
| 54 | list change -> activeWatchlist, persist, render, refreshQuotes | OK | |
| 55 | + List: prompt "Name for the new list:", ignore empty/existing, add + activate + persist | OK | tested (promptnext) |
| 56 | Delete: < 2 lists -> toast "Keep at least one list." | OK | tested |
| 57 | Delete: confirm `Delete the list "X"?`, delete, first list active, persist | OK | tested |
| 58 | Enter: trim, /api/resolve, add id if new, clear input, persist, drawRows, refreshQuotes | OK | tested (aapl -> US:AAPL) |
| 59 | Enter error -> toast message err | OK | |
| 60 | row click -> setSymbol | OK | |
| 61 | x click -> remove from list, persist, drawRows | OK | |
| 62 | row: ticker 600 + exchange 11px muted, ellipsis | OK | screenshot |
| 63 | price = app.prices[s] ?? quote price, fmtPrice | OK | |
| 64 | change fmtPct, up/down class | OK | |
| 65 | current row accent-soft, hover background, x visible on hover only | OK | |
| 66 | empty: "Empty list. Add a symbol above." | OK | |
| 67 | refreshQuotes: watchlist + paper symbols, deduped, skip when none | OK | |
| 68 | quotes merged, onPrice for truthy prices, drawRows on watchlist tab | OK | |
| 69 | quote errors ignored (offline) | OK | |
| 70 | refresh every 5000 ms and at start | OK | |

## 4. Alerts

| # | Item | Status | Note |
|---|---|---|---|
| 71 | "New alert" (primary) opens the dialog | OK | |
| 72 | "Desktop notifications" only when supported and not granted | OK | macOS notifications |
| 73 | permission click -> request, re-render | OK | |
| 74 | GET /api/alerts and /api/alerts/log together, errors -> empty lists | OK | |
| 75 | card title: "Watchlist: NAME" or ticker, muted tf | OK | screenshot |
| 76 | describeAlert line | OK | "MACD 12 hist crosses above 79,745.20" |
| 77 | sub: Active/Stopped · trigger text (only once / once per bar / once per bar close / raw) · webhook · fired Nx | OK | |
| 78 | Stop / Restart button, Delete (danger) | OK | tested |
| 79 | toggle PATCH {active: !active, lastBar: null, lastBars: {}} then re-render | OK | tested |
| 80 | delete DELETE then re-render | OK | tested |
| 81 | empty list text "No alerts yet. They run inside the app, ..." | OK | |
| 82 | heading "Recent" | OK | |
| 83 | log: first 30, text + date toLocaleString + " · webhook URL" | OK | |
| 84 | empty log "Nothing has fired yet." | OK | |
| 85 | failed toggle/delete: JS leaves the list (unhandled rejection), C++ re-renders | DIFFERENT | backend is in-process, cannot fail like a down server; harmless |
| 86 | describeAlert: message wins | OK | |
| 87 | describeAlert: number -> fmtPrice, close -> "Price", other price fields raw | OK | |
| 88 | describeAlert: indicator name without " (...)" suffix + param values + output when > 1 outputs | OK | |
| 89 | describeAlert: param values in JS key order | FIXED | QJsonObject sorted them (MACD 12 9 26); now the indicator's input order (12 26 9) |
| 90 | describeAlert: unknown key -> raw string | OK | |
| 91 | describeAlert: line operand -> "your drawn line" | OK | |
| 92 | describeAlert: conditions or legacy left/op/right, joined " and " / " or " (match any) | OK | |
| 93 | dialog title "Create alert", wide | OK | screenshot |
| 94 | default rule close crossesAbove Number(price.toPrecision(6)) of the last shown close (0 without chart) | OK | |
| 95 | preset.rules / preset.rule / preset.symbol | OK | |
| 96 | Symbol (preset or active symbol), Check select (Just this symbol / Every symbol on LIST), Timeframe (active tf or 1h) | OK | |
| 97 | h4 "Conditions", rules box | OK | |
| 98 | "+ Condition": max 5 -> toast "An alert can have up to 5 conditions." | OK | tested |
| 99 | added rule close > value | OK | |
| 100 | "Fire when" all are true / any is true, hint "Up to 5." | OK | |
| 101 | Trigger Only once / Once per bar / Once per bar close | OK | |
| 102 | Message (optional) placeholder "BTC broke out" | OK | |
| 103 | h4 "Also send to (optional)", webhook placeholder, Test button | OK | |
| 104 | Telegram hint text, "checked every 15 seconds" | OK | |
| 105 | Test: POST /api/webhook-test {url}; "Test message sent." / "Webhook replied HTTP N" err / error toast | OK | |
| 106 | Create: no rules -> "Add a condition." err | OK | |
| 107 | Create body: symbol, watchlist (only when set), tf, left/op/right of rule 0, conditions, match, indicators, trigger, message, webhook | OK | |
| 108 | Create ok: close modal, toast "Alert created.", ask permission when default, showSide("alerts") | OK | tested |
| 109 | Create error -> toast err | OK | server message shown (tested with an invalid op) |

## 5. Calendar

| # | Item | Status | Note |
|---|---|---|---|
| 110 | view select Economic / Earnings / Dividends, remembered | OK | |
| 111 | economic: filter All / Medium + high / High impact, default High | OK | |
| 112 | earnings/dividends: "My watchlists" checkbox (hint-text) | OK | |
| 113 | "Loading…" first | OK | |
| 114 | economic: GET /api/calendar, keep by filter | OK | screenshot identical |
| 115 | day headings toLocaleDateString weekday long, day numeric, month short | OK | "WEDNESDAY, OCT 28" |
| 116 | card: bold country + title, impact-X colour, time 2-digit, forecast / previous | OK | |
| 117 | economic empty "Nothing at this impact level this week." | OK | |
| 118 | earnings/dividends: GET /api/VIEW?days=5, mine filter | OK | |
| 119 | sort date, watchlist symbols first, market cap desc | OK | |
| 120 | first 250 rows | OK | |
| 121 | day heading from `${date}T12:00:00Z` | OK | |
| 122 | watchlist dot (up colour) | OK | |
| 123 | dot title "On your watchlist" | FIXED | tooltip was missing |
| 124 | name first 34 chars, muted | OK | |
| 125 | earnings sub: when · EPS forecast · last year · cap fmtNum | OK | |
| 126 | dividends sub: fmtPrice(amount) a share · paid date · fmtPrice(annual) a year | OK | |
| 127 | row click -> setSymbol, pointer cursor | OK | |
| 128 | empty: "None of your watchlist symbols in the next five trading days." / "Nothing listed for the next five trading days." | OK | |
| 129 | error: "Calendar unavailable: MSG" | OK | identical (HTTP 429 seen in both) |

## 6. Strategy tester (PanelsTester.cpp)

| # | Item | Status | Note |
|---|---|---|---|
| 130 | tState settings defaults (direction long ... intrabar worst), JS key order | OK | |
| 131 | starts with "EMA cross (9/21)" | OK | |
| 132 | strategyToUi groups via toOperand | OK | |
| 133 | strategyToUi SL/TP/trailing/maxBars `s[k] || 0`, exitMatch any/all | OK | |
| 134 | readTester: groups from the UI, data-s inputs (number -> Number, select -> value) | OK | |
| 135 | readTester result {indicators, groups, ...settings} in JS order | OK | JSON modal identical |
| 136 | "Load a strategy…" select, optgroup Examples, optgroup Saved (only when saved exist), insertion order | OK | |
| 137 | "Run on chart" (primary) | OK | |
| 138 | hint "Entry conditions must all be true. ..." | OK | |
| 139 | groups: "Buy (enter long) when", "Sell (exit long) when", "Enter short when", "Exit short when" + "+ Condition" | OK | |
| 140 | + Condition adds defaultRule | OK | |
| 141 | "Exits": Exit when select (all exit conditions are true / any one exit condition is true), "Exit after this many bars (0 = off)" | OK | |
| 142 | "Risk": Stop loss %, Take profit %, Trailing stop % | OK | |
| 143 | "Money": Starting capital, Position size %, Fee % per side, Slippage % per side, Spread % (bid to ask), "Live spread" (title) | OK | |
| 144 | "Fills inside a bar": Stops and targets select (3 texts) | OK | |
| 145 | "Your own data": Import bars (CSV), Import signal (CSV), hint text | OK | |
| 146 | "Save strategy", "Edit as JSON" | OK | |
| 147 | right: "Pick or build a strategy, then press <b>Run on chart</b>. ..." | OK | |
| 148 | preset change: strategyToUi, name, money keys when not null, re-render | OK | tested Bollinger, MACD + 200 EMA: stats identical |
| 149 | file picker accepts .csv,.txt | OK | |
| 150 | import toast "Reading NAME…" | OK | tested |
| 151 | POST /api/import/KIND?name=, error message or HTTP N | OK | |
| 152 | bars: switch tf up to the file's tf, setSymbol, toast "Imported N TF bars as SYM (skipped N rows with no price). Pick any timeframe from TF up." | OK | tested |
| 153 | signal: importCustom, strategyToUi, name "Signal NAME", toast "Loaded your signal NAME: N position changes. ..." | OK | tested; run on the bars works |
| 154 | Live spread: GET /api/spread, field = Number(toPrecision(2)), toast "TICKER on SOURCE: bid X, ask Y. Spread set to Z%." | OK | tested |
| 155 | Live spread error toast | OK | |
| 156 | Save: prompt "Name this strategy:", app.state.strategies[name] = readTester(), persist, toast `Saved "NAME".` | OK | tested |
| 157 | Saved strategy appears in Saved group on next render, loads | OK | tested |
| 158 | JSON modal "Strategy JSON", hint, textarea rows 22 with JSON.stringify(readTester(), null, 2), Apply | OK | screenshot identical |
| 159 | Apply: strategyToUi + money keys, close, re-render | OK | |
| 160 | Apply error toast "Invalid JSON: " + V8's JSON.parse message | FIXED | was Qt's parser text ("unterminated object at position N"); V8 messages reproduced, 3055 generated inputs identical to node |
| 161 | Apply with `null` -> "Invalid JSON: Cannot read properties of null (reading 'indicators')" | FIXED | |
| 162 | Apply with a number/string/array -> empty strategy | FIXED | was rejected as invalid by QJsonDocument |
| 163 | showResult on render when there is a last result | OK | |
| 164 | run: "The active chart has no data yet." | OK | |
| 165 | run: "Add at least one entry condition." | OK | tested |
| 166 | run in replay uses bars up to the replay index | OK | |
| 167 | magnifier: no finer tf -> toast "This chart is already as fine as the bar magnifier goes, so it uses the OHLC path." | OK | tested on 1m |
| 168 | magnifier: toast "Loading TF bars for the bar magnifier…", candles span/seconds + 10, filtered to the range | OK | tested |
| 169 | backtest(bars, strat, {subBars}) | OK | stats/trades identical to web |
| 170 | chart marks entry: below/above bar, arrowUp/Down, #2962ff/#ff6d00, Buy/Short | OK | screenshot |
| 171 | chart marks exit: circle, #089981/#f23645, +x.x% | OK | |
| 172 | errors -> toast err | OK | |
| 173 | header: TICKER TF, "N bars · date to date · fees · fills" | OK | |
| 174 | Export trades (CSV) -> TICKER-TF-trades.csv with tradesCsv | OK | |
| 175 | Clear chart marks on the active pane | OK | |
| 176 | stats Net profit, Buy and hold, Max drawdown, Trades, Win rate, Profit factor, Sharpe, Sortino, Avg trade, Time in market | OK | values identical |
| 177 | pc(): + sign for >= 0, — for null; up/down classes (null counts as up) | OK | |
| 178 | equity chart: area #2962ff (.3 -> 0), width 2, "Strategy"; buy & hold #787b86, 1px, "Buy & hold", no price line | OK | |
| 179 | equity box 150px, flex-shrunk in the column | OK | screenshot |
| 180 | quant checks mount, PTQ check mount with name tState.name or "TICKER TF" | OK | (contents: QuantUi/Ptq areas) |
| 181 | trades table columns Side, Entry, Exit, Entry price, Exit price, P&L %, Bars, Exit reason | OK | |
| 182 | trades newest first, max 300, toFixed(2)%, up/down | OK | |
| 183 | trades table wraps (.tester .right td white-space normal) | OK | |

## 7. Screener (PanelsMarket.cpp)

| # | Item | Status | Note |
|---|---|---|---|
| 184 | state defaults crypto, sort volume desc, rule RSI 14 < 30, tf 1D | OK | |
| 185 | Market select Crypto (USDT pairs) / US stocks / US ETFs | OK | |
| 186 | Search (120px), Price from/to (80), Change % from/to (70), Min volume (100), Min market cap (110, not crypto) | OK | screenshot |
| 187 | Sector select (All + sorted sectors) only when > 1 sector | OK | |
| 188 | Refresh | OK | |
| 189 | "Technical filter:" one rule without x, "On" 1h/4h/1D/1W, "Scan top 100", "Clear scan" when scanned | OK | |
| 190 | universe change resets filters, re-render, reload | OK | |
| 191 | inputs: trimmed, "" -> null, numbers via Number | OK | |
| 192 | loadScreener: "Loading…", GET /api/screener?universe=, clears scan, error toast + empty rows | OK | |
| 193 | filtered(): text, price, change, volume, cap (|| 0), sector, scan pass | OK | |
| 194 | sort: strings localeCompare, numbers (|| 0), stable | OK | |
| 195 | scan: rule + tf stored, targets = filtered without scan, by volume, 100 | OK | |
| 196 | scan button "Scanning N…" | OK | |
| 197 | POST /api/scan {symbols, tf, indicators, rules}, toast "H of N match.", error toast, re-render | OK | |
| 198 | scan result arriving after leaving the tab | DIFFERENT | JS re-renders the screener into whatever tab is open (a glitch); C++ drops it. Kept |
| 199 | columns Symbol, Name, Price, Change %, Volume (USDT)/Volume, Market cap, Sector (not crypto) | OK | screenshot identical |
| 200 | header arrows ↓/↑, click sorts (same column flips, new column desc) | OK | |
| 201 | name 40 chars muted, l columns | OK | |
| 202 | 500 rows max, "N match(es) (showing 500). Click a row to open its chart." + live price note (not crypto) | OK | |
| 203 | row click -> setSymbol | OK | |

## 8. Heatmap

| # | Item | Status | Note |
|---|---|---|---|
| 204 | Market US stocks (200 largest) / Crypto (100 most traded); Size by Market cap / Volume | OK | |
| 205 | hint "Colour = today's change. Click a tile to open its chart." | OK | |
| 206 | wrap height calc(100% - 64px) | OK | |
| 207 | universe change -> re-render + reload; size change -> redraw only | OK | |
| 208 | US: marketCap > 0, cap desc, 200; crypto: volume desc, 100 | OK | |
| 209 | load error -> toast | OK | |
| 210 | a new canvas is blank until its data is drawn (no stale tiles while loading or after an error) | FIXED | C++ painted the previous universe's rows at once |
| 211 | stale response from the previous universe | DIFFERENT | JS can overwrite the new universe's rows (race); C++ drops the old request with its widget. Kept |
| 212 | squarify (worst ratio, rows along the short side) | OK | tile layout identical |
| 213 | colour: cap 3 (US) / 8, base 66,69,80 -> up 8,153,129 / down 242,54,69, Math.round | OK | |
| 214 | size value max(1, volume*price (US) / volume, or marketCap || volume) | OK | |
| 215 | US sectors grouped ("Other"), group panel bg, uppercase label 600 10px when w > 50, w/7 chars | OK | |
| 216 | inner tiles inset 1/14/2/15 | OK | |
| 217 | tile fill inset .5, labels when w > 34 and h > 22, font max(10, min(22, sqrt(wh)/5)), ticker without USDT, change line when h > 40 | OK | |
| 218 | tooltip: bold ticker, name, price, change (up/down), "Market cap X" | OK | |
| 219 | tooltip position cursor + 14, removed on leave / off tiles | OK | |
| 220 | click tile -> setSymbol | OK | |
| 221 | redraw on resize | OK | |

## 9. Paper trading (PanelsPaper.cpp)

| # | Item | Status | Note |
|---|---|---|---|
| 222 | live account app.state.paper created on first use {initial 100000, cash, feePct 0.05, positions, orders, history} | OK | |
| 223 | replay account app.state.replayPaper while the active chart replays | OK | tested |
| 224 | price(): replay bar close for the replay symbol, null for others in replay, else app.prices | OK | |
| 225 | paperSymbols(): live positions + orders | OK | |
| 226 | fill: fee, realized on closing part, avg rules (new / flip / add / reduce), cash, history unshift (replay bar time), 500 max | OK | numbers identical to web |
| 227 | positions keep JS insertion order | OK | positionOrder kept beside positions |
| 228 | fill toast "Paper SIDE QTY TICKER at PRICE" | OK | |
| 229 | checkOrders on every live price: limit/stop hit rules, OCO siblings removed, fill at limit price or market | OK | |
| 230 | checkOrders re-renders the paper tab | OK | |
| 231 | price listener redraws numbers for held symbols | OK | |
| 232 | equity = cash + qty * (price ?? avg) | OK | |
| 233 | stats Equity, Cash, Total P&L (value (pct)) up/down | OK | identical |
| 234 | positions columns, Long/Short, qty 4 dp, P&L value (pct * sign), Close button | OK | identical |
| 235 | "No open positions." | OK | |
| 236 | order ticket title "Order ticket · TICKER price" | OK | |
| 237 | Type Market/Limit/Stop, Price (toPrecision 6), Amount ($) 1000, Stop loss % / Take profit % (optional) | OK | |
| 238 | Buy (up colour), Sell / short (down colour) | OK | |
| 239 | Account: Starting balance, Fee % per trade, Reset account (danger) | OK | |
| 240 | hint: replay text / live text | OK | |
| 241 | Working orders table, "(bracket)", Cancel, "None." | OK | |
| 242 | History first 100: time, symbol, side, qty, price, realised (blank when 0) | OK | |
| 243 | place: price ?? last bar close ?? quote; "Need a live price and an amount." | OK | |
| 244 | market: fill + bracket | OK | tested |
| 245 | limit/stop: "Set a price for the order.", order qty amt/entry, bracket, toast "TYPE SIDE order placed." | OK | tested |
| 246 | bracket: stop entry*(1 -/+ sl%), limit entry*(1 +/- tp%), OCO id only when both | OK | tested (stop loss filled, take profit cancelled) |
| 247 | after placing: persist, render, refreshQuotes | OK | |
| 248 | Reset: confirm text, initial/cash = Number || 100000, fee Number || 0, live or replay account | OK | |
| 249 | Fee change -> account feePct, persist | OK | |
| 250 | Close: fill opposite side at price ?? avg with note "closed", drop that symbol's orders | OK | |
| 251 | Cancel: drop order by id | OK | |
| 252 | checkReplayOrders: bar range rules, fill price min/max with open, OCO | OK | identical to web after 30 steps |

## 10. Indicator editor (PanelsEditor.cpp)

| # | Item | Status | Note |
|---|---|---|---|
| 253 | TEMPLATE text | OK | identical |
| 254 | GET /api/custom before rendering, error -> empty | OK | |
| 255 | "New indicator" (primary), h4 "Your indicators", list | OK | |
| 256 | card: file name, Edit, Add to chart, Delete (danger), error (pre-line) | OK | |
| 257 | empty "None yet." | OK | |
| 258 | hint with <code> spans | OK | |
| 259 | name input placeholder "Indicator name", Save, "Save and add to chart" (primary) | OK | |
| 260 | textarea starts with TEMPLATE, name "EMA ribbon squeeze" | OK | |
| 261 | Tab inserts two spaces | OK | |
| 262 | Shift+Tab inserts two spaces too (e.key is "Tab") | FIXED | Qt reports it as Backtab |
| 263 | New indicator: template with 'My indicator', name "My indicator" | OK | tested |
| 264 | save: clear error, name trim || "indicator", POST /api/custom | OK | tested |
| 265 | save: importCustom, "Saved, but the browser could not load it. Check the code." | OK | |
| 266 | save ok: toast "Saved FILE", add to active chart or re-render panes, edFile, list refresh | OK | tested |
| 267 | save error -> red text under the code | OK | tested |
| 268 | Edit loads the file, name from file (no .js, - -> space) | OK | tested |
| 269 | Add to chart: importCustom + addIndicator | OK | |
| 270 | Delete: confirm "Delete FILE?", DELETE, re-render | OK | tested |

## 11. CSS used by the panels

| # | Item | Status | Note |
|---|---|---|---|
| 271 | .pad 10px 12px, .row-flex gap 8 wrap | OK | |
| 272 | .card border, radius 6, padding 8 10, margin 8, panel bg; .top; .sub 11px muted | OK | |
| 273 | .impact-High/Medium/Low/Holiday colours | OK | |
| 274 | .stats grid minmax(130px, 1fr) gap 8; .stat; .k 11px; .v 16px 600 tabular | OK | |
| 275 | .tester grid minmax(330px, 420px) 1fr, left/right padding, right flex gap 10 | OK | |
| 276 | h4 12px 600 muted uppercase .04em, margin 10 0 4 | OK | |
| 277 | .equity 150, border, radius 6 | OK | |
| 278 | .tip panel bg, border, radius 6, padding 6 9, 12px | OK | |
| 279 | .form-grid 2 columns gap 10 | OK | |
| 280 | table.t, .l, row hover | OK | |
| 281 | .wl-head / .wl-row grid | OK | |
| 282 | light theme | OK | screenshots identical |
