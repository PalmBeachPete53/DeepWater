export default (function () {
return ["a.b.c".replaceAll(".", "/"), "aaa".replaceAll("a", "$&$&"), "x-y".replaceAll(/-/g, "+"), "ab".replaceAll("", "_")];
})();
