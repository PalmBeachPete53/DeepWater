// Two destructuring parameters (bound wrongly by V4 without the rewrite), rest in for-of heads and
// in nested patterns, Object.groupBy, toSorted, findLast, at(), Set methods, iterator helpers.
const accumulate = ({ sum, n, ...meta }, { close, volume, ...bar }) => ({ sum: sum + close * volume, n: n + volume, ...meta, last: bar.time });

export default {
  name: "Destructured VWAP",
  overlay: true,
  inputs: [{ key: "length", type: "int", default: 10, min: 1 }],
  outputs: [
    { key: "vwap", color: "#26a69a" },
    { key: "rank", kind: "histogram", color: "#ef5350" },
  ],
  calc(bars, { length }, ta) {
    const vwap = bars.map((_, i) => {
      if (i + 1 < length) return NaN;
      const { sum, n } = bars.slice(i + 1 - length, i + 1).reduce(accumulate, { sum: 0, n: 0, tag: "w" });
      return n > 0 ? sum / n : NaN;
    });
    const rank = [];
    for (const [i, { close, ...others }] of bars.entries()) {
      const window = bars.slice(Math.max(0, i - length + 1), i + 1).map(({ close: c }) => c);
      const sorted = window.toSorted((a, b) => a - b);
      rank.push(sorted.findLastIndex((v) => v <= close) / Math.max(1, sorted.length - 1) + (others.time > 0 ? 0 : 1));
    }
    const groups = Object.groupBy(bars, ({ close, open }) => (close > open ? "up" : close < open ? "down" : "flat"));
    const days = new Set(bars.map((b) => new Date(b.time * 1000).getUTCDay()));
    const weekend = days.intersection(new Set([0, 6]));
    const firstBig = bars.values().map(({ high, low }) => high - low).filter((r) => r > 0).take(3).toArray();
    return {
      vwap,
      rank,
      info: { up: groups.up?.length ?? 0, down: groups.down?.length ?? 0, weekend: [...weekend].toSorted(), firstBig, last: bars.at(-1)?.close ?? null },
    };
  },
};
