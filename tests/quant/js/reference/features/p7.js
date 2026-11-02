export default (function () {
function* g(n) { for (let i = 0; i < n; i++) { const r = yield i; if (r) return r; } }
const out = []; for (const v of g(3)) out.push(v);
const it = g(5); it.next(); const sent = it.next("stop");
const obj = { *[Symbol.iterator]() { yield* [1, 2]; yield 3; } };
return [out, sent, [...obj], Array.from(new Map([[1, "a"]]).entries())];
})();
