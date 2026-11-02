export default (function () {
const [a, , b = 5, ...r] = [1, 2, undefined, 4, 5];
const { x, y: { z = 9 } = {}, w: ww = 3 } = { x: 1, y: {} };
let p = 1, q = 2; [p, q] = [q, p];
const f = ({ k, l = 2 } = {}, [m] = [7]) => k + l + m;
return [a, b, r, x, z, ww, p, q, f({ k: 1 }), f()];
})();
