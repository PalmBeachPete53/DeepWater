export default (function () {
return Promise.all([1, Promise.resolve(2), new Promise((r) => r(3))]).then((v) => Promise.race([v, 9]));
})();
