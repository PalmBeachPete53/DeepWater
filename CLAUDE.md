## graphify

This project has a knowledge graph at graphify-out/ with god nodes, community structure, and cross-file relationships.

Rules:
- When graphify-out/graph.json exists, run `graphify query "<question>"` before your first grep, search or source-file read for a codebase question, and put this rule in every subagent prompt that explores code. Read raw files only for lines the graph or an earlier read already located. Use `graphify path "<A>" "<B>"` for relationships and `graphify explain "<concept>"` for focused concepts. These return a scoped subgraph, usually much smaller than GRAPH_REPORT.md or raw grep output.
- If graphify-out/wiki/index.md exists, use it for broad navigation instead of raw source browsing.
- Read graphify-out/GRAPH_REPORT.md only for broad architecture review or when query/path/explain do not surface enough context.
- After modifying code, run `graphify update .` to keep the graph current (AST-only, no API cost).
- A committed graphify-out/graph.json is rewritten whole on every build, so a large diff to it in a pull request is expected. Do not flag it as churn or suggest skipping the graph rebuild or its hooks.

## Deepwater

Qt 6 (with WebEngine) / C++20 desktop app (CMake); `cargo` builds `third_party/adblock-ffi` (the reader's ad blocker)
when present; `third_party/llama.cpp` (vendored, Metal) builds `deepwater-llm`, the sandboxed local-model helper in the
bundle (AI › Local, one-architecture builds only); on macOS the News reader renders with the system's WebKit (`src/NewsWebHostMac.mm`, Objective-C++). `README.md` documents every tab, data source and the layout of `src/`; read its
section for the tab you touch before changing it, and keep it current when you add something.

Build and tests:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt
cmake --build build -j 10 --target deepwater
./build/tests-quant/markov/qtm_test_markov [hmm|garch|execution]   # the Markov engine (src/markov)
./build/tests-quant/fusion/qtm_test_fusion                          # data fusion: golden records from several sources (src/fusion)
./build/tests-quant/core/qtm_test_core                              # the Charts port, against the JS references
./build/tests-quant/footprint/qtm_test_footprint                    # the footprint chart's aggregation (src/quant/core/Footprint.*)
./build/tests-quant/bigtrades/qtm_test_bigtrades                    # Big Trades: aggregation, thresholds, bubbles (src/quant/core/BigTrades.*)
./build/tests-quant/tradestream/qtm_test_tradestream                # the Binance trades shared by the chart panes (server/TradeStream, ui/TradeFeed), fake source
./build/tests-quant/depthstream/qtm_test_depthstream                # the order book depth shared by the chart panes (server/DepthStream, ui/DepthFeed), fake source, ~9 s
./build/tests-quant/klinestream/qtm_test_klinestream                # the live candles shared by the chart panes (server/KlineStream), fake source
./build/tests-quant/navigator/qtm_test_navigator                    # the range slider under the charts (core/RangeSlider + ChartNavigator on a real chart, offscreen)
./build/tests-quant/heatmap/qtm_test_heatmap [load]                 # liquidity heatmap: book, sampling, store, pyramid, filters, events, recordings (src/quant/core/OrderBook.*, Heatmap*.*, DepthRecord.*)
./build/tests-quant/desk/qtm_test_desk                              # Desk and Journal logic: instruments, risk, statements, stats, strength, signals, seasonality, rates (src/desk)
./build/tests-quant/pinets/qtm_test_pinets                          # Pine Script via PineTS on Node.js (src/quant/core/PineScript.cpp); SKIPPED without Node
deepwater --pine-js file.pine                                      # print the JavaScript PineTS runs for a Pine Script and exit
deepwater [--theme <id>] --page <id> [--devtools] [--country FRA] [--indicator <n>] [--search <text>] [--ai-search <text>] [--ai-local <prompt>] [--ai-dock [prompt]] [--news-ticker <sym>] [--news-article <n>] [--news-favorite <n>] [--news-url <url>] [--section <name>] [--journal-sample <file>] [--myfxbook-sample <file>] [--settings [0|1]] [--sources-tab 0|1] [--sources-disc [floor:1|ai:0|access:2|open:source:3]] [--help-page [chapter]] [--ml-train <csv> <target>] [--scroll <0..1>|--scroll-end] --delay <ms> --screenshot out.png   # render a page (a country sheet, a heatmap, the Sources page) and exit
```

Working rules:
- The graph covers `src/`, `tests/` and the docs; `third_party/`, `i18n/`, `build/` and data files are excluded
  (`.graphifyignore`). A `Stop` hook (`.claude/hooks/graphify-update.sh`) runs `graphify update .` in the background
  at the end of every turn that changed a source or doc file, so the graph is current at the start of the next
  turn; within a long turn, run `graphify update .` yourself before querying code you just edited. Code changes
  are AST-only (no tokens); after editing README.md or another doc, run `/graphify . --update` once for its
  semantic nodes. `graphify-out/` is not committed (a rebuild takes a few seconds).
- `graphify explain "Class::method()"`, `graphify affected "function"` and `graphify path "A" "B"` are the reliable
  queries; free-text `graphify query` is keyword-based and noisy. Method calls through a pointer
  (`MarketState::instance()->publishRegime(...)`) and Qt `connect()` links are not call edges: grep for those.
- UI checks run the real app, which writes the user's own QSettings: export `com.deepwater.Deepwater` with
  `defaults export` before a test launch and import it back after, then `defaults delete` the keys the launch added
  (`defaults import` keeps them); never leave a launch without a timeout.
- The whole UI is translated (`i18n/deepwater_fr.ts`, refreshed with `cmake --build build --target deepwater_lupdate`;
  keep it at 0 unfinished). Strings saved on disk (regime names, model names) are stored in English and translated
  when shown; see README › Translations.
