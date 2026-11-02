export default (function () {
const d = new Date(0), o = { a: [1, { b: 2 }], d, m: new Map([[1, 2]]), s: new Set([1]) }; o.self = o;
const c = structuredClone(o); return [c.a, c.a !== o.a, c.d instanceof Date, c.d.getTime(), c.m.get(1), c.s.has(1), c.self === c];
})();
