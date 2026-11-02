export default (function () {
return ["abc".includes("b"), "abc".startsWith("ab"), "abc".endsWith("bc"), "ab".repeat(3), "\u{1F600}".codePointAt(0), String.fromCodePoint(65, 0x1F600).length, "\u0041\u030A".normalize("NFC").length];
})();
