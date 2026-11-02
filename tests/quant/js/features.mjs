// The ES2015..ES2025 features a custom indicator could use, one small module each. Every module's
// default export is the value to compare (a promise for the async ones); ser() below turns it into
// a string the same way in Node and in QJSEngine. make_reference.mjs records what Node gives;
// qtm_test_js checks QJSEngine against it, raw (the probe) and through the custom indicator loader.
//
// Keep each probe on its own feature: a probe that also used another missing feature would report
// the wrong one.

// The serializer, as ES5 source (it has to run on the raw engine too).
export const SER = String.raw`
function ser(v, depth) {
  depth = depth || 0;
  if (depth > 8) return "...";
  var t = typeof v;
  if (v === undefined) return "undefined";
  if (v === null) return "null";
  if (t === "number") return v === 0 && 1 / v < 0 ? "-0" : String(v);
  if (t === "string") return JSON.stringify(v);
  if (t === "boolean") return String(v);
  if (t === "bigint") return String(v) + "n";
  if (t === "symbol") return v.toString();
  if (t === "function") return "function " + (v.name || "");
  var tag = Object.prototype.toString.call(v).slice(8, -1);
  if (tag === "Date") return "Date(" + (isNaN(v.getTime()) ? "invalid" : v.toISOString()) + ")";
  if (tag === "RegExp") return String(v);
  if (tag === "Error" || v instanceof Error) return "Error(" + v.name + ": " + v.message + ")";
  if (tag === "Map") { var m = []; v.forEach(function (x, k) { m.push(ser(k, depth + 1) + "=>" + ser(x, depth + 1)); }); return "Map{" + m.join(",") + "}"; }
  if (tag === "Set") { var s = []; v.forEach(function (x) { s.push(ser(x, depth + 1)); }); return "Set{" + s.join(",") + "}"; }
  if (Array.isArray(v) || /Array$/.test(tag)) {
    var a = [];
    for (var i = 0; i < v.length; i++) a.push(i in v ? ser(v[i], depth + 1) : "<hole>");
    return (Array.isArray(v) ? "" : tag) + "[" + a.join(",") + "]";
  }
  var parts = [], keys = Object.keys(v);
  for (var j = 0; j < keys.length; j++) parts.push(JSON.stringify(keys[j]) + ":" + ser(v[keys[j]], depth + 1));
  if (Object.getOwnPropertySymbols) {
    var syms = Object.getOwnPropertySymbols(v);
    for (var k = 0; k < syms.length; k++)
      if (Object.prototype.propertyIsEnumerable.call(v, syms[k])) parts.push("[" + syms[k].toString() + "]:" + ser(v[syms[k]], depth + 1));
  }
  var proto = Object.getPrototypeOf(v);
  var name = proto === null ? "null" : proto === Object.prototype ? "" : proto.constructor && proto.constructor.name || "?";
  return name + "{" + parts.join(",") + "}";
}
`;

const F = [];
// A probe whose body runs inside a function; it returns the value to compare.
const fn = (name, es, body) => F.push({ name, es, src: `export default (function () {\n${body}\n})();\n` });
// A probe that is a whole module (for module-level syntax).
const mod = (name, es, src) => F.push({ name, es, src });

