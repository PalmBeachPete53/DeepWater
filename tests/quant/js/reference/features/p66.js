export default (function () {
return [Object.hasOwn({ a: 1 }, "a"), Object.hasOwn(Object.create({ b: 1 }), "b")];
})();
