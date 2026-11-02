export default (function () {
return ["$10 €20".match(/(?<=\$)\d+/)[0], "a1b2".replace(/(?<![a])\d/g, "#"), /(?<=a)b/.test("ab")];
})();
