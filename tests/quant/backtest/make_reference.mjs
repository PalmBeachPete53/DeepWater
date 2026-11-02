// Dumps reference outputs of quant-terminal/public/lib/{backtest,quant}.js to
// tests/quant/backtest/reference/reference.json, for the C++ port to be checked against
// (qtm_test_backtest).
//
//   node tests/quant/backtest/make_reference.mjs
//
// Every number goes out as base64 of a little-endian Float64 ("f64:<base64>"), so the comparison
// is bit for bit and NaN/Infinity survive; bar sets are written the same way so both sides read
// the exact same input. A JS `undefined` field is left out, as JSON.stringify does.
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const here = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(here, "../../..");
const qt = path.join(root, "quant-terminal");
const B = await import(pathToFileURL(path.join(qt, "public/lib/backtest.js")).href);
const Q = await import(pathToFileURL(path.join(qt, "public/lib/quant.js")).href);

// ---------------------------------------------------------------- encoding
const f64 = (arr) => {
  const a = new Float64Array(arr.length);
  for (let i = 0; i < arr.length; i++) a[i] = arr[i];
  return "f64:" + Buffer.from(a.buffer).toString("base64");
};
const enc = (v) => {
  if (typeof v === "number") return f64([v]);
  if (Array.isArray(v)) return v.map((x) => (x === undefined ? null : enc(x)));
  if (v && typeof v === "object") {
    const o = {};
    for (const [k, x] of Object.entries(v)) if (x !== undefined && typeof x !== "function") o[k] = enc(x);
    return o;
  }
  return v;
};
const barsOut = (bars) => ({
  time: f64(bars.map((b) => b.time)),
  open: f64(bars.map((b) => b.open)),
  high: f64(bars.map((b) => b.high)),
  low: f64(bars.map((b) => b.low)),
  close: f64(bars.map((b) => b.close)),
  volume: f64(bars.map((b) => b.volume ?? 0)),
});
const attempt = (fn) => {
  try {
    return { ok: enc(fn()) };
  } catch (e) {
    return { error: e.message };
  }
};

// ---------------------------------------------------------------- bar sets
let seed = 12345;
const rnd = () => ((seed = (seed * 16807) % 2147483647) / 2147483647);
const walk = (n, start, step, drift, vol, p0 = 100) => {
  const out = [];
  let p = p0;
  for (let i = 0; i < n; i++) {
    const o = p;
    p = Math.max(1, p * (1 + drift(i) + (rnd() - 0.5) * vol));
    const c = p;
    const h = Math.max(o, c) * (1 + rnd() * vol * 0.6);
    const l = Math.min(o, c) * (1 - rnd() * vol * 0.6);
    out.push({ time: start + i * step, open: +o.toFixed(4), high: +h.toFixed(4), low: +l.toFixed(4), close: +c.toFixed(4), volume: Math.round(rnd() * 1e6) });
  }
  return out;
};
const T0 = 1600000000 - (1600000000 % 86400);
const sets = {};
sets.btc = JSON.parse(fs.readFileSync(path.join(qt, "test/fixtures/btc-1d.json"), "utf8"));
sets.trend = walk(600, T0, 86400, (i) => (i < 400 ? 0.004 : -0.006), 0.03);
sets.chop = walk(500, T0, 86400, (i) => 0.02 * Math.sin(i / 6), 0.025);
sets.short = walk(40, T0, 86400, () => 0.002, 0.03);
sets.flat = Array.from({ length: 300 }, (_, i) => ({ time: T0 + i * 86400, open: 100, high: 100, low: 100, close: 100, volume: 1 }));
sets.one = sets.short.slice(0, 1);
sets.two = sets.short.slice(0, 2);
sets.empty = [];
sets.big = walk(3000, T0, 3600, (i) => 0.0004 * Math.sin(i / 200), 0.012);
// 4h bars built from 1h sub-bars, for the magnifier
sets.sub1h = walk(1600, T0, 3600, (i) => 0.0006 * Math.sin(i / 90), 0.012);
sets.h4 = [];
for (let i = 0; i + 4 <= sets.sub1h.length; i += 4) {
  const g = sets.sub1h.slice(i, i + 4);
  sets.h4.push({ time: g[0].time, open: g[0].open, high: Math.max(...g.map((x) => x.high)), low: Math.min(...g.map((x) => x.low)), close: g[3].close, volume: g.reduce((a, x) => a + x.volume, 0) });
}
sets.subHalf = sets.sub1h.slice(0, 800);

