export default (function () {
function f(a, { b, ...r } = { ...{ b: 1, c: 2 } }) { return [a, b, r]; } return [f(1), f(1, { b: 3, d: 4 })];
})();
