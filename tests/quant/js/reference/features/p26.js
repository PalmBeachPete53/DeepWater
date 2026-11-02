export default (function () {
const s = new SharedArrayBuffer(8), a = new Int32Array(s); Atomics.add(a, 0, 5); return [Atomics.load(a, 0), Atomics.isLockFree(4)];
})();
