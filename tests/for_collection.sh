#!/bin/sh
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/collections.zi" <<'ZI'
calls: s32;
GetView :: (values: []s32) -> []s32 {
    calls += 1
    return values
}
#program_export
Answer :: () -> s32 {
    calls = 0
    values: [4]s32 = .[1, 2, 3, 4]
    total: s32 = 0
    for values { total += it }
    for item, index: values { total += item * cast(s32) index }
    for < item, index: values { total += item * cast(s32) index }
    range_total: s32 = 0
    for index: 0..values.count-1 { range_total += values[index] }
    if range_total != 10 { return 0 }
    view: []s32 = values[1:3]
    text: string = "abc"
    if values.count != 4 || view.count != 2 || text.count != 3 { return 0 }
    for GetView(view) { total += it + cast(s32) it_index }
    if calls != 1 { return 0 }
    reverse: s32 = 0
    for < values { reverse = reverse * 10 + it }
    if reverse != 4321 { return 0 }
    for item: values { item = 99 }
    if values[0] != 1 { return 0 }
    for value, index: view {
        if index == 0 { continue }
        total += value
    }
    for outer, outer_index: values {
        for inner, inner_index: view {
            if inner_index == 0 { continue }
            total += outer * inner
        }
    }
    for value: view {
        defer { total += value }
        if value == 2 { continue }
        break
    }
    for values[1:3] { total += it }
    empty: []s32
    for empty { total += 100 }
    for < empty { total += 100 }
    if total != 99 { return 0 }
    return 42
}
#program_export
NamedCollection :: () -> s32 {
    values: [3]s32 = .[1, 2, 3]
    view: []s32 = values[0:2]
    total: s32 = 0
    for outer: values {
        for inner: view {
            if inner == 1 { continue outer }
        }
        total += 100
    }
    for outer: values {
        for inner: view {
            if inner == 2 { break outer }
        }
        total += 1
    }
    if total != 0 { return 0 }
    return 42
}
ZI

"$ziran" check --root "$work" "$work/collections.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/collections.zi"
for input in source saved; do
    if test "$input" = source; then
        file=$work/collections.zi
        root=$work
    else
        file=$work/ir/collections.zir
        root=$work/ir
    fi
    "$ziran" bundle --root "$root" --entry collections:Answer \
        -o "$work/$input.zib" "$file"
    test "$("$ziran" run "$work/$input.zib")" = 42
    "$ziran" bundle --root "$root" --entry collections:NamedCollection \
        -o "$work/$input-named.zib" "$file"
    test "$("$ziran" run "$work/$input-named.zib")" = 42
    for target in c cpp go; do
        out=$work/$input-$target
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --root "$root" \
                -o "$out" "$file"
            cat > "$out/main.go" <<'GO'
package main
func main() {
    if Collections_Answer() != 42 { panic("Jai collection loop") }
    if Collections_NamedCollection() != 42 { panic("Jai named collection loop") }
}
GO
            GO111MODULE=off go run "$out/collections.go" "$out/main.go"
        elif test "$target" = c; then
            "$ziran" build --target=c --root "$root" -o "$out" "$file"
            cat > "$out/main.c" <<'C'
#include "collections.h"
int main(void) { return Answer() == 42 && NamedCollection() == 42 ? 0 : 1; }
C
            "${CC:-cc}" -std=c11 -I"$repo/include" -I"$out" \
                "$out/collections.c" "$out/main.c" -o "$out/app"
            "$out/app"
        else
            "$ziran" build --target=cpp --root "$root" -o "$out" "$file"
            cat > "$out/main.cpp" <<'CPP'
#include "collections.hpp"
int main() { return Answer() == 42 && NamedCollection() == 42 ? 0 : 1; }
CPP
            "${CXX:-c++}" -std=c++17 -I"$repo/include" -I"$out" \
                "$out/collections.cpp" "$out/main.cpp" -o "$out/app"
            "$out/app"
        fi
    done
done
cmp "$work/source.zib" "$work/saved.zib"
cmp "$work/source-named.zib" "$work/saved-named.zib"

cat > "$work/pointer_for.zi" <<'ZI'
#program_export
Answer :: () -> s32 {
    values: [3]s32 = .[1, 2, 3]
    for *item, index: values {
        item.* += cast(s32) index + 1
    }
    for < * item, index: values {
        item.* += cast(s32) index
    }
    return values[0] + values[1] + values[2]
}
ZI
"$ziran" check --root "$work" "$work/pointer_for.zi"
"$ziran" ir --root "$work" -o "$work/pointer-ir" "$work/pointer_for.zi"
for input in source saved; do
    if test "$input" = source; then
        file=$work/pointer_for.zi
        root=$work
    else
        file=$work/pointer-ir/pointer_for.zir
        root=$work/pointer-ir
    fi
    for target in c cpp go; do
        out=$work/pointer-$input-$target
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --root "$root" \
                -o "$out" "$file"
            cat > "$out/main.go" <<'GO'
