export default (function () {
class C { #x = 1; static read(o) { return o.#x; } static write(o) { o.#x = 2; } static has(o) { return #x in o; } }
const out = [];
for (const f of [() => C.read({}), () => C.write({}), () => C.has(1), () => C.read(new C())]) { try { out.push(f()); } catch (e) { out.push(e.constructor.name + ": " + e.message); } }
return out;
})();
