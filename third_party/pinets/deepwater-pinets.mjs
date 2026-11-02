// Deepwater's bridge to PineTS (pinets.min.es.js next to this file), run by Node.js as a child
// process of the app: one JSON request per line on stdin, one JSON reply per line on stdout
// (src/quant/core/PineScript.cpp speaks the other end). Diagnostics go to stderr; nothing but
// replies goes to stdout.
//
// Requests ({ id, cmd, ... }) and replies ({ id, ok: true, ... } | { id, ok: false, error, kind }):
//   hello                                  -> { node, pinets }
//   transpile { source }                   -> { code, version, declaration }
//        code: the JavaScript PineTS runs for the script (an `async $ => { ... }` function over
//        `$.data` and `$.pine`), version: the `//@version=` of the source, declaration:
//        "indicator" | "strategy" | null.
//   run { source, bars, inputs, tickerId, timeframe, timezone }
//        bars: [[time (UTC seconds), open, high, low, close, volume], ...] in time order;
//        inputs: { <input title | varId | id>: value } overrides.
//     -> { version, declaration, indicator: { title, shorttitle, overlay, ... }, plots: [...], warnings }
//        plots[i]: { key, title, style, color, linewidth, overlay, values, colors?, texts? }
//        values: one entry per bar: a number (null where na), or true/false for char and shape
//        plots; colors: the per-bar colours when they vary (else absent); texts: per-bar texts of
//        char and shape plots when they vary.

import { createInterface } from 'node:readline';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { dirname, join } from 'node:path';

const PINETS_VERSION = '0.13.1';

// Keep stdout for replies: PineTS's log.* and warnings print through console.
for (const k of ['log', 'info', 'warn', 'debug']) console[k] = (...a) => process.stderr.write(a.map(String).join(' ') + '\n');

const here = dirname(fileURLToPath(import.meta.url));
const lib = await import(pathToFileURL(join(here, 'pinets.min.es.js')).href);
const { PineTS, Indicator } = lib;

// Prepared scripts by source (transpiling costs about as much as a run).
const prepared = new Map();
const MAX_PREPARED = 64;
function indicatorFor(source) {
    let ind = prepared.get(source);
    if (ind) {
        prepared.delete(source);
        prepared.set(source, ind);   // most recently used last
        return ind;
    }
    ind = Indicator.from(source);
    ind.prepare();
    prepared.set(source, ind);
    if (prepared.size > MAX_PREPARED) prepared.delete(prepared.keys().next().value);
    return ind;
}

function versionOf(source) {
    const m = /^\s*\/\/\s*@version\s*=\s*(\d+)/m.exec(source);
    return m ? Number(m[1]) : null;
}

// TradingView timeframe strings ("1", "60", "D", "W", "M") from the bar spacing, when the app
// doesn't say.
function timeframeOf(bars) {
    if (bars.length < 2) return 'D';
    const deltas = [];
    for (let i = 1; i < bars.length && deltas.length < 200; i++) deltas.push(bars[i][0] - bars[i - 1][0]);
    deltas.sort((a, b) => a - b);
    const s = deltas[deltas.length >> 1];
    if (s >= 28 * 86400 - 3 * 86400 && s <= 31 * 86400 + 3 * 86400) return 'M';
    if (s >= 7 * 86400) return `${Math.round(s / (7 * 86400))}W`.replace(/^1W$/, 'W');
    if (s >= 86400) return `${Math.round(s / 86400)}D`.replace(/^1D$/, 'D');
    if (s >= 60) return String(Math.round(s / 60));
    return `${Math.max(1, Math.round(s))}S`;
}

function candlesOf(bars) {
    const n = bars.length;
    const out = new Array(n);
    let span = 86400000;
    if (n >= 2) {
        const deltas = [];
        for (let i = 1; i < n && deltas.length < 200; i++) deltas.push(bars[i][0] - bars[i - 1][0]);
        deltas.sort((a, b) => a - b);
        span = deltas[deltas.length >> 1] * 1000;
    }
    for (let i = 0; i < n; i++) {
        const b = bars[i];
        const openTime = b[0] * 1000;
        const closeTime = (i + 1 < n ? bars[i + 1][0] * 1000 : openTime + span) - 1;
        out[i] = { openTime, closeTime, open: b[1], high: b[2], low: b[3], close: b[4], volume: b[5] ?? 0 };
    }
    return out;
}

