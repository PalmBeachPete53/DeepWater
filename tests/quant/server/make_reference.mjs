// Reference outputs of quant-terminal's server-side JS (lib/*.js), for qtm_test_server to compare
// against. Offline mode and a throwaway data folder, so nothing touches the network or the app.
//   node tests/quant/server/make_reference.mjs
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import { fileURLToPath } from "node:url";

const here = path.dirname(fileURLToPath(import.meta.url));
const QT = path.join(here, "..", "..", "..", "quant-terminal");
process.env.QT_OFFLINE = "1";
process.env.QT_DATA_DIR = fs.mkdtempSync(path.join(os.tmpdir(), "qtm-ref-"));
process.env.TZ = process.env.TZ || "UTC";

const P = await import(path.join(QT, "lib/providers.js"));
const F = await import(path.join(QT, "lib/feeds.js"));
const M = await import(path.join(QT, "lib/imports.js"));
const I = await import(path.join(QT, "lib/market-info.js"));
const T = await import(path.join(QT, "lib/ptq.js"));
const A = await import(path.join(QT, "lib/alerts.js"));
const { EXAMPLE_STRATEGIES } = await import(path.join(QT, "public/lib/backtest.js"));

const tryIt = async (fn) => {
  try {
    return { ok: await fn() };
  } catch (e) {
    return { error: e.message, status: e.status ?? null };
  }
};
const out = {};

// ---------- JS numbers and dates ----------
const numbers = [0, -0, 1, -1, 0.1, 0.5, 1 / 3, 2 / 3, 1e21, 1e20, 123456789012345680000, 1.5e-7, 1e-6, 0.000001234,
  1761696000, 1761696000000, 83000.12, 42776.1, 3.14159, -2.5e-10, 5e-324, 1.7976931348623157e308, 100, 1e7, 123.456e5];
out.numbers = numbers.map((n) => ({ n, s: String(n), j: JSON.stringify(n) }));
const dates = ["2026-10-14", "2026-10-14T14:30:00Z", "2026-10-14T10:30:00-04:00", "2026-10-14T14:30:00.123Z",
  "Wed, 29 Oct 2026 10:00:00 +0000", "Wed, 29 Oct 2026 10:00:00 GMT", "Tue, 28 Oct 2026 18:05:12 -0400",
  "29 Oct 2026 10:00:00 EST", "", "garbage", "Oct 28, 2026", "10/28/2026 09:15 AM", "Oct 28, 2026 4:05PM"];
out.dates = dates.map((s) => ({ s, t: Number.isNaN(Date.parse(s)) ? null : Date.parse(s) }));

// ---------- providers ----------
const spreads = ["BTCUSDT/ETHUSDT", "EUR/USD", "SOL/USDT", "AAPL - MSFT", "AAPL-MSFT", "BTC/USD", "SPREAD:US:AAPL - US:MSFT",
  "spread:a / b", "2 * AAPL", "AAPL*2", "ETH/BTC", "A / B / C", "", "  btcusdt / 2 ", "X + Y", "XAU/USD"];
out.parseSpread = spreads.map((s) => ({ s, r: P.parseSpread(s) }));
const symbols = ["btc", "BTCUSDT", "eth", "ETH-USD", "SOL/USDT", "AAPL", "aapl", "EURUSD", "eur/usd", "XAUUSD", "xau/usd",
  "GOLD", "^FTSE", "YAHOO:^FTSE", "VOD.L", "BRK-B", "YAHOO:GC=F", "NQ", "nq1!", "MNQ", "CME_MINI:NQ1!", "CME:ES1!", "ES",
  "MES1!", "BTCUSDT/ETHUSDT", "BTC/USDT", "ETH/BTC", "2 * AAPL", "1 / 2", "A/B/C / D", "", "  ", "BINANCE:BTC&interval=1m",
  "US:../../admin", ":::", "A B?C", "x".repeat(80), "csv:my_file", "COINBASE:BTC-USD", "KRAKEN:XBTUSD", "SOL-USD", "DOGE",
  "AB-USD", "spy", "AAPL - MSFT"];
