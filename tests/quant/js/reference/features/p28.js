export default (function () {
let set = 0; const o = { set a(v) { set++; }, ...{ a: 1 } };
const d = Object.getOwnPropertyDescriptor(o, "a"); return [set, d.value, d.writable, d.enumerable, d.configurable];
})();
