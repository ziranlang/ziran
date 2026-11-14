#!/bin/sh
# check-cost: 120
set -eu
# Wasm modules from verified revision-28 ZX (zi2asm --emit wasm) must agree
# with the instruction engine. Node or the native Wasmtime CLI runs the
# emitted modules; ZIRAN_WASM_ENGINE=wasmtime selects Wasmtime for every check.
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build=$(CDPATH= cd -- "$(dirname -- "$ziran")/.." && pwd)
work=$build/tests/zx-wasm
mkdir -p "$work"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
source=$repo/tests/spec/zx_wasm_test.zi
entry=zx_wasm_test:Answer
expected=9091
nodebin=${ZIRAN_WASM_NODE:-node}

wasmtime=${ZIRAN_WASMTIME:-wasmtime}
native_engine=${ZIRAN_WASM_ENGINE:-node}
wasmtime_available=0
if command -v "$wasmtime" >/dev/null 2>&1; then wasmtime_available=1; fi
if test "$native_engine" != node && test "$native_engine" != wasmtime; then
    echo 'zx-wasm-test: ZIRAN_WASM_ENGINE must be node or wasmtime' >&2
    exit 1
fi
if test "$native_engine" = wasmtime && test "$wasmtime_available" != 1; then
    echo "zx-wasm-test: native Wasmtime is unavailable: $wasmtime" >&2
    exit 1
fi
if ! command -v "$nodebin" >/dev/null 2>&1; then
    echo 'zx-wasm-test: needs node for host-output interoperability' >&2
    test "${WASM_REQUIRED:-0}" != 1
    exit
fi

# Both engines use the same modules and budget trap contract.
run_engine() {
    mode=$1; shift
    if test "$native_engine" = wasmtime; then
        "$ziran" run --target=c --root "$repo/tests" "$repo/tests/wasmtime_engine.zi" -- "$ziran" "$work" "$wasmtime" "$mode" "$@"
    else
        case $mode in
            scalar) "$nodebin" "$run_wasm" "$@" ;;
            budget) "$nodebin" "$work/spend_budget.js" "$@" ;;
            simd) "$nodebin" "$work/run_simd.js" "$@" ;;
        esac
    fi
}

zi2asm=$(dirname -- "$ziran")/zi2asm
test -x "$zi2asm" || {
    echo "zx-wasm-test: $zi2asm is not built; run make first"
    exit 1
}

"$zi2asm" --root "$repo/tests/spec" --entry "$entry" --emit wasm \
    -o "$work/zx_wasm_test.wasm" "$source"

run_wasm="$work/run_wasm.js"
cat >"$run_wasm" <<'EOF'
// Instantiates one wasm module, wires the budget global and the
// overrun import, and prints zx_entry's i64 result.
const fs = require('fs');
const file = process.argv[2];
const budget = BigInt(process.argv[3]);
WebAssembly.instantiate(fs.readFileSync(file), {
  zx: {
    overrun: () => { throw new Error('runaway: budget spent'); },
    budget: new WebAssembly.Global({value: 'i64', mutable: true}, budget),
  },
}).then(({instance}) => {
    console.log(instance.exports.zx_entry().toString());
    process.exit(0);
}).catch((e) => { console.error(e.message); process.exit(1); });
EOF

result=$(run_engine scalar "$work/zx_wasm_test.wasm" 10000000)
test "$result" = "$expected" || {
    echo "zx-wasm-test: wasm result $result != $expected"
    exit 1
}

# Builtin output preserves percent escapes, NUL bytes and void procedure calls.
cat >"$work/print.zi" <<'ZI'
Touch :: () {}
main :: () {
    Touch()
    print("Hello, %% Wasm!\n")
    print("")
    print("a\0b\n")
}
ZI
"$zi2asm" --root "$work" --entry print:main --emit wasm -o "$work/print.wasm" "$work/print.zi"
"$ziran" bundle --root "$work" --entry print:main -o "$work/print.zx" "$work/print.zi"
"$ziran" run "$work/print.zx" > "$work/print-vm.txt"
"$nodebin" - "$work/print.wasm" > "$work/print-wasm.txt" <<'JS'
const fs = require('fs');
let memory;
const imports = { zx: {
    budget: new WebAssembly.Global({value: 'i64', mutable: true}, 1000000n),
    overrun() { throw Error('budget spent'); },
    host_i64(index, value) {
        if ((index >>> 0) !== 0xffffffff) throw Error('unexpected host capability');
        const address = Number(value & 0xffffffffn), size = Number(value >> 32n);
        process.stdout.write(Buffer.from(memory.buffer, address, size));
        return 0n;
    },
}};
WebAssembly.instantiate(fs.readFileSync(process.argv[2]), imports).then(({instance}) => {
    memory = instance.exports.memory;
    instance.exports.zx_entry();
}).catch(error => { console.error(error); process.exitCode = 1; });
JS
cmp "$work/print-vm.txt" "$work/print-wasm.txt"

