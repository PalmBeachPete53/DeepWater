export default (function () {
return [[1, [2, [3, [4]]]].flat(), [1, [2, [3, [4]]]].flat(Infinity), [1, 2].flatMap((x) => [x, [x * 2]]), [1, , 3].flat()];
})();
