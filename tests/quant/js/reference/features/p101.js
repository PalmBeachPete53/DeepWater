export default (function () {
const out = [];
for (let { a, ...r } = { a: 1, b: 2 }, i = 0; i < 2; i++) out.push([a, r, i]);
const pairs = Object.entries({ k: { v: 1, w: 2 } }).map(([key, { v, ...rest }]) => [key, v, rest]);
return [out, pairs];
})();
