export default (function () {
async function* gen() { yield 1; yield await Promise.resolve(2); }
return (async () => { const out = []; for await (const v of gen()) out.push(v); for await (const v of [Promise.resolve(3), 4]) out.push(v); return out; })();
})();
