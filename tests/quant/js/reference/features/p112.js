export default (function () {
async function* g() { try { yield 1; yield 2; yield 3; } finally { done.push("cleanup"); } }
const done = [];
return (async () => { const out = []; for await (const v of g()) { out.push(v); if (v === 2) break; } const it = g(); await it.next(); const r = await it.return(9); return [out, r, done.length >= 1]; })();
})();
