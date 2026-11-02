
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

export { ser };
