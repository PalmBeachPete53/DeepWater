// Dumps reference outputs of quant-terminal/public/lib/{series,indicators,seasonals}.js to
// tests/quant/core/reference/reference.json, for the C++ port to be checked against (qtm_test_core).
//
//   node tests/quant/core/make_reference.mjs
//
// Numbers go out as base64 of little-endian Float64 arrays ("f64:<base64>"), so the comparison is
// bit for bit and NaN/Infinity survive. Bar sets are written the same way, so both sides read the
// exact same input.
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const here = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(here, "../../..");
const qt = path.join(root, "quant-terminal");
const outDir = path.join(here, "reference");
fs.mkdirSync(path.join(outDir, "custom"), { recursive: true });
fs.mkdirSync(path.join(outDir, "problems"), { recursive: true });

// lib/imports.js pulls in lib/store.js, which creates its data folder: keep that out of the repo.
process.env.QT_DATA_DIR = fs.mkdtempSync(path.join(os.tmpdir(), "qtm-ref-"));

const I = await import(pathToFileURL(path.join(qt, "public/lib/indicators.js")).href);
const S = await import(pathToFileURL(path.join(qt, "public/lib/series.js")).href);
const { seasonals } = await import(pathToFileURL(path.join(qt, "public/lib/seasonals.js")).href);
const imports = await import(pathToFileURL(path.join(qt, "lib/imports.js")).href);

// ---------------------------------------------------------------- encoding
const f64 = (arr) => {
  const a = new Float64Array(arr.length);
  for (let i = 0; i < arr.length; i++) {
    const v = arr[i];
    a[i] = typeof v === "number" ? v : typeof v === "boolean" ? (v ? 1 : 0) : NaN;
  }
  return "f64:" + Buffer.from(a.buffer).toString("base64");
};
const num = (v) => (typeof v === "number" ? f64([v]) : v);
const barsOut = (bars) => ({
  time: f64(bars.map((b) => b.time)),
  open: f64(bars.map((b) => b.open)),
  high: f64(bars.map((b) => b.high)),
  low: f64(bars.map((b) => b.low)),
  close: f64(bars.map((b) => b.close)),
  volume: f64(bars.map((b) => b.volume ?? 0)),
  color: bars.map((b) => b.color ?? ""),
});
// Same split as the C++ IndicatorResult: number arrays -> values, markers, colors, breaks, the rest -> extra.
const isNumArray = (v) =>
  Array.isArray(v) && v.every((x) => x == null || typeof x === "number" || typeof x === "boolean");
const resultOut = (res) => {
  const out = { values: {}, extra: {} };
  if (!res || typeof res !== "object") return out;
  for (const [k, v] of Object.entries(res)) {
    if (k === "markers" && Array.isArray(v))
      out.markers = v.map((m) => ({
        time: num(Number(m.time)),
        position: m.position ?? "",
        shape: m.shape ?? "",
        color: m.color ?? "",
        text: m.text == null ? "" : String(m.text),
      }));
    else if (k === "colors" && Array.isArray(v)) out.colors = v.map((c) => (c == null ? "" : String(c)));
    else if (k === "breaks" && Array.isArray(v)) out.breaks = v.map(Boolean);
    else if (isNumArray(v)) out.values[k] = f64(v);
    else if (typeof v !== "function" && v !== undefined) out.extra[k] = v;
  }
  return out;
};
const defOut = (d) => ({
  id: d.id,
  name: d.name,
  group: d.group,
  overlay: d.overlay,
  inputs: (d.inputs || []).map((i) => ({
    key: i.key,
    label: i.label ?? "",
    type: i.type,
    default: i.default,
    options: i.options ?? [],
    ...(i.min !== undefined ? { min: i.min } : {}),
    ...(i.max !== undefined ? { max: i.max } : {}),
  })),
  outputs: (d.outputs || []).map((o) => ({
    key: o.key,
    label: o.label ?? "",
    kind: o.kind ?? "line",
    color: o.color ?? "",
    lineWidth: o.lineWidth ?? 0,
    dots: !!o.dots,
  })),
  levels: d.levels ?? [],
  noAutoscale: !!d.noAutoscale,
  volumeFormat: !!d.volumeFormat,
});

