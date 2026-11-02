export default (function () {
const a = Array.from({ length: 30 }, (_, i) => ({ k: i % 3, i }));
return a.sort((x, y) => x.k - y.k).map((o) => o.i);
})();