# Revision-28 ZX lowers to wasm with the interpreter's typed host ABI:
# canonical bits for scalars of any arity and void results, host images for
# strings, records and slices, result images the module keeps, and changed
# slice elements copied back. The same ZX gives the same module bytes.
cat >"$work/typed.zi" <<'ZI'
host_api :: #system_library "host_api";
Item :: struct {
    title: string
    weight: s32
    on: bool
}
Greet :: (name: string, times: s32) -> string #foreign host_api;
Weigh :: (item: Item) -> Item #foreign host_api;
Fill :: (values: []u8) #foreign host_api;
Flag :: (value: bool, scale: float64) -> u16 #foreign host_api;
Note :: (a: s64, b: u8, c: s16) #foreign host_api;
Count :: () -> s64 #foreign host_api;
main :: () -> s32 {
    text := Greet("zx", 2)
    item := Weigh(Item.{title = "box", weight = 3, on = true})
    buffer: [4]u8
    Fill(buffer[1:3])
    Note(-5, 200, -300)
    print("% % %\n", text, item.title, item.on)
    return cast(s32)Flag(item.on, 2.5) + item.weight + cast(s32)buffer[2] + cast(s32)buffer[3] + cast(s32)Count()
}
ZI
"$zi2asm" --root "$work" -I "$repo/std" --entry typed:main --emit zx -o "$work/typed.zx" "$work/typed.zi"
"$zi2asm" --input zx --emit wasm -o "$work/typed.wasm" "$work/typed.zx"
"$zi2asm" --input zx --emit wasm -o "$work/typed-again.wasm" "$work/typed.zx"
cmp "$work/typed.wasm" "$work/typed-again.wasm"
cat >"$work/typed.js" <<'JS'
// A JavaScript host for typed.zi, reading and writing host images.
const fs = require('fs');
const bytes = fs.readFileSync(process.argv[2]);
const section = new Uint8Array(WebAssembly.Module.customSections(new WebAssembly.Module(bytes), 'zxhost')[0]);
let at = 0;
const uleb = () => { let value = 0, shift = 0, byte; do { byte = section[at++]; value |= (byte & 127) << shift; shift += 7; } while (byte & 128); return value; };
const name = () => { const size = uleb(); const text = Buffer.from(section.slice(at, at + size)).toString(); at += size; return text; };
const capabilities = [];
for (let count = uleb(); count > 0; count--) capabilities.push({module: name(), name: name(), count: uleb()});
let wasm, notes = [];
const view = () => new DataView(wasm.memory.buffer);
const elements = (image) => { const v = view(); return new Uint8Array(wasm.memory.buffer, image + Number(v.getBigUint64(image, true)), Number(v.getBigUint64(image + 8, true))); };
const place = (record, image) => {
    const address = wasm.zx_reserve(image.length);
    if (address === 0) throw Error('zx_reserve failed');
    new Uint8Array(wasm.memory.buffer, address, image.length).set(image);
    view().setUint32(record, address, true); view().setUint32(record + 8, image.length, true);
};
const text = (value, prefix) => {
    const bytes = Buffer.from(value), image = new Uint8Array(prefix + bytes.length), v = new DataView(image.buffer);
    if (bytes.length) v.setBigUint64(0, BigInt(prefix), true);
    v.setBigUint64(8, BigInt(bytes.length), true); image.set(bytes, prefix);
    return image;
};
const calls = {
    Greet(record) {
        const image = view().getUint32(record, true), times = view().getInt32(record + 16, true);
        place(record + 32, text(('hi ' + Buffer.from(elements(image)).toString() + ' ').repeat(times).trim(), 16));
    },
    Weigh(record) {
        const image = view().getUint32(record, true), v = view();
        const result = text(Buffer.from(elements(image)).toString() + '!', 21), out = new DataView(result.buffer);
        out.setInt32(16, v.getInt32(image + 16, true) * 10, true); out.setUint8(20, v.getUint8(image + 20) ? 0 : 1);
        place(record + 16, result);
    },
    Fill(record) {
        const values = elements(view().getUint32(record, true));
        for (let index = 0; index < values.length; index++) values[index] = 40 + index;
        view().setUint32(record + 12, 1, true);
    },
    Flag(record) {
        const v = view();
        v.setBigUint64(record + 32, BigInt(Math.round(Number(v.getBigUint64(record, true)) * v.getFloat64(record + 16, true) * 100)), true);
    },
    Note(record) {
        const v = view();
        notes.push(BigInt.asIntN(64, v.getBigUint64(record, true)), v.getBigUint64(record + 16, true), v.getBigUint64(record + 32, true));
    },
    Count(record) { view().setBigUint64(record, 7n, true); },
};
WebAssembly.instantiate(bytes, {zx: {
    budget: new WebAssembly.Global({value: 'i64', mutable: true}, 100000000n),
    overrun() { throw Error('budget spent'); },
    host_i64(index, value) {
        if ((index >>> 0) === 0xfffffffa) process.stdout.write(value ? 'true' : 'false');
        else if ((index >>> 0) !== 0xffffffff) throw Error('unexpected print service');
        else process.stdout.write(Buffer.from(wasm.memory.buffer, Number(value & 0xffffffffn), Number(value >> 32n)));
        return 0n;
    },
    host(index, record) { calls[capabilities[index].name](record); return 1; },
}}).then(({instance}) => {
    wasm = instance.exports;
    const result = wasm.zx_entry();
    console.log(capabilities.map(c => c.name + '/' + c.count).join(' '), notes.join(' '), result.toString());
}).catch(error => { console.error(error); process.exitCode = 1; });
JS
typed=$("$nodebin" "$work/typed.js" "$work/typed.wasm")
expected_typed='hi zx hi zx box! false
Greet/2 Weigh/1 Fill/1 Note/3 Flag/2 Count/0 -5 200 65236 78'
test "$typed" = "$expected_typed" || {
    echo "zx-wasm-test: typed host calls gave '$typed'"
    exit 1
}

