export default (function () {
const s = Symbol("s"), base = { a: 1, b: 2, [s]: 3 };
let calls = 0; const getter = { get g() { calls++; return "got"; } };
const hidden = Object.defineProperty({ vis: 1 }, "hid", { value: 2, enumerable: false });
const proto = Object.create({ inherited: 1 }); proto.own = 2;
const o = { z: 0, ...base, b: 20, ...null, ...undefined, ...getter, ...hidden, ...proto, ..."hi", ...[7, 8], ...5, a: 10 };
const sp = { ...{ __proto__: null, x: 1 } };
return [o, calls, Object.getOwnPropertyDescriptor(o, "g").value, Object.getPrototypeOf(sp) === Object.prototype, { ...{ a: 1 }, a: 2 }, { a: 2, ...{ a: 1 } }];
})();
