export default (function () {
async function f(x) { const a = await x; const b = await Promise.resolve(a + 1); return [a, b]; }
const g = async () => { try { await Promise.reject(new Error("no")); } catch (e) { return e.message; } };
return Promise.all([f(1), g(), (async function () {})()]);
})();
