export default (function () {
const q = { p: null, n(v) { return v * 2; } }; let z = null, k = "p";
function a(v) { return (q[k] = q.n(v)); } function b(v) { return v ? (z = q.n(v)) : 0; } const c = (v) => (q.c = q.n(v));
function d(v) { return v && (q.d = q.n(v)); } function e(v) { return (0, q.e = Math.max(v, 1)); }
return [a(1), q.p, b(2), z, c(3), q.c, d(4), q.d, e(5), q.e];
})();
