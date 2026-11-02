export default (function () {
return [/\p{L}+/u.exec("héllo1")[0], /\p{Script=Greek}/u.test("α"), /\P{N}/u.test("5")];
})();
