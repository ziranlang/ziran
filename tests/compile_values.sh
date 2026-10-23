#!/bin/sh
set -eu

ziran=$1
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/lib/native_answer.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/lib.zi" <<'ZI'
Greeting :: (source: string = "hello") -> string {
    return source
}
Scale :: (source: float64 = 2.0) -> float64 {
    return source * 1.5
}
ZI
cat > "$work/app.zi" <<'ZI'
Lib :: #import "lib";
Point :: struct { x: s64; y: s64; }
Wrapper :: struct { point: Point; }
Label :: struct { text: string; number: s64; }
BuildPoint :: (n: s64) -> Point {
    value: Point = Point.{.x = n, .y = n + 1};
    return value
}
BuildWrapper :: (n: s64) -> Wrapper {
    return Wrapper.{.point = BuildPoint(n)}
}
BuildArray :: (n: s64) -> [2]s64 {
    return s64.[n, n + 1]
}
BuildLabel :: () -> Label {
    return Label.{.text = "hello", .number = 42}
}
MESSAGE :: #run Lib.Greeting("hello");
FACTOR :: #run Lib.Scale(2.0);
RAW :: #run 2.0;
POINT :: #run BuildPoint(41);
ARRAY :: #run BuildArray(41);
LABEL :: #run BuildLabel();
POINT_VALUE :: #run BuildPoint(41).y;
ARRAY_VALUE :: #run BuildArray(41)[1];
NESTED_VALUE :: #run BuildWrapper(41).point.y;
point_global: Point = POINT;
array_global: [2]s64 = ARRAY;
label_global: Label = LABEL;
#assert POINT.x == 41
#assert POINT.y == 42
#assert ARRAY.count == 2
#assert ARRAY[1] == 42
#assert POINT_VALUE == 42 && ARRAY_VALUE == 42 && NESTED_VALUE == 42
#assert MESSAGE == "hello"
#assert FACTOR == 3.0
#assert RAW == 2.0
#assert Lib.Greeting("h\u0065llo") == "hel\x6co"
#assert "\U0001F642".count == 4
#assert Lib.Greeting() == "hello"
#assert Lib.Scale() == 3.0
#if 2 + 2 == 4 {
SELECTED :: 42;
} else {
SELECTED :: Missing();
}
#if Lib.Greeting("hello") == "hello" && Lib.Scale(2.0) == 3.0 {
IMPORTED :: 42;
} else {
IMPORTED :: Missing();
}
#if MESSAGE == "hello" && FACTOR == 3.0 {
DERIVED :: 42;
} else {
DERIVED :: Missing();
}
ReadPoint :: (point: Point) -> s64 { return point.y }
#program_export
Answer :: () -> s64 {
    pick: Point = POINT;
    values: [2]s64 = ARRAY;
    if MESSAGE != "hello" || FACTOR != 3.0 || RAW != 2.0 { return 0 }
    if LABEL.text != "hello" || label_global.text != "hello" { return 0 }
    return #ifx FACTOR == 3.0 then SELECTED + IMPORTED + DERIVED + POINT_VALUE + ARRAY_VALUE + NESTED_VALUE + pick.y + values[1] + point_global.y + array_global[1] + ReadPoint(POINT) + LABEL.number - 462 else Missing()
}
ZI

"$ziran" check --root "$work" "$work/app.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/app.zi"
if rg -a -q '#run|#ifx|Missing' "$work/ir/app.zir"; then
    echo 'unresolved compile-time expression reached saved IR' >&2
    exit 1
fi
"$ziran" bundle --root "$work" --entry app:Answer \
    -o "$work/source.zib" "$work/app.zi"
"$ziran" bundle --root "$work/ir" --entry app:Answer \
    -o "$work/saved.zib" "$work/ir/app.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/saved.zib")" = 42

for input in source saved; do
    if test "$input" = source; then
        root=$work
        file=$work/app.zi
    else
        root=$work/ir
        file=$work/ir/app.zir
    fi
    for target in c cpp go; do
        out=$work/$target-$input
        native_answer_42 "$target" "$root" "$out" "$file" \
            app App_Answer 'typed #run'
    done
done

cat > "$work/effect.zi" <<'ZI'
counter: s64;
Touch :: () -> string {
    counter += 1
    return "bad"
}
MESSAGE :: #run Touch();
ZI
if "$ziran" check --root "$work" "$work/effect.zi" \
    2> "$work/effect.err"; then
    echo 'effectful typed compile-time call was accepted' >&2
    exit 1
fi
rg -q '#run expression is not a constant' "$work/effect.err"

cat > "$work/out_of_bounds.zi" <<'ZI'
Build :: () -> [2]s64 { return s64.[41, 42] }
BAD :: #run Build()[2];
ZI
if "$ziran" check --root "$work" "$work/out_of_bounds.zi" \
    2> "$work/out_of_bounds.err"; then
    echo 'out-of-bounds compile-time array selection was accepted' >&2
    exit 1
fi
rg -q '#run expression is not a constant' "$work/out_of_bounds.err"