# The heap reuses freed blocks: a loop that grows and frees a vector a
# thousand times leaves memory at its starting size.
cat >"$work/heap.zi" <<'ZI'
#import "vec"
main :: () -> s32 {
    total: s64
    for round: 0..999 {
        values: Vec(s64)
        for index: 0..99 { VecPush(values, cast(s64)index) }
        total += values[99]
        VecFree(values)
    }
    print("%\n", total)
    return 0
}
ZI
"$zi2asm" --root "$work" -I "$repo/std" --entry heap:main --emit zx -o "$work/heap.zx" "$work/heap.zi"
"$zi2asm" --input zx --emit wasm -o "$work/heap.wasm" "$work/heap.zx"
heap=$("$nodebin" - "$work/heap.wasm" <<'JS'
const fs = require('fs');
let memory;
WebAssembly.instantiate(fs.readFileSync(process.argv[2]), {zx: {
    budget: new WebAssembly.Global({value: 'i64', mutable: true}, 100000000n),
    overrun() { throw Error('budget spent'); },
    host_i64(index, value) {
        if ((index >>> 0) === 0xfffffffe) process.stdout.write(BigInt.asIntN(64, value).toString());
        else process.stdout.write(Buffer.from(memory.buffer, Number(value & 0xffffffffn), Number(value >> 32n)));
        return 0n;
    },
}}).then(({instance}) => {
    memory = instance.exports.memory;
    const pages = memory.buffer.byteLength;
    instance.exports.zx_entry();
    console.log(memory.buffer.byteLength - pages);
}).catch(error => { console.error(error); process.exitCode = 1; });
JS
)
test "$heap" = "99000
0" || {
    echo "zx-wasm-test: heap run gave '$heap'"
    exit 1
}
test "$("$zi2asm" --run --heap-report --input zx "$work/heap.zx" 2>/dev/null)" = 99000

# The bundle Wasm tier in a WebAssembly host: libziran built by emcc with
# the instruction engine, whose web provider lowers each revision-28 bundle
# through zx_wasm_lower and translates its host calls and module state to
# VmHostValue bindings. The Wasm instances and engine instances must agree.
# Skipped without emcc.
emcc=${ZIRAN_WASM_EMCC:-}
if ! command -v "$emcc" >/dev/null 2>&1 && test -x "$HOME/emsdk/upstream/emscripten/emcc"; then
    emcc=$HOME/emsdk/upstream/emscripten/emcc
fi
if command -v "$emcc" >/dev/null 2>&1; then
    make -C "$repo" -j2 BUILD_DIR="$work/lib" CC="$emcc" AR="$(dirname -- "$emcc")/emar" \
        OBJCOPY=true CFLAGS='-O2' SANITIZE_FLAGS= FRAMEFLAGS='-Wframe-larger-than=16384' \
        WASM_PRIVATE_HEADERS="$work/private" "$work/lib/libziran.a" > "$work/lib.log" 2>&1 || {
            tail -20 "$work/lib.log"
            exit 1
        }
    # A computed initializer, state that grows each run, a string global and
    # a host capability.
    cat >"$work/state.zi" <<'ZI'
host_api :: #system_library "host_api";
AddHost :: (value: s64) -> s64 #foreign host_api;
Seed :: () -> s64 { return 3 + 4 }
base: s64 = Seed();
runs: s64 = 40;
table: [4]s64;
title: string = "old";
Main :: () -> s64 {
    runs += 1
    table[runs % 4] += runs
    return AddHost(runs * 100 + base + table[1] + cast(s64)title.count)
}
ZI
    "$zi2asm" --root "$work" --entry state:Main --emit zx -o "$work/state.zx" "$work/state.zi"
    # A record that holds itself through a slice crosses both ways; one
    # that holds itself stops at the nesting bound.
    cat >"$work/tree.zi" <<'ZI'
host_api :: #system_library "host_api";
Node :: struct {
    value: s32
    children: []Node
}
Grow :: (tree: Node) -> Node #foreign host_api;
tree: Node;
leaves: [2]Node;
ring: [1]Node;
#program_export
Answer :: () -> s32 {
    leaves[0].value = 2
    leaves[1].value = 3
    tree.value = 1
    tree.children = leaves[:]
    tree = Grow(tree)
    if tree.value != 6 || tree.children.count != 2 { return 1 }
    if tree.children[0].value != 100 || tree.children[1].children.count != 1 { return 2 }
    return tree.children[1].children[0].value
}
#program_export
Cycle :: () -> s32 {
    ring[0].value = 1
    ring[0].children = ring[:]
    return Grow(ring[0]).value
}
ZI
    "$ziran" bundle --root "$work" --entry tree:Answer -o "$work/tree.zx" "$work/tree.zi"
    "$ziran" bundle --root "$work" --entry tree:Cycle -o "$work/cycle.zx" "$work/tree.zi"
    cat >"$work/tier.c" <<'C'
