export default (function () {
function* g({ a, ...r }) { yield r; } const o = { async am({ x, ...r }) { return r; }, *gm({ y, ...r }) { yield r; } };
return Promise.all([o.am({ x: 1, z: 2 })]).then((v) => [g({ a: 1, b: 2 }).next().value, v[0], o.gm({ y: 1, w: 3 }).next().value]);
})();
