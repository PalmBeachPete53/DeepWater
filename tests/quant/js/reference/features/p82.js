export default (function () {
return [RegExp.escape("a.b*c"), new RegExp(RegExp.escape("1+1")).test("1+1")];
})();
