export default (function () {
const k = "dyn", v = 1;
const o = { v, [k + 1]: 2, m() { return 3; }, get g() { return 4; }, ["s" + "q"]() { return 5; } };
return [o, o.m(), o.g, o.sq()];
})();
