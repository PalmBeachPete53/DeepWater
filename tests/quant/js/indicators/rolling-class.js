// Class fields, private members, static blocks, `#x in`, logical assignment, optional catch binding.
class Window {
  static #made = 0;
  static defaults;
  static {
    this.defaults = { size: 10, ...{ ddof: 0 } };
  }
  #values = [];
  #sum = 0;
  size;
  constructor(size = Window.defaults.size) {
    this.size = size;
    Window.#made++;
  }
  push(v) {
    this.#values.push(v);
    this.#sum += v;
    if (this.#values.length > this.size) this.#sum -= this.#values.shift();
    return this.#full ? this.#sum / this.size : NaN;
  }
  get #full() {
    return this.#values.length === this.size;
  }
  static isWindow(o) {
    return #values in o;
  }
  static get made() {
    return Window.#made;
  }
}

class Zscore extends Window {
  #sq = 0;
  #seen = [];
  cache = null;
  push(v) {
    const mean = super.push(v);
    this.#seen.push(v * v);
    this.#sq += v * v;
    if (this.#seen.length > this.size) this.#sq -= this.#seen.shift();
    if (Number.isNaN(mean)) return NaN;
    this.cache ??= { first: mean };
    this.cache.last ||= mean;
    this.cache.count &&= this.cache.count + 1;
    this.cache.count ??= 1;
    const sd = Math.sqrt(Math.max(this.#sq / this.size - mean * mean, 0));
    return sd === 0 ? 0 : (v - mean) / sd;
  }
}

export default {
  name: "Class z-score",
  overlay: false,
  inputs: [{ key: "length", type: "int", default: 14, min: 2 }],
  outputs: [
    { key: "z", color: "#7e57c2" },
    { key: "mean", color: "#ff9800" },
  ],
  levels: [-2, 2],
  calc(bars, p, ta) {
    const z = new Zscore(p.length), m = new Window(p.length);
    let parsed;
    try {
      parsed = JSON.parse("{ broken");
    } catch {
      parsed = { ok: false };
    }
    return {
      z: bars.map((b) => z.push(b.close)),
      mean: bars.map((b) => m.push(b.close)),
      info: { isWindow: Window.isWindow(z), plain: Window.isWindow({}), made: Window.made > 0, parsed, cache: Object.keys(z.cache ?? {}) },
    };
  },
};
