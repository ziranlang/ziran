#!/bin/sh
set -eu
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

"$1" check --root tests/spec --module-path std tests/spec/sort_test.zi
"$1" bundle --root tests/spec --module-path std \
    --entry sort_test:SelfTest -o "$work/test.zib" tests/spec/sort_test.zi
test "$("$1" run "$work/test.zib")" = "42"

"$1" build --target=c --root tests/spec --module-path std \
    -o "$work/c" tests/spec/sort_test.zi
printf '#include "sort_test.h"\nint main(void) { return SelfTest() == 42 ? 0 : 1; }\n' \
    > "$work/c/run.c"
"${CC:-cc}" -std=c11 -I"$work/c" "$work"/c/*.c -o "$work/c/program"
"$work/c/program"

"$1" ir --root tests/spec --module-path std -o "$work/ir" tests/spec/sort_test.zi
"$1" bundle --root "$work/ir" --entry sort_test:SelfTest -o "$work/saved.zib" "$work/ir/sort_test.zir"
cmp "$work/test.zib" "$work/saved.zib"
test "$("$1" run "$work/saved.zib")" = "42"
for form in source saved; do
    input=tests/spec/sort_test.zi; module_root=tests/spec
    if test "$form" = saved; then input=$work/ir/sort_test.zir; module_root=$work/ir; fi
    for target in c cpp; do
        output=$work/$form-$target
        "$1" build --target="$target" --root "$module_root" --module-path std -o "$output" "$input"
        if test "$target" = c; then
            printf '#include "sort_test.h"\nint main(void) { return SelfTest() == 42 ? 0 : 1; }\n' > "$output/run.c"
            "${CC:-cc}" -std=c11 -I"$output" "$output"/*.c -o "$output/program"
        else
            printf '#include "sort_test.hpp"\nint main() { return SelfTest() == 42 ? 0 : 1; }\n' > "$output/run.cpp"
            "${CXX:-c++}" -std=c++17 -I"$output" "$output"/*.cpp -o "$output/program"
        fi
        "$output/program"
    done
    "$1" build --target=py --root "$module_root" --module-path std --entry sort_test:SelfTest --exe -o "$work/$form-py" "$input"
    result=0
    python3 "$work/$form-py" || result=$?
    test "$result" = 42
done
printf 'Sorting passed source and saved VM, C, C++ and Python.\n'
