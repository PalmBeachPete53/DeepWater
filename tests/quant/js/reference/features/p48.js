export default (function () {
const o = { a: { b: [1, { c: 2 }], f() { return this.a; } } }, n = null;
return [o?.a?.b?.[1]?.c, n?.x.y.z, o.a.f?.(), o.nope?.(), n?.[0], o.a?.["b"].length, delete n?.x];
})();
