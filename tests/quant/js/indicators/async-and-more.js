// Async functions and generators defined in the file (calc itself stays synchronous), class field
// arrow functions, private methods, structuredClone, Promise helpers at load time, error cause.
const loaded = [];
async function warm(n) {
  const v = await Promise.resolve(n * 2);
  loaded.push(v);
  return v;
}
async function* ticks(limit) {
  for (let i = 0; i < limit; i++) yield await Promise.resolve(i);
}
(async () => {
  for await (const t of ticks(3)) loaded.push(t);
  await warm(21);
})();

class Smoother {
  alpha;
  #state = null;
  update = (v) => (this.#state = this.#next(v));
  constructor(alpha) {
    this.alpha = alpha;
  }
  #next(v) {
    return this.#state === null || Number.isNaN(this.#state) ? v : this.#state + this.alpha * (v - this.#state);
  }
}

export default {
  name: "Async and more",
  overlay: false,
  inputs: [{ key: "alpha", type: "float", default: 0.3, min: 0, max: 1 }],
  outputs: [{ key: "smooth", color: "#ab47bc" }, { key: "diff", kind: "histogram" }],
  calc(bars, { alpha }, ta) {
    const s = new Smoother(alpha);
    const smooth = bars.map(({ close }) => s.update(close));
    const copy = structuredClone({ smooth: smooth.slice(0, 3), when: new Date(0) });
    let causeText = "";
    try {
      throw new Error("outer", { cause: new RangeError("inner") });
    } catch (e) {
      causeText = `${e.message}<-${e.cause?.name}:${e.cause?.message}`;
    }
    return {
      smooth,
      diff: bars.map((b, i) => b.close - smooth[i]),
      info: { copy: copy.smooth, when: copy.when.toISOString(), causeText, asyncType: typeof warm(1).then },
    };
  },
};
