export default (function () {
let n = 0; const objs = [{ v: null }, { v: 0 }, { v: 1 }];
const next = () => objs[n++]; next().v ??= "a"; next().v ||= "b"; next().v &&= "c";
let k = 0; const keys = () => (k++, "v"); const o = { v: null }; o[keys()] ??= 1; o[keys()] ??= 2;
return [objs, n, o, k];
})();
