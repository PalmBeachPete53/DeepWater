export default (function () {
const a = { x: 1 }, t = `{${JSON.stringify({ ...a, y: `${"}"}${{ ...a }.x}` })}}`;
return [t, `...${"{...a}"}`, `${`${`${({ ...a }).x}`}`}`];
})();