#include "ziran_host.h"
#include "zir_zx_wasm_platform.h"
#include <emscripten.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static char text[64], title[32];
static VmHostField fields[3];
static long long tree_sum(const VmHostValue *node)
{
    long long sum = node->fields[0].value.integer;
    const VmHostValue *children = &node->fields[1].value;
    for(size_t at = 0; at < children->length; at++) sum += tree_sum(&children->elements[at]);
    return sum;
}
static VmHostValue tree_node(VmHostField parts[2], long long value, VmHostValue *children, size_t count)
{
    parts[0] = (VmHostField){"value", {.kind = VM_HOST_INTEGER, .type = "s32", .integer = value, .bits = (uint64_t)value}};
    parts[1] = (VmHostField){"children", {.kind = VM_HOST_SLICE, .type = "[]Node", .length = count, .elements = children}};
    return (VmHostValue){.kind = VM_HOST_RECORD, .type = "Node", .fields = parts, .field_count = 2};
}
static int call(void *context, const char *module, const char *function,
                const VmHostValue *args, int count, VmHostValue *result)
{
    (void)context; (void)module; (void)count;
    if(!strcmp(function, "Greet")) {
        snprintf(text, sizeof(text), "hi %.*s x%lld", (int)args[0].length, (const char *)args[0].data,
                 (long long)args[1].integer);
        result->kind = VM_HOST_STRING; result->data = (const unsigned char *)text; result->length = strlen(text);
    } else if(!strcmp(function, "Weigh")) {
        const VmHostField *in = args[0].fields;
        snprintf(title, sizeof(title), "%.*s!", (int)in[0].value.length, (const char *)in[0].value.data);
        fields[0] = (VmHostField){"title", {.kind = VM_HOST_STRING, .type = "string",
            .data = (const unsigned char *)title, .length = strlen(title)}};
        fields[1] = (VmHostField){"weight", {.kind = VM_HOST_INTEGER, .type = "s32", .integer = in[1].value.integer * 10}};
        fields[2] = (VmHostField){"on", {.kind = VM_HOST_INTEGER, .type = "bool", .integer = !in[2].value.integer}};
        result->kind = VM_HOST_RECORD; result->fields = fields; result->field_count = 3;
    } else if(!strcmp(function, "Fill")) {
        for(size_t at = 0; at < args[0].length; at++)
            args[0].elements[at].bits = (uint64_t)(args[0].elements[at].integer = 40 + (int64_t)at);
        result->kind = VM_HOST_VOID;
    } else if(!strcmp(function, "Flag")) {
        result->kind = VM_HOST_UNSIGNED; result->bits = (uint64_t)(args[0].integer * args[1].real * 100 + 0.5);
    } else if(!strcmp(function, "Note")) {
        printf("note %lld %llu %lld\n", (long long)args[0].integer, (unsigned long long)args[1].bits,
               (long long)args[2].integer);
        result->kind = VM_HOST_VOID;
    } else if(!strcmp(function, "Count")) {
        result->kind = VM_HOST_INTEGER; result->integer = 7;
    } else if(!strcmp(function, "AddHost")) {
        result->kind = VM_HOST_INTEGER; result->integer = args[0].integer + 1000;
    } else if(!strcmp(function, "Grow")) {
        /* The given tree's sum over leaves 100 and 200, the second with a
         * leaf 7 of its own. */
        static VmHostField root[2], first[2], second[2], inner[2];
        static VmHostValue leaves[2], below[1];
        below[0] = tree_node(inner, 7, NULL, 0);
        leaves[0] = tree_node(first, 100, NULL, 0);
        leaves[1] = tree_node(second, 200, below, 1);
        *result = tree_node(root, tree_sum(&args[0]), leaves, 2);
    } else return 0;
    return 1;
}
static long long run(BundleInstance *instance)
{
    long long value = -1;
    int has = 0;
    return instance != NULL && BundleInstanceRun(instance, &value, &has) && has ? value : -1;
}
static unsigned long reject(void *context, const unsigned char *bytes, size_t size)
{
    (void)context; (void)bytes; (void)size;
    return 0;
}
/* Runs typed.zx, then state.zx: two instances side by side with their own
 * globals, then a third that takes the first one's state, changed; then a
 * tree that holds itself through a slice, and a cycle that must fail. */
