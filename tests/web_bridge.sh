#!/bin/sh
set -eu

# The browser bridge (std/web.zi with web/ziran_web.js) drives real JS
# objects from WebAssembly: properties, methods, construction, callbacks,
# promises, typed arrays, and errors. Needs emcc and node; skips without them.
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
bin=$(CDPATH= cd -- "$(dirname -- "$ziran")" && pwd)
emcc=${EMCC:-$HOME/emsdk/upstream/emscripten/emcc}
if [ ! -x "$emcc" ] || ! command -v node >/dev/null 2>&1; then
    echo "web bridge test skipped: emcc or node is unavailable"
    exit 0
fi
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
cache=${EM_CACHE:-$repo/build/emscripten-cache}

cat > "$work/app.zi" <<'ZI'
#import "web"

sum: s32;
seen_index: s32;

ArrayChecks :: () -> s32 {
    push: s32 = WebName("push")
    length: s32 = WebName("length")
    if WebName("push") != push || WebName("length") == push { return 1 }
    list: s32 = WebNewArray()
    unused WebCall1(list, push, WebNumber(3.0))
    unused WebCall1(list, push, WebNumber(4.5))
    if WebGetNumber(list, length) != 2.0 { return 2 }
    if WebAsNumber(WebAt(list, 1)) != 4.5 { return 3 }
    WebSetAt(list, 0, WebNumber(9.0))
    if WebAsNumber(WebAt(list, 0)) != 9.0 { return 4 }
    if WebTypeOf(list) != WebTypeObject { return 5 }
    array_class: s32 = WebGetObject(WebGlobal, WebName("Array"))
    if !WebInstanceOf(list, array_class) { return 6 }
    WebRelease(list)
    return 0
}

ObjectChecks :: () -> s32 {
    thing: s32 = WebNewObject()
    WebSet(thing, WebName("a"), WebNumber(1.0))
    WebSet(thing, WebName("ok"), WebBool(true))
    WebSet(thing, WebName("label"), WebText("héllo"))
    json: s32 = WebGetObject(WebGlobal, WebName("JSON"))
    text: WebValue = WebCall1(json, WebName("stringify"), WebObject(thing))
    handle: s32 = WebAsObject(text)
    if text.tag != WebStringTag || handle == 0 { return 1 }
    buffer: [64]u8
    length: s32 = WebCopyString(handle, buffer[:])
    if length != WebStringLength(handle) { return 2 }
    expected: string = "{\"a\":1,\"ok\":true,\"label\":\"héllo\"}"
    if length != cast(s32)expected.count { return 3 }
    index: s32 = 0
    while index < length {
        if buffer[index] != expected[index] { return 4 }
        index += 1
    }
    if WebTypeOf(handle) != WebTypeString { return 5 }
    // Reading a property and a boolean back.
    if !WebAsBool(WebGet(thing, WebName("ok"))) { return 6 }
    if !WebIsUndefined(WebGet(thing, WebName("missing"))) { return 7 }
    // A kept handle refers to the same object.
    kept: s32 = WebKeep(thing)
    if kept == thing { return 8 }
    WebSet(kept, WebName("a"), WebNumber(7.0))
    if WebGetNumber(thing, WebName("a")) != 7.0 { return 9 }
    WebRelease(kept)
    WebRelease(handle)
    WebRelease(thing)
    return 0
}

CallbackChecks :: () -> s32 {
    list: s32 = WebNewArray()
    push: s32 = WebName("push")
    unused WebCall1(list, push, WebNumber(10.0))
    unused WebCall1(list, push, WebNumber(20.0))
    unused WebCall1(list, push, WebNumber(30.0))
    sum = 0
    seen_index = -1
    function: s32 = WebFunction(Visit, 5)
    unused WebCall1(list, WebName("forEach"), WebObject(function))
    // 10 + 20 + 30, each scaled by the context 5.
    if sum != 300 { return 1 }
    if seen_index != 2 { return 2 }
    WebRelease(function)
    WebRelease(list)
    return 0
}

Visit :: (context: s32, value: s32, index: s32, array: s32) {
    unused array
    seen_index = cast(s32)WebAsNumber(WebValueOf(index))
    // The element arrives as a number handle valid during the call.
    if WebTypeOf(value) == WebTypeNumber {
        sum += context * cast(s32)WebAsNumber(WebValueOf(value))
    }
}

PromiseChecks :: () -> s32 {
    promise: s32 = WebGetObject(WebGlobal, WebName("Promise"))
    resolved: WebValue = WebCall1(promise, WebName("resolve"), WebNumber(41.0))
    result: WebValue
    if !WebAwait(WebAsObject(resolved), *result) { return 1 }
    if WebAsNumber(result) != 41.0 { return 2 }
    rejected: WebValue = WebCall1(promise, WebName("reject"), WebText("no"))
    if WebAwait(WebAsObject(rejected), *result) { return 3 }
    if !WebFailed() { return 4 }
    if WebLastError() == 0 { return 5 }
    WebSleep(2)
    WebNextFrame(0.0)
    return 0
}

