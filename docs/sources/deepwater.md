---
id: deepwater
name: What the app computes and stores itself
rows: [markov_refresh]
catalogue: []
access: local
probed: 2026-11-05
confidence: called   # schemas from the code; of the user's data folder only names and sizes were listed, no file was opened
---

# What the app computes and stores itself

Data no outside source holds: regimes and forecasts estimated here, golden records fused here, a daily log of what no source keeps, order-book recordings. All under `~/Library/Application Support/Deepwater/Deepwater` (`<AppData>`), except the Markov state, in QSettings.

## Contents

**Markov state** (row `markov_refresh`). QSettings keys `quant/marketState/regimes`, `quant/marketState/volatility` (`src/QuantMarketState.cpp:164-165`), compact JSON.
- Regime, per symbol and source (`HMM` or `MS-GARCH`): `symbol`, `source`, `states`, `regime`, `label`, `probability`, `dailyVol`, `vols[]`, `probabilities[]`, `transition[]` (states × states), `asOf`, `observations`, `computedAt`, `spec`.
- Volatility, per symbol: `symbol`, `model`, `dailyVol`, `annualVol`, `horizonDays`, `horizonVol`, `var95`, `var99`, `es95`, `es99`, `asOf`, `observations`, `computedAt`, `spec`.
- Input: daily candles from Yahoo. Re-estimated when older than the last trading day.

**Golden records** `<AppData>/fusion/` (`src/fusion/Store.h`, version 1):
- `<ISO3>/<indicator>.json`: `version`, `country`, `indicator`, `fused`, `golden[]`, one per frequency, finest first.
- A golden: `freq`, `unit`, `spliced`, `unverified`, `points[]` as `[date, value, source index, flags]`, `sources[]`, `conflicts[]` as `[date, kept, other, keptValue, otherValue]`.
- A source: `id` (`eurostat:une_rt_m?…`), `label`, `priority`, `freq`, `relation` (`Anchor`, `Same`, `Rebased`, `Distinct`, `Unverified`, `Incompatible`), `why`, `unit_factor`, `factor`, `overlap`, `median_gap`, `correlation`, `agreement`, `points`, `used`, `conflicts`, `first`, `last`.
- `<ISO3>/<indicator>.raw.json`: the head and `inputs[]`: `id`, `label`, `priority`, `unit`, `fetched`, `points[]` as `[date, value]`.
- `index.json`: `version`, `records` `{<ISO3>/<indicator>: {fused, records, points, added, spliced, unverified, conflicts, providers[], labels[]}}`.

**Desk history** `<AppData>/desk/history.json` (`src/desk/History.h`): `{key: [[date, value], …]}`, one value per weekday from the day the app first saw it. Keys: `crowd/<symbol>` (Myfxbook long share), `rate/<currency>/<meeting date>` (the move priced for a meeting, USD and GBP) (`src/DeskHistory.cpp:61-83`).

**Depth recordings** `<data>/depth/<PAIR>` or `<PAIR>-US` (`src/quant/server/DepthStream.cpp:653`, format `src/quant/core/DepthRecord.h`). Binary:
- Header `DWDEPTH1`, version u16, pair, tick size f64, exchange u8.
- Blocks: u32 big-endian length, `qCompress`ed records. Each record: type u8, time f64, then by type `Reset` 1, `Snapshot` 2, `Update` 3, `Lost` 4, `Checkpoint` 5.
- Levels as tick differences and quantity in units of 1e-8, LEB128 varints.
- The README gives about 6 MB an hour on BTCUSDT; files older than the days kept (7) are deleted.

**Others stored**, from the code:

