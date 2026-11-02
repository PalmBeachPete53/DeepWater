export default (function () {
const ta = { compute: (id) => ({ upper: [1, 2], lower: [0, 1], id }) };
const bands = { ...ta.compute("bb"), ...ta.compute("kc", { length: 3 }), basis: [5] };
const pick = ({ upper, ...others }) => Object.keys(others);
return [bands, pick(bands)];
})();