// ---------------------------------------------------------------- bar sets
let seed = 12345;
const rand = () => (seed = (seed * 16807) % 2147483647) / 2147483647;
function synth(n, t0, step, { price = 100, vol = 1000, flatEvery = 0, zeroVol = () => false, round = 2 } = {}) {
  const out = [];
  for (let i = 0; i < n; i++) {
    const open = price;
    price = Math.max(1, price * (1 + (rand() - 0.5) * 0.03));
    let close = Number(price.toFixed(round));
    if (i % 17 === 5 && out.length) close = out.at(-1).close; // unchanged closes
    let high = Number((Math.max(open, close) * (1 + rand() * 0.008)).toFixed(round));
    let low = Number((Math.min(open, close) * (1 - rand() * 0.008)).toFixed(round));
    let o = Number(open.toFixed(round));
    if (flatEvery && i % flatEvery === 3) high = low = o = close; // flat bar
    out.push({
      time: t0 + i * step,
      open: o,
      high,
      low,
      close,
      volume: zeroVol(i) ? 0 : Math.round(vol * (0.2 + rand() * 1.6)),
    });
  }
  return out;
}
const btc = JSON.parse(fs.readFileSync(path.join(qt, "test/fixtures/btc-1d.json"), "utf8"));
const sets = {
  btc,
  // 30-minute bars over ~10 days from Wednesday 28 Jan 2026: day, week and month changes.
  intraday: synth(480, Date.UTC(2026, 0, 28, 0) / 1000, 1800, { flatEvery: 23 }),
  // 15-minute bars that start mid-session (pivots auto -> day).
  m15: synth(300, Date.UTC(2025, 11, 30, 13, 15) / 1000, 900, { price: 50, round: 4 }),
  // Hourly bars over 40 days (pivots auto -> week, VWAP weeks and months).
  hourly: synth(960, Date.UTC(2025, 10, 20, 0) / 1000, 3600, { price: 2000 }),
  weekly: S.resample(btc, "1W"),
  short: btc.slice(0, 8),
  one: btc.slice(0, 1),
  empty: [],
  // Volume 0 on a whole stretch and here and there, with flat bars.
  zerovol: synth(150, Date.UTC(2023, 2, 1) / 1000, 86400, {
    flatEvery: 11,
    zeroVol: (i) => i < 30 || i % 7 === 0,
  }),
  flat: Array.from({ length: 40 }, (_, i) => ({
    time: Date.UTC(2024, 0, 1) / 1000 + i * 86400,
    open: 10,
    high: 10,
    low: 10,
    close: 10,
    volume: i % 3 ? 5 : 0,
  })),
};
// Daily bars on weekdays only over 2014..2026, for seasonals (gaps at weekends and a missing year).
{
  const days = [];
  let price = 30;
  for (let t = Date.UTC(2014, 2, 3) / 1000; t < Date.UTC(2026, 9, 20) / 1000; t += 86400) {
    const d = new Date(t * 1000);
    if (d.getUTCDay() === 0 || d.getUTCDay() === 6) continue;
    if (d.getUTCFullYear() === 2019) continue; // a hole: 2020 does not chain from 2019
    const open = price;
    price = Math.max(1, price * (1 + (rand() - 0.49) * 0.04));
    days.push({ time: t, open, high: Math.max(open, price) * 1.01, low: Math.min(open, price) * 0.99, close: price, volume: 100 });
  }
  sets.daily = days;
}
const varSets = {
  btcTail: btc.slice(-250),
  intraday: sets.intraday,
};

// ---------------------------------------------------------------- indicators
function variations(def, bars) {
  const out = [];
  for (const inp of def.inputs) {
    let vals = [];
    if (inp.type === "int")
      vals = [1, 2, 5, 50].filter(
        (v) => v !== inp.default && (inp.min === undefined || v >= inp.min) && (inp.max === undefined || v <= inp.max),
      );
    else if (inp.type === "float") vals = [0, 0.5, 3.7].filter((v) => v !== inp.default);
    else if (inp.type === "select") vals = inp.options.filter((o) => o !== inp.default);
    else if (inp.type === "source")
      vals = ["close", "open", "high", "low", "hl2", "hlc3", "ohlc4", "hlcc4", "volume"].filter((o) => o !== inp.default);
    else if (inp.type === "time" && bars.length)
      vals = [bars[5].time, bars[Math.floor(bars.length / 2)].time + 1, bars[0].time - 100, bars.at(-1).time + 100];
    for (const v of vals) out.push({ [inp.key]: v });
  }
  return out;
}

