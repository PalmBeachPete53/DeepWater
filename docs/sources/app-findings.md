# What the inventory suggested about the app, and what came of it

The calls of 5 November 2026 raised these. Each was then checked against the code and a new call before anything was
changed; several did not hold.

## Confirmed and fixed
| Finding | Evidence | Fix | Verified |
|---|---|---|---|
| SSB table `05327` closed at 2025M12, freezing Norway's CPI-ATE | its metadata; `14706` runs to 2026M09 | `14706`, `KPIavledetSerie=KPI-JAE`, on the current `v2` API | in the app: Norway's core inflation, September 2026, Statistics Norway |
| MarketPulse feed dead (newest item 2025-07-03) | the feed | removed from the Stocks feeds | code removal |
| Hugging Face search never had `gated` or `lastModified`, so "Updated" was empty | the answer's fields | `expand[]` for the fields the app uses | in the app: the column fills |
| INEGI's row asked for a token and fed nothing | no series in `resources/country_indicators.json` | row removed; the loader stays | diagram and list rebuilt from the rows |
| Frankfurter's root calls v1 `frozen`, v2 `current` | root, and v1 and v2 equal on the days compared | v2 with `providers=ECB`; a v1 cache still loads | in the app: fetch, cache, reload |
| SCB's address `api.scb.se/.../v2beta` is the old one | the office's current API answers the same request | `statistikdatabasen.scb.se/api/v2` | exact request by hand |

## Confirmed, fixed, not testable here
- **EVDS**: `evds2` redirects to an HTML page and `TP.FG.J0` is archived. The app now calls
  `evds3.tcmb.gov.tr/igmevdsms-dis/` for `TP.GENENDEKS.T1`. Without a key that address answers 403 "header 'key' is
  not present", which shows the route exists. No EVDS key is saved, so the answer's format, including the field
  `TP_GENENDEKS_T1`, is unverified until one is.

## Confirmed, not fixed
- **aisstream**: since September 2026 uncompressed connections have a per-user bandwidth limit, and messages over it are
  dropped. Its documentation asks for `permessage-deflate`, which Qt's `QWebSocket` can't negotiate (nothing of it in the
  library). The app subscribes worldwide, to two message types. The effect on the number of ships was not measured.
  Options: a narrower bounding box, or a WebSocket client of our own with zlib.

## Did not hold
- **Bank of England RSS "empty"**: 50 items under every User-Agent tried, and it shows in Desk › Notes. A passing answer.
- **FRED's CSV answers the app**: it timed out three times in a row, so the comment in `src/ResearchData.cpp` stands.
- **SCB's 60-76 s**: 0.5 s that day. The address changed anyway (above).
- **`V41552796`**: it is M2, as labelled, and its size fits. Its description is a grouping heading.
- **ABS `RT` and `CPI_M` ended**: true, and the app reads neither. It reads `CPI` 2.0.0.
- **SGS answers 200 HTML for an unknown code**: 404 on the form tried, and the app's codes are known ones.

## Not problems of the app
- **CSO's dead links**: in CSO's own catalogue, which the app doesn't read. **Census**: the app doesn't call it.
- **Kraken's 720 candles**: the chart already says so (`src/quant/server/Providers.cpp:1253`).
- **PortWatch's "Invalid URL" in a 200**: both readers treat `error` as a failure and keep the last copy.
- **JODI's unlinked file**: still refreshed. Watch it.
- **SARB's history, crop stocks and production**: data the app could use, not faults. See `sarb.md`, `usda.md`.

## Terms and robots.txt: the owner's decision
Nothing was changed here: it decides what the app may read, not how well.
- **Yahoo** (`yahoo.md`): robots.txt disallows every data host and its terms ban automated collection. The app calls
  them, sending a bare `Mozilla/5.0` because fuller user agents get 429.
- **Nasdaq** (`nasdaq.md`): robots.txt disallows `api.nasdaq.com`, which the app calls.
- **Binance** (`binance.md`): robots.txt disallows `api.binance.com` and `api.binance.us`; `data-api.binance.vision`
  has no robots.txt and covers every spot path, so it could come first.
- **Wikimedia** (`wikipedia.md`, `wikidata.md`): robots.txt disallows the Wikipedia API and SPARQL paths; the app's
  User-Agent lacks the contact details Wikimedia asks for.
- **CNBC** (`newsfeeds.md`): robots.txt disallows its feed.
- **Bank of England** (`boe.md`): robots.txt disallows the CSV endpoint, in capitals, while its Help documents it.
- **CoinMarketCap, TradingView** (`coinmarketcap.md`, `tradingview.md`): terms restrict automated or commercial use.
- **Investing.com** (`other-hosts.md`): terms forbid storing or using its data.

## Side effects of the inventory
- **BLS**: the inventory spent the day's keyless quota (25 requests per address), so the app's BLS row fails on
  5 November 2026 until it resets; it is the one failing source in the Sources tab. `bls.md` is unverified.
- **BEA**: two files that robots.txt disallows were fetched before robots.txt was read. Their contents aren't in the
  card.
- **CoinMarketCap, TradingView**: called before their terms were read.

## The catalogue (`resources/data_sources.json`)
Corrected: see `catalogue-corrections.md`. Left as they are: bodies that aren't statistical (Shell Suisse, Ipsos,
NielsenIQ, procure.ch, the CFA Society), since they are what the indicators credit; and name variants not on the same
domain, which no check could tie together.

## Sources worth a look
- European Commission's Redisstat (`european_commission.md`): forecasts to 2027 and inflation expectations.
- `fapi.binance.com` lists 235 TradFi perpetuals.
- SARB's `MonthlyIndicatorsAll/<table>` gives whole histories, where the app builds its own from its cache.
