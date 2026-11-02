export default (function () {
const m = new Map([[1, "a"], ["1", "b"]]); m.set(NaN, "n");
const s = new Set([1, 1, 2, NaN, NaN]); const wm = new WeakMap(), k = {}; wm.set(k, 1);
const ws = new WeakSet([k]);
return [m.get(NaN), m.size, [...m.keys()], s.size, wm.get(k), ws.has(k), [...s].length];
})();
