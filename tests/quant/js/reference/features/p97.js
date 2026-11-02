export default (function () {
const o = { get: 1, set: 2, async: 3, static: 4, of: 5, new: 6, ...{ get: 10 } }; const { get, ...r } = o; return [get, r];
})();
