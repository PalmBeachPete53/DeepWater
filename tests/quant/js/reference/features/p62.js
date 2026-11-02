export default (function () {
class C { #v = 1; static #count = 0; #m() { return this.#v * 10; } get #g() { return this.#v + 100; } set #g(x) { this.#v = x; }
  static #sm() { return "sm"; } constructor() { C.#count++; } inc() { this.#v++; this.#g = this.#g; return [this.#m(), C.#count, C.#sm()]; } static has(o) { return #v in o; } }
const c = new C(); new C(); return [c.inc(), C.has(c), C.has({}), Object.keys(c)];
})();
