export default (function () {
const o = {}; const w = new WeakRef(o); const f = new FinalizationRegistry(() => {}); f.register(o, 1); return [w.deref() === o, typeof f.unregister];
})();
