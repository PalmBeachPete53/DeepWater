export default (function () {
class L extends Array { sum() { return this.reduce((a, b) => a + b, 0); } }
const l = L.from([1, 2, 3]); class E extends Error { constructor(m) { super(m); this.name = "E"; } }
return [l.sum(), l instanceof L, l.map((x) => x * 2) instanceof L, new E("m").message, new E("m") instanceof Error];
})();