// ---------------------------------------------------------------- strategies
const EX = B.EXAMPLE_STRATEGIES;
const ema = (length) => ({ id: "ema", params: { length } });
const both = {
  indicators: { fast: ema(9), slow: ema(21) },
  entryLong: [{ left: "fast", op: "crossesAbove", right: "slow" }],
  exitLong: [{ left: "fast", op: "crossesBelow", right: "slow" }],
  entryShort: [{ left: "fast", op: "crossesBelow", right: "slow" }],
  exitShort: [{ left: "fast", op: "crossesAbove", right: "slow" }],
};
const longOnly = { indicators: both.indicators, entryLong: both.entryLong, exitLong: both.exitLong };
const shortOnly = { indicators: both.indicators, entryShort: both.entryShort, exitShort: both.exitShort };
const rsiBoth = {
  indicators: { rsi: { id: "rsi", params: { length: 14 } } },
  entryLong: [{ left: "rsi", op: "crossesAbove", right: 35 }],
  exitLong: [{ left: "rsi", op: ">", right: 60 }, { left: "close", op: "<", right: "open" }],
  entryShort: [{ left: "rsi", op: "crossesBelow", right: 65 }],
  exitShort: [{ left: "rsi", op: "<", right: 40 }, { left: "close", op: ">", right: "open" }],
};
const options = [
  {},
  { exitMatch: "any" },
  { exitMatch: "ANY" },
  { maxBarsInTrade: 3 },
  { maxBarsInTrade: 10, exitMatch: "any" },
  { stopLossPct: 3 },
  { takeProfitPct: 5 },
  { trailingStopPct: 4 },
  { stopLossPct: 2, takeProfitPct: 4, trailingStopPct: 3 },
  { feePct: 0, slippagePct: 0 },
  { feePct: 0.3, slippagePct: 0.2 },
  { spreadPct: 0.4 },
  { spreadPct: 0.4, stopLossPct: 2, takeProfitPct: 3 },
  { positionPct: 50 },
  { positionPct: 250 },
  { initialCapital: 1000 },
  { stopLossPct: "3", feePct: null, intrabar: "nope" },
];
const intrabars = ["worst", "ohlc", "magnifier"];

const cases = [];
const add = (name, set, strategy, sub) => cases.push({ name, set, strategy, sub });
for (const [n, s] of Object.entries(EX))
  for (const set of ["btc", "trend", "chop", "short", "flat", "one", "two", "empty", "h4"]) add(`${n} @${set}`, set, s);
let k = 0;
for (const [bn, base] of Object.entries({ both, longOnly, shortOnly, rsiBoth }))
  for (const o of options)
    for (const ib of intrabars)
      for (const set of ["btc", "trend", "chop", "h4"]) {
        const sub = set === "h4" && ib === "magnifier" ? (k++ % 2 ? "sub1h" : "subHalf") : undefined;
        add(`${bn} ${JSON.stringify(o)} ${ib} @${set}${sub ? "+" + sub : ""}`, set, { ...base, ...o, intrabar: o.intrabar ?? ib }, sub);
      }
// magnifier without sub-bars, sub-bars with ohlc (ignored)
add("magnifier no sub", "h4", { ...both, stopLossPct: 1, takeProfitPct: 1, intrabar: "magnifier" });
add("ohlc ignores sub", "h4", { ...both, stopLossPct: 1, takeProfitPct: 1, intrabar: "ohlc" }, "sub1h");
// every operator, both ways round
for (const op of B.OPS)
  for (const set of ["btc", "chop"])
    add(`op ${op} @${set}`, set, {
      indicators: { m: { id: "sma", params: { length: 20 } }, m2: { id: "sma", params: { length: 50 } } },
      entryLong: [{ left: "close", op, right: "m" }],
      exitLong: [{ left: "m", op, right: "m2" }],
      entryShort: [{ left: "m2", op, right: "close" }],
      exitShort: [{ left: "close", op, right: "m2" }],
      stopLossPct: 5,
    });
