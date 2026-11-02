export default (function () {
const w = new WeakMap(), s = Symbol("k"); w.set(s, 1); return w.get(s);
})();
