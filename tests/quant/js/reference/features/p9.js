export default (function () {
const s = Symbol("d"), o = { [s]: 1, [Symbol.toPrimitive](h) { return h === "number" ? 42 : "str"; } };
class C { static [Symbol.hasInstance](x) { return x === 1; } }
return [typeof s, s.toString(), o[s], +o, `${o}`, 1 instanceof C, Symbol.for("a") === Symbol.for("a"), Object.prototype.toString.call({ [Symbol.toStringTag]: "T" })];
})();