// price fields
for (const f of ["open", "high", "low", "close", "volume", "hl2", "hlc3", "ohlc4", "hlcc4"])
  add(`field ${f}`, "btc", {
    indicators: { ma: { id: "sma", params: { length: 10, source: f } } },
    entryLong: [{ left: f, op: "crossesAbove", right: "ma" }],
    exitLong: [{ left: f, op: "crossesBelow", right: "ma" }],
  });
// numbers
for (const n of [30, "30", " 45 ", "4e1", "0x2A", "0b101010", "0o52", ".5e2", "Infinity", "-Infinity", "-5", "+42", "1e400", "", " ", "12abc", "inf"])
  add(`number ${JSON.stringify(n)}`, "btc", {
    indicators: { rsi: { id: "rsi", params: {} } },
    entryLong: [{ left: "rsi", op: "crossesBelow", right: n }],
    exitLong: [{ left: "rsi", op: "crossesAbove", right: 60 }],
  });
// indicator outputs
const outs = [
  [{ id: "macd", params: {} }, ["macd.macd", "macd.signal"], ["macd.hist", 0]],
  [{ id: "bb", params: { length: 20, mult: 2 } }, ["close", "bb.lower"], ["close", "bb.basis"]],
  [{ id: "supertrend", params: { atrLength: 10, factor: 3 } }, ["x.direction", 0], ["x.direction", 0]],
  [{ id: "supertrend", params: {} }, ["close", "x"], ["close", "x.down"]],
  [{ id: "stoch", params: {} }, ["x.k", "x.d"], ["x.k", 80]],
  [{ id: "dmi", params: {} }, ["x.plus", "x.minus"], ["x.adx", 40]],
  [{ id: "ichimoku", params: {} }, ["close", "x.spanA"], ["close", "x.base"]],
  [{ id: "zigzag", params: {} }, ["close", "x"], ["close", "x"]],
  [{ id: "rsi", params: {}, output: "ma" }, ["x", 45], ["x.value", 60]],
];
for (const [spec, a, b] of outs) {
  const key = a[0].startsWith("macd") || b[0].startsWith("macd") ? "macd" : a[1] === "bb.lower" ? "bb" : "x";
  add(`output ${spec.id} ${a.join(" ")}`, "btc", {
    indicators: { [key]: spec },
    entryLong: [{ left: a[0], op: "crossesAbove", right: a[1] }],
    exitLong: [{ left: b[0], op: "crossesBelow", right: b[1] }],
    entryShort: [{ left: a[0], op: "crossesBelow", right: a[1] }],
    exitShort: [{ left: b[0], op: "crossesAbove", right: b[1] }],
  });
}
// drawn lines on btc: from bar 100 to bar 400
const bt = sets.btc;
const L = (extend, i1 = 100, i2 = 400, dp = 1.3) => {
  const l = { t1: bt[i1].time, p1: bt[i1].close, t2: bt[i2].time, p2: bt[i2].close * dp };
  if (extend !== undefined) l.extend = extend;
  return { line: l };
};
for (const [n, line] of Object.entries({
  none: L("none"), right: L("right"), both: L("both"), missing: L(undefined), nul: L(null),
  reversed: L("right", 400, 100), reversedNone: L("none", 400, 100), vertical: L("both", 300, 300), verticalRight: L("right", 300, 300),
  noT1: { line: { p1: 1, t2: bt[300].time, p2: 2 } },
  falsyLine: { line: 0 },
}))
  add(`line ${n}`, "btc", {
    indicators: {},
    entryLong: [{ left: "close", op: "crossesAbove", right: line }],
    exitLong: [{ left: "close", op: "crossesBelow", right: line }],
    entryShort: [{ left: line, op: "crosses", right: "close" }],
    exitShort: [{ left: "close", op: ">", right: line }],
  });
