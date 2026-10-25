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
expected=9007
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

# Records are outside the scalar subset: the tool reports them and exits
# nonzero rather than emitting a wrong module.
cat >"$work/records.zi" <<'EOF'
Point :: struct {
    x: s32
    y: s32
}
#program_export
Answer :: () -> s64 {
    p: Point
    p.x = 3
    return cast(s64)p.x
}
EOF
if "$zi2wasm" --root "$work" --entry records:Answer \
        -o "$work/records.wasm" "$work/records.zi" >/dev/null 2>&1; then
    echo "zx-wasm-test: records lowered without a diagnostic"
    exit 1
fi

# The same fixture through the native tier and the portable VM.
"$ziran" bundle --root "$repo/tests/spec" --entry "$entry" \
    -o "$work/zx_wasm_test.zx" "$source"
test "$("$ziran" run "$work/zx_wasm_test.zx")" = "$expected"
test "$("$zi2zx_vm" run "$work/zx_wasm_test.zx")" = "$expected"

echo 'The wasm execution tier agrees with the native tier and the portable VM, and traps on spent budgets'
