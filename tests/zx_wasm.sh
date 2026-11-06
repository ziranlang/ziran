#!/bin/sh
set -eu
# The wasm execution tier must agree with the native tier and the portable
# VM on the scalar fixture, and reject out-of-subset programs with a
# diagnostic instead of guessing. node runs the emitted module; wasmtime
# is used as a second engine when its python binding is importable.
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

command -v "$nodebin" >/dev/null 2>&1 || {
    echo "zx-wasm-test: node is unavailable; skipping the wasm execution check"
    exit 0
}

zi2wasm=$(dirname -- "$ziran")/zi2wasm
zi2zx_vm=$(dirname -- "$ziran")/zi2zx
test -x "$zi2wasm" || {
    echo "zx-wasm-test: $zi2wasm is not built; run make first"
    exit 1
}

"$zi2wasm" --root "$repo/tests/spec" --entry "$entry" \
    -o "$work/zx_wasm_test.wasm" "$source"

run_wasm="$work/run_wasm.js"
cat >"$run_wasm" <<'EOF'
// Instantiates one zi2wasm module, wires the budget global and the
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

result=$("$nodebin" "$run_wasm" "$work/zx_wasm_test.wasm" 10000000)
test "$result" = "$expected" || {
    echo "zx-wasm-test: wasm result $result != $expected"
    exit 1
}

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
"$nodebin" "$work/spend_budget.js" "$work/zx_wasm_test.wasm"

# A second engine, when available, must agree on the same module.
if command -v python3 >/dev/null 2>&1 &&
   python3 -c 'import wasmtime' >/dev/null 2>&1; then
    python3 - "$work/zx_wasm_test.wasm" "$expected" <<'EOF'
import sys
from wasmtime import Engine, Linker, Module, Store, ValType, Global, Func, FuncType
path, expected = sys.argv[1], sys.argv[2]
engine = Engine()
module = Module.from_file(engine, path)
linker = Linker(engine)
linker.define("zx", "overrun",
              Func(engine, FuncType([], []), lambda: (_ for _ in ()).throw(RuntimeError("overrun"))))
linker.define("zx", "budget", Global(engine, ValType.i64(), 10000000, True))
store = Store(engine)
instance = linker.instantiate(store, module)
result = instance.exports(store)["zx_entry"](store)
if str(result) != expected:
    print(f"zx-wasm-test: wasmtime result {result} != {expected}")
    sys.exit(1)
EOF
fi

# Enums are outside the tier while strings lower: the tool reports the
# type and exits nonzero rather than emitting a wrong module.
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
if "$zi2wasm" --root "$work" --entry enums:Answer \
        -o "$work/enums.wasm" "$work/enums.zi" >/dev/null 2>&1; then
    echo "zx-wasm-test: enums lowered without a diagnostic"
    exit 1
fi

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
"$zi2wasm" --root "$work" --entry text:Answer \
    -o "$work/text.wasm" "$work/text.zi"
text=$("$nodebin" "$run_wasm" "$work/text.wasm" 1000000)
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
"$zi2wasm" --root "$work" --entry records:Answer \
    -o "$work/records.wasm" "$work/records.zi"
records=$("$nodebin" "$run_wasm" "$work/records.wasm" 1000000)
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
test "$("$zi2zx_vm" run --vm "$work/zx_wasm_test.zx")" = "$expected"

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
# The generated C needs no libc beyond its bounds-failure report.
mkdir -p "$work/freestanding"
cat >"$work/freestanding/stdio.h" <<'EOF'
typedef struct FILE FILE;
#define stderr ((FILE *)0)
static inline int fprintf(FILE *file, const char *format, ...) { (void)file; (void)format; return 0; }
EOF
printf '_Noreturn static inline void abort(void) { __builtin_trap(); }\n' >"$work/freestanding/stdlib.h"
wasmcc=${ZIRAN_WASM_CC:-clang}
simd_ran=0
wasm_build() {
    name=$1; shift
    # Unquoted on purpose: the compiler command may carry options.
    $wasmcc --target=wasm32 -std=c11 -O2 -mbulk-memory -ffreestanding -nostdlib \
        -isystem "$work/freestanding" -I"$repo/include" -I"$work/$name-c" "$@" \
        -Wl,--no-entry -Wl,--export=Answer "$work/$name-c/$name.c"
}
if wasm_build simd -o "$work/simd-baseline.wasm" >/dev/null 2>&1; then
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
    results=$("$nodebin" "$work/run_simd.js" "$work/simd-simd.wasm" \
        "$work/simd-baseline.wasm" "$work/simd-portable.wasm" | tr '\n' ' ')
    test "$results" = "$((portable + 72)) $((portable + 11)) $portable " || {
        echo "zx-wasm-test: SIMD128 results '$results' for portable $portable"
        exit 1
    }
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
        results=$("$nodebin" "$work/run_simd.js" "$work/carried-simd.wasm" \
            "$work/carried-baseline.wasm" | tr '\n' ' ')
        test "$results" = "$((portable / 100)) $((portable / 100)) " || {
            echo "zx-wasm-test: stack-carried SIMD128 results '$results' for portable $portable"
            exit 1
        }
    else
        echo "zx-wasm-test: $wasmcc is older than Clang 19; skipping the stack-carried SIMD128 kernel"
    fi
    simd_ran=1
else
    echo "zx-wasm-test: $wasmcc cannot link wasm32; skipping the SIMD128 modules"
fi
if test "${WASM_REQUIRED:-0}" = 1 && test "$simd_ran" != 1; then
    echo 'zx-wasm-test: WASM_REQUIRED=1 needs a clang with wasm-ld' >&2
    exit 1
fi
# The portable tiers run the checked bodies of the same assembly procedures.
cat >"$work/simd_path.zi" <<'EOF'
#program_export
SimdPath :: () -> s32 #asm("wasm32+simd128", "i32.const 2\n\tlocal.set %[out]", "=r", "", "", "") #asm("wasm32", "i32.const 1\n\tlocal.set %[out]", "=r", "", "", "") { return 0 }
#program_export
Answer :: () -> s64 { return 40 + cast(s64)SimdPath() }
EOF
"$zi2wasm" --root "$work" --entry simd_path:Answer \
    -o "$work/simd_path.wasm" "$work/simd_path.zi"
test "$("$nodebin" "$run_wasm" "$work/simd_path.wasm" 1000000)" = 40
"$ziran" bundle --root "$work" --entry simd_path:Answer \
    -o "$work/simd_path.zx" "$work/simd_path.zi"
test "$("$ziran" run "$work/simd_path.zx")" = 40

echo 'The wasm execution tier agrees with the native tier and the portable VM, and traps on spent budgets'
