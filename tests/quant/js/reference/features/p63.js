export default (function () {
class C { static x = 1; static { this.y = this.x + 1; C.z = 3; } } return [C.x, C.y, C.z];
})();