// bad strategies
const bad = {
  "unknown operand": { entryLong: [{ left: "foo", op: ">", right: 1 }] },
  "unknown output": { indicators: { macd: { id: "macd" } }, entryLong: [{ left: "macd.nope", op: ">", right: 1 }] },
  "unknown indicator": { indicators: { x: { id: "nope" } }, entryLong: [{ left: "x", op: ">", right: 1 }] },
  "unknown indicator in exit": { indicators: { x: { id: "nope" } }, exitShort: [{ left: 1, op: ">", right: "x.y" }] },
  "bad op": { entryLong: [{ left: "close", op: "=>", right: 1 }] },
  "bad op missing": { entryLong: [{ left: "close", right: 1 }] },
  "bad op never reached": { entryLong: [{ left: "close", op: "<", right: 0 }, { left: "close", op: "??", right: 1 }] },
  "bad op on NaN": { indicators: { m: { id: "sma", params: { length: 5000 } } }, entryLong: [{ left: "m", op: "??", right: 1 }] },
  "bad op in exit": { entryLong: [{ left: "close", op: ">", right: 0 }], exitLong: [{ left: "close", op: "=", right: 1 }] },
  "entryLong null": { entryLong: null },
  "entryLong string": { entryLong: "abc" },
  "entryLong object": { exitShort: {} },
  "rule null": { entryLong: [null] },
  "rule number": { entryLong: [5] },
  "operand null": { entryLong: [{ left: null, op: ">", right: 1 }] },
  "operand true": { entryLong: [{ left: true, op: ">", right: 1 }] },
  "operand array": { entryLong: [{ left: [1, 2], op: ">", right: 1 }] },
  "operand object": { entryLong: [{ left: { a: 1 }, op: ">", right: 1 }] },
  "spec string": { indicators: { x: "ema" }, entryLong: [{ left: "x", op: ">", right: 1 }] },
  "indicators null": { indicators: null, entryLong: [{ left: "close", op: ">", right: 1 }] },
  "output empty": { indicators: { x: { id: "ema", output: "" } }, entryLong: [{ left: "x.", op: ">", right: 1 }] },
  "extra dots": { indicators: { x: { id: "macd" } }, entryLong: [{ left: "x.signal.more", op: ">", right: 0 }] },
};
for (const [n, s] of Object.entries(bad)) add(`bad ${n}`, "btc", s);
// the JS test suite's own scenarios
const day = (i, o, h, l, c) => ({ time: 1700000000 + i * 86400, open: o, high: h, low: l, close: c, volume: 1 });
sets.t_spread = [day(0, 100, 100, 100, 100), day(1, 100, 100, 100, 100), day(2, 100, 100, 100, 100), day(3, 100, 100, 100, 100)];
sets.t_bid = [day(0, 100, 100, 100, 100), day(1, 100, 100, 98.15, 99), day(2, 99, 99, 99, 99)];
sets.t_both = [day(0, 100, 100, 100, 100), day(1, 100, 102.5, 97, 100), day(2, 100, 100, 100, 100)];
sets.t_subDown = [
  { time: sets.t_both[1].time, open: 100, high: 100, low: 97, close: 97.5 },
  { time: sets.t_both[1].time + 43200, open: 97.5, high: 102.5, low: 97.5, close: 100 },
];
sets.t_subUp = [
  { time: sets.t_both[1].time, open: 100, high: 102.5, low: 100, close: 101 },
  { time: sets.t_both[1].time + 43200, open: 101, high: 101, low: 97, close: 100 },
];
sets.t_gap = [day(0, 100, 100, 100, 100), day(1, 100, 100, 99, 99), day(2, 95, 96, 94, 95), day(3, 95, 95, 95, 95)];
const noCosts = { feePct: 0, slippagePct: 0 };
const always = [{ left: "close", op: ">", right: 0 }];
add("t spread", "t_spread", { ...noCosts, spreadPct: 0.2, entryLong: always, exitLong: always });
add("t bid", "t_bid", { ...noCosts, stopLossPct: 2, entryLong: always });
add("t bid spread", "t_bid", { ...noCosts, stopLossPct: 2, spreadPct: 0.2, entryLong: always });
for (const ib of intrabars)
  for (const sub of [undefined, "t_subDown", "t_subUp"]) {
    add(`t both ${ib} ${sub}`, "t_both", { ...noCosts, stopLossPct: 2, takeProfitPct: 2, intrabar: ib, entryLong: always }, sub);
    add(`t both short ${ib} ${sub}`, "t_both", { ...noCosts, stopLossPct: 2, takeProfitPct: 2, trailingStopPct: 1, intrabar: ib, entryShort: always }, sub);
  }