package main
func main() { if PointerFor_Answer() != 15 { panic("pointer for") } }
GO
            GO111MODULE=off go run "$out/pointer_for.go" "$out/main.go"
        elif test "$target" = c; then
            "$ziran" build --target=c --root "$root" -o "$out" "$file"
            cat > "$out/main.c" <<'C'
#include "pointer_for.h"
int main(void) { return Answer() == 15 ? 0 : 1; }
C
            "${CC:-cc}" -std=c11 -I"$repo/include" -I"$out" \
                "$out/pointer_for.c" "$out/main.c" -o "$out/app"
            "$out/app"
        else
            "$ziran" build --target=cpp --root "$root" -o "$out" "$file"
            cat > "$out/main.cpp" <<'CPP'
#include "pointer_for.hpp"
int main() { return Answer() == 15 ? 0 : 1; }
CPP
            "${CXX:-c++}" -std=c++17 -I"$repo/include" -I"$out" \
                "$out/pointer_for.cpp" "$out/main.cpp" -o "$out/app"
            "$out/app"
        fi
    done
done
# The portable runner points at its own array elements the same way.
"$ziran" bundle --root "$work" --entry pointer_for:Answer \
   -o "$work/pointer.zib" "$work/pointer_for.zi"
test "$("$ziran" run "$work/pointer.zib")" = 15

cat > "$work/non_collection.zi" <<'ZI'
Answer :: () -> s32 {
    for 42 { return 42 }
    return 0
}
ZI
if "$ziran" check --root "$work" "$work/non_collection.zi" \
   2> "$work/non_collection.err"; then
    echo 'for accepted a non-iterable scalar' >&2
    exit 1
fi
grep -Fq 'slice source requires a string, array, or slice' "$work/non_collection.err"

cat > "$work/duplicate_binding.zi" <<'ZI'
Answer :: () -> s32 {
    values: [1]s32 = .[42]
    for item, item: values { return item }
    return 0
}
ZI
if "$ziran" check --root "$work" "$work/duplicate_binding.zi" \
   2> "$work/duplicate_binding.err"; then
    echo 'for accepted duplicate value and index bindings' >&2
    exit 1
fi
grep -Fq 'for currently supports Jai integer ranges or array and slice iteration' \
    "$work/duplicate_binding.err"

cat > "$work/readonly_count.zi" <<'ZI'
Answer :: () -> s32 {
    values: [1]s32 = .[42]
    values.count = 2
    return values[0]
}
ZI
if "$ziran" check --root "$work" "$work/readonly_count.zi" \
   2> "$work/readonly_count.err"; then
    echo 'accepted assignment to a fixed array count' >&2
    exit 1
fi
grep -Fq 'collection count and borrowed string bytes are read-only' \
    "$work/readonly_count.err"

cat > "$work/legacy_string.zi" <<'ZI'
Answer :: () -> s64 {
    text: string = "hello"
    return text.length
}
ZI
cat > "$work/legacy_slice.zi" <<'ZI'
Answer :: () -> s64 {
    values: [2]s32 = .[1, 2]
    slice: []s32 = values[:]
    return slice.length
}
ZI
cat > "$work/legacy_array.zi" <<'ZI'
Answer :: () -> s64 {
    values: [2]s32 = .[1, 2]
    return values.length
}
ZI
for kind in string slice array; do
    source="$work/legacy_$kind.zi"
    if "$ziran" check --root "$work" "$source" \
       2> "$work/legacy_$kind.err"; then
        echo "accepted legacy $kind length syntax" >&2
        exit 1
    fi
    grep -Fq 'length is not Jai syntax; use count' \
        "$work/legacy_$kind.err"
    if "$ziran" ir --root "$work" -o "$work/legacy-ir-$kind" \
       "$source" 2> "$work/legacy_ir_$kind.err"; then
        echo "saved IR accepted legacy $kind length syntax" >&2
        exit 1
    fi
    for target in c cpp go; do
        if "$ziran" build --target="$target" --root "$work" \
           -o "$work/legacy-$kind-$target" "$source" \
           2> "$work/legacy_${kind}_$target.err"; then
            echo "$target accepted legacy $kind length syntax" >&2
            exit 1
        fi
    done
done

cat > "$work/record_length.zi" <<'ZI'
Segment :: struct {
    length: s32
}
#program_export
Answer :: () -> s32 {
    segment: Segment = .{length = 42}
    return segment.length
}
ZI
"$ziran" check --root "$work" "$work/record_length.zi"