function run(id, setName, bars, params) {
  try {
    return { id, set: setName, params, result: resultOut(I.compute(id, bars, params)) };
  } catch (e) {
    return { id, set: setName, params, error: e.message };
  }
}

const computes = [];
const builtinIds = I.INDICATORS.map((d) => d.id);
for (const id of builtinIds)
  for (const [name, bars] of Object.entries(sets)) if (name !== "daily") computes.push(run(id, name, bars, {}));
for (const def of I.INDICATORS)
  for (const [name, bars] of Object.entries(varSets))
    for (const p of variations(def, bars)) computes.push(run(def.id, name, bars, p));
// Out-of-range settings (0 and negative lengths, negative factors): whatever the JS gives, error included.
for (const def of I.INDICATORS)
  for (const [name, bars] of Object.entries({ short: sets.short, btcTail: varSets.btcTail }))
    for (const inp of def.inputs) {
      const vals = inp.type === "int" ? [0, -3] : inp.type === "float" ? [-1] : [];
      for (const v of vals) computes.push(run(def.id, name, bars, { [inp.key]: v }));
    }
// A few multi-parameter cases.
const extraCases = [
  ["macd", { fast: 5, slow: 35, signal: 5, source: "hl2" }],
  ["ichimoku", { conversion: 3, base: 7, spanB: 20, displacement: 1 }],
  ["ichimoku", { displacement: 40 }],
  ["alligator", { jawOffset: 0, teethOffset: 2, lipsOffset: 0 }],
  ["pivotshl", { left: 3, right: 1 }],
  ["pivotshl", { left: 0, right: 0 }],
  ["zigzag", { deviation: 1, depth: 1 }],
  ["zigzag", { deviation: 2.5, depth: 3 }],
  ["bb", { length: 5, mult: 1.25, source: "ohlc4" }],
  ["stoch", { k: 5, smoothK: 3, d: 5 }],
  ["markov", { lookback: 5, threshold: 1, minHistory: 5 }],
  ["supertrend", { atrLength: 3, factor: 1.5 }],
  ["vstop", { length: 3, factor: 0.5 }],
  ["psar", { start: 0.01, inc: 0.03, max: 0.5 }],
  ["vwap", { anchor: "nonsense" }],
  ["rsi", { maLength: 0 }],
  ["volume", { maLength: 0 }],
];
for (const [id, p] of extraCases)
  for (const [name, bars] of Object.entries({ ...varSets, hourly: sets.hourly })) computes.push(run(id, name, bars, p));

// ta helpers directly, on a source with NaN holes.
const holey = btc.slice(0, 120).map((b, i) => (i % 13 === 4 || i < 3 ? NaN : b.close));
const taCases = [];
const taRun = (name, args, fn) => {
  try {
    const v = fn();
    taCases.push({ name, args, value: Array.isArray(v) ? f64(v) : v });
  } catch (e) {
    taCases.push({ name, args, error: e.message });
  }
};
for (const src of [["close", btc.map((b) => b.close).slice(0, 120)], ["holey", holey]]) {
  for (const n of [1, 2, 3, 14, 200]) {
    for (const f of ["sma", "ema", "rma", "wma", "hma", "dema", "tema", "alma", "linreg", "stdev", "dev", "highest", "lowest", "change", "sum", "rsi"])
      taRun(f, [src[0], n], () => I.ta[f](src[1], n));
    taRun("vwma", [src[0], n], () => I.vwma(src[1], btc.slice(0, 120).map((b) => b.volume), n));
    taRun("linreg", [src[0], n, 2], () => I.linreg(src[1], n, 2));
    taRun("alma", [src[0], n, 0.5, 3], () => I.alma(src[1], n, 0.5, 3));
  }
}
const fast = I.ema(holey, 5), slow = I.sma(holey, 9);
taCases.push({ name: "crossover", args: ["holey"], value: f64(I.crossover(fast, slow)) });
taCases.push({ name: "crossunder", args: ["holey"], value: f64(I.crossunder(fast, slow)) });
taCases.push({ name: "crossover", args: ["holey", 40000], value: f64(I.crossover(holey, 40000)) });
taCases.push({ name: "crossunder", args: ["holey", 40000], value: f64(I.crossunder(holey, 40000)) });
for (const [name, bars] of Object.entries(sets)) {
  taCases.push({ name: "tr", args: [name], value: f64(I.tr(bars)) });
  taCases.push({ name: "atr", args: [name, 14], value: f64(I.atr(bars, 14)) });
  for (const s of ["close", "open", "high", "low", "volume", "hl2", "hlc3", "ohlc4", "hlcc4", "bogus"])
    taCases.push({ name: "source", args: [name, s], value: f64(I.source(bars, s)) });
}

