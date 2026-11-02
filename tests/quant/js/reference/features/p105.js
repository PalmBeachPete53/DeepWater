export default (function () {
const p = { e: 2 }; return { ...p, v: 3 ** (p.e ?? 1), w: { ...null } ?? 1 };
})();
