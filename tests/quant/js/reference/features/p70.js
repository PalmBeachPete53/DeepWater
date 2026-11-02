export default (function () {
const a = [3, 1, 2];
return [a.toSorted(), a.toReversed(), a.toSpliced(1, 1, 9, 8), a.with(-1, 0), a, new Int8Array([3, 1]).toSorted(), [, 1].toSorted()];
})();