// periodKey
const periodKeys = [];
for (const t of [0, 1, 86399, 86400, -1, -86400, 951782400, 951868800, 1709164800, 1735689599, 1735689600, 1767225600 + 3600 * 13, 4102444800, 1234567.5, -62135596800, 253402300799])
  for (const p of ["day", "week", "month", "year", "other"]) periodKeys.push({ t: num(t), period: p, key: I.periodKey(t, p) });

// ---------------------------------------------------------------- series.js
const series = { parseTf: [], bucketStart: [], resample: [], nativeBase: [], transforms: [], toCsv: [] };
for (const tf of ["1s", "30s", "59s", "1m", "5m", "15m", "1h", "4h", "1D", "3D", "1W", "2W", "1M", "3M", "m", "D", " 4h ", "0m", "00h", "007m", "banana", "1d", "1Y", "", "1.5h", "-1m", "1 m", "12"]) {
  try {
    const r = S.parseTf(tf);
    series.parseTf.push({ tf, n: r.n, unit: r.unit, seconds: r.seconds, intraday: S.isIntraday(tf) });
  } catch (e) {
    series.parseTf.push({ tf, error: e.message });
  }
}
for (const t of [0, 1, 59, 86399, 1705449600, 1705449600 + 3 * 86400 + 4000, 1760000000, 1767225599, -86400 * 3, 951782400 + 7200])
  for (const tf of ["1s", "1m", "5m", "15m", "1h", "4h", "1D", "3D", "1W", "2W", "3W", "1M", "2M", "3M", "12M", "banana"]) {
    try {
      series.bucketStart.push({ t, tf, value: S.bucketStart(t, tf) });
    } catch (e) {
      series.bucketStart.push({ t, tf, error: e.message });
    }
  }
for (const [name, bars] of Object.entries({ btc, intraday: sets.intraday, hourly: sets.hourly, m15: sets.m15, empty: [] }))
  for (const tf of ["1h", "4h", "1D", "2D", "1W", "2W", "1M", "3M"])
    series.resample.push({ set: name, tf, bars: barsOut(S.resample(bars, tf)) });
const natives = [
  ["1m", "5m", "15m", "1h", "4h", "1D", "1W", "1M"],
  ["1m", "1h", "1D"],
  ["5m", "1D"],
  ["1s", "1m"],
  ["1D", "1W"],
  [],
];
for (const list of natives)
  for (const tf of ["7m", "2h", "3D", "1W", "3M", "1M", "45m", "90m", "2W", "30s", "1D", "5m", "banana"]) {
    try {
      series.nativeBase.push({ tf, natives: list, value: S.nativeBase(tf, list) });
    } catch (e) {
      series.nativeBase.push({ tf, natives: list, error: e.message });
    }
  }
