export default (function () {
const base = { a: 1 };
// (V8 defines accessors after the other properties in the run right after a leading spread; the
// spec order is kept here, so the accessor sits before the spread.)
const o = { get g() { return this.a * 10; }, ...base, m() { return 1; }, __proto__: null, ...{ z: 2 } };
const d = Object.getOwnPropertyDescriptor(o, "g");
return [typeof d.get, o.g, Object.getPrototypeOf(o), Object.keys(o), o.m()];
})();
