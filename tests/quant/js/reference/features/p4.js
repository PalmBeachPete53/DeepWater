export default (function () {
const n = 2, tag = (s, ...v) => s.raw.join("|") + v.join(",");
return [`a${n + 1}b${`in${n}`}`, tag`x${1}y\n${2}`, String.raw`\t${n}`, `multi
line`];
})();
