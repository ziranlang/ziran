#!/bin/sh
# check-cost: 610
# An array literal has no size cap: a table of tens of thousands of elements,
# or more than 1 MB of source text, checks in linear time and runs the same
# from source and saved IR on the VM, C and Go. Limits that remain name
# themselves.
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$repo/build/tests"
work=$(mktemp -d "$repo/build/tests/large-array-literals-XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS

# NAME COUNT: a procedure holding COUNT negative and positive s32 values,
# 16 to a line, that returns 0 only when every value arrived in order.
generate() {
    "$ziran" run --target=c --root "$repo/tests" "$repo/tests/fixture_generation.zi" -- "$ziran" "$work" array "$1" "$2"
}

# Checks NAME, saves its IR, and runs both forms on the VM and Go. C runs both
# forms when FULL_C is 1; otherwise the two forms must emit the same C and
# that C runs once, since a C compiler takes long on very large tables.
run_everywhere() {
    name=$1 full_c=$2
    "$ziran" check --root "$work" "$work/$name.zi"
    "$ziran" ir --root "$work" -o "$work/$name-ir" "$work/$name.zi"
    for form in source saved; do
        input=$work/$name.zi root=$work
        if test "$form" = saved; then
            input=$work/$name-ir/$name.zir root=$work/$name-ir
        fi
        "$ziran" bundle --root "$root" --entry "$name:main" -o "$work/$name-$form.zx" "$input"
        test "$("$ziran" run "$work/$name-$form.zx")" = 0
        "$ziran" build --target=go --pkg main --root "$root" --entry "$name:main" \
            --exe -o "$work/$name-$form-go" "$input"
        GO111MODULE=off go run "$work/$name-$form-go"/*.go
        if test "$full_c" = 1; then
            "$ziran" build --target=c --root "$root" --entry "$name:main" \
                --exe -o "$work/$name-$form-c" "$input"
            "$work/$name-$form-c/$name"
        else
            "$ziran" build --target=c --root "$root" --entry "$name:main" \
                -o "$work/$name-$form-c-text" "$input"
        fi
    done
    cmp "$work/$name-source.zx" "$work/$name-saved.zx"
    if test "$full_c" != 1; then
        diff -r "$work/$name-source-c-text" "$work/$name-saved-c-text"
        "$ziran" build --target=c --root "$work/$name-ir" --entry "$name:main" \
            --exe -o "$work/$name-saved-c" "$work/$name-ir/$name.zir"
        "$work/$name-saved-c/$name"
    fi
}

generate table20k 20000
run_everywhere table20k 1
generate table70k 70000
run_everywhere table70k 0

# More than 1 MB of literal text checks and runs through saved IR on the VM.
generate table260k 260000
test "$(wc -c < "$work/table260k.zi")" -gt 1048576
"$ziran" ir --root "$work" -o "$work/table260k-ir" "$work/table260k.zi"
"$ziran" bundle --root "$work" --entry table260k:main \
    -o "$work/table260k-source.zx" "$work/table260k.zi"
"$ziran" bundle --root "$work/table260k-ir" --entry table260k:main \
    -o "$work/table260k-saved.zx" "$work/table260k-ir/table260k.zir"
cmp "$work/table260k-source.zx" "$work/table260k-saved.zx"
test "$("$ziran" run "$work/table260k-saved.zx")" = 0

# A file-scope initializer of any length starts the variable with its values.
"$ziran" run --target=c --root "$repo/tests" "$repo/tests/fixture_generation.zi" -- "$ziran" "$work" global-array
"$ziran" bundle --root "$work" --entry global:main -o "$work/global.zx" "$work/global.zi"
test "$("$ziran" run "$work/global.zx")" = 0

# An untyped .[...] gets its element type from a declaration or a prefix.
cat > "$work/assign.zi" <<'ZI'
main :: () -> s32 {
    table: [3]s32
    table = .[1, 2, 3]
    return table[2] - 3
}
ZI
if "$ziran" check --root "$work" "$work/assign.zi" 2> "$work/assign.err"; then
    echo "an untyped array literal assignment was accepted" >&2
    exit 1
fi
grep -q 'needs a declaration that names its array type, or write the element type' "$work/assign.err"
sed 's/table = \.\[/table = s32.[/' "$work/assign.zi" > "$work/typed.zi"
"$ziran" bundle --root "$work" --entry typed:main -o "$work/typed.zx" "$work/typed.zi"
test "$("$ziran" run "$work/typed.zx")" = 0
echo 'Large array literals check in linear time and agree from source and saved IR on the VM, C and Go'
