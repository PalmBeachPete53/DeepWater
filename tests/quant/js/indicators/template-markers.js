// Nested template literals with braces and `...` inside, regex named groups, replaceAll, matchAll,
// spread in marker objects, a helper module imported with a relative path.
import { describe, defaults } from "./lib/format.js";

const label = (kind, { price, ...rest }) => `${kind}{${price.toFixed(2)}}${Object.keys(rest).length ? `...${`{${Object.keys(rest).join(",")}}`}` : ""}`;

export default {
  name: "Template markers",
  overlay: true,
  inputs: [
    { key: "fast", type: "int", default: 5, min: 1 },
    { key: "slow", type: "int", default: 13, min: 2 },
  ],
  outputs: [{ key: "spread", color: "#2962ff" }],
  calc(bars, p, ta) {
    const close = ta.source(bars, "close");
    const fast = ta.ema(close, p.fast), slow = ta.ema(close, p.slow);
    const up = ta.crossover(fast, slow), down = ta.crossunder(fast, slow);
    const base = { ...defaults, position: "belowBar" };
    const markers = bars.flatMap((b, i) => {
      if (up[i]) return [{ ...base, time: b.time, shape: "arrowUp", text: label("up", { price: b.close, at: i }) }];
      if (down[i]) return [{ ...base, time: b.time, position: "aboveBar", shape: "arrowDown", text: label("down", { price: b.close }) }];
      return [];
    });
    const re = /(?<kind>up|down)\{(?<price>[\d.]+)\}/;
    const parsed = markers.map(({ text }) => text.match(re)?.groups ?? null);
    const summary = markers.map((m) => m.text).join(" ").replace(/(?<kind>up|down)\{/g, "$<kind>:").replaceAll("}", "");
    const prices = [...summary.matchAll(/:(?<p>[\d.]+)/g)].map((m) => Number(m.groups.p));
    return {
      spread: fast.map((f, i) => f - slow[i]),
      markers,
      info: { parsed: parsed.slice(0, 3), summary: describe(summary.slice(0, 60)), prices: prices.slice(0, 5) },
    };
  },
};
