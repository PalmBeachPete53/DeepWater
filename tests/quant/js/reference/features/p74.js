export default (function () {
const { promise, resolve } = Promise.withResolvers(); resolve(7); return promise;
})();