// ---------------------------------------------------------------- ES2015
fn("let/const and block scope", 2015, `let a = 1; { let a = 2; } const b = [];
for (let i = 0; i < 3; i++) b.push(() => i);
let tdz; try { tdz = x; let x = 1; } catch (e) { tdz = e.name; }
return [a, b.map((f) => f()), tdz];`);
fn("arrow functions and lexical this", 2015, `const o = { v: 7, f() { return [1, 2].map((x) => x * this.v); }, g: (a, b = 2) => ({ a, b }) };
return [o.f(), o.g(1), (() => arguments.length)()];`);
fn("classes, extends, super, static, accessors, new.target", 2015, `class A { constructor(x) { this.x = x; this.nt = new.target.name; } get dbl() { return this.x * 2; } set dbl(v) { this.x = v / 2; } static make() { return new this(3); } m() { return "A" + this.x; } }
class B extends A { constructor() { super(5); } m() { return "B:" + super.m(); } static make() { return super.make(); } }
const b = new B(); b.dbl = 20;
return [b.m(), b.dbl, b.nt, B.make() instanceof B, Object.getOwnPropertyNames(A.prototype), typeof A];`);
fn("class extends built-ins", 2015, `class L extends Array { sum() { return this.reduce((a, b) => a + b, 0); } }
const l = L.from([1, 2, 3]); class E extends Error { constructor(m) { super(m); this.name = "E"; } }
return [l.sum(), l instanceof L, l.map((x) => x * 2) instanceof L, new E("m").message, new E("m") instanceof Error];`);
fn("template literals, tagged templates, String.raw", 2015, `const n = 2, tag = (s, ...v) => s.raw.join("|") + v.join(",");
return [\`a\${n + 1}b\${\`in\${n}\`}\`, tag\`x\${1}y\\n\${2}\`, String.raw\`\\t\${n}\`, \`multi
line\`];`);
fn("destructuring: arrays, objects, defaults, nested, swap", 2015, `const [a, , b = 5, ...r] = [1, 2, undefined, 4, 5];
const { x, y: { z = 9 } = {}, w: ww = 3 } = { x: 1, y: {} };
let p = 1, q = 2; [p, q] = [q, p];
const f = ({ k, l = 2 } = {}, [m] = [7]) => k + l + m;
return [a, b, r, x, z, ww, p, q, f({ k: 1 }), f()];`);
fn("default and rest parameters, spread in calls and arrays", 2015, `function f(a, b = a + 1, ...rest) { return [a, b, rest, arguments.length]; }
const arr = [1, 2, 3];
return [f(...arr, ...[4]), Math.max(...arr), [0, ...arr, ..."ab"], f.length];`);
fn("for-of, iterators, generators", 2015, `function* g(n) { for (let i = 0; i < n; i++) { const r = yield i; if (r) return r; } }
const out = []; for (const v of g(3)) out.push(v);
const it = g(5); it.next(); const sent = it.next("stop");
const obj = { *[Symbol.iterator]() { yield* [1, 2]; yield 3; } };
return [out, sent, [...obj], Array.from(new Map([[1, "a"]]).entries())];`);
fn("Map, Set, WeakMap, WeakSet", 2015, `const m = new Map([[1, "a"], ["1", "b"]]); m.set(NaN, "n");
const s = new Set([1, 1, 2, NaN, NaN]); const wm = new WeakMap(), k = {}; wm.set(k, 1);
const ws = new WeakSet([k]);
return [m.get(NaN), m.size, [...m.keys()], s.size, wm.get(k), ws.has(k), [...s].length];`);
fn("Symbol and well-known symbols", 2015, `const s = Symbol("d"), o = { [s]: 1, [Symbol.toPrimitive](h) { return h === "number" ? 42 : "str"; } };
class C { static [Symbol.hasInstance](x) { return x === 1; } }
return [typeof s, s.toString(), o[s], +o, \`\${o}\`, 1 instanceof C, Symbol.for("a") === Symbol.for("a"), Object.prototype.toString.call({ [Symbol.toStringTag]: "T" })];`);
fn("Promise", 2015, `return Promise.all([1, Promise.resolve(2), new Promise((r) => r(3))]).then((v) => Promise.race([v, 9]));`);
fn("Proxy and Reflect", 2015, `const p = new Proxy({}, { get: (t, k) => (k in t ? t[k] : "dflt:" + String(k)), has: () => true });
p.a = 1; return [p.a, p.b, "zz" in p, Reflect.ownKeys({ a: 1, [Symbol.iterator]: 0 }).length, Reflect.apply(Math.max, null, [1, 3]), Reflect.has({ x: 1 }, "x")];`);
fn("computed keys, shorthand properties and methods", 2015, `const k = "dyn", v = 1;
const o = { v, [k + 1]: 2, m() { return 3; }, get g() { return 4; }, ["s" + "q"]() { return 5; } };
return [o, o.m(), o.g, o.sq()];`);
fn("binary and octal literals, unicode code point escapes", 2015, `return [0b1011, 0o17, 0B11, 0O7, "\\u{1F600}".length, "\\u{41}", /\\u{1F600}/u.test("\\u{1F600}")];`);
fn("regex sticky and unicode flags", 2015, `const r = /a/y; r.lastIndex = 1;
return [r.test("ba"), r.lastIndex, /^.$/u.test("\\u{1F600}"), /x/gimuy.flags, "aXbX".split(/x/i)];`);
fn("Object.assign, Object.is, getOwnPropertySymbols, setPrototypeOf", 2015, `const s = Symbol("s");
const o = Object.assign({ a: 1 }, null, { b: 2, [s]: 3 }, "xy");
return [o, Object.is(NaN, NaN), Object.is(0, -0), Object.getOwnPropertySymbols(o).length, Object.getPrototypeOf(Object.setPrototypeOf({}, null))];`);
fn("Number and Math additions", 2015, `return [Number.isInteger(5.0), Number.isSafeInteger(2 ** 53), Number.EPSILON > 0, Number.MAX_SAFE_INTEGER, Number.parseFloat("1.5x"),
Number.isNaN("x"), Math.trunc(-4.7), Math.sign(-3), Math.cbrt(27), Math.log10(1000), Math.log2(8), Math.hypot(3, 4), Math.fround(5.5), Math.clz32(1),
Math.imul(3, 4), Math.expm1(0), Math.log1p(0), Math.sinh(0), Math.cosh(0), Math.tanh(0), Math.asinh(0), Math.acosh(1), Math.atanh(0)];`);
fn("Array additions (from, of, fill, find, copyWithin, entries)", 2015, `return [Array.from({ length: 3 }, (_, i) => i * 2), Array.of(7, 8), new Array(3).fill(0), [5, 6, 7].find((x) => x > 5), [5, 6, 7].findIndex((x) => x > 9),
[1, 2, 3, 4, 5].copyWithin(0, 3), [...["a", "b"].entries()], [..."ab"].keys().next()];`);
fn("String additions", 2015, `return ["abc".includes("b"), "abc".startsWith("ab"), "abc".endsWith("bc"), "ab".repeat(3), "\\u{1F600}".codePointAt(0), String.fromCodePoint(65, 0x1F600).length, "\\u0041\\u030A".normalize("NFC").length];`);
fn("typed arrays, ArrayBuffer, DataView", 2015, `const f = new Float64Array([1.5, NaN, -0]); const b = new ArrayBuffer(8); new DataView(b).setFloat64(0, Math.PI, true);
const u = new Uint8Array(b); return [f, new Float64Array(b)[0], u[0], Int16Array.from([1, 2]).map((x) => x * 3), new Uint8ClampedArray([300, -5])];`);
fn("labels, getters in classes, Object.getOwnPropertyDescriptor on accessors", 2015, `let n = 0; outer: for (const a of [1, 2]) for (const b of [1, 2]) { if (b === 2) continue outer; n += a; }
class P { get v() { return 1; } } return [n, typeof Object.getOwnPropertyDescriptor(P.prototype, "v").get];`);

