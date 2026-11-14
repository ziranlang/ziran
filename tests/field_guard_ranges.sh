#!/bin/sh
# Generated C leaves out the bounds check of an index a guard on a
# parameter's integer field proves: through a pointer, a field such as
# frame.count holds one value until a call or a store outside the function's
# own locals may change it, so a guard bounds a loop that reads the field
# into its limit even when the loop body stores through a slice. A record
# parameter's fields change only with the parameter. Any store through a
# pointer, a cast pointer, a byte view, a slice element or a global, a call,
# even in the condition of an else if before the read, a store in the loop
# before the next read, or a guard on a sum that may wrap keeps the check, and those reads still abort on C, Go, native and the
# instruction engine from source and saved IR. Cast pointers run on C only.
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
Frame :: struct { count: s64; width: s32; above: [72]s32; }
Shape :: struct { width: s32; height: s32; }
shared: Frame;
// The loop's limit reads the count once, before the copy's stores.
Copy :: (frame: *Frame, source: []u16, destination: []u16) -> s64 {
    if frame == null || source.count != frame.count || destination.count != frame.count { return -1 }
    for i: 0..frame.count - 1 { destination[i] = source[i] }
    return frame.count
}
// Nothing in the loop stores outside its locals, so the width holds.
Sum :: (frame: *Frame, values: []s32) -> s64 {
    if frame.width < 1 || frame.width > 64 || frame.width > values.count { return -1 }
    total: s64 = 0
    for j: 0..frame.width - 1 { total += cast(s64)values[j] + cast(s64)frame.above[j + 4] }
    return total
}
// A record parameter's fields change only with the parameter.
Area :: (shape: Shape, view: []u16) -> s64 {
    if shape.width < 1 || shape.width > 64 || shape.height < 1 || shape.height > 64 ||
        view.count < cast(s64)shape.width * shape.height {
        return -1
    }
    for i: 0..shape.height - 1 { for j: 0..shape.width - 1 { view[i * shape.width + j] = 1 } }
    return cast(s64)shape.width * shape.height
}
// Stores to the function's own locals cannot reach the frame.
Local :: (frame: *Frame, view: []s64) -> s64 {
    if frame.count < 0 || frame.count >= view.count { return -1 }
    t: [4]s64
    t[1] = 5
    return view[frame.count] + t[1]
}
Grow :: (frame: *Frame) -> bool { frame.count += 1; return false }
// A store through another pointer may reach the frame.
Stored :: (frame: *Frame, other: *Frame, view: []s64) -> s64 {
    if frame.count < 0 || frame.count >= view.count { return -1 }
    other.count = frame.count + 1
    return view[frame.count]
}
// So may a call.
Called :: (frame: *Frame, view: []s64) -> s64 {
    if frame.count < 0 || frame.count >= view.count { return -1 }
    Grow(frame)
    return view[frame.count]
}
// The store at the end of one iteration comes before the next read.
Looped :: (frame: *Frame, view: []s64) -> s64 {
    if frame.count < 0 || frame.count >= view.count { return -1 }
    total: s64 = 0
    for k: 0..1 { total += view[frame.count]; frame.count += 1 }
    return total
}
// The frame may be the global.
Global :: (frame: *Frame, view: []s64) -> s64 {
    if frame.count < 0 || frame.count >= view.count { return -1 }
    shared.count = 4
    return view[frame.count]
}
// Or an element of a slice.
Element :: (frame: *Frame, frames: []Frame, view: []s64) -> s64 {
    if frame.count < 0 || frame.count >= view.count || frames.count < 1 { return -1 }
    frames[0].count = 4
    return view[frame.count]
}
// A call in the guard runs after the guard read the count.
InGuard :: (frame: *Frame, view: []s64) -> s64 {
    if frame.count < 0 || frame.count >= view.count || Grow(frame) { return -1 }
    return view[frame.count]
}
// The else if's call runs after the first branch read the count.
Branch :: (frame: *Frame, view: []s64) -> s64 {
    if frame.count < 0 || frame.count >= view.count { return -1 }
    else if Grow(frame) { return -2 }
    else { return view[frame.count] }
}
// The sum may wrap past the s64 range, so the guard bounds neither count.
Wrapped :: (first: *Frame, second: *Frame, view: []s64) -> s64 {
    count := first.count + 1 + second.count
    if first.count < 0 || second.count < 0 || count > view.count { return -1 }
    return view[first.count]
}
#program_export
Answer :: () -> s64 {
    frame: Frame
    frame.count = 6
    frame.width = 3
    for i: 0..71 { frame.above[i] = cast(s32)i }
    source: [6]u16
    destination: [6]u16
    for i: 0..5 { source[i] = cast(u16)(i * 3) }
    values: [3]s32
    values[0] = 10; values[1] = 20; values[2] = 30
    view: [12]u16
    small: [4]s64
    small[2] = 9
    if Copy(*frame, source[0:6], destination[0:6]) != 6 || destination[5] != 15 { return 1 }
    if Copy(*frame, source[0:6], destination[0:5]) != -1 { return 2 }
    if Sum(*frame, values[0:3]) != 60 + 4 + 5 + 6 { return 3 }
    if Sum(*frame, values[0:2]) != -1 { return 4 }
    if Area(Shape.{4, 3}, view[0:12]) != 12 || view[11] != 1 { return 5 }
    if Area(Shape.{4, 4}, view[0:12]) != -1 { return 6 }
    frame.count = 2
    if Local(*frame, small[0:4]) != 14 { return 7 }
    return 42
}
#program_export
Fault :: (which: s64) -> s64 {
    view: [4]s64
    frame: Frame
    frame.count = 3
    frames: [1]Frame
    frames[0].count = 3
    shared.count = 3
    big: Frame
    big.count = 6917529027641081856
    if which == 1 { return Stored(*frame, *frame, view[0:4]) }
    if which == 2 { return Called(*frame, view[0:4]) }
    if which == 3 { return Looped(*frame, view[0:4]) }
    if which == 4 { return Global(*shared, view[0:4]) }
    if which == 5 { return Element(*frames[0], frames[0:1], view[0:4]) }
    if which == 6 { return InGuard(*frame, view[0:4]) }
    if which == 7 { return Wrapped(*big, *big, view[0:4]) }
    if which == 8 { return Branch(*frame, view[0:4]) }
    return 0
}
ZI
faults="Stored Called Looped Global Element InGuard Wrapped Branch"
which=1
for name in $faults; do
    printf '#program_export\nRun%s :: () -> s64 { return Fault(%d) }\n' "$name" "$which" >> "$work/app.zi"
    which=$((which + 1))