const tfSets = { btc, intraday: sets.intraday, short: sets.short, one: sets.one, empty: [], flat: sets.flat, zerovol: sets.zerovol };
for (const [name, bars] of Object.entries(tfSets)) {
  const add = (fn, args, f) => series.transforms.push({ fn, set: name, args, bars: barsOut(f()) });
  // Box sizes in proportion to the price, so a set never turns into millions of bricks.
  const px = bars.length ? bars[0].close : 100;
  const big = Number((px * 0.03).toPrecision(2)), small = Number((px * 0.004).toPrecision(2));
  add("heikinAshi", [], () => S.heikinAshi(bars));
  add("renko", [], () => S.renko(bars));
  add("renko", [big], () => S.renko(bars, big));
  add("renko", [small], () => S.renko(bars, small));
  add("lineBreak", [], () => S.lineBreak(bars));
  add("lineBreak", [1], () => S.lineBreak(bars, 1));
  add("lineBreak", [5], () => S.lineBreak(bars, 5));
  add("rangeBars", [], () => S.rangeBars(bars));
  add("rangeBars", [big], () => S.rangeBars(bars, big));
  add("rangeBars", [small], () => S.rangeBars(bars, small));
  add("kagi", [], () => S.kagi(bars));
  add("kagi", [big], () => S.kagi(bars, big));
  add("kagi", [small], () => S.kagi(bars, small));
  add("pointFigure", [], () => S.pointFigure(bars));
  add("pointFigure", [big, 3], () => S.pointFigure(bars, big, 3));
  add("pointFigure", [small, 1], () => S.pointFigure(bars, small, 1));
  add("pointFigure", [small, 2], () => S.pointFigure(bars, small, 2));
}
const csvCase = (bars, cols, csvBars = bars) =>
  series.toCsv.push({
    bars: barsOut(bars),
    cols: cols.map(([k, v]) => [k, f64(v)]),
    csv: S.toCsv(csvBars, Object.fromEntries(cols)),
  });
csvCase(btc.slice(0, 5), []);
csvCase(btc.slice(0, 6), [
  ["rsi", [1, 2.5, 3, NaN, 5e-7]],
  ["big", [1e21, -0, Infinity, 123456789012345680000, 0.1 + 0.2, 7, 8]],
  ["short", [1]],
]);
// Renko bricks have no volume; the C++ Bar holds 0 for that, so compare with volume 0.
csvCase(S.renko(btc).slice(0, 4), [], S.renko(btc).slice(0, 4).map((b) => ({ ...b, volume: 0 })));
csvCase(sets.zerovol.slice(0, 10), [["x", sets.zerovol.slice(0, 10).map((b, i) => b.close / (i - 3))]]);
csvCase(
  [
    { time: 1e-7, open: 1e21, high: 1.5e-7, low: -0.000001, close: 2 ** 60, volume: 0.1 },
    { time: 253402300799, open: -1e-7, high: 1e300, low: 5e-324, close: 123.456, volume: 1e21 },
  ],
  [],
);

// ---------------------------------------------------------------- seasonals
const seasonalCases = [];
for (const name of ["daily", "btc", "short", "empty", "weekly"]) seasonalCases.push({ set: name, value: seasonals(sets[name]) });

