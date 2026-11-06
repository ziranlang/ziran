#!/bin/sh
set -eu
ziran=$(realpath "$1")
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$repo"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
mkdir -p build/tests
work=$(mktemp -d "$repo/build/tests/image-resize-fallible.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
export TMPDIR="$work" GOCACHE="$work/go-cache" ZIRAN_NATIVE_DIR="$work" ZIRAN_NATIVE_CACHE=0
run_vm() {
    result=$("$@") || { printf '%s\n' "$result"; return 1; }
    test "$result" = 0 || { printf '%s\n' "$result"; return 1; }
}
fixture=image_resize_fallible
"$ziran" check --root tests/spec --module-path std "tests/spec/$fixture.zi"
"$ziran" ir --root tests/spec --module-path std -o "$work/ir" "tests/spec/$fixture.zi"
for form in source saved; do
    if test "$form" = source; then
        input="$repo/tests/spec/$fixture.zi"
        set -- --root tests/spec --module-path std
    else
        input="$work/ir/$fixture.zir"
        set -- --root "$work/ir" --module-path "$work/ir"
    fi
    "$ziran" bundle "$@" --entry "$fixture:NativeMain" -o "$work/$form.zx" "$input"
    run_vm "$ziran" run "$work/$form.zx"
    run_vm "$(dirname "$ziran")/zi2zx" run --vm "$work/$form.zx"
    "$ziran" build "$@" --target=c --exe --entry "$fixture:NativeMain" -o "$work/$form-c" "$input"
    "$work/$form-c/$fixture"
    "$ziran" build "$@" --target=go --pkg main --exe --entry "$fixture:NativeMain" -o "$work/$form-go" "$input"
    GO111MODULE=off go run "$work/$form-go"/*.go
done
cmp "$work/source.zx" "$work/saved.zx"
"$ziran" build --root tests/spec --module-path std --target=c -o "$work/native" "tests/spec/$fixture.zi"
for sanitizer in plain address; do
    flags=''
    if test "$sanitizer" = address; then flags='-fno-omit-frame-pointer -fsanitize=address,undefined'; fi
    "${CC:-cc}" -std=c99 -g -O1 $flags -I"$work/native" -I"$repo/include" \
        "$work/native"/*.c "tests/spec/${fixture}_alloc.c" -Wl,--wrap=malloc -Wl,--wrap=calloc \
        -Wl,--wrap=realloc -Wl,--wrap=free -lm -o "$work/alloc-$sanitizer"
    ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 "$work/alloc-$sanitizer"
done
echo 'Fallible bicubic: independent pixel goldens, immutable input, reuse, overflow and every native allocation fault passed on source/saved C/Go/VM + ASan/UBSan/LSan'
