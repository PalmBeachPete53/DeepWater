export default (function () {
let ran = false; return Promise.resolve(1).finally(() => { ran = true; }).then((v) => [v, ran]);
})();
