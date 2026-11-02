export default (function () {
class A { x = 1; y = this.x + 1; ["c" + "k"] = 3; static s = 4; static t = A.s + 1; arrow = () => this.x; z; }
class B extends A { w = this.y * 10; }
const b = new B(); return [b.x, b.y, b.ck, A.s, A.t, b.arrow(), b.w, "z" in b, Object.keys(b)];
})();
