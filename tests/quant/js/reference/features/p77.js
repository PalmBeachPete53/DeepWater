export default (function () {
return [/[\p{L}--[a-z]]/v.test("a"), /[\p{L}&&\p{ASCII}]/v.test("é"), /x/v.unicodeSets];
})();
