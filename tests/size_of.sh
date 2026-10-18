#!/bin/sh
set -eu

ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/sizes.zi" <<'EOF'
Pair :: struct {
    first: s32
    second: s64
}
Sample :: struct {
    text: string
    value: s32
}
Box :: struct($T: Type) { value: T }
Wrapper :: struct($T: Type) { item: T; tail: u8 }
Concrete :: Box(s32)
PAIR_SIZE :: size_of(Pair);
TEXT_SIZE :: size_of(string);
SLICE_SIZE :: size_of([]s32);
BOX_SIZE :: size_of(Box(s32));
NESTED_SIZE :: size_of(Wrapper(Box(s32)));
GENERIC_RUN :: #run size_of(Box(s32)) + 1;
RAN :: #run size_of(s32) + 1;
SELECTED :: #ifx size_of(s32) == 4 then 42 else size_of(MissingType);
#assert size_of(Pair) == 16
#assert size_of(string) == 16
#assert size_of(float) == 4
#assert size_of(float) == size_of(float32)
#assert size_of([]s32) == 16
#assert size_of(Sample) == 24
#assert size_of(Concrete) == 4
#assert size_of(Wrapper(Box(s64))) == 16
#if size_of(Pair) == 16 {
FLAG :: 1;
} else {
FLAG :: MissingValue();
}
Buffer :: struct {
    bytes: [PAIR_SIZE]u8
}
TextBuffer :: struct {
    bytes: [TEXT_SIZE]u8
}
ComputedBuffer :: struct {
    bytes: [size_of(s32)*4]u8
}
NestedBuffer :: struct {
    bytes: [size_of(Wrapper(Box(s32)))]u8
}
calls: s32;
FloatEcho :: (value: float) -> float { return value }
Touch :: () -> s32 {
    calls += 1
    return 0
}
#program_export
NativeSliceSize :: () -> s32 { return size_of([]s32) }

#program_export
Answer :: () -> s32 {
    inferred := 4294967296
    text: string = "hello"
    number: float = FloatEcho(1.5)
    items: [2]s32
    view: []s32 = items[:]
    pair: Pair
    pair.second = inferred
    if pair.second != 4294967296 { return 0 }
    if size_of(type_of(inferred)) != 8 { return 0 }
    if size_of(type_of(pair)) != 16 { return 0 }
    if size_of(type_of(text)) != 16 { return 0 }
    if size_of(type_of(number)) != 4 { return 0 }
    if size_of(type_of(view)) != size_of([]s32) { return 0 }
    if size_of(type_of(pair.second)) != 8 { return 0 }
    if size_of(type_of(inferred + 1)) != 8 { return 0 }
    if size_of(type_of(inferred)) + size_of(type_of(pair)) != 24 { return 0 }
    if size_of(type_of(inferred)) + size_of(type_of(inferred)) != 16 { return 0 }
    if size_of(type_of(Touch())) != 4 { return 0 }
    if calls != 0 { return 0 }
    if size_of(s32) != 4 { return 0 }
    if size_of(int) != 8 { return 0 }
    if size_of(float) != 4 || size_of(float) != size_of(float32) { return 0 }
    if size_of(bool) != 1 { return 0 }
    if size_of([3]s32) != 12 { return 0 }
    if size_of(Pair) != 16 { return 0 }
    if size_of(string) != 16 { return 0 }
    if (size_of([]s32) != 16 && size_of([]s32) != 24) ||
        SLICE_SIZE != 16 { return 0 }
    if size_of(Sample) != 24 { return 0 }
    if size_of(Box(s32)) != 4 || BOX_SIZE != 4 { return 0 }
    if size_of(Concrete) != 4 { return 0 }
    if size_of(Wrapper(Box(s32))) != 8 || NESTED_SIZE != 8 { return 0 }
    if size_of(NestedBuffer) != 8 { return 0 }
    if size_of(Buffer) != 16 { return 0 }
    if size_of(TextBuffer) != 16 { return 0 }
    if size_of(ComputedBuffer) != 16 { return 0 }
    if PAIR_SIZE != 16 { return 0 }
    if TEXT_SIZE != 16 { return 0 }
    if RAN != 5 || GENERIC_RUN != 5 || SELECTED != 42 || FLAG != 1 { return 0 }
    if size_of(*s32) != size_of(usize) { return 0 }
    return 42
}
EOF

"$ziran" check --root "$work" "$work/sizes.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/sizes.zi"
if ! grep -aFq 'size_of' "$work/ir/sizes.zir"; then
    echo 'runtime size_of was folded before target selection' >&2
    exit 1
