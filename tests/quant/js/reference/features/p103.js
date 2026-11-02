export default (function () {
class S { #state = { ...{ a: 1 }, b: 2 }; opts = { ...S.defaults }; static defaults = { x: 1 }; get state() { return { ...this.#state }; } }
return [new S().state, new S().opts];
})();