// ---------------------------------------------------------------- ES2016, ES2017
fn("exponent operator and Array.prototype.includes", 2016, `let x = 2; x **= 3; return [2 ** 10, (-2) ** 2, 2 ** -1, x, [1, NaN].includes(NaN), [1, 2].includes(2, 2)];`);
fn("async functions and await", 2017, `async function f(x) { const a = await x; const b = await Promise.resolve(a + 1); return [a, b]; }
const g = async () => { try { await Promise.reject(new Error("no")); } catch (e) { return e.message; } };
return Promise.all([f(1), g(), (async function () {})()]);`);
fn("Object.values, entries, getOwnPropertyDescriptors", 2017, `const o = { a: 1, get b() { return 2; } };
return [Object.values(o), Object.entries(o), Object.keys(Object.getOwnPropertyDescriptors(o)), typeof Object.getOwnPropertyDescriptors(o).b.get];`);
fn("padStart and padEnd", 2017, `return ["5".padStart(3, "0"), "ab".padEnd(5, "xy"), "abc".padStart(2), "x".padEnd(3)];`);
fn("trailing commas in parameters and calls", 2017, `function f(a, b,) { return a + b; } return f(1, 2,);`);
fn("SharedArrayBuffer and Atomics", 2017, `const s = new SharedArrayBuffer(8), a = new Int32Array(s); Atomics.add(a, 0, 5); return [Atomics.load(a, 0), Atomics.isLockFree(4)];`);

