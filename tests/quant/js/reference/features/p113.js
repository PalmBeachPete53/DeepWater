export default (function () {
class A { static base = { n: 0 }; }
class B extends A { #c = null; static k = "n"; inc() { this.#c ??= 0; this.#c ||= 10; this.#c &&= this.#c + 1; return this.#c; } }
const o = { a: { b: null } }; let i = 0; const keys = ["x", "y"]; const t = {}; t[keys[i++]] ??= 1; t[keys[i++]] ||= 2; o.a.b ??= [];
return [new B().inc(), t, i, o];
})();
