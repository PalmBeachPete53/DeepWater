export default (function () {
return ["  a  ".trimStart(), "  a  ".trimEnd(), "\u00a0x\u00a0".trimStart().length, " y ".trimLeft(), " y ".trimRight()];
})();
