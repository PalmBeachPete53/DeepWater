export default (function () {
return [/(?i:a)b/.test("Ab"), /(?<y>\d{4})|(?<y>\d{2})/.exec("99").groups.y];
})();
