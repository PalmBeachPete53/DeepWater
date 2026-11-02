export default (function () {
return Promise.any([Promise.reject(1), Promise.resolve(2)]).then((v) => Promise.any([Promise.reject(3)]).catch((e) => [v, e.constructor.name, e.errors]));
})();
