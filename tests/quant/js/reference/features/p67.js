export default (function () {
const e = new Error("outer", { cause: "inner" }); return [e.cause, "cause" in new Error("x"), new TypeError("t", { cause: 1 }).cause];
})();
