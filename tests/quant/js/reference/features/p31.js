export default (function () {
let a, rest;
({ a, ...rest } = { a: 1, b: 2, c: 3 });
const f = ({ x, ...others }, [y, ...more], { z = 9, ...dflt } = {}) => [x, others, y, more, z, dflt];
const g = function ({ p: { q, ...pr }, ...top }) { return [q, pr, top]; };
const loop = []; for (const { id, ...vals } of [{ id: 1, v: 2 }, { id: 3, w: 4 }]) loop.push([id, vals]);
let caught; try { throw { code: 1, msg: "m", extra: true }; } catch ({ code, ...info }) { caught = [code, info]; }
const arrow = ({ k, ...r }) => r;
class C { m({ a, ...r }) { return r; } static s({ ...r }) { return r; } }
const objMethod = { m({ a, ...r }) { return r; } };
return [a, rest, f({ x: 1, o: 2 }, [3, 4, 5]), g({ p: { q: 1, r: 2 }, t: 3 }), loop, caught, arrow({ k: 1, j: 2 }), new C().m({ a: 1, b: 2 }), C.s({ c: 3 }), objMethod.m({ a: 1, d: 4 })];
})();
