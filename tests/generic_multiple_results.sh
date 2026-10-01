#!/bin/sh
# Generic result records specialize their fields, retain first-result behavior,
# and survive a separately saved library across every execution backend.
set -eu
ziran=$1
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/lib.zi" <<'ZI'
#scope_export
Pair :: (value: $T) -> T, T { return value, value; }
Mixed :: (left: $A, right: $B) -> (first: A, second: B, valid: bool) {
    return left, right, true;
}
MixedReverse :: (right: $B, left: $A) -> A, B, bool {
    return left, right, false;
}
Nested :: (value: $T) -> T, T {
    first, second := Pair(value);
    return first, second;
}
Forward :: (value: $T) -> T, T { return Pair(value); }
ArrayResult :: (value: $T) -> T, [2]T {
    values: [2]T = .[value, value];
    return value, values;
}
ArraySpaced :: (value: $T) -> T, [2] T {
    values: [2]T = .[value, value];
    return value, values;
}
SliceResult :: (values: []$T) -> T, []T { return values[0], values; }
calls: s32;
Counted :: (value: $T) -> T, s32 {
    calls += 1;
    return value, calls;
}
Concrete :: (value: $T) -> s32, bool { return 2, true; }
WithDefault :: (value: $T, extra: s32 = Offset()) -> T, s32 {
    return value, extra;
}
#scope_file
Offset :: () -> s32 { return 2; }
ZI
cat > "$work/app.zi" <<'ZI'
#import "lib";
Library :: #import "lib";
Item :: struct { value: s32; }
First :: (value: s32) -> s32 { return value; }
main :: () {
    number: s32 = 21;
    a, b := Pair(number);
    if a != 21 || b != 21 { print("scalar results\n"); return; }
    wide: s64 = 42;
    c, d := Library.Pair(wide);
    if c != 42 || d != 42 { print("wide results\n"); return; }
    first, text, valid := Library.Mixed(number, "text");
    if first != 21 || text != "text" || !valid { print("mixed results\n"); return; }
    reversed, label, flag := MixedReverse("reverse", number);
    if reversed != 21 || label != "reverse" || flag { print("parameter order\n"); return; }
    item: Item = Item.{value = 40};
    left, right := Library.Nested(item);
    if left.value != 40 || right.value != 40 { print("nested records\n"); return; }
    forwarded, other := Library.Forward(item);
    if forwarded.value != 40 || other.value != 40 { print("forwarded records\n"); return; }
    value, array := Library.ArrayResult(item);
    if value.value != 40 || array[0].value != 40 || array[1].value != 40 {
        print("array results\n"); return;
    }
    _, spaced := Library.ArraySpaced(item);
    if spaced[1].value != 40 { print("array type whitespace\n"); return; }
    slice_first, slice := Library.SliceResult(array[:]);
    if slice_first.value != 40 || slice.count != 2 || slice[1].value != 40 {
        print("slice results\n"); return;
    }
    counted, ordinal := Library.Counted(item);
    _, next := Library.Counted(number);
    if counted.value != 40 || ordinal != 1 || next != 2 {
        print("repeated result evaluation\n"); return;
    }
    _, skipped := Library.Pair(number);
    if skipped != 21 { print("skipped result\n"); return; }
    x: s32;
    y: s32;
    x, y = Library.Pair(number);
    if x != 21 || y != 21 { print("assigned results\n"); return; }
    single := Library.Pair(number);
    if single != 21 || First(Pair(number)) != 21 || Pair(number) + 1 != 22 {
        print("first result\n"); return;
    }
    fixed, yes := Library.Concrete(item);
    if fixed != 2 || !yes { print("concrete results\n"); return; }
    defaulted, extra := Library.WithDefault(value = item);
    if defaulted.value != 40 || extra != 2 { print("default result\n"); return; }
    print("%\n", defaulted.value + extra);
}
ZI
cat > "$work/named.zi" <<'ZI'
Library :: #import "lib";
Item :: struct { value: s32; }
main :: () {
    item: Item = Item.{value = 40};
    left, right := Library.Nested(item);
    value, array := Library.ArrayResult(right);
    if left.value != 40 || value.value != 40 || array[1].value != 40 {
        print("named record results\n"); return;
    }
    result, extra := Library.WithDefault(item);
    print("%\n", result.value + extra);
}
ZI

