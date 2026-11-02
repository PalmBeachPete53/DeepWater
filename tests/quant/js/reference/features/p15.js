export default (function () {
const s = Symbol("s");
const o = Object.assign({ a: 1 }, null, { b: 2, [s]: 3 }, "xy");
return [o, Object.is(NaN, NaN), Object.is(0, -0), Object.getOwnPropertySymbols(o).length, Object.getPrototypeOf(Object.setPrototypeOf({}, null))];
})();