| Path under `<AppData>` | Holds | Code |
|---|---|---|
| `journal/journal.json` | `accounts[]`: `name`, `currency`, `balance`, `lastImport`, `trades[]` (`id`, `symbol`, `long`, `volume`, `open`, `close`, `openPrice`, `closePrice`, `sl`, `tp`, `commission`, `swap`, `profit`, `strategy`, `notes`); `current` | `src/JournalData.cpp:20`, `src/desk/Journal.cpp:261` |
| `chokepoint_events.bin` | magic `DWCP`, v2: events `mmsi`, `timeMs`, `zone`, `vesselClass`, then the recorded hours | `src/Chokepoints.cpp:26`, `:286` |
| `vessel_static.cache` | magic `DWVS`, v2: `mmsi`, `shipType`, `name`, `destination`, `callSign`, `imo`, `draught`, `length`, `lastSeenMs` | `src/VesselStore.cpp:25`, `:150` |
| `cache_<name>.bin` | an HTTP body and its time, one per request; magic, `QDateTime`, `QByteArray` | `src/HttpCache.cpp:16` |
| `fx_rates.json` | `fetchedAt`, `response` (the source's answer) | `src/FxData.cpp:22`, `:89` |
| `portwatch.json` | PortWatch's answers; format not traced | `src/PortWatch.cpp:25` |
| `news-favorites.json` | saved articles, `savedAt` | `src/NewsData.cpp:540` |
| `ai/tool-journal.jsonl` | one line per tool call: `tool`, `arguments`, `outcome`, `detail`… | `src/ToolJournal.cpp:17` |
| `ai/exports/` | CSV files written by AI tools | `src/LocalTools.cpp:263` |
| `ml/` | `data/` (CSV exports), `runs/`, `scripts/`, `venv/` | `src/MlEnvironment.cpp:27` |
| `models/` | model files from [Hugging Face](huggingface.md) | `src/AiModels.cpp:30` |
| `pinets/` | the PineTS runtime | `src/quant/core/PineScript.cpp:101` |
| `quant-terminal/data/` | `state.json`, `alerts.json`, `feeds.json`, `imports/`, `depth/` (layouts, watchlists, drawings, paper account per `src/quant/server/Store.h`) | `src/MainWindow.cpp:150`, `src/quant/server/Server.h:42` |
| `quant-terminal/custom/` | custom indicators, `.js` and `.pine` | `src/MainWindow.cpp:151` |

`feeds.json` holds the user's own Alpaca and Polygon keys: not opened.

On this machine on this date, by listing: 2,764 entries, 8.9 GB in all; `models/` 7.3 GB, `ml/` 1.2 GB, research caches 370 MB in 2,667 files, `fusion/` 6.2 MB (`BRA`, `FRA`, `IND`, `JPN`, `USA`, `ZAF`, `index.json`), `desk/` one file of 1,968 bytes. There is no `quant-terminal/data/depth`, `imports` or `journal/` folder yet.

## How to call it
Read through the app's classes. In Python, `json.load` for the JSON files; the `.bin` and `.cache` files are Qt `QDataStream`, and the depth files need `src/quant/core/DepthRecord.*`.

## What the app reads
Row `markov_refresh`, `src/SourcesPage.cpp:611`, probe `:610`.
- `StateRefresher` `src/QuantMarketState.h:91`, every 30 minutes (`IntervalSecs`), one symbol at a time; prices from `research::Client::prices` `src/QuantMarketState.cpp:380`, `:384`. Read by `MarketState` for Quant's Portfolio and Research's strips.
- Fusion: `src/CountryData.cpp:949`, `:1018`, `:1042`. Also exposed as the `golden_series` tool and ML's `golden_series.csv` (project notes).
- Desk history: `src/DeskHistory.cpp:17`, shown in Desk.

## Not read yet
- Golden records exist only for countries and indicators opened so far: `<AppData>/fusion` has 6 countries here.
- `.raw.json` inputs are "not loaded back" (`src/fusion/Store.h:19`): written for tools.
- The transition matrices (`transition[]`) are stored for "other models", which could read them through a skill.

## Calls made
No network. `ls` and `du` of `<AppData>`; no file opened, no `defaults` read.
