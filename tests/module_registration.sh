#!/bin/sh
set -eu

# A module registers itself from a global initializer. Importing the module is
# what includes it: its initializer runs before the entry, and a program that
# does not import it carries neither the module nor what only it reaches.
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$repo/build/tests"
work=$(mktemp -d "$repo/build/tests/module-registration-XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/host.zi" <<'ZI'
Draw :: #type (value: s32) -> s32;
draw: Draw;

Register :: (procedure: Draw) -> bool {
    draw = procedure
    return true
}

// -1 when no module registered a procedure.
Call :: (value: s32) -> s32 {
    if draw == null { return -1 }
    return draw(value)
}
ZI
cat > "$work/heavy.zi" <<'ZI'
Scale :: () -> s32 { return 2 }
ZI
cat > "$work/feature.zi" <<'ZI'
Host :: #import "host";
Heavy :: #import "heavy";

Double :: (value: s32) -> s32 { return value * Heavy.Scale() }

registered: bool = Host.Register(Double);
ZI
# Nothing in feature is named: the import alone registers it.
cat > "$work/with_feature.zi" <<'ZI'
Host :: #import "host";
#import "feature"

#program_export
Answer :: () -> s32 { return Host.Call(21) }

main :: () -> s32 {
    print("%\n", Answer())
    return 0
}
ZI
cat > "$work/without_feature.zi" <<'ZI'
Host :: #import "host";

#program_export
Answer :: () -> s32 { return Host.Call(21) }

main :: () -> s32 {
    print("%\n", Answer())
    return 0
}
ZI

for program in with_feature without_feature; do
    case "$program" in
        with_feature) expected=42 ;;
        without_feature) expected=-1 ;;
    esac
    "$ziran" ir --root "$work" -o "$work/$program-ir" "$work/$program.zi"
    for input in source saved; do
        case "$input" in
            source) root=$work; file=$work/$program.zi ;;
            saved) root=$work/$program-ir; file=$work/$program-ir/$program.zir ;;
        esac
        out="$work/$program-$input-c"
        "$ziran" build --target=c --root "$root" --entry "$program:Answer" -o "$out" "$file"
        cat > "$out/main.c" <<C
#include "$program.h"
int main(void) { return Answer() == $expected ? 0 : 1; }
C
        "${CC:-cc}" -std=c11 -I"$repo/include" -I"$out" "$out"/*.c -o "$out/program"
        "$out/program"
        # The feature and its dependency exist exactly when the import does.
        case "$program" in
            with_feature) test -f "$out/feature.c"; test -f "$out/heavy.c" ;;
            without_feature) test ! -e "$out/feature.c"; test ! -e "$out/heavy.c" ;;
        esac
        test "$("$ziran" run --root "$root" --entry "$program:Answer" "$file")" = "$expected"
    done
    # Go runs the same initializer from its package initialization.
    go_out="$work/$program-go"
    "$ziran" build --target=go --pkg main --exe --root "$work" --entry "$program:main" \
        -o "$go_out" "$work/$program.zi"
    (cd "$go_out" && GO111MODULE=off go build -o program .)
    test "$("$go_out/program")" = "$expected"
    case "$program" in
        with_feature) test -f "$go_out/feature.go" ;;
        without_feature) test ! -e "$go_out/feature.go" ;;
    esac
done
# A registered procedure may call a host later: Go binds the host interface
# before the entry, so storing the procedure during startup is accepted. A
# host call made while an initializer runs is still refused.
cat > "$work/host_feature.zi" <<'ZI'
Host :: #import "host";
host_api :: #system_library "host_api";
HostScale :: () -> s32 #foreign host_api;

Scaled :: (value: s32) -> s32 { return value * HostScale() }

registered: bool = Host.Register(Scaled);
ZI
cat > "$work/with_host_feature.zi" <<'ZI'
Host :: #import "host";
#import "host_feature"

main :: () -> s32 {
    print("%\n", ifx Host.draw != null then 1 else 0)
    return 0
}
ZI
"$ziran" build --target=go --pkg main --exe --root "$work" --entry with_host_feature:main \
    -o "$work/host-feature-go" "$work/with_host_feature.zi"
(cd "$work/host-feature-go" && GO111MODULE=off go build -o program .)
test "$("$work/host-feature-go/program")" = 1
cat > "$work/host_startup.zi" <<'ZI'
host_api :: #system_library "host_api";
HostScale :: () -> s32 #foreign host_api;

Startup :: () -> s32 { return HostScale() }

scale: s32 = Startup();

main :: () -> s32 {
    print("%\n", scale)
    return 0
}
ZI
if "$ziran" build --target=go --pkg main --exe --root "$work" --entry host_startup:main \
    -o "$work/host-startup-go" "$work/host_startup.zi" 2> "$work/host-startup.err"; then
    echo 'Go accepted a host call made during module startup' >&2
    exit 1
fi
grep -Fq 'Go global startup cannot call a host before binding' "$work/host-startup.err"
# A closed program's files carry only what the program uses. A module it
# imports but never reaches has no files, and a header declares only what
# another file names: shapes keeps Pair and Sum, which only it reads, and the
# inline copy of Sum, which no other module calls, in no header. A runtime
# header keeps just the helpers the program calls.
cat > "$work/shapes.zi" <<'ZI'
Point :: struct { x: s32; y: s32; }
Pair :: struct { a: s32; b: s32; }
Sum :: (pair: Pair) -> s32 { return pair.a + pair.b }
Total :: (point: Point) -> s32 #cold {
    pair: Pair
    pair.a = point.x
    pair.b = point.y
    return Sum(pair)
}
Twice :: (value: s32) -> s32 { return value * 2 }
ZI
cat > "$work/idle.zi" <<'ZI'
Idle :: () -> s32 { return 7 }
ZI
cat > "$work/closed.zi" <<'ZI'
Shapes :: #import "shapes";
#import "idle"

main :: () -> s32 {
    point: Shapes.Point
    point.x = 20
    point.y = 22
    values: [2]s32
    values[point.x % 2] = Shapes.Total(point)
    print("%\n", values[0])
    return 0
}
ZI
out="$work/closed-c"
"$ziran" build --target=c --exe --root "$work" --entry closed:main -o "$out" "$work/closed.zi"
test "$("$out/closed")" = 42
test ! -e "$out/idle.c"
test ! -e "$out/idle.h"
grep -q 'struct Point' "$out/shapes.h"
grep -q 'shapes_Total' "$out/shapes.h"
if grep -Eq 'Pair|shapes_Sum|shapes_Twice|ziran_inline' "$out/shapes.h"; then
    echo 'shapes.h declares what only shapes.c uses' >&2
    exit 1
fi
grep -q 'struct Pair' "$out/shapes.c"
grep -q 'shapes_Sum' "$out/shapes.c"
if grep -q 'shapes_Twice' "$out/shapes.c"; then
    echo 'shapes.c defines a procedure the program never reaches' >&2
    exit 1
fi
grep -q 'BoundsCheck' "$out/zir_bounds.h"
if grep -q 'EmptyArrayIndex\|/\*@' "$out/zir_bounds.h"; then
    echo 'zir_bounds.h carries a helper the program never calls' >&2
    exit 1
fi
echo 'An imported module registers itself from its initializer; unimported modules are absent'