for (const ib of intrabars) add(`t gap ${ib}`, "t_gap", { ...noCosts, stopLossPct: 2, intrabar: ib, entryLong: always });

const backtests = cases.map((c) => ({
  ...c,
  strategy: c.strategy,
  result: attempt(() => {
    const r = B.backtest(sets[c.set], c.strategy, { subBars: c.sub ? sets[c.sub] : null });
    return { ...r, csv: B.tradesCsv(r.trades) };
  }),
}));

// ---------------------------------------------------------------- conditionFn
const condCases = [];
{
  const inds = { fast: ema(9), slow: ema(21), macd: { id: "macd", params: {} }, st: { id: "supertrend", params: {}, output: "direction" } };
  const rules = [
    { left: "fast", op: "crossesAbove", right: "slow" },
    { left: "fast", op: "crosses", right: "slow" },
    { left: "macd.hist", op: ">", right: 0 },
    { left: "macd.macd", op: "crossesBelow", right: "macd.signal" },
    { left: "st", op: "<", right: "0" },
    { left: "close", op: ">=", right: "hl2" },
    { left: "low", op: "<=", right: L("both").line ? L("both") : 0 },
    { left: "close", op: "crossesAbove", right: L("right") },
  ];
  const run = (bars, inds, rules, cache) => {
    const make = B.conditionFn(bars, inds, cache);
    return rules.map((r) => bars.map((_, i) => make(r)(i)));
  };
  condCases.push({ name: "rules btc", set: "btc", indicators: inds, rules, out: run(sets.btc, inds, rules) });
  // A shared cache: the second call keeps the first one's "fast" (ema 9), as in JS.
  const cache = {};
  const first = run(sets.btc, inds, rules.slice(0, 1), cache);
  const inds2 = { fast: ema(30), slow: ema(5) };
  const second = run(sets.btc, inds2, rules.slice(0, 1), cache);
  const fresh = run(sets.btc, inds2, rules.slice(0, 1));
  condCases.push({ name: "shared cache", set: "btc", indicators: inds, indicators2: inds2, rules: rules.slice(0, 1), first, second, fresh });
  // the JS test: price crossing a drawn line
  sets.t_line = [5, 6, 7, 12, 13].map((c, i) => ({ time: i * 10, open: c, high: c, low: c, close: c, volume: 1 }));
  const rising = { line: { t1: 0, p1: 8, t2: 40, p2: 12, extend: "right" } };
  condCases.push({ name: "line test", set: "t_line", indicators: {}, rules: [{ left: "close", op: "crossesAbove", right: rising }], out: run(sets.t_line, {}, [{ left: "close", op: "crossesAbove", right: rising }]) });
  // bad rules: errors at make time or at evaluation
  for (const r of [{ left: "nope", op: ">", right: 1 }, { left: "close", op: "x", right: 1 }, { left: "fast.zz", op: ">", right: 1 }]) {
    let err = null;
    try {
      B.conditionFn(sets.btc, inds)(r)(sets.btc.length - 1);
    } catch (e) {
      err = e.message;
    }
    condCases.push({ name: `bad ${JSON.stringify(r)}`, set: "btc", indicators: inds, rules: [r], error: err });
  }
}

