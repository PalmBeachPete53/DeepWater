export default (function () {
const re = new RegExp("(?<user>\\w+)@(?<host>\\w+)", "g"), s = "a@b, c@d";
const calls = []; const r = s.replace(re, (...args) => { calls.push(JSON.stringify(args.at(-1))); return args.at(-1).host; });
return [r, calls, [...s.matchAll(re)].map((m) => m.groups.user), s.replace(re, "$<host>.$<user>"), "x".match(/x/).groups, re.flags];
})();
