export default (function () {
const o = { a: 1, get b() { return 2; } };
return [Object.values(o), Object.entries(o), Object.keys(Object.getOwnPropertyDescriptors(o)), typeof Object.getOwnPropertyDescriptors(o).b.get];
})();
