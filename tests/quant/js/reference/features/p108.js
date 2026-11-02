export default (function () {
class A { async m(x) { return x * 2; } get v() { return 5; } }
class B extends A { async m(x) { const a = await super.m(x); const k = "m"; return [a, await super[k](1), super.v]; } }
const o = { __proto__: { hi() { return "hi"; } }, async m() { return super.hi() + "!"; } };
return Promise.all([new B().m(3), o.m()]);
})();
