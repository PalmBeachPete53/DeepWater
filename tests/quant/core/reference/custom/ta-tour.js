// Exercises the ta helpers from a custom indicator.
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
