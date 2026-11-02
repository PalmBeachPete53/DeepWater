# PineTS (vendored)

LuxAlgo's Pine Script® runtime for JavaScript: <https://github.com/LuxAlgo/PineTS>, npm package
`pinets`. Deepwater runs Pine Script indicators with it (Charts tab, `custom/*.pine` files, see
the README's "Pine Script indicators" section).

| File | What |
|---|---|
| `pinets.min.es.js` | `dist/pinets.min.es.js` of npm `pinets@0.13.1`, unmodified (the Node.js ES module build) |
| `LICENSE` | PineTS's licence: AGPL-3.0-only (dual licensed: a commercial licence is available from LuxAlgo) |
| `deepwater-pinets.mjs` | Deepwater's bridge (ours): run by Node.js as a child of the app, JSON lines on stdin/stdout; the protocol is documented at its top |

Both `.js` files are compiled into `qtm_core` as resources (`:/pinets/`) and copied to
`~/Library/Application Support/Deepwater/Deepwater/pinets/` before Node.js starts
(`src/quant/core/PineScript.cpp`).

## Licence note

PineTS is AGPL-3.0. Using it for personal work, research and internal tools carries no obligation;
distributing an application that includes it, or offering it as a network service, requires that
application's source to be released under the AGPL-3.0 as well, unless a commercial licence is
obtained from LuxAlgo (business@luxalgo.com). Deepwater is not affiliated with TradingView;
"Pine Script" and "TradingView" are trademarks of TradingView, Inc.

## Updating

1. `npm pack pinets@<version>` (or download the tarball from the npm registry), extract it, copy
   `package/dist/pinets.min.es.js` and `package/LICENSE` here.
2. Set `PINETS_VERSION` in `deepwater-pinets.mjs` and in `src/quant/core/PineScript.cpp`, and the
   version in the table above.
3. Run `build/tests-quant/pinets/qtm_test_pinets`: it checks the bridge protocol, the plot shapes
   and the numbers against the C++ `ta.*` port.

The bundle needs Node.js 18 or newer (PineTS builds for `node18`); the bridge uses `node:readline`
and top-level `await`.
