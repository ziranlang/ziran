#!/bin/sh
# Small public procedures get a static inline copy in their module's C
# header, so another module's -O2 build inlines them without LTO. The defining
# module keeps the one external definition, which a procedure's address
# names. Results agree on C, Go and the portable VM from source and saved IR.
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
run_vm="$(dirname "$ziran")/zi2zx"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/api.zi" <<'ZI'
Fault :: struct { code: s32; }
Fail :: (code: s32) -> Fault { return Fault.{code} }
ZI
cat > "$work/lib.zi" <<'ZI'
using Api :: #import "api";
Range :: struct { low: s32; high: s32; fault: Fault; }
Clamp :: (range: *Range, value: s32) -> s32 {
    if value < range.low { range.fault = Fail(1); return range.low }
    if value > range.high { return range.high }
    return value
}
Halve :: (value: s32) -> s32 { return value >> 1 }
Twice :: (range: *Range, value: s32) -> s32 { return Clamp(range, Halve(value)) * 2 }
Hidden :: (value: s32) -> s32 { return Step(value) }
#scope_file
Step :: (value: s32) -> s32 { return value + 1 }
ZI
cat > "$work/app.zi" <<'ZI'
Lib :: #import "lib";
#program_export
Answer :: () -> s64 {
    range := Lib.Range.{0, 100, .{0}}
    total: s64 = 0
    for i: -10..300 { total += cast(s64) Lib.Clamp(*range, cast(s32) i) }
    if total != 100 * 101 / 2 + 200 * 100 || range.fault.code != 1 { return 1 }
    if Lib.Twice(*range, 90) != 90 || Lib.Hidden(1) != 2 { return 2 }
    return 42
}
ZI

"$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
for input in source saved; do
    if test "$input" = source; then
        module=$work/app.zi
        root=$work
    else
        module=$work/ir/app.zir
        root=$work/ir
    fi
    "$ziran" bundle --root "$root" --entry app:Answer \
        -o "$work/$input.zx" "$module"
    test "$("$ziran" run "$work/$input.zx")" = 42
    test "$("$run_vm" run --vm "$work/$input.zx")" = 42

    output=$work/c-$input
    "$ziran" build --target=c --root "$root" -o "$output" "$module"
    # A C host takes the address of an inlined procedure and calls it.
    cat > "$output/main.c" <<'C'
#include "app.h"
#include "lib.h"
int main(void) {
    int32_t (*halve)(int32_t) = lib_Halve;
    return Answer() == 42 && halve(84) == 42 && lib_Halve(84) == 42 ? 0 : 1;
}
C
    "${CC:-cc}" -std=c99 -pedantic-errors -Wall -O2 \
        -I"$repo/include" -I"$output" "$output"/*.c -o "$output/app"
    "$output/app"
    # Calls through a using import, nested inline calls, and the shared
    # helper guards all hold; only the procedure with a file-scope callee
    # stays out of line.
    for name in Clamp Halve Twice; do
        grep -q "^#define lib_${name}(...) lib_${name}_ziran_inline(__VA_ARGS__)" "$output/lib.h"
    done
    if grep -q 'lib_Hidden_ziran_inline\|lib_Step' "$output/lib.h"; then
        echo 'a procedure with a file-scope callee was copied into its header' >&2
        exit 1
    fi
    grep -q '^#define ZIRAN_LIB_H_SOURCE 1' "$output/lib.c"
    "${CC:-cc}" -std=c99 -O2 -I"$repo/include" -I"$output" -c "$output/app.c" -o "$work/app.o"
    nm -u "$work/app.o" > "$work/undefined"
    if grep -q 'lib_Clamp\|lib_Twice\|lib_Halve' "$work/undefined"; then
        echo 'a small cross-module call was not inlined' >&2
        exit 1
    fi
    grep -q 'lib_Hidden' "$work/undefined"
    # ZIRAN_NO_HEADER_INLINE keeps every call external, so link-time
    # interposition such as ld --wrap reaches it.
    "${CC:-cc}" -std=c99 -O2 -DZIRAN_NO_HEADER_INLINE -I"$repo/include" -I"$output" \
        -c "$output/app.c" -o "$work/app-external.o"
    nm -u "$work/app-external.o" > "$work/undefined"
    for name in Clamp Twice; do
        grep -q "lib_${name}\$" "$work/undefined"
    done
    # C++ hosts see only the declarations.
    if command -v "${CXX:-c++}" > /dev/null 2>&1; then
        printf '#include "lib.h"\nint main() { return lib_Halve(2) - 1; }\n' > "$work/host.cc"
        "${CXX:-c++}" -fsyntax-only -I"$repo/include" -I"$output" "$work/host.cc"
    fi

    output=$work/go-$input
    "$ziran" build --target=go --pkg main --root "$root" -o "$output" "$module"
    cat > "$output/main.go" <<'GO'
package main
func main() { if App_Answer() != 42 { panic("cross-module inline") } }
GO
    GO111MODULE=off go run "$output"/*.go
done
cmp "$work/source.zx" "$work/saved.zx"

# Editing an inlined body reaches its callers on the next cached build.
mkdir "$work/edit"
printf 'Lib :: #import "lib";\n#program_export\nAnswer :: () -> s64 { return cast(s64) Lib.Halve(168) }\n' > "$work/edit/app.zi"
for shift in 1 2; do
    printf 'Halve :: (value: s32) -> s32 { return value >> %s }\n' "$shift" > "$work/edit/lib.zi"
    "$ziran" build --target=c --exe --entry app:Answer --root "$work/edit" \
        -o "$work/edit/out" "$work/edit/app.zi"
    set +e; "$work/edit/out/app"; status=$?; set -e
    test "$status" = $((168 >> shift))
done

# Modules whose headers include each other: whichever header a C file reads
# first precedes the other's declarations, so neither copies a body that
# calls into the other, and both still build and agree.
mkdir "$work/cycle"
cat > "$work/cycle/left.zi" <<'ZI'
Right :: #import "right";
Wide :: (value: s32) -> s32 { return Right.Pad(value) + 1 }
Double :: (value: s32) -> s32 { return value * 2 }
ZI
cat > "$work/cycle/right.zi" <<'ZI'
Left :: #import "left";
Pad :: (value: s32) -> s32 { return value + 4 }
Weigh :: (value: s32) -> s32 { return Left.Double(value) + Pad(0) }
ZI
cat > "$work/cycle/app.zi" <<'ZI'
Left :: #import "left";
Right :: #import "right";
#program_export
Answer :: () -> s64 { return cast(s64) (Left.Wide(3) + Right.Weigh(3) + 24) }
ZI
"$ziran" build --target=c --root "$work/cycle" -o "$work/cycle/out" "$work/cycle/app.zi"
grep -q '#include "right.h"' "$work/cycle/out/left.h"
grep -q '#include "left.h"' "$work/cycle/out/right.h"
for name in left_Wide right_Weigh; do
    if grep -q "${name}_ziran_inline" "$work/cycle/out/"*.h; then
        echo "${name} calls into its include cycle but was copied into a header" >&2
        exit 1
    fi
done
for name in left_Double right_Pad; do
    grep -q "^#define ${name}(...) ${name}_ziran_inline(__VA_ARGS__)" "$work/cycle/out/"*.h
done
printf '#include "app.h"\nint main(void) { return Answer() == 42 ? 0 : 1; }\n' > "$work/cycle/out/main.c"
"${CC:-cc}" -std=c99 -pedantic-errors -Wall -Werror -O2 \
    -I"$repo/include" -I"$work/cycle/out" "$work/cycle/out"/*.c -o "$work/cycle/app"
"$work/cycle/app"
