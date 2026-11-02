export default (function () {
const re = /{...x}/, d = 10 / 2 / 1, o = { ...{ r: re.source }, d };
const g = 4, h = 2, i = 1; const q = g / h / i; const w = { ...{ v: "a/b".split(/\//) } };
return [o, q, w, /[/]{.../.test("/{...")];
})();
