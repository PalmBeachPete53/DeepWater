export default (function () {
const p = new Proxy({}, { get: (t, k) => (k in t ? t[k] : "dflt:" + String(k)), has: () => true });
p.a = 1; return [p.a, p.b, "zz" in p, Reflect.ownKeys({ a: 1, [Symbol.iterator]: 0 }).length, Reflect.apply(Math.max, null, [1, 3]), Reflect.has({ x: 1 }, "x")];
})();