// ---------------------------------------------------------------- linePrice, magnifierTf
const lines = [
  { t1: 100, p1: 10, t2: 200, p2: 20, extend: "none" },
  { t1: 100, p1: 10, t2: 200, p2: 20, extend: "right" },
  { t1: 100, p1: 10, t2: 200, p2: 20, extend: "both" },
  { t1: 100, p1: 10, t2: 200, p2: 20 },
  { t1: 200, p1: 20, t2: 100, p2: 10, extend: "right" },
  { t1: 200, p1: 20, t2: 100, p2: 10, extend: "none" },
  { t1: 150, p1: 10, t2: 150, p2: 20, extend: "right" },
  { t1: 150, p1: 10, t2: 150, p2: 20, extend: "none" },
  { t1: 1.5e9, p1: 30123.45, t2: 1.6e9 + 0.3, p2: 41234.567, extend: "both" },
  { t1: 100, p1: 10, extend: "both" },
  { p1: 10, p2: 20, extend: "none" },
  { t1: 100, p1: 10, t2: 200, p2: 20, extend: null },
];
const times = [-1e10, 0, 50, 99.999, 100, 125, 150, 175.5, 200, 200.001, 250, 1e10, 1.55e9, NaN];
const linePrices = lines.map((l) => ({ line: l, out: enc(times.map((t) => B.linePrice(l)(t))) }));
const magnifier = [];
for (const tf of [1, 60, 120, 300, 900, 3600, 14400, 86400, 604800])
  for (const span of [0, 3600, 86400 * 10, 1000 * 3600, 500 * 3600, 300 * 3600, 86400 * 365, 86400 * 365 * 10, 1e9])
    for (const max of [20000, 500]) {
      const m = B.magnifierTf(tf, span, max);
      magnifier.push({ tf, span: enc(span), max, out: m ? enc(m) : null });
    }

// ---------------------------------------------------------------- quant maths
const xs = [-40, -8, -3.3, -1.96, -1, -0.5, -1e-9, 0, 1e-9, 0.25, 0.5, 1, 1.96, 2.5, 3.3, 8, 40, NaN, Infinity];
const ps = [-1, 0, 1e-12, 0.001, 0.01, 0.02425, 0.0243, 0.2, 0.5, 0.8, 0.97575, 0.98, 0.999, 1 - 1e-12, 1, 2, NaN];
const s16807 = (s) => () => ((s = (s * 16807) % 2147483647) / 2147483647) - 0.5;
const r1 = s16807(42);
const series = {
  steady: Array.from({ length: 500 }, (_, i) => 0.002 + 0.001 * Math.sin(i)),
  noise: Array.from({ length: 2000 }, () => r1() * 0.02),
  short: [0.01, -0.02, 0.03],
  nine: [1, 2, 3, 4, 5, 6, 7, 8, 9].map((x) => x / 100),
  ten: [1, -2, 3, -4, 5, -6, 7, -8, 9, 10].map((x) => x / 100),
  flat: Array(50).fill(0.001),
  one: [0.5],
  empty: [],
};
const maths = {
  normCdf: enc(xs.map(Q.normCdf)),
  normInv: enc(ps.map(Q.normInv)),
  xs: enc(xs),
  ps: enc(ps),
  series: Object.fromEntries(Object.entries(series).map(([k, r]) => [k, {
    r: enc(r),
    moments: enc(Q.moments(r)),
    sharpe: enc(Q.sharpe(r)),
    psr0: enc(Q.psr(r)),
    psr1: enc(Q.psr(r, 0.05)),
  }])),
  ems: [[0, 0.01], [1, 0.01], [2, 0.01], [10, 0.01], [25, 0.003], [100, 0.01], [7, 0], [7, -1], [3.5, 0.2]].map(([n, v]) => ({ n: enc(n), v: enc(v), out: enc(Q.expectedMaxSharpe(n, v)) })),
};
{
  const T = 400;
  const good = Array.from({ length: T }, (_, i) => 0.01 + 0.002 * Math.sin(i));
  const others = Array.from({ length: 5 }, (_, k) => Array.from({ length: T }, (_, i) => 0.001 * Math.sin(i * (k + 2))));
  let s = 7;
  const r = () => ((s = (s * 16807) % 2147483647) / 2147483647) - 0.5;
  const noise = Array.from({ length: 12 }, () => Array.from({ length: T }, () => r()));
  const mats = { good: [good, ...others], noise, single: [good], shortT: [good.slice(0, 39), others[0].slice(0, 39)], edge: [good.slice(0, 40), others[0].slice(0, 40)], flat: [Array(100).fill(0), Array(100).fill(0), good.slice(0, 100)] };
  maths.pbo = Object.entries(mats).map(([name, m]) => ({ name, matrix: enc(m), out: enc(Q.pbo(m)), out6: enc(Q.pbo(m, 6)) }));
  const trades = [5, -2, 3, -1, 8, -4, 2, 1, -3, 6];
  maths.mc = [
    [trades, 2000, 7], [trades, 300, 7], [trades, 1, 7], [trades, 0, 7], [trades, 50, 123456789], [trades, 50, -5], [trades, 50, 4294967303.7],
    [trades.slice(0, 4), 100, 7], [trades.slice(0, 5), 100, 7], [series.noise.map((x) => x * 100), 500, 7], [[-100, 50, 20, -30, 10, 0], 200, 99],
  ].map(([t, runs, sd]) => ({ trades: enc(t), runs, seed: enc(sd), out: enc(Q.monteCarlo(t, runs, sd)) }));
}