static void scenario(const char *engine, Bundle *typed, Bundle *state, Bundle *tree, Bundle *cycle)
{
    static const char *names[] = {"Greet", "Weigh", "Fill", "Flag", "Note", "Count"};
    HostBinding bindings[6];
    for(int at = 0; at < 6; at++) bindings[at] = (HostBinding){.module = "typed", .function = names[at], .call = call};
    BundleInstance *instance = BundleInstantiate(typed, bindings, 6);
    long long value = run(instance);
    printf("%s typed %lld\n", engine, value);
    fflush(stdout);
    BundleInstanceClose(instance);
    HostBinding add = {.module = "state", .function = "AddHost", .call = call};
    BundleInstance *first = BundleInstantiate(state, &add, 1), *beside = BundleInstantiate(state, &add, 1);
    long long a = run(first), b = run(first), c = run(beside);
    BundleState *saved = NULL;
    int exported = BundleInstanceExportState(first, &saved);
    size_t runs = 0, named = 0;
    int ok = exported && BundleStateFind(saved, "state", "runs", &runs) && BundleStateFind(saved, "state", "title", &named);
    BundleStateValue *count = ok ? BundleStateValueRef(saved, runs) : NULL;
    BundleStateValue *label = ok ? BundleStateValueRef(saved, named) : NULL;
    if(count != NULL && label != NULL && label->value.kind == VM_HOST_STRING) {
        count->value.integer = 100;
        count->value.bits = 100;
        unsigned char *copied = malloc(5);
        memcpy(copied, "newer", 5);
        free((void *)label->value.data);
        label->value.data = copied;
        label->value.length = 5;
    } else ok = 0;
    BundleInstance *moved = BundleInstantiate(state, &add, 1);
    long long d = ok && BundleInstanceImportState(moved, saved) ? run(moved) : -1;
    printf("%s state %lld %lld %lld %lld\n", engine, a, b, c, d);
    fflush(stdout);
    BundleStateClose(saved);
    BundleInstanceClose(moved);
    BundleInstanceClose(beside);
    BundleInstanceClose(first);
    HostBinding grow = {.module = "tree", .function = "Grow", .call = call};
    BundleInstance *grown = BundleInstantiate(tree, &grow, 1), *ring = BundleInstantiate(cycle, &grow, 1);
    long long spent = -1;
    int has = 0, cycled = ring != NULL && BundleInstanceRun(ring, &spent, &has);
    printf("%s tree %lld cycle %s\n", engine, run(grown), cycled ? "ran" : "failed");
    fflush(stdout);
    BundleInstanceClose(ring);
    BundleInstanceClose(grown);
}
int main(int argc, char **argv)
{
    Bundle *typed = argc == 5 ? BundleOpen(argv[1]) : NULL;
    Bundle *state = argc == 5 ? BundleOpen(argv[2]) : NULL;
    Bundle *tree = argc == 5 ? BundleOpen(argv[3]) : NULL;
    Bundle *cycle = argc == 5 ? BundleOpen(argv[4]) : NULL;
    if(typed == NULL || state == NULL || tree == NULL || cycle == NULL) return 3;
    const ZxWasmPlatform *active = ZxWasmPlatformActive();
    if(active == NULL) return 4;
    ZxWasmPlatform provider = *active;
    scenario("wasm", typed, state, tree, cycle);
    printf("wasm modules %d\n", EM_ASM_INT({ return globalThis.zxWasmTier.modules.size; }));
    /* A module the provider rejects is an error; no other engine runs it. */
    ZxWasmPlatform rejecting = provider;
    rejecting.instantiate = reject;
    ZxWasmSetPlatform(&rejecting);
    HostBinding add = {.module = "state", .function = "AddHost", .call = call};
    BundleInstance *refused = BundleInstantiate(state, &add, 1);
    printf("rejected %d\n", refused == NULL);
    BundleInstanceClose(refused);
    /* Without a provider the instruction engine runs the same bundles. */
    ZxWasmSetPlatform(NULL);
    scenario("engine", typed, state, tree, cycle);
    BundleClose(typed);
    BundleClose(state);
    BundleClose(tree);
    BundleClose(cycle);
    return 0;
}
C
    "$emcc" -O2 -std=gnu99 -I"$repo/include" -I"$repo/cmd/zir" "$work/tier.c" "$work/lib/libziran.a" \
        -o "$work/tier.js" -s ENVIRONMENT=node -s NODERAWFS=1 -s ALLOW_MEMORY_GROWTH=1 -s EXIT_RUNTIME=1
    tier=$("$nodebin" "$work/tier.js" "$work/typed.zx" "$work/state.zx" "$work/tree.zx" "$work/cycle.zx" 2> "$work/tier.err") || {
        cat "$work/tier.err"
        exit 1
    }
    expected_tier='note -5 200 -300
hi zx x2 box! false
wasm typed 78
wasm state 5151 5251 5151 11254
wasm tree 7 cycle failed
wasm modules 4
rejected 1
note -5 200 -300
hi zx x2 box! false
engine typed 78
engine state 5151 5251 5151 11254
engine tree 7 cycle failed'
    test "$tier" = "$expected_tier" || {
        echo "zx-wasm-test: the bundle Wasm tier and the engine gave '$tier'"
        cat "$work/tier.err"
        exit 1
    }
    echo 'zx-wasm-test: revision-28 bundles ran on the bundle Wasm tier: typed host calls, recursive host types, independent instances and moved state'
else
    echo 'zx-wasm-test: no emcc; skipping the bundle Wasm tier'
fi

# The budget guard must stop a run that exceeds its step count.
cat >"$work/spend_budget.js" <<'EOF'
const fs = require('fs');
const file = process.argv[2];
WebAssembly.instantiate(fs.readFileSync(file), {
  zx: {
    overrun: () => { throw new Error('overrun'); },
    budget: new WebAssembly.Global({value: 'i64', mutable: true}, 5n),
  },
}).then(({instance}) => {
    try { instance.exports.zx_entry(); } catch (e) { process.exit(0); }
    console.error('no overrun trap');
    process.exit(1);
}).catch((e) => { console.error(e.message); process.exit(1); });
EOF
run_engine budget "$work/zx_wasm_test.wasm"

