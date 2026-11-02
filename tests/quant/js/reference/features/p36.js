export default (function () {
const m = /(?<y>\d{4})-(?<m>\d{2})/.exec("2024-05");
return [m.groups.y, m.groups.m, "2024-05".replace(/(?<y>\d+)-(?<m>\d+)/, "$<m>/$<y>"), /(?<a>x)\k<a>/.test("xx")];
})();
