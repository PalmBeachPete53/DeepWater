export default (function () {
const o = { b: 1, 2: "two", a: 2, 1: "one" }, k = "kk";
return [Object.keys({ z: 0, ...o, [k]: 1, 0: "zero" }), Object.keys({ ...o, b: 3 })];
})();