out.resolve = [];
for (const s of symbols) out.resolve.push({ s, r: await tryIt(() => P.resolve(s)) });
out.krakenPairMap = Object.fromEntries(P.krakenPairMap({
  XXBTZUSD: { altname: "XBTUSD", wsname: "XBT/USD", status: "online" },
  XDGUSD: { altname: "XDGUSD", wsname: "XDG/USD", status: "online" },
  SOLUSD: { altname: "SOLUSD", wsname: "SOL/USD", status: "online" },
  OLDUSD: { altname: "OLDUSD", wsname: "OLD/USD", status: "delisted" },
  NOWS: { altname: "NOWS" },
  ETHEUR: { altname: "ETHEUR", wsname: "ETH/EUR", status: "reduce_only" },
}));
const ts = Array.from({ length: 30 }, (_, i) => 1760450400 + i * 3600 * 7);
const yahoo = {
  chart: {
    result: [{
      meta: { gmtoffset: -14400 },
      timestamp: [...ts, ts.at(-1)],
      indicators: { quote: [{
        open: ts.map((t, i) => (i === 3 ? null : 100 + i)).concat([200]),
        high: ts.map((t, i) => 101 + i).concat([201]),
        low: ts.map((t, i) => 99 + i).concat([199]),
        close: ts.map((t, i) => (i === 5 ? null : 100.5 + i)).concat([200.5]),
        volume: ts.map((t, i) => (i === 7 ? null : 1000 + i)).concat([5]),
      }] },
    }],
  },
};
out.parseYahoo = { daily: P.parseYahoo(yahoo, "1D"), intraday: P.parseYahoo(yahoo, "5m"),
  noData: (await tryIt(() => P.parseYahoo({ chart: { result: null, error: { description: "No data found, symbol may be delisted" } } }, "1D"))).error,
  empty: (await tryIt(() => P.parseYahoo({}, "1D"))).error };
const wall = [Date.UTC(2026, 9, 14, 9, 30), Date.UTC(2026, 0, 14, 9, 30), Date.UTC(2026, 2, 8, 9, 30), Date.UTC(2026, 10, 1, 1, 30),
  Date.UTC(2026, 10, 1, 15, 59), Date.UTC(2026, 6, 4, 23, 0)];
out.nyWallToUtc = wall.map((ms) => ({ ms, r: P.nyWallToUtc(ms) }));
out.frontContract = [];
for (const root of ["NQ", "MNQ", "ES"])
  for (const d of ["2026-10-15T12:00:00Z", "2026-12-09T12:00:00Z", "2026-12-10T14:00:00Z", "2027-03-12T12:00:00Z",
    "2026-01-02T12:00:00Z", "2029-12-31T23:00:00Z", "2026-06-11T13:29:00Z", "2026-06-11T13:31:00Z"])
    out.frontContract.push({ root, ms: Date.parse(d), r: F.frontContract(root, Date.parse(d)) });
out.inUsSession = [];
for (let t = Date.UTC(2026, 2, 6) / 1000; t < Date.UTC(2026, 2, 10) / 1000; t += 1800)
  out.inUsSession.push([t, F.inUsSession(t)]);
out.search = {};
for (const q of ["btc", "MNQ", "ES", "gold", "eur", "xa", "cme:nq1", "nasdaq", "", "e"]) out.search[q] = await P.search(q);

// ---------- market info ----------
const rss = `<?xml version="1.0"?><rss><channel><title>Feed</title>
<item><title><![CDATA[Bitcoin &amp; Ether rally]]></title><link>https://x.test/a</link><pubDate>Wed, 29 Oct 2026 10:00:00 +0000</pubDate><description><![CDATA[<p>BTC rose &quot;fast&quot; &#039;today&#39; &lt;b&gt;</p>]]></description></item>
<item><title>No date</title><link>https://x.test/b</link><description>${"word ".repeat(80)}</description></item>
<item><title></title><link>https://x.test/c</link></item>
<item>
  <title type="html">Solana &apos;news&apos;</title>
  <link>https://x.test/d</link>
  <pubDate>Tue, 28 Oct 2026 18:05:12 GMT</pubDate>
  <description>Short</description>
</item>
</channel></rss>`;
out.parseRss = I.parseRss(rss, "Test");

// ---------- imports ----------
const T0 = Date.UTC(2026, 9, 14, 13, 30) / 1000;
const minute = Array.from({ length: 40 }, (_, i) => {
  const c = 60000 + 50 * Math.sin(i / 20) + i;
  return { time: T0 + i * 60, open: c - 3, high: c + 10, low: c - 10, close: c, volume: 1 + (i % 7) };
});
const alpacaCsv = "symbol,timestamp,open,high,low,close,volume,trade_count,vwap\n" +
  minute.map((b) => `BTC/USD,${new Date(b.time * 1000).toISOString().replace("T", " ").replace(".000Z", "+00:00")},${b.open},${b.high},${b.low},${b.close},${b.volume},12,${b.close}`).join("\n");
