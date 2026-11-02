export default (function () {
function* n() { let i = 0; while (true) yield i++; }
return [n().map((x) => x * 2).filter((x) => x % 3).take(4).toArray(), n().drop(2).take(2).toArray(), n().take(5).reduce((a, b) => a + b), Iterator.from([1, 2]).toArray(), n().some((x) => x > 3), n().take(3).flatMap((x) => [x, x]).toArray()];
})();
