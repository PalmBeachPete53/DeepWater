export default (function () {
const o = { v: 7, f() { return [1, 2].map((x) => x * this.v); }, g: (a, b = 2) => ({ a, b }) };
return [o.f(), o.g(1), (() => arguments.length)()];
})();
