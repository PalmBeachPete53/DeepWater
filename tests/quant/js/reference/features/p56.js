export default (function () {
const o = { a: 0, b: null, c: 1 }; let n = 0, calls = 0;
const get = () => { calls++; return o; };
get().a ||= 5; get().b ??= 6; get().c &&= 7; o.d ??= 8; o.c ||= (n = 99);
let x = null; x ??= 1; x ||= 2; x &&= 3;
let s; const r = (s ??= "v");
const arr = [0]; let i = 0; arr[i++] ||= 4;
let setterCalls = 0; const so = { get p() { return 1; }, set p(v) { setterCalls++; } }; so.p ||= 2; so.p &&= 3;
return [o, n, calls, x, r, arr, i, setterCalls];
})();
