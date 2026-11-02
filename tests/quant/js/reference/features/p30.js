export default (function () {
const s = Symbol("s");
const { a, b: { c, ...innerRest }, ...rest } = { a: 1, b: { c: 2, d: 3, e: 4 }, f: 5, [s]: 6, g: 7 };
const { [("x" + "y")]: xy, ...r2 } = { xy: 1, z: 2 };
let calls = 0; const src = { get k() { calls++; return 1; }, m: 2 };
const { ...all } = src;
const proto = Object.create({ inh: 1 }); proto.own = 1; const { ...ownOnly } = proto;
const arr = [1, 2, 3]; const { 0: first, ...arrRest } = arr;
const { length, ...strRest } = "ab";
return [a, c, innerRest, rest, Object.getOwnPropertySymbols(rest).length, xy, r2, all, calls, ownOnly, first, arrRest, length, strRest];
})();
