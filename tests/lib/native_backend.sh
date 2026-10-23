# Source with ziran, script_dir, backend and backend_extra set. The shared
# fixtures retain every original assertion, with callback-copy and vector
# metadata cases now exercised by both backends.
mkdir -p "$script_dir/../build"
work=$(mktemp -d "$script_dir/../build/$backend-backend.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

case "$backend" in
    rust)
        backend_command=cargo
        backend_label=Rust
        backend_source=src/main.rs
        backend_bounds='assertion failed: low >= 0 && low <= high'
        backend_experimental='the Rust target is still marked experimental' ;;
    py)
        backend_command=python3
        backend_label=Python
        backend_source=__main__.py
        backend_bounds='IndexError: slice range out of bounds'
        backend_experimental='the Python target is marked experimental' ;;
esac
command -v "$backend_command" >/dev/null 2>&1 || {
    echo "$backend_command is required to test the $backend_label backend" >&2
    exit 1
}

backend_prepare() (
    if test "$backend" = rust; then
        cargo build --quiet --manifest-path "$1/Cargo.toml"
    fi
)
backend_run() (
    if test "$backend" = rust; then
        "$1/target/debug/ziran_generated"
    else
        python3 "$1"
    fi
)

cat > "$work/scalars.zi" <<'ZI'
Answer :: () -> s32 {
    mode: Mode = Identify(Mode.On)
    point: Point = MakePoint(2, 3, mode)
    point.x += 1
    if point.x != 3 || point.y != 3 || point.mode != Mode.On { return 1 }
    values: [4]s32 = .[1, 2, 3, 4]
    values[1] += 10
    if Sum(values) != 20 { return 1 }
    if Add(Loop(), -10) != 0 { return 1 }
    text: string = "A\u00e9B"
    if text.count != 4 { return 2 }
    if text[0] != cast(u8)65 { return 3 }
    if text[1:3].count != 2 { return 4 }
    if text[1:3] != "\u00e9" { return 5 }
    if "\a" != "\u0007" { return 6 }
    if text != "A\u00e9B" { return 7 }
    bytes: [4]u8
    bytes[0] = cast(u8)97
    bytes[1] = cast(u8)98
    bytes[2] = cast(u8)99
    view := TextView(bytes[0:3])
    whole := TextView(bytes[:])
    if view != "abc" || view.count != 3 || whole.count != 4 ||
       whole[3] != cast(u8)0 { return 8 }
    if Limit + Offset != 11 { return 9 }
    if Abs(-9) != 9 { return 10 }
    callback: Callback = AddOne
    if callback(41) != 42 { return 11 }
    c_callback: CCallback = AddTwo
    if c_callback(40) != 42 { return 12 }
    if Recursive(2) != 2 { return 13 }
    return 0
}
Add :: (a: s32, b: s32) -> s32 { return a + b }
Identify :: (value: Mode) -> Mode { return value }
Limit :: 8;
Offset :: 3;
libc :: #system_library "libc";
Abs :: (value: s32) -> s32 #foreign libc "abs";
Callback :: #type (value: s32) -> s32;
AddOne :: (value: s32) -> s32 { return value + 1 }
CCallback :: #type (value: s32) -> s32 #c_call;
AddTwo :: (value: s32) -> s32 { return value + 2 }
Recursive :: (value: s32) -> s32 {
    if value <= 0 { return 0 }
    callback: Callback = #this
    return 1 + callback(value - 1)
}
Loop :: () -> s32 {
    total: int = 0
    for i: 0..3 { total += i }
    if total != 6 { return 1 }
    while total < 10 { total += 1 }
    return cast(s32) total
}
Mode :: enum { Off; On :: 4; }
Point :: struct { x: s32; y: s32; mode: Mode }
MakePoint :: (x: s32, y: s32, mode: Mode) -> Point {
    return .{x = x, y = y, mode = mode}
}
Sum :: (values: [4]s32) -> s32 {
    total: s32 = 0
    for value: values { total += value }
    return total
}
ZI

"$ziran" check --root "$work" "$work/scalars.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/scalars.zi"

for input in source saved; do
    if test "$input" = source; then
        file=$work/scalars.zi
        root=$work
    else
        file=$work/ir/scalars.zir
        root=$work/ir
    fi
    "$ziran" build "--target=$backend" --entry scalars:Answer --root "$root" \
        --exe -o "$work/$input" "$file"
    if test "$backend" = rust; then test -s "$work/$input/Cargo.toml"; fi
    test -s "$work/$input/$backend_source"
    backend_prepare "$work/$input"
    backend_run "$work/$input"
done

cmp "$work/source/$backend_source" "$work/saved/$backend_source"
if test "$backend" = rust; then
    cmp "$work/source/Cargo.toml" "$work/saved/Cargo.toml"