// ---------------------------------------------------------------- custom indicators
// The shipped examples, a generated signal indicator, and malformed files. Loaded like server.js loadCustom.
const custom = [];
const sig = {
  changes: [
    [btc[10].time, 1],
    [btc[40].time + 3600, 0],
    [btc[41].time, -1],
    [btc[300].time, 1],
    [sets.intraday[100].time, -1],
    [sets.intraday[200].time + 60, 1],
  ],
};
fs.writeFileSync(path.join(outDir, "custom", "signal-test.js"), imports.signalIndicatorSource("TEST", sig, "test.csv"));
const customFiles = [
  ["ema-ribbon-squeeze.js", path.join(qt, "custom/ema-ribbon-squeeze.js")],
  ["indicator-template.js", path.join(qt, "docs/indicator-template.js")],
  ["alert-example.js", path.join(qt, "docs/alert-example.js")],
  ["signal-test.js", path.join(outDir, "custom", "signal-test.js")],
];
// An indicator that uses most of the ta helpers, including compute(), macd, supertrend, psar and dmi.
fs.writeFileSync(
  path.join(outDir, "custom", "ta-tour.js"),
  `// Exercises the ta helpers from a custom indicator.
export default {
  name: "ta tour",
  overlay: false,
  inputs: [
    { key: "length", type: "int", default: 10, min: 1 },
    { key: "source", label: "Source", type: "source", default: "hlc3" },
    { key: "mode", label: "Mode", type: "select", options: ["a", "b"], default: "a" },
  ],
  outputs: [
    { key: "a", kind: "line", color: "#2962ff" },
    { key: "b", kind: "histogram", color: "#ff9800", lineWidth: 2 },
  ],
  levels: [0, 50],
  calc(bars, p, ta) {
    const s = ta.source(bars, p.source);
    const parts = [
      ta.sma(s, p.length), ta.ema(s, p.length), ta.rma(s, p.length), ta.wma(s, p.length),
      ta.vwma(s, ta.source(bars, "volume"), p.length), ta.hma(s, p.length), ta.dema(s, p.length),
      ta.tema(s, p.length), ta.alma(s, p.length), ta.alma(s, p.length, 0.5, 4), ta.linreg(s, p.length),
      ta.linreg(s, p.length, 3), ta.stdev(s, p.length), ta.dev(s, p.length), ta.highest(s, p.length),
      ta.lowest(s, p.length), ta.change(s), ta.change(s, 3), ta.sum(s, p.length), ta.rsi(s), ta.rsi(s, 5),
      ta.atr(bars), ta.atr(bars, 5), ta.tr(bars),
    ];
    const m = ta.macd(s), m2 = ta.macd(s, 5, 10, 3);
    const st = ta.supertrend(bars), st2 = ta.supertrend(bars, 2, 5);
    const ps = ta.psar(bars), ps2 = ta.psar(bars, 0.01, 0.01, 0.1);
    const d = ta.dmi(bars), d2 = ta.dmi(bars, 7, 5);
    const bb = ta.compute("bb", bars, { length: p.length });
    const pat = ta.compute("patterns", bars);
    const ich = ta.compute("ichimoku", bars);
    const up = ta.crossover(parts[0], parts[1]), dn = ta.crossunder(parts[0], 100), x = ta.crossover(s, parts[4]);
    const a = bars.map((_, i) => {
      let t = 0;
      for (const q of parts) t += Number.isFinite(q[i]) ? q[i] : 0;
      return t + (m.macd[i] || 0) + (m.signal[i] || 0) + (m.hist[i] || 0) + (m2.hist[i] || 0) + (st.st[i] || 0) * st.dir[i]
        + (st2.st[i] || 0) + (ps[i] || 0) + (ps2[i] || 0) + (d.adx[i] || 0) + (d.plus[i] || 0) + (d2.minus[i] || 0)
        + (bb.upper[i] || 0) + (ich.spanA[i] || 0) + (up[i] ? 1 : 0) + (dn[i] ? 2 : 0) + (x[i] ? 4 : 0);
    });
    const b = p.mode === "a" ? parts[19] : bars.map((b, i) => (i % 3 ? null : b.close));
    return {
      a, b, rsi: parts[19], ups: up, mcolors: m.colors.length, ichExtend: ich.extendBars,
      colors: b.map((v) => (v > 50 ? "#26a69a" : v < 50 ? "#ef5350" : null)),
      markers: pat.markers.concat(bars.filter((_, i) => up[i]).map((bar) => ({ time: bar.time, position: "inBar", shape: "square", text: 42 }))),
      info: { n: bars.length, label: "tour", nested: [1, 2, { x: "y" }] },
    };
  },
};
`,
);
customFiles.push(["ta-tour.js", path.join(outDir, "custom", "ta-tour.js")]);

async function loadLikeServer(file, full) {
  const id = `custom:${file.replace(/\.js$/, "")}`;
  try {
    const mod = await import(`${pathToFileURL(full).href}?v=${fs.statSync(full).mtimeMs}`);
    return { def: I.registerIndicator({ ...mod.default, id }) };
  } catch (e) {
    return { error: e.message };
  }
}
for (const [file, full] of customFiles) {
  const { def, error } = await loadLikeServer(file, full);
  const entry = { file, path: path.relative(root, full), error: error ?? null, computes: [] };
  if (def) {
    entry.def = defOut(def);
    for (const [name, bars] of Object.entries(sets)) if (name !== "daily") entry.computes.push(run(def.id, name, bars, {}));
    for (const [name, bars] of Object.entries(varSets))
      for (const p of variations(def, bars)) entry.computes.push(run(def.id, name, bars, p));
  }
  custom.push(entry);
}

