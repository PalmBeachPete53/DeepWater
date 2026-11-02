export default (function () {
return [Array.from({ length: 3 }, (_, i) => i * 2), Array.of(7, 8), new Array(3).fill(0), [5, 6, 7].find((x) => x > 5), [5, 6, 7].findIndex((x) => x > 9),
[1, 2, 3, 4, 5].copyWithin(0, 3), [...["a", "b"].entries()], [..."ab"].keys().next()];
})();
