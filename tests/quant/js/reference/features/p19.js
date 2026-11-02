export default (function () {
const f = new Float64Array([1.5, NaN, -0]); const b = new ArrayBuffer(8); new DataView(b).setFloat64(0, Math.PI, true);
const u = new Uint8Array(b); return [f, new Float64Array(b)[0], u[0], Int16Array.from([1, 2]).map((x) => x * 3), new Uint8ClampedArray([300, -5])];
})();
