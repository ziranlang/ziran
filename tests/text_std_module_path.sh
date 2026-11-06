#!/bin/sh
# std/vec's Text is the same type whether std comes with the compiler or is
# named with --module-path std=DIR, which gives its modules the scoped
# p<hash>_NAME identity. Standard producers that return Text, such as
# JsonWrite, Float64Repr, TextLower and ReadStandardInput, check, and their
# results pass to string parameters. Source and saved IR, C, native and VM
# bundles agree with the default std.
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
zi2zx=$(dirname -- "$ziran")/zi2zx
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
mkdir "$work/src"
cat > "$work/src/chain.zi" <<'ZI'
Json :: #import "std/json_document"
Tools :: #import "std/text_tools"
Floats :: #import "std/float_text"

Width :: (s: string) -> s32 { return cast(s32)s.count }

// Text producers, a Text argument to a string parameter and a Text local.
#program_export
Chain :: () -> s32 {
    document: Json.JsonDocument
    object := Json.JsonNewObject(*document); document.root = object
    unused Json.JsonSet(*document, object, "k", Json.JsonNewString(*document, "v"))
    json := Json.JsonWrite(*document, object, Json.JsonPythonStyle(2))
    if Width(json) < 8 { return 1 }
    lower := Tools.TextLower(Floats.Float64Repr(1.5))
    if lower != "1.5" { return 2 }
    if Tools.TextReplace(Tools.TextLower("AB"), "b", "c") != "ac" { return 3 }
    return 30 + Width(lower) * 3
}
ZI
cat > "$work/src/main.zi" <<'ZI'
#import "chain"
Stdio :: #import "std/stdio_linux"
main :: () -> s32 {
    input := Stdio.ReadStandardInput()
    return Chain() + Width(input)
}
ZI
for std in default named; do
    set -- --root "$work/src"
    if test "$std" = named; then set -- "$@" --module-path "std=$repo/std"; fi
    "$ziran" check "$@" "$work/src/main.zi"
    "$ziran" ir "$@" -o "$work/$std-ir" "$work/src/main.zi"
    for kind in source saved; do
        input=$work/src/main.zi
        if test "$kind" = saved; then
            input=$work/$std-ir/main.zir
            set -- --root "$work/$std-ir" --module-path "$work/$std-ir"
        fi
        "$ziran" build --target=c --exe --entry main:main "$@" -o "$work/$std-$kind-c" "$input"
        status=0; printf 'xyz' | "$work/$std-$kind-c/main" || status=$?
        test "$status" = 42
        chain=$work/src/chain.zi
        if test "$kind" = saved; then chain=$work/$std-ir/chain.zir; fi
        "$ziran" bundle --entry chain:Chain "$@" -o "$work/$std-$kind.zx" "$chain"
        test "$("$zi2zx" run "$work/$std-$kind.zx")" = 39
        test "$("$zi2zx" run --vm "$work/$std-$kind.zx")" = 39
    done
done

# std/simd keeps its operations under the same scoped identity, saved IR too.
mkdir "$work/simd"
cat > "$work/simd/lanes.zi" <<'ZI'
Vector :: #import "std/simd"
main :: () -> s32 {
    v := Vector.Add(Vector.Splat(s16x8, 10), Vector.Make(s16x8, 1, 2, 3, 4, 5, 6, 7, 8))
    return Vector.Lane(Vector.MulAddPairs(v, Vector.Splat(s16x8, 1)), 0)
}
ZI
"$ziran" ir --root "$work/simd" --module-path "std=$repo/std" -o "$work/simd-ir" "$work/simd/lanes.zi"
for kind in source saved; do
    set -- --root "$work/simd" --module-path "std=$repo/std"; input=$work/simd/lanes.zi
    if test "$kind" = saved; then
        set -- --root "$work/simd-ir" --module-path "$work/simd-ir"; input=$work/simd-ir/lanes.zir
    fi
    "$ziran" build --target=c --exe --entry lanes:main "$@" -o "$work/simd-$kind-c" "$input"
    status=0; "$work/simd-$kind-c/lanes" || status=$?
    test "$status" = 23
    "$ziran" bundle --entry lanes:main "$@" -o "$work/simd-$kind.zx" "$input"
    test "$("$zi2zx" run --vm "$work/simd-$kind.zx")" = 23
done
echo 'std/vec Text and std/simd under --module-path std=DIR: std producers check; C, saved IR and native and VM bundles agree with the default std'