// ---------------------------------------------------------------- ES2018
fn("object spread", 2018, `const s = Symbol("s"), base = { a: 1, b: 2, [s]: 3 };
let calls = 0; const getter = { get g() { calls++; return "got"; } };
const hidden = Object.defineProperty({ vis: 1 }, "hid", { value: 2, enumerable: false });
const proto = Object.create({ inherited: 1 }); proto.own = 2;
const o = { z: 0, ...base, b: 20, ...null, ...undefined, ...getter, ...hidden, ...proto, ..."hi", ...[7, 8], ...5, a: 10 };
const sp = { ...{ __proto__: null, x: 1 } };
return [o, calls, Object.getOwnPropertyDescriptor(o, "g").value, Object.getPrototypeOf(sp) === Object.prototype, { ...{ a: 1 }, a: 2 }, { a: 2, ...{ a: 1 } }];`);
fn("object spread does not call setters", 2018, `let set = 0; const o = { set a(v) { set++; }, ...{ a: 1 } };
const d = Object.getOwnPropertyDescriptor(o, "a"); return [set, d.value, d.writable, d.enumerable, d.configurable];`);
fn("object spread of arrays, strings, nested spreads in expressions", 2018, `const f = (x) => ({ ...x, n: 1 });
const deep = { a: { ...{ b: { ...{ c: 1 } } } } };
return [f([1, 2]), f("ab"), deep, [{ ...{ x: 1 } }, { ...{ y: 2 } }].map((o) => ({ ...o })), { ...(1 > 0 ? { t: 1 } : {}) }];`);
fn("object rest in destructuring declarations", 2018, `const s = Symbol("s");
const { a, b: { c, ...innerRest }, ...rest } = { a: 1, b: { c: 2, d: 3, e: 4 }, f: 5, [s]: 6, g: 7 };
const { [("x" + "y")]: xy, ...r2 } = { xy: 1, z: 2 };
let calls = 0; const src = { get k() { calls++; return 1; }, m: 2 };
const { ...all } = src;
const proto = Object.create({ inh: 1 }); proto.own = 1; const { ...ownOnly } = proto;
const arr = [1, 2, 3]; const { 0: first, ...arrRest } = arr;
const { length, ...strRest } = "ab";
return [a, c, innerRest, rest, Object.getOwnPropertySymbols(rest).length, xy, r2, all, calls, ownOnly, first, arrRest, length, strRest];`);
fn("object rest in assignments, parameters, for-of heads, catch", 2018, `let a, rest;
({ a, ...rest } = { a: 1, b: 2, c: 3 });
const f = ({ x, ...others }, [y, ...more], { z = 9, ...dflt } = {}) => [x, others, y, more, z, dflt];
const g = function ({ p: { q, ...pr }, ...top }) { return [q, pr, top]; };
const loop = []; for (const { id, ...vals } of [{ id: 1, v: 2 }, { id: 3, w: 4 }]) loop.push([id, vals]);
let caught; try { throw { code: 1, msg: "m", extra: true }; } catch ({ code, ...info }) { caught = [code, info]; }
const arrow = ({ k, ...r }) => r;
class C { m({ a, ...r }) { return r; } static s({ ...r }) { return r; } }
const objMethod = { m({ a, ...r }) { return r; } };
return [a, rest, f({ x: 1, o: 2 }, [3, 4, 5]), g({ p: { q: 1, r: 2 }, t: 3 }), loop, caught, arrow({ k: 1, j: 2 }), new C().m({ a: 1, b: 2 }), C.s({ c: 3 }), objMethod.m({ a: 1, d: 4 })];`);
fn("object rest with defaults, holes and nested arrays", 2018, `const [{ a = 1, ...r1 } = {}, [{ b, ...r2 }] = [{ b: 2, c: 3 }]] = [];
const { x: [y, { z, ...r3 }], ...r4 } = { x: [1, { z: 2, q: 3 }], w: 4 };
let o = {}; ({ a: o.p, ...o.rest } = { a: 1, b: 2 });
return [a, r1, b, r2, y, z, r3, r4, o];`);
fn("object rest errors on null and undefined", 2018, `const out = [];
for (const v of [null, undefined]) { try { const { ...r } = v; out.push("no error"); } catch (e) { out.push(e.constructor.name); } }
return out;`);
fn("async iteration and for await", 2018, `async function* gen() { yield 1; yield await Promise.resolve(2); }
return (async () => { const out = []; for await (const v of gen()) out.push(v); for await (const v of [Promise.resolve(3), 4]) out.push(v); return out; })();`);
fn("Promise.prototype.finally", 2018, `let ran = false; return Promise.resolve(1).finally(() => { ran = true; }).then((v) => [v, ran]);`);
fn("regex named groups", 2018, `const m = /(?<y>\\d{4})-(?<m>\\d{2})/.exec("2024-05");
return [m.groups.y, m.groups.m, "2024-05".replace(/(?<y>\\d+)-(?<m>\\d+)/, "$<m>/$<y>"), /(?<a>x)\\k<a>/.test("xx")];`);
fn("regex lookbehind", 2018, `return ["$10 €20".match(/(?<=\\$)\\d+/)[0], "a1b2".replace(/(?<![a])\\d/g, "#"), /(?<=a)b/.test("ab")];`);
fn("regex dotAll flag", 2018, `return [/a.b/s.test("a\\nb"), /a.b/.test("a\\nb"), /x/s.dotAll, /x/s.flags];`);
fn("regex unicode property escapes", 2018, `return [/\\p{L}+/u.exec("héllo1")[0], /\\p{Script=Greek}/u.test("α"), /\\P{N}/u.test("5")];`);

// ---------------------------------------------------------------- ES2019
fn("Array flat and flatMap", 2019, `return [[1, [2, [3, [4]]]].flat(), [1, [2, [3, [4]]]].flat(Infinity), [1, 2].flatMap((x) => [x, [x * 2]]), [1, , 3].flat()];`);
fn("Object.fromEntries", 2019, `return [Object.fromEntries([["a", 1], ["b", 2]]), Object.fromEntries(new Map([["m", 1]]))];`);
fn("trimStart and trimEnd", 2019, `return ["  a  ".trimStart(), "  a  ".trimEnd(), "\\u00a0x\\u00a0".trimStart().length, " y ".trimLeft(), " y ".trimRight()];`);
fn("optional catch binding", 2019, `let r; try { throw 1; } catch { r = "caught"; } return r;`);
fn("Symbol.prototype.description", 2019, `return [Symbol("d").description, Symbol().description];`);
fn("JSON superset and well-formed JSON.stringify", 2019, `return [" ".length, JSON.stringify("\\uD800"), JSON.stringify("\\uD83D\\uDE00")];`);
fn("stable Array.prototype.sort", 2019, `const a = Array.from({ length: 30 }, (_, i) => ({ k: i % 3, i }));
return a.sort((x, y) => x.k - y.k).map((o) => o.i);`);

