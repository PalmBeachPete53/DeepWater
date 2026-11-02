// Reference data for qtm_test_js: what Node gives for
//   - every probe of features.mjs, each wrapped in a custom indicator that imports it,
//   - the custom indicators in indicators/ (modern syntax a user may write), computed on bar sets,
//   - files with errors (the message Node records for them).
// All of it loaded the way quant-terminal/server.js loadCustom does: dynamic import, then
// registerIndicator({ ...mod.default, id }).
//
//   node tests/quant/js/make_reference.mjs
//
// Numbers go out as base64 of little-endian Float64 arrays ("f64:<base64>"), like
// tests/quant/core/make_reference.mjs, so the comparison is bit for bit.
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { FEATURES, SER, HELPER } from "./features.mjs";

const here = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(here, "../../..");
const qt = path.join(root, "quant-terminal");
const outDir = path.join(here, "reference");
fs.rmSync(outDir, { recursive: true, force: true });
for (const d of ["features", "errors"]) fs.mkdirSync(path.join(outDir, d), { recursive: true });
process.env.QT_DATA_DIR = fs.mkdtempSync(path.join(os.tmpdir(), "qtm-jsref-"));

const I = await import(pathToFileURL(path.join(qt, "public/lib/indicators.js")).href);

// ---------------------------------------------------------------- encoding (as in core/make_reference.mjs)
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
});
const isNumArray = (v) => Array.isArray(v) && v.every((x) => x == null || typeof x === "number" || typeof x === "boolean");
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
    else if (typeof v !== "function" && v !== undefined) out.extra[k] = JSON.parse(JSON.stringify(v));
  }
  return out;
};

// ---------------------------------------------------------------- bar sets
let seed = 4242;
const rand = () => (seed = (seed * 16807) % 2147483647) / 2147483647;
function synth(n, t0, step, price = 100) {
  const out = [];
  for (let i = 0; i < n; i++) {
    const open = price;
    price = Math.max(1, price * (1 + (rand() - 0.5) * 0.03));
    const close = Number(price.toFixed(2));
    out.push({
      time: t0 + i * step,
      open: Number(open.toFixed(2)),
      high: Number((Math.max(open, close) * (1 + rand() * 0.008)).toFixed(2)),
      low: Number((Math.min(open, close) * (1 - rand() * 0.008)).toFixed(2)),
      close: i % 11 === 4 && out.length ? out.at(-1).close : close,
      volume: i % 9 === 2 ? 0 : Math.round(1000 * (0.2 + rand() * 1.6)),
    });
  }
  return out;
}
const btc = JSON.parse(fs.readFileSync(path.join(qt, "test/fixtures/btc-1d.json"), "utf8"));
const sets = {
  btc,
  intraday: synth(400, Date.UTC(2026, 0, 28) / 1000, 1800),
  short: btc.slice(0, 8),
  one: btc.slice(0, 1),
  empty: [],
};

async function loadLikeServer(full, id) {
  try {
    const mod = await import(`${pathToFileURL(full).href}?v=${fs.statSync(full).mtimeMs}`);
    return { def: I.registerIndicator({ ...mod.default, id }) };
  } catch (e) {
    return { error: e.message };
  }
}
const tick = (ms) => new Promise((r) => setTimeout(r, ms));
const rel = (p) => path.relative(root, p);

// ---------------------------------------------------------------- features
const fdir = path.join(outDir, "features");
fs.writeFileSync(path.join(fdir, "probe-helper.js"), HELPER);
fs.writeFileSync(path.join(fdir, "ser.js"), `${SER}\nexport { ser };\n`);
const features = [];
for (const [i, f] of FEATURES.entries()) {
  const probe = path.join(fdir, `p${i}.js`), wrap = path.join(fdir, `wrap${i}.js`);
  fs.writeFileSync(probe, f.src);
  fs.writeFileSync(
    wrap,
    `import probe from "./p${i}.js";
import { ser } from "./ser.js";
const box = { r: "pending" };
if (probe && typeof probe.then === "function") probe.then((v) => { box.r = ser(v); }, (e) => { box.r = "!! rejected " + ser(e); });
else box.r = ser(probe);
export default { name: ${JSON.stringify(f.name)}, calc(bars) { return { value: bars.map(() => 0), probe: box.r }; } };
`,
  );
  const id = `custom:wrap${i}`;
  const { def, error } = await loadLikeServer(wrap, id);
  let want;
  if (error) want = "!! " + error;
  else {
    await tick(20);
    want = I.compute(def.id, sets.short, {}).probe;
  }
  features.push({ name: f.name, es: f.es, probe: rel(probe), wrap: rel(wrap), want });
}

