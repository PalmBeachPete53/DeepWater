export default (function () {
const o = { a: { b: { c: 1 } } }, n = null; return [{ ...o?.a?.b }, { ...n?.x }, { ...o.a?.["b"] }];
})();
