export default (function () {
const src = { a: 1, b: { c: 2 } }; const { a, ...r } = src; r.b.c = 9; r.x = 1; return [src, r, "x" in src];
})();