const tv = minute.slice(0, 20).map((b) => [b.time, b.open, b.high, b.low, b.close, b.volume].join(",")).join("\n");
const mt = "Date;Time;Open;High;Low;Close;Volume\n" + minute.slice(0, 20).map((b) => {
  const d = new Date(b.time * 1000).toISOString();
  return `${d.slice(0, 10).replace(/-/g, ".")};${d.slice(11, 16)};${b.open};${b.high};${b.low};${b.close};${b.volume}`;
}).join("\n");
const csvs = {
  alpaca: alpacaCsv,
  tv,
  mt,
  tabs: "﻿time\tclose\n" + minute.slice(0, 12).map((b) => `${b.time * 1000}\t${b.close}`).join("\r\n"),
  quoted: '"Date","Open","High","Low","Close","Vol"\n' + minute.slice(0, 15).map((b) => `"${new Date(b.time * 1000).toISOString()}","${b.open}","${b.high}","${b.low}","${b.close}",""`).join("\n"),
  skips: "time,open,high,low,close\n# a comment\n" + minute.slice(0, 14).map((b, i) => `${b.time},${i === 3 ? "x" : b.open},${b.high},${b.low},${i === 5 ? "" : b.close}`).join("\n"),
  dupes: "time,close\n" + [...minute.slice(0, 12), minute[4]].map((b) => `${b.time},${b.close}`).join("\n"),
  daily: "date,close\n" + Array.from({ length: 12 }, (_, i) => `2026-01-${String(i + 1).padStart(2, "0")},${100 + i}`).join("\n"),
  weekly: "date,close\n" + Array.from({ length: 12 }, (_, i) => `${new Date(Date.UTC(2026, 0, 5 + 7 * i)).toISOString().slice(0, 10)},${100 + i}`).join("\n"),
  zone: "time,close\n" + Array.from({ length: 12 }, (_, i) => `2026-10-14T10:${String(i).padStart(2, "0")}:00-04:00,${i}`).join("\n"),
  badTime: "time,open,high,low,close\n14/10/2026,1,2,0,1\n",
  twoSymbols: alpacaCsv.replace(/\nBTC\/USD,(.+)$/m, "\nETH/USD,$1"),
  few: alpacaCsv.split("\n").slice(0, 5).join("\n"),
  noCols: "a,b\n1,2",
  empty: "\n\n# only a comment\n",
};
out.parseBars = {};
for (const [k, v] of Object.entries(csvs)) out.parseBars[k] = await tryIt(() => M.parseBars(v));
const times = ["2026-10-14 14:30:00", "2026-10-14T14:30:00Z", "2026-10-14T14:30:00.000Z", "2026-10-14 14:30:00+00:00",
  "2026-10-14T10:30:00-04:00", "1760452200", "1760452200000", "1760452200000000", "1760452200000000000", "2026.10.14",
  "2026-10-14", "14/10/2026", "2026/2/3 4:05", "2026-13-40", "0099-01-01", "12345", "1760452200.75", "2026-10-14T14:30:00 UTC",
  " 2026-10-14 ", "2026-10-14T14:30:00+0530"];
out.parseTime = times.map((s) => ({ s, t: Number.isNaN(M.parseTime(s)) ? null : M.parseTime(s) }));
out.parseTimeTod = [["2026.10.14", "14:30"], ["2026-10-14", " 09:05:07 "], ["2026-10-14", ""]].map(([d, t]) => {
  const r = M.parseTime(d, t);
  return { d, t, r: Number.isNaN(r) ? null : r };
});
out.tfLabel = [1, 30, 60, 300, 3600, 7200, 86400, 172800, 604800, 2592000, 90].map((s) => [s, M.tfLabel(s)]);
out.cleanName = [];
for (const n of ["btc 1m (Alpaca).csv", "my-file.TXT", "___", "", "a".repeat(50), "Éte data.csv"])
  out.cleanName.push({ n, r: await tryIt(() => M.cleanName(n)) });
