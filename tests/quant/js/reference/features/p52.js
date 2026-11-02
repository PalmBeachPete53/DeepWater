export default (function () {
return [...("a1b22".matchAll(/\d+/g))].map((m) => [m[0], m.index]);
})();
