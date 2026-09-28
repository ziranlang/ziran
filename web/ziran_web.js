// The Ziran browser bridge: a generic, program-independent way for Ziran code
// compiled to WebAssembly to use Web APIs. It knows nothing about any
// library. Objects live in a handle table, names are interned once, values
// cross as small records in wasm memory, and Ziran functions become JS
// callbacks. Link it with `emcc --js-library ziran_web.js`; std/web.zi is the
// Ziran side. Handles 1 and 2 are the global object and Emscripten's Module.
addToLibrary({
  $ZiranWeb__deps: ['$getWasmTableEntry', '$UTF8ToString', '$stringToUTF8',
                    '$lengthBytesUTF8'],
  $ZiranWeb: function () {
    var W = globalThis.__ziranWeb;
    if (W) return W;
    W = globalThis.__ziranWeb = {
      handles: [undefined, globalThis, Module], free: [],
      names: [""], ids: new Map(), error: 0
    };
    // Value record: s32 tag, s32 aux, f64 number.
    // Tags: 0 undefined, 1 null, 2 bool, 3 number, 4 string (in: pointer in
    // number, byte length in aux), 5 object handle, 6 string handle (out).
    W.put = function (value) {
      if (value === undefined || value === null) return 0;
      var handle = W.free.length ? W.free.pop() : W.handles.length;
      W.handles[handle] = value;
      return handle;
    };
    W.drop = function (handle) {
      if (handle > 2 && handle < W.handles.length) {
        W.handles[handle] = undefined;
        W.free.push(handle);
      }
    };
    W.read = function (record) {
      var tag = HEAP32[record >> 2], aux = HEAP32[(record >> 2) + 1];
      var number = HEAPF64[(record >> 3) + 1];
      switch (tag) {
      case 0: return undefined;
      case 1: return null;
      case 2: return number !== 0;
      case 3: return number;
      case 4: return UTF8ToString(number, aux);
      default: return W.handles[aux];
      }
    };
    W.write = function (record, value) {
      var tag = 5, aux = 0, number = 0;
      if (value === undefined) tag = 0;
      else if (value === null) tag = 1;
      else if (typeof value === 'boolean') { tag = 2; number = value ? 1 : 0; }
      else if (typeof value === 'number') { tag = 3; number = value; }
      else if (typeof value === 'string') { tag = 6; aux = W.put(value); }
      else aux = W.put(value);
      HEAP32[record >> 2] = tag;
      HEAP32[(record >> 2) + 1] = aux;
      HEAPF64[(record >> 3) + 1] = number;
    };
    W.args = function (records, count) {
      var values = [];
      for (var i = 0; i < count; i++) values.push(W.read(records + i * 16));
      return values;
    };
    W.fail = function (error) {
      if (W.error) W.drop(W.error);
      W.error = W.put(error);
      return 1;
    };
    return W;
  },

  js_web_name__deps: ['$ZiranWeb'],
  js_web_name: function(text, length) {
    var W = ZiranWeb(), name = UTF8ToString(text, length);
    var id = W.ids.get(name);
    if (!id) { id = W.names.length; W.names.push(name); W.ids.set(name, id); }
    return id;
  },
  js_web_release__deps: ['$ZiranWeb'],
  js_web_release: function(handle) { ZiranWeb().drop(handle); },
  js_web_keep__deps: ['$ZiranWeb'],
  js_web_keep: function(handle) { var W = ZiranWeb(); return W.put(W.handles[handle]); },
  js_web_error__deps: ['$ZiranWeb'],
  js_web_error: function() { return ZiranWeb().error; },
  js_web_get__deps: ['$ZiranWeb'],
  js_web_get: function(object, name, result) {
    var W = ZiranWeb();
    try { W.write(result, W.handles[object][W.names[name]]); return 0; }
    catch (e) { W.write(result, undefined); return W.fail(e); }
  },
  js_web_set__deps: ['$ZiranWeb'],
  js_web_set: function(object, name, value) {
    var W = ZiranWeb();
    try { W.handles[object][W.names[name]] = W.read(value); return 0; }
    catch (e) { return W.fail(e); }
  },
  js_web_at__deps: ['$ZiranWeb'],
  js_web_at: function(object, index, result) {
    var W = ZiranWeb();
    try { W.write(result, W.handles[object][index]); return 0; }
    catch (e) { W.write(result, undefined); return W.fail(e); }
  },
  js_web_set_at__deps: ['$ZiranWeb'],
  js_web_set_at: function(object, index, value) {
    var W = ZiranWeb();
    try { W.handles[object][index] = W.read(value); return 0; }
    catch (e) { return W.fail(e); }
  },
  js_web_call__deps: ['$ZiranWeb'],
  js_web_call: function(object, name, args, count, result) {
    var W = ZiranWeb();
    try {
      var target = W.handles[object];
      W.write(result, target[W.names[name]].apply(target, W.args(args, count)));
      return 0;
    } catch (e) { W.write(result, undefined); return W.fail(e); }
  },
  js_web_new__deps: ['$ZiranWeb'],
  js_web_new: function(constructor, args, count, result) {
    var W = ZiranWeb();
    try {
      var C = W.handles[constructor];
      W.write(result, Reflect.construct(C, W.args(args, count)));
      return 0;
    } catch (e) { W.write(result, undefined); return W.fail(e); }
  },
  // The value a handle holds, as a value record: numbers, booleans and null
  // come back directly; objects and strings as handles again.
  js_web_value__deps: ['$ZiranWeb'],
  js_web_value: function(handle, result) {
    var W = ZiranWeb();
    W.write(result, W.handles[handle]);
  },
  js_web_object__deps: ['$ZiranWeb'],
  js_web_object: function() { return ZiranWeb().put({}); },
  js_web_array__deps: ['$ZiranWeb'],
  js_web_array: function() { return ZiranWeb().put([]); },
  js_web_type__deps: ['$ZiranWeb'],
  js_web_type: function(handle) {
    var v = ZiranWeb().handles[handle];
    if (v === undefined) return 0;
    if (v === null) return 1;
    switch (typeof v) {
    case 'boolean': return 2;
    case 'number': return 3;
    case 'string': return 4;
    case 'function': return 6;
    }
    return 5;
  },
  js_web_instance__deps: ['$ZiranWeb'],
  js_web_instance: function(handle, constructor) {
    var W = ZiranWeb();
    try { return W.handles[handle] instanceof W.handles[constructor] ? 1 : 0; }
    catch (e) { return 0; }
  },

  // Wraps a Ziran function as a JS function. Its first three arguments arrive
  // as handles, released when it returns; js_web_keep retains one.
  js_web_callback__deps: ['$ZiranWeb', '$getWasmTableEntry'],
  js_web_callback: function(callback, context) {
    var W = ZiranWeb();
    return W.put(function () {
      var handles = [0, 0, 0];
      for (var i = 0; i < 3 && i < arguments.length; i++)
        handles[i] = W.put(arguments[i]);
      try {
        getWasmTableEntry(callback)(context, handles[0], handles[1], handles[2]);
      } catch (e) {
        W.fail(e);
      } finally {
        handles.forEach(function (h) { W.drop(h); });
      }
    });
  },

  js_web_string_length__deps: ['$ZiranWeb', '$lengthBytesUTF8'],
  js_web_string_length: function(handle) {
    var v = ZiranWeb().handles[handle];
    return typeof v === 'string' ? lengthBytesUTF8(v) : 0;
  },
  js_web_string_copy__deps: ['$ZiranWeb', '$stringToUTF8', '$lengthBytesUTF8'],
  js_web_string_copy: function(handle, buffer, capacity) {
    var v = ZiranWeb().handles[handle];
    if (typeof v !== 'string' || capacity <= 0) return 0;
    var length = Math.min(lengthBytesUTF8(v), capacity - 1);
    stringToUTF8(v, buffer, capacity);
    return length;
  },

  // Typed arrays. Kinds: 0 Uint8, 1 Uint8Clamped, 2 Int16, 3 Int32, 4 Float32.
  js_web_bytes__deps: ['$ZiranWeb'],
  js_web_bytes: function(kind, data, count) {
    var Kinds = [Uint8Array, Uint8ClampedArray, Int16Array, Int32Array, Float32Array];
    var C = Kinds[kind];
    if (!C || count < 0) return 0;
    var bytes = count * C.BYTES_PER_ELEMENT;
    return ZiranWeb().put(new C(HEAPU8.buffer.slice(data, data + bytes)));
  },
  js_web_copy_out__deps: ['$ZiranWeb'],
  js_web_copy_out: function(handle, data, capacity) {
    var v = ZiranWeb().handles[handle];
    if (!v || (!v.buffer && !(v instanceof ArrayBuffer))) return 0;
    var bytes = v.buffer ? new Uint8Array(v.buffer, v.byteOffset, v.byteLength)
                         : new Uint8Array(v);
    var length = Math.min(bytes.length, capacity);
    HEAPU8.set(bytes.subarray(0, length), data);
    return length;
  },
  js_web_copy_in__deps: ['$ZiranWeb'],
  js_web_copy_in: function(handle, data, length) {
    var v = ZiranWeb().handles[handle];
    if (!v || !v.buffer) return 0;
    var bytes = new Uint8Array(v.buffer, v.byteOffset, v.byteLength);
    var count = Math.min(bytes.length, length);
    bytes.set(HEAPU8.subarray(data, data + count));
    return count;
  },

  // Waiting. Each suspends the Ziran call stack until the browser is ready.
  js_web_await__deps: ['$ZiranWeb', '$Asyncify'],
  js_web_await__async: true,
  js_web_await: function(promise, result) {
    return Asyncify.handleAsync(async function () {
      var W = ZiranWeb();
      try { W.write(result, await W.handles[promise]); return 1; }
      catch (e) { W.write(result, undefined); W.fail(e); return 0; }
    });
  },
  js_web_sleep__deps: ['$Asyncify'],
  js_web_sleep__async: true,
  js_web_sleep: function(milliseconds) {
    return Asyncify.handleAsync(async function () {
      await new Promise(function (resolve) {
        setTimeout(resolve, Math.max(0, milliseconds));
      });
    });
  },
  js_web_next_frame__deps: ['$Asyncify'],
  js_web_next_frame__async: true,
  js_web_next_frame: function(minimum_delay) {
    return Asyncify.handleAsync(async function () {
      if (typeof requestAnimationFrame === 'function') {
        var earliest = performance.now() + Math.max(0, minimum_delay);
        await new Promise(function (resolve) {
          function step(time) {
            if (time + 0.25 >= earliest) resolve();
            else requestAnimationFrame(step);
          }
          requestAnimationFrame(step);
        });
        return;
      }
      await new Promise(function (resolve) {
        setTimeout(resolve, Math.max(1, minimum_delay | 0));
      });
    });
  },
});
