export default (function () {
const mk = (a, ...r) => ({ ...a, r }); const xs = [{ a: 1 }, { b: 2 }];
return [mk({ q: 1 }, 1, 2), xs.reduce((acc, x) => ({ ...acc, ...x }), {}), [...xs.map((x) => ({ ...x }))]];
})();