// ---------------------------------------------------------------- ES2020
fn("BigInt", 2020, `return [10n ** 20n, BigInt(5) + 2n, typeof 1n, BigInt.asUintN(8, 257n)];`);
fn("optional chaining", 2020, `const o = { a: { b: [1, { c: 2 }], f() { return this.a; } } }, n = null;
return [o?.a?.b?.[1]?.c, n?.x.y.z, o.a.f?.(), o.nope?.(), n?.[0], o.a?.["b"].length, delete n?.x];`);
fn("nullish coalescing", 2020, `const z = 0, e = "", u = undefined, n = null; return [z ?? 1, e ?? "d", u ?? "d", n ?? (u ?? 3), (z || 5) ?? 6];`);
fn("globalThis", 2020, `return [typeof globalThis, globalThis.Math === Math];`);
fn("Promise.allSettled", 2020, `return Promise.allSettled([Promise.resolve(1), Promise.reject(new Error("x"))]).then((r) => r.map((s) => s.status));`);
fn("String.prototype.matchAll", 2020, `return [...("a1b22".matchAll(/\\d+/g))].map((m) => [m[0], m.index]);`);
mod("dynamic import()", 2020, `export default import("./probe-helper.js").then((m) => [m.helper(2), typeof m.default]);\n`);
mod("import.meta", 2020, `export default [typeof import.meta, typeof import.meta.url, import.meta.url.endsWith(".js")];\n`);
mod("export * as namespace", 2020, `export * as helpers from "./probe-helper.js";\nimport * as self from "./probe-helper.js";\nexport default [typeof self.helper, self.helper(3)];\n`);

// ---------------------------------------------------------------- ES2021
fn("logical assignment operators", 2021, `const o = { a: 0, b: null, c: 1 }; let n = 0, calls = 0;
const get = () => { calls++; return o; };
get().a ||= 5; get().b ??= 6; get().c &&= 7; o.d ??= 8; o.c ||= (n = 99);
let x = null; x ??= 1; x ||= 2; x &&= 3;
let s; const r = (s ??= "v");
const arr = [0]; let i = 0; arr[i++] ||= 4;
let setterCalls = 0; const so = { get p() { return 1; }, set p(v) { setterCalls++; } }; so.p ||= 2; so.p &&= 3;
return [o, n, calls, x, r, arr, i, setterCalls];`);
fn("numeric separators", 2021, `return [1_000_000, 0xF_F, 1_0.0_1, 0b1010_1010, 0o7_7, 1e1_0, .0_1, 0.000_001];`);
fn("String.prototype.replaceAll", 2021, `return ["a.b.c".replaceAll(".", "/"), "aaa".replaceAll("a", "$&$&"), "x-y".replaceAll(/-/g, "+"), "ab".replaceAll("", "_")];`);
fn("Promise.any and AggregateError", 2021, `return Promise.any([Promise.reject(1), Promise.resolve(2)]).then((v) => Promise.any([Promise.reject(3)]).catch((e) => [v, e.constructor.name, e.errors]));`);
fn("WeakRef and FinalizationRegistry", 2021, `const o = {}; const w = new WeakRef(o); const f = new FinalizationRegistry(() => {}); f.register(o, 1); return [w.deref() === o, typeof f.unregister];`);

// ---------------------------------------------------------------- ES2022
fn("public class fields (instance and static)", 2022, `class A { x = 1; y = this.x + 1; ["c" + "k"] = 3; static s = 4; static t = A.s + 1; arrow = () => this.x; z; }
class B extends A { w = this.y * 10; }
const b = new B(); return [b.x, b.y, b.ck, A.s, A.t, b.arrow(), b.w, "z" in b, Object.keys(b)];`);
fn("private fields, methods and accessors", 2022, `class C { #v = 1; static #count = 0; #m() { return this.#v * 10; } get #g() { return this.#v + 100; } set #g(x) { this.#v = x; }
  static #sm() { return "sm"; } constructor() { C.#count++; } inc() { this.#v++; this.#g = this.#g; return [this.#m(), C.#count, C.#sm()]; } static has(o) { return #v in o; } }
const c = new C(); new C(); return [c.inc(), C.has(c), C.has({}), Object.keys(c)];`);
fn("class static blocks", 2022, `class C { static x = 1; static { this.y = this.x + 1; C.z = 3; } } return [C.x, C.y, C.z];`);
mod("top-level await", 2022, `const v = await Promise.resolve(5);\nexport default [v, await (async () => 6)()];\n`);
fn("Array/String at()", 2022, `return [[1, 2, 3].at(-1), "abc".at(-2), [1].at(5), new Int8Array([4, 5]).at(-1)];`);
fn("Object.hasOwn", 2022, `return [Object.hasOwn({ a: 1 }, "a"), Object.hasOwn(Object.create({ b: 1 }), "b")];`);
fn("Error cause", 2022, `const e = new Error("outer", { cause: "inner" }); return [e.cause, "cause" in new Error("x"), new TypeError("t", { cause: 1 }).cause];`);
fn("regex match indices (d flag)", 2022, `const m = /b(c)/d.exec("abc"); return [m.indices[0], m.indices[1], /x/d.hasIndices];`);

