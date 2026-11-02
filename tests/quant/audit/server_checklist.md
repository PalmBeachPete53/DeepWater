# Server audit checklist: quant-terminal/server.js + lib/*.js vs src/quant/server

Status: **OK** (same behaviour, checked by reading both sides and by tests/live diff), **FIXED**
(was DIFFERENT or MISSING, fixed in this audit, regression test added where testable), **DIFF-OK**
(differs on purpose or only in a way nobody can observe; reason given), **N/A** (no counterpart in
the port by design). "live" = same request sent to `node server.js` and `qtm_server_run` and the
JSON diffed (tools: scratch `cmp.py`; bars compared except the last two, which move).

Live runs on 29 Oct 2026: 24 Binance tf cases, 112 stock/FX/futures/spread/crypto-fallback cases,
46 resolve cases, 19 search queries, quotes, spreads, screener (crypto 517 rows, stocks 6598, etf
5539: same rows, same order, same fields), calendar, earnings, dividends, news (US + crypto),
financials (annual, quarterly, ETF 404, crypto 422), probe, indicators, state, alerts (35 add
cases, 9 patch cases, delete, evaluation with 13 alerts incl. watchlist and drawn lines, webhooks
to a local receiver incl. Discord/Slack/Telegram host routing), imports, custom, scan, backtest,
webhook-test, PTQ (not running), HTTP layer. Nasdaq is flaky from here (3-8 s per request, the
odd 10 s timeout on either side, which then skips Nasdaq for a minute); Yahoo answered throughout
(no 429 seen); faireconomy answered 429 to the probe on both sides.

Totals: 353 items; 319 OK, 26 FIXED (rows marked FIXED, alone or next to OK), 4 DIFF-OK, 4 N/A.
Regression tests for the fixes: tests/quant/server/test_server.cpp, the `testAudit*` functions.

## 1. server.js: process, helpers, HTTP layer

