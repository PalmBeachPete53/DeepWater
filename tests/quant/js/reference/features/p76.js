export default (function () {
const b = new ArrayBuffer(2, { maxByteLength: 8 }); b.resize(6); const t = b.transfer(4); return [b.detached, t.byteLength, t.resizable];
})();
