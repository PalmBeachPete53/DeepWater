export default (function () {
const f = (x) => ({ ...x, n: 1 });
const deep = { a: { ...{ b: { ...{ c: 1 } } } } };
return [f([1, 2]), f("ab"), deep, [{ ...{ x: 1 } }, { ...{ y: 2 } }].map((o) => ({ ...o })), { ...(1 > 0 ? { t: 1 } : {}) }];
})();
