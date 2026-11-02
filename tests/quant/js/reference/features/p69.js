export default (function () {
const a = [1, 2, 3, 4]; return [a.findLast((x) => x % 2), a.findLastIndex((x) => x > 9), new Float64Array([1, 2]).findLast((x) => x < 2)];
})();