fi

cat > "$work/stored.zi" <<'ZI'
Mode :: enum u8 { Off; On; }
Callback :: #type (value: s32) -> s32;
EnumCallback :: #type (mode: Mode) -> Mode;
CCallback :: #type (value: s32) -> s32 #c_call;
HolderBuilder :: #type (value: s32) -> Holder;
Holder :: struct { callback: Callback; callbacks: [2]Callback; }
EnumHolder :: struct { callback: EnumCallback; }
NestedHolder :: struct { holder: Holder; c_callback: CCallback; }
installed: Callback;
empty: CCallback;
AddOne :: (value: s32) -> s32 { return value + 1 }
AddTwo :: (value: s32) -> s32 { return value + 2 }
Echo :: (mode: Mode) -> Mode { return mode }
CUnset :: () -> CCallback { return empty }
NewHolder :: (value: s32) -> Holder {
    result: Holder
    callback: Callback = AddOne
    result.callback = callback
    return result
}
Choose :: () -> Callback { return AddTwo }

#program_export
main :: () -> s32 {
    first: Callback = AddOne
    installed = first
    holder: Holder
    selected: Callback = Choose()
    holder.callback = selected
    holder.callbacks[0] = installed
    holder.callbacks[1] = holder.callback
    a: Callback = holder.callbacks[0]
    b: Callback = holder.callbacks[1]
    c: Callback = holder.callback
    if holder.callback == null || null == holder.callback { return 1 }
    enum_callback: EnumCallback = Echo
    enum_holder: EnumHolder
    enum_holder.callback = enum_callback
    from_holder: EnumCallback = enum_holder.callback
    if from_holder(Mode.On) != Mode.On { return 2 }
    builder: HolderBuilder = NewHolder
    built: Holder = builder(0)
    built_callback: Callback = built.callback
    if built_callback(1) != 2 { return 3 }
    if a(38) + b(0) + c(1) != 44 { return 4 }
    c_callback: CCallback = AddTwo
    if c_callback == null || null != CUnset() || c_callback(40) != 42 {
        return 5
    }
    nested: NestedHolder
    nested.holder = holder
    nested.c_callback = c_callback
    pointer := *nested
    copied := pointer.*
    copied.holder.callback = AddOne
    copied_callback: Callback = copied.holder.callback
    original_callback: Callback = nested.holder.callback
    copied_c_callback: CCallback = copied.c_callback
    if copied_callback(40) != 41 || original_callback(40) != 42 ||
        copied_c_callback(40) != 42 { return 6 }
    holders: [2]NestedHolder
    holders[0] = pointer.*
    holders[1] = pointer.*
    first_callback: Callback = holders[0].holder.callback
    second_callback: CCallback = holders[1].c_callback
    if first_callback(40) != 42 || second_callback(40) != 42 { return 7 }
    return 0
}
ZI

"$ziran" ir --root "$work" -o "$work/stored-ir" "$work/stored.zi"
for input in source saved; do
    if test "$input" = source; then
        file=$work/stored.zi
        root=$work
    else
        file=$work/stored-ir/stored.zir
        root=$work/stored-ir
    fi
    "$ziran" build "--target=$backend" --entry stored:main --root "$root" \
        --exe -o "$work/stored-$input" "$file"
    backend_prepare "$work/stored-$input"
    backend_run "$work/stored-$input"
done

cmp "$work/stored-source/$backend_source" "$work/stored-saved/$backend_source"

cat > "$work/vectors.zi" <<'ZI'
#import "vec"
#import "option"
Holder :: struct { values: Vec(s32); }

Make :: () -> Vec(s32) {
    result: Vec(s32)
    VecPush(result, 12)
    return result
}

Consume :: (items: Vec(s32)) -> s32 {
    return cast(s32) items.count
}

Borrow :: (items: *Vec(s32)) -> *Vec(s32) { return items }
Metadata :: (items: *Vec(s32)) -> s32 {
    alias := items
    if items.capacity < items.count || alias.capacity != items.capacity { return -1 }
    if Borrow(items).count != alias.count { return -2 }
    return cast(s32)alias.count
}

Early :: () -> s32 {
    values: Vec(s32)
    VecPush(values, 13)
    return values[0]
}

Deferred :: () -> s32 {
    values: Vec(s32)
    defer { VecFree(values) }
    VecPush(values, 15)
    return values[0]
}

CorePrimitives :: () -> s32 {
    builder: Vec(u8)
    if !BuilderAppend(builder, "rust") { return 16 }
    if BuilderFinish(builder) != "rust" { return 17 }
    first: Vec(s32)
    second: Vec(s32)
    VecPush(first, 18)
    VecPush(second, 19)
    VecSwap(first, second)
    if first[0] != 19 || second[0] != 18 || first.capacity < first.count {
        VecFree(first); VecFree(second); return 20
    }
    VecClear(first)
    if first.count != 0 { VecFree(first); VecFree(second); return 21 }
    VecFree(first)
    VecFree(second)
    return 22
}

