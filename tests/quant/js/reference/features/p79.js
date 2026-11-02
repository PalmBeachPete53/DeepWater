export default (function () {
const a = new Set([1, 2, 3]), b = new Set([2, 3, 4]);
return [a.union(b), a.intersection(b), a.difference(b), a.symmetricDifference(b), a.isSubsetOf(b), new Set([2]).isSubsetOf(a), a.isSupersetOf(new Set([1])), a.isDisjointFrom(new Set([9]))];
})();