// Malformed files: each is checked with indicatorProblems(mod.default) and loaded like the server does.
const good = `name: 'Ok',
  inputs: [{ key: 'length', type: 'int', default: 14 }],
  outputs: [{ key: 'value', kind: 'line', color: '#2962ff' }],
  calc: (bars, p, ta) => ({ value: ta.sma(ta.source(bars), p.length) })`;
const problemFiles = {
  "good.js": `export default { ${good} };`,
  "no-calc.js": `export default { ${good}, calc: undefined };`,
  "no-name.js": `export default { ${good}, name: '' };`,
  "blank-name.js": `export default { ${good}, name: '   ' };`,
  "long-name.js": `export default { ${good}, name: '${"x".repeat(81)}' };`,
  "name-number.js": `export default { ${good}, name: 5 };`,
  "overlay.js": `export default { ${good}, overlay: 'yes' };`,
  "int-default.js": `export default { ${good}, inputs: [{ key: 'length', type: 'int', default: 2.5 }] };`,
  "select-default.js": `export default { ${good}, inputs: [{ key: 'ma', type: 'select', options: ['SMA'], default: 'EMA' }] };`,
  "select-options.js": `export default { ${good}, inputs: [{ key: 'ma', type: 'select', options: [], default: 'EMA' }, { key: 'mb', type: 'select', options: ['a', 1], default: 'a' }, { key: 'mc', type: 'select', default: 'a' }] };`,
  "source-key.js": `export default { ${good}, inputs: [{ key: 'src', type: 'source', default: 'close' }] };`,
  "source-default.js": `export default { ${good}, inputs: [{ key: 'source', type: 'source', default: 'typical' }] };`,
  "bad-type.js": `export default { ${good}, inputs: [{ key: 'on', type: 'bool', default: true }] };`,
  "bad-keys.js": `export default { ${good}, inputs: [{ key: '1x', type: 'int', default: 1 }, { type: 'int', default: 1 }, null, 5, { key: 'a', type: 'float', default: 'x' }, { key: 'a', type: 'float', default: Infinity }, { key: 'b', type: 'time', default: -1 }, { key: 'c', type: 'time', default: 0, min: 'x', max: NaN }, { key: 'd', type: 'int', default: 3, min: 1, max: 9 }] };`,
  "inputs-object.js": `export default { ${good}, inputs: { a: 1 } };`,
  "kind.js": `export default { ${good}, outputs: [{ key: 'value', kind: 'area' }] };`,
  "outputs-bad.js": `export default { ${good}, outputs: [{ key: 'value', color: '  ' }, { key: 'w', lineWidth: 5 }, { key: 'x', lineWidth: 1.5 }, null, { key: 'y', color: 3 }] };`,
  "outputs-empty.js": `export default { ${good}, outputs: [] };`,
  "outputs-missing.js": `export default { name: 'Ok', calc: (bars) => ({ value: bars.map((b) => b.close) }) };`,
  "dup-output.js": `export default { ${good}, outputs: [{ key: 'a' }, { key: 'a' }], calc: (b) => ({ a: b.map(() => 1) }) };`,
  "short-output.js": `export default { ${good}, calc: (bars) => ({ value: bars.slice(1).map((b) => b.close) }) };`,
  "no-list.js": `export default { ${good}, calc: () => ({}) };`,
  "returns-number.js": `export default { ${good}, calc: () => 5 };`,
  "strings.js": `export default { ${good}, calc: (bars) => ({ value: bars.map((b, i) => (i === 7 ? '1' : b.close)) }) };`,
  "nulls.js": `export default { ${good}, calc: (bars) => ({ value: bars.map((b, i) => (i % 2 ? null : undefined)) }) };`,
  "throws.js": `export default { ${good}, calc: () => { throw new Error('boom'); } };`,
  "throws-string.js": `export default { ${good}, calc: () => { throw 'plain'; } };`,
  "markers.js": `export default { ${good}, calc: (bars) => ({ value: bars.map((b) => b.close), markers: [{ time: bars[0].time, position: 'top', shape: 'arrowUp' }] }) };`,
  "markers-object.js": `export default { ${good}, calc: (bars) => ({ value: bars.map((b) => b.close), markers: {} }) };`,
  "markers-ok.js": `export default { ${good}, calc: (bars) => ({ value: bars.map((b) => b.close), markers: [{ time: bars[3].time, position: 'inBar', shape: 'square' }] }) };`,
  "levels.js": `export default { ${good}, levels: ['70'] };`,
  "levels-ok.js": `export default { ${good}, levels: [30, 70.5] };`,
  "many.js": `export default { name: '', overlay: 1, inputs: [{ key: 'x', type: 'int', default: 1.5 }, { key: 'x', type: 'int', default: 1 }], outputs: [{ key: 'v', kind: 'dots' }], levels: 3 };`,
  "not-object.js": `export default 42;`,
  "array.js": `export default [1, 2];`,
  "no-default.js": `export const x = 1;`,
  "syntax.js": `export default {\n  name: 'x',\n  calc(bars) { return { value: bars.map(b => b.close) ; },\n};\n`,
  "reference.js": `const k = notDefined + 1;\nexport default { ${good} };\n`,
  "throws-at-load.js": `throw new Error('top-level failure');\nexport default { ${good} };\n`,
  "uses-p.js": `export default { name: 'P', inputs: [{ key: 'n', type: 'int', default: 3 }, { key: 'm', label: 'M', type: 'select', options: ['x', 'y'], default: 'y' }, { key: 't', type: 'time', default: 0 }], calc(bars, p) { return { value: bars.map(() => p.n + (p.m === 'y' ? 0.5 : 0) + p.t) }; } };`,
};
const problems = [];
for (const [file, code] of Object.entries(problemFiles)) {
  const full = path.join(outDir, "problems", file);
  fs.writeFileSync(full, code);
  const entry = { file, path: path.relative(root, full) };
  try {
    const mod = await import(pathToFileURL(full).href);
    entry.problems = I.indicatorProblems(mod.default);
    entry.problemsNoRun = I.indicatorProblems(mod.default, { run: false });
  } catch (e) {
    entry.importError = e.message;
  }
  const r = await loadLikeServer(file, full);
  entry.loadError = r.error ?? null;
  if (r.def) {
    entry.def = defOut(r.def);
    entry.computes = [run(r.def.id, "short", sets.short, {}), run(r.def.id, "intraday", sets.intraday, {})];
  }
  problems.push(entry);
}

