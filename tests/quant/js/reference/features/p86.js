export default (function () {
return new Promise((r) => queueMicrotask(() => r("mt")));
})();