// ---------------------------------------------------------------- indicators
function variations(def) {
  const out = [{}];
  for (const inp of def.inputs) {
    if (inp.type === "int") for (const v of [1, 2, 7, 30]) if (v !== inp.default && (inp.min === undefined || v >= inp.min)) out.push({ [inp.key]: v });
    if (inp.type === "float") for (const v of [0, 0.5, 3.7]) if (v !== inp.default) out.push({ [inp.key]: v });
    if (inp.type === "source") for (const v of ["open", "hl2", "volume"]) out.push({ [inp.key]: v });
  }
  return out;
}
// Plus one that lists the keys of every built-in's ta.compute() result, in order (spread and
// Object.keys show the order).
fs.writeFileSync(
  path.join(outDir, "key-order.js"),
  `// Generated by make_reference.mjs: the key order of ta.compute() results.
const IDS = ${JSON.stringify(I.INDICATORS.map((d) => d.id).filter((id) => !id.startsWith("custom:")))};
export default {
  name: "Key order",
  calc(bars, p, ta) {
    const keys = {};
    for (const id of IDS) keys[id] = Object.keys({ ...ta.compute(id, bars, {}) }).join(",");
    return { value: bars.map(() => 0), keys };
  },
};
`,
);
const indicators = [];
const idir = path.join(here, "indicators");
const indicatorFiles = fs.readdirSync(idir).filter((f) => f.endsWith(".js")).sort().map((f) => path.join(idir, f));
indicatorFiles.push(path.join(outDir, "key-order.js"));
for (const full of indicatorFiles) {
  const file = path.basename(full), id = `custom:${file.replace(/\.js$/, "")}`;
  const { def, error } = await loadLikeServer(full, id);
  const entry = { file, path: rel(full), error: error ?? null, computes: [] };
  if (def) {
    entry.def = { name: def.name, overlay: def.overlay, inputs: def.inputs.map((i) => i.key), outputs: def.outputs.map((o) => o.key), levels: def.levels ?? [] };
    for (const [name, bars] of Object.entries(sets))
      for (const params of variations(def)) {
        try {
          entry.computes.push({ set: name, params, result: resultOut(I.compute(id, bars, params)) });
        } catch (e) {
          entry.computes.push({ set: name, params, error: e.message });
        }
      }
  }
  indicators.push(entry);
}

// ---------------------------------------------------------------- errors
const good = `name: 'E', calc(bars) { return { value: bars.map((b) => b.close) }; }`;
const errorFiles = {
  "syntax-after-spread.js": `const o = { ...{ a: 1 } };\nexport default {\n  name: 'x',\n  calc(bars) { return { value: bars.map(b => b.close) ; },\n};\n`,
  "identifier-after-rest.js": `const { a, ...r } = { a: 1 };\nconst b = 1 c;\nexport default { ${good} };\n`,
  "end-after-class-field.js": `class A { x = 1; }\nexport default { ${good}\n`,
  "keyword-after-logical.js": `let q; q ??= 1;\nconst if = 2;\nexport default { ${good} };\n`,
  "rest-of-null.js": `export default { ${good.replace("return", "const { a, ...r } = null; return")} };\n`,
  "private-brand.js": `class C { #x = 1; static get(o) { return o.#x; } }\nexport default { ${good.replace("return", "C.get({}); return")} };\n`,
  "spread-ok.js": `const base = { name: 'Spread ok' };\nexport default { ...base, calc(bars) { return { value: bars.map(({ close, ...rest }) => close + Object.keys(rest).length) }; } };\n`,
};
const errors = [];
for (const [file, code] of Object.entries(errorFiles)) {
  const full = path.join(outDir, "errors", file);
  fs.writeFileSync(full, code);
  const { error } = await loadLikeServer(full, `custom:${file.replace(/\.js$/, "")}`);
  errors.push({ file, path: rel(full), error: error ?? null });
}

const out = {
  generatedWith: `node ${process.version}`,
  sets: Object.fromEntries(Object.entries(sets).map(([k, v]) => [k, barsOut(v)])),
  features,
  indicators,
  errors,
};
const json = JSON.stringify(out, (k, v) => (typeof v === "number" && !Number.isFinite(v) ? num(v) : v));
fs.writeFileSync(path.join(outDir, "reference.json"), json);
console.log(`wrote ${rel(path.join(outDir, "reference.json"))}: ${features.length} probes, ${indicators.length} indicators, ${errors.length} error files`);
fs.rmSync(process.env.QT_DATA_DIR, { recursive: true, force: true });
process.exit(0);