fi
"$ziran" bundle --root "$work" --entry sizes:Answer \
    -o "$work/source.zib" "$work/sizes.zi"
"$ziran" bundle --root "$work/ir" --entry sizes:Answer \
    -o "$work/saved.zib" "$work/ir/sizes.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/source.zib")" = 42
test "$("$ziran" run "$work/saved.zib")" = 42

for target in c cpp go; do
    out="$work/$target"
    if test "$target" = go; then
        "$ziran" build --target=go --pkg main --root "$work" \
            -o "$out" "$work/sizes.zi"
        cat > "$out/main.go" <<'EOF'
package main
import "unsafe"
func main() {
    if unsafe.Sizeof(Pair{}) != 16 || unsafe.Sizeof(ComputedBuffer{}) != 16 ||
       unsafe.Sizeof(Sample{}) != 24 || unsafe.Sizeof(TextBuffer{}) != 16 ||
       unsafe.Sizeof(Concrete{}) != 4 || unsafe.Sizeof(NestedBuffer{}) != 8 {
        panic("size_of disagrees with Go record layout")
    }
    if Sizes_Answer() != 42 || Sizes_NativeSliceSize() != 24 {
        panic("wrong native size_of result")
    }
}
EOF
        GO111MODULE=off go run "$out/sizes.go" "$out/main.go"
    else
        "$ziran" build "--target=$target" --root "$work" \
            -o "$out" "$work/sizes.zi"
        if test "$target" = c; then
            cat > "$out/main.c" <<'EOF'
#include "sizes.h"
_Static_assert(sizeof(Pair) == 16, "size_of disagrees with C record layout");
_Static_assert(sizeof(String) == 16, "size_of disagrees with C string layout");
_Static_assert(sizeof(Sample) == 24, "size_of disagrees with C string record layout");
_Static_assert(sizeof(ComputedBuffer) == 16, "size_of disagrees with C array layout");
_Static_assert(sizeof(Concrete) == 4, "size_of disagrees with C generic layout");
_Static_assert(sizeof(NestedBuffer) == 8, "size_of disagrees with C nested generic layout");
int main(void) { return Answer() == 42 && NativeSliceSize() == 16 ? 0 : 1; }
EOF
            ${CC:-cc} -Iinclude -I"$out" "$out/sizes.c" \
                "$out/main.c" -o "$out/app"
        else
            cat > "$out/main.cpp" <<'EOF'
#include "sizes.hpp"
static_assert(sizeof(Pair) == 16, "size_of disagrees with C++ record layout");
static_assert(sizeof(String) == 16, "size_of disagrees with C++ string layout");
static_assert(sizeof(Sample) == 24, "size_of disagrees with C++ string record layout");
static_assert(sizeof(ComputedBuffer) == 16, "size_of disagrees with C++ array layout");
static_assert(sizeof(Concrete) == 4, "size_of disagrees with C++ generic layout");
static_assert(sizeof(NestedBuffer) == 8, "size_of disagrees with C++ nested generic layout");
int main() { return Answer() == 42 && NativeSliceSize() == 16 ? 0 : 1; }
EOF
            ${CXX:-c++} -Iinclude -I"$out" "$out/sizes.cpp" \
                "$out/main.cpp" -o "$out/app"
        fi
        "$out/app"
    fi
done

# Native callback types are plain function pointers inside records.
cat > "$work/callbacks.zi" <<'EOF'
Callback :: #type (value: s32) -> s32 #c_call;
Callbacks :: struct {
    context: *void
    call: Callback
}
#assert size_of(Callback) == 8
#assert size_of(Callbacks) == 16
#program_export
CallbacksSize :: () -> s32 { return cast(s32)size_of(Callbacks) }
EOF
for target in c cpp; do
    out="$work/callbacks-$target"
    "$ziran" build "--target=$target" --root "$work" -o "$out" "$work/callbacks.zi"
    if test "$target" = c; then
        cat > "$out/main.c" <<'EOF'
#include "callbacks.h"
_Static_assert(sizeof(Callbacks) == 16, "size_of disagrees with C callback layout");
int main(void) { return CallbacksSize() == 16 ? 0 : 1; }
EOF
        ${CC:-cc} -Iinclude -I"$out" "$out/callbacks.c" "$out/main.c" -o "$out/app"
    else
        cat > "$out/main.cpp" <<'EOF'