// ---------------------------------------------------------------- quantChecks
const qCases = [];
const qadd = (name, set, strategy, mcRuns, sub) => qCases.push({ name, set, strategy, mcRuns, sub, order: Object.keys(strategy.indicators || {}) });
qadd("EMA cross default runs", "btc", EX["EMA cross (9/21)"], undefined);
for (const [n, s] of Object.entries(EX)) qadd(`${n} @btc`, "btc", s, 300);
for (const set of ["trend", "chop", "short", "flat", "two", "big"]) qadd(`EMA @${set}`, set, EX["EMA cross (9/21)"], 300);
qadd("RSI @trend", "trend", EX["RSI mean reversion (30/55)"], 300);
qadd("MACD @chop", "chop", EX["MACD + 200 EMA trend filter"], 300);
qadd("both @btc", "btc", both, 500);
qadd("rsiBoth @chop", "chop", rsiBoth, 500);
qadd("magnifier @h4", "h4", { ...both, stopLossPct: 1.5, takeProfitPct: 2.5, intrabar: "magnifier" }, 300, "sub1h");
qadd("ohlc trailing @h4", "h4", { ...longOnly, trailingStopPct: 2, intrabar: "ohlc" }, 300);
const strat0 = { ...EX["EMA cross (9/21)"], slippagePct: 0 };
qadd("size 100", "btc", { ...strat0, positionPct: 100 }, 300);
qadd("size 20", "btc", { ...strat0, positionPct: 20 }, 300);
qadd("no fee no slip", "btc", { ...strat0, feePct: 0 }, 100);
qadd("spread", "btc", { ...EX["EMA cross (9/21)"], spreadPct: 0.2 }, 100);
qadd("no tunables", "btc", { indicators: {}, entryLong: [{ left: "close", op: "crossesAbove", right: 30000 }], exitLong: [{ left: "close", op: "crossesBelow", right: 30000 }] }, 100);
qadd("no trades", "btc", { indicators: { fast: ema(9), slow: ema(21) }, entryLong: [{ left: "fast", op: ">", right: 1e9 }] }, 100);
qadd("no trades flat", "flat", EX["EMA cross (9/21)"], 100);
qadd("three indicators, insertion order", "btc", {
  indicators: { zeta: { id: "sma", params: { length: 30 } }, alpha: { id: "ema", params: { length: 10 } }, mid: { id: "rsi", params: { length: 14 } } },
  entryLong: [{ left: "alpha", op: "crossesAbove", right: "zeta" }, { left: "mid", op: "<", right: 70 }],
  exitLong: [{ left: "alpha", op: "crossesBelow", right: "zeta" }],
}, 100);
qadd("index keys", "btc", {
  indicators: { b: ema(12), 10: ema(30), 2: { id: "sma", params: { length: 50 } } },
  entryLong: [{ left: "b", op: "crossesAbove", right: "2" }],
  exitLong: [{ left: "b", op: "crossesBelow", right: "10" }],
}, 100);
qadd("rsi two lengths", "btc", { indicators: { r: { id: "rsi", params: { length: 10, maLength: 6 } } }, entryLong: [{ left: "r", op: "crossesAbove", right: "r.ma" }], exitLong: [{ left: "r", op: "crossesBelow", right: "r.ma" }] }, 100);
qadd("dmi label", "btc", { indicators: { d: { id: "dmi", params: {} } }, entryLong: [{ left: "d.plus", op: "crossesAbove", right: "d.minus" }], exitLong: [{ left: "d.plus", op: "crossesBelow", right: "d.minus" }] }, 100);
// A non-integer base never is a variant (JS ===). On an indicator the rules don't use, because the
// JS indicators give all-NaN for a fractional length where the C++ port truncates it.
qadd("non-integer base", "btc", { indicators: { unused: { id: "bb", params: { length: 30.5 } }, fast: ema(9) }, entryLong: [{ left: "close", op: "crossesAbove", right: "fast" }], exitLong: [{ left: "close", op: "crossesBelow", right: "fast" }] }, 100);
// Same for a length given as a string ("9" !== 9); JS indicators would compute "9" + 1 = "91".
qadd("string param", "btc", { indicators: { unused: { id: "bb", params: { length: "30" } }, fast: ema(9) }, entryLong: [{ left: "close", op: "crossesAbove", right: "fast" }], exitLong: [{ left: "close", op: "crossesBelow", right: "fast" }] }, 100);
qadd("length 2 and 3", "chop", { indicators: { fast: ema(2), slow: ema(3) }, entryLong: EX["EMA cross (9/21)"].entryLong, exitLong: EX["EMA cross (9/21)"].exitLong }, 100);
qadd("unused indicator", "btc", { indicators: { unused: { id: "bb", params: { length: 30 } }, fast: ema(9) }, entryLong: [{ left: "close", op: "crossesAbove", right: "fast" }], exitLong: [{ left: "close", op: "crossesBelow", right: "fast" }] }, 100);
qadd("bad strategy", "btc", { indicators: {}, entryLong: [{ left: "nope", op: ">", right: 1 }] }, 100);
qadd("null spec unused", "btc", { indicators: { n: null }, entryLong: [{ left: "close", op: ">", right: 1 }] }, 100);
qadd("costs null", "btc", { ...EX["EMA cross (9/21)"], slippagePct: null, feePct: null, positionPct: null }, 100);
const quant = qCases.map((c) => ({ ...c, result: attempt(() => Q.quantChecks(sets[c.set], c.strategy, { mcRuns: c.mcRuns, subBars: c.sub ? sets[c.sub] : null })) }));

// timing: a quantChecks run on 3000 bars
const tStrat = EX["EMA cross (9/21)"];
Q.quantChecks(sets.big, tStrat);
const t0 = performance.now();
const reps = 5;
for (let i = 0; i < reps; i++) Q.quantChecks(sets.big, tStrat);
const timing = { quantChecks3000ms: (performance.now() - t0) / reps };
console.log(`node quantChecks on 3000 bars: ${timing.quantChecks3000ms.toFixed(1)} ms`);

const out = {
  sets: Object.fromEntries(Object.entries(sets).map(([k, v]) => [k, barsOut(v)])),
  examples: Object.entries(EX).map(([name, s]) => ({ name, strategy: s })),
  ops: B.OPS,
  intrabar: B.INTRABAR,
  backtests,
  conditions: condCases,
  linePrices,
  times: enc(times),
  magnifier,
  maths,
  quant,
  timing,
};
fs.writeFileSync(path.join(here, "reference/reference.json"), JSON.stringify(out));
console.log(`${backtests.length} backtests, ${condCases.length} condition cases, ${quant.length} quant checks`);
