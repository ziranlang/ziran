#!/bin/sh
set -eu

ziran=$1
host_test=$2
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
include=${ZIRAN_INCLUDE:-"$root/include"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/host_buffer.zi" <<'ZI'
host_api :: #system_library "host_api";

Fill :: (values: []u8) -> s32 #foreign host_api;
ReplaceWord :: (values: []string) -> s32 #foreign host_api;

#program_export
Answer :: () -> s32 {
    values: [4]u8 = .[9, 1, 2, 9]
    empty: []u8 = values[:0]
    if Fill(empty) != 0 { return -1 }
    no_storage: [0]u8
    empty_fixed: []u8 = no_storage[:]
    if Fill(empty_fixed) != 0 { return -4 }
    middle: []u8 = values[1:3]
    if Fill(middle) != 2 || values[0] != 9 ||
        values[1] != 40 || values[2] != 2 || values[3] != 9 {
        return -2
    }
    words: [2]string = .["old", "second"]
    if ReplaceWord(words[:]) != 2 || words[0] != "new" ||
        words[1] != "second" { return -3 }
    return cast(s32)values[1] + cast(s32)values[2]
}
ZI

"$ziran" ir --root "$work" -o "$work/ir" "$work/host_buffer.zi"
for input in source saved; do
    if test "$input" = source; then
        source=$work/host_buffer.zi
        root=$work
    else
        source=$work/ir/host_buffer.zir
        root=$work/ir
    fi
    "$ziran" bundle --root "$root" --entry host_buffer:Answer \
        -o "$work/$input.zx" "$source"
    "$host_test" "$work/$input.zx"
    for target in c cpp go; do
        output=$work/$input-$target
        "$ziran" build --target="$target" --root "$root" \
            -o "$output" "$source"
        if test "$target" = c; then
            cat > "$output/main.c" <<'C'
#include "host_buffer.h"
int32_t Fill(Slice values) {
    if(values.length == 0) return 0;
    ((uint8_t *)values.data)[0] = 40;
    return values.length;
}
int32_t ReplaceWord(Slice values) {
    ((String *)values.data)[0] = StringView("new", 3);
    return values.length;
}
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            "${CC:-cc}" -std=c11 -I"$output" -I"$include" \
                "$output"/*.c -o "$output/app"
            "$output/app"
        elif test "$target" = cpp; then
            cat > "$output/main.cpp" <<'CPP'
#include "host_buffer.hpp"
extern "C" int32_t Fill(Slice values) {
    if(values.length == 0) return 0;
    static_cast<uint8_t *>(values.data)[0] = 40;
    return values.length;
}
extern "C" int32_t ReplaceWord(Slice values) {
    static_cast<String *>(values.data)[0] = StringView("new", 3);
    return values.length;
}
int main() { return Answer() == 42 ? 0 : 1; }
CPP
            "${CXX:-c++}" -std=c++17 -I"$output" \
                -I"$include" \
                "$output"/*.cpp -o "$output/app"
            "$output/app"
        else
            cat > "$output/host_buffer_test.go" <<'GO'
package ziran
import "testing"
type bufferHost struct{}
func (bufferHost) Fill(values []uint8) int32 {
    if len(values) == 0 { return 0 }
    values[0] = 40
    return int32(len(values))
}
func (bufferHost) ReplaceWord(values []string) int32 {
    values[0] = "new"
    return int32(len(values))
}
func TestHostSlice(t *testing.T) {
    SetHostBufferHost(bufferHost{})
    if HostBuffer_Answer() != 42 { t.Fatal("host slice") }
}
GO
            GO111MODULE=off go test "$output"/*.go
        fi
    done
done
cmp "$work/source.zx" "$work/saved.zx"

# A library can expose a foreign slice parameter without any Ziran function
# body. Its generated native header must still declare Slice.
cat > "$work/foreign_only.zi" <<'ZI'
host_api :: #system_library "host_api";
Fill :: (values: []u8) -> s32 #foreign host_api;
ZI
for target in c cpp; do
    output=$work/foreign-only-$target
    "$ziran" build --target="$target" --root "$work" \
        -o "$output" "$work/foreign_only.zi"
    if test "$target" = c; then
        printf '#include "foreign_only.h"\n' > "$work/foreign_only_smoke.c"
        "${CC:-cc}" -std=c11 -I"$output" \
            -I"$include" -c \
            "$work/foreign_only_smoke.c" -o "$output/foreign_only.o"
    else
        printf '#include "foreign_only.hpp"\n' > "$work/foreign_only_smoke.cpp"
        "${CXX:-c++}" -std=c++17 -I"$output" \
            -I"$include" -c \
            "$work/foreign_only_smoke.cpp" -o "$output/foreign_only.o"
    fi
done

cat > "$work/returned.zi" <<'ZI'
host_api :: #system_library "host_api";
MakeView :: () -> []s32 #foreign host_api;
#program_export
Answer :: () -> s32 {
    view: []s32 = MakeView()
    if view.count != 3 { return -1 }
    if view[0] != 10 || view[1] != 20 || view[2] != 30 { return -2 }
    return view[0] + view[1] + view[2]
}
ZI
"$ziran" ir --root "$work" -o "$work/ret-ir" "$work/returned.zi"
for input in source saved; do
    if test "$input" = source; then
        returned=$work/returned.zi
        returned_root=$work
    else
        returned=$work/ret-ir/returned.zir
        returned_root=$work/ret-ir
    fi
    "$ziran" bundle --root "$returned_root" --entry returned:Answer \
        -o "$work/returned-$input.zx" "$returned"
    "$host_test" "$work/returned-$input.zx" return_view
    "$host_test" "$work/returned-$input.zx" return_empty
    "$host_test" "$work/returned-$input.zx" return_huge
done
cmp "$work/returned-source.zx" "$work/returned-saved.zx"

cat > "$work/record_slices.zi" <<'ZI'
Point :: struct { x: s32; y: s32; }
host_api :: #system_library "host_api";
Mutate :: (points: []Point) -> s32 #foreign host_api;
Make :: () -> []Point #foreign host_api;
#program_export
Answer :: () -> s32 {
    storage: [2]Point = Point.[Point.{.x = 1, .y = 2}, Point.{.x = 3, .y = 4}]
    view: []Point = storage[:]
    if Mutate(view) != 2 || storage[0].x != 40 || storage[1].x != 3 { return 0 }
    returned: []Point = Make()
    if returned.count != 2 || returned[0].x != 1 { return 0 }
    return storage[0].x + returned[1].y
}
ZI
"$ziran" ir --root "$work" -o "$work/record-ir" "$work/record_slices.zi"
for input in source saved; do
    if test "$input" = source; then
        record=$work/record_slices.zi
        record_root=$work
    else
        record=$work/record-ir/record_slices.zir
        record_root=$work/record-ir
    fi
    "$ziran" bundle --root "$record_root" --entry record_slices:Answer \
        -o "$work/record-$input.zx" "$record"
    "$host_test" "$work/record-$input.zx" records
    "$host_test" "$work/record-$input.zx" records_bad
    for target in c cpp go; do
        output=$work/record-$input-$target
        "$ziran" build --target="$target" --root "$record_root" \
            -o "$output" "$record"
        if test "$target" = c; then
            cat > "$output/main.c" <<'C'
#include "record_slices.h"
int32_t Mutate(Slice points) {
    if(points.length != 2) return 0;
    ((Point *)points.data)[0].x = 40;
    return 2;
}
Slice Make(void) {
    static Point points[] = {{1, 0}, {0, 2}};
    return (Slice){points, 2};
}
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            "${CC:-cc}" -std=c11 -Wall -Werror -I"$output" -I"$include" \
                "$output"/*.c -o "$output/app"
            "$output/app"
        elif test "$target" = cpp; then
            cat > "$output/main.cpp" <<'CPP'
#include "record_slices.hpp"
extern "C" int32_t Mutate(Slice points) {
    if(points.length != 2) return 0;
    static_cast<Point *>(points.data)[0].x = 40;
    return 2;
}
extern "C" Slice Make(void) {
    static Point points[] = {{1, 0}, {0, 2}};
    return Slice{points, 2};
}
int main() { return Answer() == 42 ? 0 : 1; }
CPP
            "${CXX:-c++}" -std=c++17 -Wall -Werror -I"$output" \
                -I"$include" "$output"/*.cpp -o "$output/app"
            "$output/app"
        else
            cat > "$output/record_slices_test.go" <<'GO'
package ziran
import "testing"
type recordHost struct{}
func (recordHost) Mutate(points []Point) int32 {
    if len(points) != 2 { return 0 }
    points[0].X = 40
    return 2
}
func (recordHost) Make() []Point { return []Point{{X: 1}, {Y: 2}} }
func TestRecordSlice(t *testing.T) {
    SetRecordSlicesHost(recordHost{})
    if RecordSlices_Answer() != 42 { t.Fatal("record slice") }
}
GO
            GO111MODULE=off go test "$output"/*.go
        fi
    done
done
cmp "$work/record-source.zx" "$work/record-saved.zx"

cat > "$work/bad_return.zi" <<'ZI'
host_api :: #system_library "host_api";
Borrow :: () -> [][]u8 #foreign host_api;
#program_export
Answer :: () -> s32 {
    return Borrow().count
}
ZI
if "$ziran" check --root "$work" "$work/bad_return.zi" \
    2> "$work/bad_return.err"; then
    echo 'a nested host slice return was accepted' >&2
    exit 1
fi
grep -Fq 'host slice returns need a supported element type' "$work/bad_return.err"

cat > "$work/overlap.zi" <<'ZI'
host_api :: #system_library "host_api";
Touch :: (left: []u8, right: []u8) -> s32 #foreign host_api;
#program_export
Answer :: () -> s32 {
    values: [3]u8 = .[1, 2, 3]
    return Touch(values[:], values[1:])
}
ZI
"$ziran" bundle --root "$work" --entry overlap:Answer \
    -o "$work/overlap.zx" "$work/overlap.zi"
"$host_test" "$work/overlap.zx" overlap
