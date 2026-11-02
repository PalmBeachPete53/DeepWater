export default (function () {
const f = ({ a }, { b }) => [a, b];
const g = function ([x], [y] = [9], { z } = { z: 3 }) { return [x, y, z]; };
const sum = [{ v: 1 }, { v: 2 }, { v: 4 }].reduce(({ s, n }, { v }) => ({ s: s + v, n: n + 1 }), { s: 0, n: 0 });
const h = ({ k }, [m] = [7], ...rest) => [k, m, rest];
return [f({ a: 1 }, { b: 2 }), g([1]), g([1], [2], { z: 5 }), sum, h({ k: 1 }), h({ k: 1 }, [2], 3, 4), g.length, h.length];
})();
