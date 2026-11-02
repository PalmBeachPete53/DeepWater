export default (function () {
let a = 1; { let a = 2; } const b = [];
for (let i = 0; i < 3; i++) b.push(() => i);
let tdz; try { tdz = x; let x = 1; } catch (e) { tdz = e.name; }
return [a, b.map((f) => f()), tdz];
})();
