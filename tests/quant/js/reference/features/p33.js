export default (function () {
const out = [];
for (const v of [null, undefined]) { try { const { ...r } = v; out.push("no error"); } catch (e) { out.push(e.constructor.name); } }
return out;
})();
