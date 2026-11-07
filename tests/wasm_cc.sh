#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$repo/build/tests"
work=$(mktemp -d "$repo/build/tests/wasm-cc.XXXXXX")
trap '/bin/rm -rf "$work"' EXIT
. "$repo/tests/lib/wasm_cc.sh"
mkdir "$work/bin"
cat >"$work/bin/clang" <<'EOF'
#!/bin/sh
echo 'missing wasm-ld' >&2
exit 1
EOF
cat >"$work/bin/clang-19" <<'EOF'
#!/bin/sh
# The selection must test a link, not just --version or object emission.
case " $* " in
    *' --target=wasm32 -msimd128 -ffreestanding -nostdlib -Wl,--no-entry -Wl,--export=Answer '*) exit 0 ;;
    *) exit 1 ;;
esac
EOF
chmod +x "$work/bin/clang" "$work/bin/clang-19"
saved_path=$PATH
PATH=$work/bin
unset ZIRAN_WASM_CC
wasm_cc_select
test "$wasmcc" = clang-19
# An explicit compiler command, including flags, must be respected.
ZIRAN_WASM_CC='clang-19 -fuse-ld=/chosen/wasm-ld'
wasm_cc_select
test "$wasmcc" = "$ZIRAN_WASM_CC"
# A broken explicit override must never silently fall back to another tool.
ZIRAN_WASM_CC=clang
if wasm_cc_select; then
    echo 'wasm-cc: ignored failing explicit compiler' >&2
    exit 1
fi
unset ZIRAN_WASM_CC
PATH=$saved_path
rm "$work/bin/clang-19"
PATH=$work/bin
if wasm_cc_select; then
    echo 'wasm-cc: accepted a compiler without a working wasm link' >&2
    exit 1
fi
PATH=$saved_path
echo 'wasm compiler discovery: versioned fallback, options, explicit failure and unavailable tools passed'