const sigs = {
  basic: "time,position\n2026-10-14 13:30,0\n2026-10-14 13:31,1\n2026-10-14 13:32,1\n2026-10-14 13:33,short\n2026-10-14 13:34,flat\n",
  bad: "time,position\n2026-10-14 13:30,2\n",
  headerless: `${T0},1\n${T0 + 60},-1\n${T0 + 120},buy\n${T0 + 180},`,
  half: "time,signal\n2026-10-14 13:30,0.5\n2026-10-14 13:29,-0.5\n",
  noRows: "time,position\n",
};
out.parseSignals = {};
for (const [k, v] of Object.entries(sigs)) out.parseSignals[k] = await tryIt(() => M.parseSignals(v));
out.signalSource = M.signalIndicatorSource("TEST", M.parseSignals(sigs.basic), "test\nfile.csv");
out.signalStrategy = M.signalStrategy("custom:signal-test");

// ---------- ptq ----------
const tfs = ["1D", "1h", "15m", "7m", "1m", undefined];
out.translate = [];
for (const [name, s] of Object.entries(EXAMPLE_STRATEGIES))
  for (const tf of tfs) {
    const r = T.translate(s, { tf, name });
    r.manifest.id = "ID";
    out.translate.push({ name, tf: tf ?? null, r });
  }
const extra = [
  { indicators: {}, entryLong: [{ left: "close", op: "crosses", right: 10 }] },
  { ...EXAMPLE_STRATEGIES["EMA cross (9/21)"], exitLong: [{ left: "fast", op: "crossesBelow", right: "slow" }, { left: "close", op: "<", right: "slow" }], exitMatch: "any", maxBarsInTrade: 10 },
  { indicators: { m: { id: "macross", params: { type: "SMA" } }, v: { id: "volume" }, d: { id: "donchian" } },
    entryShort: [{ left: "m.slow", op: ">", right: "m.fast" }, { left: "v.ma", op: "<", right: "v" }, { left: "close", op: ">", right: "d.upper" }],
    exitShort: [{ left: "close", op: "<", right: "d.lower" }, { left: "close", op: "<", right: "d.basis" }], spreadPct: 0.1, takeProfitPct: 5, trailingStopPct: 3 },
  { indicators: { e: { id: "ema", params: { length: 900, source: "hl2" } }, r: { id: "rsi", params: { length: "14" } } },
    entryLong: [{ left: "e", op: ">", right: "hl2" }, { left: "r", op: "<", right: "30" }, { left: "nope", op: ">", right: { line: {} } }], maxBarsInTrade: 7 },
];
for (const [k, s] of extra.entries())
  for (const tf of ["1D", "15m", "1m", "4h"]) {
    const r = T.translate(s, { tf, name: k ? "x".repeat(90) : "" });
    r.manifest.id = "ID";
    out.translate.push({ name: `extra${k}`, tf, r });
  }
out.toTerm = [
  ["macd.signal", { macd: { id: "macd" } }], ["bb.upper", { bb: { id: "bb", params: { length: 20, mult: 2 } } }],
  [30, {}], ["30", {}], [" ", {}], ["close", {}], ["hl2", {}], ["fast", { fast: { id: "ema", params: { length: 900 } } }],
  ["x.y", {}], ["st", { st: { id: "supertrend" } }], ["bb", { bb: { id: "bb", params: { mult: 20 } } }],
].map(([o, inds]) => ({ o, inds, r: T.toTerm(o, inds) }));

// ---------- alerts ----------
out.describe = [
  { symbol: "BINANCE:BTCUSDT", tf: "1h", left: "close", op: "crossesAbove", right: 100000 },
  { symbol: "X", tf: "1D", conditions: [{ left: "rsi", op: ">", right: 70 }, { left: "close", op: "<", right: { line: { t1: 1 } } }], match: "any" },
  { symbol: "X", tf: "5m", conditions: [{ left: "a", op: "weird", right: 1.5 }, { left: "b", op: "<=", right: "c" }] },
  { symbol: "", watchlist: "Main", tf: "1D", message: "Hello", left: "close", op: ">", right: 0 },
  { symbol: "Y", tf: "1D", message: "Plain", left: "close", op: ">", right: 0 },
  { symbol: "Z", tf: 15, conditions: [], left: "close", op: "<", right: 0.000001 },
].map((a) => ({ a, r: A.describe(a, a.watchlist ? "BINANCE:ETHUSDT" : a.symbol) }));

fs.writeFileSync(path.join(here, "reference", "reference.json"), JSON.stringify(out, null, 1));
fs.rmSync(process.env.QT_DATA_DIR, { recursive: true, force: true });
console.log("wrote reference/reference.json");
