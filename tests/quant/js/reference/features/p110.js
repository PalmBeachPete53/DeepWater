export default (function () {
const K = class { a = 1; b = this.a + 1; };
const log = []; class P { x = log.push("P.x"); constructor() { log.push("P()"); } } class Q extends P { y = log.push("Q.y"); constructor() { log.push("Q before super"); super(); log.push("Q after super"); } }
new Q(); return [K.name, new K().b, (class { static s = 1; }).name, log, Object.keys(new K())];
})();
