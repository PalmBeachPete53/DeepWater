export default (function () {
const o = { class: 1, function: 2, return: 3, ...{ if: 4 } }; const { class: c, ...others } = o; return [o.if, c, others];
})();
