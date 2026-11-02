// Evaluated on every custom indicator engine (src/quant/core/CustomIndicators.cpp) before a file is imported;
// compiled into the app as :/qtm/qtm-polyfills.js.
// V4 (Qt 6.12) lacks many built-ins that Node has (tests/quant/js probes them all). They are added
// here, spec-faithful, as non-enumerable members like the real ones, and the natives that don't
// follow the spec are replaced: Array.prototype.sort is not stable, Object.assign skips symbols,
// matches have no `groups`, Error ignores options.cause, JSON.stringify writes lone surrogates.
(function (g) {
  "use strict";
  // ---- helpers
  function put(o, name, value) {
    Object.defineProperty(o, name, { value: value, writable: true, configurable: true, enumerable: false });
  }
  function def(o, name, value) {
    if (!(name in o)) put(o, name, value);
  }
  function getter(o, name, fn) {
    Object.defineProperty(o, name, { get: fn, configurable: true, enumerable: false });
  }
  function named(name, fn) {
    Object.defineProperty(fn, "name", { value: name, configurable: true });
    return fn;
  }
  var hasOwn = Function.prototype.call.bind(Object.prototype.hasOwnProperty);
  function isObject(v) { return (typeof v === "object" && v !== null) || typeof v === "function"; }
  function callable(f, what) {
    if (typeof f !== "function") throw new TypeError((what || String(f)) + " is not a function");
    return f;
  }
  function coercible(v, method) {
    if (v == null) throw new TypeError(method + " called on null or undefined");
    return v;
  }
  function toInteger(v) { v = +v; return v !== v ? 0 : v === Infinity || v === -Infinity ? v : Math.trunc(v); }
  function toLength(v) { v = toInteger(v); return v <= 0 ? 0 : Math.min(v, 9007199254740991); }
  function rel(len, i) { i = toInteger(i); return i < 0 ? i + len : i; }
  function clampIndex(len, i, dflt) {
    if (i === undefined) return dflt;
    i = toInteger(i);
    return i < 0 ? Math.max(len + i, 0) : Math.min(i, len);
  }
  function toPrimitive(v, hint) {
    if (!isObject(v)) return v;
    var ex = v[Symbol.toPrimitive];
    if (ex != null) {
      var r = callable(ex).call(v, hint);
      if (isObject(r)) throw new TypeError("Cannot convert object to primitive value");
      return r;
    }
    var order = hint === "string" ? ["toString", "valueOf"] : ["valueOf", "toString"];
    for (var i = 0; i < 2; i++) {
      var f = v[order[i]];
      if (typeof f === "function") {
        var x = f.call(v);
        if (!isObject(x)) return x;
      }
    }
    throw new TypeError("Cannot convert object to primitive value");
  }
  function toKey(v) {
    v = toPrimitive(v, "string");
    return typeof v === "symbol" ? v : String(v);
  }
  function defineData(o, k, v) {
    Object.defineProperty(o, k, { value: v, writable: true, enumerable: true, configurable: true });
  }
  function getIterator(o) {
    var m = o == null ? undefined : o[Symbol.iterator];
    if (typeof m !== "function") throw new TypeError(String(o) + " is not iterable");
    var it = m.call(o);
    if (!isObject(it)) throw new TypeError("Result of the Symbol.iterator method is not an object");
    return it;
  }
  function closeIterator(it) {
    var r = it["return"];
    if (r != null) r.call(it);
  }
  function forOf(iterable, fn) {
    var it = getIterator(iterable), next = it.next;
    for (;;) {
      var step = next.call(it);
      if (!isObject(step)) throw new TypeError("Iterator result " + String(step) + " is not an object");
      if (step.done) return;
      var stop;
      try { stop = fn(step.value); } catch (e) { try { closeIterator(it); } catch (_) {} throw e; }
      if (stop === true) { closeIterator(it); return; }
    }
  }
  function speciesConstructor(o, dflt) {
    var C = o.constructor;
    if (C === undefined) return dflt;
    if (!isObject(C)) throw new TypeError("object.constructor is not an object");
    var S = C[Symbol.species];
    return S == null ? dflt : S;
  }
  // The species of an array method's result (ArraySpeciesCreate): Array unless a subclass says otherwise.
  function arraySpecies(o) {
    if (!Array.isArray(o)) return Array;
    var C = o.constructor;
    if (C === Array || C === undefined) return Array;
    if (isObject(C)) {
      C = C[Symbol.species];
      if (C === null || C === undefined) return Array;
    }
    return C;
  }

  // ---- symbols
  ["asyncIterator", "matchAll", "dispose", "asyncDispose"].forEach(function (n) {
    if (!Symbol[n]) Object.defineProperty(Symbol, n, { value: Symbol("Symbol." + n) });
  });
  if (!("description" in Symbol.prototype))
    getter(Symbol.prototype, "description", function () {
      var s = Symbol.prototype.toString.call(this);
      return s === "Symbol()" ? undefined : s.slice(7, -1);
    });
  if (typeof g.globalThis === "undefined")
    Object.defineProperty(g, "globalThis", { value: g, writable: true, configurable: true, enumerable: false });
  def(Reflect, Symbol.toStringTag, "Reflect");

  // ---- Object
  put(Object, "assign", named("assign", function assign(target, source) {
    var to = Object(coercible(target, "Object.assign"));
    for (var i = 1; i < arguments.length; i++) {
      var from = arguments[i];
      if (from == null) continue;
      from = Object(from);
      var keys = Reflect.ownKeys(from);
      for (var j = 0; j < keys.length; j++) {
        var d = Object.getOwnPropertyDescriptor(from, keys[j]);
        if (d && d.enumerable) to[keys[j]] = from[keys[j]];
      }
    }
    return to;
  }));
  put(Object, "fromEntries", function fromEntries(iterable) {
    var o = {};
    forOf(coercible(iterable, "Object.fromEntries"), function (e) {
      if (!isObject(e)) throw new TypeError("Iterator value " + String(e) + " is not an entry object");
      defineData(o, toKey(e[0]), e[1]);
    });
    return o;
  });
  def(Object, "hasOwn", function hasOwn_(o, k) { return hasOwn(Object(coercible(o, "Object.hasOwn")), toKey(k)); });
  function groupBy(items, cb, map) {
    coercible(items, "groupBy");
    callable(cb);
    var groups = map ? new Map() : Object.create(null), k = 0;
    forOf(items, function (v) {
      var key = cb(v, k++);
      if (map) {
        if (key === 0) key = 0;   // -0 -> +0
        if (!groups.has(key)) groups.set(key, []);
        groups.get(key).push(v);
      } else {
        key = toKey(key);
        if (!hasOwn(groups, key)) defineData(groups, key, []);
        groups[key].push(v);
      }
    });
    return groups;
  }
  def(Object, "groupBy", function groupBy_(items, cb) { return groupBy(items, cb, false); });
  def(Map, "groupBy", function groupBy_(items, cb) { return groupBy(items, cb, true); });
  function lookup(kind) {
    return function (k) {
      k = toKey(k);
      for (var o = Object(this); o !== null; o = Object.getPrototypeOf(o)) {
        var d = Object.getOwnPropertyDescriptor(o, k);
        if (d) return d[kind];
      }
      return undefined;
    };
  }
  def(Object.prototype, "__lookupGetter__", lookup("get"));
  def(Object.prototype, "__lookupSetter__", lookup("set"));
  def(Object.prototype, "__defineGetter__", function (k, f) { Object.defineProperty(Object(this), toKey(k), { get: callable(f), enumerable: true, configurable: true }); });
  def(Object.prototype, "__defineSetter__", function (k, f) { Object.defineProperty(Object(this), toKey(k), { set: callable(f), enumerable: true, configurable: true }); });

  // ---- Array
  // A stable merge sort (V4's sort is not stable). cmp(x, y) < 0 puts x first.
  function mergeSort(a, cmp) {
    var n = a.length;
    if (n < 2) return a;
    var buf = new Array(n), RUN = 8, i, j;
    for (var lo = 0; lo < n; lo += RUN) {   // insertion sort of short runs
      var hi = Math.min(lo + RUN, n);
      for (i = lo + 1; i < hi; i++) {
        var x = a[i];
        for (j = i - 1; j >= lo && cmp(a[j], x) > 0; j--) a[j + 1] = a[j];
        a[j + 1] = x;
      }
    }
    var src = a, dst = buf;
    for (var width = RUN; width < n; width *= 2) {
      for (var left = 0; left < n; left += 2 * width) {
        var mid = Math.min(left + width, n), right = Math.min(left + 2 * width, n), p = left, q = mid, k = left;
        while (p < mid && q < right) dst[k++] = cmp(src[q], src[p]) < 0 ? src[q++] : src[p++];
        while (p < mid) dst[k++] = src[p++];
        while (q < right) dst[k++] = src[q++];
      }
      var t = src; src = dst; dst = t;
    }
    if (src !== a) for (i = 0; i < n; i++) a[i] = src[i];
    return a;
  }
  var nativeSort = Array.prototype.sort;
  function sortCompare(comparefn) {
    if (comparefn !== undefined) return function (x, y) { var v = +comparefn(x, y); return v !== v ? 0 : v; };
    return function (x, y) { var a = String(x), b = String(y); return a < b ? -1 : a > b ? 1 : 0; };
  }
  function sortValues(items, comparefn) {
    // Without a comparator, values that are all strings, or all numbers other than -0, can't tell
    // a stable order from another: the native sort is fine and faster.
    if (comparefn === undefined) {
      var t = items.length ? typeof items[0] : "string", same = t === "string" || t === "number";
      for (var i = 0; same && i < items.length; i++) same = typeof items[i] === t && !(items[i] === 0 && 1 / items[i] < 0);
      if (same) return nativeSort.call(items);
    }
    return mergeSort(items, sortCompare(comparefn));
  }
  put(Array.prototype, "sort", function sort(comparefn) {
    if (comparefn !== undefined && typeof comparefn !== "function")
      throw new TypeError("The comparison function must be either a function or undefined");
    var o = Object(this), len = toLength(o.length), items = [], undef = 0, k;
    for (k = 0; k < len; k++) {
      if (!(k in o)) continue;
      var v = o[k];
      if (v === undefined) undef++; else items.push(v);
    }
    sortValues(items, comparefn);
    for (k = 0; k < items.length; k++) o[k] = items[k];
    for (; k < items.length + undef; k++) o[k] = undefined;
    for (; k < len; k++) delete o[k];
    return o;
  });
  // Results built by the native method, then put in the subclass's species when there is one.
  function speciesResult(name, native) {
    put(Array.prototype, name, named(name, function (a, b) {
      var C = arraySpecies(this);
      var r = native.apply(this, arguments);
      if (C === Array) return r;
      var out = new C(name === "map" ? r.length : 0);
      for (var i = 0; i < r.length; i++) if (i in r) defineData(out, i, r[i]);
      out.length = r.length;
      return out;
    }));
    Object.defineProperty(Array.prototype[name], "length", { value: native.length });
  }
  ["map", "filter", "slice", "splice", "concat"].forEach(function (n) { speciesResult(n, Array.prototype[n]); });
  function flatten(out, arr, depth) {
    for (var i = 0, n = toLength(arr.length); i < n; i++) {
      if (!(i in arr)) continue;
      var v = arr[i];
      if (depth > 0 && Array.isArray(v)) flatten(out, v, depth - 1); else out.push(v);
    }
    return out;
  }
  def(Array.prototype, "flat", function flat() {
    var depth = arguments[0];
    return flatten([], Object(this), depth === undefined ? 1 : toInteger(depth));
  });
  def(Array.prototype, "flatMap", function flatMap(fn) {
    var o = Object(this), thisArg = arguments[1], out = [];
    if (typeof fn !== "function") throw new TypeError("flatMap mapper is not callable");
    for (var i = 0, n = toLength(o.length); i < n; i++) {
      if (!(i in o)) continue;
      var v = fn.call(thisArg, o[i], i, o);
      if (Array.isArray(v)) flatten(out, v, 0); else out.push(v);
    }
    return out;
  });
  speciesResult("flat", Array.prototype.flat);
  speciesResult("flatMap", Array.prototype.flatMap);
  function findLastOf(proto) {
    def(proto, "findLast", function findLast(fn) {
      var o = Object(this), thisArg = arguments[1];
      callable(fn);
      for (var i = toLength(o.length) - 1; i >= 0; i--) if (fn.call(thisArg, o[i], i, o)) return o[i];
      return undefined;
    });
    def(proto, "findLastIndex", function findLastIndex(fn) {
      var o = Object(this), thisArg = arguments[1];
      callable(fn);
      for (var i = toLength(o.length) - 1; i >= 0; i--) if (fn.call(thisArg, o[i], i, o)) return i;
      return -1;
    });
    def(proto, "at", function at(i) {
      var o = Object(this), len = toLength(o.length), k = rel(len, i);
      return k < 0 || k >= len ? undefined : o[k];
    });
  }
  findLastOf(Array.prototype);
  function copyOf(o) {
    var len = toLength(o.length), a = new Array(len);
    for (var i = 0; i < len; i++) a[i] = o[i];
    return a;
  }
  def(Array.prototype, "toReversed", function toReversed() { return copyOf(Object(this)).reverse(); });
  def(Array.prototype, "toSorted", function toSorted(cmp) {
    if (cmp !== undefined && typeof cmp !== "function") throw new TypeError("The comparison function must be either a function or undefined");
    return copyOf(Object(this)).sort(cmp);
  });
  def(Array.prototype, "toSpliced", function toSpliced(start, skipCount) {
    var o = Object(this), len = toLength(o.length), s = clampIndex(len, start, 0), skip, items = Array.prototype.slice.call(arguments, 2);
    if (arguments.length === 0) skip = 0;
    else if (arguments.length === 1) skip = len - s;
    else skip = Math.min(Math.max(toInteger(skipCount), 0), len - s);
    var out = [], i;
    for (i = 0; i < s; i++) out.push(o[i]);
    for (i = 0; i < items.length; i++) out.push(items[i]);
    for (i = s + skip; i < len; i++) out.push(o[i]);
    return out;
  });
  def(Array.prototype, "with", named("with", function (index, value) {
    var o = Object(this), len = toLength(o.length), k = rel(len, index);
    if (k < 0 || k >= len) throw new RangeError("Invalid index : " + index);
    var a = copyOf(o);
    a[k] = value;
    return a;
  }));
  def(Array, "fromAsync", function fromAsync(items) {
    var C = this, mapfn = arguments[1], thisArg = arguments[2];
    return g.__qtm.async(null, null, function* () {
      if (mapfn !== undefined) callable(mapfn);
      var A = typeof C === "function" && C !== Array ? new C() : [], k = 0, v;
      if (items != null && (items[Symbol.asyncIterator] != null || items[Symbol.iterator] != null)) {
        var it = g.__qtm.asyncIterator(items);
        for (;;) {
          var r = yield it.next();
          if (r.done) break;
          v = r.value;
          if (mapfn !== undefined) v = yield mapfn.call(thisArg, v, k);
          defineData(A, k++, v);
        }
      } else {
        var o = Object(items), len = toLength(o.length);
        for (; k < len; k++) {
          v = yield o[k];
          if (mapfn !== undefined) v = yield mapfn.call(thisArg, v, k);
          defineData(A, k, v);
        }
      }
      A.length = k;
      return A;
    });
  });

  // ---- typed arrays
  var TA = Object.getPrototypeOf(Int8Array.prototype);
  findLastOf(TA);
  function typedCompare(comparefn) {
    if (comparefn !== undefined) return function (x, y) { var v = +comparefn(x, y); return v !== v ? 0 : v; };
    return function (x, y) {
      if (x !== x) return y !== y ? 0 : 1;
      if (y !== y) return -1;
      if (x < y) return -1;
      if (x > y) return 1;
      if (x === 0 && y === 0) return 1 / x < 0 ? (1 / y < 0 ? 0 : -1) : 1 / y < 0 ? 1 : 0;
      return 0;
    };
  }
  put(TA, "sort", function sort(comparefn) {
    if (comparefn !== undefined && typeof comparefn !== "function")
      throw new TypeError("The comparison function must be either a function or undefined");
    var items = Array.prototype.slice.call(this);
    mergeSort(items, typedCompare(comparefn));
    for (var i = 0; i < items.length; i++) this[i] = items[i];
    return this;
  });
  def(TA, "toSorted", function toSorted(cmp) {
    if (cmp !== undefined && typeof cmp !== "function") throw new TypeError("The comparison function must be either a function or undefined");
    var out = new this.constructor(this.length);
    out.set(this);
    return out.sort(cmp);
  });
  def(TA, "toReversed", function toReversed() {
    var len = this.length, out = new this.constructor(len);
    for (var i = 0; i < len; i++) out[i] = this[len - 1 - i];
    return out;
  });
  def(TA, "with", named("with", function (index, value) {
    var len = this.length, k = rel(len, index);
    value = +value;
    if (k < 0 || k >= len) throw new RangeError("Invalid typed array index");
    var out = new this.constructor(len);
    out.set(this);
    out[k] = value;
    return out;
  }));

  // ---- Math, DataView float16
  function roundHalfEven(q) {
    var f = Math.floor(q), d = q - f;
    return d > 0.5 ? f + 1 : d < 0.5 ? f : f % 2 === 0 ? f : f + 1;
  }
  function f16round(x) {
    x = +x;
    if (x !== x || x === 0 || x === Infinity || x === -Infinity) return x;
    var sign = x < 0 ? -1 : 1, a = Math.abs(x);
    if (a >= 65520) return sign * Infinity;
    if (a < 6.103515625e-5) return sign * roundHalfEven(a * 16777216) / 16777216;
    var e = Math.floor(Math.log2(a));
    if (Math.pow(2, e) > a) e--;
    if (Math.pow(2, e + 1) <= a) e++;
    var p = Math.pow(2, e);
    return sign * roundHalfEven(a / p * 1024) / 1024 * p;
  }
  def(Math, "f16round", function f16round_(x) { return f16round(x); });
  def(DataView.prototype, "getFloat16", function getFloat16(offset) {
    var h = this.getUint16(offset, arguments[1]), s = h & 0x8000 ? -1 : 1, e = (h >> 10) & 0x1f, m = h & 0x3ff;
    if (e === 0) return s * m * Math.pow(2, -24);
    if (e === 31) return m ? NaN : s * Infinity;
    return s * (1 + m / 1024) * Math.pow(2, e - 15);
  });
  def(DataView.prototype, "setFloat16", function setFloat16(offset, value) {
    var v = f16round(value), h;
    if (v !== v) h = 0x7e00;
    else {
      var s = v < 0 || (v === 0 && 1 / v < 0) ? 0x8000 : 0, a = Math.abs(v);
      if (a === Infinity) h = s | 0x7c00;
      else if (a < 6.103515625e-5) h = s | Math.round(a * 16777216);
      else {
        var e = Math.floor(Math.log2(a));
        if (Math.pow(2, e) > a) e--;
        if (Math.pow(2, e + 1) <= a) e++;
        h = s | ((e + 15) << 10) | Math.round((a / Math.pow(2, e) - 1) * 1024);
      }
    }
    this.setUint16(offset, h, arguments[2]);
  });

  // ---- String
  def(String.prototype, "at", function at(i) {
    var s = String(coercible(this, "String.prototype.at")), k = rel(s.length, i);
    return k < 0 || k >= s.length ? undefined : s.charAt(k);
  });
  var WS = "[\\s\\uFEFF\\xA0]+";
  var trimStart = function trimStart() { return String(coercible(this, "String.prototype.trimStart")).replace(new RegExp("^" + WS), ""); };
  var trimEnd = function trimEnd() { return String(coercible(this, "String.prototype.trimEnd")).replace(new RegExp(WS + "$"), ""); };
  if (!String.prototype.trimStart) { put(String.prototype, "trimStart", trimStart); put(String.prototype, "trimEnd", trimEnd); }
  def(String.prototype, "trimLeft", String.prototype.trimStart);
  def(String.prototype, "trimRight", String.prototype.trimEnd);
  def(String.prototype, "isWellFormed", function isWellFormed() {
    var s = String(coercible(this, "String.prototype.isWellFormed"));
    for (var i = 0; i < s.length; i++) {
      var c = s.charCodeAt(i);
      if (c >= 0xd800 && c <= 0xdbff) {
        var d = s.charCodeAt(i + 1);
        if (d >= 0xdc00 && d <= 0xdfff) { i++; continue; }
        return false;
      }
      if (c >= 0xdc00 && c <= 0xdfff) return false;
    }
    return true;
  });
  def(String.prototype, "toWellFormed", function toWellFormed() {
    var s = String(coercible(this, "String.prototype.toWellFormed")), out = "";
    for (var i = 0; i < s.length; i++) {
      var c = s.charCodeAt(i);
      if (c >= 0xd800 && c <= 0xdbff) {
        var d = s.charCodeAt(i + 1);
        if (d >= 0xdc00 && d <= 0xdfff) { out += s[i] + s[i + 1]; i++; continue; }
        out += String.fromCharCode(0xfffd);
      } else if (c >= 0xdc00 && c <= 0xdfff) out += String.fromCharCode(0xfffd);
      else out += s[i];
    }
    return out;
  });
  [["anchor", "a", "name"], ["big", "big"], ["blink", "blink"], ["bold", "b"], ["fixed", "tt"], ["fontcolor", "font", "color"],
   ["fontsize", "font", "size"], ["italics", "i"], ["link", "a", "href"], ["small", "small"], ["strike", "strike"], ["sub", "sub"], ["sup", "sup"]]
    .forEach(function (h) {
      def(String.prototype, h[0], named(h[0], function (v) {
        var s = String(coercible(this, "String.prototype." + h[0]));
        var attr = h[2] ? " " + h[2] + "=\"" + String(v).replace(/"/g, "&quot;") + "\"" : "";
        return "<" + h[1] + attr + ">" + s + "</" + h[1] + ">";
      }));
    });

  // ---- RegExp: named groups and the s flag
  // V4 parses (?<name>...) as a plain group and \k<name> as its back reference, but its matches have
  // no `groups`, and it has no s flag. Regular expressions are created through a wrapper that
  // remembers the group names (and turns . into [\s\S] for the s flag); exec adds `groups`.
  // (The metadata is a hidden property, not a WeakMap entry: V4's WeakMap crashed under GC here.)
  var NativeRegExp = RegExp, META = Symbol("regexp metadata"), RP = NativeRegExp.prototype;
  var reMeta = {
    get: function (re) { return isObject(re) && hasOwn(re, META) ? re[META] : undefined; },
    set: function (re, m) { Object.defineProperty(re, META, { value: m }); }
  };
  var nativeExec = RP.exec, nativeReplace = String.prototype.replace;
  function analyze(src, flags) {
    var names = null, out = "", cls = false, group = 0, dotAll = flags.indexOf("s") >= 0, unicode = flags.indexOf("u") >= 0;
    for (var i = 0; i < src.length; i++) {
      var c = src[i];
      if (c === "\\") {
        if (unicode && (src[i + 1] === "p" || src[i + 1] === "P"))
          throw new SyntaxError("Invalid regular expression: /" + src + "/" + flags + ": Unicode property escapes are not supported by this app's JavaScript engine");
        out += c + (i + 1 < src.length ? src[i + 1] : "");
        i++;
        continue;
      }
      if (cls) { if (c === "]") cls = false; out += c; continue; }
      if (c === "[") { cls = true; out += c; continue; }
      if (c === "(") {
        if (src[i + 1] !== "?") group++;
        else if (src[i + 2] === "<" && src[i + 3] !== "=" && src[i + 3] !== "!") {
          group++;
          if (!names) names = [];
          names.push([src.slice(i + 3, src.indexOf(">", i)), group]);
        }
      }
      out += c === "." && dotAll ? "[\\s\\S]" : c;
    }
    return { source: dotAll ? out : src, flags: dotAll ? flags.replace("s", "") : flags, names: names, dotAll: dotAll };
  }
  function RegExpW(pattern, flags) {
    var isRe = pattern instanceof NativeRegExp, meta = isRe ? reMeta.get(pattern) : undefined;
    if (new.target === undefined && isRe && flags === undefined && pattern.constructor === RegExpW) return pattern;
    var src = isRe ? (meta ? meta.source : pattern.source) : pattern === undefined ? "" : String(pattern);
    var fl = flags === undefined ? (isRe ? (meta ? meta.flags : pattern.flags) : "") : String(flags);
    var a = analyze(src, fl);
    var re = Reflect.construct(NativeRegExp, [a.source, a.flags], new.target || RegExpW);
    if (a.names || a.dotAll) reMeta.set(re, { names: a.names, source: src, flags: fl });
    return re;
  }
  RegExpW.prototype = RP;
  put(RP, "constructor", RegExpW);
  Object.setPrototypeOf(RegExpW, Object.getPrototypeOf(NativeRegExp));
  Object.defineProperty(RegExpW, Symbol.species, { get: function () { return this; }, configurable: true });
  Object.getOwnPropertyNames(NativeRegExp).forEach(function (k) {
    if (!(k in RegExpW) || k === "$1") {
      var d = Object.getOwnPropertyDescriptor(NativeRegExp, k);
      try { Object.defineProperty(RegExpW, k, d); } catch (e) {}
    }
  });
  put(g, "RegExp", RegExpW);
  put(RP, "exec", function exec(s) {
    var m = nativeExec.call(this, s);
    if (m !== null) {
      var meta = reMeta.get(this), groups;
      if (meta && meta.names) {
        groups = Object.create(null);
        for (var i = 0; i < meta.names.length; i++) groups[meta.names[i][0]] = m[meta.names[i][1]];
      }
      m.groups = groups;
    }
    return m;
  });
  var FLAGS = [["hasIndices", "d"], ["global", "g"], ["ignoreCase", "i"], ["multiline", "m"], ["dotAll", "s"], ["unicode", "u"], ["unicodeSets", "v"], ["sticky", "y"]];
  var nativeSource = Object.getOwnPropertyDescriptor(RP, "source").get;
  getter(RP, "source", function () { var m = reMeta.get(this); return m ? m.source : nativeSource.call(this); });
  getter(RP, "dotAll", function () { var m = reMeta.get(this); return m ? m.flags.indexOf("s") >= 0 : RP === this ? undefined : false; });
  getter(RP, "hasIndices", function () { return RP === this ? undefined : false; });
  getter(RP, "unicodeSets", function () { return RP === this ? undefined : false; });
  getter(RP, "flags", function () {
    var out = "";
    for (var i = 0; i < FLAGS.length; i++) if (this[FLAGS[i][0]]) out += FLAGS[i][1];
    return out;
  });
  def(RegExpW, "escape", function escape(s) {
    if (typeof s !== "string") throw new TypeError("RegExp.escape requires a string");
    var out = "";
    for (var i = 0; i < s.length; i++) {
      var c = s[i], code = s.charCodeAt(i);
      if (i === 0 && /[0-9A-Za-z]/.test(c)) out += "\\x" + code.toString(16);
      else if ("^$\\.*+?()[]{}|/".indexOf(c) >= 0) out += "\\" + c;
      else if (",-=<>#&!%:;@~'`\"".indexOf(c) >= 0 || [9, 10, 11, 12, 13, 32, 0xa0, 0x1680, 0x2028, 0x2029, 0x202f, 0x205f, 0x3000, 0xfeff].indexOf(code) >= 0 || (code >= 0x2000 && code <= 0x200a))
        out += code <= 0xff ? "\\x" + ("0" + code.toString(16)).slice(-2) : "\\u" + ("000" + code.toString(16)).slice(-4);
      else out += c;
    }
    return out;
  });
  // GetSubstitution: $$, $&, $`, $', $n, $nn, $<name>.
  function substitution(matched, str, position, captures, groups, replacement) {
    var out = "", m = captures.length;
    for (var i = 0; i < replacement.length; i++) {
      var c = replacement[i];
      if (c !== "$" || i + 1 >= replacement.length) { out += c; continue; }
      var n = replacement[i + 1];
      if (n === "$") { out += "$"; i++; }
      else if (n === "&") { out += matched; i++; }
      else if (n === "`") { out += str.slice(0, position); i++; }
      else if (n === "'") { out += str.slice(Math.min(position + matched.length, str.length)); i++; }
      else if (n >= "0" && n <= "9") {
        var two = replacement[i + 2], d1 = +n, d2 = two >= "0" && two <= "9" ? d1 * 10 + +two : -1;
        if (d2 >= 1 && d2 <= m) { out += captures[d2 - 1] === undefined ? "" : captures[d2 - 1]; i += 2; }
        else if (d1 >= 1 && d1 <= m) { out += captures[d1 - 1] === undefined ? "" : captures[d1 - 1]; i++; }
        else out += "$";
      } else if (n === "<" && groups !== undefined) {
        var close = replacement.indexOf(">", i + 2);
        if (close < 0) { out += "$"; continue; }
        var v = groups[replacement.slice(i + 2, close)];
        out += v === undefined ? "" : String(v);
        i = close;
      } else out += "$";
    }
    return out;
  }
  function advance(s, index, unicode) {
    if (!unicode || index + 1 >= s.length) return index + 1;
    var c = s.charCodeAt(index);
    return c >= 0xd800 && c <= 0xdbff && s.charCodeAt(index + 1) >= 0xdc00 && s.charCodeAt(index + 1) <= 0xdfff ? index + 2 : index + 1;
  }
  // RegExp.prototype[Symbol.replace] for the expressions that have group names.
  function replaceGroups(re, str, replaceValue) {
    var functional = typeof replaceValue === "function", global = re.global, results = [];
    if (!functional) replaceValue = String(replaceValue);
    if (global) re.lastIndex = 0;
    for (;;) {
      var m = re.exec(str);
      if (m === null) break;
      results.push(m);
      if (!global) break;
      if (String(m[0]) === "") re.lastIndex = advance(str, toLength(re.lastIndex), re.unicode);
    }
    var out = "", next = 0;
    for (var i = 0; i < results.length; i++) {
      var r = results[i], matched = String(r[0]), pos = Math.max(Math.min(toInteger(r.index), str.length), 0), caps = [];
      for (var k = 1; k < r.length; k++) caps.push(r[k] === undefined ? undefined : String(r[k]));
      var groups = r.groups, rep;
      if (functional) {
        var args = [matched].concat(caps, [pos, str]);
        if (groups !== undefined) args.push(groups);
        rep = String(replaceValue.apply(undefined, args));
      } else rep = substitution(matched, str, pos, caps, groups === undefined ? undefined : Object(groups), replaceValue);
      if (pos >= next) { out += str.slice(next, pos) + rep; next = pos + matched.length; }
    }
    return out + str.slice(next);
  }
  function hasGroups(re) { var m = re instanceof NativeRegExp ? reMeta.get(re) : undefined; return !!(m && m.names); }
  put(String.prototype, "replace", function replace(searchValue, replaceValue) {
    if (hasGroups(searchValue)) return replaceGroups(searchValue, String(coercible(this, "String.prototype.replace")), replaceValue);
    return nativeReplace.call(this, searchValue, replaceValue);
  });
  put(String.prototype, "replaceAll", function replaceAll(searchValue, replaceValue) {
    var o = coercible(this, "String.prototype.replaceAll");
    if (searchValue != null && searchValue instanceof NativeRegExp) {
      if (String(searchValue.flags).indexOf("g") < 0) throw new TypeError("replaceAll must be called with a global RegExp");
      return String.prototype.replace.call(o, searchValue, replaceValue);
    }
    var string = String(o), search = String(searchValue), functional = typeof replaceValue === "function";
    if (!functional) replaceValue = String(replaceValue);
    var step = Math.max(1, search.length), positions = [], pos = string.indexOf(search, 0);
    while (pos !== -1) {
      positions.push(pos);
      if (pos + step > string.length) break;
      pos = string.indexOf(search, pos + step);
    }
    var out = "", end = 0;
    for (var i = 0; i < positions.length; i++) {
      var p = positions[i];
      out += string.slice(end, p) + (functional ? String(replaceValue(search, p, string)) : substitution(search, string, p, [], undefined, replaceValue));
      end = p + search.length;
    }
    return out + string.slice(end);
  });
  var nativeMatch = String.prototype.match;
  put(String.prototype, "match", function match(regexp) {
    if (regexp instanceof NativeRegExp && !regexp.global) return regexp.exec(String(coercible(this, "String.prototype.match")));
    return nativeMatch.call(this, regexp);
  });
  var RegExpStringIterator = Object.create(Object.getPrototypeOf(Object.getPrototypeOf([][Symbol.iterator]())));
  put(RegExpStringIterator, "next", function next() {
    var s = this.__state;
    if (s.done) return { value: undefined, done: true };
    var m = s.re.exec(s.str);
    if (m === null) { s.done = true; return { value: undefined, done: true }; }
    if (!s.global) { s.done = true; return { value: m, done: false }; }
    if (String(m[0]) === "") s.re.lastIndex = advance(s.str, toLength(s.re.lastIndex), s.unicode);
    return { value: m, done: false };
  });
  def(RegExpStringIterator, Symbol.toStringTag, "RegExp String Iterator");
  def(RP, Symbol.matchAll, named("[Symbol.matchAll]", function (string) {
    var str = String(string), C = speciesConstructor(this, RegExpW), flags = String(this.flags);
    var matcher = new C(this, flags);
    matcher.lastIndex = toLength(this.lastIndex);
    var it = Object.create(RegExpStringIterator);
    Object.defineProperty(it, "__state", { value: { re: matcher, str: str, global: flags.indexOf("g") >= 0, unicode: flags.indexOf("u") >= 0, done: false } });
    return it;
  }));
  def(String.prototype, "matchAll", function matchAll(regexp) {
    var o = coercible(this, "String.prototype.matchAll");
    if (regexp != null) {
      if (regexp instanceof NativeRegExp && String(regexp.flags).indexOf("g") < 0)
        throw new TypeError("String.prototype.matchAll called with a non-global RegExp argument");
      var m = regexp[Symbol.matchAll];
      if (m != null) return m.call(regexp, o);
    }
    return new RegExpW(regexp, "g")[Symbol.matchAll](String(o));
  });

  // ---- Error: options.cause, AggregateError
  // The wrapper's frame is the error's location in V4: point lineNumber and fileName at the code
  // that made the error (the first frame outside this file) instead.
  function relocate(e) {
    var frames = String(e.stack || "").split("\n");
    for (var i = 0; i < frames.length; i++) {
      if (frames[i].indexOf("qtm-polyfills.js") >= 0) continue;
      var m = /@(.*):(\d+)$/.exec(frames[i]);
      if (m) { put(e, "fileName", m[1]); put(e, "lineNumber", +m[2]); }
      return;
    }
  }
  function wrapError(name, parent) {
    var Native = g[name];
    var W = named(name, function (message, options) {
      var e = Reflect.construct(Native, message === undefined ? [] : [message], new.target || W);
      relocate(e);
      if (isObject(options) && "cause" in options) put(e, "cause", options.cause);
      return e;
    });
    Object.defineProperty(W, "length", { value: 1 });
    W.prototype = Native.prototype;
    put(Native.prototype, "constructor", W);
    Object.setPrototypeOf(W, parent || Object.getPrototypeOf(Native));
    Object.getOwnPropertyNames(Native).forEach(function (k) {
      if (!hasOwn(W, k)) try { Object.defineProperty(W, k, Object.getOwnPropertyDescriptor(Native, k)); } catch (e) {}
    });
    put(g, name, W);
    return W;
  }
  var ErrorW = wrapError("Error");
  ["TypeError", "RangeError", "SyntaxError", "ReferenceError", "EvalError", "URIError"].forEach(function (n) { wrapError(n, ErrorW); });
  def(ErrorW, "captureStackTrace", function captureStackTrace(o) { put(o, "stack", String(new ErrorW().stack || "")); });
  def(ErrorW, "stackTraceLimit", 10);
  if (typeof g.AggregateError === "undefined") {
    var AggregateError = named("AggregateError", function (errors, message, options) {
      var e = Reflect.construct(ErrorW, message === undefined ? [] : [message, options], new.target || AggregateError);
      relocate(e);
      var list = [];
      forOf(errors, function (x) { list.push(x); });
      put(e, "errors", list);
      return e;
    });
    Object.defineProperty(AggregateError, "length", { value: 2 });
    AggregateError.prototype = Object.create(ErrorW.prototype);
    put(AggregateError.prototype, "constructor", AggregateError);
    put(AggregateError.prototype, "name", "AggregateError");
    put(AggregateError.prototype, "message", "");
    Object.setPrototypeOf(AggregateError, ErrorW);
    put(g, "AggregateError", AggregateError);
  }

  // ---- Promise
  def(Promise.prototype, "finally", named("finally", function (onFinally) {
    var C = speciesConstructor(this, Promise);
    if (typeof onFinally !== "function") return this.then(onFinally, onFinally);
    return this.then(function (v) { return C.resolve(onFinally()).then(function () { return v; }); },
                     function (e) { return C.resolve(onFinally()).then(function () { throw e; }); });
  }));
  function settleAll(C, iterable, onValue, onError, finish) {
    return new C(function (resolve, reject) {
      var results = [], remaining = 1, i = 0;
      var resolveFn = callable(C.resolve);
      forOf(iterable, function (x) {
        var k = i++;
        results.push(undefined);
        remaining++;
        var done = false;
        resolveFn.call(C, x).then(function (v) {
          if (done) return; done = true;
          if (onValue(results, k, v, resolve, reject) && --remaining === 0) finish(results, resolve, reject);
        }, function (e) {
          if (done) return; done = true;
          if (onError(results, k, e, resolve, reject) && --remaining === 0) finish(results, resolve, reject);
        });
      });
      if (--remaining === 0) finish(results, resolve, reject);
    });
  }
  def(Promise, "allSettled", function allSettled(iterable) {
    return settleAll(this, iterable,
      function (r, k, v) { r[k] = { status: "fulfilled", value: v }; return true; },
      function (r, k, e) { r[k] = { status: "rejected", reason: e }; return true; },
      function (r, resolve) { resolve(r); });
  });
  def(Promise, "any", function any(iterable) {
    return settleAll(this, iterable,
      function (r, k, v, resolve) { resolve(v); return false; },
      function (r, k, e) { r[k] = e; return true; },
      function (r, resolve, reject) { reject(new g.AggregateError(r, "All promises were rejected")); });
  });
  def(Promise, "withResolvers", function withResolvers() {
    var out = {};
    out.promise = new this(function (resolve, reject) { out.resolve = resolve; out.reject = reject; });
    return out;
  });
  def(Promise, "try", named("try", function (fn) {
    var args = Array.prototype.slice.call(arguments, 1);
    return new this(function (resolve) { resolve(fn.apply(undefined, args)); });
  }));

  // ---- Map, WeakMap upsert
  [Map, WeakMap].forEach(function (M) {
    def(M.prototype, "getOrInsert", function getOrInsert(key, value) {
      if (key === 0) key = 0;
      if (!this.has(key)) this.set(key, value);
      return this.get(key);
    });
    def(M.prototype, "getOrInsertComputed", function getOrInsertComputed(key, cb) {
      callable(cb);
      if (key === 0) key = 0;
      if (this.has(key)) return this.get(key);
      var v = cb(key);
      this.set(key, v);
      return v;
    });
  });

  // ---- Set methods
  function setRecord(o) {
    if (!isObject(o)) throw new TypeError("The argument must be an object");
    var size = +o.size;
    if (size !== size) throw new TypeError("The 'size' property must be a number");
    size = toInteger(size);
    if (size < 0) throw new RangeError("'" + size + "' is an invalid size");
    return { o: o, size: size, has: callable(o.has, "has"), keys: callable(o.keys, "keys") };
  }
  function eachKey(rec, fn) {
    var it = rec.keys.call(rec.o), next = it.next;
    for (;;) {
      var r = next.call(it);
      if (r.done) return;
      if (fn(r.value) === true) { closeIterator(it); return; }
    }
  }
  function thisSet(s) {
    if (!(s instanceof Set)) throw new TypeError("Method Set.prototype called on incompatible receiver");
    return s;
  }
  def(Set.prototype, "union", function union(other) {
    var r = setRecord(other), out = new Set(thisSet(this));
    eachKey(r, function (k) { out.add(k); });
    return out;
  });
  def(Set.prototype, "intersection", function intersection(other) {
    var s = thisSet(this), r = setRecord(other), out = new Set();
    if (s.size <= r.size) Array.from(s).forEach(function (k) { if (r.has.call(r.o, k)) out.add(k); });
    else eachKey(r, function (k) { if (s.has(k)) out.add(k); });
    return out;
  });
  def(Set.prototype, "difference", function difference(other) {
    var s = thisSet(this), r = setRecord(other), out = new Set(s);
    if (s.size <= r.size) Array.from(s).forEach(function (k) { if (r.has.call(r.o, k)) out.delete(k); });
    else eachKey(r, function (k) { out.delete(k); });
    return out;
  });
  def(Set.prototype, "symmetricDifference", function symmetricDifference(other) {
    var s = thisSet(this), r = setRecord(other), out = new Set(s);
    eachKey(r, function (k) { if (s.has(k)) out.delete(k); else out.add(k); });
    return out;
  });
  def(Set.prototype, "isSubsetOf", function isSubsetOf(other) {
    var s = thisSet(this), r = setRecord(other);
    if (s.size > r.size) return false;
    var vals = Array.from(s);
    for (var i = 0; i < vals.length; i++) if (!r.has.call(r.o, vals[i])) return false;
    return true;
  });
  def(Set.prototype, "isSupersetOf", function isSupersetOf(other) {
    var s = thisSet(this), r = setRecord(other), ok = true;
    if (s.size < r.size) return false;
    eachKey(r, function (k) { if (!s.has(k)) { ok = false; return true; } });
    return ok;
  });
  def(Set.prototype, "isDisjointFrom", function isDisjointFrom(other) {
    var s = thisSet(this), r = setRecord(other), ok = true;
    if (s.size <= r.size) {
      var vals = Array.from(s);
      for (var i = 0; i < vals.length; i++) if (r.has.call(r.o, vals[i])) return false;
    } else eachKey(r, function (k) { if (s.has(k)) { ok = false; return true; } });
    return ok;
  });

  // ---- Iterator helpers
  var IteratorProto = Object.getPrototypeOf(Object.getPrototypeOf([][Symbol.iterator]()));
  if (typeof g.Iterator === "undefined") {
    var Iterator = function Iterator() {
      if (new.target === undefined || new.target === Iterator) throw new TypeError("Abstract class Iterator not directly constructable");
    };
    Iterator.prototype = IteratorProto;
    put(IteratorProto, "constructor", Iterator);
    def(IteratorProto, Symbol.toStringTag, "Iterator");
    put(g, "Iterator", Iterator);
    var STATE = Symbol("iterator helper state");
    var HelperProto = Object.create(IteratorProto);
    put(HelperProto, Symbol.toStringTag, "Iterator Helper");
    put(HelperProto, "next", function next() {
      var s = this[STATE];
      if (!s) throw new TypeError("next method called on incompatible receiver");
      if (s.done) return { value: undefined, done: true };
      if (s.running) throw new TypeError("Generator is already running");
      s.running = true;
      var r;
      try { r = s.step(); } catch (e) { s.done = true; throw e; } finally { s.running = false; }
      if (r === undefined) { s.done = true; return { value: undefined, done: true }; }
      return { value: r[0], done: false };
    });
    put(HelperProto, "return", function () {
      var s = this[STATE];
      if (!s) throw new TypeError("return method called on incompatible receiver");
      if (!s.done) { s.done = true; closeIterator(s.it); if (s.inner) closeIterator(s.inner); }
      return { value: undefined, done: true };
    });
    // step() returns [value] or undefined when done.
    var helper = function (it, step) {
      var h = Object.create(HelperProto);
      Object.defineProperty(h, STATE, { value: { it: it, step: step, done: false, running: false, inner: null } });
      return h;
    };
    var direct = function (it) {
      if (!isObject(it)) throw new TypeError(String(it) + " is not an object");
      return { it: it, next: it.next };
    };
    var stepOf = function (d) {
      var r = d.next.call(d.it);
      if (!isObject(r)) throw new TypeError("Iterator result " + String(r) + " is not an object");
      return r;
    };
    var guarded = function (d, fn) {   // runs a callback, closing the iterator if it throws
      try { return fn(); } catch (e) { try { closeIterator(d.it); } catch (_) {} throw e; }
    };
    var positiveLimit = function (n) {
      n = +n;
      if (n !== n) throw new RangeError(String(n) + " must be positive");
      n = toInteger(n);
      if (n < 0) throw new RangeError(String(n) + " must be positive");
      return n;
    };
    put(IteratorProto, "map", function map(fn) {
      var d = direct(this); callable(fn);
      var i = 0;
      return helper(d.it, function () {
        var r = stepOf(d);
        if (r.done) return undefined;
        var v = r.value;
        return [guarded(d, function () { return fn(v, i++); })];
      });
    });
    put(IteratorProto, "filter", function filter(fn) {
      var d = direct(this); callable(fn);
      var i = 0;
      return helper(d.it, function () {
        for (;;) {
          var r = stepOf(d);
          if (r.done) return undefined;
          var v = r.value;
          if (guarded(d, function () { return fn(v, i++); })) return [v];
        }
      });
    });
    put(IteratorProto, "take", function take(limit) {
      var d = direct(this), left = positiveLimit(limit);
      return helper(d.it, function () {
        if (left === 0) { closeIterator(d.it); return undefined; }
        if (left !== Infinity) left--;
        var r = stepOf(d);
        return r.done ? undefined : [r.value];
      });
    });
    put(IteratorProto, "drop", function drop(limit) {
      var d = direct(this), left = positiveLimit(limit);
      return helper(d.it, function () {
        while (left > 0) {
          if (left !== Infinity) left--;
          if (stepOf(d).done) return undefined;
        }
        var r = stepOf(d);
        return r.done ? undefined : [r.value];
      });
    });
    put(IteratorProto, "flatMap", function flatMap(fn) {
      var d = direct(this); callable(fn);
      var i = 0, inner = null, h;
      h = helper(d.it, function () {
        for (;;) {
          if (inner) {
            var ir = stepOf(inner);
            if (!ir.done) return [ir.value];
            inner = null;
            h[STATE].inner = null;
          }
          var r = stepOf(d);
          if (r.done) return undefined;
          var v = r.value;
          var mapped = guarded(d, function () { return fn(v, i++); });
          inner = guarded(d, function () {
            if (!isObject(mapped)) throw new TypeError(String(mapped) + " is not an object");
            var m = mapped[Symbol.iterator];
            return direct(m == null ? mapped : m.call(mapped));
          });
          h[STATE].inner = inner.it;
        }
      });
      return h;
    });
    var eager = function (name, body) {
      put(IteratorProto, name, named(name, function (fn) {
        var d = direct(this);
        if (name !== "toArray" && name !== "reduce") callable(fn);
        return body(d, fn, arguments);
      }));
    };
    eager("reduce", function (d, fn, args) {
      callable(fn);
      var acc, i = 0, r;
      if (args.length < 2) {
        r = stepOf(d);
        if (r.done) throw new TypeError("Reduce of empty iterator with no initial value");
        acc = r.value;
        i = 1;
      } else acc = args[1];
      for (;;) {
        r = stepOf(d);
        if (r.done) return acc;
        var v = r.value, a = acc, k = i++;
        acc = guarded(d, function () { return fn(a, v, k); });
      }
    });
    eager("toArray", function (d) {
      var out = [];
      for (;;) { var r = stepOf(d); if (r.done) return out; out.push(r.value); }
    });
    eager("forEach", function (d, fn) {
      for (var i = 0; ; i++) { var r = stepOf(d); if (r.done) return undefined; var v = r.value, k = i; guarded(d, function () { fn(v, k); }); }
    });
    var search = function (name, onHit, onMiss, hit) {
      eager(name, function (d, fn) {
        for (var i = 0; ; i++) {
          var r = stepOf(d);
          if (r.done) return onMiss;
          var v = r.value, k = i;
          if (!!guarded(d, function () { return fn(v, k); }) === hit) { closeIterator(d.it); return onHit === "value" ? v : onHit; }
        }
      });
    };
    search("some", true, false, true);
    search("every", false, true, false);
    search("find", "value", undefined, true);
    var WrapProto = Object.create(IteratorProto);
    put(WrapProto, "next", function next() { return stepOf(this[STATE]); });
    put(WrapProto, "return", function () {
      var it = this[STATE].it, r = it["return"];
      return r == null ? { value: undefined, done: true } : r.call(it);
    });
    put(Iterator, "from", function from(o) {
      var it;
      if (typeof o === "string" || isObject(o)) {
        var m = o[Symbol.iterator];
        it = m == null ? o : m.call(o);
      } else throw new TypeError(String(o) + " is not an object");
      if (it instanceof Iterator) return it;
      var w = Object.create(WrapProto);
      Object.defineProperty(w, STATE, { value: direct(it) });
      return w;
    });
  }

  // ---- WeakRef, FinalizationRegistry (the engine collects garbage on its own schedule anyway)
  if (typeof g.WeakRef === "undefined") {
    var STATE_REF = Symbol("target");
    var WeakRef = function WeakRef(target) {
      if (new.target === undefined) throw new TypeError("Constructor WeakRef requires 'new'");
      if (!isObject(target)) throw new TypeError("WeakRef: invalid target");
      Object.defineProperty(this, STATE_REF, { value: target });
    };
    put(WeakRef.prototype, "deref", function deref() { return this[STATE_REF]; });
    def(WeakRef.prototype, Symbol.toStringTag, "WeakRef");
    put(g, "WeakRef", WeakRef);
    var FinalizationRegistry = function FinalizationRegistry(cleanup) {
      if (new.target === undefined) throw new TypeError("Constructor FinalizationRegistry requires 'new'");
      callable(cleanup);
    };
    put(FinalizationRegistry.prototype, "register", function register(target) {
      if (!isObject(target)) throw new TypeError("FinalizationRegistry.prototype.register: invalid target");
    });
    put(FinalizationRegistry.prototype, "unregister", function unregister(token) {
      if (!isObject(token)) throw new TypeError("Invalid unregisterToken ('" + String(token) + "')");
      return false;
    });
    def(FinalizationRegistry.prototype, Symbol.toStringTag, "FinalizationRegistry");
    put(g, "FinalizationRegistry", FinalizationRegistry);
  }

  // ---- JSON.stringify writes lone surrogates as \uXXXX escapes (well-formed JSON.stringify)
  var nativeStringify = JSON.stringify;
  put(JSON, "stringify", function stringify(value, replacer, space) {
    var out = nativeStringify.apply(JSON, arguments);
    if (typeof out !== "string") return out;
    var res = null, last = 0;
    for (var i = 0; i < out.length; i++) {
      var c = out.charCodeAt(i);
      if (c < 0xd800 || c > 0xdfff) continue;
      if (c <= 0xdbff) {
        var d = out.charCodeAt(i + 1);
        if (d >= 0xdc00 && d <= 0xdfff) { i++; continue; }
      }
      res = (res || "") + out.slice(last, i) + "\\u" + c.toString(16);
      last = i + 1;
    }
    return res === null ? out : res + out.slice(last);
  });

  // ---- symbols as WeakMap / WeakSet keys (kept by a strong map on the side)
  function weakKey(k, what) {
    if (typeof k === "symbol" && Symbol.keyFor(k) !== undefined) throw new TypeError("Invalid value used " + what);
    return typeof k === "symbol";
  }
  var SYMS = Symbol("symbol keys");
  function symbolsOf(o) {
    if (!hasOwn(o, SYMS)) Object.defineProperty(o, SYMS, { value: new Map() });
    return o[SYMS];
  }
  [[WeakMap, ["get", "set", "has", "delete"], "as weak map key"], [WeakSet, ["add", "has", "delete"], "in weak set"]].forEach(function (w) {
    w[1].forEach(function (name) {
      var native = w[0].prototype[name];
      put(w[0].prototype, name, named(name, function (k, v) {
        if (!weakKey(k, w[2])) {
          var r = native.apply(this, arguments);   // not a tail call: V4's tail calls mishandle arguments
          return r;
        }
        var m = symbolsOf(this);
        switch (name) {
        case "get": return m.get(k);
        case "set": m.set(k, v); return this;
        case "add": m.set(k, true); return this;
        case "has": return m.has(k);
        default: return m.delete(k);
        }
      }));
      Object.defineProperty(w[0].prototype[name], "length", { value: native.length });
    });
  });

  // ---- web/Node globals
  def(g, "queueMicrotask", function queueMicrotask(fn) {
    callable(fn, "The \"callback\" argument");
    Promise.resolve().then(function () { fn(); });
  });
  function cloneError(msg) { var e = new ErrorW(msg); put(e, "name", "DataCloneError"); return e; }
  def(g, "structuredClone", function structuredClone(value) {
    if (arguments.length === 0) throw new TypeError("The \"value\" argument must be specified");
    var memo = new Map();
    function clone(v) {
      if (typeof v === "symbol") throw cloneError(String(v) + " could not be cloned.");
      if (typeof v === "function") throw cloneError(String(v) + " could not be cloned.");
      if (!isObject(v)) return v;
      if (memo.has(v)) return memo.get(v);
      var tag = Object.prototype.toString.call(v).slice(8, -1), out, i, k;
      switch (tag) {
      case "Boolean": case "Number": case "String": out = Object(v.valueOf()); memo.set(v, out); return out;
      case "Date": out = new Date(v.getTime()); memo.set(v, out); return out;
      case "RegExp": out = new RegExpW(v.source, v.flags); memo.set(v, out); return out;
      case "ArrayBuffer": out = v.slice(0); memo.set(v, out); return out;
      case "DataView": out = new DataView(clone(v.buffer), v.byteOffset, v.byteLength); memo.set(v, out); return out;
      case "Map": out = new Map(); memo.set(v, out); v.forEach(function (x, key) { out.set(clone(key), clone(x)); }); return out;
      case "Set": out = new Set(); memo.set(v, out); v.forEach(function (x) { out.add(clone(x)); }); return out;
      case "Error":
        out = new (g[v.name] && g[v.name].prototype instanceof ErrorW ? g[v.name] : ErrorW)(v.message);
        memo.set(v, out);
        if ("stack" in v) put(out, "stack", String(v.stack));
        if (hasOwn(v, "cause")) put(out, "cause", clone(v.cause));
        return out;
      }
      if (ArrayBuffer.isView(v)) { out = new v.constructor(clone(v.buffer), v.byteOffset, v.length); memo.set(v, out); return out; }
      if (tag !== "Object" && tag !== "Array" && tag !== "Arguments") throw cloneError("#<" + tag + "> could not be cloned.");
      out = Array.isArray(v) ? new Array(v.length) : {};
      memo.set(v, out);
      var keys = Object.keys(v);
      for (i = 0; i < keys.length; i++) { k = keys[i]; defineData(out, k, clone(v[k])); }
      return out;
    }
    return clone(value);
  });
  if (typeof console !== "undefined") {
    ["table", "dir", "dirxml", "group", "groupCollapsed", "timeLog"].forEach(function (n) {
      def(console, n, named(n, function () { return console.log.apply(console, arguments); }));
    });
    ["groupEnd", "countReset", "clear"].forEach(function (n) { def(console, n, named(n, function () {})); });
  }
})(this);
