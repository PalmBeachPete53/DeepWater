export default (function () {
return Promise.allSettled([Promise.resolve(1), Promise.reject(new Error("x"))]).then((r) => r.map((s) => s.status));
})();
