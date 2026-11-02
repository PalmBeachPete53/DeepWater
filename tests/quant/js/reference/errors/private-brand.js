class C { #x = 1; static get(o) { return o.#x; } }
export default { name: 'E', calc(bars) { C.get({}); return { value: bars.map((b) => b.close) }; } };