// ---------------------------------------------------------------- ES2023
fn("findLast and findLastIndex", 2023, `const a = [1, 2, 3, 4]; return [a.findLast((x) => x % 2), a.findLastIndex((x) => x > 9), new Float64Array([1, 2]).findLast((x) => x < 2)];`);
fn("change array by copy (toSorted, toReversed, toSpliced, with)", 2023, `const a = [3, 1, 2];
return [a.toSorted(), a.toReversed(), a.toSpliced(1, 1, 9, 8), a.with(-1, 0), a, new Int8Array([3, 1]).toSorted(), [, 1].toSorted()];`);
mod("hashbang", 2023, `#!/usr/bin/env node\nexport default "ok";\n`);
fn("symbols as WeakMap keys", 2023, `const w = new WeakMap(), s = Symbol("k"); w.set(s, 1); return w.get(s);`);

// ---------------------------------------------------------------- ES2024
fn("Object.groupBy and Map.groupBy", 2024, `const g = Object.groupBy([1, 2, 3, 4], (x) => (x % 2 ? "odd" : "even"));
return [g, Object.getPrototypeOf(g), Map.groupBy([1, 2, 3], (x) => x > 1)];`);
fn("Promise.withResolvers", 2024, `const { promise, resolve } = Promise.withResolvers(); resolve(7); return promise;`);
fn("String isWellFormed and toWellFormed", 2024, `return ["ab".isWellFormed(), "a\\uD800".isWellFormed(), "a\\uD800".toWellFormed()];`);
fn("ArrayBuffer resize and transfer", 2024, `const b = new ArrayBuffer(2, { maxByteLength: 8 }); b.resize(6); const t = b.transfer(4); return [b.detached, t.byteLength, t.resizable];`);
fn("regex v flag", 2024, `return [/[\\p{L}--[a-z]]/v.test("a"), /[\\p{L}&&\\p{ASCII}]/v.test("é"), /x/v.unicodeSets];`);
fn("Atomics.waitAsync", 2024, `return typeof Atomics.waitAsync;`);

// ---------------------------------------------------------------- ES2025 (Node accepts these too)
fn("Set methods", 2025, `const a = new Set([1, 2, 3]), b = new Set([2, 3, 4]);
return [a.union(b), a.intersection(b), a.difference(b), a.symmetricDifference(b), a.isSubsetOf(b), new Set([2]).isSubsetOf(a), a.isSupersetOf(new Set([1])), a.isDisjointFrom(new Set([9]))];`);
fn("Iterator helpers", 2025, `function* n() { let i = 0; while (true) yield i++; }
return [n().map((x) => x * 2).filter((x) => x % 3).take(4).toArray(), n().drop(2).take(2).toArray(), n().take(5).reduce((a, b) => a + b), Iterator.from([1, 2]).toArray(), n().some((x) => x > 3), n().take(3).flatMap((x) => [x, x]).toArray()];`);
fn("Promise.try", 2025, `return Promise.try(() => 4);`);
fn("RegExp.escape", 2025, `return [RegExp.escape("a.b*c"), new RegExp(RegExp.escape("1+1")).test("1+1")];`);
fn("regex modifiers and duplicate named groups", 2025, `return [/(?i:a)b/.test("Ab"), /(?<y>\\d{4})|(?<y>\\d{2})/.exec("99").groups.y];`);
fn("Float16Array and Math.f16round", 2025, `return [Math.f16round(1.337), new Float16Array([1.5])[0]];`);

// ---------------------------------------------------------------- web/Node globals an indicator might use
fn("structuredClone", 0, `const d = new Date(0), o = { a: [1, { b: 2 }], d, m: new Map([[1, 2]]), s: new Set([1]) }; o.self = o;
const c = structuredClone(o); return [c.a, c.a !== o.a, c.d instanceof Date, c.d.getTime(), c.m.get(1), c.s.has(1), c.self === c];`);
fn("queueMicrotask", 0, `return new Promise((r) => queueMicrotask(() => r("mt")));`);
fn("console methods", 0, `return [typeof console.log, typeof console.warn, typeof console.error, typeof console.table];`);
fn("Intl.NumberFormat and toLocaleString", 0, `return [(1234.5).toFixed(1), typeof Intl, (1234.5).toLocaleString("en-US")];`);
fn("Date parsing and UTC formatting", 0, `return [new Date(Date.UTC(2024, 0, 31)).toISOString(), Date.parse("2024-02-01T00:00:00Z"), new Date("2024-02-01").getUTCDay(), new Date(0).toJSON()];`);