# A second engine, when available, must agree on the same module.
if test "$wasmtime_available" = 1 && test "$native_engine" != wasmtime; then
    test "$("$ziran" run --target=c --root "$repo/tests" "$repo/tests/wasmtime_engine.zi" -- "$ziran" "$work" "$wasmtime" scalar "$work/zx_wasm_test.wasm" 10000000)" = "$expected"
fi

# Enum values use their checked backing integers in direct WebAssembly.
cat >"$work/enums.zi" <<'EOF'
Direction :: enum {
    Up
    Down
}
#program_export
Answer :: () -> s64 {
    way: Direction = Direction.Down
    return ifx way == Direction.Up then 1 else 2
}
EOF
"$zi2asm" --root "$work" --entry enums:Answer --emit wasm \
    -o "$work/enums.wasm" "$work/enums.zi"
test "$(run_engine scalar "$work/enums.wasm" 1000000)" = 2

# Strings and slices lower and must agree with the other tiers.
cat >"$work/text.zi" <<'EOF'
TEXT: string = "aé中🙂";
#program_export
Answer :: () -> s64 {
    sum: s64 = 0
    i: s64 = 0
    while i < cast(s64)TEXT.count {
        sum += cast(s64)TEXT[cast(s32)i]
        i += 1
    }
    digits: [4]u8
    digits[0] = 7
    digits[1] = 9
    window: []u8 = digits[0:2]
    if TEXT == "aé中🙂" { return sum * 10 + cast(s64)window[1] }
    return -1
}
EOF
"$zi2asm" --root "$work" --entry text:Answer --emit wasm \
    -o "$work/text.wasm" "$work/text.zi"
text=$(run_engine scalar "$work/text.wasm" 1000000)
"$ziran" bundle --root "$work" --entry text:Answer \
    -o "$work/text.zx" "$work/text.zi"
case "$("$ziran" run "$work/text.zx")" in
    "$text") ;;
    *) echo "zx-wasm-test: strings disagree between tiers"; exit 1 ;;
esac

# Records and arrays lower and must agree with the native tier.
cat >"$work/records.zi" <<'EOF'
Point :: struct {
    x: s32
    y: s32
}
#program_export
Answer :: () -> s64 {
    points: [4]Point
    points[0] = Point.{x = 3, y = 4}
    points[1] = Point.{x = points[0].y, y = points[0].x}
    total: s64 = 0
    for k: 0..3 {
        total += cast(s64)points[k].x + cast(s64)points[k].y
    }
    return total
}
EOF
"$zi2asm" --root "$work" --entry records:Answer --emit wasm \
    -o "$work/records.wasm" "$work/records.zi"
records=$(run_engine scalar "$work/records.wasm" 1000000)
"$ziran" bundle --root "$work" --entry records:Answer \
    -o "$work/records.zx" "$work/records.zi"
case "$("$ziran" run "$work/records.zx")" in
    "$records") ;;
    *) echo "zx-wasm-test: records disagree between tiers"; exit 1 ;;
esac

# The same fixture through the native tier and the portable VM.
"$ziran" bundle --root "$repo/tests/spec" --entry "$entry" \
    -o "$work/zx_wasm_test.zx" "$source"
test "$("$ziran" run "$work/zx_wasm_test.zx")" = "$expected"

