#!/bin/sh
# std/simd however it is imported: `#import "std/simd"` and the short
# `#import "simd"`, from an app and from a package module, with the
# standard library given as --module-path std=DIR (as package tests pass
# it), as a plain std directory and from the compiler's own. Each build runs
# in C, Go, a bundle and the portable VM.
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

mkdir -p "$work/kit" "$work/app"
cat > "$work/kit/lanes.zi" <<'ZI'
Vector :: #import "std/simd"

#scope_export
Lanes :: (base: s16) -> s32 {
    v := Vector.Add(Vector.Splat(s16x8, base), Vector.Make(s16x8, 1, 2, 3, 4, 5, 6, 7, 8))
    return Vector.Lane(Vector.MulAddPairs(v, Vector.Splat(s16x8, 3)), 3)
}
ZI
cat > "$work/kit/short.zi" <<'ZI'
Simd :: #import "simd"

#scope_export
Short :: (base: u8) -> u8 {
    return Simd.Lane(Simd.AddSat(Simd.Splat(u8x16, base), Simd.Splat(u8x16, 200)), 15)
}
ZI
cat > "$work/app/app.zi" <<'ZI'
#import "kit/lanes"
#import "kit/short"
Simd :: #import "simd"
Wide :: #import "std/simd"

main :: () {
    a := Simd.Lane(Simd.Add(Simd.Splat(u32x4, 40), Simd.Splat(u32x4, 2)), 0)
    b := Wide.Lane(Wide.Sub(Wide.Splat(s32x8, 10), Wide.Splat(s32x8, 3)), 7)
    print("% % % %\n", a, b, Lanes(10), Short(100))
}
ZI
expected="42 7 105 255"

vm_lib=${ZIRAN_VM_LIB:-$repo/build/libziran-vm.a}
if test -f "$vm_lib"; then
    cat > "$work/vm-run.c" <<'C'
#include "ziran_host.h"
int main(int argc, char **argv) {
    Bundle *bundle = argc == 2 ? BundleOpen(argv[1]) : 0;
    long long result = 0;
    int has_result = 0;
    int ok = bundle != 0 && BundleRun(bundle, 0, 0, &result, &has_result);
    BundleClose(bundle);
    return ok ? 0 : 1;
}
C
    "${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -I"$repo/include" "$work/vm-run.c" "$vm_lib" -lm -pthread \
        -o "$work/vm-run"
fi

check() {
    label=$1 result=$2
    test "$result" = "$expected" || { echo "$label: $result, expected $expected" >&2; exit 1; }
}

mkdir -p "$work/std"
cp "$repo"/std/*.zi "$work/std/"
# --module-path std=DIR, a directory named std, and the compiler's own std.
for standard in named plain own; do
    case $standard in
        named) set -- --module-path "std=$repo/std" ;;
        plain) set -- --module-path "$work/std" ;;
        own) set -- ;;
    esac
    out=$work/$standard
    "$ziran" build --target=c --entry app:main --root "$work/app" "$@" \
        --module-path "kit=$work/kit" -o "$out-c" "$work/app/app.zi"
    printf '#include "app.h"\nint main(void) { app_main(); return 0; }\n' > "$out-c/main.c"
    "${CC:-cc}" -std=c11 -Wno-psabi -I"$out-c" "$out-c"/*.c -o "$out-c/app" -lm
    check "C ($standard std)" "$("$out-c/app")"
    if command -v go >/dev/null 2>&1; then
        "$ziran" build --target=go --pkg main --entry app:main --exe --root "$work/app" "$@" \
            --module-path "kit=$work/kit" -o "$out-go" "$work/app/app.zi"
        check "Go ($standard std)" "$(cd "$out-go" && GO111MODULE=off go run ./*.go)"
    fi
    "$ziran" bundle --root "$work/app" --entry app:main "$@" \
        --module-path "kit=$work/kit" -o "$out.zx" "$work/app/app.zi"
    check "zx ($standard std)" "$(ZIRAN_NATIVE_CACHE="$work/cache" "$ziran" run "$out.zx")"
    if test -x "$work/vm-run"; then
        check "VM ($standard std)" "$("$work/vm-run" "$out.zx")"
    fi
done
echo "std/simd imports: ok"