// ---------------------------------------------------------------- transform edge cases (syntax mixes)
fn("spread inside template literal substitutions with braces", 2018, `const a = { x: 1 }, t = \`{\${JSON.stringify({ ...a, y: \`\${"}"}\${{ ...a }.x}\` })}}\`;
return [t, \`...\${"{...a}"}\`, \`\${\`\${\`\${({ ...a }).x}\`}\`}\`];`);
fn("spread after regex literals and division", 2018, `const re = /{...x}/, d = 10 / 2 / 1, o = { ...{ r: re.source }, d };
const g = 4, h = 2, i = 1; const q = g / h / i; const w = { ...{ v: "a/b".split(/\\//) } };
return [o, q, w, /[/]{.../.test("/{...")];`);
fn("spread and rest next to comments and ASI", 2018, `const a = { x: 1 } // { ...not code }
/* ...{ nor this } */ const b = { ...a /* , ...c */, y: 2 }
const { x, ...r } = b
;[1].forEach((v) => v)
const c = { ...b,
  // comment
  z: 3, }
return [b, r, c]`);
fn("object spread with computed and numeric keys, ordering", 2018, `const o = { b: 1, 2: "two", a: 2, 1: "one" }, k = "kk";
return [Object.keys({ z: 0, ...o, [k]: 1, 0: "zero" }), Object.keys({ ...o, b: 3 })];`);
fn("rest of the same object is a fresh copy", 2018, `const src = { a: 1, b: { c: 2 } }; const { a, ...r } = src; r.b.c = 9; r.x = 1; return [src, r, "x" in src];`);
fn("logical assignment evaluates the target once", 2021, `let n = 0; const objs = [{ v: null }, { v: 0 }, { v: 1 }];
const next = () => objs[n++]; next().v ??= "a"; next().v ||= "b"; next().v &&= "c";
let k = 0; const keys = () => (k++, "v"); const o = { v: null }; o[keys()] ??= 1; o[keys()] ??= 2;
return [objs, n, o, k];`);
fn("spread of compute results in an indicator-like helper", 2018, `const ta = { compute: (id) => ({ upper: [1, 2], lower: [0, 1], id }) };
const bands = { ...ta.compute("bb"), ...ta.compute("kc", { length: 3 }), basis: [5] };
const pick = ({ upper, ...others }) => Object.keys(others);
return [bands, pick(bands)];`);
fn("getters and methods that are named like keywords", 2018, `const o = { get: 1, set: 2, async: 3, static: 4, of: 5, new: 6, ...{ get: 10 } }; const { get, ...r } = o; return [get, r];`);
fn("arrow returning object spread, spread in arguments and arrays", 2018, `const mk = (a, ...r) => ({ ...a, r }); const xs = [{ a: 1 }, { b: 2 }];
return [mk({ q: 1 }, 1, 2), xs.reduce((acc, x) => ({ ...acc, ...x }), {}), [...xs.map((x) => ({ ...x }))]];`);
fn("generators and async methods with object rest parameters", 2018, `function* g({ a, ...r }) { yield r; } const o = { async am({ x, ...r }) { return r; }, *gm({ y, ...r }) { yield r; } };
return Promise.all([o.am({ x: 1, z: 2 })]).then((v) => [g({ a: 1, b: 2 }).next().value, v[0], o.gm({ y: 1, w: 3 }).next().value]);`);
fn("default parameter values that use object spread", 2018, `function f(a, { b, ...r } = { ...{ b: 1, c: 2 } }) { return [a, b, r]; } return [f(1), f(1, { b: 3, d: 4 })];`);
fn("rest pattern in nested arrow params and destructured for-in/for loop heads", 2018, `const out = [];
for (let { a, ...r } = { a: 1, b: 2 }, i = 0; i < 2; i++) out.push([a, r, i]);
const pairs = Object.entries({ k: { v: 1, w: 2 } }).map(([key, { v, ...rest }]) => [key, v, rest]);
return [out, pairs];`);
fn("labels and object literals at statement start", 2018, `let n = 0; { const t = { ...{ a: 1 } }; n = t.a; } lbl: { break lbl; } return n;`);
fn("class fields with spread initialisers and private state", 2022, `class S { #state = { ...{ a: 1 }, b: 2 }; opts = { ...S.defaults }; static defaults = { x: 1 }; get state() { return { ...this.#state }; } }
return [new S().state, new S().opts];`);
fn("optional chaining with spread", 2020, `const o = { a: { b: { c: 1 } } }, n = null; return [{ ...o?.a?.b }, { ...n?.x }, { ...o.a?.["b"] }];`);
fn("exponent with spread and nullish", 2018, `const p = { e: 2 }; return { ...p, v: 3 ** (p.e ?? 1), w: { ...null } ?? 1 };`);
fn("keywords as property names after dots and in destructuring", 2018, `const o = { class: 1, function: 2, return: 3, ...{ if: 4 } }; const { class: c, ...others } = o; return [o.if, c, others];`);

