# Sourced by wasm checks. Probe a real link, since a clang executable alone
# does not imply that its WebAssembly backend and wasm-ld are installed.
# The caller supplies an existing scratch directory as $work.
wasm_cc_select() {
    printf '%s\n' '
#include <wasm_simd128.h>
int Answer(int x) {
    return wasm_i32x4_extract_lane(wasm_i32x4_splat(x), 0);
}
' >"$work/wasm-probe.c"
    if test -n "${ZIRAN_WASM_CC:-}"; then
        wasm_cc_probe "$ZIRAN_WASM_CC"
        return
    fi
    for candidate in clang clang-22 clang-21 clang-20 clang-19 clang-18 clang-17 clang-16 clang-15 clang-14; do
        if wasm_cc_probe "$candidate"; then return 0; fi
    done
    return 1
}

wasm_cc_probe() {
    printf 'wasm compiler probe: %s\n' "$1" >>"$work/wasm-probe.log"
    # Intentional splitting preserves compiler commands with options.
    if $1 --target=wasm32 -msimd128 -ffreestanding -nostdlib -Wl,--no-entry \
        -Wl,--export=Answer "$work/wasm-probe.c" -o "$work/wasm-probe.wasm" \
        >>"$work/wasm-probe.log" 2>&1; then
        wasmcc=$1
        return 0
    fi
    return 1
}