| # | Item | Status |
|---|---|---|
| 1.1 | `VERSION` from package.json ("1.0.0") in health and start-up line | OK |
| 1.2 | `HOST` env (default 127.0.0.1); LAN mode when HOST is something else | DIFF-OK: the port binds 127.0.0.1 only (deepwater is a desktop app; no LAN exposure) |
| 1.3 | `START_PORT = Number(PORT) \|\| 3777` | FIXED: Backend::start() ignored PORT; now honours it like the JS |
| 1.4 | `start(port, tries=20)`: EADDRINUSE -> next port, 21 ports in all; other errors reject | OK (HttpApi::listen, test "next free port") |
| 1.5 | listen on 127.0.0.1 | OK |
| 1.6 | MIME table (.html .js .mjs .css .json .svg .png .ico .txt, else octet-stream) | FIXED for /custom/ files (only .js/.mjs were mapped) |
| 1.7 | `send()`: text -> `text/plain; charset=utf-8`, object -> `application/json`, `Cache-Control: no-store` | OK (live content-type compared on every request) |
| 1.8 | `readBody` limit 2e6 -> 413 "Request too large" | OK |
| 1.9 | `readBody`: empty -> {} | OK (live PUT /api/state with empty body) |
| 1.10 | `readBody`: bad JSON -> 400 "The request body isn't valid JSON" | OK (live) |
| 1.11 | `readBody`: array / scalar / null -> 400 "The request body must be a JSON object" | OK (live `[1]`, `"str"`, `null`) |
| 1.12 | `readText` limit 300e6 -> 413 "The file is over 300 MB" (100 MB for signals) | OK (QHttpServer body cap 310e6 so the 413 text is ours) |
| 1.13 | `allowed()`: Host header port stripped, [::1] brackets stripped | OK (unit test) |
| 1.14 | allowed hosts localhost, 127.0.0.1, ::1 (+ LAN addresses in LAN mode) | OK / LAN N/A (1.2) |
| 1.15 | non-GET/HEAD needs `x-qt: 1` | OK (unit test + live) |
| 1.16 | Origin present: `new URL(origin).host` must equal Host; "null"/opaque -> refuse | OK (unit tests) |
| 1.17 | empty Origin header ignored | OK |
| 1.18 | refused -> 403 text "Forbidden: Quant Terminal only answers to this computer." | OK |
| 1.19 | pathname decoded with decodeURIComponent; malformed -> 400 "Bad URL" | OK (unit test, incl. QUrl's %25 rewrite) |
| 1.20 | dot segments resolved like `new URL()` | OK (normalizePath) |
| 1.21 | query: URLSearchParams, `+` is space, last duplicate wins | OK |
| 1.22 | `/` and `/index.html` serve public/index.html; other non-/api paths serve public/ | N/A: no browser front end in deepwater; answers 404 "Not found" |
| 1.23 | `/custom/<file>` serves the custom folder; outside it -> 403 "Forbidden"; missing -> 404 "Not found" | OK |
| 1.24 | static Cache-Control: `vendor/` -> public, max-age=86400, else no-cache | OK (FIXED the vendor/ case for /custom/) |
| 1.25 | errors inside routes: upstream -> 502, status 400-599 kept, else 500, body `{error}` | OK |
| 1.26 | unknown /api route or method -> 404 `{error:"Unknown endpoint"}` | OK (live) |
| 1.27 | `handle()` crash outside the try -> 500 `{error}` | OK (std::exception catch in HttpApi) |
| 1.28 | SSE `/api/events`: headers text/event-stream, no-cache, keep-alive; first chunk `retry: 3000\n\n` | OK (unit test) |
| 1.29 | SSE broadcast `event: alert\ndata: <json>\n\n` to every client | OK (unit test) |
| 1.30 | SSE keep-alive comment every 25 s | OK |
| 1.31 | SSE client removed on close | OK (isResponseCanceled) |
| 1.32 | `mapLimit(items, 4, fn)` for /api/scan, results in order | OK |
| 1.33 | custom indicators loaded before listening | OK (qtm_server_run, Backend::start) |
| 1.34 | `alerts.start()` after listening | OK |
| 1.35 | start-up text with URL, data folder, proxy note | OK (qInfo; wording adapted to an embedded API) |
| 1.36 | `--open` / QT_OPEN opens a browser | N/A (no browser UI) |
| 1.37 | SIGINT/SIGTERM "Stopped." | N/A (app lifecycle) |
| 1.38 | `/api/shutdown` POST: answers {ok:true} then exits | DIFF-OK: answers {ok:true}, does not quit deepwater (installer-only route) |
| 1.39 | `slug()`: lower, non [a-z0-9] -> "-", trim dashes, 60 chars, "indicator" when empty | OK (unit test) |
| 1.40 | `listCustom()`: mkdir, *.js only, sorted (libuv), {file,id,mtime,error|null} | OK (mtime: JS has sub-ms precision; C++ too) |
| 1.41 | `loadCustom()`: import fresh copy; error recorded per file; success clears it | OK (core CustomIndicators does the import) |

## 2. server.js routes

| # | Item | Status |
|---|---|---|
| 2.1 | GET /api/health: ok, app, root, version, node, platform, providers, feeds, yahoo, alerts | OK; DIFF-OK: `root` is the data folder's parent, `node` is the Qt version |
| 2.2 | health `feeds` = configured() names only, never keys | OK (unit test) |
| 2.3 | health `yahoo` "available" / "rate limited" | OK |
| 2.4 | health `alerts` = count of active alerts | OK (live) |
| 2.5 | GET /api/probe = probeAll() | OK (live: same names, same ok/error) |
| 2.6 | GET /api/resolve?symbol= | OK (live, 46 cases) |
| 2.7 | GET /api/search?q= (missing q -> []) | OK (live) |
| 2.8 | GET /api/spread?symbol= | OK (live; values move) |
| 2.9 | GET /api/candles?symbol&tf (default 1D)&limit | OK (live, 136 cases) |
| 2.10 | candles limit: `Number(limit) \|\| 1500`, clamp 10..20000 (abc, 0, -5, 5, 99999) | OK (live) |
| 2.11 | candles limit with a fraction (JS keeps 100.5 in the cache key and Binance URL) | DIFF-OK: C++ truncates for the request; cache key keeps the fraction. No caller sends one |
| 2.12 | GET /api/quotes?symbols= split on ",", empties dropped, max 200 | OK (live) |
| 2.13 | GET /api/calendar | OK (live) |
| 2.14 | GET /api/earnings?days= (`Number(days) \|\| 5`) | OK (live, abc) |
| 2.15 | GET /api/dividends?days= | OK (live) |
| 2.16 | GET /api/news?symbol= | OK (live) |
| 2.17 | GET /api/financials?symbol&frequency (default annual only when missing) | OK (live, "" kept) |
| 2.18 | GET /api/screener?universe= crypto (default) / etf / anything else -> stocks | OK (live, "weird") |
| 2.19 | POST /api/scan: 400 "Send { symbols: [...], rules: [...] }" when symbols not array or rules (?? []) not array | OK (unit test) |
| 2.20 | scan: 150 symbols max, tf default 1D, 400 bars, indicators default {} | OK |
| 2.21 | scan row {symbol, pass, passes, close} or {symbol, error} | OK (unit test) |
| 2.22 | POST /api/backtest: 400 "Send { symbol, tf, strategy: ... }" (no symbol / strategy not a plain object) | OK |
| 2.23 | backtest tf default 1D, limit `body.limit \|\| 1500` | OK |
| 2.24 | backtest < 2 bars -> 422 "Not enough bars to test on" | OK |
| 2.25 | backtest magnifier: parseTf seconds, span, magnifierTf, sub-bars fetched for the resolved symbol, filtered to the test span | OK |
| 2.26 | backtest/quantChecks errors -> 400 with the message | OK |
| 2.27 | backtest response {symbol, tf, provider, bars, from, to, stats, trades, [equity], [quantChecks{passed,total,gates{label,value,pass},walkForward,monteCarlo}]} | OK (unit test vs JS) |
| 2.28 | GET /api/imports | OK |
| 2.29 | POST /api/import/bars?name= -> summary | OK (unit + live) |
| 2.30 | POST /api/import/signal?name=: cleanName, parseSignals(100 MB), writes custom/signal-<name>.js, loads it | OK |
| 2.31 | import/signal: load error -> previous file restored (or removed), error cleared, 400 | OK (unit test) |
| 2.32 | import/signal response {ok,name,id,file,rows,changes,from,to,strategy} | OK |
| 2.33 | GET /api/indicators: {id,name,inputs[{key,type,default,options?}],outputs[keys]} | OK (live, all indicators) |
| 2.34 | GET /api/state -> state.json or {} | OK (live) |
| 2.35 | PUT /api/state (20 MB): shallow merge, write, {ok:true} | OK (live; files equal) |
| 2.36 | /api/state other methods -> 404 | OK (live POST, DELETE) |
| 2.37 | GET /api/alerts, POST /api/alerts | OK (live) |
| 2.38 | GET /api/alerts/log (any method) | OK |
| 2.39 | POST /api/alerts/check -> evaluate, {ok:true} (GET falls through to 404) | OK (live) |
| 2.40 | DELETE /api/alerts/<id> -> {ok:true} even when unknown | OK (live) |
| 2.41 | PATCH /api/alerts/<id> -> updated alert; unknown -> 404 "No such alert" | OK (live) |
| 2.42 | POST /api/webhook-test: url must match `^https?:\/\/[^\s]+$`i -> else 400 "Webhook must be an http(s) URL" | FIXED: PCRE's `$` matched before a final newline and `\s` was ASCII-only |
| 2.43 | webhook-test posts {text, content} "Quant Terminal test alert", JSON, 10 s timeout | OK (key order FIXED to text, content) |
| 2.44 | webhook-test answers {ok, status}; network failure -> 500 "fetch failed" | OK |
| 2.45 | GET /api/custom | OK (live) |
| 2.46 | POST /api/custom: code must match /export\s+default/ else 400 with the export text | OK; FIXED `\s` to JS's set |
| 2.47 | custom file name slug(name) (missing name -> "undefined.js") | OK |
| 2.48 | custom refused: old file restored / new removed, 400 {error, file} | OK (unit test) |
| 2.49 | custom saved: {ok, file, id} | OK |
| 2.50 | DELETE /api/custom/<file>: path.basename, rm force | FIXED: a trailing slash ("x.js/") deleted nothing; basename now strips it like Node |
| 2.51 | DELETE /api/custom/ (directory) -> 500 EISDIR text with the normalized path | OK (path now normalized like path.join) |
| 2.52 | GET /api/ptq/status, /api/ptq/overview | OK (live, PTQ not running) |
| 2.53 | POST /api/ptq/check: errors -> e.status \|\| 502 with {error, code, problems, notes} (readBody errors too) | OK |
| 2.54 | POST /api/ptq/kill-switch | OK (unit test) |
| 2.55 | POST /api/ptq/arms/<id>/disarm, id decodeURIComponent'd again | OK; FIXED `$` so ".../disarm\n" is a 404 like Node |
| 2.56 | no route arms a strategy or places an order | OK (unit test) |

## 3. lib/providers.js: plumbing

| # | Item | Status |
|---|---|---|
| 3.1 | UA string for public sources | OK |
| 3.2 | Yahoo bare UA "Mozilla/5.0" | OK |
| 3.3 | QT_TIMEOUT_MS (default 10000) | OK |
| 3.4 | QT_OFFLINE=1 fixtures | OK (fixtures dir from QT_FIXTURES_DIR) |
| 3.5 | `health[name] = {ok, at, error?(200 chars)}` | OK |
| 3.6 | getJson: host skipped for 60 s after a network failure: "<host> isn't reachable from here (trying again in a minute)" | OK (unit test) |
| 3.7 | getJson network failure: "<host> couldn't be reached (timed out / cause.code / message)", upstream | OK |
| 3.8 | getJson success clears the host's back-off | OK |
| 3.9 | getJson non-2xx: "<host> returned HTTP <n>", status, upstream | OK |
| 3.10 | getJson headers: UA, Accept, Accept-Language, caller's headers override | OK |
| 3.11 | res.json() parse error text (V8 SyntaxError) | FIXED: now V8's wording and 10-character context rules for non-JSON bodies ("Too Many Requests", HTML), "Unexpected end of JSON input", bad literals |
| 3.12 | `cached()`: in-flight de-duplication | OK (unit test) |
| 3.13 | cached: TTL from the time it resolved | OK |
| 3.14 | cached: failure not cached | OK |
| 3.15 | cached: 400 entries max, oldest (insertion order) non-pending first, down to 90% | OK |
| 3.16 | ttlFor(tf): <60 s 1.5 s, <1 h 4 s, <1 D 15 s, else 60 s | OK |
| 3.17 | MAJOR_CRYPTO list (33) | OK |
| 3.18 | connection reuse between requests (Node's fetch keeps one pool for the process) | FIXED: C++ had one QNetworkAccessManager per worker thread, so almost every request paid a new TLS handshake (1-3 s on Nasdaq); one shared manager on a network thread now |
| 3.19 | connections per host unlimited (undici) | FIXED: Qt queued beyond 6 per host; raised to 64 |

## 4. providers.js: symbol resolution

| # | Item | Status |
|---|---|---|
| 4.1 | binanceSymbolMap: exchangeInfo TRADING symbols, 6 h cache, last good map / empty map on failure | OK |
| 4.2 | offline map BTCUSDT, ETHUSDT | OK |
| 4.3 | usTickerSet from the stocks screener, once per process, empty on failure | OK |
| 4.4 | parseSpread: SPREAD:a op b canonical | OK (reference test) |
| 4.5 | parseSpread: EUR/USD (both fiat) is a pair | OK |
| 4.6 | parseSpread: X/USDT, X/USDC, X/FDUSD, X/BUSD, X/USD is a pair | OK |
| 4.7 | parseSpread: `a / b`, `a*b` (spaces optional), `a + b`, `a - b` (spaces needed) | OK; FIXED `\s` to JS's set |
| 4.8 | resolve: ETH/BTC is the Binance pair when listed | OK (live) |
| 4.9 | resolve: nested operators -> 400 "Use one operator per spread, like BTCUSDT/ETHUSDT" | OK (live A/B/C) |
| 4.10 | resolve: two numbers -> 400 "A spread needs at least one symbol" | OK (live 1/2) |
| 4.11 | resolve: spread sides resolved, numbers normalised String(Number(x)) | OK (live 2*AAPL, SPY*2) |
| 4.12 | resolve: upper-case, all whitespace removed | OK; FIXED Unicode spaces (JS \s) |
| 4.13 | resolve: empty -> 400 "Empty symbol" | OK (live) |
| 4.14 | futures: NQ1!, CME:/CME_MINI: prefixes, bare root unless it is a US ticker (ES stays stock) | OK (live NQ, MNQ, ES, MES, ES1!, CME_MINI:NQ1!) |
| 4.15 | ticker character check -> 400 `"<raw 40>" isn't a symbol` (leading ^ allowed) | OK (live) |
| 4.16 | EXCHANGE:TICKER passes through | OK (live) |
| 4.17 | FX: 6 letters of two fiat codes, slash allowed | OK (live) |
| 4.18 | metals XAUUSD/XAGUSD -> FX: | OK (live) |
| 4.19 | `=`, `^`, `.` -> YAHOO: | OK (live VOD.L, ^FTSE, brk.b) |
| 4.20 | Binance unreachable: XUSDT/-USDT/USDC/FDUSD pairs and MAJOR_CRYPTO(+USD) still crypto | OK (unit test) |
| 4.21 | slash pair listed on Binance | OK (live SOL/USDT) |
| 4.22 | exact Binance symbol | OK |
| 4.23 | base + USDT on Binance unless it is a US ticker (COIN stays stock) | OK (live) |
| 4.24 | XX-USD -> COINBASE: | OK (live) |
| 4.25 | else US: | OK (live) |
| 4.26 | CSV:NAME passes through | OK (live) |
| 4.27 | kindOf(id) crypto / fx / stock / global | OK |

## 5. providers.js: sources

| # | Item | Status |
|---|---|---|
| 5.1 | Binance hosts api.binance.com, data-api.binance.vision, api.binance.us | OK |
| 5.2 | binanceGet: until one answered, all three at once, preferred in order | OK |
| 5.3 | binanceGet: remembers the host that answered | OK |
| 5.4 | binanceGet: 400 is not retried on other hosts | OK (unit test) |
| 5.5 | binanceGet: health binance ok / error | OK |
| 5.6 | BINANCE_NATIVE incl. 1s, 3D, 1W, 1M; 1D->1d, 3D->3d, 1W->1w | OK (live 1s, 3D, 1W, 1M) |
| 5.7 | binance: ratio, want = min(limit*ratio+ratio, 20000), pages of 1000 with endTime | OK (live 7m, 45m, 2W) |
| 5.8 | binance: "Binance can't build a <tf> chart" | OK |
| 5.9 | Coinbase native 1m 5m 15m 1h 6h 1D, 300 per page, want min(limit*ratio,1500) | OK (live COINBASE:BTC-USD) |
| 5.10 | Coinbase rows [time, low, high, open, close, volume], reversed, deduped | OK |
| 5.11 | Coinbase health mark | OK |
| 5.12 | Kraken native 1m..1D, OHLC interval minutes, XBT for BTC | OK (live KRAKEN:XBTUSD) |
| 5.13 | Kraken error array -> "Kraken: ..." | OK |
| 5.14 | krakenPairMap: wsname, not delisted, XBT->BTC, XDG->DOGE, altname | OK (reference test); DIFF-OK duplicate keys resolved in sorted rather than source order (no duplicates in Kraken's list) |
| 5.15 | coinbaseProducts / krakenPairs 6 h caches, empty on failure | OK |
| 5.16 | Yahoo native table (1m 7d ... 1M max), 60m for 1h | OK (live) |
| 5.17 | Yahoo 429 -> 10 min back-off, "Yahoo is rate limiting this connection (retrying in a few minutes)" | OK (unit test) |
| 5.18 | parseYahoo: no result -> chart.error.description or "Yahoo returned no data for that symbol" | OK (reference test) |
| 5.19 | parseYahoo: rows without open/close skipped; daily bars on exchange-local midnight; volume `\|\| 0`; deduped | OK (reference test) |
| 5.20 | Nasdaq headers Origin/Referer | OK |
| 5.21 | num(): strip $ , % whitespace, 0 when not finite | OK; FIXED whitespace set |
| 5.22 | nasdaqQuery: remembered class first, then stocks, etf, index | OK |
| 5.23 | nasdaqQuery: bCodeMessage text or "Nasdaq has no data for X"; health mark only for HTTP failures | OK (live "Symbol not exists.") |
| 5.24 | nyWallToUtc | OK (reference test) |
| 5.25 | nasdaqToday: points 09:30-16:00, volume | OK |
| 5.26 | nasdaq intraday: minute bars from points, resampled | OK |
| 5.27 | nasdaq daily: 10 years, rows reversed, close>0, open fill-ins, today's session appended | OK (live AAPL/SPY/QQQ/MSFT 1D, 1W, 1M, 3D) |
| 5.28 | nasdaqScreener: offline fixture, 60 s cache over a 6 h full list, rows mapped, ^ and / tickers dropped | OK (live, all rows) |
| 5.29 | screener: live quotes overlaid on the 200 biggest by market cap | OK (live) |
| 5.30 | nasdaqLiveQuotes: 20 per request, class nqClass / default / ETF list / index names | OK (live) |
| 5.31 | watchlist responses cached 5 s | OK |
| 5.32 | dedupe(): later bar with the same time replaces, older times dropped | OK |
| 5.33 | tryChain: first non-empty, "<provider>: no data", errors joined, 502 "No source could load this chart. ..." | OK (live unknown symbols) |
| 5.34 | cryptoParts quote list | OK |
| 5.35 | spreadCandles: both sides fetched in parallel, constants, matching times only | OK (live BTCUSDT/ETHUSDT, AAPL - MSFT, SPY*2, 2*BTCUSDT, BTCUSDT + 100) |
| 5.36 | spread bar: open/close combined, high/low from finite points, volume 0 | OK |
| 5.37 | spread: no common bars -> 422 "The two symbols have no bars at the same times" | OK |
| 5.38 | spread provider "A + B" or "constant", note "Spread built from both symbols' bars at matching times." | OK (live) |

## 6. providers.js: getCandles chains and notes

| # | Item | Status |
|---|---|---|
| 6.1 | bad tf -> 400 with parseTf's message | OK (live 13x, 0m) |
| 6.2 | CSV: -> imports.csvCandles | OK |
| 6.3 | offline fixture bars, notes [] | OK |
| 6.4 | cache key candles:<id>:<tf>:<limit>, ttlFor(tf) | OK |
| 6.5 | BINANCE: Binance, then Coinbase and Kraken for USD-like quotes | OK |
| 6.6 | COINBASE: / KRAKEN: own exchange only | OK |
| 6.7 | metals: Yahoo futures, gold falls back to PAXG on Binance then Coinbase | OK (live XAUUSD, XAGUSD) |
| 6.8 | FX: Yahoo <pair>=X then Kraken | OK (live) |
| 6.9 | CME futures: your Polygon key first, then "Yahoo (delayed)" | OK (live NQ1!, MNQ, ES1!, MES1!) |
| 6.10 | US: your keys first; intraday Yahoo then Nasdaq; daily Nasdaq then Yahoo | OK (live) |
| 6.11 | anything else: Yahoo | OK (live ^FTSE, VOD.L, GC=F) |
| 6.12 | note for a failed key: "<Why>, so this chart uses <provider>." | OK (unit test) |
| 6.13 | note for failed public sources: "<A and B> isn't answering right now, so this chart uses <p>." | OK (live) |
| 6.14 | note "Delayed about 10 minutes, because CME charges ..." | OK (live) |
| 6.15 | note "Front contract <c> from your Polygon key, not joined to older contracts. ..." | OK (unit test) |
| 6.16 | note "Live from your Alpaca key on the free IEX feed: ..." | OK (unit test) |
| 6.17 | note "This chart follows PAXG, a token backed by real gold, ..." | OK |
| 6.18 | note "Free intraday stock data covers today only ..." | OK |
| 6.19 | note "Kraken's free feed shows the last 720 bars." | OK (live KRAKEN:XBTUSD) |
| 6.20 | response {symbol, tf, provider, candles, notes} | OK |
| 6.21 | custom tfs resampled from the native base (7m, 10m, 45m, 2h, 3D, 2W) | OK (live) |

## 7. providers.js: quotes, spread, search, screeners, calendar, probe

| # | Item | Status |
|---|---|---|
| 7.1 | getQuotes: resolve each (failures skipped) | OK |
| 7.2 | Binance 24 h tickers in one request, sorted list key, 3 s cache | OK (live) |
| 7.3 | US live quotes via Nasdaq watchlist, `status` kept | OK (live) |
| 7.4 | the rest from 3 daily bars; changePct from previous close; `{error}` per symbol | OK (live CSV:NOPE error, FX, futures) |
| 7.5 | getSpread Binance bookTicker / Coinbase ticker / Kraken Ticker | OK (live) |
| 7.6 | getSpread other markets -> 400 "A live spread is only published for crypto ..." | OK (live) |
| 7.7 | getSpread "<source> sent no usable bid and ask" | OK |
| 7.8 | spread {symbol, bid, ask, spreadPct, source} | OK |
| 7.9 | search: empty -> [] | OK |
| 7.10 | search: imported files first ("Your file, 1,234 1m bars") | OK |
| 7.11 | search: futures by root (2+ chars) or name (4+ chars) | OK (live nq, mnq, micro, cme:nq1) |
| 7.12 | search: metals by ticker or word | OK (live gold, xa) |
| 7.13 | search: Binance hits (USDT first, shorter first, 8), then Coinbase/Kraken rows for 3 bases after the first hit | OK (live btc, eth, sol, doge) |
| 7.14 | search: FX pairs by prefix | OK (live eur, eur/usd) |
| 7.15 | search: Nasdaq autocomplete, Yahoo search (non-plain, non-crypto), 429 back-off | OK (live) |
| 7.16 | search: de-duplicated by symbol, 30 max | OK |
| 7.17 | cryptoScreener: Binance USDT pairs, no leveraged tokens, volume > 0, 20 s cache | OK (live 517 rows) |
| 7.18 | cryptoScreener Kraken fallback in Kraken's key order | OK (unit test) |
| 7.19 | economicCalendar 30 min cache, fields title..previous | OK (live) |
| 7.20 | probeAll: 6 public checks + key probes, {ok, ms} / {ok:false, error} | OK (live) |

## 8. lib/feeds.js (your own keys)

| # | Item | Status |
|---|---|---|
| 8.1 | feeds.json in the data folder, re-read when its mtime changes | OK (unit test) |
| 8.2 | invalid JSON -> keys off, logged | OK |
| 8.3 | alpaca needs keyId and secretKey | OK |
| 8.4 | polygon (or massive) needs apiKey; `polygon` object wins even without a key (JS `\|\|`) | OK |
| 8.5 | configured() names only | OK |
| 8.6 | alpacaName "Alpaca (<FEED>)", default IEX | OK |
| 8.7 | isKeyProvider /^(Alpaca\|Polygon)\b/ | OK |
| 8.8 | getFeed: Accept application/json + provider headers only | OK (unit test: keys only in headers, only to their own host) |
| 8.9 | getFeed: "<name> couldn't be reached" | OK |
| 8.10 | 401 "<name> refused your key", 403 "your <name> plan doesn't include this", 429 "<name> is rate limiting your key", else "<name> returned an error" | OK (unit test) |
| 8.11 | 401/429 rest the provider for 60 s | OK (unit test) |
| 8.12 | nyOffsetHours per day; inUsSession 09:30-16:00 NY | OK (reference test) |
| 8.13 | startFor(base, want, earliest) | OK |
| 8.14 | finish(): sort, drop repeats, resample, slice | OK |
| 8.15 | alpaca tf table 1m 5m 15m 30m 1D | OK |
| 8.16 | alpaca query params, order and URLSearchParams encoding, page_token, 12 pages | OK (unit test) |
| 8.17 | alpaca headers APCA-API-KEY-ID / APCA-API-SECRET-KEY, sent only to data.alpaca.markets | OK (unit test) |
| 8.18 | alpaca bars: session filter intraday, UTC midnight for daily, volume `\|\| 0` | OK |
| 8.19 | polygon tf table, from/to, adjusted, desc, 50000 | OK (unit test) |
| 8.20 | polygon Bearer header only | OK (unit test) |
| 8.21 | polygonPages: next_url followed only on Polygon's own host, 6 pages, key never in the URL | FIXED: compared QUrl authority (with user info) instead of `new URL().host`; an unparsable next_url now throws "Invalid URL" like Node |
| 8.22 | stockChain order Alpaca then Polygon | OK |
| 8.23 | frontContract: third Friday, roll 8 days before, H/M/U/Z, one-digit year | OK (reference test) |
| 8.24 | FUT_RES table, window_start.gte, nanosecond times, session_end_date for daily | OK (unit test) |
| 8.25 | "Polygon can't build a <tf> futures chart" | OK |
| 8.26 | futuresChain name "Polygon <contract>" | OK |
| 8.27 | probes "Alpaca (your key)" / "Polygon (your key)" | OK (unit test) |

## 9. lib/imports.js

| # | Item | Status |
|---|---|---|
| 9.1 | cleanName: .csv/.txt, upper, non-alnum -> _, trim _, 30 chars, 400 "Give the file a name made of letters or digits" | OK; FIXED `$` before a final newline |
| 9.2 | rowsOf: BOM, CRLF, blank and # lines, "The file is empty" | OK (reference test) |
| 9.3 | delimiter , ; tab by count, ties in that order | OK |
| 9.4 | cells trimmed, surrounding quotes removed | OK |
| 9.5 | column aliases (time, open, high, low, close, volume, position, symbol) | OK |
| 9.6 | MetaTrader date + time columns | OK (reference test) |
| 9.7 | parseTime: unix s/ms/us/ns, ISO with zone, -, /, . dates, UTC default | OK (reference test, 20 cases); FIXED JS whitespace before the zone |
| 9.8 | TIME_HELP text | OK |
| 9.9 | tfLabel M/W/D/h/m/s | OK |
| 9.10 | parseBars headerless 5-6 columns | OK |
| 9.11 | parseBars: "Couldn't find the time and close columns. ..." | OK |
| 9.12 | parseBars: "Row N: couldn't read the time ..." | OK |
| 9.13 | parseBars: unreadable price rows skipped and counted | OK |
| 9.14 | parseBars: more than one symbol -> error | OK |
| 9.15 | parseBars: sort, duplicates replaced, < 10 bars error | OK |
| 9.16 | parseBars: timeframe = most common gap (ties: smallest), first 5000 | OK |
| 9.17 | load(): cache by mtime; missing -> 404 "There's no imported file called ..." | OK (live CSV:NOPE) |
| 9.18 | importBars writes imports/NAME.csv, returns summary | OK |
| 9.19 | listImports sorted, errors listed per file | OK |
| 9.20 | csvCandles: shorter tf -> 400 "<n> has <tf> bars, so it charts at <tf> and longer." | OK |
| 9.21 | csvCandles provider "Your file NAME.csv", notes own / built from | OK |
| 9.22 | parseSignals: position words, -1..1, "Row N: the position ..." | OK |
| 9.23 | parseSignals: only changes kept, "The file has no rows" | OK |
| 9.24 | signalIndicatorSource text | OK (reference test, byte for byte) |
| 9.25 | signalStrategy rules | OK |

## 10. lib/market-info.js

| # | Item | Status |
|---|---|---|
| 10.1 | num() null when not finite | OK; FIXED whitespace set |
| 10.2 | weekdays(n), 1..10 | OK |
| 10.3 | earnings per day, 30 min cache, failures -> [] | OK (live) |
| 10.4 | earnings row fields and "Before the open"/"After the close"/"Time not given" | OK (live) |
| 10.5 | dividends rows | OK (live) |
| 10.6 | COIN_NAMES | OK |
| 10.7 | RSS decode (CDATA, entities, tags) | OK (reference test) |
| 10.8 | parseRss title/link/pubDate/description(280) | OK (reference test) |
| 10.9 | cryptoNews: Cointelegraph + Decrypt, UA "Mozilla/5.0 QuantTerminal", 10 min cache, newest first | OK (live) |
| 10.10 | crypto news: base from ticker, \b word match, >= 3 hits or latest | OK (live BTC, SOL, XBT) |
| 10.11 | US news: articlebysymbol per class, url absolutised, time, ago, summary; 10 min cache | OK (live) |
| 10.12 | other markets: "Headlines are available for crypto and US stocks." | OK (live) |
| 10.13 | table(): Object.keys order, value1 is the label | OK |
| 10.14 | financials: 422 for non-US, 6 h cache per frequency | OK (live) |
| 10.15 | financials: stats without N/A, nextEarnings whitespace collapsed | OK; FIXED JS whitespace in nextEarnings |
| 10.16 | financials: 404 "Nasdaq has no financials for X (ETFs and funds don't file them)." | OK (live SPY) |

## 11. lib/store.js

| # | Item | Status |
|---|---|---|
| 11.1 | DATA_DIR (QT_DATA_DIR / app data folder) created | OK |
| 11.2 | read(name, fallback) | OK |
| 11.3 | write: JSON.stringify(v, null, 2) to a temp file, rename | OK (live: state.json and alerts.json byte-compared after sorting; alerts.json keys in JS order) |
| 11.4 | write: EPERM/EBUSY/EACCES fallback in place | OK |
| 11.5 | key order inside saved objects | DIFF-OK: common keys in JS order, others sorted (e.g. a drawn line's t1/p1...): JSON readers don't care |

## 12. lib/alerts.js

| # | Item | Status |
|---|---|---|
| 12.1 | alerts.json and alert-log.json loaded at start | OK |
| 12.2 | add(): "Send the alert as a JSON object" | OK (live) |
| 12.3 | add(): fields id, symbol ("" for watchlist), watchlist, tf 1m, left, op, right, conditions, match, indicators, trigger once, webhook, message, active, createdAt, lastBar null, fired 0 | OK (live, 35 cases; `active` in input ignored) |
| 12.4 | validate: watchlist must be a non-empty string | OK (live) |
| 12.5 | validate: "An alert needs a symbol or a watchlist" | OK (live) |
| 12.6 | validate: up to 5 conditions | OK (live) |
| 12.7 | validate: match "all"/"any" only | FIXED: `["all"]` was accepted (String() of the array) |
| 12.8 | validate: webhook /^https?:\/\/\S+$/i | FIXED: trailing newline and Unicode spaces (live) |
| 12.9 | validate: right must be set ("An alert needs something to compare against") | OK (live "", null, false) |
| 12.10 | validate: "Unknown operator ..." with the list | OK (live) |
| 12.11 | validate: every operand checked on the synthetic probe series | OK (live unknown indicator/output) |
| 12.12 | conditionsOf: conditions when a non-empty array, else left/op/right | OK (live "abc", []) |
| 12.13 | update(): 404, validate merged, id kept | OK (live) |
| 12.14 | remove() | OK (live) |
| 12.15 | describe(): ops words, "your drawn line", message, watchlist prefix, and/or | OK (reference test + live texts) |
| 12.16 | sendWebhook: discord {content}, slack.com {text}, telegram.org {text}, else {text, content, alert, event} | OK (live via local receiver; key order FIXED) |
| 12.17 | webhook POST JSON, 10 s, "Webhook returned HTTP n" | OK (live /status/500) |
| 12.18 | fire(): event {id, alertId, symbol, tf, text, price, barTime, at}, fired+1, lastFiredAt | OK (live) |
| 12.19 | fire(): "once" without watchlist switches off | OK (live) |
| 12.20 | fire(): webhook "sent" / "failed: <message>" on the event | OK (live, closed port "failed: fetch failed") |
| 12.21 | log newest first, 500 max, saved | OK |
| 12.22 | listeners (SSE alert event) | OK |
| 12.23 | evaluate(): one at a time, concurrent caller waits for the running one | OK (unit test) |
| 12.24 | evaluateOnce: watchlist read fresh from state, duplicates removed | OK (live) |
| 12.25 | grouping by symbol and tf, one getCandles(400) per group | OK |
| 12.26 | failed candles / < 3 bars skip the group | OK (live NOPE) |
| 12.27 | oncePerBarClose uses the previous bar | OK (live) |
| 12.28 | watchlist: lastBars per symbol; once = once per symbol | OK (live) |
| 12.29 | oncePerBar: lastBar | OK (live) |
| 12.30 | match any / all | OK (live XRP pair) |
| 12.31 | drawn-line conditions (extend right; ends before now -> no fire) | OK (live) |
| 12.32 | errors kept on alert.error | OK |
| 12.33 | save at the end | OK |
| 12.34 | timer 2 s then 15 s after each run | OK |

## 13. lib/ptq.js

| # | Item | Status |
|---|---|---|
| 13.1 | PTQ_URL default, trailing slashes trimmed | OK |
| 13.2 | PTQ_SITE | OK |
| 13.3 | leaseInfo: PTQ_HOME, ~/.ptq, ~/.trading-os; lease/exp/activated | OK |
| 13.4 | call(): content-type only with a body, Bearer lease, timeouts | OK (unit test) |
| 13.5 | call(): timeout -> 504 "timeout", unreachable -> 503 "not_running" texts | OK (live status) |
| 13.6 | call(): 401 expired / not activated texts | OK |
| 13.7 | call(): 402 credits vs refused/not found/unavailable -> 400 with "engine 422" rewritten | OK |
| 13.8 | call(): other non-2xx -> j.error or "PTQ answered HTTP n", 502/400 | OK |
| 13.9 | status(): url, site, licensed, leaseExpires, connected, version/engine or reason | OK (live) |
| 13.10 | toTerm (constants, price fields, hl2... refused, indicators, bounds, source) | OK (reference test) |
| 13.11 | group(): op table, problems and notes | OK (reference test) |
| 13.12 | translate(): direction, notes, timeframe, maxHoldingHours, manifest | OK (reference test) |
| 13.13 | resolveInstrument: BITGET:SPOT / YAHOO:STOCKS / YAHOO:FX, 422 texts | OK (unit test) |
| 13.14 | check(): strategies, backtest/run (240 s), validate, response | OK (unit test) |
| 13.15 | overview(): five reads, errors per part, journal 50 | OK (live) |
| 13.16 | engageKillSwitch, disarm only | OK |

## 14. lib/proxy.js

| # | Item | Status |
|---|---|---|
| 14.1 | proxy from HTTPS_PROXY / https_proxy / HTTP_PROXY / http_proxy | OK (Http.cpp, per scheme like Node's env agent; NO_PROXY honoured) |
| 14.2 | relaunch with NODE_USE_ENV_PROXY | N/A (Qt uses the proxy directly) |
| 14.3 | proxyNote: credentials stripped | OK |

## 15. Timing (asked by the coordinator)

| # | Item | Status |
|---|---|---|
| 15.1 | unknown symbol (ZZQQXX) via /api/candles, cold and warm | FIXED (3.18, 3.19): before, each Nasdaq try paid a new TLS handshake and the first one sometimes hit the 10 s timeout (then Nasdaq was skipped for a minute); now within Node's range (both 8-17 s here, dominated by Nasdaq's own 2-5 s per "Symbol not exists" answer) |
| 15.2 | /api/resolve of an unknown symbol | OK: 0.01 s on both once Binance's list is cached; cold both wait for exchangeInfo (~3 s) |
| 15.3 | cold /api/quotes for the 8 default watchlist symbols | FIXED (3.18, 3.19): the ETF list's ten quote batches were queued 6 at a time on fresh connections |
| 15.4 | cold /api/screener?universe=stocks | FIXED (3.18, 3.19): now within a second of Node (both ~9-10 s, most of it Nasdaq's 6 s list download) |
| 15.5 | many requests at once (the Charts page at start-up): Node serves them all concurrently | FIXED: the UI's request pool (16 threads) and the local API's pool (32) queued requests that only wait on the network; both 64 now |