function styleOf(options) {
    const s = options?.style;
    if (!s) return 'line';
    return String(s).replace(/^style_/, '');
}

function plotOut(key, plot, bars) {
    const options = plot.options ?? {};
    const style = styleOf(options);
    const flags = style === 'char' || style === 'shape';
    const n = bars.length;
    const data = plot.data ?? [];
    // One entry per bar in bar order; align by time when the lengths differ.
    let rows = data;
    if (data.length !== n) {
        const byTime = new Map();
        for (const d of data) byTime.set(d.time, d);
        rows = bars.map((b) => byTime.get(b[0] * 1000));
    }
    const values = new Array(n);
    const colors = new Array(n);
    const texts = new Array(n);
    let colorVaries = false, textVaries = false;
    const baseColor = options.color ?? null;
    const baseText = options.text ?? options.char ?? null;
    for (let i = 0; i < n; i++) {
        const d = rows[i];
        const v = d?.value;
        if (flags) values[i] = !!v && v !== 'false';
        else values[i] = typeof v === 'number' && Number.isFinite(v) ? v : typeof v === 'boolean' ? (v ? 1 : 0) : null;
        const c = d?.options?.color ?? baseColor;
        colors[i] = c;
        if (c !== baseColor) colorVaries = true;
        const t = d?.options?.text ?? d?.options?.char ?? baseText;
        texts[i] = t;
        if (t !== baseText) textVaries = true;
    }
    const out = {
        key,
        title: plot.title ?? key,
        style,
        color: baseColor,
        linewidth: options.linewidth ?? 0,
        overlay: options.overlay ?? true,
        location: options.location ?? null,
        shape: options.shape ?? null,
        char: options.char ?? null,
        text: options.text ?? null,
        values,
    };
    if (colorVaries) out.colors = colors;
    if (textVaries) out.texts = texts;
    return out;
}

async function run(req) {
    const bars = Array.isArray(req.bars) ? req.bars : [];
    const ind = indicatorFor(String(req.source ?? ''));
    ind.inputs = req.inputs && typeof req.inputs === 'object' ? req.inputs : {};
    const pine = new PineTS(candlesOf(bars), req.tickerId || 'CHART', req.timeframe || timeframeOf(bars));
    if (req.timezone) pine.setTimezone(String(req.timezone));
    const ctx = await pine.run(ind);
    const plots = [];
    for (const [key, plot] of Object.entries(ctx.plots ?? {})) {
        if (key.startsWith('__')) continue;   // drawings (labels, lines, boxes, tables, ...)
        plots.push(plotOut(key, plot, bars));
    }
    return {
        version: ctx.pineVersion ?? versionOf(req.source),
        declaration: ind.getDeclarationType(),
        indicator: ctx.indicator ?? null,
        inputs: ind.getInputsMeta(),
        plots,
        warnings: (ctx.warnings ?? []).map((w) => (typeof w === 'string' ? w : `${w.message}${w.bar != null ? ` (bar ${w.bar})` : ''}`)),
    };
}

async function handle(req) {
    switch (req.cmd) {
        case 'hello':
            return { node: process.version, pinets: PINETS_VERSION };
        case 'transpile': {
            const source = String(req.source ?? '');
            const ind = indicatorFor(source);
            const prep = ind.prepare();
            return { code: prep.fn.toString(), version: versionOf(source), declaration: ind.getDeclarationType() };
        }
        case 'run':
            return run(req);
        default:
            throw new Error(`Unknown command '${req.cmd}'`);
    }
}

const rl = createInterface({ input: process.stdin, crlfDelay: Infinity });
const queue = [];
let busy = false;
async function pump() {
    if (busy) return;
    busy = true;
    while (queue.length) {
        const line = queue.shift();
        if (!line.trim()) continue;
        let req;
        try {
            req = JSON.parse(line);
        } catch (e) {
            process.stdout.write(JSON.stringify({ id: null, ok: false, error: `Bad request: ${e.message}`, kind: 'SyntaxError' }) + '\n');
            continue;
        }
        let reply;
        try {
            reply = { id: req.id, ok: true, ...(await handle(req)) };
        } catch (e) {
            reply = { id: req.id, ok: false, error: String(e?.message ?? e), kind: e?.constructor?.name ?? 'Error' };
        }
        process.stdout.write(JSON.stringify(reply) + '\n');
    }
    busy = false;
}
rl.on('line', (line) => {
    queue.push(line);
    pump();
});
rl.on('close', () => process.exit(0));
