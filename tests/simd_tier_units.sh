#!/bin/sh
# A module whose #target clones are large builds each tier's clones in a C
# file of its own, which a parallel C build compiles beside the module. The
# module source still defines its variables, exported procedures and
# file-scope helpers once, every tier gives the same result, and a small
# #target module or one with file-scope state stays one file.
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY

case "$(uname -m)" in x86_64|amd64) ;; *) echo "simd_tier_units: x86-64 only, skipped"; exit 0 ;; esac

{
    cat <<'ZI'
Simd :: #import "simd"

Calls: s64 = 0;

Step :: (v: s32x4, k: s32) -> s32x4 #inline {
    a := Simd.Add(v, Simd.Splat(s32x4, k))
    b := Simd.Sub(Simd.Mul(a, Simd.Splat(s32x4, 3)), Simd.Add(v, v))
    return Simd.Max(Simd.Min(b, Simd.Splat(s32x4, 100000)), Simd.Splat(s32x4, -100000))
}

#scope_file
Scale :: (x: s32) -> s32 { return x * 2 + 1 }
#scope_export

Mix :: (out: []s32, seed: s32) #target {
    Calls += 1
    v := Simd.Splat(s32x4, Scale(seed))
    v = Simd.Add(v, Simd.Make(s32x4, 1, 2, 3, 4))
ZI
    i=1
    while test "$i" -le 400; do
        echo "    v = Step(v, $((i % 17 - 8)))"
        i=$((i + 1))
    done
    cat <<'ZI'
    Simd.Store(out, 0, v)
}
ZI
} > "$work/wide.zi"
cat > "$work/small.zi" <<'ZI'
Simd :: #import "simd"
Twice :: (out: []s32, seed: s32) #target {
    Simd.Store(out, 0, Simd.Add(Simd.Splat(s32x4, seed), Simd.Splat(s32x4, seed)))
}
ZI
{
    sed 's/^Calls: s64 = 0;$/#scope_file\nHidden: s64 = 0;\n#scope_export/; s/Calls += 1/Hidden += 1/' "$work/wide.zi"
} > "$work/stateful.zi"
cat > "$work/app.zi" <<'ZI'
Wide :: #import "wide"
Small :: #import "small"
Stateful :: #import "stateful"

main :: () {
    a: [4]s32
    b: [4]s32
    c: [4]s32
    Wide.Mix(a[:], 5)
    Wide.Mix(a[:], a[1])
    Small.Twice(b[:], 21)
    Stateful.Mix(c[:], 5)
    print("% % % % % %\n", a[0], a[3], b[2], c[3], Wide.Calls, Wide.Calls + 1)
}
ZI

output=$work/c
"$ziran" build --target=c --entry app:main --root "$work" -o "$output" "$work/app.zi"
for tier in avx2 sse41 ssse3; do
    test -f "$output/wide.$tier.c" || { echo "wide.zi kept its $tier clones in wide.c" >&2; exit 1; }
    for module in small stateful; do
        if test -e "$output/$module.$tier.c"; then
            echo "$module.zi split its $tier clones" >&2
            exit 1
        fi
    done
done
printf '#include "app.h"\nint main(void) { app_main(); return 0; }\n' > "$output/main.c"

compilers=${CC:-cc}
for candidate in clang clang-21 clang-20 clang-19 clang-18; do
    if command -v "$candidate" >/dev/null 2>&1; then compilers="$compilers $candidate"; break; fi
done
expected=
for compiler in $compilers; do
    for flags in "-O2" "-O0" "-O2 -mavx2" "-O2 -DZIRAN_SIMD_SCALAR"; do
        # shellcheck disable=SC2086
        "$compiler" -std=c11 -Wall -Wextra -Werror -Wno-psabi $flags -I"$repo/include" \
            -I"$output" "$output"/*.c -o "$output/app" -lm
        for tier in 1 2 3 4; do
            result=$(ZIRAN_SIMD_TIER=$tier "$output/app")
            if test -z "$expected"; then expected=$result
            elif test "$result" != "$expected"; then
                echo "$compiler $flags tier $tier: $result, expected $expected" >&2
                exit 1
            fi
        done
    done
done
# The module variable stays one object the clones and the module share.
case "$expected" in *" 2 3") ;; *) echo "Wide.Calls counted $expected" >&2; exit 1 ;; esac

# Each unit defines only its tier's clone; the module source defines the rest.
"${CC:-cc}" -std=c11 -O2 -Wno-psabi -I"$repo/include" -I"$output" -c "$output/wide.c" -o "$work/wide.o"
"${CC:-cc}" -std=c11 -O2 -Wno-psabi -I"$repo/include" -I"$output" -c "$output/wide.avx2.c" -o "$work/avx2.o"
nm "$work/wide.o" | grep -q ' T wide_Mix$'
nm "$work/wide.o" | grep -q ' U wide_Mix_ziran_avx2$'
nm "$work/avx2.o" | grep -q ' T wide_Mix_ziran_avx2$'
if nm "$work/avx2.o" | grep -q ' [TDB] wide_\(Mix\|Calls\)$'; then
    echo "wide.avx2.c defined the module's own symbols" >&2
    exit 1
fi

# ziran's own C build compiles the units too.
"$ziran" build --target=c --exe --entry app:main --root "$work" -o "$work/exe" "$work/app.zi"
test "$("$work/exe/app")" = "$expected"
echo "simd_tier_units: split clones agree at every tier ($expected)"