"$ziran" check --root "$work" "$work/app.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
"$ziran" ir --root "$work" -o "$work/named-ir" "$work/named.zi"
"$ziran" ir --root "$work" -o "$work/library-ir" "$work/lib.zi"
cp "$work/app.zi" "$work/library-ir/app.zi"
cp "$work/named.zi" "$work/library-ir/named.zi"
printf '42\n' > "$work/expected"
for fixture in app named; do
for input in source saved library; do
    case "$input" in
        source) root=$work; source=$work/$fixture.zi ;;
        saved)
            root=$work/ir
            if test "$fixture" = named; then root=$work/named-ir; fi
            source=$root/$fixture.zir ;;
        library) root=$work/library-ir; source=$root/$fixture.zi ;;
    esac
    output=$work/$fixture-$input
    "$ziran" bundle --root "$root" --entry "$fixture:main" -o "$output.zib" "$source"
    "$ziran" run "$output.zib" > "$output.vm"
    cmp "$work/expected" "$output.vm"
    "$ziran" build --target=c --exe --entry "$fixture:main" --root "$root" -o "$output/c" "$source"
    "$output/c/$fixture" > "$output.c"
    cmp "$work/expected" "$output.c"
    "$ziran" build --target=cpp --root "$root" -o "$output/cpp" "$source"
    printf '#include "%s.hpp"\nint main() { %s_main(); }\n' "$fixture" "$fixture" > "$output/cpp/run.cpp"
    "${CXX:-c++}" -std=c++17 -I"$output/cpp" "$output"/cpp/*.cpp -o "$output/cpp/app"
    "$output/cpp/app" > "$output.cpp"
    cmp "$work/expected" "$output.cpp"
    "$ziran" build --target=go --exe --entry "$fixture:main" --pkg main --root "$root" -o "$output/go" "$source"
    GO111MODULE=off go run "$output"/go/*.go > "$output.go"
    cmp "$work/expected" "$output.go"
    "$ziran" build --target=py --exe --entry "$fixture:main" --root "$root" -o "$output/py" "$source"
    python3 "$output/py" > "$output.py"
    cmp "$work/expected" "$output.py"
    if command -v cargo >/dev/null 2>&1; then
        "$ziran" build --target=rust --exe --entry "$fixture:main" --root "$root" -o "$output/rust" "$source"
        CARGO_TARGET_DIR="$work/rust-target" cargo build --quiet --manifest-path "$output/rust/Cargo.toml"
        "$work/rust-target/debug/ziran_generated" > "$output.rust"
        cmp "$work/expected" "$output.rust"
    fi
done
cmp "$work/$fixture-source.zib" "$work/$fixture-saved.zib"
done

cat > "$work/mismatch.zi" <<'ZI'
Bad :: (value: $T) -> T, T { return value, "wrong"; }
main :: () {
    value: s32 = 1;
    first, second := Bad(value);
}
ZI
if "$ziran" check --root "$work" "$work/mismatch.zi" 2> "$work/mismatch.err"; then
    echo 'generic result field type mismatch was accepted' >&2
    exit 1
fi
rg -q 'field type mismatch' "$work/mismatch.err"

cat > "$work/count.zi" <<'ZI'
Bad :: (value: $T) -> T, T { return value, value, value; }
ZI
if "$ziran" check --root "$work" "$work/count.zi" 2> "$work/count.err"; then
    echo 'generic result count mismatch was accepted' >&2
    exit 1
fi
rg -q 'return gives 3 values but the procedure has 2 results' "$work/count.err"

# New specializations may extend a saved library, but original checked
# declarations must still match. Do not conceal a forged effect classification.
mkdir "$work/forged"
cp "$work/named.zi" "$work/forged/named.zi"
python3 - "$work/library-ir/lib.zir" "$work/forged/lib.zir" <<'PY'
from pathlib import Path
import sys
data = Path(sys.argv[1]).read_bytes()
assert b'pure' in data
Path(sys.argv[2]).write_bytes(data.replace(b'pure', b'fake', 1))
PY
if "$ziran" check --root "$work/forged" "$work/forged/named.zi" 2> "$work/forged.err"; then
    echo 'changed saved library declaration was accepted' >&2
    exit 1
fi
rg -q 'saved IR does not match the checked program' "$work/forged.err"
echo 'generic multiple results: passed'
