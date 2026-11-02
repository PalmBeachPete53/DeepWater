// Evaluated on every custom indicator engine after qtm-polyfills.js; compiled in as :/qtm/qtm-runtime.js.
// The helpers the rewritten code calls (JsTransform.h): object spread and rest, class fields and
// private names, async functions and generators, import.meta and import().
(function (g) {
  "use strict";
  var Q = {};
  Object.defineProperty(g, "__qtm", { value: Q, writable: false, enumerable: false, configurable: false });
  function isObject(v) { return (typeof v === "object" && v !== null) || typeof v === "function"; }
  function hidden(o, k, v) { Object.defineProperty(o, k, { value: v, writable: true, configurable: true, enumerable: false }); }
  function data(o, k, v) { Object.defineProperty(o, k, { value: v, writable: true, enumerable: true, configurable: true }); }
  var hasOwn = Function.prototype.call.bind(Object.prototype.hasOwnProperty);

  // ToPropertyKey
  Q.key = function (v) {
    if (isObject(v)) {
      var ex = v[Symbol.toPrimitive];
      if (ex != null) v = ex.call(v, "string");
      else {
        var t = v.toString, r = typeof t === "function" ? t.call(v) : v;
        if (isObject(r)) { t = v.valueOf; r = typeof t === "function" ? t.call(v) : r; }
        if (isObject(r)) throw new TypeError("Cannot convert object to primitive value");
        v = r;
      }
    }
    return typeof v === "symbol" ? v : String(v);
  };
  // RequireObjectCoercible of a destructured value, with V8's message.
  Q.coerce = function (v, firstKey) {
    if (v == null)
      throw new TypeError(firstKey === undefined ? "Cannot destructure '" + v + "' as it is " + v + "."
                                                 : "Cannot destructure property '" + firstKey + "' of '" + v + "' as it is " + v + ".");
    return v;
  };
  // CopyDataProperties(target, source, excluded)
  function copy(target, source, excluded) {
    if (source == null) return target;
    var from = Object(source), keys = Reflect.ownKeys(from);
    for (var i = 0; i < keys.length; i++) {
      var k = keys[i];
      if (excluded && excluded.indexOf(k) >= 0) continue;
      var d = Reflect.getOwnPropertyDescriptor(from, k);
      if (d && d.enumerable) data(target, k, from[k]);
    }
    return target;
  }
  // { a, ...rest } = source: the own enumerable properties not named before the rest.
  Q.rest = function (source, excluded) {
    Q.coerce(source);
    for (var i = 0; i < excluded.length; i++) excluded[i] = Q.key(excluded[i]);
    return copy({}, source, excluded);
  };
  // { ...source } in an object literal
  Q.spreadProps = function (target, source) { return copy(target, source, null); };
  // The literal's own part after a spread: copied with its exact descriptors (accessors stay accessors).
  Q.defineProps = function (target, part) {
    var keys = Reflect.ownKeys(part);
    for (var i = 0; i < keys.length; i++) Object.defineProperty(target, keys[i], Object.getOwnPropertyDescriptor(part, keys[i]));
    var proto = Object.getPrototypeOf(part);
    if (proto !== Object.prototype) Object.setPrototypeOf(target, proto);
    return target;
  };

  // { __proto__: v } (V4 makes it an own property): the rewritten literal has v under PROTO.
  Q.PROTO = Symbol("__proto__");
  Q.protoLiteral = function (o) {
    if (hasOwn(o, Q.PROTO)) {
      var v = o[Q.PROTO];
      delete o[Q.PROTO];
      if (isObject(v) || v === null) Object.setPrototypeOf(o, v);
    }
    return o;
  };

  // ---- classes
  // A private name is a symbol; reading or writing it on an object that lacks it hits these
  // accessors on Object.prototype and throws, as a failed private brand check does.
  var privateNames = new Map();
  Q.privateName = function (name) {
    var s = Symbol(name);
    privateNames.set(s, name);
    Object.defineProperty(Object.prototype, s, {
      get: function () { throw new TypeError("Cannot read private member " + name + " from an object whose class did not declare it"); },
      set: function () { throw new TypeError("Cannot write private member " + name + " to an object whose class did not declare it"); },
      configurable: true, enumerable: false
    });
    return s;
  };
  Q.privateIn = function (sym, o) {
    if (!isObject(o)) throw new TypeError("Cannot use 'in' operator to search for '" + privateNames.get(sym) + "' in " + String(o));
    for (var p = o; p !== null && p !== Object.prototype; p = Object.getPrototypeOf(p)) if (hasOwn(p, sym)) return true;
    return false;
  };
  var SPEC = Symbol("class fields");
  function defineField(o, key, value, name) {
    if (name) {
      if (hasOwn(o, key)) throw new TypeError("Cannot initialize " + name + " twice on the same object");
      Object.defineProperty(o, key, { value: value, writable: true, enumerable: false, configurable: false });
    } else {
      Object.defineProperty(o, key, { value: value, writable: true, enumerable: true, configurable: true });
    }
  }
  // Runs after a class with fields is created. desc: [kind, key, initializer method, private name]
  // in source order; kind 0 = instance field, 1 = static field, 2 = static block. A computed key is
  // a function returning the key evaluated when the class was defined.
  Q.defineClass = function (C, desc, name) {
    var inst = [], statics = [], i;
    for (i = 0; i < desc.length; i++) {
      var d = desc[i], key = typeof d[1] === "function" ? d[1]() : d[1];
      (d[0] === 0 ? inst : statics).push({ kind: d[0], key: key, fn: d[2], name: d[3] });
    }
    hidden(C, SPEC, inst);
    if (name !== undefined) Object.defineProperty(C, "name", { value: name, configurable: true });
    for (i = 0; i < statics.length; i++) {
      var s = statics[i], init = C[s.fn];
      delete C[s.fn];
      var v = init.call(C);
      if (s.kind === 1) defineField(C, s.key, v, s.name);
    }
    return C;
  };
  Q.initFields = function (o, C) {
    var fields = hasOwn(C, SPEC) ? C[SPEC] : [], proto = C.prototype;
    for (var i = 0; i < fields.length; i++) {
      var f = fields[i];
      defineField(o, f.key, proto[f.fn].call(o), f.name);
    }
    return o;
  };

  // ---- async functions: generators driven by promises
  Q.async = function (self, args, gen) {
    return new Promise(function (resolve, reject) {
      var it = gen.apply(self, args || []);
      function step(method, arg) {
        var r;
        try { r = it[method](arg); } catch (e) { reject(e); return; }
        if (r.done) { resolve(r.value); return; }
        Promise.resolve(r.value).then(function (v) { step("next", v); }, function (e) { step("throw", e); });
      }
      step("next", undefined);
    });
  };
  function Await(v) { this.v = v; }
  Q.AWAIT = function (v) { return new Await(v); };
  var AsyncIteratorProto = {};
  AsyncIteratorProto[Symbol.asyncIterator] = function () { return this; };
  var AsyncGenProto = Object.create(AsyncIteratorProto);
  hidden(AsyncGenProto, Symbol.toStringTag, "AsyncGenerator");
  // An async generator: requests queue up; each runs the generator until it yields a value
  // (awaited first) or returns, awaiting what it awaits on the way.
  Q.asyncGenerator = function (self, args, gen) {
    var it = gen.apply(self, args || []), queue = [], busy = false, finished = false;
    function settle(req, method, arg) {
      if (finished) {
        if (method === "throw") { req.reject(arg); return next(); }
        return Promise.resolve(method === "return" ? arg : undefined).then(function (v) { req.resolve({ value: v, done: true }); next(); }, function (e) { req.reject(e); next(); });
      }
      var r;
      try { r = it[method](arg); } catch (e) { finished = true; req.reject(e); return next(); }
      if (r.value instanceof Await) {
        Promise.resolve(r.value.v).then(function (v) { settle(req, "next", v); }, function (e) { settle(req, "throw", e); });
        return;
      }
      if (r.done) finished = true;
      Promise.resolve(r.value).then(function (v) {
        if (!r.done && method !== "next" && false) return;
        req.resolve({ value: v, done: r.done });
        next();
      }, function (e) {
        if (r.done) { req.reject(e); next(); } else settle(req, "throw", e);
      });
    }
    function next() {
      busy = false;
      if (queue.length) { busy = true; var q = queue.shift(); settle(q, q.method, q.arg); }
    }
    function request(method) {
      return function (arg) {
        return new Promise(function (resolve, reject) {
          queue.push({ method: method, arg: arg, resolve: resolve, reject: reject });
          if (!busy) next();
        });
      };
    }
    var o = Object.create(AsyncGenProto);
    hidden(o, "next", request("next"));
    hidden(o, "return", request("return"));
    hidden(o, "throw", request("throw"));
    return o;
  };
  // GetIterator(obj, async): the async iterator, or the sync one with its values awaited.
  Q.asyncIterator = function (obj) {
    var m = obj == null ? undefined : obj[Symbol.asyncIterator];
    if (m != null) return m.call(obj);
    var sm = obj == null ? undefined : obj[Symbol.iterator];
    if (typeof sm !== "function") throw new TypeError(String(obj) + " is not async iterable");
    var it = sm.call(obj), o = Object.create(AsyncIteratorProto);
    function wrap(r) { return Promise.resolve(r.value).then(function (v) { return { value: v, done: !!r.done }; }); }
    hidden(o, "next", function (v) { try { return wrap(it.next(v)); } catch (e) { return Promise.reject(e); } });
    hidden(o, "return", function (v) {
      var f = it["return"];
      if (f == null) return Promise.resolve({ value: v, done: true });
      try { return wrap(f.call(it, v)); } catch (e) { return Promise.reject(e); }
    });
    return o;
  };

  // ---- modules
  function resolveUrl(base, spec) {
    spec = String(spec);
    if (/^[a-z][a-z0-9+.-]*:/i.test(spec)) return spec;
    var path = spec.charAt(0) === "/" ? spec : base.replace(/^file:\/\//, "").replace(/[^\/]*$/, "") + spec, parts = [];
    path.split("/").forEach(function (p) {
      if (p === "..") parts.pop();
      else if (p !== "." && p !== "") parts.push(p);
    });
    return "file:///" + parts.join("/");
  }
  Q.importMeta = function (url) {
    var m = Object.create(null), path = decodeURIComponent(url.replace(/^file:\/\//, ""));
    m.dirname = path.replace(/\/[^\/]*$/, "") || "/";
    m.filename = path;
    m.resolve = function resolve(spec) { return resolveUrl(url, spec); };
    m.url = url;
    return m;
  };
  // import(spec): the host (CustomIndicators.cpp) imports the file, rewritten like the others.
  Q.importDynamic = function (base, spec) {
    return new Promise(function (resolve, reject) {
      var r = Q.host ? Q.host.importModule(String(spec), base) : { ok: false, error: new Error("Cannot find module '" + spec + "'") };
      if (r.ok) resolve(r.value); else reject(r.error);
    });
  };
  Q.regex = function (pattern, flags) { return new RegExp(pattern, flags); };
  // __qtm.v(x = f()): see TailPass in JsTransform.cpp.
  Q.v = function (value) { return value; };
})(this);
