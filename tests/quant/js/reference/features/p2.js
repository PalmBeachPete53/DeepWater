export default (function () {
class A { constructor(x) { this.x = x; this.nt = new.target.name; } get dbl() { return this.x * 2; } set dbl(v) { this.x = v / 2; } static make() { return new this(3); } m() { return "A" + this.x; } }
class B extends A { constructor() { super(5); } m() { return "B:" + super.m(); } static make() { return super.make(); } }
const b = new B(); b.dbl = 20;
return [b.m(), b.dbl, b.nt, B.make() instanceof B, Object.getOwnPropertyNames(A.prototype), typeof A];
})();
