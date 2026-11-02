export default (function () {
let n = 0; { const t = { ...{ a: 1 } }; n = t.a; } lbl: { break lbl; } return n;
})();