done

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
    for name in $faults; do
        "$ziran" bundle --root "$root" --entry "app:Run$name" \
            -o "$work/fault.zx" "$module"
        if timeout 60 "$ziran" run "$work/fault.zx" > /dev/null 2>&1; then
            echo "$name read outside its slice in a bundle" >&2
            exit 1
        fi
    done

    output=$work/c-$input
    "$ziran" build --target=c --root "$root" -o "$output" "$module"
    cat > "$output/main.c" <<'C'
#include <stdlib.h>
#include "app.h"
int main(int argc, char **argv) {
    if(argc > 1) return (int)Fault(atoi(argv[1]));
    return Answer() == 42 ? 0 : 1;
}
C
    "${CC:-cc}" -std=c99 -pedantic-errors -Wall -O2 \
        -I"$repo/include" -I"$output" "$output"/*.c -o "$output/app"
    "$output/app"
    which=1
    for name in $faults; do
        if "$output/app" "$which" 2> /dev/null; then
            echo "$name read outside its slice in C" >&2
            exit 1
        fi
        which=$((which + 1))
    done
    for name in Copy Sum Area Local; do
        sed -n "/^app_$name(/,/^}/p" "$output/app.c" > "$work/proven.c"
        test -s "$work/proven.c"
        if grep -q 'ZIRAN_INDEX\|SliceIndex' "$work/proven.c"; then
            echo "a proven index in $name kept its bounds check" >&2
            grep 'ZIRAN_INDEX\|SliceIndex' "$work/proven.c" >&2
            exit 1
        fi
    done
    for name in $faults; do
        sed -n "/^app_$name(/,/^}/p" "$output/app.c" > "$work/kept.c"
        if ! grep -q 'SliceIndex' "$work/kept.c"; then
            echo "$name lost a bounds check it needs" >&2
            exit 1
        fi
    done

    output=$work/go-$input
    "$ziran" build --target=go --pkg main --root "$root" -o "$output" "$module"
    cat > "$output/main.go" <<'GO'
package main
import ("os"; "strconv")
func main() {
    if len(os.Args) > 1 {
        which, _ := strconv.Atoi(os.Args[1])
        App_Fault(int64(which))
        return
    }
    if App_Answer() != 42 { panic("field guard ranges") }
}
GO
    (cd "$output" && GO111MODULE=off go build -o app .)
    "$output/app"
    which=1
    for name in $faults; do
        if "$output/app" "$which" 2> /dev/null; then
            echo "$name read outside its slice in Go" >&2
            exit 1
        fi
        which=$((which + 1))
    done
done
cmp "$work/source.zx" "$work/saved.zx"

# Cast pointers reach the count too: an unsigned view and a byte view.
mkdir "$work/cast"
cat > "$work/cast/cast.zi" <<'ZI'
Frame :: struct { count: s64; width: s32; }
Unsigned :: (frame: *Frame, view: []s64) -> s64 {
    if frame.count < 0 || frame.count >= view.count { return -1 }
    bits := cast(*u64)frame
    bits.* = 4
    return view[frame.count]
}
Bytes :: (frame: *Frame, view: []s64) -> s64 {
    if frame.count < 0 || frame.count >= view.count { return -1 }
    bytes := cast(*u8)frame
    bytes[0] = 4
    return view[frame.count]
}
// Without the store the guard proves the read.
Kept :: (frame: *Frame, view: []s64) -> s64 {
    if frame.count < 0 || frame.count >= view.count { return -1 }
    return view[frame.count]
}
#program_export
Fault :: (which: s64) -> s64 {
    view: [4]s64
    frame: Frame
    frame.count = 3
    if which == 1 { return Unsigned(*frame, view[0:4]) }
    if which == 2 { return Bytes(*frame, view[0:4]) }
    return Kept(*frame, view[0:4])
}
ZI
output=$work/c-cast
"$ziran" build --target=c --root "$work/cast" -o "$output" "$work/cast/cast.zi"
cat > "$output/main.c" <<'C'
#include <stdlib.h>
#include "cast.h"
int main(int argc, char **argv) { return (int)Fault(atoi(argv[1])); }
C
"${CC:-cc}" -std=c99 -pedantic-errors -Wall -O2 \
    -I"$repo/include" -I"$output" "$output"/*.c -o "$output/app"
"$output/app" 0
for which in 1 2; do
    if "$output/app" "$which" 2> /dev/null; then
        echo "cast store $which read outside its slice in C" >&2
        exit 1
    fi
done
for name in Unsigned Bytes; do
    sed -n "/^cast_$name(/,/^}/p" "$output/cast.c" > "$work/kept.c"
    if ! grep -q 'SliceIndex' "$work/kept.c"; then
        echo "$name lost a bounds check it needs" >&2
        exit 1
    fi
done
sed -n "/^cast_Kept(/,/^}/p" "$output/cast.c" > "$work/proven.c"
test -s "$work/proven.c"
if grep -q 'SliceIndex' "$work/proven.c"; then
    echo "a proven index in Kept kept its bounds check" >&2
    exit 1
fi
