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
zi2zx_vm=$(dirname -- "$ziran")/zi2zx-vm
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
test "$("$zi2zx_vm" run "$work/zx_wasm_test.zx")" = "$expected"

echo 'The wasm execution tier agrees with the native tier and the portable VM, and traps on spent budgets'