# An engine rejects a whole module that holds SIMD instructions it lacks, so
# wasm32+simd128 variants enter generated C only when the build enables SIMD.
# One source yields a SIMD module and a baseline module; both must validate
# and agree with the portable body. Loads use alignment 1, and the v128
# accumulator lives in memory because Clang 18 cannot assemble typed blocks.
cat >"$work/simd.zi" <<'EOF'
#program_export
SimdPath :: () -> s32 #asm("wasm32+simd128", "i32.const 2\n\tlocal.set %[out]", "=r", "", "", "") #asm("wasm32", "i32.const 1\n\tlocal.set %[out]", "=r", "", "", "") { return 0 }
#program_export
SumChunks :: (data: *u8, chunks: u32, scratch: *u32) -> u32 #asm("wasm32+simd128", "local.get %[scratch]\n\tv128.const 0, 0, 0, 0\n\tv128.store 0:p2align=2\n\tblock\n\tlocal.get %[chunks]\n\ti32.eqz\n\tbr_if 0\n\tlocal.get %[data]\n\tlocal.set %[out]\n\tloop\n\tlocal.get %[scratch]\n\tlocal.get %[scratch]\n\tv128.load 0:p2align=2\n\tlocal.get %[out]\n\tv128.load 0:p2align=0\n\ti16x8.extadd_pairwise_i8x16_u\n\ti32x4.extadd_pairwise_i16x8_u\n\ti32x4.add\n\tv128.store 0:p2align=2\n\tlocal.get %[out]\n\ti32.const 16\n\ti32.add\n\tlocal.tee %[out]\n\tlocal.get %[data]\n\tlocal.get %[chunks]\n\ti32.const 4\n\ti32.shl\n\ti32.add\n\ti32.lt_u\n\tbr_if 0\n\tend_loop\n\tend_block\n\tlocal.get %[scratch]\n\ti32.load 0\n\tlocal.get %[scratch]\n\ti32.load 4\n\ti32.add\n\tlocal.get %[scratch]\n\ti32.load 8\n\ti32.add\n\tlocal.get %[scratch]\n\ti32.load 12\n\ti32.add\n\tlocal.set %[out]", "=&r", "r;r;r", "memory", "") {
    unused scratch
    total: u32
    i: u32 = 0
    while i < chunks * 16 { total += cast(u32)data[i]; i += 1 }
    return total
}
#program_export
BlockPath :: () -> s32 {
    path: s32
    #asm("wasm32+simd128", "i32.const 7\n\ti32x4.splat\n\ti32x4.extract_lane 3\n\tlocal.set %[path]", "=r:path", "", "", "") #asm("wasm32", "i32.const 1\n\tlocal.set %[path]", "=r:path", "", "", "") {
        path = 0
    }
    return path
}
#program_export
Answer :: () -> s64 {
    buffer: [161]u8
    i: s32 = 0
    while i < 161 { buffer[i] = cast(u8)(i * 37 + 11); i += 1 }
    scratch: [4]u32
    total: s64 = 0
    chunks: u32 = 0
    while chunks <= 10 {
        total = total * 3 + cast(s64)SumChunks(*buffer[1], chunks, *scratch[0])
        chunks += 1
    }
    return (total * 10 + cast(s64)BlockPath()) * 10 + cast(s64)SimdPath()
}
EOF
# From Clang 19, a v128 can stay on the operand stack through block and loop
# signatures; that kernel runs in a module of its own.
cat >"$work/carried.zi" <<'EOF'
#program_export
Carried :: (data: *u8, chunks: u32, scratch: *u32) -> u32 #asm("wasm32+simd128", "local.get %[scratch]\n\tv128.const 0, 0, 0, 0\n\tblock (v128) -> (v128)\n\tlocal.get %[chunks]\n\ti32.eqz\n\tbr_if 0\n\tlocal.get %[data]\n\tlocal.get %[chunks]\n\ti32.const 4\n\ti32.shl\n\ti32.add\n\tlocal.set %[out]\n\tloop (v128) -> (v128)\n\tlocal.get %[out]\n\tlocal.get %[chunks]\n\ti32.const 4\n\ti32.shl\n\ti32.sub\n\tv128.load 0:p2align=0\n\ti16x8.extadd_pairwise_i8x16_u\n\ti32x4.extadd_pairwise_i16x8_u\n\ti32x4.add\n\tlocal.get %[out]\n\ti32.const 16\n\ti32.add\n\tlocal.tee %[out]\n\tlocal.get %[data]\n\tlocal.get %[chunks]\n\ti32.const 5\n\ti32.shl\n\ti32.add\n\ti32.lt_u\n\tbr_if 0\n\tend_loop\n\tend_block\n\tv128.store 0:p2align=2\n\tlocal.get %[scratch]\n\ti32.load 0\n\tlocal.get %[scratch]\n\ti32.load 4\n\ti32.add\n\tlocal.get %[scratch]\n\ti32.load 8\n\ti32.add\n\tlocal.get %[scratch]\n\ti32.load 12\n\ti32.add\n\tlocal.set %[out]", "=&r", "r;r;r", "memory", "") {
    unused scratch
    total: u32
    i: u32 = 0
    while i < chunks * 16 { total += cast(u32)data[i]; i += 1 }
    return total
}
#program_export
Answer :: () -> s64 {
    buffer: [161]u8
    i: s32 = 0
    while i < 161 { buffer[i] = cast(u8)(i * 37 + 11); i += 1 }
    scratch: [4]u32
    total: s64 = 0
    chunks: u32 = 0
    while chunks <= 10 {
        total = total * 3 + cast(s64)Carried(*buffer[1], chunks, *scratch[0])
        chunks += 1
    }
    return total
}
EOF
for name in simd carried; do
    "$ziran" build --target=c --root "$work" -o "$work/$name-c" "$work/$name.zi"
done
printf '#include <stdio.h>\n#include "simd.h"\nint main(void) { printf("%%lld\\n", (long long)Answer()); }\n' \
    >"$work/simd-host.c"
"${CC:-cc}" -std=c11 -O2 -I"$repo/include" -I"$work/simd-c" \
    "$work/simd-c/simd.c" "$work/simd-host.c" -o "$work/simd-native"