// ---------------------------------------------------------------- V4 bugs and runtime cases
fn("two or more destructuring parameters", 2015, `const f = ({ a }, { b }) => [a, b];
const g = function ([x], [y] = [9], { z } = { z: 3 }) { return [x, y, z]; };
const sum = [{ v: 1 }, { v: 2 }, { v: 4 }].reduce(({ s, n }, { v }) => ({ s: s + v, n: n + 1 }), { s: 0, n: 0 });
const h = ({ k }, [m] = [7], ...rest) => [k, m, rest];
return [f({ a: 1 }, { b: 2 }), g([1]), g([1], [2], { z: 5 }), sum, h({ k: 1 }), h({ k: 1 }, [2], 3, 4), g.length, h.length];`);
fn("async methods that use super", 2017, `class A { async m(x) { return x * 2; } get v() { return 5; } }
class B extends A { async m(x) { const a = await super.m(x); const k = "m"; return [a, await super[k](1), super.v]; } }
const o = { __proto__: { hi() { return "hi"; } }, async m() { return super.hi() + "!"; } };
return Promise.all([new B().m(3), o.m()]);`);
fn("private member errors and brand checks", 2022, `class C { #x = 1; static read(o) { return o.#x; } static write(o) { o.#x = 2; } static has(o) { return #x in o; } }
const out = [];
for (const f of [() => C.read({}), () => C.write({}), () => C.has(1), () => C.read(new C())]) { try { out.push(f()); } catch (e) { out.push(e.constructor.name + ": " + e.message); } }
return out;`);
fn("class names and field order", 2022, `const K = class { a = 1; b = this.a + 1; };
const log = []; class P { x = log.push("P.x"); constructor() { log.push("P()"); } } class Q extends P { y = log.push("Q.y"); constructor() { log.push("Q before super"); super(); log.push("Q after super"); } }
new Q(); return [K.name, new K().b, (class { static s = 1; }).name, log, Object.keys(new K())];`);
fn("regular expression groups created at run time", 2018, `const re = new RegExp("(?<user>\\\\w+)@(?<host>\\\\w+)", "g"), s = "a@b, c@d";
const calls = []; const r = s.replace(re, (...args) => { calls.push(JSON.stringify(args.at(-1))); return args.at(-1).host; });
return [r, calls, [...s.matchAll(re)].map((m) => m.groups.user), s.replace(re, "$<host>.$<user>"), "x".match(/x/).groups, re.flags];`);
fn("for await with break and async generator return", 2018, `async function* g() { try { yield 1; yield 2; yield 3; } finally { done.push("cleanup"); } }
const done = [];
return (async () => { const out = []; for await (const v of g()) { out.push(v); if (v === 2) break; } const it = g(); await it.next(); const r = await it.return(9); return [out, r, done.length >= 1]; })();`);
fn("logical assignment on private fields, super and computed keys", 2022, `class A { static base = { n: 0 }; }
class B extends A { #c = null; static k = "n"; inc() { this.#c ??= 0; this.#c ||= 10; this.#c &&= this.#c + 1; return this.#c; } }
const o = { a: { b: null } }; let i = 0; const keys = ["x", "y"]; const t = {}; t[keys[i++]] ??= 1; t[keys[i++]] ||= 2; o.a.b ??= [];
return [new B().inc(), t, i, o];`);
fn("object spread with accessors, methods and __proto__ in later parts", 2018, `const base = { a: 1 };
// (V8 defines accessors after the other properties in the run right after a leading spread; the
// spec order is kept here, so the accessor sits before the spread.)
const o = { get g() { return this.a * 10; }, ...base, m() { return 1; }, __proto__: null, ...{ z: 2 } };
const d = Object.getOwnPropertyDescriptor(o, "g");
return [typeof d.get, o.g, Object.getPrototypeOf(o), Object.keys(o), o.m()];`);

fn("returning an assignment whose value is a call", 2015, `const q = { p: null, n(v) { return v * 2; } }; let z = null, k = "p";
function a(v) { return (q[k] = q.n(v)); } function b(v) { return v ? (z = q.n(v)) : 0; } const c = (v) => (q.c = q.n(v));
function d(v) { return v && (q.d = q.n(v)); } function e(v) { return (0, q.e = Math.max(v, 1)); }
return [a(1), q.p, b(2), z, c(3), q.c, d(4), q.d, e(5), q.e];`);

export const FEATURES = F;
export const HELPER = `export const helper = (x) => x * 10;\nexport default { h: 1 };\n`;
