export default (function () {
let r; try { throw 1; } catch { r = "caught"; } return r;
})();