portable=$("$work/simd-native")
case $(uname -m) in wasm*) ;; *) test "$((portable % 100))" = 0 ;; esac
# Supply the small freestanding libc surface used by generated C.
mkdir -p "$work/freestanding"
cat >"$work/freestanding/stdio.h" <<'EOF'
typedef struct FILE FILE;
#define stderr ((FILE *)0)
static inline int fprintf(FILE *file, const char *format, ...) { (void)file; (void)format; return 0; }
EOF
printf '_Noreturn static inline void abort(void) { __builtin_trap(); }\n' >"$work/freestanding/stdlib.h"
cat >"$work/freestanding/string.h" <<'EOF'
#ifndef ZIRAN_TEST_STRING_H
#define ZIRAN_TEST_STRING_H
#include <stddef.h>
static inline void *memcpy(void *dst, const void *src, size_t n) {
    unsigned char *d = dst;
    const unsigned char *s = src;
    for (size_t i = 0; i < n; ++i) d[i] = s[i];
    return dst;
}
static inline int memcmp(const void *a, const void *b, size_t n) {
    const unsigned char *x = a, *y = b;
    for (size_t i = 0; i < n; ++i) if (x[i] != y[i]) return x[i] - y[i];
    return 0;
}
#endif
EOF
. "$repo/tests/lib/wasm_cc.sh"
: >"$work/wasm-probe.log"
simd_ran=0
wasm_build() {
    name=$1; shift
    # Unquoted on purpose: the compiler command may carry options.
    $wasmcc --target=wasm32 -std=c11 -O2 -mbulk-memory -ffreestanding -nostdlib \
        -isystem "$work/freestanding" -I"$repo/include" -I"$work/$name-c" "$@" \
        -Wl,--no-entry -Wl,--export=Answer "$work/$name-c/$name.c"
}
if wasm_cc_select; then
    echo "zx-wasm-test: wasm32 compiler: $wasmcc"
    wasm_build simd -o "$work/simd-baseline.wasm"
    wasm_build simd -msimd128 -o "$work/simd-simd.wasm"
    wasm_build simd -msimd128 -DZIRAN_NO_ASM -o "$work/simd-portable.wasm"
    cat >"$work/run_simd.js" <<'EOF'
const fs = require('fs');
for (const file of process.argv.slice(2)) {
  const bytes = fs.readFileSync(file);
  if (!WebAssembly.validate(bytes)) { console.error(file + ' does not validate'); process.exit(1); }
  console.log(new WebAssembly.Instance(new WebAssembly.Module(bytes), {}).exports.Answer().toString());
}
EOF
    results=$(run_engine simd "$work/simd-simd.wasm" \
        "$work/simd-baseline.wasm" "$work/simd-portable.wasm" | tr '\n' ' ')
    test "$results" = "$((portable + 72)) $((portable + 11)) $portable " || {
        echo "zx-wasm-test: SIMD128 results '$results' for portable $portable"
        exit 1
    }
    echo "zx-wasm-test: executed SIMD128, baseline and portable modules: $results"
    # The assembler rejects SIMD instructions in a build without SIMD, so the
    # baseline module built above holds none of the SIMD templates.
    if wasm_build simd -D__wasm_simd128__ -o "$work/simd-forced.wasm" >"$work/simd-forced.log" 2>&1 ||
       ! grep -q 'instruction requires: simd128' "$work/simd-forced.log"; then
        echo "zx-wasm-test: a build without -msimd128 accepted the SIMD templates"
        exit 1
    fi
    if test "$($wasmcc -dumpversion | cut -d. -f1)" -ge 19; then
        wasm_build carried -msimd128 -o "$work/carried-simd.wasm"
        wasm_build carried -o "$work/carried-baseline.wasm"
        results=$(run_engine simd "$work/carried-simd.wasm" \
            "$work/carried-baseline.wasm" | tr '\n' ' ')
        test "$results" = "$((portable / 100)) $((portable / 100)) " || {
            echo "zx-wasm-test: stack-carried SIMD128 results '$results' for portable $portable"
            exit 1
        }
        echo "zx-wasm-test: executed stack-carried SIMD128 and baseline modules: $results"
    else
        echo "zx-wasm-test: $wasmcc is older than Clang 19; skipping the stack-carried SIMD128 kernel"
    fi
    simd_ran=1
else
    echo "zx-wasm-test: no working wasm32 compiler/linker; skipping the SIMD128 modules"
fi
if test "${WASM_REQUIRED:-0}" = 1 && test "$simd_ran" != 1; then
    cat "$work/wasm-probe.log" >&2
    echo 'zx-wasm-test: WASM_REQUIRED=1 needs a clang with wasm-ld (set ZIRAN_WASM_CC)' >&2
    exit 1
fi
# The portable tiers run the checked bodies of the same assembly procedures.
cat >"$work/simd_path.zi" <<'EOF'
#program_export
SimdPath :: () -> s32 #asm("wasm32+simd128", "i32.const 2\n\tlocal.set %[out]", "=r", "", "", "") #asm("wasm32", "i32.const 1\n\tlocal.set %[out]", "=r", "", "", "") { return 0 }
#program_export
Answer :: () -> s64 { return 40 + cast(s64)SimdPath() }
EOF
"$zi2asm" --root "$work" --entry simd_path:Answer --emit wasm \
    -o "$work/simd_path.wasm" "$work/simd_path.zi"
test "$(run_engine scalar "$work/simd_path.wasm" 1000000)" = 40
"$ziran" bundle --root "$work" --entry simd_path:Answer \
    -o "$work/simd_path.zx" "$work/simd_path.zi"
test "$("$ziran" run "$work/simd_path.zx")" = 40

echo 'Wasm modules from revision-28 ZX agree with the instruction engine and trap on spent budgets'