#program_export
main :: () -> s32 {
    values: Vec(s32)
    if !VecPush(values, 7) { return 1 }
    if !VecPush(values, 9) { return 2 }
    if values.count != 2 || values[1] != 9 { return 3 }
    if Metadata(*values) != 2 { return 24 }
    if !VecGet(values, 0).has_value { return 4 }
    if !VecPop(values).has_value { return 5 }
    VecFree(values)
    {
        nested: Vec(s32) = Make()
        if nested.count != 1 { return 6 }
    }
    moved: Vec(s32) = Make()
    if Consume(moved) != 1 { return 7 }
    if Early() != 13 { return 8 }
    if Deferred() != 15 { return 15 }
    if CorePrimitives() != 22 { return 23 }
    holder: Holder
    if !VecPush(holder.values, 14) { return 9 }
    if holder.values.count != 1 || holder.values[0] != 14 { return 10 }
    VecFree(holder.values)
    original: Vec(s32)
    VecPush(original, 4)
    VecPush(original, 5)
    VecPush(original, 6)
    copy: Vec(s32)
    if !VecClone(copy, original) { VecFree(original); VecFree(copy); return 11 }
    {
        view: []s32 = VecSlice(original, 1, 3)
        if view.count != 2 || view[0] != 5 || view[1] != 6 {
            return 12
        }
    }
    if !VecPush(copy, 7) { return 13 }
    if original.count != 3 || copy.count != 4 || copy[3] != 7 {
        return 14
    }
    return 0
}
ZI
if test "$backend" = py; then
    sed 's/"rust"/"python"/g' "$work/vectors.zi" > "$work/vectors-py.zi"
    mv "$work/vectors-py.zi" "$work/vectors.zi"
fi
"$ziran" check --root "$work" --module-path std "$work/vectors.zi"
"$ziran" ir --root "$work" --module-path std -o "$work/vector-ir" \
    "$work/vectors.zi"
for input in source saved; do
    if test "$input" = source; then
        file=$work/vectors.zi
        root=$work
    else
        file=$work/vector-ir/vectors.zir
        root=$work/vector-ir
    fi
    "$ziran" build "--target=$backend" --entry vectors:main --root "$root" \
        --module-path std --exe -o "$work/vector-$input" "$file"
    backend_prepare "$work/vector-$input"
    backend_run "$work/vector-$input"
done
cmp "$work/vector-source/$backend_source" "$work/vector-saved/$backend_source"

"$ziran" capabilities "--target=$backend" --json > "$work/capabilities.json"
rg -q "\"target\":\"$backend\".*\"parallel_execution\":\"serial\"" "$work/capabilities.json"
if rg -q '"target_contract"' "$work/capabilities.json"; then
    echo "$backend_experimental" >&2
    exit 1
fi

cat > "$work/bounds.zi" <<'ZI'
#program_export
main :: () -> s32 {
    text: string = "A"
    return cast(s32)text[2:1].count
}
ZI
"$ziran" build "--target=$backend" --entry bounds:main --root "$work" --exe     -o "$work/bounds" "$work/bounds.zi"
backend_prepare "$work/bounds"
set +e
backend_run "$work/bounds" > "$work/bounds.out" 2> "$work/bounds.err"
status=$?
set -e
test "$status" -ne 0
rg -q "$backend_bounds" "$work/bounds.err"

cat > "$work/union.zi" <<'ZI'
Value :: union { item: s32; bits: u32 }
Answer :: () -> s32 {
    value: Value
    value.item = -1
    if value.bits != 4294967295 { return 1 }
    value.item = 42
    return value.item
}
ZI
"$ziran" build "--target=$backend" --entry union:Answer --root "$work" --exe \
    -o "$work/union" "$work/union.zi"
backend_prepare "$work/union"
set +e
backend_run "$work/union"
status=$?
set -e
test "$status" = 42

backend_extra

# A Vec moves out of a record field, and the other field keeps its own.
cat > "$work/vec_field.zi" <<'ZI'
#import "vec"
Inner :: struct { items: Vec(s32); other: Vec(s32); }
Check :: () -> s32 {
    value: Inner
    VecPush(value.items, 1)
    VecPush(value.other, 2)
    moved := value.items
    return moved[0] + value.other[0]
}
ZI
"$ziran" build "--target=$backend" --entry vec_field:Check --root "$work" \
    --module-path std --exe -o "$work/vec_field" "$work/vec_field.zi"
backend_prepare "$work/vec_field"
set +e
backend_run "$work/vec_field"
status=$?
set -e
test "$status" = 3
