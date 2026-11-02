export default (function () {
const a = { x: 1 } // { ...not code }
/* ...{ nor this } */ const b = { ...a /* , ...c */, y: 2 }
const { x, ...r } = b
;[1].forEach((v) => v)
const c = { ...b,
  // comment
  z: 3, }
return [b, r, c]
})();