ErrorChecks :: () -> s32 {
    thing: s32 = WebNewObject()
    unused WebCall0(thing, WebName("notAMethod"))
    if !WebFailed() { return 1 }
    // A later good call clears the failure.
    unused WebCall0(thing, WebName("toString"))
    if WebFailed() { return 2 }
    WebRelease(thing)
    return 0
}

BytesChecks :: () -> s32 {
    samples: [2]float32
    samples[0] = 1.5
    samples[1] = 2.5
    view: s32 = WebBytes(WebBytesFloat32, cast(*void)*samples[0], 2)
    if view == 0 { return 1 }
    if WebGetNumber(view, WebName("length")) != 2.0 { return 2 }
    if WebAsNumber(WebAt(view, 1)) != 2.5 { return 3 }
    // The array is a copy: changing the source leaves it alone.
    samples[1] = 9.0
    if WebAsNumber(WebAt(view, 1)) != 2.5 { return 4 }
    back: [2]float32
    if WebCopyOut(view, cast(*void)*back[0], 8) != 8 { return 5 }
    if back[0] != 1.5 || back[1] != 2.5 { return 6 }
    // Writing into a typed array from memory.
    samples[0] = 0.25
    samples[1] = 0.75
    if WebCopyIn(view, cast(*void)*samples[0], 8) != 8 { return 7 }
    if WebAsNumber(WebAt(view, 0)) != 0.25 { return 8 }
    ctor: s32 = WebGetObject(WebGlobal, WebName("Uint8Array"))
    bytes: s32 = WebNew1(ctor, WebNumber(4.0))
    if WebGetNumber(bytes, WebName("length")) != 4.0 { return 9 }
    if !WebInstanceOf(bytes, ctor) { return 10 }
    WebRelease(bytes)
    WebRelease(view)
    return 0
}

ScopeChecks :: () -> s32 {
    json: s32 = WebGetObject(WebGlobal, WebName("JSON"))
    kept: s32 = WebKeep(json)
    baseline: s32 = WebLiveHandles()
    // A scope frees every temporary handle made inside it.
    round: s32 = 0
    while round < 1000 {
        mark: s32 = WebScope()
        list: s32 = WebNewArray()
        unused WebCall1(list, WebName("push"), WebNumber(1.0))
        unused WebGetObject(WebGlobal, WebName("Math"))
        WebEnd(mark)
        round += 1
    }
    if WebLiveHandles() != baseline { return 1 }
    // A kept handle survives the end of the scope that made it.
    mark: s32 = WebScope()
    made: s32 = WebKeep(WebNewObject())
    WebEnd(mark)
    if WebLiveHandles() != baseline + 1 { return 2 }
    WebSet(made, WebName("a"), WebNumber(3.0))
    if WebGetNumber(made, WebName("a")) != 3.0 { return 3 }
    WebRelease(made)
    WebRelease(kept)
    if WebLiveHandles() != baseline - 1 { return 4 }
    return 0
}

#program_export
Answer :: () -> s32 {
    failed := ArrayChecks()
    if failed != 0 { return 100 + failed }
    failed = ObjectChecks()
    if failed != 0 { return 200 + failed }
    failed = CallbackChecks()
    if failed != 0 { return 300 + failed }
    failed = PromiseChecks()
    if failed != 0 { return 400 + failed }
    failed = ErrorChecks()
    if failed != 0 { return 500 + failed }
    failed = BytesChecks()
    if failed != 0 { return 600 + failed }
    failed = ScopeChecks()
    if failed != 0 { return 700 + failed }
    return 42
}
ZI
cat > "$work/post.js" <<'JS'
Module.onRuntimeInitialized = async function () {
  var result = await Module.ccall('Answer', 'number', [], [], {async: true});
  console.log(result === 42 ? 'PASS' : 'FAIL ' + result);
  process.exit(result === 42 ? 0 : 1);
};
JS
"$bin/zi2c" --no-main --root "$work" --module-path "$repo/std" \
    -o "$work/c" "$work/app.zi"
EM_CACHE=$cache "$emcc" -O1 -I"$repo/include" -iquote "$work/c" \
    "$work"/c/*.c --js-library "$repo/web/ziran_web.js" \
    --post-js "$work/post.js" -sASYNCIFY -sEXPORTED_FUNCTIONS=_Answer \
    -sEXPORTED_RUNTIME_METHODS=ccall -sENVIRONMENT=node \
    -o "$work/app.js" 2>&1 | grep -v 'warning' || true
output=$(node "$work/app.js" 2>&1 || true)
case "$output" in
*PASS*) echo "Ziran web bridge passed" ;;
*) echo "$output" >&2; exit 1 ;;
esac
