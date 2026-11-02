export default (function () {
let n = 0; outer: for (const a of [1, 2]) for (const b of [1, 2]) { if (b === 2) continue outer; n += a; }
class P { get v() { return 1; } } return [n, typeof Object.getOwnPropertyDescriptor(P.prototype, "v").get];
})();