// ---------------------------------------------------------------- write
const out = {
  generatedWith: `node ${process.version}`,
  thisYear: new Date().getUTCFullYear(),
  sets: Object.fromEntries(Object.entries({ ...sets, btcTail: varSets.btcTail }).map(([k, v]) => [k, barsOut(v)])),
  indicators: I.INDICATORS.filter((d) => builtinIds.includes(d.id)).map(defOut),
  defaults: Object.fromEntries(builtinIds.map((id) => [id, I.defaults(id)])),
  computes,
  ta: { holey: f64(holey), close120: f64(btc.map((b) => b.close).slice(0, 120)), volume120: f64(btc.slice(0, 120).map((b) => b.volume)), cases: taCases },
  periodKeys,
  series,
  seasonals: seasonalCases,
  custom,
  problems,
  unknown: (() => {
    try {
      I.compute("nope", btc);
    } catch (e) {
      return e.message;
    }
  })(),
};
const json = JSON.stringify(out, (k, v) => (typeof v === "number" && !Number.isFinite(v) ? num(v) : v));
fs.writeFileSync(path.join(outDir, "reference.json"), json);
console.log(`wrote ${path.join(outDir, "reference.json")}: ${(json.length / 1e6).toFixed(1)} MB, ${computes.length} indicator runs`);
fs.rmSync(process.env.QT_DATA_DIR, { recursive: true, force: true });
