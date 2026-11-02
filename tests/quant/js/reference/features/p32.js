export default (function () {
const [{ a = 1, ...r1 } = {}, [{ b, ...r2 }] = [{ b: 2, c: 3 }]] = [];
const { x: [y, { z, ...r3 }], ...r4 } = { x: [1, { z: 2, q: 3 }], w: 4 };
let o = {}; ({ a: o.p, ...o.rest } = { a: 1, b: 2 });
return [a, r1, b, r2, y, z, r3, r4, o];
})();
