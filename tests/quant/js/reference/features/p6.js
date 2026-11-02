export default (function () {
function f(a, b = a + 1, ...rest) { return [a, b, rest, arguments.length]; }
const arr = [1, 2, 3];
return [f(...arr, ...[4]), Math.max(...arr), [0, ...arr, ..."ab"], f.length];
})();