#include "callbacks.hpp"
static_assert(sizeof(Callbacks) == 16, "size_of disagrees with C++ callback layout");
int main() { return CallbacksSize() == 16 ? 0 : 1; }
EOF
        ${CXX:-c++} -Iinclude -I"$out" "$out/callbacks.cpp" "$out/main.cpp" -o "$out/app"
    fi
    "$out/app"
done

cat > "$work/old_sizeof.zi" <<'EOF'
Answer :: () -> s32 { return sizeof(s32) }
EOF
if "$ziran" check --root "$work" "$work/old_sizeof.zi" \
    2> "$work/old_sizeof.err"; then
    echo 'C sizeof was accepted' >&2
    exit 1
fi
grep -Fq 'sizeof is not Jai syntax' "$work/old_sizeof.err"

cat > "$work/old_sizeof_constant.zi" <<'EOF'
BAD :: sizeof(s32);
EOF
if "$ziran" check --root "$work" "$work/old_sizeof_constant.zi" \
    2> "$work/old_sizeof_constant.err"; then
    echo 'C sizeof constant was accepted' >&2
    exit 1
fi
grep -Fq 'sizeof is not Jai syntax' "$work/old_sizeof_constant.err"

cat > "$work/unknown_type.zi" <<'EOF'
Answer :: () -> s32 { return size_of(MissingType) }
EOF
if "$ziran" check --root "$work" "$work/unknown_type.zi" \
    2> "$work/unknown_type.err"; then
    echo 'unknown size_of type was accepted' >&2
    exit 1
fi
grep -Fq 'size_of requires a known sized type' "$work/unknown_type.err"

cat > "$work/unknown_generic_element.zi" <<'EOF'
Box :: struct($T: Type) { value: T }
Answer :: () -> s32 { return size_of(Box(MissingType)) }
EOF
if "$ziran" check --root "$work" "$work/unknown_generic_element.zi" \
    2> "$work/unknown_generic_element.err"; then
    echo 'unknown generic element in size_of was accepted' >&2
    exit 1
fi
grep -Eq 'size_of requires a known sized type|unknown stored type' \
    "$work/unknown_generic_element.err"

cat > "$work/unknown_slice_element.zi" <<'EOF'
Answer :: () -> int { return size_of([]MissingType) }
EOF
if "$ziran" check --root "$work" "$work/unknown_slice_element.zi" \
    2> "$work/unknown_slice_element.err"; then
    echo 'unknown slice element in size_of was accepted' >&2
    exit 1
fi
grep -Fq 'size_of requires a known sized type' \
    "$work/unknown_slice_element.err"

cat > "$work/unknown_type_of.zi" <<'EOF'
Answer :: () -> s32 { return size_of(type_of(MissingValue)) }
EOF
if "$ziran" check --root "$work" "$work/unknown_type_of.zi" \
    2> "$work/unknown_type_of.err"; then
    echo 'unknown type_of operand was accepted' >&2
    exit 1
fi
grep -Fq 'unknown' "$work/unknown_type_of.err"

cat > "$work/recursive_size.zi" <<'EOF'
Loop :: struct {
    bytes: [size_of(Loop)]u8
}
Answer :: () -> s32 { return size_of(Loop) }
EOF
if "$ziran" check --root "$work" "$work/recursive_size.zi" \
    2> "$work/recursive_size.err"; then
    echo 'recursive size_of layout was accepted' >&2
    exit 1
fi
grep -Eq 'size_of requires a known sized type|array capacity is not a valid bounded integer constant' \
    "$work/recursive_size.err"

cat > "$work/literal.zi" <<'EOF'
TEXT :: "sizeof and size_of are literal text";
Answer :: () -> s32 { return 42 }
EOF
"$ziran" check --root "$work" "$work/literal.zi"

cat > "$work/global_size.zi" <<'EOF'
count: s32 = size_of(s32);
#program_export
Answer :: () -> s32 { return count }
EOF
"$ziran" check --root "$work" "$work/global_size.zi"
"$ziran" ir --root "$work" -o "$work/global-ir" \
    "$work/global_size.zi"
if grep -aFq 'size_of' "$work/global-ir/global_size.zir"; then
    echo 'global size_of was saved without constant evaluation' >&2
    exit 1
fi
for input in "$work/global_size.zi" "$work/global-ir/global_size.zir"; do
    case "$input" in
        *.zi) suffix=source ;;
        *) suffix=saved ;;
    esac
    for target in c cpp go; do
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --root "$work" \
                -o "$work/global-$target-$suffix" "$input"
        else
            "$ziran" build "--target=$target" --root "$work" \
                -o "$work/global-$target-$suffix" "$input"
        fi
    done
done
