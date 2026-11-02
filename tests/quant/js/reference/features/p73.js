export default (function () {
const g = Object.groupBy([1, 2, 3, 4], (x) => (x % 2 ? "odd" : "even"));
return [g, Object.getPrototypeOf(g), Map.groupBy([1, 2, 3], (x) => x > 1)];
})();
