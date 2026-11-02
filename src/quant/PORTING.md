# Quant Terminal port: rules for everyone working on it

`quant-terminal/` (repo root, now removed: put a copy back there to regenerate references or compare) is a
Node.js + browser trading terminal. We are porting it to C++/Qt
inside deepwater, as the **Charts** page. The result must be a **carbon copy**: same UI, same
behaviour, same numbers, same texts, same error messages. When in doubt, the JavaScript is the spec.
Read the original file you port line by line; don't port from memory or from the README.

## Layout

| C++ (src/quant/) | Ports | Library |
|---|---|---|
| `core/Bar.h`, `core/Error.h`, `core/CandleSet.h` | shared types | qtm_core |
| `core/Series.*` | public/lib/series.js | qtm_core |
| `core/Indicators.*` (+ more .cpp files if useful) | public/lib/indicators.js | qtm_core |
| `core/CustomIndicators.*` | custom/*.js loading (server.js loadCustom + app.js importCustom), QJSEngine | qtm_core |
| `core/JsTransform.*`, `core/CustomIndicatorsJs.*` | the modern syntax and built-ins Node has and V4 lacks, for custom files (rewrites before import, polyfills; checked by tests/quant/js) | qtm_core |
| `core/Seasonals.*` | public/lib/seasonals.js | qtm_core |
| `core/Backtest.*`, `core/Quant.*` | public/lib/backtest.js, quant.js | qtm_core |
| `server/*` | server.js, lib/*.js | qtm_server |
| `chart/*` | the Lightweight Charts library (vendor), as used by the app | qtm_chart |
| `ui/*` | public/js/*.js, index.html, css/app.css | qtm_ui |

The headers already in place are **contracts** between people working in parallel. Keep every
declared name and signature. You may add members (private state, pimpl, extra helpers) and extra
headers/sources in your own directory. If a contract is really wrong, change it minimally and say
so in your final report.

## Conventions

- Namespace `qtm` (chart engine: `lw`). C++20, Qt 6.12 (Homebrew, `/opt/homebrew/opt/qt`).
- JS `NaN` -> `qtm::NaN`; arrays of numbers -> `qtm::Values` (QVector<double>). Keep JS numeric
  semantics exactly (NaN propagation, `Math.round` = round half up toward +inf, `toFixed`,
  `||` treating 0/NaN as false, integer division via Math.floor, Date in UTC ...).
- JS objects that are free-form data (strategies, alert definitions, API responses, paper account)
  stay JSON (`QJsonObject`/`QJsonArray`) with the same keys. Mind that QJsonObject sorts keys; where
  JS order is visible to the user, use an ordered structure.
- Errors: throw `qtm::Error(message, status, upstream)` with the JS message text.
- Comment style: like the JS originals (short, plain English, explain why).
- No new third-party dependencies. Qt modules available: Core, Gui, Widgets, Network,
  WebSockets, Svg, Qml (QJSEngine), HttpServer, Concurrent, Multimedia.
- The UI text is English, exactly as in the JS. Don't wrap it in tr().

## Data locations

`Backend(dataDir, customDir)`: deepwater passes `<AppDataLocation>/quant-terminal/data` and
`<AppDataLocation>/quant-terminal/custom` (AppDataLocation = `~/Library/Application Support/Deepwater/Deepwater`).
The custom folder is seeded with quant-terminal/custom/ema-ribbon-squeeze.js on first run.

## Building and testing

Each library builds on its own. Use your **own build directory** so parallel builds don't clash:

```
cmake -S . -B build-<you> -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt
cmake --build build-<you> --target qtm_core -j     # or qtm_server, qtm_chart, qtm_ui
```

Tests live in `tests/quant/<area>/` with their own CMakeLists.txt (picked up automatically).
Check numbers against the JS originals by running them with Node (`node` is installed, v26):
write a small `.mjs` script that imports `quant-terminal/public/lib/*.js`, dumps reference output
to JSON, and compare from a C++ test. The JS test suite (`cd quant-terminal && npm test`) passes and
its fixtures (`quant-terminal/test/fixtures/`) are useful inputs.

Don't edit files outside your area (other than adding your test dir), don't touch deepwater's
existing sources, and don't modify `quant-terminal/` (it is the reference).
