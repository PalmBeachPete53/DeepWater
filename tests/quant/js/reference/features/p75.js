export default (function () {
return ["ab".isWellFormed(), "a\uD800".isWellFormed(), "a\uD800".toWellFormed()];
})();
